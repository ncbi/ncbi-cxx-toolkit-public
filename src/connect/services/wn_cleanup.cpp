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
 *    NetSchedule Worker Node - per-job and global clean-up, declarations.
 */

#include <ncbi_pch.hpp>

#include "wn_cleanup.hpp"
#include "grid_worker_impl.hpp"

#include <connect/services/grid_globals.hpp>


#define NCBI_USE_ERRCODE_X   ConnServ_WorkerNode

BEGIN_NCBI_SCOPE

void CWorkerNodeCleanup::AddListener(IWorkerNodeCleanupEventListener* listener)
{
    CFastMutexGuard g(m_ListenersLock);
    m_Listeners.insert(listener);
}

void CWorkerNodeCleanup::RemoveListener(
    IWorkerNodeCleanupEventListener* listener)
{
    CFastMutexGuard g(m_ListenersLock);
    m_Listeners.erase(listener);
}

void CWorkerNodeCleanup::CallEventHandlers()
{
    TListeners listeners;
    {
        CFastMutexGuard g(m_ListenersLock);
        listeners.swap(m_Listeners);
    }

    ITERATE(TListeners, it, listeners) {
        try {
            (*it)->HandleEvent(
                IWorkerNodeCleanupEventListener::eRegularCleanup);
            delete *it;
        }
        NCBI_CATCH_ALL_X(39, "Job clean-up error");
    }
}

void CWorkerNodeCleanup::RemoveListeners(
    const CWorkerNodeCleanup::TListeners& listeners)
{
    CFastMutexGuard g(m_ListenersLock);
    ITERATE(TListeners, it, listeners) {
        m_Listeners.erase(*it);
    }
}

void CWorkerNodeJobCleanup::AddListener(
    IWorkerNodeCleanupEventListener* listener)
{
    CWorkerNodeCleanup::AddListener(listener);
    m_WorkerNodeCleanup->AddListener(listener);
}

void CWorkerNodeJobCleanup::RemoveListener(
    IWorkerNodeCleanupEventListener* listener)
{
    CWorkerNodeCleanup::RemoveListener(listener);
    m_WorkerNodeCleanup->RemoveListener(listener);
}

void CWorkerNodeJobCleanup::CallEventHandlers()
{
    {
        CFastMutexGuard g(m_ListenersLock);
        m_WorkerNodeCleanup->RemoveListeners(m_Listeners);
    }
    CWorkerNodeCleanup::CallEventHandlers();
}

void* CGridCleanupThread::Main()
{
    m_WorkerNode->m_CleanupEventSource->CallEventHandlers();
    m_Listener->OnGridWorkerStop();
    m_Semaphore.Post();

    return NULL;
}

int SGridWorkerNodeImpl::x_WNCleanUp()
{
    CRef<CGridCleanupThread> cleanup_thread(
        new CGridCleanupThread(this, m_Listener.get()));

    cleanup_thread->Run();

    if (cleanup_thread->Wait(m_ThreadPoolTimeout)) {
        cleanup_thread->Join();
        LOG_POST_X(58, Info << "Cleanup thread finished");
    } else {
        ERR_POST_X(59, "Clean-up thread timed out");
    }

    return CGridGlobals::GetInstance().GetExitCode();
}

END_NCBI_SCOPE
