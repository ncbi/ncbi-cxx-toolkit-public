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
*   Seq-loc mapper
*
*/

#include <objmgr/seq_loc_mapper.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/objmgr_exception.hpp>
#include <objmgr/seq_map.hpp>
#include <objmgr/seq_map_ci.hpp>
#include <objmgr/impl/synonyms.hpp>
#include <objmgr/impl/seq_align_mapper.hpp>
#include <objmgr/impl/seq_loc_cvt.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seqloc/Seq_loc_equiv.hpp>
#include <objects/seqloc/Seq_bond.hpp>
#include <objects/seqalign/seqalign__.hpp>
#include <algorithm>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


CMappingRange::CMappingRange(CSeq_id_Handle     src_id,
                             TSeqPos            src_from,
                             TSeqPos            src_length,
                             ENa_strand         src_strand,
                             const CSeq_id&     dst_id,
                             TSeqPos            dst_from,
                             ENa_strand         dst_strand)
    : m_Src_id_Handle(src_id),
      m_Src_from(src_from),
      m_Src_to(src_from + src_length - 1),
      m_Src_strand(src_strand),
      m_Dst_from(dst_from),
      m_Dst_strand(dst_strand),
      m_Reverse(!SameOrientation(src_strand, dst_strand))
{
    m_Dst_id.Reset(new CSeq_id);
    m_Dst_id->Assign(dst_id);
}


bool CMappingRange::CanMap(TSeqPos from,
                           TSeqPos to,
                           bool is_set_strand,
                           ENa_strand strand) const
{
    if (from > m_Src_to || to < m_Src_from) {
        return false;
    }
    return SameOrientation(is_set_strand ? strand : eNa_strand_unknown,
        m_Src_strand)  ||  (m_Src_strand == eNa_strand_unknown);
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


CMappingRange::TRange CMappingRange::Map_Range(TSeqPos from, TSeqPos to) const
{
    if (!m_Reverse) {
        return TRange(Map_Pos(max(from, m_Src_from)),
            Map_Pos(min(to, m_Src_to)));
    }
    else {
        return TRange(Map_Pos(min(to, m_Src_to)),
            Map_Pos(max(from, m_Src_from)));
    }
}


bool CMappingRange::Map_Strand(bool is_set_strand,
                               ENa_strand src,
                               ENa_strand* dst) const
{
    _ASSERT(dst);
    if (is_set_strand) {
        *dst = m_Reverse ? Reverse(src) : src;
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


CMappingRange::TRangeFuzz CMappingRange::Map_Fuzz(TRangeFuzz& fuzz) const
{
    // Maps some fuzz types to reverse strand
    if ( !m_Reverse ) {
        return fuzz;
    }
    TRangeFuzz res(fuzz.second, fuzz.first);
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
    if ( fuzz.second ) {
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


CSeq_loc_Mapper::CSeq_loc_Mapper(const CSeq_feat&  map_feat,
                                 EFeatMapDirection dir,
                                 CScope*           scope)
    : m_Scope(scope),
      m_MergeFlag(eMergeNone),
      m_GapFlag(eGapPreserve),
      m_KeepNonmapping(false),
      m_UseWidth(false),
      m_Dst_width(0)
{
    _ASSERT(map_feat.IsSetProduct());
    int frame = 0;
    if (map_feat.GetData().IsCdregion()) {
        frame = map_feat.GetData().GetCdregion().GetFrame();
    }
    if (dir == eLocationToProduct) {
        x_Initialize(map_feat.GetLocation(), map_feat.GetProduct(), frame);
    }
    else {
        x_Initialize(map_feat.GetProduct(), map_feat.GetLocation(), frame);
    }
}


CSeq_loc_Mapper::CSeq_loc_Mapper(const CSeq_loc& source,
                                 const CSeq_loc& target,
                                 CScope* scope)
    : m_Scope(scope),
      m_MergeFlag(eMergeNone),
      m_GapFlag(eGapPreserve),
      m_KeepNonmapping(false),
      m_UseWidth(false),
      m_Dst_width(0)
{
    x_Initialize(source, target);
}


CSeq_loc_Mapper::CSeq_loc_Mapper(const CSeq_align& map_align,
                                 const CSeq_id&    to_id,
                                 CScope*           scope)
    : m_Scope(scope),
      m_MergeFlag(eMergeNone),
      m_GapFlag(eGapPreserve),
      m_KeepNonmapping(false),
      m_UseWidth(false),
      m_Dst_width(0)
{
    x_Initialize(map_align, to_id);
}


CSeq_loc_Mapper::CSeq_loc_Mapper(const CSeq_align& map_align,
                                 size_t            to_row,
                                 CScope*           scope)
    : m_Scope(scope),
      m_MergeFlag(eMergeNone),
      m_GapFlag(eGapPreserve),
      m_KeepNonmapping(false),
      m_UseWidth(false),
      m_Dst_width(0)
{
    x_Initialize(map_align, to_row);
}


CSeq_loc_Mapper::CSeq_loc_Mapper(CBioseq_Handle target_seq)
    : m_Scope(&target_seq.GetScope()),
      m_MergeFlag(eMergeNone),
      m_GapFlag(eGapPreserve),
      m_KeepNonmapping(false),
      m_UseWidth(false),
      m_Dst_width(0)
{
    CConstRef<CSeq_id> dst_id = target_seq.GetSeqId();
    x_Initialize(target_seq.GetSeqMap(),
        dst_id.GetPointerOrNull());
    // Ignore seq-map destination ranges, map whole sequence to itself,
    // use unknown strand only.
    m_DstRanges.resize(1);
    m_DstRanges[0].clear();
    if (m_Scope) {
        CConstRef<CSynonymsSet> dst_syns = m_Scope->GetSynonyms(*dst_id);
        ITERATE(CSynonymsSet, dst_syn_it, *dst_syns) {
            m_DstRanges[0][target_seq.GetSeq_id_Handle()]
                .push_back(TRange::GetWhole());
        }
    }
    else {
        m_DstRanges[0][target_seq.GetSeq_id_Handle()]
            .push_back(TRange::GetWhole());
    }
}


CSeq_loc_Mapper::CSeq_loc_Mapper(const CSeqMap& seq_map,
                                 const CSeq_id* dst_id,
                                 CScope*        scope)
    : m_Scope(scope),
      m_MergeFlag(eMergeNone),
      m_GapFlag(eGapPreserve),
      m_KeepNonmapping(false),
      m_UseWidth(false),
      m_Dst_width(0)
{
    x_Initialize(seq_map, dst_id);
}


CSeq_loc_Mapper::CSeq_loc_Mapper(size_t          depth,
                                 CBioseq_Handle& source_seq)
    : m_Scope(&source_seq.GetScope()),
      m_MergeFlag(eMergeNone),
      m_GapFlag(eGapPreserve),
      m_KeepNonmapping(false),
      m_UseWidth(false),
      m_Dst_width(0)
{
    if (depth > 0) {
        depth--;
        x_Initialize(source_seq.GetSeqMap(),
            depth, source_seq.GetSeqId().GetPointer());
    }
    else /* if (depth == 0) */ {
        // Synonyms conversion
        CConstRef<CSeq_id> dst_id = source_seq.GetSeqId();
        m_DstRanges.resize(1);
        if (m_Scope) {
            CConstRef<CSynonymsSet> dst_syns = m_Scope->GetSynonyms(*dst_id);
            ITERATE(CSynonymsSet, dst_syn_it, *dst_syns) {
                m_DstRanges[0][source_seq.GetSeq_id_Handle()]
                    .push_back(TRange::GetWhole());
            }
        }
        else {
            m_DstRanges[0][source_seq.GetSeq_id_Handle()]
                .push_back(TRange::GetWhole());
        }
    }
}


CSeq_loc_Mapper::CSeq_loc_Mapper(size_t         depth,
                                 const CSeqMap& source_seqmap,
                                 const CSeq_id* src_id,
                                 CScope*        scope)
    : m_Scope(scope),
      m_MergeFlag(eMergeNone),
      m_GapFlag(eGapPreserve),
      m_KeepNonmapping(false),
      m_UseWidth(false),
      m_Dst_width(0)
{
    if (depth > 0) {
        depth--;
        x_Initialize(source_seqmap, depth, src_id);
    }
    else /* if (depth == 0) */ {
        // Synonyms conversion
        m_DstRanges.resize(1);
        if (bool(m_Scope)  &&  src_id) {
            CSeq_id_Handle src_idh = CSeq_id_Handle::GetHandle(*src_id);
            CConstRef<CSynonymsSet> dst_syns = m_Scope->GetSynonyms(*src_id);
            ITERATE(CSynonymsSet, dst_syn_it, *dst_syns) {
                m_DstRanges[0][src_idh]
                    .push_back(TRange::GetWhole());
            }
        }
    }
}


CSeq_loc_Mapper::~CSeq_loc_Mapper(void)
{
    return;
}


void CSeq_loc_Mapper::PreserveDestinationLocs(void)
{
    for (size_t str_idx = 0; str_idx < m_DstRanges.size(); str_idx++) {
        NON_CONST_ITERATE(TDstIdMap, id_it, m_DstRanges[str_idx]) {
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
                // Separate ranges, add conversion and restart collecting
                CRef<CMappingRange> cvt(new CMappingRange(
                    id_it->first, dst_start, dst_stop - dst_start + 1,
                    ENa_strand(str_idx),
                    *id_it->first.GetSeqId(), dst_start, ENa_strand(str_idx)));
                m_IdMap[cvt->m_Src_id_Handle].insert(TRangeMap::value_type(
                    TRange(cvt->m_Src_from, cvt->m_Src_to), cvt));
            }
            if (dst_start < dst_stop) {
                CRef<CMappingRange> cvt(new CMappingRange(
                    id_it->first, dst_start, dst_stop - dst_start + 1,
                    ENa_strand(str_idx),
                    *id_it->first.GetSeqId(), dst_start, ENa_strand(str_idx)));
                m_IdMap[cvt->m_Src_id_Handle].insert(TRangeMap::value_type(
                    TRange(cvt->m_Src_from, cvt->m_Src_to), cvt));
            }
        }
    }
    m_DstRanges.clear();
}


inline
CSeq_loc_Mapper::TMappedRanges&
CSeq_loc_Mapper::x_GetMappedRanges(const CSeq_id_Handle& id,
                                   int strand_idx) const
{
    TRangesByStrand& str_vec = m_MappedLocs[id];
    if ((int)str_vec.size() <= strand_idx) {
        str_vec.resize(strand_idx + 1);
    }
    return str_vec[strand_idx];
}


inline
void CSeq_loc_Mapper::x_PushRangesToDstMix(void)
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


int CSeq_loc_Mapper::x_CheckSeqWidth(const CSeq_id& id, int width)
{
    if (!m_Scope) {
        return width;
    }
    CBioseq_Handle handle;
    try {
        handle = m_Scope->GetBioseqHandle(id);
    } catch (...) {
        return width;
    }
    if ( !handle ) {
        return width;
    }
    switch ( handle.GetBioseqMolType() ) {
    case CSeq_inst::eMol_dna:
    case CSeq_inst::eMol_rna:
    case CSeq_inst::eMol_na:
        {
            if ( width != 3 ) {
                if ( width ) {
                    // width already set to a different value
                    NCBI_THROW(CLocMapperException, eBadLocation,
                               "Location contains different sequence types");
                }
                width = 3;
            }
            break;
        }
    case CSeq_inst::eMol_aa:
        {
            if ( width != 1 ) {
                if ( width ) {
                    // width already set to a different value
                    NCBI_THROW(CLocMapperException, eBadLocation,
                               "Location contains different sequence types");
                }
                width = 1;
            }
            break;
        }
    default:
        return width;
    }
    // Destination width should always be checked first
    if ( !m_Dst_width ) {
        m_Dst_width = width;
    }
    TWidthFlags wid_cvt = 0;
    if (width == 1) {
        wid_cvt |= fWidthProtToNuc;
    }
    if (m_Dst_width == 1) {
        wid_cvt |= fWidthNucToProt;
    }
    CConstRef<CSynonymsSet> syns = m_Scope->GetSynonyms(id);
    ITERATE(CSynonymsSet, syn_it, *syns) {
        m_Widths[syns->GetSeq_id_Handle(syn_it)] = wid_cvt;
    }
    return width;
}


int CSeq_loc_Mapper::x_CheckSeqWidth(const CSeq_loc& loc,
                                     TSeqPos* total_length)
{
    int width = 0;
    *total_length = 0;
    for (CSeq_loc_CI it(loc); it; ++it) {
        *total_length += it.GetRange().GetLength();
        if ( !m_Scope ) {
            continue; // don't check sequence type;
        }
        width = x_CheckSeqWidth(it.GetSeq_id(), width);
    }
    return width;
}


TSeqPos CSeq_loc_Mapper::x_GetRangeLength(const CSeq_loc_CI& it)
{
    if (it.IsWhole()  &&  IsReverse(it.GetStrand())) {
        // For reverse strand we need real interval length, not just "whole"
        CBioseq_Handle h;
        if (m_Scope) {
            h = m_Scope->GetBioseqHandle(it.GetSeq_id());
        }
        if ( !h ) {
            NCBI_THROW(CLocMapperException, eUnknownLength,
                       "Can not map from minus strand -- "
                       "unknown sequence length");
        }
        return h.GetBioseqLength();
    }
    else {
        return it.GetRange().GetLength();
    }
}


void CSeq_loc_Mapper::x_AddConversion(const CSeq_id& src_id,
                                      TSeqPos        src_start,
                                      ENa_strand     src_strand,
                                      const CSeq_id& dst_id,
                                      TSeqPos        dst_start,
                                      ENa_strand     dst_strand,
                                      TSeqPos        length)
{
    if (m_DstRanges.size() <= size_t(dst_strand)) {
        m_DstRanges.resize(size_t(dst_strand) + 1);
    }
    if (m_Scope) {
        CConstRef<CSynonymsSet> syns = m_Scope->GetSynonyms(src_id);
        ITERATE(CSynonymsSet, syn_it, *syns) {
            CRef<CMappingRange> cvt(new CMappingRange(
                CSynonymsSet::GetSeq_id_Handle(syn_it),
                src_start, length, src_strand,
                dst_id, dst_start, dst_strand));
            m_IdMap[cvt->m_Src_id_Handle].insert(TRangeMap::value_type(
                TRange(cvt->m_Src_from, cvt->m_Src_to), cvt));
        }
        CConstRef<CSynonymsSet> dst_syns = m_Scope->GetSynonyms(dst_id);
        ITERATE(CSynonymsSet, dst_syn_it, *dst_syns) {
            m_DstRanges[size_t(dst_strand)]
                [CSynonymsSet::GetSeq_id_Handle(dst_syn_it)]
                .push_back(TRange(dst_start, dst_start + length - 1));
        }
    }
    else {
        CRef<CMappingRange> cvt(new CMappingRange(
            CSeq_id_Handle::GetHandle(src_id),
            src_start, length, src_strand,
            dst_id, dst_start, dst_strand));
        m_IdMap[cvt->m_Src_id_Handle].insert(TRangeMap::value_type(
            TRange(cvt->m_Src_from, cvt->m_Src_to), cvt));
        m_DstRanges[size_t(dst_strand)][CSeq_id_Handle::GetHandle(dst_id)]
            .push_back(TRange(dst_start, dst_start + length - 1));
    }
}


void CSeq_loc_Mapper::x_NextMappingRange(const CSeq_id& src_id,
                                         TSeqPos&       src_start,
                                         TSeqPos&       src_len,
                                         ENa_strand     src_strand,
                                         const CSeq_id& dst_id,
                                         TSeqPos&       dst_start,
                                         TSeqPos&       dst_len,
                                         ENa_strand     dst_strand)
{
    TSeqPos cvt_src_start = src_start;
    TSeqPos cvt_dst_start = dst_start;
    TSeqPos cvt_length;

    if (src_len == dst_len) {
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
        src_len -= cvt_length;
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
        dst_len -= cvt_length;
        src_len = 0;
    }
    x_AddConversion(
        src_id, cvt_src_start, src_strand,
        dst_id, cvt_dst_start, dst_strand,
        cvt_length);
}


void CSeq_loc_Mapper::x_Initialize(const CSeq_loc& source,
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
            NCBI_THROW(CLocMapperException, eBadLocation,
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
            NCBI_THROW(CAnnotException, eBadLocation,
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
            dst_it.GetSeq_id(), dst_start, dst_len, dst_it.GetStrand());
        if (src_len == 0  &&  ++src_it) {
            src_start = src_it.GetRange().GetFrom()*m_Dst_width;
            src_len = x_GetRangeLength(src_it)*m_Dst_width;
        }
        if (dst_len == 0  &&  ++dst_it) {
            dst_start = dst_it.GetRange().GetFrom()*src_width;
            dst_len = x_GetRangeLength(dst_it)*src_width;
        }
    }
}


void CSeq_loc_Mapper::x_Initialize(const CSeq_align& map_align,
                                   const CSeq_id&    to_id)
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
                    NCBI_THROW(CLocMapperException, eBadAlignment,
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
                NCBI_THROW(CLocMapperException, eBadAlignment,
                           "Target ID not found in the alignment");
            }
            x_InitAlign(dseg, to_row);
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
                    NCBI_THROW(CLocMapperException, eBadAlignment,
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
                NCBI_THROW(CLocMapperException, eBadAlignment,
                           "Target ID not found in the alignment");
            }
            x_InitAlign(pseg, to_row);
            break;
        }
    case CSeq_align::C_Segs::e_Disc:
        {
            const CSeq_align_set& aln_set = map_align.GetSegs().GetDisc();
            ITERATE(CSeq_align_set::Tdata, aln, aln_set.Get()) {
                x_Initialize(**aln, to_id);
            }
            break;
        }
    default:
        NCBI_THROW(CLocMapperException, eBadAlignment,
                   "Unsupported alignment type");
    }
}


void CSeq_loc_Mapper::x_Initialize(const CSeq_align& map_align,
                                   size_t            to_row)
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
            x_InitAlign(dseg, to_row);
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
                x_Initialize(**aln, to_row);
            }
            break;
        }
    default:
        NCBI_THROW(CLocMapperException, eBadAlignment,
                   "Unsupported alignment type");
    }
}


void CSeq_loc_Mapper::x_InitAlign(const CDense_diag& diag, size_t to_row)
{
    if (diag.GetStarts()[to_row] < 0) {
        // Destination ID is not present in this dense-diag
        return;
    }
    size_t dim = diag.GetDim();
    _ASSERT(to_row < dim);
    if (dim != diag.GetIds().size()) {
        ERR_POST(Warning << "Invalid 'ids' size in dendiag");
        dim = min(dim, diag.GetIds().size());
    }
    if (dim != diag.GetStarts().size()) {
        ERR_POST(Warning << "Invalid 'starts' size in dendiag");
        dim = min(dim, diag.GetStarts().size());
    }
    bool have_strands = diag.IsSetStrands();
    if (have_strands && dim != diag.GetStrands().size()) {
        ERR_POST(Warning << "Invalid 'strands' size in dendiag");
        dim = min(dim, diag.GetStrands().size());
    }

    ENa_strand dst_strand = have_strands ?
        diag.GetStrands()[to_row] : eNa_strand_unknown;
    const CSeq_id& dst_id = *diag.GetIds()[to_row];
    if (!m_Dst_width) {
        m_Dst_width = x_CheckSeqWidth(dst_id, m_Dst_width);
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
        src_width = x_CheckSeqWidth(src_id, src_width);
        m_UseWidth |= (src_width != m_Dst_width);
        int dst_width_rel = (m_UseWidth) ? 1 : m_Dst_width;
        int src_width_rel = (m_UseWidth) ? 1 : src_width;
        TSeqPos src_start = diag.GetStarts()[row]*dst_width_rel;
        TSeqPos src_len = diag.GetLen()*src_width*dst_width_rel;
        TSeqPos dst_start = diag.GetStarts()[to_row]*src_width_rel;
        TSeqPos dst_len = diag.GetLen()*m_Dst_width*src_width_rel;
        ENa_strand src_strand = have_strands ?
            diag.GetStrands()[row] : eNa_strand_unknown;
        x_NextMappingRange(
            src_id, src_start, src_len, src_strand,
            dst_id, dst_start, dst_len, dst_strand);
        _ASSERT(!src_len  &&  !dst_len);
    }
}


void CSeq_loc_Mapper::x_InitAlign(const CDense_seg& denseg, size_t to_row)
{
    size_t dim = denseg.GetDim();
    _ASSERT(to_row < dim);

    size_t numseg = denseg.GetNumseg();
    // claimed dimension may not be accurate :-/
    if (numseg != denseg.GetLens().size()) {
        ERR_POST(Warning << "Invalid 'lens' size in denseg");
        numseg = min(numseg, denseg.GetLens().size());
    }
    if (dim != denseg.GetIds().size()) {
        ERR_POST(Warning << "Invalid 'ids' size in denseg");
        dim = min(dim, denseg.GetIds().size());
    }
    if (dim*numseg != denseg.GetStarts().size()) {
        ERR_POST(Warning << "Invalid 'starts' size in denseg");
        dim = min(dim*numseg, denseg.GetStarts().size()) / numseg;
    }
    bool have_strands = denseg.IsSetStrands();
    if (have_strands && dim*numseg != denseg.GetStrands().size()) {
        ERR_POST(Warning << "Invalid 'strands' size in denseg");
        dim = min(dim*numseg, denseg.GetStrands().size()) / numseg;
    }

    const CSeq_id& dst_id = *denseg.GetIds()[to_row];

    if (!m_Dst_width) {
        if ( denseg.IsSetWidths() ) {
            m_Dst_width = denseg.GetWidths()[to_row];
        }
        else {
            m_Dst_width = x_CheckSeqWidth(dst_id, m_Dst_width);
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
            src_width = x_CheckSeqWidth(src_id, src_width);
        }
        m_UseWidth |= (src_width != m_Dst_width);
        int dst_width_rel = (m_UseWidth) ? 1 : m_Dst_width;
        int src_width_rel = (m_UseWidth) ? 1 : src_width;

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
            TSeqPos src_len = denseg.GetLens()[seg]*src_width*dst_width_rel;
            TSeqPos dst_len = denseg.GetLens()[seg]*m_Dst_width*src_width_rel;
            TSeqPos src_start = (TSeqPos)(i_src_start)*dst_width_rel;
            TSeqPos dst_start = (TSeqPos)(i_dst_start)*src_width_rel;
            x_NextMappingRange(
                src_id, src_start, src_len, src_strand,
                dst_id, dst_start, dst_len, dst_strand);
            _ASSERT(!src_len  &&  !dst_len);
        }
    }
}


void CSeq_loc_Mapper::x_InitAlign(const CStd_seg& sseg, size_t to_row)
{
    size_t dim = sseg.GetDim();
    if (dim != sseg.GetLoc().size()) {
        ERR_POST(Warning << "Invalid 'loc' size in std-seg");
        dim = min(dim, sseg.GetLoc().size());
    }
    if (sseg.IsSetIds()
        && dim != sseg.GetIds().size()) {
        ERR_POST(Warning << "Invalid 'ids' size in std-seg");
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
        x_Initialize(src_loc, dst_loc);
    }
}


void CSeq_loc_Mapper::x_InitAlign(const CPacked_seg& pseg, size_t to_row)
{
    size_t dim    = pseg.GetDim();
    size_t numseg = pseg.GetNumseg();
    // claimed dimension may not be accurate :-/
    if (numseg != pseg.GetLens().size()) {
        ERR_POST(Warning << "Invalid 'lens' size in packed-seg");
        numseg = min(numseg, pseg.GetLens().size());
    }
    if (dim != pseg.GetIds().size()) {
        ERR_POST(Warning << "Invalid 'ids' size in packed-seg");
        dim = min(dim, pseg.GetIds().size());
    }
    if (dim*numseg != pseg.GetStarts().size()) {
        ERR_POST(Warning << "Invalid 'starts' size in packed-seg");
        dim = min(dim*numseg, pseg.GetStarts().size()) / numseg;
    }
    bool have_strands = pseg.IsSetStrands();
    if (have_strands && dim*numseg != pseg.GetStrands().size()) {
        ERR_POST(Warning << "Invalid 'strands' size in packed-seg");
        dim = min(dim*numseg, pseg.GetStrands().size()) / numseg;
    }

    const CSeq_id& dst_id = *pseg.GetIds()[to_row];
    if (!m_Dst_width) {
        m_Dst_width = x_CheckSeqWidth(dst_id, m_Dst_width);
    }
    for (size_t row = 0; row < dim; ++row) {
        if (row == to_row) {
            continue;
        }
        const CSeq_id& src_id = *pseg.GetIds()[row];

        int src_width = 0;
        src_width = x_CheckSeqWidth(src_id, src_width);
        m_UseWidth |= (src_width != m_Dst_width);
        int dst_width_rel = (m_UseWidth) ? 1 : m_Dst_width;
        int src_width_rel = (m_UseWidth) ? 1 : src_width;

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
            TSeqPos dst_len = pseg.GetLens()[seg]*m_Dst_width*src_width_rel;
            TSeqPos src_start = pseg.GetStarts()[seg*dim + row]*dst_width_rel;
            TSeqPos dst_start = pseg.GetStarts()[seg*dim + to_row]*src_width_rel;
            x_NextMappingRange(
                src_id, src_start, src_len, src_strand,
                dst_id, dst_start, dst_len, dst_strand);
            _ASSERT(!src_len  &&  !dst_len);
        }
    }
}


void CSeq_loc_Mapper::x_Initialize(const CSeqMap& seq_map,
                                   const CSeq_id* top_id)
{
    CSeqMap::const_iterator seg_it =
        seq_map.begin_resolved(m_Scope.GetPointerOrNull(), size_t(-1),
        CSeqMap::fFindRef);

    TSeqPos top_start = kInvalidSeqPos;
    TSeqPos top_stop = kInvalidSeqPos;
    TSeqPos dst_seg_start = kInvalidSeqPos;
    CConstRef<CSeq_id> dst_id;

    for ( ; seg_it; ++seg_it) {
        _ASSERT(seg_it.GetType() == CSeqMap::eSeqRef);
        if (seg_it.GetPosition() > top_stop  ||  !dst_id) {
            // New top-level segment
            top_start = seg_it.GetPosition();
            top_stop = seg_it.GetEndPosition() - 1;
            if (top_id) {
                // Top level is a bioseq
                dst_id.Reset(top_id);
                dst_seg_start = top_start;
            }
            else {
                // Top level is a seq-loc, positions are
                // on the first-level references
                dst_id = seg_it.GetRefSeqid().GetSeqId();
                dst_seg_start = seg_it.GetRefPosition();
                continue;
            }
        }
        // when top_id is set, destination position = GetPosition(),
        // else it needs to be calculated from top_start/stop and dst_start/stop.
        TSeqPos dst_from = dst_seg_start + seg_it.GetPosition() - top_start;
        _ASSERT(dst_from >= dst_seg_start);
        TSeqPos dst_len = seg_it.GetLength();
        CConstRef<CSeq_id> src_id(seg_it.GetRefSeqid().GetSeqId());
        TSeqPos src_from = seg_it.GetRefPosition();
        TSeqPos src_len = dst_len;
        ENa_strand src_strand = seg_it.GetRefMinusStrand() ?
            eNa_strand_minus : eNa_strand_unknown;
        x_NextMappingRange(*src_id, src_from, src_len, src_strand,
            *dst_id, dst_from, dst_len, eNa_strand_unknown);
        _ASSERT(src_len == 0  &&  dst_len == 0);
    }
}


void CSeq_loc_Mapper::x_Initialize(const CSeqMap& seq_map,
                                   size_t         depth,
                                   const CSeq_id* top_id)
{
    CSeqMap::const_iterator seg_it =
        seq_map.begin_resolved(m_Scope.GetPointerOrNull(), depth,
        CSeqMap::fFindRef);

    TSeqPos top_start = kInvalidSeqPos;
    TSeqPos top_stop = kInvalidSeqPos;
    TSeqPos src_seg_start = kInvalidSeqPos;
    TSeqPos last_stop = kInvalidSeqPos;
    CConstRef<CSeq_id> src_id;

    TSeqPos src_from = 0;
    TSeqPos src_len = 0;
    TSeqPos dst_from = 0;
    TSeqPos dst_len = 0;
    CConstRef<CSeq_id> dst_id;
    ENa_strand dst_strand = eNa_strand_unknown;

    for ( ; seg_it; ++seg_it) {
        _ASSERT(seg_it.GetType() == CSeqMap::eSeqRef);
        if (seg_it.GetPosition() > top_stop  ||  !src_id) {
            // New top-level segment
            top_start = seg_it.GetPosition();
            top_stop = seg_it.GetEndPosition() - 1;
            if (top_id) {
                // Top level is a bioseq
                src_id.Reset(top_id);
                src_seg_start = top_start;
            }
            else {
                // Top level is a seq-loc, positions are
                // on the first-level references
                src_id = seg_it.GetRefSeqid().GetSeqId();
                src_seg_start = seg_it.GetRefPosition();
                continue;
            }
        }
        if (seg_it.GetPosition() >= last_stop) {
            x_NextMappingRange(*src_id, src_from, src_len, eNa_strand_unknown,
                *dst_id, dst_from, dst_len, dst_strand);
            _ASSERT(src_len == 0  &&  dst_len == 0);
        }
        src_from = src_seg_start + seg_it.GetPosition() - top_start;
        src_len = seg_it.GetLength();
        dst_from = seg_it.GetRefPosition();
        dst_len = src_len;
        dst_id.Reset(seg_it.GetRefSeqid().GetSeqId());
        dst_strand = seg_it.GetRefMinusStrand() ?
            eNa_strand_minus : eNa_strand_unknown;
        last_stop = seg_it.GetEndPosition();
    }
    if (src_len > 0) {
        x_NextMappingRange(*src_id, src_from, src_len, eNa_strand_unknown,
            *dst_id, dst_from, dst_len, dst_strand);
    }
}


CSeq_loc_Mapper::TRangeIterator
CSeq_loc_Mapper::x_BeginMappingRanges(CSeq_id_Handle id,
                                      TSeqPos from,
                                      TSeqPos to)
{
    TIdMap::iterator ranges = m_IdMap.find(id);
    if (ranges == m_IdMap.end()) {
        return TRangeIterator();
    }
    return ranges->second.begin(TRange(from, to));
}


struct CMappingRangeRef_Less
{
    bool operator()(const CRef<CMappingRange>& x,
                    const CRef<CMappingRange>& y) const;
};


bool CMappingRangeRef_Less::operator()(const CRef<CMappingRange>& x,
                                       const CRef<CMappingRange>& y) const
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


bool CSeq_loc_Mapper::x_MapInterval(const CSeq_id&   src_id,
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
    typedef vector< CRef<CMappingRange> > TSortedMappings;
    TSortedMappings mappings;
    TRangeIterator rg_it = x_BeginMappingRanges(
        src_idh, src_rg.GetFrom(), src_rg.GetTo());
    for ( ; rg_it; ++rg_it) {
        mappings.push_back(rg_it->second);
    }
    sort(mappings.begin(), mappings.end(), CMappingRangeRef_Less());
    TSeqPos last_src_to = 0;
    for (size_t idx = 0; idx < mappings.size(); ++idx) {
        const CMappingRange& cvt = *mappings[idx];
        if ( !cvt.CanMap(src_rg.GetFrom(), src_rg.GetTo(),
            is_set_strand, src_strand) ) {
            continue;
        }
        TSeqPos from = src_rg.GetFrom();
        TSeqPos to = src_rg.GetTo();
        TRangeFuzz fuzz = cvt.Map_Fuzz(orig_fuzz);
        if (from < cvt.m_Src_from) {
            from = cvt.m_Src_from;
            // Set partial only if a non-empty range is to be skipped
            if (cvt.m_Src_from > last_src_to + 1) {
                if ( cvt.m_Reverse ) {
                    if (!fuzz.second) {
                        fuzz.second.Reset(new CInt_fuzz);
                    }
                    fuzz.second->SetLim(CInt_fuzz::eLim_gt);
                }
                else {
                    if (!fuzz.first) {
                        fuzz.first.Reset(new CInt_fuzz);
                    }
                    fuzz.first->SetLim(CInt_fuzz::eLim_lt);
                }
                m_Partial = true;
            }
        }
        last_src_to = cvt.m_Src_to;
        if ( to > cvt.m_Src_to ) {
            to = cvt.m_Src_to;
            // Set partial only if a non-empty range is to be skipped
            if (idx == mappings.size() - 1 ||
                mappings[idx+1]->m_Src_from > last_src_to + 1) {
                if ( cvt.m_Reverse ) {
                    if (!fuzz.first) {
                        fuzz.first.Reset(new CInt_fuzz);
                    }
                    fuzz.first->SetLim(CInt_fuzz::eLim_lt);
                }
                else {
                    if (!fuzz.second) {
                        fuzz.second.Reset(new CInt_fuzz);
                    }
                    fuzz.second->SetLim(CInt_fuzz::eLim_gt);
                }
                m_Partial = true;
            }
        }
        if ( from > to ) {
            continue;
        }
        TRange rg = cvt.Map_Range(from, to);
        ENa_strand dst_strand;
        bool is_set_dst_strand = cvt.Map_Strand(is_set_strand,
            src_strand, &dst_strand);
        x_GetMappedRanges(CSeq_id_Handle::GetHandle(*cvt.m_Dst_id),
            is_set_dst_strand ? int(dst_strand) + 1 : 0)
            .push_back(TRangeWithFuzz(rg, fuzz));
        res = true;
    }
    return res;
}


void CSeq_loc_Mapper::x_PushLocToDstMix(CRef<CSeq_loc> loc)
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
    mix.push_back(loc);
}


CRef<CSeq_loc> CSeq_loc_Mapper::Map(const CSeq_loc& src_loc)
{
    m_Dst_loc.Reset();
    m_Partial = false; // reset for each location
    x_MapSeq_loc(src_loc);
    x_PushRangesToDstMix();
    x_OptimizeSeq_loc(m_Dst_loc);
    return m_Dst_loc;
}


CRef<CSeq_align> CSeq_loc_Mapper::Map(const CSeq_align& src_align)
{
    m_Dst_loc.Reset();
    m_Partial = false; // reset for each location
    return x_MapSeq_align(src_align);
}


void CSeq_loc_Mapper::x_MapSeq_loc(const CSeq_loc& src_loc)
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
        TRangeIterator mit = x_BeginMappingRanges(
            CSeq_id_Handle::GetHandle(src_loc.GetEmpty()),
            TRange::GetWhole().GetFrom(),
            TRange::GetWhole().GetTo());
        for ( ; mit; ++mit) {
            CMappingRange& cvt = *mit->second;
            if ( cvt.GoodSrcId(src_loc.GetEmpty()) ) {
                x_GetMappedRanges(
                    CSeq_id_Handle::GetHandle(src_loc.GetEmpty()), 0)
                    .push_back(TRangeWithFuzz(TRange::GetEmpty(),
                    TRangeFuzz(kEmptyFuzz, kEmptyFuzz)));
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
        TSeqPos src_to = kInvalidSeqPos;
        if ( m_Scope ) {
            CBioseq_Handle bh = m_Scope->GetBioseqHandle(src_id);
            src_to = bh ? bh.GetBioseqLength() : kInvalidSeqPos;
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
                    x_GetMappedRanges(CSeq_id_Handle::GetHandle(si.GetId()),
                        si.IsSetStrand() ? int(si.GetStrand()) + 1 : 0)
                        .push_back(TRangeWithFuzz(
                        TRange(si.GetFrom(), si.GetTo()), fuzz));
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
                    x_GetMappedRanges(
                        CSeq_id_Handle::GetHandle(src_pack_pnts.GetId()),
                        src_pack_pnts.IsSetStrand() ?
                        int(src_pack_pnts.GetStrand()) + 1 : 0)
                        .push_back(TRangeWithFuzz(TRange(*i, *i), fuzz));
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
        x_OptimizeSeq_loc(mix);
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
            x_OptimizeSeq_loc(m_Dst_loc);
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
        dst_loc->SetBond();
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
            if ( !pntA->IsPnt() ) {
                NCBI_THROW(CLocMapperException, eBadLocation,
                           "Bond point A can not be mapped to a point");
            }
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
            if ( !pntB->IsPnt() ) {
                NCBI_THROW(CLocMapperException, eBadLocation,
                           "Bond point B can not be mapped to a point");
            }
        }
        else {
            pntB.Reset(new CSeq_loc);
            pntB->SetPnt().Assign(src_bond.GetB());
        }
        m_Dst_loc = prev;
        if ( resA  ||  resB  ||  m_KeepNonmapping ) {
            CSeq_bond& dst_bond = dst_loc->SetBond();
            dst_bond.SetA(pntA->SetPnt());
            if ( src_bond.IsSetB() ) {
                dst_bond.SetB(pntB->SetPnt());
            }
            x_PushLocToDstMix(dst_loc);
        }
        m_Partial |= (!resA || !resB);
        break;
    }
    default:
        NCBI_THROW(CAnnotException, eBadLocation,
                   "Unsupported location type");
    }
}


CRef<CSeq_loc> CSeq_loc_Mapper::x_RangeToSeq_loc(const CSeq_id_Handle& idh,
                                                 TSeqPos from,
                                                 TSeqPos to,
                                                 int strand_idx,
                                                 TRangeFuzz rg_fuzz)
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
            loc->SetPnt().SetStrand(ENa_strand(strand_idx - 1));
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
            loc->SetInt().SetStrand(ENa_strand(strand_idx - 1));
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


void CSeq_loc_Mapper::x_OptimizeSeq_loc(CRef<CSeq_loc>& loc)
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
            break;
        }
        break;
    }
    default:
        NCBI_THROW(CAnnotException, eBadLocation,
                   "Unsupported location type");
    }
}


CRef<CSeq_loc> CSeq_loc_Mapper::x_GetMappedSeq_loc(void)
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
            id_it->second[str].sort();
            NON_CONST_ITERATE(TMappedRanges, rg_it, id_it->second[str]) {
                if ( rg_it->first.Empty() ) {
                    // Empty seq-loc
                    CRef<CSeq_loc> loc(new CSeq_loc);
                    loc->SetEmpty().Assign(*id_it->first.GetSeqId());
                    dst_mix.push_back(loc);
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
                if (m_MergeFlag == eMergeAll) {
                    if (rg_it->first.GetFrom() <= to + 1) {
                        to = rg_it->first.GetTo();
                        fuzz.second = rg_it->second.second;
                        continue;
                    }
                }
                // Add new interval or point
                dst_mix.push_back(x_RangeToSeq_loc(id_it->first, from, to,
                    str, fuzz));
                from = rg_it->first.GetFrom();
                to = rg_it->first.GetTo();
                fuzz = rg_it->second;
            }
            // last interval or point
            dst_mix.push_back(x_RangeToSeq_loc(id_it->first, from, to,
                str, fuzz));
        }
    }
    m_MappedLocs.clear();
    x_OptimizeSeq_loc(dst_loc);
    return dst_loc;
}


CRef<CSeq_align> CSeq_loc_Mapper::x_MapSeq_align(const CSeq_align& src_align)
{
    CSeq_align_Mapper aln_mapper(src_align);
    ITERATE(TIdMap, id_it, m_IdMap) {
        ITERATE(TRangeMap, rg_it, id_it->second) {
            aln_mapper.Convert(*rg_it->second,
                m_UseWidth ? m_Widths[id_it->first] : 0);
        }
    }
    return aln_mapper.GetDstAlign();
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.17  2004/05/10 18:26:37  grichenk
* Fixed 'not used' warnings
*
* Revision 1.16  2004/05/07 13:53:18  grichenk
* Preserve fuzz from original location.
* Better detection of partial locations.
*
* Revision 1.15  2004/05/05 14:04:22  grichenk
* Use fuzz to indicate truncated intervals. Added KeepNonmapping flag.
*
* Revision 1.14  2004/04/23 15:34:49  grichenk
* Added PreserveDestinationLocs().
*
* Revision 1.13  2004/04/12 14:35:59  grichenk
* Fixed mapping of alignments between nucleotides and proteins
*
* Revision 1.12  2004/04/06 13:56:33  grichenk
* Added possibility to remove gaps (NULLs) from mapped location
*
* Revision 1.11  2004/03/31 20:44:26  grichenk
* Fixed warnings about unhandled cases in switch.
*
* Revision 1.10  2004/03/30 21:21:09  grichenk
* Reduced number of includes.
*
* Revision 1.9  2004/03/30 17:00:00  grichenk
* Fixed warnings, moved inline functions to hpp.
*
* Revision 1.8  2004/03/30 15:42:34  grichenk
* Moved alignment mapper to separate file, added alignment mapping
* to CSeq_loc_Mapper.
*
* Revision 1.7  2004/03/29 15:13:56  grichenk
* Added mapping down to segments of a bioseq or a seqmap.
* Fixed: preserve ranges already on the target bioseq(s).
*
* Revision 1.6  2004/03/22 21:10:58  grichenk
* Added mapping from segments to master sequence or through a seq-map.
*
* Revision 1.5  2004/03/19 14:19:08  grichenk
* Added seq-loc mapping through a seq-align.
*
* Revision 1.4  2004/03/16 15:47:28  vasilche
* Added CBioseq_set_Handle and set of EditHandles
*
* Revision 1.3  2004/03/11 18:48:02  shomrat
* skip if empty
*
* Revision 1.2  2004/03/11 04:54:48  grichenk
* Removed inline
*
* Revision 1.1  2004/03/10 16:22:29  grichenk
* Initial revision
*
*
* ===========================================================================
*/
