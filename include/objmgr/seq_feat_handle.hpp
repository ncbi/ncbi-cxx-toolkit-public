#ifndef SEQ_FEAT_HANDLE__HPP
#define SEQ_FEAT_HANDLE__HPP

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
* Author: Aleksey Grichenko, Eugene Vasilchenko
*
* File Description:
*   Seq-feat handle
*
*/

#include <corelib/ncbiobj.hpp>
#include <util/range.hpp>
#include <objects/seqloc/Na_strand.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/SeqFeatData.hpp>
#include <objmgr/seq_annot_handle.hpp>
#include <objmgr/impl/snp_annot_info.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


/** @addtogroup ObjectManagerHandles
 *
 * @{
 */


class CSeq_annot_Handle;
class CMappedFeat;


/////////////////////////////////////////////////////////////////////////////
///
///  CSeq_feat_Handle --
///
///  Proxy to access the seq-feat objects data
///

class NCBI_XOBJMGR_EXPORT CSeq_feat_Handle
{
public:
    CSeq_feat_Handle(void);
    ~CSeq_feat_Handle(void);

    DECLARE_OPERATOR_BOOL(m_Annot);

    bool operator ==(const CSeq_feat_Handle& feat) const;
    bool operator !=(const CSeq_feat_Handle& feat) const;
    bool operator <(const CSeq_feat_Handle& feat) const;

    /// Get scope this handle belongs to
    CScope& GetScope(void) const;

    /// Get handle to seq-annot for this feature
    const CSeq_annot_Handle& GetAnnot(void) const;

    /// Get current seq-feat
    CConstRef<CSeq_feat> GetSeq_feat(void) const;

    /// Check if table is SNP
    bool IsTableSNP(void) const;

    typedef CRange<TSeqPos> TRange;

    /// Get range for current seq-feat
    TRange GetRange(void) const;

    // Mappings for CSeq_feat methods
    bool IsSetId(void) const;
    const CFeat_id& GetId(void) const;
    bool IsSetData(void) const;
    const CSeqFeatData& GetData(void) const;
    bool IsSetPartial(void) const;
    bool GetPartial(void) const;
    bool IsSetExcept(void) const;
    bool GetExcept(void) const;
    bool IsSetComment(void) const;
    const string& GetComment(void) const;
    bool IsSetProduct(void) const;
    const CSeq_loc& GetProduct(void) const;
    const CSeq_loc& GetLocation(void) const;
    bool IsSetQual(void) const;
    const CSeq_feat::TQual& GetQual(void) const;
    bool IsSetTitle(void) const;
    const string& GetTitle(void) const;
    bool IsSetExt(void) const;
    const CUser_object& GetExt(void) const;
    bool IsSetCit(void) const;
    const CPub_set& GetCit(void) const;
    bool IsSetExp_ev(void) const;
    CSeq_feat::EExp_ev GetExp_ev(void) const;
    bool IsSetXref(void) const;
    const CSeq_feat::TXref& GetXref(void) const;
    bool IsSetDbxref(void) const;
    const CSeq_feat::TDbxref& GetDbxref(void) const;
    bool IsSetPseudo(void) const;
    bool GetPseudo(void) const;
    bool IsSetExcept_text(void) const;
    const string& GetExcept_text(void) const;

    // Access to some methods of CSeq_feat members
    CSeqFeatData::E_Choice GetFeatType(void) const;
    CSeqFeatData::ESubtype GetFeatSubtype(void) const;

    // Table SNP only types and methods
    typedef SSNP_Info::TSNPId TSNPId;
    typedef SSNP_Info::TWeight TWeight;

    TSNPId GetSNPId(void) const;
    CSeq_id::TGi GetSNPGi(void) const;
    bool IsSNPMinusStrand(void) const;
    TWeight GetSNPWeight(void) const;
    size_t GetSNPAllelesCount(void) const;
    const string& GetSNPAllele(size_t index) const;

private:
    friend class CMappedFeat;
    friend class CCreatedFeat_Ref;

    const SSNP_Info& x_GetSNP_Info(void) const;
    const CSeq_annot_SNP_Info& x_GetSNP_annot_Info(void) const;
    const CSeq_feat& x_GetSeq_feat(void) const;

    enum EAnnotInfoType {
        eType_null,
        eType_Seq_annot_Info,
        eType_Seq_annot_SNP_Info
    };

    CSeq_feat_Handle(const CSeq_annot_Handle& annot,
                     EAnnotInfoType type,
                     size_t index,
                     CCreatedFeat_Ref& created_ref);

    CSeq_annot_Handle              m_Annot;
    EAnnotInfoType                 m_AnnotInfoType;
    size_t                         m_Index;
    mutable CRef<CCreatedFeat_Ref> m_CreatedFeat;
};


inline
bool CSeq_feat_Handle::operator ==(const CSeq_feat_Handle& feat) const
{
    return m_Annot == feat.m_Annot
        && m_AnnotInfoType == feat.m_AnnotInfoType
        && m_Index == feat.m_Index;
}


inline
bool CSeq_feat_Handle::operator !=(const CSeq_feat_Handle& feat) const
{
    return !(*this == feat);
}


inline
bool CSeq_feat_Handle::operator <(const CSeq_feat_Handle& feat) const
{
    if (m_Annot != feat.m_Annot) {
        return m_Annot < feat.m_Annot;
    }
    if (m_AnnotInfoType != feat.m_AnnotInfoType) {
        return m_AnnotInfoType < feat.m_AnnotInfoType;
    }
    return m_Index < feat.m_Index;
}


inline
bool CSeq_feat_Handle::IsTableSNP(void) const
{
    return m_AnnotInfoType == eType_Seq_annot_SNP_Info;
}


inline
bool CSeq_feat_Handle::IsSNPMinusStrand(void) const
{
    return x_GetSNP_Info().MinusStrand();
}


inline
CSeq_feat_Handle::TSNPId CSeq_feat_Handle::GetSNPId(void) const
{
    return x_GetSNP_Info().m_SNP_Id;
}


inline
CSeq_feat_Handle::TWeight CSeq_feat_Handle::GetSNPWeight(void) const
{
    return x_GetSNP_Info().m_Weight;
}


inline
bool CSeq_feat_Handle::IsSetId(void) const
{
    return x_GetSeq_feat().IsSetId();
}


inline
const CFeat_id& CSeq_feat_Handle::GetId(void) const
{
    return x_GetSeq_feat().GetId();
}


inline
bool CSeq_feat_Handle::IsSetData(void) const
{
    return x_GetSeq_feat().IsSetData();
}


inline
const CSeqFeatData& CSeq_feat_Handle::GetData(void) const
{
    return x_GetSeq_feat().GetData();
}


inline
bool CSeq_feat_Handle::IsSetPartial(void) const
{
    return x_GetSeq_feat().IsSetPartial();
}


inline
bool CSeq_feat_Handle::GetPartial(void) const
{
    return IsSetPartial()  &&  x_GetSeq_feat().GetPartial();
}


inline
bool CSeq_feat_Handle::IsSetExcept(void) const
{
    return x_GetSeq_feat().IsSetExcept();
}


inline
bool CSeq_feat_Handle::GetExcept(void) const
{
    return IsSetExcept()  &&  x_GetSeq_feat().GetExcept();
}


inline
bool CSeq_feat_Handle::IsSetComment(void) const
{
    return x_GetSeq_feat().IsSetComment();
}


inline
const string& CSeq_feat_Handle::GetComment(void) const
{
    return x_GetSeq_feat().GetComment();
}


inline
bool CSeq_feat_Handle::IsSetProduct(void) const
{
    return x_GetSeq_feat().IsSetProduct();
}


inline
const CSeq_loc& CSeq_feat_Handle::GetProduct(void) const
{
    return x_GetSeq_feat().GetProduct();
}


inline
const CSeq_loc& CSeq_feat_Handle::GetLocation(void) const
{
    return x_GetSeq_feat().GetLocation();
}


inline
bool CSeq_feat_Handle::IsSetQual(void) const
{
    return x_GetSeq_feat().IsSetQual();
}


inline
const CSeq_feat::TQual& CSeq_feat_Handle::GetQual(void) const
{
    return x_GetSeq_feat().GetQual();
}


inline
bool CSeq_feat_Handle::IsSetTitle(void) const
{
    return x_GetSeq_feat().IsSetTitle();
}


inline
const string& CSeq_feat_Handle::GetTitle(void) const
{
    return x_GetSeq_feat().GetTitle();
}


inline
bool CSeq_feat_Handle::IsSetExt(void) const
{
    return x_GetSeq_feat().IsSetExt();
}


inline
const CUser_object& CSeq_feat_Handle::GetExt(void) const
{
    return x_GetSeq_feat().GetExt();
}


inline
bool CSeq_feat_Handle::IsSetCit(void) const
{
    return x_GetSeq_feat().IsSetCit();
}


inline
const CPub_set& CSeq_feat_Handle::GetCit(void) const
{
    return x_GetSeq_feat().GetCit();
}


inline
bool CSeq_feat_Handle::IsSetExp_ev(void) const
{
    return x_GetSeq_feat().IsSetExp_ev();
}


inline
CSeq_feat::EExp_ev CSeq_feat_Handle::GetExp_ev(void) const
{
    return x_GetSeq_feat().GetExp_ev();
}


inline
bool CSeq_feat_Handle::IsSetXref(void) const
{
    return x_GetSeq_feat().IsSetXref();
}


inline
const CSeq_feat::TXref& CSeq_feat_Handle::GetXref(void) const
{
    return x_GetSeq_feat().GetXref();
}


inline
bool CSeq_feat_Handle::IsSetDbxref(void) const
{
    return x_GetSeq_feat().IsSetDbxref();
}


inline
const CSeq_feat::TDbxref& CSeq_feat_Handle::GetDbxref(void) const
{
    return x_GetSeq_feat().GetDbxref();
}


inline
bool CSeq_feat_Handle::IsSetPseudo(void) const
{
    return x_GetSeq_feat().IsSetPseudo();
}


inline
bool CSeq_feat_Handle::GetPseudo(void) const
{
    return IsSetPseudo()  &&  x_GetSeq_feat().GetPseudo();
}


inline
bool CSeq_feat_Handle::IsSetExcept_text(void) const
{
    return x_GetSeq_feat().IsSetExcept_text();
}


inline
const string& CSeq_feat_Handle::GetExcept_text(void) const
{
    return x_GetSeq_feat().GetExcept_text();
}


inline
CSeqFeatData::E_Choice CSeq_feat_Handle::GetFeatType(void) const
{
    return IsTableSNP()?
        CSeqFeatData::e_Imp:
        x_GetSeq_feat().GetData().Which();
}


inline
CSeqFeatData::ESubtype CSeq_feat_Handle::GetFeatSubtype(void) const
{
    return IsTableSNP()?
        CSeqFeatData::eSubtype_variation:
        x_GetSeq_feat().GetData().GetSubtype();
}


/* @} */

END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.12  2005/03/07 17:29:04  vasilche
* Added "SNP" to names of SNP access methods
*
* Revision 1.11  2005/02/25 15:58:42  grichenk
* Added IsSetData()
*
* Revision 1.10  2005/02/24 19:13:34  grichenk
* Redesigned CMappedFeat not to hold the whole annot collector.
*
* Revision 1.9  2005/02/23 20:02:55  grichenk
* Added operators ==, != and <. Bool GetXXXX also check IsSetXXXX.
*
* Revision 1.8  2005/01/24 17:09:36  vasilche
* Safe boolean operators.
*
* Revision 1.7  2004/12/28 18:40:30  vasilche
* Added GetScope() method.
*
* Revision 1.6  2004/12/22 15:56:12  vasilche
* Used CSeq_annot_Handle in annotations' handles.
*
* Revision 1.5  2004/12/03 19:25:55  shomrat
* Added export prefix
*
* Revision 1.4  2004/09/29 16:59:54  kononenk
* Added doxygen formatting
*
* Revision 1.3  2004/08/25 20:44:52  grichenk
* Added operator bool() and operator !()
*
* Revision 1.2  2004/05/06 17:32:37  grichenk
* Added CanGetXXXX() methods
*
* Revision 1.1  2004/05/04 18:06:06  grichenk
* Initial revision
*
*
* ===========================================================================
*/

#endif  // SEQ_FEAT_HANDLE__HPP
