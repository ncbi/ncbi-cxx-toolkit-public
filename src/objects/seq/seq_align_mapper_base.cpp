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
    // Make sure the row exists (this should always be true).
    _ASSERT(m_Rows.size() > idx);
    return m_Rows[idx];
}


SAlignment_Segment::SAlignment_Row&
SAlignment_Segment::CopyRow(size_t idx, const SAlignment_Row& src_row)
{
    // Copy the row to this segment. m_Rows must already contain the
    // requested index.
    SAlignment_Row& dst_row = GetRow(idx);
    dst_row = src_row;
    return dst_row;
}


// Add new alignment row. The rows vector must contain the entry.
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


// Add new alignment row. The rows vector must contain the entry.
SAlignment_Segment::SAlignment_Row&
SAlignment_Segment::AddRow(size_t                 idx,
                           const CSeq_id_Handle&  id,
                           int                    start,
                           bool                   is_set_strand,
                           ENa_strand             strand)
{
    SAlignment_Row& row = GetRow(idx);
    row.m_Id = id;
    // If start is negative (-1), use kInvalidSeqPos.
    row.m_Start = start < 0 ? kInvalidSeqPos : start;
    row.m_IsSetStrand = is_set_strand;
    row.m_Strand = strand;
    m_HaveStrands = m_HaveStrands  ||  is_set_strand;
    return row;
}


// Create an empty seq-align mapper. The mapper may be initialized later
// with a seq-align or an exon.
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


// Initialize the mapper with a seq-align.
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


// Helper function to copy a container (scores, user-objects, seq-locs).
// Copies each element, not just pointers.
template<class T, class C1, class C2>
void CloneContainer(const C1& src, C2& dst)
{
    ITERATE(typename C1, it, src) {
        CRef<T> elem(new T);
        elem->Assign(**it);
        dst.push_back(elem);
    }
}


// Copy pointers from source to destination. Used to store scores
// in the parsed segments while mapping an alignment. Should never
// be used to create final mapped alignments.
template<class C1, class C2>
void CopyContainer(const C1& src, C2& dst)
{
    ITERATE(typename C1, it, src) {
        dst.push_back(*it);
    }
}


// Parse the alignment into segments and rows.
void CSeq_align_Mapper_Base::x_Init(const CSeq_align& align)
{
    m_OrigAlign.Reset(&align);
    if (align.IsSetScore()  &&  !align.GetScore().empty()) {
        // Copy global scores. This copies the pointers, not
        // the objects, so, the result should not be copies
        // to the mapped seq-align.
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


// Add new segment with the given length and dimension.
SAlignment_Segment& CSeq_align_Mapper_Base::x_PushSeg(int len,
                                                      size_t dim,
                                                      ENa_strand strand)
{
    // The order of storing parsed segments depends on the strand
    // so that the segments always go in coordinate order, not in
    // biological one.
    if ( !IsReverse(strand) ) {
        m_Segs.push_back(SAlignment_Segment(len, dim));
        return m_Segs.back();
    }
    else {
        m_Segs.push_front(SAlignment_Segment(len, dim));
        return m_Segs.front();
    }
}


// Insert new segment. Used when splitting a partially mapped segment.
SAlignment_Segment&
CSeq_align_Mapper_Base::x_InsertSeg(TSegments::iterator& where,
                                    int                  len,
                                    size_t               dim,
                                    bool                 reverse)
{
    TSegments::iterator ins_it =
        m_Segs.insert(where, SAlignment_Segment(len, dim));
    if ( reverse ) {
        where = ins_it;
    }
    return *ins_it;
}


// Parse dense-diag alignment.
void CSeq_align_Mapper_Base::x_Init(const TDendiag& diags)
{
    ITERATE(TDendiag, diag_it, diags) {
        // Make sure all values are consistent. Post warnings and try to
        // fix any incorrect values.
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
        // Remember if the original alignment contained any strands.
        m_HaveStrands = diag.IsSetStrands();
        if (m_HaveStrands && dim != diag.GetStrands().size()) {
            ERR_POST_X(3, Warning << "Invalid 'strands' size in dendiag");
            dim = min(dim, diag.GetStrands().size());
        }
        if (dim != m_Dim) {
            if ( m_Dim ) {
                // Set the flag indicating that segments have different
                // number of rows.
                m_AlignFlags = eAlign_MultiDim;
            }
            m_Dim = max(dim, m_Dim);
        }
        bool have_prot = false;
        bool have_nuc = false;
        // Initialize next segment.
        SAlignment_Segment& seg = x_PushSeg(diag.GetLen(), dim);
        ENa_strand strand = eNa_strand_unknown;
        if ( diag.IsSetScores() ) {
            // Store per-segment scores if any.
            CopyContainer<CDense_diag::TScores, TScores>(
                diag.GetScores(), seg.m_Scores);
        }
        for (size_t row = 0; row < dim; ++row) {
            if ( m_HaveStrands ) {
                strand = diag.GetStrands()[row];
            }
            const CSeq_id& row_id = *diag.GetIds()[row];
            int row_start = diag.GetStarts()[row];
            // Adjust coordinates so that they are always genomic.
            CSeq_loc_Mapper_Base::ESeqType row_type =
                m_LocMapper.GetSeqTypeById(row_id);
            if (row_type == CSeq_loc_Mapper_Base::eSeq_prot) {
                if ( !have_prot ) {
                    // Adjust segment length only once!
                    have_prot = true;
                    seg.m_Len *= 3;
                }
                row_start *= 3;
            }
            else /*if (row_type == CSeq_loc_Mapper_Base::eSeq_nuc)*/ {
                have_nuc = true;
            }
            // Add row.
            seg.AddRow(row, row_id, row_start, m_HaveStrands, strand);
        }
        if (have_prot  &&  have_nuc) {
            // This type of alignment does not support mixing sequence types.
            NCBI_THROW(CAnnotMapperException, eBadAlignment,
                "Dense-diags with mixed sequence types are not supported");
        }
    }
}


// Parse dense-seg.
void CSeq_align_Mapper_Base::x_Init(const CDense_seg& denseg)
{
    m_Dim = denseg.GetDim();
    size_t numseg = denseg.GetNumseg();
    // Make sure all values are consistent. Post warnings and try to
    // fix any incorrect values.
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
        // Store scores in the segments. Only pointers are copied,
        // the objects are cloned only to the final mapped alignment.
        CopyContainer<CDense_seg::TScores, TScores>(
            denseg.GetScores(), m_SegsScores);
    }
    ENa_strand strand = eNa_strand_unknown;
    for (size_t seg = 0;  seg < numseg;  seg++) {
        // Create new segment.
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
                // Treat unknown type as nuc.
                have_nuc = true;
            }
            int start = denseg.GetStarts()[seg*m_Dim + row]*width;
            alnseg.AddRow(row, seq_id, start, m_HaveStrands, strand);
        }
        if (have_prot  &&  have_nuc) {
            NCBI_THROW(CAnnotMapperException, eBadAlignment,
                "Dense-segs with mixed sequence types are not supported");
        }
        // For proteins segment length needs to be adjusted.
        if ( have_prot ) {
            alnseg.m_Len *= 3;
        }
    }
}


// Parse std-seg.
void CSeq_align_Mapper_Base::x_Init(const TStd& sseg)
{
    vector<int> seglens;
    seglens.reserve(sseg.size());
    // Several passes are required to detect sequence types and lengths.
    ITERATE(CSeq_align::C_Segs::TStd, it, sseg) {
        // Two different location lengths are allowed - for nucs and prots.
        int minlen = 0;
        int maxlen = 0;
        // First pass - find min and max segment lengths.
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
            // Store min and max lengths of locations. By default use min.
            if (minlen == 0  ||  len == minlen) {
                minlen = len;
            }
            else if (maxlen == 0  ||  len == maxlen) {
                maxlen = len;
                // If necessary, swap the two lengths.
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
        // Two different lengths were found. Try to guess sequence types.
        if (minlen != 0  &&  maxlen != 0) {
            if (minlen*3 != maxlen) {
                NCBI_THROW(CAnnotMapperException, eBadAlignment,
                    "Inconsistent seq-loc lengths in std-seg rows");
            }
            // Found both nucs and prots - make the second pass and
            // store widths for all sequences.
            ITERATE( CStd_seg::TLoc, it_loc, (*it)->GetLoc()) {
                const CSeq_loc& loc = **it_loc;
                const CSeq_id* id = loc.GetId();
                int len = loc.GetTotalRange().GetLength();
                if (len == 0  ||  loc.IsWhole()) {
                    continue; // ignore unknown lengths
                }
                _ASSERT(id); // All locations should have been checked.
                CSeq_loc_Mapper_Base::ESeqType newtype = (len == minlen) ?
                    CSeq_loc_Mapper_Base::eSeq_prot
                    : CSeq_loc_Mapper_Base::eSeq_nuc;
                CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*id);
                // Check if seq-type is available from the location mapper.
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
                    // Set type anyway -- x_AdjustSeqTypesToProt could ignore it.
                    m_LocMapper.SetSeqTypeById(idh, newtype);
                }
            }
        }
        // -1 indicates unknown sequence type or equal lengths for all rows.
        // We need to know this to use the correct length below, so use -1
        // rather than real length.
        seglens.push_back(maxlen == 0 ? -1 : maxlen);
    }
    // By this point all possible sequence types should be detected and
    // stored in the loc-mapper.
    // All unknown types are treated as nucs.

    size_t seg_idx = 0;
    // Final pass - parse the alignment.
    ITERATE (CSeq_align::C_Segs::TStd, it, sseg) {
        const CStd_seg& stdseg = **it;
        size_t dim = stdseg.GetDim();
        if (stdseg.IsSetIds()
            && dim != stdseg.GetIds().size()) {
            ERR_POST_X(8, Warning << "Invalid 'ids' size in std-seg");
            dim = min(dim, stdseg.GetIds().size());
        }
        // seg_len may be -1 indicating that the real length is
        // unknown (due to unknown sequence type or a non-interval location).
        // We'll fix this later.
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
                // All supported location types must have a single id.
                NCBI_THROW(CAnnotMapperException, eBadAlignment,
                        "Missing or multiple seq-ids in std-seg alignment");
            }

            CSeq_loc_Mapper_Base::ESeqType seq_type =
                CSeq_loc_Mapper_Base::eSeq_unknown;
            seq_type = m_LocMapper.GetSeqTypeById(*id);
            int width = (seq_type == CSeq_loc_Mapper_Base::eSeq_prot) ? 3 : 1;
            // Empty and whole locations will set the correct start and length
            // below, gon't check this now.
            int start = loc.GetTotalRange().GetFrom()*width;
            int len = loc.GetTotalRange().GetLength()*width;
            ENa_strand strand = eNa_strand_unknown;
            bool have_strand = false;
            switch ( loc.Which() ) {
            case CSeq_loc::e_Empty:
                // Adjust start, length should be 0.
                start = (int)kInvalidSeqPos;
                break;
            case CSeq_loc::e_Whole:
                start = 0;
                len = 0; // Set length to 0 - it's unknown.
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
            // Now the final adjustment of the length. If for the current row
            // it's set, but not equal to the segment-wide length, there are
            // two possibilities:
            if (len > 0  &&  len != seg_len) {
                // The segment-wide length is unknown or equal for all rows.
                // We can set it now, when we have at least one row with
                // real length.
                if (seg_len == -1  &&  seg.m_Len == -1) {
                    seg_len = len;
                    seg.m_Len = len;
                }
                else {
                    // The segment-wide length is known, but different from
                    // this row's length. Fail.
                    NCBI_THROW(CAnnotMapperException, eBadAlignment,
                        "Rows have different lengths in std-seg");
                }
            }
            seg.AddRow(row_idx++, *id, start, m_HaveStrands, strand);
        }
        // Check if all segments have the same number of rows.
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
    // Make sure all values are consistent. Post warnings and try to
    // fix any incorrect values.
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
        // Copy pointers to scores if any.
        CopyContainer<CPacked_seg::TScores, TScores>(
            pseg.GetScores(), m_SegsScores);
    }
    ENa_strand strand = eNa_strand_unknown;
    for (size_t seg = 0;  seg < numseg;  seg++) {
        // By default treat the segment as nuc-only, don't adjust lengths.
        // If there are any proteins involved, this will be set to 3.
        int seg_width = 1;
        // Remember if there are any nucs.
        bool have_nuc = false;
        SAlignment_Segment& alnseg = x_PushSeg(pseg.GetLens()[seg], m_Dim);
        for (unsigned int row = 0;  row < m_Dim;  row++) {
            if ( m_HaveStrands ) {
                strand = pseg.GetStrands()[seg*m_Dim + row];
            }
            // Check sequence type for this row.
            int row_width = 1;
            const CSeq_id& id = *pseg.GetIds()[row];
            CSeq_loc_Mapper_Base::ESeqType seqtype =
                m_LocMapper.GetSeqTypeById(id);
            // If this is a protein, adjust widths.
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
        // If there are both nucs and prots, fail.
        if (have_nuc  &&  seg_width == 3) {
            NCBI_THROW(CAnnotMapperException, eBadAlignment,
                "Packed-segs with mixed sequence types are not supported");
        }
        // If there are only prots, adjust segment length.
        alnseg.m_Len *= seg_width;
    }
}


// Parse align-set
void CSeq_align_Mapper_Base::x_Init(const CSeq_align_set& align_set)
{
    // Iterate sub-alignments, create a new mapper for each of them.
    const CSeq_align_set::Tdata& data = align_set.Get();
    ITERATE(CSeq_align_set::Tdata, it, data) {
        m_SubAligns.push_back(Ref(CreateSubAlign(**it)));
    }
}


// Parse a single splices exon. A separate align-mapper is created
// for each exon.
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
        // Copy pointers to scores if any.
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

    // Get per-exon ids, use per-alignment ids if local ones are not set.
    const CSeq_id* ex_gen_id = exon.IsSetGenomic_id() ?
        &exon.GetGenomic_id() : gen_id;
    const CSeq_id* ex_prod_id = exon.IsSetProduct_id() ?
        &exon.GetProduct_id() : prod_id;
    // Make sure ids are set at least somewhere.
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

    // Both start and stop will be converted to genomic coords.
    int prod_start, prod_end;

    prod_start = exon.GetProduct_start().AsSeqPos();
    prod_end = exon.GetProduct_end().AsSeqPos() + 1;

    if ( exon.IsSetParts() ) {
        // Iterate exon parts.
        ITERATE(CSpliced_exon::TParts, it, exon.GetParts()) {
            const CSpliced_exon_chunk& part = **it;
            // The length in spliced-seg is already genomic.
            TSeqPos seg_len =
                CSeq_loc_Mapper_Base::sx_GetExonPartLength(part);
            if (seg_len == 0) {
                continue;
            }

            SAlignment_Segment& alnseg = x_PushSeg(seg_len, 2);
            alnseg.m_PartType = part.Which();

            int part_gen_start;
            // Check the genomic strand only if genomic sequence is not
            // missing.
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
            alnseg.AddRow(CSeq_loc_Mapper_Base::eSplicedRow_Gen,
                *gen_id, part_gen_start, m_HaveStrands, gen_strand);

            int part_prod_start;
            // Check the product strand only if product sequence is not
            // missing.
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
            alnseg.AddRow(CSeq_loc_Mapper_Base::eSplicedRow_Prod,
                *prod_id, part_prod_start, m_HaveStrands, prod_strand);
        }
    }
    else {
        // No parts, use the whole exon.
        TSeqPos seg_len = gen_end - gen_start;
        SAlignment_Segment& alnseg = x_PushSeg(seg_len, 2);
        alnseg.m_PartType = CSpliced_exon_chunk::e_Match;
        alnseg.AddRow(CSeq_loc_Mapper_Base::eSplicedRow_Gen,
            *ex_gen_id, gen_start, m_HaveStrands, ex_gen_strand);
        alnseg.AddRow(CSeq_loc_Mapper_Base::eSplicedRow_Prod,
            *ex_prod_id, prod_start, m_HaveStrands, ex_prod_strand);
    }
}


// Parse spliced-seg.
void CSeq_align_Mapper_Base::x_Init(const CSpliced_seg& spliced)
{
    // Iterate exons, create sub-mapper for each one.
    ITERATE(CSpliced_seg::TExons, it, spliced.GetExons() ) {
        m_SubAligns.push_back(Ref(CreateSubAlign(spliced, **it)));
    }
}


// Parse sparse-seg.
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
        // Copy pointers to the scores.
        CopyContainer<CSparse_seg::TRow_scores, TScores>(
            sparse.GetRow_scores(), m_SegsScores);
    }

    // Make sure all values are consistent. Post warnings and try to
    // fix any incorrect values.
    const CSparse_align& row = *sparse.GetRows().front();
    m_Dim = 2;

    size_t numseg = row.GetNumseg();
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

    // Check sequence types, make sure they are the same.
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
        // If per-row scores are set, store them along with the group number.
        // Only pointers are copied.
        scores_group = m_GroupScores.size();
        m_GroupScores.resize(m_GroupScores.size() + 1);
        CopyContainer<CSparse_align::TSeg_scores, TScores>(
            row.GetSeg_scores(), m_GroupScores[scores_group]);
    }
    // Iterate segments.
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

// Convert the whole seq-align.
void CSeq_align_Mapper_Base::Convert(void)
{
    m_DstAlign.Reset();

    // If the alignment is a set of sub-alignments, iterate all sub-mappers.
    if ( !m_SubAligns.empty() ) {
        NON_CONST_ITERATE(TSubAligns, it, m_SubAligns) {
            (*it)->Convert();
            // Check if the top-level scores must be invalidated.
            // If any sub-mapper has invalidated its scores due
            // to partial mapping, the global scores are also
            // not valid anymore.
            if ( (*it)->m_ScoresInvalidated ) {
                x_InvalidateScores();
            }
        }
        return;
    }
    // This is a single alignment with one level - map it.
    // NULL is a pointer to the row to be mapped. If it's NULL,
    // all rows are mapped.
    x_ConvertAlign(NULL);
}


// convert a single alignment row.
void CSeq_align_Mapper_Base::Convert(size_t row)
{
    m_DstAlign.Reset();

    // If the alignment is a set of sub-alignments, iterate all sub-mappers.
    if ( !m_SubAligns.empty() ) {
        NON_CONST_ITERATE(TSubAligns, it, m_SubAligns) {
            (*it)->Convert(row);
            if ( (*it)->m_ScoresInvalidated ) {
                x_InvalidateScores();
            }
        }
        return;
    }
    // This is a single alignment with one level - map the requested row.
    x_ConvertAlign(&row);
}


// Map a single alignment row if it't not NULL or all rows.
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


// Map a single row.
void CSeq_align_Mapper_Base::x_ConvertRow(size_t row)
{
    CSeq_id_Handle dst_id;
    // Iterate all segments.
    TSegments::iterator seg_it = m_Segs.begin();
    for ( ; seg_it != m_Segs.end(); ) {
        if (seg_it->m_Rows.size() <= row) {
            // No such row in the current segment
            ++seg_it;
            // This alignment has different number of rows in
            // different segments.
            m_AlignFlags = eAlign_MultiDim;
            continue;
        }
        // Try to convert the current segment.
        CSeq_id_Handle seg_id = x_ConvertSegment(seg_it, row);
        if (seg_id) {
            // Success. Check if all mappings resulted in the
            // same mapped id.
            if (dst_id  &&  dst_id != seg_id  &&
                m_AlignFlags == eAlign_Normal) {
                // Mark the alignment as having multiple ids per row.
                // Not all alignment types support this, so we may need
                // to change the type from the original one later.
                m_AlignFlags = eAlign_MultiId;
            }
            // Remember the last mapped id.
            dst_id = seg_id;
        }
    }
}


// Convert a single segment of a single row.
// This is where the real mapping is done.
CSeq_id_Handle
CSeq_align_Mapper_Base::x_ConvertSegment(TSegments::iterator& seg_it,
                                         size_t               row)
{
    // Remember the iterator position - mapping can add segments,
    // we need to know which should be mapped next.
    // old_it keeps the segment to be mapped, seg_it is the next segment,
    // any additional segments are inserted before it.
    TSegments::iterator old_it = seg_it;
    SAlignment_Segment& seg = *old_it;
    ++seg_it;

    SAlignment_Segment::SAlignment_Row& aln_row = seg.m_Rows[row];

    // Find all matching mappings.
    const CMappingRanges::TIdMap& idmap = m_LocMapper.m_Mappings->GetIdMap();
    CMappingRanges::TIdIterator id_it = idmap.find(aln_row.m_Id);
    if (id_it == idmap.end()) {
        // Id not found in the segment, leave the row unchanged.
        return aln_row.m_Id;
    }
    const CMappingRanges::TRangeMap& rmap = id_it->second;
    if ( rmap.empty() ) {
        // No mappings for this segment - the row should not be
        // changed. Return the original id.
        return aln_row.m_Id;
    }
    // Sort mappings related to this segment/row.
    typedef vector< CRef<CMappingRange> > TSortedMappings;
    TSortedMappings mappings;
    CMappingRanges::TRangeIterator rg_it = rmap.begin();
    for ( ; rg_it; ++rg_it) {
        mappings.push_back(rg_it->second);
    }
    sort(mappings.begin(), mappings.end(), CMappingRangeRef_Less());

    CSeq_id_Handle dst_id;

    // Handle rows with gaps.
    if (aln_row.m_Start == kInvalidSeqPos) {
        // Gap. Check the mappings. If there's at least one mapping for this
        // id, change it to the destination one.
        dst_id = mappings[0]->GetDstIdHandle();
        // If there are multiple mappings, check if they all have the same
        // destination id. If there are many of them, do nothing - this gap
        // can not be mapped.
        if (mappings.size() > 1) {
            ITERATE(TSortedMappings, it, mappings) {
                if ((*it)->GetDstIdHandle() != dst_id) {
                    return CSeq_id_Handle(); // Use empty id to report gaps.
                }
            }
        }
        // There's just one destination id, map the gap.
        seg.m_Rows[row].m_Id = dst_id;
        seg.m_Rows[row].SetMapped();
        return seg.m_Rows[row].m_Id;
    }

    // Prepare insert point depending on the source strand
    TSegments::iterator ins_point = seg_it;
    bool src_reverse = aln_row.m_IsSetStrand ? IsReverse(aln_row.m_Strand) : false;

    bool mapped = false;
    EAlignFlags align_flags = eAlign_Normal;
    TSeqPos start = aln_row.m_Start;
    TSeqPos stop = start + seg.m_Len - 1;
    // left_shift indicates which portion of the segment has been mapped
    // so far.
    TSeqPos left_shift = 0;
    int group_idx = 0;
    for (size_t map_idx = 0; map_idx < mappings.size(); ++map_idx) {
        CRef<CMappingRange> mapping(mappings[map_idx]);
        if (!mapping->CanMap(start, stop,
            aln_row.m_IsSetStrand  &&  m_LocMapper.m_CheckStrand,
            aln_row.m_Strand)) {
            // Mapping does not apply to this segment/row, leave it unchanged.
            continue;
        }

        // Check the destination id, set the flag if the row is mapped
        // to multiple ids.
        if ( dst_id ) {
            if (mapping->m_Dst_id_Handle != dst_id) {
                align_flags = eAlign_MultiId;
            }
        }
        dst_id = mapping->m_Dst_id_Handle;

        group_idx = mapping->m_Group;

        // At least part of the interval was converted. Calculate
        // trimming coords, split each row if necessary. We will need to add
        // new segments on the left/right to preserve the parts which could
        // not be mapped.
        TSeqPos dl = mapping->m_Src_from <= start ?
            0 : mapping->m_Src_from - start;
        TSeqPos dr = mapping->m_Src_to >= stop ?
            0 : stop - mapping->m_Src_to;
        if (dl > 0) {
            // Add segment for the skipped range on the left.
            // Copy the original segment.
            SAlignment_Segment& lseg =
                x_InsertSeg(ins_point, dl, seg.m_Rows.size(), src_reverse);
            lseg.m_GroupIdx = group_idx;
            lseg.m_PartType = old_it->m_PartType;
            // Iterate all rows, adjust their starts.
            for (size_t r = 0; r < seg.m_Rows.size(); ++r) {
                SAlignment_Segment::SAlignment_Row& lrow =
                    lseg.CopyRow(r, seg.m_Rows[r]);
                if (r == row) {
                    // The row which could not be mapped has a gap.
                    lrow.m_Start = kInvalidSeqPos;
                    lrow.m_Id = dst_id;
                }
                else if (lrow.m_Start != kInvalidSeqPos) {
                    // All other rows have new starts.
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
        // At least part of the interval was converted. Add new segment for
        // this range.
        SAlignment_Segment& mseg = x_InsertSeg(ins_point,
            stop - dr - start + 1, seg.m_Rows.size(), src_reverse);
        mseg.m_GroupIdx = group_idx;
        mseg.m_PartType = old_it->m_PartType;
        if (!dl  &&  !dr) {
            // Copy scores if there's no truncation.
            mseg.m_Scores = seg.m_Scores;
            mseg.m_ScoresGroupIdx = seg.m_ScoresGroupIdx;
        }
        else {
            // Invalidate all scores related to the segment and all
            // parent's scores.
            x_InvalidateScores(&seg);
        }
        ENa_strand dst_strand = eNa_strand_unknown;
        // Fill the new segment.
        for (size_t r = 0; r < seg.m_Rows.size(); ++r) {
            SAlignment_Segment::SAlignment_Row& mrow =
                mseg.CopyRow(r, seg.m_Rows[r]);
            if (r == row) {
                // Translate id and coords of the mapped row.
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
                // Adjust starts of all other rows.
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
        if (start > stop) break;
    }
    // Update alignment flags.
    if (align_flags == eAlign_MultiId  &&  m_AlignFlags == eAlign_Normal) {
        m_AlignFlags = align_flags;
    }
    if ( !mapped ) {
        // Nothing could be mapped from this row, although some mappings for
        // the id do exist. Do not erase the segment, just change the row id
        // and reset start to convert it to gap on the destination sequence.
        // Use destination id of the first mapping for the source id. This
        // should not be very important, since we have a gap anyway. (?)
        seg.m_Rows[row].m_Start = kInvalidSeqPos;
        seg.m_Rows[row].m_Id = rmap.begin()->second->m_Dst_id_Handle;
        seg.m_Rows[row].SetMapped();
        return seg.m_Rows[row].m_Id;
    }
    if (start <= stop) {
        // Add the remaining unmapped range if any.
        SAlignment_Segment& rseg = x_InsertSeg(ins_point,
            stop - start + 1, seg.m_Rows.size(), src_reverse);
        rseg.m_GroupIdx = group_idx;
        rseg.m_PartType = old_it->m_PartType;
        for (size_t r = 0; r < seg.m_Rows.size(); ++r) {
            SAlignment_Segment::SAlignment_Row& rrow =
                rseg.CopyRow(r, seg.m_Rows[r]);
            if (r == row) {
                // The mapped row was truncated and now has a gap.
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
    // Remove the original segment from the alignment.
    m_Segs.erase(old_it);
    return align_flags == eAlign_MultiId ? CSeq_id_Handle() : dst_id;
}


// Get mapped alignment

// Checks each row for strand information. If found, store the
// strand in the container. It will be used to set strand in gaps.
// Looks only for the first known strand in each row. Does not
// check if strand is the same for the whole row.
void CSeq_align_Mapper_Base::x_FillKnownStrands(TStrands& strands) const
{
    strands.clear();
    size_t max_rows = m_Segs.front().m_Rows.size();
    if (m_AlignFlags & eAlign_MultiDim) {
        // Segments may contain different number of rows, check each segment.
        ITERATE(TSegments, seg_it, m_Segs) {
            if (seg_it->m_Rows.size() > max_rows) {
                max_rows = seg_it->m_Rows.size();
            }
        }
    }
    strands.reserve(max_rows);
    for (size_t r_idx = 0; r_idx < max_rows; r_idx++) {
        ENa_strand strand = eNa_strand_unknown;
        // Skip gaps, try find a row with mapped strand
        ITERATE(TSegments, seg_it, m_Segs) {
            // Make sure the row exists in the current segment.
            if (seg_it->m_Rows.size() <= r_idx) continue;
            if (seg_it->m_Rows[r_idx].GetSegStart() != -1) {
                strand = seg_it->m_Rows[r_idx].m_Strand;
                break;
            }
        }
        // Store the strand.
        strands.push_back(strand == eNa_strand_unknown ?
                          eNa_strand_plus : strand);
    }
}


// Create dense-diag alignment.
void CSeq_align_Mapper_Base::x_GetDstDendiag(CRef<CSeq_align>& dst) const
{
    TDendiag& diags = dst->SetSegs().SetDendiag();
    TStrands strands;
    // Get information about strands for each row.
    x_FillKnownStrands(strands);
    // Create dense-diag for each segment.
    ITERATE(TSegments, seg_it, m_Segs) {
        const SAlignment_Segment& seg = *seg_it;
        CRef<CDense_diag> diag(new CDense_diag);
        diag->SetDim(seg.m_Rows.size());
        int len_width = 1;
        size_t str_idx = 0; // row index in the strands container
        // Add each row to the dense-seg.
        ITERATE(SAlignment_Segment::TRows, row, seg.m_Rows) {
            if (row->m_Start == kInvalidSeqPos) {
                // Dense-diags do not support gaps ('starts' contain
                // TSeqPos which can not be negative).
                NCBI_THROW(CAnnotMapperException, eBadAlignment,
                    "Mapped alignment contains gaps and can not be "
                    "converted to dense-diag.");
            }
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
                // For gaps use the strand of the first mapped row,
                // see x_FillKnownStrands.
                diag->SetStrands().
                    push_back((TSeqPos)row->GetSegStart() != kInvalidSeqPos ?
                    row->m_Strand : strands[str_idx]);
            }
            str_idx++; // move to the strand for the next row
        }
        // Adjust segment length is there are any proteins.
        diag->SetLen(seg_it->m_Len/len_width);
        if ( !seg.m_Scores.empty() ) {
            // This will copy every element rather just pointers.
            CloneContainer<CScore, TScores, CDense_diag::TScores>(
                seg.m_Scores, diag->SetScores());
        }
        diags.push_back(diag);
    }
}


// Create dense-seg alignment.
void CSeq_align_Mapper_Base::x_GetDstDenseg(CRef<CSeq_align>& dst) const
{
    // Make sure all segments have the same number of rows -
    // dense-seg does not support multi-dim alignments.
    _ASSERT((m_AlignFlags & eAlign_MultiDim) == 0);

    CDense_seg& dseg = dst->SetSegs().SetDenseg();
    dseg.SetDim(m_Segs.front().m_Rows.size());
    dseg.SetNumseg(m_Segs.size());
    if ( !m_SegsScores.empty() ) {
        // This will copy every element rather just pointers.
        CloneContainer<CScore, TScores, CDense_seg::TScores>(
            m_SegsScores, dseg.SetScores());
    }
    int len_width = 1;
    // First pass: find first non-gap in each row, get its seq-id.
    for (size_t r = 0; r < m_Segs.front().m_Rows.size(); r++) {
        bool only_gaps = true;
        ITERATE(TSegments, seg, m_Segs) {
            const SAlignment_Segment::SAlignment_Row& row = seg->m_Rows[r];
            if (row.m_Start != kInvalidSeqPos) {
                // Not a gap - store the id
                CRef<CSeq_id> id(new CSeq_id);
                id.Reset(&const_cast<CSeq_id&>(*row.m_Id.GetSeqId()));
                dseg.SetIds().push_back(id);
                // Check sequence type, remember if lengths
                // need to be adjusted.
                CSeq_loc_Mapper_Base::ESeqType seq_type =
                    m_LocMapper.GetSeqTypeById(row.m_Id);
                if (seq_type != CSeq_loc_Mapper_Base::eSeq_unknown) {
                    if (seq_type == CSeq_loc_Mapper_Base::eSeq_prot) {
                        len_width = 3;
                    }
                }
                only_gaps = false;
                break; // No need to check other segments of this row.
            }
        }
        // The row contains only gaps, don't know how to build a valid denseg
        if ( only_gaps ) {
            NCBI_THROW(CAnnotMapperException, eBadAlignment,
                    "Mapped denseg contains empty row.");
        }
    }
    // Get information about strands for each row.
    TStrands strands;
    x_FillKnownStrands(strands);
    ITERATE(TSegments, seg_it, m_Segs) {
        dseg.SetLens().push_back(seg_it->m_Len/len_width);
        size_t str_idx = 0; // strands index for the current row
        ITERATE(SAlignment_Segment::TRows, row, seg_it->m_Rows) {
            int width = 1;
            // Are there any proteins in the alignment?
            if (len_width == 3) {
                // Adjust coordinates for proteins.
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
            // Are there any strands involved at all?
            if (m_HaveStrands) {
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


// Create std-seg alignment.
void CSeq_align_Mapper_Base::x_GetDstStd(CRef<CSeq_align>& dst) const
{
    TStd& std_segs = dst->SetSegs().SetStd();
    ITERATE(TSegments, seg_it, m_Segs) {
        // Create new std-seg for each segment.
        CRef<CStd_seg> std_seg(new CStd_seg);
        std_seg->SetDim(seg_it->m_Rows.size());
        if ( !seg_it->m_Scores.empty() ) {
            // Copy scores (not just pointers).
            CloneContainer<CScore, TScores, CStd_seg::TScores>(
                seg_it->m_Scores, std_seg->SetScores());
        }
        // Add rows.
        ITERATE(SAlignment_Segment::TRows, row, seg_it->m_Rows) {
            // Check sequence type, set width to 3 for prots.
            int width = (m_LocMapper.GetSeqTypeById(row->m_Id) ==
                CSeq_loc_Mapper_Base::eSeq_prot) ? 3 : 1;
            CRef<CSeq_id> id(new CSeq_id);
            id.Reset(&const_cast<CSeq_id&>(*row->m_Id.GetSeqId()));
            std_seg->SetIds().push_back(id);
            CRef<CSeq_loc> loc(new CSeq_loc);
            // For gaps use empty seq-loc.
            if (row->m_Start == kInvalidSeqPos) {
                // empty
                loc->SetEmpty(*id);
            }
            else {
                // For normal ranges use seq-interval.
                loc->SetInt().SetId(*id);
                // Adjust coordinates according to the sequence type.
                TSeqPos start = row->m_Start/width;
                TSeqPos stop = (row->m_Start + seg_it->m_Len)/width;
                loc->SetInt().SetFrom(start);
                // len may be 0 after dividing by width, check it before
                // decrementing stop.
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


// Create packed-seg alignment.
void CSeq_align_Mapper_Base::x_GetDstPacked(CRef<CSeq_align>& dst) const
{
    // Multi-dim alignments are not supported by this type.
    _ASSERT((m_AlignFlags & eAlign_MultiDim) == 0);

    CPacked_seg& pseg = dst->SetSegs().SetPacked();
    pseg.SetDim(m_Segs.front().m_Rows.size());
    pseg.SetNumseg(m_Segs.size());
    if ( !m_SegsScores.empty() ) {
        // Copy elements, not just pointers.
        CloneContainer<CScore, TScores, CPacked_seg::TScores>(
            m_SegsScores, pseg.SetScores());
    }
    // Get strands for all rows.
    TStrands strands;
    x_FillKnownStrands(strands);
    // Populate ids.
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
    // Create segments and rows.
    ITERATE(TSegments, seg_it, m_Segs) {
        int len_width = 1;
        size_t str_idx = 0; // Strand index for the current row.
        ITERATE(SAlignment_Segment::TRows, row, seg_it->m_Rows) {
            TSeqPos start = row->GetSegStart();
            // Check if start needs to be converted to protein coords.
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
        // If there are any proteins, length should be adjusted.
        pseg.SetLens().push_back(seg_it->m_Len/len_width);
    }
}


// Create disc-alignment.
void CSeq_align_Mapper_Base::x_GetDstDisc(CRef<CSeq_align>& dst) const
{
    CSeq_align_set::Tdata& data = dst->SetSegs().SetDisc().Set();
    // Iterate sub-mappers, let each of them create a mapped alignment,
    // store results to the disc-align.
    ITERATE(TSubAligns, it, m_SubAligns) {
        try {
            data.push_back((*it)->GetDstAlign());
        }
        catch (CAnnotMapperException) {
            // Skip invalid sub-alignments.
        }
    }
}


// Creating exon parts - helper function to set part length
// depending on its type.
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


// Create and add a new exon part.
void CSeq_align_Mapper_Base::x_PushExonPart(
    CRef<CSpliced_exon_chunk>&    last_part,
    CSpliced_exon_chunk::E_Choice part_type,
    int                           part_len,
    CSpliced_exon&                exon) const
{
    if (last_part  &&  last_part->Which() == part_type) {
        // Merge parts of the same type.
        SetPartLength(*last_part, part_type,
            CSeq_loc_Mapper_Base::
            sx_GetExonPartLength(*last_part) + part_len);
    }
    else {
        // Add a new part.
        last_part.Reset(new CSpliced_exon_chunk);
        SetPartLength(*last_part, part_type, part_len);
        // Parts order does not depend on strands - preserve the original one.
        exon.SetParts().push_back(last_part);
    }
}


inline bool IsExonGap(CSpliced_exon_chunk::E_Choice chunk_type)
{
    return chunk_type == CSpliced_exon_chunk::e_Genomic_ins  ||
        chunk_type == CSpliced_exon_chunk::e_Product_ins;
}


// Create spliced-seg exon.
void CSeq_align_Mapper_Base::
x_GetDstExon(CSpliced_seg&              spliced,
             TSegments::const_iterator& seg,
             CSeq_id_Handle&            gen_id,
             CSeq_id_Handle&            prod_id,
             ENa_strand&                gen_strand,
             ENa_strand&                prod_strand,
             bool&                      last_exon_partial,
             const CSeq_id_Handle&      last_gen_id,
             const CSeq_id_Handle&      last_prod_id) const
{
    bool partial_left = false;
    bool partial_right = false;
    CRef<CSpliced_exon> exon(new CSpliced_exon);
    if (seg != m_Segs.begin()  &&  last_exon_partial) {
        // This is not the first segment, exon was split for some reason.
        // Mark it partial.
        exon->SetPartial(true);
        partial_left = true;
    }

    last_exon_partial = false;
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

    CRef<CSpliced_exon_chunk> last_part; // last exon part added
    int group_idx = -1;
    bool have_non_gaps = false; // are there any non-gap parts at all?
    // Continue iterating segments. Each segment becomes a new part.
    for ( ; seg != m_Segs.end(); ++seg) {
        if (group_idx != -1  &&  seg->m_GroupIdx != group_idx) {
            // New group found - start a new exon.
            partial_right = true;
            break;
        }
        // Remember the last segment's group.
        group_idx = seg->m_GroupIdx;

        const SAlignment_Segment::SAlignment_Row& gen_row =
            seg->m_Rows[CSeq_loc_Mapper_Base::eSplicedRow_Gen];
        const SAlignment_Segment::SAlignment_Row& prod_row =
            seg->m_Rows[CSeq_loc_Mapper_Base::eSplicedRow_Prod];
        // Spliced-seg can not have more than 2 rows.
        if (seg->m_Rows.size() > 2) {
            NCBI_THROW(CAnnotMapperException, eBadAlignment,
                    "Can not construct spliced-seg with more than two rows");
        }

        int gstart = gen_row.GetSegStart();
        int pstart = prod_row.GetSegStart();
        int gend = gstart + seg->m_Len;
        int pend = pstart + seg->m_Len;
        if (gstart >= 0) {
            // Not a genetic gap. Check the id.
            if (gen_id) {
                // If it's already set and the new segment has a different id,
                // fail.
                if (gen_id != gen_row.m_Id) {
                    NCBI_THROW(CAnnotMapperException, eBadAlignment,
                        "Can not construct spliced-seg -- "
                        "exon parts have different genomic seq-ids");
                }
            }
            else {
                // Genetic id not yet set. Remember it.
                gen_id = gen_row.m_Id;
                exon->SetGenomic_id(const_cast<CSeq_id&>(*gen_id.GetSeqId()));
            }
            _ASSERT(m_LocMapper.GetSeqTypeById(gen_id) !=
                CSeq_loc_Mapper_Base::eSeq_prot);
        }
        if (pstart >= 0) {
            // Not a product gap. Check the id.
            if (prod_id) {
                // Id already set, make sure the new one is the same.
                if (prod_id != prod_row.m_Id) {
                    NCBI_THROW(CAnnotMapperException, eBadAlignment,
                        "Can not construct spliced-seg -- "
                        "exon parts have different product seq-ids");
                }
            }
            else {
                // Product id not yet set.
                prod_id = prod_row.m_Id;
                exon->SetProduct_id(const_cast<CSeq_id&>(*prod_id.GetSeqId()));
            }
            if ( !spliced.IsSetProduct_type() ) {
                CSeq_loc_Mapper_Base::ESeqType prod_type =
                    m_LocMapper.GetSeqTypeById(prod_id);
                aln_protein = (prod_type == CSeq_loc_Mapper_Base::eSeq_prot);
                spliced.SetProduct_type(aln_protein ?
                    CSpliced_seg::eProduct_type_protein
                    : CSpliced_seg::eProduct_type_transcript);
            }
        }

        CSpliced_exon_chunk::E_Choice ptype = seg->m_PartType;

        // Check strands consistency
        bool gen_reverse = false;
        bool prod_reverse = false;
        // Check genomic strand if it's not a gap.
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
        }
        // Remember genomic strand.
        if ( gstrand_set ) {
            gen_reverse = IsReverse(gen_strand);
        }
        // Check product strand if it's not a gap.
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
        }
        // Remember product strand.
        if ( pstrand_set ) {
            prod_reverse = IsReverse(prod_strand);
        }

        int gins_len = 0;
        int pins_len = 0;

        if (pstart < 0) {
            // Gap on product
            if (gstart < 0) {
                // Both get and prod are missing - start new exon.
                last_exon_partial = true;
                exon->SetPartial(true);
                partial_right = true;
                seg++;
                break;
            }
            // Genomic is present.
            ptype = CSpliced_exon_chunk::e_Genomic_ins;
        }
        else {
            // Product is present.
            // Check parts order and intersection if the last part's coordinates
            // are known.
            if (prod_start >= 0  &&  prod_end > 0) {
                if (!prod_reverse) {
                    // Plus strand.
                    if (pstart < prod_end) {
                        // Intersection or bad order.
                        partial_right = true;
                        break;
                    }
                    if (pstart > prod_end) {
                        // Parts are not abutting, add insertion.
                        pins_len = pstart - prod_end;
                    }
                }
                else {
                    // Minus strand.
                    if (pend > prod_start) {
                        // Intersection or bad order.
                        partial_right = true;
                        break;
                    }
                    if (pend < prod_start) {
                        // Add insertion.
                        pins_len = prod_start - pend;
                    }
                }
            }
        }

        if (gstart < 0) {
            // Missing genomic sequence. Add product insertion.
            _ASSERT(pstart >= 0);
            ptype = CSpliced_exon_chunk::e_Product_ins;
        }
        else {
            // Genomic sequence is present.
            // Check parts order and intersection if the last part's coordinates
            // are known.
            if (gen_start >= 0  &&  gen_end > 0) {
                if (!gen_reverse) {
                    // Plus strand.
                    if (gstart < gen_end) {
                        // Intersection or bad order.
                        partial_right = true;
                        break;
                    }
                    if (gstart > gen_end) {
                        // Parts are not abutting, add insertion.
                        gins_len = gstart - gen_end;
                    }
                }
                else {
                    // Minus strand.
                    if (gend > gen_start) {
                        // Intersection or bad order.
                        partial_right = true;
                        break;
                    }
                    if (gend < gen_start) {
                        // Add insertion.
                        gins_len = gen_start - gend;
                    }
                }
            }
        }

        // Now when we know exon is not split, it's safe to update exon extremes.
        if (pstart >= 0) {
            if (prod_start < 0  ||  prod_start > pstart) {
                prod_start = pstart;
            }
            if (prod_end < pend) {
                prod_end = pend;
            }
        }
        if (gstart >= 0) {
            // Update last part's start and end.
            if (gen_start < 0  ||  gen_start > gstart) {
                gen_start = gstart;
            }
            if (gen_end < gend) {
                gen_end = gend;
            }
        }

        bool is_first = exon->GetParts().empty();
        // Add genomic or product insertions if any.
        if (gins_len > 0) {
            if ( !is_first ) {
                x_PushExonPart(last_part, CSpliced_exon_chunk::e_Genomic_ins,
                    gins_len, *exon);
            }
            else {
                // Ignore gaps at exon start.
                if ( !gen_reverse ) {
                    gen_start += gins_len;
                }
                else {
                    gen_end -= gins_len;
                }
            }
        }
        if (pins_len > 0) {
            if ( !is_first ) {
                x_PushExonPart(last_part, CSpliced_exon_chunk::e_Product_ins,
                    pins_len, *exon);
            }
            else {
                // Ignore gaps at exon start.
                if ( !prod_reverse ) {
                    prod_start += pins_len;
                }
                else {
                    prod_end -= pins_len;
                }
            }
        }

        // Remember if there are any non-gap parts.
        if ( !IsExonGap(ptype) ) {
            have_non_gaps = true;
        }

        // Add the mapped part except if it's a gap in the first position.
        if ( !is_first  ||  have_non_gaps) {
            x_PushExonPart(last_part, ptype, seg->m_Len, *exon);
        }
        else {
            if (ptype == CSpliced_exon_chunk::e_Genomic_ins) {
                if ( !gen_reverse ) {
                    gen_start += seg->m_Len;
                }
                else {
                    gen_end -= seg->m_Len;
                }
            }
            else if (ptype == CSpliced_exon_chunk::e_Product_ins) {
                if ( !prod_reverse ) {
                    prod_start += seg->m_Len;
                }
                else {
                    prod_end -= seg->m_Len;
                }
            }
        }
    }

    // The whole alignment becomes partial if any its exon is partial.
    if (!have_non_gaps  ||  exon->GetParts().empty()) {
        // No parts were inserted (or only gaps were found) - truncated exon.
        // Discard it completely.
        last_exon_partial = true;
        if (!spliced.GetExons().empty()) {
            // Mark previous exon partial
            CSpliced_exon& last_exon = *spliced.SetExons().back();
            last_exon.SetPartial(true);
            if (last_exon.IsSetGenomic_strand()  &&
                IsReverse(last_exon.GetGenomic_strand())) {
                // Minus strand - reset acceptor of the last exon
                last_exon.ResetAcceptor_before_exon();
            }
            else {
                last_exon.ResetDonor_after_exon();
            }
        }
        return;
    }

    // Make sure the last part is not a gap.
    while ( IsExonGap(exon->GetParts().back()->Which()) ) {
        const CSpliced_exon_chunk& chunk = *exon->GetParts().back();
        if ( chunk.IsGenomic_ins() ) {
            if ( !IsReverse(gen_strand) ) {
                gen_end -= chunk.GetGenomic_ins();
            }
            else {
                gen_start += chunk.GetGenomic_ins();
            }
        }
        else if ( chunk.IsProduct_ins() ) {
            if ( !IsReverse(prod_strand) ) {
                prod_end -= chunk.GetProduct_ins();
            }
            else {
                prod_start += chunk.GetProduct_ins();
            }
        }
        exon->SetParts().pop_back();
    }

    if ( IsReverse(gen_strand) ) {
        if ( !partial_right  &&  m_OrigExon->IsSetAcceptor_before_exon() ) {
            exon->SetAcceptor_before_exon().Assign(
                m_OrigExon->GetAcceptor_before_exon());
        }
        if ( !partial_left  &&  m_OrigExon->IsSetDonor_after_exon() ) {
            exon->SetDonor_after_exon().Assign(
                m_OrigExon->GetDonor_after_exon());
        }
    }
    else {
        if ( !partial_left  &&  m_OrigExon->IsSetAcceptor_before_exon() ) {
            exon->SetAcceptor_before_exon().Assign(
                m_OrigExon->GetAcceptor_before_exon());
        }
        if ( !partial_right  &&  m_OrigExon->IsSetDonor_after_exon() ) {
            exon->SetDonor_after_exon().Assign(
                m_OrigExon->GetDonor_after_exon());
        }
    }

    // If some id was not found in this exon, use the last known one.
    if (!gen_id  &&  last_gen_id) {
        gen_id = last_gen_id;
        exon->SetGenomic_id(const_cast<CSeq_id&>(*gen_id.GetSeqId()));
    }
    if (!prod_id  &&  last_prod_id) {
        prod_id = last_prod_id;
        exon->SetProduct_id(const_cast<CSeq_id&>(*prod_id.GetSeqId()));
    }
    // Set the whole exon's coordinates.
    exon->SetGenomic_start(gen_start);
    exon->SetGenomic_end(gen_end - 1);
    if (gen_strand != eNa_strand_unknown) {
        exon->SetGenomic_strand(gen_strand);
    }
    if ( aln_protein ) {
        // For proteins adjust coords and set frames.
        exon->SetProduct_start().SetProtpos().SetAmin(prod_start/3);
        exon->SetProduct_start().SetProtpos().SetFrame(prod_start%3 + 1);
        exon->SetProduct_end().SetProtpos().SetAmin((prod_end - 1)/3);
        exon->SetProduct_end().SetProtpos().SetFrame((prod_end - 1)%3 + 1);
    }
    else {
        exon->SetProduct_start().SetNucpos(prod_start);
        exon->SetProduct_end().SetNucpos(prod_end - 1);
        if (prod_strand != eNa_strand_unknown) {
            exon->SetProduct_strand(prod_strand);
        }
    }
    // Scores should be copied from the original exon.
    // If the mapping was partial, the scores should have been invalidated
    // and cleared.
    if ( !m_SegsScores.empty() ) {
        CloneContainer<CScore, TScores, CScore_set::Tdata>(
            m_SegsScores, exon->SetScores().Set());
    }
    // Copy ext from the original exon.
    if ( m_OrigExon->IsSetExt() ) {
        CloneContainer<CUser_object, CSpliced_exon::TExt, CSpliced_exon::TExt>(
            m_OrigExon->GetExt(), exon->SetExt());
    }
    // Add the new exon to the spliced-seg.
    spliced.SetExons().push_back(exon);
}


// Create spliced-seg.
void CSeq_align_Mapper_Base::x_GetDstSpliced(CRef<CSeq_align>& dst) const
{
    CSpliced_seg& spliced = dst->SetSegs().SetSpliced();
    CSeq_id_Handle gen_id;  // per-alignment genomic id
    CSeq_id_Handle prod_id; // per-alignment product id
    CSeq_id_Handle last_gen_id;  // last exon's genomic id
    CSeq_id_Handle last_prod_id; // last exon's product id
    ENa_strand gen_strand = eNa_strand_unknown;
    ENa_strand prod_strand = eNa_strand_unknown;
    bool single_gen_id = true;
    bool single_gen_str = true;
    bool single_prod_id = true;
    bool single_prod_str = true;
    bool partial = false;
    bool last_exon_partial = false;

    ITERATE(TSubAligns, it, m_SubAligns) {
        TSegments::const_iterator seg = (*it)->m_Segs.begin();
        // Convert the current sub-mapper to an exon.
        // In some cases the exon can be split (e.g. if a gap is found in
        // both rows). In this case 'seg' iterator will not be set to
        // m_Segs.end() by x_GetDstExon and the next iteration will be
        // performed.
        while (seg != (*it)->m_Segs.end()) {
            CSeq_id_Handle ex_gen_id;
            CSeq_id_Handle ex_prod_id;
            ENa_strand ex_gen_strand = eNa_strand_unknown;
            ENa_strand ex_prod_strand = eNa_strand_unknown;
            (*it)->x_GetDstExon(spliced, seg, ex_gen_id, ex_prod_id,
                ex_gen_strand, ex_prod_strand, last_exon_partial,
                last_gen_id, last_prod_id);
            partial = partial || last_exon_partial;
            // Check if all exons have the same ids in genomic and product
            // rows.
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
            // Check if all exons have the same strands.
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

    // Try to propagate some properties to the alignment level.
    if ( !gen_id ) {
        // Don't try to use genomic id if not set
        single_gen_id = false;
    }
    if ( !prod_id ) {
        // Don't try to use product id if not set
        single_prod_id = false;
    }
    if ( single_gen_id ) {
        spliced.SetGenomic_id(const_cast<CSeq_id&>(*gen_id.GetSeqId()));
    }
    if (single_gen_str  &&  gen_strand != eNa_strand_unknown) {
        spliced.SetGenomic_strand(gen_strand);
    }
    if ( single_prod_id ) {
        spliced.SetProduct_id(const_cast<CSeq_id&>(*prod_id.GetSeqId()));
    }
    if (single_prod_str  &&  prod_strand != eNa_strand_unknown) {
        spliced.SetProduct_strand(prod_strand);
    }
    // Update bounds if defined in the original alignment.
    if (single_prod_id  &&  single_gen_id  &&  m_OrigAlign->IsSetBounds()  &&
        m_LocMapper.m_Mappings) {
        CSeq_align::TBounds& bounds = dst->SetBounds();
        bounds.clear();
        ITERATE(CSeq_align::TBounds, it, m_OrigAlign->GetBounds()) {
            CRef<CSeq_loc> mapped_it = m_LocMapper.Map(**it);
            _ASSERT(mapped_it);
            if ( mapped_it->IsNull() ) {
                // Could not map the location
                mapped_it->Assign(**it);
            }
            bounds.push_back(mapped_it);
        }
    }

    // Reset local values where possible if the global ones are set.
    // Fill ids in gaps.
    NON_CONST_ITERATE(CSpliced_seg::TExons, it, spliced.SetExons()) {
        if ( single_gen_id ) {
            (*it)->ResetGenomic_id();
        }
        else if ( gen_id  &&  !(*it)->IsSetGenomic_id() ) {
            // Use the first known genomic id to fill gaps.
            (*it)->SetGenomic_id(const_cast<CSeq_id&>(*gen_id.GetSeqId()));
        }
        if ( single_prod_id ) {
            (*it)->ResetProduct_id();
        }
        else if ( prod_id  &&  !(*it)->IsSetProduct_id() ) {
            // Use the first known product id to fill gaps.
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
    // Copy some values from the original alignment.
    if ( orig.IsSetPoly_a() ) {
        spliced.SetPoly_a(orig.GetPoly_a());
    }
    if ( orig.IsSetProduct_length() ) {
        spliced.SetProduct_length(orig.GetProduct_length());
    }
    // Some properties can be copied only if the alignment was not
    // truncated.
    if (!partial  &&  orig.IsSetModifiers()) {
        CloneContainer<CSpliced_seg_modifier,
            CSpliced_seg::TModifiers, CSpliced_seg::TModifiers>(
            orig.GetModifiers(), spliced.SetModifiers());
    }
}


// Create sparse-seg alignment.
void CSeq_align_Mapper_Base::x_GetDstSparse(CRef<CSeq_align>& dst) const
{
    CSparse_seg& sparse = dst->SetSegs().SetSparse();
    if ( !m_SegsScores.empty() ) {
        // Copy scores (each element, not just pointers).
        CloneContainer<CScore, TScores, CSparse_seg::TRow_scores>(
            m_SegsScores, sparse.SetRow_scores());
    }
    CRef<CSparse_align> aln(new CSparse_align);
    sparse.SetRows().push_back(aln);
    aln->SetNumseg(m_Segs.size());

    CSeq_id_Handle first_idh;
    CSeq_id_Handle second_idh;
    size_t s = 0;
    // Check if all segments are related to the same group of scores.
    // Need two special values: -2 indicates that the scores group is
    // not yet set; -1 is used if there are segments with different
    // groups and scores should not be copied from the original align.
    int scores_group = -2; // -2 -- not yet set; -1 -- already reset.
    ITERATE(TSegments, seg, m_Segs) {
        if (seg->m_Rows.size() > 2) {
            NCBI_THROW(CAnnotMapperException, eBadAlignment,
                    "Can not construct sparse-seg with more than two ids");
        }
        const SAlignment_Segment::SAlignment_Row& first_row = seg->m_Rows[0];
        const SAlignment_Segment::SAlignment_Row& second_row = seg->m_Rows[1];

        // Skip gaps.
        int first_start = first_row.GetSegStart();
        int second_start = second_row.GetSegStart();
        if (first_start < 0  ||  second_start < 0) {
            continue; // gap in one row
        }

        // All segments must have the same seq-id.
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
        // Check sequence types, adjust coordinates.
        bool first_prot = m_LocMapper.GetSeqTypeById(first_idh) ==
            CSeq_loc_Mapper_Base::eSeq_prot;
        bool second_prot = m_LocMapper.GetSeqTypeById(second_idh) ==
            CSeq_loc_Mapper_Base::eSeq_prot;
        int first_width = first_prot ? 3 : 1;
        int second_width = second_prot ? 3 : 1;
        // If at least one row is on a protein, lengths should be
        // in AAs, not bases.
        int len_width = (first_prot  ||  second_prot) ? 3 : 1;

        aln->SetFirst_starts().push_back(first_start/first_width);
        aln->SetSecond_starts().push_back(second_start/second_width);
        aln->SetLens().push_back(seg->m_Len/len_width);

        // Set strands.
        if (aln->IsSetSecond_strands()  ||
            first_row.m_IsSetStrand  ||  second_row.m_IsSetStrand) {
            // Add missing strands to the container if necessary.
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

        // Check scores for consistency.
        if (scores_group == -2) { // not yet set
            scores_group = seg->m_ScoresGroupIdx;
        }
        else if (scores_group != seg->m_ScoresGroupIdx) {
            scores_group = -1; // reset
        }
    }
    // Copy scores if possible. All segments must be assigned to the same
    // group of scores.
    if (scores_group >= 0) {
        CloneContainer<CScore, TScores, CSparse_align::TSeg_scores>(
            m_GroupScores[scores_group], aln->SetSeg_scores());
    }
}


// When the mapped alignment can not be stored using the original
// alignment type (e.g. most types do not allow multiple ids per row),
// the whole mapped alignment is converted to a disc-align containing
// several dense-segs. The following method attempts to put as many
// mapped segments as possible to the dense-seg sub-alignment.
int CSeq_align_Mapper_Base::x_GetPartialDenseg(CRef<CSeq_align>& dst,
                                               int start_seg) const
{
    CDense_seg& dseg = dst->SetSegs().SetDenseg();
    dst->SetType(CSeq_align::eType_partial);
    dseg.SetDim(m_Segs.front().m_Rows.size());

    int len_width = 1;

    // First, find the requested segment. Since TSegments is a list, we
    // have to iterate over it and skip 'start_seg' items.
    TSegments::const_iterator start_seg_it = m_Segs.begin();
    for (int s = 0; s < start_seg && start_seg_it != m_Segs.end();
        s++, start_seg_it++) {
    }
    if (start_seg_it == m_Segs.end()) {
        return -1; // The requested segment does not exist.
    }
    const SAlignment_Segment& start_segment = *start_seg_it;
    // Remember number of rows in the first segment. Break the dense-seg
    // when the next segment has a different number of rows.
    size_t num_rows = start_segment.m_Rows.size();
    int last_seg = m_Segs.size() - 1;

    // Find first non-gap in each row, get its seq-id, detect the first
    // one which is different. Also stop if number or rows per segment
    // changes. Collect all seq-ids.
    vector<CSeq_id_Handle> ids;
    TStrands strands(num_rows, eNa_strand_unknown);
    ids.resize(num_rows);
    for (size_t r = 0; r < num_rows; r++) {
        CSeq_id_Handle last_id;
        TSegments::const_iterator seg_it = start_seg_it;
        int seg_idx = start_seg;
        int left = -1;
        int right = -1;
        for ( ; seg_idx <= last_seg  &&  seg_it != m_Segs.end();
            seg_idx++, seg_it++) {
            // Check number of rows.
            if (seg_it->m_Rows.size() != num_rows) {
                // Adjust the last segment index.
                last_seg = seg_idx - 1;
                break;
            }
            const SAlignment_Segment::SAlignment_Row& row = seg_it->m_Rows[r];
            // Check ids.
            if (last_id  &&  last_id != row.m_Id) {
                last_seg = seg_idx - 1;
                break;
            }
            if ( !last_id ) {
                last_id = row.m_Id;
                ids[r] = row.m_Id;
            }
            // Check strands and overlaps for non-gaps
            int seg_start = row.GetSegStart();
            int seg_stop = seg_start == -1 ? -1 : seg_start + seg_it->m_Len;
            if (seg_start != -1) {
                // Check strands
                if (strands[r] == eNa_strand_unknown) {
                    if ( row.m_IsSetStrand ) {
                        strands[r] = row.m_Strand;
                    }
                }
                else {
                    if ( !SameOrientation(strands[r], row.m_Strand) ) {
                        last_seg = seg_idx - 1;
                        break;
                    }
                }
                // Check overlaps
                if (left == -1) {
                    left = seg_start;
                    right = seg_stop;
                }
                else {
                    if (row.m_IsSetStrand  &&  IsReverse(row.m_Strand)) {
                        if (seg_stop > left) {
                            last_seg = seg_idx - 1;
                            break;
                        }
                        left = seg_start;
                    }
                    else {
                        if (seg_start < right) {
                            last_seg = seg_idx - 1;
                            break;
                        }
                        right = seg_stop;
                    }
                }
            }
        }
    }
    // At lease one segment may be used.
    _ASSERT(last_seg >= start_seg);

    // Now when number of rows is known, fill the ids.
    for (size_t i = 0; i < num_rows; i++) {
        CRef<CSeq_id> id(new CSeq_id);
        id->Assign(*ids[i].GetSeqId());
        dseg.SetIds().push_back(id);
        // Check sequence type and adjust length width.
        CSeq_loc_Mapper_Base::ESeqType seq_type =
            m_LocMapper.GetSeqTypeById(ids[i]);
        if (seq_type == CSeq_loc_Mapper_Base::eSeq_prot) {
            len_width = 3;
        }
    }

    // Detect strands for all rows, they will be used for gaps.
    x_FillKnownStrands(strands);
    // Count number of non-gap segments in each row.
    // If a row has only gaps, the whole sub-alignment should be
    // discarded.
    vector<size_t> segs_per_row(num_rows, 0);
    // Count total number of segments added to the alignment
    // where at least one row is non-gap.
    int non_empty_segs = 0;
    int cur_seg = start_seg;
    for (TSegments::const_iterator it = start_seg_it; it != m_Segs.end();
        ++it, ++cur_seg) {
        if (cur_seg > last_seg) {
            break;
        }
        // Check if at least one row in the current segment is non-gap.
        bool only_gaps = true;
        for (size_t row = 0; row < it->m_Rows.size(); row++) {
            if (it->m_Rows[row].m_Start != kInvalidSeqPos) {
                segs_per_row[row]++;
                only_gaps = false;
            }
        }
        if (only_gaps) continue; // ignore empty rows

        // Set segment length.
        dseg.SetLens().push_back(it->m_Len/len_width);

        size_t str_idx = 0;
        non_empty_segs++; // count segments added to the dense-seg
        // Now iterate all rows and add them to the dense-seg.
        ITERATE(SAlignment_Segment::TRows, row, it->m_Rows) {
            int width = 1;
            // Don't check sequence type if there are no proteins in the
            // used segments (len_width == 1).
            if (len_width == 3  &&  m_LocMapper.GetSeqTypeById(row->m_Id) ==
                CSeq_loc_Mapper_Base::eSeq_prot) {
                width = 3;
            }
            int start = row->GetSegStart();
            if (start >= 0) {
                start /= width;
            }
            dseg.SetStarts().push_back(start);
            if (m_HaveStrands) { // Are per-alignment strands set?
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
    else {
        ITERATE(vector<size_t>, row, segs_per_row) {
            if (*row == 0) {
                // The row contains only gaps. Discard the sub-alignment.
                dst.Reset();
                break;
            }
        }
    }
    if ( dst ) {
        dseg.SetNumseg(non_empty_segs);
    }
    return last_seg + 1;
}


// If the original alignment type does not support some features of
// the mapped alignment (multi-id rows, segments with different number
// of rows etc.), convert it to disc-align with multiple dense-segs.
void CSeq_align_Mapper_Base::x_ConvToDstDisc(CRef<CSeq_align>& dst) const
{
    // Ignore m_SegsScores -- if we are here, they are probably not valid.
    // Anyway, there's no place to put them in. The same about m_AlignScores.
    CSeq_align_set::Tdata& data = dst->SetSegs().SetDisc().Set();
    int seg = 0;
    // The iteration stops when the last segment is converted or
    // when an error occurs and x_GetPartialDenseg returns -1.
    while (seg >= 0  &&  size_t(seg) < m_Segs.size()) {
        // Convert as many segments as possible to a single dense-seg.
        CRef<CSeq_align> dseg(new CSeq_align);
        seg = x_GetPartialDenseg(dseg, seg);
        if (!dseg) continue; // The sub-align had only gaps
        data.push_back(dseg);
    }
}


// Check if the mapped alignment contains different sequence types.
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


    // Check if each row contains only one strand.
bool CSeq_align_Mapper_Base::x_HaveMixedStrand(void) const
{
    if ( m_Segs.empty() ) {
        return false;
    }
    vector<ENa_strand> strands(m_Segs.front().m_Rows.size(), eNa_strand_unknown);
    ITERATE(TSegments, seg, m_Segs) {
        for (size_t r = 0; r < seg->m_Rows.size(); ++r) {
            if (r >= strands.size()) {
                strands.resize(r, eNa_strand_unknown);
            }
            const SAlignment_Segment::SAlignment_Row& row = seg->m_Rows[r];
            // Skip gaps - they may have wrong strands.
            if (row.GetSegStart() == -1) {
                continue;
            }
            if (strands[r] == eNa_strand_unknown) {
                if ( row.m_IsSetStrand ) {
                    strands[r] = row.m_Strand;
                }
            }
            else {
                if ( !SameOrientation(strands[r], row.m_Strand) ) {
                    return true;
                }
            }
        }
    }
    return false;
}


bool CSeq_align_Mapper_Base::x_IsEmpty(void) const
{
    if ( !m_Segs.empty() ) {
        // Check if there's at least one segment with at least two rows.
        ITERATE(TSegments, seg, m_Segs) {
            if (seg->m_Rows.size() < 2) continue;
            int non_empty = 0;
            ITERATE(SAlignment_Segment::TRows, row, seg->m_Rows) {
                if (row->m_Start == kInvalidSeqPos) continue;
                if (++non_empty >= 2) {
                    return false; // Found non-empty segment.
                }
            }
        }
    }
    // No non-empty segments. Check sub-mappers.
    ITERATE(TSubAligns, sub, m_SubAligns) {
        if ( !(*sub)->x_IsEmpty() ) return false;
    }
    // No non-empty segments or sub-mappers.
    return true;
}


// Get mapped alignment. In most cases the mapper tries to
// preserve the original alignment type and copy as much
// information as possible (scores, bounds etc.).
CRef<CSeq_align> CSeq_align_Mapper_Base::GetDstAlign(void) const
{
    if (m_DstAlign) {
        // The mapped alignment has been created, just use it.
        return m_DstAlign;
    }

    if ( x_IsEmpty() ) {
        NCBI_THROW(CAnnotMapperException, eBadAlignment,
            "Mapping resulted in an empty alignment, "
            "can not initialize Seq-align.");
    }

    // Find first non-gap in each row, get its seq-id.
    TSegments::iterator seg = m_Segs.begin();
    vector<CSeq_id_Handle> row_ids;
    for ( ; seg != m_Segs.end(); ++seg) {
        if (row_ids.size() < seg->m_Rows.size()) {
            row_ids.resize(seg->m_Rows.size());
        }
        for (size_t r = 0; r < seg->m_Rows.size(); r++) {
            SAlignment_Segment::SAlignment_Row& row = seg->m_Rows[r];
            if (row.m_Start != kInvalidSeqPos) {
                // Remember seq-id used in the last non-gap segment
                row_ids[r] = row.m_Id;
                continue;
            }
            // Check if an id for this row is known
            if ( !row_ids[r] ) {
                // Try to look forward - find non-gap
                TSegments::iterator fwd = seg;
                ++fwd;
                for ( ; fwd != m_Segs.end(); ++fwd) {
                    if (fwd->m_Rows.size() <= r) continue;
                    SAlignment_Segment::SAlignment_Row& fwd_row = fwd->m_Rows[r];
                    if (fwd_row.m_Start != kInvalidSeqPos) {
                        row_ids[r] = fwd_row.m_Id;
                        break;
                    }
                }
            }
            if ( row_ids[r] ) {
                row.m_Id = row_ids[r];
            }
        }
    }

    CSeq_align::TSegs::E_Choice orig_choice = m_OrigAlign->GetSegs().Which();

    CRef<CSeq_align> dst(new CSeq_align);
    // Copy some information from the original alignment.
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
        // Only std and spliced can support mixed sequence types.
        // Since spliced-segs are mapped in a different way (through
        // sub-mappers which return mapped exons rather than whole alignments),
        // here we should always use std-seg.
        x_GetDstStd(dst);
    }
    /*
    // Commented out as it looks to be wrong approach - it discards scores and
    // changes seq-align type.

    // Even with mixed strand, do not convert std-segs - they can hold mixed
    // strands without any problems.
    else if (x_HaveMixedStrand()  &&  orig_choice != CSeq_align::TSegs::e_Std) {
        x_ConvToDstDisc(dst);
    }

    */
    else {
        // Get the proper mapped alignment. Some types still may need
        // to be converted to disc-seg.
        switch ( orig_choice ) {
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
                // Unknown original type, just copy the original alignment.
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
    // Create a sub-mapper instance for the given sub-alignment.
    return new CSeq_align_Mapper_Base(align, m_LocMapper);
}


CSeq_align_Mapper_Base*
CSeq_align_Mapper_Base::CreateSubAlign(const CSpliced_seg&  spliced,
                                       const CSpliced_exon& exon)
{
    // Create a sub-mapper instance for the exon.
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
    // Reset all scores which are related to the segment including
    // all higher-level scores. This is done when a segment is truncated
    // and scores become invalid.
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

