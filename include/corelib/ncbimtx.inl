#if defined(CORELIB___NCBIMTX__HPP)  &&  !defined(CORELIB___NCBIMTX__INL)
#define CORELIB___NCBIMTX__INL

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
 * Author: Eugene Vasilchenko
 *
 * File Description:
 *   Mutex classes' inline functions
 *
 */

/////////////////////////////////////////////////////////////////////////////
//  SSystemFastMutexStruct
//

inline
bool SSystemFastMutex::IsInitialized(void) const
{
    return m_Magic == eMutexInitialized;
}

inline
bool SSystemFastMutex::IsUninitialized(void) const
{
    return m_Magic == eMutexUninitialized;
}

inline
void SSystemFastMutex::CheckInitialized(void) const
{
#if defined(INTERNAL_MUTEX_DEBUG)
    if ( !IsInitialized() ) {
        ThrowUninitialized();
    }
#endif
}

inline
void SSystemFastMutex::Lock(void)
{
#if defined(NCBI_NO_THREADS)
    return;
#else
    // check
    CheckInitialized();

    // Acquire system mutex
#  if defined(NCBI_WIN32_THREADS)
    if (WaitForSingleObject(m_Handle, INFINITE) != WAIT_OBJECT_0) {
        ThrowLockFailed();
    }
#  elif defined(NCBI_POSIX_THREADS)
    if ( pthread_mutex_lock(&m_Handle) != 0 ) { // error
        ThrowLockFailed();
    }
#  endif
#endif
}

inline
bool SSystemFastMutex::TryLock(void)
{
#if defined(NCBI_NO_THREADS)
    return true;
#else
    // check
    CheckInitialized();

    // Check if the system mutex is acquired.
    // If not, acquire for the current thread.
#  if defined(NCBI_WIN32_THREADS)
    DWORD status = WaitForSingleObject(m_Handle, 0);
    if (status == WAIT_OBJECT_0) { // ok
        return true;
    }
    else {
        if (status != WAIT_TIMEOUT) { // error
            ThrowTryLockFailed();
        }
        return false;
    }
#  elif defined(NCBI_POSIX_THREADS)
    int status = pthread_mutex_trylock(&m_Handle);
    if (status == 0) { // ok
        return true;
    }
    else {
        if (status != EBUSY) { // error
            ThrowTryLockFailed();
        }
        return false;
    }
#  endif
#endif
}

inline
void SSystemFastMutex::Unlock(void)
{
#if defined(NCBI_NO_THREADS)
    return;
#else
    // check
    CheckInitialized();
        
    // Release system mutex
# if defined(NCBI_WIN32_THREADS)
    if ( !ReleaseMutex(m_Handle) ) { // error
        ThrowUnlockFailed();
    }
# elif defined(NCBI_POSIX_THREADS)
    if ( pthread_mutex_unlock(&m_Handle) != 0 ) { // error
        ThrowUnlockFailed();
    }
# endif
#endif
}

/////////////////////////////////////////////////////////////////////////////
//  SSystemMutex
//

inline
bool SSystemMutex::IsInitialized(void) const
{
    return m_Mutex.IsInitialized();
}

inline
bool SSystemMutex::IsUninitialized(void) const
{
    return m_Mutex.IsUninitialized();
}

inline
void SSystemMutex::InitializeStatic(void)
{
    m_Mutex.InitializeStatic();
}

inline
void SSystemMutex::InitializeDynamic(void)
{
    m_Mutex.InitializeDynamic();
    m_Count = 0;
}

#if defined(NCBI_NO_THREADS)
// empty version of Lock/Unlock methods for inlining
inline
void SSystemMutex::Lock(void)
{
}


inline
bool SSystemMutex::TryLock(void)
{
    return true;
}


inline
void SSystemMutex::Unlock(void)
{
}
#endif

#if defined(NEED_AUTO_INITIALIZE_MUTEX)

inline
CAutoInitializeStaticFastMutex::TObject&
CAutoInitializeStaticFastMutex::Get(void)
{
    if ( !m_Mutex.IsInitialized() ) {
        Initialize();
    }
    return m_Mutex;
}

inline
CAutoInitializeStaticFastMutex::
operator CAutoInitializeStaticFastMutex::TObject&(void)
{
    return Get();
}

inline
void CAutoInitializeStaticFastMutex::Lock(void)
{
    Get().Lock();
}

inline
void CAutoInitializeStaticFastMutex::Unlock(void)
{
    Get().Unlock();
}

inline
bool CAutoInitializeStaticFastMutex::TryLock(void)
{
    return Get().TryLock();
}

inline
CAutoInitializeStaticMutex::TObject&
CAutoInitializeStaticMutex::Get(void)
{
    if ( !m_Mutex.IsInitialized() ) {
        Initialize();
    }
    return m_Mutex;
}

inline
CAutoInitializeStaticMutex::
operator CAutoInitializeStaticMutex::TObject&(void)
{
    return Get();
}

inline
void CAutoInitializeStaticMutex::Lock(void)
{
    Get().Lock();
}

inline
void CAutoInitializeStaticMutex::Unlock(void)
{
    Get().Unlock();
}

inline
bool CAutoInitializeStaticMutex::TryLock(void)
{
    return Get().TryLock();
}

#endif

/////////////////////////////////////////////////////////////////////////////
//  CFastMutex::
//

inline
CFastMutex::CFastMutex(void)
{
    m_Mutex.InitializeDynamic();
}

inline
CFastMutex::~CFastMutex(void)
{
    m_Mutex.Destroy();
}

inline
CFastMutex::operator SSystemFastMutex&(void)
{
    return m_Mutex;
}

inline
void CFastMutex::Lock(void)
{
    m_Mutex.Lock();
}

inline
void CFastMutex::Unlock(void)
{
    m_Mutex.Unlock();
}

inline
bool CFastMutex::TryLock(void)
{
    return m_Mutex.TryLock();
}

inline
CMutex::CMutex(void)
{
    m_Mutex.InitializeDynamic();
}

inline
CMutex::~CMutex(void)
{
    m_Mutex.Destroy();
}

inline
CMutex::operator SSystemMutex&(void)
{
    return m_Mutex;
}

inline
void CMutex::Lock(void)
{
    m_Mutex.Lock();
}

inline
void CMutex::Unlock(void)
{
    m_Mutex.Unlock();
}

inline
bool CMutex::TryLock(void)
{
    return m_Mutex.TryLock();
}


/*
 * ===========================================================================
 * $Log$
 * Revision 1.6  2004/06/16 11:37:33  dicuccio
 * Refactored CMutexGuard, CFastMutexGuard, CReadLockGuard, and CWriteLockGuard to
 * use a templated implementation (CGuard<>).  Provided typedefs for forward and
 * backward compatibility.
 *
 * Revision 1.5  2003/09/02 16:08:48  vasilche
 * Fixed race condition with optimization on some compilers - added 'volatile'.
 * Moved mutex Lock/Unlock methods out of inline section - they are quite complex.
 *
 * Revision 1.4  2003/09/02 15:41:05  ucko
 * Re-sync preprocessor guards with ncbimtx.hpp.
 *
 * Revision 1.3  2003/06/19 19:16:37  vasilche
 * Added CMutexGuard and CFastMutexGuard constructors.
 *
 * Revision 1.2  2002/09/20 20:02:07  vasilche
 * Added public Lock/Unlock/TryLock
 *
 * Revision 1.1  2002/09/19 20:05:41  vasilche
 * Safe initialization of static mutexes
 *
 * ===========================================================================
 */

#endif
