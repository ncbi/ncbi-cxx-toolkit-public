/*  $Id$
* ===========================================================================
*
*                            PUBLIC DOMAIN NOTICE
*               National Center for Biotechnology Information
*
*  This software/database is a "United States Government Work" under the
*  terms of the United States Copyright Act.  It was written as part of
*  the author's official duties as a United States Government employee and
*  thus cannot be copyrighted.  This software/database is freely available
*  to the public for use. The National Library of Medicine and the U.S.
*  Government have not placed any restriction on its use or reproduction.
*
*  Although all reasonable efforts have been taken to ensure the accuracy
*  and reliability of the software and data, the NLM and the U.S.
*  Government do not and cannot warrant the performance or results that
*  may be obtained by using this software or data. The NLM and the U.S.
*  Government disclaim all warranties, express or implied, including
*  warranties of performance, merchantability or fitness for any particular
*  purpose.
*
*  Please cite the author in any work or product based on this material.
*
* ===========================================================================
*
* Author: Eugene Vasilchenko
*
* File Description:
*   Prefetch implementation
*
*/

#include <ncbi_pch.hpp>
#include <objmgr/impl/prefetch_manager_impl.hpp>
#include <objmgr/objmgr_exception.hpp>
#include <corelib/ncbithr.hpp>
#include <corelib/ncbi_system.hpp>
#include <corelib/ncbi_safe_static.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


class CPrefetchManager_Impl;


class CPrefetchThread :
    public CThreadInPool<CPrefetchToken>,
    public SPrefetchTypes
{
    typedef CThreadInPool<CPrefetchToken> TParent;
public:
    CPrefetchThread(TPool* pool, ERunMode mode = eNormal) 
        : CThreadInPool<CPrefetchToken>(pool, mode), m_CanceledAll(false)
        {
        }

    static CPrefetchThread* GetCurrentThread(void);

    CPrefetchToken GetCurrentToken(void) const
        {
            CMutexGuard guard(m_Mutex);
            return m_CurrentToken;
        }
    void SetCurrentToken(const CPrefetchToken& token)
        {
            CMutexGuard guard(m_Mutex);
            m_CurrentToken = token;
        }
    void ResetCurrentToken(void)
        {
            SetCurrentToken(CPrefetchToken());
        }

    virtual void Init(void);
    virtual void x_OnExit(void);
    virtual void ProcessRequest(const CPrefetchToken& token);

    void CancelAll(void);

private:
    mutable CMutex  m_Mutex;
    bool            m_CanceledAll;
    CPrefetchToken  m_CurrentToken;
};



CPrefetchToken_Impl::CPrefetchToken_Impl(CPrefetchManager_Impl* manager,
                                         TPriority priority,
                                         IPrefetchAction* action,
                                         IPrefetchListener* listener,
                                         TFlags flags)
    : m_Manager(manager),
      m_Priority(priority),
      m_Action(0),
      m_Listener(0),
      m_DoneSem(0, 1),
      m_State(eQueued),
      m_Progress(0)
{
    if ( action ) {
        m_Action.reset(action);
        if ( !(flags&fOwnAction) ) {
            m_Action.release();
        }
    }
    if ( listener ) {
        m_Listener.reset(listener);
        if ( !(flags&fOwnListener) ) {
            m_Listener.release();
        }
    }
}


CPrefetchToken_Impl::~CPrefetchToken_Impl(void)
{
}


bool CPrefetchToken_Impl::x_SetState(EState state)
{
    if ( state != m_State ) {
        if ( IsDone() ) {
            return false;
        }
        m_State = state;
        if ( m_Listener ) {
            m_Listener->PrefetchNotify(this, state);
        }
        if ( IsDone() ) {
            m_Manager = 0;
            m_DoneSem.Post();
        }
    }
    return true;
}


CMutex& CPrefetchToken_Impl::GetMutex(void) const
{
    return GetManager()->GetMutex();
}


CPrefetchToken::EState CPrefetchToken_Impl::SetState(EState state)
{
    if ( IsDone() ) {
        if ( m_State == state ) {
            return state;
        }
        NCBI_THROW(CObjMgrException, eOtherError,
                   "CPrefetchToken::SetState: already done");
    }
    CMutexGuard guard(GetMutex());
    EState old_state = m_State;
    if ( !x_SetState(state) ) {
        NCBI_THROW(CObjMgrException, eOtherError,
                   "CPrefetchToken::SetState: already done");
    }
    return old_state;
}


bool CPrefetchToken_Impl::TrySetState(EState state)
{
    if ( IsDone() ) {
        return m_State == state;
    }
    CMutexGuard guard(GetMutex());
    return x_SetState(state);
}


CPrefetchToken::TProgress CPrefetchToken_Impl::SetProgress(TProgress progress)
{
    CMutexGuard guard(GetMutex());
    if ( m_State != eStarted ) {
        NCBI_THROW(CObjMgrException, eOtherError,
                   "CPrefetchToken::SetProgress: not processing");
    }
    TProgress old_progress = m_Progress;
    if ( progress != old_progress ) {
        m_Progress = progress;
        if ( m_Listener ) {
            m_Listener->PrefetchNotify(this, eAdvanced);
        }
    }
    return old_progress;
}


void CPrefetchToken_Impl::Cancel(void)
{
    TrySetState(eCanceled);
}


void CPrefetchToken_Impl::Wait(void)
{
    if ( !IsDone() ) {
        m_DoneSem.Wait();
        m_DoneSem.Post();
        _ASSERT(IsDone());
    }
}


bool CPrefetchToken_Impl::TryWait(unsigned int timeout_sec,
                                  unsigned int timeout_nsec)
{
    if ( !IsDone() ) {
        if ( !m_DoneSem.TryWait(timeout_sec, timeout_nsec) ) {
            return false;
        }
        m_DoneSem.Post();
        _ASSERT(IsDone());
    }
    return true;
}


CPrefetchManager_Impl::CPrefetchManager_Impl(void)
    : CPoolOfThreads<CPrefetchToken>(3, kMax_Int), m_CanceledAll(false)
{
}


CPrefetchManager_Impl::~CPrefetchManager_Impl(void)
{
    try {
        KillAllThreads();
    }
    catch(...) {
        // Just to be sure that we will not throw from the destructor.
    }
}


CPrefetchToken CPrefetchManager_Impl::AddAction(TPriority priority,
                                                IPrefetchAction* action,
                                                IPrefetchListener* listener,
                                                TFlags flags)
{
    CRef<CPrefetchToken_Impl> impl
        (new CPrefetchToken_Impl(this, priority, action, listener, flags));
    CPrefetchToken token(impl);
    AcceptRequest(token, priority);
    return token;
}


CPrefetchManager_Impl::TThread* CPrefetchManager_Impl::NewThread(ERunMode mode)
{
    return new CPrefetchThread(this, mode);
}


void CPrefetchManager_Impl::CancelQueue(void)
{
    for ( ;; ) {
        CPrefetchToken token;
        try {
            token = m_Queue.Get(0);
        }
        catch ( CBlockingQueueException& ) {
            break;
        }
        token.Cancel();
    }
}


void CPrefetchManager_Impl::KillAllThreads(void)
{
    {{
        CMutexGuard guard(GetMutex());
        m_MaxThreads = 0;
        m_CanceledAll = true;
        ITERATE ( TThreads, it, m_Threads ) {
            dynamic_cast<CPrefetchThread&>(it->GetNCObject()).CancelAll();
        }
    }}
    CancelQueue();

    int n;
    {{
        CMutexGuard guard(GetMutex());
        n = m_Threads.size();
    }}
    vector<CPrefetchToken> kill_tokens;
    while ( n ) {
        try {
            kill_tokens.push_back(AddAction(0, 0, 0, 0));
            --n;
        }
        catch (CBlockingQueueException&) { // guard against races
        }
    }
    ITERATE ( vector<CPrefetchToken>, it, kill_tokens ) {
        it->x_GetImpl().Wait();
    }
    
    ITERATE(TThreads, it, m_Threads) {
        it->GetNCObject().Join();
    }
    m_Threads.clear();
}


void CPrefetchManager_Impl::Register(TThread& thread)
{
    CMutexGuard guard(GetMutex());
    if (m_MaxThreads > 0) {
        m_Threads.insert(CRef<TThread>(&thread));
    }
}


void CPrefetchManager_Impl::UnRegister(TThread& thread)
{
    CMutexGuard guard(GetMutex());
    if (m_MaxThreads > 0) {
        m_Threads.erase(CRef<TThread>(&thread));
    }
}


static CSafeStaticRef< CTls<CPrefetchThread> > s_PrefetchThread;


CPrefetchThread* CPrefetchThread::GetCurrentThread(void)
{
    return s_PrefetchThread.Get().GetValue();
}


class CSaveCurrentToken
{
public:
    CSaveCurrentToken(const CPrefetchToken& token)
        {
            CPrefetchThread::GetCurrentThread()->SetCurrentToken(token);
        }
    ~CSaveCurrentToken(void)
        {
            CPrefetchThread::GetCurrentThread()->ResetCurrentToken();
        }
private:
    CSaveCurrentToken(const CSaveCurrentToken&);
    void operator=(const CSaveCurrentToken&);
};


CPrefetchToken CPrefetchToken_Impl::GetCurrentToken(void)
{
    CPrefetchThread* thread = CPrefetchThread::GetCurrentThread();
    return thread? thread->GetCurrentToken(): CPrefetchToken();
}


void CPrefetchThread::CancelAll(void)
{
    m_CanceledAll = true;
    if ( CPrefetchToken token = GetCurrentToken() ) {
        token.Cancel();
    }
}


void CPrefetchThread::Init(void)
{
    s_PrefetchThread.Get().SetValue(this);
    TParent::Init();
}


void CPrefetchThread::x_OnExit(void)
{
    s_PrefetchThread.Get().SetValue(0);
    TParent::x_OnExit();
}


void CPrefetchThread::ProcessRequest(const CPrefetchToken& token)
{
    try {
        CPrefetchToken_Impl& impl = token.x_GetImpl();
        IPrefetchAction* action = impl.GetAction();
        if ( !action ) {
            token.Cancel();
            Exit(0);
        }
        if ( m_CanceledAll ) {
            token.Cancel();
        }
        if ( impl.TrySetState(eStarted) ) {
            CSaveCurrentToken save(token);
            bool ok = action && action->Execute(token);
            impl.TrySetState(ok? eCompleted: eFailed);
        }
    }
    catch ( CPrefetchCanceled& /* ignored */ ) {
        token.Cancel();
    }
    catch ( CException& exc ) {
        token.x_GetImpl().TrySetState(eFailed);
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE
