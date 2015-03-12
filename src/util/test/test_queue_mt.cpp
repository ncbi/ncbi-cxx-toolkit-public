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
 * Author:  Pavel Ivanov
 *
 * File Description:
 *   Test CSyncQueue class in multithreaded environment
 *   NOTE: in order to run correctly the number of threads MUST be even!
 *
 *   The purpose of this test is to force instantiation of all available
 *   methods in CSyncQueue and related classes and minimal check of
 *   inter-thread blocking.
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbithr.hpp>
#include <corelib/ncbimtx.hpp>
#include <corelib/ncbitime.hpp>
#include <corelib/ncbi_system.hpp>
#include <corelib/test_mt.hpp>
#include <util/sync_queue.hpp>

#include <common/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;


/////////////////////////////////////////////////////////////////////////////
//  Test application

typedef CSyncQueue<int> TQueue;

class CTestQueueApp : public CThreadedApp
{
public:
    virtual bool Thread_Init(int idx);
    virtual bool Thread_Run(int idx);
protected:
    virtual bool TestApp_Init(void);
    virtual bool TestApp_Exit(void);

private:
    void RunGuarder();
    void RunConstGuarder();
    void RunAtomic();

    static TQueue sm_Queue;
    static CSemaphore sm_Sem1;
    static CSemaphore sm_Sem2;
    static CSemaphore sm_Test;
    static CSemaphore sm_Atomic;
    static CSemaphore sm_Guarder;
};


/////////////////////////////////////////////////////////////////////////////

TQueue CTestQueueApp::sm_Queue(20);
CSemaphore CTestQueueApp::sm_Sem1(0, 1);
CSemaphore CTestQueueApp::sm_Sem2(1, 1);
CSemaphore CTestQueueApp::sm_Test(0, 1);
CSemaphore CTestQueueApp::sm_Atomic(1, 1);
CSemaphore CTestQueueApp::sm_Guarder(1, 1);

/////////////////////////////////////////////////////////////////////////////
//  IMPLEMENTATION

/*
 Here implemented very complicated synchronization system. Its purpose is
 to ensure that all RunAtomic are executed serially and Run*Guarder executed
 serially. Also there is synchronization of some action between RunAtomic and
 Run*Guarder.
 In brief:
 sm_Atomic - blocks RunAtomic and ensures that all RunAtomic are executed
             serially
 sm_Guarder - blocks Run*Guarder and ensures that all RunAtomic are executed
              serially
 sm_Sem1 and sm_Sem2 are used to interact between Atomic and Guarder to
                     show up some checkpoints
 sm_Test - Do one check about correct interoperation between Atomic and Guarder
*/


void CTestQueueApp::RunAtomic() {
    // Block atomic semaphore
    sm_Atomic.Wait();

#ifdef NCBI_THREADS
    // Wait when Guarder enters locked zone
    sm_Sem1.Wait();
    // Show to Guarder that we get it and going on
    sm_Sem2.Post();
#endif

    // Set test semaphore to check interoperation between Atomic and Guarder
    sm_Test.Post();
    // Try to push - queue must wait when Guarder finishes its locked zone
    CTimeSpan span(1.);
    sm_Queue.Push(0, &span);

#ifdef NCBI_THREADS
    // Here Guarder must already reset this semaphore
    assert(!sm_Test.TryWait());
#else
    // There is no Guarder thread - nobody can reset the semaphore
    assert(sm_Test.TryWait());
#endif

    // Do some operations to force methods instantiation
    for (int i = 0; 10 > i; ++i) {
        sm_Queue.Push(i);
    }
    for (int i = 0; 5 > i; ++i) {
        sm_Queue.Pop();
    }

    TQueue other_q;
    sm_Queue.CopyTo(&other_q);
    sm_Queue.Clear();
    assert(6 == other_q.GetSize());

    other_q.Clear();
    try {
        span = CTimeSpan(0.1);
        other_q.Pop(&span);
        assert(false);
    } catch (CSyncQueueException&) {
    }

#ifdef NCBI_THREADS
    // Show to Guarder that we are finished
    sm_Sem2.Post();
#endif

    // Unblock atomic semaphore
    sm_Atomic.Post();
}


void CTestQueueApp::RunGuarder() {
    // Block guarder semaphore
    sm_Guarder.Wait();

#ifdef NCBI_THREADS
    // Wait while previous atomic finishes its tasks
    sm_Sem2.Wait();
#endif

    // Block the queue
    TQueue::TAccessGuard guard(sm_Queue);

#ifdef NCBI_THREADS
    // Show to Atomic that we entered
    sm_Sem1.Post();
    // Wait when Atomic proceeds its operation
    sm_Sem2.Wait();
#endif

    // Do some tasks to instantiate methods
    for (int i = 0; 10 > i; ++i) {
        guard.Queue().Push(i);
    }
    for (int i = 0; 5 > i; ++i) {
        guard.Queue().Pop();
    }
    assert(5 == guard.Queue().GetSize());

    // Try different iterator operations
    TQueue::TAccessGuard::TIterator it = guard.Begin();
    it++;
    ++it;
    --it;
    it--;
    it += 2;
    it -= 2;
    it = it + 2;

    TQueue::TAccessGuard::TConstIterator itc = it;
    itc++;
    ++itc;
    --itc;
    itc--;
    itc += 2;
    itc -= 2;
    itc = itc + 2;

    assert((it + 2) - it == 2);
    assert(it - 2 < it);
    assert(itc - it == -(it - itc));
    assert(2 == itc - it);

    assert(guard.End() - it >= 3);
    TQueue::TAccessGuard::TRevIterator rev_it = guard.REnd();
    --rev_it;
    assert(*guard.Begin() == *rev_it);
    it = guard.End();
    --it;
    assert(*guard.RBegin() == *it);

    // Now different guard and queue methods
    it = guard.Erase(it);
    it = guard.Erase(guard.Begin(), guard.End());
    assert(guard.Queue().GetSize() == 0);
    assert(guard.Queue().IsEmpty());

    for (int i = 0; 20 > i; ++i) {
        sm_Queue.Push(i);
    }
    assert(guard.Queue().IsFull());
    guard.Queue().Clear();
    assert(sm_Queue.IsEmpty());

#ifdef NCBI_THREADS
    // Sleep to ensure that Atomic entered to Push
    SleepMilliSec(10);
    // Check that Atomic didn't exited from Push
    assert(sm_Test.TryWait());
#else
    // Nobody can set this semaphore
    assert(!sm_Test.TryWait());
#endif

    // Unblock guarder semaphore
    sm_Guarder.Post();
}


void CTestQueueApp::RunConstGuarder() {
    // Block guarder semaphore
    sm_Guarder.Wait();

#ifdef NCBI_THREADS
    // Wait while previous atomic finishes its tasks
    sm_Sem2.Wait();
#endif

    // Block the queue
    TQueue::TConstAccessGuard guard(sm_Queue);

#ifdef NCBI_THREADS
    // Show to Atomic that we entered
    sm_Sem1.Post();
    // Wait when Atomic proceeds its operation
    sm_Sem2.Wait();
#endif

    // Do some tasks to instantiate methods
    for (int i = 0; 5 > i; ++i) {
        sm_Queue.Push(i);
    }
    assert(5 == guard.Queue().GetSize());

    // Try different iterator operations
    TQueue::TAccessGuard::TConstIterator it = guard.Begin();
    it++;
    ++it;
    --it;
    it--;
    it += 2;
    it -= 2;
    it = it + 2;

    assert((it + 2) - it == 2);
    assert(it - 2 < it);

    assert(guard.End() - it >= 3);
    TQueue::TAccessGuard::TRevConstIterator rev_it = guard.REnd();
    --rev_it;
    assert(*guard.Begin() == *rev_it);
    it = guard.End();
    --it;
    assert(*guard.RBegin() == *it);

    sm_Queue.Clear();

#ifdef NCBI_THREADS
    // Sleep to ensure that Atomic entered to Push
    SleepMilliSec(10);
    // Check that Atomic didn't exited from Push
    assert(sm_Test.TryWait());
#else
    // Nobody can set this semaphore
    assert(!sm_Test.TryWait());
#endif

    // Unblock guarder semaphore
    sm_Guarder.Post();
}


bool CTestQueueApp::Thread_Init(int /*idx*/)
{
    return true;
}


bool CTestQueueApp::Thread_Run(int idx)
{
    xncbi_SetValidateAction(eValidate_Throw);

    if ( idx % 2 != 1) {
        if (idx % 4 == 0) {
            RunGuarder();
        }
        else {
            RunConstGuarder();
        }
    }
    else {
        RunAtomic();
    }
    return true;
}


bool CTestQueueApp::TestApp_Init(void)
{
    NcbiCout
        << NcbiEndl
        << "Testing queue with "
        << NStr::IntToString(s_NumThreads)
        << " threads"
        << NcbiEndl;
    if ( s_NumThreads%2 != 0 ) {
#ifdef NCBI_THREADS
        throw runtime_error("The number of threads MUST be even");
#else
        ++s_NumThreads;
#endif
    }
    return true;
}


bool CTestQueueApp::TestApp_Exit(void)
{
    NcbiCout
        << "Test completed"
        << NcbiEndl;
    // queue must be empty
    assert( sm_Queue.IsEmpty() );
    return true;
}



/////////////////////////////////////////////////////////////////////////////
//  MAIN

int main(int argc, const char* argv[])
{
    // Execute main application function
    return CTestQueueApp().AppMain(argc, argv);
}
