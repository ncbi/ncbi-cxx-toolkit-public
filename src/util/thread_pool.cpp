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
*
* ---------------------------------------------------------------------------
* $Log$
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

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <util/thread_pool.hpp>

BEGIN_NCBI_SCOPE

class CFatalRequest : public CStdRequest
{
public:
    CFatalRequest(CSemaphore* sem) : m_Sem(sem) {}

protected:
    // Kill the current thread
    virtual void Process(void);

private:
    CSemaphore* m_Sem;
};


void CFatalRequest::Process(void)
{ 
    if (m_Sem) {
        m_Sem->Post();
    }
    CThread::Exit(0);
}


void CStdPoolOfThreads::KillAllThreads(bool wait)
{
    unsigned int   n;
    {{
        CMutexGuard guard(m_Mutex);
        m_MaxThreads = 0;  // Forbid spawning new threads
        n = m_ThreadCount; // Capture for use without mutex
    }}

    auto_ptr<CSemaphore> sem;
    if (wait) {
        sem.reset(new CSemaphore(0, n));
    }
    CFatalRequest* poison = new CFatalRequest(sem.get());

    for (unsigned int i = 0;  i < n;  i++) {
        AcceptRequest(CRef<ncbi::CStdRequest>(poison));
    }
    if (wait) {
        // Wait for all threads to post to the semaphore
        for (unsigned int i = 0;  i < n;  i++) {
            sem->Wait();
        }
    }
}

END_NCBI_SCOPE
