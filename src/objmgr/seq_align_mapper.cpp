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

#include <ncbi_pch.hpp>
#include <objmgr/impl/seq_align_mapper.hpp>
#include <objmgr/seq_id_mapper.hpp>
#include <objmgr/seq_loc_mapper.hpp>
#include <objmgr/objmgr_exception.hpp>
#include <objects/seqalign/seqalign__.hpp>
#include <algorithm>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


SAlignment_Segment::SAlignment_Segment(int len, size_t dim)
    : m_Len(len),
      m_Rows(dim),
      m_HaveStrands(false)
{
    return;
}


SAlignment_Segment::SAlignment_Row&
SAlignment_Segment::GetRow(size_t idx)
{
    _ASSERT(m_Rows.size() > idx);
    return m_Rows[idx];
}


SAlignment_Segment::SAlignment_Row&
SAlignment_Segment::CopyRow(size_t idx, const SAlignment_Row& src_row)
{
    SAlignment_Row& dst_row = GetRow(idx);
    dst_row = src_row;
    return dst_row;
}


SAlignment_Segment::SAlignment_Row& SAlignment_Segment::AddRow(size_t idx,
                                                               const CSeq_id& id,
                                                               int start,
                                                               bool is_set_strand,
                                                               ENa_strand strand,
                                                               int width)
{
    SAlignment_Row& row = GetRow(idx);
    row.m_Id = CSeq_id_Handle::GetHandle(id);
    row.m_Start = start < 0 ? kInvalidSeqPos : start;
    row.m_IsSetStrand = is_set_strand;
    row.m_Strand = strand;
    row.m_Width = width;
    m_HaveStrands |= is_set_strand;
    return row;
}


SAlignment_Segment::SAlignment_Row& SAlignment_Segment::AddRow(size_t idx,
                                                               const CSeq_id_Handle& id,
                                                               int start,
                                                               bool is_set_strand,
                                                               ENa_strand strand,
                                                               int width)
{
    SAlignment_Row& row = GetRow(idx);
    row.m_Id = id;
    row.m_Start = start < 0 ? kInvalidSeqPos : start;
    row.m_IsSetStrand = is_set_strand;
    row.m_Strand = strand;
    row.m_Width = width;
    m_HaveStrands |= is_set_strand;
    return row;
}


void SAlignment_Segment::SetScores(const TScores& scores)
{
    m_Scores = scores;
}


CSeq_align_Mapper::CSeq_align_Mapper(const CSeq_align& align)
    : m_OrigAlign(&align),
      m_DstAlign(0),
      m_HaveStrands(false),
      m_HaveWidths(false),
      m_Dim(0),
      m_AlignFlags(eAlign_Normal)
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


SAlignment_Segment& CSeq_align_Mapper::x_PushSeg(int len, size_t dim)
{
    m_Segs.push_back(SAlignment_Segment(len, dim));
    return m_Segs.back();
}


SAlignment_Segment& CSeq_align_Mapper::x_InsertSeg(TSegments::iterator& where,
                                                   int len,
                                                   size_t dim)
{
    return *m_Segs.insert(where, SAlignment_Segment(len, dim));
}


void CSeq_align_Mapper::x_Init(const TDendiag& diags)
{
    ITERATE(TDendiag, diag_it, diags) {
        const CDense_diag& diag = **diag_it;
        size_t dim = diag.GetDim();
        if (dim != diag.GetIds().size()) {
            ERR_POST(Warning << "Invalid 'ids' size in dendiag");
            dim = min(dim, diag.GetIds().size());
        }
        if (dim != diag.GetStarts().size()) {
            ERR_POST(Warning << "Invalid 'starts' size in dendiag");
            dim = min(dim, diag.GetStarts().size());
        }
        m_HaveStrands = diag.IsSetStrands();
        if (m_HaveStrands && dim != diag.GetStrands().size()) {
            ERR_POST(Warning << "Invalid 'strands' size in dendiag");
            dim = min(dim, diag.GetStrands().size());
        }
        if (dim != m_Dim) {
            if ( m_Dim ) {
                m_AlignFlags = eAlign_MultiDim;
            }
            m_Dim = max(dim, m_Dim);
        }
        SAlignment_Segment& seg = x_PushSeg(diag.GetLen(), dim);
        ENa_strand strand = eNa_strand_unknown;
        if ( diag.IsSetScores() ) {
            seg.SetScores(diag.GetScores());
        }
        for (size_t row = 0; row < dim; ++row) {
            if ( m_HaveStrands ) {
                strand = diag.GetStrands()[row];
            }
            seg.AddRow(row,
                *diag.GetIds()[row],
                diag.GetStarts()[row],
                m_HaveStrands,
                strand,
                0);
        }
    }
}


void CSeq_align_Mapper::x_Init(const CDense_seg& denseg)
{
    m_Dim = denseg.GetDim();
    size_t numseg = denseg.GetNumseg();
    // claimed dimension may not be accurate :-/
    if (numseg != denseg.GetLens().size()) {
        ERR_POST(Warning << "Invalid 'lens' size in denseg");
        numseg = min(numseg, denseg.GetLens().size());
    }
    if (m_Dim != denseg.GetIds().size()) {
        ERR_POST(Warning << "Invalid 'ids' size in denseg");
        m_Dim = min(m_Dim, denseg.GetIds().size());
    }
    if (m_Dim*numseg != denseg.GetStarts().size()) {
        ERR_POST(Warning << "Invalid 'starts' size in denseg");
        m_Dim = min(m_Dim*numseg, denseg.GetStarts().size()) / numseg;
    }
    m_HaveStrands = denseg.IsSetStrands();
    if (m_HaveStrands && m_Dim*numseg != denseg.GetStrands().size()) {
        ERR_POST(Warning << "Invalid 'strands' size in denseg");
        m_Dim = min(m_Dim*numseg, denseg.GetStrands().size()) / numseg;
    }
    m_HaveWidths = denseg.IsSetWidths();
    ENa_strand strand = eNa_strand_unknown;
    for (size_t seg = 0;  seg < numseg;  seg++) {
        SAlignment_Segment& alnseg = x_PushSeg(denseg.GetLens()[seg], m_Dim);
        if ( seg == 0  &&  denseg.IsSetScores() ) {
            // Set scores for the first segment only to avoid multiple copies
            alnseg.SetScores(denseg.GetScores());
        }
        for (unsigned int row = 0;  row < m_Dim;  row++) {
            if ( m_HaveStrands ) {
                strand = denseg.GetStrands()[seg*m_Dim + row];
            }
            alnseg.AddRow(row,
                *denseg.GetIds()[row],
                denseg.GetStarts()[seg*m_Dim + row],
                m_HaveStrands,
                strand,
                m_HaveWidths ? denseg.GetWidths()[row] : 0);
        }
    }
}


void CSeq_align_Mapper::x_Init(const TStd& sseg)
{
    typedef map<CSeq_id_Handle, int> TSegLenMap;
    TSegLenMap seglens;

    ITERATE ( CSeq_align::C_Segs::TStd, it, sseg ) {
        const CStd_seg& stdseg = **it;
        size_t dim = stdseg.GetDim();
        if (stdseg.IsSetIds()
            && dim != stdseg.GetIds().size()) {
            ERR_POST(Warning << "Invalid 'ids' size in std-seg");
            dim = min(dim, stdseg.GetIds().size());
        }
        SAlignment_Segment& seg = x_PushSeg(0, dim);
        if ( stdseg.IsSetScores() ) {
            seg.SetScores(stdseg.GetScores());
        }
        int seg_len = 0;
        bool multi_width = false;
        ITERATE ( CStd_seg::TLoc, it_loc, (*it)->GetLoc() ) {
            const CSeq_loc& loc = **it_loc;
            const CSeq_id* id = 0;
            int start = -1;
            int len = 0;
            ENa_strand strand = eNa_strand_unknown;
            bool have_strand = false;
            unsigned int row_idx = 0;
            for (CSeq_loc_CI rg(loc); rg; ++rg, ++row_idx) {
                if (row_idx > dim) {
                    ERR_POST(Warning << "Invalid number of rows in std-seg");
                    dim = row_idx;
                    seg.m_Rows.resize(dim);
                }
                id = &rg.GetSeq_id();
                if ( rg.IsEmpty() ) {
                    start = kInvalidSeqPos;
                }
                else {
                    start = rg.GetRange().GetFrom();
                    len = rg.GetRange().GetLength();
                    strand = rg.GetStrand();
                    if (strand != eNa_strand_unknown) {
                        m_HaveStrands = have_strand = true;
                    }
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
                                    "Rows have different lengths in std-seg");
                        }
                    }
                }
                seglens[seg.AddRow(row_idx,
                    *id,
                    start,
                    have_strand,
                    strand,
                    0).m_Id] = len;
            }
            if (dim != m_Dim) {
                if ( m_Dim ) {
                    m_AlignFlags = eAlign_MultiDim;
                }
                m_Dim = max(dim, m_Dim);
            }
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
        m_HaveWidths |= multi_width;
    }
}


void CSeq_align_Mapper::x_Init(const CPacked_seg& pseg)
{
    m_Dim = pseg.GetDim();
    size_t numseg = pseg.GetNumseg();
    // claimed dimension may not be accurate :-/
    if (numseg != pseg.GetLens().size()) {
        ERR_POST(Warning << "Invalid 'lens' size in packed-seg");
        numseg = min(numseg, pseg.GetLens().size());
    }
    if (m_Dim != pseg.GetIds().size()) {
        ERR_POST(Warning << "Invalid 'ids' size in packed-seg");
        m_Dim = min(m_Dim, pseg.GetIds().size());
    }
    if (m_Dim*numseg != pseg.GetStarts().size()) {
        ERR_POST(Warning << "Invalid 'starts' size in packed-seg");
        m_Dim = min(m_Dim*numseg, pseg.GetStarts().size()) / numseg;
    }
    m_HaveStrands = pseg.IsSetStrands();
    if (m_HaveStrands && m_Dim*numseg != pseg.GetStrands().size()) {
        ERR_POST(Warning << "Invalid 'strands' size in packed-seg");
        m_Dim = min(m_Dim*numseg, pseg.GetStrands().size()) / numseg;
    }
    ENa_strand strand = eNa_strand_unknown;
    for (size_t seg = 0;  seg < numseg;  seg++) {
        SAlignment_Segment& alnseg = x_PushSeg(pseg.GetLens()[seg], m_Dim);
        if ( seg == 0  &&  pseg.IsSetScores() ) {
            // Set scores for the first segment only to avoid multiple copies
            alnseg.SetScores(pseg.GetScores());
        }
        for (unsigned int row = 0;  row < m_Dim;  row++) {
            if ( m_HaveStrands ) {
                strand = pseg.GetStrands()[seg*m_Dim + row];
            }
            alnseg.AddRow(row, 
                *pseg.GetIds()[row],
                (pseg.GetPresent()[seg*m_Dim + row] ?
                pseg.GetStarts()[seg*m_Dim + row] : kInvalidSeqPos),
                m_HaveStrands,
                strand,
                0);
        }
    }
}


void CSeq_align_Mapper::x_Init(const CSeq_align_set& align_set)
{
    const CSeq_align_set::Tdata& data = align_set.Get();
    ITERATE(CSeq_align_set::Tdata, it, data) {
        m_SubAligns.push_back(CSeq_align_Mapper(**it));
    }
}


// Mapping through CSeq_loc_Conversion_Set

struct CConversionRef_Less
{
    bool operator()(const CRef<CSeq_loc_Conversion>& x,
                    const CRef<CSeq_loc_Conversion>& y) const;
};


bool CConversionRef_Less::operator()(const CRef<CSeq_loc_Conversion>& x,
                                     const CRef<CSeq_loc_Conversion>& y) const
{
    if (x->m_Src_id_Handle != y->m_Src_id_Handle) {
        return x->m_Src_id_Handle < y->m_Src_id_Handle;
    }
    // Leftmost first
    if (x->m_Src_from != y->m_Src_from) {
        return x->m_Src_from < y->m_Src_from;
    }
    // Longest first
    return x->m_Src_to > y->m_Src_to;
}


void CSeq_align_Mapper::Convert(CSeq_loc_Conversion_Set& cvts,
                                unsigned int loc_index_shift)
{
    m_DstAlign.Reset();

    if (m_SubAligns.size() > 0) {
        NON_CONST_ITERATE(TSubAligns, it, m_SubAligns) {
            it->Convert(cvts, loc_index_shift);
            loc_index_shift += it->m_Dim;
        }
        return;
    }
    x_ConvertAlign(cvts, loc_index_shift);
}


void CSeq_align_Mapper::x_ConvertAlign(CSeq_loc_Conversion_Set& cvts,
                                       unsigned int loc_index_shift)
{
    if (cvts.m_CvtByIndex.size() == 0) {
        // Single mapping
        _ASSERT(cvts.m_SingleConv);
        x_ConvertRow(*cvts.m_SingleConv,
            cvts.m_SingleIndex - loc_index_shift);
        return;
    }
    CSeq_loc_Conversion_Set::TConvByIndex::iterator idx_it =
        cvts.m_CvtByIndex.lower_bound(loc_index_shift);
    for ( ; idx_it != cvts.m_CvtByIndex.end()
        &&  idx_it->first < loc_index_shift + m_Dim; ++idx_it) {
        x_ConvertRow(idx_it->second, idx_it->first - loc_index_shift);
    }
}


void CSeq_align_Mapper::x_ConvertRow(CSeq_loc_Conversion& cvt,
                                     size_t row)
{
    CSeq_id_Handle dst_id;
    TSegments::iterator seg_it = m_Segs.begin();
    for ( ; seg_it != m_Segs.end(); ) {
        if (seg_it->m_Rows.size() <= row) {
            // No such row in the current segment
            ++seg_it;
            m_AlignFlags = eAlign_MultiDim;
            continue;
        }
        CSeq_id_Handle seg_id = x_ConvertSegment(seg_it, cvt, row);
        if (dst_id) {
            if (dst_id != seg_id  &&  m_AlignFlags == eAlign_Normal) {
                m_AlignFlags = eAlign_MultiId;
            }
            dst_id = seg_id;
        }
    }
}


void CSeq_align_Mapper::x_ConvertRow(TIdMap& cvts,
                                     size_t row)
{
    CSeq_id_Handle dst_id;
    TSegments::iterator seg_it = m_Segs.begin();
    for ( ; seg_it != m_Segs.end(); ) {
        if (seg_it->m_Rows.size() <= row) {
            // No such row in the current segment
            ++seg_it;
            m_AlignFlags = eAlign_MultiDim;
            continue;
        }
        CSeq_id_Handle seg_id = x_ConvertSegment(seg_it, cvts, row);
        if (dst_id) {
            if (dst_id != seg_id  &&  m_AlignFlags == eAlign_Normal) {
                m_AlignFlags = eAlign_MultiId;
            }
            dst_id = seg_id;
        }
    }
}


CSeq_id_Handle
CSeq_align_Mapper::x_ConvertSegment(TSegments::iterator& seg_it,
                                    CSeq_loc_Conversion& cvt,
                                    size_t row)
{
    TSegments::iterator old_it = seg_it;
    ++seg_it;
    SAlignment_Segment::SAlignment_Row& aln_row = old_it->m_Rows[row];
    if (aln_row.m_Id != cvt.m_Src_id_Handle) {
        return aln_row.m_Id;
    }
    if (aln_row.m_Start == kInvalidSeqPos) {
        // ??? Skipped row - change ID
        aln_row.m_Id = cvt.m_Dst_id_Handle;
        aln_row.SetMapped();
        return aln_row.m_Id;
    }
    TRange rg(aln_row.m_Start, aln_row.m_Start + old_it->m_Len - 1);
    if (!cvt.ConvertInterval(rg.GetFrom(), rg.GetTo(), aln_row.m_Strand) ) {
        // Do not erase the segment, just change the row ID and reset start
        aln_row.m_Start = kInvalidSeqPos;
        aln_row.m_Id = cvt.m_Dst_id_Handle;
        aln_row.SetMapped();
        return aln_row.m_Id;
    }

    // At least part of the interval was converted.
    TSeqPos dl = cvt.m_Src_from <= rg.GetFrom() ?
        0 : cvt.m_Src_from - rg.GetFrom();
    TSeqPos dr = cvt.m_Src_to >= rg.GetTo() ?
        0 : rg.GetTo() - cvt.m_Src_to;
    if (dl > 0) {
        // Add segment for the skipped range
        SAlignment_Segment& lseg = x_InsertSeg(seg_it, dl,
            old_it->m_Rows.size());
        for (size_t r = 0; r < old_it->m_Rows.size(); ++r) {
            SAlignment_Segment::SAlignment_Row& lrow =
                lseg.CopyRow(r, old_it->m_Rows[r]);
            if (r == row) {
                lrow.m_Start = kInvalidSeqPos;
                lrow.m_Id = cvt.m_Dst_id_Handle;
            }
            else if (lrow.m_Start != kInvalidSeqPos &&
                !lrow.SameStrand(aln_row)) {
                // Adjust start for minus strand
                lrow.m_Start += old_it->m_Len - lseg.m_Len;
            }
        }
    }
    rg.SetFrom(rg.GetFrom() + dl);
    SAlignment_Segment& mseg = x_InsertSeg(seg_it,
        rg.GetLength() - dr,
        old_it->m_Rows.size());
    if (!dl  &&  !dr) {
        // copy scores if there's no truncation
        mseg.SetScores(old_it->m_Scores);
    }
    for (size_t r = 0; r < old_it->m_Rows.size(); ++r) {
        SAlignment_Segment::SAlignment_Row& mrow =
            mseg.CopyRow(r, old_it->m_Rows[r]);
        if (r == row) {
            // translate id and coords
            mrow.m_Id = cvt.m_Dst_id_Handle;
            mrow.m_Start = cvt.m_LastRange.GetFrom();
            mrow.m_IsSetStrand |= cvt.m_LastStrand != eNa_strand_unknown;
            mrow.m_Strand = cvt.m_LastStrand;
            mrow.SetMapped();
            mseg.m_HaveStrands |= mrow.m_IsSetStrand;
        }
        else {
            if (mrow.m_Start != kInvalidSeqPos) {
                if (mrow.SameStrand(aln_row)) {
                    mrow.m_Start += dl;
                }
                else {
                    mrow.m_Start += old_it->m_Len - dl - mseg.m_Len;
                }
            }
        }
    }
    cvt.m_LastType = cvt.eMappedObjType_not_set;
    dl += rg.GetLength() - dr;
    rg.SetFrom(rg.GetTo() - dr);
    if (dr > 0) {
        // Add the remaining unmapped range
        SAlignment_Segment& rseg = x_InsertSeg(seg_it,
            dr,
            old_it->m_Rows.size());
        for (size_t r = 0; r < old_it->m_Rows.size(); ++r) {
            SAlignment_Segment::SAlignment_Row& rrow =
                rseg.CopyRow(r, old_it->m_Rows[r]);
            if (r == row) {
                rrow.m_Start = kInvalidSeqPos;
                rrow.m_Id = cvt.m_Dst_id_Handle;
            }
            else {
                if (rrow.SameStrand(aln_row)) {
                    rrow.m_Start += dl;
                }
            }
        }
    }
    m_Segs.erase(old_it);
    return cvt.m_Dst_id_Handle;
}


CSeq_id_Handle
CSeq_align_Mapper::x_ConvertSegment(TSegments::iterator& seg_it,
                                    TIdMap& id_map,
                                    size_t row)
{
    TSegments::iterator old_it = seg_it;
    SAlignment_Segment& seg = *old_it;
    ++seg_it;
    SAlignment_Segment::SAlignment_Row& aln_row = seg.m_Rows[row];
    if (aln_row.m_Start == kInvalidSeqPos) {
        // skipped row
        return aln_row.m_Id;
    }
    TRange rg(aln_row.m_Start, aln_row.m_Start + seg.m_Len - 1);
    TIdMap::iterator id_it = id_map.find(aln_row.m_Id);
    if (id_it == id_map.end()) {
        // ID not found in the segment, leave the row unchanged
        return aln_row.m_Id;
    }
    TRangeMap& rmap = id_it->second;
    if ( rmap.empty() ) {
        // No mappings for this segment
        return aln_row.m_Id;
    }
    // Sorted mappings
    typedef vector< CRef<CSeq_loc_Conversion> > TSortedConversions;
    TSortedConversions cvts;
    for (TRangeMap::iterator rg_it = rmap.begin(rg); rg_it; ++rg_it) {
        cvts.push_back(rg_it->second);
    }
    sort(cvts.begin(), cvts.end(), CConversionRef_Less());

    bool mapped = false;
    CSeq_id_Handle dst_id;
    EAlignFlags align_flags = eAlign_Normal;
    TSeqPos left_shift = 0;
    for (size_t cvt_idx = 0; cvt_idx < cvts.size(); ++cvt_idx) {
        CSeq_loc_Conversion& cvt = *cvts[cvt_idx];
        if (!cvt.ConvertInterval(rg.GetFrom(), rg.GetTo(), aln_row.m_Strand) ) {
            continue;
        }
        // Check destination id
        if ( dst_id ) {
            if (cvt.m_Dst_id_Handle != dst_id) {
                align_flags = eAlign_MultiId;
            }
        }
        dst_id = cvt.m_Dst_id_Handle;

        // At least part of the interval was converted.
        TSeqPos dl = cvt.m_Src_from <= rg.GetFrom() ?
            0 : cvt.m_Src_from - rg.GetFrom();
        TSeqPos dr = cvt.m_Src_to >= rg.GetTo() ?
            0 : rg.GetTo() - cvt.m_Src_to;
        if (dl > 0) {
            // Add segment for the skipped range
            SAlignment_Segment& lseg = x_InsertSeg(seg_it,
                dl, seg.m_Rows.size());
            for (size_t r = 0; r < seg.m_Rows.size(); ++r) {
                SAlignment_Segment::SAlignment_Row& lrow =
                    lseg.CopyRow(r, seg.m_Rows[r]);
                if (r == row) {
                    lrow.m_Start = kInvalidSeqPos;
                    lrow.m_Id = dst_id;
                }
                else if (lrow.m_Start != kInvalidSeqPos) {
                    if (lrow.SameStrand(aln_row)) {
                        lrow.m_Start += left_shift;
                    }
                    else {
                        lrow.m_Start += seg.m_Len - lseg.m_Len - left_shift;
                    }
                }
            }
        }
        left_shift += dl;
        SAlignment_Segment& mseg = x_InsertSeg(seg_it,
            rg.GetLength() - dl - dr, seg.m_Rows.size());
        if (!dl  &&  !dr) {
            // copy scores if there's no truncation
            mseg.SetScores(seg.m_Scores);
        }
        for (size_t r = 0; r < seg.m_Rows.size(); ++r) {
            SAlignment_Segment::SAlignment_Row& mrow =
                mseg.CopyRow(r, seg.m_Rows[r]);
            if (r == row) {
                // translate id and coords
                mrow.m_Id = cvt.m_Dst_id_Handle;
                mrow.m_Start = cvt.m_LastRange.GetFrom();
                mrow.m_IsSetStrand |= cvt.m_LastStrand != eNa_strand_unknown;
                mrow.m_Strand = cvt.m_LastStrand;
                mrow.SetMapped();
                mseg.m_HaveStrands |= mrow.m_IsSetStrand;
            }
            else {
                if (mrow.m_Start != kInvalidSeqPos) {
                    if (mrow.SameStrand(aln_row)) {
                        mrow.m_Start += left_shift;
                    }
                    else {
                        mrow.m_Start += seg.m_Len - left_shift - mseg.m_Len;
                    }
                }
            }
        }
        cvt.m_LastType = cvt.eMappedObjType_not_set;
        mapped = true;
        left_shift += mseg.m_Len;
        rg.SetFrom(aln_row.m_Start + left_shift);
    }
    if (align_flags == eAlign_MultiId  &&  m_AlignFlags == eAlign_Normal) {
        m_AlignFlags = align_flags;
    }
    if ( !mapped ) {
        // Do not erase the segment, just change the row ID and reset start
        seg.m_Rows[row].m_Start = kInvalidSeqPos;
        seg.m_Rows[row].m_Id = rmap.begin()->second->m_Dst_id_Handle;
        seg.m_Rows[row].SetMapped();
        return seg.m_Rows[row].m_Id;
    }
    if (rg.GetFrom() <= rg.GetTo()) {
        // Add the remaining unmapped range
        SAlignment_Segment& rseg = x_InsertSeg(seg_it,
            rg.GetLength(), seg.m_Rows.size());
        for (size_t r = 0; r < seg.m_Rows.size(); ++r) {
            SAlignment_Segment::SAlignment_Row& rrow =
                rseg.CopyRow(r, seg.m_Rows[r]);
            if (r == row) {
                rrow.m_Start = kInvalidSeqPos;
                rrow.m_Id = dst_id;
            }
            else if (rrow.m_Start != kInvalidSeqPos) {
                if (rrow.SameStrand(aln_row)) {
                    rrow.m_Start += left_shift;
                }
            }
        }
    }
    m_Segs.erase(old_it);
    return align_flags == eAlign_MultiId ? CSeq_id_Handle() : dst_id;
}


// Mapping through CSeq_loc_Mapper

void CSeq_align_Mapper::Convert(CSeq_loc_Mapper& mapper)
{
    m_DstAlign.Reset();

    if (m_SubAligns.size() > 0) {
        NON_CONST_ITERATE(TSubAligns, it, m_SubAligns) {
            it->Convert(mapper);
        }
        return;
    }
    x_ConvertAlign(mapper);
}


void CSeq_align_Mapper::x_ConvertAlign(CSeq_loc_Mapper& mapper)
{
    if (m_Segs.size() == 0) {
        return;
    }
    for (size_t row_idx = 0; row_idx < m_Dim; ++row_idx) {
        x_ConvertRow(mapper, row_idx);
    }
}


void CSeq_align_Mapper::x_ConvertRow(CSeq_loc_Mapper& mapper,
                                     size_t row)
{
    CSeq_id_Handle dst_id;
    TSegments::iterator seg_it = m_Segs.begin();
    for ( ; seg_it != m_Segs.end(); ) {
        if (seg_it->m_Rows.size() <= row) {
            // No such row in the current segment
            ++seg_it;
            m_AlignFlags = eAlign_MultiDim;
            continue;
        }
        CSeq_id_Handle seg_id = x_ConvertSegment(seg_it, mapper, row);
        if (dst_id) {
            if (dst_id != seg_id  &&  m_AlignFlags == eAlign_Normal) {
                m_AlignFlags = eAlign_MultiId;
            }
            dst_id = seg_id;
        }
    }
}


CSeq_id_Handle
CSeq_align_Mapper::x_ConvertSegment(TSegments::iterator& seg_it,
                                    CSeq_loc_Mapper& mapper,
                                    size_t row)
{
    TSegments::iterator old_it = seg_it;
    SAlignment_Segment& seg = *old_it;
    ++seg_it;
    SAlignment_Segment::SAlignment_Row& aln_row = seg.m_Rows[row];
    if (aln_row.m_Start == kInvalidSeqPos) {
        // skipped row
        return aln_row.m_Id;
    }
    CSeq_loc_Mapper::TIdMap::iterator id_it =
        mapper.m_IdMap.find(aln_row.m_Id);
    if (id_it == mapper.m_IdMap.end()) {
        // ID not found in the segment, leave the row unchanged
        return aln_row.m_Id;
    }
    CSeq_loc_Mapper::TRangeMap& rmap = id_it->second;
    if ( rmap.empty() ) {
        // No mappings for this segment
        return aln_row.m_Id;
    }
    // Sorted mappings
    typedef vector< CRef<CMappingRange> > TSortedMappings;
    TSortedMappings mappings;
    CSeq_loc_Mapper::TRangeMap::iterator rg_it = rmap.begin();
    for ( ; rg_it; ++rg_it) {
        mappings.push_back(rg_it->second);
    }
    sort(mappings.begin(), mappings.end(), CMappingRangeRef_Less());

    bool mapped = false;
    CSeq_id_Handle dst_id;
    EAlignFlags align_flags = eAlign_Normal;
    int width_flag = mapper.m_UseWidth ?
        mapper.m_Widths[aln_row.m_Id] : 0;
    int dst_width = width_flag ? 3 : 1;
    TSeqPos start = aln_row.m_Start;
    if (width_flag & CSeq_loc_Mapper::fWidthProtToNuc) {
        dst_width = 1;
        if (aln_row.m_Width == 3) {
            start *= 3;
        }
    }
    else if (!width_flag  &&  aln_row.m_Width == 3) {
        dst_width = 3;
        start *= 3;
    }
    TSeqPos stop = start + seg.m_Len - 1;
    TSeqPos left_shift = 0;
    for (size_t map_idx = 0; map_idx < mappings.size(); ++map_idx) {
        CRef<CMappingRange> mapping(mappings[map_idx]);
        // Adjust mapping coordinates according to width
        if (width_flag & CSeq_loc_Mapper::fWidthProtToNuc) {
            if (width_flag & CSeq_loc_Mapper::fWidthNucToProt) {
                // Copy mapping
                mapping.Reset(new CMappingRange(*mappings[map_idx]));
                mapping->m_Src_from *= 3;
                mapping->m_Src_to = (mapping->m_Src_to + 1)*3 - 1;
                mapping->m_Dst_from *= 3;
            }
        }
        else if (!width_flag  &&  aln_row.m_Width == 3) {
            // Copy mapping
            mapping.Reset(new CMappingRange(*mappings[map_idx]));
            mapping->m_Src_from *= 3;
            mapping->m_Src_to = (mapping->m_Src_to + 1)*3 - 1;
            mapping->m_Dst_from *= 3;
        }

        if (!mapping->CanMap(start, stop,
            aln_row.m_IsSetStrand, aln_row.m_Strand)) {
            // Mapping does not apply to this segment/row
            continue;
        }

        // Check destination id
        if ( dst_id ) {
            if (mapping->m_Dst_id_Handle != dst_id) {
                align_flags = eAlign_MultiId;
            }
        }
        dst_id = mapping->m_Dst_id_Handle;

        // At least part of the interval was converted. Calculate
        // trimming coords, trim each row.
        TSeqPos dl = mapping->m_Src_from <= start ?
            0 : mapping->m_Src_from - start;
        TSeqPos dr = mapping->m_Src_to >= stop ?
            0 : stop - mapping->m_Src_to;
        if (dl > 0) {
            // Add segment for the skipped range
            SAlignment_Segment& lseg = x_InsertSeg(seg_it,
                dl, seg.m_Rows.size());
            for (size_t r = 0; r < seg.m_Rows.size(); ++r) {
                SAlignment_Segment::SAlignment_Row& lrow =
                    lseg.CopyRow(r, seg.m_Rows[r]);
                if (r == row) {
                    lrow.m_Start = kInvalidSeqPos;
                    lrow.m_Id = dst_id;
                }
                else if (lrow.m_Start != kInvalidSeqPos) {
                    int width = seg.m_Rows[r].m_Width ?
                        seg.m_Rows[r].m_Width : 1;
                    if (lrow.SameStrand(aln_row)) {
                        lrow.m_Start += left_shift/width;
                    }
                    else {
                        lrow.m_Start +=
                            (seg.m_Len - lseg.m_Len - left_shift)/width;
                    }
                }
            }
        }
        start += dl;
        left_shift += dl;
        // At least part of the interval was converted.
        SAlignment_Segment& mseg = x_InsertSeg(seg_it,
            stop - dr - start + 1, seg.m_Rows.size());
        if (!dl  &&  !dr) {
            // copy scores if there's no truncation
            mseg.SetScores(seg.m_Scores);
        }
        for (size_t r = 0; r < seg.m_Rows.size(); ++r) {
            SAlignment_Segment::SAlignment_Row& mrow =
                mseg.CopyRow(r, seg.m_Rows[r]);
            if (r == row) {
                // translate id and coords
                CMappingRange::TRange mapped_rg =
                    mapping->Map_Range(start, stop - dr);
                ENa_strand dst_strand = eNa_strand_unknown;
                mapping->Map_Strand(
                    aln_row.m_IsSetStrand,
                    aln_row.m_Strand,
                    &dst_strand);
                mrow.m_Id = mapping->m_Dst_id_Handle;
                mrow.m_Start = mapped_rg.GetFrom()/dst_width;
                mrow.m_IsSetStrand |= dst_strand != eNa_strand_unknown;
                mrow.m_Strand = dst_strand;
                mrow.SetMapped();
                mseg.m_HaveStrands |= mrow.m_IsSetStrand;
            }
            else {
                if (mrow.m_Start != kInvalidSeqPos) {
                    int width = seg.m_Rows[r].m_Width ?
                        seg.m_Rows[r].m_Width : 1;
                    if (mrow.SameStrand(aln_row)) {
                        mrow.m_Start += left_shift/width;
                    }
                    else {
                        mrow.m_Start +=
                            (seg.m_Len - left_shift - mseg.m_Len)/width;
                    }
                }
            }
        }
        left_shift += mseg.m_Len;
        start += mseg.m_Len;
        mapped = true;
    }
    if (align_flags == eAlign_MultiId  &&  m_AlignFlags == eAlign_Normal) {
        m_AlignFlags = align_flags;
    }
    if ( !mapped ) {
        // Do not erase the segment, just change the row ID and reset start
        seg.m_Rows[row].m_Start = kInvalidSeqPos;
        seg.m_Rows[row].m_Id = rmap.begin()->second->m_Dst_id_Handle;
        seg.m_Rows[row].SetMapped();
        return seg.m_Rows[row].m_Id;
    }
    if (start <= stop) {
        // Add the remaining unmapped range
        SAlignment_Segment& rseg = x_InsertSeg(seg_it,
            stop - start + 1, seg.m_Rows.size());
        for (size_t r = 0; r < seg.m_Rows.size(); ++r) {
            SAlignment_Segment::SAlignment_Row& rrow =
                rseg.CopyRow(r, seg.m_Rows[r]);
            if (r == row) {
                rrow.m_Start = kInvalidSeqPos;
                rrow.m_Id = dst_id;
            }
            else if (rrow.m_Start != kInvalidSeqPos) {
                int width = seg.m_Rows[r].m_Width ?
                    seg.m_Rows[r].m_Width : 1;
                if (rrow.SameStrand(aln_row)) {
                    rrow.m_Start += left_shift/width;
                }
            }
        }
    }
    m_Segs.erase(old_it);
    return align_flags == eAlign_MultiId ? CSeq_id_Handle() : dst_id;
}


// Get mapped alignment

void CSeq_align_Mapper::x_GetDstDendiag(CRef<CSeq_align>& dst) const
{
    TDendiag& diags = dst->SetSegs().SetDendiag();
    ITERATE(TSegments, seg_it, m_Segs) {
        const SAlignment_Segment& seg = *seg_it;
        CRef<CDense_diag> diag(new CDense_diag);
        diag->SetDim(seg.m_Rows.size());
        diag->SetLen(seg.m_Len);
        ITERATE(SAlignment_Segment::TRows, row, seg.m_Rows) {
            CRef<CSeq_id> id(new CSeq_id);
            id.Reset(&const_cast<CSeq_id&>(*row->m_Id.GetSeqId()));
            diag->SetIds().push_back(id);
            diag->SetStarts().push_back(row->GetSegStart());
            if (seg.m_HaveStrands) { // per-segment strands
                diag->SetStrands().push_back(row->m_Strand);
            }
        }
        if ( seg.m_Scores.size() ) {
            diag->SetScores() = seg.m_Scores;
        }
        diags.push_back(diag);
    }
}


void CSeq_align_Mapper::x_GetDstDenseg(CRef<CSeq_align>& dst) const
{
    CDense_seg& dseg = dst->SetSegs().SetDenseg();
    dseg.SetDim(m_Segs.front().m_Rows.size());
    dseg.SetNumseg(m_Segs.size());
    if ( m_Segs.front().m_Scores.size() ) {
        dseg.SetScores() = m_Segs.front().m_Scores;
    }
    ITERATE(SAlignment_Segment::TRows, row, m_Segs.front().m_Rows) {
        CRef<CSeq_id> id(new CSeq_id);
        id.Reset(&const_cast<CSeq_id&>(*row->m_Id.GetSeqId()));
        dseg.SetIds().push_back(id);
        if (m_HaveWidths) {
            dseg.SetWidths().push_back(row->m_Width);
        }
    }
    ITERATE(TSegments, seg_it, m_Segs) {
        dseg.SetLens().push_back(seg_it->m_Len);
        ITERATE(SAlignment_Segment::TRows, row, seg_it->m_Rows) {
            dseg.SetStarts().push_back(row->GetSegStart());
            if (m_HaveStrands) { // per-alignment strands
                dseg.SetStrands().push_back(row->m_Strand);
            }
        }
    }
}


void CSeq_align_Mapper::x_GetDstStd(CRef<CSeq_align>& dst) const
{
    TStd& std_segs = dst->SetSegs().SetStd();
    ITERATE(TSegments, seg_it, m_Segs) {
        CRef<CStd_seg> std_seg(new CStd_seg);
        std_seg->SetDim(seg_it->m_Rows.size());
        if ( seg_it->m_Scores.size() ) {
            std_seg->SetScores() = seg_it->m_Scores;
        }
        ITERATE(SAlignment_Segment::TRows, row, seg_it->m_Rows) {
            CRef<CSeq_id> id(new CSeq_id);
            id.Reset(&const_cast<CSeq_id&>(*row->m_Id.GetSeqId()));
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
}


void CSeq_align_Mapper::x_GetDstPacked(CRef<CSeq_align>& dst) const
{
    CPacked_seg& pseg = dst->SetSegs().SetPacked();
    pseg.SetDim(m_Segs.front().m_Rows.size());
    pseg.SetNumseg(m_Segs.size());
    if ( m_Segs.front().m_Scores.size() ) {
        pseg.SetScores() = m_Segs.front().m_Scores;
    }
    ITERATE(SAlignment_Segment::TRows, row, m_Segs.front().m_Rows) {
        CRef<CSeq_id> id(new CSeq_id);
        id.Reset(&const_cast<CSeq_id&>(*row->m_Id.GetSeqId()));
        pseg.SetIds().push_back(id);
    }
    ITERATE(TSegments, seg_it, m_Segs) {
        pseg.SetLens().push_back(seg_it->m_Len);
        ITERATE(SAlignment_Segment::TRows, row, seg_it->m_Rows) {
            pseg.SetStarts().push_back(row->GetSegStart());
            pseg.SetPresent().push_back(row->m_Start != kInvalidSeqPos);
            if (m_HaveStrands) {
                pseg.SetStrands().push_back(row->m_Strand);
            }
        }
    }
}


void CSeq_align_Mapper::x_GetDstDisc(CRef<CSeq_align>& dst) const
{
    CSeq_align_set::Tdata& data = dst->SetSegs().SetDisc().Set();
    ITERATE(TSubAligns, it, m_SubAligns) {
        data.push_back(it->GetDstAlign());
    }
}


CRef<CSeq_align> CSeq_align_Mapper::GetDstAlign(void) const
{
    if (m_DstAlign) {
        return m_DstAlign;
    }

    CRef<CSeq_align> dst(new CSeq_align);
    dst->SetType(m_OrigAlign->GetType());
    if (m_OrigAlign->IsSetDim()) {
        dst->SetDim(m_OrigAlign->GetDim());
    }
    if (m_OrigAlign->IsSetScore()) {
        dst->SetScore() = m_OrigAlign->GetScore();
    }
    if (m_OrigAlign->IsSetBounds()) {
        dst->SetBounds() = m_OrigAlign->GetBounds();
    }
    switch ( m_OrigAlign->GetSegs().Which() ) {
    case CSeq_align::C_Segs::e_Dendiag:
        {
            x_GetDstDendiag(dst);
            break;
        }
    case CSeq_align::C_Segs::e_Denseg:
        {
            if (m_AlignFlags == eAlign_Normal) {
                x_GetDstDenseg(dst);
            }
            else {
                x_GetDstDendiag(dst);
            }
            break;
        }
    case CSeq_align::C_Segs::e_Std:
        {
            x_GetDstStd(dst);
            break;
        }
    case CSeq_align::C_Segs::e_Packed:
        {
            if (m_AlignFlags == eAlign_Normal) {
                x_GetDstPacked(dst);
            }
            else {
                x_GetDstDendiag(dst);
            }
            break;
        }
    case CSeq_align::C_Segs::e_Disc:
        {
            x_GetDstDisc(dst);
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
* Revision 1.9  2004/05/26 14:57:47  ucko
* Change some types from unsigned int to size_t for consistency with STL
* containers' size() methods.
*
* Revision 1.8  2004/05/26 14:29:20  grichenk
* Redesigned CSeq_align_Mapper: preserve non-mapping intervals,
* fixed strands handling, improved performance.
*
* Revision 1.7  2004/05/21 21:42:12  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.6  2004/05/13 19:10:57  grichenk
* Preserve scores in mapped alignments
*
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
