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
 *   Multi-threading -- classes, functions, and features.
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
 */


#include <ncbi_pch.hpp>
#include <corelib/ncbithr.hpp>
#include <corelib/ncbimtx.hpp>
#include <corelib/ncbi_safe_static.hpp>
#include <corelib/ncbi_limits.h>
#include <corelib/ncbi_system.hpp>
#include <assert.h>
#ifdef NCBI_POSIX_THREADS
#  include <sys/time.h> // for gettimeofday()
#endif

#include "ncbidbg_p.hpp"

BEGIN_NCBI_SCOPE


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
struct S {
    DECLARE_CLASS_STATIC_FAST_MUTEX(s_TlsMutex0);
};

DEFINE_CLASS_STATIC_FAST_MUTEX(S::s_TlsMutex0);
#define s_TlsMutex S::s_TlsMutex0

CTlsBase::CTlsBase(void)
{
    DoDeleteThisObject();

    // Create platform-dependent TLS key (index)
#if defined(NCBI_WIN32_THREADS)
    xncbi_Verify((m_Key = TlsAlloc()) != DWORD(-1));
#elif defined(NCBI_POSIX_THREADS)
    xncbi_Verify(pthread_key_create(&m_Key, 0) == 0);
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
    NON_CONST_ITERATE(TTls_TlsSet, it, *s_Tls_TlsSet) {
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
    xncbi_Validate(TlsSetValue(key, data), err_message);
#elif defined(NCBI_POSIX_THREADS)
    xncbi_Validate(pthread_setspecific(key, data) == 0, err_message);
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
        xncbi_Validate(tls_data != 0,
                       "CTlsBase::x_SetValue() -- cannot allocate "
                       "memory for TLS data");
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
DEFINE_STATIC_FAST_MUTEX(s_ThreadMutex);
DEFINE_STATIC_FAST_MUTEX(s_TlsCleanupMutex);


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
        xncbi_Validate(thread_obj->m_ID != 0,
                       "CThread::Wrapper() -- error assigning thread ID");
        GetThreadsTls().SetValue(thread_obj);
    }}

    // Run user-provided thread main function here
    try {
        thread_obj->m_ExitData = thread_obj->Main();
    }
    catch (CExitThreadException& e) {
        e.EnterWrapper();
    }
    STD_CATCH_ALL("CThread::Wrapper: CThread::Main() failed");

    // Call user-provided OnExit()
    try {
        thread_obj->OnExit();
    }
    STD_CATCH_ALL("CThread::Wrapper: CThread::OnExit() failed");

    // Cleanup local storages used by this thread
    {{
        CFastMutexGuard tls_cleanup_guard(s_TlsCleanupMutex);
        NON_CONST_ITERATE(TTlsSet, it, thread_obj->m_UsedTls) {
            CRef<CTlsBase> tls = *it;
            tls->x_Reset();
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
#if defined(HAVE_PTHREAD_SETCONCURRENCY)  &&  defined(NCBI_POSIX_THREADS)
    // Adjust concurrency for Solaris etc.
    if (pthread_getconcurrency() == 0) {
        xncbi_Validate(pthread_setconcurrency(GetCpuCount()) == 0,
                       "CThread::CThread() -- pthread_setconcurrency(2) "
                       "failed");
    }
#endif
}


CThread::~CThread(void)
{
#if defined(NCBI_WIN32_THREADS)
    // close handle if it's not yet closed
    CFastMutexGuard state_guard(s_ThreadMutex);
    if ( m_IsRun && m_Handle != NULL ) {
        CloseHandle(m_Handle);
        m_Handle = NULL;
    }
#endif
}



#if defined(NCBI_POSIX_THREADS)
extern "C" {
    typedef TWrapperRes (*FSystemWrapper)(TWrapperArg);
}
#elif defined(NCBI_WIN32_THREADS)
extern "C" {
    typedef TWrapperRes (WINAPI *FSystemWrapper)(TWrapperArg);
}
#endif


bool CThread::Run(TRunMode flags)
{
    // Do not allow the new thread to run until m_Handle is set
    CFastMutexGuard state_guard(s_ThreadMutex);

    // Check
    xncbi_Validate(!m_IsRun,
                   "CThread::Run() -- called for already started thread");

    m_IsDetached = (flags & fRunDetached) != 0;

#if defined(NCBI_WIN32_THREADS)
    // We need this parameter in WinNT - can not use NULL instead!
    DWORD thread_id;
    // Suspend thread to adjust its priority
    DWORD creation_flags = (flags & fRunNice) == 0 ? 0 : CREATE_SUSPENDED;
    m_Handle = CreateThread(NULL, 0,
                            reinterpret_cast<FSystemWrapper>(Wrapper),
                            this, creation_flags, &thread_id);
    xncbi_Validate(m_Handle != NULL,
                   "CThread::Run() -- error creating thread");
    if (flags & fRunNice) {
        // Adjust priority and resume the thread
        SetThreadPriority(m_Handle, THREAD_PRIORITY_BELOW_NORMAL);
        ResumeThread(m_Handle);
    }
    if ( m_IsDetached ) {
        CloseHandle(m_Handle);
        m_Handle = NULL;
    }
    else {
        // duplicate handle to adjust security attributes
        HANDLE oldHandle = m_Handle;
        xncbi_Validate(DuplicateHandle(GetCurrentProcess(), oldHandle,
                                       GetCurrentProcess(), &m_Handle,
                                       0, FALSE, DUPLICATE_SAME_ACCESS),
                       "CThread::Run() -- error getting thread handle");
        xncbi_Validate(CloseHandle(oldHandle),
                       "CThread::Run() -- error closing thread handle");
    }
#elif defined(NCBI_POSIX_THREADS)
    pthread_attr_t attr;
    xncbi_Validate(pthread_attr_init (&attr) == 0,
                   "CThread::Run() - error initializing thread attributes");
    if ( ! (flags & fRunUnbound) ) {
#if defined(NCBI_OS_BSD)
        xncbi_Validate(pthread_attr_setscope(&attr,
                                             PTHREAD_SCOPE_PROCESS) == 0,
                       "CThread::Run() - error setting thread scope");
#else
        xncbi_Validate(pthread_attr_setscope(&attr,
                                             PTHREAD_SCOPE_SYSTEM) == 0,
                       "CThread::Run() - error setting thread scope");
#endif
    }
    if ( m_IsDetached ) {
        xncbi_Validate(pthread_attr_setdetachstate(&attr,
                                                   PTHREAD_CREATE_DETACHED) == 0,
                       "CThread::Run() - error setting thread detach state");
    }
    xncbi_Validate(pthread_create(&m_Handle, &attr,
                                  reinterpret_cast<FSystemWrapper>(Wrapper),
                                  this) == 0,
                   "CThread::Run() -- error creating thread");

    xncbi_Validate(pthread_attr_destroy(&attr) == 0,
                   "CThread::Run() - error destroying thread attributes");

#else
    if (flags & fRunAllowST) {
        Wrapper(this);
    }
    else {
        xncbi_Validate(0,
                       "CThread::Run() -- system does not support threads");
    }
#endif

    // prevent deletion of CThread until thread is finished
    m_SelfRef.Reset(this);

    // Indicate that the thread is run
    m_IsRun = true;
    return true;
}


void CThread::Detach(void)
{
    CFastMutexGuard state_guard(s_ThreadMutex);

    // Check the thread state: it must be run, but not detached yet
    xncbi_Validate(m_IsRun,
                   "CThread::Detach() -- called for not yet started thread");
    xncbi_Validate(!m_IsDetached,
                   "CThread::Detach() -- called for already detached thread");

    // Detach the thread
#if defined(NCBI_WIN32_THREADS)
    xncbi_Validate(CloseHandle(m_Handle),
                   "CThread::Detach() -- error closing thread handle");
    m_Handle = NULL;
#elif defined(NCBI_POSIX_THREADS)
    xncbi_Validate(pthread_detach(m_Handle) == 0,
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
        xncbi_Validate(m_IsRun,
                       "CThread::Join() -- called for not yet started thread");
        xncbi_Validate(!m_IsDetached,
                       "CThread::Join() -- called for detached thread");
        xncbi_Validate(!m_IsJoined,
                       "CThread::Join() -- called for already joined thread");
        m_IsJoined = true;
    }}

    // Join (wait for) and destroy
#if defined(NCBI_WIN32_THREADS)
    xncbi_Validate(WaitForSingleObject(m_Handle, INFINITE) == WAIT_OBJECT_0,
                   "CThread::Join() -- can not join thread");
    DWORD status;
    xncbi_Validate(GetExitCodeThread(m_Handle, &status) &&
                   status != DWORD(STILL_ACTIVE),
                   "CThread::Join() -- thread is still running after join");
    xncbi_Validate(CloseHandle(m_Handle),
                   "CThread::Join() -- can not close thread handle");
    m_Handle = NULL;
#elif defined(NCBI_POSIX_THREADS)
    xncbi_Validate(pthread_join(m_Handle, 0) == 0,
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
    xncbi_Validate(x_this != 0,
                   "CThread::Exit() -- attempt to call for the main thread");

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
    m_SelfRef.Reset(this);
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
        x_this->m_UsedTls.insert(CRef<CTlsBase>(tls));
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


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.32  2005/05/17 17:52:56  grichenk
 * Added flag to run threads with low priority (MS-Win only)
 *
 * Revision 1.31  2004/08/20 17:32:23  grichenk
 * Do not use PTHREAD_SCOPE_SYSTEM on BSD
 *
 * Revision 1.30  2004/05/14 13:59:27  gorelenk
 * Added include of ncbi_pch.hpp
 *
 * Revision 1.29  2003/09/17 15:20:46  vasilche
 * Moved atomic counter swap functions to separate file.
 * Added CRef<>::AtomicResetFrom(), CRef<>::AtomicReleaseTo() methods.
 *
 * Revision 1.28  2003/06/27 17:28:08  ucko
 * +SwapPointers
 *
 * Revision 1.27  2003/05/20 14:23:49  vasilche
 * Added call to pthread_attr_destroy as memory leak was detected.
 *
 * Revision 1.26  2003/05/08 20:50:10  grichenk
 * Allow MT tests to run in ST mode using CThread::fRunAllowST flag.
 *
 * Revision 1.25  2003/04/08 18:41:08  shomrat
 * Setting the handle to NULL after closing it
 *
 * Revision 1.24  2003/03/10 18:57:08  kuznets
 * iterate->ITERATE
 *
 * Revision 1.23  2002/11/04 21:29:04  grichenk
 * Fixed usage of const CRef<> and CRef<> constructor
 *
 * Revision 1.22  2002/09/30 16:53:28  vasilche
 * Fix typedef on Windows.
 *
 * Revision 1.21  2002/09/30 16:32:29  vasilche
 * Fixed bug with self referenced CThread.
 * Added bound running flag to CThread.
 * Fixed concurrency level on POSIX threads.
 *
 * Revision 1.20  2002/09/19 20:05:43  vasilche
 * Safe initialization of static mutexes
 *
 * Revision 1.19  2002/09/13 17:00:11  ucko
 * When using POSIX threads, #include <sys/time.h> for gettimeofday().
 *
 * Revision 1.18  2002/09/13 15:14:49  ucko
 * Give CSemaphore::TryWait an optional timeout (defaulting to 0)
 *
 * Revision 1.17  2002/04/11 21:08:03  ivanov
 * CVS log moved to end of the file
 *
 * Revision 1.16  2002/04/11 20:00:45  ivanov
 * Returned standard assert() vice CORE_ASSERT()
 *
 * Revision 1.15  2002/04/10 18:39:10  ivanov
 * Changed assert() to CORE_ASSERT()
 *
 * Revision 1.14  2001/12/13 19:45:37  gouriano
 * added xxValidateAction functions
 *
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
