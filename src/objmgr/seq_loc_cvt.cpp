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
*   Class for mapping Seq-loc between sequences.
*
*/

#include <ncbi_pch.hpp>
#include <objmgr/impl/seq_loc_cvt.hpp>

#include <objmgr/seq_loc_mapper.hpp>
#include <objmgr/impl/seq_align_mapper.hpp>
#include <objmgr/seq_map_ci.hpp>
#include <objmgr/impl/scope_impl.hpp>
#include <objmgr/annot_types_ci.hpp>
#include <objmgr/impl/annot_object.hpp>
#include <objmgr/impl/seq_annot_info.hpp>

#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Seq_point.hpp>
#include <objects/seqloc/Seq_loc_equiv.hpp>
#include <objects/seqloc/Seq_bond.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seqfeat/Code_break.hpp>
#include <objects/seqfeat/RNA_ref.hpp>
#include <objects/seqfeat/Trna_ext.hpp>
#include <algorithm>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

/////////////////////////////////////////////////////////////////////////////
// CSeq_loc_Conversion
/////////////////////////////////////////////////////////////////////////////

CSeq_loc_Conversion::CSeq_loc_Conversion(CSeq_loc&             master_loc_empty,
                                         const CSeq_id_Handle& dst_id,
                                         const CSeqMap_CI&     seg,
                                         const CSeq_id_Handle& src_id,
                                         CScope*               scope)
    : m_Src_id_Handle(src_id),
      m_Src_from(0),
      m_Src_to(0),
      m_Shift(0),
      m_Reverse(false),
      m_Dst_id_Handle(dst_id),
      m_Dst_loc_Empty(&master_loc_empty),
      m_Partial(false),
      m_PartialFlag(0),
      m_LastType(eMappedObjType_not_set),
      m_LastStrand(eNa_strand_unknown),
      m_Scope(scope)
{
    SetConversion(seg);
    Reset();
}


CSeq_loc_Conversion::CSeq_loc_Conversion(CSeq_loc&             master_loc_empty,
                                         const CSeq_id_Handle& dst_id,
                                         const TRange&         dst_rg,
                                         const CSeq_id_Handle& src_id,
                                         TSeqPos               src_start,
                                         bool                  reverse,
                                         CScope*               scope)
    : m_Src_id_Handle(src_id),
      m_Src_from(0),
      m_Src_to(0),
      m_Shift(0),
      m_Reverse(reverse),
      m_Dst_id_Handle(dst_id),
      m_Dst_loc_Empty(&master_loc_empty),
      m_Partial(false),
      m_PartialFlag(0),
      m_LastType(eMappedObjType_not_set),
      m_LastStrand(eNa_strand_unknown),
      m_Scope(scope)
{
    m_Src_from = src_start;
    m_Src_to = m_Src_from + dst_rg.GetLength() - 1;
    if ( !m_Reverse ) {
        m_Shift = dst_rg.GetFrom() - m_Src_from;
    }
    else {
        m_Shift = dst_rg.GetFrom() + m_Src_to;
    }
    Reset();
}


CSeq_loc_Conversion::~CSeq_loc_Conversion(void)
{
    _ASSERT(!IsSpecialLoc());
}


void CSeq_loc_Conversion::Reset(void)
{
    _ASSERT(!IsSpecialLoc());
    m_TotalRange = TRange::GetEmpty();
    m_Partial = false;
    m_PartialFlag = 0;
    m_DstFuzz_from.Reset();
    m_DstFuzz_to.Reset();
    m_GraphRanges.Reset();
}


void CSeq_loc_Conversion::CombineWith(CSeq_loc_Conversion& cvt)
{
    _ASSERT(cvt.m_Src_id_Handle == m_Dst_id_Handle);
    TRange dst_rg = GetDstRange();
    TRange cvt_src_rg = cvt.GetSrcRange();
    TRange overlap = dst_rg & cvt_src_rg;
    _ASSERT( !overlap.Empty() );

    TSeqPos new_dst_from = cvt.ConvertPos(overlap.GetFrom());
    _ASSERT(new_dst_from != kInvalidSeqPos);
    _ASSERT(cvt.ConvertPos(overlap.GetTo()) != kInvalidSeqPos);
    bool new_reverse = cvt.m_Reverse ? !m_Reverse : m_Reverse;
    if (overlap.GetFrom() > dst_rg.GetFrom()) {
        TSeqPos l_trunc = overlap.GetFrom() - dst_rg.GetFrom();
        // Truncated range
        if ( !m_Reverse ) {
            m_Src_from += l_trunc;
        }
        else {
            m_Src_to -= l_trunc;
        }
    }
    if (overlap.GetTo() < dst_rg.GetTo()) {
        TSeqPos r_trunc = dst_rg.GetTo() - overlap.GetTo();
        // Truncated range
        if ( !m_Reverse ) {
            m_Src_to -= r_trunc;
        }
        else {
            m_Src_from += r_trunc;
        }
    }
    m_Reverse = new_reverse;
    if ( !m_Reverse ) {
        m_Shift = new_dst_from - m_Src_from;
    }
    else {
        m_Shift = new_dst_from + m_Src_to;
    }
    m_Dst_id_Handle = cvt.m_Dst_id_Handle;
    m_Dst_loc_Empty = cvt.m_Dst_loc_Empty;
    cvt.Reset();
    Reset();
}


void CSeq_loc_Conversion::SetConversion(const CSeqMap_CI& seg)
{
    m_Src_from = seg.GetRefPosition();
    m_Src_to = m_Src_from + seg.GetLength() - 1;
    m_Reverse = seg.GetRefMinusStrand();
    if ( !m_Reverse ) {
        m_Shift = seg.GetPosition() - m_Src_from;
    }
    else {
        m_Shift = seg.GetPosition() + m_Src_to;
    }
}


CConstRef<CInt_fuzz>
CSeq_loc_Conversion::ReverseFuzz(const CInt_fuzz& fuzz) const
{
    if ( fuzz.IsLim() ) {
        CInt_fuzz::ELim lim = fuzz.GetLim();
        switch ( lim ) {
        case CInt_fuzz::eLim_lt: lim = CInt_fuzz::eLim_gt; break;
        case CInt_fuzz::eLim_gt: lim = CInt_fuzz::eLim_lt; break;
        case CInt_fuzz::eLim_tr: lim = CInt_fuzz::eLim_tl; break;
        case CInt_fuzz::eLim_tl: lim = CInt_fuzz::eLim_tr; break;
        default: return ConstRef(&fuzz);
        }
        CRef<CInt_fuzz> ret(new CInt_fuzz);
        ret->SetLim(lim);
        return ret;
    }
    return ConstRef(&fuzz);
}


void CSeq_loc_Conversion::ConvertSimpleLoc(const CSeq_id_Handle& src_id,
                                           const CRange<TSeqPos> src_range,
                                           const SAnnotObject_Index& src_index)
{
    if ( src_id != m_Src_id_Handle ) {
        m_Partial = true;
        return;
    }
    
    ENa_strand strand;
    switch ( src_index.m_Flags & src_index.fStrand_mask ) {
    case SAnnotObject_Index::fStrand_plus:
        strand = eNa_strand_plus;
        break;
    case SAnnotObject_Index::fStrand_minus:
        strand = eNa_strand_minus;
        break;
    default:
        strand = eNa_strand_unknown;
        break;
    }
    if ( src_index.LocationIsPoint() ) {
        ConvertPoint(src_range.GetFrom(), strand);
    }
    else if ( src_index.LocationIsInterval() ) {
        ConvertInterval(src_range.GetFrom(), src_range.GetTo(), strand);
    }
    else {
        _ASSERT(src_index.LocationIsWhole());
        CBioseq_Handle bh =
            m_Scope->GetBioseqHandle(src_id, CScope::eGetBioseq_All);
        ConvertInterval(0, bh.GetBioseqLength()-1, eNa_strand_unknown);
    }
}


bool CSeq_loc_Conversion::ConvertPoint(const CSeq_point& src)
{
    ENa_strand strand = src.IsSetStrand()? src.GetStrand(): eNa_strand_unknown;
    bool ret = GoodSrcId(src.GetId()) && ConvertPoint(src.GetPoint(), strand);
    if ( ret ) {
        if ( src.IsSetFuzz() ) {
            if ( m_Reverse ) {
                m_DstFuzz_from = ReverseFuzz(src.GetFuzz());
            }
            else {
                m_DstFuzz_from = &src.GetFuzz();
            }
            // normalize left and right fuzz values
            if ( m_DstFuzz_from && m_DstFuzz_from->IsLim() &&
                 m_DstFuzz_from->GetLim() == CInt_fuzz::eLim_lt ) {
                m_DstFuzz_from.Reset();
                m_PartialFlag |= fPartial_from;
            }
        }
    }
    else if ( m_GraphRanges ) {
        m_GraphRanges->IncOffset(1);
    }
    return ret;
}


bool CSeq_loc_Conversion::ConvertInterval(const CSeq_interval& src)
{
    ENa_strand strand = src.IsSetStrand()? src.GetStrand(): eNa_strand_unknown;
    bool ret = GoodSrcId(src.GetId()) &&
        ConvertInterval(src.GetFrom(), src.GetTo(), strand);
    if ( ret ) {
        if ( m_Reverse ) {
            if ( !(m_PartialFlag & fPartial_to) && src.IsSetFuzz_from() ) {
                m_DstFuzz_to = ReverseFuzz(src.GetFuzz_from());
            }
            if ( !(m_PartialFlag & fPartial_from) && src.IsSetFuzz_to() ) {
                m_DstFuzz_from = ReverseFuzz(src.GetFuzz_to());
            }
        }
        else {
            if ( !(m_PartialFlag & fPartial_from) && src.IsSetFuzz_from() ) {
                m_DstFuzz_from = &src.GetFuzz_from();
            }
            if ( !(m_PartialFlag & fPartial_to) && src.IsSetFuzz_to() ) {
                m_DstFuzz_to = &src.GetFuzz_to();
            }
        }
        // normalize left and right fuzz values
        if ( m_DstFuzz_from && m_DstFuzz_from->IsLim() &&
             m_DstFuzz_from->GetLim() == CInt_fuzz::eLim_lt ) {
            m_DstFuzz_from.Reset();
            m_PartialFlag |= fPartial_from;
        }
        if ( m_DstFuzz_to && m_DstFuzz_to->IsLim() &&
             m_DstFuzz_to->GetLim() == CInt_fuzz::eLim_gt ) {
            m_DstFuzz_to.Reset();
            m_PartialFlag |= fPartial_to;
        }
    }
    else if ( m_GraphRanges ) {
        m_GraphRanges->IncOffset(src.GetLength());
    }
    return ret;
}


bool CSeq_loc_Conversion::ConvertPoint(TSeqPos src_pos,
                                       ENa_strand src_strand)
{
    _ASSERT(!IsSpecialLoc());
    m_PartialFlag = 0;
    m_DstFuzz_from.Reset();
    m_DstFuzz_to.Reset();
    if ( src_pos < m_Src_from || src_pos > m_Src_to ) {
        m_Partial = true;
        return false;
    }
    TSeqPos dst_pos;
    if ( !m_Reverse ) {
        m_LastStrand = src_strand;
        dst_pos = m_Shift + src_pos;
    }
    else {
        m_LastStrand = Reverse(src_strand);
        dst_pos = m_Shift - src_pos;
    }
    m_LastType = eMappedObjType_Seq_point;
    m_TotalRange += m_LastRange.SetFrom(dst_pos).SetTo(dst_pos);
    if ( m_GraphRanges ) {
        m_GraphRanges->AddRange(TRange(src_pos, src_pos));
        m_GraphRanges->IncOffset(1);
    }
    return true;
}


bool CSeq_loc_Conversion::ConvertInterval(TSeqPos src_from, TSeqPos src_to,
                                          ENa_strand src_strand)
{
    _ASSERT(!IsSpecialLoc());
    m_PartialFlag = 0;
    m_DstFuzz_from.Reset();
    m_DstFuzz_to.Reset();
    bool partial_from = false, partial_to = false;
    TSeqPos len = src_to - src_from + 1;
    TRange graph_rg(0, len - 1);
    if ( src_from < m_Src_from ) {
        m_Partial = partial_from = true;
        graph_rg.SetFrom(m_Src_from - src_from);
        src_from = m_Src_from;
    }
    if ( src_to > m_Src_to ) {
        m_Partial = partial_to = true;
        graph_rg.SetLength(m_Src_to - src_from + 1);
        src_to = m_Src_to;
    }
    if ( src_from > src_to ) {
        m_Partial = true;
        return false;
    }
    TSeqPos dst_from, dst_to;
    if ( !m_Reverse ) {
        m_LastStrand = src_strand;
        dst_from = m_Shift + src_from;
        dst_to = m_Shift + src_to;
    }
    else {
        m_LastStrand = Reverse(src_strand);
        dst_from = m_Shift - src_to;
        dst_to = m_Shift - src_from;
        swap(partial_from, partial_to);
    }
    m_LastType = eMappedObjType_Seq_interval;
    m_TotalRange += m_LastRange.SetFrom(dst_from).SetTo(dst_to);
    if ( partial_from ) {
        m_PartialFlag |= fPartial_from;
    }
    if ( partial_to ) {
        m_PartialFlag |= fPartial_to;
    }
    if ( m_GraphRanges ) {
        m_GraphRanges->AddRange(graph_rg);
        m_GraphRanges->IncOffset(len);
    }
    return true;
}


inline
void CSeq_loc_Conversion::CheckDstInterval(void)
{
    if ( m_LastType != eMappedObjType_Seq_interval ) {
        NCBI_THROW(CAnnotException, eBadLocation,
                   "Wrong last location type");
    }
    m_LastType = eMappedObjType_not_set;
}


inline
void CSeq_loc_Conversion::CheckDstPoint(void)
{
    if ( m_LastType != eMappedObjType_Seq_point ) {
        NCBI_THROW(CAnnotException, eBadLocation,
                   "Wrong last location type");
    }
    m_LastType = eMappedObjType_not_set;
}


inline
void CSeq_loc_Conversion::CheckDstMix(void)
{
    if ( m_LastType != eMappedObjType_Seq_loc_mix ) {
        NCBI_THROW(CAnnotException, eBadLocation,
                   "Wrong last location type");
    }
    m_LastType = eMappedObjType_not_set;
}


CRef<CSeq_interval> CSeq_loc_Conversion::GetDstInterval(void)
{
    CheckDstInterval();
    CRef<CSeq_interval> ret(new CSeq_interval);
    CSeq_interval& interval = *ret;
    interval.SetId(GetDstId());
    interval.SetFrom(m_LastRange.GetFrom());
    interval.SetTo(m_LastRange.GetTo());
    if ( m_LastStrand != eNa_strand_unknown ) {
        interval.SetStrand(m_LastStrand);
    }
    if ( m_PartialFlag & fPartial_from ) {
        interval.SetFuzz_from().SetLim(CInt_fuzz::eLim_lt);
    }
    else if ( m_DstFuzz_from ) {
        interval.SetFuzz_from(const_cast<CInt_fuzz&>(*m_DstFuzz_from));
    }
    if ( m_PartialFlag & fPartial_to ) {
        interval.SetFuzz_to().SetLim(CInt_fuzz::eLim_gt);
    }
    else if ( m_DstFuzz_to ) {
        interval.SetFuzz_to(const_cast<CInt_fuzz&>(*m_DstFuzz_to));
    }
    return ret;
}


CRef<CSeq_point> CSeq_loc_Conversion::GetDstPoint(void)
{
    CheckDstPoint();
    _ASSERT(m_LastRange.GetLength() == 1);
    CRef<CSeq_point> ret(new CSeq_point);
    CSeq_point& point = *ret;
    point.SetId(GetDstId());
    point.SetPoint(m_LastRange.GetFrom());
    // Points can not be partial
    if ( m_LastStrand != eNa_strand_unknown ) {
        point.SetStrand(m_LastStrand);
    }
    if ( m_PartialFlag & fPartial_from ) {
        point.SetFuzz().SetLim(CInt_fuzz::eLim_lt);
    }
    else if ( m_DstFuzz_from ) {
        point.SetFuzz(const_cast<CInt_fuzz&>(*m_DstFuzz_from));
    }
    return ret;
}


void CSeq_loc_Conversion::MakeDstMix(CSeq_loc_mix& dst,
                                     const CSeq_loc_mix& src) const
{
    CSeq_loc_mix::Tdata& dst_mix = dst.Set();
    const CSeq_loc_mix::Tdata& src_mix = src.Get();
    ITERATE ( CSeq_loc_mix::Tdata, it, src_mix ) {
        const CSeq_interval& src_int = (*it)->GetInt();
        CRef<CSeq_loc> dst_loc(new CSeq_loc);
        CSeq_interval& dst_int = dst_loc->SetInt();
        dst_int.SetId(m_Dst_loc_Empty.GetNCObject().SetEmpty());
        ENa_strand src_strand =
            src_int.IsSetStrand()? src_int.GetStrand(): eNa_strand_unknown;
        TSeqPos src_from = src_int.GetFrom(), src_to = src_int.GetTo();
        ENa_strand dst_strand;
        TSeqPos dst_from, dst_to;
        if ( !m_Reverse ) {
            dst_strand = src_strand;
            dst_from = m_Shift + src_from;
            dst_to = m_Shift + src_to;
        }
        else {
            dst_strand = Reverse(src_strand);
            dst_from = m_Shift - src_to;
            dst_to = m_Shift - src_from;
        }
        if ( dst_strand != eNa_strand_unknown ) {
            dst_int.SetStrand(dst_strand);
        }
        dst_int.SetFrom(dst_from);
        dst_int.SetTo(dst_to);
        dst_mix.push_back(dst_loc);
    }
}


CRef<CSeq_loc_mix> CSeq_loc_Conversion::GetDstMix(void)
{
    CRef<CSeq_loc_mix> ret(new CSeq_loc_mix);
    CheckDstMix();
    MakeDstMix(*ret, m_SrcLoc->GetMix());
    m_SrcLoc.Reset();
    return ret;
}


void CSeq_loc_Conversion::SetDstLoc(CRef<CSeq_loc>* dst)
{
    CSeq_loc* loc = 0;
    if ( !(*dst) ) {
        switch ( m_LastType ) {
        case eMappedObjType_Seq_interval:
            dst->Reset(loc = new CSeq_loc);
            loc->SetInt(*GetDstInterval());
            break;
        case eMappedObjType_Seq_point:
            dst->Reset(loc = new CSeq_loc);
            loc->SetPnt(*GetDstPoint());
            break;
        case eMappedObjType_Seq_loc_mix:
            dst->Reset(loc = new CSeq_loc);
            loc->SetMix(*GetDstMix());
            break;
        default:
            _ASSERT(0);
            break;
        }
    }
    else {
        _ASSERT(!IsSpecialLoc());
    }
}


void CSeq_loc_Conversion::ConvertPacked_int(const CSeq_loc& src,
                                            CRef<CSeq_loc>* dst)
{
    _ASSERT(src.Which() == CSeq_loc::e_Packed_int);
    const CPacked_seqint::Tdata& src_ints = src.GetPacked_int().Get();
    CPacked_seqint::Tdata* dst_ints = 0;
    bool last_truncated = false;
    ITERATE ( CPacked_seqint::Tdata, i, src_ints ) {
        if ( ConvertInterval(**i) ) {
            if ( !dst_ints ) {
                dst->Reset(new CSeq_loc);
                dst_ints = &(*dst)->SetPacked_int().Set();
            }
            CRef<CSeq_interval> dst_int = GetDstInterval();
            if ( last_truncated  &&
                !dst_int->IsPartialStart(eExtreme_Biological) ) {
                dst_int->SetPartialStart(true, eExtreme_Biological);
            }
            dst_ints->push_back(dst_int);
            last_truncated = false;
        }
        else {
            if ( !last_truncated  &&  *dst  &&
                !(*dst)->IsPartialStop(eExtreme_Biological) ) {
                (*dst)->SetPartialStop(true, eExtreme_Biological);
            }
            last_truncated = true;
        }
    }
}


void CSeq_loc_Conversion::ConvertPacked_pnt(const CSeq_loc& src,
                                            CRef<CSeq_loc>* dst)
{
    _ASSERT(src.Which() == CSeq_loc::e_Packed_pnt);
    const CPacked_seqpnt& src_pack_pnts = src.GetPacked_pnt();
    if ( !GoodSrcId(src_pack_pnts.GetId()) ) {
        if ( m_GraphRanges ) {
            m_GraphRanges->IncOffset(src_pack_pnts.GetPoints().size());
        }
        return;
    }
    const CPacked_seqpnt::TPoints& src_pnts = src_pack_pnts.GetPoints();
    CPacked_seqpnt::TPoints* dst_pnts = 0;
    ITERATE ( CPacked_seqpnt::TPoints, i, src_pnts ) {
        TSeqPos dst_pos = ConvertPos(*i);
        if ( dst_pos != kInvalidSeqPos ) {
            if ( !dst_pnts ) {
                dst->Reset(new CSeq_loc);
                CPacked_seqpnt& pnts = (*dst)->SetPacked_pnt();
                pnts.SetId(GetDstId());
                dst_pnts = &pnts.SetPoints();
                if ( src_pack_pnts.IsSetStrand() ) {
                    pnts.SetStrand(ConvertStrand(src_pack_pnts.GetStrand()));
                }
                if ( src_pack_pnts.IsSetFuzz() ) {
                    CConstRef<CInt_fuzz> fuzz(&src_pack_pnts.GetFuzz());
                    if ( m_Reverse ) {
                        fuzz = ReverseFuzz(*fuzz);
                    }
                    pnts.SetFuzz(const_cast<CInt_fuzz&>(*fuzz));
                }
            }
            dst_pnts->push_back(dst_pos);
            m_TotalRange += TRange(dst_pos, dst_pos);
        }
    }
}


bool CSeq_loc_Conversion::ConvertSimpleMix(const CSeq_loc& src)
{
    const CSeq_loc_mix::Tdata& src_mix = src.GetMix().Get();
    if ( src_mix.empty() ) {
        return false;
    }
    const CSeq_loc& first_loc = **src_mix.begin();
    if ( !first_loc.IsInt() ) {
        return false;
    }
    const CSeq_interval& first_int = first_loc.GetInt();
    ENa_strand src_strand =
        first_int.IsSetStrand()? first_int.GetStrand(): eNa_strand_unknown;
    TSeqPos src_from, src_to;
    if ( !IsReverse(src_strand) ) {
        // forward
        TSeqPos prev_pos = m_Src_from;
        src_from = first_int.GetFrom();
        ITERATE ( CSeq_loc_mix::Tdata, i, src_mix ) {
            const CSeq_loc& loc = **i;
            if ( !loc.IsInt() ) {
                return false;
            }
            const CSeq_interval& cur_int = loc.GetInt();
            if ( cur_int.IsSetFuzz_from() || cur_int.IsSetFuzz_to() ) {
                return false;
            }
            if ( !GoodSrcId(cur_int.GetId()) ) {
                return false;
            }
            ENa_strand strand =
                cur_int.IsSetStrand()? cur_int.GetStrand(): eNa_strand_unknown;
            if ( strand != src_strand ) {
                return false;
            }

            TSeqPos from = cur_int.GetFrom();
            TSeqPos to = cur_int.GetTo();
            if ( to < from || from < prev_pos || to > m_Src_to ) {
                return false;
            }
            prev_pos = to+1;
        }
        src_to = prev_pos-1;
    }
    else {
        TSeqPos prev_pos = m_Src_to;
        src_to = first_int.GetTo();
        ITERATE ( CSeq_loc_mix::Tdata, i, src_mix ) {
            const CSeq_loc& loc = **i;
            if ( !loc.IsInt() ) {
                return false;
            }
            const CSeq_interval& cur_int = loc.GetInt();
            if ( cur_int.IsSetFuzz_from() || cur_int.IsSetFuzz_to() ) {
                return false;
            }
            if ( !GoodSrcId(cur_int.GetId()) ) {
                return false;
            }
            ENa_strand strand =
                cur_int.IsSetStrand()? cur_int.GetStrand(): eNa_strand_unknown;
            if ( strand != src_strand ) {
                return false;
            }

            TSeqPos from = cur_int.GetFrom();
            TSeqPos to = cur_int.GetTo();
            if ( to < from || to > prev_pos || from < m_Src_from ) {
                return false;
            }
            prev_pos = from-1;
        }
        src_from = prev_pos+1;
    }
    ENa_strand dst_strand;
    TSeqPos dst_from, dst_to;
    if ( !m_Reverse ) {
        dst_strand = src_strand;
        dst_from = m_Shift + src_from;
        dst_to = m_Shift + src_to;
    }
    else {
        dst_strand = Reverse(src_strand);
        dst_from = m_Shift - src_to;
        dst_to = m_Shift - src_from;
    }
    m_PartialFlag = 0;
    m_DstFuzz_from.Reset();
    m_DstFuzz_to.Reset();
    m_LastStrand = dst_strand;
    m_LastType = eMappedObjType_Seq_loc_mix;
    m_SrcLoc = &src;
    m_TotalRange += m_LastRange.SetFrom(dst_from).SetTo(dst_to);
    return true;
}


void CSeq_loc_Conversion::ConvertMix(const CSeq_loc& src,
                                     CRef<CSeq_loc>* dst,
                                     EConvertFlag flag)
{
    _ASSERT(src.Which() == CSeq_loc::e_Mix);
    if ( flag != eCnvAlways && ConvertSimpleMix(src) ) {
        return;
    }
    const CSeq_loc_mix::Tdata& src_mix = src.GetMix().Get();
    CSeq_loc_mix::Tdata* dst_mix = 0;
    CRef<CSeq_loc> dst_loc;
    bool last_truncated = false;
    ITERATE ( CSeq_loc_mix::Tdata, i, src_mix ) {
        if ( Convert(**i, &dst_loc, eCnvAlways) ) {
            if ( !dst_mix ) {
                dst->Reset(new CSeq_loc);
                dst_mix = &(*dst)->SetMix().Set();
            }
            _ASSERT(dst_loc);
            if ( last_truncated  &&
                !dst_loc->IsPartialStart(eExtreme_Biological) ) {
                dst_loc->SetPartialStart(true, eExtreme_Biological);
            }
            dst_mix->push_back(dst_loc);
            last_truncated = false;
        }
        else {
            if ( !last_truncated  &&  *dst  &&
                !(*dst)->IsPartialStop(eExtreme_Biological) ) {
                (*dst)->SetPartialStop(true, eExtreme_Biological);
            }
            last_truncated = true;
        }
    }
}


void CSeq_loc_Conversion::ConvertEquiv(const CSeq_loc& src,
                                       CRef<CSeq_loc>* dst)
{
    _ASSERT(src.Which() == CSeq_loc::e_Equiv);
    const CSeq_loc_equiv::Tdata& src_equiv = src.GetEquiv().Get();
    CSeq_loc_equiv::Tdata* dst_equiv = 0;
    CRef<CSeq_loc> dst_loc;
    ITERATE ( CSeq_loc_equiv::Tdata, i, src_equiv ) {
        if ( Convert(**i, &dst_loc, eCnvAlways) ) {
            if ( !dst_equiv ) {
                dst->Reset(new CSeq_loc);
                dst_equiv = &(*dst)->SetEquiv().Set();
            }
            dst_equiv->push_back(dst_loc);
        }
    }
}


void CSeq_loc_Conversion::ConvertBond(const CSeq_loc& src,
                                      CRef<CSeq_loc>* dst)
{
    _ASSERT(src.Which() == CSeq_loc::e_Bond);
    const CSeq_bond& src_bond = src.GetBond();
    CSeq_bond* dst_bond = 0;
    if ( ConvertPoint(src_bond.GetA()) ) {
        dst->Reset(new CSeq_loc);
        dst_bond = &(*dst)->SetBond();
        dst_bond->SetA(*GetDstPoint());
        if ( src_bond.IsSetB() ) {
            dst_bond->SetB().Assign(src_bond.GetB());
        }
    }
    if ( src_bond.IsSetB() ) {
        if ( ConvertPoint(src_bond.GetB()) ) {
            if ( !dst_bond ) {
                dst->Reset(new CSeq_loc);
                dst_bond = &(*dst)->SetBond();
                dst_bond->SetA().Assign(src_bond.GetA());
            }
            dst_bond->SetB(*GetDstPoint());
        }
    }
}


bool CSeq_loc_Conversion::Convert(const CSeq_loc& src,
                                  CRef<CSeq_loc>* dst,
                                  EConvertFlag flag)
{
    dst->Reset();
    CSeq_loc* loc = 0;
    _ASSERT(!IsSpecialLoc());
    m_LastType = eMappedObjType_Seq_loc;
    switch ( src.Which() ) {
    case CSeq_loc::e_not_set:
    case CSeq_loc::e_Feat:
        // Nothing to do, although this should never happen --
        // the seq_loc is intersecting with the conv. loc.
        _ASSERT("this cannot happen" && 0);
        break;
    case CSeq_loc::e_Null:
    {
        dst->Reset(loc = new CSeq_loc);
        loc->SetNull();
        break;
    }
    case CSeq_loc::e_Empty:
    {
        if ( GoodSrcId(src.GetEmpty()) ) {
            dst->Reset(loc = new CSeq_loc);
            loc->SetEmpty(GetDstId());
        }
        break;
    }
    case CSeq_loc::e_Whole:
    {
        const CSeq_id& src_id = src.GetWhole();
        // Convert to the allowed master seq interval
        if ( GoodSrcId(src_id) ) {
            CBioseq_Handle bh =
                m_Scope->GetBioseqHandle(CSeq_id_Handle::GetHandle(src_id),
                                         CScope::eGetBioseq_All);
            ConvertInterval(0, bh.GetBioseqLength()-1, eNa_strand_unknown);
        }
        else if ( m_GraphRanges ) {
            CBioseq_Handle bh =
                m_Scope->GetBioseqHandle(CSeq_id_Handle::GetHandle(src_id),
                                         CScope::eGetBioseq_All);
            m_GraphRanges->IncOffset(bh.GetBioseqLength());
        }
        break;
    }
    case CSeq_loc::e_Int:
    {
        ConvertInterval(src.GetInt());
        break;
    }
    case CSeq_loc::e_Pnt:
    {
        ConvertPoint(src.GetPnt());
        break;
    }
    case CSeq_loc::e_Packed_int:
    {
        ConvertPacked_int(src, dst);
        break;
    }
    case CSeq_loc::e_Packed_pnt:
    {
        ConvertPacked_pnt(src, dst);
        break;
    }
    case CSeq_loc::e_Mix:
    {
        ConvertMix(src, dst, flag);
        break;
    }
    case CSeq_loc::e_Equiv:
    {
        ConvertEquiv(src, dst);
        break;
    }
    case CSeq_loc::e_Bond:
    {
        ConvertBond(src, dst);
        break;
    }
    default:
        NCBI_THROW(CAnnotException, eBadLocation,
                   "Unsupported location type");
    }
    if ( flag == eCnvAlways && IsSpecialLoc() ) {
        SetDstLoc(dst);
    }
    return dst->NotEmpty();
}


void CSeq_loc_Conversion::ConvertCdregion(CAnnotObject_Ref& ref,
                                          const CSeq_feat& orig_feat,
                                          CRef<CSeq_feat>& mapped_feat)
{
    const CAnnotObject_Info& obj = ref.GetAnnotObject_Info();
    _ASSERT( obj.IsFeat() );
    const CSeqFeatData& src_feat_data = orig_feat.GetData();
    _ASSERT( src_feat_data.IsCdregion() );
    if (!src_feat_data.GetCdregion().IsSetCode_break()) {
        return;
    }
    const CCdregion& src_cd = src_feat_data.GetCdregion();
    // Map code-break locations
    const CCdregion::TCode_break& src_cb = src_cd.GetCode_break();
    mapped_feat.Reset(new CSeq_feat);
    // Initialize mapped feature
    ref.GetMappingInfo().InitializeMappedSeq_feat(*obj.GetFeatFast(),
                                                  *mapped_feat);
    
    // Copy Cd-region, do not change the original one
    CRef<CSeqFeatData> new_data(new CSeqFeatData);
    mapped_feat->SetData(*new_data);
    CCdregion& new_cd = new_data->SetCdregion();

    if ( src_cd.IsSetOrf() ) {
        new_cd.SetOrf(src_cd.GetOrf());
    }
    else {
        new_cd.ResetOrf();
    }
    new_cd.SetFrame(src_cd.GetFrame());
    if ( src_cd.IsSetConflict() ) {
        new_cd.SetConflict(src_cd.GetConflict());
    }
    else {
        new_cd.ResetConflict();
    }
    if ( src_cd.IsSetGaps() ) {
        new_cd.SetGaps(src_cd.GetGaps());
    }
    else {
        new_cd.ResetGaps();
    }
    if ( src_cd.IsSetMismatch() ) {
        new_cd.SetMismatch(src_cd.GetMismatch());
    }
    else {
        new_cd.ResetMismatch();
    }
    if ( src_cd.IsSetCode() ) {
        new_cd.SetCode(const_cast<CGenetic_code&>(src_cd.GetCode()));
    }
    else {
        new_cd.ResetCode();
    }
    if ( src_cd.IsSetStops() ) {
        new_cd.SetStops(src_cd.GetStops());
    }
    else {
        new_cd.ResetStops();
    }

    CCdregion::TCode_break& mapped_cbs = new_cd.SetCode_break();
    mapped_cbs.clear();
    ITERATE(CCdregion::TCode_break, it, src_cb) {
        CRef<CSeq_loc> cb_loc;
        Convert((*it)->GetLoc(), &cb_loc, eCnvAlways);
        // Preserve partial flag
        bool partial = m_Partial;
        Reset();
        m_Partial = partial;
        if (cb_loc  &&  cb_loc->Which() != CSeq_loc::e_not_set) {
            CRef<CCode_break> cb(new CCode_break);
            cb->SetAa(const_cast<CCode_break::TAa&>((*it)->GetAa()));
            cb->SetLoc(*cb_loc);
            mapped_cbs.push_back(cb);
        }
        /*else {
            // Keep the original code-break
            CRef<CCode_break> cb(&const_cast<CCode_break&>(**it));
            mapped_cbs.push_back(cb);
        }*/
    }
}


void CSeq_loc_Conversion::ConvertRna(CAnnotObject_Ref& ref,
                                     const CSeq_feat& orig_feat,
                                     CRef<CSeq_feat>& mapped_feat)
{
    const CAnnotObject_Info& obj = ref.GetAnnotObject_Info();
    _ASSERT( obj.IsFeat() );
    const CSeqFeatData& src_feat_data = orig_feat.GetData();
    _ASSERT( src_feat_data.IsRna() );
    if (!src_feat_data.GetRna().IsSetExt()  ||
        !src_feat_data.GetRna().GetExt().IsTRNA()  ||
        !src_feat_data.GetRna().GetExt().GetTRNA().IsSetAnticodon()) {
        return;
    }
    const CRNA_ref::TExt& src_ext = src_feat_data.GetRna().GetExt();
    // Map anticodon location
    const CSeq_loc& src_anticodon = src_ext.GetTRNA().GetAnticodon();
    mapped_feat.Reset(new CSeq_feat);
    // Initialize mapped feature
    ref.GetMappingInfo().InitializeMappedSeq_feat(*obj.GetFeatFast(),
                                                  *mapped_feat);
    
    // Copy RNA-ext, do not change the original one
    CRef<CRNA_ref::TExt> new_ext(new CRNA_ref::TExt);

    // Shallow-copy the feature, replace data.rna.ext.trna.anticodon
    // with the mapped location
    mapped_feat->Assign(*obj.GetFeatFast(), eShallow);
    mapped_feat->SetData(*(new CSeqFeatData));
    mapped_feat->SetData().Assign(src_feat_data, eShallow);
    mapped_feat->SetData().SetRna(*(new CRNA_ref));

    mapped_feat->SetData().SetRna().SetType(src_feat_data.GetRna().GetType());
    if ( src_feat_data.GetRna().IsSetPseudo() ) {
        mapped_feat->SetData().SetRna().SetPseudo(
            src_feat_data.GetRna().GetPseudo());
    }
    else {
        mapped_feat->SetData().SetRna().ResetPseudo();
    }
    mapped_feat->SetData().SetRna().SetExt().SetTRNA().SetAa(
        const_cast<CTrna_ext::C_Aa&>(src_ext.GetTRNA().GetAa()));
    if ( src_ext.GetTRNA().IsSetCodon() ) {
        mapped_feat->SetData().SetRna().SetExt().SetTRNA().SetCodon() =
            src_ext.GetTRNA().GetCodon();
    }
    else {
        mapped_feat->SetData().SetRna().SetExt().SetTRNA().ResetCodon();
    }
    CRef<CSeq_loc> ac_loc;
    Convert(src_anticodon, &ac_loc, eCnvAlways);
    // Preserve partial flag
    bool partial = m_Partial;
    Reset();
    m_Partial = partial;
    if (ac_loc  &&  ac_loc->Which() != CSeq_loc::e_not_set) {
        mapped_feat->SetData()
            .SetRna().SetExt().SetTRNA().SetAnticodon(*ac_loc);
    }
    else {
        mapped_feat->SetData()
            .SetRna().SetExt().SetTRNA().ResetAnticodon();
    }
}


static inline
bool NeedFullFeature(const CAnnotObject_Ref& ref,
                     CSeq_loc_Conversion::ELocationType loctype)
{
    if ( loctype != CSeq_loc_Conversion::eLocation ) {
        return false;
    }
    const CAnnotObject_Info& obj = ref.GetAnnotObject_Info();
    _ASSERT( obj.IsFeat() );
    CSeqFeatData::E_Choice type = obj.GetFeatType();

    if ( type == CSeqFeatData::e_Rna ) {
        if ( !obj.IsRegular() ) {
            return true;
        }
        const CSeqFeatData& data = obj.GetFeatFast()->GetData();
        _ASSERT( data.IsRna() );
        return data.GetRna().IsSetExt()  &&
            data.GetRna().GetExt().IsTRNA()  &&
            data.GetRna().GetExt().GetTRNA().IsSetAnticodon();
    }
    else if ( type == CSeqFeatData::e_Cdregion ) {
        if ( !obj.IsRegular() ) {
            return true;
        }
        const CSeqFeatData& data = obj.GetFeatFast()->GetData();
        _ASSERT( data.IsCdregion() );
        return data.GetCdregion().IsSetCode_break();
    }
    return false;
}


void CSeq_loc_Conversion::ConvertFeature(CAnnotObject_Ref& ref,
                                         const CSeq_feat& orig_feat,
                                         CRef<CSeq_feat>& mapped_feat)
{
    switch ( orig_feat.GetData().Which() ) {
    case CSeqFeatData::e_Cdregion:
        ConvertCdregion(ref, orig_feat, mapped_feat);
        break;
    case CSeqFeatData::e_Rna:
        ConvertRna(ref, orig_feat, mapped_feat);
        break;
    default:
        break;
    }
}


void CSeq_loc_Conversion::Convert(CAnnotObject_Ref& ref, ELocationType loctype)
{
    Reset();
    CAnnotMapping_Info& map_info = ref.GetMappingInfo();
    const CAnnotObject_Info& obj = ref.GetAnnotObject_Info();
    switch ( obj.Which() ) {
    case CSeq_annot::C_Data::e_Ftable:
    {
        if ( NeedFullFeature(ref, loctype) ) {
            CConstRef<CSeq_feat> orig_feat;
            if ( obj.IsRegular() ) {
                orig_feat = obj.GetFeatFast();
            }
            else {
                CRef<CSeq_feat> created_feat;
                CRef<CSeq_point> created_point;
                CRef<CSeq_interval> created_interval;
                const CSeq_annot_Info& annot = obj.GetSeq_annot_Info();
                annot.UpdateTableFeat(created_feat,
                                      created_point,
                                      created_interval,
                                      obj);
                orig_feat = created_feat;
            }
            CRef<CSeq_feat> mapped_feat;
            CRef<CSeq_loc> mapped_loc;
            if ( loctype == eLocation ) {
                ConvertFeature(ref, *orig_feat, mapped_feat);
                Convert(orig_feat->GetLocation(), &mapped_loc);
            }
            else {
                Convert(orig_feat->GetProduct(), &mapped_loc);
            }
            map_info.SetMappedSeq_loc(mapped_loc.GetPointerOrNull());
            if ( mapped_feat ) {
                // SetMappedLocation must be called before SetMappedSeq_feat
                SetMappedLocation(ref, loctype);
                // This will also set location and partial of mapped feature
                map_info.SetMappedSeq_feat(*mapped_feat);
                return;
            }
        }
        else {
            CConstRef<CSeq_loc> orig_loc;
            if ( obj.IsRegular() ) {
                if ( loctype == eLocation ) {
                    orig_loc = &obj.GetFeatFast()->GetLocation();
                }
                else {
                    orig_loc = &obj.GetFeatFast()->GetProduct();
                }
            }
            else {
                CRef<CSeq_loc> created_loc;
                CRef<CSeq_point> created_point;
                CRef<CSeq_interval> created_interval;
                const CSeq_annot_Info& annot = obj.GetSeq_annot_Info();
                if ( loctype == eLocation ) {
                    annot.UpdateTableFeatLocation(created_loc,
                                                  created_point,
                                                  created_interval,
                                                  obj);
                }
                else {
                    annot.UpdateTableFeatProduct(created_loc,
                                                 created_point,
                                                 created_interval,
                                                 obj);
                }
                orig_loc = created_loc;
            }
            CRef<CSeq_loc> mapped_loc;
            Convert(*orig_loc, &mapped_loc);
            map_info.SetMappedSeq_loc(mapped_loc.GetPointerOrNull());
        }
        break;
    }
    case CSeq_annot::C_Data::e_Graph:
    {
        CRef<CSeq_loc> mapped_loc;
        m_GraphRanges.Reset(new CGraphRanges);
        Convert(obj.GetGraphFast()->GetLoc(), &mapped_loc);
        map_info.SetMappedSeq_loc(mapped_loc.GetPointerOrNull());
        map_info.SetGraphRanges(m_GraphRanges.GetPointerOrNull());
        break;
    }
    default:
        _ASSERT(0);
        break;
    }
    SetMappedLocation(ref, loctype);
}


void CSeq_loc_Conversion::Convert(CAnnotObject_Ref& ref,
                                  ELocationType loctype,
                                  const CSeq_id_Handle& id,
                                  const CRange<TSeqPos>& range,
                                  const SAnnotObject_Index& index)
{
    Reset();
    CAnnotMapping_Info& map_info = ref.GetMappingInfo();
    const CAnnotObject_Info& obj = ref.GetAnnotObject_Info();
    switch ( obj.Which() ) {
    case CSeq_annot::C_Data::e_Ftable:
    {
        if ( NeedFullFeature(ref, loctype) ) {
            CConstRef<CSeq_feat> orig_feat;
            if ( obj.IsRegular() ) {
                orig_feat = obj.GetFeatFast();
            }
            else {
                CRef<CSeq_feat> created_feat;
                CRef<CSeq_point> created_point;
                CRef<CSeq_interval> created_interval;
                const CSeq_annot_Info& annot = obj.GetSeq_annot_Info();
                annot.UpdateTableFeat(created_feat,
                                      created_point,
                                      created_interval,
                                      obj);
                orig_feat = created_feat;
            }
            CRef<CSeq_feat> mapped_feat;
            CRef<CSeq_loc> mapped_loc;
            if ( loctype == eLocation ) {
                ConvertFeature(ref, *orig_feat, mapped_feat);
                Convert(orig_feat->GetLocation(), &mapped_loc, eCnvAlways);
            }
            else {
                Convert(orig_feat->GetProduct(), &mapped_loc, eCnvAlways);
            }
            map_info.SetMappedSeq_loc(mapped_loc.GetPointerOrNull());
            if ( mapped_feat ) {
                // SetMappedLocation must be called before SetMappedSeq_feat
                SetMappedLocation(ref, loctype);
                // This will also set location and partial of mapped feature
                map_info.SetMappedSeq_feat(*mapped_feat);
                return;
            }
        }
        else if ( index.LocationIsSimple() ) {
            // simple conversion of location is possible
            // no need for ConvertFeature
            ConvertSimpleLoc(id, range, index);
        }
        else {
            CConstRef<CSeq_loc> orig_loc;
            if ( obj.IsRegular() ) {
                if ( loctype == eLocation ) {
                    orig_loc = &obj.GetFeatFast()->GetLocation();
                }
                else {
                    orig_loc = &obj.GetFeatFast()->GetProduct();
                }
            }
            else {
                CRef<CSeq_loc> created_loc;
                CRef<CSeq_point> created_point;
                CRef<CSeq_interval> created_interval;
                const CSeq_annot_Info& annot = obj.GetSeq_annot_Info();
                if ( loctype == eLocation ) {
                    annot.UpdateTableFeatLocation(created_loc,
                                                  created_point,
                                                  created_interval,
                                                  obj);
                }
                else {
                    annot.UpdateTableFeatProduct(created_loc,
                                                 created_point,
                                                 created_interval,
                                                 obj);
                }
                orig_loc = created_loc;
            }
            CRef<CSeq_loc> mapped_loc;
            Convert(*orig_loc, &mapped_loc);
            map_info.SetMappedSeq_loc(mapped_loc.GetPointerOrNull());
        }
        break;
    }
    case CSeq_annot::C_Data::e_Graph:
    {
        CRef<CSeq_loc> mapped_loc;
        m_GraphRanges.Reset(new CGraphRanges);
        Convert(obj.GetGraphFast()->GetLoc(), &mapped_loc);
        map_info.SetMappedSeq_loc(mapped_loc.GetPointerOrNull());
        map_info.SetGraphRanges(m_GraphRanges.GetPointerOrNull());
        break;
    }
    default:
        _ASSERT(0);
        break;
    }
    SetMappedLocation(ref, loctype);
}


void CSeq_loc_Conversion::SetMappedLocation(CAnnotObject_Ref& ref,
                                            ELocationType loctype)
{
    CAnnotMapping_Info& map_info = ref.GetMappingInfo();
    map_info.SetProduct(loctype == eProduct);
    map_info.SetPartial(m_Partial || map_info.IsPartial());
    map_info.SetTotalRange(m_TotalRange);
    if ( IsSpecialLoc() ) {
        if ( m_DstFuzz_from || m_DstFuzz_to ) {
            CRef<CSeq_loc> mapped_loc;
            SetDstLoc(&mapped_loc);
            map_info.SetMappedSeq_loc(mapped_loc);
        }
        else if ( m_LastType != eMappedObjType_Seq_loc_mix ) {
            // special interval or point
            map_info.SetMappedSeq_id(GetDstId(),
                m_LastType == eMappedObjType_Seq_point);
            map_info.SetMappedStrand(m_LastStrand);
            if ( m_PartialFlag & fPartial_from ) {
                map_info.SetMappedPartial_from();
            }
            if ( m_PartialFlag & fPartial_to ) {
                map_info.SetMappedPartial_to();
            }
        }
        else {
            // special mix
            map_info.SetMappedConverstion(*this);
            map_info.SetMappedStrand(m_LastStrand);
        }
        m_LastType = eMappedObjType_not_set;
    }
    else if ( map_info.GetMappedObjectType() ==
        CAnnotMapping_Info::eMappedObjType_not_set ) {
        if ( m_Partial ) {
            // set empty location
            map_info.SetMappedSeq_loc(m_Dst_loc_Empty);
        }
    }
}


/////////////////////////////////////////////////////////////////////////////
// CSeq_loc_Conversion_Set
/////////////////////////////////////////////////////////////////////////////


CSeq_loc_Conversion_Set::CSeq_loc_Conversion_Set(CHeapScope& scope)
    : m_SingleConv(0),
      m_SingleIndex(0),
      m_Partial(false),
      m_TotalRange(TRange::GetEmpty()),
      m_Scope(scope)
{
    return;
}


void CSeq_loc_Conversion_Set::Add(CSeq_loc_Conversion& cvt,
                                  unsigned int loc_index)
{
    if (!m_SingleConv) {
        m_SingleConv.Reset(&cvt);
        m_SingleIndex = loc_index;
        return;
    }
    else if ( m_CvtByIndex.empty() ) {
        x_Add(*m_SingleConv, m_SingleIndex);
    }
    x_Add(cvt, loc_index);
}


void CSeq_loc_Conversion_Set::x_Add(CSeq_loc_Conversion& cvt,
                                    unsigned int loc_index)
{
    TIdMap& id_map = m_CvtByIndex[loc_index];
    TRangeMap& ranges = id_map[cvt.GetSrc_id_Handle()];
    ranges.insert(TRangeMap::value_type(TRange(cvt.GetSrc_from(),
                                               cvt.GetSrc_to()),
                                        Ref(&cvt)));
}


CSeq_loc_Conversion_Set::TRangeIterator
CSeq_loc_Conversion_Set::BeginRanges(CSeq_id_Handle id,
                                     TSeqPos from,
                                     TSeqPos to,
                                     unsigned int loc_index)
{
    TIdMap::iterator ranges = m_CvtByIndex[loc_index].find(id);
    if (ranges == m_CvtByIndex[loc_index].end()) {
        return TRangeIterator();
    }
    return ranges->second.begin(TRange(from, to));
}


void CSeq_loc_Conversion_Set::ConvertCdregion(CAnnotObject_Ref& ref,
                                              const CSeq_feat& orig_feat,
                                              CRef<CSeq_feat>& mapped_feat)
{
    const CAnnotObject_Info& obj = ref.GetAnnotObject_Info();
    _ASSERT( obj.IsFeat() );
    const CSeqFeatData& src_feat_data = orig_feat.GetData();
    _ASSERT( src_feat_data.IsCdregion() );
    if (!src_feat_data.GetCdregion().IsSetCode_break()) {
        return;
    }
    const CCdregion& src_cd = src_feat_data.GetCdregion();
    // Map code-break locations
    const CCdregion::TCode_break& src_cb = src_cd.GetCode_break();
    mapped_feat.Reset(new CSeq_feat);
    // Initialize mapped feature
    ref.GetMappingInfo().InitializeMappedSeq_feat(*obj.GetFeatFast(),
                                                  *mapped_feat);
    
    // Copy Cd-region, do not change the original one
    CRef<CSeqFeatData> new_data(new CSeqFeatData);
    mapped_feat->SetData(*new_data);
    CCdregion& new_cd = new_data->SetCdregion();

    if ( src_cd.IsSetOrf() ) {
        new_cd.SetOrf(src_cd.GetOrf());
    }
    else {
        new_cd.ResetOrf();
    }
    new_cd.SetFrame(src_cd.GetFrame());
    if ( src_cd.IsSetConflict() ) {
        new_cd.SetConflict(src_cd.GetConflict());
    }
    else {
        new_cd.ResetConflict();
    }
    if ( src_cd.IsSetGaps() ) {
        new_cd.SetGaps(src_cd.GetGaps());
    }
    else {
        new_cd.ResetGaps();
    }
    if ( src_cd.IsSetMismatch() ) {
        new_cd.SetMismatch(src_cd.GetMismatch());
    }
    else {
        new_cd.ResetMismatch();
    }
    if ( src_cd.IsSetCode() ) {
        new_cd.SetCode(const_cast<CGenetic_code&>(src_cd.GetCode()));
    }
    else {
        new_cd.ResetCode();
    }
    if ( src_cd.IsSetStops() ) {
        new_cd.SetStops(src_cd.GetStops());
    }
    else {
        new_cd.ResetStops();
    }

    CCdregion::TCode_break& mapped_cbs = new_cd.SetCode_break();
    mapped_cbs.clear();
    ITERATE(CCdregion::TCode_break, it, src_cb) {
        CRef<CSeq_loc> cb_loc;
        Convert((*it)->GetLoc(), &cb_loc, 0);
        m_TotalRange = TRange::GetEmpty();
        if (cb_loc  &&  cb_loc->Which() != CSeq_loc::e_not_set) {
            CRef<CCode_break> cb(new CCode_break);
            cb->SetAa(const_cast<CCode_break::TAa&>((*it)->GetAa()));
            cb->SetLoc(*cb_loc);
            mapped_cbs.push_back(cb);
        }
    }
}


void CSeq_loc_Conversion_Set::ConvertRna(CAnnotObject_Ref& ref,
                                         const CSeq_feat& orig_feat,
                                         CRef<CSeq_feat>& mapped_feat)
{
    const CAnnotObject_Info& obj = ref.GetAnnotObject_Info();
    _ASSERT( obj.IsFeat() );
    const CSeqFeatData& src_feat_data = orig_feat.GetData();
    _ASSERT( src_feat_data.IsRna() );
    if (!src_feat_data.GetRna().IsSetExt()  ||
        !src_feat_data.GetRna().GetExt().IsTRNA()  ||
        !src_feat_data.GetRna().GetExt().GetTRNA().IsSetAnticodon()) {
        return;
    }
    const CRNA_ref::TExt& src_ext = src_feat_data.GetRna().GetExt();
    // Map anticodon location
    const CSeq_loc& src_anticodon = src_ext.GetTRNA().GetAnticodon();
    mapped_feat.Reset(new CSeq_feat);
    // Initialize mapped feature
    ref.GetMappingInfo().InitializeMappedSeq_feat(*obj.GetFeatFast(),
                                                  *mapped_feat);
    
    // Copy RNA-ext, do not change the original one
    CRef<CRNA_ref::TExt> new_ext(new CRNA_ref::TExt);

    // Shallow-copy the feature, replace data.rna.ext.trna.anticodon
    // with the mapped location
    mapped_feat->Assign(*obj.GetFeatFast(), eShallow);
    mapped_feat->SetData(*(new CSeqFeatData));
    mapped_feat->SetData().Assign(src_feat_data, eShallow);
    mapped_feat->SetData().SetRna(*(new CRNA_ref));

    mapped_feat->SetData().SetRna().SetType(src_feat_data.GetRna().GetType());
    if ( src_feat_data.GetRna().IsSetPseudo() ) {
        mapped_feat->SetData().SetRna().SetPseudo(
            src_feat_data.GetRna().GetPseudo());
    }
    else {
        mapped_feat->SetData().SetRna().ResetPseudo();
    }
    mapped_feat->SetData().SetRna().SetExt().SetTRNA().SetAa(
        const_cast<CTrna_ext::C_Aa&>(src_ext.GetTRNA().GetAa()));
    if ( src_ext.GetTRNA().IsSetCodon() ) {
        mapped_feat->SetData().SetRna().SetExt().SetTRNA().SetCodon() =
            src_ext.GetTRNA().GetCodon();
    }
    else {
        mapped_feat->SetData().SetRna().SetExt().SetTRNA().ResetCodon();
    }
    CRef<CSeq_loc> ac_loc;
    Convert(src_anticodon, &ac_loc, 0);
    // Preserve partial flag
    m_TotalRange = TRange::GetEmpty();
    if (ac_loc  &&  ac_loc->Which() != CSeq_loc::e_not_set) {
        mapped_feat->SetData()
            .SetRna().SetExt().SetTRNA().SetAnticodon(*ac_loc);
    }
    else {
        mapped_feat->SetData()
            .SetRna().SetExt().SetTRNA().ResetAnticodon();
    }
}


void CSeq_loc_Conversion_Set::ConvertFeature(CAnnotObject_Ref& ref,
                                             const CSeq_feat& orig_feat,
                                             CRef<CSeq_feat>& mapped_feat)
{
    switch ( orig_feat.GetData().Which() ) {
    case CSeqFeatData::e_Cdregion:
        ConvertCdregion(ref, orig_feat, mapped_feat);
        break;
    case CSeqFeatData::e_Rna:
        ConvertRna(ref, orig_feat, mapped_feat);
        break;
    default:
        break;
    }
}


void CSeq_loc_Conversion_Set::Convert(CAnnotObject_Ref& ref,
                                      CSeq_loc_Conversion::ELocationType
                                      loctype)
{
    if ( !m_SingleConv ) {
        _ASSERT(m_CvtByIndex.empty());
        // Special case - empty set for filtering duplicates only,
        // no mapping required
        return;
    }
    if ( m_CvtByIndex.empty()  &&  !ref.IsAlign() ) {
        // No multiple mappings
        m_SingleConv->Convert(ref, loctype);
        return;
    }
    CRef<CSeq_feat> mapped_feat;
    CAnnotMapping_Info& map_info = ref.GetMappingInfo();
    const CAnnotObject_Info& obj = ref.GetAnnotObject_Info();
    switch ( obj.Which() ) {
    case CSeq_annot::C_Data::e_Ftable:
    {
        CRef<CSeq_loc> mapped_loc;
        const CSeq_loc* src_loc;
        unsigned int loc_index = 0;
        if ( loctype != CSeq_loc_Conversion::eProduct ) {
            ConvertFeature(ref, *obj.GetFeatFast(), mapped_feat);
            src_loc = &obj.GetFeatFast()->GetLocation();
        }
        else {
            src_loc = &obj.GetFeatFast()->GetProduct();
            loc_index = 1;
        }
        Convert(*src_loc, &mapped_loc, loc_index);
        map_info.SetMappedSeq_loc(mapped_loc.GetPointerOrNull());
        break;
    }
    case CSeq_annot::C_Data::e_Graph:
    {
        CRef<CSeq_loc> mapped_loc;
        m_GraphRanges.Reset(new CGraphRanges);
        Convert(obj.GetGraphFast()->GetLoc(), &mapped_loc, 0);
        map_info.SetMappedSeq_loc(mapped_loc.GetPointerOrNull());
        map_info.SetGraphRanges(m_GraphRanges.GetPointerOrNull());
        break;
    }
    case CSeq_annot::C_Data::e_Align:
    {
        map_info.SetMappedSeq_align_Cvts(*this);
        break;
    }
    default:
        _ASSERT(0);
        break;
    }
    map_info.SetProduct(loctype == CSeq_loc_Conversion::eProduct);
    map_info.SetPartial(m_Partial || map_info.IsPartial());
    map_info.SetTotalRange(m_TotalRange);
    if ( mapped_feat ) {
        // This will also set location and partial of the mapped feature
        map_info.SetMappedSeq_feat(*mapped_feat);
    }
}


bool CSeq_loc_Conversion_Set::ConvertPoint(const CSeq_point& src,
                                           CRef<CSeq_loc>* dst,
                                           unsigned int loc_index)
{
    _ASSERT(*dst);
    bool res = false;
    TRangeIterator mit = BeginRanges(CSeq_id_Handle::GetHandle(src.GetId()),
        src.GetPoint(), src.GetPoint(), loc_index);
    for ( ; mit; ++mit) {
        CSeq_loc_Conversion& cvt = *mit->second;
        cvt.Reset();
        if (cvt.ConvertPoint(src)) {
            (*dst)->SetPnt(*cvt.GetDstPoint());
            m_TotalRange += cvt.GetTotalRange();
            res = true;
            break;
        }
    }
    if ( !res  &&  m_GraphRanges ) {
        m_GraphRanges->IncOffset(1);
    }
    m_Partial |= !res;
    return res;
}


namespace {
    struct FConversions_Less
    {
        bool operator()(const CSeq_loc_Conversion& cvt1,
                        const CSeq_loc_Conversion& cvt2) const
            {
                if ( cvt1.GetSrc_from() != cvt2.GetSrc_from() ) {
                    return cvt1.GetSrc_from() < cvt2.GetSrc_from();
                }
                if ( cvt1.GetSrc_to() != cvt2.GetSrc_to() ) {
                    return cvt1.GetSrc_to() > cvt2.GetSrc_to();
                }
                //return &cvt1 < &cvt2;
                return false;
            }
        bool operator()(const CRef<CSeq_loc_Conversion>& cvt1,
                        const CRef<CSeq_loc_Conversion>& cvt2) const
            {
                return (*this)(*cvt1, *cvt2);
            }
    };

    struct FConversions_ReverseLess
    {
        bool operator()(const CSeq_loc_Conversion& cvt1,
                        const CSeq_loc_Conversion& cvt2) const
            {
                if ( cvt1.GetSrc_to() != cvt2.GetSrc_to() ) {
                    return cvt1.GetSrc_to() > cvt2.GetSrc_to();
                }
                if ( cvt1.GetSrc_from() != cvt2.GetSrc_from() ) {
                    return cvt1.GetSrc_from() < cvt2.GetSrc_from();
                }
                //return &cvt1 < &cvt2;
                return false;
            }
        bool operator()(const CRef<CSeq_loc_Conversion>& cvt1,
                        const CRef<CSeq_loc_Conversion>& cvt2) const
            {
                return (*this)(*cvt1, *cvt2);
            }
    };

    struct FConversions_Equal
    {
        bool operator()(const CSeq_loc_Conversion& cvt1,
                        const CSeq_loc_Conversion& cvt2) const
            {
                return cvt1.GetSrc_from() == cvt2.GetSrc_from()  &&
                    cvt1.GetSrc_to() == cvt2.GetSrc_to();
            }
        bool operator()(const CRef<CSeq_loc_Conversion>& cvt1,
                        const CRef<CSeq_loc_Conversion>& cvt2) const
            {
                return (*this)(*cvt1, *cvt2);
            }
    };
}


bool CSeq_loc_Conversion_Set::ConvertInterval(const CSeq_interval& src,
                                              CRef<CSeq_loc>* dst,
                                              unsigned int loc_index)
{
    _ASSERT(*dst);
    CRef<CSeq_loc> tmp(new CSeq_loc);
    CPacked_seqint::Tdata& ints = tmp->SetPacked_int().Set();
    TRange total_range(TRange::GetEmpty());
    bool revert_order = (src.IsSetStrand() && IsReverse(src.GetStrand()));
    bool res = false;
    typedef vector< CRef<CSeq_loc_Conversion> > TConversions;
    TConversions cvts;
    TRangeIterator mit = BeginRanges(CSeq_id_Handle::GetHandle(src.GetId()),
        src.GetFrom(), src.GetTo(), loc_index);
    for ( ; mit; ++mit) {
        cvts.push_back(mit->second);
    }
    if ( revert_order ) {
        reverse(cvts.begin(), cvts.end());
        stable_sort(cvts.begin(), cvts.end(), FConversions_ReverseLess());
        cvts.erase(unique(cvts.begin(), cvts.end(), FConversions_Equal()),
            cvts.end());
    }
    else {
        stable_sort(cvts.begin(), cvts.end(), FConversions_Less());
        cvts.erase(unique(cvts.begin(), cvts.end(), FConversions_Equal()),
            cvts.end());
    }
    CRef<CSeq_interval> last_int;
    TSeqPos last_to = kInvalidSeqPos;
    TSeqPos graph_offset = m_GraphRanges ? m_GraphRanges->GetOffset() : 0;
    NON_CONST_ITERATE ( TConversions, it, cvts ) {
        CRef<CSeq_loc_Conversion> cvt = *it;
        cvt->Reset();
        cvt->m_GraphRanges = m_GraphRanges;
        if (cvt->ConvertInterval(src)) {
            CRef<CSeq_interval> mapped = cvt->GetDstInterval();
            if ( revert_order ) {
                if (last_int && cvt->GetSrc_to() == last_to - 1) {
                    last_int->SetPartialStop(false, eExtreme_Biological);
                    mapped->SetPartialStart(false, eExtreme_Biological);
                    //last_int->ResetFuzz_from();
                    //mapped->ResetFuzz_to();
                }
                last_to = cvt->GetSrc_from();
            }
            else {
                if (last_int && cvt->GetSrc_from() == last_to + 1) {
                    last_int->SetPartialStop(false, eExtreme_Biological);
                    mapped->SetPartialStart(false, eExtreme_Biological);
                    //last_int->ResetFuzz_to();
                    //mapped->ResetFuzz_from();
                }
                last_to = cvt->GetSrc_to();
            }
            last_int = mapped;
            ints.push_back(mapped);
            total_range += cvt->GetTotalRange();
            res = true;
        }
        if (m_GraphRanges) {
            // All conversions start with the same offset
            m_GraphRanges->SetOffset(graph_offset);
        }
    }
    if ( m_GraphRanges ) {
        // Now it's time to update the offset
        m_GraphRanges->IncOffset(src.GetLength());
    }
    if (ints.size() > 1) {
        dst->Reset(tmp);
    }
    else if (ints.size() == 1) {
        (*dst)->SetInt(**ints.begin());
    }
    m_TotalRange += total_range;
    // does not guarantee the whole interval is mapped, but should work
    // in normal situations
    m_Partial |= (!res  || src.GetLength() > total_range.GetLength());
    return res;
}


bool CSeq_loc_Conversion_Set::ConvertPacked_int(const CSeq_loc& src,
                                                CRef<CSeq_loc>* dst,
                                                unsigned int loc_index)
{
    bool res = false;
    _ASSERT(src.Which() == CSeq_loc::e_Packed_int);
    const CPacked_seqint::Tdata& src_ints = src.GetPacked_int().Get();
    CPacked_seqint::Tdata& dst_ints = (*dst)->SetPacked_int().Set();
    bool last_truncated = false;
    ITERATE ( CPacked_seqint::Tdata, i, src_ints ) {
        CRef<CSeq_loc> dst_int(new CSeq_loc);
        bool mapped = ConvertInterval(**i, &dst_int, loc_index);
        if (mapped) {
            if ( last_truncated  &&
                !dst_int->IsPartialStart(eExtreme_Biological) ) {
                dst_int->SetPartialStart(true, eExtreme_Biological);
            }
            if ( dst_int->IsInt() ) {
                dst_ints.push_back(CRef<CSeq_interval>(&dst_int->SetInt()));
            }
            else if ( dst_int->IsPacked_int() ) {
                dst_ints.splice(dst_ints.end(),
                                dst_int->SetPacked_int().Set());
            }
            else {
                _ASSERT("this cannot happen" && 0);
            }
        }
        else {
            if ( !last_truncated  &&
                !(*dst)->IsPartialStop(eExtreme_Biological) ) {
                (*dst)->SetPartialStop(true, eExtreme_Biological);
            }
        }
        m_Partial |= !mapped;
        res |= mapped;
        last_truncated = !mapped;
    }
    return res;
}


bool CSeq_loc_Conversion_Set::ConvertPacked_pnt(const CSeq_loc& src,
                                                CRef<CSeq_loc>* /* dst */,
                                                unsigned int loc_index)
{
    bool res = false;
    _ASSERT(src.Which() == CSeq_loc::e_Packed_pnt);
    const CPacked_seqpnt& src_pack_pnts = src.GetPacked_pnt();
    const CPacked_seqpnt::TPoints& src_pnts = src_pack_pnts.GetPoints();
    CRef<CSeq_loc> tmp(new CSeq_loc);
    // using mix, not point, since mappings may have
    // different strand, fuzz etc.
    CSeq_loc_mix::Tdata& locs = tmp->SetMix().Set();
    ITERATE ( CPacked_seqpnt::TPoints, i, src_pnts ) {
        bool mapped = false;
        TSeqPos graph_offset = m_GraphRanges ? m_GraphRanges->GetOffset() : 0;
        TRangeIterator mit = BeginRanges(
            CSeq_id_Handle::GetHandle(src_pack_pnts.GetId()),
            *i, *i,
            loc_index);
        for ( ; mit; ++mit) {
            CSeq_loc_Conversion& cvt = *mit->second;
            cvt.Reset();
            if ( !cvt.GoodSrcId(src_pack_pnts.GetId()) ) {
                continue;
            }
            TSeqPos dst_pos = cvt.ConvertPoint(*i,
                src_pack_pnts.IsSetStrand() ?
                src_pack_pnts.GetStrand() : eNa_strand_unknown);
            if ( dst_pos != kInvalidSeqPos ) {
                CRef<CSeq_loc> pnt(new CSeq_loc);
                pnt->SetPnt(*cvt.GetDstPoint());
                _ASSERT(pnt);
                locs.push_back(pnt);
                m_TotalRange += cvt.GetTotalRange();
                mapped = true;
                break;
            }
            if ( m_GraphRanges ) {
                m_GraphRanges->SetOffset(graph_offset);
            }
        }
        if ( m_GraphRanges ) {
            m_GraphRanges->IncOffset(1);
        }
        m_Partial |= !mapped;
        res |= mapped;
    }
    return res;
}


bool CSeq_loc_Conversion_Set::ConvertMix(const CSeq_loc& src,
                                         CRef<CSeq_loc>* dst,
                                         unsigned int loc_index)
{
    bool res = false;
    _ASSERT(src.Which() == CSeq_loc::e_Mix);
    const CSeq_loc_mix::Tdata& src_mix = src.GetMix().Get();
    CRef<CSeq_loc> dst_loc;
    CSeq_loc_mix::Tdata& dst_mix = (*dst)->SetMix().Set();
    bool last_truncated = false;
    ITERATE ( CSeq_loc_mix::Tdata, i, src_mix ) {
        dst_loc.Reset(new CSeq_loc);
        if ( Convert(**i, &dst_loc, loc_index) ) {
            _ASSERT(dst_loc);
            if ( last_truncated  &&
                !dst_loc->IsPartialStart(eExtreme_Biological) ) {
                dst_loc->SetPartialStart(true, eExtreme_Biological);
            }
            dst_mix.push_back(dst_loc);
            res = true;
            last_truncated = false;
        }
        else {
            if ( !last_truncated  &&
                !(*dst)->IsPartialStop(eExtreme_Biological) ) {
                (*dst)->SetPartialStop(true, eExtreme_Biological);
            }
            last_truncated = true;
        }
    }
    m_Partial |= !res;
    return res;
}


bool CSeq_loc_Conversion_Set::ConvertEquiv(const CSeq_loc& src,
                                           CRef<CSeq_loc>* dst,
                                           unsigned int loc_index)
{
    bool res = false;
    _ASSERT(src.Which() == CSeq_loc::e_Equiv);
    const CSeq_loc_equiv::Tdata& src_equiv = src.GetEquiv().Get();
    CRef<CSeq_loc> dst_loc;
    CSeq_loc_equiv::Tdata& dst_equiv = (*dst)->SetEquiv().Set();
    ITERATE ( CSeq_loc_equiv::Tdata, i, src_equiv ) {
        if ( Convert(**i, &dst_loc, loc_index) ) {
            dst_equiv.push_back(dst_loc);
            res = true;
        }
    }
    m_Partial |= !res;
    return res;
}


bool CSeq_loc_Conversion_Set::ConvertBond(const CSeq_loc& src,
                                          CRef<CSeq_loc>* dst,
                                          unsigned int loc_index)
{
    bool res = false;
    _ASSERT(src.Which() == CSeq_loc::e_Bond);
    const CSeq_bond& src_bond = src.GetBond();
    // using mix, not bond, since mappings may have
    // different strand, fuzz etc.
    (*dst)->SetBond();
    CRef<CSeq_point> pntA;
    CRef<CSeq_point> pntB;
    {{
        TRangeIterator mit = BeginRanges(
            CSeq_id_Handle::GetHandle(src_bond.GetA().GetId()),
            src_bond.GetA().GetPoint(), src_bond.GetA().GetPoint(),
            loc_index);
        for ( ; mit  &&  !pntA; ++mit) {
            CSeq_loc_Conversion& cvt = *mit->second;
            cvt.Reset();
            if (cvt.ConvertPoint(src_bond.GetA())) {
                pntA = cvt.GetDstPoint();
                m_TotalRange += cvt.GetTotalRange();
                res = true;
            }
        }
    }}
    if ( src_bond.IsSetB() ) {
        TRangeIterator mit = BeginRanges(
            CSeq_id_Handle::GetHandle(src_bond.GetB().GetId()),
            src_bond.GetB().GetPoint(), src_bond.GetB().GetPoint(),
            loc_index);
        for ( ; mit  &&  !pntB; ++mit) {
            CSeq_loc_Conversion& cvt = *mit->second;
            cvt.Reset();
            if (!pntB  &&  cvt.ConvertPoint(src_bond.GetB())) {
                pntB = cvt.GetDstPoint();
                m_TotalRange += cvt.GetTotalRange();
                res = true;
            }
        }
    }
    CSeq_bond& dst_bond = (*dst)->SetBond();
    if ( pntA  ||  pntB ) {
        if ( pntA ) {
            dst_bond.SetA(*pntA);
        }
        else {
            dst_bond.SetA().Assign(src_bond.GetA());
        }
        if ( pntB ) {
            dst_bond.SetB(*pntB);
        }
        else if ( src_bond.IsSetB() ) {
            dst_bond.SetB().Assign(src_bond.GetB());
        }
    }
    m_Partial |= (!pntA  ||  !pntB);
    return res;
}


bool CSeq_loc_Conversion_Set::Convert(const CSeq_loc& src,
                                      CRef<CSeq_loc>* dst,
                                      unsigned int loc_index)
{
    dst->Reset(new CSeq_loc);
    bool res = false;
    switch ( src.Which() ) {
    case CSeq_loc::e_not_set:
    case CSeq_loc::e_Feat:
        // Nothing to do, although this should never happen --
        // the seq_loc is intersecting with the conv. loc.
        _ASSERT("this cannot happen" && 0);
        break;
    case CSeq_loc::e_Null:
    {
        (*dst)->SetNull();
        res = true;
        break;
    }
    case CSeq_loc::e_Empty:
    {
        TRangeIterator mit = BeginRanges(CSeq_id_Handle::GetHandle(src.GetEmpty()),
                                         TRange::GetWhole().GetFrom(),
                                         TRange::GetWhole().GetTo(),
                                         loc_index);
        for ( ; mit; ++mit) {
            CSeq_loc_Conversion& cvt = *mit->second;
            cvt.Reset();
            if ( cvt.GoodSrcId(src.GetEmpty()) ) {
                (*dst)->SetEmpty(cvt.GetDstId());
                res = true;
                break;
            }
        }
        break;
    }
    case CSeq_loc::e_Whole:
    {
        const CSeq_id& src_id = src.GetWhole();
        // Convert to the allowed master seq interval
        CSeq_interval whole_int;
        whole_int.SetId().Assign(src_id);
        whole_int.SetFrom(0);
        CBioseq_Handle bh =
            m_Scope->GetBioseqHandle(CSeq_id_Handle::GetHandle(src_id),
                                     CScope::eGetBioseq_All);
        whole_int.SetTo(bh.GetBioseqLength());
        res = ConvertInterval(whole_int, dst, loc_index);
        break;
    }
    case CSeq_loc::e_Int:
    {
        res = ConvertInterval(src.GetInt(), dst, loc_index);
        break;
    }
    case CSeq_loc::e_Pnt:
    {
        res = ConvertPoint(src.GetPnt(), dst, loc_index);
        break;
    }
    case CSeq_loc::e_Packed_int:
    {
        res = ConvertPacked_int(src, dst, loc_index);
        break;
    }
    case CSeq_loc::e_Packed_pnt:
    {
        res = ConvertPacked_pnt(src, dst, loc_index);
        break;
    }
    case CSeq_loc::e_Mix:
    {
        res = ConvertMix(src, dst, loc_index);
        break;
    }
    case CSeq_loc::e_Equiv:
    {
        res = ConvertEquiv(src, dst, loc_index);
        break;
    }
    case CSeq_loc::e_Bond:
    {
        res = ConvertBond(src, dst, loc_index);
        break;
    }
    default:
        NCBI_THROW(CAnnotException, eBadLocation,
                   "Unsupported location type");
    }
    return res;
}


void CSeq_loc_Conversion_Set::Convert(const CSeq_align& src,
                                      CRef<CSeq_align>* dst)
{
    CSeq_loc_Mapper loc_mapper(0, &m_Scope.GetScope());
    CSeq_align_Mapper mapper(src, loc_mapper);
    mapper.Convert(*this);
    *dst = mapper.GetDstAlign();
}


END_SCOPE(objects)
END_NCBI_SCOPE
