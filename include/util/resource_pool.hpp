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
#include <vector>

BEGIN_NCBI_SCOPE

/** @addtogroup ResourcePool
 *
 * @{
 */


/// General purpose resource pool.
///
/// Intended use is to store reusable objects.
/// Pool frees all vacant objects only upon pools destruction.
/// Subsequent Get/Put calls does not result in objects reallocations and
/// reinitializations. (So the prime target is performance optimization).
/// 
/// Class can be used syncronized across threads 
/// (use CFastMutex as Lock parameter)
///
template<class Value, class Lock=CNoLock>
class CResourcePool
{
public: 
    typedef Value                          TValue;
    typedef Lock                           TLock;
    typedef typename Lock::TWriteLockGuard TWriteLockGuard;
    typedef Value*                         TValuePtr;
    typedef vector<Value*>                 TPoolList;

public:
    CResourcePool()
    {}

    ~CResourcePool()
    {
        TWriteLockGuard guard(m_Lock);

        ITERATE(typename TPoolList, it, m_FreeObjects) {
            Value* v = *it;
            delete v;
        }
    }

    /// Get object from the pool. 
    ///
    /// Pool makes no reinitialization or constructor 
    /// call and object is returned in the same state it was put.
    /// If pool has no vacant objects, new is called to produce an object.
    /// Caller is responsible for deletion or returning object back to the pool.
    Value* Get()
    {
        TWriteLockGuard guard(m_Lock);

        Value* v;
        if (m_FreeObjects.empty()) {
            v = new Value;
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
        m_FreeObjects.push_back(v);
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
    CResourcePool(const CResourcePool&);
    CResourcePool& operator=(const CResourcePool&);

protected:
    TPoolList   m_FreeObjects;
    TLock       m_Lock;
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
        if (m_Value) {
            m_Pool.Return(m_Value);
        }
    }

    /// Return the pointer to the caller, not to the pool
    typename Pool::TValue* Release()
    {
        typename Pool::TValue* ret = m_Value;
        m_Value = 0;
        return ret;
    }

private:
    Pool&                     m_Pool;
    typename Pool::TValue*    m_Value;
};


/* @} */


END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.9  2006/11/22 09:17:39  kuznets
 * Fixed recoursive mutex bug
 *
 * Revision 1.8  2006/11/21 06:50:01  kuznets
 * Added Pool locking parameter (mutex)
 *
 * Revision 1.7  2006/11/17 07:36:32  kuznets
 * added guard Release() method
 *
 * Revision 1.6  2006/01/10 14:58:26  kuznets
 * +GetIfAvailable()
 *
 * Revision 1.5  2004/03/10 16:51:09  kuznets
 * Fixed compilation problems (GCC)
 *
 * Revision 1.4  2004/03/10 16:16:48  kuznets
 * Add accessors to internal list of free objects
 *
 * Revision 1.3  2004/02/23 19:18:20  kuznets
 * +CResourcePool::Forget to manually remove objects from the pool.
 *
 * Revision 1.2  2004/02/17 19:06:59  kuznets
 * GCC warning fix
 *
 * Revision 1.1  2004/02/13 20:24:47  kuznets
 * Initial revision. CResourcePool implements light weight solution for pooling
 * of heavy weight objects (intended as optimization tool).
 *
 *
 * ===========================================================================
 */

#endif  /* UTIL___RESOURCEPOOL__HPP */
