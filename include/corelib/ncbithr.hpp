#ifndef NCBITHR__HPP
#define NCBITHR__HPP

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
 *   TLS:
 *      CTlsBase         -- TLS implementation (base class for CTls<>)
 *      CTls<>           -- thread local storage template
 *
 *   THREAD:
 *      CThread          -- thread wrapper class
 *
 *   MUTEX:
 *      CMutex           -- mutex that allows nesting (with runtime checks)
 *      CAutoMutex       -- guarantee mutex release
 *      CMutexGuard      -- acquire mutex, then guarantee for its release
 *
 *   RW-LOCK:
 *      CInternalRWLock  -- platform-dependent RW-lock structure (fwd-decl)
 *      CRWLock          -- Read/Write lock related  data and methods
 *      CAutoRW          -- guarantee RW-lock release
 *      CReadLockGuard   -- acquire R-lock, then guarantee for its release
 *      CWriteLockGuard  -- acquire W-lock, then guarantee for its release
 *
 * ---------------------------------------------------------------------------
 * $Log$
 * Revision 1.6  2001/05/17 14:54:33  lavr
 * Typos corrected
 *
 * Revision 1.5  2001/03/30 22:57:32  grichenk
 * + CThread::GetSystemID()
 *
 * Revision 1.4  2001/03/26 21:45:28  vakatov
 * Workaround static initialization/destruction traps:  provide better
 * timing control, and allow safe use of the objects which are
 * either not yet initialized or already destructed. (with A.Grichenko)
 *
 * Revision 1.3  2001/03/13 22:43:19  vakatov
 * Full redesign.
 * Implemented all core functionality.
 * Thoroughly tested on different platforms.
 *
 * Revision 1.2  2000/12/11 06:48:51  vakatov
 * Revamped Mutex and RW-lock APIs
 *
 * Revision 1.1  2000/12/09 00:03:26  vakatov
 * First draft:  Mutex and RW-lock API
 *
 * ===========================================================================
 */

#include <corelib/ncbiobj.hpp>
#include <memory>
#include <set>
#include <list>


BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
//
// DECLARATIONS of internal (platform-dependent) representations
//
//    TTlsKey          -- internal TLS key type
//    TThreadHandle    -- platform-dependent thread handle type
//    TThreadSystemID  -- platform-dependent thread ID type
//
//  NOTE:  all these types are intended for internal use only!
//

#if defined(NCBI_WIN32_THREADS)

typedef DWORD  TTlsKey;
typedef HANDLE TThreadHandle;
typedef DWORD  TThreadSystemID;

typedef DWORD  TWrapperRes;
typedef LPVOID TWrapperArg;

#elif defined(NCBI_POSIX_THREADS)

typedef pthread_key_t TTlsKey;
typedef pthread_t     TThreadHandle;
typedef pthread_t     TThreadSystemID;

typedef void* TWrapperRes;
typedef void* TWrapperArg;

#else

// fake
typedef void* TTlsKey;
typedef int   TThreadHandle;
typedef int   TThreadSystemID;

typedef void* TWrapperRes;
typedef void* TWrapperArg;

#endif




/////////////////////////////////////////////////////////////////////////////
//
//  CTlsBase::
//
//  CTls<>::
//
//    Thread local storage
//
//  Store thread-specific data.
//

class CTlsBase : public CObject  // only to serve as a base class for CTls<>
{
    friend class CRef<CTlsBase>;
    friend class CThread;

public:
    typedef void (*FCleanupBase)(void* value, void* cleanup_data);

protected:
    // All other methods are described just below, in CTls<> class declaration
    CTlsBase(void);

    // Cleanup data, delete TLS key
    ~CTlsBase(void);

    void* x_GetValue(void) const;
    void x_SetValue(void* value, FCleanupBase cleanup=0, void* cleanup_data=0);
    void x_Reset(void);
    void x_Discard(void);

private:
    TTlsKey m_Key;
    bool    m_Initialized;

    // Internal structure to store all three pointers in the same TLS
    struct STlsData {
        void*        m_Value;
        FCleanupBase m_CleanupFunc;
        void*        m_CleanupData;
    };
    STlsData* x_GetTlsData(void) const;
};


template <class TValue>
class CTls : public CTlsBase
{
public:
    // Get the pointer previously stored by SetValue().
    // Return 0 if no value has been stored, or if Reset() was last called.
    TValue* GetValue(void) const
    {
        return reinterpret_cast<TValue*> (x_GetValue());
    }

    // Cleanup previously stored value, store the new value.
    // The "cleanup" function and "cleanup_data" will be used to
    // destroy the new "value" in the next call to SetValue() or Reset().
    // Do not cleanup if the new value is equal to the old one.
    typedef void (*FCleanup)(TValue* value, void* cleanup_data);
    void SetValue(TValue* value, FCleanup cleanup = 0, void* cleanup_data = 0)
    {
        x_SetValue(value,
                   reinterpret_cast<FCleanupBase> (cleanup), cleanup_data);
    }

    // Reset TLS to its initial value (as it was before the first call
    // to SetValue()). Do cleanup if the cleanup function was specified
    // in the previous call to SetValue().
    // NOTE:  Reset() will always be called automatically on the thread
    //        termination, or when the TLS is destroyed.
    void Reset(void) { x_Reset(); }

    // Schedule the TLS to be destroyed
    // (as soon as there are no CRef to it left).
    void Discard(void) { x_Discard(); }
};



/////////////////////////////////////////////////////////////////////////////
//
//  CThread::
//
//    Thread wrapper class
//
//  Base class for user-defined threads. Creates the new thread, then
//  calls user-provided Main() function. The thread then can be detached
//  or joined. In any case, explicit destruction of the thread is prohibited.
//


class CThread : public CObject
{
    friend class CRef<CThread>;
    friend class CTlsBase;

public:
    // Must be allocated in the heap (only!).
    CThread(void);

    // Run the thread:
    // create new thread, initialize it, and call user-provided Main() method.
    bool Run(void);

    // Inform the thread that user does not need to wait for its termination.
    // The thread object will be destroyed by Exit().
    // If the thread has already been terminated by Exit, Detach() will
    // also schedule the thread object for destruction.
    // NOTE:  it is no more safe to use this thread object after Detach(),
    //        unless there are still CRef<> based references to it!
    void Detach(void);

    // Wait for the thread termination.
    // The thread object will be scheduled for destruction right here,
    // inside Join(). Only one call to Join() is allowed.
    void Join(void** exit_data = 0);

    // Cancel current thread. If the thread is detached, then schedule
    // the thread object for destruction.
    static void Exit(void* exit_data);

    // If the thread has not been Run() yet, then schedule the thread object
    // for destruction, and return TRUE.
    // Otherwise, do nothing, and return FALSE.
    bool Discard(void);

    // Get ID of current thread (for main thread it is always zero).
    typedef unsigned int TID;
    static TID GetSelf(void);

    // Get system ID of the current thread - for internal use only.
    // The ID is unique only while the thread is running and may be
    // re-used by another thread later.
    static void GetSystemID(TThreadSystemID* id);

protected:
    // Derived (user-created) class must provide a real thread function.
    virtual void* Main(void) = 0;

    // Override this to execute finalization code.
    // Unlike destructor, this code will be executed before
    // thread termination and as a part of the thread.
    virtual void OnExit(void);

    // To be called only internally!
    // NOTE:  destructor of the derived (user-provided) class should be
    //        declared "protected", too!
    virtual ~CThread(void);

private:
    TID           m_ID;            // thread ID
    TThreadHandle m_Handle;        // platform-dependent thread handle
    bool          m_IsRun;         // if Run() was called for the thread
    bool          m_IsDetached;    // if the thread is detached
    bool          m_IsJoined;      // if Join() was called for the thread
    bool          m_IsTerminated;  // if Exit() was called for the thread
    CRef<CThread> m_SelfRef;       // "this" -- to avoid premature destruction
    void*         m_ExitData;      // as returned by Main() or passed to Exit()

    // Function to use (internally) as the thread's startup function
    static TWrapperRes Wrapper(TWrapperArg arg);

    // To store "CThread" object related to the current (running) thread
    static CTls<CThread>* sm_ThreadsTls;
    // Safe access to "sm_ThreadsTls"
    static CTls<CThread>& GetThreadsTls(void)
    {
        if ( !sm_ThreadsTls ) {
            CreateThreadsTls();
        }
        return *sm_ThreadsTls;
    }

    // sm_ThreadsTls initialization and cleanup functions
    static void CreateThreadsTls(void);
    friend void s_CleanupThreadsTls(void* /* ptr */);

    // Keep all TLS references to clean them up in Exit()
    typedef set< CRef<CTlsBase> > TTlsSet;
    TTlsSet m_UsedTls;
    static void AddUsedTls(CTlsBase* tls);
};



/////////////////////////////////////////////////////////////////////////////
//
//  MUTEX
//
//    CMutex::
//    CAutoMutex::
//    CMutexGuard::
//



/////////////////////////////////////////////////////////////////////////////
//
//  CMutex::
//
//    Mutex that allows nesting (with runtime checks)
//
//  Allows for nested locks by the same thread. Checks the mutex
//  owner before unlocking. This mutex should be used when performance
//  is less important than data protection.
//

class CMutex
{
public:
    // 'ctors
    CMutex (void);
    // Report error if the mutex is locked
    ~CMutex(void);

    // If the mutex is unlocked, then acquire it for the calling thread.
    // If the mutex is acquired by this thread, then increase the
    // lock counter (each call to Lock() must have corresponding
    // call to Unlock() in the same thread).
    // If the mutex is acquired by another thread, then wait until it's
    // unlocked, then act like a Lock() on an unlocked mutex.
    void Lock(void);

    // Try to acquire the mutex. On success, return "true", and acquire
    // the mutex (just as the Lock() above does).
    // If the mutex is already acquired by another thread, then return "false".
    bool TryLock(void);

    // If the mutex is acquired by this thread, then decrease the lock counter.
    // If the lock counter becomes zero, then release the mutex completely.
    // Report error if the mutex is not locked or locked by another
    // thread.
    void Unlock(void);

private:
    CInternalMutex m_Mtx;    // (low-level functionality is in CInternalMutex)
    CThread::TID   m_Owner;  // platform-dependent thread data
    int            m_Count;  // # of nested (in the same thread) locks

    // Disallow assignment and copy constructor
    CMutex(const CMutex&);
    CMutex& operator= (const CMutex&);

    friend class CRWLock; // uses m_Mtx and m_Owner members directly
};



/////////////////////////////////////////////////////////////////////////////
//
//  CAutoMutex::
//
//    Guarantee mutex release
//
//  Acts in a way like "auto_ptr<>": guarantees mutex release.
//

class CAutoMutex
{
public:
    // Register the mutex to be released by the guard destructor.
    CAutoMutex(CMutex& mtx) : m_Mutex(&mtx)  { }

    // Release the mutex, if it was (and still is) successfully acquired.
    ~CAutoMutex(void)  { if (m_Mutex)  m_Mutex->Unlock(); }

    // Release the mutex right now (do not release it in the guard destructor).
    void Release(void)  { m_Mutex->Unlock(); m_Mutex = 0; }

    // Get the mutex being guarded
    CMutex* GetMutex(void) const  { return m_Mutex; }

private:
    CMutex* m_Mutex;  // the mutex (NULL if released)

    // Disallow assignment and copy constructor
    CAutoMutex(const CAutoMutex&);
    CAutoMutex& operator= (const CAutoMutex&);
};



/////////////////////////////////////////////////////////////////////////////
//
//  CMutexGuard::
//
//    Acquire the mutex, then guarantee for its release
//

class CMutexGuard : public CAutoMutex
{
public:
    // Acquire the mutex;  register it to be released by the guard destructor.
    CMutexGuard(CMutex& mtx) : CAutoMutex(mtx) { GetMutex()->Lock(); }

private:
    // Disallow assignment and copy constructor
    CMutexGuard(const CMutexGuard&);
    CMutexGuard& operator= (const CMutexGuard&);
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


/////////////////////////////////////////////////////////////////////////////
//
//  CRWLock::
//
//    Read/Write lock related  data and methods
//
//  Allows multiple readers or single writer with nested locks.
//  R-after-W is considered to be a nested Write-lock.
//  W-after-R is not allowed.
//
//  NOTE: When _DEBUG is not defined, does not always detect W-after-R
//  correctly, so that deadlock may happen. Test your application
//  in _DEBUG mode firts!
//

class CRWLock
{
public:
    // 'ctors
    CRWLock(void);
    ~CRWLock(void);

    // Acquire the R-lock. If W-lock is already acquired by
    // another thread, then wait until it is released.
    void ReadLock(void);

    // Acquire the W-lock. If R-lock or W-lock is already acquired by
    // another thread, then wait until it is released.
    void WriteLock(void);

    // Try to acquire R-lock or W-lock, respectively. Return immediately.
    // Return TRUE if the RW-lock has been successfully acquired.
    bool TryReadLock(void);
    bool TryWriteLock(void);

    // Release the RW-lock.
    void Unlock(void);

private:
    // platform-dependent RW-lock data
    auto_ptr<CInternalRWLock>  m_RW;
    CMutex                     m_Mutex;
    // Writer ID, one of the readers ID or kThreadID_None if unlocked
    CThread::TID               m_Owner;
    // Number of readers (if >0) or writers (if <0)
    int                        m_Count;

    // List of all readers or writers (for debugging)
    list<CThread::TID>         m_Readers;

    // Disallow assignment and copy constructor
    CRWLock(const CRWLock&);
    CRWLock& operator= (const CRWLock&);
};



/////////////////////////////////////////////////////////////////////////////
//
//  CAutoRW::
//
//    Guarantee RW-lock release
//
//  Acts in a way like "auto_ptr<>": guarantees RW-Lock release.
//

class CAutoRW
{
public:
    // Register the RW-lock to be released by the guard destructor.
    // Do NOT acquire the RW-lock though!
    CAutoRW(CRWLock& rw) : m_RW(&rw) {}

    // Release the RW-lock right now (don't release it in the guard destructor)
    void Release(void) { m_RW->Unlock();  m_RW = 0; }

    // Release the R-lock, if it was successfully acquired and
    // not released already by Release().
    ~CAutoRW(void)  { if (m_RW) Release(); }

    // Get the RW-lock being guarded
    CRWLock* GetRW(void) const { return m_RW; }

private:
    CRWLock* m_RW;  // the RW-lock (NULL if not acquired)

    // Disallow assignment and copy constructor
    CAutoRW(const CAutoRW&);
    CAutoRW& operator= (const CAutoRW&);
};



/////////////////////////////////////////////////////////////////////////////
//
//  CReadLockGuard::
//
//    Acquire the R-lock, then guarantee for its release
//

class CReadLockGuard : public CAutoRW
{
public:
    // Acquire the R-lock;  register it to be released by the guard destructor.
    CReadLockGuard(CRWLock& rw) : CAutoRW(rw) { GetRW()->ReadLock(); }

private:
    // Disallow assignment and copy constructor
    CReadLockGuard(const CReadLockGuard&);
    CReadLockGuard& operator= (const CReadLockGuard&);
};



/////////////////////////////////////////////////////////////////////////////
//
//  CWriteLockGuard::
//
//    Acquire the W-lock, then guarantee for its release
//

class CWriteLockGuard : public CAutoRW
{
public:
    // Acquire the W-lock;  register it to be released by the guard destructor.
    CWriteLockGuard(CRWLock& rw) : CAutoRW(rw) { GetRW()->WriteLock(); }

private:
    // Disallow assignment and copy constructor
    CWriteLockGuard(const CWriteLockGuard&);
    CWriteLockGuard& operator= (const CWriteLockGuard&);
};





/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
//  IMPLEMENTATION of INLINE functions
/////////////////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////////////////
//  CTlsBase::
//

inline
CTlsBase::STlsData* CTlsBase::x_GetTlsData(void)
const
{
    if ( !m_Initialized ) {
        return 0;
    }

    void* tls_data;

#if defined(NCBI_WIN32_THREADS)
    tls_data = TlsGetValue(m_Key);
#elif defined(NCBI_POSIX_THREADS)
    tls_data = pthread_getspecific(m_Key);
#else
    tls_data = m_Key;
#endif

    return static_cast<STlsData*> (tls_data);
}


inline
void* CTlsBase::x_GetValue(void)
const
{
    // Get TLS-stored structure
    STlsData* tls_data = x_GetTlsData();

    // If assigned, extract and return user data
    return tls_data ? tls_data->m_Value : 0;
}



/////////////////////////////////////////////////////////////////////////////
//  CThread::
//

inline
CThread::TID CThread::GetSelf(void)
{
    // Get pointer to the current thread object
    CThread* thread_ptr = GetThreadsTls().GetValue();

    // If zero, it is main thread which has no CThread object
    return thread_ptr ? thread_ptr->m_ID : 0/*main thread*/;
}


// Special value, stands for "no thread" thread ID
const CThread::TID kThreadID_None = 0xFFFFFFFF;



/////////////////////////////////////////////////////////////////////////////
//  CMutex::
//

inline
void CMutex::Lock(void)
{
    if ( !m_Mtx.m_Initialized ) {
        return;
    }

    CThread::TID owner = CThread::GetSelf();
    if (owner == m_Owner) {
        // Don't lock twice, just increase the counter
        m_Count++;
        return;
    }

    // Lock the mutex and remember the owner
    m_Mtx.Lock();
    m_Owner = owner;
    m_Count = 1;
}


inline
bool CMutex::TryLock(void)
{
    if ( !m_Mtx.m_Initialized ) {
        return true;
    }

    CThread::TID owner = CThread::GetSelf();
    if (owner == m_Owner) {
        // Don't lock twice, just increase the counter
        m_Count++;
        return true;
    }

    // If TryLock is successful, remember the owner
    if ( m_Mtx.TryLock() ) {
        m_Owner = owner;
        m_Count = 1;
        return true;
    }

    // Cannot lock right now
    return false;
}


inline
void CMutex::Unlock(void)
{
    if ( !m_Mtx.m_Initialized ) {
        return;
    }

    // No unlocks by threads other than owner.
    // This includes no unlocks of unlocked mutex.
    if (m_Owner != CThread::GetSelf()) {
        throw runtime_error
            ("CMutex::Unlock() -- mutex is locked by another thread");
    }

    // No real unlocks if counter > 1, just decrease it
    if (--m_Count > 0) {
        return;
    }

    // This was the last lock - clear the owner and unlock the mutex
    m_Owner = kThreadID_None;
    m_Mtx.Unlock();
}


END_NCBI_SCOPE

#endif  /* NCBITHR__HPP */
