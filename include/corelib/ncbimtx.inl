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

#if defined(NCBI_NO_THREADS)
// empty version of Lock/Unlock methods for inlining
inline
void SSystemFastMutex::Lock(void)
{
}


inline
bool SSystemFastMutex::TryLock(void)
{
    return true;
}


inline
void SSystemFastMutex::Unlock(void)
{
}
#endif

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

#endif
