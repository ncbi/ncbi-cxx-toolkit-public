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
 * Authors:  Denis Vakatov, Aleksey Grichenko, Eugene Vasilchenko
 *
 * File Description:
 *   Multi-threading -- fast mutexes
 *
 *   MUTEX:
 *      CInternalMutex   -- platform-dependent mutex functionality
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbimtx.hpp>
#include <corelib/ncbi_limits.h>
#include "ncbidbg_p.hpp"
#include <stdio.h>
#include <algorithm>
#ifdef NCBI_POSIX_THREADS
#  include <sys/time.h> // for gettimeofday()
#endif

#if defined(_DEBUG) &&  defined(LOG_MUTEX_EVENTS)

#include <corelib/ncbifile.hpp>
#include <fcntl.h>
#  if defined(NCBI_OS_MSWIN)
#    include <io.h>
#  endif

#endif

#define STACK_THRESHOLD (1024)


BEGIN_NCBI_SCOPE


#if defined(_DEBUG) &&  defined(LOG_MUTEX_EVENTS)

void WriteMutexEvent(void* mutex_ptr, const char* message)
{
    static const int mode = O_WRONLY | O_APPEND | O_CREAT | O_TRUNC;
    static const mode_t perm = CDirEntry::MakeModeT(
        CDirEntry::fRead | CDirEntry::fWrite,
        CDirEntry::fRead | CDirEntry::fWrite,
        CDirEntry::fRead | CDirEntry::fWrite,
        0);
    static const char* file_name = getenv("MUTEX_EVENTS_LOG_FILE");
    static int handle = open(file_name ? file_name : "mutex_events.log",
        mode, perm);
    CNcbiOstrstream str_os;
    str_os << mutex_ptr << " "
        << CThreadSystemID::GetCurrent().m_ID << " "
        << message << "\n";
    write(handle, str_os.str(), str_os.pcount());
    str_os.rdbuf()->freeze(false);
}

#endif


/////////////////////////////////////////////////////////////////////////////
//  CInternalMutex::
//

void SSystemFastMutex::InitializeHandle(void)
{
    WRITE_MUTEX_EVENT(this, "SSystemFastMutex::InitializeHandle()");

    // Create platform-dependent mutex handle
#if defined(NCBI_WIN32_THREADS)
#  if defined(NCBI_USE_CRITICAL_SECTION)
    InitializeCriticalSection(&m_Handle);
#  else
    xncbi_Validate((m_Handle = CreateMutex(NULL, FALSE, NULL)) != NULL,
                   "Mutex creation failed");
#  endif
#elif defined(NCBI_POSIX_THREADS)
#  if defined(NCBI_OS_CYGWIN)
    if (pthread_mutex_init(&m_Handle, 0) != 0) {
        // On Cygwin, there was an attempt to check the mutex state,
        // which in some cases (uninitialized memory) could cause
        // a fake EBUSY error. This bug seems to have been fixed
        // (just looked at the source code; never tested) in the early 2006.
        memset(&m_Handle, 0, sizeof(m_Handle));
#  endif  /* NCBI_OS_CYGWIN */
        xncbi_Validate(pthread_mutex_init(&m_Handle, 0) == 0,
                       "Mutex creation failed");
#  if defined(NCBI_OS_CYGWIN)
    }
#  endif  /* NCBI_OS_CYGWIN */
#endif
}

void SSystemFastMutex::DestroyHandle(void)
{
    WRITE_MUTEX_EVENT(this, "SSystemFastMutex::DestroyHandle()");

    // Destroy system mutex handle
#if defined(NCBI_WIN32_THREADS)
#  if defined(NCBI_USE_CRITICAL_SECTION)
    DeleteCriticalSection(&m_Handle);
#  else
    xncbi_Verify(CloseHandle(m_Handle) != 0);
#  endif
#elif defined(NCBI_POSIX_THREADS)
    xncbi_Verify(pthread_mutex_destroy(&m_Handle) == 0);
#endif
}

void SSystemFastMutex::InitializeStatic(void)
{
#if !defined(NCBI_NO_THREADS)
    switch ( m_Magic ) {
    case eMutexUninitialized: // ok
        break;
    case eMutexInitialized:
        xncbi_Validate(0, "Double initialization of mutex");
        break;
    default:
        xncbi_Validate(0, "SSystemFastMutex::m_Magic contains invalid value");
        break;
    }

    InitializeHandle();
#endif

    m_Magic = eMutexInitialized;
}


void SSystemFastMutex::InitializeDynamic(void)
{
#if !defined(NCBI_NO_THREADS)
    InitializeHandle();
#endif

    m_Magic = eMutexInitialized;
}


void SSystemFastMutex::Destroy(void)
{
#if !defined(NCBI_NO_THREADS)
    xncbi_Validate(IsInitialized(), "Destruction of uninitialized mutex");
#endif

    m_Magic = eMutexUninitialized;

    DestroyHandle();
}

void SSystemFastMutex::ThrowUninitialized(void)
{
    NCBI_THROW(CMutexException, eUninitialized, "Mutex uninitialized");
}

void SSystemFastMutex::ThrowLockFailed(void)
{
    NCBI_THROW(CMutexException, eLock, "Mutex lock failed");
}

void SSystemFastMutex::ThrowUnlockFailed(void)
{
    NCBI_THROW(CMutexException, eUnlock, "Mutex unlock failed");
}

void SSystemFastMutex::ThrowTryLockFailed(void)
{
    NCBI_THROW(CMutexException, eTryLock,
               "Mutex check (TryLock) failed");
}

void SSystemMutex::Destroy(void)
{
    xncbi_Validate(m_Count == 0, "Destruction of locked mutex");
    m_Mutex.Destroy();
}

#if !defined(NCBI_NO_THREADS)
void SSystemMutex::Lock(void)
{
    m_Mutex.CheckInitialized();

    CThreadSystemID owner = CThreadSystemID::GetCurrent();
    if ( m_Count > 0 && m_Owner.Is(owner) ) {
        // Don't lock twice, just increase the counter
        m_Count++;
        return;
    }

    // Lock the mutex and remember the owner
    m_Mutex.Lock();
    assert(m_Count == 0);
    m_Owner.Set(owner);
    m_Count = 1;
}

bool SSystemMutex::TryLock(void)
{
    m_Mutex.CheckInitialized();

    CThreadSystemID owner = CThreadSystemID::GetCurrent();
    if ( m_Count > 0 && m_Owner.Is(owner) ) {
        // Don't lock twice, just increase the counter
        m_Count++;
        return true;
    }

    // If TryLock is successful, remember the owner
    if ( m_Mutex.TryLock() ) {
        assert(m_Count == 0);
        m_Owner.Set(owner);
        m_Count = 1;
        return true;
    }

    // Cannot lock right now
    return false;
}

void SSystemMutex::Unlock(void)
{
    m_Mutex.CheckInitialized();

    // No unlocks by threads other than owner.
    // This includes no unlocks of unlocked mutex.
    CThreadSystemID owner = CThreadSystemID::GetCurrent();
    if ( m_Count == 0 || m_Owner.IsNot(owner) ) {
        ThrowNotOwned();
    }

    // No real unlocks if counter > 1, just decrease it
    if (--m_Count > 0) {
        return;
    }

    assert(m_Count == 0);
    // This was the last lock - clear the owner and unlock the mutex
    m_Mutex.Unlock();
}
#endif

void SSystemMutex::ThrowNotOwned(void)
{
    NCBI_THROW(CMutexException, eOwner,
               "Mutex is not owned by current thread");
}


#if defined(NEED_AUTO_INITIALIZE_MUTEX)
//#define USE_STATIC_INIT_MUTEX_HANDLE 1

static const char* kInitMutexName = "NCBI_CAutoInitializeStaticMutex";

#ifdef USE_STATIC_INIT_MUTEX_HANDLE

static volatile TSystemMutex s_InitMutexHandle = 0;
static TSystemMutex s_GetInitMutexHandle(void)
{
    TSystemMutex init_mutex = s_InitMutexHandle;
    if ( !init_mutex ) {
        init_mutex = CreateMutex(NULL, FALSE, kInitMutexName);
        xncbi_Verify(init_mutex);
        assert(!s_InitMutexHandle || s_InitMutexHandle == init_mutex);
        s_InitMutexHandle = init_mutex;
    }
    return init_mutex;
}

static inline void s_ReleaseInitMutexHandle(TSystemMutex _DEBUG_ARG(mutex))
{
    assert(mutex == s_InitMutexHandle);
}

#else

static inline HANDLE s_GetInitMutexHandle(void)
{
    HANDLE init_mutex = CreateMutex(NULL, FALSE, kInitMutexName);
    xncbi_Verify(init_mutex);
    return init_mutex;
}

static inline void s_ReleaseInitMutexHandle(HANDLE mutex)
{
    CloseHandle(mutex);
}

#endif

void CAutoInitializeStaticFastMutex::Initialize(void)
{
    if ( m_Mutex.IsInitialized() ) {
        return;
    }
    HANDLE init_mutex = s_GetInitMutexHandle();
    xncbi_Verify(WaitForSingleObject(init_mutex, INFINITE) == WAIT_OBJECT_0);
    if ( !m_Mutex.IsInitialized() ) {
        m_Mutex.InitializeStatic();
    }
    xncbi_Verify(ReleaseMutex(init_mutex));
    assert(m_Mutex.IsInitialized());
    s_ReleaseInitMutexHandle(init_mutex);
}

void CAutoInitializeStaticMutex::Initialize(void)
{
    if ( m_Mutex.IsInitialized() ) {
        return;
    }
    HANDLE init_mutex = s_GetInitMutexHandle();
    xncbi_Verify(WaitForSingleObject(init_mutex, INFINITE) == WAIT_OBJECT_0);
    if ( !m_Mutex.IsInitialized() ) {
        m_Mutex.InitializeStatic();
    }
    xncbi_Verify(ReleaseMutex(init_mutex));
    assert(m_Mutex.IsInitialized());
    s_ReleaseInitMutexHandle(init_mutex);
}

#endif

const char* CMutexException::GetErrCodeString(void) const
{
    switch (GetErrCode()) {
    case eLock:    return "eLock";
    case eUnlock:  return "eUnlock";
    case eTryLock: return "eTryLock";
    case eOwner:   return "eOwner";
    case eUninitialized:  return "eUninitialized";
    default:       return CException::GetErrCodeString();
    }
}

/////////////////////////////////////////////////////////////////////////////
//  CInternalRWLock::
//

#if defined(NCBI_WIN32_THREADS)

class CWindowsHandle
{
public:
    CWindowsHandle(HANDLE h = NULL) : m_Handle(h) {}
    CWindowsHandle(HANDLE h, const char* errorMessage) : m_Handle(h)
    {
        xncbi_Validate(h != NULL, errorMessage);
    }
    ~CWindowsHandle(void) { Close(); }

    void Close(void)
    {
        if ( m_Handle != NULL ) {
            CloseHandle(m_Handle);
            m_Handle = NULL;
        }
    }

    void Set(HANDLE h)
    {
        Close();
        m_Handle = h;
    }
    void Set(HANDLE h, const char* errorMessage)
    {
        xncbi_Validate(h != NULL, errorMessage);
        Set(h);        
    }

    HANDLE GetHandle(void) const  { return m_Handle; }
    operator HANDLE(void) const { return m_Handle; }

protected:
    HANDLE m_Handle;

private:
    CWindowsHandle(const CWindowsHandle& h);
    CWindowsHandle& operator=(const CWindowsHandle& h);
};

class CWindowsSemaphore : public CWindowsHandle
{
public:
    CWindowsSemaphore(LONG initialCount = 0, LONG maximumCount = INFINITE)
        : CWindowsHandle(CreateSemaphore(NULL,
                                         initialCount, maximumCount,
                                         NULL),
                         "CreateSemaphore() failed")
    {
    }

    LONG Release(LONG add = 1)
    {
        LONG prev_sema;
        xncbi_Validate(ReleaseSemaphore(*this, add, &prev_sema),
                       "CWindowsSemaphore::Release() failed");
        return prev_sema;
    }
};

#endif

#if defined(NCBI_POSIX_THREADS)

class CPthreadCond
{
public:
    CPthreadCond(void)
        : m_Initialized(pthread_cond_init(&m_Handle, 0) != 0)
    {
    }
    ~CPthreadCond(void)
    {
        if ( m_Initialized ) {
            pthread_cond_destroy(&m_Handle);
        }
    }

    operator pthread_cond_t*(void) { return &m_Handle; }
    operator pthread_cond_t&(void) { return m_Handle; }

protected:
    pthread_cond_t  m_Handle;
    bool            m_Initialized;
};

#endif

class CInternalRWLock
{
public:
    CInternalRWLock(void);

    // Platform-dependent RW-lock data
#if defined(NCBI_WIN32_THREADS)
    CWindowsSemaphore   m_Rsema;  // down when locked for writing
    CWindowsSemaphore   m_Rsema2; // down when writers are waiting
    CWindowsSemaphore   m_Wsema;  // down when locked for reading OR writing
#  if defined(NCBI_USE_CRITICAL_SECTION)
    CWindowsHandle      m_Mutex;
#  else
    CFastMutex          m_Mutex;
#  endif
#elif defined(NCBI_POSIX_THREADS)
    CPthreadCond        m_Rcond;
    CPthreadCond        m_Wcond;
    CFastMutex          m_Mutex;
#endif
};

inline
CInternalRWLock::CInternalRWLock(void)
#if defined(NCBI_WIN32_THREADS)
    : m_Rsema(1, 1), m_Rsema2(1, 1), m_Wsema(1, 1)
#endif
{
#if defined(NCBI_USE_CRITICAL_SECTION)
    m_Mutex.Set(CreateMutex(NULL, FALSE, NULL));
#endif
}

/////////////////////////////////////////////////////////////////////////////
//  CRWLock::
//

CRWLock::CRWLock(TFlags flags)
    : m_Flags(flags),
      m_RW(new CInternalRWLock),
      m_Count(0),
      m_WaitingWriters(0)
{
#if defined(_DEBUG)
    m_Flags |= fTrackReaders;
#else
    if (m_Flags & fFavorWriters) {
        m_Flags |= fTrackReaders;
    }
#endif
    if (m_Flags & fTrackReaders) {
        m_Readers.reserve(16);
    }
}


CRWLock::~CRWLock(void)
{
}


bool CRWLock::x_MayAcquireForReading(CThreadSystemID self_id)
{
    _ASSERT(self_id == CThreadSystemID::GetCurrent());
    if (m_Count < 0) { // locked for writing, possibly by self
        // return m_Owner.Is(self_id);
        return false; // allow special handling of self-locked cases
    } else if ( !(m_Flags & fFavorWriters) ) {
        return true; // no other concerns
    } else if (find(m_Readers.begin(), m_Readers.end(), self_id)
               != m_Readers.end()) {
        return true; // allow recursive read locks
    } else {
        return !m_WaitingWriters;
    }
}

#if defined(NCBI_USE_CRITICAL_SECTION)

// Need special guard for system handle since mutex uses critical section
class CWin32MutexHandleGuard
{
public:
    CWin32MutexHandleGuard(HANDLE mutex);
    ~CWin32MutexHandleGuard(void);
private:
    HANDLE m_Handle;
};


inline
CWin32MutexHandleGuard::CWin32MutexHandleGuard(HANDLE mutex)
    : m_Handle(mutex)
{
    WaitForSingleObject(m_Handle, INFINITE);
}


inline
CWin32MutexHandleGuard::~CWin32MutexHandleGuard(void)
{
    ReleaseMutex(m_Handle);
}


#endif

void CRWLock::ReadLock(void)
{
#if defined(NCBI_NO_THREADS)
    return;
#else
    // Lock mutex now, unlock before exit.
    // (in fact, it will be unlocked by the waiting function for a while)
#if defined(NCBI_USE_CRITICAL_SECTION)
    CWin32MutexHandleGuard guard(m_RW->m_Mutex);
#else
    CFastMutexGuard guard(m_RW->m_Mutex);
#endif
    CThreadSystemID self_id = CThreadSystemID::GetCurrent();
    if ( !x_MayAcquireForReading(self_id) ) {
        if (m_Count < 0  &&  m_Owner.Is(self_id)) {
            // if W-locked by the same thread - update W-counter
            m_Count--;
        }
        else {
            // (due to be) W-locked by another thread
#if defined(NCBI_WIN32_THREADS)
            HANDLE obj[3];
            DWORD  wait_res;
            obj[0] = m_RW->m_Mutex.GetHandle();
            obj[1] = m_RW->m_Rsema;
            obj[2] = m_RW->m_Rsema2;
            xncbi_Validate(ReleaseMutex(m_RW->m_Mutex.GetHandle()),
                           "CRWLock::ReadLock() - release mutex error");
            wait_res = WaitForMultipleObjects(3, obj, TRUE, INFINITE);
            xncbi_Validate(wait_res >= WAIT_OBJECT_0  &&
                           wait_res < WAIT_OBJECT_0 + 3,
                           "CRWLock::ReadLock() - R-lock waiting error");
            // Success, check the semaphore
            xncbi_Validate(m_RW->m_Rsema.Release() == 0
                           &&  m_RW->m_Rsema2.Release() == 0,
                           "CRWLock::ReadLock() - invalid R-semaphore state");
            if (m_Count == 0) {
                xncbi_Validate(WaitForSingleObject(m_RW->m_Wsema,
                                                   0) == WAIT_OBJECT_0,
                               "CRWLock::ReadLock() - "
                               "failed to lock W-semaphore");
            }
#elif defined(NCBI_POSIX_THREADS)
            while ( !x_MayAcquireForReading(self_id) ) {
                xncbi_Validate(pthread_cond_wait(m_RW->m_Rcond,
                                                 m_RW->m_Mutex.GetHandle())
                               == 0,
                               "CRWLock::ReadLock() - R-lock waiting error");
            }
#else
            // Can not be already W-locked by another thread without MT
            xncbi_Validate(0,
                           "CRWLock::ReadLock() - "
                           "weird R-lock error in non-MT mode");
#endif
            xncbi_Validate(m_Count >= 0,
                           "CRWLock::ReadLock() - invalid readers counter");
            m_Count++;
        }
    }
    else {
#if defined(NCBI_WIN32_THREADS)
        if (m_Count == 0) {
            // Unlocked
            // Lock against writers
            xncbi_Validate(WaitForSingleObject(m_RW->m_Wsema, 0)
                           == WAIT_OBJECT_0,
                           "CRWLock::ReadLock() - "
                           "can not lock W-semaphore");
        }
#endif
        m_Count++;
    }

    // Remember new reader
    if ((m_Flags & fTrackReaders) != 0  &&  m_Count > 0) {
        m_Readers.push_back(self_id);
    }
#endif
}


bool CRWLock::TryReadLock(void)
{
#if defined(NCBI_NO_THREADS)
    return true;
#else

#if defined(NCBI_USE_CRITICAL_SECTION)
    CWin32MutexHandleGuard guard(m_RW->m_Mutex);
#else
    CFastMutexGuard guard(m_RW->m_Mutex);
#endif
    CThreadSystemID self_id = CThreadSystemID::GetCurrent();

    if ( !x_MayAcquireForReading(self_id) ) {
        if (m_Count >= 0  ||  m_Owner.IsNot(self_id)) {
            // (due to be) W-locked by another thread
            return false;
        }
        else {
            // W-locked, try to set R after W if in the same thread
            m_Count--;
            return true;
        }
    }

    // Unlocked - do R-lock
#if defined(NCBI_WIN32_THREADS)
    if (m_Count == 0) {
        // Lock W-semaphore in MSWIN
        xncbi_Validate(WaitForSingleObject(m_RW->m_Wsema, 0)
                       == WAIT_OBJECT_0,
                       "CRWLock::TryReadLock() - "
                       "can not lock W-semaphore");
    }
#endif
    m_Count++;
    if (m_Flags & fTrackReaders) {
        m_Readers.push_back(self_id);
    }
    return true;
#endif
}


void CRWLock::WriteLock(void)
{
#if defined(NCBI_NO_THREADS)
    return;
#else

#if defined(NCBI_USE_CRITICAL_SECTION)
    CWin32MutexHandleGuard guard(m_RW->m_Mutex);
#else
    CFastMutexGuard guard(m_RW->m_Mutex);
#endif
    CThreadSystemID self_id = CThreadSystemID::GetCurrent();

    if ( m_Count < 0 && m_Owner.Is(self_id) ) {
        // W-locked by the same thread
        m_Count--;
    }
    else {
        // Unlocked or RW-locked by another thread

        // Look in readers - must not be there
        xncbi_Validate(find(m_Readers.begin(), m_Readers.end(), self_id)
                       == m_Readers.end(),
                       "CRWLock::WriteLock() - "
                       "attempt to set W-after-R lock");

#if defined(NCBI_WIN32_THREADS)
        HANDLE obj[3];
        obj[0] = m_RW->m_Rsema;
        obj[1] = m_RW->m_Wsema;
        obj[2] = m_RW->m_Mutex.GetHandle();
        DWORD wait_res;
        if (m_Count == 0) {
            // Unlocked - lock both semaphores
            wait_res = WaitForMultipleObjects(2, obj, TRUE, 0);
            xncbi_Validate(wait_res >= WAIT_OBJECT_0  &&
                           wait_res < WAIT_OBJECT_0+2,
                           "CRWLock::WriteLock() - "
                           "error locking R&W-semaphores");
        }
        else {
            // Locked by another thread - wait for unlock
            if (m_Flags & fFavorWriters) {
                if (++m_WaitingWriters == 1) {
                    // First waiting writer - lock out readers
                    xncbi_Validate(WaitForSingleObject(m_RW->m_Rsema2, 0)
                                   == WAIT_OBJECT_0,
                                   "CRWLock::WriteLock() - "
                                   "error locking R-semaphore 2");
                }
            }
            xncbi_Validate(ReleaseMutex(m_RW->m_Mutex.GetHandle()),
                           "CRWLock::WriteLock() - release mutex error");
            wait_res = WaitForMultipleObjects(3, obj, TRUE, INFINITE);
            xncbi_Validate(wait_res >= WAIT_OBJECT_0  &&
                           wait_res < WAIT_OBJECT_0+3,
                           "CRWLock::WriteLock() - "
                           "error locking R&W-semaphores");
            if (m_Flags & fFavorWriters) {
                if (--m_WaitingWriters == 0) {
                    xncbi_Validate(m_RW->m_Rsema2.Release() == 0,
                                   "CRWLock::WriteLock() - "
                                   "invalid R-semaphore 2 state");
                    // Readers still won't be able to proceed, but releasing
                    // the semaphore here simplifies bookkeeping.
                }
            }
        }
#elif defined(NCBI_POSIX_THREADS)
        if (m_Flags & fFavorWriters) {
            m_WaitingWriters++;
        }
        while (m_Count != 0) {
            xncbi_Validate(pthread_cond_wait(m_RW->m_Wcond,
                                             m_RW->m_Mutex.GetHandle()) == 0,
                           "CRWLock::WriteLock() - "
                           "error locking R&W-conditionals");
        }
        if (m_Flags & fFavorWriters) {
            m_WaitingWriters--;
        }
#endif
        xncbi_Validate(m_Count >= 0,
                       "CRWLock::WriteLock() - invalid readers counter");
        m_Count = -1;
        m_Owner.Set(self_id);
    }

    // No readers allowed
    _ASSERT(m_Readers.empty());
#endif
}


bool CRWLock::TryWriteLock(void)
{
#if defined(NCBI_NO_THREADS)
    return true;
#else

#if defined(NCBI_USE_CRITICAL_SECTION)
    CWin32MutexHandleGuard guard(m_RW->m_Mutex);
#else
    CFastMutexGuard guard(m_RW->m_Mutex);
#endif
    CThreadSystemID self_id = CThreadSystemID::GetCurrent();

    if ( m_Count < 0 ) {
        // W-locked
        if ( m_Owner.IsNot(self_id) ) {
            // W-locked by another thread
            return false;
        }
        // W-locked by same thread
        m_Count--;
    }
    else if ( m_Count > 0 ) {
        // R-locked
        return false;
    }
    else {
        // Unlocked - do W-lock
#if defined(NCBI_WIN32_THREADS)
        // In MSWIN lock semaphores
        HANDLE obj[2];
        obj[0] = m_RW->m_Rsema;
        obj[1] = m_RW->m_Wsema;
        DWORD wait_res;
        wait_res = WaitForMultipleObjects(2, obj, TRUE, 0);
        xncbi_Validate(wait_res >= WAIT_OBJECT_0  &&
                       wait_res < WAIT_OBJECT_0 + 2,
                       "CRWLock::TryWriteLock() - "
                       "error locking R&W-semaphores");
#endif
        m_Count = -1;
        m_Owner.Set(self_id);
    }

    // No readers allowed
    _ASSERT(m_Readers.empty());

    return true;
#endif
}


void CRWLock::Unlock(void)
{
#if defined(NCBI_NO_THREADS)
    return;
#else

#if defined(NCBI_USE_CRITICAL_SECTION)
    CWin32MutexHandleGuard guard(m_RW->m_Mutex);
#else
    CFastMutexGuard guard(m_RW->m_Mutex);
#endif
    CThreadSystemID self_id = CThreadSystemID::GetCurrent();

    if (m_Count < 0) {
        // Check it is R-locked or W-locked by the same thread
        xncbi_Validate(m_Owner.Is(self_id),
                       "CRWLock::Unlock() - "
                       "RWLock is locked by another thread");
        if ( ++m_Count == 0 ) {
            // Unlock the last W-lock
#if defined(NCBI_WIN32_THREADS)
            xncbi_Validate(m_RW->m_Rsema.Release() == 0,
                           "CRWLock::Unlock() - invalid R-semaphore state");
            xncbi_Validate(m_RW->m_Wsema.Release() == 0,
                           "CRWLock::Unlock() - invalid R-semaphore state");
#elif defined(NCBI_POSIX_THREADS)
            if ( !m_WaitingWriters ) {
                xncbi_Validate(pthread_cond_broadcast(m_RW->m_Rcond) == 0,
                               "CRWLock::Unlock() - error signalling unlock");
            }
            xncbi_Validate(pthread_cond_signal(m_RW->m_Wcond) == 0,
                           "CRWLock::Unlock() - error signalling unlock");
#endif
        }
        if (m_Flags & fTrackReaders) {
            // Check if the unlocking thread is in the owners list
            _ASSERT(find(m_Readers.begin(), m_Readers.end(), self_id)
                    == m_Readers.end());
        }
    }
    else {
        xncbi_Validate(m_Count != 0,
                       "CRWLock::Unlock() - RWLock is not locked");
        if ( --m_Count == 0 ) {
            // Unlock the last R-lock
#if defined(NCBI_WIN32_THREADS)
            xncbi_Validate(m_RW->m_Wsema.Release() == 0,
                           "CRWLock::Unlock() - invalid W-semaphore state");
#elif defined(NCBI_POSIX_THREADS)
            xncbi_Validate(pthread_cond_signal(m_RW->m_Wcond) == 0,
                           "CRWLock::Unlock() - error signaling unlock");
#endif
        }
        if (m_Flags & fTrackReaders) {
            // Check if the unlocking thread is in the owners list
            vector<CThreadSystemID>::iterator found =
                find(m_Readers.begin(), m_Readers.end(), self_id);
            _ASSERT(found != m_Readers.end());
            m_Readers.erase(found);
            if ( m_Count == 0 ) {
                _ASSERT(m_Readers.empty());
            }
        }
    }
#endif
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
    xncbi_Validate(max_count != 0,
                   "CSemaphore::CSemaphore() - max_count passed zero");
    xncbi_Validate(init_count <= max_count,
                   "CSemaphore::CSemaphore() - init_count "
                   "greater than max_count");

    m_Sem = new SSemaphore;
    auto_ptr<SSemaphore> auto_sem(m_Sem);

#if defined(NCBI_POSIX_THREADS)
    m_Sem->max_count = max_count;
    m_Sem->count     = init_count;
    m_Sem->wait_count = 0;

    xncbi_Validate(pthread_mutex_init(&m_Sem->mutex, 0) == 0,
                   "CSemaphore::CSemaphore() - pthread_mutex_init() failed");
    xncbi_Validate(pthread_cond_init(&m_Sem->cond, 0) == 0,
                   "CSemaphore::CSemaphore() - pthread_cond_init() failed");
#elif defined(NCBI_WIN32_THREADS)
    m_Sem->sem = CreateSemaphore(NULL, init_count, max_count, NULL);
    xncbi_Validate(m_Sem->sem != NULL,
                   "CSemaphore::CSemaphore() - CreateSemaphore() failed");

#else
    m_Sem->max_count = max_count;
    m_Sem->count     = init_count;
#endif

    auto_sem.release();
}


CSemaphore::~CSemaphore(void)
{
#if defined(NCBI_POSIX_THREADS)
    _ASSERT(m_Sem->wait_count == 0);
    xncbi_Verify(pthread_mutex_destroy(&m_Sem->mutex) == 0);
    xncbi_Verify(pthread_cond_destroy(&m_Sem->cond)  == 0);

#elif defined(NCBI_WIN32_THREADS)
    xncbi_Verify( CloseHandle(m_Sem->sem) );
#endif

    delete m_Sem;
}


void CSemaphore::Wait(void)
{
#if defined(NCBI_POSIX_THREADS)
    xncbi_Validate(pthread_mutex_lock(&m_Sem->mutex) == 0,
                   "CSemaphore::Wait() - pthread_mutex_lock() failed");

    if (m_Sem->count != 0) {
        m_Sem->count--;
    }
    else {
        m_Sem->wait_count++;
        do {
            if (pthread_cond_wait(&m_Sem->cond, &m_Sem->mutex) != 0) {
                xncbi_Validate(pthread_mutex_unlock(&m_Sem->mutex) == 0,
                               "CSemaphore::Wait() - "
                               "pthread_cond_wait() and "
                               "pthread_mutex_unlock() failed");
                xncbi_Validate(0,
                               "CSemaphore::Wait() - "
                               "pthread_cond_wait() failed");
            }
        } while (m_Sem->count == 0);
        m_Sem->wait_count--;
        m_Sem->count--;
    }

    xncbi_Validate(pthread_mutex_unlock(&m_Sem->mutex) == 0,
                   "CSemaphore::Wait() - pthread_mutex_unlock() failed");

#elif defined(NCBI_WIN32_THREADS)
    xncbi_Validate(WaitForSingleObject(m_Sem->sem, INFINITE) == WAIT_OBJECT_0,
                   "CSemaphore::Wait() - WaitForSingleObject() failed");

#else
    xncbi_Validate(m_Sem->count != 0,
                   "CSemaphore::Wait() - "
                   "wait with zero count in one-thread mode(?!)");
    m_Sem->count--;
#endif
}

#if defined(NCBI_NO_THREADS)
# define NCBI_THREADS_ARG(arg) 
#else
# define NCBI_THREADS_ARG(arg) arg
#endif

bool CSemaphore::TryWait(unsigned int NCBI_THREADS_ARG(timeout_sec),
                         unsigned int NCBI_THREADS_ARG(timeout_nsec))
{
#if defined(NCBI_POSIX_THREADS)
    xncbi_Validate(pthread_mutex_lock(&m_Sem->mutex) == 0,
                   "CSemaphore::TryWait() - pthread_mutex_lock() failed");

    bool retval = false;
    if (m_Sem->count != 0) {
        m_Sem->count--;
        retval = true;
    }
    else if (timeout_sec > 0  ||  timeout_nsec > 0) {
# ifdef NCBI_OS_SOLARIS
        // arbitrary limit of 100Ms (~3.1 years) -- supposedly only for
        // native threads, but apparently also for POSIX threads :-/
        if (timeout_sec >= 100 * 1000 * 1000) {
            timeout_sec  = 100 * 1000 * 1000;
            timeout_nsec = 0;
        }
# endif
        static const unsigned int kBillion = 1000 * 1000 * 1000;
        struct timeval  now;
        struct timespec timeout = { 0, 0 };
        gettimeofday(&now, 0);
        // timeout_sec added below to avoid overflow
        timeout.tv_sec  = now.tv_sec;
        timeout.tv_nsec = now.tv_usec * 1000 + timeout_nsec;
        if ((unsigned int)timeout.tv_nsec >= kBillion) {
            timeout.tv_sec  += timeout.tv_nsec / kBillion;
            timeout.tv_nsec %= kBillion;
        }
        if (timeout_sec > (unsigned int)(kMax_Int - timeout.tv_sec)) {
            // Max out rather than overflowing
            timeout.tv_sec  = kMax_Int;
            timeout.tv_nsec = kBillion - 1;
        } else {
            timeout.tv_sec += timeout_sec;
        }
        
        m_Sem->wait_count++;
        do {
            int status = pthread_cond_timedwait(&m_Sem->cond, &m_Sem->mutex,
                                                &timeout);
            if (status == ETIMEDOUT) {
                break;
            } else if (status != 0  &&  status != EINTR) {
                // EINVAL, presumably?
                xncbi_Validate(pthread_mutex_unlock(&m_Sem->mutex) == 0,
                               "CSemaphore::TryWait() - "
                               "pthread_cond_timedwait() and "
                               "pthread_mutex_unlock() failed");
                xncbi_Validate(0, "CSemaphore::TryWait() - "
                               "pthread_cond_timedwait() failed");
            }
        } while (m_Sem->count == 0);
        m_Sem->wait_count--;
        if (m_Sem->count != 0) {
            m_Sem->count--;
            retval = true;
        }
    }

    xncbi_Validate(pthread_mutex_unlock(&m_Sem->mutex) == 0,
                   "CSemaphore::TryWait() - pthread_mutex_unlock() failed");

    return retval;

#elif defined(NCBI_WIN32_THREADS)
    DWORD timeout_msec; // DWORD == unsigned long
    if (timeout_sec >= kMax_ULong / 1000) {
        timeout_msec = kMax_ULong;
    } else {
        timeout_msec = timeout_sec * 1000 + timeout_nsec / (1000 * 1000);
    }
    DWORD res = WaitForSingleObject(m_Sem->sem, timeout_msec);
    xncbi_Validate(res == WAIT_OBJECT_0  ||  res == WAIT_TIMEOUT,
                   "CSemaphore::TryWait() - WaitForSingleObject() failed");
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
    xncbi_Validate(pthread_mutex_lock(&m_Sem->mutex) == 0,
                   "CSemaphore::Post() - pthread_mutex_lock() failed");

    if (m_Sem->count > kMax_UInt - count  ||
        m_Sem->count + count > m_Sem->max_count) {
        xncbi_Validate(pthread_mutex_unlock(&m_Sem->mutex) == 0,
                       "CSemaphore::Post() - "
                       "attempt to exceed max_count and "
                       "pthread_mutex_unlock() failed");
        xncbi_Validate(m_Sem->count <= kMax_UInt - count,
                       "CSemaphore::Post() - "
                       "would result in counter > MAX_UINT");
        xncbi_Validate(m_Sem->count + count <= m_Sem->max_count,
                       "CSemaphore::Post() - attempt to exceed max_count");
        _TROUBLE;
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
        xncbi_Validate(pthread_mutex_unlock(&m_Sem->mutex) == 0,
                       "CSemaphore::Post() - pthread_mutex_unlock() failed");
        return;
    }

    // Error
    xncbi_Validate(pthread_mutex_unlock(&m_Sem->mutex) == 0,
                   "CSemaphore::Post() - "
                   "pthread_cond_signal/broadcast() and "
                   "pthread_mutex_unlock() failed");
    xncbi_Validate(0,
                   "CSemaphore::Post() - "
                   "pthread_cond_signal/broadcast() failed");

#elif defined(NCBI_WIN32_THREADS)
    xncbi_Validate(ReleaseSemaphore(m_Sem->sem, count, NULL),
                   "CSemaphore::Post() - ReleaseSemaphore() failed");

#else
    xncbi_Validate(m_Sem->count + count <= m_Sem->max_count,
                   "CSemaphore::Post() - attempt to exceed max_count");
    m_Sem->count += count;
#endif
}


END_NCBI_SCOPE
