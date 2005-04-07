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

class CFatalRequest : public CStdRequest
{
protected:
    void Process(void) { CThread::Exit(0); } // Kill the current thread
};


void CStdPoolOfThreads::KillAllThreads(bool wait)
{
    TACValue n;
    {{
        CMutexGuard guard(m_Mutex);
        m_MaxThreads = 0;  // Forbid spawning new threads
        n = m_ThreadCount.Get(); // Capture for use without mutex
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
        if (wait) {
            (*it)->Join();
        } else {
            (*it)->Detach();
        }
    }
    m_Threads.clear();
}


void CStdPoolOfThreads::Register(TThread& thread)
{
    CMutexGuard guard(m_Mutex);
    m_Threads.push_back(CRef<TThread>(&thread));
}

void CStdPoolOfThreads::UnRegister(TThread& thread)
{
    CMutexGuard guard(m_Mutex);
    TThreads::iterator it = find(m_Threads.begin(), m_Threads.end(),
                                 CRef<TThread>(&thread));
    if (it !=  m_Threads.end()) {
        (*it)->Detach();
        m_Threads.erase( it );
    }
}

CStdPoolOfThreads::~CStdPoolOfThreads()
{
    try {
        KillAllThreads(false);
    } catch(...) {}    // Just to be sure that we will not throw from the destructor.
}

END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
* Revision 1.8  2005/04/07 13:13:26  didenko
* + destructor to CStdPoolOfThreads call
*
* Revision 1.7  2005/03/14 17:10:30  didenko
* + request priority to CBlockingQueue and CPoolOfThreads
* + DoxyGen
*
* Revision 1.6  2004/10/18 14:44:42  ucko
* KillAllThreads: don't try to poison full queues.
*
* Revision 1.5  2004/09/08 14:21:20  ucko
* Rework again to eliminate races in KillAllThreads.
*
* Revision 1.4  2004/06/02 17:50:49  ucko
* CPoolOfThreads: change type of m_Delta and m_ThreadCount to
* CAtomicCounter to reduce need for m_Mutex.
* KillAllThreads: make poison a CRef in the first place to make certain
* it doesn't get GCed prematurely.
* Move CVS log to end.
*
* Revision 1.3  2004/05/17 21:06:02  gorelenk
* Added include of PCH ncbi_pch.hpp
*
* Revision 1.2  2002/11/04 21:29:22  grichenk
* Fixed usage of const CRef<> and CRef<> constructor
*
* Revision 1.1  2001/12/11 19:54:45  ucko
* Introduce thread pools.
*
* ===========================================================================
*/
