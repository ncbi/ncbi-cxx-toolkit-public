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
*  Author: Eugene Vasilchenko
*
*  File Description:
*    GenBank data loader in-memory cache
*
*/

#include <ncbi_pch.hpp>
#include <objtools/data_loaders/genbank/impl/info_cache.hpp>
#include <objtools/data_loaders/genbank/impl/request_result.hpp>
#include <objtools/error_codes.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(GBL)


/////////////////////////////////////////////////////////////////////////////
// CInfo_Base
/////////////////////////////////////////////////////////////////////////////


CInfo_Base::CInfo_Base(TGCQueue& gc_queue)
    : m_UseCounter(0),
      m_ExpirationTime(0),
      m_GCQueuePos(gc_queue.end())
{
}


CInfo_Base::~CInfo_Base(void)
{
}


/////////////////////////////////////////////////////////////////////////////
// CInfoRequestorLock
/////////////////////////////////////////////////////////////////////////////


CInfoRequestorLock::CInfoRequestorLock(CInfoRequestor& requestor,
                                       CInfo_Base& info)
    : m_Requestor(requestor),
      m_Info(&info)
{
}


CInfoRequestorLock::~CInfoRequestorLock(void)
{
}


bool CInfoRequestorLock::SetLoadedFor(TExpirationTime new_expiration_time)
{
    _ASSERT(!IsLocked() || m_Mutex->m_LoadingRequestor == &GetRequestor());
    CInfo_Base& info = GetInfo();
    bool changed = new_expiration_time > info.GetExpirationTime();
    if ( changed ) {
        info.m_ExpirationTime = new_expiration_time;
    }
    GetManager().ReleaseLoadLock(*this);
    return changed;
}


bool CInfoRequestorLock::x_SetLoadedFor(TMainMutex::TWriteLockGuard& guard,
                                        TExpirationTime new_expiration_time)
{
    _ASSERT(!IsLocked() || m_Mutex->m_LoadingRequestor == &GetRequestor());
    CInfo_Base& info = GetInfo();
    bool changed = new_expiration_time > info.GetExpirationTime();
    if ( changed ) {
        info.m_ExpirationTime = new_expiration_time;
    }
    GetManager().x_ReleaseLoadLock(*this);
    return changed;
}


/////////////////////////////////////////////////////////////////////////////
// CInfoLock_Base
/////////////////////////////////////////////////////////////////////////////


#if !USE_MAIN_MUTEX_FOR_DATA
DEFINE_CLASS_STATIC_MUTEX(CInfoLock_Base::sm_DataMutex);
#endif


/////////////////////////////////////////////////////////////////////////////
// CInfoManager
/////////////////////////////////////////////////////////////////////////////


CInfoManager::CInfoManager(void)
{
}


CInfoManager::~CInfoManager(void)
{
}


void CInfoManager::x_LockInfoMutex(CInfoRequestorLock& lock)
{
    _ASSERT(!lock.IsLocked());
    CInfoRequestor& requestor = lock.GetRequestor();
    CInfo_Base& info = lock.GetInfo();
    CLoadMutex& mutex = *info.m_LoadMutex;
    _ASSERT(!mutex.IsLoading());
    //_VERIFY(requestor.m_LockedInfos.insert(Ref(&info)).second);
    mutex.Lock();
    mutex.m_LoadingRequestor = &requestor;
    lock.m_Mutex = &mutex;
    _ASSERT(lock.IsLocked());
}


void CInfoManager::x_UnlockInfoMutex(CInfoRequestorLock& lock)
{
    _ASSERT(lock.IsLocked());
    CLoadMutex& mutex = *lock.m_Mutex;
    _ASSERT(&mutex == lock.GetInfo().m_LoadMutex);
    _ASSERT(mutex.IsLoading());
    _ASSERT(mutex.m_LoadingRequestor == &lock.GetRequestor());
    mutex.m_LoadingRequestor = 0;
    mutex.Unlock();
    lock.m_Mutex = null;
    _ASSERT(!lock.IsLocked());
}


void CInfoManager::x_AssignLoadMutex(CRef<CLoadMutex>& mutex)
{
    _ASSERT(!mutex);
    if ( m_LoadMutexPool.empty() ) {
        mutex = new CLoadMutex();
    }
    else {
        mutex = m_LoadMutexPool.back();
        m_LoadMutexPool.pop_back();
    }
    _ASSERT(mutex);
    _ASSERT(mutex->ReferencedOnlyOnce());
    _ASSERT(!mutex->IsLoading());
}


void CInfoManager::x_ReleaseLoadMutex(CRef<CLoadMutex>& mutex)
{
    _ASSERT(mutex);
    _ASSERT(!mutex->IsLoading());
    if ( mutex->ReferencedOnlyOnce() ) {
        m_LoadMutexPool.push_back(mutex);
        mutex = null;
        _ASSERT(!m_LoadMutexPool.back()->IsLoading());
    }
}


void CInfoManager::x_AssignLoadMutex(CInfo_Base& info)
{
    x_AssignLoadMutex(info.m_LoadMutex);
}


void CInfoManager::x_ReleaseLoadMutex(CInfo_Base& info)
{
    x_ReleaseLoadMutex(info.m_LoadMutex);
}


void CInfoManager::x_AcquireLoadLock(CInfoRequestorLock& lock,
                                     EDoNotWait do_not_wait)
{
    _ASSERT(lock.m_Info);
    if ( lock.IsLocked() ) {
        // relocking of already locked info - do nothing
        return;
    }
    _ASSERT(!lock.IsLocked());
    TMainMutex::TWriteLockGuard guard(GetMainMutex());
    x_AcquireLoadLock(guard, lock, do_not_wait);
}


void CInfoManager::x_AcquireLoadLock(TMainMutex::TWriteLockGuard& guard,
                                     CInfoRequestorLock& lock,
                                     EDoNotWait do_not_wait)
{
    _ASSERT(lock.m_Info);
    if ( lock.IsLocked() ) {
        // relocking of already locked info - do nothing
        guard.Release();
        return;
    }
    _ASSERT(!lock.IsLocked());
    for (;;) {
        if ( lock.IsLoaded() ) {
            // no need to load, leave immediately
            _ASSERT(!lock.IsLocked());
            guard.Release();
            return;
        }

        CInfo_Base& info = lock.GetInfo();
        if ( !info.m_LoadMutex ) {
            // no active loading thread, mark as loading and lock
            x_AssignLoadMutex(info);
            x_LockInfoMutex(lock);
            guard.Release();
            return;
        }

        // check for deadlock
        if ( do_not_wait || x_DeadLock(lock.GetRequestor(), info) ) {
            // deadlock possible, continue loading without locking
            guard.Release();
            return;
        }

        // no deadlock, wait for other thread loading, then retry
        if ( x_WaitForOtherLoader(guard, lock) ) {
            return;
        }
    }
}


bool CInfoManager::x_WaitForOtherLoader(TMainMutex::TWriteLockGuard& guard,
                                        CInfoRequestorLock& lock)
{
    CInfo_Base& info = lock.GetInfo();
    CRef<CLoadMutex> mutex = info.m_LoadMutex;
    _ASSERT(mutex);
    _ASSERT(!lock.IsLocked());
    CInfoRequestor& requestor = lock.GetRequestor();
    requestor.m_WaitingForInfo = &info;
    guard.Release();
    {{
        // wait for other loading thread
        CLoadMutex::TWriteLockGuard guard(*mutex);
    }}
    if ( lock.IsLoaded() ) {
        // no need to load, leave immediately
        _ASSERT(!lock.IsLocked());
        _ASSERT(requestor.m_WaitingForInfo == &info);
        requestor.m_WaitingForInfo = null;
        return true;
    }
    guard.Guard(GetMainMutex());
    _ASSERT(!lock.IsLocked());
    _ASSERT(requestor.m_WaitingForInfo == &info);
    requestor.m_WaitingForInfo = null;
    if ( mutex == info.m_LoadMutex ) {
        // still loading or needs to be loaded
        return !mutex->IsLoading();
    }
    else {
        // loaded
        _ASSERT(!mutex->IsLoading());
        x_ReleaseLoadMutex(mutex);
    }
    return false;
}


bool CInfoManager::x_DeadLock(const CInfoRequestor& requestor,
                              const CInfo_Base& info) const
{
    _ASSERT(info.m_LoadMutex);
    const CInfo_Base* info_ptr = &info;
    for ( ;; ) {
        const CInfoRequestor* req = info_ptr->m_LoadMutex->m_LoadingRequestor;
        if ( !req ) {
            return false;
        }
        if ( req == &requestor ) {
            return true;
        }
        info_ptr = req->m_WaitingForInfo;
        if ( !info_ptr ) {
            return false;
        }
        _ASSERT(info_ptr->m_LoadMutex);
    }
}


void CInfoManager::x_ReleaseLoadLock(CInfoRequestorLock& lock)
{
    if ( lock.IsLocked() ) {
        x_UnlockInfoMutex(lock);
        x_ReleaseLoadMutex(lock.GetInfo());
    }
}


void CInfoManager::ReleaseAllLoadLocks(CInfoRequestor& requestor)
{
    TMainMutex::TWriteLockGuard guard(GetMainMutex());
    ITERATE ( CInfoRequestor::TLockMap, it, requestor.m_LockMap ) {
        x_ReleaseLoadLock(it->second.GetNCObject());
    }
}


void CInfoManager::ReleaseLoadLock(CInfoRequestorLock& lock)
{
    TMainMutex::TWriteLockGuard guard(GetMainMutex());
    x_ReleaseLoadLock(lock);
}


/////////////////////////////////////////////////////////////////////////////
// CInfoCache_Base
/////////////////////////////////////////////////////////////////////////////


#if USE_MAIN_MUTEX_IN_CACHE
CInfoCache_Base::CInfoCache_Base(CInfoManager::TMainMutex& mutex)
    : m_CacheMutex(mutex),
      m_MaxGCQueueSize(0),
      m_MinGCQueueSize(0),
      m_CurGCQueueSize(0)
{
    SetMaxGCQueueSize(kDefaultMaxSize);
}
CInfoCache_Base::CInfoCache_Base(CInfoManager::TMainMutex& mutex,
                                 size_t max_size)
    : m_CacheMutex(mutex),
      m_MaxGCQueueSize(0),
      m_MinGCQueueSize(0),
      m_CurGCQueueSize(0)
{
    SetMaxGCQueueSize(max_size);
}
#else
CInfoCache_Base::CInfoCache_Base(CInfoManager::TMainMutex& /*mutex*/)
    : m_MaxGCQueueSize(0),
      m_MinGCQueueSize(0),
      m_CurGCQueueSize(0)
{
    SetMaxGCQueueSize(kDefaultMaxSize);
}
CInfoCache_Base::CInfoCache_Base(CInfoManager::TMainMutex& /*mutex*/,
                                 size_t max_size)
    : m_MaxGCQueueSize(0),
      m_MinGCQueueSize(0),
      m_CurGCQueueSize(0)
{
    SetMaxGCQueueSize(max_size);
}
#endif

CInfoCache_Base::~CInfoCache_Base(void)
{
}


bool CInfoCache_Base::x_Check(const vector<const CInfo_Base*>& infos) const
{
    size_t unused_count = 0, used_count = 0;
    ITERATE ( vector<const CInfo_Base*>, it, infos ) {
        const CInfo_Base& info = **it;
        if ( info.m_UseCounter > 0 ) {
            ++used_count;
        }
        else {
            ++unused_count;
        }
    }
    _ASSERT(unused_count == m_CurGCQueueSize);
    return true;
}


void CInfoCache_Base::SetMaxGCQueueSize(size_t max_size)
{
    TCacheMutex::TWriteLockGuard guard(m_CacheMutex);
    m_MaxGCQueueSize = max_size;
    m_MinGCQueueSize = size_t(m_MaxGCQueueSize*0.9);
    if ( m_CurGCQueueSize > m_MaxGCQueueSize ) {
        x_GC();
    }
}


inline
void CInfoCache_Base::x_RemoveFromGCQueue(CInfo_Base& info)
{
    _ASSERT(info.m_UseCounter >= 0);
    _ASSERT(info.m_GCQueuePos != m_GCQueue.end());
    m_GCQueue.erase(info.m_GCQueuePos);
    info.m_GCQueuePos = m_GCQueue.end();
    --m_CurGCQueueSize;
}


inline
void CInfoCache_Base::x_AddToGCQueue(CInfo_Base& info)
{
    _ASSERT(info.m_UseCounter == 0);
    _ASSERT(info.m_GCQueuePos == m_GCQueue.end());
    if ( m_MaxGCQueueSize == 0 ) {
        x_ForgetInfo(info);
    }
    else {
        info.m_GCQueuePos = m_GCQueue.insert(m_GCQueue.end(), Ref(&info));
        ++m_CurGCQueueSize;
        if ( m_CurGCQueueSize > m_MaxGCQueueSize ) {
            x_GC();
        }
    }
}


inline
void CInfoCache_Base::x_SetUsed(CInfo_Base& info)
{
    _ASSERT(info.m_UseCounter >= 0);
    if ( ++info.m_UseCounter == 1 && info.m_GCQueuePos != m_GCQueue.end() ) {
        x_RemoveFromGCQueue(info);
    }
}


inline
void CInfoCache_Base::x_SetUnused(CInfo_Base& info)
{
    _ASSERT(info.m_UseCounter > 0);
    if ( --info.m_UseCounter == 0 ) {
        x_AddToGCQueue(info);
    }
}


void CInfoCache_Base::x_SetInfo(CInfoLock_Base& lock,
                                CInfoRequestor& requestor,
                                CInfo_Base& info)
{
    lock.m_Lock = requestor.x_GetLock(*this, info);
}


void CInfoCache_Base::ReleaseInfos(const vector<CInfo_Base*>& infos)
{
    TCacheMutex::TWriteLockGuard guard(m_CacheMutex);
    ITERATE ( vector<CInfo_Base*>, it, infos ) {
        x_SetUnused(**it);
    }
}


void CInfoCache_Base::x_GC(void)
{
    _ASSERT(m_CurGCQueueSize == m_GCQueue.size());
    while ( m_CurGCQueueSize > m_MinGCQueueSize ) {
        _ASSERT(!m_GCQueue.empty());
        CRef<CInfo_Base> info = m_GCQueue.front();
        _ASSERT(info);
        _ASSERT(info->m_UseCounter == 0);
        x_ForgetInfo(*info);
        x_RemoveFromGCQueue(*info);
        _ASSERT(m_GCQueue.empty() || m_GCQueue.front() != info);
    }
    _ASSERT(m_CurGCQueueSize == m_GCQueue.size());
}


/////////////////////////////////////////////////////////////////////////////
// CInfoRequestor
/////////////////////////////////////////////////////////////////////////////


CInfoRequestor::CInfoRequestor(CInfoManager& manager)
    : m_Manager(&manager)
{
}


CInfoRequestor::~CInfoRequestor(void)
{
    ReleaseAllLoadLocks();
    ReleaseAllUsedInfos();
}


CRef<CInfoRequestorLock> CInfoRequestor::x_GetLock(CInfoCache_Base& cache,
                                                   CInfo_Base& info)
{
    CRef<CInfoRequestorLock>& lock = m_LockMap[&info];
    if ( !lock ) {
        lock = new CInfoRequestorLock(*this, info);
        cache.x_SetUsed(info);
        m_CacheMap[&cache].push_back(&info);
    }
    return lock;
}


void CInfoRequestor::ReleaseAllUsedInfos(void)
{
    ITERATE ( TCacheMap, it, m_CacheMap ) {
        it->first->ReleaseInfos(it->second);
    }
    m_CacheMap.clear();
    m_LockMap.clear();
}


END_SCOPE(GBL)
END_SCOPE(objects)
END_NCBI_SCOPE
