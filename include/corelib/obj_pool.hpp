#ifndef CORELIB___OBJ_POOL__HPP
#define CORELIB___OBJ_POOL__HPP

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
 * Author:  Pavel Ivanov
 *    General purpose object pool.
 */

#include <corelib/ncbistd.hpp>
#include <corelib/ncbimtx.hpp>
#include <deque>

BEGIN_NCBI_SCOPE

/** @addtogroup ResourcePool
 *
 * @{
 */

template <class TObjType> class CObjFactory_New;


/// General object pool
///
/// @param TObjType
///   Type of objects in the pool (any type, not necessarily inherited from
///   CObject). Pool holds pointers to objects, so they shouldn't be copyable.
/// @param TObjFactory
///   Type of object factory, representing the policy of how objects created
///   and how they deleted. Factory should implement 2 methods -
///   CreateObject() and DeleteObject(). CreateObject() should get no
///   parameters, create new object and return pointer to it. Returned pointer
///   should be always non-NULL. DeleteObject() should take one parameter
///   (pointer to object) and should delete given object.
/// @param TLock
///   Type of the lock used in the pool to protect against multi-threading
///   issues. If no multi-threading protection is necessary CNoLock can be
///   used.
template <class TObjType,
          class TObjFactory = CObjFactory_New<TObjType> >
class CObjPool
{
public:
    typedef deque<TObjType*>                 TObjectsList;
    /// Synonym to be able to use outside of the pool
    typedef TObjType                         TObjectType;

    /// Create object pool
    ///
    /// @param max_storage_size
    ///   Maximum number of unused objects that can be stored in the pool.
    ///   Given default effectively means unlimited storage.
    CObjPool(size_t max_storage_size = size_t(-1))
        : m_MaxStorage(max_storage_size)
    {}

    /// Create object pool
    ///
    /// @param factory
    ///   Object factory implementing creation/deletion strategy
    /// @param max_storage_size
    ///   Maximum number of unused objects that can be stored in the pool.
    ///   Given default effectively means unlimited storage.
    CObjPool(const TObjFactory& factory,
             size_t             max_storage_size = size_t(-1))
        : m_MaxStorage(max_storage_size),
          m_Factory(factory)
    {}

    /// Destroy object pool and all objects it owns
    ~CObjPool(void)
    {
        Clear();
    }

    /// Get object from the pool, create if necessary
    TObjType* Get(void)
    {
        TObjType* obj = NULL;

        m_ObjLock.Lock();
        if (!m_FreeObjects.empty()) {
            obj = m_FreeObjects.back();
            m_FreeObjects.pop_back();
        }
        m_ObjLock.Unlock();

        if (obj == NULL) {
            obj = m_Factory.CreateObject();
        }

        _ASSERT(obj);
        return obj;
    }

    /// Return object to the pool for future use
    void Return(TObjType* obj)
    {
        _ASSERT(obj);

        m_ObjLock.Lock();
        if (m_FreeObjects.size() < m_MaxStorage) {
            m_FreeObjects.push_back(obj);
            obj = NULL;
        }
        m_ObjLock.Unlock();

        if (obj != NULL) {
            m_Factory.DeleteObject(obj);
        }
    }

    /// Delete all objects returned to the pool so far and clean it
    void Clear(void)
    {
        TObjectsList free_objects;

        m_ObjLock.Lock();
        m_FreeObjects.swap(free_objects);
        m_ObjLock.Unlock();

        ITERATE(typename TObjectsList, it, free_objects)
        {
            m_Factory.DeleteObject(*it);
        }
    }

    /// Get maximum number of unused objects that can be stored in the pool.
    /// 0 means unlimited storage.
    size_t GetMaxStorageSize(void) const
    {
        return m_MaxStorage;
    }

    /// Set maximum number of unused objects that can be stored in the pool.
    /// 0 means unlimited storage.
    void SetMaxStorageSize(size_t max_storage_size)
    {
        // Writing of size_t is always an atomic operation
        m_MaxStorage = max_storage_size;
    }

private:
    CObjPool(const CObjPool&);
    CObjPool& operator= (const CObjPool&);

    /// Maximum number of unused objects that can be stored in the pool
    size_t              m_MaxStorage;
    /// Object factory
    TObjFactory         m_Factory;
    /// Lock object to change the pool
    CSpinLock           m_ObjLock;
    /// List of unused objects
    TObjectsList        m_FreeObjects;
};


/// Guard that can be used to automatically return object to the pool after
/// leaving some scope. Guard can also be used to automatically acquire object
/// from pool and work after that as a smart pointer automatically converting
/// himself to the pointer to protected object. Guard can be used like this:
/// {{
///     TObjPoolGuard obj(pool);
///     obj->DoSomething();
///     FuncAcceptingPointerToObject(obj);
/// }}
///
/// @param TObjPool
///   Type of the pool which this guard works with
///
/// @sa CObjPool
template <class TObjPool>
class CObjPoolGuard
{
public:
    /// Type of object to protect
    typedef typename TObjPool::TObjectType  TObjType;

    /// Create guard and automatically acquire object from the pool
    CObjPoolGuard(TObjPool& pool)
        : m_Pool(pool),
          m_Object(pool.Get())
    {}

    /// Create guard to automatically return given object to the pool.
    ///
    /// @param pool
    ///   Pool to return object to
    /// @param object
    ///   Object to protect. Parameter can be NULL, in this case no object is
    ///   acquired and protected on construction, but can be passed to the
    ///   guard later via Acquire().
    CObjPoolGuard(TObjPool& pool, TObjType* object)
        : m_Pool(pool),
          m_Object(object)
    {}

    ~CObjPoolGuard(void)
    {
        Return();
    }

    /// Get pointer to protected object
    TObjType* GetObject(void) const
    {
        return m_Object;
    }

    // Operators implementing smart-pointer-type conversions

    /// Automatic conversion to the pointer to protected object
    operator TObjType*   (void) const
    {
        return  GetObject();
    }
    /// Automatic dereference to the protected object
    TObjType& operator*  (void) const
    {
        _ASSERT(m_Object);
        return *GetObject();
    }
    /// Automatic dereference to the protected object
    TObjType* operator-> (void) const
    {
        _ASSERT(m_Object);
        return  GetObject();
    }

    /// Return protected object (if any) to the pool and acquire new object
    /// for protection. If parameter is NULL then get object from the pool.
    void Acquire(TObjType* object = NULL)
    {
        Return();
        if (object) {
            m_Object = object;
        }
        else {
            m_Object = m_Pool.Get();
        }
    }

    /// Release protected object without returning it to the pool. After
    /// calling to this method it is caller responsibility to return object
    /// to the pool.
    ///
    /// @return
    ///   Object that was protected
    TObjType* Release(void)
    {
        TObjType* object = m_Object;
        m_Object = NULL;
        return object;
    }

    /// Return protected object (if any) to the pool
    void Return(void)
    {
        if (m_Object) {
            m_Pool.Return(m_Object);
            m_Object = NULL;
        }
    }

private:
    CObjPoolGuard(const CObjPoolGuard&);
    CObjPoolGuard& operator= (const CObjPoolGuard&);

    /// Pool this guard is attached to
    TObjPool& m_Pool;
    /// Protected object
    TObjType* m_Object;
};


//////////////////////////////////////////////////////////////////////////
// Set of most frequently used object factories that can be used with
// CObjPool.
//////////////////////////////////////////////////////////////////////////

/// Object factory for simple creation and deletion of the object
template <class TObjType>
class CObjFactory_New
{
public:
    TObjType* CreateObject(void)
    {
        return new TObjType();
    }

    void DeleteObject(TObjType* obj)
    {
        delete obj;
    }
};

/// Object factory for simple creation and deletion of the object with one
/// parameter passed to object's constructor.
template <class TObjType, class TParamType>
class CObjFactory_NewParam
{
public:
    /// @param param
    ///   Parameter value that will be passed to constructor of every new
    ///   object.
    CObjFactory_NewParam(const TParamType& param)
        : m_Param(param)
    {}

    TObjType* CreateObject(void)
    {
        return new TObjType(m_Param);
    }

    void DeleteObject(TObjType* obj)
    {
        delete obj;
    }

private:
    /// Parameter value that will be passed to constructor of every new object
    TParamType m_Param;
};

/// Object factory for creation implemented by method of some class and
/// simple deletion.
template <class TObjType, class TMethodClass>
class CObjFactory_NewMethod
{
public:
    typedef TObjType* (TMethodClass::*TMethod)(void);

    /// @param method_obj
    ///   Object which method will be called to create new object
    /// @param method
    ///   Method to call to create new object
    CObjFactory_NewMethod(TMethodClass* method_obj,
                          TMethod       method)
        : m_MethodObj(method_obj),
          m_Method(method)
    {}

    TObjType* CreateObject(void)
    {
        return (m_MethodObj->*m_Method)();
    }

    void DeleteObject(TObjType* obj)
    {
        delete obj;
    }

private:
    /// Object which method will be called to create new object
    TMethodClass* m_MethodObj;
    /// Method to call to create new object
    TMethod       m_Method;
};


/* @} */


END_NCBI_SCOPE


#endif  /* CORELIB___OBJ_POOL__HPP */
