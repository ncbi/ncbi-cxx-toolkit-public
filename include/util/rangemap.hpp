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
*   Generic map with range as key
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.10  2001/01/16 20:52:24  vasilche
* Simplified some CRangeMap code.
*
* Revision 1.9  2001/01/11 15:00:39  vasilche
* Added CIntervalTree for seraching on set of intervals.
*
* Revision 1.8  2001/01/05 20:08:53  vasilche
* Added util directory for various algorithms and utility classes.
*
* Revision 1.7  2001/01/05 16:29:02  vasilche
* Fixed incompatibility with MIPS C++ compiler.
*
* Revision 1.6  2001/01/05 13:59:04  vasilche
* Reduced CRangeMap* templates size.
* Added CRangeMultimap template.
*
* Revision 1.5  2001/01/03 21:29:41  vasilche
* Fixed wrong typedef.
*
* Revision 1.4  2001/01/03 18:58:39  vasilche
* Some typedefs are missing on MSVC.
*
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
#include <util/range.hpp>
#include <map>

BEGIN_NCBI_SCOPE

// range map forward declaration
template<typename Mapped, typename Position> class CRangeMap;
template<typename Mapped, typename Position> class CRangeMultimap;

// range map iterator base
template<typename Traits>
class CRangeMapIterator
{
public:
    // typedefs
    typedef Traits traits;
    typedef typename traits::position_type position_type;
    typedef typename traits::range_type range_type;
    typedef typename traits::mapped_type mapped_type;
    typedef typename traits::value_type value_type;

    typedef typename traits::reference reference;
    typedef typename traits::pointer pointer;

    typedef typename traits::TRangeMap TRangeMap;
    typedef typename traits::TSelectMapRef TSelectMapRef;
    typedef typename traits::TSelectIter TSelectIter;
    typedef typename traits::TLevelIter TLevelIter;

    // internal typedefs
    typedef CRangeMapIterator<traits> TThisType;

    // friends
    friend class CRangeMap<mapped_type, position_type>;
    friend class CRangeMultimap<mapped_type, position_type>;

    // constructors
    // singular
    CRangeMapIterator(void)
        {
        }
    // copy from non const iterator
    CRangeMapIterator(const typename traits::non_const_iterator& iter)
        : m_Range(iter.GetRange()),
          m_SelectIter(iter.GetSelectIter()),
          m_SelectIterEnd(iter.GetSelectIterEnd()),
          m_LevelIter(iter.GetLevelIter())
        {
        }

    // move
    TThisType& operator++(void)
        {
            ++m_LevelIter;
            Settle();
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
    bool Finished(void) const
        {
            return m_SelectIter == m_SelectIterEnd;
        }
    bool operator==(const TThisType& iter) const
        {
            _ASSERT(GetSelectIterEnd() == iter.GetSelectIterEnd());
            return GetSelectIter() == iter.GetSelectIter() &&
                (Finished() || GetLevelIter() == iter.GetLevelIter());
        }
    bool operator!=(const TThisType& iter) const
        {
            return !(*this == iter);
        }

    // dereference
    reference operator*(void) const
        {
            return *GetLevelIter();
        }
    pointer operator->(void) const
        {
            return &*GetLevelIter();
        }

private:
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
                    lower_bound(range_type(from - maxLength + 1, from));
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

    void SetEnd(TSelectMapRef selectMap)
        {
            m_Range = range_type::GetEmpty();
            m_SelectIter = m_SelectIterEnd = selectMap.end();
        }
    void SetBegin(TSelectMapRef selectMap)
        {
            m_Range = range_type::GetWhole();
            m_SelectIter = selectMap.begin();
            m_SelectIterEnd = selectMap.end();
            if ( !Finished() )
                m_LevelIter = m_SelectIter->second.begin();
        }
    void SetBegin(range_type range, TSelectMapRef selectMap)
        {
            m_Range = range;
            m_SelectIter = selectMap.begin();
            m_SelectIterEnd = selectMap.end();
            if ( !Finished() ) {
                InitLevelIter();
                Settle();
            }
        }
    void Find(range_type key, TSelectMapRef selectMap)
        {
            if ( !key.Empty() ) {
                TSelectIter selectIter =
                    selectMap.lower_bound(traits::get_key_length(key));
                // now selectIter->first >= key.length
                TSelectIter selectIterEnd = selectMap.end();
                if ( selectIter != selectIterEnd ) {
                    TLevelIter levelIter = selectIter->second.find(key);
                    if ( levelIter != selectIter->second.end() ) {
                        m_Range = range_type::GetWhole();
                        m_SelectIter = selectIter;
                        m_SelectIterEnd = selectIterEnd;
                        m_LevelIter = levelIter;
                        return;
                    }
                }
            }
            // not found
            SetEnd();
        }

private:
    // iterator data
    range_type m_Range;          // range to search
    TSelectIter m_SelectIter;    // first level iter
    TSelectIter m_SelectIterEnd; // first level iter limit
    TLevelIter m_LevelIter;      // second level iter
};

// range map iterators traits
template<typename Position, typename Mapped>
class CRangeMapTraitsBase
{
public:
    typedef Position position_type;
	typedef CRange<position_type> range_type;
	typedef Mapped mapped_type;
	typedef pair<const range_type, mapped_type> value_type;

    // private methods
    static position_type get_key_length(range_type key)
        {
            _ASSERT(!key.Empty());
            if ( key.HaveInfiniteBound() )
                return key.GetWholeLength();
            return key.GetLength();
        }
    // calculates selection key depending on length of interval
    //            length           key
    //                 1             1
    //             2...3             3
    //             4...7             7
    // 2^n...(2^(n+1)-1)   (2^(n+1)-1)
    static position_type get_max_length(range_type key)
        {
            position_type len = get_key_length(key);
            if ( sizeof(position_type) > 4 )
                len |= (len >> 32);
            if ( sizeof(position_type) > 2 )
                len |= (len >> 16);
            len |= (len >> 8);
            len |= (len >> 4);
            len |= (len >> 2);
            len |= (len >> 1);
            return len;
        }
};

template<typename Position, typename Mapped>
class CRangeMapTraits : public CRangeMapTraitsBase<Position, Mapped>
{
    typedef CRangeMapTraitsBase<Position, Mapped> TParent;
public:
    typedef CRangeMap<Mapped, Position> TRangeMap;
	typedef map<typename TParent::range_type, Mapped> TLevelMap;
	typedef map<Position, TLevelMap> TSelectMap;
};

template<typename Position, typename Mapped>
class CRangeMapIteratorTraits : public CRangeMapTraits<Position, Mapped>
{
    typedef CRangeMapTraits<Position, Mapped> TParent;
public:
	typedef typename TParent::TRangeMap& TRangeMapRef;
	typedef typename TParent::TSelectMap& TSelectMapRef;
	typedef typename TParent::TLevelMap& TLevelMapRef;
	typedef typename TParent::TSelectMap::iterator TSelectIter;
	typedef typename TParent::TLevelMap::iterator TLevelIter;
	typedef typename TParent::value_type& reference;
	typedef typename TParent::value_type* pointer;
    typedef CRangeMapIteratorTraits<Position, Mapped> TIteratorTraits;
    typedef CRangeMapIteratorTraits<Position, Mapped> TNCIteratorTraits;
	typedef CRangeMapIterator<TIteratorTraits> iterator;
	typedef CRangeMapIterator<TNCIteratorTraits> non_const_iterator;
};

template<typename Position, typename Mapped>
class CRangeMapConstIteratorTraits : public CRangeMapTraits<Position, Mapped>
{
    typedef CRangeMapTraits<Position, Mapped> TParent;
public:
	typedef const typename TParent::TRangeMap& TRangeMapRef;
	typedef const typename TParent::TSelectMap& TSelectMapRef;
	typedef const typename TParent::TLevelMap& TLevelMapRef;
	typedef typename TParent::TSelectMap::const_iterator TSelectIter;
	typedef typename TParent::TLevelMap::const_iterator TLevelIter;
	typedef const typename TParent::value_type& reference;
	typedef const typename TParent::value_type* pointer;
    typedef CRangeMapConstIteratorTraits<Position, Mapped> TIteratorTraits;
    typedef CRangeMapIteratorTraits<Position, Mapped> TNCIteratorTraits;
	typedef CRangeMapIterator<TIteratorTraits> iterator;
	typedef CRangeMapIterator<TNCIteratorTraits> non_const_iterator;
};

template<typename Position, typename Mapped>
class CRangeMultimapTraits : public CRangeMapTraitsBase<Position, Mapped>
{
    typedef CRangeMapTraitsBase<Position, Mapped> TParent;
public:
    typedef CRangeMultimap<Mapped, Position> TRangeMap;
	typedef multimap<typename TParent::range_type, Mapped> TLevelMap;
	typedef map<Position, TLevelMap> TSelectMap;
};

template<typename Position, typename Mapped>
class CRangeMultimapIteratorTraits : public CRangeMultimapTraits<Position, Mapped>
{
    typedef CRangeMultimapTraits<Position, Mapped> TParent;
public:
	typedef typename TParent::TRangeMap& TRangeMapRef;
	typedef typename TParent::TSelectMap& TSelectMapRef;
	typedef typename TParent::TLevelMap& TLevelMapRef;
	typedef typename TParent::TSelectMap::iterator TSelectIter;
	typedef typename TParent::TLevelMap::iterator TLevelIter;
	typedef typename TParent::value_type& reference;
	typedef typename TParent::value_type* pointer;
    typedef CRangeMultimapIteratorTraits<Position, Mapped> TIteratorTraits;
    typedef CRangeMultimapIteratorTraits<Position, Mapped> TNCIteratorTraits;
	typedef CRangeMapIterator<TIteratorTraits> iterator;
	typedef CRangeMapIterator<TNCIteratorTraits> non_const_iterator;
};

template<typename Position, typename Mapped>
class CRangeMultimapConstIteratorTraits : public CRangeMultimapTraits<Position, Mapped>
{
    typedef CRangeMultimapTraits<Position, Mapped> TParent;
public:
	typedef const typename TParent::TRangeMap& TRangeMapRef;
	typedef const typename TParent::TSelectMap& TSelectMapRef;
	typedef const typename TParent::TLevelMap& TLevelMapRef;
	typedef typename TParent::TSelectMap::const_iterator TSelectIter;
	typedef typename TParent::TLevelMap::const_iterator TLevelIter;
	typedef const typename TParent::value_type& reference;
	typedef const typename TParent::value_type* pointer;
    typedef CRangeMultimapConstIteratorTraits<Position, Mapped> TIteratorTraits;
    typedef CRangeMultimapIteratorTraits<Position, Mapped> TNCIteratorTraits;
	typedef CRangeMapIterator<TIteratorTraits> iterator;
	typedef CRangeMapIterator<TNCIteratorTraits> non_const_iterator;
};

// range map
template<typename Mapped, typename Position = int>
class CRangeMap
{
public:
    // standard typedefs
    typedef CRangeMapTraits<Position, Mapped> TRangeMapTraits;
    typedef CRangeMapIteratorTraits<Position, Mapped> TIteratorTraits;
    typedef CRangeMapConstIteratorTraits<Position, Mapped> TConstIteratorTraits;
    typedef size_t size_type;
	typedef typename TIteratorTraits::iterator iterator;
	typedef typename TConstIteratorTraits::iterator const_iterator;
    typedef typename TRangeMapTraits::position_type position_type;
    typedef typename TRangeMapTraits::range_type range_type;
	typedef typename TRangeMapTraits::mapped_type mapped_type;
	typedef typename TRangeMapTraits::value_type value_type;
	typedef range_type key_type;

private:
	// internal typedefs
    typedef CRangeMap<Mapped, Position> TThisType;
	typedef typename TRangeMapTraits::TSelectMap TSelectMap;
	typedef typename TRangeMapTraits::TLevelMap TLevelMap;
    typedef typename TSelectMap::iterator TSelectMapI;
    typedef typename TLevelMap::iterator TLevelMapI;
    typedef typename TSelectMap::const_iterator TSelectMapCI;
    typedef typename TLevelMap::const_iterator TLevelMapCI;

public:
    // constructor
    explicit CRangeMap(void)
        {
        }

    // capacity
    bool empty(void) const
        {
            return m_SelectMap.empty();
        }
    size_type size(void) const
        {
            size_type size = 0;
            for ( TSelectMapCI i = m_SelectMap.begin(),
                      end = m_SelectMap.end(); i != end; ++i ) {
                size += i->second.size();
            }
            return size;
        }
    
    // iterators
    const_iterator end(void) const
        {
            const_iterator iter;
            iter.SetEnd(m_SelectMap);
            return iter;
        }
    const_iterator begin(void) const
        {
            const_iterator iter;
            iter.SetBegin(m_SelectMap);
            return iter;
        }
    const_iterator begin(range_type range) const
        {
            const_iterator iter;
            iter.SetBegin(range, m_SelectMap);
            return iter;
        }

    iterator end(void)
        {
            iterator iter;
            iter.SetEnd(m_SelectMap);
            return iter;
        }
    iterator begin(void)
        {
            iterator iter;
            iter.SetBegin(m_SelectMap);
            return iter;
        }
    iterator begin(range_type range)
        {
            iterator iter;
            iter.SetBegin(range, m_SelectMap);
            return iter;
        }

    // element search
    const_iterator find(key_type key) const
        {
            const_iterator iter;
            iter.Find(key, m_SelectMap);
            return iter;
        }
    iterator find(key_type key)
        {
            iterator iter;
            iter.Find(key, m_SelectMap);
            return iter;
        }

    // modification
    void erase(iterator iter)
        {
            _ASSERT(iter != end());

            // get element's level
            TLevelMap& level = iter.GetSelectIter()->second;

            // erase element on level
            level.erase(iter.GetLevelIter());

            if ( level.empty() ) // end of level
                m_SelectMap.erase(iter.GetSelectIter()); // remove level
        }
    size_type erase(key_type key)
        {
            iterator iter = find(key);
            if ( iter.Finished() )
                return 0;
            erase(iter);
            return 1;
        }
    void clear(void)
        {
            m_SelectMap.clear();
        }
    void swap(TThisType& rangeMap)
        {
            m_SelectMap.swap(rangeMap.m_SelectMap);
        }

    pair<iterator, bool> insert(const value_type& value)
        {
            if ( value.first.Empty() )
                THROW1_TRACE(runtime_error, "empty key range");

            // key in select map
            position_type selectKey =
                TRangeMapTraits::get_max_length(value.first);

            // get level
            typedef typename TSelectMap::value_type select_value;
            TSelectMapI selectIter =
                m_SelectMap.insert(select_value(selectKey, TLevelMap())).first;
            // insert element
            pair<TLevelMapI, bool> levelInsert =
                selectIter->second.insert(value);
            
            pair<iterator, bool> ret;
            ret.second = levelInsert.second;
            ret.first.m_Range = range_type::GetWhole();
            ret.first.m_SelectIter = selectIter;
            ret.first.m_SelectIterEnd = m_SelectMap.end();
            ret.first.m_LevelIter = levelInsert.first;
            return ret;
        }

    // element access
    mapped_type& operator[](key_type key)
        {
            return insert(value_type(key, mapped_type())).first->second;
        }

private:
    // data
    TSelectMap m_SelectMap;
};

// range map
template<typename Mapped, typename Position = int>
class CRangeMultimap
{
public:
    // standard typedefs
    typedef CRangeMultimapTraits<Position, Mapped> TRangeMapTraits;
    typedef CRangeMultimapIteratorTraits<Position, Mapped> TIteratorTraits;
    typedef CRangeMultimapConstIteratorTraits<Position, Mapped> TConstIteratorTraits;
    typedef size_t size_type;
	typedef typename TIteratorTraits::iterator iterator;
	typedef typename TConstIteratorTraits::iterator const_iterator;
    typedef typename TRangeMapTraits::position_type position_type;
    typedef typename TRangeMapTraits::range_type range_type;
	typedef typename TRangeMapTraits::mapped_type mapped_type;
	typedef typename TRangeMapTraits::value_type value_type;
	typedef range_type key_type;

private:
	// internal typedefs
    typedef CRangeMultimap<Mapped, Position> TThisType;
	typedef typename TRangeMapTraits::TSelectMap TSelectMap;
	typedef typename TRangeMapTraits::TLevelMap TLevelMap;
    typedef typename TSelectMap::iterator TSelectMapI;
    typedef typename TLevelMap::iterator TLevelMapI;
    typedef typename TSelectMap::const_iterator TSelectMapCI;
    typedef typename TLevelMap::const_iterator TLevelMapCI;

public:
    // constructor
    explicit CRangeMultimap(void)
        {
        }

    // capacity
    bool empty(void) const
        {
            return m_SelectMap.empty();
        }
    size_type size(void) const
        {
            size_type size = 0;
            for ( TSelectMapCI i = m_SelectMap.begin(),
                      end = m_SelectMap.end(); i != end; ++i ) {
                size += i->second.size();
            }
            return size;
        }
    
    // iterators
    const_iterator end(void) const
        {
            const_iterator iter;
            iter.SetEnd(m_SelectMap);
            return iter;
        }
    const_iterator begin(void) const
        {
            const_iterator iter;
            iter.SetBegin(m_SelectMap);
            return iter;
        }
    const_iterator begin(range_type range) const
        {
            const_iterator iter;
            iter.SetBegin(range, m_SelectMap);
            return iter;
        }

    iterator end(void)
        {
            iterator iter;
            iter.SetEnd(m_SelectMap);
            return iter;
        }
    iterator begin(void)
        {
            iterator iter;
            iter.SetBegin(m_SelectMap);
            return iter;
        }
    iterator begin(range_type range)
        {
            iterator iter;
            iter.SetBegin(range, m_SelectMap);
            return iter;
        }

    // element search
    const_iterator find(key_type key) const
        {
            const_iterator iter;
            iter.Find(key, m_SelectMap);
            return iter;
        }
    iterator find(key_type key)
        {
            iterator iter;
            iter.Find(key, m_SelectMap);
            return iter;
        }

    // modification
    void erase(iterator iter)
        {
            _ASSERT(iter != end());

            // get element's level
            TLevelMap& level = iter.GetSelectIter()->second;

            // erase element on level
            level.erase(iter.GetLevelIter());

            if ( level.empty() ) // end of level
                m_SelectMap.erase(iter.GetSelectIter()); // remove level
        }
    size_type erase(key_type key)
        {
            size_type count = 0;
            iterator iter = find(key);
            while ( !iter.Finished() && iter->first == key ) {
                iterator toErase = iter;
                ++iter;
                ++count;
                erase(toErase);
            }
            return count;
        }
    void clear(void)
        {
            m_SelectMap.clear();
        }
    void swap(TThisType& rangeMap)
        {
            m_SelectMap.swap(rangeMap.m_SelectMap);
        }

    void insert(const value_type& value)
        {
            if ( value.first.Empty() )
                THROW1_TRACE(runtime_error, "empty key range");

            // select key
            position_type selectKey =
                TRangeMapTraits::get_max_length(value.first);

            // insert element
            m_SelectMap[selectKey].insert(value);
        }
private:
    // data
    TSelectMap m_SelectMap;
};

//#include <util/rangemap.inl>

END_NCBI_SCOPE

#endif  /* RANGEMAP__HPP */
