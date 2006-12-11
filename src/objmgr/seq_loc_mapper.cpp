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
                                      TSynonyms&            synonyms)
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

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.53  2006/12/11 17:14:12  grichenk
* Added CSeq_loc_Mapper_Base and CSeq_align_Mapper_Base.
*
* Revision 1.52  2006/08/28 18:36:38  grichenk
* BeginMappingRanges() made const.
*
* Revision 1.51  2006/08/21 15:46:42  grichenk
* Added CMappingRanges for storing mappings.
* Added CSeq_loc_Mapper(CMappingRanges&) constructor.
*
* Revision 1.50  2006/06/26 16:56:42  grichenk
* Fixed order of intervals in mapped locations.
* Filter duplicate mappings.
*
* Revision 1.49  2006/05/17 20:06:58  dicuccio
* Added optional flags to CSeq_loc_Mapper for interpretation of alignments:
* optionally consider each block of a dense-seg alignment by its total ranges
* instead of by the internal indel structure
*
* Revision 1.48  2006/05/10 15:11:44  grichenk
* Convert the resulting mix into packed int if possible.
*
* Revision 1.47  2006/05/04 21:07:24  grichenk
* Fixed mapping of std-segs.
*
* Revision 1.46  2006/03/21 16:38:19  grichenk
* Added flag to check strands before mapping
*
* Revision 1.45  2006/03/16 18:58:30  grichenk
* Indicate intervals truncated while mapping by fuzz lim tl/tr.
*
* Revision 1.44  2006/02/01 19:48:22  grichenk
* CBioseq_Handle& top_level_seq argument made const.
*
* Revision 1.43  2005/12/08 17:33:17  vasilche
* Fixed exception with null scope where it's optional.
*
* Revision 1.42  2005/09/22 20:49:33  grichenk
* Adjust segment length when mapping alignment between nuc and prot.
*
* Revision 1.41  2005/04/13 19:39:27  grichenk
* Extend partial ranges when mapping prot to nuc.
*
* Revision 1.40  2005/03/02 22:10:38  grichenk
* Removed strand check from CanMap()
*
* Revision 1.39  2005/03/01 22:22:10  grichenk
* Added strand to index conversion macro, changed strand index type to size_t.
*
* Revision 1.38  2005/03/01 17:33:36  grichenk
* Fixed strand indexing.
*
* Revision 1.37  2005/02/10 22:14:16  grichenk
* Do not add gap to mix when bond point B is not set.
*
* Revision 1.36  2005/02/10 16:21:03  grichenk
* Fixed mapping of bonds
*
* Revision 1.35  2005/02/01 21:55:11  grichenk
* Added direction flag for mapping between top level sequence
* and segments.
*
* Revision 1.34  2004/12/22 15:56:14  vasilche
* Used CHeapScope instead of plain CRef<CScope>.
* Reorganized x_Initialize to allow used TSE linking.
*
* Revision 1.33  2004/12/15 16:26:21  grichenk
* Fixed resolve count in seq-map iterator
*
* Revision 1.32  2004/12/14 17:41:03  grichenk
* Reduced number of CSeqMap::FindResolved() methods, simplified
* BeginResolved and EndResolved. Marked old methods as deprecated.
*
* Revision 1.31  2004/11/24 16:21:36  grichenk
* Check iterator when merging abutting ranges
*
* Revision 1.30  2004/10/28 15:33:49  grichenk
* Fixed fuzzes.
*
* Revision 1.29  2004/10/27 20:01:04  grichenk
* Fixed mapping: strands, circular locations, fuzz.
*
* Revision 1.28  2004/10/25 14:04:21  grichenk
* Fixed order of ranges in mapped seq-loc.
*
* Revision 1.27  2004/10/14 13:55:08  grichenk
* Fixed usage of widths
*
* Revision 1.26  2004/09/27 16:52:27  grichenk
* Fixed order of mapped intervals
*
* Revision 1.25  2004/09/27 14:36:52  grichenk
* Set eDestinationPreserve in some constructors by default
*
* Revision 1.24  2004/09/03 16:57:13  dicuccio
* Fixed type: 'fuzz.second' should be 'res.second' (prevent dereference of a null
* pointer)
*
* Revision 1.23  2004/08/25 15:03:56  grichenk
* Removed duplicate methods from CSeqMap
*
* Revision 1.22  2004/08/11 17:23:09  grichenk
* Added eMergeContained flag. Fixed convertion of whole to seq-interval.
*
* Revision 1.21  2004/08/06 15:28:16  grichenk
* Changed PreserveDestinationLocs() to map all synonyms to the target seq-id.
*
* Revision 1.20  2004/05/26 14:29:20  grichenk
* Redesigned CSeq_align_Mapper: preserve non-mapping intervals,
* fixed strands handling, improved performance.
*
* Revision 1.19  2004/05/21 21:42:13  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.18  2004/05/10 20:22:24  grichenk
* Fixed more warnings (removed inlines)
*
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
