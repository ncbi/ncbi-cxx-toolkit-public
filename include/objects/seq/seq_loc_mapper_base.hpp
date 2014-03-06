#ifndef SEQ_LOC_MAPPER_BASE__HPP
#define SEQ_LOC_MAPPER_BASE__HPP

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

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>
#include <util/range.hpp>
#include <util/rangemap.hpp>
#include <objects/seqloc/Na_strand.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Spliced_exon.hpp>
#include <objects/seq/seq_id_handle.hpp>
#include <objects/general/Int_fuzz.hpp>
#include <objects/seq/annot_mapper_exception.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


/** @addtogroup ObjectManagerCore
 *
 * @{
 */


class CSeq_id;
class CSeq_loc;
class CSeq_interval;
class CPacked_seqpnt;
class CSeq_loc_CI;
class CSeq_feat;
class CSeq_align;
class CSeq_align_Mapper_Base;
class CSeq_graph;
class IMapper_Sequence_Info;


/// CMappingRange - describles a single interval to interval
/// mapping.
class NCBI_SEQ_EXPORT CMappingRange : public CObject
{
public:
    CMappingRange(CSeq_id_Handle    src_id,
                  TSeqPos           src_from,
                  TSeqPos           src_length,
                  ENa_strand        src_strand,
                  CSeq_id_Handle    dst_id,
                  TSeqPos           dst_from,
                  ENa_strand        dst_strand,
                  bool              ext_to = false,
                  int               frame = 0,
                  TSeqPos           dst_total_len = kInvalidSeqPos,
                  TSeqPos           src_bioseq_len = kInvalidSeqPos,
                  TSeqPos           dst_len = kInvalidSeqPos );

    /// Check if the id is on the source sequence.
    bool GoodSrcId(const CSeq_id& id) const;
    CRef<CSeq_id> GetDstId(void) const;
    const CSeq_id_Handle& GetDstIdHandle(void) const
        { return m_Dst_id_Handle; }

    typedef CRange<TSeqPos>    TRange;
    typedef CRef<CInt_fuzz>    TFuzz;
    typedef pair<TFuzz, TFuzz> TRangeFuzz;

    /// Check if the interval can be mapped through this mapping range.
    /// Strand direction is checked only if is_set_strand is true.
    bool CanMap(TSeqPos    from,
                TSeqPos    to,
                bool       is_set_strand,
                ENa_strand strand) const;
    /// Map a single point
    TSeqPos Map_Pos(TSeqPos pos) const;
    /// Map an interval, set fuzz when the mapping truncates the original
    /// range.
    TRange Map_Range(TSeqPos           from,
                     TSeqPos           to,
                     const TRangeFuzz* fuzz = 0) const;
    /// Map the strand, return true if the destination strand sould be
    /// set (even if it's eNa_strand_unknown -- this may happen if the
    /// source strand is set to unknown).
    bool Map_Strand(bool is_set_strand,
                    ENa_strand src,
                    ENa_strand* dst) const;
    /// Map fuzz if one is set in the original location.
    TRangeFuzz Map_Fuzz(const TRangeFuzz& fuzz) const;

private:
    // Get new fuzz value when reversing location's strand.
    CInt_fuzz::ELim x_ReverseFuzzLim(CInt_fuzz::ELim lim) const;
    void x_Map_Fuzz(TFuzz& fuzz) const;

    CSeq_id_Handle      m_Src_id_Handle;
    TSeqPos             m_Src_from;
    TSeqPos             m_Src_to;
    ENa_strand          m_Src_strand;
    CSeq_id_Handle      m_Dst_id_Handle;
    TSeqPos             m_Dst_from;
    ENa_strand          m_Dst_strand;
    // Wether the mapping reverses the strand or not.
    // This value can be calculated from source and destination
    // strands, but is cached for better performance.
    bool                m_Reverse;
    // Wether to extend the mapped location to the end of
    // destination range. Used when mapping from a prot to a nuc.
    // ExtTo is set when both conditions are met:
    // - the mapping is from a protein to a nucleotide
    // - the destination interval has partial 'to' (set as fuzz)
    // ExtTo is used only when the interval to be mapped has
    // partial 'to' set through the fuzz and the mapped range is
    // just 1 or 2 bases shorter than the mapping destination.
    bool                m_ExtTo;
    // Holds the frame shift (0 if none) of the underlying CDS (if any).
    int                 m_Frame;
    // Holds the complete length of the destination.  This is needed
    // to detect whether or not fuzzy edges should be extended to the end.
    TSeqPos             m_Dst_total_len;
    // This holds the complete length of the original source bioseq.
    // Needed to detect whether or not fuzzy edges should be extended to the end.
    TSeqPos             m_Src_bioseq_len;
    // For example, if the end of a source maps to just before the end of the
    // dest, then we sometimes extend to the end of the dest, so we do need
    // to store this, even though it's not needed for the mapping itself.
    TSeqPos             m_Dst_len;
    // Group of mapping ranges - used with alignments, e.g. to group
    // mapped ranges by exon.
    int                 m_Group;

    friend class CSeq_loc_Mapper_Base;
    //friend class CSeq_loc_Mapper;
    friend class CMappingRanges;
    friend class CSeq_align_Mapper_Base;
    //friend class CSeq_align_Mapper;
    friend struct CMappingRangeRef_Less;
    friend struct CMappingRangeRef_LessRev;
public:
    // Interface for CPairwiseAln converter
    TSeqPos GetSrc_from(void) const { return m_Src_from; }
    TSeqPos GetDst_from(void) const { return m_Dst_from; }
    TSeqPos GetLength(void) const { return m_Src_to - m_Src_from; }
    bool GetReverse(void) const { return m_Reverse; }
    int GetGroup(void) const { return m_Group; }
    void SetGroup(int grp) { m_Group = grp; }
};


/// Storage for multiple mapping ranges. Stores mappings grouped
/// by the source seq-id, then sorted by start coordinate.
class NCBI_SEQ_EXPORT CMappingRanges : public CObject
{
public:
    CMappingRanges(void);

    // Conversions
    typedef CMappingRange::TRange                        TRange;
    typedef CRangeMultimap<CRef<CMappingRange>, TSeqPos> TRangeMap;
    typedef TRangeMap::const_iterator                    TRangeIterator;
    typedef map<CSeq_id_Handle, TRangeMap>               TIdMap;
    typedef TIdMap::const_iterator                       TIdIterator;
    typedef vector< CRef<CMappingRange> >                TSortedMappings;

    const TIdMap& GetIdMap() const { return m_IdMap; }
    TIdMap& GetIdMap(void) { return m_IdMap; }

    /// Add new mapping range to the proper place.
    void AddConversion(CRef<CMappingRange> cvt);
    CRef<CMappingRange> AddConversion(CSeq_id_Handle    src_id,
                                      TSeqPos           src_from,
                                      TSeqPos           src_length,
                                      ENa_strand        src_strand,
                                      CSeq_id_Handle    dst_id,
                                      TSeqPos           dst_from,
                                      ENa_strand        dst_strand,
                                      bool              ext_to = false,
                                      int               frame = 0,
                                      TSeqPos           dst_total_len = kInvalidSeqPos,
                                      TSeqPos           src_bioseq_len = kInvalidSeqPos,
                                      TSeqPos           dst_len = kInvalidSeqPos );

    /// Get mapping ranges iterator for the given seq-id and range.
    TRangeIterator BeginMappingRanges(CSeq_id_Handle id,
                                      TSeqPos        from,
                                      TSeqPos        to) const;

    // Overall source and destination orientation. The order of mapped ranges
    // is reversed if ReverseSrc != ReverseDst (except in some merging modes).
    void SetReverseSrc(bool value = true) { m_ReverseSrc = value; };
    bool GetReverseSrc(void) const { return m_ReverseSrc; }
    void SetReverseDst(bool value = true) { m_ReverseDst = value; };
    bool GetReverseDst(void) const { return m_ReverseDst; }

private:
    TIdMap m_IdMap;

    // Mapping source and destination orientations
    bool   m_ReverseSrc;
    bool   m_ReverseDst;
};


/// Helper class for mapping graphs. Used to collect ranges
/// relative to the graph location and adjust mapped graph data
/// accordingly.
class NCBI_SEQ_EXPORT CGraphRanges : public CObject
{
public:
    CGraphRanges(void) : m_Offset(0) {}

    typedef CRange<TSeqPos> TRange;
    typedef vector<TRange>  TGraphRanges;

    // Offset is relative to the original graph location, indicates
    // the part of the original location which has been already
    // mapped (or truncated).
    TSeqPos GetOffset(void) const { return m_Offset; }
    void SetOffset(TSeqPos offset) { m_Offset = offset; }
    void IncOffset(TSeqPos inc) { m_Offset += inc; }

    const TGraphRanges& GetRanges(void) const { return m_Ranges; }

    // Add new mapped range. The range is relative to the not yet mapped
    // part of the original location. See:
    //   CSeq_loc_Mapper_Base::x_MapNextRange()
    //   CSeq_loc_Mapper_Base::x_MapInterval()
    void AddRange(const TRange& rg)
    {
        if ( rg.Empty() ) {
            return;
        }
        TRange offset_rg = rg.IsWhole() ? rg :
            TRange(rg.GetFrom() + m_Offset, rg.GetTo() + m_Offset);
        m_Ranges.push_back(offset_rg);
        m_TotalRange.CombineWith(offset_rg);
    }

    const TRange& GetTotalRange(void) const { return m_TotalRange; }

private:
    TSeqPos      m_Offset;
    TGraphRanges m_Ranges;
    TRange       m_TotalRange;
};


/////////////////////////////////////////////////////////////////////////////
///
///  CSeq_loc_Mapper_Base --
///
///  Mapping locations and alignments between bioseqs through seq-locs,
///  features, alignments or between parts of segmented bioseqs.


class NCBI_SEQ_EXPORT CSeq_loc_Mapper_Base : public CObject
{
public:
    /// Mapping direction used when initializing the mapper with a feature.
    enum EFeatMapDirection {
        eLocationToProduct, ///< Map from the feature's location to product
        eProductToLocation  ///< Map from the feature's product to location
    };

    /// Options for interpretations of locations
    enum EMapOptions {
        /// Ignore internal dense-seg structure - map each
        /// dense-seg according to the total ranges involved
        fAlign_Dense_seg_TotalRange = 0x01,

        /// Flags used to indicate mapping direction when mapping
        /// through a sparse-seg.
        fAlign_Sparse_ToFirst       = 0x00, ///< Map to first-id
        fAlign_Sparse_ToSecond      = 0x02, ///< Map to second-id
        
        /// Flag used when mapping through a seq-map (this includes
        /// mapping through a bioseq or a GC-assembly). If set, each
        /// call to Map() goes only one level up or down, unlike normal
        /// mode which maps from any level as far up/down as possible.
        /// The result of mapping can be mapped further by making another
        /// call to Map().
        fMapSingleLevel             = 0x04
    };
    typedef int TMapOptions;

    /// Spliced-seg row indexing constants.
    enum ESplicedRow {
        eSplicedRow_Prod = 0,
        eSplicedRow_Gen  = 1
    };

    enum FFuzzOption {
        // used for backwards compatibility with C toolkit's output.
        // TODO: we should remove this one day since the
        // normal output is superior.
        fFuzzOption_CStyle      = 1 << 0,
        // Don't set eLim_tl or eLim_tr and instead set greater than or less
        // than if appropriate.
        fFuzzOption_RemoveLimTlOrTr = 1 << 1
    };
    typedef int TFuzzOption;

    /// Mapping through a pre-filled CMappipngRanges.
    /// @param mapping_ranges
    ///  CMappingRanges filled with the desired source and destination
    ///  ranges. Must be a heap object (will be stored in a CRef<>).
    ///  NOTE: If the mapper is used with mixed sequence types, the
    ///  ranges must use genomic coordinates (for ranges on proteins
    ///  multiply all coordinates by 3).
    /// @param seq_info
    ///  Sequence type, length etc. provider. If any ids from the mapping
    ///  ranges are not available through this object, they must be
    ///  registered using SetSeqTypeById.
    /// @sa IMapper_Sequence_Info
    /// @sa SetSeqTypeById
    CSeq_loc_Mapper_Base(CMappingRanges*        mapping_ranges,
                         IMapper_Sequence_Info* seq_info = 0);

    /// Mapping through a feature, both location and product must be set.
    CSeq_loc_Mapper_Base(const CSeq_feat&       map_feat,
                         EFeatMapDirection      dir,
                         IMapper_Sequence_Info* seq_info = 0);

    /// Mapping between two seq_locs.
    CSeq_loc_Mapper_Base(const CSeq_loc&        source,
                         const CSeq_loc&        target,
                         IMapper_Sequence_Info* seq_info = 0);

    /// Mapping through an alignment. Need to specify target ID or
    /// target row of the alignment. Any other ID is mapped to the
    /// target one. Only the first row matching target ID is used,
    /// all other rows are considered source.
    /// If the alignment is a spliced-seg, the merging is set to
    /// 'merge by segment' automatically to merge mapped locations
    /// by exon.
    /// @sa SetMergeBySeg
    CSeq_loc_Mapper_Base(const CSeq_align&      map_align,
                         const CSeq_id&         to_id,
                         TMapOptions            opts = 0,
                         IMapper_Sequence_Info* seq_info = 0);
    /// Sparse alignments require special row indexing since each
    /// row contains two seq-ids. Use options to specify mapping
    /// direction.
    /// If the alignment is a spliced-seg, the merging is set to
    /// 'merge by segment' automatically to merge mapped locations
    /// by exon.
    /// @sa SetMergeBySeg
    CSeq_loc_Mapper_Base(const CSeq_align&      map_align,
                         size_t                 to_row,
                         TMapOptions            opts = 0,
                         IMapper_Sequence_Info* seq_info = 0);

    ~CSeq_loc_Mapper_Base(void);

    void SetFuzzOption( TFuzzOption newOption );

    /// Intervals' merging mode
    /// MergeNone and MergeAbutting do not change the order of ranges
    /// in the destination seq-loc. No ranges will be merged if they
    /// are separated by any other sub-range.
    /// MergeContained and MergeAll sort ranges before sorting, so that
    /// any overlapping ranges can be merged. The sorting takes the
    /// mapped location strand into account.
    /// NOTE: any merging (except None) is incompatible with collecting
    /// source ranges.
    /// @sa IncludeSourceLocs

    /// No merging
    CSeq_loc_Mapper_Base& SetMergeNone(void);
    /// Merge only abutting intervals, keep overlapping
    CSeq_loc_Mapper_Base& SetMergeAbutting(void);
    /// Merge only intervals from the same group. Group is created
    /// for each exon, dense-diag, std-seg and disc sub-alignment.
    /// For spliced segs this mode is turned on by default.
    CSeq_loc_Mapper_Base& SetMergeBySeg(void);
    /// Merge intervals only if one is completely covered by another
    CSeq_loc_Mapper_Base& SetMergeContained(void);
    /// Merge any abutting or overlapping intervals
    CSeq_loc_Mapper_Base& SetMergeAll(void);

    /// Wether to preserve or remove NULL sub-locations (usually
    /// indicating gaps) from the result. By default gaps are preserved.
    CSeq_loc_Mapper_Base& SetGapPreserve(void);
    CSeq_loc_Mapper_Base& SetGapRemove(void);

    /// Keep ranges which can not be mapped. Does not affect truncation
    /// of partially mapped ranges. By default nonmapping ranges are
    /// removed.
    CSeq_loc_Mapper_Base& KeepNonmappingRanges(void);
    CSeq_loc_Mapper_Base& TruncateNonmappingRanges(void);

    /// Check strands before mapping a range. By default strand is not
    /// checked and a range will be mapped even if its strand does not
    /// correspond to the strand of the mapping source.
    CSeq_loc_Mapper_Base& SetCheckStrand(bool value = true);

    /// Include source ranges in the mapped location. If turned
    /// on, the resulting seq-loc will be an equiv with the
    /// first sub-loc containing the usual mapped seq-loc, and
    /// the second one - the set of source locations used in the
    /// mapping.
    /// NOTE: this option is incompatible with any merging.
    /// Merging mode must be set to MergeNone.
    CSeq_loc_Mapper_Base& IncludeSourceLocs(bool value = true);

    /// Map seq-loc
    CRef<CSeq_loc>   Map(const CSeq_loc& src_loc);
    /// Map the whole alignment. Searches all rows for ranges
    /// which can be mapped.
    CRef<CSeq_align> Map(const CSeq_align& src_align);
    /// Map a single row of the alignment.
    CRef<CSeq_align> Map(const CSeq_align& src_align,
                         size_t            row);
    /// Map seq-graph. This will map both location and data.
    /// The data may be truncated to match the new location.
    CRef<CSeq_graph> Map(const CSeq_graph& src_graph);

    /// Map each object from the Seq-annot and replace the original
    /// with the mapped one.
    void Map(CSeq_annot& annot);

    /// Check if the last mapping resulted in partial location
    /// (not all ranges from the original location could be mapped
    /// to the target).
    bool LastIsPartial(void);

    typedef set<CSeq_id_Handle> TSynonyms;

    // Collect synonyms for the id, store mapping of each synonym
    // to the primary id. Returns primary id for the argument or the
    // argument itself.
    const CSeq_id_Handle& CollectSynonyms(const CSeq_id_Handle& id) const;

    // Sequence type - to recalculate coordinates.
    enum ESeqType {
        eSeq_unknown = 0,
        eSeq_nuc = 1,
        eSeq_prot = 3
    };

protected:

    // Get molecule type for the given id. The default implementation
    // returns eSeq_unknown. The overrided methods should return
    // real sequence type. The returned type is stored in the mapper's
    // cache. The method should not be called directly, use
    // GetSeqTypeById instead for it uses the cached types.
    // It's also a good idea to cache the same sequence type for all
    // synonyms in the overrided method to prevent multiple requests
    // to GetSeqType.
    ESeqType GetSeqType(const CSeq_id_Handle& idh) const;

    // Get sequence length for the given seq-id. Returns kInvalidSeqPos
    // if the length is unknown (the default behavior).
    TSeqPos GetSequenceLength(const CSeq_id& id);

    // Create CSeq_align_Mapper_Base, add any necessary arguments.
    virtual CSeq_align_Mapper_Base*
        InitAlignMapper(const CSeq_align& src_align);

    // Initialize the mapper from a feature. The feature must have
    // both location and product set, mapping direction is set by
    // the flag.
    void x_InitializeFeat(const CSeq_feat&  map_feat,
                          EFeatMapDirection dir);
    // Map between two locations. Optional frame is used by x_InitializeFeat()
    // only with cd-region features.
    void x_InitializeLocs(const CSeq_loc& source,
                          const CSeq_loc& target,
                          int             frame = 0);
    // Initialize the mapper from an alignment. Looks for the first
    // row containing the id and sets it as mapping target. All other
    // rows become mapping source.
    void x_InitializeAlign(const CSeq_align& map_align,
                           const CSeq_id&    to_id);
    // Initialize the mapper from an alignment, map to the specified row.
    void x_InitializeAlign(const CSeq_align& map_align,
                           size_t            to_row);

    // Create dummy mapping from the whole destination location to itself.
    // This will prevent truncation of ranges already on the target.
    // For some reason (?) the function is used only by CSeq_loc_Mapper,
    // not CSeq_loc_Mapper_Base, and only when initializing the mapper
    // from a bioseq handle or a seq-map. When mapping through a feature
    // or a pair of seq-locs it's not called and ranges on destination
    // are truncated or preserved the same way as any other non-mapping
    // ranges.
    void x_PreserveDestinationLocs(void);

    // Add new mapping range while initializing the mapper. The function
    // adjusts starts and lengths according to the used range and strand.
    void x_NextMappingRange(const CSeq_id&   src_id,
                            TSeqPos&         src_start,
                            TSeqPos&         src_len,
                            ENa_strand       src_strand,
                            const CSeq_id&   dst_id,
                            TSeqPos&         dst_start,
                            TSeqPos&         dst_len,
                            ENa_strand       dst_strand,
                            const CInt_fuzz* fuzz_from = 0,
                            const CInt_fuzz* fuzz_to = 0,
                            int              frame = 0,
                            TSeqPos          dst_total_len = kInvalidSeqPos,
                            TSeqPos          src_bioseq_len = kInvalidSeqPos);

    // Add new CMappingRange. This includes collecting all synonyms for the id,
    // creating a new mapping for each of them and updating the destination
    // ranges.
    void x_AddConversion(const CSeq_id& src_id,
                         TSeqPos        src_start,
                         ENa_strand     src_strand,
                         const CSeq_id& dst_id,
                         TSeqPos        dst_start,
                         ENa_strand     dst_strand,
                         TSeqPos        length,
                         bool           ext_right,
                         int            frame,
                         TSeqPos        dst_total_len,
                         TSeqPos        src_bioseq_len,
                         TSeqPos        dst_length );

    // Parse and map the seq-loc.
    void x_MapSeq_loc(const CSeq_loc& src_loc);

    // Convert collected ranges into a seq-loc and push it into the destination
    // seq-loc mix. This is done to preserve the original seq-loc structure
    // when possible (although some optimizations are done - see
    // x_OptimizeSeq_loc).
    void x_PushRangesToDstMix(void);

    typedef CMappingRange::TRange           TRange;
    typedef CMappingRanges::TRangeMap       TRangeMap;
    typedef CMappingRanges::TRangeIterator  TRangeIterator;
    typedef CMappingRanges::TSortedMappings TSortedMappings;

    // List and map of target ranges to construct target-to-target mapping
    typedef list<TRange>                    TDstRanges;
    typedef map<CSeq_id_Handle, TDstRanges> TDstIdMap;
    typedef vector<TDstIdMap>               TDstStrandMap;

    // Destination locations arranged by ID/range
    typedef CRef<CInt_fuzz>                 TFuzz;
    typedef pair<TFuzz, TFuzz>              TRangeFuzz;

    // Structure to hold information about mapped ranges until they are
    // converted to seq-loc parts.
    struct SMappedRange {
        SMappedRange(void) : group(0) {}
        SMappedRange(const TRange&      rg,
                     const TRangeFuzz&  fz,
                     int                grp = 0)
            : range(rg), fuzz(fz), group(grp) {}

        TRange      range;
        TRangeFuzz  fuzz;
        int         group; // used mostly to group ranges by exon

        bool operator<(const SMappedRange& rg) const
            {
                return range < rg.range;
            }
    };
    typedef list<SMappedRange>                   TMappedRanges;
    // Ranges grouped by strand. [0] contains ranges without strand,
    // [i] where i>0 stands for 'eNa_strand_XXXX + 1'.
    typedef vector<TMappedRanges>                TRangesByStrand;
    typedef map<CSeq_id_Handle, TRangesByStrand> TRangesById;
    typedef map<CSeq_id_Handle, ESeqType>        TSeqTypeById;

    typedef CSeq_align::C_Segs::TDendiag         TDendiag;
    typedef CSeq_align::C_Segs::TStd             TStd;

private:
    CSeq_loc_Mapper_Base(const CSeq_loc_Mapper_Base&);
    CSeq_loc_Mapper_Base& operator=(const CSeq_loc_Mapper_Base&);

    friend class CSeq_align_Mapper_Base;

    enum EMergeFlags {
        eMergeNone,      // no merging
        eMergeAbutting,  // merge only abutting intervals, keep overlapping
        eMergeContained, // merge if one range is contained in another
        eMergeBySeg,     // merge ranges by mapping group (e.g. by exon)
        eMergeAll        // merge both abutting and overlapping intervals
    };
    enum EGapFlags {
        eGapPreserve,    // Leave gaps as-is
        eGapRemove       // Remove gaps (NULL seq-locs)
    };

    // Check types of all sequences referenced by the location,
    // calculate the total length of the location, return true
    // if types are known for all sequences.
    // Set seqtype to the detected sequence type or to unknown
    // if the type can not be detected or there are multiple types.
    bool x_CheckSeqTypes(const CSeq_loc& loc,
                         ESeqType&       seqtype,
                         TSeqPos&        len) const;
    // If x_CheckSeqTypes returns false, it may indicate that some
    // sequence types could not be detected. In this case the mapper
    // will attempt to find at least one known type in the location
    // and force it for all sub-locations with unknown types.
    // The function will fail if there are diferent known types in the
    // same seq-loc.
    ESeqType x_ForceSeqTypes(const CSeq_loc& loc) const;

    // In some cases the mapper may fail to detect that both source
    // and destination locations are on proteins rather than on nucs.
    // CSeq_align_Mapper_Base may detect this mistake while mapping
    // an alignment. In this case it will try to change all types to
    // protein.
    void x_AdjustSeqTypesToProt(const CSeq_id_Handle& idh);

    // Get sequence length, try to get the real length for
    // reverse strand, do not use "whole".
    TSeqPos x_GetRangeLength(const CSeq_loc_CI& it);

    // Initialize the mapper from different alignment types.
    void x_InitAlign(const CDense_diag& diag, size_t to_row);
    void x_InitAlign(const CDense_seg& denseg, size_t to_row);
    void x_InitAlign(const CStd_seg& sseg, size_t to_row);
    void x_InitAlign(const CPacked_seg& pseg, size_t to_row);
    void x_InitSpliced(const CSpliced_seg& spliced,
                       const CSeq_id&      to_id);
    void x_InitSpliced(const CSpliced_seg& spliced, ESplicedRow to_row);
    void x_InitSparse(const CSparse_seg& sparse, int to_row);

    void x_IterateExonParts(const CSpliced_exon::TParts& parts,
                            ESplicedRow                  to_row,
                            const CSeq_id&               gen_id,
                            TSeqPos&                     gen_start,
                            TSeqPos&                     gen_len,
                            ENa_strand                   gen_strand,
                            const CSeq_id&               prod_id,
                            TSeqPos&                     prod_start,
                            TSeqPos&                     prod_len,
                            ENa_strand                   prod_strand);
    void x_AddExonPartsMapping(TSeqPos&        mapping_len,
                               ESplicedRow     to_row,
                               const CSeq_id&  gen_id,
                               TSeqPos&        gen_start,
                               TSeqPos&        gen_len,
                               ENa_strand      gen_strand,
                               const CSeq_id&  prod_id,
                               TSeqPos&        prod_start,
                               TSeqPos&        prod_len,
                               ENa_strand      prod_strand);
    // Helper method to simplify getting exon part length regardless of
    // its type.
    static TSeqPos sx_GetExonPartLength(const CSpliced_exon_chunk& part);

    // Map a single range from source to destination.
    bool x_MapNextRange(const TRange&     src_rg,
                        bool              is_set_strand,
                        ENa_strand        src_strand,
                        const TRangeFuzz& src_fuzz,
                        TSortedMappings&  mappings,
                        size_t            cvt_idx,
                        TSeqPos*          last_src_to);
    // Map the interval through all matching mappings.
    bool x_MapInterval(const CSeq_id&   src_id,
                       TRange           src_rg,
                       bool             is_set_strand,
                       ENa_strand       src_strand,
                       TRangeFuzz       orig_fuzz);
    // Set the flag to indicate that the last range was truncated
    // during mapping.
    void x_SetLastTruncated(void);

    // Pushes the location to the destination seq-loc mix.
    // See also x_PushRangesToDstMix.
    void x_PushLocToDstMix(CRef<CSeq_loc> loc);

    // Map the alignment. If row is NULL, map all rows. Otherwise
    // map only the selected row.
    CRef<CSeq_align> x_MapSeq_align(const CSeq_align& src_align,
                                    size_t*           row);

    // Get mapped ranges for the given id and strand index.
    // See TRangesByStrand for strand indexing.
    TMappedRanges& x_GetMappedRanges(const CSeq_id_Handle& id,
                                     size_t                strand_idx) const;
    // Push mapped range to the list of mapped ranges. Try to merge the new
    // range with the existing ones based on the selected merging mode.
    void x_PushMappedRange(const CSeq_id_Handle& id,
                           size_t                strand_idx,
                           const TRange&         range,
                           const TRangeFuzz&     fuzz,
                           bool                  push_reverse,
                           int                   group);
    // Store the source range just mapped. Used only if storing source
    // locations is enabled - see IncludeSourceLocs.
    void x_PushSourceRange(const CSeq_id_Handle& idh,
                           size_t                src_strand,
                           size_t                dst_strand,
                           const TRange&         range,
                           bool                  push_reverse);

    // Convert mapped range data to a seq-loc (point or interval).
    // Set fuzzes to indicate truncated range if necessary.
    CRef<CSeq_loc> x_RangeToSeq_loc(const CSeq_id_Handle& idh,
                                    TSeqPos               from,
                                    TSeqPos               to,
                                    size_t                strand_idx,
                                    TRangeFuzz            rg_fuzz);

    // Convert all collected and not yet converted mapped ranges to a seq-loc.
    // May be called multiple times while mapping a complex location and
    // storing its parts to a destination seq-loc mix (see
    // x_PushRangesToDstMix).
    CRef<CSeq_loc> x_GetMappedSeq_loc(void);

    // For mix locations, we remove fuzz from in-between the parts.
    void x_StripExtraneousFuzz(CRef<CSeq_loc>& loc) const;

    // This removes fuzz of type "range" if any.
    // Don't give this mix locations; it won't do anything.
    CConstRef<CSeq_loc> x_FixNonsenseFuzz( CConstRef<CSeq_loc> loc_piece ) const;

    // Try to optimize the mapped location if it's a mix.
    // The allowed optimizations are:
    // - empty mix is converted to Null
    // - if the mix contains a single element, use just this element
    // - if the mix contains only intervals, convert it to packed-int
    // When mapping a complex location (e.g. a multi-level mix) each
    // sub-location is optimized individually.
    void x_OptimizeSeq_loc(CRef<CSeq_loc>& loc) const;

    // Returns true if the new mapped range should be added to the
    // existing mapped ranges in the reverse order (in the front).
    // If merging is set to contained or all, used the provided strand
    // index to check the order of ranges. For all other merging modes
    // compares the directions of mapping source and target.
    bool x_ReverseRangeOrder(int str) const;

    // Map parts of a complex seq-loc.
    void x_Map_PackedInt_Element(const CSeq_interval& si);
    void x_Map_PackedPnt_Element(const CPacked_seqpnt& pp, TSeqPos p);

    // Get main seq-id for a synonym. If no mapping exists, returns the
    // original id.
    const CSeq_id_Handle& x_GetPrimaryId(const CSeq_id_Handle& synonym) const;

    typedef map<CSeq_id_Handle, CSeq_id_Handle> TSynonymMap;
    typedef map<CSeq_id_Handle, TSeqPos> TLengthMap;

    // How to merge mapped locations.
    EMergeFlags          m_MergeFlag;
    // How to treat gaps (Null sub-locations) if any.
    EGapFlags            m_GapFlag;
    // Wether to keep or discard ranges which can not be mapped.
    bool                 m_KeepNonmapping;
    // Wether to check or not if the original location is on the same strand
    // as the mapping source.
    bool                 m_CheckStrand;
    // Wether to include a source of each mapped range to the mapped seq-loc.
    bool                 m_IncludeSrcLocs;

    // Mapped ranges collected from the currently parsed sub-location.
    mutable TRangesById  m_MappedLocs;
    // Source locations for all mapped ranges.
    CRef<CSeq_loc>       m_SrcLocs;

    // Collected ranges for mapped graph. Used to adjust mapped graph data.
    CRef<CGraphRanges>   m_GraphRanges;

    // Map each synonym to a primary seq-id.
    mutable TSynonymMap  m_SynonymMap;

    // Map each primary seq-id to sequence length.
    mutable TLengthMap   m_LengthMap;

protected:
    // Storage for sequence types.
    mutable TSeqTypeById m_SeqTypes;
    // Flag indicating if the mapping truncated at least some ranges.
    bool                 m_Partial;
    // Flag indicating if the last range could not be mapped and was
    // dropped.
    bool                 m_LastTruncated;
    // Mapping ranges grouped by source id and strand.
    CRef<CMappingRanges> m_Mappings;
    // Mapped seq-loc
    CRef<CSeq_loc>       m_Dst_loc;
    // All ranges on the mapping destination.
    TDstStrandMap        m_DstRanges;
    // Current mapping group. Incremented for each mapping sub-location
    // (e.g. exon).
    int                  m_CurrentGroup;
    // Control how fuzz is generated and propagated
    TFuzzOption          m_FuzzOption;
    // Misc mapping options
    TMapOptions          m_MapOptions;

    // Sequence info provider
    mutable CRef<IMapper_Sequence_Info> m_SeqInfo;

public:
    // Initialize the mapper with default values
    CSeq_loc_Mapper_Base(IMapper_Sequence_Info* seqinfo = 0);

    /// Methods for getting sequence types, use cached types (m_SeqTypes)
    /// if possible.
    ESeqType GetSeqTypeById(const CSeq_id_Handle& idh) const;
    ESeqType GetSeqTypeById(const CSeq_id& id) const;
    /// Methods for setting sequence types. May be used to populate the
    /// cache before mapping huge alignments if the types are already
    /// known. Throw exception if the sequence type is already set to
    /// a different value.
    /// NOTE: setting sequence type does not adjust mapping ranges for this
    /// id. All mapping ranges must use genomic coordinates.
    void SetSeqTypeById(const CSeq_id_Handle& idh, ESeqType seqtype) const;
    void SetSeqTypeById(const CSeq_id& id, ESeqType seqtype) const;

    /// Get sequence width. Return 3 for proteins, 1 for nucleotides and
    /// unknown sequence types.
    int GetWidthById(const CSeq_id_Handle& idh) const;
    int GetWidthById(const CSeq_id& id) const;

    /// Get mapping ranges.
    const CMappingRanges& GetMappingRanges(void) const { return *m_Mappings; }

    /// NOTE: In most cases CollectSynonyms(const CSeq_id_Handle& id) should
    /// be used instead, since it takes care of synonym storage and mapping.
    /// This method does nothing but storing synonyms in the container.
    void CollectSynonyms(const CSeq_id_Handle& id,
                         TSynonyms&            synonyms) const;

};


/////////////////////////////////////////////////////////////////////////////
///
///  IMapper_Sequence_Info
///
///  Interface for providing sequence information to CSeq_loc_Mapper_Base.
///  Returns information about sequence type, length and synonyms.

class IMapper_Sequence_Info : public CObject
{
public:
    typedef CSeq_loc_Mapper_Base::ESeqType  TSeqType;
    typedef CSeq_loc_Mapper_Base::TSynonyms TSynonyms;

    /// Get information about sequence type (nuc or prot).
    virtual TSeqType GetSequenceType(const CSeq_id_Handle& idh) = 0;

    /// Get sequence length or kInvalidSeqPos.
    virtual TSeqPos GetSequenceLength(const CSeq_id_Handle& idh) = 0;

    /// Collect all synonyms for the id including the id itself.
    /// Any derived class must add at least the original id to the collection.
    virtual void CollectSynonyms(const CSeq_id_Handle& id,
                                 TSynonyms&            synonyms) = 0;
};


struct CMappingRangeRef_Less
{
    bool operator()(const CRef<CMappingRange>& x,
                    const CRef<CMappingRange>& y) const;
};


struct CMappingRangeRef_LessRev
{
    bool operator()(const CRef<CMappingRange>& x,
                    const CRef<CMappingRange>& y) const;
};


inline
bool CMappingRangeRef_Less::operator()(const CRef<CMappingRange>& x,
                                       const CRef<CMappingRange>& y) const
{
    // Leftmost first
    if (x->m_Src_from != y->m_Src_from) {
        return x->m_Src_from < y->m_Src_from;
    }
    // Longest first
    if (x->m_Src_to != y->m_Src_to) {
        return x->m_Src_to > y->m_Src_to;
    }
    return x < y;
}


inline
bool CMappingRangeRef_LessRev::operator()(const CRef<CMappingRange>& x,
                                          const CRef<CMappingRange>& y) const
{
    // Rightmost first
    if (x->m_Src_to != y->m_Src_to) {
        return x->m_Src_to > y->m_Src_to;
    }
    // Longest first
    if (x->m_Src_from != y->m_Src_from) {
        return x->m_Src_from < y->m_Src_from;
    }
    return x > y;
}


inline
bool CMappingRange::GoodSrcId(const CSeq_id& id) const
{
    return m_Src_id_Handle == id;
}


inline
CRef<CSeq_id> CMappingRange::GetDstId(void) const
{
    return m_Dst_id_Handle ?
        Ref(&const_cast<CSeq_id&>(*m_Dst_id_Handle.GetSeqId())) :
        CRef<CSeq_id>(0);
}


inline
CSeq_loc_Mapper_Base& CSeq_loc_Mapper_Base::SetMergeNone(void)
{
    m_MergeFlag = eMergeNone;
    return *this;
}


inline
CSeq_loc_Mapper_Base& CSeq_loc_Mapper_Base::SetMergeAbutting(void)
{
    m_MergeFlag = eMergeAbutting;
    return *this;
}


inline
CSeq_loc_Mapper_Base& CSeq_loc_Mapper_Base::SetMergeBySeg(void)
{
    m_MergeFlag = eMergeBySeg;
    return *this;
}


inline
CSeq_loc_Mapper_Base& CSeq_loc_Mapper_Base::SetMergeContained(void)
{
    m_MergeFlag = eMergeContained;
    return *this;
}


inline
CSeq_loc_Mapper_Base& CSeq_loc_Mapper_Base::SetMergeAll(void)
{
    m_MergeFlag = eMergeAll;
    return *this;
}


inline
CSeq_loc_Mapper_Base& CSeq_loc_Mapper_Base::SetGapPreserve(void)
{
    m_GapFlag = eGapPreserve;
    return *this;
}


inline
CSeq_loc_Mapper_Base& CSeq_loc_Mapper_Base::SetGapRemove(void)
{
    m_GapFlag = eGapRemove;
    return *this;
}


inline
CSeq_loc_Mapper_Base& CSeq_loc_Mapper_Base::SetCheckStrand(bool value)
{
    m_CheckStrand = value;
    return *this;
}


inline
bool CSeq_loc_Mapper_Base::LastIsPartial(void)
{
    return m_Partial;
}


inline
CSeq_loc_Mapper_Base& CSeq_loc_Mapper_Base::KeepNonmappingRanges(void)
{
    m_KeepNonmapping = true;
    return *this;
}


inline
CSeq_loc_Mapper_Base& CSeq_loc_Mapper_Base::TruncateNonmappingRanges(void)
{
    m_KeepNonmapping = false;
    return *this;
}


inline
CSeq_loc_Mapper_Base& CSeq_loc_Mapper_Base::IncludeSourceLocs(bool value)
{
    m_IncludeSrcLocs = value;
    return *this;
}


inline
CRef<CSeq_align> CSeq_loc_Mapper_Base::Map(const CSeq_align& src_align)
{
    return x_MapSeq_align(src_align, 0);
}


inline
CRef<CSeq_align> CSeq_loc_Mapper_Base::Map(const CSeq_align& src_align,
                                           size_t            row)
{
    return x_MapSeq_align(src_align, &row);
}


inline
CSeq_loc_Mapper_Base::ESeqType
CSeq_loc_Mapper_Base::GetSeqTypeById(const CSeq_id_Handle& idh) const
{
    CSeq_id_Handle primary_id = CollectSynonyms(idh);
    TSeqTypeById::const_iterator it = m_SeqTypes.find(primary_id);
    if (it != m_SeqTypes.end()) {
        return it->second;
    }
    return GetSeqType(primary_id);
}


inline
CSeq_loc_Mapper_Base::ESeqType
CSeq_loc_Mapper_Base::GetSeqTypeById(const CSeq_id& id) const
{
    return GetSeqTypeById(CSeq_id_Handle::GetHandle(id));
}


inline
void CSeq_loc_Mapper_Base::SetSeqTypeById(const CSeq_id& id,
                                          ESeqType       seqtype) const
{
    SetSeqTypeById(CSeq_id_Handle::GetHandle(id), seqtype);
}


inline
int CSeq_loc_Mapper_Base::GetWidthById(const CSeq_id_Handle& idh) const
{
    return (GetSeqTypeById(idh) == eSeq_prot) ? 3 : 1;
}


inline
int CSeq_loc_Mapper_Base::GetWidthById(const CSeq_id& id) const
{
    return GetWidthById(CSeq_id_Handle::GetHandle(id));
}


/* @} */


END_SCOPE(objects)
END_NCBI_SCOPE

#endif  // SEQ_LOC_MAPPER_BASE__HPP
