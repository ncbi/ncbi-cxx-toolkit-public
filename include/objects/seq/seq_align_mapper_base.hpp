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

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CDense_seg;
class CPacked_seg;
class CSeq_align_set;
class CSpliced_seg;
class CSparse_seg;
class CMappingRange;
class CSeq_loc_Mapper_Base;

struct NCBI_SEQ_EXPORT SAlignment_Segment
{
    struct NCBI_SEQ_EXPORT SAlignment_Row
    {
        SAlignment_Row(void);

        void SetMapped(void);
        int GetSegStart(void) const;
        bool SameStrand(const SAlignment_Row& r) const;

        CSeq_id_Handle m_Id;
        TSeqPos        m_Start; ///< kInvalidSeqPos means skipped segment
        bool           m_IsSetStrand;
        ENa_strand     m_Strand;
        int            m_Width; ///< not stored in ASN.1, width of a character
        bool           m_Mapped;
        int            m_Frame;
    };
    typedef vector<SAlignment_Row> TRows;

    SAlignment_Segment(int len, size_t dim);

    SAlignment_Row& GetRow(size_t idx);
    SAlignment_Row& CopyRow(size_t idx, const SAlignment_Row& src_row);
    SAlignment_Row& AddRow(size_t         idx,
                           const CSeq_id& id,
                           int            start,
                           bool           is_set_strand,
                           ENa_strand     strand,
                           int            width,
                           int            frame);
    SAlignment_Row& AddRow(size_t                idx,
                           const CSeq_id_Handle& id,
                           int                   start,
                           bool                  is_set_strand,
                           ENa_strand            strand,
                           int                   width,
                           int                   frame);

    typedef vector< CRef<CScore> > TScores;
    typedef CSpliced_exon::TParts  TParts;

    void SetScores(const TScores& scores);

    int       m_Len;
    TRows     m_Rows;
    bool      m_HaveStrands;
    TScores   m_Scores;
    TParts    m_Parts; // Used in splised-seg to store chunks
};


class NCBI_SEQ_EXPORT CSeq_align_Mapper_Base : public CObject
{
public:
    typedef CSeq_align::C_Segs::TDendiag TDendiag;
    typedef CSeq_align::C_Segs::TStd TStd;

    enum EWidthFlag {
        eWidth_NoMap,  // map sequences' widths
        eWidth_Map     // do not map sequences' widths
    };

    CSeq_align_Mapper_Base(const CSeq_align& align,
                           EWidthFlag        map_widths);
    ~CSeq_align_Mapper_Base(void);

    /// Map the whole alignment
    void Convert(CSeq_loc_Mapper_Base& mapper);
    /// Map a single row of the alignment
    void Convert(CSeq_loc_Mapper_Base& mapper,
                 size_t                row);

    CRef<CSeq_align> GetDstAlign(void) const;

    size_t GetDim(void) const;
    const CSeq_id_Handle& GetRowId(size_t idx) const;

    typedef list<SAlignment_Segment>   TSegments;
    const TSegments& GetSegments() const;

protected:
    CSeq_align_Mapper_Base(void);

    virtual int GetSeqWidth(const CSeq_id& id) const;
    virtual CSeq_align_Mapper_Base* CreateSubAlign(const CSeq_align& align,
                                                   EWidthFlag map_widths);

    void x_Init(const CSeq_align& align);
    SAlignment_Segment& x_InsertSeg(TSegments::iterator& where,
                                    int                  len,
                                    size_t               dim);

private:

    // Segment insertion functions
    SAlignment_Segment& x_PushSeg(int len, size_t dim);

    void x_Init(const TDendiag& diags);
    void x_Init(const CDense_seg& denseg);
    void x_Init(const TStd& sseg);
    void x_Init(const CPacked_seg& pseg);
    void x_Init(const CSeq_align_set& align_set);
    void x_Init(const CSpliced_seg& spliced);
    void x_Init(const CSparse_seg& sparse);

    // Mapping through CSeq_loc_Mapper
    void x_ConvertAlign(CSeq_loc_Mapper_Base& mapper, size_t* row);
    void x_ConvertRow(CSeq_loc_Mapper_Base& mapper,
                      size_t                row);
    CSeq_id_Handle x_ConvertSegment(TSegments::iterator&  seg_it,
                                    CSeq_loc_Mapper_Base& mapper,
                                    size_t                row);

    typedef vector<ENa_strand> TStrands;
    void x_FillKnownStrands(TStrands& strands) const;
    void x_CopyParts(const SAlignment_Segment& src,
                     SAlignment_Segment&       dst,
                     TSeqPos                   from);

    void x_GetDstDendiag(CRef<CSeq_align>& dst) const;
    void x_GetDstDenseg(CRef<CSeq_align>& dst) const;
    void x_GetDstStd(CRef<CSeq_align>& dst) const;
    void x_GetDstPacked(CRef<CSeq_align>& dst) const;
    void x_GetDstDisc(CRef<CSeq_align>& dst) const;
    void x_GetDstSpliced(CRef<CSeq_align>& dst) const;
    void x_GetDstSparse(CRef<CSeq_align>& dst) const;

    // Special case: have to convert multi-id alignments to disc.
    void x_ConvToDstDisc(CRef<CSeq_align>& dst) const;
    int x_GetPartialDenseg(CRef<CSeq_align>& dst,
                           int start_seg) const;

    CConstRef<CSeq_align>        m_OrigAlign;
    bool                         m_HaveStrands;
    bool                         m_HaveWidths;
    bool                         m_OnlyNucs;
    size_t                       m_Dim;

protected:
    // Used for e_Disc alignments
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

    mutable CRef<CSeq_align>     m_DstAlign;
    TSubAligns                   m_SubAligns;
    TSegments                    m_Segs;
    bool                         m_MapWidths;
    EAlignFlags                  m_AlignFlags;
};


inline
SAlignment_Segment::SAlignment_Row::SAlignment_Row(void)
    : m_Start(kInvalidSeqPos),
      m_IsSetStrand(false),
      m_Strand(eNa_strand_unknown),
      m_Width(0),
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


END_SCOPE(objects)
END_NCBI_SCOPE

#endif  // SEQ_ALIGN_MAPPER_BASE__HPP
