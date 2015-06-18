#ifndef WN_COMMIT_THREAD__HPP
#define WN_COMMIT_THREAD__HPP


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
 *   Government have not placed any restriction on its use or reproduction.
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
 * Authors:  Dmitry Kazimirov
 *
 * File Description:
 *    NetSchedule Worker Node - job committer thread, declarations.
 */

#include <connect/services/grid_worker.hpp>

#include <deque>

BEGIN_NCBI_SCOPE

/////////////////////////////////////////////////////////////////////////////
//
/// @internal
class CRequestContextSwitcher
{
public:
    CRequestContextSwitcher() {}

    CRequestContextSwitcher(CRequestContext* new_request_context)
        : m_SavedRequestContext(&CDiagContext::GetRequestContext())
    {
        CDiagContext::SetRequestContext(new_request_context);
    }

    void Release()
    {
        if (m_SavedRequestContext) {
            CDiagContext::SetRequestContext(m_SavedRequestContext);
            m_SavedRequestContext.Reset();
        }
    }

    ~CRequestContextSwitcher()
    {
        Release();
    }

private:
    CRef<CRequestContext> m_SavedRequestContext;
};

/////////////////////////////////////////////////////////////////////////////
//
/// @internal
class CJobCommitterThread : public CThread
{
public:
    CJobCommitterThread(SGridWorkerNodeImpl* worker_node);

    CWorkerNodeJobContext AllocJobContext();

    void RecycleJobContextAndCommitJob(SWorkerNodeJobContextImpl* job_context,
            CRequestContextSwitcher& rctx_switcher);

    void Stop();

private:
    typedef CRef<SWorkerNodeJobContextImpl> TEntry;
    typedef deque<TEntry> TCommitJobTimeline;

    virtual void* Main();

    bool WaitForTimeout();
    bool x_CommitJob(SWorkerNodeJobContextImpl* job_context);

    void WakeUp()
    {
        if (m_ImmediateActions.empty())
            m_Semaphore.Post();
    }

    SGridWorkerNodeImpl* m_WorkerNode;
    CSemaphore m_Semaphore;
    TCommitJobTimeline m_ImmediateActions, m_Timeline, m_JobContextPool;
    CFastMutex m_TimelineMutex;
    const string m_ThreadName;

    typedef CGuard<CFastMutex, SSimpleUnlock<CFastMutex>,
            SSimpleLock<CFastMutex> > TFastMutexUnlockGuard;
};

END_NCBI_SCOPE

#endif // WN_COMMIT_THREAD__HPP
