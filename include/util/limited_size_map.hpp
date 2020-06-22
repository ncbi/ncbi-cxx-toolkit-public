#ifndef UTIL__LIMITED_SIZE_MAP__HPP
#define UTIL__LIMITED_SIZE_MAP__HPP

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


/// @file limited_size_map.hpp
/// Generic map with size limited by some number


#include <corelib/ncbistd.hpp>
#include <set>
#include <list>


/** @addtogroup Cache
 *
 * @{
 */

BEGIN_NCBI_SCOPE


template<class Key, class Value, class Less = less<Key> >
class limited_size_map
{
public:
    typedef Key key_type;
    typedef Value mapped_type;
    typedef pair<const key_type, mapped_type> value_type;

protected:
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
    };
    struct SLess {
        TLess m_KeyComp;
        SLess()
            {
            }
        SLess(const TLess& key_comp)
            : m_KeyComp(key_comp)
            {
            }
        bool operator()(const SNode& a, const SNode& b) const {
            return m_KeyComp(a.first, b.first);
        }
    };
    TMap m_Map;

public:
    class iterator {
    protected:
        TMapIterator m_Iter;
        friend class limited_size_map<key_type, mapped_type, TLess>;
    public:
        iterator() {}
        explicit iterator(TMapIterator iter) : m_Iter(iter) {}
        value_type& operator*() const { return const_cast<SNode&>(*m_Iter); }
        value_type* operator->() const { return &const_cast<SNode&>(*m_Iter); }
        iterator& operator++() {
            ++m_Iter;
            return *this;
        }
        iterator operator++(int) {
            iterator ret = *this;
            ++m_Iter;
            return ret;
        }
        iterator& operator--() {
            --m_Iter;
            return *this;
        }
        iterator operator--(int) {
            iterator ret = *this;
            --m_Iter;
            return ret;
        }
        bool operator==(iterator a) const {
            return m_Iter == a.m_Iter;
        }
        bool operator!=(iterator a) const {
            return !(*this == a);
        }
    };
    class const_iterator {
    protected:
        TMapConstIterator m_Iter;
        friend class limited_size_map<key_type, mapped_type, TLess>;
    public:
        const_iterator() {}
        explicit const_iterator(TMapConstIterator iter) : m_Iter(iter) {}
        const_iterator(iterator iter) : m_Iter(iter.m_Iter) {}
        const value_type* operator->() const { return m_Iter.operator->(); }
        const value_type& operator*() const { return m_Iter.operator*(); }
        const_iterator& operator++() {
            ++m_Iter;
            return *this;
        }
        const_iterator operator++(int) {
            const_iterator ret = *this;
            ++m_Iter;
            return ret;
        }
        const_iterator& operator--() {
            --m_Iter;
            return *this;
        }
        const_iterator operator--(int) {
            const_iterator ret = *this;
            --m_Iter;
            return ret;
        }
        bool operator==(const_iterator a) const {
            return m_Iter == a.m_Iter;
        }
        bool operator!=(const_iterator a) const {
            return !(*this == a);
        }
    };

    explicit
    limited_size_map(size_t size_limit = 0)
        : m_SizeLimit(size_limit)
        {
        }

    size_t get_size_limit(void) const {
        return m_SizeLimit;
    }
    void set_size_limit(size_t size_limit) {
        m_SizeLimit = size_limit;
        x_GC();
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
    }

    iterator begin(void) {
        return iterator(m_Map.begin());
    }
    iterator end(void) {
        return iterator(m_Map.end());
    }
    iterator find(const key_type& key) {
        TMapIterator iter = m_Map.find(key);
        if ( iter != m_Map.end() ) {
            x_MarkUsed(iter);
        }
        return iterator(iter);
    }
    iterator lower_bound(const key_type& key) {
        TMapIterator iter = m_Map.lower_bound(key);
        if ( iter != m_Map.end() && !m_Map.key_comp()(key, iter->first) ) {
            x_MarkUsed(iter);
        }
        return iterator(iter);
    }
    iterator upper_bound(const key_type& key) {
        return iterator(m_Map.upper_bound(key));
    }
    const_iterator begin(void) const {
        return const_iterator(m_Map.begin());
    }
    const_iterator end(void) const {
        return const_iterator(m_Map.end());
    }
    const_iterator find(const key_type& key) const {
        return const_iterator(m_Map.find(key));
    }
    const_iterator lower_bound(const key_type& key) const {
        return const_iterator(m_Map.lower_bound(key));
    }
    const_iterator upper_bound(const key_type& key) const {
        return const_iterator(m_Map.upper_bound(key));
    }
    
    pair<iterator, bool> insert(const value_type& value) {
        pair<TMapIterator, bool> ins = m_Map.insert(SNode(value));
        if ( ins.second ) {
            x_MarkAdded(ins.first);
        }
        else {
            x_MarkUsed(ins.first);
        }
        return pair<iterator, bool>(iterator(ins.first), ins.second);
    }
    mapped_type& operator[](const key_type& key) {
        TMapIterator iter = m_Map.lower_bound(SNode(key));
        if ( iter == m_Map.end() || m_Map.key_comp()(key, iter->first) ) {
            // insert
            iter = m_Map.insert(iter, SNode(key, mapped_type()));
            x_MarkAdded(iter);
        }
        else {
            x_MarkUsed(iter);
        }
        // the underlying type is set<> so the value_type is unnecessary const
        return x_GetNode(iter).second;
    }
    void erase(iterator iter) {
        m_RemoveList.erase(iter.m_Iter->m_RemoveListIter);
        m_Map.erase(iter.m_Iter);
    }
    
protected:
    SNode& x_GetNode(TMapIterator iter) {
        return const_cast<SNode&>(*iter);
    }
    void x_GC(void) {
        if ( m_SizeLimit ) {
            while ( size() > m_SizeLimit ) {
                m_Map.erase(m_RemoveList.front());
                m_RemoveList.erase(m_RemoveList.begin());
            }
        }
    }
    void x_MarkAdded(TMapIterator iter) {
        x_GetNode(iter).m_RemoveListIter =
            m_RemoveList.insert(m_RemoveList.end(), iter);
        x_GC();
    }
    void x_MarkUsed(TMapIterator iter) {
        m_RemoveList.splice(m_RemoveList.end(), m_RemoveList,
                            iter->m_RemoveListIter);
    }

private:
    size_t m_SizeLimit;
    TRemoveList m_RemoveList;
};


END_NCBI_SCOPE


/* @} */

#endif  /* UTIL__LIMITED_SIZE_MAP__HPP */
