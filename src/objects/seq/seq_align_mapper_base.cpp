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
*   Alignment mapper base
*
*/

#include <ncbi_pch.hpp>
#include <objects/seq/seq_align_mapper_base.hpp>
#include <objects/seq/seq_loc_mapper_base.hpp>
#include <objects/seqalign/seqalign__.hpp>
#include <objects/seqloc/seqloc__.hpp>
#include <objects/misc/error_codes.hpp>
#include <algorithm>


#define NCBI_USE_ERRCODE_X   Objects_SeqAlignMap

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


SAlignment_Segment::SAlignment_Row& SAlignment_Segment::AddRow
(size_t         idx,
 const CSeq_id& id,
 int            start,
 bool           is_set_strand,
 ENa_strand     strand,
 int            width,
 int            frame)
{
    SAlignment_Row& row = GetRow(idx);
    row.m_Id = CSeq_id_Handle::GetHandle(id);
    row.m_Start = start < 0 ? kInvalidSeqPos : start;
    row.m_IsSetStrand = is_set_strand;
    row.m_Strand = strand;
    row.m_Width = width;
    row.m_Frame = frame;
    m_HaveStrands |= is_set_strand;
    return row;
}


SAlignment_Segment::SAlignment_Row& SAlignment_Segment::AddRow
(size_t                 idx,
 const CSeq_id_Handle&  id,
 int                    start,
 bool                   is_set_strand,
 ENa_strand             strand,
 int                    width,
 int                    frame)
{
    SAlignment_Row& row = GetRow(idx);
    row.m_Id = id;
    row.m_Start = start < 0 ? kInvalidSeqPos : start;
    row.m_IsSetStrand = is_set_strand;
    row.m_Strand = strand;
    row.m_Width = width;
    row.m_Frame = frame;
    m_HaveStrands |= is_set_strand;
    return row;
}


void SAlignment_Segment::SetScores(const TScores& scores)
{
    m_Scores = scores;
}


CSeq_align_Mapper_Base::CSeq_align_Mapper_Base(void)
    : m_OrigAlign(0),
      m_HaveStrands(false),
      m_HaveWidths(false),
      m_OnlyNucs(true),
      m_Dim(0),
      m_DstAlign(0),
      m_MapWidths(false),
      m_AlignFlags(eAlign_Normal)
{
}


CSeq_align_Mapper_Base::CSeq_align_Mapper_Base(const CSeq_align& align,
                                               EWidthFlag        map_widths)
    : m_OrigAlign(0),
      m_HaveStrands(false),
      m_HaveWidths(false),
      m_OnlyNucs(true),
      m_Dim(0),
      m_DstAlign(0),
      m_MapWidths(map_widths == eWidth_Map),
      m_AlignFlags(eAlign_Normal)
{
    x_Init(align);
}


CSeq_align_Mapper_Base::~CSeq_align_Mapper_Base(void)
{
}


void CSeq_align_Mapper_Base::x_Init(const CSeq_align& align)
{
    m_OrigAlign.Reset(&align);
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
    case CSeq_align::C_Segs::e_Spliced:
        x_Init(align.GetSegs().GetSpliced());
        break;
    case CSeq_align::C_Segs::e_Sparse:
        x_Init(align.GetSegs().GetSparse());
        break;
    default:
        break;
    }
}


SAlignment_Segment& CSeq_align_Mapper_Base::x_PushSeg(int len, size_t dim)
{
    m_Segs.push_back(SAlignment_Segment(len, dim));
    return m_Segs.back();
}


SAlignment_Segment&
CSeq_align_Mapper_Base::x_InsertSeg(TSegments::iterator& where,
                                    int                  len,
                                    size_t               dim)
{
    return *m_Segs.insert(where, SAlignment_Segment(len, dim));
}


void CSeq_align_Mapper_Base::x_Init(const TDendiag& diags)
{
    m_OnlyNucs &= m_MapWidths;
    ITERATE(TDendiag, diag_it, diags) {
        const CDense_diag& diag = **diag_it;
        size_t dim = diag.GetDim();
        if (dim != diag.GetIds().size()) {
            ERR_POST_X(1, Warning << "Invalid 'ids' size in dendiag");
            dim = min(dim, diag.GetIds().size());
        }
        if (dim != diag.GetStarts().size()) {
            ERR_POST_X(2, Warning << "Invalid 'starts' size in dendiag");
            dim = min(dim, diag.GetStarts().size());
        }
        m_HaveStrands = diag.IsSetStrands();
        if (m_HaveStrands && dim != diag.GetStrands().size()) {
            ERR_POST_X(3, Warning << "Invalid 'strands' size in dendiag");
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
            SAlignment_Segment::SAlignment_Row& last_row =
                seg.AddRow(row,
                *diag.GetIds()[row],
                diag.GetStarts()[row],
                m_HaveStrands,
                strand,
                m_MapWidths ? GetSeqWidth(*diag.GetIds()[row]) : 0,
                0);
            m_OnlyNucs &= (last_row.m_Width == 1);
        }
    }
}


void CSeq_align_Mapper_Base::x_Init(const CDense_seg& denseg)
{
    m_Dim = denseg.GetDim();
    size_t numseg = denseg.GetNumseg();
    // claimed dimension may not be accurate :-/
    if (numseg != denseg.GetLens().size()) {
        ERR_POST_X(4, Warning << "Invalid 'lens' size in denseg");
        numseg = min(numseg, denseg.GetLens().size());
    }
    if (m_Dim != denseg.GetIds().size()) {
        ERR_POST_X(5, Warning << "Invalid 'ids' size in denseg");
        m_Dim = min(m_Dim, denseg.GetIds().size());
    }
    if (m_Dim*numseg != denseg.GetStarts().size()) {
        ERR_POST_X(6, Warning << "Invalid 'starts' size in denseg");
        m_Dim = min(m_Dim*numseg, denseg.GetStarts().size()) / numseg;
    }
    m_HaveStrands = denseg.IsSetStrands();
    if (m_HaveStrands && m_Dim*numseg != denseg.GetStrands().size()) {
        ERR_POST_X(7, Warning << "Invalid 'strands' size in denseg");
        m_Dim = min(m_Dim*numseg, denseg.GetStrands().size()) / numseg;
    }
    m_HaveWidths = denseg.IsSetWidths();
    m_MapWidths &= m_HaveWidths;
    m_OnlyNucs &= m_MapWidths;
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
            const CSeq_id& seq_id = *denseg.GetIds()[row];
            SAlignment_Segment::SAlignment_Row& last_row =
                alnseg.AddRow(row,
                seq_id,
                denseg.GetStarts()[seg*m_Dim + row],
                m_HaveStrands,
                strand,
                m_HaveWidths ?
                denseg.GetWidths()[row] :
                (m_MapWidths ? GetSeqWidth(seq_id) : 0),
                0);
            m_OnlyNucs &= (last_row.m_Width == 1);
        }
    }
}


void CSeq_align_Mapper_Base::x_Init(const TStd& sseg)
{
    typedef map<CSeq_id_Handle, int> TSegLenMap;
    typedef map<CSeq_id_Handle, int> TSeqWidthMap;
    TSegLenMap seglens;
    TSeqWidthMap seqwid;
    // Need to know if there are empty locations
    bool have_empty = false;

    ITERATE ( CSeq_align::C_Segs::TStd, it, sseg ) {
        const CStd_seg& stdseg = **it;
        size_t dim = stdseg.GetDim();
        if (stdseg.IsSetIds()
            && dim != stdseg.GetIds().size()) {
            ERR_POST_X(8, Warning << "Invalid 'ids' size in std-seg");
            dim = min(dim, stdseg.GetIds().size());
        }
        SAlignment_Segment& seg = x_PushSeg(0, dim);
        if ( stdseg.IsSetScores() ) {
            seg.SetScores(stdseg.GetScores());
        }
        int seg_len = 0;
        bool multi_width = false;
        unsigned int row_idx = 0;
        ITERATE ( CStd_seg::TLoc, it_loc, (*it)->GetLoc() ) {
            if (row_idx > dim) {
                ERR_POST_X(9, Warning << "Invalid number of rows in std-seg");
                dim = row_idx;
                seg.m_Rows.resize(dim);
            }
            const CSeq_loc& loc = **it_loc;
            const CSeq_id* id = loc.GetId();
            int start = loc.GetTotalRange().GetFrom();
            int len = loc.GetTotalRange().GetLength();
            ENa_strand strand = eNa_strand_unknown;
            bool have_strand = false;
            switch ( loc.Which() ) {
            case CSeq_loc::e_Empty:
                start = (int)kInvalidSeqPos;
                have_empty = true;
                break;
            case CSeq_loc::e_Whole:
                start = 0;
                len = 0;
                break;
            case CSeq_loc::e_Int:
                have_strand = loc.GetInt().IsSetStrand();
                break;
            case CSeq_loc::e_Pnt:
                have_strand = loc.GetPnt().IsSetStrand();
                break;
            default:
                NCBI_THROW(CAnnotMapperException, eBadLocation,
                        "Unsupported seq-loc type in std-seg alignment");
            }
            if ( have_strand ) {
                m_HaveStrands = true;
                strand = loc.GetStrand();
            }
            if (len > 0) {
                if (seg_len == 0) {
                    seg_len = len;
                }
                else if (len != seg_len) {
                    multi_width = true;
                    if (len/3 == seg_len) {
                        seg_len = len/3;
                    }
                    else if (len*3 == seg_len) {
                        seg_len = len;
                    }
                    else {
                        NCBI_THROW(CAnnotMapperException, eBadLocation,
                                "Rows have different lengths in std-seg");
                    }
                }
            }
            seglens[seg.AddRow(row_idx,
                *id,
                start,
                have_strand,
                strand,
                0,
                0).m_Id] = len;

            row_idx++;
        }
        if (dim != m_Dim) {
            if ( m_Dim ) {
                m_AlignFlags = eAlign_MultiDim;
            }
            m_Dim = max(dim, m_Dim);
        }
        seg.m_Len = seg_len;
        if ( multi_width ) {
            // Adjust each segment width. Do not check if sequence always
            // has the same width.
            for (size_t i = 0; i < seg.m_Rows.size(); ++i) {
                int w = GetSeqWidth(*seg.m_Rows[i].m_Id.GetSeqId());
                if ( !w ) {
                    w = (seglens[seg.m_Rows[i].m_Id] != seg_len) ? 1 : 3;
                }
                seg.m_Rows[i].m_Width = w;
                // Remember widths of non-empty intervals
                if ( w  &&  seg.m_Rows[i].m_Start != (int)kInvalidSeqPos ) {
                    int& seq_width = seqwid[seg.m_Rows[i].m_Id];
                    if ( seq_width  &&  seq_width != w ) {
                        NCBI_THROW(CAnnotMapperException, eBadLocation,
                                "Inconsistent sequence width in in std-seg");
                    }
                    seq_width = w;
                }
            }
        }
        seglens.clear();
        m_HaveWidths |= multi_width;
        m_MapWidths |= multi_width;
        if (multi_width) {
            m_OnlyNucs = false;
        }
    }
    // Set the correct widths and segment lengths for empty intervals
    if ( m_HaveWidths  &&  have_empty ) {
        NON_CONST_ITERATE(TSegments, seg, m_Segs) {
            bool have_prot = false;
            NON_CONST_ITERATE(SAlignment_Segment::TRows, row, seg->m_Rows) {
                if ( !row->m_Width ) {
                    int w = seqwid[row->m_Id];
                    if ( w ) {
                        row->m_Width = w;
                    }
                }
                if (row->m_Start != (int)kInvalidSeqPos) {
                    have_prot |= (row->m_Width == 3);
                }
            }
            if ( !have_prot ) {
                // The segment's length has not been adjusted yet
                seg->m_Len /= 3;
            }
        }
    }
}


void CSeq_align_Mapper_Base::x_Init(const CPacked_seg& pseg)
{
    m_OnlyNucs &= m_MapWidths;
    m_Dim = pseg.GetDim();
    size_t numseg = pseg.GetNumseg();
    // claimed dimension may not be accurate :-/
    if (numseg != pseg.GetLens().size()) {
        ERR_POST_X(10, Warning << "Invalid 'lens' size in packed-seg");
        numseg = min(numseg, pseg.GetLens().size());
    }
    if (m_Dim != pseg.GetIds().size()) {
        ERR_POST_X(11, Warning << "Invalid 'ids' size in packed-seg");
        m_Dim = min(m_Dim, pseg.GetIds().size());
    }
    if (m_Dim*numseg != pseg.GetStarts().size()) {
        ERR_POST_X(12, Warning << "Invalid 'starts' size in packed-seg");
        m_Dim = min(m_Dim*numseg, pseg.GetStarts().size()) / numseg;
    }
    m_HaveStrands = pseg.IsSetStrands();
    if (m_HaveStrands && m_Dim*numseg != pseg.GetStrands().size()) {
        ERR_POST_X(13, Warning << "Invalid 'strands' size in packed-seg");
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
            SAlignment_Segment::SAlignment_Row& last_row =
                alnseg.AddRow(row,
                *pseg.GetIds()[row],
                (pseg.GetPresent()[seg*m_Dim + row] ?
                pseg.GetStarts()[seg*m_Dim + row] : kInvalidSeqPos),
                m_HaveStrands,
                strand,
                m_MapWidths ? GetSeqWidth(*pseg.GetIds()[row]) : 0,
                0);
            m_OnlyNucs &= (last_row.m_Width == 1);
        }
    }
}


void CSeq_align_Mapper_Base::x_Init(const CSeq_align_set& align_set)
{
    const CSeq_align_set::Tdata& data = align_set.Get();
    ITERATE(CSeq_align_set::Tdata, it, data) {
        m_SubAligns.push_back(
            Ref(CreateSubAlign(**it,
                               m_MapWidths ? eWidth_Map : eWidth_NoMap)));
    }
}


void CSeq_align_Mapper_Base::x_Init(const CSpliced_seg& spliced)
{
    const CSeq_id* gen_id = spliced.IsSetGenomic_id() ?
        &spliced.GetGenomic_id() : 0;
    const CSeq_id* prod_id = spliced.IsSetProduct_id() ?
        &spliced.GetProduct_id() : 0;

    m_Dim = 2;

    bool is_prot_prod =
        spliced.GetProduct_type() == CSpliced_seg::eProduct_type_protein;
    // Always use widths
    m_HaveWidths = true;
    m_MapWidths = true;
    m_OnlyNucs = !is_prot_prod;

    m_HaveStrands =
        spliced.IsSetGenomic_strand() || spliced.IsSetProduct_strand();
    ENa_strand gen_strand = spliced.IsSetGenomic_strand() ?
        spliced.GetGenomic_strand() : eNa_strand_unknown;
    ENa_strand prod_strand = spliced.IsSetProduct_strand() ?
        spliced.GetProduct_strand() : eNa_strand_unknown;
    ITERATE ( CSpliced_seg::TExons, it, spliced.GetExons() ) {
        const CSpliced_exon& ex = **it;
        TSeqPos seg_len = ex.GetGenomic_end() - ex.GetGenomic_start() + 1;
        SAlignment_Segment& alnseg = x_PushSeg(seg_len, 2);
        if ( ex.IsSetScores() ) {
            ITERATE(CScore_set::Tdata, it, ex.GetScores().Get()) {
                CRef<CScore> score(new CScore);
                score->Assign(**it);
                alnseg.m_Scores.push_back(score);
            }
        }
        const CSeq_id* ex_gen_id = ex.IsSetGenomic_id() ?
            &ex.GetGenomic_id() : gen_id;
        const CSeq_id* ex_prod_id = ex.IsSetProduct_id() ?
            &ex.GetProduct_id() : prod_id;
        m_HaveStrands |= ex.IsSetGenomic_strand() || ex.IsSetProduct_strand();
        ENa_strand ex_gen_strand = ex.IsSetGenomic_strand() ?
            ex.GetGenomic_strand() : gen_strand;
        ENa_strand ex_prod_strand = ex.IsSetProduct_strand() ?
            ex.GetProduct_strand() : prod_strand;
        if ( ex_gen_id ) {
            alnseg.AddRow(0, *ex_gen_id,
                ex.GetGenomic_start(),
                m_HaveStrands,
                ex_gen_strand,
                1,
                0);
        }
        else {
            ERR_POST_X(14, Warning << "Missing genomic id in spliced-seg");
        }
        if ( ex_prod_id ) {
            alnseg.AddRow(1, *ex_prod_id,
                ex.GetProduct_start().IsNucpos() ?
                ex.GetProduct_start().GetNucpos()
                : ex.GetProduct_start().GetProtpos().GetAmin(),
                m_HaveStrands,
                ex_prod_strand,
                is_prot_prod ? 3 : 1,
                ex.GetProduct_start().IsProtpos() ?
                ex.GetProduct_start().GetProtpos().GetFrame() : 0);
        }
        else {
            ERR_POST_X(15, Warning << "Missing product id in spliced-seg");
        }
    }
}


void CSeq_align_Mapper_Base::x_Init(const CSparse_seg& sparse)
{
    // Only single-row alignments are currently supported
    if ( sparse.GetRows().size() > 1) {
        NCBI_THROW(CAnnotMapperException, eBadAlignment,
                "Sparse-segs with multiple rows are not supported");
    }
    if ( sparse.GetRows().size() == 0) {
        return;
    }
    const CSparse_align& row = *sparse.GetRows().front();
    m_Dim = 2;

    size_t numseg = row.GetNumseg();
    // claimed dimension may not be accurate :-/
    if (numseg != row.GetFirst_starts().size()) {
        ERR_POST_X(16, Warning <<
            "Invalid 'first-starts' size in sparse-align");
        numseg = min(numseg, row.GetFirst_starts().size());
    }
    if (numseg != row.GetSecond_starts().size()) {
        ERR_POST_X(17, Warning <<
            "Invalid 'second-starts' size in sparse-align");
        numseg = min(numseg, row.GetSecond_starts().size());
    }
    if (numseg != row.GetLens().size()) {
        ERR_POST_X(18, Warning << "Invalid 'lens' size in sparse-align");
        numseg = min(numseg, row.GetLens().size());
    }
    m_HaveStrands = row.IsSetSecond_strands();
    if (m_HaveStrands  &&  numseg != row.GetSecond_strands().size()) {
        ERR_POST_X(19, Warning <<
            "Invalid 'second-strands' size in sparse-align");
        numseg = min(numseg, row.GetSecond_strands().size());
    }
    if (row.IsSetSeg_scores()  &&  numseg != row.GetSeg_scores().size()) {
        ERR_POST_X(20, Warning <<
            "Invalid 'seg-scores' size in sparse-align");
        numseg = min(numseg, row.GetSeg_scores().size());
    }

    int first_width = GetSeqWidth(row.GetFirst_id());
    int second_width = GetSeqWidth(row.GetSecond_id());
    m_HaveWidths = (first_width > 0)  &&  (second_width > 0);
    m_OnlyNucs &= (first_width == 1)  &&  (second_width == 1);
    m_MapWidths &= m_HaveWidths;
    if ( !m_MapWidths ) {
        first_width = 1;
        second_width = 1;
    }
    for (size_t seg = 0;  seg < numseg;  seg++) {
        SAlignment_Segment& alnseg = x_PushSeg(row.GetLens()[seg], m_Dim);
        if ( row.IsSetSeg_scores() ) {
            alnseg.m_Scores.push_back(row.GetSeg_scores()[seg]);
        }
        alnseg.AddRow(0, row.GetFirst_id(),
            row.GetFirst_starts()[seg],
            m_HaveStrands,
            eNa_strand_unknown,
            m_HaveWidths ? first_width : 1,
            0);
        alnseg.AddRow(1, row.GetSecond_id(),
            row.GetSecond_starts()[seg],
            m_HaveStrands,
            m_HaveStrands ? row.GetSecond_strands()[seg] : eNa_strand_unknown,
            m_HaveWidths ? second_width : 1,
            0);
    }
}


// Mapping through CSeq_loc_Mapper

void CSeq_align_Mapper_Base::Convert(CSeq_loc_Mapper_Base& mapper)
{
    m_DstAlign.Reset();

    if (m_SubAligns.size() > 0) {
        NON_CONST_ITERATE(TSubAligns, it, m_SubAligns) {
            (*it)->Convert(mapper);
        }
        return;
    }
    x_ConvertAlign(mapper, 0);
}


void CSeq_align_Mapper_Base::Convert(CSeq_loc_Mapper_Base& mapper,
                                     size_t                row)
{
    m_DstAlign.Reset();

    if (m_SubAligns.size() > 0) {
        NON_CONST_ITERATE(TSubAligns, it, m_SubAligns) {
            (*it)->Convert(mapper, row);
        }
        return;
    }
    x_ConvertAlign(mapper, &row);
}


void CSeq_align_Mapper_Base::x_ConvertAlign(CSeq_loc_Mapper_Base& mapper,
                                            size_t*               row)
{
    if (m_Segs.size() == 0) {
        return;
    }
    if ( row ) {
        x_ConvertRow(mapper, *row);
        return;
    }
    for (size_t row_idx = 0; row_idx < m_Dim; ++row_idx) {
        x_ConvertRow(mapper, row_idx);
    }
}


void CSeq_align_Mapper_Base::x_ConvertRow(CSeq_loc_Mapper_Base& mapper,
                                          size_t                row)
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
        if (seg_id) {
            if (dst_id  &&  dst_id != seg_id  &&
                m_AlignFlags == eAlign_Normal) {
                m_AlignFlags = eAlign_MultiId;
            }
            dst_id = seg_id;
        }
    }
}


CSeq_id_Handle
CSeq_align_Mapper_Base::x_ConvertSegment(TSegments::iterator&  seg_it,
                                         CSeq_loc_Mapper_Base& mapper,
                                         size_t                row)
{
    TSegments::iterator old_it = seg_it;
    SAlignment_Segment& seg = *old_it;
    ++seg_it;
    SAlignment_Segment::SAlignment_Row& aln_row = seg.m_Rows[row];
    if (aln_row.m_Start == kInvalidSeqPos) {
        // skipped row
        return CSeq_id_Handle(); // aln_row.m_Id;
    }
    const CMappingRanges::TIdMap& idmap = mapper.m_Mappings->GetIdMap();
    CMappingRanges::TIdIterator id_it = idmap.find(aln_row.m_Id);
    if (id_it == idmap.end()) {
        // ID not found in the segment, leave the row unchanged
        return aln_row.m_Id;
    }
    const CMappingRanges::TRangeMap& rmap = id_it->second;
    if ( rmap.empty() ) {
        // No mappings for this segment
        return aln_row.m_Id;
    }
    // Sorted mappings
    typedef vector< CRef<CMappingRange> > TSortedMappings;
    TSortedMappings mappings;
    CMappingRanges::TRangeIterator rg_it = rmap.begin();
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
    int len_wid = 1;
    TSeqPos start = aln_row.m_Start;
    if (width_flag & CSeq_loc_Mapper_Base::fWidthProtToNuc) {
        dst_width = 1;
        if (aln_row.m_Width == 3) {
            start *= 3;
            len_wid = 3;
            if ( aln_row.m_Frame ) {
                start += aln_row.m_Frame - 1;
            }
        }
    }
    else if (!width_flag  &&  aln_row.m_Width == 3) {
        dst_width = 3;
        start *= 3;
        len_wid = 3;
        if ( aln_row.m_Frame ) {
            start += aln_row.m_Frame - 1;
        }
    }
    TSeqPos stop = start + seg.m_Len*len_wid - 1;
    TSeqPos left_shift = 0;
    for (size_t map_idx = 0; map_idx < mappings.size(); ++map_idx) {
        CRef<CMappingRange> mapping(mappings[map_idx]);
        // Adjust mapping coordinates according to width
        if (width_flag & CSeq_loc_Mapper_Base::fWidthProtToNuc) {
            if (width_flag & CSeq_loc_Mapper_Base::fWidthNucToProt) {
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
            aln_row.m_IsSetStrand  &&  mapper.m_CheckStrand,
            aln_row.m_Strand)) {
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
                dl/len_wid, seg.m_Rows.size());
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
                        if (lrow.m_Frame) {
                            lrow.m_Frame = (lrow.m_Frame+left_shift-1)%3+1;
                        }
                    }
                    else {
                        lrow.m_Start +=
                            ((seg.m_Len - lseg.m_Len)*len_wid - left_shift)/width;
                        if (lrow.m_Frame) {
                            lrow.m_Frame = (lrow.m_Frame +
                                (seg.m_Len - lseg.m_Len)*len_wid -
                                left_shift - 1)%3+1;
                        }
                    }
                }
            }
        }
        start += dl;
        left_shift += dl;
        // At least part of the interval was converted.
        SAlignment_Segment& mseg = x_InsertSeg(seg_it,
            (stop - dr - start + 1)/len_wid, seg.m_Rows.size());
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
                mrow.m_Width = dst_width;
                mrow.m_IsSetStrand |= (dst_strand != eNa_strand_unknown);
                mrow.m_Strand = dst_strand;
                mrow.m_Frame = mrow.m_Frame ?
                    (mrow.m_Frame + left_shift - 1)%3+1 : 0;
                mrow.SetMapped();
                mseg.m_HaveStrands |= mrow.m_IsSetStrand;
                m_HaveStrands |= mseg.m_HaveStrands;
            }
            else {
                if (mrow.m_Start != kInvalidSeqPos) {
                    int width = seg.m_Rows[r].m_Width ?
                        seg.m_Rows[r].m_Width : 1;
                    if (mrow.SameStrand(aln_row)) {
                        mrow.m_Start += left_shift/width;
                        if (mrow.m_Frame) {
                            mrow.m_Frame = (mrow.m_Frame + left_shift - 1)%3+1;
                        }
                    }
                    else {
                        mrow.m_Start +=
                            ((seg.m_Len - mseg.m_Len)*len_wid - left_shift)/width;
                        if (mrow.m_Frame) {
                            mrow.m_Frame = (mrow.m_Frame +
                            (seg.m_Len - mseg.m_Len)*len_wid -
                                left_shift - 1)%3+1;
                        }
                    }
                }
            }
        }
        left_shift += seg.m_Len*len_wid;
        start += seg.m_Len*len_wid;
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
            (stop - start + 1)/len_wid, seg.m_Rows.size());
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
                    if (rrow.m_Frame) {
                        rrow.m_Frame = (rrow.m_Frame + left_shift - 1)%3+1;
                    }
                }
            }
        }
    }
    m_Segs.erase(old_it);
    return align_flags == eAlign_MultiId ? CSeq_id_Handle() : dst_id;
}


// Get mapped alignment

void CSeq_align_Mapper_Base::x_FillKnownStrands(TStrands& strands) const
{
    // Remember first known strand for each row, use it in gaps
    strands.clear();
    strands.reserve(m_Segs.front().m_Rows.size());
    for (size_t r_idx = 0; r_idx < m_Segs.front().m_Rows.size(); r_idx++) {
        ENa_strand strand = eNa_strand_unknown;
        // Skip gaps, try find a row with mapped strand
        ITERATE(TSegments, seg_it, m_Segs) {
            if ((TSeqPos)seg_it->m_Rows[r_idx].GetSegStart() != kInvalidSeqPos) {
                strand = seg_it->m_Rows[r_idx].m_Strand;
                break;
            }
        }
        strands.push_back(strand == eNa_strand_unknown ?
                          eNa_strand_plus : strand);
    }
}


void CSeq_align_Mapper_Base::x_GetDstDendiag(CRef<CSeq_align>& dst) const
{
    TDendiag& diags = dst->SetSegs().SetDendiag();
    TStrands strands;
    x_FillKnownStrands(strands);
    ITERATE(TSegments, seg_it, m_Segs) {
        const SAlignment_Segment& seg = *seg_it;
        CRef<CDense_diag> diag(new CDense_diag);
        diag->SetDim(seg.m_Rows.size());
        bool have_prots = false;
        size_t str_idx = 0;
        ITERATE(SAlignment_Segment::TRows, row, seg.m_Rows) {
            CRef<CSeq_id> id(new CSeq_id);
            id.Reset(&const_cast<CSeq_id&>(*row->m_Id.GetSeqId()));
            diag->SetIds().push_back(id);
            diag->SetStarts().push_back(row->GetSegStart());
            if (seg.m_HaveStrands) { // per-segment strands
                // For gaps use the strand of the first mapped row
                diag->SetStrands().
                    push_back((TSeqPos)row->GetSegStart() != kInvalidSeqPos ?
                    row->m_Strand : strands[str_idx]);
            }
            have_prots |= row->m_Width == 3;
            str_idx++;
        }
        int new_len = seg_it->m_Len;
        if ( m_MapWidths ) {
            if ( m_OnlyNucs  &&  have_prots ) {
                new_len /= 3;
            }
            if ( !m_OnlyNucs  &&  !have_prots ) {
                new_len *= 3;
            }
        }
        diag->SetLen(new_len);
        if ( seg.m_Scores.size() ) {
            diag->SetScores() = seg.m_Scores;
        }
        diags.push_back(diag);
    }
}


void CSeq_align_Mapper_Base::x_GetDstDenseg(CRef<CSeq_align>& dst) const
{
    CDense_seg& dseg = dst->SetSegs().SetDenseg();
    dseg.SetDim(m_Segs.front().m_Rows.size());
    dseg.SetNumseg(m_Segs.size());
    if ( m_Segs.front().m_Scores.size() ) {
        dseg.SetScores() = m_Segs.front().m_Scores;
    }
    bool have_prots = false;
    // Find first non-gap in each row, get its seq-id
    for (size_t r = 0; r < m_Segs.front().m_Rows.size(); r++) {
        ITERATE(TSegments, seg, m_Segs) {
            const SAlignment_Segment::SAlignment_Row& row = seg->m_Rows[r];
            if (row.m_Start != kInvalidSeqPos) {
                CRef<CSeq_id> id(new CSeq_id);
                id.Reset(&const_cast<CSeq_id&>(*row.m_Id.GetSeqId()));
                dseg.SetIds().push_back(id);
                if ( m_HaveWidths ) {
                    dseg.SetWidths().push_back(row.m_Width);
                }
                have_prots |= row.m_Width == 3;
                break;
            }
        }
    }
    TStrands strands;
    x_FillKnownStrands(strands);
    ITERATE(TSegments, seg_it, m_Segs) {
        int new_len = seg_it->m_Len;
        if ( m_MapWidths ) {
            if ( m_OnlyNucs  &&  have_prots ) {
                new_len /= 3;
            }
            if ( !m_OnlyNucs  &&  !have_prots ) {
                new_len *= 3;
            }
        }
        dseg.SetLens().push_back(new_len);
        size_t str_idx = 0;
        ITERATE(SAlignment_Segment::TRows, row, seg_it->m_Rows) {
            dseg.SetStarts().push_back(row->GetSegStart());
            if (m_HaveStrands) { // per-alignment strands
                // For gaps use the strand of the first mapped row
                dseg.SetStrands().
                    push_back((TSeqPos)row->GetSegStart() != kInvalidSeqPos ?
                    (row->m_Strand != eNa_strand_unknown ?
                    row->m_Strand : eNa_strand_plus): strands[str_idx]);
            }
            str_idx++;
        }
    }
}


void CSeq_align_Mapper_Base::x_GetDstStd(CRef<CSeq_align>& dst) const
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
                if (m_HaveWidths  &&  row->m_Width == 1) {
                    len *= 3;
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


void CSeq_align_Mapper_Base::x_GetDstPacked(CRef<CSeq_align>& dst) const
{
    CPacked_seg& pseg = dst->SetSegs().SetPacked();
    pseg.SetDim(m_Segs.front().m_Rows.size());
    pseg.SetNumseg(m_Segs.size());
    if ( m_Segs.front().m_Scores.size() ) {
        pseg.SetScores() = m_Segs.front().m_Scores;
    }
    TStrands strands;
    x_FillKnownStrands(strands);
    for (size_t r = 0; r < m_Segs.front().m_Rows.size(); r++) {
        ITERATE(TSegments, seg, m_Segs) {
            const SAlignment_Segment::SAlignment_Row& row = seg->m_Rows[r];
            if (row.m_Start != kInvalidSeqPos) {
                CRef<CSeq_id> id(new CSeq_id);
                id.Reset(&const_cast<CSeq_id&>(*row.m_Id.GetSeqId()));
                pseg.SetIds().push_back(id);
                break;
            }
        }
    }
    ITERATE(TSegments, seg_it, m_Segs) {
        bool have_prots = false;
        size_t str_idx = 0;
        ITERATE(SAlignment_Segment::TRows, row, seg_it->m_Rows) {
            pseg.SetStarts().push_back(row->GetSegStart());
            pseg.SetPresent().push_back(row->m_Start != kInvalidSeqPos);
            if (m_HaveStrands) {
                pseg.SetStrands().
                    push_back((TSeqPos)row->GetSegStart() != kInvalidSeqPos ?
                    row->m_Strand : strands[str_idx]);
            }
            have_prots |= row->m_Width == 3;
            str_idx++;
        }
        int new_len = seg_it->m_Len;
        if ( m_MapWidths ) {
            if ( m_OnlyNucs  &&  have_prots ) {
                new_len /= 3;
            }
            if ( !m_OnlyNucs  &&  !have_prots ) {
                new_len *= 3;
            }
        }
        pseg.SetLens().push_back(new_len);
    }
}


void CSeq_align_Mapper_Base::x_GetDstDisc(CRef<CSeq_align>& dst) const
{
    CSeq_align_set::Tdata& data = dst->SetSegs().SetDisc().Set();
    ITERATE(TSubAligns, it, m_SubAligns) {
        data.push_back((*it)->GetDstAlign());
    }
}


int CSeq_align_Mapper_Base::x_GetPartialDenseg(CRef<CSeq_align>& dst,
                                               int start_seg) const
{
    CDense_seg& dseg = dst->SetSegs().SetDenseg();
    dseg.SetDim(m_Segs.front().m_Rows.size());
    CDense_seg::TNumseg num_seg = m_Segs.size();

    bool have_prots = false;
    bool have_scores = !m_Segs.front().m_Scores.empty();

    // Find first non-gap in each row, get its seq-id, detect the first
    // one which is different.
    CDense_seg::TDim cur_seg;
    for (size_t r = 0; r < m_Segs.front().m_Rows.size(); r++) {
        cur_seg = 0;
        CSeq_id_Handle last_id;
        ITERATE(TSegments, seg, m_Segs) {
            if (cur_seg >= start_seg) {
                const SAlignment_Segment::SAlignment_Row& row = seg->m_Rows[r];
                if (last_id  &&  last_id != row.m_Id) {
                    break;
                }
                last_id = row.m_Id;
                CRef<CSeq_id> id(new CSeq_id);
                id.Reset(&const_cast<CSeq_id&>(*row.m_Id.GetSeqId()));
                dseg.SetIds().push_back(id);
                if ( m_HaveWidths ) {
                    dseg.SetWidths().push_back(row.m_Width);
                }
                have_prots |= row.m_Width == 3;
                if ( have_scores ) {
                    CRef<CScore> score(new CScore);
                    score->Assign(*m_Segs.front().m_Scores[r]);
                    dseg.SetScores().push_back(score);
                }
            }
            cur_seg++;
        }
        if (cur_seg < num_seg) {
            num_seg = cur_seg - start_seg;
        }
    }
    dseg.SetNumseg(num_seg);
    TStrands strands;
    x_FillKnownStrands(strands);
    cur_seg = 0;
    ITERATE(TSegments, seg_it, m_Segs) {
        if (cur_seg < start_seg) continue;
        if (cur_seg >= start_seg + num_seg) {
            break;
        }
        cur_seg++;
        int new_len = seg_it->m_Len;
        if ( m_MapWidths ) {
            if ( m_OnlyNucs  &&  have_prots ) {
                new_len /= 3;
            }
            if ( !m_OnlyNucs  &&  !have_prots ) {
                new_len *= 3;
            }
        }
        dseg.SetLens().push_back(new_len);
        size_t str_idx = 0;
        ITERATE(SAlignment_Segment::TRows, row, seg_it->m_Rows) {
            dseg.SetStarts().push_back(row->GetSegStart());
            if (m_HaveStrands) { // per-alignment strands
                // For gaps use the strand of the first mapped row
                dseg.SetStrands().
                    push_back((TSeqPos)row->GetSegStart() != kInvalidSeqPos ?
                    (row->m_Strand != eNa_strand_unknown ?
                    row->m_Strand : eNa_strand_plus): strands[str_idx]);
            }
            str_idx++;
        }
    }
    return start_seg + num_seg;
}


void CSeq_align_Mapper_Base::x_GetDstSpliced(CRef<CSeq_align>& dst) const
{
    CSpliced_seg& spliced = dst->SetSegs().SetSpliced();
    CSeq_id_Handle gen_id;
    CSeq_id_Handle prod_id;
    ENa_strand gen_strand = eNa_strand_unknown;
    ENa_strand prod_strand = eNa_strand_unknown;
    bool single_gen_id = true;
    bool single_gen_str = true;
    bool single_prod_id = true;
    bool single_prod_str = true;
    bool prod_protein = true;

    TSeqPos prod_len = 0;

    ITERATE(TSegments, seg, m_Segs) {
        const SAlignment_Segment::SAlignment_Row& gen_row = seg->m_Rows[0];
        const SAlignment_Segment::SAlignment_Row& prod_row = seg->m_Rows[1];
        if (seg->m_Rows.size() > 2) {
            NCBI_THROW(CAnnotMapperException, eBadAlignment,
                    "Can not construct spliced-seg with more than two rows");
        }
        CRef<CSpliced_exon> ex(new CSpliced_exon);
        CSeq_id_Handle ex_gen_id = gen_row.m_Id;
        CSeq_id_Handle ex_prod_id = prod_row.m_Id;
        ex->SetGenomic_id(const_cast<CSeq_id&>(*ex_gen_id.GetSeqId()));
        ex->SetProduct_id(const_cast<CSeq_id&>(*ex_prod_id.GetSeqId()));
        if ( !gen_id ) {
            gen_id = ex_gen_id;
        }
        else {
            single_gen_id &= gen_id == ex_gen_id;
        }
        if ( !prod_id ) {
            prod_id = ex_prod_id;
        }
        else {
            single_prod_id &= prod_id == ex_prod_id;
        }
        int start = gen_row.GetSegStart();
        if (start < 0) {
            continue; // ??? How to reflect a missing part
        }
        ex->SetGenomic_start(start);
        ex->SetGenomic_end(start + seg->m_Len - 1);
        start = prod_row.GetSegStart();
        if (start < 0) {
            continue; // ??? How to reflect a missing part
        }
        prod_len += seg->m_Len;
        if ( prod_row.m_Width == 3 ) {
            ex->SetProduct_start().SetProtpos().SetAmin(start);
            ex->SetProduct_start().SetProtpos().SetFrame(prod_row.m_Frame);
            int fr = prod_row.m_Frame ? prod_row.m_Frame - 1 : 0;
            TSeqPos end_pos = start*3 + fr + seg->m_Len - 1;
            ex->SetProduct_end().SetProtpos().SetAmin(end_pos/3);
            ex->SetProduct_end().SetProtpos().SetFrame((end_pos - fr)%3+1);
        }
        else {
            prod_protein = false;
            ex->SetProduct_start().SetNucpos(start);
            ex->SetProduct_end().SetNucpos(start + seg->m_Len - 1);
        }
        if ( gen_row.m_IsSetStrand ) {
            ex->SetGenomic_strand(gen_row.m_Strand);
            single_gen_str &= (gen_strand == eNa_strand_unknown) ||
                gen_strand == gen_row.m_Strand;
            gen_strand = gen_row.m_Strand;
        }
        else {
            single_gen_str &= gen_strand == eNa_strand_unknown;
        }
        if ( prod_row.m_IsSetStrand ) {
            ex->SetProduct_strand(prod_row.m_Strand);
            single_prod_str &= (prod_strand == eNa_strand_unknown) ||
                prod_strand == prod_row.m_Strand;
            prod_strand = prod_row.m_Strand;
        }
        else {
            single_prod_str &= prod_strand == eNa_strand_unknown;
        }
        spliced.SetExons().push_back(ex);
    }
    if ( single_gen_id ) {
        spliced.SetGenomic_id(const_cast<CSeq_id&>(*gen_id.GetSeqId()));
    }
    if ( single_gen_str  &&  gen_strand != eNa_strand_unknown ) {
        spliced.SetGenomic_strand(gen_strand);
    }
    if ( single_prod_id ) {
        spliced.SetProduct_id(const_cast<CSeq_id&>(*prod_id.GetSeqId()));
    }
    if ( single_prod_str  &&  prod_strand != eNa_strand_unknown ) {
        spliced.SetProduct_strand(prod_strand);
    }

    // Reset local values where possible
    if (single_gen_id || single_prod_id || single_gen_str || single_prod_str) {
        NON_CONST_ITERATE(CSpliced_seg::TExons, it, spliced.SetExons()) {
            if ( single_gen_id ) {
                (*it)->ResetGenomic_id();
            }
            if ( single_prod_id ) {
                (*it)->ResetProduct_id();
            }
            if ( single_gen_str ) {
                (*it)->ResetGenomic_strand();
            }
            if ( single_prod_str ) {
                (*it)->ResetProduct_strand();
            }
        }
    }

    if ( prod_protein ) {
        spliced.SetProduct_type(CSpliced_seg::eProduct_type_protein);
        spliced.SetProduct_length(prod_len/3);
    }
    else {
        spliced.SetProduct_type(CSpliced_seg::eProduct_type_transcript);
        spliced.SetProduct_length(prod_len);
    }
}


void CSeq_align_Mapper_Base::x_GetDstSparse(CRef<CSeq_align>& dst) const
{
    CSparse_seg& sparse = dst->SetSegs().SetSparse();
    CRef<CSparse_align> aln(new CSparse_align);
    sparse.SetRows().push_back(aln);
    aln->SetNumseg(m_Segs.size());

    CSeq_id_Handle first_idh;
    CSeq_id_Handle second_idh;
    size_t s = 0;
    ITERATE(TSegments, seg, m_Segs) {
        if (seg->m_Rows.size() > 2) {
            NCBI_THROW(CAnnotMapperException, eBadAlignment,
                    "Can not construct sparse-seg with more than two ids");
        }
        const SAlignment_Segment::SAlignment_Row& first_row = seg->m_Rows[0];
        const SAlignment_Segment::SAlignment_Row& second_row = seg->m_Rows[1];
        if ( first_idh ) {
            if (first_idh != first_row.m_Id) {
                NCBI_THROW(CAnnotMapperException, eBadAlignment,
                        "Can not construct sparse-seg with multiple ids per row");
            }
        }
        else {
            first_idh = first_row.m_Id;
            aln->SetFirst_id(const_cast<CSeq_id&>(*first_row.m_Id.GetSeqId()));
        }
        if ( second_idh ) {
            if (second_idh != second_row.m_Id) {
                NCBI_THROW(CAnnotMapperException, eBadAlignment,
                        "Can not construct sparse-seg with multiple ids per row");
            }
        }
        else {
            second_idh = second_row.m_Id;
            aln->SetSecond_id(const_cast<CSeq_id&>(*second_row.m_Id.GetSeqId()));
        }

        int first_start = first_row.GetSegStart();
        int second_start = second_row.GetSegStart();
        if (first_start < 0  ||  second_start < 0) {
            continue;
        }
        aln->SetFirst_starts().push_back(first_start);
        aln->SetSecond_starts().push_back(second_start);

        int len = seg->m_Len;
        bool have_prots =
            (first_row.m_Width == 3) || (second_row.m_Width == 3);
        if ( m_MapWidths ) {
            if ( m_OnlyNucs  &&  have_prots ) {
                len /= 3;
            }
            if ( !m_OnlyNucs  &&  !have_prots ) {
                len *= 3;
            }
        }
        aln->SetLens().push_back(len);

        if (aln->IsSetSecond_strands()  ||
            first_row.m_IsSetStrand  ||  second_row.m_IsSetStrand) {
            // Add missing strands if any
            for (size_t i = aln->SetSecond_strands().size(); i < s; i++) {
                aln->SetSecond_strands().push_back(eNa_strand_unknown);
            }
            ENa_strand first_strand = first_row.m_IsSetStrand ?
                first_row.m_Strand : eNa_strand_unknown;
            ENa_strand second_strand = second_row.m_IsSetStrand ?
                second_row.m_Strand : eNa_strand_unknown;
            aln->SetSecond_strands().push_back(IsForward(first_strand)
                ? second_strand : Reverse(second_strand));
        }

        if ( !seg->m_Scores.empty() ) {
            aln->SetSeg_scores().push_back(seg->m_Scores[0]);
        }
    }
}


void CSeq_align_Mapper_Base::x_ConvToDstDisc(CRef<CSeq_align>& dst) const
{
    CSeq_align_set::Tdata& data = dst->SetSegs().SetDisc().Set();
    int seg = 0;
    while (size_t(seg) < m_Segs.size()) {
        CRef<CSeq_align> dseg(new CSeq_align);
        seg = x_GetPartialDenseg(dseg, seg);
        data.push_back(dseg);
    }
}


CRef<CSeq_align> CSeq_align_Mapper_Base::GetDstAlign(void) const
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
                x_ConvToDstDisc(dst); // x_GetDstDendiag(dst);
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
                x_ConvToDstDisc(dst); // x_GetDstDendiag(dst);
            }
            break;
        }
    case CSeq_align::C_Segs::e_Disc:
        {
            x_GetDstDisc(dst);
            break;
        }
    case CSeq_align::C_Segs::e_Spliced:
        {
            x_GetDstSpliced(dst);
            break;
        }
    case CSeq_align::C_Segs::e_Sparse:
        {
            x_GetDstSparse(dst);
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


int CSeq_align_Mapper_Base::GetSeqWidth(const CSeq_id& id) const
{
    return 0;
}


CSeq_align_Mapper_Base*
CSeq_align_Mapper_Base::CreateSubAlign(const CSeq_align& align,
                                       EWidthFlag map_widths)
{
    return new CSeq_align_Mapper_Base(align, map_widths);
}


size_t CSeq_align_Mapper_Base::GetDim(void) const
{
    if ( m_Segs.empty() ) {
        return 0;
    }
    return m_Segs.begin()->m_Rows.size();
}


const CSeq_id_Handle& CSeq_align_Mapper_Base::GetRowId(size_t idx) const
{
    if ( m_Segs.empty() || idx >= m_Segs.begin()->m_Rows.size() ) {
        NCBI_THROW(CAnnotMapperException, eOtherError,
                   "Invalid row index");
    }
    return m_Segs.begin()->m_Rows[idx].m_Id;
}


END_SCOPE(objects)
END_NCBI_SCOPE

