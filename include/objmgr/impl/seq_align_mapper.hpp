#ifndef SEQ_ALIGN_MAPPER__HPP
#define SEQ_ALIGN_MAPPER__HPP

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
*   Alignment mapper
*
*/

#include <objects/seq/seq_id_handle.hpp>
#include <objmgr/impl/seq_loc_cvt.hpp>
#include <objects/seqloc/Na_strand.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Score.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CDense_seg;
class CPacked_seg;
class CSeq_align_set;
class CMappingRange;
class CSeq_loc_Mapper;

struct NCBI_XOBJMGR_EXPORT SAlignment_Segment
{
    struct NCBI_XOBJMGR_EXPORT SAlignment_Row
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
    };
    typedef vector<SAlignment_Row> TRows;

    SAlignment_Segment(int len, size_t dim);

    SAlignment_Row& GetRow(size_t idx);
    SAlignment_Row& CopyRow(size_t idx, const SAlignment_Row& src_row);
    SAlignment_Row& AddRow(size_t idx,
                           const CSeq_id& id,
                           int start,
                           bool is_set_strand,
                           ENa_strand strand,
                           int width);
    SAlignment_Row& AddRow(size_t idx,
                           const CSeq_id_Handle& id,
                           int start,
                           bool is_set_strand,
                           ENa_strand strand,
                           int width);

    typedef vector< CRef<CScore> >     TScores;

    void SetScores(const TScores& scores);

    int       m_Len;
    TRows     m_Rows;
    bool      m_HaveStrands;
    TScores   m_Scores;
};


class NCBI_XOBJMGR_EXPORT CSeq_align_Mapper : public CObject
{
public:
    typedef CSeq_align::C_Segs::TDendiag TDendiag;
    typedef CSeq_align::C_Segs::TStd TStd;

    CSeq_align_Mapper(const CSeq_align& align);
    ~CSeq_align_Mapper(void) {}

    void Convert(CSeq_loc_Conversion_Set& cvts,
                 unsigned int loc_index_shift = 0);
    void Convert(CSeq_loc_Mapper& mapper);

    CRef<CSeq_align> GetDstAlign(void) const;

private:
    typedef list<SAlignment_Segment>   TSegments;

    // Segment insertion functions
    SAlignment_Segment& x_PushSeg(int len, size_t dim);
    SAlignment_Segment& x_InsertSeg(TSegments::iterator& where,
                                    int len,
                                    size_t dim);

    void x_Init(const TDendiag& diags);
    void x_Init(const CDense_seg& denseg);
    void x_Init(const TStd& sseg);
    void x_Init(const CPacked_seg& pseg);
    void x_Init(const CSeq_align_set& align_set);

    typedef CSeq_loc_Conversion_Set::TRange       TRange;
    typedef CSeq_loc_Conversion_Set::TRangeMap    TRangeMap;
    typedef CSeq_loc_Conversion_Set::TIdMap       TIdMap;
    typedef CSeq_loc_Conversion_Set::TConvByIndex TConvByIndex;

    // Mapping through CSeq_loc_Conversion
    void x_ConvertAlign(CSeq_loc_Conversion_Set& cvts,
                        unsigned int loc_index_shift);
    void x_ConvertRow(CSeq_loc_Conversion& cvt,
                      size_t row);
    void x_ConvertRow(TIdMap& cvts,
                      size_t row);
    CSeq_id_Handle x_ConvertSegment(TSegments::iterator& seg_it,
                                    CSeq_loc_Conversion& cvt,
                                    size_t row);
    CSeq_id_Handle x_ConvertSegment(TSegments::iterator& seg_it,
                                    TIdMap& id_map,
                                    size_t row);

    // Mapping through CSeq_loc_Mapper
    void x_ConvertAlign(CSeq_loc_Mapper& mapper);
    void x_ConvertRow(CSeq_loc_Mapper& mapper,
                      size_t row);
    CSeq_id_Handle x_ConvertSegment(TSegments::iterator& seg_it,
                                    CSeq_loc_Mapper& mapper,
                                    size_t row);

    void x_GetDstDendiag(CRef<CSeq_align>& dst) const;
    void x_GetDstDenseg(CRef<CSeq_align>& dst) const;
    void x_GetDstStd(CRef<CSeq_align>& dst) const;
    void x_GetDstPacked(CRef<CSeq_align>& dst) const;
    void x_GetDstDisc(CRef<CSeq_align>& dst) const;

    // Used for e_Disc alignments
    typedef vector<CSeq_align_Mapper>  TSubAligns;

    // Flags to indicate possible destination alignment types:
    // multi-dim or multi-id alignments can be packed into std-seg
    // or dense-diag only.
    enum EAlignFlags {
        eAlign_Normal,      // Normal alignment, may be packed into any type
        eAlign_Empty,       // Empty alignment
        eAlign_MultiId,     // A row contains different IDs
        eAlign_MultiDim     // Segments have different number of rows
    };

    CConstRef<CSeq_align>        m_OrigAlign;
    mutable CRef<CSeq_align>     m_DstAlign;
    TSegments                    m_Segs;
    TSubAligns                   m_SubAligns;
    bool                         m_HaveStrands;
    bool                         m_HaveWidths;
    size_t                       m_Dim;
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
    return m_IsSetStrand  &&  SameOrientation(m_Strand, r.m_Strand);
}


inline
int SAlignment_Segment::SAlignment_Row::GetSegStart(void) const
{
    return m_Start != kInvalidSeqPos ? int(m_Start) : -1;
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.10  2005/03/29 19:21:56  jcherry
* Added export specifiers
*
* Revision 1.9  2004/07/12 15:05:31  grichenk
* Moved seq-id mapper from xobjmgr to seq library
*
* Revision 1.8  2004/06/15 14:01:24  vasilche
* Fixed int <> unsigned conversion warning.
*
* Revision 1.7  2004/05/26 14:57:36  ucko
* Change CSeq_align_Mapper::m_Dim's type from unsigned int to size_t
* for consistency with STL containers' size() methods.
*
* Revision 1.6  2004/05/26 14:29:20  grichenk
* Redesigned CSeq_align_Mapper: preserve non-mapping intervals,
* fixed strands handling, improved performance.
*
* Revision 1.5  2004/05/13 19:10:57  grichenk
* Preserve scores in mapped alignments
*
* Revision 1.4  2004/05/10 20:22:24  grichenk
* Fixed more warnings (removed inlines)
*
* Revision 1.3  2004/04/12 14:35:59  grichenk
* Fixed mapping of alignments between nucleotides and proteins
*
* Revision 1.2  2004/04/07 18:36:12  grichenk
* Fixed std-seg mapping
*
* Revision 1.1  2004/03/30 15:40:34  grichenk
* Initial revision
*
*
* ===========================================================================
*/

#endif  // SEQ_ALIGN_MAPPER__HPP
