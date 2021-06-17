#if defined(WEAKMAP__HPP)  &&  !defined(WEAKMAP__INL)
#define WEAKMAP__INL

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
*   Inline methods for classes defined in weakmap.hpp
*/

template<class Object>
inline
void CWeakMapKey<Object>::Register(TWeakMap* m)
{
    _ASSERT(m_Maps.find(m) == m_Maps.end());
    m_Maps.insert(m);
}

template<class Object>
inline
void CWeakMapKey<Object>::Deregister(TWeakMap* m)
{
    _ASSERT(m_Maps.find(m) != m_Maps.end());
    m_Maps.erase(m);
}

template<class Object>
inline
size_t CWeakMap<Object>::size(void) const
{
    return m_Map.size();
}

template<class Object>
inline
bool CWeakMap<Object>::empty(void) const
{
    return m_Map.empty();
}

template<class Object>
inline
CWeakMap<Object>::CWeakMap(void)
{
}

template<class Object>
inline
CWeakMap<Object>::~CWeakMap(void)
{
    while ( !empty() ) {
        erase(*(m_Map.begin()->first));
    }
}

template<class Object>
inline
void CWeakMap<Object>::insert(key_type& key, const mapped_type& object)
{
    pair<typename TMap::iterator, bool> insert =
        m_Map.insert(TMap::value_type(&key, object));
    if ( insert.second ) {
        key.Register(this);
    }
    else {
        insert.first->second = object;
    }
}

template<class Object>
inline
void CWeakMap<Object>::erase(key_type& key)
{
    typename TMap::iterator mi = m_Map.find(&key);
    if ( mi != m_Map.end() ) {
        m_Map.erase(mi);
        key.Deregister(this);
    }
}

template<class Object>
inline
void CWeakMap<Object>::Forget(key_type& key)
{
    _ASSERT(m_Map.find(&key) != m_Map.end());
    m_Map.erase(&key);
    key.Deregister(this);
}

template<class Object>
inline
typename CWeakMap<Object>::const_iterator
CWeakMap<Object>::find(key_type& key) const
{
    return m_Map.find(&key);
}

template<class Object>
inline
typename CWeakMap<Object>::iterator
CWeakMap<Object>::find(key_type& key)
{
    return m_Map.find(&key);
}

template<class Object>
inline
typename CWeakMap<Object>::const_iterator
CWeakMap<Object>::begin(void) const
{
    return m_Map.begin();
}

template<class Object>
inline
typename CWeakMap<Object>::iterator
CWeakMap<Object>::begin(void)
{
    return m_Map.begin();
}

template<class Object>
inline
typename CWeakMap<Object>::const_iterator
CWeakMap<Object>::end(void) const
{
    return m_Map.end();
}

template<class Object>
inline
typename CWeakMap<Object>::iterator
CWeakMap<Object>::end(void)
{
    return m_Map.end();
}

template<class Object>
inline
CWeakMapKey<Object>::CWeakMapKey(void)
{
}

template<class Object>
inline
CWeakMapKey<Object>::~CWeakMapKey(void)
{
    while ( !m_Maps.empty() ) {
        (*m_Maps.begin())->Forget(*this);
    }
}

#endif /* def WEAKMAP__HPP  &&  ndef WEAKMAP__INL */
