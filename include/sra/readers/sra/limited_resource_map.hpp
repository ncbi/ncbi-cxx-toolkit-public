#ifndef SRA__READER__SRA__LIMITED_RESOURCE_MAP__HPP
#define SRA__READER__SRA__LIMITED_RESOURCE_MAP__HPP

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
 */


/// @file limited_resource_map.hpp
/// Generic map with additional resource limited by some value


#include <corelib/ncbistd.hpp>
#include <set>
#include <list>


BEGIN_NCBI_SCOPE


template<class Key, class Value, class Resource, class Less = less<Key>>
class limited_resource_map
{
public:
    typedef Key key_type;
    typedef Value mapped_type;
    typedef pair<const key_type, mapped_type> value_type;
    typedef Resource resource_type;
        
private:
    struct SNode;
    struct SLess;
    typedef Less TLess;
    typedef set<SNode, SLess> TMap;
    typedef typename TMap::iterator TMapIterator;
    typedef typename TMap::const_iterator TMapConstIterator;
    typedef list<TMapIterator> TRemoveList;
    typedef typename TRemoveList::iterator TRemoveListIterator;
    struct SNode : public value_type {
        SNode(const value_type& value)
            : value_type(value)
            {
            }
        SNode(const key_type& key, const mapped_type& value)
            : value_type(key, value)
            {
            }
        SNode(const key_type& key)
            : value_type(key, mapped_type())
            {
            }
        TRemoveListIterator m_RemoveListIter;
        resource_type m_ResourceUsed;
    };
    struct SLess : TLess {
        SLess()
            {
            }
        SLess(const TLess& key_comp)
            : TLess(key_comp)
            {
            }
        bool operator()(const SNode& a, const SNode& b) const {
            return TLess::operator()(a.first, b.first);
        }
    };
    TMap m_Map;
public:
    
    explicit
    limited_resource_map(const resource_type& resource_limit)
        : m_ResourceLimit(resource_limit),
          m_ResourceUsed()
        {
        }
        
    resource_type get_resource_limit(void) const {
        return m_ResourceLimit;
    }
    void set_resource_limit(resource_type resource_limit) {
        m_ResourceLimit = resource_limit;
        x_GC();
    }
    resource_type resource_used() const {
        return m_ResourceUsed;
    }

    bool empty(void) const {
        return m_Map.empty();
    }
    size_t size(void) const {
        return m_Map.size();
    }
    void clear(void) {
        m_RemoveList.clear();
        m_Map.clear();
        m_ResourceUsed = resource_type();
    }

    mapped_type get(const key_type& key) {
        TMapIterator iter = m_Map.find(key);
        if ( iter != m_Map.end() ) {
            x_MarkUsed(iter);
            return iter->second;
        }
        else {
            return mapped_type();
        }
    }
    void put(const key_type& key, const mapped_type& value, const resource_type& resource_used) {
        pair<TMapIterator, bool> ins = m_Map.insert(SNode(key, value));
        if ( ins.second ) {
            x_MarkAdded(ins.first);
        }
        else {
            x_MarkUsed(ins.first);
            m_ResourceUsed -= ins.first->m_ResourceUsed;
        }
        const_cast<resource_type&>(ins.first->m_ResourceUsed) = resource_used;
        m_ResourceUsed += ins.first->m_ResourceUsed;
        x_GC();
    }
    void erase(const key_type& key) {
        TMapIterator iter = m_Map.find(key);
        if ( iter != m_Map.end() ) {
            x_Erase(iter);
        }
    }

protected:
    SNode& x_GetNode(TMapIterator iter) {
        return const_cast<SNode&>(*iter);
    }
    void x_GC(void) {
        while ( resource_used() > get_resource_limit() ) {
            TRemoveListIterator remove_iter = m_RemoveList.begin();
            if ( remove_iter == m_RemoveList.end() ) {
                // empty
                return;
            }
            if ( next(remove_iter) == m_RemoveList.end() ) {
                // don't remove last element
                return;
            }
            x_Erase(*remove_iter);
        }
    }
    void x_Erase(TMapIterator iter) {
        m_ResourceUsed -= iter->m_ResourceUsed;
        m_RemoveList.erase(iter->m_RemoveListIter);
        m_Map.erase(iter);
    }
    void x_MarkAdded(TMapIterator iter) {
        x_GetNode(iter).m_RemoveListIter =
            m_RemoveList.insert(m_RemoveList.end(), iter);
    }
    void x_MarkUsed(TMapIterator iter) {
        m_RemoveList.splice(m_RemoveList.end(), m_RemoveList,
                            iter->m_RemoveListIter);
    }

private:
    resource_type m_ResourceLimit;
    resource_type m_ResourceUsed;
    TRemoveList m_RemoveList;
};


END_NCBI_SCOPE


#endif  /* SRA__READER__SRA__LIMITED_RESOURCE_MAP__HPP */
