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


#include <ncbi_pch.hpp>
#include <objmgr/seq_feat_handle.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/impl/seq_annot_info.hpp>
#include <objmgr/impl/snp_annot_info.hpp>
#include <objmgr/impl/scope_impl.hpp>
#include <objmgr/impl/annot_collector.hpp>

#include <objmgr/impl/seq_annot_edit_commands.hpp>

#include <objects/general/Dbtag.hpp>
#include <objects/general/Object_id.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CScope;

/////////////////////////////////////////////////////////////////////////////
// CSeq_feat_Handle


CSeq_feat_Handle::CSeq_feat_Handle(const CSeq_annot_Handle& annot,
                                   TFeatIndex feat_index)
    : m_Seq_annot(annot),
      m_FeatIndex(feat_index)
{
    _ASSERT(!IsTableSNP());
    _ASSERT(!IsRemoved());
    _ASSERT(x_GetAnnotObject_Info().IsFeat());
}


CSeq_feat_Handle::CSeq_feat_Handle(const CSeq_annot_Handle& annot,
                                   const SSNP_Info& snp_info,
                                   CCreatedFeat_Ref& created_ref)
    : m_Seq_annot(annot),
      m_FeatIndex(TFeatIndex(annot.x_GetInfo().x_GetSNP_annot_Info().GetIndex(snp_info))
                  | kSNPTableBit),
      m_CreatedFeat(&created_ref)
{
    _ASSERT(IsTableSNP());
    _ASSERT(!IsRemoved());
}


CSeq_feat_Handle::CSeq_feat_Handle(CScope& scope,
                                   CAnnotObject_Info* info)
    : m_Seq_annot(scope.GetSeq_annotHandle
                  (*info->GetSeq_annot_Info().GetSeq_annotSkeleton())),
      m_FeatIndex(info->GetAnnotIndex())
{
}


CSeq_feat_Handle::~CSeq_feat_Handle(void)
{
}


void CSeq_feat_Handle::Reset(void)
{
    m_CreatedOriginalFeat.Reset();
    m_CreatedFeat.Reset();
    m_FeatIndex = 0;
    m_Seq_annot.Reset();
}


const CSeq_annot_SNP_Info& CSeq_feat_Handle::x_GetSNP_annot_Info(void) const
{
    return x_GetSeq_annot_Info().x_GetSNP_annot_Info();
}


bool CSeq_feat_Handle::IsTableSNP(void) const
{
    return (m_FeatIndex & kSNPTableBit) != 0;
}


bool CSeq_feat_Handle::IsPlainFeat(void) const
{
    return (m_FeatIndex & kSNPTableBit) == 0 &&
        x_GetAnnotObject_InfoAny().IsRegular();
}


bool CSeq_feat_Handle::IsTableFeat(void) const
{
    return (m_FeatIndex & kSNPTableBit) == 0 &&
        !x_GetAnnotObject_InfoAny().IsRegular();
}


const CAnnotObject_Info& CSeq_feat_Handle::x_GetAnnotObject_InfoAny(void) const
{
    if ( IsTableSNP() ) {
        NCBI_THROW(CObjMgrException, eInvalidHandle,
                   "CSeq_feat_Handle::x_GetAnnotObject: not Seq-feat info");
    }
    return x_GetSeq_annot_Info().GetInfo(x_GetFeatIndex());
}


const CAnnotObject_Info& CSeq_feat_Handle::x_GetAnnotObject_Info(void) const
{
    const CAnnotObject_Info& info = x_GetAnnotObject_InfoAny();
    if ( info.IsRemoved() ) {
        NCBI_THROW(CObjMgrException, eInvalidHandle,
                   "CSeq_feat_Handle::x_GetAnnotObject_Info: "
                   "Seq-feat was removed");
    }
    return info;
}


const SSNP_Info& CSeq_feat_Handle::x_GetSNP_InfoAny(void) const
{
    if ( !IsTableSNP() ) {
        NCBI_THROW(CObjMgrException, eInvalidHandle,
                   "CSeq_feat_Handle::GetSNP_Info: not SNP info");
    }
    return x_GetSNP_annot_Info().GetInfo(x_GetFeatIndex());
}


const SSNP_Info& CSeq_feat_Handle::x_GetSNP_Info(void) const
{
    const SSNP_Info& info = x_GetSNP_InfoAny();
    if ( info.IsRemoved() ) {
        NCBI_THROW(CObjMgrException, eInvalidHandle,
                   "CSeq_feat_Handle::GetSNP_Info: SNP was removed");
    }
    return info;
}


const CSeq_feat& CSeq_feat_Handle::x_GetPlainSeq_feat(void) const
{
    return x_GetAnnotObject_Info().GetFeat();
}


CConstRef<CSeq_feat> CSeq_feat_Handle::GetPlainSeq_feat(void) const
{
    return ConstRef(&x_GetPlainSeq_feat());
}


CConstRef<CSeq_feat> CSeq_feat_Handle::GetOriginalSeq_feat(void) const
{
    if ( IsPlainFeat() ) {
        return ConstRef(&x_GetPlainSeq_feat());
    }
    else {
        return m_CreatedFeat->GetOriginalFeature(*this);
    }
}


CConstRef<CSeq_feat> CSeq_feat_Handle::GetSeq_feat(void) const
{
    return GetOriginalSeq_feat();
}


bool CSeq_feat_Handle::IsSetPartial(void) const
{
    // table SNP features do not have partial
    return !IsTableSNP() && GetSeq_feat()->IsSetPartial();
}


bool CSeq_feat_Handle::GetPartial(void) const
{
    // table SNP features do not have partial
    return !IsTableSNP() && GetSeq_feat()->GetPartial();
}


const CSeq_loc& CSeq_feat_Handle::GetProduct(void) const
{
    return GetSeq_feat()->GetProduct();
}


const CSeq_loc& CSeq_feat_Handle::GetLocation(void) const
{
    return GetSeq_feat()->GetLocation();
}


bool CSeq_feat_Handle::IsSetData(void) const
{
    if ( !*this ) {
        return false;
    }
    if ( !IsTableSNP() ) {
        return GetSeq_feat()->IsSetData();
    }
    else {
        return true;
    }
}


CSeq_id_Handle CSeq_feat_Handle::GetLocationId(void) const
{
    if ( IsTableSNP() ) {
        return CSeq_id_Handle::GetGiHandle(GetSNPGi());
    }
    CConstRef<CSeq_loc> loc(&GetLocation());
    const CSeq_id* id = loc->GetId();
    if ( id ) {
        return CSeq_id_Handle::GetHandle(*id);
    }
    return CSeq_id_Handle();
}


CSeq_feat_Handle::TRange CSeq_feat_Handle::GetRange(void) const
{
    if ( !IsTableSNP() ) {
        return GetSeq_feat()->GetLocation().GetTotalRange();
    }
    else {
        const SSNP_Info& info = x_GetSNP_Info();
        return TRange(info.GetFrom(), info.GetTo());
    }
}


CSeq_id_Handle CSeq_feat_Handle::GetProductId(void) const
{
    if ( IsSetProduct() ) {
        CConstRef<CSeq_loc> loc(&GetProduct());
        const CSeq_id* id = loc->GetId();
        if ( id ) {
            return CSeq_id_Handle::GetHandle(*id);
        }
    }
    return CSeq_id_Handle();
}


CSeq_feat_Handle::TRange CSeq_feat_Handle::GetProductTotalRange(void) const
{
    if ( IsSetProduct() ) {
        return GetProduct().GetTotalRange();
    }
    return TRange::GetEmpty();
}


CSeq_id::TGi CSeq_feat_Handle::GetSNPGi(void) const
{
    return x_GetSNP_annot_Info().GetGi();
}


const string& CSeq_feat_Handle::GetSNPComment(void) const
{
    return x_GetSNP_annot_Info().x_GetComment(x_GetSNP_Info().m_CommentIndex);
}


size_t CSeq_feat_Handle::GetSNPAllelesCount(void) const
{
    return x_GetSNP_Info().GetAllelesCount();
}


const string& CSeq_feat_Handle::GetSNPAllele(size_t index) const
{
    return x_GetSNP_annot_Info().x_GetAllele(x_GetSNP_Info().GetAlleleStrIndex(index));
}


const string& CSeq_feat_Handle::GetSNPExtra(void) const
{
    return x_GetSNP_annot_Info().x_GetExtra(x_GetSNP_Info().GetExtraIndex());
}


CUser_field::TData::E_Choice
CSeq_feat_Handle::GetSNPQualityCodeWhich(void) const
{
    return x_GetSNP_Info().GetQualityCodesWhich();
}


const string& CSeq_feat_Handle::GetSNPQualityCodeStr(void) const
{
    return x_GetSNP_annot_Info()
        .x_GetQualityCodesStr(x_GetSNP_Info().GetQualityCodesStrIndex());
}


void CSeq_feat_Handle::GetSNPQualityCodeOs(vector<char>& os) const
{
    x_GetSNP_annot_Info()
        .x_GetQualityCodesOs(x_GetSNP_Info().GetQualityCodesOsIndex(), os);
}


bool CSeq_feat_Handle::IsRemoved(void) const
{
    if ( IsTableSNP() ) {
        return x_GetSNP_InfoAny().IsRemoved();
    }
    else {
        return x_GetAnnotObject_InfoAny().IsRemoved();
    }
}


void CSeq_feat_Handle::Remove(void) const
{
    CSeq_feat_EditHandle(*this).Remove();
}


void CSeq_feat_Handle::Replace(const CSeq_feat& new_feat) const
{
    CSeq_feat_EditHandle(*this).Replace(new_feat);
}


/////////////////////////////////////////////////////////////////////////////
// Methods redirected to corresponding Seq-feat object
/////////////////////////////////////////////////////////////////////////////

const CGene_ref* CSeq_feat_Handle::GetGeneXref(void) const
{
    return GetSeq_feat()->GetGeneXref();
}


const CProt_ref* CSeq_feat_Handle::GetProtXref(void) const
{
    return GetSeq_feat()->GetProtXref();
}


CConstRef<CDbtag> CSeq_feat_Handle::GetNamedDbxref(const CTempString& db) const
{
    return GetSeq_feat()->GetNamedDbxref(db);
}


const string& CSeq_feat_Handle::GetNamedQual(const CTempString& qual_name) const
{
    return GetSeq_feat()->GetNamedQual(qual_name);
}


/////////////////////////////////////////////////////////////////////////////
// CSeq_feat_EditHandle


CSeq_feat_EditHandle::CSeq_feat_EditHandle(const CSeq_feat_Handle& h)
    : CSeq_feat_Handle(h)
{
    GetAnnot(); // force check of editing mode
}


CSeq_feat_EditHandle::CSeq_feat_EditHandle(const CSeq_annot_EditHandle& annot,
                                           TFeatIndex feat_index)
    : CSeq_feat_Handle(annot, feat_index)
{
}


CSeq_feat_EditHandle::CSeq_feat_EditHandle(const CSeq_annot_EditHandle& annot,
                                           const SSNP_Info& snp_info,
                                           CCreatedFeat_Ref& created_ref)
    : CSeq_feat_Handle(annot, snp_info, created_ref)
{
}


void CSeq_feat_EditHandle::Remove(void) const
{
    typedef CSeq_annot_Remove_EditCommand<CSeq_feat_EditHandle> TCommand;
    CCommandProcessor processor(GetAnnot().x_GetScopeImpl());
    processor.run(new TCommand(*this));
}


void CSeq_feat_EditHandle::Replace(const CSeq_feat& new_feat) const
{
    typedef CSeq_annot_Replace_EditCommand<CSeq_feat_EditHandle> TCommand;
    CCommandProcessor processor(GetAnnot().x_GetScopeImpl());
    processor.run(new TCommand(*this, new_feat));
}

void CSeq_feat_EditHandle::Update(void) const
{
    GetAnnot().x_GetInfo().Update(x_GetFeatIndex());
}

void CSeq_feat_EditHandle::x_RealRemove(void) const
{
    if ( IsPlainFeat() ) {
        GetAnnot().x_GetInfo().Remove(x_GetFeatIndex());
        _ASSERT(IsRemoved());
    }
    else {
        NCBI_THROW(CObjMgrException, eNotImplemented,
                   "CSeq_feat_Handle::Remove: "
                   "handle is SNP table or Seq-table");
    }
}


void CSeq_feat_EditHandle::x_RealReplace(const CSeq_feat& new_feat) const
{
    if ( IsRemoved() || IsPlainFeat() ) {
        GetAnnot().x_GetInfo().Replace(x_GetFeatIndex(), new_feat);
        _ASSERT(!IsRemoved());
    }
    else {
        NCBI_THROW(CObjMgrException, eNotImplemented,
                   "CSeq_feat_Handle::Replace: "
                   "handle is SNP table or Seq-table");
    }
}


void CSeq_feat_EditHandle::SetGeneXref(CGene_ref& value)
{
    const_cast<CSeq_feat&>(*GetSeq_feat()).SetGeneXref(value);
    //Update(); no index information is changed by GeneXref
}


CGene_ref& CSeq_feat_EditHandle::SetGeneXref(void)
{
    CGene_ref& ret = const_cast<CSeq_feat&>(*GetSeq_feat()).SetGeneXref();
    //Update(); no index information is changed by GeneXref
    return ret;
}


void CSeq_feat_EditHandle::SetProtXref(CProt_ref& value)
{
    const_cast<CSeq_feat&>(*GetSeq_feat()).SetProtXref(value);
    //Update(); no index information is changed by ProtXref
}


CProt_ref& CSeq_feat_EditHandle::SetProtXref(void)
{
    CProt_ref& ret = const_cast<CSeq_feat&>(*GetSeq_feat()).SetProtXref();
    //Update(); no index information is changed by ProtXref
    return ret;
}


void CSeq_feat_EditHandle::AddQualifier(const string& qual_name,
                                        const string& qual_val)
{
    const_cast<CSeq_feat&>(*GetSeq_feat()).AddQualifier(qual_name, qual_val);
    //Update(); no index information is changed by qualifiers
}

void CSeq_feat_EditHandle::RemoveQualifier(const string& qual_name)
{
    const_cast<CSeq_feat&>(*GetSeq_feat()).RemoveQualifier(qual_name);
    //Update(); no index information is changed by qualifiers
}


void CSeq_feat_EditHandle::AddDbxref(const string& db_name,
                                     const string& db_key)
{
    const_cast<CSeq_feat&>(*GetSeq_feat()).AddDbxref(db_name, db_key);
    //Update(); no index information is changed by dbxref
}


void CSeq_feat_EditHandle::AddDbxref(const string& db_name, int db_key)
{
    const_cast<CSeq_feat&>(*GetSeq_feat()).AddDbxref(db_name, db_key);
    //Update(); no index information is changed by dbxref
}

void CSeq_feat_EditHandle::AddExceptText(const string & exception_text)
{
    const_cast<CSeq_feat&>(*GetSeq_feat()).AddExceptText(exception_text);
    //Update(); no index information is changed by except_text
}

void CSeq_feat_EditHandle::RemoveExceptText(const string & exception_text)
{
    const_cast<CSeq_feat&>(*GetSeq_feat()).RemoveExceptText(exception_text);
    //Update(); no index information is changed by except_text
}

void CSeq_feat_EditHandle::AddFeatId(const CObject_id& id)
{
    if ( !IsPlainFeat() ) {
        NCBI_THROW(CObjMgrException, eInvalidHandle,
                   "CSeq_feat_EditHandle::AddFeatId: not plain Seq-feat");
    }
    GetAnnot().x_GetInfo().AddFeatId(x_GetFeatIndex(), id, eFeatId_id);
}


void CSeq_feat_EditHandle::AddFeatId(int id)
{
    CObject_id obj_id;
    obj_id.SetId(id);
    AddFeatId(obj_id);
}


void CSeq_feat_EditHandle::AddFeatId(const string& id)
{
    CObject_id obj_id;
    obj_id.SetStr(id);
    AddFeatId(obj_id);
}


void CSeq_feat_EditHandle::AddFeatXref(const CObject_id& id)
{
    if ( !IsPlainFeat() ) {
        NCBI_THROW(CObjMgrException, eInvalidHandle,
                   "CSeq_feat_EditHandle::AddFeatXref: not plain Seq-feat");
    }
    GetAnnot().x_GetInfo().AddFeatId(x_GetFeatIndex(), id, eFeatId_xref);
}


void CSeq_feat_EditHandle::AddFeatXref(int id)
{
    CObject_id obj_id;
    obj_id.SetId(id);
    AddFeatXref(obj_id);
}


void CSeq_feat_EditHandle::AddFeatXref(const string& id)
{
    CObject_id obj_id;
    obj_id.SetStr(id);
    AddFeatXref(obj_id);
}


void CSeq_feat_EditHandle::RemoveFeatId(const CObject_id& id)
{
    if ( !IsPlainFeat() ) {
        NCBI_THROW(CObjMgrException, eInvalidHandle,
                   "CSeq_feat_EditHandle::RemoveFeatId: not plain Seq-feat");
    }
    GetAnnot().x_GetInfo().RemoveFeatId(x_GetFeatIndex(), id, eFeatId_id);
}


void CSeq_feat_EditHandle::RemoveFeatId(int id)
{
    CObject_id obj_id;
    obj_id.SetId(id);
    RemoveFeatId(obj_id);
}


void CSeq_feat_EditHandle::RemoveFeatId(const string& id)
{
    CObject_id obj_id;
    obj_id.SetStr(id);
    RemoveFeatId(obj_id);
}


void CSeq_feat_EditHandle::RemoveFeatXref(const CObject_id& id)
{
    if ( !IsPlainFeat() ) {
        NCBI_THROW(CObjMgrException, eInvalidHandle,
                   "CSeq_feat_EditHandle::RemoveFeatXref: not plain Seq-feat");
    }
    GetAnnot().x_GetInfo().RemoveFeatId(x_GetFeatIndex(), id, eFeatId_xref);
}


void CSeq_feat_EditHandle::RemoveFeatXref(int id)
{
    CObject_id obj_id;
    obj_id.SetId(id);
    RemoveFeatXref(obj_id);
}


void CSeq_feat_EditHandle::RemoveFeatXref(const string& id)
{
    CObject_id obj_id;
    obj_id.SetStr(id);
    RemoveFeatXref(obj_id);
}


void CSeq_feat_EditHandle::ClearFeatIds(void)
{
    if ( !IsPlainFeat() ) {
        NCBI_THROW(CObjMgrException, eInvalidHandle,
                   "CSeq_feat_EditHandle::ClearFeatIds: not plain Seq-feat");
    }
    GetAnnot().x_GetInfo().ClearFeatIds(x_GetFeatIndex(), eFeatId_id);
}


void CSeq_feat_EditHandle::ClearFeatXrefs(void)
{
    if ( !IsPlainFeat() ) {
        NCBI_THROW(CObjMgrException, eInvalidHandle,
                   "CSeq_feat_EditHandle::ClearFeatXrefs: not plain Seq-feat");
    }
    GetAnnot().x_GetInfo().ClearFeatIds(x_GetFeatIndex(), eFeatId_xref);
}


void CSeq_feat_EditHandle::SetFeatId(const CObject_id& id)
{
    ClearFeatIds();
    AddFeatId(id);
}


void CSeq_feat_EditHandle::SetFeatId(int id)
{
    CObject_id obj_id;
    obj_id.SetId(id);
    SetFeatId(obj_id);
}


void CSeq_feat_EditHandle::SetFeatId(const string& id)
{
    CObject_id obj_id;
    obj_id.SetStr(id);
    SetFeatId(obj_id);
}


/////////////////////////////////////////////////////////////////////////////
// CSeq_annot_ftable_CI

CSeq_annot_ftable_CI::CSeq_annot_ftable_CI(const CSeq_annot_Handle& annot,
                                           TFlags flags)
    : m_Flags(flags)
{
    if ( !annot.IsFtable() ) {
        NCBI_THROW(CObjMgrException, eInvalidHandle,
                   "CSeq_annot_ftable_CI: annot is not ftable");
    }
    m_Feat.m_Seq_annot = annot;
    m_Feat.m_FeatIndex = 0;
    if ( m_Flags & fIncludeTable && annot.x_GetInfo().x_HasSNP_annot_Info() ) {
        m_Feat.m_FeatIndex |= m_Feat.kSNPTableBit;
    }
    x_Settle();
}


void CSeq_annot_ftable_CI::x_Step(void)
{
    ++m_Feat.m_FeatIndex;
    x_Settle();
}


void CSeq_annot_ftable_CI::x_Reset(void)
{
    // mark end of features
    m_Feat.Reset();
}


void CSeq_annot_ftable_CI::x_Settle(void)
{
    for ( ;; ) {
        CSeq_feat_Handle::TFeatIndex end;
        bool is_snp_table = m_Feat.IsTableSNP();
        if ( is_snp_table ) {
            end = GetAnnot().x_GetInfo().x_GetSNPFeatCount()
                | m_Feat.kSNPTableBit;
        }
        else {
            end = GetAnnot().x_GetInfo().x_GetAnnotCount();
        }
        while ( m_Feat.m_FeatIndex < end ) {
            if ( !m_Feat.IsRemoved() ) {
                return;
            }
            ++m_Feat.m_FeatIndex;
        }
        if ( !is_snp_table || (m_Flags & fOnlyTable) ) {
            break;
        }
        m_Feat.m_FeatIndex = 0;
    }
    x_Reset();
}


/////////////////////////////////////////////////////////////////////////////
// CSeq_annot_ftable_I

CSeq_annot_ftable_I::CSeq_annot_ftable_I(const CSeq_annot_EditHandle& annot,
                                           TFlags flags)
    : m_Annot(annot), m_Flags(flags)
{
    if ( !annot.IsFtable() ) {
        NCBI_THROW(CObjMgrException, eInvalidHandle,
                   "CSeq_annot_ftable_I: annot is not ftable");
    }
    m_Feat.m_Seq_annot = annot;
    m_Feat.m_FeatIndex = 0;
    if ( m_Flags & fIncludeTable && annot.x_GetInfo().x_HasSNP_annot_Info() ) {
        m_Feat.m_FeatIndex |= m_Feat.kSNPTableBit;
    }
    x_Settle();
}


void CSeq_annot_ftable_I::x_Step(void)
{
    ++m_Feat.m_FeatIndex;
    x_Settle();
}


void CSeq_annot_ftable_I::x_Reset(void)
{
    // mark end of features
    m_Feat.Reset();
}


void CSeq_annot_ftable_I::x_Settle(void)
{
    for ( ;; ) {
        CSeq_feat_Handle::TFeatIndex end;
        bool is_snp_table = m_Feat.IsTableSNP();
        if ( is_snp_table ) {
            end = GetAnnot().x_GetInfo().x_GetSNPFeatCount()
                | m_Feat.kSNPTableBit;
        }
        else {
            end = GetAnnot().x_GetInfo().x_GetAnnotCount();
        }
        while ( m_Feat.m_FeatIndex < end ) {
            if ( !m_Feat.IsRemoved() ) {
                return;
            }
            ++m_Feat.m_FeatIndex;
        }
        if ( !is_snp_table || (m_Flags & fOnlyTable) ) {
            break;
        }
        m_Feat.m_FeatIndex = 0;
    }
    x_Reset();
}


END_SCOPE(objects)
END_NCBI_SCOPE
