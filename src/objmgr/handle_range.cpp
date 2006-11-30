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
    return m_Ranges.size() > 1;
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.29  2006/11/30 18:13:46  vasilche
* Fixed GetRight() for single-strand CHandleRange.
*
* Revision 1.28  2006/01/17 17:21:14  vasilche
* Commented out asserion check incompatible with alignments.
*
* Revision 1.27  2006/01/10 20:04:33  vasilche
* Better indexing of annotations spanning several segments.
*
* Revision 1.26  2004/12/22 15:56:22  vasilche
* Explicitly mark ETotalRangeFlags values as bits.
* Allow to include origin in GetCircularRangeStart() and GetCircularRangeEnd().
* GetStrandsFlag() made non-inlined for its size.
*
* Revision 1.25  2004/12/08 16:39:37  grichenk
* Optimized total ranges in CHandleRange
*
* Revision 1.24  2004/11/05 19:29:28  grichenk
* Fixed sorting of circular features
*
* Revision 1.23  2004/08/25 21:55:25  grichenk
* Fixed ranges splitting by strand.
*
* Revision 1.22  2004/08/16 18:00:40  grichenk
* Added detection of circular locations, improved annotation
* indexing by strand.
*
* Revision 1.21  2004/05/21 21:42:12  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.20  2004/02/19 17:17:23  vasilche
* Removed unused include.
*
* Revision 1.19  2003/07/17 20:07:56  vasilche
* Reduced memory usage by feature indexes.
* SNP data is loaded separately through PUBSEQ_OS.
* String compression for SNP data.
*
* Revision 1.18  2003/06/02 16:06:38  dicuccio
* Rearranged src/objects/ subtree.  This includes the following shifts:
*     - src/objects/asn2asn --> arc/app/asn2asn
*     - src/objects/testmedline --> src/objects/ncbimime/test
*     - src/objects/objmgr --> src/objmgr
*     - src/objects/util --> src/objmgr/util
*     - src/objects/alnmgr --> src/objtools/alnmgr
*     - src/objects/flat --> src/objtools/flat
*     - src/objects/validator --> src/objtools/validator
*     - src/objects/cddalignview --> src/objtools/cddalignview
* In addition, libseq now includes six of the objects/seq... libs, and libmmdb
* replaces the three libmmdb? libs.
*
* Revision 1.17  2003/04/24 16:12:38  vasilche
* Object manager internal structures are splitted more straightforward.
* Removed excessive header dependencies.
*
* Revision 1.16  2003/03/14 19:10:41  grichenk
* + SAnnotSelector::EIdResolving; fixed operator=() for several classes
*
* Revision 1.15  2003/03/11 15:51:06  kuznets
* iterate -> ITERATE
*
* Revision 1.14  2003/02/24 18:57:22  vasilche
* Make feature gathering in one linear pass using CSeqMap iterator.
* Do not use feture index by sub locations.
* Sort features at the end of gathering in one vector.
* Extracted some internal structures and classes in separate header.
* Delay creation of mapped features.
*
* Revision 1.13  2003/02/05 17:59:17  dicuccio
* Moved formerly private headers into include/objects/objmgr/impl
*
* Revision 1.12  2003/01/23 18:26:02  ucko
* Explicitly #include <algorithm>
*
* Revision 1.11  2003/01/22 20:11:54  vasilche
* Merged functionality of CSeqMapResolved_CI to CSeqMap_CI.
* CSeqMap_CI now supports resolution and iteration over sequence range.
* Added several caches to CScope.
* Optimized CSeqVector().
* Added serveral variants of CBioseqHandle::GetSeqVector().
* Tried to optimize annotations iterator (not much success).
* Rewritten CHandleRange and CHandleRangeMap classes to avoid sorting of list.
*
* Revision 1.10  2002/12/19 20:18:01  grichenk
* Fixed test case for minus strand location
*
* Revision 1.9  2002/07/08 20:51:01  grichenk
* Moved log to the end of file
* Replaced static mutex (in CScope, CDataSource) with the mutex
* pool. Redesigned CDataSource data locking.
*
* Revision 1.8  2002/06/12 14:40:47  grichenk
* Made some methods inline
*
* Revision 1.7  2002/05/24 14:58:55  grichenk
* Fixed Empty() for unsigned intervals
* SerialAssign<>() -> CSerialObject::Assign()
* Improved performance for eResolve_None case
*
* Revision 1.6  2002/05/09 14:18:55  grichenk
* Fixed "unused variable" warnings
*
* Revision 1.5  2002/04/22 20:05:35  grichenk
* +MergeRange()
*
* Revision 1.4  2002/02/21 19:27:05  grichenk
* Rearranged includes. Added scope history. Added searching for the
* best seq-id match in data sources and scopes. Updated tests.
*
* Revision 1.3  2002/02/15 20:35:38  gouriano
* changed implementation of HandleRangeMap
*
* Revision 1.2  2002/01/23 21:59:31  grichenk
* Redesigned seq-id handles and mapper
*
* Revision 1.1  2002/01/11 19:06:19  gouriano
* restructured objmgr
*
*
* ===========================================================================
*/
