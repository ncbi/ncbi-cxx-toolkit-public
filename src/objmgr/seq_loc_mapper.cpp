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

#include <ncbi_pch.hpp>
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

/////////////////////////////////////////////////////////////////////
//
// CSeq_loc_Mapper
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


CSeq_loc_Mapper::CSeq_loc_Mapper(CMappingRanges* mapping_ranges,
                                 CScope*         scope)
    : CSeq_loc_Mapper_Base(mapping_ranges),
      m_Scope(scope)
{
}


CSeq_loc_Mapper::CSeq_loc_Mapper(const CSeq_feat&  map_feat,
                                 EFeatMapDirection dir,
                                 CScope*           scope)
    : m_Scope(scope)
{
    x_InitializeFeat(map_feat, dir);
}


CSeq_loc_Mapper::CSeq_loc_Mapper(const CSeq_loc& source,
                                 const CSeq_loc& target,
                                 CScope* scope)
    : m_Scope(scope)
{
    x_InitializeLocs(source, target);
}


CSeq_loc_Mapper::CSeq_loc_Mapper(const CSeq_align& map_align,
                                 const CSeq_id&    to_id,
                                 CScope*           scope,
                                 TMapOptions       opts)
    : m_Scope(scope)
{
    x_InitializeAlign(map_align, to_id, opts);
}


CSeq_loc_Mapper::CSeq_loc_Mapper(const CSeq_align& map_align,
                                 size_t            to_row,
                                 CScope*           scope,
                                 TMapOptions       opts)
    : m_Scope(scope)
{
    x_InitializeAlign(map_align, to_row, opts);
}


CSeq_loc_Mapper::CSeq_loc_Mapper(CBioseq_Handle top_level_seq,
                                 ESeqMapDirection direction)
    : m_Scope(&top_level_seq.GetScope())
{
    CConstRef<CSeq_id> top_level_id = top_level_seq.GetSeqId();
    if ( !top_level_id ) {
        // Bioseq handle has no id, try to get one.
        CConstRef<CSynonymsSet> syns = top_level_seq.GetSynonyms();
        if ( !syns->empty() ) {
            top_level_id = syns->GetSeq_id_Handle(syns->begin()).GetSeqId();
        }
    }
    x_InitializeBioseq(top_level_seq,
                       top_level_id.GetPointerOrNull(),
                       direction);
    if (direction == eSeqMap_Up) {
        // Ignore seq-map destination ranges, map whole sequence to itself,
        // use unknown strand only.
        m_DstRanges.resize(1);
        m_DstRanges[0].clear();
        m_DstRanges[0][CSeq_id_Handle::GetHandle(*top_level_id)]
            .push_back(TRange::GetWhole());
    }
    x_PreserveDestinationLocs();
}


CSeq_loc_Mapper::CSeq_loc_Mapper(const CSeqMap&   seq_map,
                                 ESeqMapDirection direction,
                                 const CSeq_id*   top_level_id,
                                 CScope*          scope)
    : m_Scope(scope)
{
    x_InitializeSeqMap(seq_map, top_level_id, direction);
    x_PreserveDestinationLocs();
}


CSeq_loc_Mapper::CSeq_loc_Mapper(size_t                 depth,
                                 const CBioseq_Handle&  top_level_seq,
                                 ESeqMapDirection       direction)
    : m_Scope(&top_level_seq.GetScope())
{
    if (depth > 0) {
        depth--;
        x_InitializeBioseq(top_level_seq,
                           depth,
                           top_level_seq.GetSeqId().GetPointer(),
                           direction);
    }
    else if (direction == eSeqMap_Up) {
        // Synonyms conversion
        CConstRef<CSeq_id> top_level_id = top_level_seq.GetSeqId();
        m_DstRanges.resize(1);
        m_DstRanges[0][CSeq_id_Handle::GetHandle(*top_level_id)]
            .push_back(TRange::GetWhole());
    }
    x_PreserveDestinationLocs();
}


CSeq_loc_Mapper::CSeq_loc_Mapper(size_t           depth,
                                 const CSeqMap&   top_level_seq,
                                 ESeqMapDirection direction,
                                 const CSeq_id*   top_level_id,
                                 CScope*          scope)
    : m_Scope(scope)
{
    if (depth > 0) {
        depth--;
        x_InitializeSeqMap(top_level_seq, depth, top_level_id, direction);
    }
    else if (direction == eSeqMap_Up) {
        // Synonyms conversion
        m_DstRanges.resize(1);
        m_DstRanges[0][CSeq_id_Handle::GetHandle(*top_level_id)]
            .push_back(TRange::GetWhole());
    }
    x_PreserveDestinationLocs();
}


CSeq_loc_Mapper::~CSeq_loc_Mapper(void)
{
    return;
}


void CSeq_loc_Mapper::x_InitializeSeqMap(const CSeqMap&   seq_map,
                                         const CSeq_id*   top_id,
                                         ESeqMapDirection direction)
{
    x_InitializeSeqMap(seq_map, size_t(-1), top_id, direction);
}


void CSeq_loc_Mapper::x_InitializeBioseq(const CBioseq_Handle& bioseq,
                                         const CSeq_id* top_id,
                                         ESeqMapDirection direction)
{
    x_InitializeBioseq(bioseq, size_t(-1), top_id, direction);
}


void CSeq_loc_Mapper::x_InitializeSeqMap(const CSeqMap&   seq_map,
                                         size_t           depth,
                                         const CSeq_id*   top_id,
                                         ESeqMapDirection direction)
{
    SSeqMapSelector sel(CSeqMap::fFindRef, depth);
    sel.SetLinkUsedTSE();
    x_InitializeSeqMap(CSeqMap_CI(ConstRef(&seq_map),
                       m_Scope.GetScopeOrNull(), sel),
                       top_id,
                       direction);
}


void CSeq_loc_Mapper::x_InitializeBioseq(const CBioseq_Handle& bioseq,
                                         size_t                depth,
                                         const CSeq_id*        top_id,
                                         ESeqMapDirection      direction)
{
    x_InitializeSeqMap(CSeqMap_CI(bioseq,
                                  SSeqMapSelector(CSeqMap::fFindRef, depth)),
                       top_id,
                       direction);
}


void CSeq_loc_Mapper::x_InitializeSeqMap(CSeqMap_CI       seg_it,
                                         const CSeq_id*   top_id,
                                         ESeqMapDirection direction)
{
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
        switch (direction) {
        case eSeqMap_Up:
            x_NextMappingRange(*src_id, src_from, src_len, src_strand,
                               *dst_id, dst_from, dst_len, eNa_strand_unknown);
            break;
        case eSeqMap_Down:
            x_NextMappingRange(*dst_id, dst_from, dst_len, eNa_strand_unknown,
                               *src_id, src_from, src_len, src_strand);
            break;
        }
        _ASSERT(src_len == 0  &&  dst_len == 0);
    }
}


/////////////////////////////////////////////////////////////////////
//
//   Initialization helpers
//


void CSeq_loc_Mapper::CollectSynonyms(const CSeq_id_Handle& id,
                                      TSynonyms&            synonyms) const
{
    if ( m_Scope.IsSet() ) {
        CConstRef<CSynonymsSet> syns =
            m_Scope.GetScope().GetSynonyms(id);
        ITERATE(CSynonymsSet, syn_it, *syns) {
            synonyms.push_back(CSynonymsSet::GetSeq_id_Handle(syn_it));
        }
    }
    else {
        synonyms.push_back(id);
    }
}


int CSeq_loc_Mapper::CheckSeqWidth(const CSeq_id& id,
                                   int            width,
                                   TSeqPos*       length)
{
    if ( m_Scope.IsNull() ) {
        return width;
    }
    CBioseq_Handle handle;
    try {
        handle = m_Scope.GetScope().GetBioseqHandle(id);
    } catch (...) {
        return width;
    }
    if ( !handle ) {
        return width;
    }
    if ( length ) {
        *length = handle.GetBioseqLength();
    }
    switch ( handle.GetBioseqMolType() ) {
    case CSeq_inst::eMol_dna:
    case CSeq_inst::eMol_rna:
    case CSeq_inst::eMol_na:
        {
            if ( width  &&  width != 3 ) {
                // width already set to a different value
                NCBI_THROW(CAnnotMapperException, eBadLocation,
                            "Location contains different sequence types");
            }
            width = 3;
            break;
        }
    case CSeq_inst::eMol_aa:
        {
            if ( width  &&  width != 1 ) {
                // width already set to a different value
                NCBI_THROW(CAnnotMapperException, eBadLocation,
                            "Location contains different sequence types");
            }
            width = 1;
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
    CConstRef<CSynonymsSet> syns = m_Scope.GetScope().GetSynonyms(id);
    ITERATE(CSynonymsSet, syn_it, *syns) {
        m_Widths[syns->GetSeq_id_Handle(syn_it)] = wid_cvt;
    }
    return width;
}


TSeqPos CSeq_loc_Mapper::GetSequenceLength(const CSeq_id& id)
{
    CBioseq_Handle h;
    if ( m_Scope.IsSet() ) {
        h = m_Scope.GetScope().GetBioseqHandle(id);
    }
    if ( !h ) {
        NCBI_THROW(CAnnotMapperException, eUnknownLength,
                    "Can not map from minus strand -- "
                    "unknown sequence length");
    }
    return h.GetBioseqLength();
}


CSeq_align_Mapper_Base*
CSeq_loc_Mapper::InitAlignMapper(const CSeq_align& src_align)
{
    return new CSeq_align_Mapper(src_align, m_UseWidth, m_Scope);
}


END_SCOPE(objects)
END_NCBI_SCOPE
