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


void CHandleRange::MergeRange(TRange range, ENa_strand /*strand*/)
{
    TRange mrg = range;
    non_const_iterate ( TRanges, it, m_Ranges ) {
        // Find intersecting intervals, discard strand information
        // Also merge adjacent ranges, prevent merging whole-to + whole-from
        if ( it->first.IntersectingWith(mrg)  ||
            (it->first.GetFrom() == mrg.GetTo()+1 && !mrg.IsWholeTo()) ||
            (it->first.GetTo()+1 == mrg.GetFrom() && !mrg.IsWholeFrom()) ) {
            // Remove the intersecting interval, update the merged range.
            // We assume that WholeFrom is less than any non-whole value
            // and WholeTo is greater than any non-whole value.
            mrg += it->first;
            it = m_Ranges.erase(it);
        }
    }
    m_Ranges.push_back(TRangeWithStrand(mrg, eNa_strand_unknown));
    m_Ranges.sort();
}


CHandleRange::TRange CHandleRange::GetOverlappingRange(void) const
{
    TRange range = TRange::GetEmpty();
    iterate ( TRanges, it, m_Ranges ) {
        x_CombineRanges(range, it->first);
    }
    return range;
}


void CHandleRange::x_CombineRanges(TRange& dest, const TRange& src)
{
    if ( src.Empty() ) {
        return;
    }
    TSeqPos from = dest.GetFrom(), to = dest.GetTo();
    if ( !dest.IsWholeFrom() ) {
        if (dest.GetFrom() > src.GetFrom()  ||
            src.IsWholeFrom()  ||
            dest.IsEmptyFrom()) {
            from = src.GetFrom();
        }
    }
    if ( !dest.IsWholeTo() ) {
        if (dest.GetTo() < src.GetTo()  ||
            src.IsWholeTo()  ||
            dest.IsEmptyTo()) {
            to = src.GetTo();
        }
    }
    dest.Set(from, to);
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
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
