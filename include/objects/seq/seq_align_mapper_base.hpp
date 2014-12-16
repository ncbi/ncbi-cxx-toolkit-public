#ifndef SEQ_ALIGN_MAPPER_BASE__HPP
#define SEQ_ALIGN_MAPPER_BASE__HPP

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

#include <objects/seq/seq_id_handle.hpp>
#include <objects/seq/annot_mapper_exception.hpp>
#include <objects/seqloc/Na_strand.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Score.hpp>
#include <objects/seqalign/Spliced_exon.hpp>
#include <objects/seqalign/Spliced_exon_chunk.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CDense_seg;
class CPacked_seg;
class CSeq_align_set;
class CSpliced_seg;
class CSparse_seg;
class CMappingRange;
class CSeq_loc_Mapper_Base;

/// Structure to hold information about a single alignment segment.
/// Used internally by CSeq_align_Mapper_Base.
struct NCBI_SEQ_EXPORT SAlignment_Segment
{
    /// Single row of a single alignment segment.
    struct NCBI_SEQ_EXPORT SAlignment_Row
    {
        SAlignment_Row(void);

        /// Mark the row as mapped. Some rows or their parts are just
        /// copied without real mapping. Setting this flag indicates
        /// that the segment/row matched some mapping and was converted.
        void SetMapped(void);

        /// Get segment start or -1 if it's a gap. The wrapper is required
        /// mostly to convert kInvalidSeqPos to -1 (used in alignments).
        int GetSegStart(void) const;

        /// Check if the query row has the same strand orientation.
        bool SameStrand(const SAlignment_Row& r) const;

        CSeq_id_Handle m_Id;          ///< Row's seq-id
        TSeqPos        m_Start;       ///< kInvalidSeqPos means gap
        bool           m_IsSetStrand; ///< Is strand set for the row?
        ENa_strand     m_Strand;      ///< Strand value
        bool           m_Mapped;      ///< Flag indicating mapped rows
    };
    typedef vector<SAlignment_Row> TRows;

    /// Create a new segment with the given length and number of rows.
    SAlignment_Segment(int len, size_t dim);

    /// Get row data with the given index.
    SAlignment_Row& GetRow(size_t idx);
    /// Create a copy of the given row, store is to this segment as
    /// row number 'idx'. The source row may originate from a different
    /// segment. Used to split segments when a row is truncated by mapping.
    /// NOTE: the rows vector must already have entry [idx].
    SAlignment_Row& CopyRow(size_t idx, const SAlignment_Row& src_row);
    /// Add new row.
    SAlignment_Row& AddRow(size_t         idx,
                           const CSeq_id& id,
                           int            start,
                           bool           is_set_strand,
                           ENa_strand     strand);
    /// Add new row.
    SAlignment_Row& AddRow(size_t                idx,
                           const CSeq_id_Handle& id,
                           int                   start,
                           bool                  is_set_strand,
                           ENa_strand            strand);

    typedef vector< CRef<CScore> > TScores;
    typedef CSpliced_exon_chunk::E_Choice TPartType;

    int       m_Len;            ///< Segment length
    TRows     m_Rows;           ///< Segment rows
    bool      m_HaveStrands;    ///< Do at least some rows have strand set?
    TScores   m_Scores;         ///< Scores for this segment
    int       m_GroupIdx;       ///< Group of segments (e.g. an exon)
    /// Group of scores. Set when several segments share the same set of
    /// scores. Currently used only for sparse-segs. -1 = unassigned.
    int       m_ScoresGroupIdx;

    // Used only for spliced exon parts to indicate their type.
    TPartType m_PartType;
};


/// Class used to map seq-alignments. Parses, maps and generates alignments.
/// Does not contain mapping information and can be used only with an instance
/// of CSeq_loc_Mapper_Base class. The seq-loc mapper is also used to retrieve
/// information about types of sequences.
class NCBI_SEQ_EXPORT CSeq_align_Mapper_Base : public CObject
{
public:
    typedef CSeq_align::C_Segs::TDendiag TDendiag;
    typedef CSeq_align::C_Segs::TStd TStd;

    CSeq_align_Mapper_Base(const CSeq_align&     align,
                           CSeq_loc_Mapper_Base& loc_mapper);
    ~CSeq_align_Mapper_Base(void);

    /// Map the whole alignment through the linked seq-loc mapper.
    void Convert(void);
    /// Map a single row of the alignment through the linked seq-loc mapper.
    void Convert(size_t row);

    /// Create mapped alignment.
    CRef<CSeq_align> GetDstAlign(void) const;

    /// Some of the following methods use only the first segment to get
    /// information about rows. They do not check if this information is
    /// consistent through all segments, but it should be.

    /// Get maximal number of rows in the alignment's segments.
    size_t GetDim(void) const { return m_Dim; }
    /// Get seq-id for the given row. Throw exception if the row
    /// does not exist. The function uses row id from the first segment.
    /// Other segments may have different id for the same row.
    const CSeq_id_Handle& GetRowId(size_t idx) const;

    typedef list<SAlignment_Segment>   TSegments;

    /// Get parsed segments. There is no storage for the original set of
    /// segments - it's modified during the mapping to produce mapped
    /// alignment.
    const TSegments& GetSegments() const;

protected:
    CSeq_align_Mapper_Base(CSeq_loc_Mapper_Base& loc_mapper);

    // Get the linked seq-loc mapper
    CSeq_loc_Mapper_Base& GetLocMapper(void) const { return m_LocMapper; }

    /// Get max number of rows of all segments
    size_t GetMaxDim(void) const;

    // Create sub-mapper to map sub-alignment. Used to map nested alignments.
    virtual CSeq_align_Mapper_Base*
        CreateSubAlign(const CSeq_align& align);
    // Create sub-mapper to map a single spliced-seg exon. Each exon is mapped
    // by a separate sub-mapper.
    virtual CSeq_align_Mapper_Base*
        CreateSubAlign(const CSpliced_seg&  spliced,
                       const CSpliced_exon& exon);
    // Initialize the mapper with the exon.
    void InitExon(const CSpliced_seg&  spliced,
                  const CSpliced_exon& exon);

    // Initialize the mapper with the seq-align.
    void x_Init(const CSeq_align& align);
    // Add new segment before the specified position.
    // Required to split segments which can not be mapped as a whole.
    SAlignment_Segment& x_InsertSeg(TSegments::iterator& where,
                                    int                  len,
                                    size_t               dim,
                                    bool                 reverse);
    // Reset scores for the given segment and/or for the whole alignment.
    // This always resets global scores. Segment scores are reset only if
    // the segment is not NULL.
    // Resetting scores is done when a segment needs to be truncated (split)
    // because this operation makes them invalid.
    void x_InvalidateScores(SAlignment_Segment* seg = NULL);

private:

    // Add new alignment segment. Sorting depends on the strand.
    SAlignment_Segment& x_PushSeg(int len, size_t dim,
        ENa_strand strand = eNa_strand_unknown);

    // Initialization methods for different alignment types.
    void x_Init(const TDendiag& diags);
    void x_Init(const CDense_seg& denseg);
    void x_Init(const TStd& sseg);
    void x_Init(const CPacked_seg& pseg);
    void x_Init(const CSeq_align_set& align_set);
    void x_Init(const CSpliced_seg& spliced);
    void x_Init(const CSparse_seg& sparse);

    // Mapping through CSeq_loc_Mapper_Base

    // Map the whole alignment. If row is set, map only this row.
    // Otherwise iterate all rows and try to map each of them.
    void x_ConvertAlign(size_t* row);
    // Map a single alignment row. Iterates all segments of the given row.
    void x_ConvertRow(size_t row);
    // Map a single segment of the given row. The iterator is advanced
    // to the next segment to be mapped. Additional segments may be
    // inserted before the new iterator position if the mapping is partial
    // and the original segment is split.
    CSeq_id_Handle x_ConvertSegment(TSegments::iterator& seg_it,
                                    size_t               row);

    // Scan all rows for ranges with strands, store the result.
    // If the strand info can not be found, plus strand is used.
    // The collected strands are used in gaps (in the alignments where
    // strand can not be left unset). The method does not check consistency
    // of strands in the whole row - it's not required in this case.
    typedef vector<ENa_strand> TStrands;
    void x_FillKnownStrands(TStrands& strands) const;

    // Create mapped alignment.
    void x_GetDstDendiag(CRef<CSeq_align>& dst) const;
    void x_GetDstDenseg(CRef<CSeq_align>& dst) const;
    void x_GetDstStd(CRef<CSeq_align>& dst) const;
    void x_GetDstPacked(CRef<CSeq_align>& dst) const;
    void x_GetDstDisc(CRef<CSeq_align>& dst) const;
    void x_GetDstSpliced(CRef<CSeq_align>& dst) const;
    void x_GetDstSparse(CRef<CSeq_align>& dst) const;

    // Create mapped exon and add it to the spliced-seg.
    // 'seg' is the segment to start with (the original exon could be split).
    // 'gen_id' and 'prod_id' are used to return exon level seq-ids.
    // 'gen_strand' and 'prod_strand' are used to return exon level strands.
    // 'partial' indicates if the original exon was truncated.
    // 'last_gen_id' and 'last_prod_id' provide the ids found in previous
    // exons (if any).
    // Return true if an exon was added to the spliced-seg, false otherwise.
    bool x_GetDstExon(CSpliced_seg&              spliced,
                      TSegments::const_iterator& seg,
                      CSeq_id_Handle&            gen_id,
                      CSeq_id_Handle&            prod_id,
                      ENa_strand&                gen_strand,
                      ENa_strand&                prod_strand,
                      bool&                      last_exon_partial,
                      const CSeq_id_Handle&      last_gen_id,
                      const CSeq_id_Handle&      last_prod_id) const;
    // Adds new part to the exon. If last part had the same type, it is
    // merged with the new one.
    void x_PushExonPart(CRef<CSpliced_exon_chunk>&    last_part,
                        CSpliced_exon_chunk::E_Choice part_type,
                        int                           part_len,
                        CSpliced_exon&                exon) const;

    // Some mapping results can not be represented by the original alignment
    // type (e.g. when a row contains multiple ids). In this case the result
    // is converted to to disc.
    void x_ConvToDstDisc(CRef<CSeq_align>& dst) const;
    // Get the next part of the disc align - see x_ConvToDstDisc.
    int x_GetPartialDenseg(CRef<CSeq_align>& dst,
                           int               start_seg) const;

    // Collect exons from a single sub-alignment.
    void x_GetDstSplicedSubAlign(CSpliced_seg&                 spliced,
                                 const CSeq_align_Mapper_Base& sub_align,
                                 bool&                         last_exon_partial,
                                 CSeq_id_Handle&               gen_id,
                                 CSeq_id_Handle&               last_gen_id,
                                 bool&                         single_gen_id,
                                 ENa_strand&                   gen_strand,
                                 bool&                         single_gen_str,
                                 CSeq_id_Handle&               prod_id,
                                 CSeq_id_Handle&               last_prod_id,
                                 bool&                         single_prod_id,
                                 ENa_strand&                   prod_strand,
                                 bool&                         single_prod_str,
                                 bool&                         partial) const;

    // Check if both nucs and prots are present in the segments.
    bool x_HaveMixedSeqTypes(void) const;
    // Check if each row contains only one strand.
    bool x_HaveMixedStrand(void) const;

    // Check if the mapped alignment has at least one segment with at least
    // two non-gap rows.
    bool x_IsEmpty(void) const;

    CSeq_loc_Mapper_Base&        m_LocMapper;
    // Original alignment
    CConstRef<CSeq_align>        m_OrigAlign;
    // Original exon when mapping a splices seg through multiple mappers
    CConstRef<CSpliced_exon>     m_OrigExon;
    // Flag indicating if the original alignment contains any strands
    bool                         m_HaveStrands;
    // Number of rows in the original alignment (sometimes hard to calculate).
    size_t                       m_Dim;

    // Alignment scores
    typedef SAlignment_Segment::TScores TScores;
    typedef vector<TScores>             TScoresGroups;

    // Global seq-align scores.
    TScores                      m_AlignScores;
    // Seq-align.segs scores.
    TScores                      m_SegsScores;
    // Group scores (e.g. per-exon).
    TScoresGroups                m_GroupScores;
    // Flag used to invalidate parent's scores if any of the children
    // is invalidated.
    bool                         m_ScoresInvalidated;

protected:
    // Used for nested alignments - a set of child mappers, each mapping
    // its own sub-alignment.
    typedef vector< CRef<CSeq_align_Mapper_Base> >  TSubAligns;

    // Flags to indicate possible destination alignment types:
    // multi-dim or multi-id alignments can be packed into std-seg
    // or dense-diag only.
    enum EAlignFlags {
        eAlign_Normal,      // Normal alignment, may be packed into any type
        eAlign_Empty,       // Empty alignment
        eAlign_MultiId,     // A row contains different IDs
        eAlign_MultiDim     // Segments have different number of rows
    };

    mutable CRef<CSeq_align>     m_DstAlign;   // Mapped alignment
    TSubAligns                   m_SubAligns;  // Sub-mappers
    mutable TSegments            m_Segs;       // Parsed segments
    EAlignFlags                  m_AlignFlags; // Spesial case flags
};


inline
SAlignment_Segment::SAlignment_Row::SAlignment_Row(void)
    : m_Start(kInvalidSeqPos),
      m_IsSetStrand(false),
      m_Strand(eNa_strand_unknown),
      m_Mapped(false)
{
    return;
}


inline
void SAlignment_Segment::SAlignment_Row::SetMapped(void)
{
    m_Mapped = true;
}


inline
bool SAlignment_Segment::SAlignment_Row::
SameStrand(const SAlignment_Row& r) const
{
    return SameOrientation(m_Strand, r.m_Strand);
}


inline
int SAlignment_Segment::SAlignment_Row::GetSegStart(void) const
{
    return m_Start != kInvalidSeqPos ? int(m_Start) : -1;
}


inline
const CSeq_align_Mapper_Base::TSegments&
CSeq_align_Mapper_Base::GetSegments(void) const
{
    return m_Segs;
}


inline
size_t
CSeq_align_Mapper_Base::GetMaxDim(void) const
{
    return m_Dim;
}


END_SCOPE(objects)
END_NCBI_SCOPE

#endif  // SEQ_ALIGN_MAPPER_BASE__HPP
