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
*    Handle to Seq-entry object
*
*/

#include <objmgr/seq_entry_handle.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/bioseq_set_handle.hpp>
#include <objmgr/scope.hpp>

#include <objmgr/impl/seq_entry_info.hpp>
#include <objmgr/impl/scope_impl.hpp>
#include <objmgr/impl/bioseq_set_info.hpp>
#include <objmgr/impl/bioseq_info.hpp>

#include <objects/seqset/Bioseq_set.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


CSeq_entry_Handle::CSeq_entry_Handle(CScope& scope,
                                     const CSeq_entry_Info& info)
    : m_Scope(&scope), m_Info(&info)
{
}


void CSeq_entry_Handle::Reset(void)
{
    m_Scope.Reset();
    m_Info.Reset();
}


CSeq_entry::E_Choice CSeq_entry_Handle::Which(void) const
{
    return x_GetInfo().Which();
}


CConstRef<CSeq_entry> CSeq_entry_Handle::GetCompleteSeq_entry(void) const
{
    return x_GetInfo().GetCompleteSeq_entry();
}


CConstRef<CSeq_entry> CSeq_entry_Handle::GetSeq_entryCore(void) const
{
    return x_GetInfo().GetSeq_entryCore();
}


CSeq_entry_Handle CSeq_entry_Handle::GetParentEntry(void) const
{
    CSeq_entry_Handle ret;
    const CSeq_entry_Info& info = x_GetInfo();
    if ( info.HaveParent_Info() ) {
        ret = CSeq_entry_Handle(GetScope(), info.GetParentSeq_entry_Info());
    }
    return ret;
}


CSeq_entry_EditHandle CSeq_entry_EditHandle::GetParentEntry(void) const
{
    CSeq_entry_EditHandle ret;
    CSeq_entry_Info& info = x_GetInfo();
    if ( info.HaveParent_Info() ) {
        ret = CSeq_entry_EditHandle(GetScope(),
                                    info.GetParentSeq_entry_Info());
    }
    return ret;
}


CBioseq_set_Handle CSeq_entry_Handle::GetParentBioseq_set(void) const
{
    CBioseq_set_Handle ret;
    const CSeq_entry_Info& info = x_GetInfo();
    if ( info.HaveParent_Info() ) {
        ret = CBioseq_set_Handle(GetScope(), info.GetParentBioseq_set_Info());
    }
    return ret;
}


CBioseq_set_EditHandle CSeq_entry_EditHandle::GetParentBioseq_set(void) const
{
    CBioseq_set_EditHandle ret;
    CSeq_entry_Info& info = x_GetInfo();
    if ( info.HaveParent_Info() ) {
        ret = CBioseq_set_EditHandle(GetScope(),
                                     info.GetParentBioseq_set_Info());
    }
    return ret;
}


CBioseq_Handle CSeq_entry_Handle::GetSeq(void) const
{
    _ASSERT( IsSeq() );
    CBioseq_Handle ret;
    CConstRef<CBioseq_Info> info(&x_GetInfo().GetSeq());
    const CBioseq_Info::TId& syns = info->GetId();
    ITERATE(CBioseq_Info::TId, id, syns) {
        ret = m_Scope->GetBioseqHandleFromTSE(*id, *this);
        if ( ret ) {
            break;
        }
    }
    return ret;
}


CConstRef<CTSE_Info> CSeq_entry_Handle::GetTSE_Info(void) const
{
    CConstRef<CTSE_Info> ret;
    if ( *this ) {
        ret.Reset(&x_GetInfo().GetTSE_Info());
    }
    return ret;
}


CBioseq_set_Handle CSeq_entry_Handle::GetSet(void) const
{
    return CBioseq_set_Handle(GetScope(), x_GetInfo().GetSet());
}


bool CSeq_entry_Handle::Set_IsSetId(void) const
{
    return GetSet().IsSetId();
}


const CBioseq_set::TId& CSeq_entry_Handle::Set_GetId(void) const
{
    return GetSet().GetId();
}


bool CSeq_entry_Handle::Set_IsSetColl(void) const
{
    return GetSet().IsSetColl();
}


const CBioseq_set::TColl& CSeq_entry_Handle::Set_GetColl(void) const
{
    return GetSet().GetColl();
}


bool CSeq_entry_Handle::Set_IsSetLevel(void) const
{
    return GetSet().IsSetLevel();
}


CBioseq_set::TLevel CSeq_entry_Handle::Set_GetLevel(void) const
{
    return GetSet().GetLevel();
}


bool CSeq_entry_Handle::Set_IsSetClass(void) const
{
    return GetSet().IsSetClass();
}


CBioseq_set::TClass CSeq_entry_Handle::Set_GetClass(void) const
{
    return GetSet().GetClass();
}


bool CSeq_entry_Handle::Set_IsSetRelease(void) const
{
    return GetSet().IsSetRelease();
}


const CBioseq_set::TRelease& CSeq_entry_Handle::Set_GetRelease(void) const
{
    return GetSet().GetRelease();
}


bool CSeq_entry_Handle::Set_IsSetDate(void) const
{
    return GetSet().IsSetDate();
}


const CBioseq_set::TDate& CSeq_entry_Handle::Set_GetDate(void) const
{
    return GetSet().GetDate();
}


bool CSeq_entry_Handle::IsSetDescr(void) const
{
    return x_GetInfo().IsSetDescr();
}


const CSeq_descr& CSeq_entry_Handle::GetDescr(void) const
{
    return x_GetInfo().GetDescr();
}


CConstRef<CObject> CSeq_entry_Handle::GetBlobId(void) const
{
    return x_GetInfo().GetTSE_Info().GetBlobId();
}


CSeq_entry_Info& CSeq_entry_EditHandle::x_GetInfo(void) const
{
    return const_cast<CSeq_entry_Info&>(CSeq_entry_Handle::x_GetInfo());
}


CBioseq_set_EditHandle CSeq_entry_EditHandle::SetSet(void) const
{
    return CBioseq_set_EditHandle(GetScope(), x_GetInfo().SetSet());
}


CSeq_entry_EditHandle
CSeq_entry_EditHandle::AttachEntry(const CSeq_entry& entry)
{
    return SetSet().AttachEntry(entry);
}


CSeq_annot_EditHandle
CSeq_entry_EditHandle::AttachAnnot(const CSeq_annot& annot)
{
    return m_Scope->AttachAnnot(*this, annot);
}


void CSeq_entry_EditHandle::Remove(void)
{
    m_Scope->RemoveEntry(*this);
}


void CSeq_entry_EditHandle::RemoveEntry(CSeq_entry_EditHandle& entry)
{
    if ( entry.GetParentEntry() != *this ) {
        NCBI_THROW(CObjMgrException, eModifyDataError,
                   "CSeq_entry_EditHandle::RemoveEntry: entry is not owned");
    }
    entry.Remove();
}


void CSeq_entry_EditHandle::RemoveAnnot(CSeq_annot_EditHandle& annot)
{
    if ( annot.GetParentEntry() != *this ) {
        NCBI_THROW(CObjMgrException, eModifyDataError,
                   "CSeq_entry_EditHandle::RemoveAnnot: annot is not owned");
    }
    m_Scope->RemoveAnnot(annot);
}


CSeq_annot_EditHandle
CSeq_entry_EditHandle::ReplaceAnnot(CSeq_annot_EditHandle& old_annot,
                                    const CSeq_annot& new_annot)
{
    if ( old_annot.GetParentEntry() != *this ) {
        NCBI_THROW(CObjMgrException, eModifyDataError,
                   "CSeq_entry_EditHandle::ReplaceAnnot: annot is not owned");
    }
    return m_Scope->ReplaceAnnot(old_annot, new_annot);
}


CBioseq_set_EditHandle
CSeq_entry_EditHandle::MakeSet(void)
{
    if ( !IsSet() ) {
        NCBI_THROW(CObjMgrException, eOtherError,
                   "CSeq_entry_EditHandle::MakeSet: not implemented");
    }
    return SetSet();
}


void CSeq_entry_EditHandle::MakeSeq(CBioseq_EditHandle& seq)
{
    if ( !IsSeq() ) {
        NCBI_THROW(CObjMgrException, eOtherError,
                   "CSeq_entry_EditHandle::MakeSet: not implemented");
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.5  2004/03/16 21:01:32  vasilche
* Added methods to move Bioseq withing Seq-entry
*
* Revision 1.4  2004/03/16 15:47:28  vasilche
* Added CBioseq_set_Handle and set of EditHandles
*
* Revision 1.3  2004/02/09 22:09:14  grichenk
* Use CConstRef for id
*
* Revision 1.2  2004/02/09 19:18:54  grichenk
* Renamed CDesc_CI to CSeq_descr_CI. Redesigned CSeq_descr_CI
* and CSeqdesc_CI to avoid using data directly.
*
* Revision 1.1  2003/11/28 15:12:31  grichenk
* Initial revision
*
*
* ===========================================================================
*/
