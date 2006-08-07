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
#include <objmgr/bioseq_set_handle.hpp>

#include <objmgr/scope.hpp>
#include <objmgr/bio_object_id.hpp>

#include <objmgr/impl/scope_impl.hpp>
#include <objmgr/impl/bioseq_set_info.hpp>
#include <objmgr/impl/tse_info.hpp>

#include <objmgr/impl/bioseq_set_edit_commands.hpp>

#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seq/Seqdesc.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


CBioseq_set_Handle::CBioseq_set_Handle(const CBioseq_set_Info& info,
                                       const CTSE_Handle& tse)
    : m_Info(tse.x_GetScopeInfo().GetScopeLock(tse, info))
{
}


CBioseq_set_Handle::CBioseq_set_Handle(const TLock& lock)
    : m_Info(lock)
{
}


void CBioseq_set_Handle::Reset(void)
{
    m_Info.Reset();
}


const CBioseq_set_Info& CBioseq_set_Handle::x_GetInfo(void) const
{
    return m_Info->GetObjectInfo();
}


CConstRef<CBioseq_set> CBioseq_set_Handle::GetCompleteBioseq_set(void) const
{
    return x_GetInfo().GetCompleteBioseq_set();
}


CConstRef<CBioseq_set> CBioseq_set_Handle::GetBioseq_setCore(void) const
{
    return x_GetInfo().GetBioseq_setCore();
}


bool CBioseq_set_Handle::IsEmptySeq_set(void) const
{
    return x_GetInfo().IsEmptySeq_set();
}

const CBioObjectId& CBioseq_set_Handle::GetBioObjectId(void) const
{
    return x_GetInfo().GetBioObjectId();
}

CSeq_entry_Handle CBioseq_set_Handle::GetParentEntry(void) const
{
    CSeq_entry_Handle ret;
    const CBioseq_set_Info& info = x_GetInfo();
    if ( info.HasParent_Info() ) {
        ret = CSeq_entry_Handle(info.GetParentSeq_entry_Info(),
                                GetTSE_Handle());
    }
    return ret;
}


CSeq_entry_Handle CBioseq_set_Handle::GetTopLevelEntry(void) const
{
    return GetTSE_Handle();
}


CBioseq_set_EditHandle CBioseq_set_Handle::GetEditHandle(void) const
{
    return GetScope().GetEditHandle(*this);
}


bool CBioseq_set_Handle::IsSetId(void) const
{
    return x_GetInfo().IsSetId();
}


bool CBioseq_set_Handle::CanGetId(void) const
{
    return *this  &&  x_GetInfo().CanGetId();
}


const CBioseq_set::TId& CBioseq_set_Handle::GetId(void) const
{
    return x_GetInfo().GetId();
}


bool CBioseq_set_Handle::IsSetColl(void) const
{
    return x_GetInfo().IsSetColl();
}


bool CBioseq_set_Handle::CanGetColl(void) const
{
    return *this  &&  x_GetInfo().CanGetColl();
}


const CBioseq_set::TColl& CBioseq_set_Handle::GetColl(void) const
{
    return x_GetInfo().GetColl();
}


bool CBioseq_set_Handle::IsSetLevel(void) const
{
    return x_GetInfo().IsSetLevel();
}


bool CBioseq_set_Handle::CanGetLevel(void) const
{
    return *this  &&  x_GetInfo().CanGetLevel();
}


CBioseq_set::TLevel CBioseq_set_Handle::GetLevel(void) const
{
    return x_GetInfo().GetLevel();
}


bool CBioseq_set_Handle::IsSetClass(void) const
{
    return x_GetInfo().IsSetClass();
}


bool CBioseq_set_Handle::CanGetClass(void) const
{
    return *this  &&  x_GetInfo().CanGetClass();
}


CBioseq_set::TClass CBioseq_set_Handle::GetClass(void) const
{
    return x_GetInfo().GetClass();
}


bool CBioseq_set_Handle::IsSetRelease(void) const
{
    return x_GetInfo().IsSetRelease();
}


bool CBioseq_set_Handle::CanGetRelease(void) const
{
    return *this  &&  x_GetInfo().CanGetRelease();
}


const CBioseq_set::TRelease& CBioseq_set_Handle::GetRelease(void) const
{
    return x_GetInfo().GetRelease();
}


bool CBioseq_set_Handle::IsSetDate(void) const
{
    return x_GetInfo().IsSetDate();
}


bool CBioseq_set_Handle::CanGetDate(void) const
{
    return *this  &&  x_GetInfo().CanGetDate();
}


const CBioseq_set::TDate& CBioseq_set_Handle::GetDate(void) const
{
    return x_GetInfo().GetDate();
}


bool CBioseq_set_Handle::IsSetDescr(void) const
{
    return x_GetInfo().IsSetDescr();
}


bool CBioseq_set_Handle::CanGetDescr(void) const
{
    return *this  &&  x_GetInfo().CanGetDescr();
}


const CBioseq_set::TDescr& CBioseq_set_Handle::GetDescr(void) const
{
    return x_GetInfo().GetDescr();
}


CBioseq_set_Handle::TComplexityTable
CBioseq_set_Handle::sm_ComplexityTable = {
    0, // not-set (0)
    3, // nuc-prot (1)
    2, // segset (2)
    2, // conset (3)
    1, // parts (4)
    1, // gibb (5)
    1, // gi (6)
    5, // genbank (7)
    3, // pir (8)
    4, // pub-set (9)
    4, // equiv (10)
    3, // swissprot (11)
    3, // pdb-entry (12)
    4, // mut-set (13)
    4, // pop-set (14)
    4, // phy-set (15)
    4, // eco-set (16)
    4, // gen-prod-set (17)
    4, // wgs-set (18)
    0, // other (255 - processed separately)
};


const CBioseq_set_Handle::TComplexityTable&
CBioseq_set_Handle::sx_GetComplexityTable(void)
{
    return sm_ComplexityTable;
}


CSeq_entry_Handle
CBioseq_set_Handle::GetComplexityLevel(CBioseq_set::EClass cls) const
{
    const TComplexityTable& ctab = sx_GetComplexityTable();
    if (cls == CBioseq_set::eClass_other) {
        // adjust 255 to the correct value
        cls = CBioseq_set::EClass(sizeof(ctab) - 1);
    }
    CSeq_entry_Handle e = GetParentEntry();
    CSeq_entry_Handle last = e;
    _ASSERT(e && e.IsSet());
    while ( e ) {
        _ASSERT(e.IsSet());
        // Found good level
        if (ctab[e.GetSet().GetClass()] == ctab[cls]) {
            last = e;
            break;
        }
        // Gone too high
        if ( ctab[e.GetSet().GetClass()] > ctab[cls] ) {
            break;
        }
        // Go up one level
        last = e;
        e = e.GetParentEntry();
    }
    return last;
}


CSeq_entry_Handle
CBioseq_set_Handle::GetExactComplexityLevel(CBioseq_set::EClass cls) const
{
    CSeq_entry_Handle ret = GetComplexityLevel(cls);
    if ( ret  &&
         (!ret.GetSet().IsSetClass()  ||
         ret.GetSet().GetClass() != cls) ) {
        ret.Reset();
    }
    return ret;
}

int CBioseq_set_Handle::GetSeq_entry_Index(const CSeq_entry_Handle& entry) const
{
    return x_GetInfo().GetEntryIndex(entry.x_GetInfo());
}

/////////////////////////////////////////////////////////////////////////////
// CBioseq_set_EditHandle

CBioseq_set_EditHandle::CBioseq_set_EditHandle(const CBioseq_set_Handle& h)
    : CBioseq_set_Handle(h)
{
    if ( !h.GetTSE_Handle().CanBeEdited() ) {
        NCBI_THROW(CObjMgrException, eInvalidHandle,
                   "object is not in editing mode");
    }
}


CBioseq_set_EditHandle::CBioseq_set_EditHandle(CBioseq_set_Info& info,
                                               const CTSE_Handle& tse)
    : CBioseq_set_Handle(info, tse)
{
}


CSeq_entry_EditHandle CBioseq_set_EditHandle::GetParentEntry(void) const
{
    CSeq_entry_EditHandle ret;
    CBioseq_set_Info& info = x_GetInfo();
    if ( info.HasParent_Info() ) {
        ret = CSeq_entry_EditHandle(info.GetParentSeq_entry_Info(),
                                    GetTSE_Handle());
    }
    return ret;
}


CBioseq_set_Info& CBioseq_set_EditHandle::x_GetInfo(void) const
{
    return const_cast<CBioseq_set_Info&>(CBioseq_set_Handle::x_GetInfo());
}


void CBioseq_set_EditHandle::ResetId(void) const
{
    typedef CReset_BioseqSetId_EditCommand TCommand;
    CCommandProcessor processor(x_GetScopeImpl());
    processor.run(new TCommand(*this));
    //x_GetInfo().ResetId();
}


void CBioseq_set_EditHandle::SetId(TId& v) const
{
    typedef CSet_BioseqSetId_EditCommand TCommand;
    CCommandProcessor processor(x_GetScopeImpl());
    processor.run(new TCommand(*this,v));
    //    x_GetInfo().SetId(v);
}


void CBioseq_set_EditHandle::ResetColl(void) const
{
    typedef CReset_BioseqSetColl_EditCommand TCommand;
    CCommandProcessor processor(x_GetScopeImpl());
    processor.run(new TCommand(*this));
    //x_GetInfo().ResetColl();
}


void CBioseq_set_EditHandle::SetColl(TColl& v) const
{
    typedef CSet_BioseqSetColl_EditCommand TCommand;
    CCommandProcessor processor(x_GetScopeImpl());
    processor.run(new TCommand(*this, v));
    //x_GetInfo().SetColl(v);
}


void CBioseq_set_EditHandle::ResetLevel(void) const
{
    typedef CReset_BioseqSetLevel_EditCommand TCommand;
    CCommandProcessor processor(x_GetScopeImpl());
    processor.run(new TCommand(*this));
    //    x_GetInfo().ResetLevel();
}


void CBioseq_set_EditHandle::SetLevel(TLevel v) const
{
    typedef CSet_BioseqSetLevel_EditCommand TCommand;
    CCommandProcessor processor(x_GetScopeImpl());
    processor.run(new TCommand(*this, v));
    //x_GetInfo().SetLevel(v);
}


void CBioseq_set_EditHandle::ResetClass(void) const
{
    typedef CReset_BioseqSetClass_EditCommand TCommand;
    CCommandProcessor processor(x_GetScopeImpl());
    processor.run(new TCommand(*this));
    //x_GetInfo().ResetClass();
}


void CBioseq_set_EditHandle::SetClass(TClass v) const
{
    typedef CSet_BioseqSetClass_EditCommand TCommand;
    CCommandProcessor processor(x_GetScopeImpl());
    processor.run(new TCommand(*this, v));
    //x_GetInfo().SetClass(v);
}


void CBioseq_set_EditHandle::ResetRelease(void) const
{
    typedef CReset_BioseqSetRelease_EditCommand TCommand;
    CCommandProcessor processor(x_GetScopeImpl());
    processor.run(new TCommand(*this));
    //    x_GetInfo().ResetRelease();
}


void CBioseq_set_EditHandle::SetRelease(TRelease& v) const
{
    typedef CSet_BioseqSetRelease_EditCommand TCommand;
    CCommandProcessor processor(x_GetScopeImpl());
    processor.run(new TCommand(*this, v));
    //    x_GetInfo().SetRelease(v);
}


void CBioseq_set_EditHandle::ResetDate(void) const
{
    typedef CReset_BioseqSetDate_EditCommand TCommand;
    CCommandProcessor processor(x_GetScopeImpl());
    processor.run(new TCommand(*this));
    //x_GetInfo().ResetDate();
}


void CBioseq_set_EditHandle::SetDate(TDate& v) const
{
    typedef CSet_BioseqSetDate_EditCommand TCommand;
    CCommandProcessor processor(x_GetScopeImpl());
    processor.run(new TCommand(*this,v));
    //    x_GetInfo().SetDate(v);
}


void CBioseq_set_EditHandle::ResetDescr(void) const
{
    typedef CResetValue_EditCommand<CBioseq_set_EditHandle,TDescr> TCommand;
    CCommandProcessor processor(x_GetScopeImpl());
    processor.run(new TCommand(*this));
    //    x_GetInfo().ResetDescr();
}


void CBioseq_set_EditHandle::SetDescr(TDescr& v) const
{
    typedef CSetValue_EditCommand<CBioseq_set_EditHandle,TDescr> TCommand;
    CCommandProcessor processor(x_GetScopeImpl());
    processor.run(new TCommand(*this,v));
    //    x_GetInfo().SetDescr(v);
}


CBioseq_set_EditHandle::TDescr& CBioseq_set_EditHandle::SetDescr(void) const
{
    if (x_GetScopeImpl().IsTransactionActive() 
        || GetTSE_Handle().x_GetTSE_Info().GetEditSaver() )  {
        NCBI_THROW(CObjMgrException, eTransaction,
                       "TDescr& CBioseq_set_EditHandle::SetDescr(): "
                       "method can not be called if a transaction is requered");
    }
    return x_GetInfo().SetDescr();
}


bool CBioseq_set_EditHandle::AddSeqdesc(CSeqdesc& d) const
{
    typedef CDesc_EditCommand<CBioseq_set_EditHandle,true> TCommand;
    CCommandProcessor processor(x_GetScopeImpl());
    return processor.run(new TCommand(*this, d));
    //    return x_GetInfo().AddSeqdesc(d);
}


CRef<CSeqdesc> CBioseq_set_EditHandle::RemoveSeqdesc(const CSeqdesc& d) const
{
    typedef CDesc_EditCommand<CBioseq_set_EditHandle,false> TCommand;
    CCommandProcessor processor(x_GetScopeImpl());
    return processor.run(new TCommand(*this, d));
    //return x_GetInfo().RemoveSeqdesc(d);
}


void CBioseq_set_EditHandle::AddSeq_descr(TDescr& v) const
{
    typedef CAddDescr_EditCommand<CBioseq_set_EditHandle> TCommand;
    CCommandProcessor processor(x_GetScopeImpl());
    processor.run(new TCommand(*this, v));
    //    x_GetInfo().AddSeq_descr(v);
}


CSeq_entry_EditHandle CBioseq_set_EditHandle::AddNewEntry(int index) const
{
    return AttachEntry(*new CSeq_entry, index);
}


CBioseq_EditHandle
CBioseq_set_EditHandle::AttachBioseq(CBioseq& seq, int index) const
{
    CRef<IScopeTransaction_Impl> tr(x_GetScopeImpl().CreateTransaction());
    CBioseq_EditHandle ret = AddNewEntry(index).SelectSeq(seq);
    tr->Commit();
    return ret;
}


CBioseq_EditHandle
CBioseq_set_EditHandle::CopyBioseq(const CBioseq_Handle& seq,
                                   int index) const
{
    CRef<IScopeTransaction_Impl> tr(x_GetScopeImpl().CreateTransaction());
    CBioseq_EditHandle ret = AddNewEntry(index).CopySeq(seq);
    tr->Commit();
    return ret;
}


CBioseq_EditHandle
CBioseq_set_EditHandle::TakeBioseq(const CBioseq_EditHandle& seq,
                                   int index) const
{
    CRef<IScopeTransaction_Impl> tr(x_GetScopeImpl().CreateTransaction());
    CBioseq_EditHandle ret = AddNewEntry(index).TakeSeq(seq);
    tr->Commit();
    return ret;
}


CSeq_entry_EditHandle
CBioseq_set_EditHandle::AttachEntry(CSeq_entry& entry, int index) const
{
    return AttachEntry(Ref(new CSeq_entry_Info(entry)), index);
    //    return x_GetScopeImpl().AttachEntry(*this, entry, index);
}

CSeq_entry_EditHandle
CBioseq_set_EditHandle::AttachEntry(CRef<CSeq_entry_Info> entry, int index) const
{
    typedef CAttachEntry_EditCommand<CRef<CSeq_entry_Info> > TCommand;
    CCommandProcessor processor(x_GetScopeImpl());
    return processor.run(new TCommand(*this, entry, index, x_GetScopeImpl()));
}


CSeq_entry_EditHandle
CBioseq_set_EditHandle::CopyEntry(const CSeq_entry_Handle& entry,
                                  int index) const
{
    return AttachEntry(Ref(new CSeq_entry_Info(entry.x_GetInfo(), 0)), 
                       index);
    //return x_GetScopeImpl().CopyEntry(*this, entry, index);
}


CSeq_entry_EditHandle
CBioseq_set_EditHandle::TakeEntry(const CSeq_entry_EditHandle& entry,
                                  int index) const
{
    CRef<IScopeTransaction_Impl> tr(x_GetScopeImpl().CreateTransaction());
    entry.Remove();
    CSeq_entry_EditHandle handle = AttachEntry(entry, index);
    tr->Commit();
    return handle;
    //return x_GetScopeImpl().TakeEntry(*this, entry, index);
}


CSeq_entry_EditHandle
CBioseq_set_EditHandle::AttachEntry(const CSeq_entry_EditHandle& entry,
                                    int index) const
{
    typedef CAttachEntry_EditCommand<CSeq_entry_EditHandle> TCommand;
    CCommandProcessor processor(x_GetScopeImpl());
    return processor.run(new TCommand(*this, entry, index, x_GetScopeImpl()));
    //    return x_GetScopeImpl().AttachEntry(*this, entry, index);
}


CSeq_annot_EditHandle
CBioseq_set_EditHandle::AttachAnnot(CSeq_annot& annot) const
{
    return GetParentEntry().AttachAnnot(annot);
}


CSeq_annot_EditHandle
CBioseq_set_EditHandle::CopyAnnot(const CSeq_annot_Handle& annot) const
{
    return GetParentEntry().CopyAnnot(annot);
}


CSeq_annot_EditHandle
CBioseq_set_EditHandle::TakeAnnot(const CSeq_annot_EditHandle& annot) const
{
    return GetParentEntry().TakeAnnot(annot);
}


void CBioseq_set_EditHandle::Remove(CBioseq_set_EditHandle::ERemoveMode mode) const
{
    if (mode == eKeepSeq_entry) 
        x_Detach();
    else {
        CRef<IScopeTransaction_Impl> tr(x_GetScopeImpl().CreateTransaction());
        CSeq_entry_EditHandle parent = GetParentEntry();
        x_Detach();
        parent.Remove();
        tr->Commit();
    }
}
void CBioseq_set_EditHandle::x_Detach(void) const
{
    typedef CRemoveBioseq_set_EditCommand TCommand;
    CCommandProcessor processor(x_GetScopeImpl());
    processor.run(new TCommand(*this, x_GetScopeImpl()));
    //    x_GetScopeImpl().RemoveBioseq_set(*this);
}

//////////////////////////////////////////////////////////////////////////
void CBioseq_set_EditHandle::x_RealResetDescr(void) const
{
    x_GetInfo().ResetDescr();
}


void CBioseq_set_EditHandle::x_RealSetDescr(TDescr& v) const
{
    x_GetInfo().SetDescr(v);
}

bool CBioseq_set_EditHandle::x_RealAddSeqdesc(CSeqdesc& d) const
{
    return x_GetInfo().AddSeqdesc(d);
}


CRef<CSeqdesc> CBioseq_set_EditHandle::x_RealRemoveSeqdesc(const CSeqdesc& d) const
{
    return x_GetInfo().RemoveSeqdesc(d);
}


void CBioseq_set_EditHandle::x_RealAddSeq_descr(TDescr& v) const
{
    x_GetInfo().AddSeq_descr(v);
}

void CBioseq_set_EditHandle::x_RealResetId(void) const
{
    x_GetInfo().ResetId();
}


void CBioseq_set_EditHandle::x_RealSetId(TId& v) const
{
    x_GetInfo().SetId(v);
}


void CBioseq_set_EditHandle::x_RealResetColl(void) const
{
    x_GetInfo().ResetColl();
}


void CBioseq_set_EditHandle::x_RealSetColl(TColl& v) const
{
    x_GetInfo().SetColl(v);
}


void CBioseq_set_EditHandle::x_RealResetLevel(void) const
{
    x_GetInfo().ResetLevel();
}


void CBioseq_set_EditHandle::x_RealSetLevel(TLevel v) const
{
    x_GetInfo().SetLevel(v);
}


void CBioseq_set_EditHandle::x_RealResetClass(void) const
{
    x_GetInfo().ResetClass();
}


void CBioseq_set_EditHandle::x_RealSetClass(TClass v) const
{
    x_GetInfo().SetClass(v);
}


void CBioseq_set_EditHandle::x_RealResetRelease(void) const
{
    x_GetInfo().ResetRelease();
}


void CBioseq_set_EditHandle::x_RealSetRelease(TRelease& v) const
{
    x_GetInfo().SetRelease(v);
}


void CBioseq_set_EditHandle::x_RealResetDate(void) const
{
    x_GetInfo().ResetDate();
}


void CBioseq_set_EditHandle::x_RealSetDate(TDate& v) const
{
    x_GetInfo().SetDate(v);
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.25  2006/08/07 15:25:02  vasilche
* CBioseq_set_EditHandle(CBioseq_set_Handle) made public and explicit.
* Avoid unnecessary GetEditHandle() calls.
*
* Revision 1.24  2006/05/01 16:56:45  didenko
* Attach SeqEntry edit command revamp
*
* Revision 1.23  2006/01/25 18:59:04  didenko
* Redisgned bio objects edit facility
*
* Revision 1.22  2005/11/15 22:00:57  ucko
* Don't return expressions from functions declared void.
*
* Revision 1.21  2005/11/15 19:22:07  didenko
* Added transactions and edit commands support
*
* Revision 1.20  2005/09/20 15:42:16  vasilche
* AttachAnnot takes non-const object.
*
* Revision 1.19  2005/07/14 17:04:14  vasilche
* Fixed detaching from data loader.
* Implemented 'Removed' handles.
* Use 'Removed' handles when transferring object from one place to another.
* Fixed MT locking when removing/unlocking handles, clearing scope's history.
*
* Revision 1.18  2005/06/30 19:38:15  vasilche
* Renamed to match CBioseq_Handle, and implemented methods modifying descr.
*
* Revision 1.17  2005/06/22 14:27:31  vasilche
* Implemented copying of shared Seq-entries at edit request.
* Added invalidation of handles to removed objects.
*
* Revision 1.16  2005/06/20 18:37:55  grichenk
* Optimized loading of whole split bioseqs
*
* Revision 1.15  2005/04/07 16:30:42  vasilche
* Inlined handles' constructors and destructors.
* Optimized handles' assignment operators.
*
* Revision 1.14  2005/01/12 17:16:14  vasilche
* Avoid performance warning on MSVC.
*
* Revision 1.13  2004/12/22 15:56:04  vasilche
* Introduced CTSE_Handle.
*
* Revision 1.12  2004/08/05 18:28:17  vasilche
* Fixed order of CRef<> release in destruction and assignment of handles.
*
* Revision 1.11  2004/08/04 14:53:26  vasilche
* Revamped object manager:
* 1. Changed TSE locking scheme
* 2. TSE cache is maintained by CDataSource.
* 3. CObjectManager::GetInstance() doesn't hold CRef<> on the object manager.
* 4. Fixed processing of split data.
*
* Revision 1.10  2004/06/09 16:42:26  grichenk
* Added GetComplexityLevel() and GetExactComplexityLevel() to CBioseq_set_Handle
*
* Revision 1.9  2004/05/21 21:42:12  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.8  2004/05/06 17:32:37  grichenk
* Added CanGetXXXX() methods
*
* Revision 1.7  2004/04/29 15:44:30  grichenk
* Added GetTopLevelEntry()
*
* Revision 1.6  2004/03/31 19:54:08  vasilche
* Fixed removal of bioseqs and bioseq-sets.
*
* Revision 1.5  2004/03/31 17:08:07  vasilche
* Implemented ConvertSeqToSet and ConvertSetToSeq.
*
* Revision 1.4  2004/03/29 20:13:06  vasilche
* Implemented whole set of methods to modify Seq-entry object tree.
* Added CBioseq_Handle::GetExactComplexityLevel().
*
* Revision 1.3  2004/03/24 18:30:29  vasilche
* Fixed edit API.
* Every *_Info object has its own shallow copy of original object.
*
* Revision 1.2  2004/03/16 21:01:32  vasilche
* Added methods to move Bioseq withing Seq-entry
*
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
