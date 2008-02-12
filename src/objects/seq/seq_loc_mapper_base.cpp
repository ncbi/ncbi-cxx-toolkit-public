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
*   Seq-loc mapper base
*
*/

#include <ncbi_pch.hpp>
#include <objects/seq/seq_loc_mapper_base.hpp>
#include <objects/seq/seq_align_mapper_base.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seqloc/seqloc__.hpp>
#include <objects/seqalign/seqalign__.hpp>
#include <objects/error_codes.hpp>
#include <algorithm>


#define NCBI_USE_ERRCODE_X   Objects_SeqLocMap


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


const char* CAnnotMapperException::GetErrCodeString(void) const
{
    switch ( GetErrCode() ) {
    case eBadLocation:      return "eBadLocation";
    case eUnknownLength:    return "eUnknownLength";
    case eBadAlignment:     return "eBadAlignment";
    case eOtherError:       return "eOtherError";
    default:                return CException::GetErrCodeString();
    }
}


/////////////////////////////////////////////////////////////////////
//
// CMappingRange
//
//   Helper class for mapping points, ranges, strands and fuzzes
//


CMappingRange::CMappingRange(CSeq_id_Handle     src_id,
                             TSeqPos            src_from,
                             TSeqPos            src_length,
                             ENa_strand         src_strand,
                             CSeq_id_Handle     dst_id,
                             TSeqPos            dst_from,
                             ENa_strand         dst_strand,
                             bool               ext_to)
    : m_Src_id_Handle(src_id),
      m_Src_from(src_from),
      m_Src_to(src_from + src_length - 1),
      m_Src_strand(src_strand),
      m_Dst_id_Handle(dst_id),
      m_Dst_from(dst_from),
      m_Dst_strand(dst_strand),
      m_Reverse(!SameOrientation(src_strand, dst_strand)),
      m_ExtTo(ext_to)
{
    return;
}


bool CMappingRange::CanMap(TSeqPos    from,
                           TSeqPos    to,
                           bool       is_set_strand,
                           ENa_strand strand) const
{
    if ( is_set_strand  &&  (IsReverse(strand) != IsReverse(m_Src_strand)) ) {
        return false;
    }
    return from <= m_Src_to  &&  to >= m_Src_from;
}


TSeqPos CMappingRange::Map_Pos(TSeqPos pos) const
{
    _ASSERT(pos >= m_Src_from  &&  pos <= m_Src_to);
    if (!m_Reverse) {
        return m_Dst_from + pos - m_Src_from;
    }
    else {
        return m_Dst_from + m_Src_to - pos;
    }
}


CMappingRange::TRange CMappingRange::Map_Range(TSeqPos           from,
                                               TSeqPos           to,
                                               const TRangeFuzz* fuzz) const
{
    // Special case of mapping from a protein to a nucleotide through
    // a partial cd-region. Extend the mapped interval to the end of
    // destination range if all of the following conditions are true:
    // - source is a protein (m_ExtTo)
    // - destination is a nucleotide (m_ExtTo)
    // - destination interval has partial "to" (m_ExtTo)
    // - interval to be mapped has partial "to"
    // - destination range is 1 or 2 bases beyond the end of the source range
    if ( m_ExtTo ) {
        bool partial_to = false;
        if (!m_Reverse) {
            partial_to = fuzz  &&  fuzz->second  &&  fuzz->second->IsLim()  &&
                fuzz->second->GetLim() == CInt_fuzz::eLim_gt;
        }
        else {
            partial_to = fuzz  &&  fuzz->first  &&  fuzz->first->IsLim()  &&
                fuzz->first->GetLim() == CInt_fuzz::eLim_lt;
        }
        if (to < m_Src_to  &&  m_Src_to - to < 3) {
            to = m_Src_to;
        }
    }
    if (!m_Reverse) {
        return TRange(Map_Pos(max(from, m_Src_from)),
            Map_Pos(min(to, m_Src_to)));
    }
    else {
        return TRange(Map_Pos(min(to, m_Src_to)),
            Map_Pos(max(from, m_Src_from)));
    }
}


bool CMappingRange::Map_Strand(bool        is_set_strand,
                               ENa_strand  src,
                               ENa_strand* dst) const
{
    _ASSERT(dst);
    if ( m_Reverse ) {
        // Always convert to reverse strand
        *dst = Reverse(src);
        return true;
    }
    if (is_set_strand) {
        // Use original strand if set
        *dst = src;
        return true;
    }
    if (m_Dst_strand != eNa_strand_unknown) {
        // Destination strand may be set for nucleotides
        // even if the source one is not set.
        *dst = m_Dst_strand;
        return true;
    }
    return false;
}


const CMappingRange::TFuzz kEmptyFuzz(0);

CInt_fuzz::ELim CMappingRange::x_ReverseFuzzLim(CInt_fuzz::ELim lim) const
{
    switch ( lim ) {
    case CInt_fuzz::eLim_gt:
        return CInt_fuzz::eLim_lt;
    case CInt_fuzz::eLim_lt:
        return CInt_fuzz::eLim_gt;
    case CInt_fuzz::eLim_tr:
        return CInt_fuzz::eLim_tl;
    case CInt_fuzz::eLim_tl:
        return CInt_fuzz::eLim_tr;
    default:
        return lim;
    }
}


CMappingRange::TRangeFuzz CMappingRange::Map_Fuzz(const TRangeFuzz& fuzz) const
{
    TRangeFuzz res = fuzz;
    // Maps some fuzz types to reverse strand
    if ( !m_Reverse ) {
        return res;
    }
    // Swap ends
    res = TRangeFuzz(fuzz.second, fuzz.first);
    if ( res.first ) {
        switch ( res.first->Which() ) {
        case CInt_fuzz::e_Lim:
            {
                res.first->SetLim(x_ReverseFuzzLim(res.first->GetLim()));
                break;
            }
        default:
            // Other types are not converted
            break;
        }
    }
    if ( res.second ) {
        switch ( res.second->Which() ) {
        case CInt_fuzz::e_Lim:
            {
                res.second->SetLim(x_ReverseFuzzLim(res.second->GetLim()));
                break;
            }
        default:
            // Other types are not converted
            break;
        }
    }
    return res;
}


/////////////////////////////////////////////////////////////////////
//
// CMappingRanges
//
//   Collection of mapping ranges


CMappingRanges::CMappingRanges(void)
    : m_ReverseSrc(false),
      m_ReverseDst(false)
{
}


void CMappingRanges::AddConversion(CRef<CMappingRange> cvt)
{
    m_IdMap[cvt->m_Src_id_Handle].insert(TRangeMap::value_type(
        TRange(cvt->m_Src_from, cvt->m_Src_to), cvt));
}


void CMappingRanges::AddConversion(CSeq_id_Handle    src_id,
                                   TSeqPos           src_from,
                                   TSeqPos           src_length,
                                   ENa_strand        src_strand,
                                   CSeq_id_Handle    dst_id,
                                   TSeqPos           dst_from,
                                   ENa_strand        dst_strand,
                                   bool              ext_to)
{
    CRef<CMappingRange> cvt(new CMappingRange(
        src_id, src_from, src_length, src_strand,
        dst_id, dst_from, dst_strand,
        ext_to));
    AddConversion(cvt);
}

    
CMappingRanges::TRangeIterator
CMappingRanges::BeginMappingRanges(CSeq_id_Handle id,
                                   TSeqPos        from,
                                   TSeqPos        to) const
{
    TIdMap::const_iterator ranges = m_IdMap.find(id);
    if (ranges == m_IdMap.end()) {
        return TRangeIterator();
    }
    return ranges->second.begin(TRange(from, to));
}


/////////////////////////////////////////////////////////////////////
//
// CSeq_loc_Mapper_Base
//


/////////////////////////////////////////////////////////////////////
//
//   Initialization of the mapper
//


inline
ENa_strand s_IndexToStrand(size_t idx)
{
    _ASSERT(idx != 0);
    return ENa_strand(idx - 1);
}

#define STRAND_TO_INDEX(is_set, strand) \
    ((is_set) ? size_t((strand) + 1) : 0)

#define INDEX_TO_STRAND(idx) \
    s_IndexToStrand(idx)


CSeq_loc_Mapper_Base::CSeq_loc_Mapper_Base(void)
    : m_MergeFlag(eMergeNone),
      m_GapFlag(eGapPreserve),
      m_KeepNonmapping(false),
      m_CheckStrand(false),
      m_UseWidth(false),
      m_Dst_width(0),
      m_Partial(false),
      m_LastTruncated(false),
      m_Mappings(new CMappingRanges)
{
}


CSeq_loc_Mapper_Base::CSeq_loc_Mapper_Base(CMappingRanges* mapping_ranges)
    : m_MergeFlag(eMergeNone),
      m_GapFlag(eGapPreserve),
      m_KeepNonmapping(false),
      m_CheckStrand(false),
      m_UseWidth(false),
      m_Dst_width(1),
      m_Partial(false),
      m_LastTruncated(false),
      m_Mappings(mapping_ranges)
{
}


CSeq_loc_Mapper_Base::CSeq_loc_Mapper_Base(const CSeq_feat&  map_feat,
                                           EFeatMapDirection dir)
    : m_MergeFlag(eMergeNone),
      m_GapFlag(eGapPreserve),
      m_KeepNonmapping(false),
      m_CheckStrand(false),
      m_UseWidth(false),
      m_Dst_width(0),
      m_Partial(false),
      m_LastTruncated(false),
      m_Mappings(new CMappingRanges)
{
    x_InitializeFeat(map_feat, dir);
}


CSeq_loc_Mapper_Base::CSeq_loc_Mapper_Base(const CSeq_loc& source,
                                           const CSeq_loc& target)
    : m_MergeFlag(eMergeNone),
      m_GapFlag(eGapPreserve),
      m_KeepNonmapping(false),
      m_CheckStrand(false),
      m_UseWidth(false),
      m_Dst_width(0),
      m_Partial(false),
      m_LastTruncated(false),
      m_Mappings(new CMappingRanges)
{
    x_InitializeLocs(source, target);
}


CSeq_loc_Mapper_Base::CSeq_loc_Mapper_Base(const CSeq_align& map_align,
                                           const CSeq_id&    to_id,
                                           TMapOptions       opts)
    : m_MergeFlag(eMergeNone),
      m_GapFlag(eGapPreserve),
      m_KeepNonmapping(false),
      m_CheckStrand(false),
      m_UseWidth(false),
      m_Dst_width(0),
      m_Partial(false),
      m_LastTruncated(false),
      m_Mappings(new CMappingRanges)
{
    x_InitializeAlign(map_align, to_id, opts);
}


CSeq_loc_Mapper_Base::CSeq_loc_Mapper_Base(const CSeq_align& map_align,
                                           size_t            to_row,
                                           TMapOptions       opts)
    : m_MergeFlag(eMergeNone),
      m_GapFlag(eGapPreserve),
      m_KeepNonmapping(false),
      m_CheckStrand(false),
      m_UseWidth(false),
      m_Dst_width(0),
      m_Partial(false),
      m_LastTruncated(false),
      m_Mappings(new CMappingRanges)
{
    x_InitializeAlign(map_align, to_row, opts);
}


CSeq_loc_Mapper_Base::~CSeq_loc_Mapper_Base(void)
{
    return;
}


void CSeq_loc_Mapper_Base::x_InitializeFeat(const CSeq_feat&  map_feat,
                                            EFeatMapDirection dir)
{
    _ASSERT(map_feat.IsSetProduct());
    int frame = 0;
    if (map_feat.GetData().IsCdregion()) {
        frame = map_feat.GetData().GetCdregion().GetFrame();
    }
    if (dir == eLocationToProduct) {
        x_InitializeLocs(map_feat.GetLocation(), map_feat.GetProduct(), frame);
    }
    else {
        x_InitializeLocs(map_feat.GetProduct(), map_feat.GetLocation(), frame);
    }
}


void CSeq_loc_Mapper_Base::x_InitializeLocs(const CSeq_loc& source,
                                            const CSeq_loc& target,
                                            int             frame)
{
    // Check sequence type or total location length to
    // adjust intervals according to character width.
    TSeqPos dst_total_len;
    x_CheckSeqWidth(target, &dst_total_len);
    TSeqPos src_total_len;
    int src_width = x_CheckSeqWidth(source, &src_total_len);

    if (!src_width  ||  !m_Dst_width) {
        // Try to detect types by lengths
        if (src_total_len == kInvalidSeqPos || dst_total_len== kInvalidSeqPos) {
            NCBI_THROW(CAnnotMapperException, eBadLocation,
                       "Undefined location length -- "
                       "unable to detect sequence type");
        }
        if (src_total_len == dst_total_len) {
            src_width = 1;
            m_Dst_width = 1;
            _ASSERT(!frame);
        }
        // truncate incomplete left and right codons
        else if (src_total_len/3 == dst_total_len) {
            src_width = 3;
            m_Dst_width = 1;
        }
        else if (dst_total_len/3 == src_total_len) {
            src_width = 1;
            m_Dst_width = 3;
        }
        else {
            NCBI_THROW(CAnnotMapperException, eBadLocation,
                       "Wrong location length -- "
                       "unable to detect sequence type");
        }
    }
    else {
        if (src_width == m_Dst_width) {
            src_width = 1;
            m_Dst_width = 1;
            _ASSERT(!frame);
        }
    }
    m_UseWidth |= (src_width != m_Dst_width);

    // Create conversions
    CSeq_loc_CI src_it(source);
    CSeq_loc_CI dst_it(target);
    TSeqPos src_start = src_it.GetRange().GetFrom()*m_Dst_width;
    TSeqPos src_len = x_GetRangeLength(src_it)*m_Dst_width;
    TSeqPos dst_start = dst_it.GetRange().GetFrom()*src_width;
    TSeqPos dst_len = x_GetRangeLength(dst_it)*src_width;
    if ( frame ) {
        // ignore pre-frame range
        if (src_width == 3) {
            src_start += frame - 1;
        }
        if (m_Dst_width == 3) {
            dst_start += frame - 1;
        }
    }
    while (src_it  &&  dst_it) {
        x_NextMappingRange(
            src_it.GetSeq_id(), src_start, src_len, src_it.GetStrand(),
            dst_it.GetSeq_id(), dst_start, dst_len, dst_it.GetStrand(),
            dst_it.GetFuzzFrom(), dst_it.GetFuzzTo(),
            src_width);
        if (src_len == 0  &&  ++src_it) {
            src_start = src_it.GetRange().GetFrom()*m_Dst_width;
            src_len = x_GetRangeLength(src_it)*m_Dst_width;
        }
        if (dst_len == 0  &&  ++dst_it) {
            dst_start = dst_it.GetRange().GetFrom()*src_width;
            dst_len = x_GetRangeLength(dst_it)*src_width;
        }
    }
    m_Mappings->SetReverseSrc(source.IsReverseStrand());
    m_Mappings->SetReverseDst(target.IsReverseStrand());
}


void CSeq_loc_Mapper_Base::x_InitializeAlign(const CSeq_align& map_align,
                                             const CSeq_id&    to_id,
                                             TMapOptions       opts)
{
    switch ( map_align.GetSegs().Which() ) {
    case CSeq_align::C_Segs::e_Dendiag:
        {
            const TDendiag& diags = map_align.GetSegs().GetDendiag();
            ITERATE(TDendiag, diag_it, diags) {
                size_t to_row = size_t(-1);
                for (size_t i = 0; i < (*diag_it)->GetIds().size(); ++i) {
                    // find the first row with required ID
                    // ??? check if there are multiple rows with the same ID?
                    if ( (*diag_it)->GetIds()[i]->Equals(to_id) ) {
                        to_row = i;
                        break;
                    }
                }
                if (to_row == size_t(-1)) {
                    NCBI_THROW(CAnnotMapperException, eBadAlignment,
                               "Target ID not found in the alignment");
                }
                x_InitAlign(**diag_it, to_row);
            }
            break;
        }
    case CSeq_align::C_Segs::e_Denseg:
        {
            const CDense_seg& dseg = map_align.GetSegs().GetDenseg();
            size_t to_row = size_t(-1);
            for (size_t i = 0; i < dseg.GetIds().size(); ++i) {
                if (dseg.GetIds()[i]->Equals(to_id)) {
                    to_row = i;
                    break;
                }
            }
            if (to_row == size_t(-1)) {
                NCBI_THROW(CAnnotMapperException, eBadAlignment,
                           "Target ID not found in the alignment");
            }
            x_InitAlign(dseg, to_row, opts);
            break;
        }
    case CSeq_align::C_Segs::e_Std:
        {
            const TStd& std_segs = map_align.GetSegs().GetStd();
            ITERATE(TStd, std_seg, std_segs) {
                size_t to_row = size_t(-1);
                for (size_t i = 0; i < (*std_seg)->GetIds().size(); ++i) {
                    if ((*std_seg)->GetIds()[i]->Equals(to_id)) {
                        to_row = i;
                        break;
                    }
                }
                if (to_row == size_t(-1)) {
                    NCBI_THROW(CAnnotMapperException, eBadAlignment,
                               "Target ID not found in the alignment");
                }
                x_InitAlign(**std_seg, to_row);
            }
            break;
        }
    case CSeq_align::C_Segs::e_Packed:
        {
            const CPacked_seg& pseg = map_align.GetSegs().GetPacked();
            size_t to_row = size_t(-1);
            for (size_t i = 0; i < pseg.GetIds().size(); ++i) {
                if (pseg.GetIds()[i]->Equals(to_id)) {
                    to_row = i;
                    break;
                }
            }
            if (to_row == size_t(-1)) {
                NCBI_THROW(CAnnotMapperException, eBadAlignment,
                           "Target ID not found in the alignment");
            }
            x_InitAlign(pseg, to_row);
            break;
        }
    case CSeq_align::C_Segs::e_Disc:
        {
            const CSeq_align_set& aln_set = map_align.GetSegs().GetDisc();
            ITERATE(CSeq_align_set::Tdata, aln, aln_set.Get()) {
                x_InitializeAlign(**aln, to_id, opts);
            }
            break;
        }
    case CSeq_align::C_Segs::e_Spliced:
        {
            x_InitSpliced(map_align.GetSegs().GetSpliced(), to_id);
            break;
        }
    case CSeq_align::C_Segs::e_Sparse:
        {
            const CSparse_seg& sparse = map_align.GetSegs().GetSparse();
            size_t row = 0;
            ITERATE(CSparse_seg::TRows, it, sparse.GetRows()) {
                // Prefer to map from second to first subrow if their
                // IDs are the same.
                if ((*it)->GetFirst_id().Equals(to_id)) {
                    x_InitSparse(sparse, row, fAlign_Sparse_ToFirst);
                }
                else if ((*it)->GetSecond_id().Equals(to_id)) {
                    x_InitSparse(sparse, row, fAlign_Sparse_ToSecond);
                }
            }
            break;
        }
    default:
        NCBI_THROW(CAnnotMapperException, eBadAlignment,
                   "Unsupported alignment type");
    }
}


void CSeq_loc_Mapper_Base::x_InitializeAlign(const CSeq_align& map_align,
                                             size_t            to_row,
                                             TMapOptions       opts)
{
    switch ( map_align.GetSegs().Which() ) {
    case CSeq_align::C_Segs::e_Dendiag:
        {
            const TDendiag& diags = map_align.GetSegs().GetDendiag();
            ITERATE(TDendiag, diag_it, diags) {
                x_InitAlign(**diag_it, to_row);
            }
            break;
        }
    case CSeq_align::C_Segs::e_Denseg:
        {
            const CDense_seg& dseg = map_align.GetSegs().GetDenseg();
            x_InitAlign(dseg, to_row, opts);
            break;
        }
    case CSeq_align::C_Segs::e_Std:
        {
            const TStd& std_segs = map_align.GetSegs().GetStd();
            ITERATE(TStd, std_seg, std_segs) {
                x_InitAlign(**std_seg, to_row);
            }
            break;
        }
    case CSeq_align::C_Segs::e_Packed:
        {
            const CPacked_seg& pseg = map_align.GetSegs().GetPacked();
            x_InitAlign(pseg, to_row);
            break;
        }
    case CSeq_align::C_Segs::e_Disc:
        {
            // The same row in each sub-alignment?
            const CSeq_align_set& aln_set = map_align.GetSegs().GetDisc();
            ITERATE(CSeq_align_set::Tdata, aln, aln_set.Get()) {
                x_InitializeAlign(**aln, to_row, opts);
            }
            break;
        }
    case CSeq_align::C_Segs::e_Spliced:
        {
            if (to_row == 0  ||  to_row == 1) {
                x_InitSpliced(map_align.GetSegs().GetSpliced(), to_row);
            }
            else {
                NCBI_THROW(CAnnotMapperException, eBadAlignment,
                    "Invalid row number in spliced-seg alignment");
            }
            break;
        }
    case CSeq_align::C_Segs::e_Sparse:
        {
            x_InitSparse(map_align.GetSegs().GetSparse(), to_row, opts);
            break;
        }
    default:
        NCBI_THROW(CAnnotMapperException, eBadAlignment,
                   "Unsupported alignment type");
    }
}


void CSeq_loc_Mapper_Base::x_InitAlign(const CDense_diag& diag, size_t to_row)
{
    if (diag.GetStarts()[to_row] < 0) {
        // Destination ID is not present in this dense-diag
        return;
    }
    size_t dim = diag.GetDim();
    _ASSERT(to_row < dim);
    if (dim != diag.GetIds().size()) {
        ERR_POST_X(1, Warning << "Invalid 'ids' size in dendiag");
        dim = min(dim, diag.GetIds().size());
    }
    if (dim != diag.GetStarts().size()) {
        ERR_POST_X(2, Warning << "Invalid 'starts' size in dendiag");
        dim = min(dim, diag.GetStarts().size());
    }
    bool have_strands = diag.IsSetStrands();
    if (have_strands && dim != diag.GetStrands().size()) {
        ERR_POST_X(3, Warning << "Invalid 'strands' size in dendiag");
        dim = min(dim, diag.GetStrands().size());
    }

    ENa_strand dst_strand = have_strands ?
        diag.GetStrands()[to_row] : eNa_strand_unknown;
    const CSeq_id& dst_id = *diag.GetIds()[to_row];
    if (!m_Dst_width) {
        m_Dst_width = CheckSeqWidth(dst_id, m_Dst_width);
    }

    for (size_t row = 0; row < dim; ++row) {
        if (row == to_row) {
            continue;
        }
        if (diag.GetStarts()[row] < 0) {
            continue;
        }
        // In alignments with multiple sequence types segment length
        // should be multiplied by character width.
        int src_width = 0;
        const CSeq_id& src_id = *diag.GetIds()[row];
        src_width = CheckSeqWidth(src_id, src_width);
        m_UseWidth |= (src_width != m_Dst_width);
        int dst_width_rel = (m_UseWidth) ? m_Dst_width : 1;
        int src_width_rel = (m_UseWidth) ? src_width : 1;
        TSeqPos src_len = diag.GetLen()*src_width*dst_width_rel;
        TSeqPos dst_len = src_len;
        TSeqPos src_start = diag.GetStarts()[row]*dst_width_rel;
        TSeqPos dst_start = diag.GetStarts()[to_row]*src_width_rel;
        ENa_strand src_strand = have_strands ?
            diag.GetStrands()[row] : eNa_strand_unknown;
        x_NextMappingRange(
            src_id, src_start, src_len, src_strand,
            dst_id, dst_start, dst_len, dst_strand,
            0, 0, src_width_rel);
        _ASSERT(!src_len  &&  !dst_len);
    }
}


void CSeq_loc_Mapper_Base::x_InitAlign(const CDense_seg& denseg,
                                       size_t to_row,
                                       TMapOptions opts)
{
    size_t dim = denseg.GetDim();
    _ASSERT(to_row < dim);

    size_t numseg = denseg.GetNumseg();
    // claimed dimension may not be accurate :-/
    if (numseg != denseg.GetLens().size()) {
        ERR_POST_X(4, Warning << "Invalid 'lens' size in denseg");
        numseg = min(numseg, denseg.GetLens().size());
    }
    if (dim != denseg.GetIds().size()) {
        ERR_POST_X(5, Warning << "Invalid 'ids' size in denseg");
        dim = min(dim, denseg.GetIds().size());
    }
    if (dim*numseg != denseg.GetStarts().size()) {
        ERR_POST_X(6, Warning << "Invalid 'starts' size in denseg");
        dim = min(dim*numseg, denseg.GetStarts().size()) / numseg;
    }
    bool have_strands = denseg.IsSetStrands();
    if (have_strands && dim*numseg != denseg.GetStrands().size()) {
        ERR_POST_X(7, Warning << "Invalid 'strands' size in denseg");
        dim = min(dim*numseg, denseg.GetStrands().size()) / numseg;
    }

    const CSeq_id& dst_id = *denseg.GetIds()[to_row];

    if (!m_Dst_width) {
        if ( denseg.IsSetWidths() ) {
            m_Dst_width = denseg.GetWidths()[to_row];
        }
        else {
            m_Dst_width = CheckSeqWidth(dst_id, m_Dst_width);
        }
    }
    for (size_t row = 0; row < dim; ++row) {
        if (row == to_row) {
            continue;
        }
        const CSeq_id& src_id = *denseg.GetIds()[row];

        int src_width = 0;
        if ( denseg.IsSetWidths() ) {
            src_width = denseg.GetWidths()[row];
        }
        else {
            src_width = CheckSeqWidth(src_id, src_width);
        }
        m_UseWidth |= (src_width != m_Dst_width);
        int dst_width_rel = (m_UseWidth) ? m_Dst_width : 1;
        int src_width_rel = (m_UseWidth) ? src_width : 1;

        if (opts & fAlign_Dense_seg_TotalRange) {
            TSeqRange r_src = denseg.GetSeqRange(row);
            TSeqRange r_dst = denseg.GetSeqRange(to_row);

            _ASSERT(r_src.GetLength() != 0  &&  r_dst.GetLength() != 0);
            ENa_strand dst_strand = have_strands ?
                denseg.GetStrands()[to_row] : eNa_strand_unknown;
            ENa_strand src_strand = have_strands ?
                denseg.GetStrands()[row] : eNa_strand_unknown;

            TSeqPos src_len = r_src.GetLength();
            TSeqPos dst_len = r_dst.GetLength();
            TSeqPos src_start = r_src.GetFrom();
            TSeqPos dst_start = r_dst.GetFrom();

            if (src_len != dst_len) {
                _TRACE("");
            }
            x_NextMappingRange(
                src_id, src_start, src_len, src_strand,
                dst_id, dst_start, dst_len, dst_strand,
                0, 0, src_width_rel);

        } else {
            for (size_t seg = 0; seg < numseg; ++seg) {
                int i_src_start = denseg.GetStarts()[seg*dim + row];
                int i_dst_start = denseg.GetStarts()[seg*dim + to_row];
                if (i_src_start < 0  ||  i_dst_start < 0) {
                    continue;
                }

                ENa_strand dst_strand = have_strands ?
                    denseg.GetStrands()[seg*dim + to_row] : eNa_strand_unknown;
                ENa_strand src_strand = have_strands ?
                    denseg.GetStrands()[seg*dim + row] : eNa_strand_unknown;

                // In alignments with multiple sequence types segment length
                // should be multiplied by character width.
                TSeqPos src_len =
                    denseg.GetLens()[seg]*src_width_rel*dst_width_rel;
                TSeqPos dst_len = src_len;
                TSeqPos src_start = (TSeqPos)(i_src_start)*dst_width_rel;
                TSeqPos dst_start = (TSeqPos)(i_dst_start)*src_width_rel;
                x_NextMappingRange(
                    src_id, src_start, src_len, src_strand,
                    dst_id, dst_start, dst_len, dst_strand,
                    0, 0, src_width_rel);
                _ASSERT(!src_len  &&  !dst_len);
            }
        }
    }
}


void CSeq_loc_Mapper_Base::x_InitAlign(const CStd_seg& sseg, size_t to_row)
{
    size_t dim = sseg.GetDim();
    if (dim != sseg.GetLoc().size()) {
        ERR_POST_X(8, Warning << "Invalid 'loc' size in std-seg");
        dim = min(dim, sseg.GetLoc().size());
    }
    if (sseg.IsSetIds()
        && dim != sseg.GetIds().size()) {
        ERR_POST_X(9, Warning << "Invalid 'ids' size in std-seg");
        dim = min(dim, sseg.GetIds().size());
    }

    const CSeq_loc& dst_loc = *sseg.GetLoc()[to_row];

    for (size_t row = 0; row < dim; ++row ) {
        if (row == to_row) {
            continue;
        }
        const CSeq_loc& src_loc = *sseg.GetLoc()[row];
        if ( src_loc.IsEmpty() ) {
            // skipped row in this segment
            continue;
        }
        x_InitializeLocs(src_loc, dst_loc);
    }
}


void CSeq_loc_Mapper_Base::x_InitAlign(const CPacked_seg& pseg, size_t to_row)
{
    size_t dim    = pseg.GetDim();
    size_t numseg = pseg.GetNumseg();
    // claimed dimension may not be accurate :-/
    if (numseg != pseg.GetLens().size()) {
        ERR_POST_X(10, Warning << "Invalid 'lens' size in packed-seg");
        numseg = min(numseg, pseg.GetLens().size());
    }
    if (dim != pseg.GetIds().size()) {
        ERR_POST_X(11, Warning << "Invalid 'ids' size in packed-seg");
        dim = min(dim, pseg.GetIds().size());
    }
    if (dim*numseg != pseg.GetStarts().size()) {
        ERR_POST_X(12, Warning << "Invalid 'starts' size in packed-seg");
        dim = min(dim*numseg, pseg.GetStarts().size()) / numseg;
    }
    bool have_strands = pseg.IsSetStrands();
    if (have_strands && dim*numseg != pseg.GetStrands().size()) {
        ERR_POST_X(13, Warning << "Invalid 'strands' size in packed-seg");
        dim = min(dim*numseg, pseg.GetStrands().size()) / numseg;
    }

    const CSeq_id& dst_id = *pseg.GetIds()[to_row];
    if (!m_Dst_width) {
        m_Dst_width = CheckSeqWidth(dst_id, m_Dst_width);
    }
    for (size_t row = 0; row < dim; ++row) {
        if (row == to_row) {
            continue;
        }
        const CSeq_id& src_id = *pseg.GetIds()[row];

        int src_width = 0;
        src_width = CheckSeqWidth(src_id, src_width);
        m_UseWidth |= (src_width != m_Dst_width);
        int dst_width_rel = (m_UseWidth) ? m_Dst_width : 1;
        int src_width_rel = (m_UseWidth) ? src_width : 1;

        for (size_t seg = 0; seg < numseg; ++seg) {
            if (!pseg.GetPresent()[row]  ||  !pseg.GetPresent()[to_row]) {
                continue;
            }

            ENa_strand dst_strand = have_strands ?
                pseg.GetStrands()[seg*dim + to_row] : eNa_strand_unknown;
            ENa_strand src_strand = have_strands ?
                pseg.GetStrands()[seg*dim + row] : eNa_strand_unknown;

            // In alignments with multiple sequence types segment length
            // should be multiplied by character width.
            TSeqPos src_len = pseg.GetLens()[seg]*src_width*dst_width_rel;
            TSeqPos dst_len = src_len;
            TSeqPos src_start = pseg.GetStarts()[seg*dim + row]*dst_width_rel;
            TSeqPos dst_start = pseg.GetStarts()[seg*dim + to_row]*src_width_rel;
            x_NextMappingRange(
                src_id, src_start, src_len, src_strand,
                dst_id, dst_start, dst_len, dst_strand,
                0, 0, src_width_rel);
            _ASSERT(!src_len  &&  !dst_len);
        }
    }
}


void CSeq_loc_Mapper_Base::x_InitSpliced(const CSpliced_seg& spliced,
                                         const CSeq_id&      to_id)
{
    // Assume the same ID can not be used in both genomic and product rows,
    // try find the correct row.
    if (spliced.IsSetGenomic_id()  &&  spliced.GetGenomic_id().Equals(to_id)) {
        x_InitSpliced(spliced, 0);
        return;
    }
    if (spliced.IsSetProduct_id()  &&  spliced.GetProduct_id().Equals(to_id)) {
        x_InitSpliced(spliced, 1);
        return;
    }
    // Global IDs are not set or not equal to to_id
    ITERATE(CSpliced_seg::TExons, it, spliced.GetExons()) {
        const CSpliced_exon& ex = **it;
        if (ex.IsSetGenomic_id()  &&  ex.GetGenomic_id().Equals(to_id)) {
            x_InitSpliced(spliced, 0);
            return;
        }
        if (ex.IsSetProduct_id()  &&  ex.GetProduct_id().Equals(to_id)) {
            x_InitSpliced(spliced, 1);
            return;
        }
    }
}


void CSeq_loc_Mapper_Base::x_InitSpliced(const CSpliced_seg& spliced,
                                         int                 to_row)
{
    bool have_gen_strand = spliced.IsSetGenomic_strand();
    ENa_strand gen_strand = have_gen_strand ?
        spliced.GetGenomic_strand() : eNa_strand_unknown;
    bool have_prod_strand = spliced.IsSetProduct_strand();
    ENa_strand prod_strand = have_prod_strand ?
        spliced.GetProduct_strand() : eNa_strand_unknown;

    const CSeq_id* gen_id = spliced.IsSetGenomic_id() ?
        &spliced.GetGenomic_id() : 0;
    const CSeq_id* prod_id = spliced.IsSetProduct_id() ?
        &spliced.GetProduct_id() : 0;

    int src_width;
    switch ( spliced.GetProduct_type() ) {
    case CSpliced_seg::eProduct_type_protein:
        if ( to_row == 1 ) {
            src_width = 3;
            m_Dst_width = 1;
        }
        else {
            src_width = 1;
            m_Dst_width = 3;
        }
        m_UseWidth = true;
        break;
    case CSpliced_seg::eProduct_type_transcript:
        src_width = 1;
        m_Dst_width = 1;
        m_UseWidth = false;
        break;
    default:
        ERR_POST_X(14, Error << "Unknown product type in spliced-seg");
        return;
    }
    int dst_width = m_UseWidth ? m_Dst_width : 1;

    ITERATE(CSpliced_seg::TExons, it, spliced.GetExons()) {
        const CSpliced_exon& ex = **it;
        const CSeq_id* ex_gen_id = ex.IsSetGenomic_id() ?
            &ex.GetGenomic_id() : gen_id;
        const CSeq_id* ex_prod_id = ex.IsSetProduct_id() ?
            &ex.GetProduct_id() : prod_id;
        if (!ex_gen_id  ||  !ex_prod_id) {
            ERR_POST_X(15, Error << "Missing id in spliced-exon");
            continue;
        }
        ENa_strand ex_gen_strand = ex.IsSetGenomic_strand() ?
            ex.GetGenomic_strand() : gen_strand;
        ENa_strand ex_prod_strand = ex.IsSetProduct_strand() ?
            ex.GetProduct_strand() : prod_strand;
        TSeqPos gen_from = ex.GetGenomic_start();
        TSeqPos gen_to = ex.GetGenomic_end();
        TSeqPos prod_from, prod_to;
        if (m_UseWidth != ex.GetProduct_start().IsProtpos()  ||
            m_UseWidth != ex.GetProduct_end().IsProtpos()) {
            ERR_POST_X(16, Error <<
                "Invalid product position type in spliced-exon");
            continue;
        }
        if ( m_UseWidth ) {
            const CProt_pos& from_pos = ex.GetProduct_start().GetProtpos();
            const CProt_pos& to_pos = ex.GetProduct_end().GetProtpos();
            prod_from = from_pos.GetAmin()*3;
            if ( from_pos.GetFrame() ) {
                prod_from += from_pos.GetFrame() - 1;
            }
            prod_to = to_pos.GetAmin()*3;
            if ( to_pos.GetFrame() ) {
                prod_to += to_pos.GetFrame() - 1;
            }
        }
        else {
            prod_from = ex.GetProduct_start().IsNucpos() ?
                ex.GetProduct_start().GetNucpos()
                : ex.GetProduct_start().GetProtpos().GetAmin();
            prod_to = ex.GetProduct_end().IsNucpos() ?
                ex.GetProduct_end().GetNucpos()
                : ex.GetProduct_end().GetProtpos().GetAmin();
        }
        TSeqPos gen_len = gen_to - gen_from + 1;
        TSeqPos prod_len = prod_to - prod_from + 1;
        if ( to_row == 1 ) {
            x_NextMappingRange(
                *ex_gen_id, gen_from, gen_len, ex_gen_strand,
                *ex_prod_id, prod_from, prod_len, prod_strand,
                0, 0, src_width);
        }
        else {
            x_NextMappingRange(
                *ex_prod_id, prod_from, prod_len, ex_prod_strand,
                *ex_gen_id, gen_from, gen_len, gen_strand,
                0, 0, src_width);
        }
        if (gen_len  ||  prod_len) {
            ERR_POST_X(17, Error <<
                "Genomic vs product length mismatch in spliced-exon");
        }
    }
}


void CSeq_loc_Mapper_Base::x_InitSparse(const CSparse_seg& sparse,
                                        int to_row,
                                        TMapOptions opts)
{
    _ASSERT(size_t(to_row) < sparse.GetRows().size());
    const CSparse_align& row = *sparse.GetRows()[to_row];
    bool to_second = (opts & fAlign_Sparse_ToSecond) != 0;

    size_t numseg = row.GetNumseg();
    // claimed dimension may not be accurate :-/
    if (numseg != row.GetFirst_starts().size()) {
        ERR_POST_X(18, Warning <<
            "Invalid 'first-starts' size in sparse-align");
        numseg = min(numseg, row.GetFirst_starts().size());
    }
    if (numseg != row.GetSecond_starts().size()) {
        ERR_POST_X(19, Warning <<
            "Invalid 'second-starts' size in sparse-align");
        numseg = min(numseg, row.GetSecond_starts().size());
    }
    if (numseg != row.GetLens().size()) {
        ERR_POST_X(20, Warning << "Invalid 'lens' size in sparse-align");
        numseg = min(numseg, row.GetLens().size());
    }
    bool have_strands = row.IsSetSecond_strands();
    if (have_strands  &&  numseg != row.GetSecond_strands().size()) {
        ERR_POST_X(21, Warning <<
            "Invalid 'second-strands' size in sparse-align");
        numseg = min(numseg, row.GetSecond_strands().size());
    }

    const CSeq_id& first_id = row.GetFirst_id();
    const CSeq_id& second_id = row.GetSecond_id();

    // Destination width must be checked first
    m_Dst_width = CheckSeqWidth(to_second ? second_id : first_id, m_Dst_width);
    int src_width = CheckSeqWidth(to_second ? first_id : second_id, 0);
    int first_width = to_second ? src_width : m_Dst_width;
    int second_width = to_second ? m_Dst_width : src_width;
    // Always use widths because another pair of rows may have different widths
    m_UseWidth = true;

    const CSparse_align::TFirst_starts& first_starts = row.GetFirst_starts();
    const CSparse_align::TSecond_starts& second_starts = row.GetSecond_starts();
    const CSparse_align::TLens& lens = row.GetLens();
    const CSparse_align::TSecond_strands& strands = row.GetSecond_strands();

    for (size_t i = 0; i < numseg; i++) {
        TSeqPos first_start = first_starts[i]*second_width;
        TSeqPos second_start = second_starts[i]*first_width;
        TSeqPos first_len = lens[i]*first_width*second_width;
        TSeqPos second_len = first_len;
        ENa_strand strand = have_strands ? strands[i] : eNa_strand_unknown;
        if ( to_second ) {
            x_NextMappingRange(
                first_id, first_start, first_len, eNa_strand_unknown,
                second_id, second_start, second_len, strand,
                0, 0, src_width);
        }
        else {
            x_NextMappingRange(
                second_id, second_start, second_len, strand,
                first_id, first_start, first_len, eNa_strand_unknown,
                0, 0, src_width);
        }
        _ASSERT(!first_len  &&  !second_len);
    }
}


/////////////////////////////////////////////////////////////////////
//
//   Initialization helpers
//


void CSeq_loc_Mapper_Base::CollectSynonyms(const CSeq_id_Handle& id,
                                           TSynonyms& synonyms) const
{
    synonyms.push_back(id);
}


int CSeq_loc_Mapper_Base::CheckSeqWidth(const CSeq_id& id,
                                        int            width,
                                        TSeqPos*       length)
{
    return width;
}


int CSeq_loc_Mapper_Base::x_CheckSeqWidth(const CSeq_loc& loc,
                                          TSeqPos*        total_length)
{
    int width = 0;
    *total_length = 0;
    for (CSeq_loc_CI it(loc); it; ++it) {
        if (*total_length != kInvalidSeqPos) {
            if ( it.GetRange().IsWhole() ) {
                *total_length = kInvalidSeqPos;
            }
            else {
                *total_length += it.GetRange().GetLength();
            }
        }
        width = CheckSeqWidth(it.GetSeq_id(), width, total_length);
        if (*total_length == kInvalidSeqPos) {
            break; // nothing more to collect
        }
    }
    return width;
}


TSeqPos CSeq_loc_Mapper_Base::GetSequenceLength(const CSeq_id& id)
{
    return kInvalidSeqPos;
}


TSeqPos CSeq_loc_Mapper_Base::x_GetRangeLength(const CSeq_loc_CI& it)
{
    if (it.IsWhole()  &&  IsReverse(it.GetStrand())) {
        // For reverse strand we need real interval length, not just "whole"
        return GetSequenceLength(it.GetSeq_id());
    }
    else {
        return it.GetRange().GetLength();
    }
}


void CSeq_loc_Mapper_Base::x_NextMappingRange(const CSeq_id&   src_id,
                                              TSeqPos&         src_start,
                                              TSeqPos&         src_len,
                                              ENa_strand       src_strand,
                                              const CSeq_id&   dst_id,
                                              TSeqPos&         dst_start,
                                              TSeqPos&         dst_len,
                                              ENa_strand       dst_strand,
                                              const CInt_fuzz* fuzz_from,
                                              const CInt_fuzz* fuzz_to,
                                              int              src_width)
{
    TSeqPos cvt_src_start = src_start;
    TSeqPos cvt_dst_start = dst_start;
    TSeqPos cvt_length;

    if (src_len == dst_len) {
        if (src_len == kInvalidSeqPos) {
            // Mapping whole to whole - need to know real length.
            // Do not check strand - if it's reversed, the real length
            // must be already known.
            src_len = GetSequenceLength(src_id)*m_Dst_width - src_start;
            dst_len = GetSequenceLength(dst_id)*src_width - dst_start;
        }
        cvt_length = src_len;
        src_len = 0;
        dst_len = 0;
    }
    else if (src_len > dst_len) {
        // reverse interval for minus strand
        if (IsReverse(src_strand)) {
            cvt_src_start += src_len - dst_len;
        }
        else {
            src_start += dst_len;
        }
        cvt_length = dst_len;
        if (src_len != kInvalidSeqPos) {
            src_len -= cvt_length;
        }
        dst_len = 0;
    }
    else { // if (src_len < dst_len)
        // reverse interval for minus strand
        if ( IsReverse(dst_strand) ) {
            cvt_dst_start += dst_len - src_len;
        }
        else {
            dst_start += src_len;
        }
        cvt_length = src_len;
        if (dst_len != kInvalidSeqPos) {
            dst_len -= cvt_length;
        }
        src_len = 0;
    }
    // Special case: prepare to extend mapped "to" if:
    // - mapping is from prot to nuc
    // - destination "to" is partial
    bool ext_to = false;
    if ( m_Dst_width == 3 ) {
        if ( IsReverse(dst_strand) && fuzz_from ) {
            ext_to = fuzz_from  &&
                fuzz_from->IsLim()  &&
                fuzz_from->GetLim() == CInt_fuzz::eLim_lt;
        }
        else if ( !IsReverse(dst_strand) && fuzz_to ) {
            ext_to = fuzz_to  &&
                fuzz_to->IsLim()  &&
                fuzz_to->GetLim() == CInt_fuzz::eLim_gt;
        }
    }
    x_AddConversion(
        src_id, cvt_src_start, src_strand,
        dst_id, cvt_dst_start, dst_strand,
        cvt_length, ext_to);
}


void CSeq_loc_Mapper_Base::x_AddConversion(const CSeq_id& src_id,
                                           TSeqPos        src_start,
                                           ENa_strand     src_strand,
                                           const CSeq_id& dst_id,
                                           TSeqPos        dst_start,
                                           ENa_strand     dst_strand,
                                           TSeqPos        length,
                                           bool           ext_right)
{
    if (m_DstRanges.size() <= size_t(dst_strand)) {
        m_DstRanges.resize(size_t(dst_strand) + 1);
    }
    TSynonyms syns;
    CollectSynonyms(CSeq_id_Handle::GetHandle(src_id), syns);
    ITERATE(TSynonyms, syn_it, syns) {
        m_Mappings->AddConversion(
            *syn_it, src_start, length, src_strand,
            CSeq_id_Handle::GetHandle(dst_id), dst_start, dst_strand,
            ext_right);
    }
    m_DstRanges[size_t(dst_strand)][CSeq_id_Handle::GetHandle(dst_id)]
        .push_back(TRange(dst_start, dst_start + length - 1));
}


void CSeq_loc_Mapper_Base::x_PreserveDestinationLocs(void)
{
    for (size_t str_idx = 0; str_idx < m_DstRanges.size(); str_idx++) {
        NON_CONST_ITERATE(TDstIdMap, id_it, m_DstRanges[str_idx]) {
            TSynonyms syns;
            CollectSynonyms(id_it->first, syns);
            id_it->second.sort();
            TSeqPos dst_start = kInvalidSeqPos;
            TSeqPos dst_stop = kInvalidSeqPos;
            ITERATE(TDstRanges, rg_it, id_it->second) {
                // Collect and merge ranges
                if (dst_start == kInvalidSeqPos) {
                    dst_start = rg_it->GetFrom();
                    dst_stop = rg_it->GetTo();
                    continue;
                }
                if (rg_it->GetFrom() <= dst_stop + 1) {
                    // overlapping or abutting ranges, continue collecting
                    dst_stop = max(dst_stop, rg_it->GetTo());
                    continue;
                }
                ITERATE(TSynonyms, syn_it, syns) {
                    // Separate ranges, add conversion and restart collecting
                    m_Mappings->AddConversion(
                        *syn_it, dst_start, dst_stop - dst_start + 1,
                        ENa_strand(str_idx),
                        id_it->first, dst_start, ENa_strand(str_idx));
                }
            }
            if (dst_start < dst_stop) {
                ITERATE(TSynonyms, syn_it, syns) {
                    m_Mappings->AddConversion(
                        *syn_it, dst_start, dst_stop - dst_start + 1,
                        ENa_strand(str_idx),
                        id_it->first, dst_start, ENa_strand(str_idx));
                }
            }
        }
    }
    m_DstRanges.clear();
}


/////////////////////////////////////////////////////////////////////
//
//   Mapping methods
//


// Check location type, optimize if possible (empty mix to NULL,
// mix with a single element to this element etc.).
void OptimizeSeq_loc(CRef<CSeq_loc>& loc)
{
    if ( !loc ) {
        loc.Reset(new CSeq_loc);
        loc->SetNull();
        return;
    }
    switch (loc->Which()) {
    case CSeq_loc::e_not_set:
    case CSeq_loc::e_Feat:
    case CSeq_loc::e_Null:
    case CSeq_loc::e_Empty:
    case CSeq_loc::e_Whole:
    case CSeq_loc::e_Int:
    case CSeq_loc::e_Pnt:
    case CSeq_loc::e_Equiv:
    case CSeq_loc::e_Bond:
    case CSeq_loc::e_Packed_int:  // pack ?
    case CSeq_loc::e_Packed_pnt:  // pack ?
        return;
    case CSeq_loc::e_Mix:
        {
            switch ( loc->GetMix().Get().size() ) {
            case 0:
                loc->SetNull();
                break;
            case 1:
                {
                    CRef<CSeq_loc> single = *loc->SetMix().Set().begin();
                    loc = single;
                    break;
                }
            default:
                {
                    // Try to convert to packed-int
                    CRef<CSeq_loc> packed;
                    NON_CONST_ITERATE(CSeq_loc_mix::Tdata, it,
                        loc->SetMix().Set()) {
                        if ( !(*it)->IsInt() ) {
                            packed.Reset();
                            break;
                        }
                        if ( !packed ) {
                            packed.Reset(new CSeq_loc);
                        }
                        packed->SetPacked_int().Set().
                            push_back(Ref(&(*it)->SetInt()));
                    }
                    if ( packed ) {
                        loc = packed;
                    }
                    break;
                }
            }
            break;
        }
    default:
        NCBI_THROW(CAnnotMapperException, eBadLocation,
                   "Unsupported location type");
    }
}


bool CSeq_loc_Mapper_Base::x_MapNextRange(const TRange&     src_rg,
                                          bool              is_set_strand,
                                          ENa_strand        src_strand,
                                          const TRangeFuzz& src_fuzz,
                                          TSortedMappings&  mappings,
                                          size_t            cvt_idx,
                                          TSeqPos*          last_src_to)
{
    const CMappingRange& cvt = *mappings[cvt_idx];
    if ( !cvt.CanMap(src_rg.GetFrom(), src_rg.GetTo(),
        is_set_strand && m_CheckStrand, src_strand) ) {
        // Can not map the range
        return false;
    }

    TSeqPos left = src_rg.GetFrom();
    TSeqPos right = src_rg.GetTo();
    bool partial_left = false;
    bool partial_right = false;

    bool reverse = IsReverse(src_strand);

    if (left < cvt.m_Src_from) {
        left = cvt.m_Src_from;
        if ( !reverse ) {
            // Partial if there's gap between left and last_src_to
            partial_left = left != *last_src_to + 1;
        }
        else {
            // Partial if there's gap between left and next cvt. right end
            partial_left = (cvt_idx == mappings.size() - 1)  ||
                (mappings[cvt_idx + 1]->m_Src_to + 1 != left);
        }
    }
    if (right > cvt.m_Src_to) {
        right = cvt.m_Src_to;
        if ( !reverse ) {
            // Partial if there's gap between right and next cvt. left end
            partial_right = (cvt_idx == mappings.size() - 1)  ||
                (mappings[cvt_idx + 1]->m_Src_from != right + 1);
        }
        else {
            // Partial if there's gap between right and last_src_to
            partial_right = right + 1 != *last_src_to;
        }
    }
    if (right < left) {
        // Empty range
        return false;
    }
    *last_src_to = reverse ? left : right;

    TRangeFuzz fuzz;

    if ( partial_left ) {
        // Set fuzz-from if a range was skipped on the left
        fuzz.first.Reset(new CInt_fuzz);
        fuzz.first->SetLim(CInt_fuzz::eLim_lt);
    }
    else {
        if ( (!reverse  &&  cvt_idx == 0)  ||
            (reverse  &&  cvt_idx == mappings.size() - 1) ) {
            // Preserve fuzz-from on the left end
            fuzz.first = src_fuzz.first;
        }
    }
    if ( partial_right ) {
        // Set fuzz-to if a range will be skipped on the right
        fuzz.second.Reset(new CInt_fuzz);
        fuzz.second->SetLim(CInt_fuzz::eLim_gt);
    }
    else {
        if ( (reverse  &&  cvt_idx == 0)  ||
            (!reverse  &&  cvt_idx == mappings.size() - 1) ) {
            // Preserve fuzz-to on the right end
            fuzz.second = src_fuzz.second;
        }
    }
    if ( m_LastTruncated ) {
        if ( !reverse && !fuzz.first ) {
            fuzz.first.Reset(new CInt_fuzz);
            fuzz.first->SetLim(CInt_fuzz::eLim_tl);
        }
        else if ( reverse  &&  !fuzz.second ) {
            fuzz.second.Reset(new CInt_fuzz);
            fuzz.second->SetLim(CInt_fuzz::eLim_tr);
        }
        m_LastTruncated = false;
    }

    TRangeFuzz mapped_fuzz = cvt.Map_Fuzz(fuzz);

    TRange rg = cvt.Map_Range(left, right, &src_fuzz);
    ENa_strand dst_strand;
    bool is_set_dst_strand = cvt.Map_Strand(is_set_strand,
        src_strand, &dst_strand);
    x_PushMappedRange(cvt.m_Dst_id_Handle,
                      STRAND_TO_INDEX(is_set_dst_strand, dst_strand),
                      rg, mapped_fuzz);
    return true;
}


void CSeq_loc_Mapper_Base::x_SetLastTruncated(void)
{
    if ( m_LastTruncated ) {
        return;
    }
    m_LastTruncated = true;
    x_PushRangesToDstMix();
    if ( m_Dst_loc  &&  !m_Dst_loc->IsPartialStop(eExtreme_Biological) ) {
        m_Dst_loc->SetTruncatedStop(true, eExtreme_Biological);
    }
}


bool CSeq_loc_Mapper_Base::x_MapInterval(const CSeq_id&   src_id,
                                         TRange           src_rg,
                                         bool             is_set_strand,
                                         ENa_strand       src_strand,
                                         TRangeFuzz       orig_fuzz)
{
    bool res = false;
    if (m_UseWidth  &&
        m_Widths[CSeq_id_Handle::GetHandle(src_id)] & fWidthProtToNuc) {
        src_rg = TRange(src_rg.GetFrom()*3, src_rg.GetTo()*3 + 2);
    }

    CSeq_id_Handle src_idh = CSeq_id_Handle::GetHandle(src_id);

    // Sorted mapping ranges
    TSortedMappings mappings;
    TRangeIterator rg_it = m_Mappings->BeginMappingRanges(
        src_idh, src_rg.GetFrom(), src_rg.GetTo());
    for ( ; rg_it; ++rg_it) {
        mappings.push_back(rg_it->second);
    }
    if ( IsReverse(src_strand) ) {
        sort(mappings.begin(), mappings.end(), CMappingRangeRef_LessRev());
    }
    else {
        sort(mappings.begin(), mappings.end(), CMappingRangeRef_Less());
    }
    TSeqPos last_src_to = 0;
    for (size_t idx = 0; idx < mappings.size(); ++idx) {
        res |= x_MapNextRange(src_rg,
                              is_set_strand, src_strand,
                              orig_fuzz,
                              mappings, idx,
                              &last_src_to);
    }
    if ( !res ) {
        x_SetLastTruncated();
    }
    return res;
}


void CSeq_loc_Mapper_Base::x_MapSeq_loc(const CSeq_loc& src_loc)
{
    switch ( src_loc.Which() ) {
    case CSeq_loc::e_Null:
        if (m_GapFlag == eGapRemove) {
            return;
        }
        // Proceed to seq-loc duplication
    case CSeq_loc::e_not_set:
    case CSeq_loc::e_Feat:
    {
        x_PushRangesToDstMix();
        CRef<CSeq_loc> loc(new CSeq_loc);
        loc->Assign(src_loc);
        x_PushLocToDstMix(loc);
        break;
    }
    case CSeq_loc::e_Empty:
    {
        bool res = false;
        TRangeIterator mit = m_Mappings->BeginMappingRanges(
            CSeq_id_Handle::GetHandle(src_loc.GetEmpty()),
            TRange::GetWhole().GetFrom(),
            TRange::GetWhole().GetTo());
        for ( ; mit; ++mit) {
            const CMappingRange& cvt = *mit->second;
            if ( cvt.GoodSrcId(src_loc.GetEmpty()) ) {
                TRangeFuzz fuzz(kEmptyFuzz, kEmptyFuzz);
                x_PushMappedRange(
                    cvt.GetDstIdHandle(),
                    // CSeq_id_Handle::GetHandle(src_loc.GetEmpty()),
                    STRAND_TO_INDEX(false, eNa_strand_unknown),
                    TRange::GetEmpty(), fuzz);
                res = true;
                break;
            }
        }
        if ( !res ) {
            if ( m_KeepNonmapping ) {
                x_PushRangesToDstMix();
                CRef<CSeq_loc> loc(new CSeq_loc);
                loc->Assign(src_loc);
                x_PushLocToDstMix(loc);
            }
            else {
                m_Partial = true;
            }
        }
        break;
    }
    case CSeq_loc::e_Whole:
    {
        const CSeq_id& src_id = src_loc.GetWhole();
        // Convert to the allowed master seq interval
        TSeqPos src_to = GetSequenceLength(src_id);
        if ( src_to != kInvalidSeqPos) {
            src_to--;
        }
        bool res = x_MapInterval(src_id, TRange(0, src_to),
            false, eNa_strand_unknown,
            TRangeFuzz(kEmptyFuzz, kEmptyFuzz));
        if ( !res ) {
            if ( m_KeepNonmapping ) {
                x_PushRangesToDstMix();
                CRef<CSeq_loc> loc(new CSeq_loc);
                loc->Assign(src_loc);
                x_PushLocToDstMix(loc);
            }
            else {
                m_Partial = true;
            }
        }
        break;
    }
    case CSeq_loc::e_Int:
    {
        const CSeq_interval& src_int = src_loc.GetInt();
        TRangeFuzz fuzz(kEmptyFuzz, kEmptyFuzz);
        if ( src_int.IsSetFuzz_from() ) {
            fuzz.first.Reset(new CInt_fuzz);
            fuzz.first->Assign(src_int.GetFuzz_from());
        }
        if ( src_int.IsSetFuzz_to() ) {
            fuzz.second.Reset(new CInt_fuzz);
            fuzz.second->Assign(src_int.GetFuzz_to());
        }
        bool res = x_MapInterval(src_int.GetId(),
            TRange(src_int.GetFrom(), src_int.GetTo()),
            src_int.IsSetStrand(),
            src_int.IsSetStrand() ? src_int.GetStrand() : eNa_strand_unknown,
            fuzz);
        if ( !res ) {
            if ( m_KeepNonmapping ) {
                x_PushRangesToDstMix();
                CRef<CSeq_loc> loc(new CSeq_loc);
                loc->Assign(src_loc);
                x_PushLocToDstMix(loc);
            }
            else {
                m_Partial = true;
            }
        }
        break;
    }
    case CSeq_loc::e_Pnt:
    {
        const CSeq_point& pnt = src_loc.GetPnt();
        TRangeFuzz fuzz(kEmptyFuzz, kEmptyFuzz);
        if ( pnt.IsSetFuzz() ) {
            fuzz.first.Reset(new CInt_fuzz);
            fuzz.first->Assign(pnt.GetFuzz());
        }
        bool res = x_MapInterval(pnt.GetId(),
            TRange(pnt.GetPoint(), pnt.GetPoint()),
            pnt.IsSetStrand(),
            pnt.IsSetStrand() ? pnt.GetStrand() : eNa_strand_unknown,
            fuzz);
        if ( !res ) {
            if ( m_KeepNonmapping ) {
                x_PushRangesToDstMix();
                CRef<CSeq_loc> loc(new CSeq_loc);
                loc->Assign(src_loc);
                x_PushLocToDstMix(loc);
            }
            else {
                m_Partial = true;
            }
        }
        break;
    }
    case CSeq_loc::e_Packed_int:
    {
        const CPacked_seqint::Tdata& src_ints = src_loc.GetPacked_int().Get();
        ITERATE ( CPacked_seqint::Tdata, i, src_ints ) {
            const CSeq_interval& si = **i;
            TRangeFuzz fuzz(kEmptyFuzz, kEmptyFuzz);
            if ( si.IsSetFuzz_from() ) {
                fuzz.first.Reset(new CInt_fuzz);
                fuzz.first->Assign(si.GetFuzz_from());
            }
            if ( si.IsSetFuzz_to() ) {
                fuzz.second.Reset(new CInt_fuzz);
                fuzz.second->Assign(si.GetFuzz_to());
            }
            bool res = x_MapInterval(si.GetId(),
                TRange(si.GetFrom(), si.GetTo()),
                si.IsSetStrand(),
                si.IsSetStrand() ? si.GetStrand() : eNa_strand_unknown,
                fuzz);
            if ( !res ) {
                if ( m_KeepNonmapping ) {
                    x_PushRangesToDstMix();
                    TRange rg(si.GetFrom(), si.GetTo());
                    x_PushMappedRange(CSeq_id_Handle::GetHandle(si.GetId()),
                        STRAND_TO_INDEX(si.IsSetStrand(), si.GetStrand()),
                        rg, fuzz);
                }
                else {
                    m_Partial = true;
                }
            }
        }
        break;
    }
    case CSeq_loc::e_Packed_pnt:
    {
        const CPacked_seqpnt& src_pack_pnts = src_loc.GetPacked_pnt();
        const CPacked_seqpnt::TPoints& src_pnts = src_pack_pnts.GetPoints();
        ITERATE ( CPacked_seqpnt::TPoints, i, src_pnts ) {
            TRangeFuzz fuzz(kEmptyFuzz, kEmptyFuzz);
            if ( src_pack_pnts.IsSetFuzz() ) {
                fuzz.first.Reset(new CInt_fuzz);
                fuzz.first->Assign(src_pack_pnts.GetFuzz());
            }
            bool res = x_MapInterval(
                src_pack_pnts.GetId(),
                TRange(*i, *i), src_pack_pnts.IsSetStrand(),
                src_pack_pnts.IsSetStrand() ?
                src_pack_pnts.GetStrand() : eNa_strand_unknown,
                fuzz);
            if ( !res ) {
                if ( m_KeepNonmapping ) {
                    x_PushRangesToDstMix();
                    TRange rg(*i, *i);
                    x_PushMappedRange(
                        CSeq_id_Handle::GetHandle(src_pack_pnts.GetId()),
                        STRAND_TO_INDEX(src_pack_pnts.IsSetStrand(),
                                        src_pack_pnts.GetStrand()),
                        rg, fuzz);
                }
                else {
                    m_Partial = true;
                }
            }
        }
        break;
    }
    case CSeq_loc::e_Mix:
    {
        x_PushRangesToDstMix();
        CRef<CSeq_loc> prev = m_Dst_loc;
        m_Dst_loc.Reset();
        const CSeq_loc_mix::Tdata& src_mix = src_loc.GetMix().Get();
        ITERATE ( CSeq_loc_mix::Tdata, i, src_mix ) {
            x_MapSeq_loc(**i);
        }
        x_PushRangesToDstMix();
        CRef<CSeq_loc> mix = m_Dst_loc;
        m_Dst_loc = prev;
        OptimizeSeq_loc(mix);
        x_PushLocToDstMix(mix);
        break;
    }
    case CSeq_loc::e_Equiv:
    {
        x_PushRangesToDstMix();
        CRef<CSeq_loc> prev = m_Dst_loc;
        m_Dst_loc.Reset();
        const CSeq_loc_equiv::Tdata& src_equiv = src_loc.GetEquiv().Get();
        CRef<CSeq_loc> equiv(new CSeq_loc);
        equiv->SetEquiv();
        ITERATE ( CSeq_loc_equiv::Tdata, i, src_equiv ) {
            x_MapSeq_loc(**i);
            x_PushRangesToDstMix();
            OptimizeSeq_loc(m_Dst_loc);
            equiv->SetEquiv().Set().push_back(m_Dst_loc);
            m_Dst_loc.Reset();
        }
        m_Dst_loc = prev;
        x_PushLocToDstMix(equiv);
        break;
    }
    case CSeq_loc::e_Bond:
    {
        x_PushRangesToDstMix();
        CRef<CSeq_loc> prev = m_Dst_loc;
        m_Dst_loc.Reset();
        const CSeq_bond& src_bond = src_loc.GetBond();
        CRef<CSeq_loc> dst_loc(new CSeq_loc);
        CRef<CSeq_loc> pntA;
        CRef<CSeq_loc> pntB;
        TRangeFuzz fuzzA(kEmptyFuzz, kEmptyFuzz);
        if ( src_bond.GetA().IsSetFuzz() ) {
            fuzzA.first.Reset(new CInt_fuzz);
            fuzzA.first->Assign(src_bond.GetA().GetFuzz());
        }
        bool resA = x_MapInterval(src_bond.GetA().GetId(),
            TRange(src_bond.GetA().GetPoint(), src_bond.GetA().GetPoint()),
            src_bond.GetA().IsSetStrand(),
            src_bond.GetA().IsSetStrand() ?
            src_bond.GetA().GetStrand() : eNa_strand_unknown,
            fuzzA);
        if ( resA ) {
            pntA = x_GetMappedSeq_loc();
            _ASSERT(pntA);
        }
        else {
            pntA.Reset(new CSeq_loc);
            pntA->SetPnt().Assign(src_bond.GetA());
        }
        bool resB = false;
        if ( src_bond.IsSetB() ) {
            TRangeFuzz fuzzB(kEmptyFuzz, kEmptyFuzz);
            if ( src_bond.GetB().IsSetFuzz() ) {
                fuzzB.first.Reset(new CInt_fuzz);
                fuzzB.first->Assign(src_bond.GetB().GetFuzz());
            }
            resB = x_MapInterval(src_bond.GetB().GetId(),
                TRange(src_bond.GetB().GetPoint(), src_bond.GetB().GetPoint()),
                src_bond.GetB().IsSetStrand(),
                src_bond.GetB().IsSetStrand() ?
                src_bond.GetB().GetStrand() : eNa_strand_unknown,
                fuzzB);
        }
        if ( resB ) {
            pntB = x_GetMappedSeq_loc();
            _ASSERT(pntB);
        }
        else {
            pntB.Reset(new CSeq_loc);
            pntB->SetPnt().Assign(src_bond.GetB());
        }
        m_Dst_loc = prev;
        if ( resA  ||  resB  ||  m_KeepNonmapping ) {
            if (pntA->IsPnt()  &&  pntB->IsPnt()) {
                // Mapped locations are points - pack into bond
                CSeq_bond& dst_bond = dst_loc->SetBond();
                dst_bond.SetA(pntA->SetPnt());
                if ( src_bond.IsSetB() ) {
                    dst_bond.SetB(pntB->SetPnt());
                }
            }
            else {
                // Convert the whole bond to mix, add gaps between A and B
                CSeq_loc_mix& dst_mix = dst_loc->SetMix();
                if ( pntA ) {
                    dst_mix.Set().push_back(pntA);
                }
                if ( pntB ) {
                    // Add null only if B is set.
                    CRef<CSeq_loc> null_loc(new CSeq_loc);
                    null_loc->SetNull();
                    dst_mix.Set().push_back(null_loc);
                    dst_mix.Set().push_back(pntB);
                }
            }
            x_PushLocToDstMix(dst_loc);
        }
        m_Partial |= (!resA || !resB);
        break;
    }
    default:
        NCBI_THROW(CAnnotMapperException, eBadLocation,
                   "Unsupported location type");
    }
}

CSeq_align_Mapper_Base*
CSeq_loc_Mapper_Base::InitAlignMapper(const CSeq_align& src_align)
{
    return new CSeq_align_Mapper_Base(src_align,
        m_UseWidth ? CSeq_align_Mapper_Base::eWidth_Map
        : CSeq_align_Mapper_Base::eWidth_NoMap);
}


CRef<CSeq_loc> CSeq_loc_Mapper_Base::Map(const CSeq_loc& src_loc)
{
    m_Dst_loc.Reset();
    m_Partial = false; // reset for each location
    m_LastTruncated = false;
    x_MapSeq_loc(src_loc);
    x_PushRangesToDstMix();
    OptimizeSeq_loc(m_Dst_loc);
    return m_Dst_loc;
}


CRef<CSeq_align>
CSeq_loc_Mapper_Base::x_MapSeq_align(const CSeq_align& src_align,
                                     size_t*           row)
{
    m_Dst_loc.Reset();
    m_Partial = false; // reset for each location
    m_LastTruncated = false;
    CRef<CSeq_align_Mapper_Base> aln_mapper(InitAlignMapper(src_align));
    if ( row ) {
        aln_mapper->Convert(*this, *row);
    }
    else {
        aln_mapper->Convert(*this);
    }
    return aln_mapper->GetDstAlign();
}


/////////////////////////////////////////////////////////////////////
//
//   Produce result of the mapping
//


CRef<CSeq_loc> CSeq_loc_Mapper_Base::x_RangeToSeq_loc
(const CSeq_id_Handle& idh,
 TSeqPos               from,
 TSeqPos               to,
 size_t                strand_idx,
 TRangeFuzz            rg_fuzz)
{
    if (m_UseWidth  &&  (m_Widths[idh] & fWidthNucToProt)) {
        from = from/3;
        to = to/3;
    }

    CRef<CSeq_loc> loc(new CSeq_loc);
    // If both fuzzes are set, create interval, not point.
    if (from == to  &&  (!rg_fuzz.first  ||  !rg_fuzz.second)) {
        // point
        loc->SetPnt().SetId().Assign(*idh.GetSeqId());
        loc->SetPnt().SetPoint(from);
        if (strand_idx > 0) {
            loc->SetPnt().SetStrand(INDEX_TO_STRAND(strand_idx));
        }
        if ( rg_fuzz.first ) {
            loc->SetPnt().SetFuzz(*rg_fuzz.first);
        }
        else if ( rg_fuzz.second ) {
            loc->SetPnt().SetFuzz(*rg_fuzz.second);
        }
    }
    else {
        // interval
        loc->SetInt().SetId().Assign(*idh.GetSeqId());
        loc->SetInt().SetFrom(from);
        loc->SetInt().SetTo(to);
        if (strand_idx > 0) {
            loc->SetInt().SetStrand(INDEX_TO_STRAND(strand_idx));
        }
        if ( rg_fuzz.first ) {
            loc->SetInt().SetFuzz_from(*rg_fuzz.first);
        }
        if ( rg_fuzz.second ) {
            loc->SetInt().SetFuzz_to(*rg_fuzz.second);
        }
    }
    return loc;
}


CSeq_loc_Mapper_Base::TMappedRanges&
CSeq_loc_Mapper_Base::x_GetMappedRanges(const CSeq_id_Handle& id,
                                        size_t                strand_idx) const
{
    TRangesByStrand& str_vec = m_MappedLocs[id];
    if (str_vec.size() <= strand_idx) {
        str_vec.resize(strand_idx + 1);
    }
    return str_vec[strand_idx];
}


void CSeq_loc_Mapper_Base::x_PushMappedRange(const CSeq_id_Handle& id,
                                             size_t                strand_idx,
                                             const TRange&         range,
                                             const TRangeFuzz&     fuzz)
{
    bool reverse = (strand_idx > 0) &&
        IsReverse(INDEX_TO_STRAND(strand_idx));
    switch ( m_MergeFlag ) {
    case eMergeContained:
    case eMergeAll:
        {
            if ( reverse ) {
                x_GetMappedRanges(id, strand_idx)
                    .push_front(TRangeWithFuzz(range, fuzz));
            }
            else {
                x_GetMappedRanges(id, strand_idx)
                    .push_back(TRangeWithFuzz(range, fuzz));
            }
            break;
        }
    case eMergeNone:
        {
            x_PushRangesToDstMix();
            if ( reverse ) {
                x_GetMappedRanges(id, strand_idx)
                    .push_front(TRangeWithFuzz(range, fuzz));
            }
            else {
                x_GetMappedRanges(id, strand_idx)
                    .push_back(TRangeWithFuzz(range, fuzz));
            }
            break;
        }
    case eMergeAbutting:
    default:
        {
            TRangesById::iterator it = m_MappedLocs.begin();
            // Start new sub-location for:
            // New ID
            bool no_merge = it == m_MappedLocs.end()  ||  it->first != id;
            // New strand
            no_merge |= it->second.size() <= strand_idx
                ||  it->second.empty();
            // Ranges are not abutting
            if ( !no_merge ) {
                if ( reverse ) {
                    no_merge |= it->second[strand_idx].front().first.GetFrom() !=
                        range.GetToOpen();
                }
                else {
                    no_merge |= it->second[strand_idx].back().first.GetToOpen() !=
                        range.GetFrom();
                }
            }
            if ( no_merge ) {
                x_PushRangesToDstMix();
                if ( reverse ) {
                    x_GetMappedRanges(id, strand_idx)
                        .push_front(TRangeWithFuzz(range, fuzz));
                }
                else {
                    x_GetMappedRanges(id, strand_idx)
                        .push_back(TRangeWithFuzz(range, fuzz));
                }
            }
            else {
                if ( reverse ) {
                    TRangeWithFuzz& last_rg = it->second[strand_idx].front();
                    last_rg.first.SetFrom(range.GetFrom());
                    last_rg.second.first = fuzz.first;
                }
                else {
                    TRangeWithFuzz& last_rg = it->second[strand_idx].back();
                    last_rg.first.SetTo(range.GetTo());
                    last_rg.second.second = fuzz.second;
                }
            }
        }
    }
}


void CSeq_loc_Mapper_Base::x_PushRangesToDstMix(void)
{
    if (m_MappedLocs.size() == 0) {
        return;
    }
    // Push everything already mapped to the destination mix
    CRef<CSeq_loc> loc = x_GetMappedSeq_loc();
    if ( !m_Dst_loc ) {
        m_Dst_loc = loc;
        return;
    }
    if ( !loc->IsNull() ) {
        x_PushLocToDstMix(loc);
    }
}


void CSeq_loc_Mapper_Base::x_PushLocToDstMix(CRef<CSeq_loc> loc)
{
    _ASSERT(loc);
    if ( !m_Dst_loc  ||  !m_Dst_loc->IsMix() ) {
        CRef<CSeq_loc> tmp = m_Dst_loc;
        m_Dst_loc.Reset(new CSeq_loc);
        m_Dst_loc->SetMix();
        if ( tmp ) {
            m_Dst_loc->SetMix().Set().push_back(tmp);
        }
    }
    CSeq_loc_mix::Tdata& mix = m_Dst_loc->SetMix().Set();
    if ( loc->IsNull() ) {
        if ( m_GapFlag == eGapRemove ) {
            return;
        }
        if ( mix.size() > 0  &&  (*mix.rbegin())->IsNull() ) {
            // do not create duplicate NULLs
            return;
        }
    }
    if ( x_ReverseRangeOrder() ) {
        mix.push_front(loc);
    }
    else {
        mix.push_back(loc);
    }
}


CRef<CSeq_loc> CSeq_loc_Mapper_Base::x_GetMappedSeq_loc(void)
{
    CRef<CSeq_loc> dst_loc(new CSeq_loc);
    CSeq_loc_mix::Tdata& dst_mix = dst_loc->SetMix().Set();
    NON_CONST_ITERATE(TRangesById, id_it, m_MappedLocs) {
        if ( !id_it->first ) {
            if (m_GapFlag == eGapPreserve) {
                CRef<CSeq_loc> null_loc(new CSeq_loc);
                null_loc->SetNull();
                dst_mix.push_back(null_loc);
            }
            continue;
        }
        for (int str = 0; str < (int)id_it->second.size(); ++str) {
            if (id_it->second[str].size() == 0) {
                continue;
            }
            TSeqPos from = kInvalidSeqPos;
            TSeqPos to = kInvalidSeqPos;
            TRangeFuzz fuzz(kEmptyFuzz, kEmptyFuzz);
            if (m_MergeFlag == eMergeContained  || m_MergeFlag == eMergeAll) {
                id_it->second[str].sort();
            }
            NON_CONST_ITERATE(TMappedRanges, rg_it, id_it->second[str]) {
                if ( rg_it->first.Empty() ) {
                    // Empty seq-loc
                    CRef<CSeq_loc> loc(new CSeq_loc);
                    loc->SetEmpty().Assign(*id_it->first.GetSeqId());
                    if ( x_ReverseRangeOrder() ) {
                        dst_mix.push_front(loc);
                    }
                    else {
                        dst_mix.push_back(loc);
                    }
                    continue;
                }
                if (to == kInvalidSeqPos) {
                    from = rg_it->first.GetFrom();
                    to = rg_it->first.GetTo();
                    fuzz = rg_it->second;
                    continue;
                }
                if (m_MergeFlag == eMergeAbutting) {
                    if (rg_it->first.GetFrom() == to + 1) {
                        to = rg_it->first.GetTo();
                        fuzz.second = rg_it->second.second;
                        continue;
                    }
                }
                if (m_MergeFlag == eMergeContained) {
                    // Ignore interval completely covered by another one
                    if (rg_it->first.GetTo() <= to) {
                        continue;
                    }
                    if (rg_it->first.GetFrom() == from) {
                        to = rg_it->first.GetTo();
                        fuzz.second = rg_it->second.second;
                        continue;
                    }
                }
                if (m_MergeFlag == eMergeAll) {
                    if (rg_it->first.GetFrom() <= to + 1) {
                        to = rg_it->first.GetTo();
                        fuzz.second = rg_it->second.second;
                        continue;
                    }
                }
                // Add new interval or point
                if ( x_ReverseRangeOrder() ) {
                    dst_mix.push_front(x_RangeToSeq_loc(id_it->first, from, to,
                        str, fuzz));
                }
                else {
                    dst_mix.push_back(x_RangeToSeq_loc(id_it->first, from, to,
                        str, fuzz));
                }
                from = rg_it->first.GetFrom();
                to = rg_it->first.GetTo();
                fuzz = rg_it->second;
            }
            // last interval or point
            if ( x_ReverseRangeOrder() ) {
                dst_mix.push_front(x_RangeToSeq_loc(id_it->first, from, to,
                    str, fuzz));
            }
            else {
                dst_mix.push_back(x_RangeToSeq_loc(id_it->first, from, to,
                    str, fuzz));
            }
        }
    }
    m_MappedLocs.clear();
    OptimizeSeq_loc(dst_loc);
    return dst_loc;
}


END_SCOPE(objects)
END_NCBI_SCOPE
