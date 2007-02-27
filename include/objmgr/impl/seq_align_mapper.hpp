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
#include <objects/seq/seq_align_mapper_base.hpp>
#include <objmgr/scope.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


class NCBI_XOBJMGR_EXPORT CSeq_align_Mapper : public CSeq_align_Mapper_Base
{
public:
    CSeq_align_Mapper(const CSeq_align& align,
                      bool map_widths,
                      CScope* scope = 0);
    ~CSeq_align_Mapper(void);

protected:
    virtual int GetSeqWidth(const CSeq_id& id) const;
    virtual CSeq_align_Mapper_Base* CreateSubAlign(const CSeq_align& align,
                                                   EWidthFlag map_widths);

private:
    typedef CSeq_loc_Conversion_Set::TRange       TRange;
    typedef CSeq_loc_Conversion_Set::TRangeMap    TRangeMap;
    typedef CSeq_loc_Conversion_Set::TIdMap       TIdMap;
    typedef CSeq_loc_Conversion_Set::TConvByIndex TConvByIndex;

    friend class CSeq_loc_Conversion_Set;

    void Convert(CSeq_loc_Conversion_Set& cvts);

    // Mapping through CSeq_loc_Conversion
    void x_ConvertAlignCvt(CSeq_loc_Conversion_Set& cvts);
    void x_ConvertRowCvt(CSeq_loc_Conversion& cvt,
                         size_t row);
    void x_ConvertRowCvt(TIdMap& cvts,
                         size_t row);
    CSeq_id_Handle x_ConvertSegmentCvt(TSegments::iterator& seg_it,
                                       CSeq_loc_Conversion& cvt,
                                       size_t row);
    CSeq_id_Handle x_ConvertSegmentCvt(TSegments::iterator& seg_it,
                                       TIdMap& id_map,
                                       size_t row);

    mutable CRef<CScope> m_Scope;
};


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.17  2006/12/11 17:14:11  grichenk
* Added CSeq_loc_Mapper_Base and CSeq_align_Mapper_Base.
*
* Revision 1.16  2006/11/29 20:44:04  grichenk
* Set correct strand for gaps.
*
* Revision 1.15  2006/09/27 21:27:58  vasilche
* Added accessors.
*
* Revision 1.14  2005/10/18 15:36:47  vasilche
* Removed trailing comma.
*
* Revision 1.13  2005/09/26 21:38:42  grichenk
* Adjust segment length when mapping between nucleotide and protein
* (den-diag, packed-seg).
*
* Revision 1.12  2005/09/26 16:33:18  ucko
* Pull CSeq_align_Mapper's destructor out of line, as its (implicit)
* destruction of m_Scope may require a full declaration thereof.
*
* Revision 1.11  2005/09/22 20:49:33  grichenk
* Adjust segment length when mapping alignment between nuc and prot.
*
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
