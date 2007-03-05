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
*   CHandle_Range_Map is a substitute for seq-loc to make searching
*   over locations more effective.
*
*/

#include <ncbi_pch.hpp>
#include <objmgr/impl/handle_range_map.hpp>
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
    if (this != &rmap) {
        m_LocMap = rmap.m_LocMap;
    }
    return *this;
}


void CHandleRangeMap::clear(void)
{
    m_LocMap.clear();
}


void CHandleRangeMap::AddLocation(const CSeq_loc& loc,
                                  bool more_before, bool more_after)
{
    switch ( loc.Which() ) {
    case CSeq_loc::e_not_set:
    case CSeq_loc::e_Null:
    {
        return;
    }
    case CSeq_loc::e_Empty:
    {
        AddRange(loc.GetEmpty(), TRange::GetEmpty(),
                 eNa_strand_unknown, more_before, more_after);
        return;
    }
    case CSeq_loc::e_Whole:
    {
        AddRange(loc.GetWhole(), TRange::GetWhole(),
                 eNa_strand_unknown, more_before, more_after);
        return;
    }
    case CSeq_loc::e_Int:
    {
        const CSeq_interval& i = loc.GetInt();
        AddRange(i.GetId(),
                 i.GetFrom(),
                 i.GetTo(),
                 i.IsSetStrand()? i.GetStrand(): eNa_strand_unknown,
                 more_before, more_after);
        return;
    }
    case CSeq_loc::e_Pnt:
    {
        const CSeq_point& p = loc.GetPnt();
        AddRange(p.GetId(),
                 p.GetPoint(),
                 p.GetPoint(),
                 p.IsSetStrand()? p.GetStrand(): eNa_strand_unknown,
                 more_before, more_after);
        return;
    }
    case CSeq_loc::e_Packed_int:
    {
        // extract each range
        const CPacked_seqint& pi = loc.GetPacked_int();
        if ( !pi.Get().empty() ) {
            CPacked_seqint::Tdata::const_iterator last = pi.Get().end();
            --last;
            ITERATE( CPacked_seqint::Tdata, ii, pi.Get() ) {
                const CSeq_interval& i = **ii;
                AddRange(i.GetId(),
                         i.GetFrom(),
                         i.GetTo(),
                         i.IsSetStrand()? i.GetStrand(): eNa_strand_unknown,
                         more_before, ii != last || more_after);
                more_before = true;
            }
        }
        return;
    }
    case CSeq_loc::e_Packed_pnt:
    {
        // extract each point
        const CPacked_seqpnt& pp = loc.GetPacked_pnt();
        if ( !pp.GetPoints().empty() ) {
            CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(pp.GetId());
            CHandleRange& hr = m_LocMap[idh];
            ENa_strand strand =
                pp.IsSetStrand()? pp.GetStrand(): eNa_strand_unknown;
            CPacked_seqpnt::TPoints::const_iterator last =
                pp.GetPoints().end();
            --last;
            ITERATE ( CPacked_seqpnt::TPoints, pi, pp.GetPoints() ) {
                hr.AddRange(CRange<TSeqPos>(*pi, *pi), strand,
                            more_before, pi != last || more_after);
                more_before = true;
            }
        }
        return;
    }
    case CSeq_loc::e_Mix:
    {
        // extract sub-locations
        if ( !loc.GetMix().Get().empty() ) {
            CSeq_loc_mix::Tdata::const_iterator last =
                loc.GetMix().Get().end();
            --last;
            ITERATE ( CSeq_loc_mix::Tdata, li, loc.GetMix().Get() ) {
                AddLocation(**li, more_before, li != last || more_after);
                more_before = true;
            }
        }
        return;
    }
    case CSeq_loc::e_Equiv:
    {
        // extract sub-locations
        ITERATE ( CSeq_loc_equiv::Tdata, li, loc.GetEquiv().Get() ) {
            AddLocation(**li, more_before, more_after);
        }
        return;
    }
    case CSeq_loc::e_Bond:
    {
        const CSeq_bond& bond = loc.GetBond();
        const CSeq_point& pa = bond.GetA();
        AddRange(pa.GetId(),
                 pa.GetPoint(),
                 pa.GetPoint(),
                 pa.IsSetStrand()? pa.GetStrand(): eNa_strand_unknown,
                 more_before, more_after);
        if ( bond.IsSetB() ) {
            const CSeq_point& pb = bond.GetB();
            AddRange(pb.GetId(),
                     pb.GetPoint(),
                     pb.GetPoint(),
                     pb.IsSetStrand()? pb.GetStrand(): eNa_strand_unknown,
                     more_before, more_after);
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
                               const TRange& range,
                               ENa_strand strand,
                               bool more_before,
                               bool more_after)
{
    m_LocMap[h].AddRange(range, strand, more_before, more_after);
}


void CHandleRangeMap::AddRange(const CSeq_id& id,
                               const TRange& range,
                               ENa_strand strand,
                               bool more_before, bool more_after)
{
    AddRange(CSeq_id_Handle::GetHandle(id), range, strand,
             more_before, more_after);
}


void CHandleRangeMap::AddRanges(const CSeq_id_Handle& h,
                                const CHandleRange& hr)
{
    m_LocMap[h].AddRanges(hr);
}


CHandleRange& CHandleRangeMap::AddRanges(const CSeq_id_Handle& h)
{
    return m_LocMap[h];
}


bool CHandleRangeMap::IntersectingWithLoc(const CSeq_loc& loc) const
{
    CHandleRangeMap rmap;
    rmap.AddLocation(loc);
    return IntersectingWithMap(rmap);
}


bool CHandleRangeMap::IntersectingWithMap(const CHandleRangeMap& rmap) const
{
    if ( rmap.m_LocMap.size() > m_LocMap.size() ) {
        return rmap.IntersectingWithMap(*this);
    }
    ITERATE ( CHandleRangeMap, it1, rmap ) {
        const_iterator it2 = m_LocMap.find(it1->first);
        if ( it2 != end() && it1->second.IntersectingWith(it2->second) ) {
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
    ITERATE ( CHandleRangeMap, it1, rmap ) {
        TLocMap::const_iterator it2 = m_LocMap.find(it1->first);
        if ( it2 != end() && it1->second.GetOverlappingRange()
             .IntersectingWith(it2->second.GetOverlappingRange()) ) {
            return true;
        }
    }
    return false;
}


END_SCOPE(objects)
END_NCBI_SCOPE
