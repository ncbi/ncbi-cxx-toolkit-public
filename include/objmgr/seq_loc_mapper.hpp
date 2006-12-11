#ifndef SEQ_LOC_MAPPER__HPP
#define SEQ_LOC_MAPPER__HPP

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

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>
#include <util/range.hpp>
#include <util/rangemap.hpp>
#include <objects/seqloc/Na_strand.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seq/seq_id_handle.hpp>
#include <objects/general/Int_fuzz.hpp>
#include <objmgr/impl/heap_scope.hpp>
#include <objects/seq/seq_loc_mapper_base.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


/** @addtogroup ObjectManagerCore
 *
 * @{
 */


class CScope;
class CBioseq_Handle;
class CSeqMap;
class CSeqMap_CI;


/////////////////////////////////////////////////////////////////////////////
///
///  CSeq_loc_Mapper --
///
///  Mapping locations and alignments between bioseqs through seq-locs,
///  features, alignments or between parts of segmented bioseqs.


class NCBI_XOBJMGR_EXPORT CSeq_loc_Mapper : public CSeq_loc_Mapper_Base
{
public:
    enum ESeqMapDirection {
        eSeqMap_Up,    ///< map from segments to the top level bioseq
        eSeqMap_Down   ///< map from a segmented bioseq to segments
    };

    /// Mapping through a pre-filled CMappipngRanges. Source(s) and
    /// destination(s) are considered as having the same width.
    /// @param mapping_ranges
    ///  CMappingRanges filled with the desired source and destination
    ///  ranges. Must be a heap object (will be stored in a CRef<>).
    /// @param scope
    ///  Optional scope (required only for mapping alignments).
    CSeq_loc_Mapper(CMappingRanges* mapping_ranges,
                    CScope*         scope = 0);

    /// Mapping through a feature, both location and product must be set.
    /// If scope is set, synonyms are resolved for each source ID.
    CSeq_loc_Mapper(const CSeq_feat&  map_feat,
                    EFeatMapDirection dir,
                    CScope*           scope = 0);

    /// Mapping between two seq_locs. If scope is set, synonyms are resolved
    /// for each source ID.
    CSeq_loc_Mapper(const CSeq_loc&   source,
                    const CSeq_loc&   target,
                    CScope*           scope = 0);

    /// Mapping through an alignment. Need to specify target ID or
    /// target row of the alignment. Any other ID is mapped to the
    /// target one. If scope is set, synonyms are resolved for each source ID.
    /// Only the first row matching target ID is used, all other rows
    /// are considered source.
    CSeq_loc_Mapper(const CSeq_align& map_align,
                    const CSeq_id&    to_id,
                    CScope*           scope = 0,
                    TMapOptions       opts = 0);
    CSeq_loc_Mapper(const CSeq_align& map_align,
                    size_t            to_row,
                    CScope*           scope = 0,
                    TMapOptions       opts = 0);

    /// Mapping between segments and the top level sequence.
    /// @param top_level_seq
    ///  Top level bioseq
    /// @param direction
    ///  Direction of mapping: up (from segments to master) or down.
    CSeq_loc_Mapper(CBioseq_Handle   target_seq,
                    ESeqMapDirection direction);

    /// Mapping through a seq-map.
    /// @param seq_map
    ///  Sequence map defining the mapping
    /// @param direction
    ///  Direction of mapping: up (from segments to master) or down.
    /// @param top_level_id
    ///  Explicit destination id when mapping up, may be used with
    ///  seq-maps constructed from a seq-loc with multiple ids.
    CSeq_loc_Mapper(const CSeqMap&   seq_map,
                    ESeqMapDirection direction,
                    const CSeq_id*   top_level_id = 0,
                    CScope*          scope = 0);

    /// Mapping between segments and the top level sequence limited by depth.
    /// @param depth
    ///  Mapping depth. Depth of 0 converts synonyms.
    /// @param top_level_seq
    ///  Top level bioseq
    /// @param direction
    ///  Direction of mapping: up (from segments to master) or down.
    CSeq_loc_Mapper(size_t                depth,
                    const CBioseq_Handle& top_level_seq,
                    ESeqMapDirection      direction);

    /// Depth-limited mapping through a seq-map.
    /// @param depth
    ///  Mapping depth. Depth of 0 converts synonyms.
    /// @param seq_map
    ///  Sequence map defining the mapping
    /// @param direction
    ///  Direction of mapping: up (from segments to master) or down.
    /// @param top_level_id
    ///  Explicit destination id when mapping up, may be used with
    ///  seq-maps constructed from a seq-loc with multiple ids.
    CSeq_loc_Mapper(size_t           depth,
                    const CSeqMap&   top_level_seq,
                    ESeqMapDirection direction,
                    const CSeq_id*   top_level_id = 0,
                    CScope*          scope = 0);

    ~CSeq_loc_Mapper(void);

protected:
    // Collect synonyms for the given seq-id
    virtual void CollectSynonyms(const CSeq_id_Handle& id,
                                 TSynonyms&            synonyms);

    // Check molecule type, return character width (3=na, 1=aa, 0=unknown).
    virtual int CheckSeqWidth(const CSeq_id& id,
                              int            width,
                              TSeqPos*       length = 0);

    // Get sequence length for the given seq-id
    virtual TSeqPos GetSequenceLength(const CSeq_id& id);

    // Create CSeq_align_Mapper, add any necessary arguments
    virtual CSeq_align_Mapper_Base*
        InitAlignMapper(const CSeq_align& src_align);

private:
    CSeq_loc_Mapper(const CSeq_loc_Mapper&);
    CSeq_loc_Mapper& operator=(const CSeq_loc_Mapper&);

    void x_InitializeSeqMap(const CSeqMap&   seq_map,
                            const CSeq_id*   top_id,
                            ESeqMapDirection direction);
    void x_InitializeSeqMap(const CSeqMap&   seq_map,
                            size_t           depth,
                            const CSeq_id*   top_id,
                            ESeqMapDirection direction);
    void x_InitializeSeqMap(CSeqMap_CI       seg_it,
                            const CSeq_id*   top_id,
                            ESeqMapDirection direction);
    void x_InitializeBioseq(const CBioseq_Handle& bioseq,
                            const CSeq_id*        top_id,
                            ESeqMapDirection      direction);
    void x_InitializeBioseq(const CBioseq_Handle& bioseq,
                            size_t                depth,
                            const CSeq_id*        top_id,
                            ESeqMapDirection      direction);

private:
    CHeapScope        m_Scope;
};


/* @} */


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.37  2006/12/11 17:14:11  grichenk
* Added CSeq_loc_Mapper_Base and CSeq_align_Mapper_Base.
*
* Revision 1.36  2006/08/28 18:35:02  grichenk
* BeginMappingRanges() made const.
*
* Revision 1.35  2006/08/21 15:46:42  grichenk
* Added CMappingRanges for storing mappings.
* Added CSeq_loc_Mapper(CMappingRanges&) constructor.
*
* Revision 1.34  2006/06/26 16:56:41  grichenk
* Fixed order of intervals in mapped locations.
* Filter duplicate mappings.
*
* Revision 1.33  2006/05/17 20:06:57  dicuccio
* Added optional flags to CSeq_loc_Mapper for interpretation of alignments:
* optionally consider each block of a dense-seg alignment by its total ranges
* instead of by the internal indel structure
*
* Revision 1.32  2006/05/04 21:07:24  grichenk
* Fixed mapping of std-segs.
*
* Revision 1.31  2006/03/21 16:38:19  grichenk
* Added flag to check strands before mapping
*
* Revision 1.30  2006/03/16 18:58:30  grichenk
* Indicate intervals truncated while mapping by fuzz lim tl/tr.
*
* Revision 1.29  2006/02/14 15:02:34  grichenk
* Removed wrong comment
*
* Revision 1.28  2006/02/01 19:48:22  grichenk
* CBioseq_Handle& top_level_seq argument made const.
*
* Revision 1.27  2005/04/13 19:39:27  grichenk
* Extend partial ranges when mapping prot to nuc.
*
* Revision 1.26  2005/03/29 19:22:12  jcherry
* Added export specifiers
*
* Revision 1.25  2005/03/01 22:22:10  grichenk
* Added strand to index conversion macro, changed strand index type to size_t.
*
* Revision 1.24  2005/02/01 21:55:11  grichenk
* Added direction flag for mapping between top level sequence
* and segments.
*
* Revision 1.23  2004/12/22 15:56:14  vasilche
* Used CHeapScope instead of plain CRef<CScope>.
* Reorganized x_Initialize to allow used TSE linking.
*
* Revision 1.22  2004/11/22 16:04:06  grichenk
* Fixed/added doxygen comments
*
* Revision 1.21  2004/11/15 22:21:48  grichenk
* Doxygenized comments, fixed group names.
*
* Revision 1.20  2004/10/27 20:01:04  grichenk
* Fixed mapping: strands, circular locations, fuzz.
*
* Revision 1.19  2004/10/25 14:04:21  grichenk
* Fixed order of ranges in mapped seq-loc.
*
* Revision 1.18  2004/09/27 16:52:27  grichenk
* Fixed order of mapped intervals
*
* Revision 1.17  2004/09/27 14:36:52  grichenk
* Set eDestinationPreserve in some constructors by default
*
* Revision 1.16  2004/08/11 17:23:09  grichenk
* Added eMergeContained flag. Fixed convertion of whole to seq-interval.
*
* Revision 1.15  2004/08/06 15:28:16  grichenk
* Changed PreserveDestinationLocs() to map all synonyms to the target seq-id.
*
* Revision 1.14  2004/07/12 15:05:31  grichenk
* Moved seq-id mapper from xobjmgr to seq library
*
* Revision 1.13  2004/05/26 14:29:20  grichenk
* Redesigned CSeq_align_Mapper: preserve non-mapping intervals,
* fixed strands handling, improved performance.
*
* Revision 1.12  2004/05/07 13:53:18  grichenk
* Preserve fuzz from original location.
* Better detection of partial locations.
*
* Revision 1.11  2004/05/05 14:04:22  grichenk
* Use fuzz to indicate truncated intervals. Added KeepNonmapping flag.
*
* Revision 1.10  2004/04/23 15:34:49  grichenk
* Added PreserveDestinationLocs().
*
* Revision 1.9  2004/04/12 14:35:59  grichenk
* Fixed mapping of alignments between nucleotides and proteins
*
* Revision 1.8  2004/04/06 13:56:33  grichenk
* Added possibility to remove gaps (NULLs) from mapped location
*
* Revision 1.7  2004/03/30 21:21:09  grichenk
* Reduced number of includes.
*
* Revision 1.6  2004/03/30 17:00:00  grichenk
* Fixed warnings, moved inline functions to hpp.
*
* Revision 1.5  2004/03/30 15:42:33  grichenk
* Moved alignment mapper to separate file, added alignment mapping
* to CSeq_loc_Mapper.
*
* Revision 1.4  2004/03/29 15:13:56  grichenk
* Added mapping down to segments of a bioseq or a seqmap.
* Fixed: preserve ranges already on the target bioseq(s).
*
* Revision 1.3  2004/03/22 21:10:58  grichenk
* Added mapping from segments to master sequence or through a seq-map.
*
* Revision 1.2  2004/03/19 14:19:08  grichenk
* Added seq-loc mapping through a seq-align.
*
* Revision 1.1  2004/03/10 16:22:29  grichenk
* Initial revision
*
*
* ===========================================================================
*/

#endif  // SEQ_LOC_MAPPER__HPP
