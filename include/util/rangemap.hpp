#ifndef RANGEMAP__HPP
#define RANGEMAP__HPP

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
*   Generic map with rang as key
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.3  2001/01/03 16:39:18  vasilche
* Added CAbstractObjectManager - stub for object manager.
* CRange extracted to separate file.
*
* Revision 1.2  2000/12/26 17:27:42  vasilche
* Implemented CRangeMap<> template for sorting Seq-loc objects.
*
* Revision 1.1  2000/12/21 21:52:41  vasilche
* Added CRangeMap<> template for sorting integral ranges (Seq-loc).
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <objects/objmgr/range.hpp>
#include <map>

BEGIN_NCBI_SCOPE

// forward template declarations
template<typename Mapped, typename Position> class CRangeMap;
template<typename Mapped, typename Position,
    typename LevelIter, typename SelectIter> class CRangeMapIterator;

// iterator
template<typename Mapped, typename Position,
    typename LevelIter, typename SelectIter>
class CRangeMapIterator
{
public:
    // typedefs
    typedef Position position_type;
    typedef CRange<position_type> range_type;
    typedef Mapped mapped_type;
    typedef typename LevelIter::value_type value_type;
    typedef typename LevelIter::reference reference;
    typedef typename LevelIter::pointer pointer;

    // internal typedefs
    typedef map<range_type, mapped_type> TNCLevelType;
    typedef map<position_type, TNCLevelType> TNCSelectType;
    typedef LevelIter TLevelIter;
    typedef typename TNCLevelType::iterator TNCLevelIter;
    typedef SelectIter TSelectIter;
    typedef typename TNCSelectType::iterator TNCSelectIter;
    typedef CRangeMapIterator<mapped_type, position_type,
        TLevelIter, TSelectIter> TThisType;
    typedef CRangeMapIterator<mapped_type, position_type,
        TNCLevelIter, TNCSelectIter> TNCThisType;

    // constructors
    // singular
    CRangeMapIterator(void)
        {
        }
    // begin(range)
    CRangeMapIterator(TSelectIter selectBegin, TSelectIter selectEnd,
                      range_type range)
        : m_Range(range),
          m_SelectIter(selectBegin), m_SelectIterEnd(selectEnd)
        {
            if ( !Finished() ) {
                InitLevelIter();
                Settle();
            }
        }
    // begin()
    CRangeMapIterator(TSelectIter selectBegin, TSelectIter selectEnd)
        : m_Range(range_type::GetWhole()),
          m_SelectIter(selectBegin), m_SelectIterEnd(selectEnd)
        {
            if ( !Finished() ) {
                m_LevelIter = m_SelectIter->second.begin();
                _ASSERT(m_LevelIter != m_SelectIter->second.end());
            }
        }
    // find(key)
    CRangeMapIterator(TSelectIter selectIter, TSelectIter selectEnd,
                      position_type selectKey, range_type key)
        : m_Range(range_type::GetWhole()),
          m_SelectIter(selectIter), m_SelectIterEnd(selectEnd)
        {
            if ( !Finished() ) {
                m_LevelIter = m_SelectIter->second.find(key);
                if ( m_LevelIter == m_SelectIter->second.end() ) {
                    // not found: reset
                    m_SelectIter = m_SelectIterEnd;
                }
            }
        }
    // insert()
    CRangeMapIterator(TSelectIter selectIter, TSelectIter selectEnd,
                      TLevelIter levelIter)
        : m_Range(range_type::GetWhole()),
          m_SelectIter(selectIter), m_SelectIterEnd(selectEnd),
          m_LevelIter(levelIter)
        {
            _ASSERT(levelIter != m_SelectIter->second.end());
        }
    // end()
    CRangeMapIterator(TSelectIter selectEnd)
        : m_Range(0, 0),
          m_SelectIter(selectEnd), m_SelectIterEnd(selectEnd)
        {
        }

    // copy: non const -> const
    CRangeMapIterator(const TNCThisType& iter)
        : m_Range(iter.GetRange()),
          m_SelectIter(iter.GetSelectIter()),
          m_SelectIterEnd(iter.GetSelectIterEnd()),
          m_LevelIter(iter.GetLevelIter())
        {
        }

    // assignment: non const -> const
    TThisType& operator=(const TNCThisType& iter)
        {
            m_Range = iter.GetRange();
            m_SelectIter = iter.GetSelectIter();
            m_SelectIterEnd = iter.GetSelectIterEnd();
            m_LevelIter = iter.GetLevelIter();
            return *this;
        }

    // get parameters
    const range_type& GetRange(void) const
        {
            return m_Range;
        }
    // get state (for copying)
    TSelectIter GetSelectIter(void) const
        {
            return m_SelectIter;
        }
    TSelectIter GetSelectIterEnd(void) const
        {
            return m_SelectIterEnd;
        }
    TLevelIter GetLevelIter(void) const
        {
            return m_LevelIter;
        }

    // check state
    bool operator==(const TThisType& iter) const
        {
            _ASSERT(m_SelectIterEnd == iter.GetSelectIterEnd());
            return m_SelectIter == iter.GetSelectIter() &&
                (Finished() || m_LevelIter == iter.GetLevelIter());
        }
    bool operator!=(const TThisType& iter) const
        {
            return !(*this == iter);
        }

    // move
    TThisType& operator++(void)
        {
            ++m_LevelIter;
            Settle();
            return *this;
        }

    // dereference
    reference operator*(void) const
        {
            return *m_LevelIter;
        }
    pointer operator->(void) const
        {
            return &*m_LevelIter;
        }

private:
    bool Finished(void) const
        {
            return m_SelectIter == m_SelectIterEnd;
        }
    void InitLevelIter(void)
        {
            position_type from = m_Range.GetFrom();
            if ( from == range_type::GetWholeFrom() ) {
                // special case: whole region
                m_LevelIter = m_SelectIter->second.begin();
            }
            else {
                // get maximum length of ranges in the level
                position_type maxLength = m_SelectIter->first;
                // starting level point
                m_LevelIter = m_SelectIter->second.
                    lower_bound(range_type(from - maxLength + 1, maxLength));
            }
        }
    // stop at suitable point
    void Settle(void)
        {
            for ( ;; ) {
                // scan level
                while ( m_LevelIter != m_SelectIter->second.end() &&
                        m_LevelIter->first.GetFrom() <= m_Range.GetTo() ) {
                    // we need to check only right bound of current range
                    if ( m_LevelIter->first.GetTo() >= m_Range.GetFrom() )
                        return; // found intersecting range
                    ++m_LevelIter;
                }
                // next level
                ++m_SelectIter;
                if ( Finished() )
                    return; // no more levels
                InitLevelIter();
            }
        }
private:
    // iterator data
    range_type m_Range;          // range to search
    TSelectIter m_SelectIter;    // first level iter
    TSelectIter m_SelectIterEnd; // first level iter limit
    TLevelIter m_LevelIter;      // second level iter
};

template<typename Mapped, typename Position = int>
class CRangeMap
{
public:
    // standard typedefs
    typedef Position position_type; // -1 means unlimited
    typedef CRange<position_type> range_type;
    typedef range_type key_type;
    typedef Mapped mapped_type;
    typedef pair<const key_type, mapped_type> value_type;

    // internal typedefs
    typedef map<key_type, mapped_type> TLevelMap;
    typedef typename TLevelMap::iterator TLevelMapI;
    typedef typename TLevelMap::const_iterator TLevelMapCI;
    typedef map<position_type, TLevelMap> TSelectMap;
    typedef typename TSelectMap::iterator TSelectMapI;
    typedef typename TSelectMap::const_iterator TSelectMapCI;
    typedef CRangeMap<Mapped, Position> TThisType;
    
    // iterators
    typedef CRangeMapIterator<mapped_type, position_type,
        TLevelMapI, TSelectMapI> iterator;
    typedef CRangeMapIterator<mapped_type, position_type,
        TLevelMapCI, TSelectMapCI> const_iterator;

    // constructor
    explicit CRangeMap(void)
        : m_ElementCount(0)
        {
        }
    ~CRangeMap(void)
        {
        }

    // capacity
    bool empty(void) const
        {
            return m_ElementCount == 0;
        }
    size_t size(void) const
        {
            return m_ElementCount;
        }
    
    // iterators
    const_iterator begin(range_type range) const
        {
            return const_iterator(m_SelectMap.begin(), m_SelectMap.end(),
                                  range);
        }
    const_iterator begin(void) const
        {
            return const_iterator(m_SelectMap.begin(), m_SelectMap.end());
        }
    const_iterator end(void) const
        {
            return const_iterator(m_SelectMap.end());
        }
    iterator begin(range_type range)
        {
            return iterator(m_SelectMap.begin(), m_SelectMap.end(), range);
        }
    iterator begin(void)
        {
            return iterator(m_SelectMap.begin(), m_SelectMap.end());
        }
    iterator end(void)
        {
            return iterator(m_SelectMap.end());
        }

    // element search
    const_iterator find(key_type key) const
        {
            if ( key.Empty() )
                return end();
            position_type selectKey = get_max_length(key);
            return const_iterator(m_SelectMap.find(selectKey),
                                  m_SelectMap.end(), selectKey, key);
        }
    iterator find(key_type key)
        {
            if ( key.Empty() )
                return end();
            position_type selectKey = get_max_length(key);
            return iterator(m_SelectMap.find(selectKey),
                            m_SelectMap.end(), get_max_length(key), key);
        }

    // modification
    iterator erase(iterator iter)
        {
            _ASSERT(iter != end());

            // get element position
            TSelectMapI selectIter = iter.GetSelectIter();
            TLevelMapI levelIter = iter.GetLevelIter();
            // element range
            key_type erased_key = levelIter->first;

            // erase element on level
            levelIter = selectIter->second.erase(levelIter);

            // update total count
            --m_ElementCount;

            if ( levelIter == selectIter->second.end() ) {
                // end of level
                if ( selectIter->second.empty() )
                    selectIter = m_SelectMap.erase(selectIter); // remove level
                else
                    ++selectIter; // go to next level

                TSelectMapI selectEnd = m_SelectMap.end();
                if ( selectIter == selectEnd )
                    return iterator(selectEnd); // no more levels
                else
                    return iterator(selectIter, selectEnd,
                                    selectIter->second.begin());
            }
            return iterator(m_SelectMap, selectIter, levelIter);
        }
    size_t erase(key_type key)
        {
            iterator iter = find(key);
            if ( iter == end() )
                return 0;
            erase(iter);
            return 1;
        }
    void clear(void)
        {
            m_SelectMap.clear();
            m_ElementCount = 0;
        }

    pair<iterator, bool> insert(const value_type& value)
        {
            if ( value.first.Empty() )
                THROW1_TRACE(runtime_error, "empty key range");
            position_type selectKey = get_max_length(value.first);
            typedef typename TSelectMap::value_type select_value;
            pair<TSelectMapI, bool> selectInsert =
                m_SelectMap.insert(select_value(selectKey, TLevelMap()));
            pair<TLevelMapI, bool> levelInsert =
                selectInsert.first->second.insert(value);

            if ( levelInsert.second )
                ++m_ElementCount; // new element -> update count

            return make_pair(iterator(selectInsert.first, m_SelectMap.end(),
                                      levelInsert.first),
                             levelInsert.second);
        }

    // element access
    mapped_type& operator[](key_type key)
        {
            return insert(value_type(key, mapped_type())).first->second;
        }
    
private:
    // private methods
    static position_type high_bit_of(position_type value)
        {
            position_type high_bit = 1;
            while ( (value >>= 1) != 0 ) {
                high_bit <<= 1;
            }
            return high_bit;
        }

    static position_type get_max_length(key_type key)
        {
            _ASSERT(!key.Empty());
            if ( key.HaveInfiniteBound() )
                return key.GetWholeLength();
            position_type len = key.GetLength() - 1;
            len |= (len >> 16);
            len |= (len >> 8);
            len |= (len >> 4);
            len |= (len >> 2);
            len |= (len >> 1);
            return len + 1;
        }

private:
    // data
    TSelectMap m_SelectMap;
    size_t m_ElementCount; // count of elements
};

#include <objects/objmgr/rangemap.inl>

END_NCBI_SCOPE

#endif  /* RANGEMAP__HPP */
