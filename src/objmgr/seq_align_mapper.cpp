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
*   Alignment mapper
*
*/

#include <objmgr/impl/seq_align_mapper.hpp>
#include <objmgr/impl/seq_loc_cvt.hpp>
#include <objmgr/seq_id_mapper.hpp>
#include <objmgr/seq_loc_mapper.hpp>
#include <objmgr/objmgr_exception.hpp>
#include <objects/seqalign/seqalign__.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


SAlignment_Segment::SAlignment_Segment(int len)
    : m_Len(len),
      m_HaveStrands(false)
{
    return;
}


SAlignment_Segment::SAlignment_Row& SAlignment_Segment::AddRow(const CSeq_id& id,
                                                               int start,
                                                               bool is_set_strand,
                                                               ENa_strand strand,
                                                               int width)
{
    m_Rows.push_back(SAlignment_Row(id, start, is_set_strand, strand, width));
    m_HaveStrands |= is_set_strand;
    return m_Rows.back();
}


CSeq_align_Mapper::CSeq_align_Mapper(const CSeq_align& align)
    : m_OrigAlign(&align),
      m_DstAlign(0),
      m_HaveStrands(false),
      m_HaveWidths(false)
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
    default:
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
    m_HaveWidths = denseg.IsSetWidths();
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
            int width = m_HaveWidths ? denseg.GetWidths()[seq] : 0;
            alnseg.AddRow(**id_it, *start_it, m_HaveStrands, strand, width);
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
        size_t dim = stdseg.GetDim();
        if (dim != stdseg.GetLoc().size()) {
            ERR_POST(Warning << "Invalid 'loc' size in std-seg");
            dim = min(dim, stdseg.GetLoc().size());
        }
        if (stdseg.IsSetIds()
            && dim != stdseg.GetIds().size()) {
            ERR_POST(Warning << "Invalid 'ids' size in std-seg");
            dim = min(dim, stdseg.GetIds().size());
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
            bool have_strand = false;
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
                    m_HaveStrands = have_strand = true;
                }
                break;
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
            seglens[seg.AddRow(*id, start, have_strand, strand, 0).m_Id] = len;
        }
        seg.m_Len = seg_len;
        if ( multi_width ) {
            // Adjust each segment width. Do not check if sequence always
            // has the same width.
            for (size_t i = 0; i < seg.m_Rows.size(); ++i) {
                if (seglens[seg.m_Rows[i].m_Id] != seg_len) {
                    seg.m_Rows[i].m_Width = 3;
                }
            }
        }
        seglens.clear();
        m_SrcSegs.push_back(seg);
        m_HaveWidths |= multi_width;
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
            alnseg.AddRow(**id_it, (*pr_it ? *start_it : kInvalidSeqPos),
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


bool CSeq_align_Mapper::x_IsValidAlign(TSegments segments) const
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
    if (m_HaveWidths) {
        for (size_t nrow = 0; nrow < seg0.m_Rows.size(); ++nrow) {
            wid.push_back(seg0.m_Rows[nrow].m_Width);
        }
    }
    for (size_t nseg = 1; nseg < segments.size(); ++nseg) {
        // skip for dendiags
        if (segments[nseg].m_Rows.size() != seg0.m_Rows.size()) {
            return false;
        }

        for (size_t nrow = 0; nrow < seg0.m_Rows.size(); ++nrow) {
            // skip for dendiags and std-segs
            if ( !m_OrigAlign->GetSegs().IsStd() ) {
                if (seg0.m_Rows[nrow].m_Id != segments[nseg].m_Rows[nrow].m_Id) {
                    return false;
                }
            }
            if (m_HaveWidths) {
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
    }
    return true;
}


void CSeq_align_Mapper::x_MapSegment(SAlignment_Segment& sseg,
                                     size_t row_idx,
                                     CSeq_loc_Conversion& cvt)
{
    // start and stop on the original sequence
    TSeqPos start = sseg.m_Rows[row_idx].m_Start;
    TSeqPos len = sseg.m_Len;
    TSeqPos stop = start + len;
    if ( start != kInvalidSeqPos
        &&  cvt.ConvertInterval(start, stop, sseg.m_Rows[row_idx].m_Strand) ) {
        // At least part of the interval was converted. Calculate
        // trimming coords, trim each row.
        TSeqPos dl = cvt.m_Src_from <= start ? 0 : cvt.m_Src_from - start;
        TSeqPos dr = cvt.m_Src_to >= stop ? 0 : stop - cvt.m_Src_to;
        SAlignment_Segment dseg(len - dl - dr);
        for (size_t i = 0; i < sseg.m_Rows.size(); ++i) {
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
                TSeqPos dst_start = sseg.m_Rows[i].m_Start;
                if (dst_start != kInvalidSeqPos) {
                    dst_start += dl;
                }
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
            CSeq_id_Mapper::GetSeq_id_Mapper().GetHandle(cvt.GetDstId());
        dseg.m_Rows[row_idx].m_Start = kInvalidSeqPos;
        sseg.m_Mappings.push_back(dseg);
    }
}


void CSeq_align_Mapper::x_MapSegment(SAlignment_Segment& sseg,
                                     size_t row_idx,
                                     const CMappingRange& mapping_range,
                                     int width_flag)
{
    int dst_width = width_flag ? 3 : 1;
    CMappingRange mapping_copy = mapping_range;
    const SAlignment_Segment::SAlignment_Row& src_row = sseg.m_Rows[row_idx];
    TSeqPos start = src_row.m_Start;
    if (width_flag & CSeq_loc_Mapper::fWidthProtToNuc) {
        dst_width = 1;
        if (start != kInvalidSeqPos  &&  src_row.m_Width == 3) {
            start *= 3;
        }
        if (width_flag & CSeq_loc_Mapper::fWidthNucToProt) {
            mapping_copy.m_Src_from *= 3;
            mapping_copy.m_Src_to = (mapping_copy.m_Src_to + 1)*3 - 1;
            mapping_copy.m_Dst_from *= 3;
        }
    }
    else if (!width_flag  &&  src_row.m_Width == 3) {
        dst_width = 3;
        if (start != kInvalidSeqPos) {
            start *= 3;
        }
        mapping_copy.m_Src_from *= 3;
        mapping_copy.m_Src_to = (mapping_copy.m_Src_to + 1)*3 - 1;
        mapping_copy.m_Dst_from *= 3;
    }
    // stop on the original sequence
    TSeqPos stop = start + sseg.m_Len - 1;
    if ( start != kInvalidSeqPos
        &&  mapping_copy.CanMap(start, stop,
        src_row.m_IsSetStrand,
        src_row.m_Strand) ) {
        // At least part of the interval was converted. Calculate
        // trimming coords, trim each row.
        TSeqPos dl = mapping_copy.m_Src_from <= start ? 0
            : mapping_copy.m_Src_from - start;
        TSeqPos dr = mapping_copy.m_Src_to >= stop ? 0
            : stop - mapping_copy.m_Src_to;
        SAlignment_Segment dseg(sseg.m_Len - dl - dr);
        for (size_t i = 0; i < sseg.m_Rows.size(); ++i) {
            if (i == row_idx) {
                // translate id and coords
                CMappingRange::TRange mapped_rg =
                    mapping_copy.Map_Range(start, stop);
                ENa_strand dst_strand = eNa_strand_unknown;
                mapping_copy.Map_Strand(
                    src_row.m_IsSetStrand,
                    src_row.m_Strand,
                    &dst_strand);
                TSeqPos mapped_start = mapped_rg.GetFrom()/dst_width;
                dseg.AddRow(*mapping_range.m_Dst_id,
                    mapped_start,
                    src_row.m_IsSetStrand,
                    dst_strand,
                    dst_width)
                    .SetMapped();
            }
            else {
                TSeqPos dst_start = sseg.m_Rows[i].m_Start;
                if (dst_start != kInvalidSeqPos) {
                    int width = sseg.m_Rows[i].m_Width ?
                        sseg.m_Rows[i].m_Width : 1;
                    dst_start += dl/width;
                }
                dseg.AddRow(*sseg.m_Rows[i].m_Id.GetSeqId(),
                    dst_start,
                    sseg.m_Rows[i].m_IsSetStrand, sseg.m_Rows[i].m_Strand,
                    sseg.m_Rows[i].m_Width);
            }
        }
        sseg.m_Mappings.push_back(dseg);
    }
    else {
        // Empty mapping. Remove the row.
        SAlignment_Segment dseg = sseg;
        dseg.m_Rows[row_idx].m_Id =
            CSeq_id_Handle::GetHandle(*mapping_copy.m_Dst_id);
        dseg.m_Rows[row_idx].m_Start = kInvalidSeqPos;
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
        for (size_t row = 0; row < ss->m_Rows.size(); ++row) {
            if (ss->m_Rows[row].m_Id == cvt.m_Src_id_Handle) {
                x_MapSegment(*ss, row, cvt);
                res = true;
            }
        }
    }
    return res;
}


bool CSeq_align_Mapper::x_ConvertSegments(TSegments& segs,
                                          const CMappingRange& mapping_range,
                                          int width_flag)
{
    bool res = false;
    NON_CONST_ITERATE(TSegments, ss, segs) {
        // Check already mapped parts
        if (ss->m_Mappings.size() > 0) {
            if ( x_ConvertSegments(ss->m_Mappings,
                mapping_range, width_flag) ) {
                // Don't use source segment if the ID was
                // found in mappings.
                res = true;
                continue;
            }
        }
        // Check the original segment - some IDs may be mapped more than once.
        // This should not happen if the ID is present in mappings.
        for (size_t row = 0; row < ss->m_Rows.size(); ++row) {
            if (ss->m_Rows[row].m_Id == mapping_range.m_Src_id_Handle) {
                x_MapSegment(*ss, row, mapping_range, width_flag);
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


void CSeq_align_Mapper::Convert(const CMappingRange& mapping_range,
                                int width_flag)
{
    _ASSERT(m_SubAligns.size() > 0  ||  x_IsValidAlign(m_SrcSegs));

    m_DstAlign.Reset();

    if (m_SubAligns.size() > 0) {
        NON_CONST_ITERATE(TSubAligns, it, m_SubAligns) {
            it->Convert(mapping_range, width_flag);
        }
        return;
    }
    x_ConvertSegments(m_SrcSegs, mapping_range, width_flag);
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
                if (m_HaveWidths) {
                    dseg.SetWidths().push_back(row->m_Width);
                }
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
                    if (row->m_Start == kInvalidSeqPos) {
                        // empty
                        loc->SetEmpty(*id);
                    }
                    else {
                        // interval
                        loc->SetInt().SetId(*id);
                        TSeqPos len = seg_it->m_Len;
                        if (row->m_Width) {
                            len /= row->m_Width;
                        }
                        loc->SetInt().SetFrom(row->m_Start);
                        // len may be 0 after dividing by width
                        loc->SetInt().SetTo(row->m_Start + (len ? len - 1 : 0));
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
                    pseg.SetPresent().push_back(row->m_Start != kInvalidSeqPos);
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
    default:
        {
            dst->Assign(*m_OrigAlign);
            break;
        }
    }
    return m_DstAlign = dst;
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.5  2004/05/10 20:22:24  grichenk
* Fixed more warnings (removed inlines)
*
* Revision 1.4  2004/04/12 14:35:59  grichenk
* Fixed mapping of alignments between nucleotides and proteins
*
* Revision 1.3  2004/04/07 18:36:12  grichenk
* Fixed std-seg mapping
*
* Revision 1.2  2004/04/01 20:11:17  rsmith
* add missing break to switch on seq-id types.
*
* Revision 1.1  2004/03/30 15:40:35  grichenk
* Initial revision
*
*
* ===========================================================================
*/
