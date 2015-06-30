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
 * Author:  Aleksey Grichenko
 *
 * File Description:
 *   Test for multithreading classes
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbithr.hpp>
#include <corelib/ncbimtx.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbidiag.hpp>
#include <corelib/ncbi_system.hpp>
#include <map>

#include <common/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;


//#define USE_NATIVE_THREADS


/////////////////////////////////////////////////////////////////////////////
// Globals


const int   cNumThreadsMin = 1;
const int   cNumThreadsMax = 500;
const int   cSpawnByMin    = 1;
const int   cSpawnByMax    = 100;
const int   cRCyclesMin    = 10;
const int   cRCyclesMax    = 5000;
const int   cWCyclesMin    = 5;
const int   cWCyclesMax    = 1000;

static int  sNumThreads    = 35;
static int  sSpawnBy       = 6;
static int  sRCycles       = 100;
static int  sWCycles       = 50;
static int  s_NextIndex    = 0;


DEFINE_STATIC_FAST_MUTEX(s_GlobalLock);


void delay(int value)
{
    for (int i=0; i<value; i++) {
        CFastMutexGuard out_guard(s_GlobalLock);
    }
}


void TestTlsCleanup(int* p_value, void* p_ref)
{
    // Copy current value, then delete it
    *static_cast<int*>(p_ref) = *p_value;
    delete p_value;
}


void Main_Thread_Tls_Cleanup(int* p_value, void* /* p_data */)
{
    (*p_value)++;
}



/////////////////////////////////////////////////////////////////////////////
// Shared resource imitation
//
//   Imitates reading & writing of a shared resource.
//   Checks if there are violations of read-write locks.
//

class CSharedResource
{
public:
    CSharedResource(void) : m_Readers(0),
                            m_Writer(-1) {}
    ~CSharedResource(void) { assert(!m_Readers); }
    void BeginRead(int ID);
    void BeginWrite(int ID);
    void EndRead(int ID);
    void EndWrite(int ID);
private:
    int m_Readers;
    int m_Writer;
    CFastMutex m_Mutex;
};


void CSharedResource::BeginRead(int ID)
{
    CFastMutexGuard guard(m_Mutex);
    // Must be unlocked, R-locked, or W-locked by the same thread
    assert(m_Readers >= 0 || (m_Readers < 0 && ID == m_Writer));
    (m_Readers >= 0) ? m_Readers++ : m_Readers--;
}


void CSharedResource::BeginWrite(int ID)
{
    CFastMutexGuard guard(m_Mutex);
    // Must be unlocked or W-locked by the same thread
    assert(m_Readers == 0 || (m_Readers < 0 && ID == m_Writer));
    m_Readers--;
    m_Writer = ID;
}


void CSharedResource::EndRead(int ID)
{
    CFastMutexGuard guard(m_Mutex);
    // Must be R-locked or W-locked by the same thread
    assert(m_Readers > 0 || (m_Readers < 0 && m_Writer == ID));
    (m_Readers > 0) ? m_Readers-- : m_Readers++;
    if (m_Readers == 0) {
        m_Writer = -1;
    }
}


void CSharedResource::EndWrite(int ID)
{
    CFastMutexGuard guard(m_Mutex);
    // Must be W-locked by the same thread;
    assert(m_Readers < 0 && m_Writer == ID);
    m_Readers++;
    if (m_Readers == 0) {
        m_Writer = -1;
    }
}


// Prevent threads from termination before Detach() or Join()
CMutex*             exit_locks[cNumThreadsMax];

template<int N>
struct SValue
{
    int value;
};

#define USE_STATIC_TLS
#ifdef USE_STATIC_TLS
# define TTls CStaticTls
#else
# define TTls CTls
#endif

template<class Value>
static TTls<Value>& s_GetTls()
{
#ifdef USE_STATIC_TLS
    static CStaticTls<Value> s_tls;
    return s_tls;
#else
    static CSafeStaticRef< TTls<Value> > s_tls;
    return s_tls.Get();
#endif
}

#if !defined(NCBI_COMPILER_WORKSHOP)  ||  NCBI_COMPILER_VERSION >= 590
static
#endif
void wait_threads(CAtomicCounter& counter)
{
    int add = 1;
    int wait_count = 0;
    while ( counter.Add(add) < CAtomicCounter::TValue(sNumThreads) ) {
        add = 0;
        if ( (++wait_count & 0xff) == 0 ) { NCBI_SCHED_YIELD(); }
    }
}

template<int N>
void test_static_tls(int idx, bool init = true)
{
    static CAtomicCounter thread_counter;
    if ( idx >= 0 && init ) {
        wait_threads(thread_counter);
    }
    idx += N*1000;
    typedef SValue<N> Value;
    TTls<Value>& tls = s_GetTls<Value>();
    if ( init ) {
        Value* v = new Value;
        v->value = idx;
        tls.SetValue(v);
    }
    assert(tls.GetValue()->value == idx);
}

template<int N, int CNT>
struct STestStaticTlss
{
    void do_test(int idx, bool init) const {
        STestStaticTlss<N, CNT/2> t1;
        t1.do_test(idx, init);
        STestStaticTlss<N+CNT/2, CNT-CNT/2> t2;
        t2.do_test(idx, init);
    }
};

template<int N>
struct STestStaticTlss<N, 1>
{
    void do_test(int idx, bool init) const {
        test_static_tls<N>(idx, init);
    }
};

template<int N>
struct STestStaticTlss<N, 0>
{
    void do_test(int, bool) const {
    }
};

template<int N, int CNT>
void test_static_tlss(int idx, bool init = true)
{
    STestStaticTlss<N, CNT> t;
    t.do_test(idx, init);
}



/////////////////////////////////////////////////////////////////////////////
// Test thread


#ifndef USE_NATIVE_THREADS
class CTestThread : public CThread
#else
class CTestThread : public CObject
#endif
{
public:
    CTestThread(int index, CTls<int>* tls, CRWLock* rw, CSharedResource* res);
    ~CTestThread(void);

protected:
    virtual void* Main(void);
    virtual void  OnExit(void);

private:
    int              m_Index;        // Thread sequential index
    CTls<int>*       m_Tls;          // Test TLS object
    int              m_CheckValue;   // Value to compare with the TLS data
    CRWLock*         m_RW;           // Test RWLock object
    CSharedResource* m_Res;         // Shared resource imitation

#ifdef USE_NATIVE_THREADS
    friend void NativeWrapperCallerImpl(TWrapperArg arg);

    CAtomicCounter_WithAutoInit m_IsFinished;
    CAtomicCounter_WithAutoInit m_IsValid;
    TThreadHandle    m_Handle;

public:
    bool Run(CThread::TRunMode flags = CThread::fRunDefault);
    void Join(void** result = 0);
    void Detach(void);
    void Discard(void);
#endif
};


// Thread states checked by the main thread
enum   TTestThreadState {
    eNull,          // Initial value
    eCreated,       // Set by CTestThread::CTestThread()
    eRunning,       // Set by CTestThread::Main()
    eTerminated,    // Set by CTestThread::OnExit()
    eDestroyed      // Set by CTestThread::~CTestThread()
};


// Pointers to all threads and corresponding states
CTestThread*        thr[cNumThreadsMax];
TTestThreadState    states[cNumThreadsMax];


CTestThread::CTestThread(int index,
                         CTls<int>* tls,
                         CRWLock* rw,
                         CSharedResource* res)
    : m_Index(index),
      m_Tls(tls),
      m_CheckValue(15),
      m_RW(rw),
      m_Res(res)
#ifdef USE_NATIVE_THREADS
      ,
      m_IsFinished(0),
      m_IsValid(0)
#endif
{
    CFastMutexGuard guard(s_GlobalLock);
    states[m_Index] = eCreated;
}


CTestThread::~CTestThread(void)
{
    CFastMutexGuard guard(s_GlobalLock);
    assert(m_CheckValue == 15);
    states[m_Index] = eDestroyed;
}

void CTestThread::OnExit(void)
{
    CFastMutexGuard guard(s_GlobalLock);
    states[m_Index] = eTerminated;
}


bool Test_CThreadExit(void)
{
    DEFINE_STATIC_FAST_MUTEX(s_Exit_Mutex);
    CFastMutexGuard guard(s_Exit_Mutex);
    // The mutex must be unlocked after call to CThread::Exit()
    try {
        CThread::Exit(reinterpret_cast<void*>(-1));
    }
    catch (...) {
        throw;
    }
    return false;   // this line should never be executed
}


void* CTestThread::Main(void)
{
    {{
        CFastMutexGuard guard(s_GlobalLock);
        states[m_Index] = eRunning;
    }}

    // ======= CTls test =======
    // Verify TLS - initial value must be 0
    m_CheckValue = 0;
    assert(m_Tls->GetValue() == 0);
    int* stored_value = new int;
    assert(stored_value != 0);
    *stored_value = 0;
    m_Tls->SetValue(stored_value, TestTlsCleanup, &m_CheckValue);
    for (int i=0; i<5; i++) {
        stored_value = new int;
        assert(stored_value != 0);
        *stored_value = *m_Tls->GetValue()+1;
        m_Tls->SetValue(stored_value,
                        TestTlsCleanup, &m_CheckValue);
        assert(*stored_value == m_CheckValue+1);
    }

    // ======= CThread test =======
    for (int i = 0; i < sSpawnBy; i++) {
        int idx;  /* NCBI_FAKE_WARNING: GCC */
        {{
            CFastMutexGuard spawn_guard(s_GlobalLock);
            if (s_NextIndex >= sNumThreads) {
                break;
            }
            idx = s_NextIndex;
            s_NextIndex++;
        }}
        thr[idx] = new CTestThread(idx, m_Tls, m_RW, m_Res);
        assert(states[idx] == eCreated);
        thr[idx]->Run();
        {{
            CFastMutexGuard guard(s_GlobalLock);
            NcbiCout << idx << " ";
        }}
    }

    // ======= CTls test =======
    // Verify TLS - current value must be 5
    assert(*m_Tls->GetValue() == 5);
    for (int i=0; i<5; i++) {
        stored_value = new int;
        assert(stored_value != 0);
        *stored_value = *m_Tls->GetValue()+1;
        m_Tls->SetValue(stored_value,
                        TestTlsCleanup, &m_CheckValue);
        assert(*stored_value == m_CheckValue+1);
    }

    // ======= CStaticTls test =======
    {
        m_RW->ReadLock();
        test_static_tlss<1, 10>(m_Index);
        test_static_tlss<1, 10>(m_Index, false);
        m_RW->Unlock();
    }

    // ======= CRWLock test =======
    static int s_var = 0; // global value
    int lock_success = 0;
    int lock_fail = 0;
    if (m_Index % 5) {         // <------ Reader
        for (int r_cycle=0; r_cycle<sRCycles*2; r_cycle++) {
            // Allow writers to obtain lock.
            delay(100);
            // Lock immediately or wait for locking
            bool locked = false;
            if ( r_cycle % 3 ) {
                locked = m_RW->TryReadLock();
            }
            else {
                locked = m_RW->TryReadLock(CTimeout(0, 3000));
                if ( locked ) {
                    lock_success++;
                }
                else {
                    lock_fail++;
                }
            }
            if ( !locked ) {
                m_RW->ReadLock();
            }

            int l_var = s_var;    // must remain the same while R-locked
            
            m_Res->BeginRead(m_Index);
            assert(l_var == s_var);

            // Cascaded R-lock
            if (r_cycle % 6 == 0) {
                m_RW->ReadLock();
                m_Res->BeginRead(m_Index);
            }
            assert(l_var == s_var);

            // Cascaded R-lock must be allowed
            if (r_cycle % 12 == 0) {
                assert(m_RW->TryReadLock());
                m_Res->BeginRead(m_Index);
                m_Res->EndRead(m_Index);
                m_RW->Unlock();
	        }

            // W-after-R must be prohibited
            assert( !m_RW->TryWriteLock() );

            delay(10);

            // ======= CTls test =======
            // Verify TLS - current value must be 10
            assert(*m_Tls->GetValue() == 10);

            assert(l_var == s_var);

            // Cascaded R-lock must be allowed
            if (r_cycle % 7 == 0) {
                assert(m_RW->TryReadLock());
                m_Res->BeginRead(m_Index);
                m_Res->EndRead(m_Index);
                m_RW->Unlock();
            }
            assert(l_var == s_var);

            if (r_cycle % 6 == 0) {
                m_Res->EndRead(m_Index);
                m_RW->Unlock();
            }
            assert(l_var == s_var);

            m_Res->EndRead(m_Index);
            m_RW->Unlock();
        }
    }
    else {                     // <------ Writer
        for (int w_cycle=0; w_cycle<sWCycles; w_cycle++) {
            // Lock immediately or wait for locking
            bool locked = false;
            if ( w_cycle % 3 ) {
                locked = m_RW->TryWriteLock();
            }
            else {
                locked = m_RW->TryWriteLock(CTimeout(0, 3000));
                if ( locked ) {
                    lock_success++;
                }
                else {
                    lock_fail++;
                }
            }
            if ( !locked ) {
                m_RW->WriteLock();
            }
            m_Res->BeginWrite(m_Index);
            // Allow other threads to try-lock with timeout.
            SleepMilliSec(10);

            // Cascaded W-lock
            if (w_cycle % 4 == 0) {
                m_RW->WriteLock();
                m_Res->BeginWrite(m_Index);
            }

            // R-after-W (treated as cascaded W-lock)
            if (w_cycle % 6 == 0) {
                m_RW->ReadLock();
                m_Res->BeginRead(m_Index);
            }

            // Cascaded W-lock must be allowed
            if (w_cycle % 8 == 0) {
                assert(m_RW->TryWriteLock());
                m_Res->BeginWrite(m_Index);
            }

            // Cascaded R-lock must be allowed
            if (w_cycle % 10 == 0) {
                assert(m_RW->TryReadLock());
                m_Res->BeginRead(m_Index);
            }

            // ======= CTls test =======
            // Verify TLS - current value must be 10
            assert(*m_Tls->GetValue() == 10);

            // Continue CRWLock test
            for (int i=0; i<7; i++) {
                delay(1);
                s_var++;
            }

            if (w_cycle % 4 == 0) {
                m_Res->EndWrite(m_Index);
                m_RW->Unlock();
            }
            if (w_cycle % 6 == 0) {
                m_Res->EndRead(m_Index);
                m_RW->Unlock();
            }
            if (w_cycle % 8 == 0) {
                m_Res->EndWrite(m_Index);
                m_RW->Unlock();
            }
            if (w_cycle % 10 == 0) {
                m_Res->EndRead(m_Index);
                m_RW->Unlock();
            }
            m_Res->EndWrite(m_Index);
            m_RW->Unlock();
            delay(1);
        }
    }

    // ======= CTls test =======
    // Verify TLS - current value must be 10
    assert(*m_Tls->GetValue() == 10);
    for (int i=0; i<5; i++) {
        stored_value = new int;
        assert(stored_value != 0);
        *stored_value = *m_Tls->GetValue()+1;
        m_Tls->SetValue(stored_value,
                        TestTlsCleanup, &m_CheckValue);
        assert(*stored_value == m_CheckValue+1);
    }
    assert(*m_Tls->GetValue() == 15);
    assert(m_CheckValue == 14);

    // ======= CThread::Detach() and CThread::Join() test =======
    if (m_Index % 2 == 0) {
        CMutexGuard exit_guard(*exit_locks[m_Index]);
        delay(10); // Provide delay for join-before-exit
    }

#ifndef USE_NATIVE_THREADS
    if (m_Index % 3 == 0)
    {
        // Never verified, since CThread::Exit() terminates the thread
        // inside Test_CThreadExit().
        assert(Test_CThreadExit());
    }
#endif

    return reinterpret_cast<void*>(-1);
}


/////////////////////////////////////////////////////////////////////////////
// Use native threads rather than CThread to run the tests.

#ifdef USE_NATIVE_THREADS

void CTestThread::Join(void** result)
{
    // Join (wait for) and destroy
    bool valid = m_IsValid.Add(-1) == 0;
    if (valid) {
#if defined(NCBI_WIN32_THREADS)
        _ASSERT(WaitForSingleObject(m_Handle, INFINITE) == WAIT_OBJECT_0);
        DWORD status;
        _ASSERT(GetExitCodeThread(m_Handle, &status));
        _ASSERT(status != DWORD(STILL_ACTIVE));
        _ASSERT(CloseHandle(m_Handle));
#elif defined(NCBI_POSIX_THREADS)
        _ASSERT(pthread_join(m_Handle, 0) == 0);
#endif
    }
    if (result) {
        *result = this;
    }
    delete this;
}


void CTestThread::Detach(void)
{
    // The second who increments this can delete the object
    if (m_IsFinished.Add(1) == 2) {
        delete this;
        return;
    }
    bool valid = m_IsValid.Add(-1) == 0;
    if (valid) {
#if defined(NCBI_WIN32_THREADS)
        _ASSERT(CloseHandle(m_Handle));
#elif defined(NCBI_POSIX_THREADS)
        _ASSERT(pthread_detach(m_Handle) == 0);
#endif
    }
}


void CTestThread::Discard(void)
{
    delete this;
    return;
}


void NativeWrapperCallerImpl(TWrapperArg arg)
{
    CTestThread* thread_obj = static_cast<CTestThread*>(arg);
    thread_obj->Main();
    thread_obj->OnExit();
    CUsedTlsBases::GetUsedTlsBases().ClearAll();
    // The first who increments this can delete the object
    if (thread_obj->m_IsFinished.Add(1) == 2) {
        delete thread_obj;
    }
}


#if defined(NCBI_WIN32_THREADS)
extern "C" {
    typedef TWrapperRes (WINAPI *FSystemWrapper)(TWrapperArg);

    static TWrapperRes WINAPI NativeWrapperCaller(TWrapperArg arg) {
        NativeWrapperCallerImpl(arg);
        return 0;
    }
}
#elif defined(NCBI_POSIX_THREADS)
extern "C" {
    typedef TWrapperRes (*FSystemWrapper)(TWrapperArg);

    static TWrapperRes NativeWrapperCaller(TWrapperArg arg) {
        NativeWrapperCallerImpl(arg);
        return 0;
    }
}
#endif


bool CTestThread::Run(CThread::TRunMode flags)
{
    // Run as the platform native thread rather than CThread
    // Not all functionality will work in this mode. E.g. TLS
    // cleanup can not be done automatically.
#if defined(NCBI_WIN32_THREADS)
    // We need this parameter in WinNT - can not use NULL instead!
    DWORD thread_id;
    // Suspend thread to adjust its priority
    DWORD creation_flags = (flags & CThread::fRunNice) == 0 ? 0 : CREATE_SUSPENDED;
    m_Handle = CreateThread(NULL, 0, NativeWrapperCaller,
        this, creation_flags, &thread_id);
    _ASSERT(m_Handle != NULL);
    m_IsValid.Set(1);
    if (flags & CThread::fRunNice) {
        // Adjust priority and resume the thread
        SetThreadPriority(m_Handle, THREAD_PRIORITY_BELOW_NORMAL);
        ResumeThread(m_Handle);
    }
    if ((flags & CThread::fRunDetached) != 0) {
        CloseHandle(m_Handle);
        m_IsValid.Set(0);
    }
    else {
        // duplicate handle to adjust security attributes
        HANDLE oldHandle = m_Handle;
        _ASSERT(DuplicateHandle(GetCurrentProcess(), oldHandle,
            GetCurrentProcess(), &m_Handle,
            0, FALSE, DUPLICATE_SAME_ACCESS));
        _ASSERT(CloseHandle(oldHandle));
    }
#elif defined(NCBI_POSIX_THREADS)
        pthread_attr_t attr;
        _ASSERT(pthread_attr_init (&attr) == 0);
        if ( ! (flags & CThread::fRunUnbound) ) {
#if defined(NCBI_OS_BSD)  ||  defined(NCBI_OS_CYGWIN)  ||  defined(NCBI_OS_IRIX)
            _ASSERT(pthread_attr_setscope(&attr,
                                          PTHREAD_SCOPE_PROCESS) == 0);
#else
            _ASSERT(pthread_attr_setscope(&attr,
                                          PTHREAD_SCOPE_SYSTEM) == 0);
#endif
        }
        if ( flags & CThread::fRunDetached ) {
            _ASSERT(pthread_attr_setdetachstate(&attr,
                                                PTHREAD_CREATE_DETACHED) == 0);
        }
        _ASSERT(pthread_create(&m_Handle, &attr,
                               NativeWrapperCaller, this) == 0);

        _ASSERT(pthread_attr_destroy(&attr) == 0);
        m_IsValid.Set(1);
#else
        if (flags & fRunAllowST) {
            Wrapper(this);
        }
        else {
            _ASSERT(0);
        }
#endif
    return true;
}


/////////////////////////////////////////////////////////////////////////////
//  Test application

#endif // USE_NATIVE_THREADS


class CThreadedApp : public CNcbiApplication
{
    void Init(void);
    int Run(void);
};


void CThreadedApp::Init(void)
{
    // Prepare command line descriptions
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // sNumThreads
    arg_desc->AddDefaultKey
        ("threads", "NumThreads",
         "Total number of threads to create and run",
         CArgDescriptions::eInteger, NStr::IntToString(sNumThreads));
    arg_desc->SetConstraint
        ("threads", new CArgAllow_Integers(cNumThreadsMin, cNumThreadsMax));

    // sSpawnBy
    arg_desc->AddDefaultKey
        ("spawnby", "SpawnBy",
         "Threads spawning factor",
         CArgDescriptions::eInteger, NStr::IntToString(sSpawnBy));
    arg_desc->SetConstraint
        ("spawnby", new CArgAllow_Integers(cSpawnByMin, cSpawnByMax));

    // sNumRCycles
    arg_desc->AddDefaultKey
        ("rcycles", "RCycles",
         "Number of read cycles by each reader thread",
         CArgDescriptions::eInteger, NStr::IntToString(sRCycles));
    arg_desc->SetConstraint
        ("rcycles", new CArgAllow_Integers(cRCyclesMin, cRCyclesMax));

    // sNumWCycles
    arg_desc->AddDefaultKey
        ("wcycles", "WCycles",
         "Number of write cycles by each writer thread",
         CArgDescriptions::eInteger, NStr::IntToString(sWCycles));
    arg_desc->SetConstraint
        ("wcycles", new CArgAllow_Integers(cWCyclesMin, cWCyclesMax));

    arg_desc->AddFlag("favorwriters",
                      "Operate the RW-lock in fFavorWriters mode.");

    string prog_description =
        "This is a program testing thread, TLS, mutex and RW-lock classes.";
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              prog_description, false);

    SetupArgDescriptions(arg_desc.release());
}


int CThreadedApp::Run(void)
{
# if defined(NCBI_POSIX_THREADS)
    NcbiCout << "OS: Unix" << NcbiEndl;
# elif defined(NCBI_WIN32_THREADS)
    NcbiCout << "OS: MS-Windows" << NcbiEndl;
# else
    NcbiCout << "OS: unknown" << NcbiEndl;
# endif

    // Process command line
    const CArgs& args = GetArgs();

    sNumThreads = args["threads"].AsInteger();
    sSpawnBy    = args["spawnby"].AsInteger();
    sRCycles    = args["rcycles"].AsInteger();
    sWCycles    = args["wcycles"].AsInteger();

    CRWLock::TFlags rwflags = 0;
    if (args["favorwriters"]) {
        rwflags |= CRWLock::fFavorWriters;
    }

    NcbiCout << "Test parameters:" << NcbiEndl;
    NcbiCout << "\tTotal threads     " << sNumThreads << NcbiEndl;
    NcbiCout << "\tSpawn threads by  " << sSpawnBy << NcbiEndl;
    NcbiCout << "\tR-cycles          " << sRCycles << NcbiEndl;
    NcbiCout << "\tW-cycles          " << sWCycles << NcbiEndl;
    NcbiCout << "\tRW-lock flags     0x" << NStr::IntToString(rwflags, 0, 16)
             << NcbiEndl;
#ifdef USE_NATIVE_THREADS
    NcbiCout << "\tUsing native threads" << NcbiEndl;
#endif
    NcbiCout << NcbiEndl;

    // Redirect error log to hide messages sent by delay()
    //SetDiagStream(0);

    // Test CBaseTls::Discard()
    NcbiCout << "Creating/discarding TLS test...";
    int main_cleanup_flag = 0;
    CTls<int>* dummy_tls = new CTls<int>;
    assert(main_cleanup_flag == 0);
    dummy_tls->SetValue(&main_cleanup_flag, Main_Thread_Tls_Cleanup);
    assert(main_cleanup_flag == 0);
    dummy_tls->Discard();
    assert(main_cleanup_flag == 1);
    NcbiCout << " Passed" << NcbiEndl << NcbiEndl;

    // Create test objects
    main_cleanup_flag = 0;
    CRef< CTls<int> > tls(new CTls<int>);
    tls->SetValue(0);
    CRWLock rw(rwflags);
    CSharedResource res;
    {{
        CFastMutexGuard guard(s_GlobalLock);
        NcbiCout << "===== Starting threads =====" << NcbiEndl;
    }}

    // Prepare exit-locks, lock each 2nd mutex
    // They will be used by join/detach before exit
    for (int i=0; i<sNumThreads; i++) {
        states[i] = eNull;
        exit_locks[i] = new CMutex;
        if (i % 2 == 0) {
            exit_locks[i]->Lock();
        }
    }

    // Prepare stop with RWLock
    rw.WriteLock();
    if ( 0 ) { // pre-initialize static tlss
        test_static_tlss<1, 10>(-1);
        test_static_tlss<1, 10>(-1, false);
    }
    
    // Create and run threads
    for (int i = 0; i < sSpawnBy;  i++) {
        int idx;  /* NCBI_FAKE_WARNING: GCC */
        {{
            CFastMutexGuard spawn_guard(s_GlobalLock);
            if (s_NextIndex >= sNumThreads) {
                break;
            }
            idx = s_NextIndex;
            s_NextIndex++;
        }}

        // Check Discard() for some threads
        if (i % 2 == 0) {
            thr[idx] = new CTestThread(idx, tls, &rw, &res);
            assert(states[idx] == eCreated);
            thr[idx]->Discard();
            assert(states[idx] == eDestroyed);
        }
        
        thr[idx] = new CTestThread(idx, tls, &rw, &res);
        assert(states[idx] == eCreated);
        thr[idx]->Run();
        {{
            CFastMutexGuard guard(s_GlobalLock);
            NcbiCout << idx << " ";
        }}
    }

    // Wait for all threads running
    {{
        CFastMutexGuard guard(s_GlobalLock);
        NcbiCout << NcbiEndl << NcbiEndl <<
                    "===== Waiting for all threads to run =====" << NcbiEndl;
    }}
    map<int, bool> thread_map;
    for (int i=0; i<sNumThreads; i++) {
        thread_map[i] = false;
    }
    int ready = 0;
    while (ready < sNumThreads) {
        ready = 0;
        for (int i=0; i<sNumThreads; i++) {
            if (states[i] >= eRunning) {
                ready++;
                if ( !thread_map[i] ) {
                    CFastMutexGuard guard(s_GlobalLock);
                    NcbiCout << i << " ";
                    thread_map[i] = true;
                }
            }
        }
    }

    // pause and release RWLock
    {{
        CFastMutexGuard guard(s_GlobalLock);
        NcbiCout << NcbiEndl << NcbiEndl <<
                    "===== Releasing threads run after a pause ====="
                 << NcbiEndl;
    }}
    SleepSec(1);
    rw.Unlock();

    {{
        CFastMutexGuard guard(s_GlobalLock);
        NcbiCout << NcbiEndl << NcbiEndl <<
                    "===== Joining threads before exit =====" << NcbiEndl;
    }}
    for (int i=0; i<sNumThreads; i++) {
        // Try to join before exit
        if (i % 4 == 2) {
            assert(states[i] < eTerminated);
            exit_locks[i]->Unlock();
            void* exit_data = 0;
            thr[i]->Join(&exit_data);
            // Must be set to 1 by Main()
            assert(exit_data != 0);
            {{
                CFastMutexGuard guard(s_GlobalLock);
                NcbiCout << i << " ";
            }}
        }
    }

    // Reset CRef to the test tls. One more CRef to the object
    // must have been stored in the CThread's m_UsedTlsSet.
    assert(main_cleanup_flag == 0);
    tls.Reset();
    assert(main_cleanup_flag == 0);

    {{
        CFastMutexGuard guard(s_GlobalLock);
        NcbiCout << NcbiEndl << NcbiEndl <<
                    "===== Detaching threads before exit =====" << NcbiEndl;
    }}
    for (int i=0; i<sNumThreads; i++) {
        // Detach before exit
        if (i % 4 == 0) {
            assert(states[i] < eTerminated);
            thr[i]->Detach();
            exit_locks[i]->Unlock();
            {{
                CFastMutexGuard guard(s_GlobalLock);
                NcbiCout << i << " ";
            }}
        }
    }

    // Wait for all threads to exit
    delay(10);

    {{
        CFastMutexGuard guard(s_GlobalLock);
        NcbiCout << NcbiEndl << NcbiEndl <<
                    "===== Detaching threads after exit =====" << NcbiEndl;
    }}
    for (int i=0; i<sNumThreads; i++) {
        // Detach after exit
        if (i % 4 == 1) {
            assert(states[i] != eDestroyed);
            thr[i]->Detach();
            {{
                CFastMutexGuard guard(s_GlobalLock);
                NcbiCout << i << " ";
            }}
        }
    }

    {{
        CFastMutexGuard guard(s_GlobalLock);
        NcbiCout << NcbiEndl << NcbiEndl <<
                    "===== Joining threads after exit =====" << NcbiEndl;
    }}
    for (int i=0; i<sNumThreads; i++) {
        // Join after exit
        if (i % 4 == 3) {
            assert(states[i] != eDestroyed);
            thr[i]->Join();
            {{
                CFastMutexGuard guard(s_GlobalLock);
                NcbiCout << i << " ";
            }}
        }
    }

    // Wait for all threads to be destroyed
    {{
        CFastMutexGuard guard(s_GlobalLock);
        NcbiCout << NcbiEndl << NcbiEndl <<
                    "===== Waiting for all threads to be destroyed =====" << NcbiEndl;
    }}
    for (int i=0; i<sNumThreads; i++) {
        thread_map[i] = false;
    }
    ready = 0;
    while (ready < sNumThreads) {
        ready = 0;
        for (int i=0; i<sNumThreads; i++) {
            if (states[i] == eDestroyed) {
                ready++;
                if ( !thread_map[i] ) {
                    CFastMutexGuard guard(s_GlobalLock);
                    NcbiCout << i << " ";
                    thread_map[i] = true;
                    delete exit_locks[i];
                }
            }
            else {
                // Provide a possibility for the child thread to run
                CMutexGuard exit_guard(*exit_locks[i]);
            }
        }
    }

    {{
        CFastMutexGuard guard(s_GlobalLock);
        NcbiCout << NcbiEndl << NcbiEndl << "Test passed" << NcbiEndl;
    }}

    return 0;
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN

int main(int argc, const char* argv[]) 
{
    CThread::InitializeMainThreadId();
    return CThreadedApp().AppMain(argc, argv);
}
