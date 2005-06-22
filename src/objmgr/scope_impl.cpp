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

#include <objmgr/impl/data_source.hpp>
#include <objmgr/impl/tse_info.hpp>
#include <objmgr/impl/scope_info.hpp>
#include <objmgr/impl/bioseq_info.hpp>
#include <objmgr/impl/bioseq_set_info.hpp>
#include <objmgr/impl/seq_annot_info.hpp>
#include <objmgr/impl/priority.hpp>
#include <objmgr/impl/synonyms.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Delta_seq.hpp>
#include <objects/seq/Seq_literal.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Textseq_id.hpp>
#include <objects/seqset/Seq_entry.hpp>

#include <objmgr/impl/scope_impl.hpp>
#include <objmgr/seq_annot_ci.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


/////////////////////////////////////////////////////////////////////////////
//
//  CScope_Impl
//
/////////////////////////////////////////////////////////////////////////////


CScope_Impl::CScope_Impl(CObjectManager& objmgr)
    : m_HeapScope(0), m_ObjMgr(0)
{
    TWriteLockGuard guard(m_Scope_Conf_RWLock);
    x_AttachToOM(objmgr);
}


CScope_Impl::~CScope_Impl(void)
{
    TWriteLockGuard guard(m_Scope_Conf_RWLock);
    x_DetachFromOM();
}


CScope& CScope_Impl::GetScope(void)
{
    _ASSERT(m_HeapScope);
    return *m_HeapScope;
}


void CScope_Impl::x_AttachToOM(CObjectManager& objmgr)
{
    if ( m_ObjMgr != &objmgr ) {
        x_DetachFromOM();
        
        m_ObjMgr.Reset(&objmgr);
        m_ObjMgr->RegisterScope(*this);
    }
}


void CScope_Impl::x_DetachFromOM(void)
{
    if ( m_ObjMgr ) {
        // Drop and release all TSEs
        x_ResetHistory();
        m_ObjMgr->RevokeScope(*this);
        m_DSMap.clear();
        for (CPriority_I it(m_setDataSrc); it; ++it) {
            it->DetachFromOM(*m_ObjMgr);
        }
        m_ObjMgr.Reset();
    }
}


void CScope_Impl::AddDefaults(TPriority priority)
{
    CObjectManager::TDataSourcesLock ds_set;
    m_ObjMgr->AcquireDefaultDataSources(ds_set);

    TWriteLockGuard guard(m_Scope_Conf_RWLock);
    NON_CONST_ITERATE( CObjectManager::TDataSourcesLock, it, ds_set ) {
        m_setDataSrc.Insert(*x_GetDSInfo(const_cast<CDataSource&>(**it)),
                            (priority == kPriority_NotSet) ?
                            (*it)->GetDefaultPriority() : priority);
    }
    x_ClearCacheOnNewData();
}


void CScope_Impl::AddDataLoader(const string& loader_name, TPriority priority)
{
    CRef<CDataSource> ds = m_ObjMgr->AcquireDataLoader(loader_name);

    TWriteLockGuard guard(m_Scope_Conf_RWLock);
    m_setDataSrc.Insert(*x_GetDSInfo(*ds),
                        (priority == kPriority_NotSet) ?
                        ds->GetDefaultPriority() : priority);
    x_ClearCacheOnNewData();
}


void CScope_Impl::AddScope(CScope_Impl& scope, TPriority priority)
{
    TReadLockGuard src_guard(scope.m_Scope_Conf_RWLock);
    CPriorityTree tree(*this, scope.m_setDataSrc);
    src_guard.Release();
    
    TWriteLockGuard guard(m_Scope_Conf_RWLock);
    m_setDataSrc.Insert(tree, (priority == kPriority_NotSet) ? 9 : priority);
    x_ClearCacheOnNewData();
}


CSeq_entry_Handle CScope_Impl::AddSeq_entry(CSeq_entry& entry,
                                            TPriority priority)
{
    TWriteLockGuard guard(m_Scope_Conf_RWLock);
    CRef<CDataSource> ds(new CDataSource);
    CRef<CDataSource_ScopeInfo> ds_info = AddDS(ds, priority);
    CTSE_Lock tse_lock = ds->AddStaticTSE(entry);

    return CSeq_entry_Handle(*tse_lock, *ds_info->GetTSE_Lock(tse_lock));
}


CSeq_entry_Handle CScope_Impl::AddSharedSeq_entry(const CSeq_entry& entry,
                                                 TPriority priority)
{
    TWriteLockGuard guard(m_Scope_Conf_RWLock);
    CRef<CDataSource> ds = m_ObjMgr->AcquireSharedSeq_entry(entry);
    CRef<CDataSource_ScopeInfo> ds_info = AddDS(ds, priority);
    CTSE_Lock tse_lock = ds->GetSharedTSE();
    _ASSERT(tse_lock->GetTSECore() == &entry);

    return CSeq_entry_Handle(*tse_lock, *ds_info->GetTSE_Lock(tse_lock));
}


CBioseq_Handle CScope_Impl::AddBioseq(CBioseq& bioseq,
                                      TPriority priority)
{
    TWriteLockGuard guard(m_Scope_Conf_RWLock);
    CRef<CDataSource> ds(new CDataSource);
    CRef<CDataSource_ScopeInfo> ds_info = AddDS(ds, priority);
    CRef<CSeq_entry> entry(new CSeq_entry);
    entry->SetSeq(bioseq);
    CTSE_Lock tse_lock = ds->AddStaticTSE(*entry);

    return x_GetBioseqHandle(tse_lock->GetSeq(),
                             *ds_info->GetTSE_Lock(tse_lock));
}


CBioseq_Handle CScope_Impl::AddSharedBioseq(const CBioseq& bioseq,
                                            TPriority priority)
{
    TWriteLockGuard guard(m_Scope_Conf_RWLock);
    CRef<CDataSource> ds = m_ObjMgr->AcquireSharedBioseq(bioseq);
    CRef<CDataSource_ScopeInfo> ds_info = AddDS(ds, priority);
    CTSE_Lock tse_lock = ds->GetSharedTSE();
    _ASSERT(tse_lock->IsSeq() &&
            tse_lock->GetSeq().GetBioseqCore() == &bioseq);

    return x_GetBioseqHandle(tse_lock->GetSeq(),
                             *ds_info->GetTSE_Lock(tse_lock));
}


CSeq_annot_Handle CScope_Impl::AddSeq_annot(CSeq_annot& annot,
                                            TPriority priority)
{
    TWriteLockGuard guard(m_Scope_Conf_RWLock);
    CRef<CDataSource> ds(new CDataSource);
    CRef<CDataSource_ScopeInfo> ds_info = AddDS(ds, priority);
    CRef<CSeq_entry> entry(new CSeq_entry);
    entry->SetSet().SetSeq_set(); // it's not optional
    entry->SetSet().SetAnnot().push_back(Ref(&annot));
    CTSE_Lock tse_lock = ds->AddStaticTSE(*entry);

    return CSeq_annot_Handle(*tse_lock->GetSet().GetAnnot()[0],
                             *ds_info->GetTSE_Lock(tse_lock));
}


CSeq_annot_Handle CScope_Impl::AddSharedSeq_annot(const CSeq_annot& annot,
                                                  TPriority priority)
{
    TWriteLockGuard guard(m_Scope_Conf_RWLock);
    CRef<CDataSource> ds = m_ObjMgr->AcquireSharedSeq_annot(annot);
    CRef<CDataSource_ScopeInfo> ds_info = AddDS(ds, priority);
    CTSE_Lock tse_lock = ds->GetSharedTSE();
    _ASSERT(tse_lock->IsSet() &&
            tse_lock->GetSet().IsSetAnnot() &&
            tse_lock->GetSet().GetAnnot().size() == 1 &&
            tse_lock->GetSet().GetAnnot()[0]->GetSeq_annotCore() == &annot);

    return CSeq_annot_Handle(*tse_lock->GetSet().GetAnnot()[0],
                             *ds_info->GetTSE_Lock(tse_lock));
}


void CScope_Impl::RemoveDataLoader(const string& loader_name)
{
    CRef<CDataSource> ds = m_ObjMgr->AcquireDataLoader(loader_name);
    if ( !ds ) {
        NCBI_THROW(CObjMgrException, eFindFailed,
                "CScope_Impl::RemoveDataLoader: "
                "data loader not found in the scope");
    }

    TWriteLockGuard guard(m_Scope_Conf_RWLock);
    set<CRef<CTSE_ScopeInfo> > tse_set;
    CPriority_I ds_it(m_setDataSrc);
    while ( ds_it ) {
        CDataSource_ScopeInfo& ds_info = *ds_it;
        if (&ds_info.GetDataSource() == ds) {
            ITERATE(CDataSource_ScopeInfo::TTSE_InfoMap,
                it, ds_info.GetTSE_InfoMap()) {
                tse_set.insert(it->second);
            }
            NON_CONST_ITERATE(set<CRef<CTSE_ScopeInfo> >, tse, tse_set) {
                CTSE_Handle tseh(const_cast<CTSE_ScopeInfo&>(**tse));
                x_RemoveFromHistory(tseh);
                if (tseh) {
                    NCBI_THROW(CObjMgrException, eModifyDataError,
                            "CScope_Impl::RemoveDataLoader: "
                            "data source contains locked TSEs");
                }
            }
            m_setDataSrc.Erase(*ds_it);
            return; // Are there other priorities of the same loader?
        }
        ++ds_it;
    }
}


void CScope_Impl::RemoveTopLevelSeqEntry(const CTSE_Handle& entry)
{
    if ( !entry ) {
        NCBI_THROW(CObjMgrException, eFindFailed,
                "CScope_Impl::RemoveTopLevelSeqEntry: "
                "TSE not found in the scope");
    }
    CRef<CDataSource_ScopeInfo> ds_info(&entry.x_GetScopeInfo().GetDSInfo());
    CConstRef<CTSE_Info> tse_info(&entry.x_GetTSE_Info());
    if ( &ds_info->GetScopeImpl() != this ) {
        NCBI_THROW(CObjMgrException, eFindFailed,
                "CScope_Impl::RemoveTopLevelSeqEntry: "
                "TSE not found in the scope");
    }
    if ( ds_info->GetDataLoader() ) {
        NCBI_THROW(CObjMgrException, eModifyDataError,
                "CScope_Impl::RemoveTopLevelSeqEntry: "
                "can not remove a loaded TSE");
    }
    TWriteLockGuard guard(m_Scope_Conf_RWLock);
    x_RemoveFromHistory(entry);
    if ( entry ) {
        NCBI_THROW(CObjMgrException, eModifyDataError,
                "CScope_Impl::RemoveTopLevelSeqEntry: "
                "TSE is still locked");
    }
    if ( ds_info->IsShared() ) {
        if ( !m_setDataSrc.Erase(*ds_info) ) {
            NCBI_THROW(CObjMgrException, eModifyDataError,
                       "CScope_Impl::RemoveTopLevelSeqEntry: "
                       "can not remove data source from the scope");
        }
    }
    else {
        ds_info->GetDataSource().DropTSE(const_cast<CTSE_Info&>(*tse_info));
    }
}


CSeq_entry_EditHandle
CScope_Impl::x_AttachEntry(const CBioseq_set_EditHandle& seqset,
                           CRef<CSeq_entry_Info> entry,
                           int index)
{
    _ASSERT(seqset && entry);

    TWriteLockGuard guard(m_Scope_Conf_RWLock);

    //seqset.GetDataSource().AttachEntry(seqset, entry, index);
    seqset.x_GetInfo().AddEntry(entry, index);

    x_ClearCacheOnNewData();

    return CSeq_entry_EditHandle(*entry, seqset.GetTSE_Handle());
}


CBioseq_EditHandle
CScope_Impl::x_SelectSeq(const CSeq_entry_EditHandle& entry,
                         CRef<CBioseq_Info> bioseq)
{
    CBioseq_EditHandle ret;
    _ASSERT(entry && bioseq);
    _ASSERT(entry.Which() == CSeq_entry::e_not_set);

    TWriteLockGuard guard(m_Scope_Conf_RWLock);

    // duplicate bioseq info
    entry.x_GetInfo().SelectSeq(*bioseq);

    x_ClearCacheOnNewData();

    ret.m_Info = entry.x_GetScopeInfo().x_GetTSE_ScopeInfo()
        .GetBioseqLock(null, bioseq);
    return ret;
}


CBioseq_set_EditHandle
CScope_Impl::x_SelectSet(const CSeq_entry_EditHandle& entry,
                         CRef<CBioseq_set_Info> seqset)
{
    _ASSERT(entry && seqset);
    _ASSERT(entry.Which() == CSeq_entry::e_not_set);

    TWriteLockGuard guard(m_Scope_Conf_RWLock);

    // duplicate bioseq info
    entry.x_GetInfo().SelectSet(*seqset);

    x_ClearCacheOnNewData();

    return CBioseq_set_EditHandle(*seqset, entry.GetTSE_Handle());
}


CSeq_annot_EditHandle
CScope_Impl::x_AttachAnnot(const CSeq_entry_EditHandle& entry,
                           CRef<CSeq_annot_Info> annot)
{
    _ASSERT(entry && annot);

    TWriteLockGuard guard(m_Scope_Conf_RWLock);

    entry.x_GetInfo().AddAnnot(annot);

    x_ClearAnnotCache();

    return CSeq_annot_EditHandle(*annot, entry.GetTSE_Handle());
}


CSeq_entry_EditHandle
CScope_Impl::AttachEntry(const CBioseq_set_EditHandle& seqset,
                         CSeq_entry& entry,
                         int index)
{
    return x_AttachEntry(seqset, Ref(new CSeq_entry_Info(entry)), index);
}


CSeq_entry_EditHandle
CScope_Impl::CopyEntry(const CBioseq_set_EditHandle& seqset,
                       const CSeq_entry_Handle& entry,
                       int index)
{
    return x_AttachEntry(seqset,
                         Ref(new CSeq_entry_Info(entry.x_GetInfo(), 0)),
                         index);
}


CSeq_entry_EditHandle
CScope_Impl::TakeEntry(const CBioseq_set_EditHandle& seqset,
                       const CSeq_entry_EditHandle& entry,
                       int index)
{
    CRef<CSeq_entry_Info> info(&entry.x_GetInfo());
    entry.Remove();
    return x_AttachEntry(seqset, info, index);
}


CBioseq_EditHandle CScope_Impl::SelectSeq(const CSeq_entry_EditHandle& entry,
                                          CBioseq& seq)
{
    return x_SelectSeq(entry, Ref(new CBioseq_Info(seq)));
}


CBioseq_EditHandle CScope_Impl::CopySeq(const CSeq_entry_EditHandle& entry,
                                        const CBioseq_Handle& seq)
{
    return x_SelectSeq(entry,
                       Ref(new CBioseq_Info(seq.x_GetInfo(), 0)));
}


CBioseq_EditHandle CScope_Impl::TakeSeq(const CSeq_entry_EditHandle& entry,
                                        const CBioseq_EditHandle& seq)
{
    CRef<CBioseq_Info> info(&seq.x_GetInfo());
    seq.Remove();
    return x_SelectSeq(entry, info);
}


CBioseq_set_EditHandle
CScope_Impl::SelectSet(const CSeq_entry_EditHandle& entry,
                       CBioseq_set& seqset)
{
    return x_SelectSet(entry, Ref(new CBioseq_set_Info(seqset)));
}


CBioseq_set_EditHandle
CScope_Impl::CopySet(const CSeq_entry_EditHandle& entry,
                     const CBioseq_set_Handle& seqset)
{
    return x_SelectSet(entry,
                       Ref(new CBioseq_set_Info(seqset.x_GetInfo(), 0)));
}


CBioseq_set_EditHandle
CScope_Impl::TakeSet(const CSeq_entry_EditHandle& entry,
                     const CBioseq_set_EditHandle& seqset)
{
    CRef<CBioseq_set_Info> info(&seqset.x_GetInfo());
    seqset.Remove();
    return x_SelectSet(entry, info);
}


CSeq_annot_EditHandle
CScope_Impl::AttachAnnot(const CSeq_entry_EditHandle& entry,
                         const CSeq_annot& annot)
{
    return x_AttachAnnot(entry, Ref(new CSeq_annot_Info(annot)));
}


CSeq_annot_EditHandle
CScope_Impl::CopyAnnot(const CSeq_entry_EditHandle& entry,
                       const CSeq_annot_Handle& annot)
{
    return x_AttachAnnot(entry,
                         Ref(new CSeq_annot_Info(annot.x_GetInfo(), 0)));
}


CSeq_annot_EditHandle
CScope_Impl::TakeAnnot(const CSeq_entry_EditHandle& entry,
                       const CSeq_annot_EditHandle& annot)
{
    CRef<CSeq_annot_Info> info(&annot.x_GetInfo());
    annot.Remove();
    return x_AttachAnnot(entry, info);
}


void CScope_Impl::x_ClearCacheOnNewData(void)
{
    // Clear unresolved bioseq handles
    // Clear annot cache
    if (!m_Seq_idMap.empty()) {
        LOG_POST(Info <<
            "CScope_Impl: -- "
            "adding new data to a scope with non-empty history "
            "may cause the data to become inconsistent");
    }
    NON_CONST_ITERATE ( TSeq_idMap, it, m_Seq_idMap ) {
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
    }
}


void CScope_Impl::x_ClearCacheOnRemoveData(void)
{
    // Clear removed bioseq handles
    for ( TSeq_idMap::iterator it = m_Seq_idMap.begin();
          it != m_Seq_idMap.end(); ) {
        it->second.m_AllAnnotRef_Info.Reset();
        if ( it->second.m_Bioseq_Info ) {
            CBioseq_ScopeInfo& binfo = *it->second.m_Bioseq_Info;
            binfo.m_BioseqAnnotRef_Info.Reset();
            if ( binfo.IsRemoved() ) {
                binfo.m_SynCache.Reset();
                m_Seq_idMap.erase(it++);
                continue;
            }
        }
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


CSeq_entry_Handle CScope_Impl::GetSeq_entryHandle(const CSeq_entry& entry)
{
    TReadLockGuard guard(m_Scope_Conf_RWLock);
    TSeq_entry_Lock lock = x_GetSeq_entry_Lock(entry);
    return CSeq_entry_Handle(*lock.first, *lock.second);
}


CSeq_entry_Handle CScope_Impl::GetSeq_entryHandle(const CTSE_Handle& tse)
{
    return CSeq_entry_Handle(tse.x_GetTSE_Info(), tse);
}


CSeq_annot_Handle CScope_Impl::GetSeq_annotHandle(const CSeq_annot& annot)
{
    TReadLockGuard guard(m_Scope_Conf_RWLock);
    TSeq_annot_Lock lock = x_GetSeq_annot_Lock(annot);
    return CSeq_annot_Handle(*lock.first, *lock.second);
}


CBioseq_Handle CScope_Impl::GetBioseqHandle(const CBioseq& seq)
{
    CBioseq_Handle ret;
    {{
        TReadLockGuard guard(m_Scope_Conf_RWLock);
        ret.m_Info = x_GetBioseq_Lock(seq);
    }}
    return ret;
}


CRef<CDataSource_ScopeInfo> CScope_Impl::AddDS(CRef<CDataSource> ds,
                                               TPriority priority)
{
    TWriteLockGuard guard(m_Scope_Conf_RWLock);
    CRef<CDataSource_ScopeInfo> ds_info = x_GetDSInfo(*ds);
    m_setDataSrc.Insert(*ds_info,
                        (priority == kPriority_NotSet) ?
                        ds->GetDefaultPriority() : priority);
    x_ClearCacheOnNewData();
    return ds_info;
}


CRef<CDataSource_ScopeInfo>
CScope_Impl::AddDSBefore(CRef<CDataSource> ds,
                         CRef<CDataSource_ScopeInfo> ds2)
{
    TWriteLockGuard guard(m_Scope_Conf_RWLock);
    CRef<CDataSource_ScopeInfo> ds_info = x_GetDSInfo(*ds, false);
    for (CPriority_I it(m_setDataSrc); it; ++it) {
        if ( &*it == ds2 ) {
            it.InsertBefore(*ds_info);
            x_ClearCacheOnNewData();
            return ds_info;
        }
    }
    NCBI_THROW(CObjMgrException, eOtherError,
               "CScope_Impl::AddDSBefore: ds2 is not attached");
}


CRef<CDataSource_ScopeInfo> CScope_Impl::x_GetDSInfo(CDataSource& ds,
                                                     bool shared)
{
    CRef<CDataSource_ScopeInfo>& slot = m_DSMap[Ref(&ds)];
    if ( !slot ) {
        slot = new CDataSource_ScopeInfo(*this, ds, shared);
    }
    _ASSERT(slot->IsShared() == shared);
    return slot;
}


CScope_Impl::TTSE_Lock CScope_Impl::x_GetTSE_Lock(const CSeq_entry& tse)
{
    for (CPriority_I it(m_setDataSrc); it; ++it) {
        TTSE_Lock lock = it->FindTSE_Lock(tse);
        if ( lock ) {
            return lock;
        }
    }
    NCBI_THROW(CObjMgrException, eFindFailed,
               "CScope_Impl::x_GetTSE_Lock: entry is not attached");
}


CScope_Impl::TSeq_entry_Lock
CScope_Impl::x_GetSeq_entry_Lock(const CSeq_entry& entry)
{
    for (CPriority_I it(m_setDataSrc); it; ++it) {
        TSeq_entry_Lock lock = it->FindSeq_entry_Lock(entry);
        if ( lock.first ) {
            return lock;
        }
    }
    NCBI_THROW(CObjMgrException, eFindFailed,
               "CScope_Impl::x_GetSeq_entry_Lock: entry is not attached");
}


CScope_Impl::TSeq_annot_Lock
CScope_Impl::x_GetSeq_annot_Lock(const CSeq_annot& annot)
{
    for (CPriority_I it(m_setDataSrc); it; ++it) {
        TSeq_annot_Lock lock = it->FindSeq_annot_Lock(annot);
        if ( lock.first ) {
            return lock;
        }
    }
    NCBI_THROW(CObjMgrException, eFindFailed,
               "CScope_Impl::x_GetSeq_annot_Lock: annot is not attached");
}


CScope_Impl::TBioseq_Lock
CScope_Impl::x_GetBioseq_Lock(const CBioseq& bioseq)
{
    for (CPriority_I it(m_setDataSrc); it; ++it) {
        TBioseq_Lock lock = it->FindBioseq_Lock(bioseq);
        if ( lock ) {
            return lock;
        }
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
    if ( !entry.GetParentEntry() ) {
        CTSE_Handle tse = entry.GetTSE_Handle();
        // TODO entry.Reset();
        RemoveTopLevelSeqEntry(tse);
        return;
    }
    TWriteLockGuard guard(m_Scope_Conf_RWLock);

    entry.GetTSE_Handle().x_GetScopeInfo().RemoveEntry(entry.x_GetScopeInfo());

    x_ClearCacheOnRemoveData();
}


void CScope_Impl::RemoveAnnot(const CSeq_annot_EditHandle& annot)
{
    TWriteLockGuard guard(m_Scope_Conf_RWLock);

    annot.GetTSE_Handle().x_GetScopeInfo().RemoveAnnot(annot.x_GetScopeInfo());

    x_ClearAnnotCache();
}


void CScope_Impl::SelectNone(const CSeq_entry_EditHandle& entry)
{
    _ASSERT(entry);

    TWriteLockGuard guard(m_Scope_Conf_RWLock);
    
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


CScope_Impl::TSeq_idMapValue&
CScope_Impl::x_GetSeq_id_Info(const CSeq_id_Handle& id)
{
    TSeq_idMap::iterator it;
    {{
        TReadLockGuard guard(m_Seq_idMapLock);
        it = m_Seq_idMap.lower_bound(id);
        if ( it != m_Seq_idMap.end() && it->first == id )
            return *it;
    }}
    {{
        TWriteLockGuard guard(m_Seq_idMapLock);
        it = m_Seq_idMap.insert(it,
                                TSeq_idMapValue(id, SSeq_id_ScopeInfo()));
        return *it;
    }}
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
        TReadLockGuard guard(m_Seq_idMapLock);
        TSeq_idMap::iterator it = m_Seq_idMap.lower_bound(id);
        if ( it != m_Seq_idMap.end() && it->first == id )
            return *it;
    }}
    {{
        TWriteLockGuard guard(m_Seq_idMapLock);
        return *m_Seq_idMap.insert(
            TSeq_idMapValue(id, SSeq_id_ScopeInfo())).first;
    }}
*/
}


CScope_Impl::TSeq_idMapValue*
CScope_Impl::x_FindSeq_id_Info(const CSeq_id_Handle& id)
{
    TReadLockGuard guard(m_Seq_idMapLock);
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
            _ASSERT(&ret->x_GetScopeImpl() == this);
        }
    }
    return ret;
}


CBioseq_Handle CScope_Impl::x_GetBioseqHandleFromTSE(const CSeq_id_Handle& id,
                                                     const CTSE_Handle& tse)
{
    CBioseq_Handle ret;
    TReadLockGuard rguard(m_Scope_Conf_RWLock);
    SSeqMatch_Scope match;
    CRef<CBioseq_ScopeInfo> info =
        x_FindBioseq_Info(id, CScope::eGetBioseq_Loaded, match);
    if ( info ) {
        if ( info->HasBioseq() &&
             &info->x_GetTSE_ScopeInfo() == &tse.x_GetScopeInfo() ) {
            ret = CBioseq_Handle(id, *info);
        }
    }
    else {
        // new bioseq - try to find it in source TSE
        if ( tse.x_GetScopeInfo().ContainsMatchingBioseq(id) ) {
            ret = GetBioseqHandle(id, CScope::eGetBioseq_Loaded);
            if ( ret.GetTSE_Handle() != tse ) {
                ret.Reset();
            }
        }
    }
    return ret;
}


CBioseq_Handle CScope_Impl::GetBioseqHandle(const CSeq_id_Handle& id,
                                            int get_flag)
{
    CBioseq_Handle ret;
    if ( id )  {
        SSeqMatch_Scope match;
        CRef<CBioseq_ScopeInfo> info;
        TReadLockGuard rguard(m_Scope_Conf_RWLock);
        info = x_GetBioseq_Info(id, get_flag, match);
        if ( info && info->HasBioseq() ) {
            ret.m_Handle_Seq_id = id;
            ret.m_Info = info->GetLock(match.m_Bioseq);
        }
    }
    return ret;
}


CRef<CDataSource_ScopeInfo>
CScope_Impl::GetEditDataSource(CDataSource_ScopeInfo& src_ds)
{
    if ( !src_ds.m_EditDS ) {
        TWriteLockGuard guard(m_Scope_Conf_RWLock);
        if ( !src_ds.m_EditDS ) {
            CRef<CDataSource> ds(new CDataSource);
            src_ds.m_EditDS = AddDSBefore(ds, Ref(&src_ds));
            _ASSERT(src_ds.m_EditDS);
        }
    }
    return src_ds.m_EditDS;
}


CTSE_Handle CScope_Impl::GetEditHandle(const CTSE_Handle& src_tse)
{
    _ASSERT(src_tse && !src_tse.CanBeEdited());
    CTSE_ScopeInfo& scope_info = src_tse.x_GetScopeInfo();
    if ( !scope_info.m_EditLock ) {
        TWriteLockGuard guard(m_Scope_Conf_RWLock);
        if ( !scope_info.m_EditLock ) {
            // load all missing information if split
            src_tse.x_GetTSE_Info().GetCompleteSeq_entry();
            CRef<CDataSource_ScopeInfo> edit_ds =
                GetEditDataSource(scope_info.GetDSInfo());
            CRef<CTSE_Info> new_tse
                (new CTSE_Info(src_tse.x_GetScopeInfo().m_TSE_Lock));
#if 1 && defined(_DEBUG)
            LOG_POST("CTSE_Info is copied, map.size()="<<
                     new_tse->m_BaseTSE->m_ObjectCopyMap.size());
            LOG_POST(typeid(*new_tse->m_BaseTSE->m_BaseTSE).name() <<
                     "(" << &*new_tse->m_BaseTSE->m_BaseTSE << ")" <<
                     " -> " <<
                     typeid(*new_tse).name() <<
                     "(" << new_tse << ")");
            ITERATE ( TEditInfoMap, it, new_tse->m_BaseTSE->m_ObjectCopyMap ) {
                LOG_POST(typeid(*it->first).name() <<
                         "(" << it->first << ")" <<
                         " -> " <<
                         typeid(*it->second).name() <<
                         "(" << it->second << ")");
            }
#endif
            CTSE_Lock edit_tse_lock = edit_ds->GetDataSource().AddTSE(new_tse);
            scope_info.m_EditLock = edit_ds->GetTSE_Lock(edit_tse_lock);
        }
    }
    return *scope_info.m_EditLock;
}


CBioseq_EditHandle CScope_Impl::GetEditHandle(const CBioseq_Handle& h)
{
    if ( !h ) {
        NCBI_THROW(CObjMgrException, eInvalidHandle,
                   "CScope::GetEditHandle: null handle");
    }
    if ( h.GetTSE_Handle().CanBeEdited() ) {
        // use original TSE
        return CBioseq_EditHandle(h);
    }
    
    CBioseq_EditHandle ret;

    CTSE_Handle edit_tse(GetEditHandle(h.GetTSE_Handle()));
    _ASSERT(edit_tse && edit_tse != h.GetTSE_Handle());
    const CTSE_Info& edit_tse_info = edit_tse.x_GetTSE_Info();

    // find corresponding info in edit_tse
    const TEditInfoMap& info_map = edit_tse_info.m_BaseTSE->m_ObjectCopyMap;
    TEditInfoMap::const_iterator iter =
        info_map.find(CConstRef<CObject>(&h.x_GetInfo()));
    CConstRef<CBioseq_Info> edit_info;
    if ( iter != info_map.end() ) {
        edit_info = dynamic_cast<const CBioseq_Info*>(&*iter->second);
    }

    if ( !edit_info || !edit_info->BelongsToTSE_Info(edit_tse_info) ) {
        NCBI_THROW(CObjMgrException, eModifyDataError,
                   "CScope::GetEditHandle: "
                   "Bioseq already removed from TSE");
    }
    
    ret.m_Handle_Seq_id = h.m_Handle_Seq_id;
    ret.m_Info = edit_tse.x_GetScopeInfo().GetBioseqLock(null, edit_info);
    return ret;
}


CSeq_entry_EditHandle CScope_Impl::GetEditHandle(const CSeq_entry_Handle& h)
{
    if ( !h ) {
        NCBI_THROW(CObjMgrException, eInvalidHandle,
                   "CScope::GetEditHandle: null handle");
    }
    if ( h.GetTSE_Handle().CanBeEdited() ) {
        // use original TSE
        return CSeq_entry_EditHandle(h);
    }
    
    CTSE_Handle edit_tse(GetEditHandle(h.GetTSE_Handle()));
    _ASSERT(edit_tse && edit_tse != h.GetTSE_Handle());
    const CTSE_Info& edit_tse_info = edit_tse.x_GetTSE_Info();

    // find corresponding info in edit_tse
    const TEditInfoMap& info_map = edit_tse_info.m_BaseTSE->m_ObjectCopyMap;
    TEditInfoMap::const_iterator iter =
        info_map.find(CConstRef<CObject>(&h.x_GetInfo()));
    CRef<CSeq_entry_Info> edit_info;
    if ( iter != info_map.end() ) {
        edit_info =
            dynamic_cast<CSeq_entry_Info*>(iter->second.GetNCPointer());
    }

    if ( !edit_info || !edit_info->BelongsToTSE_Info(edit_tse_info) ) {
        NCBI_THROW(CObjMgrException, eModifyDataError,
                   "CScope::GetEditHandle: "
                   "Seq-entry already removed from TSE");
    }
    
    return CSeq_entry_EditHandle(*edit_info, edit_tse);
}


CSeq_annot_EditHandle CScope_Impl::GetEditHandle(const CSeq_annot_Handle& h)
{
    if ( !h ) {
        NCBI_THROW(CObjMgrException, eInvalidHandle,
                   "CScope::GetEditHandle: null handle");
    }
    if ( h.GetTSE_Handle().CanBeEdited() ) {
        // use original TSE
        return CSeq_annot_EditHandle(h);
    }
    
    CTSE_Handle edit_tse(GetEditHandle(h.GetTSE_Handle()));
    _ASSERT(edit_tse && edit_tse != h.GetTSE_Handle());
    const CTSE_Info& edit_tse_info = edit_tse.x_GetTSE_Info();

    // find corresponding info in edit_tse
    const TEditInfoMap& info_map = edit_tse_info.m_BaseTSE->m_ObjectCopyMap;
    TEditInfoMap::const_iterator iter =
        info_map.find(CConstRef<CObject>(&h.x_GetInfo()));
    CRef<CSeq_annot_Info> edit_info;
    if ( iter != info_map.end() ) {
        edit_info =
            dynamic_cast<CSeq_annot_Info*>(iter->second.GetNCPointer());
    }

    if ( !edit_info || !edit_info->BelongsToTSE_Info(edit_tse_info) ) {
        NCBI_THROW(CObjMgrException, eModifyDataError,
                   "CScope::GetEditHandle: "
                   "Seq-annot already removed from TSE");
    }
    
    return CSeq_annot_EditHandle(*edit_info, edit_tse);
}


CBioseq_set_EditHandle CScope_Impl::GetEditHandle(const CBioseq_set_Handle& h)
{
    if ( !h ) {
        NCBI_THROW(CObjMgrException, eInvalidHandle,
                   "CScope::GetEditHandle: null handle");
    }
    if ( h.GetTSE_Handle().CanBeEdited() ) {
        // use original TSE
        return CBioseq_set_EditHandle(h);
    }
    
    CTSE_Handle edit_tse(GetEditHandle(h.GetTSE_Handle()));
    _ASSERT(edit_tse && edit_tse != h.GetTSE_Handle());
    const CTSE_Info& edit_tse_info = edit_tse.x_GetTSE_Info();

    // find corresponding info in edit_tse
    const TEditInfoMap& info_map = edit_tse_info.m_BaseTSE->m_ObjectCopyMap;
    TEditInfoMap::const_iterator iter =
        info_map.find(CConstRef<CObject>(&h.x_GetInfo()));
    CRef<CBioseq_set_Info> edit_info;
    if ( iter != info_map.end() ) {
        edit_info =
            dynamic_cast<CBioseq_set_Info*>(iter->second.GetNCPointer());
    }

    if ( !edit_info || !edit_info->BelongsToTSE_Info(edit_tse_info) ) {
        NCBI_THROW(CObjMgrException, eModifyDataError,
                   "CScope::GetEditHandle: "
                   "Bioseq-set already removed from TSE");
    }
    
    return CBioseq_set_EditHandle(*edit_info, edit_tse);
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
    for ( CSeq_loc_CI citer (loc); citer; ++citer) {
        bh = GetBioseqHandle(CSeq_id_Handle::GetHandle(citer.GetSeq_id()),
                             get_flag);
        if ( bh ) {
            break;
        }
    }
    return bh;
}


CBioseq_Handle CScope_Impl::GetBioseqHandle(const CBioseq_Info& seq,
                                            const CTSE_Handle& tse)
{
    CBioseq_Handle ret;
    {{
        TReadLockGuard guard(m_Scope_Conf_RWLock);
        ret = x_GetBioseqHandle(seq, tse);
    }}
    return ret;
}


CBioseq_Handle CScope_Impl::x_GetBioseqHandle(const CBioseq_Info& seq,
                                              const CTSE_Handle& tse)
{
    CBioseq_Handle ret;
    ret.m_Info = tse.x_GetScopeInfo().GetBioseqLock(null, ConstRef(&seq));
    return ret;
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
        ret =
            x_FindBioseqInfo(const_cast<CDataSource_ScopeInfo&>(node.GetLeaf()),
                             idh, get_flag);
    }
    return ret;
}


void CScope_Impl::x_ResolveSeq_id(TSeq_idMapValue& id_info,
                                  int get_flag,
                                  SSeqMatch_Scope& match)
{
    // Use priority, do not scan all DSs - find the first one.
    // Protected by m_Scope_Conf_RWLock in upper-level functions
    match = x_FindBioseqInfo(m_setDataSrc, id_info.first, get_flag);
    if ( !match ) {
        // Map unresoved ids only if loading was requested
        _ASSERT(!id_info.second.m_Bioseq_Info);
        if (get_flag == CScope::eGetBioseq_All) {
            id_info.second.m_Bioseq_Info.Reset
                (new CBioseq_ScopeInfo(match.m_BlobState));
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


CScope_Impl::TTSE_LockMatchSet
CScope_Impl::GetTSESetWithAnnots(const CSeq_id_Handle& idh)
{
    TTSE_LockMatchSet lock;

    {{
        TReadLockGuard rguard(m_Scope_Conf_RWLock);
        TSeq_idMapValue& info = x_GetSeq_id_Info(idh);
        SSeqMatch_Scope seq_match;
        CRef<CBioseq_ScopeInfo> binfo =
            x_InitBioseq_Info(info, CScope::eGetBioseq_All, seq_match);
        if ( binfo->HasBioseq() ) {
            CInitGuard init(binfo->m_BioseqAnnotRef_Info, m_MutexPool);
            if ( init ) {
                CRef<CBioseq_ScopeInfo::TTSE_MatchSetObject> match
                    (new CBioseq_ScopeInfo::TTSE_MatchSetObject);
                x_GetTSESetWithBioseqAnnots(lock, match->GetData(), *binfo);
                binfo->m_BioseqAnnotRef_Info = match;
            }
            else {
                x_LockMatchSet(lock, *binfo->m_BioseqAnnotRef_Info);
            }
        }
        else {
            CInitGuard init(info.second.m_AllAnnotRef_Info, m_MutexPool);
            if ( init ) {
                CRef<CBioseq_ScopeInfo::TTSE_MatchSetObject> match
                    (new CBioseq_ScopeInfo::TTSE_MatchSetObject);
                CSeq_id_Handle::TMatches ids;
                idh.GetReverseMatchingHandles(ids);
                x_GetTSESetWithOrphanAnnots(lock, match->GetData(), ids, 0);
                info.second.m_AllAnnotRef_Info = match;
            }
            else {
                x_LockMatchSet(lock, *info.second.m_AllAnnotRef_Info);
            }
        }
    }}

    return lock;
}


CScope_Impl::TTSE_LockMatchSet
CScope_Impl::GetTSESetWithAnnots(const CBioseq_Handle& bh)
{
    TTSE_LockMatchSet lock;
    if ( bh ) {
        TReadLockGuard rguard(m_Scope_Conf_RWLock);
        CRef<CBioseq_ScopeInfo> binfo
            (&const_cast<CBioseq_ScopeInfo&>(bh.x_GetScopeInfo()));
        
        _ASSERT(binfo->HasBioseq());
        
        CInitGuard init(binfo->m_BioseqAnnotRef_Info, m_MutexPool);
        if ( init ) {
            CRef<CBioseq_ScopeInfo::TTSE_MatchSetObject> match
                (new CBioseq_ScopeInfo::TTSE_MatchSetObject);
            x_GetTSESetWithBioseqAnnots(lock, match->GetData(), *binfo);
            binfo->m_BioseqAnnotRef_Info = match;
        }
        else {
            x_LockMatchSet(lock, *binfo->m_BioseqAnnotRef_Info);
        }
    }
    return lock;
}


void CScope_Impl::x_AddTSESetWithAnnots(TTSE_LockMatchSet& lock,
                                        TTSE_MatchSet& match,
                                        const TTSE_LockMatchSet_DS& add,
                                        CDataSource_ScopeInfo& ds_info)
{
    ITERATE( TTSE_LockMatchSet_DS, it, add ) {
        CTSE_Handle tse(*x_GetTSE_Lock(it->first, ds_info));
        CTSE_ScopeInfo* info =
            const_cast<CTSE_ScopeInfo*>(&tse.x_GetScopeInfo());
        match[Ref(info)].insert(it->second.begin(), it->second.end());
        lock[tse].insert(it->second.begin(), it->second.end());
    }
}


void CScope_Impl::x_GetTSESetWithOrphanAnnots(TTSE_LockMatchSet& lock,
                                              TTSE_MatchSet& match,
                                              const TSeq_idSet& ids,
                                              CDataSource_ScopeInfo* excl_ds)
{
    for (CPriority_I it(m_setDataSrc); it; ++it) {
        if ( &*it == excl_ds ) {
            // skip non-orphan annotations
            continue;
        }
        CDataSource& ds = it->GetDataSource();
        TTSE_LockMatchSet_DS ds_lock = ds.GetTSESetWithOrphanAnnots(ids);
        x_AddTSESetWithAnnots(lock, match, ds_lock, *it);
    }
}


void CScope_Impl::x_GetTSESetWithBioseqAnnots(TTSE_LockMatchSet& lock,
                                              TTSE_MatchSet& match,
                                              CBioseq_ScopeInfo& binfo)
{
    CDataSource_ScopeInfo& ds_info = binfo.x_GetTSE_ScopeInfo().GetDSInfo();
    CDataSource& ds = ds_info.GetDataSource();

    if ( !m_setDataSrc.IsEmpty()  &&  !m_setDataSrc.IsSingle() ) {
        // orphan annotations on all synonyms of Bioseq
        TSeq_idSet ids;
        // collect ids
        CConstRef<CSynonymsSet> syns = x_GetSynonyms(binfo);
        ITERATE ( CSynonymsSet, syn_it, *syns ) {
            // CSynonymsSet already contains all matching ids
            ids.insert(syns->GetSeq_id_Handle(syn_it));
        }
        // add orphan annots
        x_GetTSESetWithOrphanAnnots(lock, match, ids, &ds_info);
    }

    // datasource annotations on all ids of Bioseq
    // add external annots
    TBioseq_Lock bioseq = binfo.GetLock(null);
    TTSE_LockMatchSet_DS ds_lock =
        ds.GetTSESetWithBioseqAnnots(bioseq->GetObjectInfo(),
                                     bioseq->x_GetTSE_ScopeInfo().m_TSE_Lock);
    x_AddTSESetWithAnnots(lock, match, ds_lock, ds_info);
}


void CScope_Impl::x_LockMatchSet(TTSE_LockMatchSet& lock,
                                 const TTSE_MatchSet& match)
{
    ITERATE ( TTSE_MatchSet, it, match ) {
        lock[*x_GetTSE_Lock(*it->first)].insert(it->second.begin(),
                                                it->second.end());
    }
}

namespace {
    inline
    string sx_GetDSName(const SSeqMatch_Scope& match)
    {
        return match.m_TSE_Lock->GetDSInfo().GetDataSource().GetName();
    }
}


void CScope_Impl::ResetHistory(void)
{
    TWriteLockGuard guard(m_Scope_Conf_RWLock);
    x_ResetHistory();
}


void CScope_Impl::RemoveFromHistory(const CBioseq_Handle& bioseq)
{
    CTSE_Handle& tse = const_cast<CTSE_Handle&>(bioseq.GetTSE_Handle());
    {{
        TWriteLockGuard guard(m_Scope_Conf_RWLock);
        x_RemoveFromHistory(tse);
    }}
    if ( !tse ) {
        // TODO bioseq.Reset();
    }
}


void CScope_Impl::RemoveFromHistory(const CTSE_Handle& tse)
{
    TWriteLockGuard guard(m_Scope_Conf_RWLock);
    x_RemoveFromHistory(tse);
}


void CScope_Impl::x_RemoveFromHistory(const CTSE_Handle& tse)
{
    CTSE_ScopeInfo& tse_info = tse.x_GetScopeInfo();
    /*
    if ( tse_info.LockedMoreThanOnce() ) {
        return;
    }
    */

    const CTSE_ScopeInfo::TBioseqById& bs_ids = tse_info.m_BioseqById;
    ITERATE(CTSE_ScopeInfo::TBioseqById, id, bs_ids) {
        TSeq_idMap::iterator mapped_id = m_Seq_idMap.find(id->first);
        if (mapped_id == m_Seq_idMap.end()) {
            continue;
        }
        if ( mapped_id->second.m_Bioseq_Info ) {
            CBioseq_ScopeInfo& binfo = *mapped_id->second.m_Bioseq_Info;
            binfo.m_BioseqAnnotRef_Info.Reset();
            binfo.m_SynCache.Reset();
        }
        m_Seq_idMap.erase(mapped_id);
    }
    tse_info.GetDSInfo().RemoveFromHistory(tse_info);
    // TODO tse.m_TSE.Release();
}


void CScope_Impl::x_ResetHistory(void)
{
    // 1. detach all CBbioseq_Handle objects from scope, and
    // 2. break circular link:
    // CBioseq_ScopeInfo-> CSynonymsSet-> SSeq_id_ScopeInfo-> CBioseq_ScopeInfo

    // Check for locked TSEs ???
    set< CRef<CTSE_ScopeInfo> > unlocked_infos;
    TSeq_idMap::iterator id_it = m_Seq_idMap.begin();
    while ( id_it != m_Seq_idMap.end() ) {
        if ( id_it->second.m_Bioseq_Info ) {
            CBioseq_ScopeInfo& binfo = *id_it->second.m_Bioseq_Info;
            if ( binfo.HasBioseq() ) {
                if ( binfo.x_GetTSE_ScopeInfo().IsLocked() ) {
                    ++id_it;
                    continue;
                }
                binfo.m_BioseqAnnotRef_Info.Reset(); // break circular link
                CTSE_ScopeInfo& tse_info = const_cast<CTSE_ScopeInfo&>(
                    id_it->second.m_Bioseq_Info->x_GetTSE_ScopeInfo());
                unlocked_infos.insert(Ref(&tse_info));
            }
            binfo.m_SynCache.Reset(); // break circular link
        }
        m_Seq_idMap.erase(id_it++);
    }
    NON_CONST_ITERATE (set< CRef<CTSE_ScopeInfo> >, it, unlocked_infos) {
        CRef<CTSE_ScopeInfo> tse = *it;
        tse->GetDSInfo().RemoveFromHistory(*tse);
    }
}


void CScope_Impl::x_PopulateBioseq_HandleSet(const CSeq_entry_Handle& seh,
                                             TBioseq_HandleSet& handles,
                                             CSeq_inst::EMol filter,
                                             TBioseqLevelFlag level)
{
    if ( seh ) {
        TReadLockGuard rguard(m_Scope_Conf_RWLock);
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
    TReadLockGuard rguard(m_Scope_Conf_RWLock);
    SSeqMatch_Scope match;
    CRef<CBioseq_ScopeInfo> info =
        x_FindBioseq_Info(idh, CScope::eGetBioseq_Resolved, match);
    if ( info  &&  info->HasBioseq() ) {
        return info->GetIds();
    }
    // Unknown bioseq, try to find in data sources
    TIds ret;
    for (CPriority_I it(m_setDataSrc); it; ++it) {
        it->GetDataSource().GetIds(idh, ret);
        if ( !ret.empty() ) {
            return ret;
        }
    }
    return ret;
}


CConstRef<CSynonymsSet> CScope_Impl::GetSynonyms(const CSeq_id_Handle& id,
                                                 int get_flag)
{
    _ASSERT(id);
    TReadLockGuard rguard(m_Scope_Conf_RWLock);
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
    TReadLockGuard rguard(m_Scope_Conf_RWLock);
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
        LOG_POST(Warning << "CScope::GetSynonyms: "
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
    TReadLockGuard rguard(m_Scope_Conf_RWLock);
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
    TReadLockGuard rguard(m_Scope_Conf_RWLock);
    for (CPriority_I it(m_setDataSrc); it; ++it) {
        if ( it->GetDataLoader() ) {
            return &it->GetDataSource();
        }
    }
    return 0;
}


END_SCOPE(objects)
END_NCBI_SCOPE
