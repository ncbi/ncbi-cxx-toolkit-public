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

#include <objmgr/seq_id_handle.hpp>
#include <objects/seqloc/Na_strand.hpp>
#include <objects/seqalign/Seq_align.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CDense_seg;
class CPacked_seg;
class CSeq_align_set;
class CSeq_loc_Conversion;
class CMappingRange;

struct SAlignment_Segment
{
    struct SAlignment_Row
    {
        SAlignment_Row(const CSeq_id& id,
                       int start,
                       bool is_set_strand,
                       ENa_strand strand,
                       int width);

        void SetMapped(void);

        CSeq_id_Handle m_Id;
        TSeqPos        m_Start; ///< kInvalidSeqPos means skipped segment
        bool           m_IsSetStrand;
        ENa_strand     m_Strand;
        int            m_Width; ///< not stored in ASN.1, width of a character
        bool           m_Mapped;
    };
    typedef vector<SAlignment_Row> TRows;

    SAlignment_Segment(int len);

    SAlignment_Row& AddRow(const CSeq_id& id,
                           int start,
                           bool is_set_strand,
                           ENa_strand strand,
                           int width);

    typedef vector<SAlignment_Segment> TSegments;

    int       m_Len;
    TRows     m_Rows;
    bool      m_HaveStrands;
    TSegments m_Mappings;
};


class CSeq_align_Mapper : public CObject
{
public:
    typedef CSeq_align::C_Segs::TDendiag TDendiag;
    typedef CSeq_align::C_Segs::TStd TStd;

    CSeq_align_Mapper(const CSeq_align& align);
    ~CSeq_align_Mapper(void) {}

    void Convert(CSeq_loc_Conversion& cvt);
    void Convert(const CMappingRange& mapping_range,
                 int width_flag);

    CRef<CSeq_align> GetDstAlign(void) const;

private:
    typedef SAlignment_Segment::TSegments TSegments;

    void x_Init(const TDendiag& diags);
    void x_Init(const CDense_seg& denseg);
    void x_Init(const TStd& sseg);
    void x_Init(const CPacked_seg& pseg);
    void x_Init(const CSeq_align_set& align_set);

    void x_MapSegment(SAlignment_Segment& sseg,
                      size_t row_idx,
                      CSeq_loc_Conversion& cvt);
    void x_MapSegment(SAlignment_Segment& sseg,
                      size_t row_idx,
                      const CMappingRange& mapping_range,
                      int width_flag);
    bool x_ConvertSegments(TSegments& segs, CSeq_loc_Conversion& cvt);
    bool x_ConvertSegments(TSegments& segs,
                           const CMappingRange& mapping_range,
                           int width_flag);
    void x_GetDstSegments(const TSegments& ssegs, TSegments& dsegs) const;

    // Used for e_Disc alignments
    typedef vector<CSeq_align_Mapper>  TSubAligns;

    bool x_IsValidAlign(TSegments segments) const;

    CConstRef<CSeq_align>        m_OrigAlign;
    mutable CRef<CSeq_align>     m_DstAlign;
    TSegments                    m_SrcSegs;
    mutable TSegments            m_DstSegs;
    TSubAligns                   m_SubAligns;
    bool                         m_HaveStrands;
    bool                         m_HaveWidths;
};


inline
SAlignment_Segment::SAlignment_Row::SAlignment_Row(const CSeq_id& id,
                                                   int start,
                                                   bool is_set_strand,
                                                   ENa_strand strand,
                                                   int width)
    : m_Id(CSeq_id_Handle::GetHandle(id)),
      m_Start(start < 0 ? kInvalidSeqPos : start),
      m_IsSetStrand(is_set_strand),
      m_Strand(strand),
      m_Width(width),
      m_Mapped(false)
{
    return;
}


inline
void SAlignment_Segment::SAlignment_Row::SetMapped(void)
{
    m_Mapped = true;
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
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
