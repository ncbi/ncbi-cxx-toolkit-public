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
    if ( info.HasParent_Info() ) {
        ret = CSeq_entry_Handle(GetScope(), info.GetParentSeq_entry_Info());
    }
    return ret;
}


CSeq_entry_EditHandle CSeq_entry_Handle::GetEditHandle(void) const
{
    return m_Scope->GetEditHandle(*this);
}


CSeq_entry_EditHandle CSeq_entry_EditHandle::GetParentEntry(void) const
{
    CSeq_entry_EditHandle ret;
    CSeq_entry_Info& info = x_GetInfo();
    if ( info.HasParent_Info() ) {
        ret = CSeq_entry_EditHandle(GetScope(),
                                    info.GetParentSeq_entry_Info());
    }
    return ret;
}


CBioseq_set_Handle CSeq_entry_Handle::GetParentBioseq_set(void) const
{
    CBioseq_set_Handle ret;
    const CSeq_entry_Info& info = x_GetInfo();
    if ( info.HasParent_Info() ) {
        ret = CBioseq_set_Handle(GetScope(), info.GetParentBioseq_set_Info());
    }
    return ret;
}


CBioseq_set_EditHandle CSeq_entry_EditHandle::GetParentBioseq_set(void) const
{
    CBioseq_set_EditHandle ret;
    CSeq_entry_Info& info = x_GetInfo();
    if ( info.HasParent_Info() ) {
        ret = CBioseq_set_EditHandle(GetScope(),
                                     info.GetParentBioseq_set_Info());
    }
    return ret;
}


CBioseq_Handle CSeq_entry_Handle::GetSeq(void) const
{
    return m_Scope->GetBioseqHandle(x_GetInfo().GetSeq());
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


CBioseq_EditHandle CSeq_entry_EditHandle::SetSeq(void) const
{
    return m_Scope->GetBioseqHandle(x_GetInfo().SetSeq()).GetEditHandle();
}


CBioseq_EditHandle
CSeq_entry_EditHandle::AttachBioseq(CBioseq& seq, int index) const
{
    return SetSet().AttachBioseq(seq, index);
}


CBioseq_EditHandle
CSeq_entry_EditHandle::CopyBioseq(const CBioseq_Handle& seq,
                                  int index) const
{
    return SetSet().CopyBioseq(seq, index);
}


CBioseq_EditHandle
CSeq_entry_EditHandle::TakeBioseq(const CBioseq_EditHandle& seq,
                                  int index) const
{
    return SetSet().TakeBioseq(seq, index);
}


CSeq_entry_EditHandle
CSeq_entry_EditHandle::AttachEntry(CSeq_entry& entry, int index) const
{
    return SetSet().AttachEntry(entry, index);
}


CSeq_entry_EditHandle
CSeq_entry_EditHandle::CopyEntry(const CSeq_entry_Handle& entry,
                                 int index) const
{
    return SetSet().CopyEntry(entry, index);
}


CSeq_entry_EditHandle
CSeq_entry_EditHandle::TakeEntry(const CSeq_entry_EditHandle& entry,
                                 int index) const
{
    return SetSet().TakeEntry(entry, index);
}


CSeq_annot_EditHandle
CSeq_entry_EditHandle::AttachAnnot(const CSeq_annot& annot) const
{
    return m_Scope->AttachAnnot(*this, annot);
}


CSeq_annot_EditHandle
CSeq_entry_EditHandle::CopyAnnot(const CSeq_annot_Handle& annot) const
{
    return m_Scope->CopyAnnot(*this, annot);
}


CSeq_annot_EditHandle
CSeq_entry_EditHandle::TakeAnnot(const CSeq_annot_EditHandle& annot) const
{
    return m_Scope->TakeAnnot(*this, annot);
}


void CSeq_entry_EditHandle::Remove(void) const
{
    m_Scope->RemoveEntry(*this);
}


CBioseq_set_EditHandle CSeq_entry_EditHandle::SelectSet(void) const
{
    return SelectSet(*new CBioseq_set);
}


CBioseq_set_EditHandle
CSeq_entry_EditHandle::SelectSet(CBioseq_set& seqset) const
{
    return m_Scope->SelectSet(*this, seqset);
}


CBioseq_set_EditHandle
CSeq_entry_EditHandle::CopySet(const CBioseq_set_Handle& seqset) const
{
    return m_Scope->CopySet(*this, seqset);
}


CBioseq_set_EditHandle
CSeq_entry_EditHandle::TakeSet(const CBioseq_set_EditHandle& seqset) const
{
    return m_Scope->TakeSet(*this, seqset);
}


CBioseq_EditHandle CSeq_entry_EditHandle::SelectSeq(CBioseq& seq) const
{
    return m_Scope->SelectSeq(*this, seq);
}


CBioseq_EditHandle
CSeq_entry_EditHandle::CopySeq(const CBioseq_Handle& seq) const
{
    return m_Scope->CopySeq(*this, seq);
}


CBioseq_EditHandle
CSeq_entry_EditHandle::TakeSeq(const CBioseq_EditHandle& seq) const
{
    return m_Scope->TakeSeq(*this, seq);
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.7  2004/03/29 20:13:06  vasilche
* Implemented whole set of methods to modify Seq-entry object tree.
* Added CBioseq_Handle::GetExactComplexityLevel().
*
* Revision 1.6  2004/03/24 18:30:30  vasilche
* Fixed edit API.
* Every *_Info object has its own shallow copy of original object.
*
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
