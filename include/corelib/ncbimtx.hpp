#ifndef CORELIB___NCBIMTX__HPP
#define CORELIB___NCBIMTX__HPP

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
/// SEMAPHORE:
/// - CSemaphore       -- application-wide semaphore
///


#include <corelib/ncbithr_conf.hpp>
#include <corelib/ncbicntr.hpp>
#include <corelib/guard.hpp>
#include <vector>
#include <memory>


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
// DECLARATIONS of internal (platform-dependent) representations
//
//    TMutex         -- internal mutex type
//

#if defined(NCBI_NO_THREADS)

/// Define a platform independent system mutex.
typedef int TSystemMutex; // fake
# define SYSTEM_MUTEX_INITIALIZER 0

#elif defined(NCBI_POSIX_THREADS)

/// Define a platform independent system mutex.
typedef pthread_mutex_t TSystemMutex;
# define SYSTEM_MUTEX_INITIALIZER PTHREAD_MUTEX_INITIALIZER

#elif defined(NCBI_WIN32_THREADS)

/// Define a platform independent system mutex.
typedef HANDLE TSystemMutex;
# undef SYSTEM_MUTEX_INITIALIZER

#else
#  error Unknown threads type
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
            CThreadSystemID id;
#if defined(NCBI_NO_THREADS)
            id.m_ID = TID(0);
#elif defined(NCBI_POSIX_THREADS)
            id.m_ID = pthread_self();
#elif defined(NCBI_WIN32_THREADS)
            id.m_ID = GetCurrentThreadId();
#endif
            return id;
        }

    /// Equality operator for thread ID.
    bool operator==(const CThreadSystemID& id) const
        {
            return m_ID == id.m_ID;
        }

    /// Non-equality operator for thread ID.
    bool operator!=(const CThreadSystemID& id) const
        {
            return m_ID != id.m_ID;
        }

    /// volatile versions of above methods
    void Set(const CThreadSystemID& id) volatile
        {
            m_ID = id.m_ID;
        }
    bool Is(const CThreadSystemID& id) const volatile
        {
            return m_ID == id.m_ID;
        }
    bool IsNot(const CThreadSystemID& id) const volatile
        {
            return m_ID != id.m_ID;
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

    /// Acquire mutex for the current thread with no nesting checks.
    void Lock(void);

    /// Release mutex with no owner or nesting checks.
    void Unlock(void);

    /// Try to lock.
    /// 
    /// @return
    ///   TRUE on success; FALSE, otherwise.
    bool TryLock(void);

    /// Check initialized value of mutex.
    void CheckInitialized(void) const;

    // Methods for throwing exceptions, to make inlined methods lighter

    /// Throw uninitialized ("eUninitialized") exception.
    NCBI_XNCBI_EXPORT
    static void ThrowUninitialized(void);

    /// Throw lock failed("eLocked") exception.
    NCBI_XNCBI_EXPORT
    static void ThrowLockFailed(void);

    /// Throw unlock failed("eUnlocked") exception.
    NCBI_XNCBI_EXPORT
    static void ThrowUnlockFailed(void);

    /// Throw try lock failed("eTryLock") exception.
    NCBI_XNCBI_EXPORT
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
    void Lock(void);

    /// Release mutex.
    NCBI_XNCBI_EXPORT
    void Unlock(void);

    /// Try to lock.
    /// 
    /// @return
    ///   TRUE on success; FALSE, otherwise.
    NCBI_XNCBI_EXPORT
    bool TryLock(void);

    // Methods for throwing exceptions, to make inlined methods lighter
    // throw exception eOwner

    /// Throw not owned("eOwner") exception.
    NCBI_XNCBI_EXPORT
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
#if defined(NCBI_WIN32_THREADS)
    /// Get handle - Windows version.
    /// 
    /// Also used by CRWLock.
    HANDLE GetHandle(void) { return m_Mutex.m_Handle; }
#else
    /// Get handle - Unix version.
    /// 
    /// Also used by CRWLock.
    TSystemMutex* GetHandle(void) { return &m_Mutex.m_Handle; }
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
///   http://www.ncbi.nlm.nih.gov/books/bv.fcgi?call=bv.View..ShowSection&rid=toolkit.section.threads#CMutex

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
    ///   - On success, return TRUE, and acquire the mutex just as the Lock() does.
    ///   - If the mutex is already acquired by another thread, then return FALSE.
    /// @sa
    ///   Lock()
    bool TryLock(void);

    /// Unlock mutex.
    ///
    /// Operation:
    /// - If the mutex is acquired by this thread, then decrease the lock counter.
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
    bool operator()(Class& inst)
    {
        inst.ReadLock();
        return true;
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
    bool operator()(Class& inst)
    {
        inst.WriteLock();
        return true;
    }
};

typedef CGuard<CRWLock, SSimpleWriteLock<CRWLock> > TWriteLockGuard;
typedef TWriteLockGuard                              CWriteLockGuard;


/////////////////////////////////////////////////////////////////////////////
///
/// CRWLock --
///
/// Read/Write lock related data and methods.
///
/// Allows multiple readers or single writer with recursive locks.
/// R-after-W is considered to be a recursive Write-lock. W-after-R is not
/// allowed.
///
/// NOTE: When _DEBUG is not defined, does not always detect W-after-R
/// correctly, so that deadlock may happen. Test your application
/// in _DEBUG mode first!

class NCBI_XNCBI_EXPORT CRWLock
{
public:
    /// Constructor.
    CRWLock(void);

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

    /// Try write lock.
    ///
    /// Try to acquire W-lock and return immediately.
    /// @return
    ///   TRUE if the W-lock has been successfully acquired;
    ///   FALSE, otherwise.
    bool TryWriteLock(void);

    /// Release the RW-lock.
    void Unlock(void);

private:
    
    auto_ptr<CInternalRWLock>  m_RW;    ///< Platform-dependent RW-lock data
    
    volatile CThreadSystemID   m_Owner; ///< Writer ID, one of the readers ID
    
    volatile int               m_Count; ///< Number of readers (if >0) or
                                        ///< writers (if <0)

    
    vector<CThreadSystemID>    m_Readers;   ///< List of all readers or writers
                                            ///< for debugging
    
    /// Private copy constructor to disallow initialization.
    CRWLock(const CRWLock&);
                           
    /// Private assignment operator to disallow assignment.
    CRWLock& operator= (const CRWLock&);
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


/* @} */


#include <corelib/ncbimtx.inl>

/////////////////////////////////////////////////////////////////////////////

END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.37  2004/06/16 21:22:20  vasilche
 * Fixed Read/Write typo.
 *
 * Revision 1.36  2004/06/16 11:37:33  dicuccio
 * Refactored CMutexGuard, CFastMutexGuard, CReadLockGuard, and CWriteLockGuard to
 * use a templated implementation (CGuard<>).  Provided typedefs for forward and
 * backward compatibility.
 *
 * Revision 1.35  2004/03/10 20:00:24  gorelenk
 * Added NCBI_XNCBI_EXPORT prefix to SSystemMutex struct members Lock, Unlock,
 * Trylock.
 *
 * Revision 1.34  2004/03/10 18:40:21  gorelenk
 * Added/Removed NCBI_XNCBI_EXPORT prefixes.
 *
 * Revision 1.33  2004/03/10 17:34:05  gorelenk
 * Removed NCBI_XNCBI_EXPORT prefix for classes members-functions
 * that are implemented as a inline functions.
 *
 * Revision 1.32  2003/10/30 14:56:27  siyan
 * Added a test for @sa hyperlink.
 *
 * Revision 1.31  2003/09/30 16:07:39  vasilche
 * Allow release and reacqure rw lock in guard.
 *
 * Revision 1.30  2003/09/29 20:50:30  ivanov
 * Removed CAutoInitializeStaticBase
 *
 * Revision 1.29  2003/09/17 18:09:05  vasilche
 * Removed unnecessary assignment operator - it causes problems on MSVC.
 *
 * Revision 1.28  2003/09/17 17:56:46  vasilche
 * Fixed volatile methods of CThreadSystemID.
 *
 * Revision 1.27  2003/09/17 15:20:45  vasilche
 * Moved atomic counter swap functions to separate file.
 * Added CRef<>::AtomicResetFrom(), CRef<>::AtomicReleaseTo() methods.
 *
 * Revision 1.26  2003/09/02 16:36:50  vasilche
 * Fixed warning on MSVC.
 *
 * Revision 1.25  2003/09/02 16:08:48  vasilche
 * Fixed race condition with optimization on some compilers - added 'volatile'.
 * Moved mutex Lock/Unlock methods out of inline section - they are quite complex.
 *
 * Revision 1.24  2003/09/02 15:06:14  siyan
 * Minor comment changes.
 *
 * Revision 1.23  2003/08/27 12:35:15  siyan
 * Added documentation.
 *
 * Revision 1.22  2003/06/19 18:21:15  vasilche
 * Added default constructors to CFastMutexGuard and CMutexGuard.
 * Added typedefs TReadLockGuard and TWriteLockGuard to mutexes and rwlock.
 *
 * Revision 1.21  2003/03/31 13:30:31  siyan
 * Minor changes to doxygen support
 *
 * Revision 1.20  2003/03/31 13:09:19  siyan
 * Added doxygen support
 *
 * Revision 1.19  2002/12/18 22:53:21  dicuccio
 * Added export specifier for building DLLs in windows.  Added global list of
 * all such specifiers in mswin_exports.hpp, included through ncbistl.hpp
 *
 * Revision 1.18  2002/09/23 13:58:29  vasilche
 * MS VC 6 doesn't accept empty initialization block
 *
 * Revision 1.17  2002/09/23 13:47:23  vasilche
 * Made static mutex structures POD-types on Win32
 *
 * Revision 1.16  2002/09/20 20:02:07  vasilche
 * Added public Lock/Unlock/TryLock
 *
 * Revision 1.15  2002/09/20 19:28:21  vasilche
 * Fixed missing initialization with NCBI_NO_THREADS
 *
 * Revision 1.14  2002/09/20 19:13:58  vasilche
 * Fixed single-threaded mode on Win32
 *
 * Revision 1.13  2002/09/20 13:51:56  vasilche
 * Added #include <memory> for auto_ptr<>
 *
 * Revision 1.12  2002/09/19 20:05:41  vasilche
 * Safe initialization of static mutexes
 *
 * Revision 1.11  2002/08/19 19:37:17  vakatov
 * Use <corelib/ncbi_os_mswin.hpp> in the place of <windows.h>
 *
 * Revision 1.10  2002/08/19 18:15:58  ivanov
 * +include <corelib/winundef.hpp>
 *
 * Revision 1.9  2002/07/15 18:17:51  gouriano
 * renamed CNcbiException and its descendents
 *
 * Revision 1.8  2002/07/11 14:17:54  gouriano
 * exceptions replaced by CNcbiException-type ones
 *
 * Revision 1.7  2002/04/11 20:39:18  ivanov
 * CVS log moved to end of the file
 *
 * Revision 1.6  2002/01/23 23:31:17  vakatov
 * Added CFastMutexGuard::Guard()
 *
 * Revision 1.5  2001/05/17 14:53:50  lavr
 * Typos corrected
 *
 * Revision 1.4  2001/04/16 18:45:28  vakatov
 * Do not include system MT-related headers if in single-thread mode
 *
 * Revision 1.3  2001/03/26 22:50:24  grichenk
 * Fixed CFastMutex::Unlock() bug
 *
 * Revision 1.2  2001/03/26 21:11:37  vakatov
 * Allow use of not yet initialized mutexes (with A.Grichenko)
 *
 * Revision 1.1  2001/03/13 22:34:24  vakatov
 * Initial revision
 *
 * ===========================================================================
 */

#endif  /* NCBIMTX__HPP */
