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
#include <objects/seq/seq__.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Seq_point.hpp>
#include <objects/seqloc/Seq_bond.hpp>
#include <objects/seqloc/Seq_loc_equiv.hpp>
#include <objmgr/seq_map_ci.hpp>
#include <objmgr/impl/bioseq_info.hpp>
#include <objmgr/impl/tse_info.hpp>

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
                                  ETransSplicing trans_splcing)
{
    SAddState state;
    state.m_TransSplicing = trans_splcing;
    AddLocation(loc, state);
}


void CHandleRangeMap::AddLocation(const CSeq_loc& loc,
                                  SAddState& state)
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
                 eNa_strand_unknown, state);
        return;
    }
    case CSeq_loc::e_Whole:
    {
        AddRange(loc.GetWhole(), TRange::GetWhole(),
                 eNa_strand_unknown, state);
        return;
    }
    case CSeq_loc::e_Int:
    {
        const CSeq_interval& i = loc.GetInt();
        AddRange(i.GetId(),
                 i.GetFrom(),
                 i.GetTo(),
                 i.IsSetStrand()? i.GetStrand(): eNa_strand_unknown,
                 state);
        return;
    }
    case CSeq_loc::e_Pnt:
    {
        const CSeq_point& p = loc.GetPnt();
        AddRange(p.GetId(),
                 p.GetPoint(),
                 p.GetPoint(),
                 p.IsSetStrand()? p.GetStrand(): eNa_strand_unknown,
                 state);
        return;
    }
    case CSeq_loc::e_Packed_int:
    {
        // extract each range
        const CPacked_seqint& pi = loc.GetPacked_int();
        ITERATE( CPacked_seqint::Tdata, ii, pi.Get() ) {
            const CSeq_interval& i = **ii;
            AddRange(i.GetId(),
                     i.GetFrom(),
                     i.GetTo(),
                     i.IsSetStrand()? i.GetStrand(): eNa_strand_unknown,
                     state);
        }
        return;
    }
    case CSeq_loc::e_Packed_pnt:
    {
        // extract each point
        const CPacked_seqpnt& pp = loc.GetPacked_pnt();
        CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(pp.GetId());
        ENa_strand strand =
            pp.IsSetStrand()? pp.GetStrand(): eNa_strand_unknown;
        ITERATE ( CPacked_seqpnt::TPoints, pi, pp.GetPoints() ) {
            AddRange(idh, CRange<TSeqPos>(*pi, *pi), strand, state);
        }
        return;
    }
    case CSeq_loc::e_Mix:
    {
        // extract sub-locations
        ITERATE ( CSeq_loc_mix::Tdata, li, loc.GetMix().Get() ) {
            AddLocation(**li, state);
        }
        return;
    }
    case CSeq_loc::e_Equiv:
    {
        // extract sub-locations
        ITERATE ( CSeq_loc_equiv::Tdata, li, loc.GetEquiv().Get() ) {
            AddLocation(**li, state);
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
                 state);
        if ( bond.IsSetB() ) {
            const CSeq_point& pb = bond.GetB();
            AddRange(pb.GetId(),
                     pb.GetPoint(),
                     pb.GetPoint(),
                     pb.IsSetStrand()? pb.GetStrand(): eNa_strand_unknown,
                     state);
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
                               const TRange& range, ENa_strand strand)
{
    SAddState state;
    AddRange(h, range, strand, state);
}


void CHandleRangeMap::AddRange(const CSeq_id& id,
                               const TRange& range, ENa_strand strand)
{
    SAddState state;
    AddRange(id, range, strand, state);
}


void CHandleRangeMap::AddRange(const CSeq_id& id,
                               TSeqPos from, TSeqPos to, ENa_strand strand,
                               SAddState& state)
{
    AddRange(id, TRange(from, to), strand, state);
}


void CHandleRangeMap::AddRange(const CSeq_id& id,
                               TSeqPos from, TSeqPos to, ENa_strand strand)
{
    SAddState state;
    AddRange(id, from, to, strand, state);
}


void CHandleRangeMap::AddRange(const CSeq_id_Handle& h,
                               const TRange& range,
                               ENa_strand strand,
                               SAddState& state)
{
    CHandleRange& hr = m_LocMap[h];
    if ( state.m_TransSplicing == eNoTransSplicing &&
         state.m_PrevId && h && state.m_PrevId != h ) {
        m_LocMap[state.m_PrevId].m_MoreAfter = true;
        hr.m_MoreBefore = true;
        if ( m_MasterSeq ) {
            int pos1 = m_MasterSeq->FindSeg(state.m_PrevId);
            int pos2 = m_MasterSeq->FindSeg(h);
            if ( pos1 >= 0 && pos2 >= 0 && abs(pos2-pos1) > 1 ) {
                bool minus1 = m_MasterSeq->GetMinusStrand(pos1);
                bool minus2 = m_MasterSeq->GetMinusStrand(pos2);
                bool backw = pos2 < pos1;
                bool backw1 = IsReverse(state.m_PrevStrand) != minus1;
                bool backw2 = IsReverse(strand) != minus2;
                if ( backw1 == backw && backw2 == backw ) {
                    ENa_strand strand2 = backw? Reverse(strand): strand;
                    int dir = backw ? -1: 1;
                    for ( int pos = pos1+dir; pos != pos2; pos += dir ) {
                        CHandleRange& mhr =
                            m_LocMap[m_MasterSeq->GetHandle(pos)];
                        mhr.AddRange(TRange::GetEmpty(), strand2, true, true);
                    }
                }
            }
        }
    }
    hr.AddRange(range, strand);
    state.m_PrevId = h;
    state.m_PrevStrand = strand;
    state.m_PrevRange = range;
}


void CHandleRangeMap::AddRange(const CSeq_id& id,
                               const TRange& range,
                               ENa_strand strand,
                               SAddState& state)
{
    AddRange(CSeq_id_Handle::GetHandle(id), range, strand, state);
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


/////////////////////////////////////////////////////////////////////////////
// CMasterSeqSegments
/////////////////////////////////////////////////////////////////////////////

CMasterSeqSegments::CMasterSeqSegments(void)
{
}


CMasterSeqSegments::~CMasterSeqSegments(void)
{
}


CMasterSeqSegments::CMasterSeqSegments(const CBioseq_Info& master)
{
    AddSegments(master.GetSeqMap());
    for ( int idx = 0; idx < GetSegmentCount(); ++idx ) {
        const CSeq_id_Handle& h = GetHandle(idx);
        CConstRef<CBioseq_Info> seg =
            master.GetTSE_Info().FindMatchingBioseq(h);
        if ( seg ) {
            AddSegmentIds(idx, seg->GetId());
        }
    }
}


int CMasterSeqSegments::AddSegment(const CSeq_id_Handle& id, bool minus_strand)
{
    int idx = GetSegmentCount();
    m_SegSet.push_back(TSeg(id, minus_strand));
    AddSegmentId(idx, id);
    return idx;
}


void CMasterSeqSegments::AddSegmentId(int idx, const CSeq_id_Handle& id)
{
    m_Id2Seg[id] = idx;
}


void CMasterSeqSegments::AddSegmentIds(int idx, const TIds& ids)
{
    ITERATE ( TIds, it, ids ) {
        AddSegmentId(idx, *it);
    }
}


void CMasterSeqSegments::AddSegmentIds(int idx, const TIds2& ids)
{
    ITERATE ( TIds2, it, ids ) {
        AddSegmentId(idx, CSeq_id_Handle::GetHandle(**it));
    }
}


void CMasterSeqSegments::AddSegmentIds(const TIds& ids)
{
    ITERATE ( TIds, it, ids ) {
        int idx = FindSeg(*it);
        if ( idx >= 0 ) {
            AddSegmentIds(idx, ids);
            return;
        }
    }
}


void CMasterSeqSegments::AddSegmentIds(const TIds2& ids)
{
    ITERATE ( TIds2, it, ids ) {
        int idx = FindSeg(CSeq_id_Handle::GetHandle(**it));
        if ( idx >= 0 ) {
            AddSegmentIds(idx, ids);
            return;
        }
    }
}


void CMasterSeqSegments::AddSegments(const CSeqMap& seq)
{
    for ( CSeqMap_CI it(ConstRef(&seq), 0, CSeqMap::fFindRef); it; ++it ) {
        AddSegment(it.GetRefSeqid(), it.GetRefMinusStrand());
    }
}


int CMasterSeqSegments::FindSeg(const CSeq_id_Handle& h) const
{
    TId2Seg::const_iterator it = m_Id2Seg.find(h);
    return it == m_Id2Seg.end()? -1: it->second;
}


const CSeq_id_Handle& CMasterSeqSegments::GetHandle(int seg) const
{
    _ASSERT(size_t(seg) < m_SegSet.size());
    return m_SegSet[seg].first;
}


bool CMasterSeqSegments::GetMinusStrand(int seg) const
{
    _ASSERT(size_t(seg) < m_SegSet.size());
    return m_SegSet[seg].second;
}


END_SCOPE(objects)
END_NCBI_SCOPE
