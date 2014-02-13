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
* Author:  Aaron Ucko
*
* File Description:
*   Pools of generic request-handling threads.
*/

#include <ncbi_pch.hpp>
#include <util/thread_pool.hpp>
#include <corelib/ncbi_system.hpp>
#include <algorithm>

BEGIN_NCBI_SCOPE


NCBI_PARAM_DEF_EX(bool, ThreadPool, Catch_Unhandled_Exceptions, true, 0,
                  THREADPOOL_CATCH_UNHANDLED_EXCEPTIONS);


class CFatalRequest : public CStdRequest
{
protected:
    void Process(void) { CThread::Exit(0); } // Kill the current thread
};


void CStdPoolOfThreads::KillAllThreads(TKillFlags flags)
{
    TACValue n, old_max;
    bool queuing_was_forbidden;
    {{
        CMutexGuard guard(m_Mutex);
        old_max = m_MaxThreads;
        queuing_was_forbidden = m_QueuingForbidden;
        m_MaxThreads = 0;  // Forbid spawning new threads
        m_QueuingForbidden = false; // Always queue normally here.
        n = m_ThreadCount.Get(); // Capture for use without mutex
    }}

    {{
        size_t n2 = m_Threads.size();
        if (n != n2) {
            ERR_POST(Warning << "Registered " << n2 << " threads but expected "
                     << n);
            if (n2 > n) {
                n = n2;
            }
        }
    }}

    CRef<CStdRequest> poison(new CFatalRequest);

    for (TACValue i = 0;  i < n;  ) {
        try {
            WaitForRoom();
            AcceptRequest(poison);
            ++i;
        } catch (CBlockingQueueException&) { // guard against races
            continue;
        }
    }
    NON_CONST_ITERATE(TThreads, it, m_Threads) {
        if ((flags & fKill_Wait) != 0) {
            (*it)->Join();
        } else {
            (*it)->Detach();
        }
    }
    m_Threads.clear();
    CMutexGuard guard(m_Mutex);
    m_QueuingForbidden = queuing_was_forbidden;
    if ((flags & fKill_Reopen) != 0) {
        m_MaxThreads = old_max;
    }
}


void CStdPoolOfThreads::Register(TThread& thread)
{
    CMutexGuard guard(m_Mutex);
    if (m_MaxThreads > 0) {
        m_Threads.push_back(CRef<TThread>(&thread));
    } else {
        NCBI_THROW(CThreadException, eRunError,
                   "No more threads allowed in pool.");
    }
}

void CStdPoolOfThreads::UnRegister(TThread& thread)
{
    CMutexGuard guard(m_Mutex);
    if (m_MaxThreads > 0) {
        TThreads::iterator it = find(m_Threads.begin(), m_Threads.end(),
                                     CRef<TThread>(&thread));
        if (it != m_Threads.end()) {
            (*it)->Detach();
            m_Threads.erase(it);
        }
    }
}

CStdPoolOfThreads::~CStdPoolOfThreads()
{
    try {
        KillAllThreads(0);
    } catch(...) {}    // Just to be sure that we will not throw from the destructor.
}

END_NCBI_SCOPE
