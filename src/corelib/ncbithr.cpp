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
 * Author:  Denis Vakatov, Aleksey Grichenko
 *
 * File Description:
 *   Multi-threading -- classes and features.
 *
 *    TLS:
 *      CTlsBase         -- TLS implementation (base class for CTls<>)
 *
 *    THREAD:
 *      CThread          -- thread wrapper class
 *
 *    RW-LOCK:
 *      CInternalRWLock  -- platform-dependent RW-lock structure (fwd-decl)
 *      CRWLock          -- Read/Write lock related  data and methods
 *
 * ---------------------------------------------------------------------------
 * $Log$
 * Revision 1.13  2001/12/12 17:11:23  vakatov
 * [NCBI_POSIX_THREADS] CSemaphore::Post() -- assert(0) just to make sure
 *
 * Revision 1.12  2001/12/11 22:58:16  vakatov
 * [NCBI_POSIX_THREADS] CSemaphore::Post() -- avoid throwing exception
 * without unlocking the embracing mutex first
 *
 * Revision 1.11  2001/12/10 18:07:55  vakatov
 * Added class "CSemaphore" -- application-wide semaphore
 *
 * Revision 1.10  2001/05/17 15:05:00  lavr
 * Typos corrected
 *
 * Revision 1.9  2001/04/03 18:20:45  grichenk
 * CThread::Exit() and CThread::Wrapper() redesigned to use
 * CExitThreadException instead of system functions
 *
 * Revision 1.8  2001/03/30 22:57:34  grichenk
 * + CThread::GetSystemID()
 *
 * Revision 1.7  2001/03/27 18:12:35  grichenk
 * CRWLock destructor releases system resources
 *
 * Revision 1.5  2001/03/26 21:45:29  vakatov
 * Workaround static initialization/destruction traps:  provide better
 * timing control, and allow safe use of the objects which are
 * either not yet initialized or already destructed. (with A.Grichenko)
 *
 * Revision 1.4  2001/03/13 22:43:20  vakatov
 * Full redesign.
 * Implemented all core functionality.
 * Thoroughly tested on different platforms.
 *
 * Revision 1.3  2000/12/11 06:48:49  vakatov
 * Revamped Mutex and RW-lock APIs
 *
 * Revision 1.2  2000/12/09 18:41:59  vakatov
 * Fixed for the extremely smart IRIX MIPSpro73 compiler
 *
 * Revision 1.1  2000/12/09 00:04:21  vakatov
 * First draft:  Fake implementation of Mutex and RW-lock API
 *
 * ===========================================================================
 */

#include <corelib/ncbithr.hpp>
#include <corelib/ncbi_safe_static.hpp>
#include <corelib/ncbi_limits.h>
#include <algorithm>
#include <memory>
#include <assert.h>


BEGIN_NCBI_SCOPE



/////////////////////////////////////////////////////////////////////////////
//  Auxiliary
//


#if defined(_DEBUG)
#  define verify(expr) assert(expr)
#  define s_Verify(state, message)  assert(((void) message, (state)))
#else  /* _DEBUG */
#  define verify(expr) ((void)(expr))
inline
void s_Verify(bool state, const char* message)
{
    if ( !state ) {
        throw runtime_error(message);
    }
}
#endif  /* _DEBUG */




/////////////////////////////////////////////////////////////////////////////
//  CTlsBase::
//


// Flag and function to report s_Tls_TlsSet destruction
static bool s_TlsSetDestroyed = false;

static void s_TlsSetCleanup(void* /* ptr */)
{
    s_TlsSetDestroyed = true;
}


// Set of all TLS objects -- to prevent memory leaks due to
// undestroyed TLS objects, and to avoid premature TLS destruction.
typedef set< CRef<CTlsBase> > TTls_TlsSet;
static CSafeStaticPtr<TTls_TlsSet> s_Tls_TlsSet(s_TlsSetCleanup);


// Protects "s_Tls_TlsSet"
static CFastMutex s_TlsMutex;


CTlsBase::CTlsBase(void)
{
    DoDeleteThisObject();

    // Create platform-dependent TLS key (index)
#if defined(NCBI_WIN32_THREADS)
    s_Verify((m_Key = TlsAlloc()) != DWORD(-1),
             "CTlsBase::CTlsBase() -- error creating key");
#elif defined(NCBI_POSIX_THREADS)
    s_Verify(pthread_key_create(&m_Key, 0) == 0,
             "CTlsBase::CTlsBase() -- error creating key");
#else
    m_Key = 0;
#endif

    m_Initialized = true;
    // Add to the cleanup set if it still exists
    if ( !s_TlsSetDestroyed ) {
        CFastMutexGuard guard(s_TlsMutex);
        s_Tls_TlsSet->insert(CRef<CTlsBase> (this));
    }
}


CTlsBase::~CTlsBase(void)
{
    x_Reset();
    m_Initialized = false;

    // Destroy system TLS key
#if defined(NCBI_WIN32_THREADS)
    if ( TlsFree(m_Key) ) {
        m_Key = 0;
        return;
    }
    assert(0);
#elif defined(NCBI_POSIX_THREADS)
    if (pthread_key_delete(m_Key) == 0) {
        m_Key = 0;
        return;
    }
    assert(0);
#else
    m_Key = 0;
    return;
#endif
}


void CTlsBase::x_Discard(void)
{
    if ( s_TlsSetDestroyed ) {
        return;  // Nothing to do - the TLS set has been destroyed
    }

    CFastMutexGuard guard(s_TlsMutex);
    non_const_iterate(TTls_TlsSet, it, *s_Tls_TlsSet) {
        if (it->GetPointer() == this) {
            s_Tls_TlsSet->erase(it);
            break;
        }
    }
}


// Platform-specific TLS data storing
inline
void s_TlsSetValue(TTlsKey& key, void* data, const char* err_message)
{
#if defined(NCBI_WIN32_THREADS)
    s_Verify(TlsSetValue(key, data), err_message);
#elif defined(NCBI_POSIX_THREADS)
    s_Verify(pthread_setspecific(key, data) == 0, err_message);
#else
    key = data;
    assert(err_message);  // to get rid of the "unused variable" warning
#endif
}


void CTlsBase::x_SetValue(void*        value,
                          FCleanupBase cleanup,
                          void*        cleanup_data)
{
    if ( !m_Initialized ) {
        return;
    }

    // Get previously stored data
    STlsData* tls_data = static_cast<STlsData*> (x_GetTlsData());

    // Create and initialize TLS structure, if it was not present
    if ( !tls_data ) {
        tls_data = new STlsData;
        s_Verify(tls_data != 0,
                 "CTlsBase::x_SetValue() -- cannot alloc memory for TLS data");
        tls_data->m_Value       = 0;
        tls_data->m_CleanupFunc = 0;
        tls_data->m_CleanupData = 0;
    }

    // Cleanup
    if (tls_data->m_CleanupFunc  &&  tls_data->m_Value != value) {
        tls_data->m_CleanupFunc(tls_data->m_Value, tls_data->m_CleanupData);
    }

    // Store the values
    tls_data->m_Value       = value;
    tls_data->m_CleanupFunc = cleanup;
    tls_data->m_CleanupData = cleanup_data;

    // Store the structure in the TLS
    s_TlsSetValue(m_Key, tls_data,
                  "CTlsBase::x_SetValue() -- error setting value");

    // Add to the used TLS list to cleanup data in the thread Exit()
    CThread::AddUsedTls(this);
}


void CTlsBase::x_Reset(void)
{
    if ( !m_Initialized ) {
        return;
    }

    // Get previously stored data
    STlsData* tls_data = static_cast<STlsData*> (x_GetTlsData());
    if ( !tls_data ) {
        return;
    }

    // Cleanup & destroy
    if ( tls_data->m_CleanupFunc ) {
        tls_data->m_CleanupFunc(tls_data->m_Value, tls_data->m_CleanupData);
    }
    delete tls_data;

    // Store NULL in the TLS
    s_TlsSetValue(m_Key, 0,
                  "CTlsBase::x_Reset() -- error cleaning-up TLS");
}



/////////////////////////////////////////////////////////////////////////////
//  CExitThreadException::
//
//    Exception used to terminate threads safely, cleaning up
//    all the resources allocated.
//


class CExitThreadException
{
public:
    // Create new exception object, initialize counter.
    CExitThreadException(void);

    // Create a copy of exception object, increase counter.
    CExitThreadException(const CExitThreadException& prev);

    // Destroy the object, decrease counter. If the counter is
    // zero outside of CThread::Wrapper(), rethrow exception.
    ~CExitThreadException(void);

    // Inform the object it has reached CThread::Wrapper().
    void EnterWrapper(void)
    {
        *m_InWrapper = true;
    }
private:
    int* m_RefCount;
    bool* m_InWrapper;
};


CExitThreadException::CExitThreadException(void)
    : m_RefCount(new int),
      m_InWrapper(new bool)
{
    *m_RefCount = 1;
    *m_InWrapper = false;
}


CExitThreadException::CExitThreadException(const CExitThreadException& prev)
    : m_RefCount(prev.m_RefCount),
      m_InWrapper(prev.m_InWrapper)
{
    (*m_RefCount)++;
}


CExitThreadException::~CExitThreadException(void)
{
    if (--(*m_RefCount) > 0) {
        // Not the last object - continue to handle exceptions
        return;
    }

    bool tmp_in_wrapper = *m_InWrapper; // save the flag
    delete m_RefCount;
    delete m_InWrapper;

    if ( !tmp_in_wrapper ) {
        // Something is wrong - terminate the thread
        assert(((void)("CThread::Exit() -- cannot exit thread"), 0));
#if defined(NCBI_WIN32_THREADS)
        ExitThread(0);
#elif defined(NCBI_POSIX_THREADS)
        pthread_exit(0);
#endif
    }

}



/////////////////////////////////////////////////////////////////////////////
//  CThread::
//

// Mutex to protect CThread members and to make sure that Wrapper() function
// will not proceed until after the appropriate Run() is finished.
static CFastMutex s_ThreadMutex;
static CFastMutex s_TlsCleanupMutex;


// Internal storage for thread objects and related variables/functions
CTls<CThread>* CThread::sm_ThreadsTls;


void s_CleanupThreadsTls(void* /* ptr */)
{
    CThread::sm_ThreadsTls = 0;  // Indicate that the TLS is destroyed
}


void CThread::CreateThreadsTls(void)
{
    static CSafeStaticRef< CTls<CThread> >
        s_ThreadsTlsRef(s_CleanupThreadsTls);

    sm_ThreadsTls = &s_ThreadsTlsRef.Get();
}


TWrapperRes CThread::Wrapper(TWrapperArg arg)
{
    // Get thread object and self ID
    CThread* thread_obj = static_cast<CThread*>(arg);

    // Set Toolkit thread ID. Otherwise no mutexes will work!
    {{
        CFastMutexGuard guard(s_ThreadMutex);

        static int s_ThreadCount = 0;
        s_ThreadCount++;

        thread_obj->m_ID = s_ThreadCount;
        s_Verify(thread_obj->m_ID != 0,
                 "CThread::Wrapper() -- error assigning thread ID");
        GetThreadsTls().SetValue(thread_obj);
    }}

    // Run user-provided thread main function here
    try {
        thread_obj->m_ExitData = thread_obj->Main();
    }
    catch (CExitThreadException& E) {
        E.EnterWrapper();
    }

    // Call user-provided OnExit()
    thread_obj->OnExit();

    // Cleanup local storages used by this thread
    {{
        CFastMutexGuard tls_cleanup_guard(s_TlsCleanupMutex);
        non_const_iterate(TTlsSet, it, thread_obj->m_UsedTls) {
            (*it)->x_Reset();
        }
    }}

    {{
        CFastMutexGuard state_guard(s_ThreadMutex);

        // Indicate the thread is terminated
        thread_obj->m_IsTerminated = true;

        // Schedule the thread object for destruction, if detached
        if ( thread_obj->m_IsDetached ) {
            thread_obj->m_SelfRef.Reset();
        }
    }}

    return 0;
}


CThread::CThread(void)
    : m_IsRun(false),
      m_IsDetached(false),
      m_IsJoined(false),
      m_IsTerminated(false),
      m_ExitData(0)
{
    DoDeleteThisObject();
    m_SelfRef.Reset(this);
#if defined(HAVE_PTHREAD_SETCONCURRENCY)  &&  defined(NCBI_POSIX_THREADS)
    // Adjust concurrency for Solaris etc.
    if (pthread_getconcurrency() < 2) {
        s_Verify(pthread_setconcurrency(2) == 0,
                 "CThread::CThread() -- pthread_setconcurrency(2) failed");
    }
#endif
}


CThread::~CThread(void)
{
    return;
}



#if defined(NCBI_POSIX_THREADS)
extern "C" {
    typedef TWrapperRes (*FSystemWrapper)(TWrapperArg);
}
#endif


bool CThread::Run(void)
{
    // Do not allow the new thread to run until m_Handle is set
    CFastMutexGuard state_guard(s_ThreadMutex);

    // Check
    s_Verify(!m_IsRun,
             "CThread::Run() -- called for already started thread");

#if defined(NCBI_WIN32_THREADS)
    // We need this parameter in WinNT - can not use NULL instead!
    DWORD  thread_id;
    HANDLE thread_handle;
    typedef TWrapperRes (WINAPI *FSystemWrapper)(TWrapperArg);
    thread_handle = CreateThread
        (NULL, 0,
         reinterpret_cast<FSystemWrapper>(Wrapper),
         this, 0, &thread_id);
    s_Verify(thread_handle != NULL,
             "CThread::Run() -- error creating thread");
    s_Verify(DuplicateHandle(GetCurrentProcess(), thread_handle,
                             GetCurrentProcess(), &m_Handle,
                             0, FALSE, DUPLICATE_SAME_ACCESS),
             "CThread::Run() -- error getting thread handle");
    s_Verify(CloseHandle(thread_handle),
             "CThread::Run() -- error closing thread handle");
#elif defined(NCBI_POSIX_THREADS)
    s_Verify(pthread_create
             (&m_Handle, 0,
              reinterpret_cast<FSystemWrapper>(Wrapper), this) == 0,
             "CThread::Run() -- error creating thread");
#else
    s_Verify(0,
             "CThread::Run() -- system does not support threads");
#endif

    // Indicate that the thread is run
    m_IsRun = true;
    return true;
}


void CThread::Detach(void)
{
    CFastMutexGuard state_guard(s_ThreadMutex);

    // Check the thread state: it must be run, but not detached yet
    s_Verify(m_IsRun,
             "CThread::Detach() -- called for not yet started thread");
    s_Verify(!m_IsDetached,
             "CThread::Detach() -- called for already detached thread");

    // Detach the thread
#if defined(NCBI_WIN32_THREADS)
    s_Verify(CloseHandle(m_Handle),
             "CThread::Detach() -- error closing thread handle");
#elif defined(NCBI_POSIX_THREADS)
    s_Verify(pthread_detach(m_Handle) == 0,
             "CThread::Detach() -- error detaching thread");
#endif

    // Indicate the thread is detached
    m_IsDetached = true;

    // Schedule the thread object for destruction, if already terminated
    if ( m_IsTerminated ) {
        m_SelfRef.Reset();
    }
}


void CThread::Join(void** exit_data)
{
    // Check the thread state: it must be run, but not detached yet
    {{
        CFastMutexGuard state_guard(s_ThreadMutex);
        s_Verify(m_IsRun,
                 "CThread::Join() -- called for not yet started thread");
        s_Verify(!m_IsDetached,
                 "CThread::Join() -- called for already detached thread");
        s_Verify(!m_IsJoined,
                 "CThread::Join() -- called for already joined thread");
        m_IsJoined = true;
    }}

    // Join (wait for) and destroy
#if defined(NCBI_WIN32_THREADS)
    s_Verify(WaitForSingleObject(m_Handle, INFINITE) == WAIT_OBJECT_0,
             "CThread::Join() -- can not join thread");
    DWORD status;
    s_Verify(GetExitCodeThread(m_Handle, &status) &&
             status != DWORD(STILL_ACTIVE),
             "CThread::Join() -- thread is still running after join");
    s_Verify(CloseHandle(m_Handle),
             "CThread::Join() -- can not close thread handle");
#elif defined(NCBI_POSIX_THREADS)
    s_Verify(pthread_join(m_Handle, 0) == 0,
             "CThread::Join() -- can not join thread");
#endif

    // Set exit_data value
    if ( exit_data ) {
        *exit_data = m_ExitData;
    }

    // Schedule the thread object for destruction
    {{
        CFastMutexGuard state_guard(s_ThreadMutex);
        m_SelfRef.Reset();
    }}
}


void CThread::Exit(void* exit_data)
{
    // Don't exit from the main thread
    CThread* x_this = GetThreadsTls().GetValue();
    s_Verify(x_this != 0,
             "CThread::Exit() -- attempt to call it for the main thread");

    {{
        CFastMutexGuard state_guard(s_ThreadMutex);
        x_this->m_ExitData = exit_data;
    }}

    // Throw the exception to be caught by Wrapper()
    throw CExitThreadException();
}


bool CThread::Discard(void)
{
    CFastMutexGuard state_guard(s_ThreadMutex);

    // Do not discard after Run()
    if ( m_IsRun ) {
        return false;
    }

    // Schedule for destruction (or, destroy it right now if there is no
    // other CRef<>-based references to this object left).
    m_SelfRef.Reset();
    return true;
}


void CThread::OnExit(void)
{
    return;
}


void CThread::AddUsedTls(CTlsBase* tls)
{
    // Can not use s_ThreadMutex in POSIX since it may be already locked
    CFastMutexGuard tls_cleanup_guard(s_TlsCleanupMutex);

    // Get current thread object
    CThread* x_this = GetThreadsTls().GetValue();
    if ( x_this ) {
        x_this->m_UsedTls.insert(tls);
    }
}


void CThread::GetSystemID(TThreadSystemID* id)
{
#if defined(NCBI_WIN32_THREADS)
    // Can not use GetCurrentThread() since it also requires
    // DuplicateHandle() and CloseHandle() to be called for the result.
    *id = GetCurrentThreadId();
#elif defined(NCBI_POSIX_THREADS)
    *id = pthread_self();
#else
    *id = 0;
#endif
    return;
}



/////////////////////////////////////////////////////////////////////////////
//  CMutex::
//

CMutex::CMutex(void)
    : m_Owner(kThreadID_None),
      m_Count(0)
{
    return;
}


CMutex::~CMutex(void)
{
    // Release system mutex if owned by this thread
    if (m_Owner == CThread::GetSelf()) {
        m_Mtx.Unlock();
    }
    assert(m_Owner == kThreadID_None  ||  m_Owner == CThread::GetSelf());

    return;
}



/////////////////////////////////////////////////////////////////////////////
//  CInternalRWLock::
//

class CInternalRWLock
{
public:
    CInternalRWLock(void);
    ~CInternalRWLock(void);

    // Platform-dependent RW-lock data
#if defined(NCBI_WIN32_THREADS)
    HANDLE m_Rsema;
    HANDLE m_Wsema;
#elif defined(NCBI_POSIX_THREADS)
    pthread_cond_t m_Rcond;
    pthread_cond_t m_Wcond;
#endif
    bool m_Initialized;
};



inline
CInternalRWLock::CInternalRWLock(void)
{
    m_Initialized = true;
    // Create system handles
#if defined(NCBI_WIN32_THREADS)
    if ((m_Rsema = CreateSemaphore(NULL, 1, 1, NULL)) != NULL) {
        if ((m_Wsema = CreateSemaphore(NULL, 1, 1, NULL)) != NULL) {
            return;
        }
        CloseHandle(m_Rsema);
    }
    m_Initialized = false;
    s_Verify(0,
             "CInternalRMLock::InternalRWLock() -- initialization error");
#elif defined(NCBI_POSIX_THREADS)
    if (pthread_cond_init(&m_Rcond, 0) == 0) {
        if (pthread_cond_init(&m_Wcond, 0) == 0) {
            return;
        }
        pthread_cond_destroy(&m_Rcond);
    }
    m_Initialized = false;
    s_Verify(0,
             "CInternalRMLock::InternalRWLock() -- initialization error");
#else
    return;
#endif
}


inline
CInternalRWLock::~CInternalRWLock(void)
{
#if defined(NCBI_WIN32_THREADS)
    verify(CloseHandle(m_Rsema));
    verify(CloseHandle(m_Wsema));
#elif defined(NCBI_POSIX_THREADS)
    verify(pthread_cond_destroy(&m_Rcond) == 0);
    verify(pthread_cond_destroy(&m_Wcond) == 0);
#endif
    m_Initialized = false;
}




/////////////////////////////////////////////////////////////////////////////
//  CRWLock::
//

CRWLock::CRWLock(void)
    : m_RW(new CInternalRWLock),
      m_Mutex(),
      m_Owner(kThreadID_None),
      m_Count(0)
{
    return;
}


CRWLock::~CRWLock(void)
{
    CThread::TID self_id = CThread::GetSelf();

    if (m_Count < 0  &&  m_Owner == self_id) {
        // Release system resources if W-locked by the calling thread
#if defined(NCBI_WIN32_THREADS)
        verify(ReleaseSemaphore(m_RW->m_Rsema, 1, NULL));
        verify(ReleaseSemaphore(m_RW->m_Wsema, 1, NULL));
#elif defined(NCBI_POSIX_THREADS)
        verify(pthread_cond_broadcast(&m_RW->m_Rcond) == 0);
        verify(pthread_cond_signal(&m_RW->m_Wcond) == 0);
#endif
    }
    else if (m_Count > 0) {
        // Release system resources if R-locked by any thread
#if defined(NCBI_WIN32_THREADS)
        verify(ReleaseSemaphore(m_RW->m_Wsema, 1, NULL));
#elif defined(NCBI_POSIX_THREADS)
        verify(pthread_cond_signal(&m_RW->m_Wcond) == 0);
#endif

    }

    assert(m_Count >= 0  ||  m_Owner == self_id);
    return;
}


void CRWLock::ReadLock(void)
{
    if ( !m_RW->m_Initialized ) {
        return;
    }

    // Lock mutex now, unlock before exit.
    // (in fact, it will be unlocked by the waiting function for a while)
    CMutexGuard guard(m_Mutex);
    CThread::TID self_id = CThread::GetSelf();

    if (m_Count < 0  &&  m_Owner == self_id) {
        // if W-locked by the same thread -- update W-counter
        m_Count--;
    }
    else if (m_Count < 0) {
        // W-locked by another thread
#if defined(NCBI_WIN32_THREADS)
        HANDLE obj[2];
        DWORD  wait_res;
        obj[0] = m_Mutex.m_Mtx.m_Handle;
        obj[1] = m_RW->m_Rsema;

        m_Mutex.Unlock();
        wait_res = WaitForMultipleObjects(2, obj, TRUE, INFINITE);
        s_Verify(wait_res >= WAIT_OBJECT_0  &&  wait_res < WAIT_OBJECT_0 + 2,
                 "CRWLock::ReadLock() -- R-lock waiting error");
        // Success, check the semaphore
        LONG prev_sema;
        s_Verify(ReleaseSemaphore(m_RW->m_Rsema, 1, &prev_sema),
                 "CRWLock::ReadLock() -- failed to release R-semaphore");
        s_Verify(prev_sema == 0,
                 "CRWLock::ReadLock() -- invalid R-semaphore state");
        if (m_Count == 0) {
            s_Verify(WaitForSingleObject(m_RW->m_Wsema, 0) == WAIT_OBJECT_0,
                     "CRWLock::ReadLock() - failed to lock W-semaphore");
        }
#elif defined(NCBI_POSIX_THREADS)
        while (m_Count < 0) {
            s_Verify(pthread_cond_wait(&m_RW->m_Rcond,
                                       &m_Mutex.m_Mtx.m_Handle) == 0,
                     "CRWLock::ReadLock() -- R-lock waiting error");
        }
#else
        // Can not be already W-locked by another thread without MT
        s_Verify(0,
                 "CRWLock::ReadLock() -- weird R-lock error in non-MT mode");
#endif

        // Update mutex owner -- now it belongs to this thread
        m_Mutex.m_Owner = self_id;

        s_Verify(m_Count >= 0,
                 "CRWLock::ReadLock() -- invalid readers counter");
        m_Count++;
        m_Owner = self_id;
    }
    else if (m_Count == 0) {
        // Unlocked
#if defined(NCBI_WIN32_THREADS)
        // Lock against writers
        s_Verify(WaitForSingleObject(m_RW->m_Wsema, 0) == WAIT_OBJECT_0,
                 "CRWLock::ReadLock() - failed to lock W-semaphore");
#endif
        m_Count = 1;
        m_Owner = self_id;
    }
    else {
        // R-locked by other threads
        m_Count++;
        if (m_Owner == kThreadID_None) {
            m_Owner = self_id;
        }
    }

#if defined(_DEBUG)
    // Remember new reader
    if (m_Count > 0) {
        m_Readers.push_back(self_id);
    }
#endif
}


void CRWLock::WriteLock(void)
{
    if ( !m_RW->m_Initialized ) {
        return;
    }

    CMutexGuard guard(m_Mutex);
    CThread::TID self_id = CThread::GetSelf();

    if (m_Count < 0  &&  m_Owner == self_id) {
        // W-locked by the same thread
        m_Count--;
    }
    else if (m_Count == 0 || m_Owner != self_id) {
        // Unlocked or RW-locked by another thread

        // Look in readers - must not be there
        assert(find(m_Readers.begin(), m_Readers.end(), self_id)
               == m_Readers.end());

#if defined(NCBI_WIN32_THREADS)
        HANDLE obj[3];
        obj[0] = m_RW->m_Rsema;
        obj[1] = m_RW->m_Wsema;
        obj[2] = m_Mutex.m_Mtx.m_Handle;
        DWORD wait_res;
        if (m_Count == 0) {
            // Unlocked - lock both semaphores
            wait_res = WaitForMultipleObjects(2, obj, TRUE, 0);
            s_Verify(wait_res >= WAIT_OBJECT_0 && wait_res < WAIT_OBJECT_0+2,
                     "CRWLock::WriteLock() -- error locking R&W-semaphores");
        }
        else {
            // Locked by another thread - wait for unlock
            m_Mutex.Unlock();
            wait_res = WaitForMultipleObjects(3, obj, TRUE, INFINITE);
            s_Verify(wait_res >= WAIT_OBJECT_0 && wait_res < WAIT_OBJECT_0+3,
                     "CRWLock::WriteLock() -- error locking R&W-semaphores");
        }
#elif defined(NCBI_POSIX_THREADS)
        while (m_Count != 0) {
            s_Verify(pthread_cond_wait(&m_RW->m_Wcond,
                                       &m_Mutex.m_Mtx.m_Handle) == 0,
                     "CRWLock::WriteLock() -- error locking R&W-conditionals");
        }
#endif

        // Update mutex owner - now it's this thread
        m_Mutex.m_Owner = self_id;

        s_Verify(m_Count >= 0,
                 "CRWLock::WriteLock() -- invalid readers counter");
        m_Count = -1;
        m_Owner = self_id;
    }
    else {
        // R-locked by the same thread, not always detectable in POSIX
        s_Verify(0,
                 "CRWLock::WriteLock() -- attempt to set W-after-R lock");
    }

    // No readers allowed
    assert(m_Readers.begin() == m_Readers.end());
}


bool CRWLock::TryReadLock(void)
{
    if ( !m_RW->m_Initialized ) {
        return true;
    }

    CMutexGuard guard(m_Mutex);
    CThread::TID self_id = CThread::GetSelf();

    if (m_Count < 0 && m_Owner != self_id) {
        // W-locked by another thread
        return false;
    }

    if (m_Count >= 0) {
        // Unlocked - do R-lock
#if defined(NCBI_WIN32_THREADS)
        if (m_Count == 0) {
            // Lock W-semaphore in MSWIN
            s_Verify(WaitForSingleObject(m_RW->m_Wsema, 0) == WAIT_OBJECT_0,
                     "CRWLock::TryReadLock() -- can not lock W-semaphore");
        }
#endif
        m_Count++;
        m_Owner = self_id;
#if defined(_DEBUG)
        m_Readers.push_back(CThread::GetSelf());
#endif
    }
    else {
        // W-locked, try to set R after W if in the same thread
        if (m_Owner != self_id) {
            return false;
        }
        m_Count--;
    }

    return true;
}


bool CRWLock::TryWriteLock(void)
{
    if ( !m_RW->m_Initialized ) {
        return true;
    }

    CMutexGuard guard(m_Mutex);
    CThread::TID self_id = CThread::GetSelf();

    if (m_Count > 0  ||  (m_Count < 0  &&  m_Owner != self_id)) {
        return false;
    }

    if (m_Count == 0) {
        // Unlocked - do W-lock
#if defined(NCBI_WIN32_THREADS)
        // In MSWIN lock semaphores
        HANDLE obj[2];
        obj[0] = m_RW->m_Rsema;
        obj[1] = m_RW->m_Wsema;
        DWORD wait_res;
        wait_res = WaitForMultipleObjects(2, obj, TRUE, 0);
        s_Verify(wait_res >= WAIT_OBJECT_0  &&  wait_res < WAIT_OBJECT_0 + 2,
                 "CRWLock::TryWriteLock() -- error locking R&W-semaphores");
#endif
        m_Count = -1;
        m_Owner = self_id;
    }
    else if (m_Count > 0) {
        // R-locked, can not set W-lock
        return false;
    }
    else {
        // W-locked, check for the same thread
        if (m_Owner != self_id) {
            return false;
        }
        m_Count--;
    }

    // No readers allowed
    assert(m_Readers.begin() == m_Readers.end());

    return true;
}


void CRWLock::Unlock(void)
{
    if ( !m_RW->m_Initialized ) {
        return;
    }

    CMutexGuard guard(m_Mutex);
    CThread::TID self_id = CThread::GetSelf();

    // Check it is R-locked or W-locked by the same thread
    s_Verify(m_Count != 0,
             "CRWLock::Unlock() -- RWLock is not locked");
    s_Verify(m_Count >= 0 || m_Owner == self_id,
             "CRWLock::Unlock() -- RWLock is locked by another thread");

    if (m_Count == -1) {
        // Unlock the last W-lock
#if defined(NCBI_WIN32_THREADS)
        LONG prev_sema;
        s_Verify(ReleaseSemaphore(m_RW->m_Rsema, 1, &prev_sema),
                 "CRWLock::Unlock() -- error releasing R-semaphore");
        s_Verify(prev_sema == 0,
                 "CRWLock::Unlock() -- invalid R-semaphore state");
        s_Verify(ReleaseSemaphore(m_RW->m_Wsema, 1, &prev_sema),
                 "CRWLock::Unlock() -- error releasing W-semaphore");
        s_Verify(prev_sema == 0,
                 "CRWLock::Unlock() -- invalid W-semaphore state");
#elif defined(NCBI_POSIX_THREADS)
        s_Verify(pthread_cond_broadcast(&m_RW->m_Rcond) == 0,
                 "CRWLock::Unlock() -- error signalling unlock");
        s_Verify(pthread_cond_signal(&m_RW->m_Wcond) == 0,
                 "CRWLock::Unlock() -- error signalling unlock");
#endif
        // Update mutex owner - now it's this thread
        m_Mutex.m_Owner = self_id;

        m_Count = 0;
        m_Owner = kThreadID_None;
    }
    else if (m_Count < -1) {
        // Unlock W-lock (not the last one)
        m_Count++;
    }
    else if (m_Count == 1) {
        // Unlock the last R-lock
#if defined(NCBI_WIN32_THREADS)
        LONG prev_sema;
        s_Verify(ReleaseSemaphore(m_RW->m_Wsema, 1, &prev_sema),
                 "CRWLock::Unlock() -- error releasing W-semaphore");
        s_Verify(prev_sema == 0,
                 "CRWLock::Unlock() -- invalid W-semaphore state");
#elif defined(NCBI_POSIX_THREADS)
        s_Verify(pthread_cond_signal(&m_RW->m_Wcond) == 0,
                 "CRWLock::Unlock() -- error signaling unlock");
#endif

        m_Count = 0;
        m_Owner = kThreadID_None;

#if defined(_DEBUG)
        m_Readers.erase(m_Readers.begin(), m_Readers.end());
#endif
    }
    else {
        // Unlock R-lock (not the last one)
        m_Count--;
        // Reset the owner, it may become incorrect after unlock
        if (m_Owner == self_id) {
            m_Owner = kThreadID_None;
        }

#if defined(_DEBUG)
        // Check if the unlocking thread is in the owners list
        list<CThread::TID>::iterator found =
            find(m_Readers.begin(), m_Readers.end(), self_id);
        assert(found != m_Readers.end());
        m_Readers.erase(found);
#endif
    }
}




/////////////////////////////////////////////////////////////////////////////
//  SEMAPHORE
//


// Platform-specific representation (or emulation) of semaphore
struct SSemaphore
{
#if defined(NCBI_POSIX_THREADS)
    unsigned int     max_count;
    unsigned int     count; 
    unsigned int     wait_count;  // # of threads currently waiting on the sema
    pthread_mutex_t  mutex;
    pthread_cond_t   cond;

#elif defined(NCBI_WIN32_THREADS)
    HANDLE           sem;

#else
    unsigned int     max_count;
    unsigned int     count;
#endif
};


CSemaphore::CSemaphore(unsigned int init_count, unsigned int max_count)
{
    s_Verify(max_count != 0,
             "CSemaphore::CSemaphore() -- max_count passed zero");
    s_Verify(init_count <= max_count,
             "CSemaphore::CSemaphore() -- init_count greater than max_count");

    m_Sem = new SSemaphore;
    auto_ptr<SSemaphore> auto_sem(m_Sem);

#if defined(NCBI_POSIX_THREADS)
    m_Sem->max_count = max_count;
    m_Sem->count     = init_count;
    m_Sem->wait_count = 0;

    s_Verify(pthread_mutex_init(&m_Sem->mutex, 0) == 0,
             "CSemaphore::CSemaphore() -- pthread_mutex_init() failed");
    s_Verify(pthread_cond_init(&m_Sem->cond, 0) == 0,
             "CSemaphore::CSemaphore() -- pthread_cond_init() failed");

#elif defined(NCBI_WIN32_THREADS)
    m_Sem->sem = CreateSemaphore(NULL, init_count, max_count, NULL);
    s_Verify(m_Sem->sem != NULL,
             "CSemaphore::CSemaphore() -- CreateSemaphore() failed");

#else
    m_Sem->max_count = max_count;
    m_Sem->count     = init_count;
#endif

    auto_sem.release();
}


CSemaphore::~CSemaphore(void)
{
#if defined(NCBI_POSIX_THREADS)
    assert(m_Sem->wait_count == 0);
    verify(pthread_mutex_destroy(&m_Sem->mutex) == 0);
    verify(pthread_cond_destroy (&m_Sem->cond)  == 0);

#elif defined(NCBI_WIN32_THREADS)
    verify( CloseHandle(m_Sem->sem) );
#endif

    delete m_Sem;
}


void CSemaphore::Wait(void)
{
#if defined(NCBI_POSIX_THREADS)
    s_Verify(pthread_mutex_lock(&m_Sem->mutex) == 0,
             "CSemaphore::Wait() -- pthread_mutex_lock() failed");

    if (m_Sem->count != 0) {
        m_Sem->count--;
    }
    else {
        m_Sem->wait_count++;
        do {
            if (pthread_cond_wait(&m_Sem->cond, &m_Sem->mutex) != 0) {
                s_Verify(pthread_mutex_unlock(&m_Sem->mutex) == 0,
                         "CSemaphore::Wait() -- pthread_cond_wait() and "
                         "pthread_mutex_unlock() failed");
                s_Verify(0,"CSemaphore::Wait() -- pthread_cond_wait() failed");
            }
        } while (m_Sem->count == 0);
        m_Sem->wait_count--;
        m_Sem->count--;
    }

    s_Verify(pthread_mutex_unlock(&m_Sem->mutex) == 0,
             "CSemaphore::Wait() -- pthread_mutex_unlock() failed");

#elif defined(NCBI_WIN32_THREADS)
    s_Verify(WaitForSingleObject(m_Sem->sem, INFINITE) == WAIT_OBJECT_0,
             "CSemaphore::Wait() -- WaitForSingleObject() failed");

#else
    s_Verify(m_Sem->count != 0,
             "CSemaphore::Wait() -- wait with zero count in 1-thread mode(?)");
    m_Sem->count--;
#endif
}


bool CSemaphore::TryWait(void)
{
#if defined(NCBI_POSIX_THREADS)
    s_Verify(pthread_mutex_lock(&m_Sem->mutex) == 0,
             "CSemaphore::TryWait() -- pthread_mutex_lock() failed");

    bool retval;
    if (m_Sem->count != 0) {
        m_Sem->count--;
        retval = true;
    }
    else {
        retval = false;
    }

    s_Verify(pthread_mutex_unlock(&m_Sem->mutex) == 0,
             "CSemaphore::TryWait() -- pthread_mutex_unlock() failed");

    return retval;

#elif defined(NCBI_WIN32_THREADS)
    DWORD res = WaitForSingleObject(m_Sem->sem, 0);
    s_Verify(res == WAIT_OBJECT_0  ||  res == WAIT_TIMEOUT,
             "CSemaphore::TryWait() -- WaitForSingleObject() failed");
    return (res == WAIT_OBJECT_0);

#else
    if (m_Sem->count == 0)
        return false;
    m_Sem->count--;
    return true;
#endif
}


void CSemaphore::Post(unsigned int count)
{
    if (count == 0)
        return;

#if defined (NCBI_POSIX_THREADS)
    s_Verify(pthread_mutex_lock(&m_Sem->mutex) == 0,
             "CSemaphore::Post() -- pthread_mutex_lock() failed");

    if (m_Sem->count > kMax_UInt - count  ||
        m_Sem->count + count > m_Sem->max_count) {
        s_Verify(pthread_mutex_unlock(&m_Sem->mutex) == 0,
                 "CSemaphore::Post() -- attempt to exceed max_count and "
                 "pthread_mutex_unlock() failed");
        s_Verify(m_Sem->count <= kMax_UInt - count,
                 "CSemaphore::Post() -- would result in counter > MAX_UINT");
        s_Verify(m_Sem->count + count <= m_Sem->max_count,
                 "CSemaphore::Post() -- attempt to exceed max_count");
        assert(0);
    }

    // Signal some (or all) of the threads waiting on this semaphore
    int err_code = 0;
    if (m_Sem->count + count >= m_Sem->wait_count) {
        err_code = pthread_cond_broadcast(&m_Sem->cond);
    } else {
        // Do not use broadcast here to avoid waking up more threads
        // than really needed...
        for (unsigned int n_sig = 0;  n_sig < count;  n_sig++) {
            err_code = pthread_cond_signal(&m_Sem->cond);
            if (err_code != 0) {
                err_code = pthread_cond_broadcast(&m_Sem->cond);
                break;
            }
        }
    }

    // Success
    if (err_code == 0) {
        m_Sem->count += count;
        s_Verify(pthread_mutex_unlock(&m_Sem->mutex) == 0,
                 "CSemaphore::Post() -- pthread_mutex_unlock() failed");
        return;
    }

    // Error
    s_Verify(pthread_mutex_unlock(&m_Sem->mutex) == 0,
             "CSemaphore::Post() -- pthread_cond_signal/broadcast() and "
             "pthread_mutex_unlock() failed");
    s_Verify(0,
             "CSemaphore::Post() -- pthread_cond_signal/broadcast() "
             "failed");

#elif defined(NCBI_WIN32_THREADS)
    s_Verify(ReleaseSemaphore(m_Sem->sem, count, NULL),
             "CSemaphore::Post() -- ReleaseSemaphore() failed");

#else
    s_Verify(m_Sem->count + count <= m_Sem->max_count,
             "CSemaphore::Post() -- attempt to exceed max_count");
    m_Sem->count += count;
#endif
}


END_NCBI_SCOPE
