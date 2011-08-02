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
 * Author:  Andrei Gourianov
 *
 * File Description:
 *   Test CConditionVariable class in multithreaded environment
 *   (producer/consumer queue)
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbithr.hpp>
#include <corelib/ncbimtx.hpp>
#include <corelib/ncbi_system.hpp>
#include <corelib/test_mt.hpp>

#include <common/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;


#define USE_FAST_MUTEX  0
#define USE_WRONG_MUTEX 0
#define USE_INF_TIMEOUT 0
#define USE_TEST_OUTPUT 0


#if USE_TEST_OUTPUT
#  define __TEST_OUTPUT(x)  NcbiCout << idx << x << NcbiEndl
#else
#  define __TEST_OUTPUT(x)  
#endif



/////////////////////////////////////////////////////////////////////////////
//  Test application

class CTestCondVarApp : public CThreadedApp
{
public:
    CTestCondVarApp(void);
    virtual bool Thread_Init(int idx);
    virtual bool Thread_Run(int idx);
protected:
    virtual bool TestApp_Init(void);
    virtual bool TestApp_Exit(void);

private:
    void Produce(int idx);
    void Consume(int idx);
    
    CConditionVariable m_BufferNotFull;
    CConditionVariable m_BufferNotEmpty;
#if USE_FAST_MUTEX
    CFastMutex         m_BufferLock;
#else
    CMutex             m_BufferLock;
#endif

    const size_t m_BufferSize;
    long   m_Buffer[10];
    long   m_LastItemProduced;
    size_t m_QueueSize;
    size_t m_QueueStartOffset;
    size_t m_TotalItemsProduced;
    size_t m_TotalItemsConsumed;

    bool m_StopRequested;
    
    size_t m_TotalWaitForNotFull;
    size_t m_TotalWaitForNotEmpty;
    size_t m_TotalWaitTimeout;
    size_t m_TotalRetired;
};


/////////////////////////////////////////////////////////////////////////////

CTestCondVarApp::CTestCondVarApp(void)
    : m_BufferSize(10)
{
    m_LastItemProduced = 0;
    m_QueueSize = 0;
    m_QueueStartOffset = 0;
    m_TotalItemsProduced = 0;
    m_TotalItemsConsumed = 0;
    m_StopRequested = false;
    
    m_TotalWaitForNotFull = 0;
    m_TotalWaitForNotEmpty = 0;
    m_TotalWaitTimeout = 0;
    m_TotalRetired = 0;
}


void CTestCondVarApp::Produce(int idx)
{
    int i = 1;
    bool alt = ((idx+1) % 4) != 0;
    __TEST_OUTPUT(" producer started");
    srand((unsigned) time(0));
    for (;;) {
//        SleepMilliSec( i < 25 ? 5 : (i%10)*20 );
        if (i % 50 < 25) {
            SleepMilliSec( rand() % 10 );
        } else {
            SleepMilliSec( 120 );
        }
        m_BufferLock.Lock();

        while (m_QueueSize == m_BufferSize && !m_StopRequested) {
            __TEST_OUTPUT(" producer waiting for not full");
            // Buffer is full - sleep so consumers can get items.
            m_BufferNotFull.WaitForSignal( m_BufferLock);
            ++m_TotalWaitForNotFull;
        }
        if (m_StopRequested) {
            m_BufferLock.Unlock();
            break;
        }
        size_t Item = m_LastItemProduced;
        i = Item;
        __TEST_OUTPUT(" produced: " << Item);
        m_Buffer[(m_QueueStartOffset + m_QueueSize)%m_BufferSize] =
            m_LastItemProduced++;
        ++m_QueueSize;
        ++m_TotalItemsProduced;

        if (alt) {
            m_BufferNotEmpty.SignalSome();
        } else {
            m_BufferNotEmpty.SignalAll();
        }
        m_BufferLock.Unlock();

    }
    __TEST_OUTPUT(" producer stopped");
}


void CTestCondVarApp::Consume(int idx)
{
    srand((unsigned) time(0));
#if USE_WRONG_MUTEX
    CMutex wrongMtx;
    bool wrong = (idx % 4) != 0;
#endif
    int i=1;
    __TEST_OUTPUT(" consumer started");
    for (;;) {

#if USE_WRONG_MUTEX
        if (wrong) {
            wrongMtx.Lock();
        } else {
            m_BufferLock.Lock();
        }
#else
        m_BufferLock.Lock();
#endif

#if USE_INF_TIMEOUT
// with infinite timeout
        while (m_QueueSize == 0  &&  !m_StopRequested) {
            __TEST_OUTPUT(" consumer waiting for not empty");
            m_BufferNotEmpty.WaitForSignal( m_BufferLock);
            ++m_TotalWaitForNotEmpty;
        }
#else  /* USE_INF_TIMEOUT */
// with finite timeout
        bool timedout = false;
        do {
            timedout = false;
            CAbsTimeout tout(0, 100*1000*1000);
            while (m_QueueSize == 0 && !m_StopRequested) {

#  if USE_WRONG_MUTEX
                if (wrong) {
                    __TEST_OUTPUT(" consumer waiting for not empty WRONG");
                    if (!m_BufferNotEmpty.WaitForSignal( wrongMtx, tout)) {
                        break;
                    }
                } else {
                    __TEST_OUTPUT(" consumer waiting for not empty");
                    if (!m_BufferNotEmpty.WaitForSignal( m_BufferLock, tout)) {
                        break;
                    }
                }
#  else
                __TEST_OUTPUT(" consumer waiting for not empty");
                if (!m_BufferNotEmpty.WaitForSignal( m_BufferLock, tout)) {
                    // timeout
                    __TEST_OUTPUT(" consumer timed out;  will retry");
                    timedout = true;
                    ++m_TotalWaitTimeout;
                    break;
                }
#  endif
                ++m_TotalWaitForNotEmpty;
            }
        } while (timedout);
        if (m_QueueSize == 0) {
            __TEST_OUTPUT(" consumer retired");
            if (!m_StopRequested) {
                NcbiCout << idx << " consumer retired" << NcbiEndl;
                ++m_TotalRetired;
            }
#  if USE_WRONG_MUTEX
            if (wrong) {
                wrongMtx.Unlock();
            } else {
                m_BufferLock.Unlock();
            }
#  else
            m_BufferLock.Unlock();
#  endif
            return;
        }
#endif  /* USE_INF_TIMEOUT */

        if (m_StopRequested && m_QueueSize == 0) {
#if USE_WRONG_MUTEX
            if (wrong) {
                wrongMtx.Unlock();
            } else {
                m_BufferLock.Unlock();
            }
#else
            m_BufferLock.Unlock();
#endif
            break;
        }
#if !USE_FAST_MUTEX
        {
            SSystemMutex& sysmtx = m_BufferLock;
            assert(sysmtx.m_Count == 1);
            assert(sysmtx.m_Owner.Is( CThreadSystemID::GetCurrent()));
            assert(m_BufferLock.TryLock());
            assert(sysmtx.m_Count == 2);
            m_BufferLock.Unlock();
            assert(sysmtx.m_Count == 1);
        }
#endif

        size_t Item = m_Buffer[m_QueueStartOffset];
        i=Item;
        __TEST_OUTPUT(" consumed: " << Item);

        --m_QueueSize;
        ++m_QueueStartOffset;
        ++m_TotalItemsConsumed;
        if (m_QueueStartOffset == m_BufferSize) {
            m_QueueStartOffset = 0;
        }

        m_BufferNotFull.SignalSome();
#if USE_WRONG_MUTEX
        if (wrong) {
            wrongMtx.Unlock();
        } else {
            m_BufferLock.Unlock();
        }
#else
        m_BufferLock.Unlock();
#endif

//        SleepMilliSec( (10-(i%10))*5 );
        SleepMilliSec( rand() % 40 );
    }

    __TEST_OUTPUT(" consumer stopped");
}


bool CTestCondVarApp::Thread_Init(int idx)
{
    return true;
}


bool CTestCondVarApp::Thread_Run(int idx)
{
    if (idx == 0) {
        SleepMilliSec(10000);
        m_BufferLock.Lock();
        __TEST_OUTPUT(" Request STOP");
        m_StopRequested = true;
        m_BufferNotEmpty.SignalAll();
        m_BufferNotFull.SignalAll();
        m_BufferLock.Unlock();
    } else {
        if (idx%2) {
            Produce(idx);
        } else {
            Consume(idx);
        }
    }
    return true;
}


bool CTestCondVarApp::TestApp_Init(void)
{
//    s_NumThreads = 5;
//    s_NumThreads = 100;
    NcbiCout
        << NcbiEndl
        << "Testing condition variables with "
        << NStr::UIntToString(s_NumThreads)
        << " threads"
        << NcbiEndl;
    return true;
}


bool CTestCondVarApp::TestApp_Exit(void)
{
    NcbiCout
        << "Test completed"
        << NcbiEndl << " LastItemProduced = " << m_LastItemProduced
        << NcbiEndl << " QueueSize = "        << m_QueueSize
        << NcbiEndl << " TotalWaitForNotFull = " << m_TotalWaitForNotFull
        << NcbiEndl << " TotalWaitForNotEmpty = " << m_TotalWaitForNotEmpty
        << NcbiEndl << " TotalWaitTimeout = " << m_TotalWaitTimeout
        << NcbiEndl << " TotalRetired = " << m_TotalRetired
        << NcbiEndl;
    assert(m_QueueSize == 0);
    assert(m_TotalItemsProduced == m_TotalItemsConsumed);

    NcbiCout << "done" << NcbiEndl;
    return true;
}



/////////////////////////////////////////////////////////////////////////////
//  MAIN

int main(int argc, const char* argv[]) 
{
    if (!CConditionVariable::IsSupported()) {
        NcbiCout
            << NcbiEndl
            << "===================================================="
            << NcbiEndl
            << "Condition variable is NOT SUPPORTED on this platform"
            << NcbiEndl
            << "===================================================="
            << NcbiEndl;
            return 0;
    }
    // Execute main application function
    return CTestCondVarApp().AppMain(argc, argv, 0, eDS_Default, 0);
}
