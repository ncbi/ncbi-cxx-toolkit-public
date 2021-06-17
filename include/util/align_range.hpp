#ifndef UTIL___ALIGN_RANGE__HPP
#define UTIL___ALIGN_RANGE__HPP

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
* Author: Andrey Yazhuk
*
* File Description:
*   
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbi_limits.hpp>


/** @addtogroup RangeSupport
 *
 * @{
 */

#include <util/range.hpp>

BEGIN_NCBI_SCOPE


///////////////////////////////////////////////////////////////////////////////
/// CAlignRange
/// Represents an element of pairwise alignment of two sequences.
/// CAlignRange := [from_1, to_open_1), [from_2, to_open_2), direction
/// where:  
///         from_1 <= to_open_1, from_2 <= to_open_2
///         (to_open_1 - from_1) == (to_open_2 - from_2)
/// mapping:
///         if direct       from_1 -> from_2,  to_1 -> to_2
///         if reversed     from_1 -> to_2,    to_1 -> from_2

template<class Position> class CAlignRange
{
public:
    typedef Position    position_type;
    typedef CAlignRange<Position>   TThisType;
    typedef CRange<Position>        TRange;

    enum    EFlags  {
        fReversed = 0x01, // Second reversed compared to the first.
        fFirstRev = 0x02  // First is on minus strand.
    };

    CAlignRange(void)
    :   m_FirstFrom(GetEmptyFrom()), 
        m_SecondFrom(GetEmptyFrom()),        
        m_Length(GetEmptyLength()),
        m_Flags(0)
    {
    }

    CAlignRange(position_type first_from,
                position_type second_from,
                position_type len,
                bool direct = true,
                bool first_direct = true)
    :   m_FirstFrom(first_from),
        m_SecondFrom(second_from),
        m_Length(len),
        m_Flags(0)
    {
        SetDirect(direct);
        SetFirstDirect(first_direct);
    }

    bool IsDirect() const {
        return (m_Flags & fReversed) == 0;
    }
    bool IsReversed() const {
        return (m_Flags & fReversed) != 0;
    }
    bool IsFirstDirect() const {
        return (m_Flags & fFirstRev) == 0;
    }
    bool IsFirstReversed() const {
        return (m_Flags & fFirstRev) != 0;
    }

    position_type GetFirstFrom(void) const
    {
        return m_FirstFrom;
    }
    position_type GetFirstToOpen(void) const
    {
        return m_FirstFrom + m_Length;
    }
    position_type GetFirstTo(void) const
    {
        return GetFirstToOpen() - 1;
    }
    position_type GetSecondFrom(void) const
    {
        return m_SecondFrom;
    }
    position_type GetSecondToOpen(void) const
    {
        return m_SecondFrom + m_Length;
    }
    position_type GetSecondTo(void) const
    {
        return GetSecondToOpen() - 1;
    }
    TRange GetFirstRange()  const
    {
        return TRange(GetFirstFrom(), GetFirstTo());
    }
    TRange GetSecondRange()  const
    {
        return TRange(GetSecondFrom(), GetSecondTo());
    }
    bool Empty(void) const
    {
        return m_Length <= 0;
    }
    bool NotEmpty(void) const
    {
        return m_Length > 0;
    }
    // return length of regular region
    position_type GetLength(void) const
    {
        return m_Length;
    }
    TThisType& SetFirstFrom(position_type from)
    {
        m_FirstFrom = from;
        return *this;
    }
    TThisType& SetSecondFrom(position_type second_from)
    {
        m_SecondFrom = second_from;
        return *this;
    }
    TThisType& SetLength(position_type len)
    {
        m_Length = len;
        return *this;
    }
    TThisType& Set(position_type first_from, position_type second_from, 
                   position_type len)
    {
        return SetFirstFrom(first_from).SetSecondFrom(second_from).SetLength(len);
    }
    void SetDirect(bool direct = true)
    {
        SetReversed(!direct);
    }
    void SetReversed(bool reversed = true)
    {
        if (reversed) {
            m_Flags |= fReversed;
        }
        else {
            m_Flags &= ~fReversed;
        }
    }
    void SetFirstDirect(bool direct = true)
    {
        SetFirstReversed(!direct);
    }
    void SetFirstReversed(bool reversed = true)
    {
        if (reversed) {
            m_Flags |= fFirstRev;
        }
        else {
            m_Flags &= ~fFirstRev;
        }
    }
    bool operator==(const TThisType& r) const
    {
        return GetFirstFrom() == r.GetFirstFrom()  &&
               GetSecondFrom() == r.GetSecondFrom()  &&
               GetLength() == r.GetLength() &&
               m_Flags == r.m_Flags;
    }
    bool operator!=(const TThisType& r) const
    {
        return !(*this == r);
    }
    position_type GetSecondPosByFirstPos(position_type pos) const
    {
        if(FirstContains(pos))    {
            int off = pos - m_FirstFrom;            
            if(IsReversed())    {
                return GetSecondTo() - off;
            } else {
                return m_SecondFrom + off;
            }
        } else {
            return -1;
        }
    }
    position_type GetFirstPosBySecondPos(position_type pos) const
    {
        if(SecondContains(pos)) {
            int off = IsReversed() ? (GetSecondTo() - pos) : (pos - m_SecondFrom);
            return m_FirstFrom + off;
        } else {
            return -1;
        }
    }
    bool    FirstContains(position_type pos) const
    {
        return pos >= m_FirstFrom  &&  pos < GetFirstToOpen();
    }
    bool    SecondContains(position_type pos) const
    {
        return pos >= m_SecondFrom  &&  pos < GetSecondToOpen();
    }
    /// Intersection
    TThisType IntersectionWith(const CRange<position_type>& r) const
    {
        TThisType al_r(*this);
        al_r.IntersectWith(r);
        return al_r;
    }
    TThisType& IntersectWith(const CRange<position_type>& r)
    {
        int new_from = max(GetFirstFrom(), r.GetFrom());
        int new_to = min(GetFirstTo(), r.GetTo());
        if(new_to < new_from)  {
            new_to = new_from - 1;
        }
        if(IsReversed())    {
            m_SecondFrom += GetFirstTo() - new_to; 
        } else {
            m_SecondFrom += new_from - GetFirstFrom();
        }
       
        m_FirstFrom = new_from;
        m_Length = new_to - new_from + 1;
        return *this;
    }
    bool IntersectingWith(const CRange<position_type>& r) const
    {
        return ! (this->GetFirstFrom() > r.GetTo()
                  ||  r.GetFrom() > this->GetFirstTo());
    }
    bool    IsAbutting(const TThisType& r) const
    {
        if(IsDirect() == r.IsDirect()  &&  GetLength() >= 0  && r.GetLength() >= 0)    { 
            const TThisType *r_1 = this, *r_2 = &r;
            if(r_1->GetFirstFrom() > r_2->GetFirstFrom()  ||  
                r_1->GetFirstToOpen() > r_2->GetFirstToOpen())  {
                swap(r_1, r_2); // reorder by from_1
            }
            if(r_1->GetFirstToOpen() == r_2->GetFirstFrom())    {
                return IsDirect()   ?   r_1->GetSecondToOpen() == r_2->GetSecondFrom()   
                                    :   r_1->GetSecondFrom() == r_2->GetSecondToOpen();
            }
        }
        return false;
    }
    TThisType&  CombineWithAbutting(const TThisType& r)
    {
        _ASSERT(IsAbutting(r)); 

        m_Length += r.GetLength();                    
        if(GetFirstFrom() <= r.GetFirstFrom()  &&  GetFirstToOpen() <= r.GetFirstToOpen())  {
            if(IsReversed())   {        
                SetSecondFrom(r.GetSecondFrom());            
            }
        } else {
            SetFirstFrom(r.GetFirstFrom());                
            if(IsDirect()) {
                SetSecondFrom(r.GetSecondFrom());
            }
        }
        return *this;
    }
    TThisType CombinationWithAbutting(const TThisType& r) const
    {
        TThisType al_r(*this);
        al_r.CombineWithAbutting(r);
        return al_r;
    }
    static position_type GetEmptyFrom(void)
    {
        return GetPositionMax();
    }
    static position_type GetEmptyToOpen(void)
    {
        return GetPositionMax();
    }
    static position_type GetEmptyTo(void)
    {
        return GetEmptyToOpen()-1;
    }
    static position_type GetEmptyLength(void)
    {
        return 0;
    }
    static TThisType GetEmpty(void)
    {
        return TThisType();
    }
    static position_type GetPositionMin(void)
    {
        return numeric_limits<position_type>::min();
    }
    static position_type GetPositionMax(void)
    {
        return numeric_limits<position_type>::max();
    }

private:
    position_type   m_FirstFrom;     /// start 
    position_type   m_SecondFrom;   /// start on the aligned sequence
    position_type   m_Length; /// length of the segment
    int             m_Flags;
};

/* @} */

END_NCBI_SCOPE

#endif  /* UTIL___ALIGN_RANGE__HPP */
