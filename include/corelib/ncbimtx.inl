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
void SSystemFastMutex::Lock(ELockSemantics)
{
}


inline
bool SSystemFastMutex::TryLock(void)
{
    return true;
}


inline
void SSystemFastMutex::Unlock(ELockSemantics)
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
    m_Owner = 0;
    m_Count = 0;
}


#if defined(NCBI_NO_THREADS)
// empty version of Lock/Unlock methods for inlining
inline
void SSystemMutex::Lock(SSystemFastMutex::ELockSemantics)
{
}


inline
bool SSystemMutex::TryLock(void)
{
    return true;
}


inline
void SSystemMutex::Unlock(SSystemFastMutex::ELockSemantics)
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


inline
CSpinLock::CSpinLock(void)
    : m_Value(NULL)
{}

inline
CSpinLock::~CSpinLock(void)
{
    _ASSERT(m_Value == NULL);
}

inline
bool CSpinLock::IsLocked(void) const
{
    return m_Value != NULL;
}


inline
CFastRWLock::CFastRWLock(void)
{
    m_LockCount.Set(0);
}

inline
CFastRWLock::~CFastRWLock(void)
{
    _ASSERT(m_LockCount.Get() == 0);
}


inline void
CRWLockHolder::Init(CYieldingRWLock* lock, ERWLockType typ)
{
    _ASSERT(lock);

    m_Lock = lock;
    m_Type = typ;
}

inline void
CRWLockHolder::Reset(void)
{
    m_Lock = NULL;
    m_LockAcquired = false;
    m_Listeners.clear();
}

inline
CRWLockHolder::CRWLockHolder(IRWLockHolder_Factory* factory)
    : m_Factory(factory)
{
    _ASSERT(factory);

    Reset();
}

inline
IRWLockHolder_Factory* CRWLockHolder::GetFactory(void) const
{
    return m_Factory;
}

inline
CYieldingRWLock* CRWLockHolder::GetRWLock(void) const
{
    return m_Lock;
}

inline
ERWLockType CRWLockHolder::GetLockType(void) const
{
    return m_Type;
}

inline
bool CRWLockHolder::IsLockAcquired(void) const
{
    return m_LockAcquired;
}

inline
void CRWLockHolder::ReleaseLock(void)
{
    _ASSERT(m_Lock);

    m_Lock->x_ReleaseLock(this);
}

inline
void CRWLockHolder::AddListener(IRWLockHolder_Listener* listener)
{
    _ASSERT(m_Lock);

    m_ObjLock.Lock();
    m_Listeners.push_back(TRWLockHolder_ListenerWeakRef(listener));
    m_ObjLock.Unlock();
}

inline
void CRWLockHolder::RemoveListener(IRWLockHolder_Listener* listener)
{
    _ASSERT(m_Lock);

    m_ObjLock.Lock();
    // m_Listeners.remove(TRWLockHolder_ListenerWeakRef(listener));
    // The above gives strange errors about invalid operands to operator==
    // with the Apple Developer Tools release containing Xcode 6.3.1 and
    // "Apple LLVM version 6.1.0 (clang-602.0.49) (based on LLVM 3.6.0svn)".
    // The below workaround should be equivalent.
    TRWLockHolder_ListenerWeakRef ref(listener);
    TListenersList::iterator it;
    while ((it = find(m_Listeners.begin(), m_Listeners.end(), ref))
           != m_Listeners.end()) {
        m_Listeners.erase(it);
    }
    m_ObjLock.Unlock();
}


inline
TRWLockHolderRef CYieldingRWLock::AcquireReadLock(void)
{
    return AcquireLock(eReadLock);
}

inline
TRWLockHolderRef CYieldingRWLock::AcquireWriteLock(void)
{
    return AcquireLock(eWriteLock);
}

inline
bool CYieldingRWLock::IsLocked(void)
{
    m_ObjLock.Lock();
    bool locked = m_Locks[eReadLock] + m_Locks[eWriteLock] != 0;
    m_ObjLock.Unlock();
    return locked;
}

#endif
