/* $Id$
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
 *   Wrapper for testing modules in MT environment
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/test_mt.hpp>
#include <corelib/ncbimtx.hpp>
#include <corelib/ncbi_system.hpp>
#include <corelib/ncbi_param.hpp>
#include <math.h>
#include <common/test_assert.h>  /* This header must go last */


BEGIN_NCBI_SCOPE

// Uncomment the definition to use platform native threads rather
// than CThread.
//#define USE_NATIVE_THREADS

DEFINE_STATIC_FAST_MUTEX(s_GlobalLock);
static CThreadedApp* s_Application;

// Default values
unsigned int  s_NumThreads = 34;
int           s_SpawnBy    = 6;

// Next test thread index
static volatile unsigned int s_NextIndex = 0;

#define TESTAPP_LOG_POST(x)  do { ++m_LogMsgCount; ERR_POST(x); } while (0)

/////////////////////////////////////////////////////////////////////////////
// Randomization parameters

// if (rand() % 100 < threshold) then use cascading threads
NCBI_PARAM_DECL(unsigned int, TEST_MT, Cascading);
NCBI_PARAM_DEF(unsigned int, TEST_MT, Cascading, 25);

// calculate # of thread groups
static string s_GroupsCount(void)
{
    return NStr::UIntToString( (unsigned int)sqrt((double)s_NumThreads));
}
NCBI_PARAM_DECL(string, TEST_MT, GroupsCount);
NCBI_PARAM_DEF_WITH_INIT(string, TEST_MT, GroupsCount, kEmptyStr, s_GroupsCount);

// group.has_sync_point = (rand() % 100) < threshold;
NCBI_PARAM_DECL(unsigned int, TEST_MT, IntragroupSyncPoint);
NCBI_PARAM_DEF(unsigned int, TEST_MT, IntragroupSyncPoint, 75);

/////////////////////////////////////////////////////////////////////////////
// Test thread
//

class CTestThread : public CThread
{
public:
    static void StartCascadingThreads(void);

    CTestThread(int id);
    virtual void SyncPoint(void) {};
    virtual void GlobalSyncPoint(void);

protected:
    ~CTestThread(void);
    virtual void* Main(void);
    virtual void  OnExit(void);

    int m_Idx;

#ifdef USE_NATIVE_THREADS
    TThreadHandle m_Handle;

public:
    void RunNative(void);
    void JoinNative(void** result);

    friend TWrapperRes NativeWrapper(TWrapperArg arg);
#endif
};


static CSemaphore           s_Semaphore(0, INT_MAX); /* For GlobalSyncPoint()*/
static CAtomicCounter       s_SyncCounter;           /* For GlobalSyncPoint()*/
static CAtomicCounter       s_NumberOfThreads;       /* For GlobalSyncPoint()*/


CTestThread::CTestThread(int idx)
    : m_Idx(idx)
{
    /* We want to know total number of threads, and the easiest way is to make
       them register themselves */
    s_NumberOfThreads.Add(1);
    if ( s_Application != 0 )
        assert(s_Application->Thread_Init(m_Idx));
}


CTestThread::~CTestThread(void)
{
    s_NumberOfThreads.Add(-1);
    assert(s_NumberOfThreads.Get() >= 0);
    if ( s_Application != 0 )
        assert(s_Application->Thread_Destroy(m_Idx));
}


void CTestThread::OnExit(void)
{
    if ( s_Application != 0 )
        assert(s_Application->Thread_Exit(m_Idx));
}


void CTestThread::GlobalSyncPoint(void)
{
    /* Semaphore is supposed to have zero value when threads come here,
       so Wait() causes stop */
    if (s_SyncCounter.Add(1) != s_NumberOfThreads.Get()) {
        s_Semaphore.Wait();
        return;
    }
    /* If we are the last thread to come to sync point, we yield 
       so that threads that were waiting for us go first      */
    if (s_NumberOfThreads.Get() > 1) {
        s_Semaphore.Post(s_NumberOfThreads.Get() - 1);
        SleepMilliSec(0);
    }
}


#ifdef USE_NATIVE_THREADS

TWrapperRes NativeWrapper(TWrapperArg arg)
{
    CTestThread* thread_obj = static_cast<CTestThread*>(arg);
    thread_obj->Main();
    return 0;
}


#if defined(NCBI_POSIX_THREADS)
extern "C" {
    typedef TWrapperRes (*FSystemWrapper)(TWrapperArg);

    static TWrapperRes NativeWrapperCaller(TWrapperArg arg) {
        return NativeWrapper(arg);
    }
}
#elif defined(NCBI_WIN32_THREADS)
extern "C" {
    typedef TWrapperRes (WINAPI *FSystemWrapper)(TWrapperArg);

    static TWrapperRes WINAPI NativeWrapperCaller(TWrapperArg arg) {
        return NativeWrapper(arg);
    }
}
#endif


void CTestThread::RunNative(void)
{
    // Run as the platform native thread rather than CThread
    // Not all functionality will work in this mode. E.g. TLS
    // cleanup can not be done automatically.
#if defined(NCBI_WIN32_THREADS)
    // We need this parameter on WinNT - cannot use NULL instead!
    DWORD thread_id;
    // Suspend thread to adjust its priority
    DWORD creation_flags = 0;
    m_Handle = CreateThread(NULL, 0, NativeWrapperCaller,
                            this, creation_flags, &thread_id);
    _ASSERT(m_Handle != NULL);
    // duplicate handle to adjust security attributes
    HANDLE oldHandle = m_Handle;
    _ASSERT(DuplicateHandle(GetCurrentProcess(), oldHandle,
                            GetCurrentProcess(), &m_Handle,
                            0, FALSE, DUPLICATE_SAME_ACCESS));
    _ASSERT(CloseHandle(oldHandle));
#elif defined(NCBI_POSIX_THREADS)
    pthread_attr_t attr;
    _ASSERT(pthread_attr_init(&attr) == 0);
    _ASSERT(pthread_create(&m_Handle, &attr,
                           NativeWrapperCaller, this) == 0);
    _ASSERT(pthread_attr_destroy(&attr) == 0);
#else
    if (flags & fRunAllowST) {
        Wrapper(this);
    }
    else {
        _ASSERT(0);
    }
#endif
}


void CTestThread::JoinNative(void** result)
{
    // Join (wait for) and destroy
#if defined(NCBI_WIN32_THREADS)
    _ASSERT(WaitForSingleObject(m_Handle, INFINITE) == WAIT_OBJECT_0);
    DWORD status;
    _ASSERT(GetExitCodeThread(m_Handle, &status)
            &&  status != DWORD(STILL_ACTIVE));
    _ASSERT(CloseHandle(m_Handle));
    m_Handle = NULL;
#elif defined(NCBI_POSIX_THREADS)
    _ASSERT(pthread_join(m_Handle, 0) == 0);
#endif
    *result = this;
}

#endif // USE_NATIVE_THREADS


CRef<CTestThread> thr[k_NumThreadsMax];

void CTestThread::StartCascadingThreads(void)
{
    int spawn_max;
    int first_idx;
    {{
        CFastMutexGuard spawn_guard(s_GlobalLock);
        spawn_max = s_NumThreads - s_NextIndex;
        if (spawn_max > s_SpawnBy) {
            spawn_max = s_SpawnBy;
        }
        first_idx = s_NextIndex;
        s_NextIndex += s_SpawnBy;
    }}
    // Spawn more threads
    for (int i = first_idx;  i < first_idx + spawn_max;  i++) {
        thr[i] = new CTestThread(i);
        // Allow threads to run even in single thread environment
#ifdef USE_NATIVE_THREADS
        thr[i]->RunNative();
#else
        thr[i]->Run(CThread::fRunAllowST);
#endif
    }
}

void* CTestThread::Main(void)
{
    StartCascadingThreads();
    // Run the test
    if ( s_Application != 0  &&  s_Application->Thread_Run(m_Idx) ) {
        return this;
    }

    return 0;
}

/////////////////////////////////////////////////////////////////////////////
//  Thread group

class CThreadGroup;
class CInGroupThread : public CTestThread
{
public:
    CInGroupThread(CThreadGroup& group, int id);
    virtual void SyncPoint(void);

protected:
    ~CInGroupThread(void);
    virtual void* Main(void);
    CThreadGroup& m_Group;
};

class CThreadGroup : public CObject
{
public:
    CThreadGroup(
        unsigned int number_of_threads,
        bool has_sync_point);
    ~CThreadGroup(void);

    void Go(void);
    void SyncPoint(void);

    void ThreadWait(void);
    void ThreadComplete(void);

private:
    unsigned int m_Number_of_threads;
    bool m_Has_sync_point;
    CSemaphore m_Semaphore;
    CFastMutex m_Mutex;
    unsigned int m_SyncCounter;
};


static CRef<CThreadGroup>   thr_group[k_NumThreadsMax];
static CStaticTls<int>      s_ThreadIdxTLS;

CInGroupThread::CInGroupThread(CThreadGroup& group, int id)
    : CTestThread(id), m_Group(group)
{
}

CInGroupThread::~CInGroupThread(void)
{
}

void CInGroupThread::SyncPoint(void)
{
    m_Group.SyncPoint();
}


void* CInGroupThread::Main(void)
{
    m_Group.ThreadWait();
    s_ThreadIdxTLS.SetValue(reinterpret_cast<int*>(m_Idx));
    // Run the test
    if ( s_Application != 0  &&  s_Application->Thread_Run(m_Idx) ) {
        m_Group.ThreadComplete();
        return this;
    }
    return 0;
}

CThreadGroup::CThreadGroup(
        unsigned int number_of_threads,
        bool has_sync_point)
    : m_Number_of_threads(number_of_threads), m_Has_sync_point(has_sync_point),
      m_Semaphore(0,number_of_threads), m_SyncCounter(0)
{
    for (unsigned int t = 0;  t < m_Number_of_threads;  ++t) {
        thr[s_NextIndex] = new CInGroupThread(*this, s_NextIndex);
#ifdef USE_NATIVE_THREADS
        thr[s_NextIndex]->RunNative();
#else
        thr[s_NextIndex]->Run();
#endif
        ++s_NextIndex;
    }
}

CThreadGroup::~CThreadGroup(void)
{
}

inline
void CThreadGroup::Go(void)
{
    s_NumberOfThreads.Add(m_Number_of_threads);
    m_Semaphore.Post(m_Number_of_threads);
}

void CThreadGroup::SyncPoint(void)
{
    if (m_Has_sync_point) {
        bool reached = false;
        m_Mutex.Lock();
        ++m_SyncCounter;
        if (m_SyncCounter == m_Number_of_threads) {
            m_SyncCounter = 0;
            reached = true;
        }
        m_Mutex.Unlock();
        if (reached) {
            if (m_Number_of_threads > 1) {
                m_Semaphore.Post(m_Number_of_threads-1);
                SleepMilliSec(0);
            }
        } else {
            m_Semaphore.Wait();
        }
    }
}


inline
void CThreadGroup::ThreadWait(void)
{
    s_NumberOfThreads.Add(-1);
    assert(s_NumberOfThreads.Get() >= 0);
    m_Semaphore.Wait();
}

inline
void CThreadGroup::ThreadComplete(void)
{
    if (m_Has_sync_point) {
        m_Semaphore.Post();
    }
}


/////////////////////////////////////////////////////////////////////////////
//  Test application


CThreadedApp::CThreadedApp(void)
{
    m_Min = m_Max = 0;
    m_NextGroup   = 0;
    m_LogMsgCount = 0;
    s_Application = this;
    CThread::InitializeMainThreadId();
}


CThreadedApp::~CThreadedApp(void)
{
    s_Application = 0;
}


void CThreadedApp::Init(void)
{
    // Prepare command line descriptions
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // s_NumThreads
    arg_desc->AddDefaultKey
        ("threads", "NumThreads",
         "Total number of threads to create and run",
         CArgDescriptions::eInteger, NStr::IntToString(s_NumThreads));
    arg_desc->SetConstraint
        ("threads", new CArgAllow_Integers(k_NumThreadsMin, k_NumThreadsMax));

    // s_NumThreads (emulation in ST)
    arg_desc->AddDefaultKey
        ("repeats", "NumRepeats",
         "In non-MT mode only(!) -- how many times to repeat the test. "
         "If passed 0, then the value of argument `-threads' will be used.",
         CArgDescriptions::eInteger, "0");
    arg_desc->SetConstraint
        ("repeats", new CArgAllow_Integers(0, k_NumThreadsMax));

    // s_SpawnBy
    arg_desc->AddDefaultKey
        ("spawnby", "SpawnBy",
         "Threads spawning factor",
         CArgDescriptions::eInteger, NStr::IntToString(s_SpawnBy));
    arg_desc->SetConstraint
        ("spawnby", new CArgAllow_Integers(k_SpawnByMin, k_SpawnByMax));

    arg_desc->AddOptionalKey("seed", "Randomization",
                             "Randomization seed value",
                             CArgDescriptions::eInteger);

    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "MT-environment test");

    // Let test application add its own arguments
    TestApp_Args(*arg_desc);

    SetupArgDescriptions(arg_desc.release());
}


int CThreadedApp::Run(void)
{
    // Process command line
    const CArgs& args = GetArgs();

#if !defined(NCBI_THREADS)
    s_NumThreads = args["repeats"].AsInteger();
    if ( !s_NumThreads )
#endif
        s_NumThreads = args["threads"].AsInteger();

#if !defined(NCBI_THREADS)
    // Set reasonable repeats if not set through the argument
    if (!args["repeats"].AsInteger()) {
        unsigned int repeats = s_NumThreads / 6;
        if (repeats < 4)
            repeats = 4;
        if (repeats < s_NumThreads)
            s_NumThreads = repeats;
    }
#endif

    s_SpawnBy = args["spawnby"].AsInteger();

    assert(TestApp_Init());

    unsigned int seed = GetArgs()["seed"]
        ? static_cast<unsigned int>(GetArgs()["seed"].AsInteger())
        : (static_cast<unsigned int>(CProcess::GetCurrentPid()) ^
           static_cast<unsigned int>(time(NULL)) % 1000000);
    TESTAPP_LOG_POST("Randomization seed value: " << seed);
    srand(seed);

    unsigned int threshold = NCBI_PARAM_TYPE(TEST_MT, Cascading)::GetDefault();
    if (threshold > 100) {
        ERR_FATAL("Cascading threshold must be less than 100");
    }
    bool cascading = (static_cast<unsigned int>(rand() % 100)) < threshold;
#if !defined(NCBI_THREADS)
    cascading = true;
#endif
    if ( !cascading ) {
        x_InitializeThreadGroups();
        x_PrintThreadGroups();
    }
    cascading = cascading || (m_ThreadGroups.size() == 0);

#if defined(NCBI_THREADS)
    TESTAPP_LOG_POST("Running " << s_NumThreads << " threads");
#else
    TESTAPP_LOG_POST("Simulating " << s_NumThreads << " threads in ST mode");
#endif

    if (cascading) {
        CTestThread::StartCascadingThreads();
    } else {
        unsigned int start_now = x_InitializeDelayedStart();

        for (unsigned int g = 0;  g < m_ThreadGroups.size();  ++g) {
            thr_group[g] = new CThreadGroup
                (m_ThreadGroups[g].number_of_threads,
                 m_ThreadGroups[g].has_sync_point);
        }
        x_StartThreadGroup(start_now);
    }

    // Wait for all threads
    if ( cascading ) {
        for (unsigned int i = 0;  i < s_NumThreads;  i++) {
            void* ok;
            // make sure all threads have started
            assert(thr[i].NotEmpty());
#ifdef USE_NATIVE_THREADS
            if (thr[i]) {
                thr[i]->JoinNative(&ok);
                assert(ok);
            }
#else
            thr[i]->Join(&ok);
            assert(ok);
#endif
        }
    } else {
        // join only those that started
        unsigned int i = 0;
        for (unsigned int g = 0;  g < m_NextGroup;  ++g) {
            for (unsigned int t = 0;
                 t < m_ThreadGroups[g].number_of_threads; ++t, ++i) {
                void* ok;
                thr[i]->Join(&ok);
                assert(ok);
            }
        }
        assert(m_Reached.size() >= m_Min);
    }

    assert(TestApp_Exit());

    // Destroy all threads
    for (unsigned int i=0; i<s_NumThreads; i++) {
        thr[i].Reset();
    }
    // Destroy all groups
    for (unsigned int i=0; i<m_ThreadGroups.size(); i++) {
        thr_group[i].Reset();
    }

    return 0;
}


void CThreadedApp::x_InitializeThreadGroups(void)
{
    unsigned int count = NStr::StringToUInt
        (NCBI_PARAM_TYPE(TEST_MT, GroupsCount)::GetDefault());
    if (count == 0) {
        return;
    }

    if(count > s_NumThreads) {
        ERR_FATAL("Thread groups with no threads are not allowed");
    }

    unsigned int threshold =
        NCBI_PARAM_TYPE(TEST_MT, IntragroupSyncPoint)::GetDefault();
    if (threshold > 100) {
        ERR_FATAL("IntragroupSyncPoint threshold must be less than 100");
    }

    unsigned int threads_left = s_NumThreads;
    for (unsigned int g = 0;  g < count;  ++g) {
        SThreadGroup group;
        // randomize intra-group sync points
        group.has_sync_point = ((unsigned int)(rand() % 100)) < threshold;
        group.number_of_threads = 1; // mio
        m_ThreadGroups.push_back(group);
    }

    if (s_NumThreads > count) {
        threads_left = s_NumThreads - count;
        for (unsigned int t = 0;  t < threads_left;  ++t) {
            // randomize # of threads
            m_ThreadGroups[ rand() % count ].number_of_threads += 1;
        }
    }
}


void CThreadedApp::x_PrintThreadGroups( void)
{
    size_t count = m_ThreadGroups.size();
    if (count != 0) {
        TESTAPP_LOG_POST("Thread groups: " << count);
        TESTAPP_LOG_POST("Number of delayed thread groups: from "
                         << m_Min << " to " << m_Max);
        TESTAPP_LOG_POST("------------------------");
        TESTAPP_LOG_POST("group threads sync_point");
        for (unsigned int g = 0;  g < count;  ++g) {
            CNcbiOstrstream os;
            os.width(6);
            os << left << g;
            os.width(8);
            os << left << m_ThreadGroups[g].number_of_threads;
            if (m_ThreadGroups[g].has_sync_point) {
                os << "yes";
            } else {
                os << "no ";
            }
            TESTAPP_LOG_POST(string(CNcbiOstrstreamToString(os)));
        }
        TESTAPP_LOG_POST("------------------------");
    }
}


unsigned int CThreadedApp::x_InitializeDelayedStart(void)
{
    const unsigned int count = static_cast<unsigned int>(m_ThreadGroups.size());
    unsigned int start_now = count;
    unsigned int g;
    if (m_Max == 0)
        return start_now;

    for (g = 0;  g < m_Max;  ++g) {
        m_Delayed.push_back(0);
    }

    for (g = 1;  g < count;  ++g) {
        unsigned int dest = rand() % (m_Max+1);
        if (dest != 0) {
            m_Delayed[dest - 1] += 1;
            --start_now;
        }
    }

    CNcbiOstrstream os;
    os << "Delayed thread groups: " << (count - start_now)
       << ", starting order: " << start_now;
    for (g = 0; g < m_Max; ++g) {
        os << '+' << m_Delayed[g];
    }
    TESTAPP_LOG_POST(string(CNcbiOstrstreamToString(os)));

    return start_now;
}


void CThreadedApp::x_StartThreadGroup(unsigned int count)
{
    CFastMutexGuard LOCK(m_AppMutex);
    while (count--) {
        thr_group[m_NextGroup++]->Go();
    }
}


/////////////////////////////////////////////////////////////////////////////

bool CThreadedApp::Thread_Init(int /*idx*/)
{
    return true;
}


bool CThreadedApp::Thread_Run(int /*idx*/)
{
    return true;
}


bool CThreadedApp::Thread_Exit(int /*idx*/)
{
    return true;
}


bool CThreadedApp::Thread_Destroy(int /*idx*/)
{
    return true;
}

bool CThreadedApp::TestApp_Args(CArgDescriptions& /*args*/)
{
    return true;
}

bool CThreadedApp::TestApp_Init(void)
{
    return true;
}


void CThreadedApp::TestApp_IntraGroupSyncPoint(void)
{
    int idx = (int)(intptr_t(s_ThreadIdxTLS.GetValue()));
    thr[idx]->SyncPoint();
}


void CThreadedApp::TestApp_GlobalSyncPoint(void)
{
    {{
        CFastMutexGuard LOCK(m_AppMutex);
        if (!m_Delayed.empty()) {
            TESTAPP_LOG_POST("There were delayed threads, running them now, "
                "because TestApp_GlobalSyncPoint() was called");
            for (int i = m_Reached.size(); i < m_Delayed.size(); i++) {
                m_Reached.insert(NStr::IntToString(i));
                x_StartThreadGroup(m_Delayed[i]);
            }
        }
    }}
    int idx = static_cast<int>(intptr_t(s_ThreadIdxTLS.GetValue()));
    thr[idx]->GlobalSyncPoint();
}


void CThreadedApp::SetNumberOfDelayedStartSyncPoints(
    unsigned int num_min, unsigned int num_max)
{
    m_Min = num_min;
    m_Max = num_max;
}


void CThreadedApp::TestApp_DelayedStartSyncPoint(const string& name)
{
    CFastMutexGuard LOCK(m_AppMutex);
    if (!m_Delayed.empty()  &&  m_Reached.find(name) == m_Reached.end()) {
        m_Reached.insert(name);
        if (m_Reached.size() <= m_Delayed.size()) {
            x_StartThreadGroup(m_Delayed[m_Reached.size() - 1]);
        }
    }
}


bool CThreadedApp::TestApp_Exit(void)
{
    return true;
}


END_NCBI_SCOPE
