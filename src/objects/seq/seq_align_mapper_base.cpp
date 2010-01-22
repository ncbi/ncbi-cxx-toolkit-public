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
#include <objects/general/User_object.hpp>
#include <objects/general/User_field.hpp>
#include <objects/general/Object_id.hpp>
#include <algorithm>


#define NCBI_USE_ERRCODE_X   Objects_SeqAlignMap

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


SAlignment_Segment::SAlignment_Segment(int len, size_t dim)
    : m_Len(len),
      m_Rows(dim),
      m_HaveStrands(false),
      m_GroupIdx(0),
      m_ScoresGroupIdx(-1),
      m_PartType(CSpliced_exon_chunk::e_not_set)
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


SAlignment_Segment::SAlignment_Row&
SAlignment_Segment::AddRow(size_t         idx,
                           const CSeq_id& id,
                           int            start,
                           bool           is_set_strand,
                           ENa_strand     strand)
{
    SAlignment_Row& row = GetRow(idx);
    row.m_Id = CSeq_id_Handle::GetHandle(id);
    row.m_Start = start < 0 ? kInvalidSeqPos : start;
    row.m_IsSetStrand = is_set_strand;
    row.m_Strand = strand;
    m_HaveStrands = m_HaveStrands  ||  is_set_strand;
    return row;
}


SAlignment_Segment::SAlignment_Row&
SAlignment_Segment::AddRow(size_t                 idx,
                           const CSeq_id_Handle&  id,
                           int                    start,
                           bool                   is_set_strand,
                           ENa_strand             strand)
{
    SAlignment_Row& row = GetRow(idx);
    row.m_Id = id;
    row.m_Start = start < 0 ? kInvalidSeqPos : start;
    row.m_IsSetStrand = is_set_strand;
    row.m_Strand = strand;
    m_HaveStrands = m_HaveStrands  ||  is_set_strand;
    return row;
}


CSeq_align_Mapper_Base::
CSeq_align_Mapper_Base(CSeq_loc_Mapper_Base& loc_mapper)
    : m_LocMapper(loc_mapper),
      m_OrigAlign(0),
      m_HaveStrands(false),
      m_Dim(0),
      m_ScoresInvalidated(false),
      m_DstAlign(0),
      m_AlignFlags(eAlign_Normal)
{
}


CSeq_align_Mapper_Base::
CSeq_align_Mapper_Base(const CSeq_align&     align,
                       CSeq_loc_Mapper_Base& loc_mapper)
    : m_LocMapper(loc_mapper),
      m_OrigAlign(0),
      m_HaveStrands(false),
      m_Dim(0),
      m_ScoresInvalidated(false),
      m_DstAlign(0),
      m_AlignFlags(eAlign_Normal)
{
    x_Init(align);
}


CSeq_align_Mapper_Base::~CSeq_align_Mapper_Base(void)
{
}


// Copy each element from source to destination
template<class T, class C1, class C2>
void CloneContainer(const C1& src, C2& dst)
{
    ITERATE(typename C1, it, src) {
        CRef<T> elem(new T);
        elem->Assign(**it);
        dst.push_back(elem);
    }
}


// Copy pointers from source to destination
template<class C1, class C2>
void CopyContainer(const C1& src, C2& dst)
{
    ITERATE(typename C1, it, src) {
        dst.push_back(*it);
    }
}


void CSeq_align_Mapper_Base::x_Init(const CSeq_align& align)
{
    m_OrigAlign.Reset(&align);
    if (align.IsSetScore()  &&  !align.GetScore().empty()) {
        CopyContainer<CSeq_align::TScore, TScores>(
            align.GetScore(), m_AlignScores);
    }
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


SAlignment_Segment& CSeq_align_Mapper_Base::x_PushSeg(int len,
                                                      size_t dim,
                                                      ENa_strand strand)
{
    if ( !IsReverse(strand) ) {
        m_Segs.push_back(SAlignment_Segment(len, dim));
        return m_Segs.back();
    }
    else {
        m_Segs.push_front(SAlignment_Segment(len, dim));
        return m_Segs.front();
    }
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
        bool have_prot = false;
        bool have_nuc = false;
        SAlignment_Segment& seg = x_PushSeg(diag.GetLen(), dim);
        ENa_strand strand = eNa_strand_unknown;
        if ( diag.IsSetScores() ) {
            CopyContainer<CDense_diag::TScores, TScores>(
                diag.GetScores(), seg.m_Scores);
        }
        for (size_t row = 0; row < dim; ++row) {
            if ( m_HaveStrands ) {
                strand = diag.GetStrands()[row];
            }
            const CSeq_id& row_id = *diag.GetIds()[row];
            int row_start = diag.GetStarts()[row];
            CSeq_loc_Mapper_Base::ESeqType row_type =
                m_LocMapper.GetSeqTypeById(row_id);
            if (row_type == CSeq_loc_Mapper_Base::eSeq_prot) {
                if ( !have_prot ) {
                    // Adjust segment length once
                    have_prot = true;
                    seg.m_Len *= 3;
                }
                row_start *= 3;
            }
            else /*if (row_type == CSeq_loc_Mapper_Base::eSeq_nuc)*/ {
                have_nuc = true;
            }
            seg.AddRow(row, row_id, row_start, m_HaveStrands, strand);
        }
        if (have_prot  &&  have_nuc) {
            NCBI_THROW(CAnnotMapperException, eBadAlignment,
                "Dense-diags with mixed sequence types are not supported");
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
    if ( denseg.IsSetScores() ) {
        CopyContainer<CDense_seg::TScores, TScores>(
            denseg.GetScores(), m_SegsScores);
    }
    ENa_strand strand = eNa_strand_unknown;
    for (size_t seg = 0;  seg < numseg;  seg++) {
        SAlignment_Segment& alnseg = x_PushSeg(denseg.GetLens()[seg], m_Dim);
        bool have_prot = false;
        bool have_nuc = false;
        for (unsigned int row = 0;  row < m_Dim;  row++) {
            if ( m_HaveStrands ) {
                strand = denseg.GetStrands()[seg*m_Dim + row];
            }
            const CSeq_id& seq_id = *denseg.GetIds()[row];

            int width = 1;
            CSeq_loc_Mapper_Base::ESeqType seq_type =
                m_LocMapper.GetSeqTypeById(seq_id);
            if (seq_type == CSeq_loc_Mapper_Base::eSeq_prot) {
                have_prot = true;
                width = 3;
            }
            else /*if (seq_type == CSeq_loc_Mapper_Base::eSeq_nuc)*/ {
                have_nuc = true;
            }
            int start = denseg.GetStarts()[seg*m_Dim + row]*width;
            alnseg.AddRow(row, seq_id, start, m_HaveStrands, strand);
        }
        if (have_prot  &&  have_nuc) {
            NCBI_THROW(CAnnotMapperException, eBadAlignment,
                "Dense-segs with mixed sequence types are not supported");
        }
        if ( have_prot ) {
            alnseg.m_Len *= 3;
        }
    }
}


void CSeq_align_Mapper_Base::x_Init(const TStd& sseg)
{
    vector<int> seglens;
    seglens.reserve(sseg.size());
    ITERATE(CSeq_align::C_Segs::TStd, it, sseg) {
        // Two different lengths are allowed - for nucs and prots.
        int minlen = 0;
        int maxlen = 0;
        // First pass - find min and max segment lengths
        ITERATE( CStd_seg::TLoc, it_loc, (*it)->GetLoc()) {
            const CSeq_loc& loc = **it_loc;
            const CSeq_id* id = loc.GetId();
            int len = loc.GetTotalRange().GetLength();
            if (len == 0  ||  loc.IsWhole()) {
                continue; // ignore unknown lengths
            }
            if ( !id ) {
                // Mixed ids in the same row?
                NCBI_THROW(CAnnotMapperException, eBadAlignment,
                    "Locations with mixed seq-ids are not supported "
                    "in std-seg alignments");
            }
            if (minlen == 0  ||  len == minlen) {
                minlen = len;
            }
            else if (maxlen == 0  ||  len == maxlen) {
                maxlen = len;
                if (minlen > maxlen) {
                    swap(minlen, maxlen);
                }
            }
            else {
                // Both minlen and maxlen are set, len differs from both.
                // More than two different lengths in the same segment.
                NCBI_THROW(CAnnotMapperException, eBadAlignment,
                    "Rows of the same std-seg have different lengths");
            }
        }
        if (minlen != 0  &&  maxlen != 0) {
            if (minlen*3 != maxlen) {
                NCBI_THROW(CAnnotMapperException, eBadAlignment,
                    "Inconsistent seq-loc lengths in std-seg rows");
            }
            // Found both nucs and prots - make the second pass and
            // store widths.
            ITERATE( CStd_seg::TLoc, it_loc, (*it)->GetLoc()) {
                const CSeq_loc& loc = **it_loc;
                const CSeq_id* id = loc.GetId();
                int len = loc.GetTotalRange().GetLength();
                if (len == 0  ||  loc.IsWhole()) {
                    continue; // ignore unknown lengths
                }
                _ASSERT(id); // All locations should have been checked
                CSeq_loc_Mapper_Base::ESeqType newtype = (len == minlen) ?
                    CSeq_loc_Mapper_Base::eSeq_prot
                    : CSeq_loc_Mapper_Base::eSeq_nuc;
                CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*id);
                // Check if seq-type is available from the location mapper
                CSeq_loc_Mapper_Base::ESeqType seqtype =
                    m_LocMapper.GetSeqTypeById(idh);
                if (seqtype != CSeq_loc_Mapper_Base::eSeq_unknown) {
                    if (seqtype != newtype) {
                        NCBI_THROW(CAnnotMapperException, eBadAlignment,
                            "Segment lengths in std-seg alignment are "
                            "inconsistent with sequence types");
                    }
                }
                else {
                    if (newtype == CSeq_loc_Mapper_Base::eSeq_prot) {
                        // Try to change all types to prot, adjust coords
                        // This is required in cases when the loc-mapper
                        // could not detect protein during initialization
                        // because there were no nucs to compare to.
                        m_LocMapper.x_AdjustSeqTypesToProt(idh);
                    }
                    // Set type anyway -- x_AdjustSeqTypesToProt could ignore it
                    m_LocMapper.SetSeqTypeById(idh, newtype);
                }
            }
        }
        // -1 indicates unknown sequence type or equal lengths for all rows
        seglens.push_back(maxlen == 0 ? -1/*minlen*/ : maxlen);
    }
    // By this point all possible sequence types should be detected and
    // stored in the loc-mapper.
    // All unknown types are treated as nucs.

    size_t seg_idx = 0;
    ITERATE (CSeq_align::C_Segs::TStd, it, sseg) {
        const CStd_seg& stdseg = **it;
        size_t dim = stdseg.GetDim();
        if (stdseg.IsSetIds()
            && dim != stdseg.GetIds().size()) {
            ERR_POST_X(8, Warning << "Invalid 'ids' size in std-seg");
            dim = min(dim, stdseg.GetIds().size());
        }
        int seg_len = seglens[seg_idx++];
        SAlignment_Segment& seg = x_PushSeg(seg_len, dim);
        if ( stdseg.IsSetScores() ) {
            CopyContainer<CStd_seg::TScores, TScores>(
                stdseg.GetScores(), seg.m_Scores);
        }
        unsigned int row_idx = 0;
        ITERATE ( CStd_seg::TLoc, it_loc, (*it)->GetLoc() ) {
            if (row_idx > dim) {
                ERR_POST_X(9, Warning << "Invalid number of rows in std-seg");
                dim = row_idx;
                seg.m_Rows.resize(dim);
            }
            const CSeq_loc& loc = **it_loc;
            const CSeq_id* id = loc.GetId();
            if ( !id ) {
                // All supported location types must have id
                NCBI_THROW(CAnnotMapperException, eBadAlignment,
                        "Missing or multiple seq-ids in std-seg alignment");
            }

            CSeq_loc_Mapper_Base::ESeqType seq_type =
                CSeq_loc_Mapper_Base::eSeq_unknown;
            seq_type = m_LocMapper.GetSeqTypeById(*id);
            int width = (seq_type == CSeq_loc_Mapper_Base::eSeq_prot) ? 3 : 1;
            int start = loc.GetTotalRange().GetFrom()*width;
            int len = loc.GetTotalRange().GetLength()*width;
            ENa_strand strand = eNa_strand_unknown;
            bool have_strand = false;
            switch ( loc.Which() ) {
            case CSeq_loc::e_Empty:
                start = (int)kInvalidSeqPos;
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
                NCBI_THROW(CAnnotMapperException, eBadAlignment,
                        "Unsupported seq-loc type in std-seg alignment");
            }
            if ( have_strand ) {
                m_HaveStrands = true;
                strand = loc.GetStrand();
            }
            if (len > 0  &&  len != seg_len) {
                if (seg_len == -1  &&  seg.m_Len == -1) {
                    // Segment length could not be calculated or is equal for
                    // all rows - safe to use any row's length.
                    seg_len = len;
                    seg.m_Len = len;
                }
                else {
                    NCBI_THROW(CAnnotMapperException, eBadAlignment,
                        "Rows have different lengths in std-seg");
                }
            }
            seg.AddRow(row_idx++, *id, start, m_HaveStrands, strand);
        }
        if (dim != m_Dim) {
            if ( m_Dim ) {
                m_AlignFlags = eAlign_MultiDim;
            }
            m_Dim = max(dim, m_Dim);
        }
    }
}


void CSeq_align_Mapper_Base::x_Init(const CPacked_seg& pseg)
{
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
    if (m_Dim*numseg != pseg.GetPresent().size()) {
        ERR_POST_X(20, Warning << "Invalid 'present' size in packed-seg");
        m_Dim = min(m_Dim*numseg, pseg.GetPresent().size()) / numseg;
    }
    m_HaveStrands = pseg.IsSetStrands();
    if (m_HaveStrands && m_Dim*numseg != pseg.GetStrands().size()) {
        ERR_POST_X(13, Warning << "Invalid 'strands' size in packed-seg");
        m_Dim = min(m_Dim*numseg, pseg.GetStrands().size()) / numseg;
    }
    if ( pseg.IsSetScores() ) {
        CopyContainer<CPacked_seg::TScores, TScores>(
            pseg.GetScores(), m_SegsScores);
    }
    ENa_strand strand = eNa_strand_unknown;
    for (size_t seg = 0;  seg < numseg;  seg++) {
        int seg_width = 1;
        bool have_nuc = false;
        SAlignment_Segment& alnseg = x_PushSeg(pseg.GetLens()[seg], m_Dim);
        for (unsigned int row = 0;  row < m_Dim;  row++) {
            if ( m_HaveStrands ) {
                strand = pseg.GetStrands()[seg*m_Dim + row];
            }
            int row_width = 1;
            const CSeq_id& id = *pseg.GetIds()[row];
            CSeq_loc_Mapper_Base::ESeqType seqtype =
                m_LocMapper.GetSeqTypeById(id);
            if (seqtype == CSeq_loc_Mapper_Base::eSeq_prot) {
                seg_width = 3;
                row_width = 3;
            }
            else {
                have_nuc = true;
            }
            alnseg.AddRow(row, id,
                (pseg.GetPresent()[seg*m_Dim + row] ?
                pseg.GetStarts()[seg*m_Dim + row]*row_width : kInvalidSeqPos),
                m_HaveStrands, strand);
        }
        if (have_nuc  &&  seg_width == 3) {
            NCBI_THROW(CAnnotMapperException, eBadAlignment,
                "Packed-segs with mixed sequence types are not supported");
        }
        alnseg.m_Len *= seg_width;
    }
}


void CSeq_align_Mapper_Base::x_Init(const CSeq_align_set& align_set)
{
    const CSeq_align_set::Tdata& data = align_set.Get();
    ITERATE(CSeq_align_set::Tdata, it, data) {
        m_SubAligns.push_back(Ref(CreateSubAlign(**it)));
    }
}


void CSeq_align_Mapper_Base::InitExon(const CSpliced_seg& spliced,
                                      const CSpliced_exon& exon)
{
    m_OrigExon.Reset(&exon);
    const CSeq_id* gen_id = spliced.IsSetGenomic_id() ?
        &spliced.GetGenomic_id() : 0;
    const CSeq_id* prod_id = spliced.IsSetProduct_id() ?
        &spliced.GetProduct_id() : 0;

    m_Dim = 2;

    if ( exon.IsSetScores() ) {
        CopyContainer<CScore_set::Tdata, TScores>(
            exon.GetScores(), m_SegsScores);
    }

    bool is_prot_prod =
        spliced.GetProduct_type() == CSpliced_seg::eProduct_type_protein;

    m_HaveStrands =
        spliced.IsSetGenomic_strand() || spliced.IsSetProduct_strand();
    ENa_strand gen_strand = spliced.IsSetGenomic_strand() ?
        spliced.GetGenomic_strand() : eNa_strand_unknown;
    ENa_strand prod_strand = spliced.IsSetProduct_strand() ?
        spliced.GetProduct_strand() : eNa_strand_unknown;

    const CSeq_id* ex_gen_id = exon.IsSetGenomic_id() ?
        &exon.GetGenomic_id() : gen_id;
    const CSeq_id* ex_prod_id = exon.IsSetProduct_id() ?
        &exon.GetProduct_id() : prod_id;
    if ( !ex_gen_id  ) {
        ERR_POST_X(14, Warning << "Missing genomic id in spliced-seg");
        return;
    }
    if ( !ex_prod_id ) {
        ERR_POST_X(15, Warning << "Missing product id in spliced-seg");
    }
    m_HaveStrands = m_HaveStrands  ||
        exon.IsSetGenomic_strand() || exon.IsSetProduct_strand();
    ENa_strand ex_gen_strand = exon.IsSetGenomic_strand() ?
        exon.GetGenomic_strand() : gen_strand;
    ENa_strand ex_prod_strand = exon.IsSetProduct_strand() ?
        exon.GetProduct_strand() : prod_strand;

    int gen_start = exon.GetGenomic_start();
    int gen_end = exon.GetGenomic_end() + 1;

    // both start and stop will be converted to genomic coords
    int prod_start, prod_end;

    if ( is_prot_prod ) {
        TSeqPos pstart = exon.GetProduct_start().GetProtpos().GetAmin();
        prod_start = pstart*3 +
            exon.GetProduct_start().GetProtpos().GetFrame() - 1;
        TSeqPos pend = exon.GetProduct_end().GetProtpos().GetAmin();
        prod_end = pend*3 + exon.GetProduct_end().GetProtpos().GetFrame();
    }
    else {
        prod_start = exon.GetProduct_start().GetNucpos();
        prod_end = exon.GetProduct_end().GetNucpos() + 1;
    }

    if ( exon.IsSetParts() ) {
        ITERATE(CSpliced_exon::TParts, it, exon.GetParts()) {
            const CSpliced_exon_chunk& part = **it;
            TSeqPos seg_len =
                CSeq_loc_Mapper_Base::sx_GetExonPartLength(part);
            if (seg_len == 0) {
                continue;
            }

            SAlignment_Segment& alnseg = x_PushSeg(seg_len, 2, gen_strand);
            alnseg.m_PartType = part.Which();

            int part_gen_start;
            if ( part.IsProduct_ins() ) {
                part_gen_start = -1;
            }
            else {
                if ( !IsReverse(gen_strand) ) {
                    part_gen_start = gen_start;
                    gen_start += seg_len;
                }
                else {
                    gen_end -= seg_len;
                    part_gen_start = gen_end;
                }
            }
            alnseg.AddRow(0, *gen_id, part_gen_start,
                m_HaveStrands, gen_strand);

            int part_prod_start;
            if ( part.IsGenomic_ins() ) {
                part_prod_start = -1;
            }
            else {
                if ( !IsReverse(prod_strand) ) {
                    part_prod_start = prod_start;
                    prod_start += seg_len;
                }
                else {
                    prod_end -= seg_len;
                    part_prod_start = prod_end;
                }
            }
            alnseg.AddRow(1, *prod_id, part_prod_start,
                m_HaveStrands, prod_strand);
        }
    }
    else {
        // No parts, use the whole exon
        TSeqPos seg_len = gen_end - gen_start;
        SAlignment_Segment& alnseg = x_PushSeg(seg_len, 2);
        alnseg.AddRow(0, *ex_gen_id, gen_start,
            m_HaveStrands, ex_gen_strand);
        alnseg.AddRow(1, *ex_prod_id, prod_start,
            m_HaveStrands, ex_prod_strand);
    }
}


void CSeq_align_Mapper_Base::x_Init(const CSpliced_seg& spliced)
{
    ITERATE(CSpliced_seg::TExons, it, spliced.GetExons() ) {
        m_SubAligns.push_back(Ref(CreateSubAlign(spliced, **it)));
    }
}


void CSeq_align_Mapper_Base::x_Init(const CSparse_seg& sparse)
{
    // Only single-row alignments are currently supported
    if ( sparse.GetRows().size() > 1) {
        NCBI_THROW(CAnnotMapperException, eBadAlignment,
                "Sparse-segs with multiple rows are not supported");
    }
    if ( sparse.GetRows().empty() ) {
        return;
    }
    if ( sparse.IsSetRow_scores() ) {
        CopyContainer<CSparse_seg::TRow_scores, TScores>(
            sparse.GetRow_scores(), m_SegsScores);
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

    CSeq_loc_Mapper_Base::ESeqType first_type =
        m_LocMapper.GetSeqTypeById(row.GetFirst_id());
    int width = (first_type == CSeq_loc_Mapper_Base::eSeq_prot) ? 3 : 1;
    CSeq_loc_Mapper_Base::ESeqType second_type =
        m_LocMapper.GetSeqTypeById(row.GetSecond_id());
    int second_width =
        (second_type == CSeq_loc_Mapper_Base::eSeq_prot) ? 3 : 1;
    if (width != second_width) {
        NCBI_THROW(CAnnotMapperException, eBadAlignment,
            "Sparse-segs with mixed sequence types are not supported");
    }
    int scores_group = -1;
    if ( row.IsSetSeg_scores() ) {
        scores_group = m_GroupScores.size();
        m_GroupScores.resize(m_GroupScores.size() + 1);
        CopyContainer<CSparse_align::TSeg_scores, TScores>(
            row.GetSeg_scores(), m_GroupScores[scores_group]);
    }
    for (size_t seg = 0;  seg < numseg;  seg++) {
        SAlignment_Segment& alnseg =
            x_PushSeg(row.GetLens()[seg]*width, m_Dim);
        alnseg.m_ScoresGroupIdx = scores_group;
        alnseg.AddRow(0, row.GetFirst_id(),
            row.GetFirst_starts()[seg]*width,
            m_HaveStrands,
            eNa_strand_unknown);
        alnseg.AddRow(1, row.GetSecond_id(),
            row.GetSecond_starts()[seg]*width,
            m_HaveStrands,
            m_HaveStrands ? row.GetSecond_strands()[seg] : eNa_strand_unknown);
    }
}


// Mapping through CSeq_loc_Mapper

void CSeq_align_Mapper_Base::Convert(void)
{
    m_DstAlign.Reset();

    if ( !m_SubAligns.empty() ) {
        NON_CONST_ITERATE(TSubAligns, it, m_SubAligns) {
            (*it)->Convert();
            if ( (*it)->m_ScoresInvalidated ) {
                x_InvalidateScores();
            }
        }
        return;
    }
    x_ConvertAlign(0);
}


void CSeq_align_Mapper_Base::Convert(size_t row)
{
    m_DstAlign.Reset();

    if ( !m_SubAligns.empty() ) {
        NON_CONST_ITERATE(TSubAligns, it, m_SubAligns) {
            (*it)->Convert(row);
            if ( (*it)->m_ScoresInvalidated ) {
                x_InvalidateScores();
            }
        }
        return;
    }
    x_ConvertAlign(&row);
}


void CSeq_align_Mapper_Base::x_ConvertAlign(size_t* row)
{
    if ( m_Segs.empty() ) {
        return;
    }
    if ( row ) {
        x_ConvertRow(*row);
        return;
    }
    for (size_t row_idx = 0; row_idx < m_Dim; ++row_idx) {
        x_ConvertRow(row_idx);
    }
}


void CSeq_align_Mapper_Base::x_ConvertRow(size_t row)
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
        CSeq_id_Handle seg_id = x_ConvertSegment(seg_it, row);
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
CSeq_align_Mapper_Base::x_ConvertSegment(TSegments::iterator& seg_it,
                                         size_t               row)
{
    TSegments::iterator old_it = seg_it;
    SAlignment_Segment& seg = *old_it;
    ++seg_it;
    SAlignment_Segment::SAlignment_Row& aln_row = seg.m_Rows[row];
    if (aln_row.m_Start == kInvalidSeqPos) {
        // skipped row
        return CSeq_id_Handle(); // aln_row.m_Id;
    }

    const CMappingRanges::TIdMap& idmap = m_LocMapper.m_Mappings->GetIdMap();
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
    TSeqPos start = aln_row.m_Start;
    TSeqPos stop = start + seg.m_Len - 1;
    TSeqPos left_shift = 0;
    int group_idx = 0;
    for (size_t map_idx = 0; map_idx < mappings.size(); ++map_idx) {
        CRef<CMappingRange> mapping(mappings[map_idx]);
        if (!mapping->CanMap(start, stop,
            aln_row.m_IsSetStrand  &&  m_LocMapper.m_CheckStrand,
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

        group_idx = mapping->m_Group;

        // At least part of the interval was converted. Calculate
        // trimming coords, trim each row.
        TSeqPos dl = mapping->m_Src_from <= start ?
            0 : mapping->m_Src_from - start;
        TSeqPos dr = mapping->m_Src_to >= stop ?
            0 : stop - mapping->m_Src_to;
        if (dl > 0) {
            // Add segment for the skipped range
            SAlignment_Segment& lseg =
                x_InsertSeg(seg_it, dl, seg.m_Rows.size());
            lseg.m_GroupIdx = group_idx;
            lseg.m_PartType = old_it->m_PartType;
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
        start += dl;
        left_shift += dl;
        // At least part of the interval was converted.
        SAlignment_Segment& mseg = x_InsertSeg(seg_it,
            stop - dr - start + 1, seg.m_Rows.size());
        mseg.m_GroupIdx = group_idx;
        mseg.m_PartType = old_it->m_PartType;
        if (!dl  &&  !dr) {
            // copy scores if there's no truncation
            mseg.m_Scores = seg.m_Scores;
            mseg.m_ScoresGroupIdx = seg.m_ScoresGroupIdx;
        }
        else {
            // Invalidate all scores related to the segment (this
            // includes alignment-level scores).
            x_InvalidateScores(&seg);
        }
        ENa_strand dst_strand = eNa_strand_unknown;
        for (size_t r = 0; r < seg.m_Rows.size(); ++r) {
            SAlignment_Segment::SAlignment_Row& mrow =
                mseg.CopyRow(r, seg.m_Rows[r]);
            if (r == row) {
                // translate id and coords
                CMappingRange::TRange mapped_rg =
                    mapping->Map_Range(start, stop - dr);
                mapping->Map_Strand(
                    aln_row.m_IsSetStrand,
                    aln_row.m_Strand,
                    &dst_strand);
                mrow.m_Id = mapping->m_Dst_id_Handle;
                mrow.m_Start = mapped_rg.GetFrom();
                mrow.m_IsSetStrand =
                    mrow.m_IsSetStrand  ||  (dst_strand != eNa_strand_unknown);
                mrow.m_Strand = dst_strand;
                mrow.SetMapped();
                mseg.m_HaveStrands = mseg.m_HaveStrands  ||
                    mrow.m_IsSetStrand;
                m_HaveStrands = m_HaveStrands  ||  mseg.m_HaveStrands;
            }
            else {
                if (mrow.m_Start != kInvalidSeqPos) {
                    if (mrow.SameStrand(aln_row)) {
                        mrow.m_Start += left_shift;
                    }
                    else {
                        mrow.m_Start +=
                            seg.m_Len - mseg.m_Len - left_shift;
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
        rseg.m_GroupIdx = group_idx;
        rseg.m_PartType = old_it->m_PartType;
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
        int len_width = 1;
        size_t str_idx = 0;
        ITERATE(SAlignment_Segment::TRows, row, seg.m_Rows) {
            CSeq_loc_Mapper_Base::ESeqType seq_type =
                m_LocMapper.GetSeqTypeById(row->m_Id);
            if (seq_type == CSeq_loc_Mapper_Base::eSeq_prot) {
                // If prots are present, segment length must be
                // converted to AAs.
                len_width = 3;
            }
            int seq_width =
                (seq_type == CSeq_loc_Mapper_Base::eSeq_prot) ? 3 : 1;
            CRef<CSeq_id> id(new CSeq_id);
            id.Reset(&const_cast<CSeq_id&>(*row->m_Id.GetSeqId()));
            diag->SetIds().push_back(id);
            diag->SetStarts().push_back(row->GetSegStart()/seq_width);
            if (seg.m_HaveStrands) { // per-segment strands
                // For gaps use the strand of the first mapped row
                diag->SetStrands().
                    push_back((TSeqPos)row->GetSegStart() != kInvalidSeqPos ?
                    row->m_Strand : strands[str_idx]);
            }
            str_idx++;
        }
        diag->SetLen(seg_it->m_Len/len_width);
        if ( !seg.m_Scores.empty() ) {
            CloneContainer<CScore, TScores, CDense_diag::TScores>(
                seg.m_Scores, diag->SetScores());
        }
        diags.push_back(diag);
    }
}


void CSeq_align_Mapper_Base::x_GetDstDenseg(CRef<CSeq_align>& dst) const
{
    CDense_seg& dseg = dst->SetSegs().SetDenseg();
    dseg.SetDim(m_Segs.front().m_Rows.size());
    dseg.SetNumseg(m_Segs.size());
    if ( !m_SegsScores.empty() ) {
        CloneContainer<CScore, TScores, CDense_seg::TScores>(
            m_SegsScores, dseg.SetScores());
    }
    int len_width = 1;
    // Find first non-gap in each row, get its seq-id
    for (size_t r = 0; r < m_Segs.front().m_Rows.size(); r++) {
        bool only_gaps = true;
        ITERATE(TSegments, seg, m_Segs) {
            const SAlignment_Segment::SAlignment_Row& row = seg->m_Rows[r];
            if (row.m_Start != kInvalidSeqPos) {
                CRef<CSeq_id> id(new CSeq_id);
                id.Reset(&const_cast<CSeq_id&>(*row.m_Id.GetSeqId()));
                dseg.SetIds().push_back(id);
                int width = 3; // Dense-seg widths are reversed
                CSeq_loc_Mapper_Base::ESeqType seq_type =
                    m_LocMapper.GetSeqTypeById(row.m_Id);
                if (seq_type != CSeq_loc_Mapper_Base::eSeq_unknown) {
                    // At least some widths are known and can be stored
                    if (seq_type == CSeq_loc_Mapper_Base::eSeq_prot) {
                        len_width = 3;
                        width = 1;
                    }
                }
                only_gaps = false;
                break;
            }
        }
        // The row contains only gaps, don't know how to build a valid denseg
        if ( only_gaps ) {
            NCBI_THROW(CAnnotMapperException, eBadAlignment,
                    "Mapped denseg contains empty row.");
        }
    }
    TStrands strands;
    x_FillKnownStrands(strands);
    ITERATE(TSegments, seg_it, m_Segs) {
        dseg.SetLens().push_back(seg_it->m_Len/len_width);
        size_t str_idx = 0;
        ITERATE(SAlignment_Segment::TRows, row, seg_it->m_Rows) {
            int width = 1;
            if (len_width == 3) {
                // Need to check for prots
                if (m_LocMapper.GetSeqTypeById(row->m_Id) ==
                    CSeq_loc_Mapper_Base::eSeq_prot) {
                    width = 3;
                }
            }
            int start = row->GetSegStart();
            if (start >= 0) {
                start /= width;
            }
            dseg.SetStarts().push_back(start);
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
        if ( !seg_it->m_Scores.empty() ) {
            CloneContainer<CScore, TScores, CStd_seg::TScores>(
                seg_it->m_Scores, std_seg->SetScores());
        }
        ITERATE(SAlignment_Segment::TRows, row, seg_it->m_Rows) {
            int width = (m_LocMapper.GetSeqTypeById(row->m_Id) ==
                CSeq_loc_Mapper_Base::eSeq_prot) ? 3 : 1;
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
                TSeqPos start = row->m_Start/width;
                TSeqPos stop = (row->m_Start + seg_it->m_Len)/width;
                loc->SetInt().SetFrom(start);
                // len may be 0 after dividing by width
                loc->SetInt().SetTo(stop ? stop - 1 : 0);
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
    if ( !m_SegsScores.empty() ) {
        CloneContainer<CScore, TScores, CPacked_seg::TScores>(
            m_SegsScores, pseg.SetScores());
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
        int len_width = 1;
        size_t str_idx = 0;
        ITERATE(SAlignment_Segment::TRows, row, seg_it->m_Rows) {
            TSeqPos start = row->GetSegStart();
            if (m_LocMapper.GetSeqTypeById(row->m_Id) ==
                CSeq_loc_Mapper_Base::eSeq_prot) {
                len_width = 3;
                if (start != kInvalidSeqPos) {
                    start *= 3;
                }
            }
            pseg.SetStarts().push_back(start);
            pseg.SetPresent().push_back(start != kInvalidSeqPos);
            if (m_HaveStrands) {
                pseg.SetStrands().
                    push_back((TSeqPos)row->GetSegStart() != kInvalidSeqPos ?
                    row->m_Strand : strands[str_idx]);
            }
            str_idx++;
        }
        pseg.SetLens().push_back(seg_it->m_Len/len_width);
    }
}


void CSeq_align_Mapper_Base::x_GetDstDisc(CRef<CSeq_align>& dst) const
{
    CSeq_align_set::Tdata& data = dst->SetSegs().SetDisc().Set();
    ITERATE(TSubAligns, it, m_SubAligns) {
        try {
            data.push_back((*it)->GetDstAlign());
        }
        catch (CAnnotMapperException) {
            // Skip invalid sub-alignments (e.g. containing empty rows)
        }
    }
}


void SetPartLength(CSpliced_exon_chunk&          part,
                   CSpliced_exon_chunk::E_Choice ptype,
                   TSeqPos                       len)
{
    switch ( ptype ) {
    case CSpliced_exon_chunk::e_Match:
        part.SetMatch(len);
        break;
    case CSpliced_exon_chunk::e_Mismatch:
        part.SetMismatch(len);
        break;
    case CSpliced_exon_chunk::e_Diag:
        part.SetDiag(len);
        break;
    case CSpliced_exon_chunk::e_Product_ins:
        part.SetProduct_ins(len);
        break;
    case CSpliced_exon_chunk::e_Genomic_ins:
        part.SetGenomic_ins(len);
        break;
    default:
        break;
    }
}


void CSeq_align_Mapper_Base::x_PushExonPart(
    CRef<CSpliced_exon_chunk>&    last_part,
    CSpliced_exon_chunk::E_Choice part_type,
    int                           part_len,
    bool                          reverse,
    CSpliced_exon&                exon) const
{
    if (last_part  &&  last_part->Which() == part_type) {
        SetPartLength(*last_part, part_type,
            CSeq_loc_Mapper_Base::
            sx_GetExonPartLength(*last_part) + part_len);
    }
    else {
        last_part.Reset(new CSpliced_exon_chunk);
        SetPartLength(*last_part, part_type, part_len);
        // Parts order depend on the genomic strand
        if ( !reverse ) {
            exon.SetParts().push_back(last_part);
        }
        else {
            exon.SetParts().push_front(last_part);
        }
    }
}


void CSeq_align_Mapper_Base::
x_GetDstExon(CSpliced_seg&              spliced,
             TSegments::const_iterator& seg,
             CSeq_id_Handle&            gen_id,
             CSeq_id_Handle&            prod_id,
             ENa_strand&                gen_strand,
             ENa_strand&                prod_strand,
             bool&                      partial,
             const CSeq_id_Handle&      last_gen_id,
             const CSeq_id_Handle&      last_prod_id) const
{
    CRef<CSpliced_exon> exon(new CSpliced_exon);
    if (seg != m_Segs.begin()) {
        partial = true;
    }
    int gen_start = -1;
    int prod_start = -1;
    int gen_end = 0;
    int prod_end = 0;
    gen_strand = eNa_strand_unknown;
    prod_strand = eNa_strand_unknown;
    bool gstrand_set = false;
    bool pstrand_set = false;
    bool aln_protein = false;
    if ( spliced.IsSetProduct_type() ) {
        aln_protein =
            spliced.GetProduct_type() == CSpliced_seg::eProduct_type_protein;
    }
    bool ex_partial = false;
    CRef<CSpliced_exon_chunk> last_part;
    int group_idx = -1;
    bool have_non_gaps = false;
    for ( ; seg != m_Segs.end(); ++seg) {
        if (group_idx != -1  &&  seg->m_GroupIdx != group_idx) {
            // New exon
            ex_partial = true;
            break;
        }
        group_idx = seg->m_GroupIdx;

        const SAlignment_Segment::SAlignment_Row& gen_row = seg->m_Rows[0];
        const SAlignment_Segment::SAlignment_Row& prod_row = seg->m_Rows[1];
        if (seg->m_Rows.size() > 2) {
            NCBI_THROW(CAnnotMapperException, eBadAlignment,
                    "Can not construct spliced-seg with more than two rows");
        }

        int gstart = gen_row.GetSegStart();
        int pstart = prod_row.GetSegStart();
        if (gstart >= 0) {
            if (gen_id) {
                if (gen_id != gen_row.m_Id) {
                    NCBI_THROW(CAnnotMapperException, eBadAlignment,
                        "Can not construct spliced-seg -- "
                        "exon parts have different genomic seq-ids");
                }
            }
            else {
                gen_id = gen_row.m_Id;
                exon->SetGenomic_id(const_cast<CSeq_id&>(*gen_id.GetSeqId()));
            }
            _ASSERT(m_LocMapper.GetSeqTypeById(gen_id) !=
                CSeq_loc_Mapper_Base::eSeq_prot);
        }
        if (pstart >= 0) {
            if (prod_id) {
                if (prod_id != prod_row.m_Id) {
                    NCBI_THROW(CAnnotMapperException, eBadAlignment,
                        "Can not construct spliced-seg -- "
                        "exon parts have different product seq-ids");
                }
            }
            else {
                prod_id = prod_row.m_Id;
                exon->SetProduct_id(const_cast<CSeq_id&>(*prod_id.GetSeqId()));
            }
            CSeq_loc_Mapper_Base::ESeqType prod_type =
                m_LocMapper.GetSeqTypeById(prod_id);
            bool part_protein = prod_type == CSeq_loc_Mapper_Base::eSeq_prot;
            if ( spliced.IsSetProduct_type() ) {
                if (part_protein != aln_protein) {
                    // Make sure types are consistent
                    NCBI_THROW(CAnnotMapperException, eBadAlignment,
                        "Can not construct spliced-seg -- "
                        "exons have different product types");
                }
            }
            else {
                aln_protein = part_protein;
                spliced.SetProduct_type(part_protein ?
                    CSpliced_seg::eProduct_type_protein
                    : CSpliced_seg::eProduct_type_transcript);
            }
        }

        CSpliced_exon_chunk::E_Choice ptype = seg->m_PartType;

        // Check strands consistency
        bool gen_reverse = false;
        bool prod_reverse = false;
        if (gstart >= 0  &&  gen_row.m_IsSetStrand) {
            if ( !gstrand_set ) {
                gen_strand = gen_row.m_Strand;
                gstrand_set = true;
            }
            else if (gen_strand != gen_row.m_Strand) {
                NCBI_THROW(CAnnotMapperException, eBadAlignment,
                        "Can not construct spliced-seg "
                        "with different genomic strands in the same exon");
            }
            gen_reverse = IsReverse(gen_strand);
        }
        if (pstart >= 0  &&  prod_row.m_IsSetStrand) {
            if ( !pstrand_set ) {
                prod_strand = prod_row.m_Strand;
                pstrand_set = true;
            }
            else if (prod_strand != prod_row.m_Strand) {
                NCBI_THROW(CAnnotMapperException, eBadAlignment,
                        "Can not construct spliced-seg "
                        "with different product strands in the same exon");
            }
            prod_reverse = IsReverse(prod_strand);
        }

        int gins_len = 0;
        int pins_len = 0;
        if (gstart < 0) {
            if (pstart < 0) {
                // Both gen and prod are missing - start new exon
                ex_partial = true;
                seg++;
                break;
            }
            else {
                ptype = CSpliced_exon_chunk::e_Product_ins;
            }
        }
        else {
            int gend = gen_row.GetSegStart() + seg->m_Len;
            if (gen_start >= 0  &&  gen_end > 0) {
                if (gstart < gen_end) {
                    // The order of starts is not correct, break the exon
                    ex_partial = true;
                    break;
                }
                else if (gstart > gen_end) {
                    // Add genomic insertion
                    gins_len = gstart - gen_end;
                }
            }
            if (gen_start < 0  ||  gen_start > gstart) {
                gen_start = gstart;
            }
            if (gen_end < gend) {
                gen_end = gend;
            }
        }

        if (pstart < 0) {
            _ASSERT(gstart >= 0); // Already checked above
            ptype = CSpliced_exon_chunk::e_Genomic_ins;
        }
        else {
            int pend = prod_row.GetSegStart() + seg->m_Len;
            if (prod_start >= 0  &&  prod_end > 0) {
                // Product and genomic directions are not independent
                if ( prod_reverse == gen_reverse ) {
                    if (pstart < prod_end) {
                        // The order of starts is not correct, break the exon
                        ex_partial = true;
                        break;
                    }
                    else if (pstart > prod_end) {
                        // Add product insertion
                        pins_len = pstart - prod_end;
                    }
                }
                else {
                    if (pend > prod_start) {
                        // The order of starts is not correct, break the exon
                        ex_partial = true;
                        break;
                    }
                    else if (pend < prod_start) {
                        // Add product insertion
                        pins_len = prod_start - pend;
                    }
                }
            }
            if (prod_start < 0  ||  prod_start > pstart) {
                prod_start = pstart;
            }
            if (prod_end < pend) {
                prod_end = pend;
            }
        }

        if (gins_len > 0) {
            x_PushExonPart(last_part, CSpliced_exon_chunk::e_Genomic_ins,
                gins_len, gen_reverse, *exon);
        }
        if (pins_len > 0) {
            x_PushExonPart(last_part, CSpliced_exon_chunk::e_Product_ins,
                pins_len, gen_reverse, *exon);
        }
        x_PushExonPart(last_part, ptype, seg->m_Len, gen_reverse, *exon);

        if (ptype != CSpliced_exon_chunk::e_Genomic_ins  &&
            ptype != CSpliced_exon_chunk::e_Product_ins) {
            have_non_gaps = true;
        }
    }
    partial |= ex_partial;
    if (!have_non_gaps  ||  exon->GetParts().empty()) {
        // No parts were inserted (or only gaps were found) - truncated exon
        partial = true;
        return;
    }
    if ( ex_partial ) {
        exon->SetPartial(true);
    }
    else {
        if ( m_OrigExon->IsSetAcceptor_before_exon() ) {
            exon->SetAcceptor_before_exon().Assign(
                m_OrigExon->GetAcceptor_before_exon());
        }
        if ( m_OrigExon->IsSetDonor_after_exon() ) {
            exon->SetDonor_after_exon().Assign(
                m_OrigExon->GetDonor_after_exon());
        }
    }
    if (!gen_id  &&  last_gen_id) {
        gen_id = last_gen_id;
        exon->SetGenomic_id(const_cast<CSeq_id&>(*gen_id.GetSeqId()));
    }
    if (!prod_id  &&  last_prod_id) {
        prod_id = last_prod_id;
        exon->SetProduct_id(const_cast<CSeq_id&>(*prod_id.GetSeqId()));
    }
    exon->SetGenomic_start(gen_start);
    exon->SetGenomic_end(gen_end - 1);
    if (gen_strand != eNa_strand_unknown) {
        exon->SetGenomic_strand(gen_strand);
    }
    if ( aln_protein ) {
        exon->SetProduct_start().SetProtpos().SetAmin(prod_start/3);
        exon->SetProduct_start().SetProtpos().SetFrame(prod_start%3 + 1);
        exon->SetProduct_end().SetProtpos().SetAmin((prod_end - 1)/3);
        exon->SetProduct_end().SetProtpos().SetFrame((prod_end - 1)%3 + 1);
    }
    else {
        exon->SetProduct_start().SetNucpos(prod_start);
        exon->SetProduct_end().SetNucpos(prod_end - 1);
        if (prod_strand != eNa_strand_unknown) {
            exon->SetGenomic_strand(prod_strand);
        }
    }
    // scores should be copied from the original exon
    if ( !m_SegsScores.empty() ) {
        CloneContainer<CScore, TScores, CScore_set::Tdata>(
            m_SegsScores, exon->SetScores().Set());
    }
    if ( m_OrigExon->IsSetExt() ) {
        CloneContainer<CUser_object, CSpliced_exon::TExt, CSpliced_exon::TExt>(
            m_OrigExon->GetExt(), exon->SetExt());
    }
    spliced.SetExons().push_back(exon);
}


class SegByFirstRow_Less
{
public:
    // prod_rev indicates if product strand is reversed to the genomic
    // one, not just minus.
    SegByFirstRow_Less(bool prod_rev)
        : m_ProdRev(prod_rev) {}

    bool operator()(const SAlignment_Segment& seg1,
                    const SAlignment_Segment& seg2) const
    {
        // Used only for spliced segs, all segments must have two rows
        const SAlignment_Segment::SAlignment_Row& r1 = seg1.m_Rows[0];
        const SAlignment_Segment::SAlignment_Row& r2 = seg2.m_Rows[0];
        // Check if it's a genomic insertion
        if (r1.m_Start != kInvalidSeqPos  &&  r2.m_Start != kInvalidSeqPos) {
            if (r1.m_Id != r2.m_Id) {
                return r1.m_Id < r2.m_Id;
            }
            return r1.m_Start < r2.m_Start;
        }
        // Use product coords in case of genomic insertion
        const SAlignment_Segment::SAlignment_Row& pr1 = seg1.m_Rows[1];
        const SAlignment_Segment::SAlignment_Row& pr2 = seg2.m_Rows[1];
        if (pr1.m_Id != pr2.m_Id) {
            return pr1.m_Id < pr2.m_Id;
        }
        return m_ProdRev ?
            pr1.m_Start > pr2.m_Start : pr1.m_Start < pr2.m_Start;
    }

private:
    bool m_ProdRev; // Product orientation is relative to genetic strand
};


void CSeq_align_Mapper_Base::x_SortSegs(void) const
{
    // Check the strand firts
    bool gen_reverse = false;
    bool prod_reverse = false;
    bool found_gen_strand = false;
    bool found_prod_strand = false;
    ITERATE(TSegments, seg, m_Segs) {
        const SAlignment_Segment::SAlignment_Row& r = seg->m_Rows[0];
        if ( r.m_IsSetStrand ) {
            bool gen_row_rev = IsReverse(r.m_Strand);
            if ( !found_gen_strand ) {
                gen_reverse = gen_row_rev;
                found_gen_strand = true;
            }
            else if (gen_reverse != gen_row_rev) {
                gen_reverse = false; // for mixed strands use direct order
            }
        }
        const SAlignment_Segment::SAlignment_Row& pr = seg->m_Rows[1];
        if ( pr.m_IsSetStrand ) {
            bool prod_row_rev = IsReverse(pr.m_Strand);
            if ( !found_prod_strand ) {
                prod_reverse = prod_row_rev;
                found_prod_strand = true;
            }
            else if (prod_reverse != prod_row_rev) {
                prod_reverse = false; // for mixed strands use direct order
            }
        }
    }
#if defined(NCBI_COMPILER_WORKSHOP)
    // Workshop' STL implementation can not sort lists using functors.
    // This will probably not be fixed in the future due to binaries
    // compatibility. The only way to sort is to use a temporary vector.
    typedef vector<SAlignment_Segment> TSegmentsVector;
    TSegmentsVector tmp;
    tmp.reserve(m_Segs.size());
    ITERATE(TSegments, it, m_Segs) {
        tmp.push_back(*it);
    }
    sort(tmp.begin(), tmp.end(),
         SegByFirstRow_Less(gen_reverse != prod_reverse));
    m_Segs.clear();
    ITERATE(TSegmentsVector, it, tmp) {
        m_Segs.push_back(*it);
    }
#else
    m_Segs.sort(SegByFirstRow_Less(gen_reverse != prod_reverse));
#endif
}


void CSeq_align_Mapper_Base::x_GetDstSpliced(CRef<CSeq_align>& dst) const
{
    CSpliced_seg& spliced = dst->SetSegs().SetSpliced();
    CSeq_id_Handle gen_id;
    CSeq_id_Handle prod_id;
    CSeq_id_Handle last_gen_id;
    CSeq_id_Handle last_prod_id;
    ENa_strand gen_strand = eNa_strand_unknown;
    ENa_strand prod_strand = eNa_strand_unknown;
    bool single_gen_id = true;
    bool single_gen_str = true;
    bool single_prod_id = true;
    bool single_prod_str = true;
    bool partial = false;

    ITERATE(TSubAligns, it, m_SubAligns) {
        // First, sort segments by the first row (genetic) starts.
        (*it)->x_SortSegs();
        TSegments::const_iterator seg = (*it)->m_Segs.begin();
        while (seg != (*it)->m_Segs.end()) {
            CSeq_id_Handle ex_gen_id;
            CSeq_id_Handle ex_prod_id;
            ENa_strand ex_gen_strand = eNa_strand_unknown;
            ENa_strand ex_prod_strand = eNa_strand_unknown;
            (*it)->x_GetDstExon(spliced, seg, ex_gen_id, ex_prod_id,
                ex_gen_strand, ex_prod_strand, partial,
                last_gen_id, last_prod_id);
            if (ex_gen_id) {
                last_gen_id = ex_gen_id;
                if ( !gen_id ) {
                    gen_id = ex_gen_id;
                }
                else {
                    single_gen_id &= gen_id == ex_gen_id;
                }
            }
            if (ex_prod_id) {
                if ( !prod_id ) {
                    prod_id = ex_prod_id;
                }
                else {
                    single_prod_id &= prod_id == ex_prod_id;
                }
            }
            if (ex_gen_strand != eNa_strand_unknown) {
                single_gen_str &= (gen_strand == eNa_strand_unknown) ||
                    (gen_strand == ex_gen_strand);
                gen_strand = ex_gen_strand;
            }
            else {
                single_gen_str &= gen_strand == eNa_strand_unknown;
            }
            if (ex_prod_strand != eNa_strand_unknown) {
                single_prod_str &= (prod_strand == eNa_strand_unknown) ||
                    (prod_strand == ex_prod_strand);
                prod_strand = ex_prod_strand;
            }
            else {
                single_prod_str &= prod_strand == eNa_strand_unknown;
            }
        }
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

    // Reset local values where possible, fill ids in gaps
    NON_CONST_ITERATE(CSpliced_seg::TExons, it, spliced.SetExons()) {
        if ( single_gen_id ) {
            (*it)->ResetGenomic_id();
        }
        else if ( gen_id  &&  !(*it)->IsSetGenomic_id() ) {
            // Use the first known genomic id to fill gaps
            (*it)->SetGenomic_id(const_cast<CSeq_id&>(*gen_id.GetSeqId()));
        }
        if ( single_prod_id ) {
            (*it)->ResetProduct_id();
        }
        else if ( prod_id  &&  !(*it)->IsSetProduct_id() ) {
            // Use the first known product id to fill gaps
            (*it)->SetProduct_id(const_cast<CSeq_id&>(*prod_id.GetSeqId()));
        }
        if ( single_gen_str ) {
            (*it)->ResetGenomic_strand();
        }
        if ( single_prod_str ) {
            (*it)->ResetProduct_strand();
        }
    }

    const CSpliced_seg& orig = m_OrigAlign->GetSegs().GetSpliced();
    if ( orig.IsSetPoly_a() ) {
        spliced.SetPoly_a(orig.GetPoly_a());
    }
    if ( orig.IsSetProduct_length() ) {
        spliced.SetProduct_length(orig.GetProduct_length());
    }
    if (!partial  &&  orig.IsSetModifiers()) {
        ITERATE(CSpliced_seg::TModifiers, it, orig.GetModifiers()) {
            CRef<CSpliced_seg_modifier> mod(new CSpliced_seg_modifier);
            mod->Assign(**it);
            spliced.SetModifiers().push_back(mod);
        }
    }
}


void CSeq_align_Mapper_Base::x_GetDstSparse(CRef<CSeq_align>& dst) const
{
    CSparse_seg& sparse = dst->SetSegs().SetSparse();
    if ( !m_SegsScores.empty() ) {
        CloneContainer<CScore, TScores, CSparse_seg::TRow_scores>(
            m_SegsScores, sparse.SetRow_scores());
    }
    CRef<CSparse_align> aln(new CSparse_align);
    sparse.SetRows().push_back(aln);
    aln->SetNumseg(m_Segs.size());

    CSeq_id_Handle first_idh;
    CSeq_id_Handle second_idh;
    size_t s = 0;
    int scores_group = -2; // -2 -- not yet set; -1 -- already reset.
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
        bool first_prot = m_LocMapper.GetSeqTypeById(first_idh) ==
            CSeq_loc_Mapper_Base::eSeq_prot;
        bool second_prot = m_LocMapper.GetSeqTypeById(second_idh) ==
            CSeq_loc_Mapper_Base::eSeq_prot;
        int first_width = first_prot ? 3 : 1;
        int second_width = second_prot ? 3 : 1;
        int len_width = (first_prot  ||  second_prot) ? 3 : 1;

        int first_start = first_row.GetSegStart();
        int second_start = second_row.GetSegStart();
        if (first_start < 0  ||  second_start < 0) {
            continue;
        }
        aln->SetFirst_starts().push_back(first_start/first_width);
        aln->SetSecond_starts().push_back(second_start/second_width);
        aln->SetLens().push_back(seg->m_Len/len_width);

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

        if (scores_group == -2) { // not yet set
            scores_group = seg->m_ScoresGroupIdx;
        }
        else if (scores_group != seg->m_ScoresGroupIdx) {
            scores_group = -1; // reset
        }
    }
    if (scores_group >= 0) {
        CloneContainer<CScore, TScores, CSparse_align::TSeg_scores>(
            m_GroupScores[scores_group], aln->SetSeg_scores());
    }
}


int CSeq_align_Mapper_Base::x_GetPartialDenseg(CRef<CSeq_align>& dst,
                                               int start_seg) const
{
    CDense_seg& dseg = dst->SetSegs().SetDenseg();
    dst->SetType(CSeq_align::eType_partial);
    dseg.SetDim(m_Segs.front().m_Rows.size());
    CDense_seg::TNumseg num_seg = m_Segs.size();

    int len_width = 1;

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
                if ( !last_id ) {
                    // push id, width and scores only once
                    last_id = row.m_Id;
                    CRef<CSeq_id> id(new CSeq_id);
                    id.Reset(&const_cast<CSeq_id&>(*row.m_Id.GetSeqId()));
                    dseg.SetIds().push_back(id);
                    // Dense-seg's widths are reversed
                    int width = 3;
                    CSeq_loc_Mapper_Base::ESeqType seq_type =
                        m_LocMapper.GetSeqTypeById(row.m_Id);
                    if (seq_type != CSeq_loc_Mapper_Base::eSeq_unknown) {
                        if (seq_type == CSeq_loc_Mapper_Base::eSeq_prot) {
                            width = 1;
                            len_width = 3;
                        }
                    }
                }
            }
            cur_seg++;
            if (cur_seg - start_seg >= num_seg) break;
        }
        num_seg = cur_seg - start_seg;
    }
    dseg.SetNumseg(num_seg);
    TStrands strands;
    x_FillKnownStrands(strands);
    cur_seg = 0;
    int non_empty_segs = 0;
    ITERATE(TSegments, seg_it, m_Segs) {
        if (cur_seg < start_seg) {
            cur_seg++;
            continue;
        }
        if (cur_seg >= start_seg + num_seg) {
            break;
        }
        cur_seg++;
        dseg.SetLens().push_back(seg_it->m_Len/len_width);
        size_t str_idx = 0;
        bool only_gaps = true;
        ITERATE(SAlignment_Segment::TRows, row, seg_it->m_Rows) {
            if (row->m_Start != kInvalidSeqPos) {
                only_gaps = false;
            }
        }
        if (only_gaps) continue;
        non_empty_segs++;
        ITERATE(SAlignment_Segment::TRows, row, seg_it->m_Rows) {
            int width = 1;
            if (m_LocMapper.GetSeqTypeById(row->m_Id) ==
                CSeq_loc_Mapper_Base::eSeq_prot) {
                width = 3;
            }
            int start = row->GetSegStart();
            if (start >= 0) {
                start /= width;
            }
            dseg.SetStarts().push_back(start);
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
    if (non_empty_segs == 0) {
        // The sub-align contains only gaps in all rows, ignore it
        dst.Reset();
    }
    return start_seg + num_seg;
}


void CSeq_align_Mapper_Base::x_ConvToDstDisc(CRef<CSeq_align>& dst) const
{
    // Ignore m_SegsScores -- if we are here, they are probably not valid.
    // Anyway, there's no place to put them in.
    // m_AlignScores may be still used though. (?)
    CSeq_align_set::Tdata& data = dst->SetSegs().SetDisc().Set();
    int seg = 0;
    while (size_t(seg) < m_Segs.size()) {
        CRef<CSeq_align> dseg(new CSeq_align);
        seg = x_GetPartialDenseg(dseg, seg);
        if (!dseg) continue; // The sub-align had only gaps
        data.push_back(dseg);
    }
}


bool CSeq_align_Mapper_Base::x_HaveMixedSeqTypes(void) const
{
    bool have_prot = false;
    bool have_nuc = false;
    ITERATE(TSegments, seg, m_Segs) {
        ITERATE(SAlignment_Segment::TRows, row, seg->m_Rows) {
            CSeq_loc_Mapper_Base::ESeqType seqtype =
                m_LocMapper.GetSeqTypeById(row->m_Id);
            if (seqtype == CSeq_loc_Mapper_Base::eSeq_prot) {
                have_prot = true;
            }
            else /*if (seqtype == CSeq_loc_Mapper_Base::eSeq_nuc)*/ {
                // unknown == nuc
                have_nuc = true;
            }
            if (have_prot  &&  have_nuc) return true;
        }
    }
    return false;
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
    if ( !m_AlignScores.empty() ) {
        CloneContainer<CScore, TScores, CSeq_align::TScore>(
            m_AlignScores, dst->SetScore());
    }
    if (m_OrigAlign->IsSetBounds()) {
        CloneContainer<CSeq_loc, CSeq_align::TBounds, CSeq_align::TBounds>(
            m_OrigAlign->GetBounds(), dst->SetBounds());
    }
    if (m_OrigAlign->IsSetExt()) {
        CloneContainer<CUser_object, CSeq_align::TExt, CSeq_align::TExt>(
            m_OrigAlign->GetExt(), dst->SetExt());
    }
    if ( x_HaveMixedSeqTypes() ) {
        // Only std and spliced can support mixed types.
        x_GetDstStd(dst);
    }
    else {
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
                    x_ConvToDstDisc(dst);
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
                    x_ConvToDstDisc(dst);
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
    }
    return m_DstAlign = dst;
}


CSeq_align_Mapper_Base*
CSeq_align_Mapper_Base::CreateSubAlign(const CSeq_align& align)
{
    return new CSeq_align_Mapper_Base(align, m_LocMapper);
}


CSeq_align_Mapper_Base*
CSeq_align_Mapper_Base::CreateSubAlign(const CSpliced_seg&  spliced,
                                       const CSpliced_exon& exon)
{
    auto_ptr<CSeq_align_Mapper_Base> sub(
        new CSeq_align_Mapper_Base(m_LocMapper));
    sub->InitExon(spliced, exon);
    return sub.release();
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


void CSeq_align_Mapper_Base::
x_InvalidateScores(SAlignment_Segment* seg)
{
    m_ScoresInvalidated = true;
    // Invalidate all global scores
    m_AlignScores.clear();
    m_SegsScores.clear();
    if ( seg ) {
        // Invalidate segment-related scores
        seg->m_Scores.clear();
        seg->m_ScoresGroupIdx = -1;
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE

