#ifndef SNP_INFO__HPP
#define SNP_INFO__HPP

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
* Author: Eugene Vasilchenko
*
* File Description:
*   SNP Seq-annot object information
*
*/

#include <corelib/ncbiobj.hpp>
#include <corelib/ncbi_limits.hpp>

#include <util/range.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CSeq_entry;
class CSeq_feat;
class CSeq_annot;
class CSeq_annot_Info;
class CSeq_annot_SNP_Info;
class CSeq_point;
class CSeq_interval;

struct NCBI_XOBJMGR_EXPORT SSNP_Info
{
public:
    typedef CRange<TSeqPos> TRange;

    TSeqPos GetFrom(void) const;
    TSeqPos GetTo(void) const;
    bool MinusStrand(void) const;

    bool operator<(const SSNP_Info& snp) const;
    bool operator<(TSeqPos end_position) const;

    bool NoMore(const TRange& range) const;
    bool NotThis(const TRange& range) const;

    // type of SNP feature returned by parsing method
    enum ESNP_Type {
        eSNP_Simple,
        eSNP_Bad_WrongMemberSet,
        eSNP_Bad_WrongTextId,
        eSNP_Complex_HasComment,
        eSNP_Complex_LocationIsNotPoint,
        eSNP_Complex_LocationIsNotGi,
        eSNP_Complex_LocationGiIsBad,
        eSNP_Complex_LocationStrandIsBad,
        eSNP_Complex_IdCountTooLarge,
        eSNP_Complex_IdCountIsNotOne,
        eSNP_Complex_AlleleLengthBad,
        eSNP_Complex_AlleleCountTooLarge,
        eSNP_Complex_AlleleCountIsNonStandard,
        eSNP_Complex_WeightBadValue,
        eSNP_Complex_WeightCountIsNotOne,
        eSNP_Type_last
    };
    // names of types for logging
    static const char* const s_SNP_Type_Label[eSNP_Type_last];

    // parser, if returned value is eSNP_Simple, then
    // other members are filled and can be stored.
    ESNP_Type ParseSeq_feat(const CSeq_feat& feat,
                            CSeq_annot_SNP_Info& annot_info);
    // restore Seq-feat object from parsed info.
    CRef<CSeq_feat>
    CreateSeq_feat(const CSeq_annot_SNP_Info& annot_info) const;

    void UpdateSeq_feat(CRef<CSeq_feat>& seq_feat,
                        const CSeq_annot_SNP_Info& annot_info) const;
    void UpdateSeq_feat(CRef<CSeq_feat>& seq_feat,
                        CRef<CSeq_point>& seq_point,
                        CRef<CSeq_interval>& seq_interval,
                        const CSeq_annot_SNP_Info& annot_info) const;
    
    CRef<CSeq_feat> x_CreateSeq_feat(void) const;
    void x_UpdateSeq_featData(CSeq_feat& feat,
                              const CSeq_annot_SNP_Info& annot_info) const;
    void x_UpdateSeq_feat(CSeq_feat& feat,
                          const CSeq_annot_SNP_Info& annot_info) const;
    void x_UpdateSeq_feat(CSeq_feat& feat,
                          CRef<CSeq_point>& seq_point,
                          CRef<CSeq_interval>& seq_interval,
                          const CSeq_annot_SNP_Info& annot_info) const;

    typedef int TSNPId;
    typedef Int1 TPositionDelta;
    enum {
        kMax_PositionDelta = kMax_I1
    };
    typedef Uint1 TCommentIndex;
    enum {
        kNo_CommentIndex   = kMax_UI1,
        kMax_CommentIndex  = kNo_CommentIndex - 1,
        kMax_CommentLength = 65530
    };
    typedef Uint1 TAlleleIndex;
    enum {
        kNo_AlleleIndex    = kMax_UI1,
        kMax_AlleleIndex   = kNo_AlleleIndex - 1,
        kMax_AlleleLength  = 5
    };
    enum {
        kMax_AllelesCount  = 4
    };
    typedef Uint1 TWeight;
    enum {
        kMax_Weight        = kMax_I1
    };
    typedef Uint1 TFlags;
    enum FFlags {
        fMinusStrand = 1,
        fQualReplace = 2,
        fWeightQual  = 4
    };

    TSeqPos         m_ToPosition;
    TSNPId          m_SNP_Id;
    TFlags          m_Flags;
    TPositionDelta  m_PositionDelta;
    TCommentIndex   m_CommentIndex;
    TWeight         m_Weight;
    TAlleleIndex    m_AllelesIndices[kMax_AllelesCount];
};


/////////////////////////////////////////////////////////////////////////////
// SSNP_Info
/////////////////////////////////////////////////////////////////////////////

inline
TSeqPos SSNP_Info::GetFrom(void) const
{
    return m_ToPosition - m_PositionDelta;
}


inline
TSeqPos SSNP_Info::GetTo(void) const
{
    return m_ToPosition;
}


inline
bool SSNP_Info::MinusStrand(void) const
{
    return (m_Flags & fMinusStrand) != 0;
}


inline
bool SSNP_Info::operator<(const SSNP_Info& snp) const
{
    return m_ToPosition < snp.m_ToPosition;
}


inline
bool SSNP_Info::operator<(TSeqPos to_position) const
{
    return m_ToPosition < to_position;
}


inline
bool SSNP_Info::NoMore(const TRange& range) const
{
    return GetTo() >= min(kInvalidSeqPos-kMax_PositionDelta,
                          range.GetToOpen()) + kMax_PositionDelta;
}


inline
bool SSNP_Info::NotThis(const TRange& range) const
{
    return GetFrom() >= range.GetToOpen();
}


END_SCOPE(objects)
END_NCBI_SCOPE


#endif  // SNP_INFO__HPP

