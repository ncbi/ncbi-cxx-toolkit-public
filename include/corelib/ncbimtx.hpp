#ifndef NCBIMTX__HPP
#define NCBIMTX__HPP

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
 *   Multi-threading -- fast mutexes;  platform-specific headers and defines
 *
 *   MUTEX:
 *      CInternalMutex   -- platform-dependent mutex functionality
 *      CFastMutex       -- simple mutex with fast lock/unlock functions
 *      CFastMutexGuard  -- acquire fast mutex, then guarantee for its release
 *
 */


#include <corelib/ncbistl.hpp>
#include <stdexcept>


#if defined(_MT)  &&  !defined(NCBI_WITHOUT_MT)
#  if defined(NCBI_OS_MSWIN)
#    define NCBI_WIN32_THREADS
#    include <windows.h>
#  elif defined(NCBI_OS_UNIX)
#    define NCBI_POSIX_THREADS
#    include <pthread.h>
#    include <sys/errno.h>
#  else
#    define NCBI_NO_THREADS
#  endif
#else
#  define NCBI_NO_THREADS
#endif


BEGIN_NCBI_SCOPE



/////////////////////////////////////////////////////////////////////////////
//
// DECLARATIONS of internal (platform-dependent) representations
//
//    TMutex         -- internal mutex type
//

#if defined(NCBI_WIN32_THREADS)
typedef HANDLE TMutex;
#elif defined(NCBI_POSIX_THREADS)
typedef pthread_mutex_t TMutex;
#else
typedef int TMutex; // fake
#endif



/////////////////////////////////////////////////////////////////////////////
//
//  FAST MUTEX
//
//    CInternalMutex::
//
//    CFastMutex::
//    CFastMutexGuard::
//


/////////////////////////////////////////////////////////////////////////////
//
//  CInternalMutex::
//
//    Internal platform-dependent mutex implementation
//    To be used by CMutex and CFastMutex only.
//

class CInternalMutex
{
public:
    // Create mutex handle
    CInternalMutex (void);
    // Close mutex handle (no checks if it's still acquired)
    ~CInternalMutex(void);

protected:
    // Acquire mutex for the current thread (no nesting checks)
    void Lock  (void);
    // Release mutex (no owner or nesting checks)
    void Unlock(void);

private:
    // Try to lock, return "true" on success
    bool TryLock(void);

    // Platform-dependent mutex handle, also used by CRWLock
    TMutex m_Handle;
    bool m_Initialized;
    // Disallow assignment and copy constructor
    CInternalMutex(const CInternalMutex&);
    CInternalMutex& operator= (const CInternalMutex&);

    friend class CMutex;
    friend class CRWLock; // Uses m_Handle member
};



/////////////////////////////////////////////////////////////////////////////
//
//  CFastMutex::
//
//    Simple mutex with fast lock/unlock functions
//
//  This mutex can be used instead of CMutex if it's guaranteed that
//  there is no nesting. This mutex does not check nesting or owner.
//  It has better performance than CMutex, but is less secure.
//

#if defined(NCBI_WIN32_THREADS)

class CFastMutex
{
public:
    // Create mutex handle
    CFastMutex (void);
    // Close mutex handle (no checks if it's still acquired)
    ~CFastMutex(void);

    // Acquire mutex for the current thread (no nesting checks)
    void Lock  (void);
    // Release mutex (no owner or nesting checks)
    void Unlock(void);

private:
    // Platform-dependent mutex handle, also used by CRWLock
    CRITICAL_SECTION m_Handle;
    bool             m_Initialized;

    // Disallow assignment and copy constructor
    CFastMutex(const CFastMutex&);
    CFastMutex& operator= (const CFastMutex&);
};

#else /* Not Win32 */

class CFastMutex : public CInternalMutex
{
public:
    CFastMutex(void)  {}
    ~CFastMutex(void) {}
    void Lock(void)   { CInternalMutex::Lock(); }
    void Unlock(void) { CInternalMutex::Unlock(); }
};

#endif  /* NCBI_WIN32_THREADS */



/////////////////////////////////////////////////////////////////////////////
//
//  CFastMutexGuard::
//
//    Acquire fast mutex, then guarantee for its release.
//

class CFastMutexGuard
{
public:
    // Register the mutex to be released by the guard destructor.
    CFastMutexGuard(CFastMutex& mtx) : m_Mutex(&mtx) { m_Mutex->Lock(); }

    // Release the mutex, if it was (and still is) successfully acquired.
    ~CFastMutexGuard(void)  { if (m_Mutex)  m_Mutex->Unlock(); }

    // Release the mutex right now (do not release it in the guard destructor).
    void Release(void) { m_Mutex->Unlock();  m_Mutex = 0; }

    // Lock on mutex "mtx" (if it's not guarded yet) and start guarding it.
    // NOTE: it never holds more than one lock on the guarded mutex!
    void Guard(CFastMutex& mtx) {
        if (&mtx == m_Mutex)
            return;
        if ( m_Mutex )
            m_Mutex->Unlock();
        m_Mutex = &mtx;
        m_Mutex->Lock();
    }

    // Get the mutex being guarded
    CFastMutex* GetMutex(void) const { return m_Mutex; }

private:
    CFastMutex* m_Mutex;  // the mutex (NULL if released)

    // Disallow assignment and copy constructor
    CFastMutexGuard(const CFastMutexGuard&);
    CFastMutexGuard& operator= (const CFastMutexGuard&);
};



/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
//  IMPLEMENTATION of INLINE functions
/////////////////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////////////////
//  CInternalMutex::
//

inline
void CInternalMutex::Lock(void)
{
    if ( !m_Initialized ) {
        return;
    }

    // Acquire system mutex
#if defined(NCBI_NO_THREADS)
    return;
#else
#  if defined(NCBI_WIN32_THREADS)
    if (WaitForSingleObject(m_Handle, INFINITE) == WAIT_OBJECT_0) {
        return;
    }
#  elif defined(NCBI_POSIX_THREADS)
    if (pthread_mutex_lock(&m_Handle) == 0) {
        return;
    }
#  endif
    throw runtime_error("CInternalMutex::Lock() -- sys.error locking mutex");
#endif
}


inline
void CInternalMutex::Unlock(void)
{
    if ( !m_Initialized ) {
        return;
    }

    // Release system mutex
#if defined(NCBI_NO_THREADS)
    return;
#else
#  if defined(NCBI_WIN32_THREADS)
    if ( ReleaseMutex(m_Handle) ) {
        return;
    }
#  elif defined(NCBI_POSIX_THREADS)
    if (pthread_mutex_unlock(&m_Handle) == 0) {
        return;
    }
#  endif
    throw runtime_error("CInternalMutex::Unlock() -- sys.err unlocking mutex");
#endif
}


inline
bool CInternalMutex::TryLock(void)
{
    if ( !m_Initialized ) {
        return true;
    }

    // Check if the system mutex is acquired.
    // If not, acquire for the current thread.
#if defined(NCBI_WIN32_THREADS)
    DWORD status = WaitForSingleObject(m_Handle, 0);
    if (status == WAIT_OBJECT_0) {
        return true;
    }
    else if (status == WAIT_TIMEOUT) {
        return false;
    }
    throw runtime_error("CInternalMutex::TryLock() -- sys.err checking mutex");
#elif defined(NCBI_POSIX_THREADS)
    int status = pthread_mutex_trylock(&m_Handle);
    if (status == 0) {
        return true;
    }
    else if (status == EBUSY) {
        return false;
    }
    throw runtime_error("CInternalMutex::TryLock() -- sys.err checking mutex");
#else
    return true;
#endif
}



#if defined(NCBI_WIN32_THREADS) /* Begin Win32 CFastMutex */

/////////////////////////////////////////////////////////////////////////////
//  CFastMutex::
//

inline
CFastMutex::CFastMutex(void)
{
    // Create platform-dependent mutex handle
    InitializeCriticalSection(&m_Handle);
    m_Initialized = true;
}


inline
CFastMutex::~CFastMutex(void)
{
    // Delete platform-dependent mutex handle
    DeleteCriticalSection(&m_Handle);
    m_Initialized = false;
}


inline
void CFastMutex::Lock(void)
{
    if ( m_Initialized ) {
        EnterCriticalSection(&m_Handle);
    }
}


inline
void CFastMutex::Unlock(void)
{
    if ( m_Initialized ) {
        LeaveCriticalSection(&m_Handle);
    }
}

#endif /* End of Win32 CFastMutex */


END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
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
