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
#include <objmgr/seq_id_handle.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


class CSeq_id;
class CSeq_loc;
class CSeq_loc_CI;
class CSeq_feat;
class CSeq_align;
class CScope;
class CBioseq_Handle;
class CSeqMap;

class CMappingRange : public CObject
{
public:
    CMappingRange(CSeq_id_Handle    src_id,
                  TSeqPos           src_from,
                  TSeqPos           src_length,
                  ENa_strand        src_strand,
                  const CSeq_id&    dst_id,
                  TSeqPos           dst_from,
                  ENa_strand        dst_strand);

    bool GoodSrcId(const CSeq_id& id) const;
    CSeq_id& GetDstId(void);

    typedef CRange<TSeqPos> TRange;

    bool CanMap(TSeqPos from,
                TSeqPos to,
                bool is_set_strand,
                ENa_strand strand) const;
    TSeqPos Map_Pos(TSeqPos pos) const;
    TRange Map_Range(TSeqPos from, TSeqPos to) const;
    bool Map_Strand(bool is_set_strand, ENa_strand src, ENa_strand* dst) const;

private:
    CSeq_id_Handle      m_Src_id_Handle;
    TSeqPos             m_Src_from;
    TSeqPos             m_Src_to;
    ENa_strand          m_Src_strand;
    CRef<CSeq_id>       m_Dst_id;
    TSeqPos             m_Dst_from;
    ENa_strand          m_Dst_strand;
    bool                m_Reverse;

    friend class CSeq_loc_Mapper;
    friend class CSeq_align_Mapper;
};


class NCBI_XOBJMGR_EXPORT CSeq_loc_Mapper : public CObject
{
public:
    enum EFeatMapDirection {
        eLocationToProduct,
        eProductToLocation
    };

    // Mapping through a feature, both location and product must be set.
    // If scope is set, synonyms are resolved for each source ID.
    CSeq_loc_Mapper(const CSeq_feat&  map_feat,
                    EFeatMapDirection dir,
                    CScope*           scope = 0);

    // Mapping between two seq_locs. If scope is set, synonyms are resolved
    // for each source ID.
    CSeq_loc_Mapper(const CSeq_loc&   source,
                    const CSeq_loc&   target,
                    CScope*           scope = 0);

    // Mapping through an alignment. Need to specify target ID or
    // target row of the alignment. Any other ID is mapped to the
    // target one. If scope is set, synonyms are resolved for each source ID.
    // Only the first row matching target ID is used, all other rows
    // are considered source.
    CSeq_loc_Mapper(const CSeq_align& map_align,
                    const CSeq_id&    to_id,
                    CScope*           scope = 0);
    CSeq_loc_Mapper(const CSeq_align& map_align,
                    size_t            to_row,
                    CScope*           scope = 0);

    // Mapping from segments to the segmented sequence (same as
    // in annot iterator). If dst_id is set, all segments are
    // mapped to the id. Otherwise mapping is done to the top
    // level references in the map (e.g. if the map is created from
    // a seq-loc).
    CSeq_loc_Mapper(CBioseq_Handle target_seq);
    CSeq_loc_Mapper(const CSeqMap& seq_map,
                    const CSeq_id* dst_id = 0,
                    CScope*        scope = 0);

    // Mapping from master sequence to its segments, restricted
    // by depth. Depth = 0 is for synonyms conversion.
    CSeq_loc_Mapper(size_t          depth,
                    CBioseq_Handle& source_seq);
    CSeq_loc_Mapper(size_t         depth,
                    const CSeqMap& source_seqmap,
                    const CSeq_id* src_id = 0,
                    CScope*        scope = 0);

    ~CSeq_loc_Mapper(void);

    // Intervals' merging mode
    CSeq_loc_Mapper& SetMergeNone(void);
    CSeq_loc_Mapper& SetMergeAbutting(void);
    CSeq_loc_Mapper& SetMergeAll(void);

    CSeq_loc_Mapper& SetGapPreserve(void);
    CSeq_loc_Mapper& SetGapRemove(void);

    // Create target-to-target mapping to avoid truncation of ranges
    // already on the target sequence(s).
    void PreserveDestinationLocs(void);

    CRef<CSeq_loc>   Map(const CSeq_loc& src_loc);
    CRef<CSeq_align> Map(const CSeq_align& src_align);

    // Check if the last mapping resulted in partial location
    bool             LastIsPartial(void);

private:
    CSeq_loc_Mapper(const CSeq_loc_Mapper&);
    CSeq_loc_Mapper& operator=(const CSeq_loc_Mapper&);

    friend class CSeq_loc_Conversion_Set;
    friend class CSeq_align_Mapper;

    enum EMergeFlags {
        eMergeNone,      // no merging
        eMergeAbutting,  // merge only abutting intervals, keep overlapping
        eMergeAll        // merge both abutting and overlapping intervals
    };
    enum EGapFlags {
        eGapPreserve,    // Leave gaps as-is
        eGapRemove       // Remove gaps (NULLs)
    };

    enum EWidthFlags {
        fWidthProtToNuc = 1,
        fWidthNucToProt = 2
    };
    typedef int TWidthFlags; // binary OR of "EWidthFlags"
    // Conversions
    typedef CRange<TSeqPos>                              TRange;
    typedef CRangeMultimap<CRef<CMappingRange>, TSeqPos> TRangeMap;
    typedef TRangeMap::iterator                          TRangeIterator;
    typedef map<CSeq_id_Handle, TRangeMap>               TIdMap;
    // List and map of target ranges to construct target-to-target mapping
    typedef list<TRange>                    TDstRanges;
    typedef map<CSeq_id_Handle, TDstRanges> TDstIdMap;
    typedef vector<TDstIdMap>               TDstStrandMap;

    // Destination locations arranged by ID/range
    typedef list<TRange>                         TMappedRanges;
    // 0 = not set, any other index = na_strand + 1
    typedef vector<TMappedRanges>                TRangesByStrand;
    typedef map<CSeq_id_Handle, TRangesByStrand> TRangesById;
    typedef map<CSeq_id_Handle, TWidthFlags>     TWidthById;

    typedef CSeq_align::C_Segs::TDendiag TDendiag;
    typedef CSeq_align::C_Segs::TStd     TStd;

    // Check molecule type, return character width (3=na, 1=aa, 0=unknown).
    int x_CheckSeqWidth(const CSeq_id& id, int width);
    int x_CheckSeqWidth(const CSeq_loc& loc,
                        TSeqPos* total_length);
    // Get sequence length, try to get the real length for
    // reverse strand, do not use "whole".
    TSeqPos x_GetRangeLength(const CSeq_loc_CI& it);
    void x_AddConversion(const CSeq_id& src_id,
                         TSeqPos        src_start,
                         ENa_strand     src_strand,
                         const CSeq_id& dst_id,
                         TSeqPos        dst_start,
                         ENa_strand     dst_strand,
                         TSeqPos        length);
    void x_NextMappingRange(const CSeq_id&  src_id,
                            TSeqPos&        src_start,
                            TSeqPos&        src_len,
                            ENa_strand      src_strand,
                            const CSeq_id&  dst_id,
                            TSeqPos&        dst_start,
                            TSeqPos&        dst_len,
                            ENa_strand      dst_strand);

    // Optional frame is used for cd-region only
    void x_Initialize(const CSeq_loc& source,
                      const CSeq_loc& target,
                      int             frame = 0);
    void x_Initialize(const CSeq_align& map_align,
                      const CSeq_id&    to_id);
    void x_Initialize(const CSeq_align& map_align,
                      size_t            to_row);
    void x_Initialize(const CSeqMap& seq_map,
                      const CSeq_id* top_id = 0);
    void x_Initialize(const CSeqMap& seq_map,
                      size_t         depth,
                      const CSeq_id* top_id = 0);

    void x_InitAlign(const CDense_diag& diag, size_t to_row);
    void x_InitAlign(const CDense_seg& denseg, size_t to_row);
    void x_InitAlign(const CStd_seg& sseg, size_t to_row);
    void x_InitAlign(const CPacked_seg& pseg, size_t to_row);

    TRangeIterator x_BeginMappingRanges(CSeq_id_Handle id,
                                        TSeqPos from,
                                        TSeqPos to);

    bool x_MapInterval(const CSeq_id&   src_id,
                       TRange           src_rg,
                       bool             is_set_strand,
                       ENa_strand       src_strand);

    void x_PushLocToDstMix(CRef<CSeq_loc> loc);

    // Convert collected ranges into seq-loc and push into destination mix.
    void x_PushRangesToDstMix(void);

    void x_MapSeq_loc(const CSeq_loc& src_loc);
    CRef<CSeq_align> x_MapSeq_align(const CSeq_align& src_align);

    // Access mapped ranges, check vector size
    TMappedRanges& x_GetMappedRanges(const CSeq_id_Handle& id,
                                     int                   strand_idx) const;

    CRef<CSeq_loc> x_RangeToSeq_loc(const CSeq_id_Handle& idh,
                                    TSeqPos               from,
                                    TSeqPos               to,
                                    int                   strand_idx);

    // Check location type, optimize if possible (empty mix to NULL,
    // mix with a single element to this element etc.).
    void x_OptimizeSeq_loc(CRef<CSeq_loc>& loc);

    CRef<CSeq_loc> x_GetMappedSeq_loc(void);

    CRef<CScope>      m_Scope;
    // CSeq_loc_Conversion_Set m_Cvt;
    EMergeFlags       m_MergeFlag;
    EGapFlags         m_GapFlag;

    // Sources may have different widths, e.g. in an alignment
    TWidthById      m_Widths;
    bool            m_UseWidth;
    int             m_Dst_width;
    TIdMap          m_IdMap;
    bool            m_Partial;

    mutable TRangesById m_MappedLocs;
    CRef<CSeq_loc>      m_Dst_loc;
    TDstStrandMap       m_DstRanges;
};


inline
bool CMappingRange::GoodSrcId(const CSeq_id& id) const
{
    return m_Src_id_Handle == id;
}


inline
CSeq_id& CMappingRange::GetDstId(void)
{
    return *m_Dst_id;
}


inline
CSeq_loc_Mapper& CSeq_loc_Mapper::SetMergeNone(void)
{
    m_MergeFlag = eMergeNone;
    return *this;
}


inline
CSeq_loc_Mapper& CSeq_loc_Mapper::SetMergeAbutting(void)
{
    m_MergeFlag = eMergeAbutting;
    return *this;
}


inline
CSeq_loc_Mapper& CSeq_loc_Mapper::SetMergeAll(void)
{
    m_MergeFlag = eMergeAll;
    return *this;
}


inline
CSeq_loc_Mapper& CSeq_loc_Mapper::SetGapPreserve(void)
{
    m_GapFlag = eGapPreserve;
    return *this;
}


inline
CSeq_loc_Mapper& CSeq_loc_Mapper::SetGapRemove(void)
{
    m_GapFlag = eGapRemove;
    return *this;
}


inline
bool CSeq_loc_Mapper::LastIsPartial(void)
{
    return m_Partial;
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
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
