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
*   CHandle_Range_Map is a substitute for seq-loc to make searching
*   over locations more effective.
*
*/

#include "handle_range_map.hpp"
#include <objects/objmgr/seq_id_mapper.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Seq_point.hpp>
#include <objects/seqloc/Seq_bond.hpp>
#include <objects/seqloc/Seq_loc_equiv.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


////////////////////////////////////////////////////////////////////
//
//  CHandleRangeMap::
//


CHandleRangeMap::CHandleRangeMap(CSeq_id_Mapper& id_mapper)
    : m_IdMapper(&id_mapper)
{
}


CHandleRangeMap::CHandleRangeMap(const CHandleRangeMap& rmap)
{
    *this = rmap;
}


CHandleRangeMap::~CHandleRangeMap(void)
{
}


CHandleRangeMap& CHandleRangeMap::operator= (const CHandleRangeMap& rmap)
{
    m_IdMapper = rmap.m_IdMapper;
    m_LocMap = rmap.m_LocMap;
    return *this;
}


void CHandleRangeMap::AddLocation(const CSeq_loc& loc)
{
    switch ( loc.Which() ) {
    case CSeq_loc::e_not_set:
    case CSeq_loc::e_Null:
    case CSeq_loc::e_Empty:
        {
            return;
        }
    case CSeq_loc::e_Whole:
        {
            AddRange(loc.GetWhole(), CHandleRange::TRange::GetWhole());
            return;
        }
    case CSeq_loc::e_Int:
        {
            AddRange(loc.GetInt().GetId(),
                     loc.GetInt().GetFrom(),
                     loc.GetInt().GetTo(),
                     loc.GetInt().GetStrand());
            return;
        }
    case CSeq_loc::e_Pnt:
        {
            AddRange(loc.GetPnt().GetId(),
                     loc.GetPnt().GetPoint(),
                     loc.GetPnt().GetPoint(),
                     loc.GetPnt().GetStrand());
            return;
        }
    case CSeq_loc::e_Packed_int:
        {
            // extract each range
            iterate ( CPacked_seqint::Tdata, ii, loc.GetPacked_int().Get() ) {
                AddRange((*ii)->GetId(),
                         (*ii)->GetFrom(),
                         (*ii)->GetTo(),
                         (*ii)->GetStrand());
            }
            return;
        }
    case CSeq_loc::e_Packed_pnt:
        {
            // extract each point
            CHandleRange& hr =
                m_LocMap[m_IdMapper->GetHandle(loc.GetPacked_pnt().GetId())];
            ENa_strand strand = loc.GetPacked_pnt().GetStrand();
            iterate ( CPacked_seqpnt::TPoints, pi,
                      loc.GetPacked_pnt().GetPoints() ) {
                hr.AddRange(CHandleRange::TRange(*pi, *pi), strand);
            }
            return;
        }
    case CSeq_loc::e_Mix:
        {
            // extract sub-locations
            iterate ( CSeq_loc_mix::Tdata, li, loc.GetMix().Get() ) {
                AddLocation(**li);
            }
            return;
        }
    case CSeq_loc::e_Equiv:
        {
            // extract sub-locations
            iterate ( CSeq_loc_equiv::Tdata, li, loc.GetEquiv().Get() ) {
                AddLocation(**li);
            }
            return;
        }
    case CSeq_loc::e_Bond:
        {
            AddRange(loc.GetBond().GetA().GetId(),
                     loc.GetBond().GetA().GetPoint(),
                     loc.GetBond().GetA().GetPoint(),
                     loc.GetBond().GetA().GetStrand());
            if ( loc.GetBond().IsSetB() ) {
                AddRange(loc.GetBond().GetB().GetId(),
                         loc.GetBond().GetB().GetPoint(),
                         loc.GetBond().GetB().GetPoint(),
                         loc.GetBond().GetB().GetStrand());
            }
            return;
        }
    case CSeq_loc::e_Feat:
        {
            //### Not implemented (do we need it?)
            return;
        }
    } // switch
}


void CHandleRangeMap::AddRange(const CSeq_id_Handle& h,
                               CHandleRange::TRange range,
                               ENa_strand strand)
{
    m_LocMap[h].AddRange(range, strand);
}


void CHandleRangeMap::AddRange(const CSeq_id& id,
                               CHandleRange::TRange range,
                               ENa_strand strand)
{
    AddRange(m_IdMapper->GetHandle(id), range, strand);
}


void CHandleRangeMap::AddRanges(const CSeq_id_Handle& h,
                                const CHandleRange& hr)
{
    m_LocMap[h].AddRanges(hr);
}


bool CHandleRangeMap::IntersectingWithLoc(const CSeq_loc& loc) const
{
    CHandleRangeMap rmap(*m_IdMapper);
    rmap.AddLocation(loc);
    return IntersectingWithMap(rmap);
}


bool CHandleRangeMap::IntersectingWithMap(const CHandleRangeMap& rmap) const
{
    if ( rmap.m_LocMap.size() > m_LocMap.size() ) {
        return rmap.IntersectingWithMap(*this);
    }
    iterate ( TLocMap, it1, rmap.m_LocMap ) {
        TLocMap::iterator it2 = m_LocMap.find(it1->first);
        if ( it2 != m_LocMap.end() &&it1->second.IntersectingWith(it2->second) ) {
            return true;
        }
    }
    return false;
}


bool CHandleRangeMap::TotalRangeIntersectingWith(const CHandleRangeMap& rmap) const
{
    if ( rmap.m_LocMap.size() > m_LocMap.size() ) {
        return rmap.TotalRangeIntersectingWith(*this);
    }
    iterate ( TLocMap, it1, rmap.m_LocMap ) {
        TLocMap::iterator it2 = m_LocMap.find(it1->first);
        if ( it2 != m_LocMap.end() &&
             it1->second.GetOverlappingRange()
             .IntersectingWith(it2->second.GetOverlappingRange()) ) {
            return true;
        }
    }
    return false;
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.9  2003/01/22 20:11:54  vasilche
* Merged functionality of CSeqMapResolved_CI to CSeqMap_CI.
* CSeqMap_CI now supports resolution and iteration over sequence range.
* Added several caches to CScope.
* Optimized CSeqVector().
* Added serveral variants of CBioseqHandle::GetSeqVector().
* Tried to optimize annotations iterator (not much success).
* Rewritten CHandleRange and CHandleRangeMap classes to avoid sorting of list.
*
* Revision 1.8  2002/12/26 20:55:17  dicuccio
* Moved seq_id_mapper.hpp, tse_info.hpp, and bioseq_info.hpp -> include/ tree
*
* Revision 1.7  2002/12/06 15:36:00  grichenk
* Added overlap type for annot-iterators
*
* Revision 1.6  2002/07/08 20:51:01  grichenk
* Moved log to the end of file
* Replaced static mutex (in CScope, CDataSource) with the mutex
* pool. Redesigned CDataSource data locking.
*
* Revision 1.5  2002/06/12 14:40:47  grichenk
* Made some methods inline
*
* Revision 1.4  2002/02/21 19:27:06  grichenk
* Rearranged includes. Added scope history. Added searching for the
* best seq-id match in data sources and scopes. Updated tests.
*
* Revision 1.3  2002/02/15 20:35:38  gouriano
* changed implementation of HandleRangeMap
*
* Revision 1.2  2002/01/23 21:59:31  grichenk
* Redesigned seq-id handles and mapper
*
* Revision 1.1  2002/01/11 19:06:20  gouriano
* restructured objmgr
*
*
* ===========================================================================
*/
