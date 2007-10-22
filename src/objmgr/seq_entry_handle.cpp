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
#include <objmgr/bio_object_id.hpp>

#include <objmgr/impl/seq_entry_info.hpp>
#include <objmgr/impl/scope_impl.hpp>
#include <objmgr/impl/tse_info.hpp>
#include <objmgr/impl/bioseq_set_info.hpp>
#include <objmgr/impl/bioseq_info.hpp>
#include <objmgr/impl/seq_annot_info.hpp>
#include <objmgr/impl/seq_entry_edit_commands.hpp>

#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seq/Seqdesc.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


CSeq_entry_Handle::CSeq_entry_Handle(const CTSE_Handle& tse)
    : m_Info(tse.x_GetScopeInfo().GetScopeLock(tse, tse.x_GetTSE_Info()))
{
}


CSeq_entry_Handle::CSeq_entry_Handle(const CSeq_entry_Info& info,
                                     const CTSE_Handle& tse)
    : m_Info(tse.x_GetScopeInfo().GetScopeLock(tse, info))
{
}


CSeq_entry_Handle::CSeq_entry_Handle(const TLock& lock)
    : m_Info(lock)
{
}


void CSeq_entry_Handle::Reset(void)
{
    m_Info.Reset();
}

const CBioObjectId& CSeq_entry_Handle::GetBioObjectId(void) const
{
    return x_GetInfo().GetBioObjectId();
}

const CSeq_entry_Info& CSeq_entry_Handle::x_GetInfo(void) const
{
    return m_Info->GetObjectInfo();
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
    return *this  &&  x_GetInfo().HasParent_Info();
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


CSeq_entry_EditHandle::CSeq_entry_EditHandle(const CSeq_entry_Handle& h)
    : CSeq_entry_Handle(h)
{
    if ( !h.GetTSE_Handle().CanBeEdited() ) {
        NCBI_THROW(CObjMgrException, eInvalidHandle,
                   "object is not in editing mode");
    }
}


CSeq_entry_EditHandle::CSeq_entry_EditHandle(CSeq_entry_Info& info,
                                             const CTSE_Handle& tse)
    : CSeq_entry_Handle(info, tse)
{
}


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
    typedef CSetValue_EditCommand<CSeq_entry_EditHandle,TDescr> TCommand;
    CCommandProcessor processor(x_GetScopeImpl());
    processor.run(new TCommand(*this, v));
}

void CSeq_entry_EditHandle::AddDescr(TDescr& v) const
{
    typedef CAddDescr_EditCommand<CSeq_entry_EditHandle> TCommand;
    CCommandProcessor processor(x_GetScopeImpl());
    processor.run(new TCommand(*this, v));
}


void CSeq_entry_EditHandle::ResetDescr(void) const
{
    typedef CResetValue_EditCommand<CSeq_entry_EditHandle,TDescr> TCommand;
    CCommandProcessor processor(x_GetScopeImpl());
    processor.run(new TCommand(*this));
}


bool CSeq_entry_EditHandle::AddSeqdesc(CSeqdesc& v) const
{
    typedef CDesc_EditCommand<CSeq_entry_EditHandle,true> TCommand;
    CCommandProcessor processor(x_GetScopeImpl());
    return processor.run(new TCommand(*this, v));
}


CRef<CSeqdesc> CSeq_entry_EditHandle::RemoveSeqdesc(const CSeqdesc& v) const
{
    typedef CDesc_EditCommand<CSeq_entry_EditHandle,false> TCommand;
    CCommandProcessor processor(x_GetScopeImpl());
    return processor.run(new TCommand(*this, v));
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


CSeq_entry_EditHandle
CSeq_entry_EditHandle::AttachEntry(const CSeq_entry_EditHandle& entry,
                                   int index) const
{
    return SetSet().AttachEntry(entry, index);
}


CSeq_annot_EditHandle
CSeq_entry_EditHandle::AttachAnnot(CSeq_annot& annot) const
{
    return AttachAnnot(Ref(new CSeq_annot_Info(annot)));
    //    return x_GetScopeImpl().AttachAnnot(*this, annot);
}

CSeq_annot_EditHandle
CSeq_entry_EditHandle::AttachAnnot(CRef<CSeq_annot_Info> annot) const
{
    typedef CAttachAnnot_EditCommand<CRef<CSeq_annot_Info> > TCommand;
    CCommandProcessor processor(x_GetScopeImpl());
    return processor.run(new TCommand(*this, annot, x_GetScopeImpl()));
}


CSeq_annot_EditHandle
CSeq_entry_EditHandle::CopyAnnot(const CSeq_annot_Handle& annot) const
{
    return AttachAnnot(Ref(new CSeq_annot_Info(annot.x_GetInfo(), 0)));
    //    return x_GetScopeImpl().CopyAnnot(*this, annot);
}


CSeq_annot_EditHandle
CSeq_entry_EditHandle::TakeAnnot(const CSeq_annot_EditHandle& annot) const
{
    CRef<IScopeTransaction_Impl> tr(x_GetScopeImpl().CreateTransaction());
    annot.Remove();
    CSeq_annot_EditHandle handle = AttachAnnot(annot);
    tr->Commit();
    return handle;
    //    return x_GetScopeImpl().TakeAnnot(*this, annot);
}


CSeq_annot_EditHandle
CSeq_entry_EditHandle::AttachAnnot(const CSeq_annot_EditHandle& annot) const
{
    typedef CAttachAnnot_EditCommand<CSeq_annot_EditHandle> TCommand;
    CCommandProcessor processor(x_GetScopeImpl());
    return processor.run(new TCommand(*this, annot, x_GetScopeImpl()));
    //return x_GetScopeImpl().AttachAnnot(*this, annot);
}


void
CSeq_entry_EditHandle::TakeAllAnnots(const CSeq_entry_EditHandle& entry) const
{
    vector<CSeq_annot_EditHandle> annots;
    // we have to copy all handles as moving annots directly could break iter
    for ( CSeq_annot_CI it(entry, CSeq_annot_CI::eSearch_entry); it; ++it ) {
        annots.push_back(it->GetEditHandle());
    }
    ITERATE ( vector<CSeq_annot_EditHandle>, it, annots ) {
        TakeAnnot(*it);
    }
}

void CSeq_entry_EditHandle::TakeAllDescr(const CSeq_entry_EditHandle& entry) const
{
    if (entry.IsSetDescr()) {
        CRef<IScopeTransaction_Impl> tr(x_GetScopeImpl().CreateTransaction());
        TDescr& descr = const_cast<TDescr&>(entry.GetDescr());
        AddDescr(descr);
        entry.ResetDescr();
        tr->Commit();
    }
        //    x_GetInfo().AddDescr(entry.x_GetInfo());
        //    entry.ResetDescr();
    
}

void CSeq_entry_EditHandle::Remove(void) const
{
    if( !GetParentEntry() ) {
        typedef CRemoveTSE_EditCommand TCommand;
        CCommandProcessor processor(x_GetScopeImpl());
        processor.run(new TCommand(*this, x_GetScopeImpl()));
    } else {
        typedef CSeq_entry_Remove_EditCommand TCommand;
        CCommandProcessor processor(x_GetScopeImpl());
        processor.run(new TCommand(*this, x_GetScopeImpl()));
        //    x_GetScopeImpl().RemoveEntry(*this);
    }
}


CBioseq_set_EditHandle CSeq_entry_EditHandle::SelectSet(TClass set_class) const
{
    CRef<IScopeTransaction_Impl> tr(x_GetScopeImpl().CreateTransaction());
    CBioseq_set_EditHandle seqset = SelectSet(*new CBioseq_set);
    if ( set_class != CBioseq_set::eClass_not_set ) {
        seqset.SetClass(set_class);
    }
    tr->Commit();
    return seqset;
}


void CSeq_entry_EditHandle::SelectNone(void) const
{
    typedef CSeq_entry_SelectNone_EditCommand TCommand;
    CCommandProcessor processor(x_GetScopeImpl());
    processor.run(new TCommand(*this, x_GetScopeImpl()));
    //x_GetScopeImpl().SelectNone(*this);
}


CBioseq_set_EditHandle
CSeq_entry_EditHandle::SelectSet(CBioseq_set& seqset) const
{
    return SelectSet(Ref(new CBioseq_set_Info(seqset)));
    //return x_GetScopeImpl().SelectSet(*this, seqset);
}

CBioseq_set_EditHandle
CSeq_entry_EditHandle::SelectSet(CRef<CBioseq_set_Info> seqset) const
{
    typedef CSeq_entry_Select_EditCommand
                 <CBioseq_set_EditHandle,CRef<CBioseq_set_Info> > TCommand;
    CCommandProcessor processor(x_GetScopeImpl());
    return processor.run(new TCommand(*this, seqset, x_GetScopeImpl()));
}


CBioseq_set_EditHandle
CSeq_entry_EditHandle::CopySet(const CBioseq_set_Handle& seqset) const
{
    return SelectSet(Ref(new CBioseq_set_Info(seqset.x_GetInfo(),0)));
    //return x_GetScopeImpl().CopySet(*this, seqset);
}


CBioseq_set_EditHandle
CSeq_entry_EditHandle::TakeSet(const CBioseq_set_EditHandle& seqset) const
{
    CRef<IScopeTransaction_Impl> tr(x_GetScopeImpl().CreateTransaction());
    seqset.Remove(CBioseq_set_EditHandle::eKeepSeq_entry);
    CBioseq_set_EditHandle handle = SelectSet(seqset);
    tr->Commit();
    return handle;
    //return x_GetScopeImpl().TakeSet(*this, seqset);
}


CBioseq_set_EditHandle
CSeq_entry_EditHandle::SelectSet(const CBioseq_set_EditHandle& seqset) const
{
    typedef CSeq_entry_Select_EditCommand
                   <CBioseq_set_EditHandle,CBioseq_set_EditHandle> TCommand;
    CCommandProcessor processor(x_GetScopeImpl());
    return processor.run(new TCommand(*this, seqset, x_GetScopeImpl()));
    //    return x_GetScopeImpl().SelectSet(*this, seqset);
}


CBioseq_EditHandle CSeq_entry_EditHandle::SelectSeq(CBioseq& seq) const
{
    return SelectSeq(Ref(new CBioseq_Info(seq)));
    //    return x_GetScopeImpl().SelectSeq(*this, seq);
}

CBioseq_EditHandle 
CSeq_entry_EditHandle::SelectSeq(CRef<CBioseq_Info> seq) const
{
    typedef CSeq_entry_Select_EditCommand
                 <CBioseq_EditHandle,CRef<CBioseq_Info> > TCommand;
    CCommandProcessor processor(x_GetScopeImpl());
    return processor.run(new TCommand(*this, seq, x_GetScopeImpl()));
}

CBioseq_EditHandle
CSeq_entry_EditHandle::CopySeq(const CBioseq_Handle& seq) const
{
    return SelectSeq(Ref(new CBioseq_Info(seq.x_GetInfo(),0)));
    //    return x_GetScopeImpl().CopySeq(*this, seq);
}


CBioseq_EditHandle
CSeq_entry_EditHandle::TakeSeq(const CBioseq_EditHandle& seq) const
{
    CRef<IScopeTransaction_Impl> tr(x_GetScopeImpl().CreateTransaction());
    seq.Remove(CBioseq_EditHandle::eKeepSeq_entry);
    CBioseq_EditHandle handle = SelectSeq(seq);
    tr->Commit();
    return handle;
    //    return x_GetScopeImpl().TakeSeq(*this, seq);
}


CBioseq_EditHandle
CSeq_entry_EditHandle::SelectSeq(const CBioseq_EditHandle& seq) const
{
    typedef CSeq_entry_Select_EditCommand
                   <CBioseq_EditHandle,CBioseq_EditHandle> TCommand;
    CCommandProcessor processor(x_GetScopeImpl());
    return processor.run(new TCommand(*this, seq, x_GetScopeImpl()));
    //    return x_GetScopeImpl().SelectSeq(*this, seq);
}


CBioseq_set_EditHandle
CSeq_entry_EditHandle::ConvertSeqToSet(TClass set_class) const
{
    if ( !IsSeq() ) {
        NCBI_THROW(CObjMgrException, eModifyDataError,
                   "CSeq_entry_EditHandle::ConvertSeqToSet: "
                   "Seq-entry is not in 'seq' state");
    }
    CBioseq_EditHandle seq = SetSeq();
    CRef<IScopeTransaction_Impl> tr(x_GetScopeImpl().CreateTransaction());
    SelectNone();
    _ASSERT(Which() == CSeq_entry::e_not_set);
    _ASSERT(seq.IsRemoved());
    _ASSERT(!seq);
    CBioseq_set_EditHandle seqset = SelectSet(set_class);
    seqset.AddNewEntry(-1).SelectSeq(seq);
    _ASSERT(seq);
    tr->Commit();
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
    CRef<IScopeTransaction_Impl> tr(x_GetScopeImpl().CreateTransaction());
    entry.TakeAllDescr(*this);
    entry.TakeAllAnnots(*this);
    if ( entry.IsSet() ) {
        CBioseq_set_EditHandle seqset = entry.SetSet();
        entry.SelectNone();
        SelectNone();
        _ASSERT(seqset.IsRemoved());
        _ASSERT(!seqset);
        SelectSet(seqset);
        _ASSERT(seqset);
    }
    else {
        CBioseq_EditHandle seq = entry.SetSeq();
        entry.SelectNone();
        SelectNone();
        _ASSERT(seq.IsRemoved());
        _ASSERT(!seq);
        SelectSeq(seq);
        _ASSERT(seq);
    }
    tr->Commit();
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
    CRef<IScopeTransaction_Impl> tr(x_GetScopeImpl().CreateTransaction());
    entry.TakeAllDescr(*this);
    entry.TakeAllAnnots(*this);
    CBioseq_EditHandle seq = entry.SetSeq();
    entry.SelectNone();
    SelectNone();
    _ASSERT(seq.IsRemoved());
    _ASSERT(!seq);
    SelectSeq(seq);
    _ASSERT(seq);
    tr->Commit();
    return seq;
}

////////////////////////////////////////////////////////////////////////
void CSeq_entry_EditHandle::x_RealSetDescr(TDescr& v) const
{
    x_GetInfo().SetDescr(v);
}


void CSeq_entry_EditHandle::x_RealResetDescr(void) const
{
    x_GetInfo().ResetDescr();
}


bool CSeq_entry_EditHandle::x_RealAddSeqdesc(CSeqdesc& v) const
{
    return x_GetInfo().AddSeqdesc(v);
}


CRef<CSeqdesc> CSeq_entry_EditHandle::x_RealRemoveSeqdesc(const CSeqdesc& v) const
{
    return x_GetInfo().RemoveSeqdesc(v);
}


void CSeq_entry_EditHandle::x_RealAddSeq_descr(TDescr& v) const
{
    x_GetInfo().AddSeq_descr(v);
}


void CSeq_entry_EditHandle::UpdateAnnotations(void) const
{
    x_GetInfo();
}


END_SCOPE(objects)
END_NCBI_SCOPE
