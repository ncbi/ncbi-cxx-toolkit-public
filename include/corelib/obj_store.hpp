#ifndef UTIL__OBJECTSTORE__HPP
#define UTIL__OBJECTSTORE__HPP

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
 *
 *
 */
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>
#include <corelib/ncbimtx.hpp>

BEGIN_NCBI_SCOPE


/// Storage container for CObject derived classes
/// Objects are indexed by a key (usually string or integer).
/// Template makes sure objects are destroyed in the order reverse 
/// to the order of insertion (similar to C++ object constr/destr rules).
/// First in last out.
///
template<class TKey, class TObject>
class CReverseObjectStore
{
public:
    typedef TObject*  TObjectPtr;

public:
    ~CReverseObjectStore()
    {
        Clear();
    }

    /// Clear all objects from the store
    void Clear()
    {
        m_ObjMap.clear();
        m_ObjList.erase(m_ObjList.begin(), m_ObjList.end());
    }

    /// Retrieve a named object from the data store. 
    /// (Advised to use CRef<> to host the return value)
    /// Method returns NULL if object cannot be found.
    TObject* GetObject(const TKey& key)
    {
        typename TObjectMap::const_iterator it(m_ObjMap.find(key));
        return (it != m_ObjMap.end()) ? it->second : 0;
    }

    /// Put an object in the store.  This will return TRUE if the
    /// operation succeded, FALSE if the object already exists in the store.
    bool PutObject(const TKey& key, TObject* obj)
    {
        typename TObjectMap::const_iterator it(m_ObjMap.find(key));
        if (it == m_ObjMap.end()) {
            m_ObjList.push_front(CRef<TObject>(obj));
            m_ObjMap.insert(pair<string, TObjectPtr>(key, obj));
            return true;
        }
        return false;
    }

    /// Release an object from the data store
    void ReleaseObject(const TKey& key)
    {
        typename TObjectMap::iterator it(m_ObjMap.find(key));
        if (it != m_ObjMap.end()) {
            TObject* obj = it->second;
            m_ObjMap.erase(it);
            NON_CONST_ITERATE(typename TObjectList, lit, m_ObjList) {
                TObject* ptr = lit->GetPointer();
                if (ptr == obj) {
                    m_ObjList.erase(lit);
                    break;
                }
            }
        }
    }

    /// Check to see if a named object exists
    bool HasObject(const TKey& key)
    {
        return (m_ObjMap.find(key) != m_ObjMap.end());
    }

    /// check to see if a given object is in the store
    bool  HasObject(const CObject* obj)
    {
        ITERATE(typename TObjectList, lit, m_ObjList) {
            const CObject* ptr = lit->GetPointer();
            if (ptr == obj) {
                return true;
            }
        }
        return false;
    }

protected:
    typedef map<TKey, TObjectPtr>     TObjectMap;
    typedef list<CRef<TObject> >      TObjectList;

    TObjectMap     m_ObjMap;  //< String to object locator
    TObjectList    m_ObjList; //< Object storage (objects kept in the reverse order)
};

/// Protected base class for CSingletonObjectStore
/// Holds a syncronization mutex.
///
/// @note 
///   Zero functionality base class is created to make sure
///   that mutex is initialized just only once 
///   (in case of a template member this is not guaranteed)
///
class NCBI_XUTIL_EXPORT CObjectStoreProtectedBase
{
protected:
    static CFastMutex& GetMutex(void); 
};

/// System wide dumping ground for objects
///
/// @note
///   Thread safe, synctonized singleton
///
template<class TKey, class TObject>
class CSingletonObjectStore : protected CObjectStoreProtectedBase
{
public:
    typedef CReverseObjectStore<TKey, TObject> TReverseObjectStore;
public:
    ~CSingletonObjectStore(void)
    {
        Clear();
    }

    /// Clear all objects from the store
    static 
    void Clear() 
    {
        CFastMutexGuard guard( GetMutex() );
        GetObjStore().Clear();
    }

    /// Retrieve a named object from the data store. 
    /// (Advised to use CRef<> to host the return value)
    /// Method returns NULL if object cannot be found.
    static 
    TObject* GetObject(const TKey& key)
    {
        CFastMutexGuard guard( GetMutex() );
        return GetObjStore().GetObject(key);
    }

    /// Put an object in the store.  This will return TRUE if the
    /// operation succeded, FALSE if the object already exists in the store.
    static
    bool PutObject(const TKey& key, TObject* obj)
    {
        CFastMutexGuard guard( GetMutex() );
        return GetObjStore().PutObject(key, obj);
    }

    /// Release an object from the data store
    static
    void ReleaseObject(const TKey& key)
    {
        CFastMutexGuard guard( GetMutex() );
        GetObjStore().ReleaseObject(key);
    }

    /// Check to see if a named object exists
    static
    bool HasObject(const TKey& key)
    {
        CFastMutexGuard guard( GetMutex() );
        return GetObjStore().HasObject(key);
    }

    /// check to see if a given object is in the store
    static
    bool  HasObject(const CObject* obj)
    {
        CFastMutexGuard guard( GetMutex() );
        return GetObjStore().HasObject(obj);
    }

protected:
    static
    TReverseObjectStore& GetObjStore(void)
    {
        static TReverseObjectStore s_obj_store;
        return s_obj_store;
    }
};


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.6  2005/03/07 14:39:31  ssikorsk
 * Replaced static member m_Lock with a static function GetMutex
 *
 * Revision 1.5  2004/12/23 18:08:25  vasilche
 * Fixed use of static variables in template class.
 *
 * Revision 1.4  2004/08/02 16:35:16  kuznets
 * Added missing static
 *
 * Revision 1.3  2004/08/02 14:12:51  kuznets
 * Cosmetics..
 *
 * Revision 1.2  2004/08/02 14:11:02  kuznets
 * GCC compilation fix (proper use of typename)
 *
 * Revision 1.1  2004/08/02 13:43:46  kuznets
 * Initial revision
 *
 *
 * ==========================================================================
 */

#endif 
