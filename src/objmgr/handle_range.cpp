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
* Author: Aleksey Grichenko
*
* File Description:
*       CHandleRange:
*       Internal class to be used instead of CSeq_loc
*       for better performance.
*
*/

#include "handle_range.hpp"


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

////////////////////////////////////////////////////////////////////
//
//
//


CHandleRange::CHandleRange(void)
{
}


CHandleRange::CHandleRange(const CHandleRange& hr)
{
    *this = hr;
}


CHandleRange::~CHandleRange(void)
{
}


CHandleRange& CHandleRange::operator=(const CHandleRange& hr)
{
    m_Ranges = hr.m_Ranges;
    return *this;
}


void CHandleRange::AddRange(TRange range, ENa_strand strand)
{
    m_Ranges.push_back(TRanges::value_type(range, strand));
    sort(m_Ranges.begin(), m_Ranges.end());
}


void CHandleRange::AddRanges(const CHandleRange& hr)
{
    iterate ( CHandleRange, it, hr ) {
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
    //if ( hloc.m_Handle0 != m_Handle0 )
    //    return false;
    //### Optimize this
    iterate ( TRanges, it1, hr.m_Ranges ) {
        if ( it1->first.Empty() )
            continue;
        iterate ( TRanges, it2, m_Ranges ) {
            if ( it2->first.Empty() )
                continue;
            if ( it2->first.GetFrom() >= it1->first.GetToOpen() )
                break;
            if ( x_IntersectingStrands(it1->second, it2->second)  &&
                it1->first.IntersectingWith(it2->first)) {
                return true;
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


CHandleRange::TRange CHandleRange::GetOverlappingRange(void) const
{
    return Empty() ?
        TOpenRange::GetEmpty() :
        TOpenRange(m_Ranges.front().first.GetFrom(),
                   m_Ranges.back().first.GetToOpen());
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
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
