#ifndef CORELIB___OBJPOOL__HPP
#define CORELIB___OBJPOOL__HPP

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


/// Policy of pool's ownership over objects it created.
/// Ownership by pool is not necessarily mean that object will be deleted with
/// the pool. It means only that on the pool destruction (or overflowing of
/// storage) TObjFactory::DeleteObject() will be called. Whether or not object
/// is deleted depends solely on factory implementation.
///
/// @sa CObjPool
enum EObjPool_OwnPolicy
{
    ePoolOwnsUnused,  ///< Own only objects returned back to the pool
    ePoolOwnsAll      ///< Own all objects created by the pool
};

/// General object pool
///
/// @param TObjType
///   Type of objects in the pool (any type, not necessarily inherited from
///   CObject). Pool holds pointers to objects, so they shouldn't be copiable.
/// @param TObjFactory
///   Type of object factory, representing the policy of how objects created
///   and how they deleted.
/// @param TLock
///   Type of the lock used in the pool to protect against multi-threading
///   issues. If no multi-threading protection is necessary CNoLock can be
///   used.
template <class TObjType,
          class TObjFactory = CObjFactory_New<TObjType>,
          class TLock       = CFastMutex>
class CObjPool
{
private:
    typedef typename TLock::TWriteLockGuard  TLockGuard;
    typedef deque<TObjType*>                 TObjectsList;

public:
    /// Create object pool
    ///
    /// @param max_storage_size
    ///   Maximum number of unused objects that can be stored in the pool.
    ///   0 means unlimited storage.
    /// @param policy
    ///   Ownership policy of the pool
    CObjPool(size_t             max_storage_size = 0,
             EObjPool_OwnPolicy policy = ePoolOwnsAll)
        : m_MaxStorage(max_storage_size),
          m_OwnPolicy(policy)
    {}

    /// Create object pool
    ///
    /// @param max_storage_size
    ///   Maximum number of unused objects that can be stored in the pool.
    ///   0 means unlimited storage.
    /// @param policy
    ///   Ownership policy of the pool
    /// @param factory
    ///   Object factory implementing creation/deletion strategy
    CObjPool(size_t             max_storage_size,
             EObjPool_OwnPolicy policy,
             TObjFactory        factory)
        : m_MaxStorage(max_storage_size),
          m_OwnPolicy(policy),
          m_Factory(factory)
    {}

    /// Destroy object pool and all objects it owns
    ~CObjPool(void)
    {
        TLockGuard guard(m_ObjLock);

        TObjectsList& obj_list = (m_OwnPolicy == ePoolOwnsAll? m_AllObjects:
                                                               m_FreeObjects);
        ITERATE(TObjectsList, it, obj_list)
        {
            m_Factory.DeleteObject(*it);
        }
    }

    /// Get object from the pool, create if necessary
    TObjType* Get(void)
    {
        TLockGuard guard(m_ObjLock);

        TObjType* obj;
        if (m_FreeObjects.empty()) {
            obj = m_Factory.CreateObject();
            if (m_OwnPolicy == ePoolOwnsAll) {
                m_AllObjects.push_back(obj);
            }
        }
        else {
            obj = m_FreeObjects.back();
            m_FreeObjects.pop_back();
        }

        return obj;
    }

    /// Return object to the pool for future use
    void Return(TObjType* obj)
    {
        TLockGuard guard(m_ObjLock);

        if (m_FreeObjects.size() >= m_MaxStorage) {
            m_Factory.DeleteObject(obj);
        }
        else {
            m_FreeObjects.push_back(obj);
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
        m_MaxStorage = max_storage_size;
    }

private:
    /// Maximum number of unused objects that can be stored in the pool
    size_t              m_MaxStorage;
    /// Ownership policy
    EObjPool_OwnPolicy  m_OwnPolicy;
    /// Object factory
    TObjFactory         m_Factory;
    /// Lock object to change the pool
    TLock               m_ObjLock;
    /// List of all objects created by the pool
    TObjectsList        m_AllObjects;
    /// List of unused objects
    TObjectsList        m_FreeObjects;
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

/// Object factory for objects driven by reference counting with simple
/// creation.
template <class TObjType>
class CObjFactory_Ref
{
public:
    TObjType* CreateObject(void)
    {
        TObjType* obj = new TObjType();
        obj->AddReference();
        return obj;
    }

    void DeleteObject(TObjType* obj)
    {
        obj->RemoveReference();
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
    CObjFactory_NewParam(TParamType param)
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

/// Object factory for objects driven by reference counting with one
/// parameter passed to object's constructor.
template <class TObjType, class TParamType>
class CObjFactory_RefParam
{
public:
    /// @param param
    ///   Parameter value that will be passed to constructor of every new
    ///   object.
    CObjFactory_RefParam(TParamType param)
        : m_Param(param)
    {}

    TObjType* CreateObject(void)
    {
        TObjType* obj = new TObjType(m_Param);
        obj->AddReference();
        return obj;
    }

    void DeleteObject(TObjType* obj)
    {
        obj->RemoveReference();
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


/// Object factory for creation implemented by method of some class and
/// object lifetime driven by reference counting.
template <class TObjType, class TMethodClass>
class CObjFactory_RefMethod
{
public:
    typedef TObjType* (TMethodClass::*TMethod)(void);

    /// @param method_obj
    ///   Object which method will be called to create new object
    /// @param method
    ///   Method to call to create new object
    CObjFactory_RefMethod(TMethodClass* method_obj,
                          TMethod       method)
        : m_MethodObj(method_obj),
          m_Method(method)
    {}

    TObjType* CreateObject(void)
    {
        TObjType* obj = (m_MethodObj->*m_Method)();
        obj->AddReference();
        return obj;
    }

    void DeleteObject(TObjType* obj)
    {
        obj->RemoveReference();
    }

private:
    /// Object which method will be called to create new object
    TMethodClass* m_MethodObj;
    /// Method to call to create new object
    TMethod       m_Method;
};


/* @} */


END_NCBI_SCOPE


#endif  /* CORELIB___OBJPOOL__HPP */
