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
#include <objmgr/prefetch_manager.hpp>
#include <objmgr/objmgr_exception.hpp>
#include <corelib/ncbithr.hpp>
#include <corelib/ncbi_system.hpp>
#include <corelib/ncbi_safe_static.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


class CPrefetchManager_Impl;


CPrefetchRequest::CPrefetchRequest(CObjectFor<CMutex>* state_mutex,
                                   IPrefetchAction* action,
                                   IPrefetchListener* listener,
                                   unsigned int priority)
    : CThreadPool_Task(priority),
      m_StateMutex(state_mutex),
      m_Action(action),
      m_Listener(listener),
      m_Progress(0)
{
}


CPrefetchRequest::~CPrefetchRequest(void)
{
}


CPrefetchRequest::EState CPrefetchRequest::GetState(void) const
{
    if (IsCancelRequested())
        return SPrefetchTypes::eCanceled;

    switch (GetStatus()) {
    case CThreadPool_Task::eIdle:
        return eInvalid;
    case CThreadPool_Task::eQueued:
        return SPrefetchTypes::eQueued;
    case CThreadPool_Task::eExecuting:
        return eStarted;
    case CThreadPool_Task::eCompleted:
        return SPrefetchTypes::eCompleted;
    case CThreadPool_Task::eCanceled:
        return SPrefetchTypes::eCanceled;
    case CThreadPool_Task::eFailed:
        return SPrefetchTypes::eFailed;
    }

    return eInvalid;
}


void CPrefetchRequest::SetListener(IPrefetchListener* listener)
{
    CMutexGuard guard(m_StateMutex->GetData());
    if ( m_Listener ) {
        NCBI_THROW(CObjMgrException, eOtherError,
                   "CPrefetchToken::SetListener: listener already set");
    }
    m_Listener = listener;
}


void CPrefetchRequest::OnStatusChange(EStatus /* old */)
{
    // Changing to status eCanceled will be already notified
    // via OnCancelRequested(). So we will not notify twice.
    if (GetStatus() != CThreadPool_Task::eCanceled  &&  m_Listener) {
        m_Listener->PrefetchNotify(Ref(this), GetState());
    }
}

void CPrefetchRequest::OnCancelRequested(void)
{
    OnStatusChange(GetStatus());
}

CPrefetchRequest::TProgress
CPrefetchRequest::SetProgress(TProgress progress)
{
    CMutexGuard guard(m_StateMutex->GetData());
    if ( GetStatus() != eExecuting ) {
        NCBI_THROW(CObjMgrException, eOtherError,
                   "CPrefetchToken::SetProgress: not processing");
    }
    TProgress old_progress = m_Progress;
    if ( progress != old_progress ) {
        m_Progress = progress;
        if ( m_Listener ) {
            m_Listener->PrefetchNotify(Ref(this), eAdvanced);
        }
    }
    return old_progress;
}


CPrefetchRequest::EStatus CPrefetchRequest::Execute(void)
{
    try {
        EStatus result = CThreadPool_Task::eCompleted;
        if (m_Action.NotNull()) {
            if (! GetAction()->Execute(Ref(this)))
                result = CThreadPool_Task::eFailed;
        }
        return result;
    }
    catch ( CPrefetchCanceled& /* ignored */ ) {
        return CThreadPool_Task::eCanceled;
    }
}


CPrefetchManager_Impl::CPrefetchManager_Impl(void)
    : CThreadPool(kMax_Int, 3),
      m_StateMutex(new CObjectFor<CMutex>())
{
}


CPrefetchManager_Impl::~CPrefetchManager_Impl(void)
{
}


CRef<CPrefetchRequest> CPrefetchManager_Impl::AddAction(TPriority priority,
                                                        IPrefetchAction* action,
                                                        IPrefetchListener* listener)
{
    CMutexGuard guard0(GetMainPoolMutex());
    if ( action && IsAborted() ) {
        NCBI_THROW(CPrefetchCanceled, eCanceled,
                   "prefetch manager is canceled");
    }
    CMutexGuard guard(m_StateMutex->GetData());
    CRef<CPrefetchRequest> req(new CPrefetchRequest(m_StateMutex,
                                                    action,
                                                    listener,
                                                    priority));
    AddTask(req);
    return req;
}


CPrefetchManager::EState CPrefetchManager::GetCurrentTokenState(void)
{
    CThreadPool_Thread* thread = dynamic_cast<CThreadPool_Thread*>(
                                                CThread::GetCurrentThread());
    if ( !thread ) {
        return eInvalid;
    }

    CPrefetchRequest* req = static_cast<CPrefetchRequest*>(
                                    thread->GetCurrentTask().GetNCPointer());
    if (req)
        return req->GetState();
    else
        return eInvalid;
}


bool CPrefetchManager::IsActive(void)
{
    switch ( GetCurrentTokenState() ) {
    case eCanceled:
        NCBI_THROW(CPrefetchCanceled, eCanceled, "canceled");
    case eInvalid:
        return false;
    default:
        return true;
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE
