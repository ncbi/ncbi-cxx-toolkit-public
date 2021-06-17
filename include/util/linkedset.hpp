#ifndef LINKEDSET__HPP
#define LINKEDSET__HPP

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
*   !!! PUT YOUR DESCRIPTION HERE !!!
*/

#include <corelib/ncbistd.hpp>
#include <set>


/** @addtogroup LinkedSet
 *
 * @{
 */


BEGIN_NCBI_SCOPE

template<typename Key> struct SLinkedSetValue;
template<typename Key> class CLinkedMultisetBase;

template<typename Key>
struct SLinkedSetValue
{
    typedef Key key_type;
    typedef SLinkedSetValue<Key> value_type;

    SLinkedSetValue(const key_type& key, value_type* next = 0)
        : m_Key(key), m_Next(next)
        {
        }

    const key_type& GetKey(void) const
        {
            return m_Key;
        }
    value_type* GetNext(void)
        {
            return m_Next;
        }
    const value_type* GetNext(void) const
        {
            return m_Next;
        }

    bool operator<(const value_type& value) const
        {
            return GetKey() < value.GetKey();
        }
private:
    friend class CLinkedMultisetBase<key_type>;

    const key_type m_Key;
    value_type* m_Next;
};

#if 0
template<typename Value, typename Compare>
class CLinkedSetBase
{
public:
    struct SValue
    {
        typedef Value value_type;

        Value value;

        SValue* Next(void) const
            {
                return m_Next;
            }
    private:
        friend class CLinkedSetBase<Value, Compare>;

        SValue* m_Next;
    };
    typedef set<Value, Compare> TSet;

    typedef typename TSet::iterator iterator;
    typedef typename TSet::const_iterator const_iterator;
    typedef SValue* TForwardIterator;

    CLinkedSetBase(void)
        : m_Start(0)
        {
        }
    CLinkedSetBase(Compare comp)
        : m_Set(comp), m_Start(0)
        {
        }

    bool empty(void) const
        {
            return m_Start == 0;
        }

    TForwardIterator ForwardBegin(void) const
        {
            return m_Start;
        }
    TForwardIterator ForwardEnd(void) const
        {
            return 0;
        }

    iterator begin(void)
        {
            return m_Set.begin();
        }
    iterator end(void)
        {
            return m_Set.end();
        }
    iterator find(const Value& value)
        {
            return m_Set.find(value);
        }
    iterator lower_bound(const Value& value)
        {
            return m_Set.lower_bound(value);
        }

    const_iterator begin(void) const
        {
            return m_Set.begin();
        }
    const_iterator end(void) const
        {
            return m_Set.end();
        }
    const_iterator find(const Value& value) const
        {
            return m_Set.find(value);
        }
    const_iterator lower_bound(const Value& value) const
        {
            return m_Set.lower_bound(value);
        }

protected:
    void clear(void)
        {
            m_Start = 0;
        }

    void insertAfter(const SValue& prevValue, const SValue& newValue)
        {
            _ASSERT(!newValue.m_Next);
            newValue.m_Next = prevValue.m_Next;
            prevValue.m_Next = &newValue;
        }
    void insertToStart(const SValue& newValue)
        {
            _ASSERT(!newValue.m_Next);
            newValue.m_Next = m_Start;
            m_Start = &newValue;
        }

    void removeAfter(const SValue& prevValue, const SValue& value)
        {
            prevValue.m_Next = value.m_Next;
            
        }
    void removeFromStart(const SValue& value)
        {
            m_Start = value.m_Next;
        }

private:
    TSet m_Set;
    SValue* m_Start;
};

template<typename Mapped>
class CLinkedSet : public CLinkedSetBase<typename Mapped::key_type>
{
    typedef CLinkedSetBase<typename Mapped::key_type> TParent;
public:
    typedef set<Mapped> TContainer;
    typedef typename TContainer::size_type size_type;
    typedef typename TContainer::value_type value_type;
    typedef typename TContainer::iterator iterator;
    typedef typename TContainer::const_iterator const_iterator;
    
    size_type size(void) const
        {
            return m_Container.size();
        }

    void clear(void)
        {
            m_Container.clear();
            TParent::clear();
        }

    const_iterator begin(void) const
        {
            return m_Container.begin();
        }
    const_iterator end(void) const
        {
            return m_Container.end();
        }
    const_iterator find(const value_type& value) const
        {
            return m_Container.find(value);
        }
    const_iterator lower_bound(const value_type& value) const
        {
            return m_Container.lower_bound(value);
        }
    const_iterator upper_bound(const value_type& value) const
        {
            return m_Container.upper_bound(value);
        }

    iterator begin(void)
        {
            return m_Container.begin();
        }
    iterator end(void)
        {
            return m_Container.end();
        }
    iterator find(const value_type& value)
        {
            return m_Container.find(value);
        }
    iterator lower_bound(const value_type& value)
        {
            return m_Container.lower_bound(value);
        }
    iterator upper_bound(const value_type& value)
        {
            return m_Container.upper_bound(value);
        }

    pair<iterator, bool> insert(const value_type& value)
        {
            pair<iterator, bool> ins = m_Container.insert(value);
            if ( ins.second ) {
                if ( ins.first == begin() )
                    this->insertToStart(*ins.first);
                else {
                    iterator prev = ins.first;
                    this->insertAfter(*--prev, *ins.first);
                }
            }
            return ins;
        }

    void erase(iterator iter)
        {
            if ( iter == begin() )
                this->removeFromStart(*iter);
            else {
                iterator prev = iter;
                this->removeAfter(*--prev, *iter);
            }
            m_Container.erase(iter);
        }

private:
    TContainer m_Container;
};
#endif

template<typename Key>
class CLinkedMultisetBase
{
public:
    typedef Key key_type;
    typedef SLinkedSetValue<key_type> value_type;

    CLinkedMultisetBase(void)
        : m_Start(0)
        {
        }

    bool empty(void) const
        {
            return m_Start == 0;
        }
    value_type* GetStart(void)
        {
            return m_Start;
        }
    const value_type* GetStart(void) const
        {
            return m_Start;
        }

protected:
    void clear(void)
        {
            m_Start = 0;
        }

    void insertAfter(value_type& prevValue, value_type& newValue)
        {
            _ASSERT(!newValue.m_Next);
            newValue.m_Next = prevValue.m_Next;
            prevValue.m_Next = &newValue;
        }
    void insertToStart(value_type& newValue)
        {
            _ASSERT(!newValue.m_Next);
            newValue.m_Next = m_Start;
            m_Start = &newValue;
        }

    void removeAfter(value_type& prevValue, value_type& value)
        {
            _ASSERT(prevValue.m_Next == &value);
            prevValue.m_Next = value.m_Next;
            value.m_Next = 0;
        }
    void removeFromStart(value_type& value)
        {
            _ASSERT(m_Start == &value);
            m_Start = value.m_Next;
            value.m_Next = 0;
        }

private:
    value_type* m_Start;
};

template<typename Mapped>
class CLinkedMultiset : public CLinkedMultisetBase<typename Mapped::key_type>
{
    typedef CLinkedMultisetBase<typename Mapped::key_type> TParent;
public:
    typedef multiset<Mapped> TContainer;
    typedef typename TContainer::size_type size_type;
    typedef typename TContainer::value_type value_type;
    typedef typename TContainer::iterator iterator;
    typedef typename TContainer::const_iterator const_iterator;
    
    size_type size(void) const
        {
            return m_Container.size();
        }

    void clear(void)
        {
            m_Container.clear();
            TParent::clear();
        }

    const_iterator begin(void) const
        {
            return m_Container.begin();
        }
    const_iterator end(void) const
        {
            return m_Container.end();
        }
    const_iterator find(const value_type& value) const
        {
            return m_Container.find(value);
        }
    const_iterator lower_bound(const value_type& value) const
        {
            return m_Container.lower_bound(value);
        }
    const_iterator upper_bound(const value_type& value) const
        {
            return m_Container.upper_bound(value);
        }

    iterator begin(void)
        {
            return m_Container.begin();
        }
    iterator end(void)
        {
            return m_Container.end();
        }
    iterator find(const value_type& value)
        {
            return m_Container.find(value);
        }
    iterator lower_bound(const value_type& value)
        {
            return m_Container.lower_bound(value);
        }
    iterator upper_bound(const value_type& value)
        {
            return m_Container.upper_bound(value);
        }

    iterator insert(const value_type& value)
        {
            iterator iter = m_Container.insert(value);
            if ( iter == begin() )
                this->insertToStart(get(iter));
            else {
                iterator prev = iter;
                this->insertAfter(get(--prev), get(iter));
            }
            return iter;
        }

    void erase(iterator iter)
        {
            if ( iter == begin() )
                this->removeFromStart(get(iter));
            else {
                iterator prev = iter;
                this->removeAfter(get(--prev), get(iter));
            }
            m_Container.erase(iter);
        }

    static value_type& get(iterator iter)
        {
            return const_cast<value_type&>(*iter);
        }
    
#if defined(_RWSTD_VER) && !defined(_RWSTD_STRICT_ANSI)
    size_type allocation_size(size_type buffer_size)
        {
            return m_Container.allocation_size(buffer_size);
        }
#endif

private:
    TContainer m_Container;
};


/* @} */


//#include <util/linkedset.inl>

END_NCBI_SCOPE

#endif  /* LINKEDSET__HPP */
