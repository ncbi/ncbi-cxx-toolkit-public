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
*   Class for mapping Seq-loc petween sequences.
*
*/

#include <ncbi_pch.hpp>
#include <objmgr/impl/seq_loc_cvt.hpp>

#include <objmgr/impl/seq_align_mapper.hpp>
#include <objmgr/seq_map_ci.hpp>
#include <objmgr/impl/scope_impl.hpp>
#include <objmgr/annot_types_ci.hpp>
#include <objmgr/impl/annot_object.hpp>

#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Seq_point.hpp>
#include <objects/seqloc/Seq_loc_equiv.hpp>
#include <objects/seqloc/Seq_bond.hpp>
#include <objects/seqfeat/Seq_feat.hpp>

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


CSeq_loc_Conversion::CSeq_loc_Conversion(const CSeq_id_Handle& master_id,
                                         CScope*               scope)
    : m_Src_id_Handle(master_id),
      m_Src_from(0),
      m_Src_to(kInvalidSeqPos - 1),
      m_Shift(0),
      m_Reverse(false),
      m_Dst_id_Handle(master_id),
      m_Dst_loc_Empty(0),
      m_Partial(false),
      m_LastType(eMappedObjType_not_set),
      m_LastStrand(eNa_strand_unknown),
      m_Scope(scope)
{
    m_Dst_loc_Empty.Reset(new CSeq_loc);
    m_Dst_loc_Empty->SetEmpty().Assign(*master_id.GetSeqId());
    Reset();
}


CSeq_loc_Conversion::~CSeq_loc_Conversion(void)
{
    _ASSERT(!IsSpecialLoc());
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


bool CSeq_loc_Conversion::ConvertPoint(TSeqPos src_pos,
                                       ENa_strand src_strand)
{
    _ASSERT(!IsSpecialLoc());
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
    return true;
}


bool CSeq_loc_Conversion::ConvertInterval(TSeqPos src_from, TSeqPos src_to,
                                          ENa_strand src_strand)
{
    _ASSERT(!IsSpecialLoc());
    if ( src_from < m_Src_from ) {
        m_Partial = true;
        src_from = m_Src_from;
    }
    if ( src_to > m_Src_to ) {
        m_Partial = true;
        src_to = m_Src_to;
    }
    if ( src_from > src_to ) {
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
    }
    m_LastType = eMappedObjType_Seq_interval;
    m_TotalRange += m_LastRange.SetFrom(dst_from).SetTo(dst_to);
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
    if ( m_LastStrand != eNa_strand_unknown ) {
        point.SetStrand(m_LastStrand);
    }
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
    ITERATE ( CPacked_seqint::Tdata, i, src_ints ) {
        if ( ConvertInterval(**i) ) {
            if ( !dst_ints ) {
                dst->Reset(new CSeq_loc);
                dst_ints = &(*dst)->SetPacked_int().Set();
            }
            dst_ints->push_back(GetDstInterval());
        }
    }
}


void CSeq_loc_Conversion::ConvertPacked_pnt(const CSeq_loc& src,
                                            CRef<CSeq_loc>* dst)
{
    _ASSERT(src.Which() == CSeq_loc::e_Packed_pnt);
    const CPacked_seqpnt& src_pack_pnts = src.GetPacked_pnt();
    if ( !GoodSrcId(src_pack_pnts.GetId()) ) {
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
            }
            dst_pnts->push_back(dst_pos);
            m_TotalRange += TRange(dst_pos, dst_pos);
        }
    }
}


void CSeq_loc_Conversion::ConvertMix(const CSeq_loc& src,
                                     CRef<CSeq_loc>* dst)
{
    _ASSERT(src.Which() == CSeq_loc::e_Mix);
    const CSeq_loc_mix::Tdata& src_mix = src.GetMix().Get();
    CSeq_loc_mix::Tdata* dst_mix = 0;
    CRef<CSeq_loc> dst_loc;
    ITERATE ( CSeq_loc_mix::Tdata, i, src_mix ) {
        if ( Convert(**i, &dst_loc, eCnvAlways) ) {
            if ( !dst_mix ) {
                dst->Reset(new CSeq_loc);
                dst_mix = &(*dst)->SetMix().Set();
            }
            _ASSERT(dst_loc);
            dst_mix->push_back(dst_loc);
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


bool CSeq_loc_Conversion::Convert(const CSeq_loc& src, CRef<CSeq_loc>* dst,
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
            CBioseq_Handle bh = m_Scope->GetBioseqHandle(src_id,
                CScope::eGetBioseq_All);
            ConvertInterval(0, bh.GetBioseqLength()-1, eNa_strand_unknown);
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
        ConvertMix(src, dst);
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
    return *dst;
}


void CSeq_loc_Conversion::Convert(CAnnotObject_Ref& ref, ELocationType loctype)
{
    Reset();
    const CAnnotObject_Info& obj = ref.GetAnnotObject_Info();
    switch ( obj.Which() ) {
    case CSeq_annot::C_Data::e_Ftable:
    {
        CRef<CSeq_loc> mapped_loc;
        const CSeq_loc* src_loc;
        if ( loctype != eProduct ) {
            src_loc = &obj.GetFeatFast()->GetLocation();
        }
        else {
            src_loc = &obj.GetFeatFast()->GetProduct();
        }
        Convert(*src_loc, &mapped_loc);
        ref.SetMappedSeq_loc(mapped_loc.GetPointerOrNull());
        break;
    }
    case CSeq_annot::C_Data::e_Graph:
    {
        CRef<CSeq_loc> mapped_loc;
        Convert(obj.GetGraphFast()->GetLoc(), &mapped_loc);
        ref.SetMappedSeq_loc(mapped_loc.GetPointerOrNull());
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
    ref.SetProduct(loctype == eProduct);
    ref.SetPartial(m_Partial || ref.IsPartial());
    ref.SetTotalRange(m_TotalRange);
    if ( IsSpecialLoc() ) {
        // special interval or point
        ref.SetMappedSeq_id(GetDstId(),
                            m_LastType == eMappedObjType_Seq_point);
        ref.SetMappedStrand(m_LastStrand);
        m_LastType = eMappedObjType_not_set;
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
    else if (m_CvtByIndex.size() == 0) {
        TIdMap& id_map = m_CvtByIndex[m_SingleIndex];
        TRangeMap& ranges = id_map[m_SingleConv->m_Src_id_Handle];
        ranges.insert(TRangeMap::value_type
                      (TRange(cvt.m_Src_from, cvt.m_Src_to),
                       m_SingleConv));
    }
    TIdMap& id_map = m_CvtByIndex[loc_index];
    TRangeMap& ranges = id_map[cvt.m_Src_id_Handle];
    ranges.insert(TRangeMap::value_type(TRange(cvt.m_Src_from, cvt.m_Src_to),
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


void CSeq_loc_Conversion_Set::Convert(CAnnotObject_Ref& ref,
                                      CSeq_loc_Conversion::ELocationType
                                      loctype)
{
    _ASSERT(m_SingleConv);
    if (m_CvtByIndex.size() == 0  &&
        !ref.IsAlign()) {
        // No multiple mappings
        m_SingleConv->Convert(ref, loctype);
        return;
    }
    const CAnnotObject_Info& obj = ref.GetAnnotObject_Info();
    switch ( obj.Which() ) {
    case CSeq_annot::C_Data::e_Ftable:
    {
        CRef<CSeq_loc> mapped_loc;
        const CSeq_loc* src_loc;
        unsigned int loc_index = 0;
        if ( loctype != CSeq_loc_Conversion::eProduct ) {
            src_loc = &obj.GetFeatFast()->GetLocation();
        }
        else {
            src_loc = &obj.GetFeatFast()->GetProduct();
            loc_index = 1;
        }
        Convert(*src_loc, &mapped_loc, loc_index);
        ref.SetMappedSeq_loc(mapped_loc.GetPointerOrNull());
        break;
    }
    case CSeq_annot::C_Data::e_Graph:
    {
        CRef<CSeq_loc> mapped_loc;
        Convert(obj.GetGraphFast()->GetLoc(), &mapped_loc, 0);
        ref.SetMappedSeq_loc(mapped_loc.GetPointerOrNull());
        break;
    }
    case CSeq_annot::C_Data::e_Align:
    {
        ref.SetMappedSeq_align_Cvts(*this);
        break;
    }
    default:
        _ASSERT(0);
        break;
    }
    ref.SetProduct(loctype == CSeq_loc_Conversion::eProduct);
    ref.SetPartial(m_Partial);
    ref.SetTotalRange(m_TotalRange);
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
    m_Partial |= !res;
    return res;
}


bool CSeq_loc_Conversion_Set::ConvertInterval(const CSeq_interval& src,
                                              CRef<CSeq_loc>* dst,
                                              unsigned int loc_index)
{
    _ASSERT(*dst);
    CRef<CSeq_loc> tmp(new CSeq_loc);
    CPacked_seqint::Tdata& ints = tmp->SetPacked_int().Set();
    TRange total_range(TRange::GetEmpty());
    bool revert_order = (src.IsSetStrand()
        && src.GetStrand() == eNa_strand_minus);
    bool res = false;
    TRangeIterator mit = BeginRanges(CSeq_id_Handle::GetHandle(src.GetId()),
        src.GetFrom(), src.GetTo(), loc_index);
    for ( ; mit; ++mit) {
        CSeq_loc_Conversion& cvt = *mit->second;
        cvt.Reset();
        if (cvt.ConvertInterval(src)) {
            if (revert_order) {
                ints.push_front(cvt.GetDstInterval());
            }
            else {
                ints.push_back(cvt.GetDstInterval());
            }
            total_range += cvt.GetTotalRange();
            res = true;
        }
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
    ITERATE ( CPacked_seqint::Tdata, i, src_ints ) {
        CRef<CSeq_loc> dst_int(new CSeq_loc);
        bool mapped = ConvertInterval(**i, &dst_int, loc_index);
        if (mapped) {
            if ( dst_int->IsInt() ) {
                dst_ints.push_back(CRef<CSeq_interval>(&dst_int->SetInt()));
            }
            else if ( dst_int->IsPacked_int() ) {
                CPacked_seqint::Tdata& splitted = dst_int->SetPacked_int().Set();
                dst_ints.merge(splitted);
            }
            else {
                _ASSERT("this cannot happen" && 0);
            }
        }
        m_Partial |= !mapped;
        res |= mapped;
    }
    return res;
}


bool CSeq_loc_Conversion_Set::ConvertPacked_pnt(const CSeq_loc& src,
                                                CRef<CSeq_loc>* dst,
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
            TSeqPos dst_pos = cvt.ConvertPos(*i);
            if ( dst_pos != kInvalidSeqPos ) {
                CRef<CSeq_loc> pnt(new CSeq_loc);
                pnt->SetPnt(*cvt.GetDstPoint());
                _ASSERT(pnt);
                locs.push_back(pnt);
                m_TotalRange += cvt.GetTotalRange();
                mapped = true;
                break;
            }
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
    ITERATE ( CSeq_loc_mix::Tdata, i, src_mix ) {
        dst_loc.Reset(new CSeq_loc);
        if ( Convert(**i, &dst_loc, loc_index) ) {
            _ASSERT(dst_loc);
            dst_mix.push_back(dst_loc);
            res = true;
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
        for ( ; mit  &&  !bool(pntA); ++mit) {
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
        for ( ; mit  &&  !bool(pntB); ++mit) {
            CSeq_loc_Conversion& cvt = *mit->second;
            cvt.Reset();
            if (!bool(pntB)  &&  cvt.ConvertPoint(src_bond.GetB())) {
                pntB = cvt.GetDstPoint();
                m_TotalRange += cvt.GetTotalRange();
                res = true;
            }
        }
    }
    CSeq_bond& dst_bond = (*dst)->SetBond();
    if ( bool(pntA)  ||  bool(pntB) ) {
        if ( bool(pntA) ) {
            dst_bond.SetA(*pntA);
        }
        else {
            dst_bond.SetA().Assign(src_bond.GetA());
        }
        if ( bool(pntB) ) {
            dst_bond.SetB(*pntB);
        }
        else if ( src_bond.IsSetB() ) {
            dst_bond.SetB().Assign(src_bond.GetB());
        }
    }
    m_Partial |= (!bool(pntA)  ||  !bool(pntB));
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
        CBioseq_Handle bh = m_Scope->GetBioseqHandle(src_id,
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
    CRef<CSeq_align_Mapper> mapper(new CSeq_align_Mapper(src));
    mapper->Convert(*this);
    *dst = mapper->GetDstAlign();
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.35  2004/08/13 18:34:28  grichenk
* Fixed conversion collecting
*
* Revision 1.34  2004/08/11 16:41:32  vasilche
* Assign instead of |= for single bool value.
*
* Revision 1.33  2004/08/04 14:53:26  vasilche
* Revamped object manager:
* 1. Changed TSE locking scheme
* 2. TSE cache is maintained by CDataSource.
* 3. CObjectManager::GetInstance() doesn't hold CRef<> on the object manager.
* 4. Fixed processing of split data.
*
* Revision 1.32  2004/07/19 17:41:34  grichenk
* Simplified switches in Convert() methods.
*
* Revision 1.31  2004/07/19 14:24:00  grichenk
* Simplified and fixed mapping through annot.locs
*
* Revision 1.30  2004/06/07 17:01:17  grichenk
* Implemented referencing through locs annotations
*
* Revision 1.29  2004/05/26 14:29:20  grichenk
* Redesigned CSeq_align_Mapper: preserve non-mapping intervals,
* fixed strands handling, improved performance.
*
* Revision 1.28  2004/05/21 21:42:13  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.27  2004/05/10 18:26:37  grichenk
* Fixed 'not used' warnings
*
* Revision 1.26  2004/04/13 15:59:35  grichenk
* Added CScope::GetBioseqHandle() with id resolving flag.
*
* Revision 1.25  2004/03/30 21:21:09  grichenk
* Reduced number of includes.
*
* Revision 1.24  2004/03/30 15:42:33  grichenk
* Moved alignment mapper to separate file, added alignment mapping
* to CSeq_loc_Mapper.
*
* Revision 1.23  2004/03/16 15:47:28  vasilche
* Added CBioseq_set_Handle and set of EditHandles
*
* Revision 1.22  2004/02/23 15:23:16  grichenk
* Removed unused members
*
* Revision 1.21  2004/02/19 19:25:09  vasilche
* Hidded implementation of CAnnotObject_Ref.
* Added necessary access methods.
*
* Revision 1.20  2004/02/19 17:18:44  vasilche
* Formatting + use CRef<> assignment directly.
*
* Revision 1.19  2004/02/06 16:07:27  grichenk
* Added CBioseq_Handle::GetSeq_entry_Handle()
* Fixed MapLocation()
*
* Revision 1.18  2004/02/05 22:49:19  grichenk
* Added default cases in switches
*
* Revision 1.17  2004/02/05 20:18:37  grichenk
* Fixed std-segs processing
*
* Revision 1.16  2004/02/02 14:44:54  vasilche
* Removed several compilation warnings.
*
* Revision 1.15  2004/01/30 15:25:45  grichenk
* Fixed alignments mapping and sorting
*
* Revision 1.14  2004/01/28 22:09:35  grichenk
* Added CSeq_align_Mapper initialization for e_Disc
*
* Revision 1.13  2004/01/28 20:54:36  vasilche
* Fixed mapping of annotations.
*
* Revision 1.12  2004/01/23 22:13:48  ucko
* Fix variable shadowing that some compilers treated as an error.
*
* Revision 1.11  2004/01/23 16:14:48  grichenk
* Implemented alignment mapping
*
* Revision 1.10  2004/01/02 16:06:53  grichenk
* Skip location mapping for seq-aligns
*
* Revision 1.9  2003/12/04 20:04:24  grichenk
* Fixed bugs in seq-loc converters.
*
* Revision 1.8  2003/11/10 18:11:04  grichenk
* Moved CSeq_loc_Conversion_Set to seq_loc_cvt
*
* Revision 1.7  2003/11/04 16:21:37  grichenk
* Updated CAnnotTypes_CI to map whole features instead of splitting
* them by sequence segments.
*
* Revision 1.6  2003/10/29 19:55:47  vasilche
* Avoid making 'whole' features on 'whole' segment partial (by Aleksey Grichenko)
*
* Revision 1.5  2003/09/30 16:22:03  vasilche
* Updated internal object manager classes to be able to load ID2 data.
* SNP blobs are loaded as ID2 split blobs - readers convert them automatically.
* Scope caches results of requests for data to data loaders.
* Optimized CSeq_id_Handle for gis.
* Optimized bioseq lookup in scope.
* Reduced object allocations in annotation iterators.
* CScope is allowed to be destroyed before other objects using this scope are
* deleted (feature iterators, bioseq handles etc).
* Optimized lookup for matching Seq-ids in CSeq_id_Mapper.
* Added 'adaptive' option to objmgr_demo application.
*
* Revision 1.4  2003/09/05 17:29:40  grichenk
* Structurized Object Manager exceptions
*
* Revision 1.3  2003/08/27 14:29:52  vasilche
* Reduce object allocations in feature iterator.
*
* Revision 1.2  2003/08/15 19:19:16  vasilche
* Fixed memory leak in string packing hooks.
* Fixed processing of 'partial' flag of features.
* Allow table packing of non-point SNP.
* Allow table packing of SNP with long alleles.
*
* Revision 1.1  2003/08/14 20:05:19  vasilche
* Simple SNP features are stored as table internally.
* They are recreated when needed using CFeat_CI.
*
* ===========================================================================
*/
