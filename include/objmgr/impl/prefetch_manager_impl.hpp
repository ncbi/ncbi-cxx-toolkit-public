#ifndef PREFETCH_MANAGER_IMPL__HPP
#define PREFETCH_MANAGER_IMPL__HPP

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
*   Prefetch manager
*
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>
#include <corelib/ncbimisc.hpp>
#include <objmgr/prefetch_manager.hpp>
#include <util/thread_pool.hpp>
#include <set>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CScope;

/** @addtogroup ObjectManagerCore
 *
 * @{
 */


class NCBI_XOBJMGR_EXPORT CPrefetchToken_Impl : public CObject,
                                                public SPrefetchTypes
{
public:
    CPrefetchToken_Impl(CPrefetchManager_Impl* manager,
                        TPriority priority,
                        IPrefetchAction* action,
                        IPrefetchListener* listener,
                        TFlags flags);
    ~CPrefetchToken_Impl(void);
    
    CPrefetchManager_Impl* GetManager(void) const
        {
            return m_Manager;
        }

    TPriority GetPriority(void) const
        {
            return m_Priority;
        }
    
    IPrefetchAction* GetAction(void) const
        {
            return m_Action.get();
        }

    IPrefetchListener* GetListener(void) const
        {
            return m_Listener.get();
        }
    IPrefetchListener* SetListener(IPrefetchListener* listener);
    
    EState GetState(void) const
        {
            return m_State;
        }

    // in one of final states: completed, failed, canceled 
    bool IsDone(void) const
        {
            return GetState() >= eCompleted;
        }

    // returns old state
    EState SetState(EState state);
    // returns true if new state is equal to 'state'
    bool TrySetState(EState state);

    TProgress GetProgress(void) const
        {
            return m_Progress;
        }
    TProgress SetProgress(TProgress progress);

    void Wait(void);
    bool TryWait(unsigned int timeout_sec = 0, unsigned int timeout_nsec = 0);
    void Cancel(void);
    
    static CPrefetchToken GetCurrentToken(void);
    //static CPrefetchToken_Impl* SetCurrentToken(CPrefetchToken_Impl* token);

protected:
    CMutex& GetMutex(void) const;
    bool x_SetState(EState state);
    
private:
    friend class CPrefetchManager;
    friend class CPrefetchManager_Impl;

    CPrefetchManager_Impl*      m_Manager;
    TPriority                   m_Priority;
    AutoPtr<IPrefetchAction>    m_Action;
    AutoPtr<IPrefetchListener>  m_Listener;
    CSemaphore                  m_DoneSem; // raised if done
    EState                      m_State;
    TProgress                   m_Progress;
    
private:
    CPrefetchToken_Impl(const CPrefetchToken_Impl&);
    void operator=(const CPrefetchToken_Impl&);
};


class NCBI_XOBJMGR_EXPORT CPrefetchManager_Impl
    : public CObject,
      public CPoolOfThreads<CPrefetchToken>,
      public SPrefetchTypes
{
public:
    CPrefetchManager_Impl(void);
    ~CPrefetchManager_Impl(void);

    typedef CPrefetchToken_Impl::TPriority TPriority;

    size_t GetThreadCount(void) const;
    size_t SetThreadCount(size_t count);

    CPrefetchToken AddAction(TPriority priority,
                             IPrefetchAction* action,
                             IPrefetchListener* listener,
                             TFlags flags);

    void Register(TThread& thread);
    void UnRegister(TThread& thread);

    void CancelQueue(void);
    void KillAllThreads(void);

protected:
    friend class CPrefetchThread;
    friend class CPrefetchToken_Impl;

    virtual TThread* NewThread(ERunMode mode);
    CMutex& GetMutex(void)
        {
            return m_Mutex;
        }

private:
    typedef set<CRef<TThread> > TThreads;

    bool        m_CanceledAll;
    TThreads    m_Threads;
};


/* @} */


END_SCOPE(objects)
END_NCBI_SCOPE

#endif  // PREFETCH_MANAGER_IMPL__HPP
