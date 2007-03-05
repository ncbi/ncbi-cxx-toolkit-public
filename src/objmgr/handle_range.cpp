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
* Author: Aleksey Grichenko, Eugene Vasilchenko
*
* File Description:
*       CHandleRange:
*       Internal class to be used instead of CSeq_loc
*       for better performance.
*
*/

#include <ncbi_pch.hpp>
#include <objmgr/impl/handle_range.hpp>

#include <algorithm>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

////////////////////////////////////////////////////////////////////
//
//
//


CHandleRange::CHandleRange(void)
    : m_TotalRanges_plus(TRange::GetEmpty()),
      m_TotalRanges_minus(TRange::GetEmpty()),
      m_IsCircular(false),
      m_IsSingleStrand(true),
      m_MoreBefore(false),
      m_MoreAfter(false)
{
}


CHandleRange::~CHandleRange(void)
{
}


CHandleRange::TTotalRangeFlags CHandleRange::GetStrandsFlag(void) const
{
    TTotalRangeFlags ret = 0;
    if ( m_Ranges.empty() ) {
        return ret;
    }
    if ( !m_IsCircular ) {
        if ( !m_TotalRanges_plus.Empty() ) {
            ret |= eStrandPlus;
        }
        if ( !m_TotalRanges_minus.Empty() ) {
            ret |= eStrandMinus;
        }
    }
    else {
        if ( x_IncludesPlus(m_Ranges.front().second) ) {
            ret |= eStrandPlus;
        }
        if ( x_IncludesMinus(m_Ranges.front().second) ) {
            ret |= eStrandMinus;
        }
    }
    return ret;
}


void CHandleRange::AddRange(TRange range, ENa_strand strand)
{
    AddRange(range, strand, false, false);
}


void CHandleRange::AddRange(TRange range, ENa_strand strand,
                            bool more_before, bool more_after)
{
    if ( !m_Ranges.empty()  &&  m_IsSingleStrand ) {
        if ( strand != m_Ranges.front().second) {
            // Different strands, the location can not be circular
            if ( m_IsCircular ) {
                // Different strands, the location can not be circular
                // Reorganize total ranges by strand
                TRange total_range = m_TotalRanges_plus;
                total_range += m_TotalRanges_minus;
                if ( x_IncludesPlus(m_Ranges.front().second) ) {
                    m_TotalRanges_plus = total_range;
                }
                else {
                    m_TotalRanges_plus = TRange::GetEmpty();
                }
                if ( x_IncludesMinus(m_Ranges.front().second) ) {
                    m_TotalRanges_minus = total_range;
                }
                else {
                    m_TotalRanges_minus = TRange::GetEmpty();
                }
                m_IsCircular = false;
            }
            m_IsSingleStrand = false;
        }
        else {
            // Same strand, but location may become circular
            if ( !m_IsCircular ) {
                // Check if location becomes circular
                if ( x_IncludesPlus(strand) ) {
                    m_IsCircular =
                        range.GetFrom() < m_Ranges.back().first.GetFrom();
                }
                else {
                    m_IsCircular =
                        range.GetFrom() > m_Ranges.back().first.GetFrom();
                }
                if ( m_IsCircular ) {
                    // Reorganize total ranges.
                    // First part (everything already collected)
                    // goes to m_TotalRanges_plus,
                    // second part (all new ranges)
                    // will go to m_TotalRanges_minus.

                    // Verify that until now all ranges are on the same strand.
                    _ASSERT(m_TotalRanges_plus.Empty() ||
                            m_TotalRanges_minus.Empty() ||
                            m_TotalRanges_plus == m_TotalRanges_minus);
                    m_TotalRanges_plus += m_TotalRanges_minus;
                    m_TotalRanges_minus = TRange::GetEmpty();
                }
                else {
                    _ASSERT(m_IsSingleStrand && !m_IsCircular);
                    _ASSERT(!m_Ranges.empty());
                    //_ASSERT(more_before);
                    //_ASSERT(m_MoreAfter);
                    m_MoreAfter = more_after;
                }
            }
        }
    }
    else {
        m_MoreBefore = more_before;
        m_MoreAfter = more_after;
    }
    m_Ranges.push_back(TRanges::value_type(range, strand));
    if ( !m_IsCircular ) {
        // Regular location
        if ( x_IncludesPlus(strand) ) {
            m_TotalRanges_plus += range;
        }
        if ( x_IncludesMinus(strand) ) {
            m_TotalRanges_minus += range;
        }
    }
    else {
        // Circular location, second part
        m_TotalRanges_minus += range;
    }
}


void CHandleRange::AddRanges(const CHandleRange& hr)
{
    ITERATE ( CHandleRange, it, hr ) {
        AddRange(it->first, it->second);
    }
}


bool CHandleRange::x_IntersectingStrands(ENa_strand str1, ENa_strand str2)
{
    return
        str1 == eNa_strand_unknown // str1 includes anything
        ||
        str2 == eNa_strand_unknown // str2 includes anything
        ||
        str1 == str2;              // accept only equal strands
    //### Not sure about "eNa_strand_both includes eNa_strand_plus" etc.
}


bool CHandleRange::IntersectingWith(const CHandleRange& hr) const
{
    if ( !m_TotalRanges_plus.IntersectingWith(hr.m_TotalRanges_plus) &&
         !m_TotalRanges_minus.IntersectingWith(hr.m_TotalRanges_minus) ) {
        return false;
    }
    ITERATE(TRanges, it1, m_Ranges) {
        ITERATE(TRanges, it2, hr.m_Ranges) {
            if ( it1->first.IntersectingWith(it2->first) ) {
                if ( x_IntersectingStrands(it1->second, it2->second) ) {
                    return true;
                }
            }
        }
    }
    return false;
}


void CHandleRange::MergeRange(TRange range, ENa_strand /*strand*/)
{
    for ( TRanges::iterator it = m_Ranges.begin(); it != m_Ranges.end(); ) {
        // Find intersecting intervals, discard strand information
        // Also merge adjacent ranges, prevent merging whole-to + whole-from
        if ( !it->first.Empty() &&
             (it->first.IntersectingWith(range) ||
              it->first.GetFrom() == range.GetToOpen() ||
              it->first.GetToOpen() == range.GetFrom()) ) {
            // Remove the intersecting interval, update the merged range.
            // We assume that WholeFrom is less than any non-whole value
            // and WholeTo is greater than any non-whole value.
            range += it->first;
            it = m_Ranges.erase(it);
        }
        else {
            ++it;
        }
    }
    AddRange(range, eNa_strand_unknown);
}


TSeqPos CHandleRange::GetLeft(void) const
{
    if ( !m_IsCircular ) {
        // Since empty ranges have extremely large 'from' coordinate it's
        // ok to simply return min of 'from' coordinates.
        return min(m_TotalRanges_plus.GetFrom(), m_TotalRanges_minus.GetFrom());
    }
    return IsReverse(m_Ranges.front().second) ?
        m_TotalRanges_minus.GetFrom() : m_TotalRanges_plus.GetFrom();
}


TSeqPos CHandleRange::GetRight(void) const
{
    if ( !m_IsCircular ) {
        // A bit more logic is required to check empty ranges.
        if ( m_TotalRanges_minus.Empty() ) {
            return m_TotalRanges_plus.GetTo();
        }
        else if ( m_TotalRanges_plus.Empty() ) {
            return m_TotalRanges_minus.GetTo();
        }
        else {
            return max(m_TotalRanges_plus.GetTo(), m_TotalRanges_minus.GetTo());
        }
    }
    return IsReverse(m_Ranges.front().second) ?
        m_TotalRanges_plus.GetTo() : m_TotalRanges_minus.GetTo();
}


CHandleRange::TRange
CHandleRange::GetOverlappingRange(TTotalRangeFlags flags) const
{
    TRange ret = TRange::GetEmpty();
    if (m_IsCircular) {
        ETotalRangeFlags circular_strand =
            IsReverse(m_Ranges.front().second) ?
            eStrandMinus : eStrandPlus;
        if (flags & circular_strand) {
            ret = TRange::GetWhole();
        }
        return ret;
    }
    if (flags & eStrandPlus) {   // == eCircularStart
        ret += m_TotalRanges_plus;
    }
    if (flags & eStrandMinus) {  // == eCircularEnd
        ret += m_TotalRanges_minus;
    }
    if ( m_IsSingleStrand && (m_MoreBefore || m_MoreAfter) ) {
        _ASSERT(!m_Ranges.empty());
        if ( x_IncludesPlus(m_Ranges.front().second) ) {
            if ( (flags & eStrandPlus) ) {
                if ( m_MoreBefore ) {
                    ret.SetFrom(TRange::GetWholeFrom());
                }
                if ( m_MoreAfter ) {
                    ret.SetTo(TRange::GetWholeTo());
                }
            }
        }
        else {
            if ( (flags & eStrandMinus) ) {
                if ( m_MoreAfter ) {
                    ret.SetFrom(TRange::GetWholeFrom());
                }
                if ( m_MoreBefore ) {
                    ret.SetTo(TRange::GetWholeTo());
                }
            }
        }
    }
    return ret;
}


CHandleRange::TRange
CHandleRange::GetCircularRangeStart(bool include_origin) const
{
    _ASSERT(m_IsCircular);
    TRange ret = m_TotalRanges_plus;
    if ( include_origin ) {
        // Adjust start/stop to include cut point
        if ( !IsReverse(m_Ranges.front().second) ) {
            // Include end
            ret.SetTo(TRange::GetWholeTo());
        }
        else {
            // Include start
            ret.SetFrom(TRange::GetWholeFrom());
        }
    }
    return ret;
}


CHandleRange::TRange
CHandleRange::GetCircularRangeEnd(bool include_origin) const
{
    _ASSERT(m_IsCircular);
    TRange ret = m_TotalRanges_minus;
    if ( include_origin ) {
        // Adjust start/stop to include cut point
        if ( !IsReverse(m_Ranges.front().second) ) {
            // Include end
            ret.SetFrom(TRange::GetWholeFrom());
        }
        else {
            // Include start
            ret.SetTo(TRange::GetWholeTo());
        }
    }
    return ret;
}


CHandleRange::TRange
CHandleRange::GetOverlappingRange(const TRange& range) const
{
    TRange ret = TRange::GetEmpty();
    if ( !range.Empty() ) {
        ITERATE ( TRanges, it, m_Ranges ) {
            ret += it->first.IntersectionWith(range);
        }
    }
    return ret;
}


bool CHandleRange::IntersectingWith(const TRange& range,
                                    ENa_strand strand) const
{
    if ( !range.Empty() ) {
        ITERATE ( TRanges, it, m_Ranges ) {
            if ( range.IntersectingWith(it->first) &&
                 x_IntersectingStrands(strand, it->second) ) {
                return true;
            }
        }
    }
    return false;
}


bool CHandleRange::HasGaps(void) const
{
    return m_Ranges.size() > 1  ||  m_MoreBefore  ||  m_MoreAfter;
}


END_SCOPE(objects)
END_NCBI_SCOPE
