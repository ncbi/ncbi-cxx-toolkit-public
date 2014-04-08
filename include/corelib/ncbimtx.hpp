#ifndef CORELIB___NCBIMTX__HPP
#define CORELIB___NCBIMTX__HPP

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
 * Author:  Denis Vakatov, Aleksey Grichenko, Andrei Gourianov
 *
 *
 */

/// @file ncbimtx.hpp
/// Multi-threading -- mutexes;  rw-locks; semaphore
///
///   MUTEX:
///
/// MUTEX CLASSES:
/// - SSystemFastMutex -- platform-dependent mutex functionality
/// - SSystemMutex     -- platform-dependent mutex functionality
/// - CFastMutex       -- simple mutex with fast lock/unlock functions
/// - CMutex           -- mutex that allows nesting (with runtime checks)
/// - CFastMutexGuard  -- acquire fast mutex, then guarantee for its release
/// - CMutexGuard      -- acquire mutex, then guarantee for its release
///
/// RW-LOCK:
/// - CInternalRWLock  -- platform-dependent RW-lock structure (fwd-decl)
/// - CRWLock          -- Read/Write lock related  data and methods
/// - CAutoRW          -- guarantee RW-lock release
/// - CReadLockGuard   -- acquire R-lock, then guarantee for its release
/// - CWriteLockGuard  -- acquire W-lock, then guarantee for its release
///
/// CONDITION VARIABLE:
/// - CConditionVariable -- condition variable
///
/// SEMAPHORE:
/// - CSemaphore       -- application-wide semaphore
///


#include <corelib/ncbithr_conf.hpp>
#include <corelib/guard.hpp>
#include <corelib/ncbiobj.hpp>
#include <corelib/ncbitime.hpp>
#include <memory>
#include <deque>
#ifdef NCBI_COMPILER_MSVC
# include <intrin.h>
# pragma intrinsic(_ReadWriteBarrier)
#endif


/** @addtogroup Threads
 *
 * @{
 */


#if defined(_DEBUG)
/// Mutex debug setting.
#   define  INTERNAL_MUTEX_DEBUG
#else
#   undef   INTERNAL_MUTEX_DEBUG
/// Mutex debug setting.
#   define  INTERNAL_MUTEX_DEBUG
#endif


BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
//
// Log mutex events if LOG_MUTEX_EVENTS is defined.
//
//    The mutex events (create/destroy/lock/unlock) are logged into
//    ./mutex_events.log or the one specified in MUTEX_EVENTS_LOG_FILE
//    env. variable.
//

#if defined(_DEBUG) &&  defined(LOG_MUTEX_EVENTS)

// Logging function, prints pointer to the mutex, system thread ID
// and the message.
NCBI_XNCBI_EXPORT void WriteMutexEvent(void* mutex_ptr, const char* message);
#  define WRITE_MUTEX_EVENT(mutex, message) WriteMutexEvent(mutex, message)

#else

#  define WRITE_MUTEX_EVENT(mutex, message) ((void)0)

#endif


/////////////////////////////////////////////////////////////////////////////
//
// DECLARATIONS of internal (platform-dependent) representations
//
//    TMutex         -- internal mutex type
//

#if defined(NCBI_NO_THREADS)

/// Define a platform independent system mutex.
typedef int TSystemMutex; // fake
#  define SYSTEM_MUTEX_INITIALIZER 0

#elif defined(NCBI_POSIX_THREADS)

/// Define a platform independent system mutex.
typedef pthread_mutex_t TSystemMutex;
#  define SYSTEM_MUTEX_INITIALIZER PTHREAD_MUTEX_INITIALIZER

#elif defined(NCBI_WIN32_THREADS)

#  define NCBI_USE_CRITICAL_SECTION

/// Define a platform independent system mutex.
#  if defined(NCBI_USE_CRITICAL_SECTION)
typedef CRITICAL_SECTION TSystemMutex;
#  else
typedef HANDLE TSystemMutex;
#  endif

#  undef SYSTEM_MUTEX_INITIALIZER

#else
#  error "Unknown threading model."
#endif



/////////////////////////////////////////////////////////////////////////////
///
/// CThreadSystemID --
///
/// Define thread system ID.
///
/// The CThreadSystemID is based on the platform dependent thread ID type,
/// TThreadSystemID, defined in ncbithr_conf.hpp.

class NCBI_XNCBI_EXPORT CThreadSystemID
{
public:
    /// Define a simpler alias for TThreadSystemID.
    typedef TThreadSystemID TID;

    TID m_ID;           ///< Thread ID.

    /// Get the current thread ID.
    static CThreadSystemID GetCurrent(void)
        {
            CThreadSystemID tid;
#if defined(NCBI_NO_THREADS)
            tid.m_ID = TID(0);
#elif defined(NCBI_POSIX_THREADS)
            tid.m_ID = pthread_self();
#elif defined(NCBI_WIN32_THREADS)
            tid.m_ID = GetCurrentThreadId();
#endif
            return tid;
        }

    /// Equality operator for thread ID.
    bool operator==(const CThreadSystemID& tid) const
        {
            return m_ID == tid.m_ID;
        }

    /// Non-equality operator for thread ID.
    bool operator!=(const CThreadSystemID& tid) const
        {
            return m_ID != tid.m_ID;
        }

    /// volatile versions of above methods
    void Set(const CThreadSystemID& tid) volatile
        {
            m_ID = tid.m_ID;
        }
    bool Is(const CThreadSystemID& tid) const volatile
        {
            return m_ID == tid.m_ID;
        }
    bool IsNot(const CThreadSystemID& tid) const volatile
        {
            return m_ID != tid.m_ID;
        }
};


/// Use in defining initial value of system mutex.
#define THREAD_SYSTEM_ID_INITIALIZER { 0 }



/////////////////////////////////////////////////////////////////////////////
///
/// CMutexException --
///
/// Define exceptions generated by mutexes.
///
/// CMutexException inherits its basic functionality from CCoreException
/// and defines additional error codes for applications.

class NCBI_XNCBI_EXPORT CMutexException : public CCoreException
{
public:
    /// Error types that a mutex can generate.
    enum EErrCode {
        eLock,          ///< Lock error
        eUnlock,        ///< Unlock error
        eTryLock,       ///< Attempted lock error
        eOwner,         ///< Not owned error
        eUninitialized  ///< Uninitialized error
    };

    /// Translate from the error code value to its string representation.
    virtual const char* GetErrCodeString(void) const;

    // Standard exception boilerplate code.
    NCBI_EXCEPTION_DEFAULT(CMutexException,CCoreException);
};

/////////////////////////////////////////////////////////////////////////////
//
//  SYSTEM MUTEX
//
//    SSystemFastMutex
//    SSystemMutex
//

class CFastMutex;



/////////////////////////////////////////////////////////////////////////////
///
/// SSystemFastMutex --
///
/// Define system fast mutex.
///
/// Internal platform-dependent fast mutex implementation to be used by CMutex
/// and CFastMutex only.

struct SSystemFastMutex
{
    TSystemMutex m_Handle;      ///< Mutex handle

    /// Initialization flag values.
    enum EMagic {
        eMutexUninitialized = 0,        ///< Uninitialized value.
        eMutexInitialized = 0x2487adab  ///< Magic initialized value,
    };
    volatile EMagic m_Magic;    ///< Magic flag

    /// This is for condition variables
    enum ELockSemantics
    {
        eNormal, /// Modify object data and call system
        ePseudo  /// Modify object data, but do not call system
    };

    /// Acquire mutex for the current thread with no nesting checks.
    NCBI_XNCBI_EXPORT
    void Lock(ELockSemantics lock = eNormal);

    /// Release mutex with no owner or nesting checks.
    NCBI_XNCBI_EXPORT
    void Unlock(ELockSemantics lock = eNormal);

    /// Try to lock.
    /// 
    /// @return
    ///   TRUE on success; FALSE, otherwise.
    NCBI_XNCBI_EXPORT
    bool TryLock(void);

    /// Check initialized value of mutex.
    void CheckInitialized(void) const;

    // Methods for throwing exceptions, to make inlined methods lighter

    /// Throw uninitialized ("eUninitialized") exception.
    NCBI_XNCBI_EXPORT NCBI_NORETURN
    static void ThrowUninitialized(void);

    /// Throw lock failed("eLocked") exception.
    NCBI_XNCBI_EXPORT NCBI_NORETURN
    static void ThrowLockFailed(void);

    /// Throw unlock failed("eUnlocked") exception.
    NCBI_XNCBI_EXPORT NCBI_NORETURN
    static void ThrowUnlockFailed(void);

    /// Throw try lock failed("eTryLock") exception.
    NCBI_XNCBI_EXPORT NCBI_NORETURN
    static void ThrowTryLockFailed(void);

#if !defined(NCBI_OS_MSWIN)
    // MS VC 6 treats classes with any non-public member as non-POD.
protected:
#endif

    /// Check if mutex is initialized.
    /// 
    /// @return
    ///   TRUE if initialized; FALSE, otherwise.
    bool IsInitialized(void) const;

    /// Check if mutex is un-initialized.
    /// 
    /// @return
    ///   TRUE if un-initialized; FALSE, otherwise.
    bool IsUninitialized(void) const;

    /// Initialize static mutex. 
    ///
    /// Must be called only once.
    NCBI_XNCBI_EXPORT
    void InitializeStatic(void);

    /// Initialize dynamic mutex.
    ///
    /// Initialize mutex if it located in heap or stack. This must be called
    /// only once.  Do not count on zeroed memory values for initializing
    /// mutex values.
    NCBI_XNCBI_EXPORT
    void InitializeDynamic(void);

    /// Destroy mutex.
    NCBI_XNCBI_EXPORT
    void Destroy(void);

    /// Initialize mutex handle.
    ///
    /// Must be called only once.
    NCBI_XNCBI_EXPORT
    void InitializeHandle(void);

    /// Destroy mutex handle.
    ///
    /// Must be called only once.
    NCBI_XNCBI_EXPORT
    void DestroyHandle(void);

    friend struct SSystemMutex;
    friend class CAutoInitializeStaticFastMutex;

    friend class CFastMutex;

    friend class CSafeStaticPtr_Base;
};

/// typedefs for ease of use
typedef CGuard<SSystemFastMutex> TFastMutexGuard;

/// ...and backward compatibility
typedef TFastMutexGuard          CFastMutexGuard;


class CMutex;



/////////////////////////////////////////////////////////////////////////////
///
/// SSystemMutex --
///
/// Define system mutex.
///
/// Internal platform-dependent mutex implementation to be used by CMutex
/// and CFastMutex only.

struct SSystemMutex
{
    SSystemFastMutex m_Mutex; ///< Mutex value

    volatile CThreadSystemID m_Owner; ///< Platform-dependent owner thread ID

    volatile int m_Count; ///< # of recursive (in the same thread) locks

    /// Acquire mutex for the current thread.
    NCBI_XNCBI_EXPORT
    void Lock
    (SSystemFastMutex::ELockSemantics lock = SSystemFastMutex::eNormal);

    /// Release mutex.
    NCBI_XNCBI_EXPORT
    void Unlock
    (SSystemFastMutex::ELockSemantics lock = SSystemFastMutex::eNormal);

    /// Try to lock.
    /// 
    /// @return
    ///   TRUE on success; FALSE, otherwise.
    NCBI_XNCBI_EXPORT
    bool TryLock(void);

    // Methods for throwing exceptions, to make inlined methods lighter
    // throw exception eOwner

    /// Throw not owned("eOwner") exception.
    NCBI_XNCBI_EXPORT NCBI_NORETURN
    static void ThrowNotOwned(void);

#if !defined(NCBI_OS_MSWIN)
protected:
#endif
    /// Check if mutex is initialized.
    /// 
    /// @return
    ///   TRUE if initialized; FALSE, otherwise.
    bool IsInitialized(void) const;

    /// Check if mutex is un-initialized.
    /// 
    /// @return
    ///   TRUE if un-initialized; FALSE, otherwise.
    bool IsUninitialized(void) const;

    /// Initialize static mutex. 
    ///
    /// Must be called only once.
    void InitializeStatic(void);

    /// Initialize dynamic mutex.
    ///
    /// Initialize mutex if it located in heap or stack. This must be called
    /// only once.  Do not count on zeroed memory values for initializing
    /// mutex values.
    void InitializeDynamic(void);

    /// Destroy mutex.
    NCBI_XNCBI_EXPORT
    void Destroy(void);

    friend class CAutoInitializeStaticMutex;
    friend class CMutex;
};


/// typedefs for ease of use
typedef CGuard<SSystemMutex> TMutexGuard;

/// ...and backward compatibility
typedef TMutexGuard          CMutexGuard;


/// Determine type of system mutex initialization.
#if defined(SYSTEM_MUTEX_INITIALIZER)

/// Define static fast mutex initial value.
#   define STATIC_FAST_MUTEX_INITIALIZER \
    { SYSTEM_MUTEX_INITIALIZER, NCBI_NS_NCBI::SSystemFastMutex::eMutexInitialized }

/// Define static fast mutex and initialize it.
#   define DEFINE_STATIC_FAST_MUTEX(id) \
static NCBI_NS_NCBI::SSystemFastMutex id = STATIC_FAST_MUTEX_INITIALIZER

/// Declare static fast mutex.
#   define DECLARE_CLASS_STATIC_FAST_MUTEX(id) \
static NCBI_NS_NCBI::SSystemFastMutex id

/// Define fast mutex and initialize it.
#   define DEFINE_CLASS_STATIC_FAST_MUTEX(id) \
NCBI_NS_NCBI::SSystemFastMutex id = STATIC_FAST_MUTEX_INITIALIZER

/// Define static mutex initializer.
#   define STATIC_MUTEX_INITIALIZER \
    { STATIC_FAST_MUTEX_INITIALIZER, THREAD_SYSTEM_ID_INITIALIZER, 0 }

/// Define static mutex and initialize it.
#   define DEFINE_STATIC_MUTEX(id) \
static NCBI_NS_NCBI::SSystemMutex id = STATIC_MUTEX_INITIALIZER

/// Declare static mutex.
#   define DECLARE_CLASS_STATIC_MUTEX(id) \
static NCBI_NS_NCBI::SSystemMutex id

/// Define mutex and initialize it.
#   define DEFINE_CLASS_STATIC_MUTEX(id) \
NCBI_NS_NCBI::SSystemMutex id = STATIC_MUTEX_INITIALIZER

#else

/// Auto initialization for mutex will be used.
#   define NEED_AUTO_INITIALIZE_MUTEX

/// Define auto-initialized static fast mutex.
#   define DEFINE_STATIC_FAST_MUTEX(id) \
static NCBI_NS_NCBI::CAutoInitializeStaticFastMutex id

/// Declare auto-initialized static fast mutex.
#   define DECLARE_CLASS_STATIC_FAST_MUTEX(id) \
static NCBI_NS_NCBI::CAutoInitializeStaticFastMutex id

/// Define auto-initialized mutex.
#   define DEFINE_CLASS_STATIC_FAST_MUTEX(id) \
NCBI_NS_NCBI::CAutoInitializeStaticFastMutex id

/// Define auto-initialized static mutex.
#   define DEFINE_STATIC_MUTEX(id) \
static NCBI_NS_NCBI::CAutoInitializeStaticMutex id

/// Declare auto-initialized static mutex.
#   define DECLARE_CLASS_STATIC_MUTEX(id) \
static NCBI_NS_NCBI::CAutoInitializeStaticMutex id

/// Define auto-initialized mutex.
#   define DEFINE_CLASS_STATIC_MUTEX(id) \
NCBI_NS_NCBI::CAutoInitializeStaticMutex id

#endif



#if defined(NEED_AUTO_INITIALIZE_MUTEX)



/////////////////////////////////////////////////////////////////////////////
///
/// CAutoInitializeStaticFastMutex --
///
/// Define thread safe initializer static for SSystemFastMutex. 
///
/// Needed on platforms where system mutex struct cannot be initialized at
/// compile time (e.g. Win32).

class CAutoInitializeStaticFastMutex
{
public:
    typedef SSystemFastMutex TObject; ///< Simplified alias name for fast mutex

    /// Lock mutex.
    void Lock(void);

    /// Unlock mutex.
    void Unlock(void);

    /// Try locking the mutex.
    bool TryLock(void);

    /// Return initialized mutex object.
    operator TObject&(void);

protected:
    /// Initialize mutex.
    ///
    /// This method can be called many times it will return only after
    /// successful initialization of m_Mutex.
    NCBI_XNCBI_EXPORT
    void Initialize(void);

    /// Get initialized mutex object.
    TObject& Get(void);

private:
    TObject m_Mutex;                ///< Mutex object.
};



/////////////////////////////////////////////////////////////////////////////
///
/// CAutoInitializeStaticMutex --
///
/// Define thread safe initializer static for SSystemMutex. 
///
/// Needed on platforms where system mutex struct cannot be initialized at
/// compile time (e.g. Win32).

class CAutoInitializeStaticMutex
{
public:
    typedef SSystemMutex TObject;   ///< Simplified alias name for fast mutex

    /// Lock mutex.
    void Lock(void);

    /// Unlock mutex.
    void Unlock(void);

    /// Try locking the mutex.
    bool TryLock(void);

    /// Return initialized mutex object.
    operator TObject&(void);

protected:
    /// Initialize mutex.
    ///
    /// This method can be called many times it will return only after
    /// successful initialization of m_Mutex.
    NCBI_XNCBI_EXPORT
    void Initialize(void);

    /// Get initialized mutex object.
    TObject& Get(void);

private:
    TObject m_Mutex;                ///< Mutex object.
};

#endif

/////////////////////////////////////////////////////////////////////////////
//
//  FAST MUTEX
//
//    CFastMutex::
//    CFastMutexGuard::
//



/////////////////////////////////////////////////////////////////////////////
///
/// CFastMutex --
///
/// Simple mutex with fast lock/unlock functions.
///
/// This mutex can be used instead of CMutex if it's guaranteed that
/// there is no nesting. This mutex does not check nesting or owner.
/// It has better performance than CMutex, but is less secure.

class CFastMutex
{
public:
    /// Constructor.
    ///
    /// Creates mutex handle.
    CFastMutex(void);

    /// Destructor.
    ///
    /// Close mutex handle. No checks if it's still acquired.
    ~CFastMutex(void);

    /// Define Read Lock Guard.
    typedef CFastMutexGuard TReadLockGuard;

    /// Define Write Lock Guard.
    typedef CFastMutexGuard TWriteLockGuard;

    /// Acquire mutex for the current thread with no nesting checks.
    void Lock(void);

    /// Release mutex with no owner or nesting checks.
    void Unlock(void);

    /// Try locking the mutex.
    bool TryLock(void);

    /// Get SSystemFastMutex.
    operator SSystemFastMutex&(void);

private:
#if   !defined(NCBI_WIN32_THREADS)
    /// Get handle - Unix version.
    /// 
    /// Also used by CRWLock.
    TSystemMutex* GetHandle(void) { return &m_Mutex.m_Handle; }
#elif !defined(NCBI_USE_CRITICAL_SECTION)
    /// Get handle - Windows version.
    /// 
    /// Also used by CRWLock.
    HANDLE GetHandle(void) { return m_Mutex.m_Handle; }
#endif

    friend class CRWLock;

    /// Platform-dependent mutex handle, also used by CRWLock.
    SSystemFastMutex m_Mutex;

    /// Private copy constructor to disallow initialization.
    CFastMutex(const CFastMutex&);

    /// Private assignment operator to disallow assignment.
    CFastMutex& operator= (const CFastMutex&);
};



/////////////////////////////////////////////////////////////////////////////
//
//  MUTEX
//
//    CMutex::
//    CMutexGuard::
//



/////////////////////////////////////////////////////////////////////////////
///
/// CMutex --
///
/// Mutex that allows nesting with runtime checks.
///
/// Allows for recursive locks by the same thread. Checks the mutex
/// owner before unlocking. This mutex should be used when performance
/// is less important than data protection. For faster performance see
/// CFastMutex.
///
/// @sa
///   http://www.ncbi.nlm.nih.gov/books/n/toolkit/ch_core#ch_core.mutexes

class CMutex
{
public:
    /// Constructor.
    CMutex(void);

    /// Destructor.
    ///
    /// Report error if the mutex is locked.
    ~CMutex(void);

    /// Define Read Lock Guard.
    typedef CMutexGuard TReadLockGuard;

    /// Define Write Lock Guard.
    typedef CMutexGuard TWriteLockGuard;

    /// Get SSystemMutex.
    operator SSystemMutex&(void);

    /// Lock mutex.
    ///
    /// Operation:
    /// - If the mutex is unlocked, then acquire it for the calling thread.
    /// - If the mutex is acquired by this thread, then increase the
    /// lock counter (each call to Lock() must have corresponding
    /// call to Unlock() in the same thread).
    /// - If the mutex is acquired by another thread, then wait until it's
    /// unlocked, then act like a Lock() on an unlocked mutex.
    void Lock(void);

    /// Try locking mutex.
    ///
    /// Try to acquire the mutex.
    /// @return
    ///  TRUE if succesfully acquired;  FALSE otherwise (e.g if the mutex is
    ///  already acquired by another thread).
    /// @sa
    ///   Lock()
    bool TryLock(void);

    /// Unlock mutex.
    ///
    /// Operation:
    /// - If the mutex is acquired by this thread, then decrease the lock
    ///   counter.
    /// - If the lock counter becomes zero, then release the mutex completely.
    /// - Report error if the mutex is not locked or locked by another thread.
    void Unlock(void);

private:
    SSystemMutex m_Mutex;    ///< System mutex

    /// Private copy constructor to disallow initialization.
    CMutex(const CMutex&);

    /// Private assignment operator to disallow assignment.
    CMutex& operator= (const CMutex&);

    friend class CRWLock; ///< Allow use of m_Mtx and m_Owner members directly
};



/////////////////////////////////////////////////////////////////////////////
///
/// CNoMutex --
///
/// Fake mutex that does not lock anything.
///
/// Allows to create template classes which use CMutex/CFastMutex/CNoMutex as
/// an argument. In case of CNoMutex no real locking is performed.
///
/// @sa
///   CNoLock

typedef CNoLock CNoMutex;


class CSpinLock;
typedef CGuard<CSpinLock>  CSpinGuard;

/////////////////////////////////////////////////////////////////////////////
///
/// CSpinLock --
///
/// Simple lock with lock/unlock functions even faster than in CFastMutex.
///
/// This mutex can be used instead of CFastMutex when it's guaranteed that
/// lock will be always held for a short period of time. CSpinLock doesn't do
/// any system calls to wait for lock acquiring and relies on a fact that a
/// couple of thread reschedulings will be enough for another thread to
/// release the lock. As with CFastMutex no recursive locks are allowed.

class CSpinLock
{
public:
    CSpinLock(void);
    ~CSpinLock(void);

    /// Define Read Lock Guard.
    typedef CSpinGuard TReadLockGuard;
    /// Define Write Lock Guard.
    typedef CSpinGuard TWriteLockGuard;

    /// Lock the mutex
    NCBI_XNCBI_EXPORT
    void Lock(void);
    /// Attempt to lock the mutex and return TRUE if it succeeded or FALSE if
    /// mutex is locked by other thread.
    NCBI_XNCBI_EXPORT
    bool TryLock(void);
    /// Unlock the mutex.
    /// There's no check that mutex is unlocked by the same thread that
    /// locked it.
    NCBI_XNCBI_EXPORT
    void Unlock(void);

    /// Check if mutex is currently locked
    bool IsLocked(void) const;

private:
    /// Prohibit copying of the object
    CSpinLock(const CSpinLock&);
    CSpinLock& operator= (const CSpinLock&);

    /// Flag showing if mutex is locked (non-NULL value) or unlocked
    /// (NULL value).
    void* volatile m_Value;
};



/////////////////////////////////////////////////////////////////////////////
//
//  RW-LOCK
//
//    CRWLock::
//    CAutoRW::
//    CReadLockGuard::
//    CWriteLockGuard::
//


// Forward declaration of internal (platform-dependent) RW-lock representation
class CInternalRWLock;
class CRWLock;
//class CReadLockGuard;
//class CWriteLockGuard;



/////////////////////////////////////////////////////////////////////////////
///
/// SSimpleReadLock --
///
/// Acquire a read lock
template <class Class>
struct SSimpleReadLock
{
    void operator()(Class& inst) const
    {
        inst.ReadLock();
    }
};

typedef CGuard<CRWLock, SSimpleReadLock<CRWLock> > TReadLockGuard;
typedef TReadLockGuard                             CReadLockGuard;


/////////////////////////////////////////////////////////////////////////////
///
/// SSimpleWriteLock --
///
/// Acquire a write lock
template <class Class>
struct SSimpleWriteLock
{
    void operator()(Class& inst) const
    {
        inst.WriteLock();
    }
};

typedef CGuard<CRWLock, SSimpleWriteLock<CRWLock> > TWriteLockGuard;
typedef TWriteLockGuard                             CWriteLockGuard;


/////////////////////////////////////////////////////////////////////////////
///
/// CRWLock --
///
/// Read/Write lock.
/// Allows multiple readers or single writer.
///
/// NOTE about recursive locks by the same thread:
///  - W-after-W and R-after-R are okay.
///  - R-after-W is considered to be a recursive Write-lock.
///  - W-after-R is not allowed; note that if  _DEBUG is not defined we do not
///    always catch W-after-R, and deadlock may happen. Test your application
///    in _DEBUG mode first!

class NCBI_XNCBI_EXPORT CRWLock
{
public:
    /// Flags (passed at construction time) for fine-tuning lock behavior.
    enum EFlags {
        /// Forbid further readers from acquiring the lock if any writers
        /// are waiting for it, to keep would-be writers from starving.
        fFavorWriters = 0x1
    };
    typedef int TFlags; ///< binary OR of EFlags

    /// Constructor.
    CRWLock(TFlags flags = 0);

    /// Destructor.
    ~CRWLock(void);

    /// Define Read Lock Guard.
    typedef CReadLockGuard TReadLockGuard;

    /// Define Write Lock Guard.
    typedef CWriteLockGuard TWriteLockGuard;

    /// Read lock.
    ///
    /// Acquire the R-lock. If W-lock is already acquired by
    /// another thread, then wait until it is released.
    void ReadLock(void);

    /// Write lock.
    ///
    /// Acquire the W-lock. If R-lock or W-lock is already acquired by
    /// another thread, then wait until it is released.
    void WriteLock(void);

    /// Try read lock.
    ///
    /// Try to acquire R-lock and return immediately.
    /// @return
    ///   TRUE if the R-lock has been successfully acquired;
    ///   FALSE, otherwise.
    bool TryReadLock(void);

    /// Try read lock with timeout.
    /// @param timeout
    ///   Timeout for the read lock attempt.
    /// @return
    ///   TRUE if the R-lock has been successfully acquired;
    ///   FALSE, otherwise.
    bool TryReadLock(const CTimeout& timeout);

    /// Try write lock.
    ///
    /// Try to acquire W-lock and return immediately.
    /// @return
    ///   TRUE if the W-lock has been successfully acquired;
    ///   FALSE, otherwise.
    bool TryWriteLock(void);

    /// Try write lock with timeout.
    /// @param timeout
    ///   Timeout for the write lock attempt.
    /// @return
    ///   TRUE if the W-lock has been successfully acquired;
    ///   FALSE, otherwise.
    bool TryWriteLock(const CTimeout& timeout);

    /// Release the RW-lock.
    void Unlock(void);

private:
    enum EInternalFlags {
        /// Keep track of which threads have read locks
        fTrackReaders = 0x40000000
    };

    TFlags                     m_Flags; ///< Configuration flags

    auto_ptr<CInternalRWLock>  m_RW;    ///< Platform-dependent RW-lock data

    volatile CThreadSystemID   m_Owner; ///< Writer ID, one of the readers ID

    volatile long              m_Count; ///< Number of readers (if >0) or
                                        ///< writers (if <0)

    volatile unsigned int      m_WaitingWriters; ///< Number of writers waiting;
                                                 ///< zero if not keeping track

    vector<CThreadSystemID>    m_Readers;   ///< List of all readers or writers
                                            ///< for debugging

    bool x_MayAcquireForReading(CThreadSystemID self_id);

    /// Private copy constructor to disallow initialization.
    CRWLock(const CRWLock&);

    /// Private assignment operator to disallow assignment.
    CRWLock& operator= (const CRWLock&);
};


class CFastRWLock;

/////////////////////////////////////////////////////////////////////////////
///
/// SSimpleReadUnlock --
///
/// Release a read lock

template <class Class>
struct SSimpleReadUnlock
{
    void operator()(Class& inst) const
    {
        inst.ReadUnlock();
    }
};

typedef CGuard< CFastRWLock,
                SSimpleReadLock  <CFastRWLock>,
                SSimpleReadUnlock<CFastRWLock> >  CFastReadGuard;

/////////////////////////////////////////////////////////////////////////////
///
/// SSimpleWriteUnlock --
///
/// Release a write lock

template <class Class>
struct SSimpleWriteUnlock
{
    void operator()(Class& inst) const
    {
        inst.WriteUnlock();
    }
};

typedef CGuard< CFastRWLock,
                SSimpleWriteLock  <CFastRWLock>,
                SSimpleWriteUnlock<CFastRWLock> > CFastWriteGuard;


/////////////////////////////////////////////////////////////////////////////
///
/// CFastRWLock --
///
/// Fast implementation of Read/Write lock.
///
/// Allows multiple readers or single writer. Behaves similar to CRWLock
/// though with some assumptions and limitations:
/// - Does not try to detect recursive locks, actually any attempt to lock
///   recursively will end up in undefined behavior (the only exception is
///   read-after-read lock - this is ok).
/// - Does not remember when and where it was actually locked, so that it will
///   not be able to recover after call to ReadUnlock() or WriteUnlock() if it
///   was not preceded by call to corresponding Lock() method. So the best way
///   to use it is just via CFastReadGuard and CFastWriteGuard.
/// - Assumes that read lock is always held for a very small amount of time,
///   though write lock can be held as long as you want. Failure to comply
///   with this assumption will result in heavy CPU usage when trying to
///   acquire write lock while long read lock is held.
/// - Assumes that write lock is taken pretty rarely so that there is no
///   possibility of their overlapping (actually it easily resolves
///   overlapping though with possibility of readers starvation).
/// - As a consequence of previous assumption assumes that writer
///   "favoredness" (see fFavorWriters in CRWLock) is always needed.
/// - Assumes that there will not be too much simultaneous read locks
///   acquired, at least not more than 1048576.
///
/// All these assumptions and limitations allowed to make extremely
/// lightweight implementation of RWLock where the most common operation -
/// read lock and unlock - is performed very quickly without blocking each
/// other.

class NCBI_XNCBI_EXPORT CFastRWLock
{
public:
    typedef CFastReadGuard  TReadLockGuard;
    typedef CFastWriteGuard TWriteLockGuard;

    CFastRWLock(void);
    ~CFastRWLock(void);

    /// Acquire read lock
    void ReadLock(void);
    /// Release read lock
    void ReadUnlock(void);

    /// Acquire write lock
    void WriteLock(void);
    /// Release write lock
    void WriteUnlock(void);

private:
    CFastRWLock(const CFastRWLock&);
    CFastRWLock& operator= (const CFastRWLock&);

    enum {
        /// Number in lock count showing that write lock is acquired.
        kWriteLockValue = 0x100000
    };

    /// Number of read locks acquired or value of kWriteLockValue if write
    /// lock was acquired
    CAtomicCounter  m_LockCount;
    /// Mutex implementing write lock
    CFastMutex      m_WriteLock;
};



class CYieldingRWLock;
class CRWLockHolder;

/// Type of locking provided by CYieldingRWLock
enum ERWLockType
{
    eReadLock  = 0,
    eWriteLock = 1
};


/// Interface for receiving messages about state changes in CRWLockHolder.
/// Implementations of this interface should inherit from CObjectEx to allow
/// to take smart references on them.
class NCBI_XNCBI_EXPORT IRWLockHolder_Listener
{
public:
    virtual ~IRWLockHolder_Listener(void);

    /// Callback called when lock represented by CRWLockHolder is acquired
    virtual void OnLockAcquired(CRWLockHolder* holder) = 0;

    /// Callback called when lock represented by CRWLockHolder is released
    virtual void OnLockReleased(CRWLockHolder* holder) = 0;
};

/// Types of smart references to IRWLockHolder_Listener
typedef CIRef<IRWLockHolder_Listener>      TRWLockHolder_ListenerRef;
typedef CWeakIRef<IRWLockHolder_Listener>  TRWLockHolder_ListenerWeakRef;


/// Interface for factory creating CRWLockHolder objects.
/// Default interface implementation supports pooling of CRWLockHolder objects
/// to avoid extensive use of new/delete
class NCBI_XNCBI_EXPORT IRWLockHolder_Factory
{
public:
    virtual ~IRWLockHolder_Factory(void);

    /// Obtain new CRWLockHolder object for given CYieldingRWLock and
    /// necessary lock type.
    virtual CRWLockHolder* CreateHolder(CYieldingRWLock* lock,
                                        ERWLockType      typ) = 0;

    /// Free unnecessary (and unreferenced by anybody) CRWLockHolder object
    virtual void DeleteHolder(CRWLockHolder* holder) = 0;
};

/// Holder of the lock inside CYieldingRWLock.
/// This class should be used for 2 different reasons:
/// - while IsLockAcquired() is not true the requested lock is not provided
///   to you, so this object will be as a signal for you to continue operation
/// - the lock is held while this object is alive, though it's always better
///   to explicitly call ReleaseLock().
class NCBI_XNCBI_EXPORT CRWLockHolder : public CObject
{
    friend class CYieldingRWLock;

public:
    /// Create lock holder bound to given object factory
    CRWLockHolder(IRWLockHolder_Factory* factory);

    virtual ~CRWLockHolder(void);

    /// Get factory which this object was created from
    IRWLockHolder_Factory* GetFactory(void) const;

    /// Get lock object that is locked by this holder
    CYieldingRWLock* GetRWLock(void) const;

    /// Get type of lock held
    ERWLockType GetLockType(void) const;

    /// Check if lock requested is already granted
    bool IsLockAcquired(void) const;

    /// Release the lock held or cancel request for the lock
    void ReleaseLock(void);

    /// Add object keeping track of holder state changes
    void AddListener(IRWLockHolder_Listener* listener);

    /// Remove object keeping track of holder state changes
    void RemoveListener(IRWLockHolder_Listener* listener);

public:
    /// Initialize holder for given CYieldingRWLock and necessary lock type.
    /// Method is for use only inside IRWLockHolder_Factory implementation
    void Init(CYieldingRWLock* lock, ERWLockType typ);

    /// Reset holder to be able to use it later (after calling Init() )
    void Reset(void);

private:
    CRWLockHolder(const CRWLockHolder&);
    CRWLockHolder& operator= (const CRWLockHolder&);

    /// Callback called at the moment when lock is granted
    void x_OnLockAcquired(void);
    /// Callback called at the moment when lock is released. Method is not
    /// called if request for lock was canceled before it was actually
    /// granted.
    void x_OnLockReleased(void);
    /// "Delete" this holder after last reference was removed.
    /// Actually deletes using factory's DeleteHolder().
    virtual void DeleteThis(void);


    typedef list<TRWLockHolder_ListenerWeakRef> TListenersList;

    /// Factory created the holder
    IRWLockHolder_Factory* m_Factory;
    /// Lock object the holder is assigned to
    CYieldingRWLock*       m_Lock;
    /// Type of lock held
    ERWLockType            m_Type;
    /// Flag if lock was acquired
    bool                   m_LockAcquired;
    /// Mutex for operating listeners
    CSpinLock              m_ObjLock;
    /// List of holder listeners
    TListenersList         m_Listeners;
};

/// Type that should be always used to store pointers to CRWLockHolder
typedef CRef<CRWLockHolder> TRWLockHolderRef;


/// Read/write lock without blocking calls.
///
/// Neither R-lock nor W-lock is bound to thread acquired it and can be
/// released in any other thread. Any lock is bound to CRWLockHolder object
/// only.
/// Allows to exist several readers at a time or one writer. Always respects
/// the time of lock request. I.e. no new readers are granted access if there
/// is some writer waiting for access and no new writers are granted access if
/// there is some readers waiting for access (while another writer is
/// working).
/// Can be customizable by instance of IRWLockHolder_Factory to adopt custom
/// memory management for CRWLockHolder objects.
class NCBI_XNCBI_EXPORT CYieldingRWLock
{
    friend class CRWLockHolder;

public:
    /// Create read/write lock with custom holders factory.
    /// By default (if factory == NULL) pooling of CRWLockHolder will be used.
    CYieldingRWLock(IRWLockHolder_Factory* factory = NULL);

    /// It is fatal error to destroy the object while some locks are pending.
    /// Thus this object should be destroyed only after calls to ReleaseLock()
    /// for all CRWLockHolder objects.
    ~CYieldingRWLock(void);

    /// Read lock.
    /// Method returns immediately no matter if lock is granted or not. If
    /// lock is not granted then request for lock is remembered and will be
    /// granted later unless CRWLockHolder::ReleaseLock() is called (or object
    /// is deleted).
    TRWLockHolderRef AcquireReadLock(void);

    /// Write lock.
    /// Method returns immediately no matter if lock is granted or not. If
    /// lock is not granted then request for lock is remembered and will be
    /// granted later unless CRWLockHolder::ReleaseLock() is called (or object
    /// is deleted).
    TRWLockHolderRef AcquireWriteLock(void);

    /// General method to request read or write lock.
    TRWLockHolderRef AcquireLock(ERWLockType lock_type);

    /// Check if any type of lock on this object is held
    bool IsLocked(void);

private:
    typedef deque<TRWLockHolderRef>  THoldersList;

    /// Main implementation releasing lock
    void x_ReleaseLock(CRWLockHolder* holder);


    /// Factory creating CRWLockHolder objects
    IRWLockHolder_Factory* m_Factory;
    /// Main locking mutex for object operations
    CSpinLock              m_ObjLock;
    /// Number of locks granted on this object by type
    int                    m_Locks[2];
    /// Queue for waiting lock requests
    THoldersList           m_LockWaits;
};



/////////////////////////////////////////////////////////////////////////////
///
/// CSemaphore --
///
/// Implement the semantics of an application-wide semaphore.

class NCBI_XNCBI_EXPORT CSemaphore
{
public:
    /// Constructor.
    ///
    /// @param
    ///   int_count   The initial value of the semaphore.
    ///   max_count   Maximum value that semaphore value can be incremented to.
    CSemaphore(unsigned int init_count, unsigned int max_count);

    /// Destructor.
    ///
    /// Report error if the semaphore is locked.
    ~CSemaphore(void);

    /// Wait on semaphore.
    ///
    /// Decrement the counter by one. If the semaphore's count is zero then
    /// wait until it's not zero.
    void Wait(void);

    /// Timed wait.
    ///
    /// Wait up to timeout_sec + timeout_nsec/1E9 seconds for the
    /// semaphore's count to exceed zero.  If that happens, decrement
    /// the counter by one and return TRUE; otherwise, return FALSE.
    bool TryWait(unsigned int timeout_sec = 0, unsigned int timeout_nsec = 0);

    /// Timed wait. Wait time specified by CTimeout.
    bool TryWait(const CTimeout& timeout);

    /// Increment the semaphore by "count".
    ///
    /// Do nothing and throw an exception if counter would exceed "max_count".
    void Post(unsigned int count = 1);

private:
    struct SSemaphore* m_Sem;  ///< System-specific semaphore data.

    /// Private copy constructor to disallow initialization.
    CSemaphore(const CSemaphore&);

    /// Private assignment operator to disallow assignment.
    CSemaphore& operator= (const CSemaphore&);
};



/////////////////////////////////////////////////////////////////////////////
///
/// CConditionVariable --
///
///   Condition variable.

#if defined(NCBI_POSIX_THREADS) || (defined(NCBI_WIN32_THREADS) && defined(NCBI_USE_CRITICAL_SECTION))
#  define NCBI_HAVE_CONDITIONAL_VARIABLE
#endif

#if defined(NCBI_HAVE_CONDITIONAL_VARIABLE)

class NCBI_XNCBI_EXPORT CConditionVariable
{
public:
    CConditionVariable(void);
    ~CConditionVariable(void);
    
    static bool IsSupported(void);

    /// Release mutex and lock the calling thread until the condition
    /// variable is signalled.
    ///
    /// @param mutex
    ///  Mutex to release while waiting for a signal.
    ///  At the time of the call the mutex must be locked by this thread
    ///  exactly once. Otherwise, an exception will be thrown.
    /// @param deadline
    ///   The wait will time out when system time equals or exceeds
    ///   the absolute time specified in 'deadline'
    /// @return
    ///   - TRUE when condition variable is signalled
    ///   - FALSE if the wait has timed out
    /// @sa
    ///   SignalSome, SignalAll, CConditionVariableException
    bool WaitForSignal
    (CMutex&           mutex,
     const CDeadline&  deadline = CDeadline(CTimeout::eInfinite));

    /// Release mutex and lock the calling thread until the condition
    /// variable is signalled.
    ///
    /// @param mutex
    ///  Mutex to release while waiting for a signal.
    ///  At the time of the call the mutex must be locked by this thread.
    ///  Otherwise, an exception will be thrown.
    /// @param deadline
    ///   The wait will time out when system time equals or exceeds
    ///   the absolute time specified in 'deadline'
    /// @return
    ///   TRUE when condition variable is signalled;  FALSE if timed out
    /// @sa
    ///   SignalSome, SignalAll, CConditionVariableException
    bool WaitForSignal
    (CFastMutex&       mutex,
     const CDeadline&  deadline = CDeadline(CTimeout::eInfinite));

    /// Wake at least one of the threads that are currently waiting on this
    /// condition variable (if any threads are waiting on it).
    /// @note
    ///  More than one thread can be awaken.
    /// @sa WaitForSignal, SignalAll
    void SignalSome(void);

    /// Wake all threads that are currently waiting on the condition variable.
    /// @sa WaitForSignal, SignalSome
    void SignalAll(void);

private:
    bool x_WaitForSignal(SSystemFastMutex& mutex, const CDeadline& timeout);

#if defined(NCBI_OS_MSWIN)
    CONDITION_VARIABLE m_ConditionVar;
#else
    pthread_cond_t     m_ConditionVar;
#endif
    CAtomicCounter_WithAutoInit  m_WaitCounter;
    SSystemFastMutex* volatile   m_WaitMutex;
};

#endif  /* NCBI_HAVE_CONDITIONAL_VARIABLE */



/////////////////////////////////////////////////////////////////////////////
///
/// CConditionVariableException --
///
///  Exceptions generated by condition variable.
///
/// CConditionVariableException inherits its basic functionality from
/// CCoreException, and defines additional error codes.
///
/// @sa CConditionVariable, CCoreException

class NCBI_XNCBI_EXPORT CConditionVariableException : public CCoreException
{
public:
    /// Error types that a condition variable can generate.
    enum EErrCode {
        ///< Parameter of WaitForSignal function is invalid
        eInvalidValue,
         ///< Mutex passed to WaitForSignal is not locked exactly once
        eMutexLockCount,
        ///< Mutex passed to WaitForSignal is not owned by the current thread
        eMutexOwner,
        ///< Different mutexes were supplied for concurrent WaitForSignal
        ///< operations on this condition variable
        eMutexDifferent,
        ///< Condition variable is not supported on this platform
        eUnsupported
    };

    /// Translate from the error code value to its string representation.
    virtual const char* GetErrCodeString(void) const;

    // Standard exception boilerplate code.
    NCBI_EXCEPTION_DEFAULT(CConditionVariableException,CCoreException);
};

/* @} */


#include <corelib/ncbimtx.inl>

/////////////////////////////////////////////////////////////////////////////

END_NCBI_SCOPE

#endif  /* NCBIMTX__HPP */
