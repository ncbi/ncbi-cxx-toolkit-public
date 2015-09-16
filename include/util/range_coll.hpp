#ifndef UTIL___RANGE_COLL__HPP
#define UTIL___RANGE_COLL__HPP

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
 * Authors:  Andrey Yazhuk
 *
 * File Description:
 *  class CRangeCollection - sorted collection of non-overlapping CRange-s
 */

#include <corelib/ncbistd.hpp>
#include <corelib/ncbistl.hpp>

#include <util/range.hpp>

#include <algorithm>

BEGIN_NCBI_SCOPE

template<class Range, class Position>
struct PRangeLessPos
{
    bool    operator()(const Range &R, Position Pos)     { return R.GetToOpen() <= Pos;  }    
    bool    operator()(Position Pos, const Range &R)     { return Pos < R.GetToOpen();  }    
    bool    operator()(const Range &R1, const Range &R2) { return R1.GetToOpen() < R2.GetToOpen();  }    
};
    
///////////////////////////////////////////////////////////////////////////////
// class CRangeCollection<Position> represent a sorted collection of 
// CRange<Position>. It is guaranteed that ranges in collection do not overlap.
// Unless DivideAt() was explicitly called, it is also guarantees that they
// aren't adjacent so that there is no gap beween them.
// Adding an interval to the collection leads to possible merging it with 
// existing intervals.

template<class Position>
class CRangeCollection 
{
public:
    typedef Position    position_type;
    typedef CRangeCollection<position_type>  TThisType;

    typedef CRange<position_type>    TRange;
    typedef vector<TRange>      TRangeVector;    
    typedef typename TRangeVector::const_iterator    const_iterator;
    typedef typename TRangeVector::const_reverse_iterator    const_reverse_iterator;
    typedef typename TRangeVector::size_type size_type;
    
    CRangeCollection()   { }
    explicit CRangeCollection(const TRange& r)
    {
        m_vRanges.push_back(r);
    }
    CRangeCollection(const CRangeCollection &c)
    {
        m_vRanges = c.m_vRanges;
    }
    
    // immitating vector, but providing only "const" access to elements
    const_iterator  begin() const   
    {   
        return m_vRanges.begin();   
    }
    const_iterator  end() const     
    {   
        return m_vRanges.end(); 
    }
    const_reverse_iterator  rbegin() const  
    {   
        return m_vRanges.rbegin();  
    }
    const_reverse_iterator  rend() const    
    {   
        return m_vRanges.rend();    
    }
    size_type size() const  
    {   
        return m_vRanges.size();    
    }
    bool    empty() const   
    {   
        return m_vRanges.empty();   
    }
    const   TRange& operator[](size_type pos)   const   
    {  
        return m_vRanges[pos];  
    }
    void    clear()
    {
        m_vRanges.clear();
    }
    // returns iterator pointing to the TRange that has ToOpen > pos
    const_iterator  find(position_type pos)   const
    {
        PRangeLessPos<TRange, position_type> p;
        return lower_bound(begin(), end(), pos, p);
    }
    position_type   GetFrom() const
    {
        if(! m_vRanges.empty())
            return begin()->GetFrom();
        else return TRange::GetEmptyFrom();
    }
    position_type   GetToOpen() const
    {
        if(! m_vRanges.empty())
            return rbegin()->GetToOpen();
        else return TRange::GetEmptyToOpen();
    }
    position_type   GetTo() const
    {
        if(! m_vRanges.empty())
            return rbegin()->GetTo();
        else return TRange::GetEmptyTo();
    }
    bool            Empty() const
    {
        return empty();
    } 
    bool            NotEmpty() const
    {
        return ! empty();
    }
    position_type   GetLength (void) const
    {
       if(! m_vRanges.empty())  {
            position_type From = begin()->GetFrom();
            return rbegin()->GetToOpen() - From;
       } else return 0;
    }
    ///
    /// Returns total length covered by ranges in this collection, i.e.
    /// sum of lengths of ranges
    ///
    position_type   GetCoveredLength (void) const
    {
        position_type length = 0;
        ITERATE(typename TThisType, it, m_vRanges)   {
            length += it->GetLength();
        }
        return length;
    }
    TRange          GetLimits() const
    {
        if(! m_vRanges.empty())  {
            position_type From = begin()->GetFrom();
            position_type To = rbegin()->GetTo();
            return TRange(From, To);
        } else return TRange::GetEmpty();
    }
    TThisType&  IntersectWith (const TRange& r)
    {
         x_IntersectWith(r);
         return *this;
    }
    TThisType &  operator &= (const TRange& r)
    {
         x_IntersectWith(r);
         return *this;
    }
    bool    operator == (const TThisType& c) const
    {
         return x_Equals(c);         
    }
    bool  IntersectingWith (const TRange& r) const
    {
        return x_Intersects(r).second;
    }
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
        x_CombineWith(r);
        return *this;
    }
    TThisType&  Subtract(const TRange& r)
    {
        x_Subtract(r);
        return *this;
    }
    TThisType &  operator-= (const TRange& r)
    {
       x_Subtract(r);
       return *this;
    }    
    TThisType&  IntersectWith (const TThisType &c)
    {
        x_IntersectWith(c);
        return *this;
    }
    TThisType &  operator&= (const TThisType &c)
    {
        x_IntersectWith(c);
        return *this;
    }
    TThisType&  CombineWith (const TThisType &c)
    {
        x_CombineWith(c);
        return *this;
    }
    TThisType &  operator+= (const TThisType &c)
    {
        x_CombineWith(c);
        return *this;
    }
    TThisType&  Subtract(const TThisType &c)
    {
        x_Subtract(c);
        return *this;
    }
    TThisType &  operator-= (const TThisType &c)
    {
       x_Subtract(c);
       return *this;
    }    

    /// If position is in middle of range, divide into two consecutive ranges
    /// after this position
    TThisType & DivideAfter(const position_type &p)
    {
       x_Divide(p);
       return *this;
    }

protected:
    typedef typename TRangeVector::iterator    iterator;
    typedef typename TRangeVector::reverse_iterator    reverse_iterator;

    iterator  begin_nc()   
    {   
        return m_vRanges.begin();   
    }
    iterator  end_nc()
    {   
        return m_vRanges.end(); 
    }    
    iterator find_nc(position_type pos) {
        PRangeLessPos<TRange, position_type> p;
        return lower_bound(begin_nc(), end_nc(), pos, p);
    }        
    pair<const_iterator, bool>    x_Find(position_type pos) const
    {
        PRangeLessPos<TRange, position_type>    p;
        const_iterator it = lower_bound(begin(), end(), pos, p);
        bool b_contains = it != end()  &&  it->GetFrom() >= pos;
        return make_pair(it, b_contains);
    }
    pair<const_iterator, bool>    x_Intersects(const TRange& r) const
    {
        PRangeLessPos<TRange, position_type>    p;
        const_iterator it = lower_bound(begin(), end(), r.GetFrom(), p);
        bool b_intersects = it != end()  &&  it->GetFrom() < r.GetToOpen();
        return make_pair(it, b_intersects);
    }
    pair<const_iterator, bool>    x_Contains(const TRange& r) const
    {
        PRangeLessPos<TRange, position_type>    p;
        const_iterator it = lower_bound(begin(), end(), r.GetFrom(), p);
        bool contains = false;
        if (it != end()) {
            contains = (it->GetFrom() <= r.GetFrom()  &&
                        it->GetTo()   >= r.GetTo());
        }
        return make_pair(it, contains);
    }
   
    void    x_IntersectWith(const TRange& r)
    {
        PRangeLessPos<TRange, position_type>    p;

        position_type pos_to = r.GetTo();
        iterator it_right = lower_bound(begin_nc(), end_nc(), pos_to, p);
        if(it_right != end_nc()) {
            if(it_right->GetFrom() <= pos_to) {   //intersects
                it_right->SetTo(pos_to);
                ++it_right; // exclude from removal
            }
            m_vRanges.erase(it_right, end_nc()); // erase ranges to the right
        }

        position_type pos_from = r.GetFrom();
        iterator it_left =
            lower_bound(begin_nc(), end_nc(), pos_from, p);
        if(it_left != end_nc()) {        
            if(it_left->GetFrom() < pos_from)    
                it_left->SetFrom(pos_from);
        }
        m_vRanges.erase(begin_nc(), it_left); //erase ranges to the left
    }

    // returns iterator to the range representing result of combination
    iterator    x_CombineWith(const TRange& r)
    {
        PRangeLessPos<TRange, position_type>    p;

        position_type pos_from = r.GetFrom();
        position_type pos_to_open = r.GetToOpen();                    

        if (pos_from == TRange::GetWholeFrom()) {
            pos_from++; // Prevent overflow
        }
        iterator it_begin_m =
            lower_bound(begin_nc(), end_nc(), pos_from - 1, p); /* NCBI_FAKE_WARNING: WorkShop */
        if(it_begin_m != end_nc() && it_begin_m->GetFrom() <= pos_to_open)  { // intersection
            iterator it_end_m = lower_bound(it_begin_m, end_nc(), pos_to_open, p);
            it_begin_m->CombineWith(r);

            if(it_end_m != end_nc()  &&  it_end_m->GetFrom() <= pos_to_open) {// subject to merge
                it_begin_m->SetToOpen(it_end_m->GetToOpen()); 
                ++it_end_m; // including it into erased set
            }
            m_vRanges.erase(it_begin_m + 1, it_end_m); 
        } else {
            m_vRanges.insert(it_begin_m, r);
        }
        return it_begin_m;
    }

    void    x_Subtract(const TRange& r)
    {
        PRangeLessPos<TRange, position_type>    P;

        position_type pos_from = r.GetFrom();
        position_type pos_to_open = r.GetToOpen();

        iterator it_begin_e = lower_bound(begin_nc(), end_nc(), pos_from, P);
        if(it_begin_e != end_nc()) {   // possible intersection
            
            if(it_begin_e->GetFrom() < pos_from  &&  it_begin_e->GetToOpen() > pos_to_open)    { 
                //it_begin_e contains R, split it in two
                TRange it_r = *it_begin_e;
                it_begin_e = m_vRanges.insert(it_begin_e, it_r);
                it_begin_e->SetToOpen(pos_from);
                (++it_begin_e)->SetFrom(pos_to_open);
            } else  {
                if(it_begin_e->GetFrom() < pos_from) { // cut this segement , but don't erase
                    it_begin_e->SetToOpen(pos_from);
                    ++it_begin_e;
                }
                iterator it_end_e = lower_bound(it_begin_e, end_nc(), pos_to_open, P);
                if(it_end_e != end_nc()  &&  it_end_e->GetFrom() < pos_to_open)    { 
                    it_end_e->SetFrom(pos_to_open); // truncate
                }
                // erase ranges fully covered by R
                m_vRanges.erase(it_begin_e, it_end_e); 
            }
        } 
    }
    void    x_IntersectWith(const TThisType &c)
    {
        TRangeVector intersection_ranges;
        const_iterator my_iterator = begin(),
                       c_iterator = c.begin();
        while(my_iterator != end() && c_iterator != c.end())
        {
	    TRange intersection = my_iterator->IntersectionWith(*c_iterator);
            if(intersection.NotEmpty())
                intersection_ranges.push_back(intersection);
            if(my_iterator->GetTo() < c_iterator->GetTo())
                ++my_iterator;
            else
                ++c_iterator;
        }
        m_vRanges = intersection_ranges;
    }

    void    x_Divide(const position_type& p)
    {
        PRangeLessPos<TRange, position_type>    P;
        iterator it = lower_bound(begin_nc(), end_nc(), p, P);
        if (it != end_nc() && it->GetFrom() <= p && it->GetTo() > p) {
            position_type end_pos = it->GetTo();
            it->SetTo(p);
            m_vRanges.insert(++it, TRange(p+1, end_pos));
        }
    }

    void    x_CombineWith(const TThisType &c)
    {
        ITERATE(typename TThisType, it, c)   {
            x_CombineWith(*it);
        }
    }
    void    x_Subtract(const TThisType &c)
    {
        ITERATE(typename TThisType, it, c)   {
            x_Subtract(*it);
        }
    }
    bool    x_Equals(const TThisType &c) const
    {
        if(&c == this)  {
            return true;
        } else {
            return m_vRanges == c.m_vRanges;
        }
    }

protected:
    TRangeVector    m_vRanges;  
};

END_NCBI_SCOPE

#endif  // UTIL___RANGE_COLL__HPP
