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
* Revision 1.13  2001/06/28 12:44:56  grichenk
* Added some comments
*
* Revision 1.12  2001/01/29 15:18:39  vasilche
* Cleaned CRangeMap and CIntervalTree classes.
*
* Revision 1.11  2001/01/16 21:37:22  vasilche
* Fixed warning.
*
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
template<class Traits> class CRangeMapBase;
template<class Traits> class CRangeMapIterator;
template<typename Mapped, typename Position> class CRangeMap;
template<typename Mapped, typename Position> class CRangeMultimap;

// range map traits
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
            position_type len = get_key_length(key) | 64;
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
class CRangeMultimapTraits : public CRangeMapTraitsBase<Position, Mapped>
{
    typedef CRangeMapTraitsBase<Position, Mapped> TParent;
public:
    typedef CRangeMultimap<Mapped, Position> TRangeMap;
	typedef multimap<typename TParent::range_type, Mapped> TLevelMap;
	typedef map<Position, TLevelMap> TSelectMap;
};

// range map iterators traits
template<typename MapTraits>
class CRangeMapIteratorTraits : public MapTraits
{
public:
    typedef MapTraits TMapTraits;
    typedef CRangeMapIteratorTraits<TMapTraits> TIteratorTraits;
    typedef CRangeMapIteratorTraits<TMapTraits> TNCIteratorTraits;

	typedef typename TMapTraits::TRangeMap& TRangeMapRef;
	typedef typename TMapTraits::TSelectMap& TSelectMapRef;
	typedef typename TMapTraits::TLevelMap& TLevelMapRef;
	typedef typename TMapTraits::TSelectMap::iterator TSelectIter;
	typedef typename TMapTraits::TLevelMap::iterator TLevelIter;
	typedef typename TMapTraits::value_type& reference;
	typedef typename TMapTraits::value_type* pointer;

	typedef CRangeMapIterator<TIteratorTraits> iterator;
	typedef CRangeMapIterator<TNCIteratorTraits> non_const_iterator;
};

template<typename MapTraits>
class CRangeMapConstIteratorTraits : public MapTraits
{
public:
    typedef MapTraits TMapTraits;
    typedef CRangeMapConstIteratorTraits<TMapTraits> TIteratorTraits;
    typedef CRangeMapIteratorTraits<TMapTraits> TNCIteratorTraits;

	typedef const typename TMapTraits::TRangeMap& TRangeMapRef;
	typedef const typename TMapTraits::TSelectMap& TSelectMapRef;
	typedef const typename TMapTraits::TLevelMap& TLevelMapRef;
	typedef typename TMapTraits::TSelectMap::const_iterator TSelectIter;
	typedef typename TMapTraits::TLevelMap::const_iterator TLevelIter;
	typedef const typename TMapTraits::value_type& reference;
	typedef const typename TMapTraits::value_type* pointer;

	typedef CRangeMapIterator<TIteratorTraits> iterator;
	typedef CRangeMapIterator<TNCIteratorTraits> non_const_iterator;
};

// range map iterator base
template<class Traits>
class CRangeMapIterator
{
public:
    // typedefs
    typedef Traits TTraits;
    typedef typename TTraits::position_type position_type;
    typedef typename TTraits::range_type range_type;
    typedef typename TTraits::mapped_type mapped_type;
    typedef typename TTraits::value_type value_type;

    typedef typename TTraits::reference reference;
    typedef typename TTraits::pointer pointer;

    typedef typename TTraits::TRangeMap TRangeMap;
    typedef typename TTraits::TSelectMapRef TSelectMapRef;
    typedef typename TTraits::TSelectIter TSelectIter;
    typedef typename TTraits::TLevelIter TLevelIter;

    // internal typedefs
    typedef typename TTraits::iterator TThisType;

    // friends
    friend class CRangeMapBase<typename TTraits::TMapTraits>;
    friend class CRangeMap<mapped_type, position_type>;
    friend class CRangeMultimap<mapped_type, position_type>;

    // constructors
    // singular
    CRangeMapIterator(void)
        {
        }
    // copy from non const iterator
    CRangeMapIterator(const typename TTraits::non_const_iterator& iter)
        : m_Range(iter.GetRange()),
          m_SelectIter(iter.GetSelectIter()),
          m_SelectIterEnd(iter.GetSelectIterEnd()),
          m_LevelIter(iter.GetLevelIter())
        {
        }

    // get range to search
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
    TLevelIter GetLevelIterEnd(void) const
        {
            return GetSelectIter()->second.end();
        }

    // check state
    bool Valid(void) const
        {
            return m_SelectIter != m_SelectIterEnd;
        }
    operator bool(void) const
        {
            return Valid();
        }
    bool operator!(void) const
        {
            return !Valid();
        }

    bool operator==(const TThisType& iter) const
        {
            _ASSERT(GetSelectIterEnd() == iter.GetSelectIterEnd());
            return GetSelectIter() == iter.GetSelectIter() &&
                (!*this || GetLevelIter() == iter.GetLevelIter());
        }
    bool operator!=(const TThisType& iter) const
        {
            return !(*this == iter);
        }

    // dereference
    range_type GetInterval(void) const
        {
            return GetLevelIter()->first;
        }
    reference operator*(void) const
        {
            return *GetLevelIter();
        }
    pointer operator->(void) const
        {
            return &*GetLevelIter();
        }

private:
    // Move level iterator to the first entry following or equal to
    // the levelIter and intersecting with the range to search
    bool SetLevelIter(TLevelIter levelIter)
        {
            TLevelIter levelIterEnd = GetLevelIterEnd();
            while ( levelIter != levelIterEnd ) {
                // find an entry intersecting with the range
                if ( levelIter->first.GetTo() >= m_Range.GetFrom() ) {
                    if ( levelIter->first.GetFrom() <= m_Range.GetTo() ) {
                        m_LevelIter = levelIter; // set the iterator
                        return true; // found
                    }
                    return false; // no such entry
                }
                ++levelIter; // check next entry
            }
            return false; // entry not found
        }

    // Find the first level entry, which can (but not always does)
    // intersect with the range to search.
    TLevelIter FirstLevelIter(void)
        {
            position_type from = m_Range.GetFrom();
            if ( from == range_type::GetWholeFrom() ) {
                // special case: whole region
                return m_SelectIter->second.begin();
            }
            else {
                // get maximum length of ranges in the level
                position_type maxLength = m_SelectIter->first;
                // Skip all ranges, which can NOT intersect with the
                // range to search, return the first one that CAN
                // (but not always does).
                return m_SelectIter->second.lower_bound
                    (range_type(from - maxLength + 1, from));
            }
        }

    // Go to the last element, reset range to empty
    void SetEnd(TSelectMapRef selectMap)
        {
            m_Range = range_type::GetEmpty();
            m_SelectIter = m_SelectIterEnd = selectMap.end();
        }
    // Go to the very first element, reset range to whole
    void SetBegin(TSelectMapRef selectMap)
        {
            m_Range = range_type::GetWhole();
            TSelectIter selectIter = m_SelectIter = selectMap.begin();
            TSelectIter selectIterEnd = m_SelectIterEnd = selectMap.end();
            if ( selectIter != selectIterEnd ) {
                m_LevelIter = selectIter->second.begin();
            }
        }
    // Set range, find the first element possibly intersecting the range
    void SetBegin(range_type range, TSelectMapRef selectMap)
        {
            m_Range = range;
            TSelectIter selectIter = m_SelectIter = selectMap.begin();
            TSelectIter selectIterEnd = m_SelectIterEnd = selectMap.end();
            // Go through all select map elements, search for a "good"
            // level entry
            while ( selectIter != selectIterEnd &&
                    !SetLevelIter(FirstLevelIter()) ) {
                m_SelectIter = ++selectIter;
            }
        }
    // Find an entry specified by the key in the 2-level map
    void Find(range_type key, TSelectMapRef selectMap);

public:
    // move
    TThisType& operator++(void)
        {
            TLevelIter levelIter = m_LevelIter;
            TSelectIter selectIterEnd = m_SelectIterEnd;
            ++levelIter;
            while ( !SetLevelIter(levelIter) &&
                    ++m_SelectIter != selectIterEnd ) {
                levelIter = FirstLevelIter();
            }
            return *this;
        }

private:
    // iterator data
    range_type m_Range;          // range to search
    TSelectIter m_SelectIter;    // first level iter
    TSelectIter m_SelectIterEnd; // first level iter limit
    TLevelIter m_LevelIter;      // second level iter
};

template<class Traits>
void CRangeMapIterator<Traits>::Find(range_type key, TSelectMapRef selectMap)
{
    TSelectIter selectIterEnd = selectMap.end();
    if ( !key.Empty() ) {
        TSelectIter selectIter =
            selectMap.lower_bound(TTraits::get_key_length(key));
        // now selectIter->first >= key.length
        if ( selectIter != selectIterEnd ) {
            TLevelIter levelIter = selectIter->second.find(key);
            if ( levelIter != selectIter->second.end() ) {
                // found the key
                m_Range = range_type::GetWhole();
                m_SelectIter = selectIter;
                m_SelectIterEnd = selectIterEnd;
                m_LevelIter = levelIter;
                return;
            }
        }
    }
    // not found
    m_Range = range_type::GetEmpty();
    m_SelectIter = m_SelectIterEnd = selectIterEnd;
}

// range map
template<class Traits>
class CRangeMapBase
{
public:
    // standard typedefs
    typedef Traits TRangeMapTraits;
    typedef CRangeMapIteratorTraits<TRangeMapTraits> TIteratorTraits;
    typedef CRangeMapConstIteratorTraits<TRangeMapTraits> TConstIteratorTraits;

    typedef size_t size_type;
    typedef typename TRangeMapTraits::position_type position_type;
    typedef typename TRangeMapTraits::range_type range_type;
	typedef typename TRangeMapTraits::mapped_type mapped_type;
	typedef typename TRangeMapTraits::value_type value_type;
	typedef range_type key_type;

    typedef typename TRangeMapTraits::TRangeMap TThisType;
	typedef typename TIteratorTraits::iterator iterator;
	typedef typename TConstIteratorTraits::iterator const_iterator;

protected:
	// internal typedefs
	typedef typename TRangeMapTraits::TSelectMap TSelectMap;
	typedef typename TRangeMapTraits::TLevelMap TLevelMap;

    // constructor
    CRangeMapBase(void)
        {
#if defined(_RWSTD_VER) && !defined(_RWSTD_STRICT_ANSI)
            m_SelectMap.allocation_size(8);
#endif
        }

public:
    // capacity
    bool empty(void) const
        {
            return m_SelectMap.empty();
        }
    size_type size(void) const
        {
            typedef typename TSelectMap::const_iterator TSelectMapCI;

            size_type size = 0;
            for ( TSelectMapCI i = m_SelectMap.begin(),
                      end = m_SelectMap.end(); i != end; ++i ) {
                size += i->second.size();
            }
            return size;
        }
    pair<double, size_type> stat(void) const
        {
            return make_pair(0.0, size_type(0));
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
            while ( iter && iter.GetInterval() == key ) {
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

protected:
    // data
    TSelectMap m_SelectMap;
};

// range map
template<typename Mapped, typename Position = int>
class CRangeMap : public CRangeMapBase< CRangeMapTraits<Position, Mapped> >
{
    typedef CRangeMapBase< CRangeMapTraits<Position, Mapped> > TParent;
public:
    // standard typedefs
    typedef typename TParent::size_type size_type;
    typedef typename TParent::position_type position_type;
    typedef typename TParent::range_type range_type;
	typedef typename TParent::mapped_type mapped_type;
	typedef typename TParent::value_type value_type;
	typedef typename TParent::key_type key_type;

	typedef typename TParent::iterator iterator;
	typedef typename TParent::const_iterator const_iterator;

    // constructor
    explicit CRangeMap(void)
        {
        }

    pair<iterator, bool> insert(const value_type& value)
        {
            if ( value.first.Empty() )
                THROW1_TRACE(runtime_error, "empty key range");

            typedef typename TParent::TRangeMapTraits TRangeMapTraits;
            // key in select map
            position_type selectKey =
                TRangeMapTraits::get_max_length(value.first);

            // get level
            typedef typename TParent::TSelectMap::value_type select_value;
            typedef typename TParent::TSelectMap::iterator TSelectMapI;
            typedef typename TParent::TLevelMap::iterator TLevelMapI;

            // insert element
            pair<TSelectMapI, bool> selectInsert =
                m_SelectMap.insert(select_value(selectKey, TLevelMap()));
            pair<TLevelMapI, bool> levelInsert =
                selectInsert.first->second.insert(value);
            
            pair<iterator, bool> ret;
            ret.second = levelInsert.second;
            ret.first.m_Range = range_type::GetWhole();
            ret.first.m_SelectIter = selectInsert.first;
            ret.first.m_SelectIterEnd = m_SelectMap.end();
            ret.first.m_LevelIter = levelInsert.first;
            return ret;
        }

    // element access
    mapped_type& operator[](key_type key)
        {
            return insert(value_type(key, mapped_type())).first->second;
        }
};

// range map
template<typename Mapped, typename Position = int>
class CRangeMultimap : public CRangeMapBase< CRangeMultimapTraits<Position, Mapped> >
{
    typedef CRangeMapBase< CRangeMultimapTraits<Position, Mapped> > TParent;
public:
    // standard typedefs
    typedef typename TParent::size_type size_type;
    typedef typename TParent::position_type position_type;
    typedef typename TParent::range_type range_type;
	typedef typename TParent::mapped_type mapped_type;
	typedef typename TParent::value_type value_type;
	typedef typename TParent::key_type key_type;

	typedef typename TParent::iterator iterator;
	typedef typename TParent::const_iterator const_iterator;

    // constructor
    explicit CRangeMultimap(void)
        {
        }

    iterator insert(const value_type& value)
        {
            if ( value.first.Empty() )
                THROW1_TRACE(runtime_error, "empty key range");

            typedef typename TParent::TRangeMapTraits TRangeMapTraits;
            // select key
            position_type selectKey =
                TRangeMapTraits::get_max_length(value.first);

            typedef typename TParent::TSelectMap::value_type select_value;
            typedef typename TParent::TSelectMap::iterator TSelectMapI;
            typedef typename TParent::TLevelMap::iterator TLevelMapI;

            // insert element
            pair<TSelectMapI, bool> selectInsert =
                m_SelectMap.insert(select_value(selectKey, TLevelMap()));

            iterator ret;
            ret.m_Range = range_type::GetWhole();
            ret.m_SelectIter = selectInsert.first;
            ret.m_SelectIterEnd = m_SelectMap.end();
            ret.m_LevelIter = selectInsert.first->second.insert(value);
            return ret;
        }
};

//#include <util/rangemap.inl>

END_NCBI_SCOPE

#endif  /* RANGEMAP__HPP */
