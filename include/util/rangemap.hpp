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
* Revision 1.1  2000/12/21 21:52:41  vasilche
* Added CRangeMap<> template for sorting integral ranges (Seq-loc).
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <map>

BEGIN_NCBI_SCOPE

// compare function
template<class Position>
struct SRangeLess
{
    typedef Position position_type;
    typedef pair<position_type, position_type> range_type;
    
    // r1 is less then r2 (for ordering)
    bool operator()(range_type r1, range_type r2) const
        {
            return r1.first < r2.first ||
                r1.first == r2.first && r1.second < r2.second;
        }
    
    // normalized ranges r1 & r2 intersect
    static bool intersect(range_type r1, range_type r2)
        {
            if ( r1.first <= r2.first ) {
                return r1.second >= r2.first;
            }
            else {
                return r1.first <= r2.second;
            }
        }
    // any ranges r1 & r2 intersect
    static bool intersect_long(range_type r1, range_type r2)
        {
            if ( r1.first <= r2.first ) {
                return r1.second >= r2.first && r2.second >= r2.first;
            }
            else {
                return r1.first <= r2.second && r1.first <= r1.second;
            }
        }
};

// iterators
template<typename Mapped, typename Position,
    typename ValueType, typename Level, typename Select,
    typename LevelIter, typename SelectIter> class CRangeMapIterator;

#if 1
template<typename Num> class numeric_limits;

template<>
class numeric_limits<int>
{
public:
    static int max() { return INT_MAX; }
};
#endif

template<typename Mapped, typename Position,
    typename ValueType = pair<const pair<Position, Position>, Mapped>,
    class Level = map<pair<Position, Position>, Mapped, SRangeLess<Position> >,
    class Select = map<Position, Level>,
    typename LevelIter = typename Level::iterator,
    typename SelectIter = typename Select::iterator>
class CRangeMapIterator
{
public:
    // typedefs
    typedef Position position_type;
    typedef pair<position_type, position_type> range_type;
    typedef range_type key_type;
    typedef Mapped mapped_type;
    typedef pair<const key_type, mapped_type> value_type;

    // internal typedefs
    typedef Level TLevel;
    typedef Select TSelect;
    typedef LevelIter TLevelIter;
    typedef SelectIter TSelectIter;
    typedef CRangeMapIterator<Mapped, Position,
        ValueType, Level, Select, LevelIter, SelectIter> TThisType;

    static range_type max_range(void)
        {
            return range_type(0, numeric_limits<position_type>::max());
        }

    // constructors
    // singular
    CRangeMapIterator(void)
        {
        }
    // begin()
    CRangeMapIterator(TSelect& selectMap)
        : m_Range(max_range()),
          m_SelectIter(selectMap.begin()), m_SelectIterEnd(selectMap.end())
        {
            if ( !Finished() ) {
                m_LevelIter = GetLevel().begin();
                _ASSERT(m_LevelIter != GetLevel().end());
            }
        }
    // begin(range)
    CRangeMapIterator(TSelect& selectMap, range_type range)
        : m_Range(range), m_SelectIterEnd(selectMap.end())
        {
            if ( range.second < range.first ) {
                // empty region
                m_SelectIter = m_SelectIterEnd;
            }
            else {
                // range.first <= range.second
                m_SelectIter = selectMap.begin();
                if ( !Finished() ) {
                    InitLevelIter();
                    Settle();
                }
            }
        }
    // find(key)
    CRangeMapIterator(TSelect& selectMap,
                      position_type selectKey, key_type key)
        : m_Range(max_range()),
          m_SelectIter(selectMap.find(selectKey)),
          m_SelectIterEnd(selectMap.end())
        {
            if ( !Finished() ) {
                _ASSERT(range.second >= range.first);
                m_LevelIter = GetLevel().find(key);
                if ( m_LevelIter == GetLevel().end() ) {
                    // not found: reset
                    m_SelectIter = m_SelectIterEnd;
                }
            }
        }
    // insert()
    CRangeMapIterator(TSelect& selectMap,
                      TSelectIter selectIter, TLevelIter levelIter)
        : m_Range(max_range()),
          m_SelectIter(selectIter), m_SelectIterEnd(selectMap.end()),
          m_LevelIter(levelIter)
        {
            _ASSERT(levelIter != GetLevel().end());
        }
    // end()
    CRangeMapIterator(TSelectIter selectIterEnd)
        : m_Range(0, 0),
          m_SelectIter(selectIterEnd), m_SelectIterEnd(selectIterEnd)
        {
        }

    // copy: non const -> const
    CRangeMapIterator(const CRangeMapIterator<Mapped, Position>& iter)
        : m_Range(iter.GetRange()),
          m_SelectIter(iter.GetSelectIter()),
          m_SelectIterEnd(iter.GetSelectIterEnd()),
          m_LevelIter(iter.GetLevelIter())
        {
        }

    // assignment: non const -> const
    TThisType& operator=(const CRangeMapIterator<Mapped, Position>& iter)
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
    ValueType& operator*(void) const
        {
            return *m_LevelIter;
        }
    ValueType* operator->(void) const
        {
            return &*m_LevelIter;
        }

private:
    bool Finished(void) const
        {
            return m_SelectIter == m_SelectIterEnd;
        }
    TLevel& GetLevel(void) const
        {
            return m_SelectIter->second;
        }
    void InitLevelIter(void)
        {
            // get maximum length of ranges in the level
            position_type maxLength = m_SelectIter->first;
            // starting level point
            m_LevelIter = GetLevel().
                lower_bound(range_type(m_Range.first - maxLength + 1,
                                       maxLength));
        }
    // stop at suitable point
    void Settle(void)
        {
            for ( ;; ) {
                // scan level
                while ( m_LevelIter != GetLevel().end() ) {
                    // we need to check only right bound of current range
                    if ( m_LevelIter->first.first >= m_Range.second )
                        break; // out of limiting range
                    if ( m_LevelIter->first.second >= m_Range.first )
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
    typedef pair<position_type, position_type> range_type;
    typedef range_type key_type;
    typedef Mapped mapped_type;
    typedef pair<const key_type, mapped_type> value_type;

    // internal typedefs
    typedef map<key_type, mapped_type, SRangeLess<position_type> > TLevelMap;
    typedef typename TLevelMap::iterator TLevelMapI;
    typedef typename TLevelMap::const_iterator TLevelMapCI;
    typedef map<position_type, TLevelMap> TSelectMap;
    typedef typename TSelectMap::iterator TSelectMapI;
    typedef typename TSelectMap::const_iterator TSelectMapCI;
    typedef CRangeMap<Mapped, Position> TThisType;
    
    // iterators
    typedef CRangeMapIterator<mapped_type, position_type> iterator;
    typedef CRangeMapIterator<mapped_type, position_type,
        const value_type, const TLevelMap, const TSelectMap,
        TLevelMapCI, TSelectMapCI> const_iterator;

    // constructor
    explicit CRangeMap(void)
        : m_TotalCount(0), m_TotalRange(0, 0)
        {
        }
    ~CRangeMap(void)
        {
        }

    // capacity
    bool empty(void) const
        {
            return m_TotalCount == 0;
        }
    size_t size(void) const
        {
            return m_TotalCount;
        }
    const range_type& GetTotalRange(void) const
        {
            return m_TotalRange;
        }
    
    // iterators
    const_iterator begin(range_type range) const
        {
            return const_iterator(m_SelectMap, range);
        }
    const_iterator begin(void) const
        {
            return const_iterator(m_SelectMap);
        }
    const_iterator end(void) const
        {
            return const_iterator(m_SelectMap.end());
        }
    iterator begin(range_type range)
        {
            return iterator(m_SelectMap, range);
        }
    iterator begin(void)
        {
            return iterator(m_SelectMap);
        }
    iterator end(void)
        {
            return iterator(m_SelectMap.end());
        }

    // element search
    const_iterator find(key_type key) const
        {
            return const_iterator(m_SelectMap, get_max_length(key), key);
        }
    iterator find(key_type key)
        {
            return iterator(m_SelectMap, get_max_length(key), key);
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
            if ( --m_TotalCount == 0 ) {
                m_TotalRange = range_type(0, 0);
            }
            else {
                // update total range
                if ( erased_key.first == m_TotalRange.first )
                    update_total_from();
                if ( erased_key.second == m_TotalRange.second )
                    update_total_to();
            }

            if ( levelIter == selectIter->second.end() ) {
                // end of level
                if ( selectIter->second.empty() )
                    selectIter = m_SelectMap.erase(selectIter); // remove level
                else
                    ++selectIter; // go to next level

                if ( selectIter == m_SelectMap.end() )
                    return end(); // no more levels
                else
                    return iterator(m_SelectMap,
                                    selectIter, selectIter->second.begin());
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
            m_TotalCount = 0;
            m_TotalRange = range_type(0, 0);
        }

    pair<iterator, bool> insert(const value_type& value)
        {
            if ( value.first.second < value.first.first )
                THROW1_TRACE(runtime_error, "empty key range");
            position_type selectKey = get_max_length(value.first);
            typedef typename TSelectMap::value_type select_value;
            pair<TSelectMapI, bool> selectInsert =
                m_SelectMap.insert(select_value(selectKey, TLevelMap()));
            pair<TLevelMapI, bool> levelInsert =
                selectInsert.first->second.insert(value);
            if ( levelInsert.second ) {
                // new element
                if ( ++m_TotalCount == 1 )
                    m_TotalRange = value.first;
                else {
                    m_TotalRange.first =
                        min(m_TotalRange.first, value.first.first);
                    m_TotalRange.second =
                        max(m_TotalRange.second, value.first.second);
                }
            }
            return make_pair(iterator(m_SelectMap,
                                      selectInsert.first, levelInsert.first),
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
            _ASSERT(key.second >= key.first);
            position_type len = key.second - key.first + 1;
            len |= (len >> 16);
            len |= (len >> 8);
            len |= (len >> 4);
            len |= (len >> 2);
            return len | (len >> 1);
        }

private:
    // data
    TSelectMap m_SelectMap;
    size_t m_TotalCount; // count of ranges
    range_type m_TotalRange; // enclosing range
};

#include <objects/objmgr/rangemap.inl>

END_NCBI_SCOPE

#endif  /* RANGEMAP__HPP */
