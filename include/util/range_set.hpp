#ifndef UTIL___RANGE_SET__HPP
#define UTIL___RANGE_SET__HPP

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
 * Authors:  Andrey Yazhuk, Eugene Vasilchenko
 *
 * File Description:
 *  class CRangeSet - sorted collection of non-overlapping CRange-s
 */

#include <corelib/ncbistd.hpp>
#include <corelib/ncbistl.hpp>

#include <util/range.hpp>

#include <algorithm>

BEGIN_NCBI_SCOPE

template<class Range, class Position>
struct PRangeLessPos2
{
    using is_transparent = void;
    bool operator()(const Range &R, Position Pos) const { return R.GetToOpen() <= Pos;  }    
//    bool operator()(Position Pos, const Range &R) const { return Pos < R.GetToOpen();  }    
    bool operator()(const Range &R1, const Range &R2) const { return R1.GetToOpen() < R2.GetToOpen();  }    
};
    
///////////////////////////////////////////////////////////////////////////////
// class CRangeSet<Position> represent a sorted collection of CRange<Position>.
// It is guaranteed that ranges in collection aren't empty and do not overlap.
// Unless DivideAt() was explicitly called, it is also guarantees that they
// aren't adjacent so that there is no gap beween them.
// Adding an interval to the collection leads to possible merging it with 
// existing intervals.

template<class Position>
class CRangeSet 
{
public:
    typedef Position    position_type;
    typedef CRangeSet<position_type>  TThisType;
    typedef CRange<position_type>    TRange;
    typedef COpenRange<position_type>    TOpenRange;
    typedef PRangeLessPos2<TRange, position_type> TLess;
    typedef set<TRange, TLess>       TRanges;
    typedef typename TRanges::size_type                 size_type;
    typedef typename TRanges::const_iterator            const_iterator;
    typedef typename TRanges::const_reverse_iterator    const_reverse_iterator;
    
    CRangeSet()   { }
    explicit CRangeSet(const TRange& r)
    {
        if ( r.NotEmpty() ) {
            m_Ranges.insert(r);
        }
    }
    
    // immitating vector, but providing only "const" access to elements
    const_iterator  begin() const   
    {   
        return m_Ranges.begin();   
    }
    const_iterator  end() const     
    {   
        return m_Ranges.end(); 
    }
    const_reverse_iterator  rbegin() const  
    {   
        return m_Ranges.rbegin();  
    }
    const_reverse_iterator  rend() const    
    {   
        return m_Ranges.rend();    
    }
    size_type size() const  
    {   
        return m_Ranges.size();    
    }
    bool    empty() const   
    {   
        return m_Ranges.empty();   
    }
    /*
    const   TRange& operator[](size_type pos)   const   
    {  
        return m_Ranges[pos];  
    }
    */
    void    clear()
    {
        m_Ranges.clear();
    }
    // returns iterator pointing to the TRange that has ToOpen > pos
    // it will be either range containing the pos, or starting after pos
    const_iterator  find(position_type pos)   const
    {
        return m_Ranges.lower_bound(pos);
    }
    position_type   GetFrom() const
    {
        return empty()? TRange::GetEmptyFrom(): begin()->GetFrom();
    }
    position_type   GetToOpen() const
    {
        return empty()? TRange::GetEmptyToOpen(): rbegin()->GetToOpen();
    }
    position_type   GetTo() const
    {
        return empty()? TRange::GetEmptyTo(): rbegin()->GetTo();
    }
    bool            Empty() const
    {
        return empty();
    } 
    bool            NotEmpty() const
    {
        return !empty();
    }
    position_type   GetLength (void) const
    {
        return empty()? 0: rbegin()->GetToOpen() - begin()->GetFrom();
    }
    ///
    /// Returns total length covered by ranges in this collection, i.e.
    /// sum of lengths of ranges
    ///
    position_type   GetCoveredLength (void) const
    {
        position_type length = 0;
        for ( auto& r : m_Ranges ) {
            length += r.GetLength();
        }
        return length;
    }
    TRange          GetLimits() const
    {
        return empty()?
            TRange::GetEmpty():
            TOpenRange(begin()->GetFrom(), rbegin()->GetToOpen());
    }
    TThisType&  IntersectWith (const TRange& r)
    {
        x_IntersectWith(r);
        return *this;
    }
    TThisType &  operator &= (const TRange& r)
    {
        return IntersectWith(r);
    }
    // true if there is any intersection with the argument range
    bool  IntersectingWith (const TRange& r) const
    {
        return x_Intersects(r).second;
    }
    // true if the argument range is fully contained in one of the set's ranges
    bool  Contains (const TRange& r) const
    {
        return x_Contains(r).second;
    }
    TThisType&  CombineWith (const TRange& r)
    {
        x_CombineWith(r);
        return *this;
    }
    TThisType &  operator+= (const TRange& r)
    {
        return CombineWith(r);
    }
    TThisType&  Subtract(const TRange& r)
    {
        x_Subtract(r);
        return *this;
    }
    TThisType &  operator-= (const TRange& r)
    {
        return Subtract(r);
    }    
    TThisType&  IntersectWith (const TThisType &c)
    {
        x_IntersectWith(c);
        return *this;
    }
    TThisType &  operator&= (const TThisType &c)
    {
        return IntersectWith(c);
    }
    TThisType&  CombineWith (const TThisType &c)
    {
        x_CombineWith(c);
        return *this;
    }
    TThisType&  CombineWithAndKeepAbutting (const TThisType &c)
    {
        x_CombineWithAndKeepAbutting(c);
        return *this;
    }
    TThisType &  operator+= (const TThisType &c)
    {
        return CombineWith(c);
    }
    TThisType&  Subtract(const TThisType &c)
    {
        x_Subtract(c);
        return *this;
    }
    TThisType &  operator-= (const TThisType &c)
    {
        return Subtract(c);
    }    

    /// If position is in middle of range, divide into two consecutive ranges
    /// after this position
    TThisType & DivideAfter(const position_type &p)
    {
        x_Divide(p);
        return *this;
    }

private:
    typedef typename TRanges::iterator          iterator;
    typedef typename TRanges::reverse_iterator  reverse_iterator;

    /*
    iterator  begin_nc()   
    {   
        return m_Ranges.begin();
    }
    */
    iterator  end_nc()
    {   
        return m_Ranges.end(); 
    }    
    iterator find_nc(position_type pos)
    {
        return m_Ranges.lower_bound(pos);
    }
    // return pair(containing or next range, true if returned range contain pos)
    pair<const_iterator, bool>    x_Find(position_type pos) const
    {
        const_iterator it = find(pos);
        bool b_contains = it != end()  &&  it->GetFrom() <= pos;
        return make_pair(it, b_contains);
    }
    // return pair(intersecting or next range, true if there is an intersection)
    pair<const_iterator, bool>    x_Intersects(const TRange& r) const
    {
        const_iterator it = find(r.GetFrom());
        bool intersects = it != end()  &&  r.NotEmpty() && it->GetFrom() < r.GetToOpen();
        return make_pair(it, intersects);
    }
    // return pair(intersecting or next range, true if the argument range is fully contained)
    pair<const_iterator, bool>    x_Contains(const TRange& r) const
    {
        const_iterator it = find(r.GetFrom());
        bool contains = it != end() && it->GetFrom() <= r.GetFrom() && it->GetToOpen() >= r.GetToOpen();
        return make_pair(it, contains);
    }
    // trim all ranges that start before position keep_from
    // insert back trimmed last range if necessary
    void x_TrimBefore(position_type keep_from)
    {
        // simpler iterative trimming, since std::set ranged erase is iterative
        while ( !m_Ranges.empty() ) {
            iterator it = m_Ranges.begin();
            if ( it->GetFrom() >= keep_from ) {
                break;
            }
            position_type to_open = it->GetToOpen();
            m_Ranges.erase(it);
            if ( to_open > keep_from ) { // add trimmed intersecting range
                m_Ranges.insert(m_Ranges.begin(), TOpenRange(keep_from, to_open));
                break;
            }
        }
    }
    // trim all ranges that end at or above position keep_to_open
    // insert back trimmed last range if necessary
    void x_TrimStartingWith(position_type trim_at)
    {
        // simpler iterative trimming, since std::set ranged erase is iterative
        while ( !m_Ranges.empty() ) {
            iterator it = prev(m_Ranges.end());
            if ( it->GetToOpen() <= trim_at ) {
                break;
            }
            position_type from = it->GetFrom();
            m_Ranges.erase(it);
            if ( from < trim_at ) { // add trimmed intersecting range
                m_Ranges.insert(m_Ranges.end(), TOpenRange(from, trim_at));
                break;
            }
        }
    }
    // trim the set to the argument range
    void    x_IntersectWith(const TRange& r)
    {
        if ( r.Empty() ) {
            m_Ranges.clear();
        }
        else {
            x_TrimStartingWith(r.GetToOpen());
            x_TrimBefore(r.GetFrom());
        }
    }

    // returns iterator to the range representing result of combination
    // the range must be not empty
    iterator    x_CombineWith(const TRange& r)
    {
        position_type pos_from = r.GetFrom();
        position_type pos_to_open = r.GetToOpen();
        
        iterator it_begin = m_Ranges.begin();
        if ( pos_from > r.GetPositionMin() ) {
            // skip till the range that may touch lower bound of r
            it_begin = find(pos_from-1);
            if ( it_begin == m_Ranges.end() || it_begin->GetFrom() > pos_to_open ) {
                // no overlapping ranges, plain insert
                return m_Ranges.insert(it_begin, r);
            }
            // it_begin
            if ( it_begin->GetFrom() < pos_from ) {
                pos_from = it_begin->GetFrom();
            }
        }
        // now remove all ranges starting with it_begin until they are above and not touching r
        while ( it_begin != m_Ranges.end() && it_begin->GetFrom() <= pos_to_open ) {
            if ( it_begin->GetToOpen() > pos_to_open ) {
                pos_to_open = it_begin->GetToOpen();
            }
            m_Ranges.erase(it_begin++);
        }
        return m_Ranges.insert(it_begin, TOpenRange(pos_from, pos_to_open));
    }

    void    x_Subtract(const TRange& r)
    {
        if ( r.Empty() ) {
            return;
        }
        iterator it = find(r.GetFrom());
        if ( it != m_Ranges.end() && it->GetFrom() < r.GetFrom() ) {
            // keep beginning of found range
            m_Ranges.insert(it, TOpenRange(it->GetFrom(), r.GetFrom()));
        }
        // erase all covered ranges
        while ( it != m_Ranges.end() && it->GetFrom() < r.GetToOpen() ) {
            position_type pos_to_open = it->GetToOpen();
            m_Ranges.erase(it++);
            if ( pos_to_open > r.GetToOpen() ) {
                // keep end of current range
                m_Ranges.insert(it, TOpenRange(r.GetToOpen(), pos_to_open));
                break;
            }
        }
    }
    void    x_IntersectWith(const TThisType &c)
    {
        TRanges intersection_ranges;
        const_iterator my_iterator = begin(),
                       c_iterator = c.begin();
        while(my_iterator != end() && c_iterator != c.end()) {
            TRange intersection = my_iterator->IntersectionWith(*c_iterator);
            if(intersection.NotEmpty())
                intersection_ranges.insert(intersection);
            if(my_iterator->GetTo() < c_iterator->GetTo())
                ++my_iterator;
            else
                ++c_iterator;
        }
        m_Ranges = move(intersection_ranges);
    }

    void    x_Divide(const position_type& p)
    {
        iterator it = find_nc(p);
        if (it != end_nc() ) {
            position_type from_pos = it->GetFrom();
            position_type to_pos = it->GetTo();
            if ( from_pos <= p && to_pos > p) {
                m_Ranges.erase(it++);
                m_Ranges.insert(it, TRange(from_pos, p));
                m_Ranges.insert(it, TRange(p+1, to_pos));
            }
        }
    }

    void    x_CombineWith(const TThisType &c)
    {
        ITERATE(typename TThisType, it, c)   {
            x_CombineWith(*it);
        }
    }
    void    x_CombineWithAndKeepAbutting(const TThisType &c)
    {
        iterator it = find_nc(c.begin()->GetToOpen());
        ITERATE(typename TThisType, add_it, c) {
            const TRange& rr = *add_it;
            while ( it != m_Ranges.end()  && rr.GetFrom() >= it->GetFrom() ) {
                ++it;
            }
            it = m_Ranges.insert(it, rr);
            ++it;
        }
    }
    void    x_Subtract(const TThisType &c)
    {
        if ( empty() || c.empty() ) {
            return;
        }
        position_type stop_pos = GetToOpen();
        // skip till the range that may touch lower bound of r
        for ( auto it = c.find(GetFrom()); it != c.end(); ++it ) {
            if ( it->GetFrom() >= stop_pos ) {
                break;
            }
            x_Subtract(*it);
        }        
    }

protected:
    TRanges    m_Ranges;
};

END_NCBI_SCOPE

#endif  // UTIL___RANGE_SET__HPP
