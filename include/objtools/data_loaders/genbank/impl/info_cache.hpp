#ifndef GENBANK_IMPL_INFO_CACHE
#define GENBANK_IMPL_INFO_CACHE

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
*  ===========================================================================
*
*  Author: Eugene Vasilchenko
*
*  File Description:
*    GenBank data loader in-memory cache
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbimtx.hpp>
#include <corelib/ncbiobj.hpp>
#include <corelib/ncbitime.hpp>

#include <list>
#include <set>
#include <map>
#include <corelib/hash_map.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(GBL)


#define USE_MAIN_MUTEX_IN_CACHE 0
#define USE_MAIN_MUTEX_FOR_DATA 0

class CInfo_Base;
class CInfoLock_Base;
class CInfoCache_Base;
class CInfoManager;
class CInfoRequestor;
class CInfoRequestorLock;

class INoCopying
{
public:
    INoCopying() {}

private:
    // prevent copying
    INoCopying(const INoCopying&);
    void operator=(const INoCopying&);
};

enum EDoNotWait {
    eAllowWaiting,
    eDoNotWait
};

enum EExpirationType {
    eExpire_normal,
    eExpire_fast
};

class CLoadMutex : public CObject, public CMutex // CFastMutex
{
public:
    CLoadMutex(void)
        : m_LoadingRequestor(0)
        {
        }

    bool IsLoading(void) const
        {
            return m_LoadingRequestor != 0;
        }
    
protected:
    friend class CInfoManager;
    friend class CInfoRequestor;
    friend class CInfoRequestorLock;

    CInfoRequestor* volatile m_LoadingRequestor;
};


class CInfo_Base : public CObject, INoCopying
{
public:
    typedef Uint4 TExpirationTime; // UTC time, seconds since epoch (1970)
    typedef int TUseCounter;
    typedef list< CRef<CInfo_Base> > TGCQueue;
    
    explicit CInfo_Base(TGCQueue& gc_queue);
    virtual ~CInfo_Base(void);

    bool IsLoaded(TExpirationTime expiration_time) const;
    bool IsLoaded(CInfoRequestor& requestor) const;
    TExpirationTime GetExpirationTime(void) const
        {
            return m_ExpirationTime;
        }
    
protected:
    friend class CInfoRequestorLock;
    friend class CInfoCache_Base;
    friend class CInfoManager;
    friend class CInfoRequestor;

    // info usage counter for garbage collector
    TUseCounter      m_UseCounter;
    // info expiration UTC time, seconds is actual until this time
    volatile TExpirationTime  m_ExpirationTime;
    CRef<CLoadMutex> m_LoadMutex; // mutex for loading info
    TGCQueue::iterator m_GCQueuePos; // pos in GC queue if info is not used
};


// CInfoRequestorLock is a lock within a single requesting thread.
// There is no MT-safety required when operating on the CInfoRequestorLock.
// It holds MT-lock for the loaded info object.
class CInfoRequestorLock : public CObject, INoCopying
{
public:
    CInfoRequestorLock(CInfoRequestor& requestor, CInfo_Base& info);
    ~CInfoRequestorLock(void);

protected:
    friend class CInfoLock_Base;
    friend class CInfoCache_Base;
    friend class CInfoRequestor;
    friend class CInfoManager;
    typedef CInfo_Base::TExpirationTime TExpirationTime;

    CInfo_Base& GetInfo(void) const
        {
            return m_Info.GetNCObject();
        }

    CInfoRequestor& GetRequestor(void) const
        {
            return m_Requestor;
        }
    TExpirationTime GetNewExpirationTime(EExpirationType type) const;
    CInfoManager& GetManager(void) const;

    bool IsLocked(void) const
        {
            return m_Mutex.NotNull();
        }

    bool IsLoaded(void) const;
    bool SetLoadedFor(TExpirationTime new_expiration_time);
    bool SetLoaded(EExpirationType type);
    TExpirationTime GetExpirationTime(void) const
        {
            return GetInfo().GetExpirationTime();
        }

    typedef CMutex TMainMutex; // CFastMutex
    bool x_SetLoadedFor(TMainMutex::TWriteLockGuard& guard,
                        TExpirationTime new_expiration_time);
    bool x_SetLoaded(TMainMutex::TWriteLockGuard& guard,
                     EExpirationType type);

    CInfoRequestor& m_Requestor;
    CRef<CInfo_Base> m_Info;
    CRef<CLoadMutex> m_Mutex;
};


class CInfoManager : public CObject, INoCopying
{
public:
    CInfoManager(void);
    virtual ~CInfoManager(void);

    typedef CMutex TMainMutex; // CFastMutex
    TMainMutex& GetMainMutex(void)
        {
            return m_MainMutex;
        }

    void ReleaseAllLoadLocks(CInfoRequestor& requestor);
    void ReleaseLoadLock(CInfoRequestorLock& lock);

protected:
    friend class CInfoRequestor;
    friend class CInfoRequestorLock;
    friend class CInfoCache_Base;

    bool x_WaitForOtherLoader(TMainMutex::TWriteLockGuard& guard,
                              CInfoRequestorLock& lock);
    bool x_DeadLock(const CInfoRequestor& requestor,
                    const CInfo_Base& info) const;

    void x_AssignLoadMutex(CRef<CLoadMutex>& mutex);
    void x_ReleaseLoadMutex(CRef<CLoadMutex>& mutex);

    void x_AssignLoadMutex(CInfo_Base& info);
    void x_ReleaseLoadMutex(CInfo_Base& info);

    void x_LockInfoMutex(CInfoRequestorLock& lock);
    void x_UnlockInfoMutex(CInfoRequestorLock& lock);

    void x_ReleaseLoadLock(CInfoRequestorLock& lock);
    void x_AcquireLoadLock(TMainMutex::TWriteLockGuard& guard,
                           CInfoRequestorLock& lock,
                           EDoNotWait do_not_wait);
    void x_AcquireLoadLock(CInfoRequestorLock& lock,
                           EDoNotWait do_not_wait);

    typedef vector< CRef<CLoadMutex> > TLoadMutexPool;

    TMainMutex m_MainMutex;
    TLoadMutexPool m_LoadMutexPool;
};


class CInfoRequestor : INoCopying
{
public:
    explicit CInfoRequestor(CInfoManager& manager);
    virtual ~CInfoRequestor(void);

    void ReleaseAllLoadLocks(void)
        {
            GetManager().ReleaseAllLoadLocks(*this);
        }

    typedef CInfo_Base::TExpirationTime TExpirationTime;
    virtual TExpirationTime GetRequestTime(void) const = 0;
    virtual TExpirationTime GetNewExpirationTime(EExpirationType type) const = 0;
    
protected:
    friend class CInfoManager;
    friend class CInfoRequestorLock;
    friend class CInfoCache_Base;

    CInfoManager& GetManager(void) const
        {
            return m_Manager.GetNCObject();
        }

    void ReleaseAllUsedInfos(void);

    void ReleaseLoadLock(CInfoRequestorLock& lock)
        {
            GetManager().ReleaseLoadLock(lock);
        }
    
    CRef<CInfoRequestorLock> x_GetLock(CInfoCache_Base& cache,
                                       CInfo_Base& info);

    struct PtrHash {
        size_t operator()(const void* ptr) const {
            return size_t(ptr)>>3;
        }
    };
    typedef hash_map<CInfo_Base*, CRef<CInfoRequestorLock>, PtrHash> TLockMap;
    typedef hash_map<CInfoCache_Base*, vector<CInfo_Base*>, PtrHash> TCacheMap;
    
    CRef<CInfoManager> m_Manager;
    TLockMap m_LockMap; // map from CInfo_Base -> CInfoRequestorLock
    TCacheMap m_CacheMap; // map of used infos

    //set< CRef<CInfo_Base> > m_LockedInfos;
    CRef<CInfo_Base> m_WaitingForInfo;
};


inline
bool CInfo_Base::IsLoaded(TExpirationTime expiration_time) const
{
    return GetExpirationTime() >= expiration_time;
}


inline
bool CInfo_Base::IsLoaded(CInfoRequestor& requestor) const
{
    return IsLoaded(requestor.GetRequestTime());
}


inline
bool CInfoRequestorLock::IsLoaded(void) const
{
    return GetInfo().IsLoaded(GetRequestor());
}


inline
CInfoRequestorLock::TExpirationTime
CInfoRequestorLock::GetNewExpirationTime(EExpirationType type) const
{
    return GetRequestor().GetNewExpirationTime(type);
}


inline
bool CInfoRequestorLock::SetLoaded(EExpirationType type)
{
    return SetLoadedFor(GetNewExpirationTime(type));
}


inline
bool CInfoRequestorLock::x_SetLoaded(TMainMutex::TWriteLockGuard& guard,
                                     EExpirationType type)
{
    return x_SetLoadedFor(guard, GetNewExpirationTime(type));
}


inline
CInfoManager& CInfoRequestorLock::GetManager(void) const
{
    return GetRequestor().GetManager();
}


class CInfoLock_Base
{
public:
    typedef CInfo_Base::TExpirationTime TExpirationTime;

    bool IsLocked(void) const
        {
            return m_Lock->IsLocked();
        }

    bool IsLoaded(void) const
        {
            return m_Lock->IsLoaded();
        }
    bool SetLoadedFor(TExpirationTime expiration_time)
        {
            return m_Lock->SetLoadedFor(expiration_time);
        }
    bool SetLoaded(EExpirationType type)
        {
            return m_Lock->SetLoaded(type);
        }
    TExpirationTime GetExpirationTime(void) const
        {
            return m_Lock->GetExpirationTime();
        }
    TExpirationTime GetNewExpirationTime(EExpirationType type) const
        {
            return m_Lock->GetNewExpirationTime(type);
        }

    CInfoRequestor& GetRequestor(void) const
        {
            return m_Lock->GetRequestor();
        }

    DECLARE_OPERATOR_BOOL_REF(m_Lock);

protected:
    friend class CInfoManager;
    friend class CInfoCache_Base;

#if USE_MAIN_MUTEX_FOR_DATA
    typedef CInfoManager::TMainMutex TDataMutex;
    TDataMutex& GetDataLock(void) const
        {
            return m_Lock->GetManager().GetMainMutex();
        }
    bool x_SetLoadedFor(TDataMutex::TWriteLockGuard& guard,
                        TExpirationTime expiration_time)
        {
            return m_Lock->x_SetLoadedFor(guard, expiration_time);
        }
    bool x_SetLoaded(TDataMutex::TWriteLockGuard& guard,
                     EExpirationType type)
        {
            return m_Lock->x_SetLoaded(guard, type);
        }
#else
    typedef CMutex TDataMutex; // CFastMutex
    DECLARE_CLASS_STATIC_MUTEX(sm_DataMutex);
    SSystemMutex& GetDataLock(void) const
        {
            return sm_DataMutex;
        }
    bool x_SetLoadedFor(TDataMutex::TWriteLockGuard& /*guard*/,
                     TExpirationTime expiration_time)
        {
            return m_Lock->SetLoadedFor(expiration_time);
        }
    bool x_SetLoaded(TDataMutex::TWriteLockGuard& /*guard*/,
                     EExpirationType type)
        {
            return m_Lock->SetLoaded(type);
        }
#endif

    CInfo_Base& GetInfo(void) const
        {
            return m_Lock->GetInfo();
        }

    CRef<CInfoRequestorLock> m_Lock;
};

class CInfoCache_Base : INoCopying
{
public:
    enum {
        kDefaultMaxSize = 10240
    };

    explicit CInfoCache_Base(CInfoManager::TMainMutex& mutex);
    CInfoCache_Base(CInfoManager::TMainMutex& mutex, size_t max_size);
    virtual ~CInfoCache_Base(void);

    size_t GetMaxGCQueueSize(void) const
        {
            return m_MaxGCQueueSize;
        }

    void SetMaxGCQueueSize(size_t max_size);

protected:
    friend class CInfoLock_Base;
    friend class CInfoManager;
    friend class CInfoRequestor;

    typedef CInfoManager::TMainMutex TMainMutex;
#if USE_MAIN_MUTEX_IN_CACHE
    typedef TMainMutex TCacheMutex;
    TCacheMutex& m_CacheMutex;
#else
    typedef CMutex TCacheMutex; // CFastMutex
    TCacheMutex  m_CacheMutex;
#endif

    // mark info as used and set it into CInfoLock_Base
    void x_SetInfo(CInfoLock_Base& lock,
                   CInfoRequestor& requestor,
                   CInfo_Base& info);

    // try to acquire lock or leave it unlocked if deadlock is possible
#if USE_MAIN_MUTEX_IN_CACHE
    void x_AcquireLoadLock(TMainMutex::TWriteLockGuard& guard,
                           CInfoRequestorLock& lock,
                           EDoNotWait do_not_wait)
        {
            lock.GetManager().x_AcquireLoadLock(guard, lock, do_not_wait);
        }
#else
    void x_AcquireLoadLock(TCacheMutex::TWriteLockGuard& guard,
                           CInfoRequestorLock& lock,
                           EDoNotWait do_not_wait)
        {
            guard.Release();
            lock.GetManager().x_AcquireLoadLock(lock, do_not_wait);
        }
#endif
    void x_AcquireLoadLock(TMainMutex::TWriteLockGuard& guard,
                           CInfoLock_Base& lock,
                           EDoNotWait do_not_wait)
        {
            x_AcquireLoadLock(guard, *lock.m_Lock, do_not_wait);
        }

    bool x_Check(const vector<const CInfo_Base*>& infos) const;

    void ReleaseInfos(const vector<CInfo_Base*>& infos);

    void x_SetUsed(CInfo_Base& info);
    void x_SetUnused(CInfo_Base& info);

    void x_RemoveFromGCQueue(CInfo_Base& info);
    void x_AddToGCQueue(CInfo_Base& info);

    void x_GC(void);

    virtual void x_ForgetInfo(CInfo_Base& info) = 0;

    typedef CInfo_Base::TGCQueue TGCQueue;

    size_t m_MaxGCQueueSize, m_MinGCQueueSize, m_CurGCQueueSize;
    TGCQueue m_GCQueue;
};


template<class DataType> class CInfoLock;

template<class DataType>
class CInfo_DataBase : public CInfo_Base
{
public:
    typedef DataType TData;

protected:
    friend class CInfoLock<TData>;

    explicit CInfo_DataBase(typename CInfo_Base::TGCQueue& gc_queue)
        : CInfo_Base(gc_queue)
        {
        }

    TData m_Data;
};


template<class DataType>
class CInfoLock : public CInfoLock_Base
{
public:
    typedef CInfo_DataBase<DataType> TInfo;
    typedef DataType TData;
        
    TData GetData(void) const
        {
            TDataMutex::TReadLockGuard guard(GetDataLock());
            return GetInfo().m_Data;
        }
    bool SetLoaded(const TData& data, EExpirationType type)
        {
            TDataMutex::TWriteLockGuard guard(GetDataLock());
            bool changed = x_SetLoaded(guard, type);
            if ( changed ) {
                GetInfo().m_Data = data;
            }
            return changed;
        }
    bool SetLoadedFor(const TData& data, TExpirationTime expiration_time)
        {
            TDataMutex::TWriteLockGuard guard(GetDataLock());
            bool changed = x_SetLoadedFor(guard, expiration_time);
            if ( changed ) {
                GetInfo().m_Data = data;
            }
            return changed;
        }

protected:
    TInfo& GetInfo(void) const
        {
            return static_cast<TInfo&>(CInfoLock_Base::GetInfo());
        }
};



template<class KeyType, class DataType>
class CInfoCache : public CInfoCache_Base
{
public:
    typedef KeyType key_type;
    typedef DataType data_type;
    typedef CInfo_Base::TExpirationTime TExpirationTime;

    explicit CInfoCache(CInfoManager::TMainMutex& mutex)
        : CInfoCache_Base(mutex)
        {
        }
    CInfoCache(CInfoManager::TMainMutex& mutex, size_t max_size)
        : CInfoCache_Base(mutex, max_size)
        {
        }
    ~CInfoCache(void)
        {
        }

    class CInfo : public CInfo_DataBase<DataType>
    {
    public:
        typedef KeyType TKey;

        const TKey& GetKey(void) const
            {
                return m_Key;
            }

    protected:
        friend class CInfoCache;
        CInfo(typename CInfo_Base::TGCQueue& gc_queue, const TKey& key)
            : CInfo_DataBase<DataType>(gc_queue),
              m_Key(key)
            {
            }


        TKey  m_Key;
    };

    typedef CInfo TInfo;
    typedef CInfoLock<DataType> TInfoLock;

    bool IsLoaded(CInfoRequestor& requestor,
                  const key_type& key)
        {
            TCacheMutex::TReadLockGuard guard(m_CacheMutex);
            _ASSERT(x_Check());
            typename TIndex::iterator iter = m_Index.find(key);
            return iter != m_Index.end() && iter->second->IsLoaded(requestor);
        }
    bool MarkLoading(CInfoRequestor& requestor,
                     const key_type& key)
        {
            return !GetLoadLock(requestor, key).IsLoaded();
        }
    TInfoLock GetLoadLock(CInfoRequestor& requestor,
                          const key_type& key,
                          EDoNotWait do_not_wait = eAllowWaiting)
        {
            TInfoLock lock;
            _ASSERT(x_Check());
            TCacheMutex::TWriteLockGuard guard(m_CacheMutex);
            CRef<CInfo>& slot = m_Index[key];
            if ( !slot ) {
                // new slot
                slot = new CInfo(m_GCQueue, key);
            }
            x_SetInfo(lock, requestor, *slot);
            x_AcquireLoadLock(guard, lock, do_not_wait);
            _ASSERT(x_Check());
            return lock;
        }
    bool SetLoaded(CInfoRequestor& requestor,
                   const key_type& key,
                   const data_type& value,
                   EExpirationType type)
        {
            TCacheMutex::TWriteLockGuard guard(m_CacheMutex);
            _ASSERT(x_Check());
            CRef<CInfo>& slot = m_Index[key];
            if ( !slot ) {
                // new slot
                slot = new CInfo(m_GCQueue, key);
            }
            TInfoLock lock;
            x_SetInfo(lock, requestor, *slot);
            _ASSERT(x_Check());
            return lock.SetLoaded(value, type);
        }
    bool SetLoadedFor(CInfoRequestor& requestor,
                      const key_type& key,
                      const data_type& value,
                      TExpirationTime expiration_time)
        {
            TCacheMutex::TWriteLockGuard guard(m_CacheMutex);
            _ASSERT(x_Check());
            CRef<CInfo>& slot = m_Index[key];
            if ( !slot ) {
                // new slot
                slot = new CInfo(m_GCQueue, key);
            }
            TInfoLock lock;
            x_SetInfo(lock, requestor, *slot);
            _ASSERT(x_Check());
            return lock.SetLoadedFor(value, expiration_time);
        }
    TInfoLock GetLoaded(CInfoRequestor& requestor,
                        const key_type& key)
        {
            TInfoLock lock;
            TCacheMutex::TWriteLockGuard guard(m_CacheMutex);
            _ASSERT(x_Check());
            typename TIndex::iterator iter = m_Index.find(key);
            if ( iter != m_Index.end() &&
                 iter->second->IsLoaded(requestor) ) {
                x_SetInfo(lock, requestor, *iter->second);
            }
            _ASSERT(x_Check());
            return lock;
        }

protected:
    friend class CInfoLock_Base;


    bool x_Check(void) const
        {
            return true;
            vector<const CInfo_Base*> infos;
            ITERATE ( typename TIndex, it, m_Index ) {
                infos.push_back(it->second);
            }
            return CInfoCache_Base::x_Check(infos);
        }


    virtual void x_ForgetInfo(CInfo_Base& info_base)
        {
            _ASSERT(dynamic_cast<TInfo*>(&info_base));
            _VERIFY(m_Index.erase(static_cast<TInfo&>(info_base).GetKey()));
        }

private:
    typedef map<key_type, CRef<TInfo> >    TIndex;

    TIndex m_Index;
};


END_SCOPE(GBL)
END_SCOPE(objects)
END_NCBI_SCOPE

#endif // GENBANK_IMPL_INFO_CACHE
