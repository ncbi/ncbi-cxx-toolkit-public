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

class NCBI_XOBJMGR_EXPORT CSeq_feat_Handle : public ISeq_feat
{
public:
    CSeq_feat_Handle(void);
    ~CSeq_feat_Handle(void);

    void Reset(void);

    DECLARE_OPERATOR_BOOL(m_Seq_annot && !IsRemoved());

    bool operator ==(const CSeq_feat_Handle& feat) const;
    bool operator !=(const CSeq_feat_Handle& feat) const;
    bool operator <(const CSeq_feat_Handle& feat) const;

    /// Get scope this handle belongs to
    CScope& GetScope(void) const;

    /// Get handle to seq-annot for this feature
    const CSeq_annot_Handle& GetAnnot(void) const;

    /// Get current seq-feat
    CConstRef<CSeq_feat> GetPlainSeq_feat(void) const;
    CConstRef<CSeq_feat> GetOriginalSeq_feat(void) const;
    virtual CConstRef<CSeq_feat> GetSeq_feat(void) const;

    /// Check if this is plain feature
    bool IsPlainFeat(void) const;

    /// Check if this is non-SNP table feature
    bool IsTableFeat(void) const;

    /// Check if this is SNP table feature
    bool IsTableSNP(void) const;

    typedef CRange<TSeqPos> TRange;

    /// Get range for current seq-feat
    virtual TRange GetRange(void) const;

    virtual CSeq_id_Handle GetLocationId(void) const;
    TRange GetLocationTotalRange(void) const
        { return GetRange(); }

    virtual CSeq_id_Handle GetProductId(void) const;
    virtual TRange GetProductTotalRange(void) const;

    // Mappings for CSeq_feat methods
    bool IsSetId(void) const;
    const CFeat_id& GetId(void) const;
    bool IsSetData(void) const;
    const CSeqFeatData& GetData(void) const;
    virtual bool IsSetPartial(void) const;
    virtual bool GetPartial(void) const;
    bool IsSetExcept(void) const;
    bool GetExcept(void) const;
    bool IsSetComment(void) const;
    const string& GetComment(void) const;
    bool IsSetProduct(void) const;
    virtual const CSeq_loc& GetProduct(void) const;
    virtual const CSeq_loc& GetLocation(void) const;
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
    bool IsSetIds(void) const;
    const CSeq_feat::TIds& GetIds(void) const;
    bool IsSetExts(void) const;
    const CSeq_feat::TExts& GetExts(void) const;

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
    void GetSNPQualityCodeOs(vector<char>& os) const;

    bool IsSetSNPExtra(void) const;
    const string& GetSNPExtra(void) const;

    /// Return true if this feature was removed already
    bool IsRemoved(void) const;
    /// Remove the feature from Seq-annot
    /// The method is deprecated, use CSeq_feat_EditHandle
    NCBI_DEPRECATED void Remove(void) const;
    /// Replace the feature with new Seq-feat object.
    /// All indexes are updated correspondingly.
    /// The method is deprecated, use CSeq_feat_EditHandle
    NCBI_DEPRECATED void Replace(const CSeq_feat& new_feat) const;


    // Methods redirected to corresponding Seq-feat object:
    /// get gene (if present) from Seq-feat.xref list
    const CGene_ref* GetGeneXref(void) const;

    /// get protein (if present) from Seq-feat.xref list
    const CProt_ref* GetProtXref(void) const;

    /// Return a specified DB xref.  This will find the *first* item in the
    /// given referenced database.  If no item is found, an empty CConstRef<>
    /// is returned.
    CConstRef<CDbtag> GetNamedDbxref(const string& db) const;

    /// Return a named qualifier.  This will return the first item matching the
    /// qualifier name.  If no such qualifier is found, an empty string is
    /// returned.
    const string& GetNamedQual(const string& qual_name) const;

protected:
    friend class CMappedFeat;
    friend class CFeat_CI;
    friend class CCreatedFeat_Ref;
    friend class CSeq_annot_Info;
    friend class CSeq_annot_Handle;
    friend class CSeq_annot_ftable_CI;
    friend class CSeq_annot_ftable_I;
    friend class CTSE_Handle;
    friend class CScope_Impl;
    typedef Int4 TFeatIndex;
    enum {
        kSNPTableBit   = 0x80000000,
        kFeatIndexMask = 0x7fffffff
    };

    TFeatIndex x_GetFeatIndex() const;

    // Seq-annot retrieval
    const CSeq_annot_Info& x_GetSeq_annot_Info(void) const;
    const CSeq_annot_SNP_Info& x_GetSNP_annot_Info(void) const;

    const CAnnotObject_Info& x_GetAnnotObject_InfoAny(void) const;
    const CAnnotObject_Info& x_GetAnnotObject_Info(void) const;
    const CSeq_feat& x_GetPlainSeq_feat(void) const;

    const SSNP_Info& x_GetSNP_InfoAny(void) const;
    const SSNP_Info& x_GetSNP_Info(void) const;

    CSeq_feat_Handle(const CSeq_annot_Handle& annot,
                     TFeatIndex feat_index);
    CSeq_feat_Handle(const CSeq_annot_Handle& annot,
                     const SSNP_Info& snp_info,
                     CCreatedFeat_Ref& created_ref);
    CSeq_feat_Handle(CScope& scope, CAnnotObject_Info* info);

private:
    CSeq_annot_Handle              m_Seq_annot;
    TFeatIndex                     m_FeatIndex;
    mutable CRef<CCreatedFeat_Ref> m_CreatedFeat;
};


inline
CSeq_feat_Handle::CSeq_feat_Handle(void)
    : m_FeatIndex(0)
{
}


inline
const CSeq_annot_Handle& CSeq_feat_Handle::GetAnnot(void) const
{
    return m_Seq_annot;
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
CSeq_feat_Handle::TFeatIndex CSeq_feat_Handle::x_GetFeatIndex(void) const
{
    return m_FeatIndex & kFeatIndexMask;
}


inline
bool CSeq_feat_Handle::operator ==(const CSeq_feat_Handle& feat) const
{
    return GetAnnot() == feat.GetAnnot() &&
        x_GetFeatIndex() == feat.x_GetFeatIndex();
}


inline
bool CSeq_feat_Handle::operator !=(const CSeq_feat_Handle& feat) const
{
    return GetAnnot() != feat.GetAnnot() ||
        x_GetFeatIndex() != feat.x_GetFeatIndex();
}


inline
bool CSeq_feat_Handle::operator <(const CSeq_feat_Handle& feat) const
{
    if ( GetAnnot() != feat.GetAnnot() ) {
        return GetAnnot() < feat.GetAnnot();
    }
    return x_GetFeatIndex() < feat.x_GetFeatIndex();
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
    return (x_GetSNP_Info().m_Flags & SSNP_Info::fQualityCodesMask) != 0;
}


inline
bool CSeq_feat_Handle::IsSetSNPExtra(void) const
{
    return x_GetSNP_Info().m_ExtraIndex != SSNP_Info::kNo_ExtraIndex;
}


inline
bool CSeq_feat_Handle::IsSetId(void) const
{
    // table SNP features do not have id
    return !IsTableSNP() && GetSeq_feat()->IsSetId();
}


inline
const CFeat_id& CSeq_feat_Handle::GetId(void) const
{
    return GetSeq_feat()->GetId();
}


inline
const CSeqFeatData& CSeq_feat_Handle::GetData(void) const
{
    return GetSeq_feat()->GetData();
}


inline
bool CSeq_feat_Handle::IsSetExcept(void) const
{
    // table SNP features do not have except
    return !IsTableSNP() && GetSeq_feat()->IsSetExcept();
}


inline
bool CSeq_feat_Handle::GetExcept(void) const
{
    return GetSeq_feat()->GetExcept();
}


inline
bool CSeq_feat_Handle::IsSetComment(void) const
{
    // table SNP features may have comment
    return IsTableSNP()? IsSetSNPComment(): GetSeq_feat()->IsSetComment();
}


inline
const string& CSeq_feat_Handle::GetComment(void) const
{
    // table SNP features may have comment
    return IsTableSNP()? GetSNPComment(): GetSeq_feat()->GetComment();
}


inline
bool CSeq_feat_Handle::IsSetProduct(void) const
{
    // table SNP features do not have product
    return !IsTableSNP() && GetSeq_feat()->IsSetProduct();
}


inline
bool CSeq_feat_Handle::IsSetQual(void) const
{
    // table SNP features always have qual
    return IsTableSNP() || GetSeq_feat()->IsSetQual();
}


inline
const CSeq_feat::TQual& CSeq_feat_Handle::GetQual(void) const
{
    return GetSeq_feat()->GetQual();
}


inline
bool CSeq_feat_Handle::IsSetTitle(void) const
{
    // table SNP features do not have title
    return !IsTableSNP() && GetSeq_feat()->IsSetTitle();
}


inline
const string& CSeq_feat_Handle::GetTitle(void) const
{
    return GetSeq_feat()->GetTitle();
}


inline
bool CSeq_feat_Handle::IsSetExt(void) const
{
    // table SNP features always have ext
    return IsTableSNP() || GetSeq_feat()->IsSetExt();
}


inline
const CUser_object& CSeq_feat_Handle::GetExt(void) const
{
    return GetSeq_feat()->GetExt();
}


inline
bool CSeq_feat_Handle::IsSetCit(void) const
{
    // table SNP features do not have cit
    return !IsTableSNP() && GetSeq_feat()->IsSetCit();
}


inline
const CPub_set& CSeq_feat_Handle::GetCit(void) const
{
    return GetSeq_feat()->GetCit();
}


inline
bool CSeq_feat_Handle::IsSetExp_ev(void) const
{
    // table SNP features do not have exp-ev
    return !IsTableSNP() && GetSeq_feat()->IsSetExp_ev();
}


inline
CSeq_feat::EExp_ev CSeq_feat_Handle::GetExp_ev(void) const
{
    return GetSeq_feat()->GetExp_ev();
}


inline
bool CSeq_feat_Handle::IsSetXref(void) const
{
    // table SNP features do not have xref
    return !IsTableSNP() && GetSeq_feat()->IsSetXref();
}


inline
const CSeq_feat::TXref& CSeq_feat_Handle::GetXref(void) const
{
    return GetSeq_feat()->GetXref();
}


inline
bool CSeq_feat_Handle::IsSetDbxref(void) const
{
    // table SNP features always have dbxref
    return IsTableSNP() || GetSeq_feat()->IsSetDbxref();
}


inline
const CSeq_feat::TDbxref& CSeq_feat_Handle::GetDbxref(void) const
{
    return GetSeq_feat()->GetDbxref();
}


inline
bool CSeq_feat_Handle::IsSetPseudo(void) const
{
    // table SNP features do not have pseudo
    return !IsTableSNP() && GetSeq_feat()->IsSetPseudo();
}


inline
bool CSeq_feat_Handle::GetPseudo(void) const
{
    return GetSeq_feat()->GetPseudo();
}


inline
bool CSeq_feat_Handle::IsSetExcept_text(void) const
{
    // table SNP features do not have except-text
    return !IsTableSNP() && GetSeq_feat()->IsSetExcept_text();
}


inline
const string& CSeq_feat_Handle::GetExcept_text(void) const
{
    return GetSeq_feat()->GetExcept_text();
}


inline
bool CSeq_feat_Handle::IsSetIds(void) const
{
    // table SNP features do not have ids
    return !IsTableSNP() && GetSeq_feat()->IsSetIds();
}


inline
const CSeq_feat::TIds& CSeq_feat_Handle::GetIds(void) const
{
    return GetSeq_feat()->GetIds();
}


inline
bool CSeq_feat_Handle::IsSetExts(void) const
{
    // table SNP features do not have exts
    return !IsTableSNP() && GetSeq_feat()->IsSetExts();
}


inline
const CSeq_feat::TExts& CSeq_feat_Handle::GetExts(void) const
{
    return GetSeq_feat()->GetExts();
}


inline
CSeqFeatData::E_Choice CSeq_feat_Handle::GetFeatType(void) const
{
    return IsTableSNP()?
        CSeqFeatData::e_Imp:
        x_GetAnnotObject_Info().GetFeatType();
}


inline
CSeqFeatData::ESubtype CSeq_feat_Handle::GetFeatSubtype(void) const
{
    return IsTableSNP()?
        CSeqFeatData::eSubtype_variation:
        x_GetAnnotObject_Info().GetFeatSubtype();
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

    // Methods redirected to corresponding Seq-feat object:
    /// get gene (if present) from Seq-feat.xref list
    void SetGeneXref(CGene_ref& value);
    CGene_ref& SetGeneXref(void);

    /// get protein (if present) from Seq-feat.xref list
    void SetProtXref(CProt_ref& value);
    CProt_ref& SetProtXref(void);

    /// Add a qualifier to this feature
    void AddQualifier(const string& qual_name, const string& qual_val);

    /// add a DB xref to this feature
    void AddDbxref(const string& db_name, const string& db_key);
    void AddDbxref(const string& db_name, int db_key);

protected:
    friend class CSeq_annot_EditHandle;

    CSeq_feat_EditHandle(const CSeq_annot_EditHandle& annot,
                         TFeatIndex feat_index);
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
    return *this;
}

/* @} */

END_SCOPE(objects)
END_NCBI_SCOPE

#endif  // SEQ_FEAT_HANDLE__HPP
