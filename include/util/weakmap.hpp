#ifndef WEAKMAP__HPP
#define WEAKMAP__HPP

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
*   CWeakMap<Object> template work like STL map<> with the only difference:
*       it automatically forgets entries with key which are deleted.
*       To do this, key type is fixed - CWeakMapKey<Object>.
*   CWeakMap<Object> defines mostly used methods from map<>.
*/

#include <corelib/ncbistd.hpp>
#include <map>
#include <set>


#if defined __GNUG__
# warning This file is deprecated and will be removed soon.
#endif

/** @addtogroup WeakMap
 *
 * @{
 */


BEGIN_NCBI_SCOPE


/*
//   Generic example of usage of these templates:
class NCBI_XUTIL_EXPORT CKey
{
public:
    CWeakMapKey<string> m_MapKey;
};
void Test(void)
{
    // declare map object
    CWeakMap<string> map;
    {
        // declare temporary key object
        CWeakMapKey<string> key;
        // insert string value
        map.insert(key, "value");
        cout << map.size(); // == 1
        cout << map.empty(); // == false
    } // end of block: key object is destructed and map forgets about value
    cout << map.size(); // == 0
    cout << map.empty(); // == true
};
*/

template<class Object> class CWeakMap;
template<class Object> class CWeakMapKey;

template<class Object>
class CWeakMapKey
{
public:
    typedef Object mapped_type;
    typedef CWeakMap<mapped_type> TWeakMap;

    CWeakMapKey(void);
    ~CWeakMapKey(void);

private:
    friend class CWeakMap<mapped_type>;

    void Register(TWeakMap* map);
    void Deregister(TWeakMap* map);

    typedef set<TWeakMap*> TMapSet;
    TMapSet m_Maps;
};

template<class Object>
class CWeakMap
{
public:
    typedef Object mapped_type;
    typedef CWeakMapKey<mapped_type> key_type;
    typedef map<key_type*, mapped_type> TMap;
    typedef typename TMap::value_type value_type;
    typedef typename TMap::const_iterator const_iterator;
    typedef typename TMap::iterator iterator;

public:
    NCBI_DEPRECATED_CTOR(CWeakMap(void));
    ~CWeakMap(void);

    size_t size(void) const;
    bool empty(void) const;

    void insert(key_type& key, const mapped_type& object);
    void erase(key_type& key);

    const_iterator find(key_type& key) const;
    iterator find(key_type& key);

    const_iterator begin(void) const;
    iterator begin(void);

    const_iterator end(void) const;
    iterator end(void);

private:
    friend class CWeakMapKey<mapped_type>;

    void Forget(key_type& key);

    TMap m_Map;
};


/* @} */


#include <util/weakmap.inl>

END_NCBI_SCOPE

#endif  /* WEAKMAP__HPP */
