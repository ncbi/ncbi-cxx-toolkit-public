
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
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2002/01/11 19:06:20  gouriano
* restructured objmgr
*
*
* ===========================================================================
*/

#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Seq_point.hpp>
#include <objects/seqloc/Seq_bond.hpp>
#include <objects/seqloc/Seq_loc_equiv.hpp>

#include "handle_range_map.hpp"
#include "seq_id_mapper.hpp"


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


////////////////////////////////////////////////////////////////////
//
//  CHandleRangeMap::
//


CHandleRangeMap::CHandleRangeMap(void)
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
    m_LocMap.clear();
    iterate ( TLocMap, it, rmap.m_LocMap ) {
        m_LocMap.insert(TLocMap::value_type(it->first, it->second));
    }
    return *this;
}


void CHandleRangeMap::AddLocation(const CSeq_loc& loc)
{
    CBioseqHandle h;
    TLocMap::iterator newh;
    ENa_strand strand = eNa_strand_unknown;
    switch ( loc.Which() ) {
    case CSeq_loc::e_not_set:
    case CSeq_loc::e_Null:
    case CSeq_loc::e_Empty:
        {
            return;
        }
    case CSeq_loc::e_Whole:
        {
            h = CSeqIdMapper::SeqIdToHandle(loc.GetWhole());
            newh = m_LocMap.insert
                (TLocMap::value_type(h, CHandleRange(h))).first;
            newh->second.AddRange(CHandleRange::TRange::GetWhole(), strand);
            return;
        }
    case CSeq_loc::e_Int:
        {
            h = CSeqIdMapper::SeqIdToHandle(loc.GetInt().GetId());
            newh = m_LocMap.insert
                (TLocMap::value_type(h, CHandleRange(h))).first;
            if ( loc.GetInt().IsSetStrand() )
                strand = loc.GetInt().GetStrand();
            newh->second.AddRange(CHandleRange::TRange(
                loc.GetInt().GetFrom(), loc.GetInt().GetTo()), strand);
            return;
        }
    case CSeq_loc::e_Pnt:
        {
            h = CSeqIdMapper::SeqIdToHandle(loc.GetPnt().GetId());
            newh = m_LocMap.insert
                (TLocMap::value_type(h, CHandleRange(h))).first;
            if ( loc.GetPnt().IsSetStrand() )
                strand = loc.GetPnt().GetStrand();
            newh->second.AddRange(CHandleRange::TRange(
                loc.GetPnt().GetPoint(), loc.GetPnt().GetPoint()), strand);
            return;
        }
    case CSeq_loc::e_Packed_int:
        {
            // extract each range
            iterate ( CPacked_seqint::Tdata, ii, loc.GetPacked_int().Get() ) {
                h = CSeqIdMapper::SeqIdToHandle((*ii)->GetId());
                newh = m_LocMap.insert
                    (TLocMap::value_type(h, CHandleRange(h))).first;
                if ( (*ii)->IsSetStrand() )
                    strand = (*ii)->GetStrand();
                else
                    strand = eNa_strand_unknown;
                newh->second.AddRange(CHandleRange::TRange(
                    (*ii)->GetFrom(), (*ii)->GetTo()), strand);
            }
            return;
        }
    case CSeq_loc::e_Packed_pnt:
        {
            // extract each point
            h = CSeqIdMapper::SeqIdToHandle(loc.GetPacked_pnt().GetId());
            newh = m_LocMap.insert
                (TLocMap::value_type(h, CHandleRange(h))).first;
            if ( loc.GetPacked_pnt().IsSetStrand() )
                strand = loc.GetPacked_pnt().GetStrand();
            iterate ( CPacked_seqpnt::TPoints, pi,
                      loc.GetPacked_pnt().GetPoints() ) {
                newh->second.AddRange(CHandleRange::TRange(*pi, *pi), strand);
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
            h = CSeqIdMapper::SeqIdToHandle(loc.GetBond().GetA().GetId());
            newh = m_LocMap.insert
                (TLocMap::value_type(h, CHandleRange(h))).first;
            if ( loc.GetBond().GetA().IsSetStrand() )
                strand = loc.GetBond().GetA().GetStrand();
            newh->second.AddRange(CHandleRange::TRange(
                loc.GetBond().GetA().GetPoint(),
                loc.GetBond().GetA().GetPoint()),
                strand);
            if ( loc.GetBond().IsSetB() ) {
                h = CSeqIdMapper::SeqIdToHandle(
                    loc.GetBond().GetB().GetId());
                newh = m_LocMap.insert
                    (TLocMap::value_type(h, CHandleRange(h))).first;
                if ( loc.GetBond().GetA().IsSetStrand() )
                    strand = loc.GetBond().GetB().GetStrand();
                else
                    strand = eNa_strand_unknown;
                newh->second.AddRange(CHandleRange::TRange(
                    loc.GetBond().GetB().GetPoint(),
                    loc.GetBond().GetB().GetPoint()),
                    strand);
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


void CHandleRangeMap::AddRanges(const CBioseqHandle& h,
                                const CHandleRange& range)
{
    TLocMap::iterator hr = m_LocMap.insert
        (TLocMap::value_type(h, CHandleRange(h))).first;
    iterate ( CHandleRange::TRanges, it, range.GetRanges() ) {
        hr->second.AddRange(it->first, it->second);
    }
}


bool CHandleRangeMap::IntersectingWithLoc(const CSeq_loc& loc)
{
    CHandleRangeMap rmap;
    rmap.AddLocation(loc);
    return IntersectingWithMap(rmap);
}


bool CHandleRangeMap::IntersectingWithMap(const CHandleRangeMap& rmap)
{
    iterate ( TLocMap, it1, rmap.m_LocMap ) {
        TLocMap::iterator it2 = m_LocMap.find(it1->first);
        if ( it2 == m_LocMap.end() )
            continue;
        if ( it1->second.IntersectingWith(it2->second) )
            return true;
    }
    return false;
}


END_SCOPE(objects)
END_NCBI_SCOPE
