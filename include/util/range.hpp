#ifndef RANGE__HPP
#define RANGE__HPP

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
*   CRange<> class represents interval
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.3  2001/01/05 20:08:53  vasilche
* Added util directory for various algorithms and utility classes.
*
* Revision 1.2  2001/01/03 17:24:52  vasilche
* Fixed typo.
*
* Revision 1.1  2001/01/03 16:39:18  vasilche
* Added CAbstractObjectManager - stub for object manager.
* CRange extracted to separate file.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbi_limits.hpp>

BEGIN_NCBI_SCOPE

// range
template<class Position>
class CRange
{
public:
    typedef Position position_type;
    typedef CRange<Position> TThisType;

    // constructors
    CRange(void)
        {
        }
    CRange(position_type from, position_type to)
        : m_From(from), m_To(to)
        {
        }
    
    // parameters
    position_type GetFrom(void) const
        {
            return m_From;
        }
    position_type GetTo(void) const
        {
            return m_To;
        }

    // state
    bool HaveInfiniteBound(void) const
        {
            return GetFrom() == GetWholeFrom() || GetTo() == GetWholeTo();
        }
    bool HaveEmptyBound(void) const
        {
            return GetFrom() == GetEmptyFrom() || GetTo() == GetEmptyTo();
        }
    bool Empty(void) const
        {
            return HaveEmptyBound() || GetTo() < GetFrom();
        }
    bool Regular(void) const
        {
            return !Empty() && !HaveInfiniteBound();
        }
    // return length of regular region
    position_type GetLength(void) const
        {
            return GetTo() - GetFrom() + 1;
        }

    // modifiers
    TThisType& SetFrom(position_type from)
        {
            m_From = from;
            return *this;
        }
    TThisType& SetTo(position_type to)
        {
            m_To = to;
            return *this;
        }
    TThisType& SetLength(position_type length)
        {
            SetTo(GetFrom() + length - 1);
            return *this;
        }
    TThisType& SetLengthDown(position_type length)
        {
            SetFrom(GetTo() - length + 1);
            return *this;
        }

    // comparison
    bool operator==(TThisType range) const
        {
            return GetFrom() == range.GetFrom() && GetTo() == range.GetTo();
        }
    bool operator!=(TThisType range) const
        {
            return !(*this == range);
        }
    bool operator<(TThisType range) const
        {
            return GetFrom() < range.GetFrom() ||
                GetFrom() == range.GetFrom() && GetTo() < range.GetTo();
        }
    bool operator<=(TThisType range) const
        {
            return GetFrom() < range.GetFrom() ||
                GetFrom() == range.GetFrom() && GetTo() <= range.GetTo();
        }
    bool operator>(TThisType range) const
        {
            return GetFrom() > range.GetFrom() ||
                GetFrom() == range.GetFrom() && GetTo() > range.GetTo();
        }
    bool operator>=(TThisType range) const
        {
            return GetFrom() > range.GetFrom() ||
                GetFrom() == range.GetFrom() && GetTo() >= range.GetTo();
        }

    // check if intersected when ranges may be empty
    bool IntersectingWithPossiblyEmpty(TThisType range) const
        {
            if ( GetFrom() <= range.GetFrom() )
                return GetTo() >= range.GetFrom() && !range.Empty();
            else
                return GetFrom() <= range.GetTo() && !Empty();
        }
    // check if intersected when ranges are not empty
    bool IntersectingWith(TThisType range) const
        {
            if ( GetFrom() <= range.GetFrom() )
                return GetTo() >= range.GetFrom();
            else
                return GetFrom() <= range.GetTo();
        }

    // special values
    static position_type GetPositionMin(void)
        {
            return numeric_limits<position_type>::min();
        }
    static position_type GetPositionMax(void)
        {
            return numeric_limits<position_type>::max();
        }

    // whole range
    static position_type GetWholeFrom(void)
        {
            return GetPositionMin();
        }
    static position_type GetWholeTo(void)
        {
            return GetPositionMax();
        }
    static position_type GetWholeLength(void)
        {
            return GetPositionMax();
        }
    static TThisType GetWhole(void)
        {
            return TThisType(GetWholeFrom(), GetWholeTo());
        }
    bool IsWholeFrom(void) const
        {
            return GetFrom() == GetWholeFrom();
        }
    bool IsWholeTo(void) const
        {
            return GetTo() == GetWholeTo();
        }

    // empty range
    static position_type GetEmptyFrom(void)
        {
            return GetPositionMax();
        }
    static position_type GetEmptyTo(void)
        {
            return GetPositionMin();
        }
    static position_type GetEmptyLength(void)
        {
            return 0;
        }
    static TThisType GetEmpty(void)
        {
            return TThisType(GetEmptyFrom(), GetEmptyTo());
        }
    bool IsEmptyFrom(void) const
        {
            return GetFrom() == GetEmptyFrom();
        }
    bool IsEmptyTo(void) const
        {
            return GetTo() == GetEmptyTo();
        }

    // combine ranges
    TThisType& CombineFrom(position_type from)
        {
            if ( from <= GetFrom() ) {
                // from?
                if ( from == GetWholeFrom() )
                    return *this; // not
            }
            else {
                // GetFrom()?
                if ( !IsWholeFrom() )
                    return *this; // yes
            }
            return SetFrom(from);
        }
    TThisType& CombineTo(position_type to)
        {
            if ( to <= GetTo() ) {
                // to?
                if ( to == GetWholeTo() )
                    return *this; // not
            }
            else {
                // GetTo()?
                if ( !IsWholeTo() )
                    return *this; // yes
            }
            return SetTo(to);
        }
    TThisType& operator+=(TThisType range)
        {
            if ( !range.Empty() ) {
                if ( Empty() )
                    *this = range;
                else {
                    CombineFrom(range.GetFrom());
                    CombineTo(range.GetTo());
                }
            }
            return *this;
        }

private:
    position_type m_From, m_To;
};

//#include <util/range.inl>

END_NCBI_SCOPE

#endif  /* RANGE__HPP */
