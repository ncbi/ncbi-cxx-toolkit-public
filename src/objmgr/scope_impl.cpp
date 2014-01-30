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
* Authors:
*           Andrei Gourianov
*           Aleksey Grichenko
*           Michael Kimelman
*           Denis Vakatov
*           Eugene Vasilchenko
*
* File Description:
*           Scope is top-level object available to a client.
*           Its purpose is to define a scope of visibility and reference
*           resolution and provide access to the bio sequence data
*
*/

#include <ncbi_pch.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/seq_entry_handle.hpp>
#include <objmgr/seq_annot_handle.hpp>
#include <objmgr/bioseq_set_handle.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/objmgr_exception.hpp>
#include <objmgr/prefetch_manager.hpp>

#include <objmgr/impl/data_source.hpp>
#include <objmgr/impl/tse_info.hpp>
#include <objmgr/impl/scope_info.hpp>
#include <objmgr/impl/bioseq_info.hpp>
#include <objmgr/impl/bioseq_set_info.hpp>
#include <objmgr/impl/seq_entry_info.hpp>
#include <objmgr/impl/seq_annot_info.hpp>
#include <objmgr/impl/priority.hpp>
#include <objmgr/impl/synonyms.hpp>
#include <objmgr/impl/handle_range_map.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Delta_seq.hpp>
#include <objects/seq/Seq_literal.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Textseq_id.hpp>
#include <objects/seqset/Seq_entry.hpp>

#include <objmgr/impl/scope_impl.hpp>
#include <objmgr/impl/scope_transaction_impl.hpp>

#include <objmgr/seq_annot_ci.hpp>
#include <objmgr/error_codes.hpp>
#include <math.h>
#include <algorithm>

#define USE_OBJMGR_SHARED_POOL 0

#define NCBI_USE_ERRCODE_X   ObjMgr_Scope

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

#define EXCLUDE_EDITED_BIOSEQ_ANNOT_SET

/////////////////////////////////////////////////////////////////////////////
//
//  CScope_Impl
//
/////////////////////////////////////////////////////////////////////////////


CScope_Impl::CScope_Impl(CObjectManager& objmgr)
    : m_HeapScope(0), m_ObjMgr(0), m_Transaction(NULL)
{
    TConfWriteLockGuard guard(m_ConfLock);
    x_AttachToOM(objmgr);
}


CScope_Impl::~CScope_Impl(void)
{
    TConfWriteLockGuard guard(m_ConfLock);
    x_DetachFromOM();
}


CScope& CScope_Impl::GetScope(void)
{
    _ASSERT(m_HeapScope);
    return *m_HeapScope;
}


void CScope_Impl::x_AttachToOM(CObjectManager& objmgr)
{
    _ASSERT(!m_ObjMgr);
    m_ObjMgr.Reset(&objmgr);
    m_ObjMgr->RegisterScope(*this);
}


void CScope_Impl::x_DetachFromOM(void)
{
    _ASSERT(m_ObjMgr);
    // Drop and release all TSEs
    ResetScope();
    m_ObjMgr->RevokeScope(*this);
    m_ObjMgr.Reset();
}


void CScope_Impl::AddDefaults(TPriority priority)
{
    CObjectManager::TDataSourcesLock ds_set;
    m_ObjMgr->AcquireDefaultDataSources(ds_set);

    TConfWriteLockGuard guard(m_ConfLock);
    NON_CONST_ITERATE( CObjectManager::TDataSourcesLock, it, ds_set ) {
        m_setDataSrc.Insert(*x_GetDSInfo(const_cast<CDataSource&>(**it)),
                            (priority == CScope::kPriority_Default) ?
                            (*it)->GetDefaultPriority() : priority);
    }
    x_ClearCacheOnNewDS();
}


void CScope_Impl::AddDataLoader(const string& loader_name, TPriority priority)
{
    CRef<CDataSource> ds = m_ObjMgr->AcquireDataLoader(loader_name);

    TConfWriteLockGuard guard(m_ConfLock);
    m_setDataSrc.Insert(*x_GetDSInfo(*ds),
                        (priority == CScope::kPriority_Default) ?
                        ds->GetDefaultPriority() : priority);
    x_ClearCacheOnNewDS();
}


void CScope_Impl::AddScope(CScope_Impl& scope, TPriority priority)
{
    TConfReadLockGuard src_guard(scope.m_ConfLock);
    CPriorityTree tree(*this, scope.m_setDataSrc);
    src_guard.Release();
    
    TConfWriteLockGuard guard(m_ConfLock);
    m_setDataSrc.Insert(tree,
                        (priority == CScope::kPriority_Default) ? 9 : priority);
    x_ClearCacheOnNewDS();
}


CSeq_entry_Handle CScope_Impl::AddSeq_entry(CSeq_entry& entry,
                                            TPriority priority,
                                            TExist action)
{
    TConfWriteLockGuard guard(m_ConfLock);
    
    // first check if object already added to the scope
    TSeq_entry_Lock lock = x_GetSeq_entry_Lock(entry, CScope::eMissing_Null);
    if ( lock.first ) {
        if ( action == CScope::eExist_Throw ) {
            NCBI_THROW(CObjMgrException, eAddDataError,
                       "Seq-entry already added to the scope");
        }
        return CSeq_entry_Handle(*lock.first, *lock.second);
    }
    
    CRef<CDataSource_ScopeInfo> ds_info = GetEditDS(priority);
    CTSE_Lock tse_lock = ds_info->GetDataSource().AddStaticTSE(entry);
    x_ClearCacheOnNewData(*tse_lock);

    return CSeq_entry_Handle(*tse_lock, *ds_info->GetTSE_Lock(tse_lock));
}


CSeq_entry_Handle CScope_Impl::AddSharedSeq_entry(const CSeq_entry& entry,
                                                  TPriority priority,
                                                  TExist action)
{
    TConfWriteLockGuard guard(m_ConfLock);
    
    // first check if object already added to the scope
    TSeq_entry_Lock lock = x_GetSeq_entry_Lock(entry, CScope::eMissing_Null);
    if ( lock.first ) {
        if ( action == CScope::eExist_Throw ) {
            NCBI_THROW(CObjMgrException, eAddDataError,
                       "Seq-entry already added to the scope");
        }
        return CSeq_entry_Handle(*lock.first, *lock.second);
    }
    
#if USE_OBJMGR_SHARED_POOL
    CRef<CDataSource> ds = m_ObjMgr->AcquireSharedSeq_entry(entry);
    CRef<CDataSource_ScopeInfo> ds_info = AddDS(ds, priority);
    CTSE_Lock tse_lock = ds->GetSharedTSE();
#else
    CRef<CDataSource_ScopeInfo> ds_info = GetConstDS(priority);
    CTSE_Lock tse_lock = ds_info->GetDataSource().AddStaticTSE(const_cast<CSeq_entry&>(entry));
#endif
    x_ClearCacheOnNewData(*tse_lock);
    _ASSERT(tse_lock->GetTSECore() == &entry);

    return CSeq_entry_Handle(*tse_lock, *ds_info->GetTSE_Lock(tse_lock));
}


CBioseq_Handle CScope_Impl::AddBioseq(CBioseq& bioseq,
                                      TPriority priority,
                                      TExist action)
{
    TConfWriteLockGuard guard(m_ConfLock);
    
    // first check if object already added to the scope
    TBioseq_Lock lock = x_GetBioseq_Lock(bioseq, CScope::eMissing_Null);
    if ( lock ) {
        if ( action == CScope::eExist_Throw ) {
            NCBI_THROW(CObjMgrException, eAddDataError,
                       "Bioseq already added to the scope");
        }
        return CBioseq_Handle(CSeq_id_Handle(), *lock);
    }
    
    CRef<CDataSource_ScopeInfo> ds_info = GetEditDS(priority);
    CRef<CSeq_entry> entry = x_MakeDummyTSE(bioseq);
    CTSE_Lock tse_lock = ds_info->GetDataSource().AddStaticTSE(*entry);
    x_ClearCacheOnNewData(*tse_lock);

    return x_GetBioseqHandle(tse_lock->GetSeq(),
                             *ds_info->GetTSE_Lock(tse_lock));
}


CBioseq_Handle CScope_Impl::AddSharedBioseq(const CBioseq& bioseq,
                                            TPriority priority,
                                            TExist action)
{
    TConfWriteLockGuard guard(m_ConfLock);

    // first check if object already added to the scope
    TBioseq_Lock lock = x_GetBioseq_Lock(bioseq, CScope::eMissing_Null);
    if ( lock ) {
        if ( action == CScope::eExist_Throw ) {
            NCBI_THROW(CObjMgrException, eAddDataError,
                       "Bioseq already added to the scope");
        }
        return CBioseq_Handle(CSeq_id_Handle(), *lock);
    }
    
#if USE_OBJMGR_SHARED_POOL
    CRef<CDataSource> ds = m_ObjMgr->AcquireSharedBioseq(bioseq);
    CRef<CDataSource_ScopeInfo> ds_info = AddDS(ds, priority);
    CTSE_Lock tse_lock = ds->GetSharedTSE();
#else
    CRef<CDataSource_ScopeInfo> ds_info = GetConstDS(priority);
    CRef<CSeq_entry> entry = x_MakeDummyTSE(const_cast<CBioseq&>(bioseq));
    CTSE_Lock tse_lock = ds_info->GetDataSource().AddStaticTSE(*entry);
#endif
    _ASSERT(tse_lock->IsSeq() &&
            tse_lock->GetSeq().GetBioseqCore() == &bioseq);

    return x_GetBioseqHandle(tse_lock->GetSeq(),
                             *ds_info->GetTSE_Lock(tse_lock));
}


CSeq_annot_Handle CScope_Impl::AddSeq_annot(CSeq_annot& annot,
                                            TPriority priority,
                                            TExist action)
{
    TConfWriteLockGuard guard(m_ConfLock);
    
    // first check if object already added to the scope
    TSeq_annot_Lock lock = x_GetSeq_annot_Lock(annot, CScope::eMissing_Null);
    if ( lock.first ) {
        if ( action == CScope::eExist_Throw ) {
            NCBI_THROW(CObjMgrException, eAddDataError,
                       "Seq-annot already added to the scope");
        }
        return CSeq_annot_Handle(*lock.first, *lock.second);
    }
    
    CRef<CDataSource_ScopeInfo> ds_info = GetEditDS(priority);
    CRef<CSeq_entry> entry = x_MakeDummyTSE(annot);
    CTSE_Lock tse_lock = ds_info->GetDataSource().AddStaticTSE(*entry);
    x_ClearCacheOnNewAnnot(*tse_lock);

    return CSeq_annot_Handle(*tse_lock->GetSet().GetAnnot()[0],
                             *ds_info->GetTSE_Lock(tse_lock));
}


CSeq_annot_Handle CScope_Impl::AddSharedSeq_annot(const CSeq_annot& annot,
                                                  TPriority priority,
                                                  TExist action)
{
    TConfWriteLockGuard guard(m_ConfLock);
    
    // first check if object already added to the scope
    TSeq_annot_Lock lock = x_GetSeq_annot_Lock(annot, CScope::eMissing_Null);
    if ( lock.first ) {
        if ( action == CScope::eExist_Throw ) {
            NCBI_THROW(CObjMgrException, eAddDataError,
                       "Seq-annot already added to the scope");
        }
        return CSeq_annot_Handle(*lock.first, *lock.second);
    }
    
#if USE_OBJMGR_SHARED_POOL
    CRef<CDataSource> ds = m_ObjMgr->AcquireSharedSeq_annot(annot);
    CRef<CDataSource_ScopeInfo> ds_info = AddDS(ds, priority);
    CTSE_Lock tse_lock = ds->GetSharedTSE();
#else
    CRef<CDataSource_ScopeInfo> ds_info = GetConstDS(priority);
    CRef<CSeq_entry> entry = x_MakeDummyTSE(const_cast<CSeq_annot&>(annot));
    CTSE_Lock tse_lock = ds_info->GetDataSource().AddStaticTSE(*entry);
#endif
    _ASSERT(tse_lock->IsSet() &&
            tse_lock->GetSet().IsSetAnnot() &&
            tse_lock->GetSet().GetAnnot().size() == 1 &&
            tse_lock->GetSet().GetAnnot()[0]->GetSeq_annotCore() == &annot);

    return CSeq_annot_Handle(*tse_lock->GetSet().GetAnnot()[0],
                             *ds_info->GetTSE_Lock(tse_lock));
}


namespace {
    class CClearCacheOnRemoveGuard
    {
    public:
        CClearCacheOnRemoveGuard(CScope_Impl* scope)
            : m_Scope(scope)
            {
            }
        ~CClearCacheOnRemoveGuard(void)
            {
                if ( m_Scope ) {
                    m_Scope->x_ClearCacheOnRemoveData();
                }
            }

        void Done(void)
            {
                m_Scope = 0;
            }
    private:
        CScope_Impl* m_Scope;

    private:    
        CClearCacheOnRemoveGuard(const CClearCacheOnRemoveGuard&);
        void operator=(const CClearCacheOnRemoveGuard&);
    };
}


void CScope_Impl::RemoveDataLoader(const string& name,
                                   int action)
{
    CRef<CDataSource> ds(m_ObjMgr->AcquireDataLoader(name));
    TConfWriteLockGuard guard(m_ConfLock);
    TDSMap::iterator ds_it = m_DSMap.find(ds);
    if ( ds_it == m_DSMap.end() ) {
        NCBI_THROW(CObjMgrException, eFindFailed,
                   "CScope_Impl::RemoveDataLoader: "
                   "data loader not found in the scope");
    }
    CRef<CDataSource_ScopeInfo> ds_info = ds_it->second;
    {{
        CClearCacheOnRemoveGuard guard2(this);
        ds_info->ResetHistory(action);
        guard2.Done();
    }}
    if ( action != CScope::eRemoveIfLocked ) {
        // we need to process each TSE individually checking if it's unlocked
        CDataSource_ScopeInfo::TTSE_InfoMap tse_map;
        {{
            CDataSource_ScopeInfo::TTSE_InfoMapMutex::TReadLockGuard guard2
                (ds_info->GetTSE_InfoMapMutex());
            tse_map = ds_info->GetTSE_InfoMap();
        }}
        ITERATE( CDataSource_ScopeInfo::TTSE_InfoMap, tse_it, tse_map ) {
            {{
                CClearCacheOnRemoveGuard guard2(this);
                tse_it->second.GetNCObject().RemoveFromHistory(CScope::eThrowIfLocked);
                guard2.Done();
            }}
        }
    }
    _VERIFY(m_setDataSrc.Erase(*ds_info));
    _VERIFY(m_DSMap.erase(ds));
    ds.Reset();
    ds_info->DetachScope();
    x_ClearCacheOnRemoveData();
}


void CScope_Impl::RemoveTopLevelSeqEntry(CTSE_Handle tse)
{
    TConfWriteLockGuard guard(m_ConfLock);
    if ( !tse ) {
        NCBI_THROW(CObjMgrException, eInvalidHandle,
                "CScope_Impl::RemoveTopLevelSeqEntry: "
                "TSE not found in the scope");
    }
    CRef<CTSE_ScopeInfo> tse_info(&tse.x_GetScopeInfo());
    CRef<CDataSource_ScopeInfo> ds_info(&tse_info->GetDSInfo());
    CTSE_Lock tse_lock(tse_info->GetTSE_Lock());
    _ASSERT(tse_lock);
    if ( &ds_info->GetScopeImpl() != this ) {
        NCBI_THROW(CObjMgrException, eInvalidHandle,
                "CScope_Impl::RemoveTopLevelSeqEntry: "
                "TSE doesn't belong to the scope");
    }
    if ( ds_info->GetDataLoader() ) {
        NCBI_THROW(CObjMgrException, eInvalidHandle,
                "CScope_Impl::RemoveTopLevelSeqEntry: "
                "can not remove a loaded TSE");
    }
    x_ClearCacheOnRemoveData(&*tse_lock);
    tse_lock.Reset();
    tse_info->RemoveFromHistory(CScope::eRemoveIfLocked, true);//drop_from_ds
    _ASSERT(!tse_info->IsAttached());
    _ASSERT(!tse);
    if ( !ds_info->CanBeEdited() ) { // shared -> remove whole DS
        CRef<CDataSource> ds(&ds_info->GetDataSource());
        _VERIFY(m_setDataSrc.Erase(*ds_info));
        _VERIFY(m_DSMap.erase(ds));
        ds.Reset();
        ds_info->DetachScope();
    }
    /*
    else { // private -> remove TSE only
        CRef<CTSE_Info> info(&const_cast<CTSE_Info&>(*tse_lock));
        tse_lock.Reset();
        ds_info->GetDataSource().DropStaticTSE(*info);
    }
    */
    x_ClearCacheOnRemoveData();
}


CSeq_entry_EditHandle
CScope_Impl::x_AttachEntry(const CBioseq_set_EditHandle& seqset,
                           CRef<CSeq_entry_Info> entry,
                           int index)
{
    TConfWriteLockGuard guard(m_ConfLock);

    _ASSERT(seqset);
    _ASSERT(entry);

    seqset.x_GetInfo().AddEntry(entry, index, true);    

    x_ClearCacheOnNewData(entry->GetTSE_Info(), *entry);

    return CSeq_entry_EditHandle(*entry, seqset.GetTSE_Handle());
}


void CScope_Impl::x_AttachEntry(const CBioseq_set_EditHandle& seqset,
                                const CSeq_entry_EditHandle& entry,
                                int index)
{
    TConfWriteLockGuard guard(m_ConfLock);

    _ASSERT(seqset);
    _ASSERT(entry.IsRemoved());
    _ASSERT(!entry);

    seqset.GetTSE_Handle().x_GetScopeInfo()
        .AddEntry(seqset.x_GetScopeInfo(), entry.x_GetScopeInfo(), index);
    
    x_ClearCacheOnNewData(seqset.x_GetInfo().GetTSE_Info(), entry.x_GetInfo());
    
    _ASSERT(entry);
}


CBioseq_EditHandle
CScope_Impl::x_SelectSeq(const CSeq_entry_EditHandle& entry,
                         CRef<CBioseq_Info> bioseq)
{
    CBioseq_EditHandle ret;

    TConfWriteLockGuard guard(m_ConfLock);

    _ASSERT(entry);
    _ASSERT(entry.Which() == CSeq_entry::e_not_set);
    _ASSERT(bioseq);

    // duplicate bioseq info
    entry.x_GetInfo().SelectSeq(*bioseq);

    x_ClearCacheOnNewData(bioseq->GetTSE_Info(), entry.x_GetInfo());

    ret.m_Info = entry.x_GetScopeInfo().x_GetTSE_ScopeInfo()
        .GetBioseqLock(null, bioseq);
    x_UpdateHandleSeq_id(ret);
    return ret;
}


CBioseq_set_EditHandle
CScope_Impl::x_SelectSet(const CSeq_entry_EditHandle& entry,
                         CRef<CBioseq_set_Info> seqset)
{
    TConfWriteLockGuard guard(m_ConfLock);

    _ASSERT(entry);
    _ASSERT(entry.Which() == CSeq_entry::e_not_set);
    _ASSERT(seqset);

    // duplicate bioseq info
    entry.x_GetInfo().SelectSet(*seqset);

    x_ClearCacheOnNewData(seqset->GetTSE_Info(), entry.x_GetInfo());

    return CBioseq_set_EditHandle(*seqset, entry.GetTSE_Handle());
}


void CScope_Impl::x_SelectSeq(const CSeq_entry_EditHandle& entry,
                              const CBioseq_EditHandle& bioseq)
{
    TConfWriteLockGuard guard(m_ConfLock);

    _ASSERT(entry);
    _ASSERT(entry.Which() == CSeq_entry::e_not_set);
    _ASSERT(bioseq.IsRemoved());
    _ASSERT(!bioseq);

    entry.GetTSE_Handle().x_GetScopeInfo()
        .SelectSeq(entry.x_GetScopeInfo(), bioseq.x_GetScopeInfo());

    x_ClearCacheOnNewData(entry.x_GetInfo().GetTSE_Info(), entry.x_GetInfo());

    _ASSERT(bioseq);
}


void CScope_Impl::x_SelectSet(const CSeq_entry_EditHandle& entry,
                              const CBioseq_set_EditHandle& seqset)
{
    TConfWriteLockGuard guard(m_ConfLock);

    _ASSERT(entry);
    _ASSERT(entry.Which() == CSeq_entry::e_not_set);
    _ASSERT(seqset.IsRemoved());
    _ASSERT(!seqset);

    entry.GetTSE_Handle().x_GetScopeInfo()
        .SelectSet(entry.x_GetScopeInfo(), seqset.x_GetScopeInfo());

    x_ClearCacheOnNewData(entry.x_GetInfo().GetTSE_Info(), entry.x_GetInfo());

    _ASSERT(seqset);
}


CSeq_annot_EditHandle
CScope_Impl::x_AttachAnnot(const CSeq_entry_EditHandle& entry,
                           CRef<CSeq_annot_Info> annot)
{
    TConfWriteLockGuard guard(m_ConfLock);

    _ASSERT(entry);
    _ASSERT(annot);

    entry.x_GetInfo().AddAnnot(annot);

    x_ClearCacheOnNewAnnot(annot->GetTSE_Info());

    return CSeq_annot_EditHandle(*annot, entry.GetTSE_Handle());
}


void CScope_Impl::x_AttachAnnot(const CSeq_entry_EditHandle& entry,
                                const CSeq_annot_EditHandle& annot)
{
    TConfWriteLockGuard guard(m_ConfLock);

    _ASSERT(entry);
    _ASSERT(annot.IsRemoved());
    _ASSERT(!annot);

    entry.GetTSE_Handle().x_GetScopeInfo()
        .AddAnnot(entry.x_GetScopeInfo(), annot.x_GetScopeInfo());

    x_ClearCacheOnNewAnnot(annot.x_GetInfo().GetTSE_Info());

    _ASSERT(annot);
}


#define CHECK_HANDLE(func, handle)                                     \
    if ( !handle ) {                                                   \
        NCBI_THROW(CObjMgrException, eInvalidHandle,                   \
                   "CScope_Impl::" #func ": null " #handle " handle"); \
    }

#define CHECK_REMOVED_HANDLE(func, handle)                             \
    if ( !handle.IsRemoved() ) {                                       \
        NCBI_THROW(CObjMgrException, eInvalidHandle,                   \
                   "CScope_Impl::" #func ": "                          \
                   #handle " handle is not removed");                  \
    }


CSeq_entry_EditHandle
CScope_Impl::AttachEntry(const CBioseq_set_EditHandle& seqset,
                         CSeq_entry& entry,
                         int index)
{
    return AttachEntry(seqset, Ref(new CSeq_entry_Info(entry)), index);
}

CSeq_entry_EditHandle
CScope_Impl::AttachEntry(const CBioseq_set_EditHandle& seqset,
                         CRef<CSeq_entry_Info> entry,
                         int index)
{
    CHECK_HANDLE(AttachEntry, seqset);
    _ASSERT(seqset);
    return x_AttachEntry(seqset,entry, index);
}

/*
CSeq_entry_EditHandle
CScope_Impl::CopyEntry(const CBioseq_set_EditHandle& seqset,
                       const CSeq_entry_Handle& entry,
                       int index)
{
    CHECK_HANDLE(CopyEntry, seqset);
    CHECK_HANDLE(CopyEntry, entry);
    _ASSERT(seqset);
    _ASSERT(entry);
    return x_AttachEntry(seqset,
                         Ref(new CSeq_entry_Info(entry.x_GetInfo(), 0)),
                         index);
}


CSeq_entry_EditHandle
CScope_Impl::TakeEntry(const CBioseq_set_EditHandle& seqset,
                       const CSeq_entry_EditHandle& entry,
                       int index)
{
    CHECK_HANDLE(TakeEntry, seqset);
    CHECK_HANDLE(TakeEntry, entry);
    _ASSERT(seqset);
    _ASSERT(entry);
    entry.Remove();
    return AttachEntry(seqset, entry, index);
}
*/

CSeq_entry_EditHandle
CScope_Impl::AttachEntry(const CBioseq_set_EditHandle& seqset,
                         const CSeq_entry_EditHandle& entry,
                         int index)
{
    CHECK_HANDLE(AttachEntry, seqset);
    CHECK_REMOVED_HANDLE(AttachEntry, entry);
    _ASSERT(seqset);
    _ASSERT(!entry);
    _ASSERT(entry.IsRemoved());
    x_AttachEntry(seqset, entry, index);
    _ASSERT(!entry.IsRemoved());
    _ASSERT(entry);
    return entry;
}


CBioseq_EditHandle CScope_Impl::SelectSeq(const CSeq_entry_EditHandle& entry,
                                          CBioseq& seq)
{
    return SelectSeq(entry, Ref(new CBioseq_Info(seq)));
    /*CHECK_HANDLE(SelectSeq, entry);
    _ASSERT(entry);
    return x_SelectSeq(entry, Ref(new CBioseq_Info(seq)));
    */
}
CBioseq_EditHandle CScope_Impl::SelectSeq(const CSeq_entry_EditHandle& entry,
                                          CRef<CBioseq_Info> seq)
{
    CHECK_HANDLE(SelectSeq, entry);
    _ASSERT(entry);
    return x_SelectSeq(entry, seq);
}

/*
CBioseq_EditHandle CScope_Impl::CopySeq(const CSeq_entry_EditHandle& entry,
                                        const CBioseq_Handle& seq)
{
    CHECK_HANDLE(CopySeq, entry);
    CHECK_HANDLE(CopySeq, seq);
    _ASSERT(entry);
    _ASSERT(seq);
    return x_SelectSeq(entry,
                       Ref(new CBioseq_Info(seq.x_GetInfo(), 0)));
}


CBioseq_EditHandle CScope_Impl::TakeSeq(const CSeq_entry_EditHandle& entry,
                                        const CBioseq_EditHandle& seq)
{
    CHECK_HANDLE(TakeSeq, entry);
    CHECK_HANDLE(TakeSeq, seq);
    _ASSERT(entry);
    _ASSERT(seq);
    seq.Remove();
    return SelectSeq(entry, seq);
}
*/

CBioseq_EditHandle CScope_Impl::SelectSeq(const CSeq_entry_EditHandle& entry,
                                          const CBioseq_EditHandle& seq)
{
    CHECK_HANDLE(SelectSeq, entry);
    CHECK_REMOVED_HANDLE(SelectSeq, seq);
    _ASSERT(entry);
    _ASSERT(seq.IsRemoved());
    _ASSERT(!seq);
    x_SelectSeq(entry, seq);
    _ASSERT(seq);
    return seq;
}


CBioseq_set_EditHandle
CScope_Impl::SelectSet(const CSeq_entry_EditHandle& entry,
                       CBioseq_set& seqset)
{
    return SelectSet(entry, Ref(new CBioseq_set_Info(seqset)));
    /*    CHECK_HANDLE(SelectSet, entry);
    _ASSERT(entry);
    return x_SelectSet(entry, Ref(new CBioseq_set_Info(seqset)));*/
}

CBioseq_set_EditHandle
CScope_Impl::SelectSet(const CSeq_entry_EditHandle& entry,
                       CRef<CBioseq_set_Info> seqset)
{
    CHECK_HANDLE(SelectSet, entry);
    _ASSERT(entry);
    return x_SelectSet(entry, seqset);
}

/*
CBioseq_set_EditHandle
CScope_Impl::CopySet(const CSeq_entry_EditHandle& entry,
                     const CBioseq_set_Handle& seqset)
{
    CHECK_HANDLE(CopySet, entry);
    CHECK_HANDLE(CopySet, seqset);
    _ASSERT(entry);
    _ASSERT(seqset);
    return x_SelectSet(entry,
                       Ref(new CBioseq_set_Info(seqset.x_GetInfo(), 0)));
}


CBioseq_set_EditHandle
CScope_Impl::TakeSet(const CSeq_entry_EditHandle& entry,
                     const CBioseq_set_EditHandle& seqset)
{
    CHECK_HANDLE(TakeSet, entry);
    CHECK_HANDLE(TakeSet, seqset);
    _ASSERT(entry);
    _ASSERT(seqset);
    seqset.Remove();
    return SelectSet(entry, seqset);
}
*/

CBioseq_set_EditHandle
CScope_Impl::SelectSet(const CSeq_entry_EditHandle& entry,
                       const CBioseq_set_EditHandle& seqset)
{
    CHECK_HANDLE(SelectSet, entry);
    CHECK_REMOVED_HANDLE(SelectSet, seqset);
    _ASSERT(entry);
    _ASSERT(seqset.IsRemoved());
    _ASSERT(!seqset);
    x_SelectSet(entry, seqset);
    _ASSERT(seqset);
    return seqset;
}


CSeq_annot_EditHandle
CScope_Impl::AttachAnnot(const CSeq_entry_EditHandle& entry,
                         CSeq_annot& annot)
{
    return AttachAnnot(entry, Ref(new CSeq_annot_Info(annot)));
    /*CHECK_HANDLE(AttachAnnot, entry);
    _ASSERT(entry);
    return x_AttachAnnot(entry, Ref(new CSeq_annot_Info(annot)));
    */
}

CSeq_annot_EditHandle
CScope_Impl::AttachAnnot(const CSeq_entry_EditHandle& entry,
                         CRef<CSeq_annot_Info> annot)
{
    CHECK_HANDLE(AttachAnnot, entry);
    _ASSERT(entry);
    return x_AttachAnnot(entry, annot);
}

/*
CSeq_annot_EditHandle
CScope_Impl::CopyAnnot(const CSeq_entry_EditHandle& entry,
                       const CSeq_annot_Handle& annot)
{
    CHECK_HANDLE(CopyAnnot, entry);
    CHECK_HANDLE(CopyAnnot, annot);
    _ASSERT(entry);
    _ASSERT(annot);
    return x_AttachAnnot(entry,
                         Ref(new CSeq_annot_Info(annot.x_GetInfo(), 0)));
}


CSeq_annot_EditHandle
CScope_Impl::TakeAnnot(const CSeq_entry_EditHandle& entry,
                       const CSeq_annot_EditHandle& annot)
{
    CHECK_HANDLE(TakeAnnot, entry);
    CHECK_HANDLE(TakeAnnot, annot);
    _ASSERT(entry);
    _ASSERT(annot);
    annot.Remove();
    return AttachAnnot(entry, annot);
}
*/

CSeq_annot_EditHandle
CScope_Impl::AttachAnnot(const CSeq_entry_EditHandle& entry,
                         const CSeq_annot_EditHandle& annot)
{
    CHECK_HANDLE(AttachAnnot, entry);
    CHECK_REMOVED_HANDLE(AttachAnnot, annot);
    _ASSERT(entry);
    _ASSERT(annot.IsRemoved());
    _ASSERT(!annot);
    x_AttachAnnot(entry, annot);
    _ASSERT(annot);
    return annot;
}


void CScope_Impl::x_ReportNewDataConflict(const CSeq_id_Handle* conflict_id)
{
    if ( conflict_id ) {
        LOG_POST_X(12, Info <<
                   "CScope_Impl: -- "
                   "adding new data to a scope with non-empty history "
                   "make data inconsistent on "<<conflict_id->AsString());
    }
    else {
        LOG_POST_X(13, Info <<
                   "CScope_Impl: -- "
                   "adding new data to a scope with non-empty history "
                   "may cause the data to become inconsistent");
    }
}


void CScope_Impl::x_ClearCacheOnNewData(const CTSE_Info& new_tse)
{
    // Clear unresolved bioseq handles
    // Clear annot cache
    TIds seq_ids, annot_ids;
    new_tse.GetSeqAndAnnotIds(seq_ids, annot_ids);
    x_ClearCacheOnNewData(seq_ids, annot_ids);
    //x_ClearCacheOnNewAnnot(new_tse);
}


void CScope_Impl::x_ClearCacheOnNewData(const CTSE_Info& /*new_tse*/,
                                        const CSeq_id_Handle& new_id)
{
    TIds seq_ids(1, new_id), annot_ids;
    x_ClearCacheOnNewData(seq_ids, annot_ids);
}


void CScope_Impl::x_ClearCacheOnNewData(const CTSE_Info& /*new_tse*/,
                                        const CSeq_entry_Info& new_entry)
{
    TIds seq_ids, annot_ids;
    new_entry.GetSeqAndAnnotIds(seq_ids, annot_ids);
    x_ClearCacheOnNewData(seq_ids, annot_ids);
}


void CScope_Impl::x_ClearCacheOnNewData(const TIds& seq_ids,
                                        const TIds& annot_ids)
{
    //if ( 1 ) return;
    const CSeq_id_Handle* conflict_id = 0;
    if ( !m_Seq_idMap.empty() && !seq_ids.empty() ) {
        // scan for conflicts and mark new seq-ids for new scan if unresolved
        size_t add_count = seq_ids.size();
        size_t old_count = m_Seq_idMap.size();
        size_t scan_time = add_count + old_count;
        double lookup_time = min(add_count, old_count)*
            (2. * log(max(add_count, old_count)+2.));
        if ( scan_time < lookup_time ) {
            // scan both
            TIds::const_iterator it1 = seq_ids.begin();
            TSeq_idMap::iterator it2 = m_Seq_idMap.begin();
            while ( it1 != seq_ids.end() && it2 != m_Seq_idMap.end() ) {
                if ( *it1 < it2->first ) {
                    ++it1;
                }
                else if ( it2->first < *it1 ) {
                    ++it2;
                }
                else {
                    if ( it2->second.m_Bioseq_Info ) {
                        conflict_id = &*it1;
                        CBioseq_ScopeInfo& binfo = *it2->second.m_Bioseq_Info;
                        if ( !binfo.HasBioseq() ) {
                            // try to resolve again
                            binfo.m_SynCache.Reset(); // break circular link
                            it2->second.m_Bioseq_Info.Reset();
                        }
                    }
                    ++it1;
                    ++it2;
                }
            }
        }
        else if ( add_count < old_count ) {
            // lookup in old
            ITERATE ( TIds, it1, seq_ids ) {
                TSeq_idMap::iterator it2 = m_Seq_idMap.find(*it1);
                if ( it2 != m_Seq_idMap.end() &&
                     it2->second.m_Bioseq_Info ) {
                    conflict_id = &*it1;
                    CBioseq_ScopeInfo& binfo = *it2->second.m_Bioseq_Info;
                    if ( !binfo.HasBioseq() ) {
                        // try to resolve again
                        binfo.m_SynCache.Reset(); // break circular link
                        it2->second.m_Bioseq_Info.Reset();
                    }
                }
            }
        }
        else {
            // lookup in add
            NON_CONST_ITERATE ( TSeq_idMap, it2, m_Seq_idMap ) {
                if ( it2->second.m_Bioseq_Info &&
                     binary_search(seq_ids.begin(), seq_ids.end(), it2->first) ) {
                    conflict_id = &it2->first;
                    CBioseq_ScopeInfo& binfo = *it2->second.m_Bioseq_Info;
                    if ( !binfo.HasBioseq() ) {
                        // try to resolve again
                        binfo.m_SynCache.Reset(); // break circular link
                        it2->second.m_Bioseq_Info.Reset();
                    }
                }
            }
        }
    }
    if ( conflict_id ) {
        x_ReportNewDataConflict(conflict_id);
    }
    if ( !annot_ids.empty() ) {
        // recollect annot TSEs
        x_ClearAnnotCache();
    }
}


void CScope_Impl::x_ClearCacheOnEdit(const CTSE_ScopeInfo& replaced_tse)
{
    // Clear unresolved bioseq handles
    // Clear annot cache
    for ( TSeq_idMap::iterator it = m_Seq_idMap.begin();
          it != m_Seq_idMap.end(); ) {
        if ( it->second.m_Bioseq_Info ) {
            CBioseq_ScopeInfo& binfo = *it->second.m_Bioseq_Info;
            if ( binfo.HasBioseq() ) {
                if ( &binfo.x_GetTSE_ScopeInfo() == &replaced_tse ) {
                    binfo.m_SynCache.Reset(); // break circular link
                    m_Seq_idMap.erase(it++);
                    continue;
                }
                binfo.m_BioseqAnnotRef_Info.Reset();
            }
            else {
                binfo.m_SynCache.Reset(); // break circular link
                it->second.m_Bioseq_Info.Reset(); // try to resolve again
            }
        }
        it->second.m_AllAnnotRef_Info.Reset();
        ++it;
    }
}


void CScope_Impl::x_ClearAnnotCache(void)
{
    // Clear annot cache
    NON_CONST_ITERATE ( TSeq_idMap, it, m_Seq_idMap ) {
        if ( it->second.m_Bioseq_Info ) {
            CBioseq_ScopeInfo& binfo = *it->second.m_Bioseq_Info;
            binfo.m_BioseqAnnotRef_Info.Reset();
        }
        it->second.m_AllAnnotRef_Info.Reset();
    }
}


void CScope_Impl::x_ClearCacheOnNewAnnot(const CTSE_Info& new_tse)
{
    //if ( 1 ) return;
    x_ClearAnnotCache();
}


void CScope_Impl::x_ClearCacheOnNewDS(void)
{
    if ( 1 ) return;
    // Clear unresolved bioseq handles
    // Clear annot cache
    if ( !m_Seq_idMap.empty() ) {
        x_ReportNewDataConflict();
    }
    for ( TSeq_idMap::iterator it = m_Seq_idMap.begin();
          it != m_Seq_idMap.end(); ) {
        if ( it->second.m_Bioseq_Info ) {
            CBioseq_ScopeInfo& binfo = *it->second.m_Bioseq_Info;
            if ( binfo.HasBioseq() ) {
                binfo.m_BioseqAnnotRef_Info.Reset();
            }
            else {
                binfo.m_SynCache.Reset(); // break circular link
                it->second.m_Bioseq_Info.Reset(); // try to resolve again
            }
        }
        it->second.m_AllAnnotRef_Info.Reset();
        ++it;
    }
}


void CScope_Impl::x_ClearCacheOnRemoveData(const CTSE_Info* old_tse)
{
    // Clear removed bioseq handles
    for ( TSeq_idMap::iterator it = m_Seq_idMap.begin();
          it != m_Seq_idMap.end(); ) {
        it->second.m_AllAnnotRef_Info.Reset();
        if ( it->second.m_Bioseq_Info ) {
            CBioseq_ScopeInfo& binfo = *it->second.m_Bioseq_Info;
            binfo.m_BioseqAnnotRef_Info.Reset();
            if ( binfo.IsDetached() ) {
                binfo.m_SynCache.Reset();
                m_Seq_idMap.erase(it++);
                continue;
            }
        }
        ++it;
    }
}


void CScope_Impl::x_ClearCacheOnRemoveAnnot(const CTSE_Info& old_tse)
{
    // Clear annot cache
    x_ClearAnnotCache();
}


CSeq_entry_Handle CScope_Impl::GetSeq_entryHandle(CDataLoader* loader,
                                                  const CBlobIdKey& blob_id,
                                                  TMissing action)
{
    TConfReadLockGuard guard(m_ConfLock);
    CRef<CDataSource_ScopeInfo> ds = x_GetDSInfo(*loader->GetDataSource());
    if ( !ds ) {
        NCBI_THROW(CObjMgrException, eFindFailed,
                   "CScope::GetSeq_entryHandle(loader, blob_id): "
                   "data loader is not in the scope");
    }
    TSeq_entry_Lock lock = ds->GetSeq_entry_Lock(blob_id);
    if ( lock.first ) {
        return CSeq_entry_Handle(*lock.first, *lock.second);
    }
    if ( action == CScope::eMissing_Null ) {
        return CSeq_entry_Handle();
    }
    NCBI_THROW(CObjMgrException, eFindFailed,
               "CScope::GetSeq_entryHandle(loader, blob_id): "
               "entry is not found");
}


CBioseq_set_Handle CScope_Impl::GetBioseq_setHandle(const CBioseq_set& seqset,
                                                    TMissing action)
{
    CBioseq_set_Handle ret;
    TConfReadLockGuard guard(m_ConfLock);
    TBioseq_set_Lock lock = x_GetBioseq_set_Lock(seqset, action);
    if ( lock.first ) {
        ret = CBioseq_set_Handle(*lock.first, *lock.second);
    }
    return ret;
}


CSeq_entry_Handle CScope_Impl::GetSeq_entryHandle(const CSeq_entry& entry,
                                                  TMissing action)
{
    CSeq_entry_Handle ret;
    TConfReadLockGuard guard(m_ConfLock);
    TSeq_entry_Lock lock = x_GetSeq_entry_Lock(entry, action);
    if ( lock.first ) {
        ret = CSeq_entry_Handle(*lock.first, *lock.second);
    }
    return ret;
}


CSeq_annot_Handle CScope_Impl::GetSeq_annotHandle(const CSeq_annot& annot,
                                                  TMissing action)
{
    CSeq_annot_Handle ret;
    TConfReadLockGuard guard(m_ConfLock);
    TSeq_annot_Lock lock = x_GetSeq_annot_Lock(annot, action);
    if ( lock.first ) {
        ret = CSeq_annot_Handle(*lock.first, *lock.second);
    }
    return ret;
}


CBioseq_Handle CScope_Impl::GetBioseqHandle(const CBioseq& seq,
                                            TMissing action)
{
    CBioseq_Handle ret;
    TConfReadLockGuard guard(m_ConfLock);
    ret.m_Info = x_GetBioseq_Lock(seq, action);
    if ( ret.m_Info ) {
        x_UpdateHandleSeq_id(ret);
    }
    return ret;
}


CSeq_entry_Handle CScope_Impl::GetSeq_entryHandle(const CTSE_Handle& tse)
{
    return CSeq_entry_Handle(tse.x_GetTSE_Info(), tse);
}


CSeq_feat_Handle CScope_Impl::GetSeq_featHandle(const CSeq_feat& feat,
                                                TMissing action)
{
    CSeq_id_Handle loc_id;
    TSeqPos loc_pos = kInvalidSeqPos;
    for ( CSeq_loc_CI it = feat.GetLocation(); it; ++it ) {
        if ( !it.GetRange().Empty() ) {
            loc_id = it.GetSeq_id_Handle();
            loc_pos = it.GetRange().GetFrom();
            break;
        }
    }
    if ( !loc_id || loc_pos == kInvalidSeqPos ) {
        if ( action == CScope::eMissing_Null ) {
            return CSeq_feat_Handle();
        }
        NCBI_THROW(CObjMgrException, eFindFailed,
                   "CScope_Impl::GetSeq_featHandle: "
                   "Seq-feat location is empty");
    }
    
    TConfWriteLockGuard guard(m_ConfLock);
    for (CPriority_I it(m_setDataSrc); it; ++it) {
        CDataSource_ScopeInfo::TSeq_feat_Lock lock =
            it->FindSeq_feat_Lock(loc_id, loc_pos, feat);
        if ( lock.first.first ) {
            return CSeq_feat_Handle(CSeq_annot_Handle(*lock.first.first,
                                                      *lock.first.second),
                                    lock.second);
        }
    }
    if ( action == CScope::eMissing_Null ) {
        return CSeq_feat_Handle();
    }
    NCBI_THROW(CObjMgrException, eFindFailed,
               "CScope_Impl::GetSeq_featHandle: Seq-feat not found");
}


CRef<CDataSource_ScopeInfo> CScope_Impl::AddDS(CRef<CDataSource> ds,
                                               TPriority priority)
{
    TConfWriteLockGuard guard(m_ConfLock);
    CRef<CDataSource_ScopeInfo> ds_info = x_GetDSInfo(*ds);
    m_setDataSrc.Insert(*ds_info,
                        (priority == CScope::kPriority_Default) ?
                        ds->GetDefaultPriority() : priority);
    CTSE_Lock tse_lock = ds->GetSharedTSE();
    if ( tse_lock ) {
        x_ClearCacheOnNewData(*tse_lock);
    }
    else {
        x_ClearCacheOnNewDS();
    }
    return ds_info;
}


CRef<CDataSource_ScopeInfo>
CScope_Impl::GetEditDS(TPriority priority)
{
    TConfWriteLockGuard guard(m_ConfLock);
    typedef CPriorityTree::TPriorityMap TMap;
    TMap& pmap = m_setDataSrc.GetTree();
    TMap::iterator iter = pmap.lower_bound(priority);
    while ( iter != pmap.end() && iter->first == priority ) {
        if ( iter->second.IsLeaf() && iter->second.GetLeaf().CanBeEdited() ) {
            return Ref(&iter->second.GetLeaf());
        }
        ++iter;
    }
    CRef<CDataSource> ds(new CDataSource);
    _ASSERT(ds->CanBeEdited());
    CRef<CDataSource_ScopeInfo> ds_info = x_GetDSInfo(*ds);
    _ASSERT(ds_info->CanBeEdited());
    pmap.insert(iter, TMap::value_type(priority, CPriorityNode(*ds_info)));
    return ds_info;
}


CRef<CDataSource_ScopeInfo>
CScope_Impl::GetConstDS(TPriority priority)
{
    TConfWriteLockGuard guard(m_ConfLock);
    typedef CPriorityTree::TPriorityMap TMap;
    TMap& pmap = m_setDataSrc.GetTree();
    TMap::iterator iter = pmap.lower_bound(priority);
    while ( iter != pmap.end() && iter->first == priority ) {
        if ( iter->second.IsLeaf() && iter->second.GetLeaf().IsConst() ) {
            return Ref(&iter->second.GetLeaf());
        }
        ++iter;
    }
    CRef<CDataSource> ds(new CDataSource);
    _ASSERT(ds->CanBeEdited());
    CRef<CDataSource_ScopeInfo> ds_info = x_GetDSInfo(*ds);
    _ASSERT(ds_info->CanBeEdited());
    pmap.insert(iter, TMap::value_type(priority, CPriorityNode(*ds_info)));
    ds_info->SetConst();
    _ASSERT(ds_info->IsConst());
    _ASSERT(!ds_info->CanBeEdited());
    return ds_info;
}


CRef<CDataSource_ScopeInfo>
CScope_Impl::AddDSBefore(CRef<CDataSource> ds,
                         CRef<CDataSource_ScopeInfo> ds2,
                         const CTSE_ScopeInfo* replaced_tse)
{
    TConfWriteLockGuard guard(m_ConfLock);
    CRef<CDataSource_ScopeInfo> ds_info = x_GetDSInfo(*ds);
    for (CPriority_I it(m_setDataSrc); it; ++it) {
        if ( &*it == ds2 ) {
            it.InsertBefore(*ds_info);
            x_ClearCacheOnEdit(*replaced_tse);
            return ds_info;
        }
    }
    NCBI_THROW(CObjMgrException, eOtherError,
               "CScope_Impl::AddDSBefore: ds2 is not attached");
}


CRef<CDataSource_ScopeInfo> CScope_Impl::x_GetDSInfo(CDataSource& ds)
{
    CRef<CDataSource_ScopeInfo>& slot = m_DSMap[Ref(&ds)];
    if ( !slot ) {
        slot = new CDataSource_ScopeInfo(*this, ds);
    }
    return slot;
}


CScope_Impl::TTSE_Lock CScope_Impl::x_GetTSE_Lock(const CSeq_entry& tse,
                                                  int action)
{
    for (CPriority_I it(m_setDataSrc); it; ++it) {
        TTSE_Lock lock = it->FindTSE_Lock(tse);
        if ( lock ) {
            return lock;
        }
    }
    if ( action == CScope::eMissing_Null ) {
        return TTSE_Lock();
    }
    NCBI_THROW(CObjMgrException, eFindFailed,
               "CScope_Impl::x_GetTSE_Lock: entry is not attached");
}


CScope_Impl::TSeq_entry_Lock
CScope_Impl::x_GetSeq_entry_Lock(const CSeq_entry& entry, int action)
{
    for (CPriority_I it(m_setDataSrc); it; ++it) {
        TSeq_entry_Lock lock = it->FindSeq_entry_Lock(entry);
        if ( lock.first ) {
            return lock;
        }
    }
    if ( action == CScope::eMissing_Null ) {
        return TSeq_entry_Lock();
    }
    NCBI_THROW(CObjMgrException, eFindFailed,
               "CScope_Impl::x_GetSeq_entry_Lock: entry is not attached");
}


CScope_Impl::TSeq_annot_Lock
CScope_Impl::x_GetSeq_annot_Lock(const CSeq_annot& annot, int action)
{
    for (CPriority_I it(m_setDataSrc); it; ++it) {
        TSeq_annot_Lock lock = it->FindSeq_annot_Lock(annot);
        if ( lock.first ) {
            return lock;
        }
    }
    if ( action == CScope::eMissing_Null ) {
        return TSeq_annot_Lock();
    }
    NCBI_THROW(CObjMgrException, eFindFailed,
               "CScope_Impl::x_GetSeq_annot_Lock: annot is not attached");
}


CScope_Impl::TBioseq_set_Lock
CScope_Impl::x_GetBioseq_set_Lock(const CBioseq_set& seqset, int action)
{
    for (CPriority_I it(m_setDataSrc); it; ++it) {
        TBioseq_set_Lock lock = it->FindBioseq_set_Lock(seqset);
        if ( lock.first ) {
            return lock;
        }
    }
    if ( action == CScope::eMissing_Null ) {
        return TBioseq_set_Lock();
    }
    NCBI_THROW(CObjMgrException, eFindFailed,
               "CScope_Impl::x_GetBioseq_set_Lock: "
               "bioseq set is not attached");
}


CScope_Impl::TBioseq_Lock
CScope_Impl::x_GetBioseq_Lock(const CBioseq& bioseq, int action)
{
    for (CPriority_I it(m_setDataSrc); it; ++it) {
        TBioseq_Lock lock = it->FindBioseq_Lock(bioseq);
        if ( lock ) {
            return lock;
        }
    }
    if ( action == CScope::eMissing_Null ) {
        return TBioseq_Lock();
    }
    NCBI_THROW(CObjMgrException, eFindFailed,
               "CScope_Impl::x_GetBioseq_Lock: bioseq is not attached");
}


CScope_Impl::TTSE_Lock
CScope_Impl::x_GetTSE_Lock(const CTSE_Lock& lock, CDataSource_ScopeInfo& ds)
{
    _ASSERT(&ds.GetScopeImpl() == this);
    return ds.GetTSE_Lock(lock);
}


CScope_Impl::TTSE_Lock
CScope_Impl::x_GetTSE_Lock(const CTSE_ScopeInfo& tse)
{
    _ASSERT(&tse.GetScopeImpl() == this);
    return CTSE_ScopeUserLock(const_cast<CTSE_ScopeInfo*>(&tse));
}


void CScope_Impl::RemoveEntry(const CSeq_entry_EditHandle& entry)
{
    entry.GetCompleteSeq_entry();
    if ( !entry.GetParentEntry() ) {
        CTSE_Handle tse = entry.GetTSE_Handle();
        // TODO entry.Reset();
        RemoveTopLevelSeqEntry(tse);
        return;
    }
    TConfWriteLockGuard guard(m_ConfLock);

    x_ClearCacheOnRemoveData(&entry.x_GetInfo().GetTSE_Info());

    entry.GetTSE_Handle().x_GetScopeInfo().RemoveEntry(entry.x_GetScopeInfo());

    x_ClearCacheOnRemoveData();
}


CRef<CSeq_entry> CScope_Impl::x_MakeDummyTSE(CBioseq& seq) const
{
    CRef<CSeq_entry> entry(new CSeq_entry);
    entry->SetSeq(seq);
    return entry;
}


CRef<CSeq_entry> CScope_Impl::x_MakeDummyTSE(CBioseq_set& seqset) const
{
    CRef<CSeq_entry> entry(new CSeq_entry);
    entry->SetSet(seqset);
    return entry;
}


CRef<CSeq_entry> CScope_Impl::x_MakeDummyTSE(CSeq_annot& annot) const
{
    CRef<CSeq_entry> entry(new CSeq_entry);
    entry->SetSet().SetSeq_set(); // it's not optional
    entry->SetSet().SetAnnot().push_back(Ref(&annot));
    return entry;
}


bool CScope_Impl::x_IsDummyTSE(const CTSE_Info& tse,
                               const CBioseq_Info& seq) const
{
    if ( &tse != &seq.GetParentSeq_entry_Info() ) {
        return false;
    }
    return true;
}


bool CScope_Impl::x_IsDummyTSE(const CTSE_Info& tse,
                               const CBioseq_set_Info& seqset) const
{
    if ( &tse != &seqset.GetParentSeq_entry_Info() ) {
        return false;
    }
    return true;
}


bool CScope_Impl::x_IsDummyTSE(const CTSE_Info& tse,
                               const CSeq_annot_Info& annot) const
{
    if ( &tse != &annot.GetParentSeq_entry_Info() ) {
        return false;
    }
    if ( !tse.IsSet() ) {
        return false;
    }
    const CBioseq_set_Info& seqset = tse.GetSet();
    if ( seqset.IsSetId() ) {
        return false;
    }
    if ( seqset.IsSetColl() ) {
        return false;
    }
    if ( seqset.IsSetLevel() ) {
        return false;
    }
    if ( seqset.IsSetClass() ) {
        return false;
    }
    if ( seqset.IsSetRelease() ) {
        return false;
    }
    if ( seqset.IsSetDate() ) {
        return false;
    }
    if ( seqset.IsSetDescr() ) {
        return false;
    }
    if ( !seqset.IsSetSeq_set() || !seqset.IsEmptySeq_set() ) {
        return false;
    }
    if ( !seqset.IsSetAnnot() ||
         seqset.GetAnnot().size() != 1 ||
         seqset.GetAnnot()[0] != &annot ) {
        return false;
    }
    return true;
}


void CScope_Impl::RemoveAnnot(const CSeq_annot_EditHandle& annot)
{
    TConfWriteLockGuard guard(m_ConfLock);

    x_ClearCacheOnRemoveAnnot(annot.x_GetInfo().GetTSE_Info());

    annot.GetTSE_Handle().x_GetScopeInfo().RemoveAnnot(annot.x_GetScopeInfo());

    x_ClearAnnotCache();
}


void CScope_Impl::SelectNone(const CSeq_entry_EditHandle& entry)
{
    _ASSERT(entry);
    entry.GetCompleteSeq_entry();

    TConfWriteLockGuard guard(m_ConfLock);

    x_ClearCacheOnRemoveData(&entry.x_GetInfo().GetTSE_Info());

    entry.GetTSE_Handle().x_GetScopeInfo().ResetEntry(entry.x_GetScopeInfo());

    x_ClearCacheOnRemoveData();
}


void CScope_Impl::RemoveBioseq(const CBioseq_EditHandle& seq)
{
    SelectNone(seq.GetParentEntry());
}


void CScope_Impl::RemoveBioseq_set(const CBioseq_set_EditHandle& seqset)
{
    SelectNone(seqset.GetParentEntry());
}


void CScope_Impl::RemoveTopLevelBioseq(const CBioseq_Handle& seq)
{
    CTSE_Handle tse = seq.GetTSE_Handle();
    if ( !x_IsDummyTSE(tse.x_GetTSE_Info(), seq.x_GetInfo()) ) {
        NCBI_THROW(CObjMgrException, eInvalidHandle,
                   "Not a top level Bioseq");
    }
    RemoveTopLevelSeqEntry(tse);
}


void CScope_Impl::RemoveTopLevelBioseq_set(const CBioseq_set_Handle& seqset)
{
    CTSE_Handle tse = seqset.GetTSE_Handle();
    if ( !x_IsDummyTSE(tse.x_GetTSE_Info(), seqset.x_GetInfo()) ) {
        NCBI_THROW(CObjMgrException, eInvalidHandle,
                   "Not a top level Bioseq-set");
    }
    RemoveTopLevelSeqEntry(tse);
}


void CScope_Impl::RemoveTopLevelAnnot(const CSeq_annot_Handle& annot)
{
    CTSE_Handle tse = annot.GetTSE_Handle();
    if ( !x_IsDummyTSE(tse.x_GetTSE_Info(), annot.x_GetInfo()) ) {
        NCBI_THROW(CObjMgrException, eInvalidHandle,
                   "Not a top level Seq-annot");
    }
    RemoveTopLevelSeqEntry(tse);
}


CScope_Impl::TSeq_idMapValue&
CScope_Impl::x_GetSeq_id_Info(const CSeq_id_Handle& id)
{
    TSeq_idMapLock::TWriteLockGuard guard(m_Seq_idMapLock);
    TSeq_idMap::iterator it = m_Seq_idMap.lower_bound(id);
    if ( it == m_Seq_idMap.end() || it->first != id ) {
        it = m_Seq_idMap.insert(it, TSeq_idMapValue(id, SSeq_id_ScopeInfo()));
    }
    return *it;
/*
    TSeq_idMap::iterator it;
    {{
        TSeq_idMapLock::TReadLockGuard guard(m_Seq_idMapLock);
        it = m_Seq_idMap.lower_bound(id);
        if ( it != m_Seq_idMap.end() && it->first == id ) {
            return *it;
        }
    }}
    {{
        TSeq_idMapLock::TWriteLockGuard guard(m_Seq_idMapLock);
        it = m_Seq_idMap.insert(it, TSeq_idMapValue(id, SSeq_id_ScopeInfo()));
        return *it;
    }}
*/
/*
    {{
        TWriteLockGuard guard(m_Seq_idMapLock);
        TSeq_idMap::iterator it = m_Seq_idMap.lower_bound(id);
        if ( it == m_Seq_idMap.end() || it->first != id ) {
            it = m_Seq_idMap.insert(it,
                                    TSeq_idMapValue(id, SSeq_id_ScopeInfo()));
        }
        return *it;
    }}
*/
/*
    {{
        TSeq_idMapLock::TReadLockGuard guard(m_Seq_idMapLock);
        TSeq_idMap::iterator it = m_Seq_idMap.lower_bound(id);
        if ( it != m_Seq_idMap.end() && it->first == id )
            return *it;
    }}
    {{
        TSeq_idMapLock::TWriteLockGuard guard(m_Seq_idMapLock);
        return *m_Seq_idMap.insert(
            TSeq_idMapValue(id, SSeq_id_ScopeInfo())).first;
    }}
*/
}


CScope_Impl::TSeq_idMapValue*
CScope_Impl::x_FindSeq_id_Info(const CSeq_id_Handle& id)
{
    TSeq_idMapLock::TReadLockGuard guard(m_Seq_idMapLock);
    TSeq_idMap::iterator it = m_Seq_idMap.lower_bound(id);
    if ( it != m_Seq_idMap.end() && it->first == id )
        return &*it;
    return 0;
}


CRef<CBioseq_ScopeInfo>
CScope_Impl::x_InitBioseq_Info(TSeq_idMapValue& info,
                               int get_flag,
                               SSeqMatch_Scope& match)
{
    if (get_flag != CScope::eGetBioseq_Resolved) {
        // Resolve only if the flag allows
        CInitGuard init(info.second.m_Bioseq_Info, m_MutexPool);
        if ( init ) {
            x_ResolveSeq_id(info, get_flag, match);
        }
    }
    if ( get_flag == CScope::eGetBioseq_All ) {
        _ASSERT(info.second.m_Bioseq_Info);
        _ASSERT(!info.second.m_Bioseq_Info->HasBioseq() ||
                &info.second.m_Bioseq_Info->x_GetScopeImpl() == this);
    }
    return info.second.m_Bioseq_Info;
}


bool CScope_Impl::x_InitBioseq_Info(TSeq_idMapValue& info,
                                    CBioseq_ScopeInfo& bioseq_info)
{
    _ASSERT(&bioseq_info.x_GetScopeImpl() == this);
    {{
        CInitGuard init(info.second.m_Bioseq_Info, m_MutexPool);
        if ( init ) {
            _ASSERT(!info.second.m_Bioseq_Info);
            info.second.m_Bioseq_Info.Reset(&bioseq_info);
            return true;
        }
    }}
    return info.second.m_Bioseq_Info.GetPointerOrNull() == &bioseq_info;
}


CRef<CBioseq_ScopeInfo>
CScope_Impl::x_GetBioseq_Info(const CSeq_id_Handle& id,
                              int get_flag,
                              SSeqMatch_Scope& match)
{
    return x_InitBioseq_Info(x_GetSeq_id_Info(id), get_flag, match);
}


CRef<CBioseq_ScopeInfo>
CScope_Impl::x_FindBioseq_Info(const CSeq_id_Handle& id,
                               int get_flag,
                               SSeqMatch_Scope& match)
{
    CRef<CBioseq_ScopeInfo> ret;
    TSeq_idMapValue* info = x_FindSeq_id_Info(id);
    if ( info ) {
        ret = x_InitBioseq_Info(*info, get_flag, match);
        if ( ret ) {
            _ASSERT(!ret->HasBioseq() || &ret->x_GetScopeImpl() == this);
        }
    }
    return ret;
}


CBioseq_Handle CScope_Impl::x_GetBioseqHandleFromTSE(const CSeq_id_Handle& id,
                                                     const CTSE_Handle& tse)
{
    TConfReadLockGuard rguard(m_ConfLock);
    SSeqMatch_Scope match;
    CRef<CBioseq_ScopeInfo> info =
        x_FindBioseq_Info(id, CScope::eGetBioseq_Loaded, match);
    CTSE_ScopeInfo& tse_info = tse.x_GetScopeInfo();
    if ( !info || !info->HasBioseq() ||
         &info->x_GetTSE_ScopeInfo() != &tse_info ) {
        info.Reset();
        if ( CSeq_id_Handle match_id = tse_info.ContainsMatchingBioseq(id) ) {
            match = tse_info.Resolve(match_id);
            if ( match ) {
                info = tse_info.GetBioseqInfo(match);
                _ASSERT(info && info->HasBioseq());
            }
        }
    }
    if ( info ) {
        return CBioseq_Handle(id, *info);
    }
    else {
        return CBioseq_Handle();
    }
}


CBioseq_Handle CScope_Impl::GetBioseqHandle(const CSeq_id_Handle& id,
                                            int get_flag)
{
    CBioseq_Handle ret;
    if ( id )  {
        SSeqMatch_Scope match;
        CRef<CBioseq_ScopeInfo> info;
        TConfReadLockGuard rguard(m_ConfLock);
        info = x_GetBioseq_Info(id, get_flag & fUserFlagMask, match);
        if ( info ) {
            ret.m_Handle_Seq_id = id;
            if ( info->HasBioseq() && !(get_flag & fNoLockFlag) ) {
                ret.m_Info = info->GetLock(match.m_Bioseq);
            }
            else {
                ret.m_Info.Reset(info);
            }
        }
    }
    return ret;
}


bool CScope_Impl::IsSameBioseq(const CSeq_id_Handle& id1,
                               const CSeq_id_Handle& id2,
                               int get_flag)
{
    if ( id1 == id2 ) {
        return true;
    }
    CBioseq_Handle bh1 = GetBioseqHandle(id1, get_flag | fNoLockFlag);
    if ( !bh1 ) {
        return false;
    }
    CBioseq_Handle bh2 = GetBioseqHandle(id2, get_flag | fNoLockFlag);
    return bh2 == bh1;
}


CRef<CDataSource_ScopeInfo>
CScope_Impl::GetEditDataSource(CDataSource_ScopeInfo& src_ds,
                               const CTSE_ScopeInfo* replaced_tse)
{
    if ( !src_ds.m_EditDS ) {
        TConfWriteLockGuard guard(m_ConfLock);
        if ( !src_ds.m_EditDS ) {
            CRef<CDataSource> ds(new CDataSource);
            _ASSERT(ds->CanBeEdited());
            src_ds.m_EditDS = AddDSBefore(ds, Ref(&src_ds), replaced_tse);
            _ASSERT(src_ds.m_EditDS);
            _ASSERT(src_ds.m_EditDS->CanBeEdited());
            if ( src_ds.GetDataLoader() ) {
                src_ds.m_EditDS->SetCanRemoveOnResetHistory();
            }
        }
    }
    return src_ds.m_EditDS;
}


CTSE_Handle CScope_Impl::GetEditHandle(const CTSE_Handle& handle)
{
    _ASSERT(handle);
    if ( handle.CanBeEdited() ) {
        return handle;
    }
    TConfWriteLockGuard guard(m_ConfLock);
    if ( handle.CanBeEdited() ) {
        return handle;
    }
    CTSE_ScopeInfo& scope_info = handle.x_GetScopeInfo();
    CRef<CDataSource_ScopeInfo> old_ds(&scope_info.GetDSInfo());
    CRef<CDataSource_ScopeInfo> new_ds =
        GetEditDataSource(*old_ds, &scope_info);
    // load all missing information if split
    //scope_info.m_TSE_Lock->GetCompleteSeq_entry();
    CRef<CTSE_Info> old_tse(const_cast<CTSE_Info*>(&*scope_info.m_TSE_Lock));
    CRef<CTSE_Info> new_tse(new CTSE_Info(scope_info.m_TSE_Lock));
    CTSE_Lock new_tse_lock = new_ds->GetDataSource().AddStaticTSE(new_tse);
    scope_info.SetEditTSE(new_tse_lock, *new_ds,
                          new_tse_lock->m_BaseTSE->m_ObjectCopyMap);
    const_cast<CTSE_Info&>(*new_tse_lock).m_BaseTSE->m_ObjectCopyMap.clear();
    _ASSERT(handle.CanBeEdited());
    _ASSERT(!old_ds->CanBeEdited());

    CRef<CDataSource> ds(&old_ds->GetDataSource());
    if ( ds->GetSharedObject() ) {
        // remove old shared object
        _ASSERT(!ds->GetDataLoader());
        _VERIFY(m_setDataSrc.Erase(*old_ds));
        _VERIFY(m_DSMap.erase(ds));
        ds.Reset();
        old_ds->DetachScope();
    }
    else if ( old_ds->IsConst() ) {
        _ASSERT(!ds->GetDataLoader());
        const_cast<CTSE_Info&>(*new_tse_lock).m_BaseTSE.reset();
        _VERIFY(ds->DropStaticTSE(*old_tse));
    }
    return handle;
}


CBioseq_EditHandle CScope_Impl::GetEditHandle(const CBioseq_Handle& h)
{
    CHECK_HANDLE(GetEditHandle, h);
    _VERIFY(GetEditHandle(h.GetTSE_Handle()) == h.GetTSE_Handle());
    _ASSERT(h.GetTSE_Handle().CanBeEdited());
    return CBioseq_EditHandle(h);
}


CSeq_entry_EditHandle CScope_Impl::GetEditHandle(const CSeq_entry_Handle& h)
{
    CHECK_HANDLE(GetEditHandle, h);
    _VERIFY(GetEditHandle(h.GetTSE_Handle()) == h.GetTSE_Handle());
    _ASSERT(h.GetTSE_Handle().CanBeEdited());
    return CSeq_entry_EditHandle(h);
}


CSeq_annot_EditHandle CScope_Impl::GetEditHandle(const CSeq_annot_Handle& h)
{
    CHECK_HANDLE(GetEditHandle, h);
    _VERIFY(GetEditHandle(h.GetTSE_Handle()) == h.GetTSE_Handle());
    _ASSERT(h.GetTSE_Handle().CanBeEdited());
    return CSeq_annot_EditHandle(h);
}


CBioseq_set_EditHandle CScope_Impl::GetEditHandle(const CBioseq_set_Handle& h)
{
    CHECK_HANDLE(GetEditHandle, h);
    _VERIFY(GetEditHandle(h.GetTSE_Handle()) == h.GetTSE_Handle());
    _ASSERT(h.GetTSE_Handle().CanBeEdited());
    return CBioseq_set_EditHandle(h);
}


CBioseq_Handle
CScope_Impl::GetBioseqHandleFromTSE(const CSeq_id_Handle& id,
                                    const CTSE_Handle& tse)
{
    CBioseq_Handle ret;
    if ( tse ) {
        ret = x_GetBioseqHandleFromTSE(id, tse);
    }
    return ret;
}


CBioseq_Handle CScope_Impl::GetBioseqHandle(const CSeq_loc& loc, int get_flag)
{
    CBioseq_Handle bh;
    TSeq_idSet ids;
    for (CSeq_loc_CI citer(loc); citer; ++citer) {
        ids.insert(citer.GetSeq_id_Handle());
    }
    if ( ids.empty() ) {
        // No ids found
        return bh;
    }

    // Find at least one bioseq handle
    ITERATE(TSeq_idSet, id, ids) {
        bh = GetBioseqHandle(*ids.begin(), get_flag);
        if ( bh ) {
            break;
        }
    }
    if ( !bh ) {
        if (ids.size() == 1) {
            return bh;
        }
        // Multiple unresolvable ids
        NCBI_THROW(CObjMgrException, eFindFailed,
                    "CScope_Impl::GetBioseqHandle: "
                    "Seq-loc references multiple unresolvable seq-ids");
    }

    const CTSE_Info& tse = bh.GetTSE_Handle().x_GetTSE_Info();
    CConstRef<CBioseq_Info> master = tse.GetSegSetMaster();

    bool valid = true;
    if ( master ) {
        CConstRef<CMasterSeqSegments> segs = tse.GetMasterSeqSegments();
        // Segmented set - check if all ids are parts of the segset,
        // return master sequence.
        ITERATE(TSeq_idSet, id, ids) {
            if (segs->FindSeg(*id) < 0) {
                if (ids.size() > 1) {
                    valid = false;
                }
                else {
                    // Allow a single bioseq which is not a segment (it can be
                    // the master sequence or a standalone sequence).
                    master.Reset();
                }
                break;
            }
        }
        if (valid  &&  master) {
            bh = GetBioseqHandle(*master, bh.GetTSE_Handle());
        }
    }
    else if (ids.size() > 1) {
        // Multiple ids, not a segset.
        valid = false;
    }

    if ( !valid ) {
        NCBI_THROW(CObjMgrException, eFindFailed,
                    "CScope_Impl::GetBioseqHandle: "
                    "Seq-loc references multiple seq-ids");
    }

    return bh;
}


CBioseq_Handle CScope_Impl::GetBioseqHandle(const CBioseq_Info& seq,
                                            const CTSE_Handle& tse)
{
    CBioseq_Handle ret;
    {{
        TConfReadLockGuard guard(m_ConfLock);
        ret = x_GetBioseqHandle(seq, tse);
    }}
    return ret;
}


CBioseq_Handle CScope_Impl::x_GetBioseqHandle(const CBioseq_Info& seq,
                                              const CTSE_Handle& tse)
{
    CBioseq_Handle ret;
    ret.m_Info = tse.x_GetScopeInfo().GetBioseqLock(null, ConstRef(&seq));
    x_UpdateHandleSeq_id(ret);
    return ret;
}


void CScope_Impl::x_UpdateHandleSeq_id(CBioseq_Handle& bh)
{
    if ( 1 || bh.m_Handle_Seq_id ) {
        return;
    }
    ITERATE ( CBioseq_Handle::TId, id, bh.GetId() ) {
        CBioseq_Handle bh2 = x_GetBioseqHandleFromTSE(*id, bh.GetTSE_Handle());
        if ( bh2 && &bh2.x_GetInfo() == &bh.x_GetInfo() ) {
            bh.m_Handle_Seq_id = *id;
            return;
        }
    }
}


SSeqMatch_Scope CScope_Impl::x_FindBioseqInfo(const CPriorityTree& tree,
                                              const CSeq_id_Handle& idh,
                                              int get_flag)
{
    SSeqMatch_Scope ret;
    // Process sub-tree
    TPriority last_priority = 0;
    ITERATE( CPriorityTree::TPriorityMap, mit, tree.GetTree() ) {
        // Search in all nodes of the same priority regardless
        // of previous results
        TPriority new_priority = mit->first;
        if ( new_priority != last_priority ) {
            // Don't process lower priority nodes if something
            // was found
            if ( ret ) {
                break;
            }
            last_priority = new_priority;
        }
        SSeqMatch_Scope new_ret = x_FindBioseqInfo(mit->second, idh, get_flag);
        if ( new_ret ) {
            _ASSERT(&new_ret.m_TSE_Lock->GetScopeImpl() == this);
            if ( ret && ret.m_Bioseq != new_ret.m_Bioseq ) {
                ret.m_BlobState = CBioseq_Handle::fState_conflict;
                ret.m_Bioseq.Reset();
                return ret;
            }
            ret = new_ret;
        }
        else if (new_ret.m_BlobState != 0) {
            // Remember first blob state
            if (!ret  &&  ret.m_BlobState == 0) {
                ret = new_ret;
            }
        }
    }
    return ret;
}


SSeqMatch_Scope CScope_Impl::x_FindBioseqInfo(CDataSource_ScopeInfo& ds_info,
                                              const CSeq_id_Handle& idh,
                                              int get_flag)
{
    _ASSERT(&ds_info.GetScopeImpl() == this);
    try {
        CPrefetchManager::IsActive();
        return ds_info.BestResolve(idh, get_flag);
    }
    catch (CBlobStateException& e) {
        SSeqMatch_Scope ret;
        ret.m_BlobState = e.GetBlobState();
        return ret;
    }
}


SSeqMatch_Scope CScope_Impl::x_FindBioseqInfo(const CPriorityNode& node,
                                              const CSeq_id_Handle& idh,
                                              int get_flag)
{
    SSeqMatch_Scope ret;
    if ( node.IsTree() ) {
        // Process sub-tree
        ret = x_FindBioseqInfo(node.GetTree(), idh, get_flag);
    }
    else if ( node.IsLeaf() ) {
        CDataSource_ScopeInfo& ds_info = 
            const_cast<CDataSource_ScopeInfo&>(node.GetLeaf());
        ret = x_FindBioseqInfo(ds_info, idh, get_flag);
    }
    return ret;
}


void CScope_Impl::x_ResolveSeq_id(TSeq_idMapValue& id_info,
                                  int get_flag,
                                  SSeqMatch_Scope& match)
{
    // Use priority, do not scan all DSs - find the first one.
    // Protected by m_ConfLock in upper-level functions
    match = x_FindBioseqInfo(m_setDataSrc, id_info.first, get_flag);
    if ( !match ) {
        // Map unresoved ids only if loading was requested
        _ASSERT(!id_info.second.m_Bioseq_Info);
        if (get_flag == CScope::eGetBioseq_All) {
            id_info.second.m_Bioseq_Info.Reset
                (new CBioseq_ScopeInfo(match.m_BlobState |
                                       CBioseq_Handle::fState_no_data));
        }
    }
    else {
        CTSE_ScopeInfo& tse_info = *match.m_TSE_Lock;
        _ASSERT(&tse_info.GetScopeImpl() == this);
        CRef<CBioseq_ScopeInfo> bioseq = tse_info.GetBioseqInfo(match);
        _ASSERT(!id_info.second.m_Bioseq_Info);
        _ASSERT(&bioseq->x_GetScopeImpl() == this);
        id_info.second.m_Bioseq_Info = bioseq;
    }
}


void CScope_Impl::GetTSESetWithAnnots(const CSeq_id_Handle& idh,
                                      TTSE_LockMatchSet& lock)
{
    {{
        TConfReadLockGuard rguard(m_ConfLock);
        TSeq_idMapValue& info = x_GetSeq_id_Info(idh);
        SSeqMatch_Scope seq_match;
        CRef<CBioseq_ScopeInfo> binfo =
            x_InitBioseq_Info(info, CScope::eGetBioseq_All, seq_match);
        if ( binfo->HasBioseq() ) {
            CInitGuard init(binfo->m_BioseqAnnotRef_Info, m_MutexPool);
            if ( init ) {
                CRef<CBioseq_ScopeInfo::TTSE_MatchSetObject> match
                    (new CBioseq_ScopeInfo::TTSE_MatchSetObject);
                x_GetTSESetWithBioseqAnnots(lock, match->GetData(), *binfo, 0);
                binfo->m_BioseqAnnotRef_Info = match;
            }
            else {
                x_LockMatchSet(lock, *binfo->m_BioseqAnnotRef_Info);
            }
#ifdef EXCLUDE_EDITED_BIOSEQ_ANNOT_SET
            if ( binfo->x_GetTSE_ScopeInfo().CanBeEdited() ) {
                x_GetTSESetWithBioseqAnnots(lock, *binfo, 0);
                return;
            }
#endif
        }
        else {
            CInitGuard init(info.second.m_AllAnnotRef_Info, m_MutexPool);
            if ( init ) {
                CRef<CBioseq_ScopeInfo::TTSE_MatchSetObject> match
                    (new CBioseq_ScopeInfo::TTSE_MatchSetObject);
                CSeq_id_Handle::TMatches ids;
                idh.GetReverseMatchingHandles(ids);
                x_GetTSESetWithOrphanAnnots(lock, match->GetData(), ids, 0, 0);
                info.second.m_AllAnnotRef_Info = match;
            }
            else {
                x_LockMatchSet(lock, *info.second.m_AllAnnotRef_Info);
            }
        }
    }}
}


void CScope_Impl::GetTSESetWithAnnots(const CBioseq_Handle& bh,
                                      TTSE_LockMatchSet& lock)
{
    if ( bh ) {
        TConfReadLockGuard rguard(m_ConfLock);
        CRef<CBioseq_ScopeInfo> binfo
            (&const_cast<CBioseq_ScopeInfo&>(bh.x_GetScopeInfo()));
        
        _ASSERT(binfo->HasBioseq());
        
        CInitGuard init(binfo->m_BioseqAnnotRef_Info, m_MutexPool);
        if ( init ) {
            CRef<CBioseq_ScopeInfo::TTSE_MatchSetObject> match
                (new CBioseq_ScopeInfo::TTSE_MatchSetObject);
            x_GetTSESetWithBioseqAnnots(lock, match->GetData(), *binfo, 0);
            binfo->m_BioseqAnnotRef_Info = match;
        }
        else {
            x_LockMatchSet(lock, *binfo->m_BioseqAnnotRef_Info);
        }
#ifdef EXCLUDE_EDITED_BIOSEQ_ANNOT_SET
        if ( binfo->x_GetTSE_ScopeInfo().CanBeEdited() ) {
            x_GetTSESetWithBioseqAnnots(lock, *binfo, 0);
            return;
        }
#endif
    }
}


void CScope_Impl::GetTSESetWithAnnots(const CSeq_id_Handle& idh,
                                      TTSE_LockMatchSet& lock,
                                      const SAnnotSelector& sel)
{
    TConfReadLockGuard rguard(m_ConfLock);
    TSeq_idMapValue& info = x_GetSeq_id_Info(idh);
    SSeqMatch_Scope seq_match;
    CRef<CBioseq_ScopeInfo> binfo =
        x_InitBioseq_Info(info, CScope::eGetBioseq_All, seq_match);
    TTSE_MatchSet match;
    if ( binfo->HasBioseq() ) {
        x_GetTSESetWithBioseqAnnots(lock, match, *binfo, &sel);
#ifdef EXCLUDE_EDITED_BIOSEQ_ANNOT_SET
        if ( binfo->x_GetTSE_ScopeInfo().CanBeEdited() ) {
            x_GetTSESetWithBioseqAnnots(lock, *binfo, &sel);
        }
#endif
    }
    else {
        CSeq_id_Handle::TMatches ids;
        idh.GetReverseMatchingHandles(ids);
        x_GetTSESetWithOrphanAnnots(lock, match, ids, 0, &sel);
    }
}


void CScope_Impl::GetTSESetWithAnnots(const CBioseq_Handle& bh,
                                      TTSE_LockMatchSet& lock,
                                      const SAnnotSelector& sel)
{
    if ( bh ) {
        TConfReadLockGuard rguard(m_ConfLock);
        CRef<CBioseq_ScopeInfo> binfo
            (&const_cast<CBioseq_ScopeInfo&>(bh.x_GetScopeInfo()));
        
        _ASSERT(binfo->HasBioseq());
        
        TTSE_MatchSet match;
        x_GetTSESetWithBioseqAnnots(lock, match, *binfo, &sel);
#ifdef EXCLUDE_EDITED_BIOSEQ_ANNOT_SET
        if ( binfo->x_GetTSE_ScopeInfo().CanBeEdited() ) {
            x_GetTSESetWithBioseqAnnots(lock, *binfo, &sel);
        }
#endif
    }
}


void CScope_Impl::x_AddTSESetWithAnnots(TTSE_LockMatchSet& lock,
                                        TTSE_MatchSet& match,
                                        const TTSE_LockMatchSet_DS& add,
                                        CDataSource_ScopeInfo& ds_info)
{
/*
    ITERATE( TTSE_LockMatchSet_DS, it, add ) {
        CTSE_Handle tse(*x_GetTSE_Lock(it->first, ds_info));
        CTSE_ScopeInfo& tse_info = tse.x_GetScopeInfo();
        match[Ref(&tse_info)].insert(it->second.begin(), it->second.end());
        lock[tse].insert(it->second.begin(), it->second.end());
    }
*/
    lock.reserve(add.size());
    ITERATE( TTSE_LockMatchSet_DS, it, add ) {
        CTSE_Handle tse(*x_GetTSE_Lock(it->first, ds_info));
        CTSE_ScopeInfo& tse_info = tse.x_GetScopeInfo();
        match.push_back(TTSE_MatchSet::value_type(Ref(&tse_info), it->second));
        lock.push_back(pair<CTSE_Handle, CSeq_id_Handle>(tse, it->second));
    }
}


void CScope_Impl::x_GetTSESetWithOrphanAnnots(TTSE_LockMatchSet& lock,
                                              TTSE_MatchSet& match,
                                              const TSeq_idSet& ids,
                                              CDataSource_ScopeInfo* excl_ds,
                                              const SAnnotSelector* sel)
{
    for (CPriority_I it(m_setDataSrc); it; ++it) {
        CPrefetchManager::IsActive();
        if ( &*it == excl_ds ) {
            // skip non-orphan annotations
            continue;
        }
        CDataSource& ds = it->GetDataSource();
        TTSE_LockMatchSet_DS ds_lock;
        ds.GetTSESetWithOrphanAnnots(ids, ds_lock, sel);
        x_AddTSESetWithAnnots(lock, match, ds_lock, *it);
    }
}


void CScope_Impl::x_GetTSESetWithBioseqAnnots(TTSE_LockMatchSet& lock,
                                              TTSE_MatchSet& match,
                                              CBioseq_ScopeInfo& binfo,
                                              const SAnnotSelector* sel)
{
    CDataSource_ScopeInfo& ds_info = binfo.x_GetTSE_ScopeInfo().GetDSInfo();
    CDataSource& ds = ds_info.GetDataSource();

    if ( m_setDataSrc.HasSeveralNodes() ) {
        // orphan annotations on all synonyms of Bioseq
        TSeq_idSet ids;
        // collect ids
        CConstRef<CSynonymsSet> syns = x_GetSynonyms(binfo);
        ITERATE ( CSynonymsSet, syn_it, *syns ) {
            // CSynonymsSet already contains all matching ids
            ids.insert(syns->GetSeq_id_Handle(syn_it));
        }
        // add orphan annots
        x_GetTSESetWithOrphanAnnots(lock, match, ids, &ds_info, sel);
    }

#ifdef EXCLUDE_EDITED_BIOSEQ_ANNOT_SET
    if ( binfo.x_GetTSE_ScopeInfo().CanBeEdited() )
        return;
#endif

    CPrefetchManager::IsActive();
    // datasource annotations on all ids of Bioseq
    // add external annots
    TBioseq_Lock bioseq = binfo.GetLock(null);
    TTSE_LockMatchSet_DS ds_lock;
    ds.GetTSESetWithBioseqAnnots(bioseq->GetObjectInfo(),
                                 bioseq->x_GetTSE_ScopeInfo().m_TSE_Lock,
                                 ds_lock, sel);
    x_AddTSESetWithAnnots(lock, match, ds_lock, ds_info);
}


void CScope_Impl::x_GetTSESetWithBioseqAnnots(TTSE_LockMatchSet& lock,
                                              CBioseq_ScopeInfo& binfo,
                                              const SAnnotSelector* sel)
{
    CDataSource_ScopeInfo& ds_info = binfo.x_GetTSE_ScopeInfo().GetDSInfo();
    CDataSource& ds = ds_info.GetDataSource();
    // datasource annotations on all ids of Bioseq
    // add external annots
    TBioseq_Lock bioseq = binfo.GetLock(null);
    TTSE_LockMatchSet_DS ds_lock;
    ds.GetTSESetWithBioseqAnnots(bioseq->GetObjectInfo(),
                                 bioseq->x_GetTSE_ScopeInfo().m_TSE_Lock,
                                 ds_lock, sel);
    CBioseq_ScopeInfo::TTSE_MatchSet match;
    x_AddTSESetWithAnnots(lock, match, ds_lock, ds_info);
    sort(lock.begin(), lock.end());
    lock.erase(unique(lock.begin(), lock.end()), lock.end());
}


void CScope_Impl::x_LockMatchSet(TTSE_LockMatchSet& lock,
                                 const TTSE_MatchSet& match)
{
    size_t size = match.size();
    lock.resize(size);
    for ( size_t i = 0; i < size; ++i ) {
        lock[i].first = *x_GetTSE_Lock(*match[i].first);
        lock[i].second = match[i].second;
    }
}

namespace {
    inline
    string sx_GetDSName(const SSeqMatch_Scope& match)
    {
        return match.m_TSE_Lock->GetDSInfo().GetDataSource().GetName();
    }
}


void CScope_Impl::RemoveFromHistory(CTSE_Handle tse)
{
    TConfWriteLockGuard guard(m_ConfLock);
    x_RemoveFromHistory(Ref(&tse.x_GetScopeInfo()), CScope::eRemoveIfLocked);
    _ASSERT(!tse);
}


void CScope_Impl::x_RemoveFromHistory(CRef<CTSE_ScopeInfo> tse_info,
                                      int action)
{
    _ASSERT(tse_info->IsAttached());
    tse_info->RemoveFromHistory(action);
    if ( !tse_info->IsAttached() ) {
        // removed
        x_ClearCacheOnRemoveData();
    }
}


void CScope_Impl::ResetHistory(int action)
{
    TConfWriteLockGuard guard(m_ConfLock);
    NON_CONST_ITERATE ( TDSMap, it, m_DSMap ) {
        it->second->ResetHistory(action);
    }
    x_ClearCacheOnRemoveData();
    //m_Seq_idMap.clear();
}


void CScope_Impl::ResetDataAndHistory(void)
{
    TConfWriteLockGuard guard(m_ConfLock);
    NON_CONST_ITERATE ( TDSMap, it, m_DSMap ) {
        it->second->ResetHistory(CScope::eRemoveIfLocked);
    }
    x_ClearCacheOnRemoveData();
    m_Seq_idMap.clear();
    NON_CONST_ITERATE ( TDSMap, it, m_DSMap ) {
        CDataSource_ScopeInfo& ds_info = *it->second;
        if ( ds_info.IsConst() || ds_info.CanBeEdited() ) {
            ds_info.ResetDS();
            ds_info.GetDataSource().DropAllTSEs();
        }
    }
}


void CScope_Impl::ResetScope(void)
{
    TConfWriteLockGuard guard(m_ConfLock);

    while ( !m_DSMap.empty() ) {
        TDSMap::iterator iter = m_DSMap.begin();
        CRef<CDataSource_ScopeInfo> ds_info(iter->second);
        m_DSMap.erase(iter);
        ds_info->DetachScope();
    }
    m_setDataSrc.Clear();
    m_Seq_idMap.clear();
}


void CScope_Impl::x_PopulateBioseq_HandleSet(const CSeq_entry_Handle& seh,
                                             TBioseq_HandleSet& handles,
                                             CSeq_inst::EMol filter,
                                             TBioseqLevelFlag level)
{
    if ( seh ) {
        TConfReadLockGuard rguard(m_ConfLock);
        const CSeq_entry_Info& info = seh.x_GetInfo();
        CDataSource::TBioseq_InfoSet info_set;
        info.GetDataSource().GetBioseqs(info, info_set, filter, level);
        // Convert each bioseq info into bioseq handle
        ITERATE (CDataSource::TBioseq_InfoSet, iit, info_set) {
            CBioseq_Handle bh = x_GetBioseqHandle(**iit, seh.GetTSE_Handle());
            if ( bh ) {
                handles.push_back(bh);
            }
        }
    }
}


CScope_Impl::TIds CScope_Impl::GetIds(const CSeq_id_Handle& idh)
{
    TIds ret;
    if (!idh) return ret;
    TConfReadLockGuard rguard(m_ConfLock);
    SSeqMatch_Scope match;
    CRef<CBioseq_ScopeInfo> info =
        x_FindBioseq_Info(idh, CScope::eGetBioseq_Resolved, match);
    if ( info ) {
        if ( info->HasBioseq() ) {
            ret = info->GetIds();
        }
    }
    else {
        // Unknown bioseq, try to find in data sources
        for (CPriority_I it(m_setDataSrc); it; ++it) {
            CPrefetchManager::IsActive();
            it->GetDataSource().GetIds(idh, ret);
            if ( !ret.empty() ) {
                break;
            }
        }
    }
    return ret;
}


CSeq_id_Handle CScope_Impl::GetAccVer(const CSeq_id_Handle& idh,
                                      bool force_load)
{
    CSeq_id_Handle ret;
    if ( !force_load ) {
        CConstRef<CSeq_id> id = idh.GetSeqId();
        const CTextseq_id* text_id = id->GetTextseq_Id();
        if ( text_id &&
             text_id->IsSetAccession() &&
             text_id->IsSetVersion() ) {
            ret = idh;
            return ret;
        }
    }
    TConfReadLockGuard rguard(m_ConfLock);

    if ( !force_load ) {
        SSeqMatch_Scope match;
        CRef<CBioseq_ScopeInfo> info =
            x_FindBioseq_Info(idh, CScope::eGetBioseq_Resolved, match);
        if ( info ) {
            if ( info->HasBioseq() ) {
                ret = CScope::x_GetAccVer(info->GetIds());
            }
            return ret;
        }
    }
    
    // Unknown bioseq, try to find in data sources
    for (CPriority_I it(m_setDataSrc); it; ++it) {
        CPrefetchManager::IsActive();
        ret = it->GetDataSource().GetAccVer(idh);
        if ( ret ) {
            break;
        }
    }
    return ret;
}


TGi CScope_Impl::GetGi(const CSeq_id_Handle& idh,
                       bool force_load)
{
    TGi ret = ZERO_GI;
    TConfReadLockGuard rguard(m_ConfLock);

    if ( !force_load ) {
        SSeqMatch_Scope match;
        CRef<CBioseq_ScopeInfo> info =
            x_FindBioseq_Info(idh, CScope::eGetBioseq_Resolved, match);
        if ( info ) {
            if ( info->HasBioseq() ) {
                ret = CScope::x_GetGi(info->GetIds());
            }
            return ret;
        }
    }
    
    // Unknown bioseq, try to find in data sources
    for (CPriority_I it(m_setDataSrc); it; ++it) {
        CPrefetchManager::IsActive();
        ret = it->GetDataSource().GetGi(idh);
        if (ret != ZERO_GI) {
            break;
        }
    }
    return ret;
}


string CScope_Impl::GetLabel(const CSeq_id_Handle& idh, bool force_load)
{
    string ret;
    if ( !force_load ) {
        ret = GetDirectLabel(idh);
        if ( !ret.empty() ) {
            return ret;
        }
    }

    TConfReadLockGuard rguard(m_ConfLock);
    if ( !force_load ) {
        SSeqMatch_Scope match;
        CRef<CBioseq_ScopeInfo> info =
            x_FindBioseq_Info(idh, CScope::eGetBioseq_Resolved, match);
        if ( info ) {
            if ( info->HasBioseq() ) {
                ret = objects::GetLabel(info->GetIds());
            }
            return ret;
        }
    }
    
    // Unknown bioseq, try to find in data sources
    for (CPriority_I it(m_setDataSrc); it; ++it) {
        ret = it->GetDataSource().GetLabel(idh);
        if ( !ret.empty() ) {
            break;
        }
    }
    return ret;
}


int CScope_Impl::GetTaxId(const CSeq_id_Handle& idh, bool force_load)
{
    if ( !force_load ) {
        if ( idh.Which() == CSeq_id::e_General ) {
            CConstRef<CSeq_id> id = idh.GetSeqId();
            const CDbtag& dbtag = id->GetGeneral();
            const CObject_id& obj_id = dbtag.GetTag();
            if ( obj_id.IsId() && dbtag.GetDb() == "TAXID" ) {
                return obj_id.GetId();
            }
        }
    }

    int ret = -1;
    TConfReadLockGuard rguard(m_ConfLock);
    if ( !force_load ) {
        SSeqMatch_Scope match;
        CRef<CBioseq_ScopeInfo> info =
            x_FindBioseq_Info(idh, CScope::eGetBioseq_Resolved, match);
        if ( info ) {
            if ( info->HasBioseq() ) {
                TBioseq_Lock bioseq = info->GetLock(null);
                ret = info->GetObjectInfo().GetTaxId();
            }
        }
    }

    if ( ret == -1 ) {
        // Unknown bioseq, try to find in data sources
        for (CPriority_I it(m_setDataSrc); it; ++it) {
            ret = it->GetDataSource().GetTaxId(idh);
            if ( ret >= 0 ) {
                break;
            }
        }
    }
    return ret;
}


CConstRef<CSynonymsSet> CScope_Impl::GetSynonyms(const CSeq_id_Handle& id,
                                                 int get_flag)
{
    _ASSERT(id);
    TConfReadLockGuard rguard(m_ConfLock);
    SSeqMatch_Scope match;
    CRef<CBioseq_ScopeInfo> info = x_GetBioseq_Info(id, get_flag, match);
    if ( !info ) {
        return CConstRef<CSynonymsSet>(0);
    }
    return x_GetSynonyms(*info);
}


CConstRef<CSynonymsSet> CScope_Impl::GetSynonyms(const CBioseq_Handle& bh)
{
    if ( !bh ) {
        return CConstRef<CSynonymsSet>();
    }
    TConfReadLockGuard rguard(m_ConfLock);
    return x_GetSynonyms(const_cast<CBioseq_ScopeInfo&>(bh.x_GetScopeInfo()));
}


void CScope_Impl::x_AddSynonym(const CSeq_id_Handle& idh,
                               CSynonymsSet& syn_set,
                               CBioseq_ScopeInfo& info)
{
    // Check current ID for conflicts, add to the set.
    TSeq_idMapValue& seq_id_info = x_GetSeq_id_Info(idh);
    if ( x_InitBioseq_Info(seq_id_info, info) ) {
        // the same bioseq - add synonym
        if ( !syn_set.ContainsSynonym(seq_id_info.first) ) {
            syn_set.AddSynonym(&seq_id_info);
        }
    }
    else {
        CRef<CBioseq_ScopeInfo> info2 = seq_id_info.second.m_Bioseq_Info;
        _ASSERT(info2 != &info);
        LOG_POST_X(17, Warning << "CScope::GetSynonyms: "
                   "Bioseq["<<info.IdString()<<"]: "
                   "id "<<idh.AsString()<<" is resolved to another "
                   "Bioseq["<<info2->IdString()<<"]");
    }
}


CConstRef<CSynonymsSet>
CScope_Impl::x_GetSynonyms(CBioseq_ScopeInfo& info)
{
    {{
        CInitGuard init(info.m_SynCache, m_MutexPool);
        if ( init ) {
            // It's OK to use CRef, at least one copy should be kept
            // alive by the id cache (for the ID requested).
            CRef<CSynonymsSet> syn_set(new CSynonymsSet);
            //syn_set->AddSynonym(id);
            if ( info.HasBioseq() ) {
                ITERATE ( CBioseq_ScopeInfo::TIds, it, info.GetIds() ) {
                    if ( it->HaveReverseMatch() ) {
                        CSeq_id_Handle::TMatches hset;
                        it->GetReverseMatchingHandles(hset);
                        ITERATE ( CSeq_id_Handle::TMatches, mit, hset ) {
                            x_AddSynonym(*mit, *syn_set, info);
                        }
                    }
                    else {
                        x_AddSynonym(*it, *syn_set, info);
                    }
                }
            }
            info.m_SynCache = syn_set;
        }
    }}
    return info.m_SynCache;
}


void CScope_Impl::GetAllTSEs(TTSE_Handles& tses, int kind)
{
    TConfReadLockGuard rguard(m_ConfLock);
    for (CPriority_I it(m_setDataSrc); it; ++it) {
        if (it->GetDataLoader() &&  kind == CScope::eManualTSEs) {
            // Skip data sources with loaders
            continue;
        }
        CDataSource_ScopeInfo::TTSE_InfoMapMutex::TReadLockGuard
            guard(it->GetTSE_InfoMapMutex());
        ITERATE(CDataSource_ScopeInfo::TTSE_InfoMap, j, it->GetTSE_InfoMap()) {
            tses.push_back(CTSE_Handle(*x_GetTSE_Lock(*j->second)));
        }
    }
}


CDataSource* CScope_Impl::GetFirstLoaderSource(void)
{
    TConfReadLockGuard rguard(m_ConfLock);
    for (CPriority_I it(m_setDataSrc); it; ++it) {
        if ( it->GetDataLoader() ) {
            return &it->GetDataSource();
        }
    }
    return 0;
}


IScopeTransaction_Impl& CScope_Impl::GetTransaction()
{
    if( !m_Transaction )
        m_Transaction = CreateTransaction();
    return *m_Transaction;   
}


IScopeTransaction_Impl* CScope_Impl::CreateTransaction()
{
    /*    if ( m_Transaction ) {
        m_Transaction = new CScopeSubTransaction_Impl(*this);
    } else {
        m_Transaction = new CScopeTransaction_Impl(*this);
        }*/
    m_Transaction = new CScopeTransaction_Impl(*this, m_Transaction);
    return m_Transaction;   
}

void CScope_Impl::SetActiveTransaction(IScopeTransaction_Impl* transaction)
{
    if (m_Transaction && (transaction && !transaction->HasScope(*this))) {
        NCBI_THROW(CObjMgrException, eModifyDataError,
                   "CScope_Impl::AttachToTransaction: already attached to another transaction");
    }
    if (transaction)
        transaction->AddScope(*this);
    m_Transaction = transaction;
}

bool CScope_Impl::IsTransactionActive() const
{
    return m_Transaction != 0;
}


static size_t sx_CountFalse(const vector<bool>& loaded)
{
#ifdef NCBI_COMPILER_WORKSHOP
    int tmp_count = 0;
    std::count(loaded.begin(), loaded.end(), false, tmp_count);
    return size_t(tmp_count);
#else
    return std::count(loaded.begin(), loaded.end(), false);
#endif
}


/// Bulk retrieval methods
CScope_Impl::TBioseqHandles CScope_Impl::GetBioseqHandles(const TIds& ids)
{
    TBioseqHandles ret;
    size_t count = ids.size();
    ret.reserve(count);
    if ( count > 200 ) {
        // split batch into smaller pieces to avoid problems with GC
        TIds ids1;
        for ( size_t pos = 0; pos < count; ) {
            size_t cnt = count - pos;
            if ( cnt > 150 ) cnt = 100;
            ids1.assign(ids.begin()+pos, ids.begin()+pos+cnt);
            TBioseqHandles ret1 = GetBioseqHandles(ids1);
            ret.insert(ret.end(), ret1.begin(), ret1.end());
            pos += cnt;
        }
        return ret;
    }
    ret.resize(count);
    TConfReadLockGuard rguard(m_ConfLock);
    // Keep locks to prevent cleanup of the loaded TSEs.
    typedef CDataSource_ScopeInfo::TSeqMatchMap TSeqMatchMap;
    TSeqMatchMap match_map;
    for ( size_t i = 0; i < count; ++i ) {
        ret[i] = GetBioseqHandle(ids[i], CScope::eGetBioseq_Resolved);
        if ( !ret[i] ) {
            match_map[ids[i]];
        }
    }
    if ( match_map.empty() ) {
        return ret;
    }
    for (CPriority_I it(m_setDataSrc); it; ++it) {
        it->GetBlobs(match_map);
    }
    for ( size_t i = 0; i < count; ++i ) {
        if ( ret[i] ) {
            continue;
        }
        TSeqMatchMap::iterator match = match_map.find(ids[i]);
        if (match != match_map.end()  &&  match->second) {
            ret[i] = GetBioseqHandle(ids[i], CScope::eGetBioseq_Loaded);
        }
        else {
            TSeq_idMapValue& id_info = x_GetSeq_id_Info(ids[i]);
            CInitGuard init(id_info.second.m_Bioseq_Info, m_MutexPool);
            if ( init ) {
                _ASSERT(!id_info.second.m_Bioseq_Info);
                id_info.second.m_Bioseq_Info.Reset(new CBioseq_ScopeInfo(
                    CBioseq_Handle::fState_no_data |
                    CBioseq_Handle::fState_not_found));
            }
            CRef<CBioseq_ScopeInfo> info = id_info.second.m_Bioseq_Info;
            ret[i].m_Handle_Seq_id = ids[i];
            ret[i].m_Info.Reset(info);
        }
    }
    return ret;
}


void CScope_Impl::GetAccVers(TIds& ret,
                             const TIds& ids,
                             bool force_load)
{
    size_t count = ids.size(), remaining = count;
    ret.assign(count, CSeq_id_Handle());
    vector<bool> loaded(count);
    if ( !force_load ) {
        for ( size_t i = 0; i < count; ++i ) {
            CConstRef<CSeq_id> id = ids[i].GetSeqId();
            const CTextseq_id* text_id = id->GetTextseq_Id();
            if ( text_id &&
                 text_id->IsSetAccession() &&
                 text_id->IsSetVersion() ) {
                ret[i] = ids[i];
                loaded[i] = true;
                --remaining;
            }
        }
    }
    if ( remaining ) {
        TConfReadLockGuard rguard(m_ConfLock);
        
        if ( !force_load ) {
            for ( size_t i = 0; i < count; ++i ) {
                if ( loaded[i] ) {
                    continue;
                }
                SSeqMatch_Scope match;
                CRef<CBioseq_ScopeInfo> info =
                    x_FindBioseq_Info(ids[i],
                                      CScope::eGetBioseq_Resolved,
                                      match);
                if ( info ) {
                    if ( info->HasBioseq() ) {
                        ret[i] = CScope::x_GetAccVer(info->GetIds());
                        loaded[i] = true;
                        --remaining;
                    }
                }
            }
        }
    
        // Unknown bioseq, try to find in data sources
        for (CPriority_I it(m_setDataSrc); it; ++it) {
            if ( !remaining ) {
                break;
            }
            CPrefetchManager::IsActive();
            it->GetDataSource().GetAccVers(ids, loaded, ret);
            remaining = sx_CountFalse(loaded);
        }
    }
}


void CScope_Impl::GetGis(TGIs& ret,
                         const TIds& ids,
                         bool force_load)
{
    size_t count = ids.size(), remaining = count;
    ret.assign(count, ZERO_GI);
    vector<bool> loaded(count);
    if ( !force_load ) {
        for ( size_t i = 0; i < count; ++i ) {
            if ( ids[i].IsGi() ) {
                ret[i] = ids[i].GetGi();
                loaded[i] = true;
                --remaining;
            }
        }
    }
    if ( remaining ) {
        TConfReadLockGuard rguard(m_ConfLock);
        
        if ( !force_load ) {
            for ( size_t i = 0; i < count; ++i ) {
                if ( loaded[i] ) {
                    continue;
                }
                SSeqMatch_Scope match;
                CRef<CBioseq_ScopeInfo> info =
                    x_FindBioseq_Info(ids[i],
                                      CScope::eGetBioseq_Resolved,
                                      match);
                if ( info ) {
                    if ( info->HasBioseq() ) {
                        ret[i] = CScope::x_GetGi(info->GetIds());
                        loaded[i] = true;
                        --remaining;
                    }
                }
            }
        }
    
        // Unknown bioseq, try to find in data sources
        for (CPriority_I it(m_setDataSrc); it; ++it) {
            if ( !remaining ) {
                break;
            }
            CPrefetchManager::IsActive();
            it->GetDataSource().GetGis(ids, loaded, ret);
            remaining = sx_CountFalse(loaded);
        }
    }
}


void CScope_Impl::GetLabels(TLabels& ret,
                            const TIds& ids,
                            bool force_load)
{
    size_t count = ids.size(), remaining = count;
    ret.assign(count, string());
    vector<bool> loaded(count);
    if ( !force_load ) {
        for ( size_t i = 0; i < count; ++i ) {
            ret[i] = GetDirectLabel(ids[i]);
            if ( !ret[i].empty() ) {
                loaded[i] = true;
                --remaining;
            }
        }
    }
    if ( remaining ) {
        TConfReadLockGuard rguard(m_ConfLock);
        
        if ( !force_load ) {
            for ( size_t i = 0; i < count; ++i ) {
                if ( loaded[i] ) {
                    continue;
                }
                SSeqMatch_Scope match;
                CRef<CBioseq_ScopeInfo> info =
                    x_FindBioseq_Info(ids[i],
                                      CScope::eGetBioseq_Resolved,
                                      match);
                if ( info ) {
                    if ( info->HasBioseq() ) {
                        ret[i] = objects::GetLabel(info->GetIds());
                        loaded[i] = true;
                        --remaining;
                    }
                }
            }
        }
    
        // Unknown bioseq, try to find in data sources
        for (CPriority_I it(m_setDataSrc); it; ++it) {
            if ( !remaining ) {
                break;
            }
            CPrefetchManager::IsActive();
            it->GetDataSource().GetLabels(ids, loaded, ret);
            remaining = sx_CountFalse(loaded);
        }
    }
}


void CScope_Impl::GetTaxIds(TTaxIds& ret,
                            const TIds& ids,
                            bool force_load)
{
    size_t count = ids.size(), remaining = count;
    ret.assign(count, -1);
    vector<bool> loaded(count);
    if ( !force_load ) {
        for ( size_t i = 0; i < count; ++i ) {
            if ( ids[i].Which() == CSeq_id::e_General ) {
                CConstRef<CSeq_id> id = ids[i].GetSeqId();
                const CDbtag& dbtag = id->GetGeneral();
                const CObject_id& obj_id = dbtag.GetTag();
                if ( obj_id.IsId() && dbtag.GetDb() == "TAXID" ) {
                    ret[i] = obj_id.GetId();
                    loaded[i] = true;
                    --remaining;
                }
            }
        }
    }
    if ( remaining ) {
        TConfReadLockGuard rguard(m_ConfLock);
        
        if ( !force_load ) {
            for ( size_t i = 0; i < count; ++i ) {
                if ( loaded[i] ) {
                    continue;
                }
                SSeqMatch_Scope match;
                CRef<CBioseq_ScopeInfo> info =
                    x_FindBioseq_Info(ids[i],
                                      CScope::eGetBioseq_Resolved,
                                      match);
                if ( info ) {
                    if ( info->HasBioseq() ) {
                        TBioseq_Lock bioseq = info->GetLock(null);
                        ret[i] = info->GetObjectInfo().GetTaxId();
                        loaded[i] = true;
                        --remaining;
                    }
                }
            }
        }
    
        // Unknown bioseq, try to find in data sources
        for (CPriority_I it(m_setDataSrc); it; ++it) {
            if ( !remaining ) {
                break;
            }
            CPrefetchManager::IsActive();
            it->GetDataSource().GetTaxIds(ids, loaded, ret);
            remaining = sx_CountFalse(loaded);
        }
    }
}


TSeqPos CScope_Impl::GetSequenceLength(const CSeq_id_Handle& idh,
                                       bool force_load)
{
    TConfReadLockGuard rguard(m_ConfLock);

    if ( !force_load ) {
        SSeqMatch_Scope match;
        CRef<CBioseq_ScopeInfo> info =
            x_FindBioseq_Info(idh, CScope::eGetBioseq_Resolved, match);
        if ( info ) {
            if ( info->HasBioseq() ) {
                TBioseq_Lock bioseq = info->GetLock(null);
                return info->GetObjectInfo().GetBioseqLength();
            }
            return kInvalidSeqPos;
        }
    }
    
    // Unknown bioseq, try to find in data sources
    for (CPriority_I it(m_setDataSrc); it; ++it) {
        CPrefetchManager::IsActive();
        TSeqPos length = it->GetDataSource().GetSequenceLength(idh);
        if ( length != kInvalidSeqPos ) {
            return length;
        }
    }
    return kInvalidSeqPos;
}
                                  

CSeq_inst::TMol CScope_Impl::GetSequenceType(const CSeq_id_Handle& idh,
                                             bool force_load)
{
    TConfReadLockGuard rguard(m_ConfLock);

    if ( !force_load ) {
        SSeqMatch_Scope match;
        CRef<CBioseq_ScopeInfo> info =
            x_FindBioseq_Info(idh, CScope::eGetBioseq_Resolved, match);
        if ( info ) {
            if ( info->HasBioseq() ) {
                TBioseq_Lock bioseq = info->GetLock(null);
                return info->GetObjectInfo().GetInst_Mol();
            }
            return CSeq_inst::eMol_not_set;
        }
    }
    
    // Unknown bioseq, try to find in data sources
    for (CPriority_I it(m_setDataSrc); it; ++it) {
        CPrefetchManager::IsActive();
        CSeq_inst::TMol type = it->GetDataSource().GetSequenceType(idh);
        if ( type != CSeq_inst::eMol_not_set ) {
            return type;
        }
    }
    return CSeq_inst::eMol_not_set;
}
                                  

void CScope_Impl::GetSequenceLengths(TSequenceLengths& ret,
                                     const TIds& ids,
                                     bool force_load)
{
    size_t count = ids.size(), remaining = count;
    ret.assign(count, kInvalidSeqPos);
    vector<bool> loaded(count);
    
    TConfReadLockGuard rguard(m_ConfLock);
    
    if ( !force_load ) {
        for ( size_t i = 0; i < count; ++i ) {
            if ( loaded[i] ) {
                continue;
            }
            SSeqMatch_Scope match;
            CRef<CBioseq_ScopeInfo> info =
                x_FindBioseq_Info(ids[i],
                                  CScope::eGetBioseq_Resolved,
                                  match);
            if ( info ) {
                if ( info->HasBioseq() ) {
                    TBioseq_Lock bioseq = info->GetLock(null);
                    ret[i] = info->GetObjectInfo().GetBioseqLength();
                    loaded[i] = true;
                    --remaining;
                }
            }
        }
    }
    
    // Unknown bioseq, try to find in data sources
    for (CPriority_I it(m_setDataSrc); it; ++it) {
        if ( !remaining ) {
            break;
        }
        CPrefetchManager::IsActive();
        it->GetDataSource().GetSequenceLengths(ids, loaded, ret);
        remaining = sx_CountFalse(loaded);
    }
}


void CScope_Impl::GetSequenceTypes(TSequenceTypes& ret,
                                   const TIds& ids,
                                   bool force_load)
{
    size_t count = ids.size(), remaining = count;
    ret.assign(count, CSeq_inst::eMol_not_set);
    vector<bool> loaded(count);
    
    TConfReadLockGuard rguard(m_ConfLock);
    
    if ( !force_load ) {
        for ( size_t i = 0; i < count; ++i ) {
            if ( loaded[i] ) {
                continue;
            }
            SSeqMatch_Scope match;
            CRef<CBioseq_ScopeInfo> info =
                x_FindBioseq_Info(ids[i],
                                  CScope::eGetBioseq_Resolved,
                                  match);
            if ( info ) {
                if ( info->HasBioseq() ) {
                    TBioseq_Lock bioseq = info->GetLock(null);
                    ret[i] = info->GetObjectInfo().GetInst_Mol();
                    loaded[i] = true;
                    --remaining;
                }
            }
        }
    }
    
    // Unknown bioseq, try to find in data sources
    for (CPriority_I it(m_setDataSrc); it; ++it) {
        if ( !remaining ) {
            break;
        }
        CPrefetchManager::IsActive();
        it->GetDataSource().GetSequenceTypes(ids, loaded, ret);
        remaining = sx_CountFalse(loaded);
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE
