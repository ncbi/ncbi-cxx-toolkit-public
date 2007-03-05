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

#endif  // SEQ_ALIGN_MAPPER__HPP
