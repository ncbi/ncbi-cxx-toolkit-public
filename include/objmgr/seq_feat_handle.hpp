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
#include <corelib/ncbi_limits.h>
#include <util/range.hpp>
#include <objects/seqloc/Na_strand.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/SeqFeatData.hpp>
#include <objmgr/seq_annot_handle.hpp>
#include <objmgr/impl/annot_collector.hpp>
#include <objmgr/impl/snp_info.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


/** @addtogroup ObjectManagerHandles
 *
 * @{
 */


class CSeq_annot_Handle;
class CMappedFeat;
class CSeq_annot_ftable_CI;
class CSeq_annot_ftable_I;

template<typename Handle>
class CSeq_annot_Add_EditCommand;
template<typename Handle>
class CSeq_annot_Replace_EditCommand;
template<typename Handle>
class CSeq_annot_Remove_EditCommand;

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

    void Reset(void);

    DECLARE_OPERATOR_BOOL(m_Annot && m_AnnotIndex != eNull && !IsRemoved());

    bool operator ==(const CSeq_feat_Handle& feat) const;
    bool operator !=(const CSeq_feat_Handle& feat) const;
    bool operator <(const CSeq_feat_Handle& feat) const;

    /// Get scope this handle belongs to
    CScope& GetScope(void) const;

    /// Get handle to seq-annot for this feature
    const CSeq_annot_Handle& GetAnnot(void) const;

    /// Get current seq-feat
    CConstRef<CSeq_feat> GetSeq_feat(void) const;

    /// Check if this is plain feature
    bool IsPlainFeat(void) const;

    /// Check if this is SNP table feature
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
    bool IsSetSNPComment(void) const;
    const string& GetSNPComment(void) const;

    bool IsSetSNPQualityCode(void) const;
    CUser_field::TData::E_Choice GetSNPQualityCodeWhich(void) const;
    const string& GetSNPQualityCodeStr(void) const;
    vector<char> GetSNPQualityCodeOs(void) const;

    /// Return true if this feature was removed already
    bool IsRemoved(void) const;
    /// Remove the feature from Seq-annot
    /// The method is deprecated, use CSeq_feat_EditHandle
    NCBI_DEPRECATED void Remove(void) const;
    /// Replace the feature with new Seq-feat object.
    /// All indexes are updated correspondingly.
    /// The method is deprecated, use CSeq_feat_EditHandle
    NCBI_DEPRECATED void Replace(const CSeq_feat& new_feat) const;

protected:
    friend class CMappedFeat;
    friend class CCreatedFeat_Ref;
    friend class CSeq_annot_Handle;
    friend class CSeq_annot_ftable_CI;
    friend class CSeq_annot_ftable_I;
    friend class CTSE_Handle;
    typedef Int4 TIndex;

    TIndex x_GetAnnotIndex() const;

    // Seq-annot retrieval
    const CSeq_annot_Info& x_GetSeq_annot_Info(void) const;
    const CSeq_annot_SNP_Info& x_GetSNP_annot_Info(void) const;

    const CAnnotObject_Info& x_GetAnnotObject_InfoAny(void) const;
    const CAnnotObject_Info& x_GetAnnotObject_Info(void) const;
    const CSeq_feat& x_GetPlainSeq_feat(void) const;

    const SSNP_Info& x_GetSNP_InfoAny(void) const;
    const SSNP_Info& x_GetSNP_Info(void) const;

    enum EAnnotInfoType {
        eType_null,
        eType_Seq_annot_Info,
        eType_Seq_annot_SNP_Info
    };

    enum {
        eNull = kMax_I4
    };

    CSeq_feat_Handle(const CSeq_annot_Handle& annot, TIndex index);
    CSeq_feat_Handle(const CSeq_annot_Handle& annot,
                     const SSNP_Info& snp_info,
                     CCreatedFeat_Ref& created_ref);
    CSeq_feat_Handle(CScope& scope, CAnnotObject_Info* info);

private:
    CSeq_annot_Handle              m_Annot;
    TIndex                         m_AnnotIndex;
    mutable CRef<CCreatedFeat_Ref> m_CreatedFeat;
};


inline
CSeq_feat_Handle::CSeq_feat_Handle(void)
    : m_AnnotIndex(eNull)
{
}


inline
const CSeq_annot_Handle& CSeq_feat_Handle::GetAnnot(void) const
{
    return m_Annot;
}


inline
const CSeq_annot_Info& CSeq_feat_Handle::x_GetSeq_annot_Info(void) const
{
    return GetAnnot().x_GetInfo();
}


inline
CScope& CSeq_feat_Handle::GetScope(void) const
{
    return GetAnnot().GetScope();
}


inline
CSeq_feat_Handle::TIndex CSeq_feat_Handle::x_GetAnnotIndex(void) const
{
    return m_AnnotIndex;
}


inline
bool CSeq_feat_Handle::operator ==(const CSeq_feat_Handle& feat) const
{
    return GetAnnot() == feat.GetAnnot() &&
        x_GetAnnotIndex() == feat.x_GetAnnotIndex();
}


inline
bool CSeq_feat_Handle::operator !=(const CSeq_feat_Handle& feat) const
{
    return !(*this == feat);
}


inline
bool CSeq_feat_Handle::operator <(const CSeq_feat_Handle& feat) const
{
    return GetAnnot() < feat.GetAnnot() ||
        GetAnnot() == feat.GetAnnot() &&
        x_GetAnnotIndex() < feat.x_GetAnnotIndex();
}


inline
bool CSeq_feat_Handle::IsPlainFeat(void) const
{
    return x_GetAnnotIndex() >= 0;
}


inline
bool CSeq_feat_Handle::IsTableSNP(void) const
{
    return x_GetAnnotIndex() < 0;
}


inline
bool CSeq_feat_Handle::IsSNPMinusStrand(void) const
{
    return x_GetSNP_Info().MinusStrand();
}


inline
bool CSeq_feat_Handle::IsSetSNPComment(void) const
{
    return x_GetSNP_Info().m_CommentIndex != SSNP_Info::kNo_CommentIndex;
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
bool CSeq_feat_Handle::IsSetSNPQualityCode(void) const
{
    return (x_GetSNP_Info().m_Flags & SSNP_Info::fQualityCodeMask) != 0;
}


inline
bool CSeq_feat_Handle::IsSetId(void) const
{
    return IsPlainFeat() && x_GetPlainSeq_feat().IsSetId();
}


inline
const CFeat_id& CSeq_feat_Handle::GetId(void) const
{
    return x_GetPlainSeq_feat().GetId();
}


inline
bool CSeq_feat_Handle::IsSetData(void) const
{
    return IsTableSNP() || IsPlainFeat() && x_GetPlainSeq_feat().IsSetData();
}


inline
const CSeqFeatData& CSeq_feat_Handle::GetData(void) const
{
    return GetSeq_feat()->GetData();
}


inline
bool CSeq_feat_Handle::IsSetPartial(void) const
{
    return IsPlainFeat() && x_GetPlainSeq_feat().IsSetPartial();
}


inline
bool CSeq_feat_Handle::GetPartial(void) const
{
    return IsSetPartial() && x_GetPlainSeq_feat().GetPartial();
}


inline
bool CSeq_feat_Handle::IsSetExcept(void) const
{
    return IsPlainFeat() && x_GetPlainSeq_feat().IsSetExcept();
}


inline
bool CSeq_feat_Handle::GetExcept(void) const
{
    return IsSetExcept() && x_GetPlainSeq_feat().GetExcept();
}


inline
bool CSeq_feat_Handle::IsSetComment(void) const
{
    return IsPlainFeat() && x_GetPlainSeq_feat().IsSetComment() ||
        IsTableSNP() && IsSetSNPComment();
}


inline
const string& CSeq_feat_Handle::GetComment(void) const
{
    return IsTableSNP()? GetSNPComment(): x_GetPlainSeq_feat().GetComment();
}


inline
bool CSeq_feat_Handle::IsSetProduct(void) const
{
    return IsPlainFeat() && x_GetPlainSeq_feat().IsSetProduct();
}


inline
const CSeq_loc& CSeq_feat_Handle::GetProduct(void) const
{
    return x_GetPlainSeq_feat().GetProduct();
}


inline
const CSeq_loc& CSeq_feat_Handle::GetLocation(void) const
{
    return GetSeq_feat()->GetLocation();
}


inline
bool CSeq_feat_Handle::IsSetQual(void) const
{
    return IsTableSNP() || IsPlainFeat() && x_GetPlainSeq_feat().IsSetQual();
}


inline
const CSeq_feat::TQual& CSeq_feat_Handle::GetQual(void) const
{
    return GetSeq_feat()->GetQual();
}


inline
bool CSeq_feat_Handle::IsSetTitle(void) const
{
    return IsPlainFeat() && x_GetPlainSeq_feat().IsSetTitle();
}


inline
const string& CSeq_feat_Handle::GetTitle(void) const
{
    return x_GetPlainSeq_feat().GetTitle();
}


inline
bool CSeq_feat_Handle::IsSetExt(void) const
{
    return IsTableSNP() || IsPlainFeat() && x_GetPlainSeq_feat().IsSetExt();
}


inline
const CUser_object& CSeq_feat_Handle::GetExt(void) const
{
    return GetSeq_feat()->GetExt();
}


inline
bool CSeq_feat_Handle::IsSetCit(void) const
{
    return IsPlainFeat() && x_GetPlainSeq_feat().IsSetCit();
}


inline
const CPub_set& CSeq_feat_Handle::GetCit(void) const
{
    return x_GetPlainSeq_feat().GetCit();
}


inline
bool CSeq_feat_Handle::IsSetExp_ev(void) const
{
    return IsPlainFeat() && x_GetPlainSeq_feat().IsSetExp_ev();
}


inline
CSeq_feat::EExp_ev CSeq_feat_Handle::GetExp_ev(void) const
{
    return x_GetPlainSeq_feat().GetExp_ev();
}


inline
bool CSeq_feat_Handle::IsSetXref(void) const
{
    return IsPlainFeat() && x_GetPlainSeq_feat().IsSetXref();
}


inline
const CSeq_feat::TXref& CSeq_feat_Handle::GetXref(void) const
{
    return x_GetPlainSeq_feat().GetXref();
}


inline
bool CSeq_feat_Handle::IsSetDbxref(void) const
{
    return IsTableSNP() || IsPlainFeat() && x_GetPlainSeq_feat().IsSetDbxref();
}


inline
const CSeq_feat::TDbxref& CSeq_feat_Handle::GetDbxref(void) const
{
    return GetSeq_feat()->GetDbxref();
}


inline
bool CSeq_feat_Handle::IsSetPseudo(void) const
{
    return IsPlainFeat() && x_GetPlainSeq_feat().IsSetPseudo();
}


inline
bool CSeq_feat_Handle::GetPseudo(void) const
{
    return IsSetPseudo() && x_GetPlainSeq_feat().GetPseudo();
}


inline
bool CSeq_feat_Handle::IsSetExcept_text(void) const
{
    return IsPlainFeat() && x_GetPlainSeq_feat().IsSetExcept_text();
}


inline
const string& CSeq_feat_Handle::GetExcept_text(void) const
{
    return x_GetPlainSeq_feat().GetExcept_text();
}


inline
CSeqFeatData::E_Choice CSeq_feat_Handle::GetFeatType(void) const
{
    return IsTableSNP()?
        CSeqFeatData::e_Imp:
        x_GetPlainSeq_feat().GetData().Which();
}


inline
CSeqFeatData::ESubtype CSeq_feat_Handle::GetFeatSubtype(void) const
{
    return IsTableSNP()?
        CSeqFeatData::eSubtype_variation:
        x_GetPlainSeq_feat().GetData().GetSubtype();
}


/////////////////////////////////////////////////////////////////////////////
///
///  CSeq_feat_EditHandle --
///
///  Proxy to edit the seq-feat objects data
///

class NCBI_XOBJMGR_EXPORT CSeq_feat_EditHandle : public CSeq_feat_Handle
{
public:
    CSeq_feat_EditHandle(void);
    explicit CSeq_feat_EditHandle(const CSeq_feat_Handle& h);

    CSeq_annot_EditHandle GetAnnot(void) const;

    /// Remove the feature from Seq-annot
    void Remove(void) const;
    /// Replace the feature with new Seq-feat object.
    /// All indexes are updated correspondingly.
    void Replace(const CSeq_feat& new_feat) const;

    /// Update index after manual modification of the object
    void Update(void) const;

protected:
    friend class CSeq_annot_EditHandle;

    CSeq_feat_EditHandle(const CSeq_annot_EditHandle& annot, TIndex index);
    CSeq_feat_EditHandle(const CSeq_annot_EditHandle& annot,
                         const SSNP_Info& snp_info,
                         CCreatedFeat_Ref& created_ref);

    friend class CSeq_annot_Add_EditCommand<CSeq_feat_EditHandle>;
    friend class CSeq_annot_Replace_EditCommand<CSeq_feat_EditHandle>;
    friend class CSeq_annot_Remove_EditCommand<CSeq_feat_EditHandle>;
    friend class CSeq_annot_ftable_I;

    /// Remove the feature from Seq-annot
    void x_RealRemove(void) const;
    /// Replace the feature with new Seq-feat object.
    /// All indexes are updated correspondingly.
    void x_RealReplace(const CSeq_feat& new_feat) const;

};


inline
CSeq_feat_EditHandle::CSeq_feat_EditHandle(void)
{
}


inline
CSeq_annot_EditHandle CSeq_feat_EditHandle::GetAnnot(void) const
{
    return CSeq_annot_EditHandle(CSeq_feat_Handle::GetAnnot());
}


/////////////////////////////////////////////////////////////////////////////
// CSeq_annot_ftable_CI

class NCBI_XOBJMGR_EXPORT CSeq_annot_ftable_CI
{
public:
    enum EFlags {
        fIncludeTable    = 1<<0,
        fOnlyTable       = 1<<1
    };
    typedef int TFlags;

    CSeq_annot_ftable_CI(void);
    explicit CSeq_annot_ftable_CI(const CSeq_annot_Handle& annot,
                                  TFlags flags = 0);

    CScope& GetScope(void) const;
    const CSeq_annot_Handle& GetAnnot(void) const;

    DECLARE_OPERATOR_BOOL(m_Feat);

    const CSeq_feat_Handle& operator*(void) const;
    const CSeq_feat_Handle* operator->(void) const;

    CSeq_annot_ftable_CI& operator++(void);

protected:
    bool x_IsValid(void) const;
    bool x_IsExcluded(void) const;
    void x_Step(void);
    void x_Reset(void);
    void x_Settle(void);

private:
    CSeq_feat_Handle  m_Feat;
    TFlags            m_Flags;
};


inline
CSeq_annot_ftable_CI::CSeq_annot_ftable_CI(void)
{
}


inline
const CSeq_annot_Handle& CSeq_annot_ftable_CI::GetAnnot(void) const
{
    return m_Feat.GetAnnot();
}


inline
CScope& CSeq_annot_ftable_CI::GetScope(void) const
{
    return GetAnnot().GetScope();
}


inline
const CSeq_feat_Handle& CSeq_annot_ftable_CI::operator*(void) const
{
    return m_Feat;
}


inline
const CSeq_feat_Handle* CSeq_annot_ftable_CI::operator->(void) const
{
    return &m_Feat;
}


inline
CSeq_annot_ftable_CI& CSeq_annot_ftable_CI::operator++(void)
{
    x_Step();
    x_Settle();
    return *this;
}

/////////////////////////////////////////////////////////////////////////////
// CSeq_annot_ftable_I

class NCBI_XOBJMGR_EXPORT CSeq_annot_ftable_I
{
public:
    enum EFlags {
        fIncludeTable    = 1<<0,
        fOnlyTable       = 1<<1
    };
    typedef int TFlags;

    CSeq_annot_ftable_I(void);
    explicit CSeq_annot_ftable_I(const CSeq_annot_EditHandle& annot,
                                 TFlags flags = 0);

    CScope& GetScope(void) const;
    const CSeq_annot_EditHandle& GetAnnot(void) const;

    DECLARE_OPERATOR_BOOL(m_Feat);

    const CSeq_feat_EditHandle& operator*(void) const;
    const CSeq_feat_EditHandle* operator->(void) const;

    CSeq_annot_ftable_I& operator++(void);

protected:
    bool x_IsValid(void) const;
    bool x_IsExcluded(void) const;
    void x_Step(void);
    void x_Reset(void);
    void x_Settle(void);

private:
    CSeq_annot_EditHandle m_Annot;
    TFlags                m_Flags;
    CSeq_feat_EditHandle  m_Feat;
};


inline
CSeq_annot_ftable_I::CSeq_annot_ftable_I(void)
    : m_Flags(0)
{
}


inline
const CSeq_annot_EditHandle& CSeq_annot_ftable_I::GetAnnot(void) const
{
    return m_Annot;
}


inline
CScope& CSeq_annot_ftable_I::GetScope(void) const
{
    return GetAnnot().GetScope();
}


inline
const CSeq_feat_EditHandle& CSeq_annot_ftable_I::operator*(void) const
{
    return m_Feat;
}


inline
const CSeq_feat_EditHandle* CSeq_annot_ftable_I::operator->(void) const
{
    return &m_Feat;
}


inline
CSeq_annot_ftable_I& CSeq_annot_ftable_I::operator++(void)
{
    x_Step();
    x_Settle();
    return *this;
}

/* @} */

END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log: seq_feat_handle.hpp,v $
* Revision 1.22  2006/11/14 19:21:59  vasilche
* Added feature ids index and retrieval.
*
* Revision 1.21  2006/09/20 14:00:19  vasilche
* Implemented user API to Update() annotation index.
*
* Revision 1.20  2006/08/07 15:25:06  vasilche
* Introduced CSeq_feat_EditHandle.
* Introduced CSeq_annot_ftable_CI & CSeq_annot_ftable_I.
*
* Revision 1.19  2006/07/12 16:17:30  vasilche
* Added CSeq_annot_ftable_CI.
*
* Revision 1.18  2005/11/15 19:22:06  didenko
* Added transactions and edit commands support
*
* Revision 1.17  2005/09/20 15:45:35  vasilche
* Feature editing API.
* Annotation handles remember annotations by index.
*
* Revision 1.16  2005/08/23 17:03:01  vasilche
* Use CAnnotObject_Info pointer instead of annotation index in annot handles.
*
* Revision 1.15  2005/04/07 20:09:12  ucko
* Include annot_collector.hpp so that CSeq_feat_Handle::m_CreatedFeat
* has a fully known type for the sake of its inline constructor.
*
* Revision 1.14  2005/04/07 16:30:42  vasilche
* Inlined handles' constructors and destructors.
* Optimized handles' assignment operators.
*
* Revision 1.13  2005/03/15 19:10:11  vasilche
* SSNP_Info structure is defined in separate header.
*
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
