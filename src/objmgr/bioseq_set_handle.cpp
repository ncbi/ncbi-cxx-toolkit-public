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

#include <objmgr/bioseq_set_handle.hpp>

#include <objmgr/scope.hpp>

#include <objmgr/impl/scope_impl.hpp>
#include <objmgr/impl/bioseq_set_info.hpp>

#include <objects/seqset/Bioseq_set.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


CBioseq_set_Handle::CBioseq_set_Handle(CScope& scope,
                                       const CBioseq_set_Info& info)
    : m_Scope(&scope), m_Info(&info)
{
}


CConstRef<CBioseq_set> CBioseq_set_Handle::GetCompleteBioseq_set(void) const
{
    return x_GetInfo().GetCompleteBioseq_set();
}


CConstRef<CBioseq_set> CBioseq_set_Handle::GetBioseq_setCore(void) const
{
    return x_GetInfo().GetBioseq_setCore();
}


CSeq_entry_Handle CBioseq_set_Handle::GetParentEntry(void) const
{
    CSeq_entry_Handle ret;
    const CBioseq_set_Info& info = x_GetInfo();
    if ( info.HaveParent_Info() ) {
        ret = CSeq_entry_Handle(GetScope(), info.GetParentSeq_entry_Info());
    }
    return ret;
}


CSeq_entry_EditHandle CBioseq_set_EditHandle::GetParentEntry(void) const
{
    CSeq_entry_EditHandle ret;
    CBioseq_set_Info& info = x_GetInfo();
    if ( info.HaveParent_Info() ) {
        ret = CSeq_entry_EditHandle(GetScope(),
                                    info.GetParentSeq_entry_Info());
    }
    return ret;
}


bool CBioseq_set_Handle::IsSetId(void) const
{
    return x_GetInfo().IsSetId();
}


const CBioseq_set::TId& CBioseq_set_Handle::GetId(void) const
{
    return x_GetInfo().GetId();
}


bool CBioseq_set_Handle::IsSetColl(void) const
{
    return x_GetInfo().IsSetColl();
}


const CBioseq_set::TColl& CBioseq_set_Handle::GetColl(void) const
{
    return x_GetInfo().GetColl();
}


bool CBioseq_set_Handle::IsSetLevel(void) const
{
    return x_GetInfo().IsSetLevel();
}


CBioseq_set::TLevel CBioseq_set_Handle::GetLevel(void) const
{
    return x_GetInfo().GetLevel();
}


bool CBioseq_set_Handle::IsSetClass(void) const
{
    return x_GetInfo().IsSetClass();
}


CBioseq_set::TClass CBioseq_set_Handle::GetClass(void) const
{
    return x_GetInfo().GetClass();
}


bool CBioseq_set_Handle::IsSetRelease(void) const
{
    return x_GetInfo().IsSetRelease();
}


const CBioseq_set::TRelease& CBioseq_set_Handle::GetRelease(void) const
{
    return x_GetInfo().GetRelease();
}


bool CBioseq_set_Handle::IsSetDate(void) const
{
    return x_GetInfo().IsSetDate();
}


const CBioseq_set::TDate& CBioseq_set_Handle::GetDate(void) const
{
    return x_GetInfo().GetDate();
}


bool CBioseq_set_Handle::IsSetDescr(void) const
{
    return x_GetInfo().IsSetDescr();
}


const CSeq_descr& CBioseq_set_Handle::GetDescr(void) const
{
    return x_GetInfo().GetDescr();
}


CSeq_entry_EditHandle
CBioseq_set_EditHandle::AttachEntry(const CSeq_entry& entry)
{
    return x_GetScopeImpl()->AttachEntry(*this, entry);
}


CSeq_annot_EditHandle
CBioseq_set_EditHandle::AttachAnnot(const CSeq_annot& annot)
{
    return x_GetScopeImpl()->AttachAnnot(*this, annot);
}


void CBioseq_set_EditHandle::RemoveEntry(void)
{
    CSeq_entry_EditHandle entry = GetParentEntry();
    x_GetScopeImpl()->RemoveEntry(entry);
}


void CBioseq_set_EditHandle::RemoveEntry(CSeq_entry_EditHandle& entry)
{
    if ( entry.GetParentEntry() != GetParentEntry() ) {
        NCBI_THROW(CObjMgrException, eModifyDataError,
                   "CBioseq_set_EditHandle::RemoveEntry: entry is not owned");
    }
    entry.RemoveEntry();
}


void CBioseq_set_EditHandle::RemoveAnnot(CSeq_annot_EditHandle& annot)
{
    if ( annot.GetParentEntry() != GetParentEntry() ) {
        NCBI_THROW(CObjMgrException, eModifyDataError,
                   "CBioseq_set_EditHandle::RemoveAnnot: annot is not owned");
    }
    annot.RemoveAnnot();
}


CSeq_annot_EditHandle
CBioseq_set_EditHandle::ReplaceAnnot(CSeq_annot_EditHandle& old_annot,
                                    const CSeq_annot& new_annot)
{
    if ( old_annot.GetParentEntry() != GetParentEntry() ) {
        NCBI_THROW(CObjMgrException, eModifyDataError,
                   "CBioseq_set_EditHandle::ReplaceAnnot: annot is not owned");
    }
    return x_GetScopeImpl()->ReplaceAnnot(old_annot, new_annot);
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2004/03/16 15:47:27  vasilche
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
