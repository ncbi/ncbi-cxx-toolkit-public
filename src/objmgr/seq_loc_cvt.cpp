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

#include <objmgr/impl/seq_loc_cvt.hpp>

#include <objmgr/seq_map_ci.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/annot_types_ci.hpp>
#include <objmgr/objmgr_exception.hpp>

#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Seq_point.hpp>
#include <objects/seqloc/Seq_loc_equiv.hpp>
#include <objects/seqloc/Seq_bond.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqres/Seq_graph.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seqalign/seqalign__.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

/////////////////////////////////////////////////////////////////////////////
// CSeq_loc_Conversion
/////////////////////////////////////////////////////////////////////////////

CSeq_loc_Conversion::CSeq_loc_Conversion(const CSeq_id_Handle& master_id,
                                         CSeq_loc& master_loc_empty,
                                         const CSeqMap_CI& seg,
                                         const CSeq_id_Handle& src_id,
                                         CScope* scope)
    : m_Src_id_Handle(src_id),
      m_Src_length(kInvalidSeqPos),
      m_Src_from(0),
      m_Src_to(0),
      m_Shift(0),
      m_Reverse(false),
      m_Dst_id(&master_loc_empty.SetEmpty()),
      m_Dst_loc_Empty(&master_loc_empty),
      m_Partial(false),
      m_LastType(eMappedObjType_not_set),
      m_LastStrand(eNa_strand_unknown),
      m_Scope(scope)
{
    SetConversion(seg);
    Reset();
}


CSeq_loc_Conversion::~CSeq_loc_Conversion(void)
{
    _ASSERT(!IsSpecialLoc());
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
            CBioseq_Handle bh = m_Scope->GetBioseqHandle(src_id);
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
        const CPacked_seqint::Tdata& src_ints = src.GetPacked_int().Get();
        CPacked_seqint::Tdata* dst_ints = 0;
        ITERATE ( CPacked_seqint::Tdata, i, src_ints ) {
            if ( ConvertInterval(**i) ) {
                if ( !dst_ints ) {
                    dst->Reset(loc = new CSeq_loc);
                    dst_ints = &loc->SetPacked_int().Set();
                }
                dst_ints->push_back(GetDstInterval());
            }
        }
        break;
    }
    case CSeq_loc::e_Packed_pnt:
    {
        const CPacked_seqpnt& src_pack_pnts = src.GetPacked_pnt();
        if ( !GoodSrcId(src_pack_pnts.GetId()) ) {
            break;
        }
        const CPacked_seqpnt::TPoints& src_pnts = src_pack_pnts.GetPoints();
        CPacked_seqpnt::TPoints* dst_pnts = 0;
        ITERATE ( CPacked_seqpnt::TPoints, i, src_pnts ) {
            TSeqPos dst_pos = ConvertPos(*i);
            if ( dst_pos != kInvalidSeqPos ) {
                if ( !dst_pnts ) {
                    dst->Reset(loc = new CSeq_loc);
                    CPacked_seqpnt& pnts = loc->SetPacked_pnt();
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
        break;
    }
    case CSeq_loc::e_Mix:
    {
        const CSeq_loc_mix::Tdata& src_mix = src.GetMix().Get();
        CSeq_loc_mix::Tdata* dst_mix = 0;
        CRef<CSeq_loc> dst_loc;
        ITERATE ( CSeq_loc_mix::Tdata, i, src_mix ) {
            if ( Convert(**i, &dst_loc, eCnvAlways) ) {
                if ( !dst_mix ) {
                    dst->Reset(loc = new CSeq_loc);
                    dst_mix = &loc->SetMix().Set();
                }
                _ASSERT(dst_loc);
                dst_mix->push_back(dst_loc);
            }
        }
        break;
    }
    case CSeq_loc::e_Equiv:
    {
        const CSeq_loc_equiv::Tdata& src_equiv = src.GetEquiv().Get();
        CSeq_loc_equiv::Tdata* dst_equiv = 0;
        CRef<CSeq_loc> dst_loc;
        ITERATE ( CSeq_loc_equiv::Tdata, i, src_equiv ) {
            if ( Convert(**i, &dst_loc, eCnvAlways) ) {
                if ( !dst_equiv ) {
                    dst->Reset(loc = new CSeq_loc);
                    dst_equiv = &loc->SetEquiv().Set();
                }
                dst_equiv->push_back(dst_loc);
            }
        }
        break;
    }
    case CSeq_loc::e_Bond:
    {
        const CSeq_bond& src_bond = src.GetBond();
        CSeq_bond* dst_bond = 0;
        if ( ConvertPoint(src_bond.GetA()) ) {
            dst->Reset(loc = new CSeq_loc);
            dst_bond = &loc->SetBond();
            dst_bond->SetA(*GetDstPoint());
            if ( src_bond.IsSetB() ) {
                dst_bond->SetB().Assign(src_bond.GetB());
            }
        }
        if ( src_bond.IsSetB() ) {
            if ( ConvertPoint(src_bond.GetB()) ) {
                if ( !dst_bond ) {
                    dst->Reset(loc = new CSeq_loc);
                    dst_bond = &loc->SetBond();
                    dst_bond->SetA().Assign(src_bond.GetA());
                }
                dst_bond->SetB(*GetDstPoint());
            }
        }
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
    case CSeq_annot::C_Data::e_Align:
    {
        const CSeq_align& orig_align = ref.GetAlign();
        CRef<CSeq_align_Mapper> mapped_align;
        Convert(orig_align, &mapped_align);
        ref.SetMappedSeq_align_Mapper(mapped_align.GetPointerOrNull());
        return;
    }
    default:
        _ASSERT(0);
        break;
    }
    SetMappedLocation(ref, loctype);
}


CSeq_align_Mapper::CSeq_align_Mapper(const CSeq_align& align)
    : m_OrigAlign(&align),
      m_DstAlign(0),
      m_HaveStrands(false)
{
    switch ( align.GetSegs().Which() ) {
    case CSeq_align::C_Segs::e_Dendiag:
        x_Init(align.GetSegs().GetDendiag());
        break;
    case CSeq_align::C_Segs::e_Denseg:
        x_Init(align.GetSegs().GetDenseg());
        break;
    case CSeq_align::C_Segs::e_Std:
        x_Init(align.GetSegs().GetStd());
        break;
    case CSeq_align::C_Segs::e_Packed:
        x_Init(align.GetSegs().GetPacked());
        break;
    case CSeq_align::C_Segs::e_Disc:
        x_Init(align.GetSegs().GetDisc());
        break;
    }
}


void CSeq_align_Mapper::x_Init(const TDendiag& diags)
{
    ITERATE(TDendiag, diag_it, diags) {
        const CDense_diag& diag = **diag_it;
        int dim = diag.GetDim();
        if (dim != (int)diag.GetIds().size()) {
            ERR_POST(Warning << "Invalid 'ids' size in dendiag");
            dim = min(dim, (int)diag.GetIds().size());
        }
        if (dim != (int)diag.GetStarts().size()) {
            ERR_POST(Warning << "Invalid 'starts' size in dendiag");
            dim = min(dim, (int)diag.GetStarts().size());
        }
        m_HaveStrands = diag.IsSetStrands();
        if (m_HaveStrands && dim != (int)diag.GetStrands().size()) {
            ERR_POST(Warning << "Invalid 'strands' size in dendiag");
            dim = min(dim, (int)diag.GetStrands().size());
        }
        SAlignment_Segment seg(diag.GetLen());
        CDense_diag::TIds::const_iterator id_it = diag.GetIds().begin();
        CDense_diag::TStarts::const_iterator start_it = diag.GetStarts().begin();
        CDense_diag::TStrands::const_iterator strand_it;
        if ( m_HaveStrands ) {
            strand_it = diag.GetStrands().begin();
        }
        ENa_strand strand = eNa_strand_unknown;
        for (int i = 0; i < dim; ++i) {
            if ( m_HaveStrands ) {
                strand = *strand_it;
                ++strand_it;
            }
            seg.AddRow(**id_it, *start_it, m_HaveStrands, strand, 0);
            ++id_it;
            ++start_it;
        }
        m_SrcSegs.push_back(seg);
    }
}


void CSeq_align_Mapper::x_Init(const CDense_seg& denseg)
{
    int dim    = denseg.GetDim();
    int numseg = denseg.GetNumseg();
    // claimed dimension may not be accurate :-/
    if (numseg != (int)denseg.GetLens().size()) {
        ERR_POST(Warning << "Invalid 'lens' size in denseg");
        numseg = min(numseg, (int)denseg.GetLens().size());
    }
    if (dim != (int)denseg.GetIds().size()) {
        ERR_POST(Warning << "Invalid 'ids' size in denseg");
        dim = min(dim, (int)denseg.GetIds().size());
    }
    if (dim*numseg != (int)denseg.GetStarts().size()) {
        ERR_POST(Warning << "Invalid 'starts' size in denseg");
        dim = min(dim*numseg, (int)denseg.GetStarts().size()) / numseg;
    }
    m_HaveStrands = denseg.IsSetStrands();
    if (m_HaveStrands && dim*numseg != (int)denseg.GetStrands().size()) {
        ERR_POST(Warning << "Invalid 'strands' size in denseg");
        dim = min(dim*numseg, (int)denseg.GetStrands().size()) / numseg;
    }
    CDense_seg::TStarts::const_iterator start_it =
        denseg.GetStarts().begin();
    CDense_seg::TLens::const_iterator len_it =
        denseg.GetLens().begin();
    CDense_seg::TStrands::const_iterator strand_it;
    if ( m_HaveStrands ) {
        strand_it = denseg.GetStrands().begin();
    }
    ENa_strand strand = eNa_strand_unknown;
    for (int seg = 0;  seg < numseg;  seg++, ++len_it) {
        CDense_seg::TIds::const_iterator id_it =
            denseg.GetIds().begin();
        SAlignment_Segment alnseg(*len_it);
        for (int seq = 0;  seq < dim;  seq++, ++start_it, ++id_it) {
            if ( m_HaveStrands ) {
                strand = *strand_it;
                ++strand_it;
            }
            alnseg.AddRow(**id_it, *start_it, m_HaveStrands, strand, 0);
        }
        m_SrcSegs.push_back(alnseg);
    }
}


void CSeq_align_Mapper::x_Init(const TStd& sseg)
{
    typedef map<CSeq_id_Handle, int> TSegLenMap;
    TSegLenMap seglens;

    ITERATE ( CSeq_align::C_Segs::TStd, it, sseg ) {
        const CStd_seg& stdseg = **it;
        int dim = stdseg.GetDim();
        if (dim != stdseg.GetLoc().size()) {
            ERR_POST(Warning << "Invalid 'loc' size in std-seg");
            dim = min(dim, (int)stdseg.GetLoc().size());
        }
        if (stdseg.IsSetIds()
            && dim != stdseg.GetIds().size()) {
            ERR_POST(Warning << "Invalid 'ids' size in std-seg");
            dim = min(dim, (int)stdseg.GetIds().size());
        }
        SAlignment_Segment seg(0); // length is unknown yet
        int seg_len = 0;
        bool multi_width = false;
        ITERATE ( CStd_seg::TLoc, it_loc, (*it)->GetLoc() ) {
            const CSeq_loc& loc = **it_loc;
            const CSeq_id* id = 0;
            int start = -1;
            int len = 0;
            ENa_strand strand = eNa_strand_unknown;
            switch ( loc.Which() ) {
            case CSeq_loc::e_Empty:
                id = &loc.GetEmpty();
                break;
            case CSeq_loc::e_Int:
                id = &loc.GetInt().GetId();
                start = loc.GetInt().GetFrom();
                len = loc.GetInt().GetTo() - start + 1; // ???
                if ( loc.GetInt().IsSetStrand() ) {
                    strand = loc.GetInt().GetStrand();
                    m_HaveStrands = true;
                }
            default:
                NCBI_THROW(CAnnotException, eBadLocation,
                           "Unsupported location type in std-seg");
            }
            if (len > 0) {
                if (seg_len == 0) {
                    seg_len = len;
                }
                else if (len != seg_len) {
                    multi_width = true;
                    if (len/3 == seg_len) {
                        seg_len = len;
                    }
                    else if (len*3 != seg_len) {
                        NCBI_THROW(CAnnotException, eBadLocation,
                                   "Invalid segment length in std-seg");
                    }
                }
            }
            seglens[seg.AddRow(*id, len, m_HaveStrands, strand, 0).m_Id] = len;
        }
        seg.m_Len = seg_len;
        if ( multi_width ) {
            // Adjust each segment width. Do not check if sequence always
            // has the same width.
            for (int i = 0; i < seg.m_Rows.size(); ++i) {
                if (seglens[seg.m_Rows[i].m_Id] != seg_len) {
                    seg.m_Rows[i].m_Width = 3;
                }
            }
        }
        seglens.clear();
        m_SrcSegs.push_back(seg);
    }
}


void CSeq_align_Mapper::x_Init(const CPacked_seg& pseg)
{
    int dim    = pseg.GetDim();
    int numseg = pseg.GetNumseg();
    // claimed dimension may not be accurate :-/
    if (numseg != (int)pseg.GetLens().size()) {
        ERR_POST(Warning << "Invalid 'lens' size in packed-seg");
        numseg = min(numseg, (int)pseg.GetLens().size());
    }
    if (dim != (int)pseg.GetIds().size()) {
        ERR_POST(Warning << "Invalid 'ids' size in packed-seg");
        dim = min(dim, (int)pseg.GetIds().size());
    }
    if (dim*numseg != (int)pseg.GetStarts().size()) {
        ERR_POST(Warning << "Invalid 'starts' size in packed-seg");
        dim = min(dim*numseg, (int)pseg.GetStarts().size()) / numseg;
    }
    m_HaveStrands = pseg.IsSetStrands();
    if (m_HaveStrands && dim*numseg != (int)pseg.GetStrands().size()) {
        ERR_POST(Warning << "Invalid 'strands' size in packed-seg");
        dim = min(dim*numseg, (int)pseg.GetStrands().size()) / numseg;
    }
    CPacked_seg::TStarts::const_iterator start_it =
        pseg.GetStarts().begin();
    CPacked_seg::TLens::const_iterator len_it =
        pseg.GetLens().begin();
    CPacked_seg::TStrands::const_iterator strand_it;
    if ( m_HaveStrands ) {
        strand_it = pseg.GetStrands().begin();
    }
    ENa_strand strand = eNa_strand_unknown;
    CPacked_seg::TPresent::const_iterator pr_it =
        pseg.GetPresent().begin();
    for (int seg = 0;  seg < numseg;  seg++, ++len_it) {
        CPacked_seg::TIds::const_iterator id_it =
            pseg.GetIds().begin();
        SAlignment_Segment alnseg(*len_it);
        for (int seq = 0;  seq < dim;  seq++, ++start_it, ++id_it, ++pr_it) {
            if ( m_HaveStrands ) {
                strand = *strand_it;
                ++strand_it;
            }
            alnseg.AddRow(**id_it, (*pr_it ? *start_it : -1),
                m_HaveStrands, strand, 0);
        }
        m_SrcSegs.push_back(alnseg);
    }
}


void CSeq_align_Mapper::x_Init(const CSeq_align_set& align_set)
{
    const CSeq_align_set::Tdata& data = align_set.Get();
    ITERATE(CSeq_align_set::Tdata, it, data) {
        m_SubAligns.push_back(CSeq_align_Mapper(**it));
    }
}


bool CSeq_align_Mapper::x_IsValidAlign(TSegments segments)
{
    // Each segment must contain the same set of IDs,
    // in the same order. If width is set for an ID,
    // it should be the same over all intervals on the ID.
    typedef vector<int> TWidths;
    TWidths wid;

    if ( !segments.size() ) {
        return false; // empty alignment is not valid
    }
    SAlignment_Segment seg0 = segments[0];
    for (int nrow = 0; nrow < seg0.m_Rows.size(); ++nrow) {
        wid.push_back(seg0.m_Rows[nrow].m_Width);
    }
    for (int nseg = 1; nseg < segments.size(); ++nseg) {
        // skip for dendiags
        if (segments[nseg].m_Rows.size() != seg0.m_Rows.size()) {
            return false;
        }

        for (int nrow = 0; nrow < seg0.m_Rows.size(); ++nrow) {
            // skip for dendiags
            if (seg0.m_Rows[nrow].m_Id != segments[nseg].m_Rows[nrow].m_Id) {
                return false;
            }
            if (seg0.m_Rows[nrow].m_Width != 0) {
                if (wid[nrow] == 0) {
                    wid[nrow] = seg0.m_Rows[nrow].m_Width;
                }
                if (seg0.m_Rows[nrow].m_Width != wid[nrow]) {
                    return false;
                }
            }
        }
    }
    return true;
}


void CSeq_align_Mapper::x_MapSegment(SAlignment_Segment& sseg,
                                     int row_idx,
                                     CSeq_loc_Conversion& cvt)
{
    // start and stop on the original sequence
    int start = sseg.m_Rows[row_idx].m_Start;
    int stop = start + sseg.m_Len;
    if ( start >= 0
        &&  cvt.ConvertInterval(start, stop, sseg.m_Rows[row_idx].m_Strand) ) {
        // At least part of the interval was converted. Calculate
        // trimming coords, trim each row.
        int dl = cvt.m_Src_from <= start ? 0 : cvt.m_Src_from - start;
        int dr = cvt.m_Src_to >= stop ? 0 : cvt.m_Src_to - stop;
        SAlignment_Segment dseg(sseg.m_Len - dl + dr);
        for (int i = 0; i < sseg.m_Rows.size(); ++i) {
            if (i == row_idx) {
                // translate id and coords
                dseg.AddRow(cvt.GetDstId(),
                    cvt.m_LastRange.GetFrom(),
                    sseg.m_Rows[i].m_IsSetStrand,
                    cvt.m_LastStrand,
                    sseg.m_Rows[i].m_Width)
                    .SetMapped();
            }
            else {
                int dst_start = sseg.m_Rows[i].m_Start < 0 ? -1
                    : sseg.m_Rows[i].m_Start + dl;
                dseg.AddRow(*sseg.m_Rows[i].m_Id.GetSeqId(),
                    dst_start,
                    sseg.m_Rows[i].m_IsSetStrand, sseg.m_Rows[i].m_Strand,
                    sseg.m_Rows[i].m_Width);
            }
        }
        sseg.m_Mappings.push_back(dseg);
        cvt.m_LastType = cvt.eMappedObjType_not_set;
    }
    else {
        // Empty mapping. Remove the row.
        SAlignment_Segment dseg = sseg;
        dseg.m_Rows[row_idx].m_Id =
            CSeq_id_Mapper::GetSeq_id_Mapper().GetHandle(*cvt.m_Dst_id);
        dseg.m_Rows[row_idx].m_Start = -1;
        sseg.m_Mappings.push_back(dseg);
    }
}


bool CSeq_align_Mapper::x_ConvertSegments(TSegments& segs,
                                          CSeq_loc_Conversion& cvt)
{
    bool res = false;
    NON_CONST_ITERATE(TSegments, ss, segs) {
        // Check already mapped parts
        if (ss->m_Mappings.size() > 0) {
            if ( x_ConvertSegments(ss->m_Mappings, cvt) ) {
                // Don't use source segment if the ID was
                // found in mappings.
                res = true;
                continue;
            }
        }
        // Check the original segment - some IDs may be mapped more than once.
        // This should not happen if the ID is present in mappings.
        for (int row = 0; row < ss->m_Rows.size(); ++row) {
            if (ss->m_Rows[row].m_Id == cvt.m_Src_id_Handle) {
                x_MapSegment(*ss, row, cvt);
                res = true;
            }
        }
    }
    return res;
}


void CSeq_align_Mapper::Convert(CSeq_loc_Conversion& cvt)
{
    _ASSERT(m_SubAligns.size() > 0  ||  x_IsValidAlign(m_SrcSegs));

    m_DstAlign.Reset();

    if (m_SubAligns.size() > 0) {
        NON_CONST_ITERATE(TSubAligns, it, m_SubAligns) {
            it->Convert(cvt);
        }
        return;
    }
    x_ConvertSegments(m_SrcSegs, cvt);
}


void CSeq_align_Mapper::x_GetDstSegments(const TSegments& ssegs,
                                         TSegments& dsegs) const
{
    ITERATE(TSegments, ss, ssegs) {
        if (ss->m_Mappings.size() == 0) {
            // umapped segment, use as-is
            dsegs.push_back(*ss);
            continue;
        }
        x_GetDstSegments(ss->m_Mappings, dsegs);
    }
}


CRef<CSeq_align> CSeq_align_Mapper::GetDstAlign(void) const
{
    if (m_DstAlign) {
        return m_DstAlign;
    }

    m_DstSegs.clear();
    x_GetDstSegments(m_SrcSegs, m_DstSegs);

    _ASSERT(m_SubAligns.size() > 0  ||  x_IsValidAlign(m_DstSegs));
    CRef<CSeq_align> dst(new CSeq_align);
    dst->Assign(*m_OrigAlign);
    dst->ResetScore();
    dst->SetSegs().Reset();
    switch ( m_OrigAlign->GetSegs().Which() ) {
    case CSeq_align::C_Segs::e_Dendiag:
        {
            TDendiag& diags = dst->SetSegs().SetDendiag();
            ITERATE(TSegments, seg_it, m_DstSegs) {
                const SAlignment_Segment& seg = *seg_it;
                CRef<CDense_diag> diag(new CDense_diag);
                // scores ???
                diag->SetDim(seg.m_Rows.size());
                diag->SetLen(seg.m_Len);
                ITERATE(SAlignment_Segment::TRows, row, seg.m_Rows) {
                    CRef<CSeq_id> id(new CSeq_id);
                    id->Assign(*row->m_Id.GetSeqId());
                    diag->SetIds().push_back(id);
                    diag->SetStarts().push_back(row->m_Start);
                    if (seg.m_HaveStrands) { // per-segment strands
                        diag->SetStrands().push_back(row->m_Strand);
                    }
                }
                diags.push_back(diag);
            }
            break;
        }
    case CSeq_align::C_Segs::e_Denseg:
        {
            CDense_seg& dseg = dst->SetSegs().SetDenseg();
            // dseg.SetScores().Assign(m_OrigAnnot->GetDenseg().GetScores();
            dseg.SetDim(m_DstSegs[0].m_Rows.size());
            dseg.SetNumseg(m_DstSegs.size());
            ITERATE(SAlignment_Segment::TRows, row, m_DstSegs[0].m_Rows) {
                CRef<CSeq_id> id(new CSeq_id);
                id->Assign(*row->m_Id.GetSeqId());
                dseg.SetIds().push_back(id);
                dseg.SetWidths().push_back(row->m_Width ? row->m_Width : 1);
            }
            ITERATE(TSegments, seg_it, m_DstSegs) {
                dseg.SetLens().push_back(seg_it->m_Len);
                ITERATE(SAlignment_Segment::TRows, row, seg_it->m_Rows) {
                    dseg.SetStarts().push_back(row->m_Start);
                    if (m_HaveStrands) { // per-alignment strands
                        dseg.SetStrands().push_back(row->m_Strand);
                    }
                }
            }
            break;
        }
    case CSeq_align::C_Segs::e_Std:
        {
            TStd& std_segs = dst->SetSegs().SetStd();
            ITERATE(TSegments, seg_it, m_DstSegs) {
                CRef<CStd_seg> std_seg(new CStd_seg);
                // scores ???
                std_seg->SetDim(seg_it->m_Rows.size());
                ITERATE(SAlignment_Segment::TRows, row, seg_it->m_Rows) {
                    CRef<CSeq_id> id(new CSeq_id);
                    id->Assign(*row->m_Id.GetSeqId());
                    std_seg->SetIds().push_back(id);
                    CRef<CSeq_loc> loc(new CSeq_loc);
                    if (row->m_Start < 0) {
                        // empty
                        loc->SetEmpty(*id);
                    }
                    else {
                        // interval
                        loc->SetInt().SetId(*id);
                        loc->SetInt().SetFrom(row->m_Start);
                        loc->SetInt().SetTo(row->m_Start + seg_it->m_Len);
                        if (row->m_IsSetStrand) {
                            loc->SetInt().SetStrand(row->m_Strand);
                        }
                    }
                    std_seg->SetLoc().push_back(loc);
                }
                std_segs.push_back(std_seg);
            }
            break;
        }
    case CSeq_align::C_Segs::e_Packed:
        {
            CPacked_seg& pseg = dst->SetSegs().SetPacked();
            // pseg.SetScores().Assign(m_OrigAnnot->GetPacked().GetScores();
            pseg.SetDim(m_DstSegs[0].m_Rows.size());
            pseg.SetNumseg(m_DstSegs.size());
            ITERATE(SAlignment_Segment::TRows, row, m_DstSegs[0].m_Rows) {
                CRef<CSeq_id> id(new CSeq_id);
                id->Assign(*row->m_Id.GetSeqId());
                pseg.SetIds().push_back(id);
            }
            ITERATE(TSegments, seg_it, m_DstSegs) {
                pseg.SetLens().push_back(seg_it->m_Len);
                ITERATE(SAlignment_Segment::TRows, row, seg_it->m_Rows) {
                    pseg.SetStarts().push_back(row->m_Start);
                    pseg.SetPresent().push_back(row->m_Start >= 0);
                    if (m_HaveStrands) {
                        pseg.SetStrands().push_back(row->m_Strand);
                    }
                }
            }
            break;
        }
    case CSeq_align::C_Segs::e_Disc:
        {
            CSeq_align_set::Tdata& data = dst->SetSegs().SetDisc().Set();
            ITERATE(TSubAligns, it, m_SubAligns) {
                data.push_back(it->GetDstAlign());
            }
            break;
        }
    }
    return m_DstAlign = dst;
}


void CSeq_loc_Conversion::Convert(const CSeq_align& src, CRef<CSeq_align_Mapper>* dst)
{
    dst->Reset(new CSeq_align_Mapper(src));
    (*dst)->Convert(*this);
}


void CSeq_loc_Conversion::SetMappedLocation(CAnnotObject_Ref& ref,
                                            ELocationType loctype)
{
    ref.SetProduct(loctype == eProduct);
    ref.SetPartial(m_Partial || ref.IsPartial());
    ref.m_TotalRange = m_TotalRange;
    if ( IsSpecialLoc() ) {
        // special interval or point
        ref.SetMappedSeq_id(*m_Dst_id, m_LastType == eMappedObjType_Seq_point);
        m_LastType = eMappedObjType_not_set;
        ref.m_MappedStrand = m_LastStrand;
    }
}


/////////////////////////////////////////////////////////////////////////////
// CSeq_loc_Conversion_Set
/////////////////////////////////////////////////////////////////////////////


CSeq_loc_Conversion_Set::CSeq_loc_Conversion_Set(void)
: m_Partial(false), m_TotalRange(TRange::GetEmpty())
{
}


void CSeq_loc_Conversion_Set::Add(CSeq_loc_Conversion& cvt)
{
    TRangeMap& ranges = m_IdMap[cvt.m_Src_id_Handle];
    ranges.insert(TRangeMap::value_type(TRange(cvt.m_Src_from, cvt.m_Src_to), Ref(&cvt)));
}


CSeq_loc_Conversion_Set::TRangeIterator
CSeq_loc_Conversion_Set::BeginRanges(CSeq_id_Handle id,
                                     TSeqPos from,
                                     TSeqPos to)
{
    TIdMap::iterator ranges = m_IdMap.find(id);
    if (ranges == m_IdMap.end()) {
        return TRangeIterator();
    }
    return ranges->second.begin(TRange(from, to));
}


void CSeq_loc_Conversion_Set::Convert(CAnnotObject_Ref& ref,
                                      CSeq_loc_Conversion::ELocationType loctype)
{
    if (m_IdMap.size() == 1  &&  m_IdMap.begin()->second.size() == 1) {
        // No multiple mappings
        m_IdMap.begin()->second.begin()->second->Convert(ref, loctype);
        return;
    }
    const CAnnotObject_Info& obj = ref.GetAnnotObject_Info();
    switch ( obj.Which() ) {
    case CSeq_annot::C_Data::e_Ftable:
    {
        CRef<CSeq_loc> mapped_loc;
        const CSeq_loc* src_loc;
        if ( loctype != CSeq_loc_Conversion::eProduct ) {
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
    case CSeq_annot::C_Data::e_Align:
    {
        const CSeq_align& orig_align = ref.GetAlign();
        CRef<CSeq_align_Mapper> mapped_align;
        Convert(orig_align, &mapped_align);
        ref.SetMappedSeq_align_Mapper(mapped_align.GetPointerOrNull());
        break;
    }
    default:
        _ASSERT(0);
        break;
    }
    ref.SetProduct(loctype == CSeq_loc_Conversion::eProduct);
    ref.SetPartial(m_Partial);
    ref.m_TotalRange = m_TotalRange;
}


bool CSeq_loc_Conversion_Set::ConvertPoint(const CSeq_point& src,
                                           CRef<CSeq_loc>* dst)
{
    _ASSERT(*dst);
    bool res = false;
    TRangeIterator mit = BeginRanges(CSeq_id_Handle::GetHandle(src.GetId()),
        src.GetPoint(), src.GetPoint());
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
                                              CRef<CSeq_loc>* dst)
{
    _ASSERT(*dst);
    CRef<CSeq_loc> tmp(new CSeq_loc);
    CPacked_seqint::Tdata& ints = tmp->SetPacked_int().Set();
    TRange total_range(TRange::GetEmpty());
    bool revert_order = (src.IsSetStrand()
        && src.GetStrand() == eNa_strand_minus);
    bool res = false;
    TRangeIterator mit = BeginRanges(CSeq_id_Handle::GetHandle(src.GetId()),
        src.GetFrom(), src.GetTo());
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


bool CSeq_loc_Conversion_Set::Convert(const CSeq_loc& src, CRef<CSeq_loc>* dst)
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
            TRange::GetWhole().GetFrom(), TRange::GetWhole().GetTo());
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
        CBioseq_Handle bh = m_Scope->GetBioseqHandle(src_id);
        whole_int.SetTo(bh.GetBioseqLength());
        res = ConvertInterval(whole_int, dst);
        break;
    }
    case CSeq_loc::e_Int:
    {
        res = ConvertInterval(src.GetInt(), dst);
        break;
    }
    case CSeq_loc::e_Pnt:
    {
        res = ConvertPoint(src.GetPnt(), dst);
        break;
    }
    case CSeq_loc::e_Packed_int:
    {
        const CPacked_seqint::Tdata& src_ints = src.GetPacked_int().Get();
        CPacked_seqint::Tdata& dst_ints = (*dst)->SetPacked_int().Set();
        ITERATE ( CPacked_seqint::Tdata, i, src_ints ) {
            CRef<CSeq_loc> dst_int(new CSeq_loc);
            bool mapped = ConvertInterval(**i, &dst_int);
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
        break;
    }
    case CSeq_loc::e_Packed_pnt:
    {
        const CPacked_seqpnt& src_pack_pnts = src.GetPacked_pnt();
        const CPacked_seqpnt::TPoints& src_pnts = src_pack_pnts.GetPoints();
        CRef<CSeq_loc> tmp(new CSeq_loc);
        // using mix, not point, since mappings may have
        // different strand, fuzz etc.
        CSeq_loc_mix::Tdata& locs = tmp->SetMix().Set();
        ITERATE ( CPacked_seqpnt::TPoints, i, src_pnts ) {
            bool mapped = false;
            TRangeIterator mit = BeginRanges(
                CSeq_id_Handle::GetHandle(src_pack_pnts.GetId()), *i, *i);
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
        break;
    }
    case CSeq_loc::e_Mix:
    {
        const CSeq_loc_mix::Tdata& src_mix = src.GetMix().Get();
        CRef<CSeq_loc> dst_loc;
        CSeq_loc_mix::Tdata& dst_mix = (*dst)->SetMix().Set();
        ITERATE ( CSeq_loc_mix::Tdata, i, src_mix ) {
            dst_loc.Reset(new CSeq_loc);
            if ( Convert(**i, &dst_loc) ) {
                _ASSERT(dst_loc);
                dst_mix.push_back(dst_loc);
                res = true;
            }
        }
        m_Partial |= !res;
        break;
    }
    case CSeq_loc::e_Equiv:
    {
        const CSeq_loc_equiv::Tdata& src_equiv = src.GetEquiv().Get();
        CRef<CSeq_loc> dst_loc;
        CSeq_loc_equiv::Tdata& dst_equiv = (*dst)->SetEquiv().Set();
        ITERATE ( CSeq_loc_equiv::Tdata, i, src_equiv ) {
            if ( Convert(**i, &dst_loc) ) {
                dst_equiv.push_back(dst_loc);
                res = true;
            }
        }
        m_Partial |= !res;
        break;
    }
    case CSeq_loc::e_Bond:
    {
        const CSeq_bond& src_bond = src.GetBond();
        // using mix, not bond, since mappings may have
        // different strand, fuzz etc.
        (*dst)->SetBond();
        CRef<CSeq_point> pntA(0);
        CRef<CSeq_point> pntB(0);
        TRangeIterator mit = BeginRanges(
            CSeq_id_Handle::GetHandle(src_bond.GetA().GetId()),
            src_bond.GetA().GetPoint(), src_bond.GetA().GetPoint());
        for ( ; mit  &&  !bool(pntA); ++mit) {
            CSeq_loc_Conversion& cvt = *mit->second;
            cvt.Reset();
            if (cvt.ConvertPoint(src_bond.GetA())) {
                pntA.Reset(cvt.GetDstPoint());
                m_TotalRange += cvt.GetTotalRange();
                res = true;
            }
        }
        if ( src_bond.IsSetB() ) {
            TRangeIterator mit = BeginRanges(
                CSeq_id_Handle::GetHandle(src_bond.GetB().GetId()),
                src_bond.GetB().GetPoint(), src_bond.GetB().GetPoint());
            for ( ; mit  &&  !bool(pntB); ++mit) {
                CSeq_loc_Conversion& cvt = *mit->second;
                cvt.Reset();
                if (!bool(pntB)  &&  cvt.ConvertPoint(src_bond.GetB())) {
                    pntB.Reset(cvt.GetDstPoint());
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
        break;
    }
    default:
        NCBI_THROW(CAnnotException, eBadLocation,
                   "Unsupported location type");
    }
    return res;
}


void CSeq_loc_Conversion_Set::Convert(const CSeq_align& src,
                                      CRef<CSeq_align_Mapper>* dst)
{
    dst->Reset(new CSeq_align_Mapper(src));
    NON_CONST_ITERATE(TIdMap, id_it, m_IdMap) {
        NON_CONST_ITERATE(TRangeMap, range_it, id_it->second) {
            range_it->second->Reset();
            (*dst)->Convert(*range_it->second);
            m_TotalRange += range_it->second->GetTotalRange();
        }
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
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
