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

#include <ncbi_pch.hpp>
#include <objmgr/seq_entry_handle.hpp>
#include <objmgr/seq_entry_ci.hpp>
#include <objmgr/seq_annot_ci.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/bioseq_set_handle.hpp>
#include <objmgr/scope.hpp>

#include <objmgr/impl/seq_entry_info.hpp>
#include <objmgr/impl/scope_impl.hpp>
#include <objmgr/impl/tse_info.hpp>
#include <objmgr/impl/bioseq_set_info.hpp>
#include <objmgr/impl/bioseq_info.hpp>

#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seq/Seqdesc.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


CSeq_entry_Handle::CSeq_entry_Handle(const CTSE_Handle& tse)
    : m_TSE(tse), m_Info(&tse.x_GetTSE_Info())
{
}


CSeq_entry_Handle::CSeq_entry_Handle(const CSeq_entry_Info& info,
                                     const CTSE_Handle& tse)
    : m_TSE(tse), m_Info(&info)
{
}


CSeq_entry_Handle& CSeq_entry_Handle::operator=(const CSeq_entry_Handle& seh)
{
    // order is significant
    if ( this != &seh ) {
        m_Info = seh.m_Info;
        m_TSE = seh.m_TSE;
    }
    return *this;
}


void CSeq_entry_Handle::Reset(void)
{
    // order is significant
    m_Info.Reset();
    m_TSE.Reset();
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


bool CSeq_entry_Handle::HasParentEntry(void) const
{
    return m_Info  &&  x_GetInfo().HasParent_Info();
}


CSeq_entry_Handle CSeq_entry_Handle::GetParentEntry(void) const
{
    CSeq_entry_Handle ret;
    const CSeq_entry_Info& info = x_GetInfo();
    if ( info.HasParent_Info() ) {
        ret = CSeq_entry_Handle(info.GetParentSeq_entry_Info(),
                                GetTSE_Handle());
    }
    return ret;
}


CSeq_entry_Handle CSeq_entry_Handle::GetTopLevelEntry(void) const
{
    return GetTSE_Handle();
}


CSeq_entry_Handle CSeq_entry_Handle::GetSingleSubEntry(void) const
{
    if ( !IsSet() ) {
        NCBI_THROW(CObjMgrException, eModifyDataError,
                   "CSeq_entry_Handle::GetSingleSubEntry: "
                   "Seq-entry is not Bioseq-set");
    }
    CSeq_entry_CI iter(*this);
    if ( !iter ) {
        NCBI_THROW(CObjMgrException, eModifyDataError,
                   "CSeq_entry_Handle::GetSingleSubEntry: "
                   "Seq-entry is empty");
    }
    CSeq_entry_Handle entry = *iter;
    if ( ++iter ) {
        NCBI_THROW(CObjMgrException, eModifyDataError,
                   "CSeq_entry_Handle::GetSingleSubEntry: "
                   "Seq-entry contains more than one sub entry");
    }
    return entry;
}


CSeq_entry_EditHandle CSeq_entry_Handle::GetEditHandle(void) const
{
    return GetScope().GetEditHandle(*this);
}


CBioseq_set_Handle CSeq_entry_Handle::GetParentBioseq_set(void) const
{
    CBioseq_set_Handle ret;
    const CSeq_entry_Info& info = x_GetInfo();
    if ( info.HasParent_Info() ) {
        ret = CBioseq_set_Handle(info.GetParentBioseq_set_Info(),
                                 GetTSE_Handle());
    }
    return ret;
}


CBioseq_Handle CSeq_entry_Handle::GetSeq(void) const
{
    return x_GetScopeImpl().GetBioseqHandle(x_GetInfo().GetSeq(),
                                            GetTSE_Handle());
}


CBioseq_set_Handle CSeq_entry_Handle::GetSet(void) const
{
    return CBioseq_set_Handle(x_GetInfo().GetSet(),
                              GetTSE_Handle());
}


bool CSeq_entry_Handle::IsSetDescr(void) const
{
    return x_GetInfo().IsSetDescr();
}


const CSeq_descr& CSeq_entry_Handle::GetDescr(void) const
{
    return x_GetInfo().GetDescr();
}


CSeq_entry_Handle::TBlobId CSeq_entry_Handle::GetBlobId(void) const
{
    return x_GetInfo().GetTSE_Info().GetBlobId();
}


CSeq_entry_Handle::TBlobVersion CSeq_entry_Handle::GetBlobVersion(void) const
{
    return x_GetInfo().GetTSE_Info().GetBlobVersion();
}


/////////////////////////////////////////////////////////////////////////////
// CSeq_entry_EditHandle


CSeq_entry_EditHandle CSeq_entry_EditHandle::GetParentEntry(void) const
{
    CSeq_entry_EditHandle ret;
    CSeq_entry_Info& info = x_GetInfo();
    if ( info.HasParent_Info() ) {
        ret = CSeq_entry_EditHandle(info.GetParentSeq_entry_Info(),
                                    GetTSE_Handle());
    }
    return ret;
}


CSeq_entry_EditHandle CSeq_entry_EditHandle::GetSingleSubEntry(void) const
{
    return CSeq_entry_Handle::GetSingleSubEntry().GetEditHandle();
}


CBioseq_set_EditHandle CSeq_entry_EditHandle::GetParentBioseq_set(void) const
{
    CBioseq_set_EditHandle ret;
    CSeq_entry_Info& info = x_GetInfo();
    if ( info.HasParent_Info() ) {
        ret = CBioseq_set_EditHandle(info.GetParentBioseq_set_Info(),
                                     GetTSE_Handle());
    }
    return ret;
}


CSeq_entry_Info& CSeq_entry_EditHandle::x_GetInfo(void) const
{
    return const_cast<CSeq_entry_Info&>(CSeq_entry_Handle::x_GetInfo());
}


CBioseq_set_EditHandle CSeq_entry_EditHandle::SetSet(void) const
{
    return CBioseq_set_EditHandle(x_GetInfo().SetSet(),
                                  GetTSE_Handle());
}


CBioseq_EditHandle CSeq_entry_EditHandle::SetSeq(void) const
{
    return x_GetScopeImpl().GetBioseqHandle(x_GetInfo().SetSeq(),
                                            GetTSE_Handle()).GetEditHandle();
}


void CSeq_entry_EditHandle::SetDescr(TDescr& v) const
{
    x_GetInfo().SetDescr(v);
}


void CSeq_entry_EditHandle::ResetDescr(void) const
{
    x_GetInfo().ResetDescr();
}


bool CSeq_entry_EditHandle::AddSeqdesc(CSeqdesc& v) const
{
    return x_GetInfo().AddSeqdesc(v);
}


CRef<CSeqdesc> CSeq_entry_EditHandle::RemoveSeqdesc(const CSeqdesc& v) const
{
    return x_GetInfo().RemoveSeqdesc(v);
}


void CSeq_entry_EditHandle::AddDescr(const CSeq_entry_EditHandle& entry) const
{
    x_GetInfo().AddDescr(entry.x_GetInfo());
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
    return x_GetScopeImpl().AttachAnnot(*this, annot);
}


CSeq_annot_EditHandle
CSeq_entry_EditHandle::CopyAnnot(const CSeq_annot_Handle& annot) const
{
    return x_GetScopeImpl().CopyAnnot(*this, annot);
}


CSeq_annot_EditHandle
CSeq_entry_EditHandle::TakeAnnot(const CSeq_annot_EditHandle& annot) const
{
    return x_GetScopeImpl().TakeAnnot(*this, annot);
}


void
CSeq_entry_EditHandle::TakeAllAnnots(const CSeq_entry_EditHandle& entry) const
{
    vector<CSeq_annot_Handle> annots;
    // we have to copy all handles as moving annots directly could break iter
    for ( CSeq_annot_CI it(entry, CSeq_annot_CI::eSearch_entry); it; ++it ) {
        annots.push_back(*it);
    }
    ITERATE ( vector<CSeq_annot_Handle>, it, annots ) {
        TakeAnnot(it->GetEditHandle());
    }
}


void CSeq_entry_EditHandle::Remove(void) const
{
    x_GetScopeImpl().RemoveEntry(*this);
}


CBioseq_set_EditHandle CSeq_entry_EditHandle::SelectSet(TClass set_class) const
{
    CBioseq_set_EditHandle seqset = SelectSet(*new CBioseq_set);
    if ( set_class != CBioseq_set::eClass_not_set ) {
        seqset.SetClass(set_class);
    }
    return seqset;
}


void CSeq_entry_EditHandle::SelectNone(void) const
{
    x_GetScopeImpl().SelectNone(*this);
}


CBioseq_set_EditHandle
CSeq_entry_EditHandle::SelectSet(CBioseq_set& seqset) const
{
    return x_GetScopeImpl().SelectSet(*this, seqset);
}


CBioseq_set_EditHandle
CSeq_entry_EditHandle::CopySet(const CBioseq_set_Handle& seqset) const
{
    return x_GetScopeImpl().CopySet(*this, seqset);
}


CBioseq_set_EditHandle
CSeq_entry_EditHandle::TakeSet(const CBioseq_set_EditHandle& seqset) const
{
    return x_GetScopeImpl().TakeSet(*this, seqset);
}


CBioseq_EditHandle CSeq_entry_EditHandle::SelectSeq(CBioseq& seq) const
{
    return x_GetScopeImpl().SelectSeq(*this, seq);
}


CBioseq_EditHandle
CSeq_entry_EditHandle::CopySeq(const CBioseq_Handle& seq) const
{
    return x_GetScopeImpl().CopySeq(*this, seq);
}


CBioseq_EditHandle
CSeq_entry_EditHandle::TakeSeq(const CBioseq_EditHandle& seq) const
{
    return x_GetScopeImpl().TakeSeq(*this, seq);
}


CBioseq_set_EditHandle
CSeq_entry_EditHandle::ConvertSeqToSet(TClass set_class) const
{
    if ( !IsSeq() ) {
        NCBI_THROW(CObjMgrException, eModifyDataError,
                   "CSeq_entry_EditHandle::ConvertSeqToSet: "
                   "Seq-entry is not in 'seq' state");
    }
    CRef<CBioseq_Info> seq(&x_GetInfo().SetSeq());
    SetSeq().Remove();
    _ASSERT(Which() == CSeq_entry::e_not_set);
    CBioseq_set_EditHandle seqset = SelectSet(set_class);
    x_GetScopeImpl().x_SelectSeq(seqset.AddNewEntry(-1), seq);
    return seqset;
}


void CSeq_entry_EditHandle::CollapseSet(void) const
{
    CSeq_entry_EditHandle entry = GetSingleSubEntry();
    if ( entry.Which() == CSeq_entry::e_not_set ) {
        NCBI_THROW(CObjMgrException, eModifyDataError,
                   "CSeq_entry_EditHandle::CollapseSet: "
                   "sub entry should be non-empty");
    }
    entry.AddDescr(*this);
    entry.TakeAllAnnots(*this);
    if ( entry.IsSet() ) {
        CRef<CBioseq_set_Info> info(&entry.x_GetInfo().SetSet());
        SelectNone();
        x_GetScopeImpl().x_SelectSet(*this, info);
    }
    else {
        CRef<CBioseq_Info> info(&entry.x_GetInfo().SetSeq());
        SelectNone();
        x_GetScopeImpl().x_SelectSeq(*this, info);
    }
}

CBioseq_EditHandle
CSeq_entry_EditHandle::ConvertSetToSeq(void) const
{
    CSeq_entry_EditHandle entry = GetSingleSubEntry();
    if ( !entry.IsSeq() ) {
        NCBI_THROW(CObjMgrException, eModifyDataError,
                   "CSeq_entry_EditHandle::ConvertSetToSeq: "
                   "sub entry should contain Bioseq");
    }
    entry.AddDescr(*this);
    entry.TakeAllAnnots(*this);
    CRef<CBioseq_Info> info(&entry.x_GetInfo().SetSeq());
    SelectNone();
    return x_GetScopeImpl().x_SelectSeq(*this, info);
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.17  2005/04/07 16:30:42  vasilche
* Inlined handles' constructors and destructors.
* Optimized handles' assignment operators.
*
* Revision 1.16  2005/02/28 15:23:05  grichenk
* RemoveDesc() returns CRef<CSeqdesc>
*
* Revision 1.15  2005/01/12 17:16:14  vasilche
* Avoid performance warning on MSVC.
*
* Revision 1.14  2004/12/22 15:56:04  vasilche
* Introduced CTSE_Handle.
*
* Revision 1.13  2004/08/05 18:28:17  vasilche
* Fixed order of CRef<> release in destruction and assignment of handles.
*
* Revision 1.12  2004/08/04 14:53:26  vasilche
* Revamped object manager:
* 1. Changed TSE locking scheme
* 2. TSE cache is maintained by CDataSource.
* 3. CObjectManager::GetInstance() doesn't hold CRef<> on the object manager.
* 4. Fixed processing of split data.
*
* Revision 1.11  2004/05/21 21:42:13  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.10  2004/05/11 19:26:19  grichenk
* Implemented HasParentSeq_entry()
*
* Revision 1.9  2004/04/29 15:44:30  grichenk
* Added GetTopLevelEntry()
*
* Revision 1.8  2004/03/31 17:08:07  vasilche
* Implemented ConvertSeqToSet and ConvertSetToSeq.
*
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
