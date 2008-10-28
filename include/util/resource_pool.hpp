#ifndef UTIL___RESOURCEPOOL__HPP
#define UTIL___RESOURCEPOOL__HPP

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
 * Author:  Anatoliy Kuznetsov
 *    General purpose resource pool.
 */

#include <corelib/ncbistd.hpp>
#include <corelib/ncbimtx.hpp>
#include <deque>
#include <vector>

BEGIN_NCBI_SCOPE

/** @addtogroup ResourcePool
 *
 * @{
 */

 /// Default class factory for resource pool (C++ new/delete)
 /// 
template<class Value>
struct CResoursePool_NewClassFactory
{
public:
    static Value* Create() { return new Value; }
    static void Delete(Value* v) { delete v; }
};

 
/// General purpose resource pool (base class, sans default
/// constructor arguments for maximum versatility).
///
/// Intended use is to store reusable objects.
/// Pool frees all vacant objects only upon pools destruction.
/// Subsequent Get/Put calls does not result in objects reallocations and
/// re-initializations. (So the prime target is performance optimization).
/// 
/// Class can be used synchronized across threads 
/// (use CFastMutex as Lock parameter)
///
/// Default creation/destruction protocol is C++ new/delete.
/// Can be overloaded using CF parameter
///
template<class Value, 
         class Lock = CNoLock, 
         class CF   = CResoursePool_NewClassFactory<Value> >
class CResourcePool_Base
{
public: 
    typedef Value                          TValue;
    typedef Lock                           TLock;
    typedef typename Lock::TReadLockGuard  TReadLockGuard;
    typedef typename Lock::TWriteLockGuard TWriteLockGuard;
    typedef CF                             TClassFactory;
    typedef Value*                         TValuePtr;
    typedef deque<Value*>                  TPoolList;

public:
    /// Construction
    ///
    /// @param upper_limit
    ///     Max pool size. Everything coming to pool above this limit is
    ///     destroyed right away. 0 - upper limit not set
    ///
    CResourcePool_Base(size_t capacity_upper_limit, const TClassFactory& cf)
        : m_CF(cf),
          m_UpperLimit(capacity_upper_limit)
    {}

    ~CResourcePool_Base()
    {
        FreeAll();
    }

    /// Return max pool size.
    size_t GetCapacityLimit() const { return m_UpperLimit; }

    /// Set upper limit for pool capacity (everything above this is deleted)
    void SetCapacityLimit(size_t capacity_upper_limit)
    {
        m_UpperLimit = capacity_upper_limit;
    }

    /// Get current pool size (number of objects in the pool)
    size_t GetSize() const 
    {
        TReadLockGuard guard(m_Lock);
        return m_FreeObjects.size();
    }

    /// Get object from the pool. 
    ///
    /// Pool makes no reinitialization or constructor 
    /// call and object is returned in the same state it was put.
    /// If pool has no vacant objects, class factory is called to produce an object.
    /// Caller is responsible for deletion or returning object back to the pool.
    Value* Get()
    {
        TWriteLockGuard guard(m_Lock);

        Value* v;
        if (m_FreeObjects.empty()) {
            v = m_CF.Create();
        } else {
            typename TPoolList::iterator it = m_FreeObjects.end();
            v = *(--it);
            m_FreeObjects.pop_back();
        }
        return v;
    }

    /// Get object from the pool if there is a vacancy, 
    /// otherwise returns NULL
    Value* GetIfAvailable()
    {
        TWriteLockGuard guard(m_Lock);

        if (m_FreeObjects.empty()) {
            return 0;
        }
        Value* v;
        typename TPoolList::iterator it = m_FreeObjects.end();
        v = *(--it);
        m_FreeObjects.pop_back();
        return v;
    }

    /// Put object into the pool. 
    ///
    /// Pool does not check if object is actually
    /// originated in the very same pool. It's ok to get an object from one pool
    /// and return it to another pool.
    /// Method does NOT immidiately destroy the object v. 
    void Put(Value* v)
    {        
        TWriteLockGuard guard(m_Lock);

        _ASSERT(v);
        if (m_UpperLimit && (m_FreeObjects.size() >= m_UpperLimit)) {
            m_CF.Delete(v);
        } else {
            m_FreeObjects.push_back(v);
        }
    }

    void Return(Value* v) 
    { 
        Put(v); 
    }

    /// Makes the pool to forget the object.
    ///
    /// Method scans the free objects list, finds the object and removes
    /// it from the structure. It is important that the object is not
    /// deleted and it is responsibility of the caller to destroy it.
    ///
    /// @return NULL if object does not belong to the pool or 
    ///    object's pointer otherwise.
    Value* Forget(Value* v)
    {
        TWriteLockGuard guard(m_Lock);

        NON_CONST_ITERATE(typename TPoolList, it, m_FreeObjects) {
            Value* vp = *it;
            if (v == vp) {
                m_FreeObjects.erase(it);
                return v;
            }
        }
        return 0;
    }

    /// Makes pool to forget all objects
    ///
    /// Method removes all objects from the internal list but does NOT
    /// deallocate the objects.
    void ForgetAll()
    {
        TWriteLockGuard guard(m_Lock);

        m_FreeObjects.clear();
    }

    /// Free all pool objects.
    ///
    /// Method removes all objects from the internal list and
    /// deallocates the objects.
    void FreeAll()
    {
        TWriteLockGuard guard(m_Lock);

        ITERATE(typename TPoolList, it, m_FreeObjects) {
            Value* v = *it;
            m_CF.Delete(v);
        }
        m_FreeObjects.clear();
    }


    /// Get internal list of free objects
    ///
    /// Be very careful with this function! It does not provide MT sync.
    TPoolList& GetFreeList() 
    { 
        return m_FreeObjects; 
    }

    /// Get internal list of free objects
    ///
    /// No MT sync here !
    const TPoolList& GetFreeList() const 
    { 
        return m_FreeObjects; 
    }

protected:
    CResourcePool_Base(const CResourcePool_Base&);
    CResourcePool_Base& operator=(const CResourcePool_Base&);

protected:
    TPoolList                 m_FreeObjects;
    mutable TLock             m_Lock;
    TClassFactory             m_CF;
    size_t                    m_UpperLimit; ///< Upper limit how much to pool
};


/// General purpose resource pool (standard version, requiring CF to
/// have a default constructor).
///
template<class Value, 
         class Lock = CNoLock, 
         class CF   = CResoursePool_NewClassFactory<Value> >
class CResourcePool : public CResourcePool_Base<Value, Lock, CF>
{
public:
    typedef CResourcePool_Base<Value, Lock, CF> TBase;
    typedef typename TBase::TValue              TValue;
    typedef typename TBase::TLock               TLock;
    typedef typename TBase::TReadLockGuard      TReadLockGuard;
    typedef typename TBase::TWriteLockGuard     TWriteLockGuard;
    typedef typename TBase::TClassFactory       TClassFactory;
    typedef typename TBase::TValuePtr           TValuePtr;
    typedef typename TBase::TPoolList           TPoolList;

    CResourcePool(size_t capacity_upper_limit = 0,
                  const TClassFactory& cf     = TClassFactory())
        : TBase(capacity_upper_limit, cf)
    {}

protected:
    CResourcePool(const CResourcePool&);
    CResourcePool& operator=(const CResourcePool&);
};


/// Guard object. Returns object pointer to the pool upon destruction.
/// @sa CResourcePool
template<class Pool>
class CResourcePoolGuard
{
public:
    CResourcePoolGuard(Pool& pool, typename Pool::TValue* v)
    : m_Pool(pool),
      m_Value(v)
    {}

    ~CResourcePoolGuard()
    {
        Return();
    }

    /// Return the pointer to the caller, not to the pool
    typename Pool::TValue* Release()
    {
        typename Pool::TValue* ret = m_Value;
        m_Value = 0;
        return ret;
    }
    
    /// Get the protected pointer
    typename Pool::TValue* Get() { return m_Value; }

    /// Return the protected object back to the pool
    void Return() 
    {
        if (m_Value) {
            m_Pool.Return(m_Value);
        }
        m_Value = 0;
    }
private:
    CResourcePoolGuard(const CResourcePoolGuard&);
    CResourcePoolGuard& operator=(const CResourcePoolGuard&);
private:
    Pool&                     m_Pool;
    typename Pool::TValue*    m_Value;
};


/// Bucket of resourse pools.
///
/// This object is a wrap on a vector of resource pools, it
/// automates management of multiple resource pools. 
/// 
template<class Value,
         class Lock = CNoLock,
         class RPool = CResourcePool<Value, Lock> >
class CBucketPool
{
public:
    typedef Value                           TValue;
    typedef Lock                            TLock;
    typedef typename Lock::TWriteLockGuard  TWriteLockGuard;
    typedef RPool                           TResourcePool;
    typedef vector<TResourcePool*>          TBucketVector;

public:
    /// Construction
    ///
    /// @param bucket_ini
    ///     Initial size of pool bucket. 
    ///     Backet resized dynamically if it needs to.
    /// @param resource_pool_capacity_limit
    ///     Upper limit for how many objects pool can store
    ///
    CBucketPool(size_t bucket_ini = 0, 
                size_t resource_pool_capacity_limit = 0)
        : m_Bucket(bucket_ini),
          m_ResourcePoolUpperLimit(resource_pool_capacity_limit)
    {
        for (size_t i = 0; i < m_Bucket.size(); ++i) {
            m_Bucket[i] = 0;
        }
    }

    ~CBucketPool() 
    { 
        TWriteLockGuard guard(m_Lock);
        x_FreeAll_NoLock(); 
    }

    /// Free all objects in all pools
    ///
    void FreeAllPools()
    {
        // code here optimized for MT competitive execution
        // it does not hold the top level lock for a long time
        //
        size_t bsize;
        {{
        TWriteLockGuard guard(m_Lock);
        bsize = m_Bucket.size();
        }}
        for (size_t i = 0; i < bsize; ++i) {
            TResourcePool* rp = 0;
            {{
                TWriteLockGuard guard(m_Lock);
                if (m_Bucket.size() < i) {
                    break;
                }
                TResourcePool* rp = m_Bucket[i];
            }}
            if (rp) {
                rp->FreeAll();
            }
        } // for
    }

    /// Get resource pool for the specified backet
    /// 
    /// Backet grows automatically upon request
    ///
    TResourcePool* GetResourcePool(size_t backet)
    {
        TWriteLockGuard guard(m_Lock);

        // bucket resize
        while (m_Bucket.size() < backet+1) {
            m_Bucket.push_back(0);
        }
        TResourcePool* rp = m_Bucket[backet];
        if (!rp) {
            rp = new TResourcePool(m_ResourcePoolUpperLimit);
            m_Bucket[backet] = rp;
        }
        return rp;
    }

    const TBucketVector& GetBucketVector() const { return m_Bucket; }

private:
    CBucketPool(const CBucketPool&);
    CBucketPool& operator=(const CBucketPool&);
private:
    void x_FreeAll_NoLock()
    {
        for (size_t i = 0; i < m_Bucket.size(); ++i) {
            delete m_Bucket[i]; m_Bucket[i] = 0;
        }
    }

protected:
    TBucketVector           m_Bucket;
    TLock                   m_Lock;
    size_t                  m_ResourcePoolUpperLimit;
};


/* @} */


END_NCBI_SCOPE


#endif  /* UTIL___RESOURCEPOOL__HPP */
