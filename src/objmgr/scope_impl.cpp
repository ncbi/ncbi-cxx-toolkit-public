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
#include <objmgr/seqmatch_info.hpp>
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

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


/////////////////////////////////////////////////////////////////////////////
//
//  CScope_Impl
//
/////////////////////////////////////////////////////////////////////////////


CScope_Impl::CScope_Impl(CObjectManager& objmgr)
    : m_HeapScope(0), m_pObjMgr(0)
{
    TWriteLockGuard guard(m_Scope_Conf_RWLock);
    x_AttachToOM(objmgr);
}


CScope_Impl::~CScope_Impl(void)
{
    TWriteLockGuard guard(m_Scope_Conf_RWLock);
    x_DetachFromOM();
}

void CScope_Impl::x_AttachToOM(CObjectManager& objmgr)
{
    if ( m_pObjMgr != &objmgr ) {
        x_DetachFromOM();
        
        m_pObjMgr = &objmgr;
        m_pObjMgr->RegisterScope(*this);
    }
}


void CScope_Impl::x_DetachFromOM(void)
{
    if ( m_pObjMgr ) {
        // Drop and release all TSEs
        x_ResetHistory();
        m_pObjMgr->RevokeScope(*this);
        for (CPriority_I it(m_setDataSrc); it; ++it) {
            _ASSERT(it->m_DataSource);
            m_pObjMgr->ReleaseDataSource(it->m_DataSource);
            _ASSERT(!it->m_DataSource);
        }
        m_pObjMgr = 0;
    }
}


void CScope_Impl::AddDefaults(TPriority priority)
{
    CObjectManager::TDataSourcesLock ds_set;
    m_pObjMgr->AcquireDefaultDataSources(ds_set);

    TWriteLockGuard guard(m_Scope_Conf_RWLock);
    NON_CONST_ITERATE( CObjectManager::TDataSourcesLock, it, ds_set ) {
        m_setDataSrc.Insert(const_cast<CDataSource&>(**it),
            (priority == kPriority_NotSet) ?
            (*it)->GetDefaultPriority() : priority);
    }
    x_ClearCacheOnNewData();
}


void CScope_Impl::AddDataLoader (const string& loader_name,
                                 TPriority priority)
{
    CRef<CDataSource> ds = m_pObjMgr->AcquireDataLoader(loader_name);

    TWriteLockGuard guard(m_Scope_Conf_RWLock);
    m_setDataSrc.Insert(*ds, (priority == kPriority_NotSet) ?
        ds->GetDefaultPriority() : priority);
    x_ClearCacheOnNewData();
}


CSeq_entry_Handle CScope_Impl::AddTopLevelSeqEntry(CSeq_entry& top_entry,
                                                   TPriority priority)
{
    CRef<CDataSource> ds = m_pObjMgr->AcquireTopLevelSeqEntry(top_entry);
    TWriteLockGuard guard(m_Scope_Conf_RWLock);
    m_setDataSrc.Insert(*ds, (priority == kPriority_NotSet) ?
        ds->GetDefaultPriority() : priority);
    x_ClearCacheOnNewData();
    TSeq_entry_Lock lock = x_GetSeq_entry_Lock(top_entry);
    return CSeq_entry_Handle(*m_HeapScope, *lock.first, lock.second);
}


void CScope_Impl::AddScope(CScope_Impl& scope,
                           TPriority priority)
{
    TReadLockGuard src_guard(scope.m_Scope_Conf_RWLock);
    CPriorityTree tree(scope.m_setDataSrc);
    src_guard.Release();
    
    TWriteLockGuard guard(m_Scope_Conf_RWLock);
    m_setDataSrc.Insert(tree, priority);
    x_ClearCacheOnNewData();
}


CBioseq_Handle CScope_Impl::AddBioseq(CBioseq& bioseq,
                                      TPriority priority)
{
    CRef<CSeq_entry> entry(new CSeq_entry);
    entry->SetSeq(bioseq);
    return AddTopLevelSeqEntry(*entry, priority).GetSeq();
}


CSeq_entry_EditHandle
CScope_Impl::x_AttachEntry(const CBioseq_set_EditHandle& seqset,
                           CRef<CSeq_entry_Info> entry,
                           int index)
{
    _ASSERT(seqset && bool(entry));

    TWriteLockGuard guard(m_Scope_Conf_RWLock);

    //seqset.GetDataSource().AttachEntry(seqset, entry, index);
    seqset.x_GetInfo().AddEntry(entry, index);

    x_ClearCacheOnNewData();

    return CSeq_entry_EditHandle(*m_HeapScope, *entry, seqset.GetTSE_Lock());
}


CBioseq_EditHandle
CScope_Impl::x_SelectSeq(const CSeq_entry_EditHandle& entry,
                         CRef<CBioseq_Info> bioseq)
{
    _ASSERT(entry && bool(bioseq));
    _ASSERT(entry.Which() == CSeq_entry::e_not_set);

    TWriteLockGuard guard(m_Scope_Conf_RWLock);

    // duplicate bioseq info
    entry.x_GetInfo().SelectSeq(*bioseq);

    x_ClearCacheOnNewData();

    return GetBioseqHandle(*bioseq, entry.GetTSE_Lock()).GetEditHandle();
}


CBioseq_set_EditHandle
CScope_Impl::x_SelectSet(const CSeq_entry_EditHandle& entry,
                         CRef<CBioseq_set_Info> seqset)
{
    _ASSERT(entry && bool(seqset));
    _ASSERT(entry.Which() == CSeq_entry::e_not_set);

    TWriteLockGuard guard(m_Scope_Conf_RWLock);

    // duplicate bioseq info
    entry.x_GetInfo().SelectSet(*seqset);

    x_ClearCacheOnNewData();

    return CBioseq_set_EditHandle(*m_HeapScope, *seqset, entry.GetTSE_Lock());
}


CSeq_annot_EditHandle
CScope_Impl::x_AttachAnnot(const CSeq_entry_EditHandle& entry,
                           CRef<CSeq_annot_Info> annot)
{
    _ASSERT(entry && bool(annot));

    TWriteLockGuard guard(m_Scope_Conf_RWLock);

    entry.x_GetInfo().AddAnnot(annot);

    x_ClearAnnotCache();

    return CSeq_annot_EditHandle(*m_HeapScope, *annot, entry.GetTSE_Lock());
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
    return x_AttachEntry(seqset, Ref(new CSeq_entry_Info(entry.x_GetInfo())),
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
    return x_SelectSeq(entry, Ref(new CBioseq_Info(seq.x_GetInfo())));
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
    return x_SelectSet(entry, Ref(new CBioseq_set_Info(seqset.x_GetInfo())));
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
    return x_AttachAnnot(entry, Ref(new CSeq_annot_Info(annot.x_GetInfo())));
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
            "adding new data to a scope with non-empty cache "
            "may cause the cached data to become inconsistent");
    }
    NON_CONST_ITERATE ( TSeq_idMap, it, m_Seq_idMap ) {
        if ( it->second.m_Bioseq_Info &&
             !it->second.m_Bioseq_Info->HasBioseq() ) {
            it->second.m_Bioseq_Info->m_SynCache.Reset();
            it->second.m_Bioseq_Info.Reset();
        }
        it->second.m_AllAnnotRef_Info.Reset();
    }
}


void CScope_Impl::x_ClearAnnotCache(void)
{
    // Clear annot cache
    NON_CONST_ITERATE ( TSeq_idMap, it, m_Seq_idMap ) {
        it->second.m_AllAnnotRef_Info.Reset();
    }
}


void CScope_Impl::x_ClearCacheOnRemoveData(const CSeq_entry_Info& info)
{
    if ( info.IsSeq() ) {
        x_ClearCacheOnRemoveData(info.GetSeq());
    }
    else {
        x_ClearCacheOnRemoveData(info.GetSet());
    }
}


void CScope_Impl::x_ClearCacheOnRemoveData(const CBioseq_Info& seq)
{
    // Clear bioseq from the scope cache
    ITERATE ( CBioseq_Info::TId, si, seq.GetId() ) {
        TSeq_idMap::iterator bs_id = m_Seq_idMap.find(*si);
        if (bs_id != m_Seq_idMap.end()) {
            if ( bs_id->second.m_Bioseq_Info ) {
                // detaching from scope
                bs_id->second.m_Bioseq_Info->m_ScopeInfo = 0;
                // breaking the link
                bs_id->second.m_Bioseq_Info->m_SynCache.Reset();
            }
            m_Seq_idMap.erase(bs_id);
        }
        m_BioseqMap.erase(&seq);
    }
}


void CScope_Impl::x_ClearCacheOnRemoveData(const CBioseq_set_Info& seqset)
{
    ITERATE( CBioseq_set_Info::TSeq_set, ei, seqset.GetSeq_set() ) {
        x_ClearCacheOnRemoveData(**ei);
    }
}


CSeq_entry_Handle CScope_Impl::GetSeq_entryHandle(const CSeq_entry& entry)
{
    TReadLockGuard guard(m_Scope_Conf_RWLock);
    TSeq_entry_Lock lock = x_GetSeq_entry_Lock(entry);
    return CSeq_entry_Handle(*m_HeapScope, *lock.first, lock.second);
}


CSeq_entry_Handle CScope_Impl::GetSeq_entryHandle(const CTSE_Lock& tse_lock)
{
    return CSeq_entry_Handle(*m_HeapScope, *tse_lock, tse_lock);
}


CSeq_annot_Handle CScope_Impl::GetSeq_annotHandle(const CSeq_annot& annot)
{
    TReadLockGuard guard(m_Scope_Conf_RWLock);
    TSeq_annot_Lock lock = x_GetSeq_annot_Lock(annot);
    return CSeq_annot_Handle(*m_HeapScope, *lock.first, lock.second);
}


CBioseq_Handle CScope_Impl::GetBioseqHandle(const CBioseq& seq)
{
    CBioseq_Handle ret;
    {{
        TReadLockGuard guard(m_Scope_Conf_RWLock);
        TBioseq_Lock lock = x_GetBioseq_Lock(seq);
        ITERATE ( CBioseq_Info::TId, id, lock.first->GetId() ) {
            ret = x_GetBioseqHandleFromTSE(*id, lock.second);
            if ( ret ) {
                _ASSERT(ret.GetBioseqCore() == &seq);
                break;
            }
        }
    }}
    return ret;
}


TTSE_Lock CScope_Impl::x_GetTSE_Lock(const CSeq_entry& tse)
{
    for (CPriority_I it(m_setDataSrc); it; ++it) {
        CFastMutexGuard guard(it->GetMutex());
        TTSE_Lock lock =
            it->GetDataSource().FindTSE_Lock(tse, it->GetTSESet());
        if ( lock ) {
            /*
            {{
                CFastMutexGuard guard(it->GetMutex());
                it->AddTSE(lock);
            }}
            */
            _ASSERT(it->GetTSESet().find(lock)!=it->GetTSESet().end());
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
        CFastMutexGuard guard(it->GetMutex());
        TSeq_entry_Lock lock =
            it->GetDataSource().FindSeq_entry_Lock(entry, it->GetTSESet());
        if ( lock.first ) {
            /*
            {{
                CFastMutexGuard guard(it->GetMutex());
                it->AddTSE(lock.second);
            }}
            */
            _ASSERT(it->GetTSESet().find(lock.second)!=it->GetTSESet().end());
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
        CFastMutexGuard guard(it->GetMutex());
        TSeq_annot_Lock lock =
            it->GetDataSource().FindSeq_annot_Lock(annot, it->GetTSESet());
        if ( lock.first ) {
            /*
            {{
                CFastMutexGuard guard(it->GetMutex());
                it->AddTSE(lock.second);
            }}
            */
            _ASSERT(it->GetTSESet().find(lock.second)!=it->GetTSESet().end());
            return lock;
        }
    }
    NCBI_THROW(CObjMgrException, eFindFailed,
               "CScope_Impl::x_GetSeq_annot_Lock: annot is not attached");
}


CScope_Impl::TBioseq_Lock CScope_Impl::x_GetBioseq_Lock(const CBioseq& bioseq)
{
    for (CPriority_I it(m_setDataSrc); it; ++it) {
        CFastMutexGuard guard(it->GetMutex());
        TBioseq_Lock lock =
            it->GetDataSource().FindBioseq_Lock(bioseq, it->GetTSESet());
        if ( lock.first ) {
            /*
            {{
                CFastMutexGuard guard(it->GetMutex());
                it->AddTSE(lock.second);
            }}
            */
            _ASSERT(it->GetTSESet().find(lock.second)!=it->GetTSESet().end());
            return lock;
        }
    }
    NCBI_THROW(CObjMgrException, eFindFailed,
               "CScope_Impl::x_GetBioseq_Lock: bioseq is not attached");
}


void CScope_Impl::RemoveAnnot(const CSeq_annot_EditHandle& annot)
{
    TWriteLockGuard guard(m_Scope_Conf_RWLock);

    CSeq_annot_Info& info = annot.x_GetInfo();
    info.GetDataSource().RemoveAnnot(info);

    x_ClearAnnotCache();
}


void CScope_Impl::SelectNone(const CSeq_entry_EditHandle& entry)
{
    _ASSERT(entry);

    TWriteLockGuard guard(m_Scope_Conf_RWLock);

    CSeq_entry_Info& info = entry.x_GetInfo();
    x_ClearAnnotCache();
    switch ( info.Which() ) {
    case CSeq_entry::e_Set:
        x_ClearCacheOnRemoveData(info.GetSet());
        break;
    case CSeq_entry::e_Seq:
        x_ClearCacheOnRemoveData(info.GetSeq());
        break;
    default:
        break;
    }
    // duplicate bioseq info
    info.Reset();
}


void CScope_Impl::RemoveEntry(const CSeq_entry_EditHandle& entry)
{
    TWriteLockGuard guard(m_Scope_Conf_RWLock);

    CSeq_entry_Info& info = entry.x_GetInfo();
    x_ClearAnnotCache();
    x_ClearCacheOnRemoveData(info);

    if ( entry.GetParentEntry() ) {
        info.GetDataSource().RemoveEntry(info);
    }
    else {
        // top level entry
        CDataSource_ScopeInfo* ds_info = 0;
        for (CPriority_I it(m_setDataSrc); it; ++it) {
            if ( &it->GetDataSource() == &info.GetDataSource() ) {
                ds_info = &*it;
                break;
            }
        }
        if ( ds_info ) {
            m_setDataSrc.Erase(*ds_info);
        }
        else {
            // not found...
            NCBI_THROW(CObjMgrException, eModifyDataError,
                       "CScope::RemoveEntry(): cannot find data source");
        }
    }
}


void CScope_Impl::RemoveBioseq(const CBioseq_EditHandle& seq)
{
    TWriteLockGuard guard(m_Scope_Conf_RWLock);

    CBioseq_Info& info = seq.x_GetInfo();
    x_ClearAnnotCache();
    x_ClearCacheOnRemoveData(info);

    info.GetParentSeq_entry_Info().Reset();
}


void CScope_Impl::RemoveBioseq_set(const CBioseq_set_EditHandle& seqset)
{
    TWriteLockGuard guard(m_Scope_Conf_RWLock);

    CBioseq_set_Info& info = seqset.x_GetInfo();
    x_ClearAnnotCache();
    x_ClearCacheOnRemoveData(info);

    info.GetParentSeq_entry_Info().Reset();
}


CScope_Impl::TSeq_idMapValue&
CScope_Impl::x_GetSeq_id_Info(const CSeq_id_Handle& id)
{
    {{
        TReadLockGuard guard(m_Seq_idMapLock);
        TSeq_idMap::iterator it = m_Seq_idMap.lower_bound(id);
        if ( it != m_Seq_idMap.end() && it->first == id )
            return *it;
    }}
    {{
        TWriteLockGuard guard(m_Seq_idMapLock);
        return *m_Seq_idMap.insert(
            TSeq_idMapValue(id, SSeq_id_ScopeInfo(m_HeapScope))).first;
    }}
}


inline
CScope_Impl::TSeq_idMapValue&
CScope_Impl::x_GetSeq_id_Info(const CBioseq_Handle& bh)
{
    TSeq_idMapValue* info = bh.x_GetScopeInfo().m_ScopeInfo;
    if ( info->first != bh.GetSeq_id_Handle() ) {
        info = &x_GetSeq_id_Info(bh.GetSeq_id_Handle());
    }
    return *info;
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
CScope_Impl::x_InitBioseq_Info(TSeq_idMapValue& info, int get_flag)
{
    if (get_flag != CScope::eGetBioseq_Resolved) {
        // Resolve only if the flag allows
        CInitGuard init(info.second.m_Bioseq_Info, m_MutexPool);
        if ( init ) {
            x_ResolveSeq_id(info, get_flag);
        }
    }
    //_ASSERT(info.second.m_Bioseq_Info);
    return info.second.m_Bioseq_Info;
}


bool CScope_Impl::x_InitBioseq_Info(TSeq_idMapValue& info,
                                    CBioseq_ScopeInfo& bioseq_info)
{
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
CScope_Impl::x_GetBioseq_Info(const CSeq_id_Handle& id, int get_flag)
{
    return x_InitBioseq_Info(x_GetSeq_id_Info(id), get_flag);
}


CRef<CBioseq_ScopeInfo>
CScope_Impl::x_FindBioseq_Info(const CSeq_id_Handle& id, int get_flag)
{
    CRef<CBioseq_ScopeInfo> ret;
    TSeq_idMapValue* info = x_FindSeq_id_Info(id);
    if ( info ) {
        ret = x_InitBioseq_Info(*info, get_flag);
    }
    return ret;
}


CBioseq_Handle CScope_Impl::x_GetBioseqHandleFromTSE(const CSeq_id_Handle& id,
                                                     const TTSE_Lock& tse)
{
    CBioseq_Handle ret;
    TReadLockGuard rguard(m_Scope_Conf_RWLock);
    CRef<CBioseq_ScopeInfo> info =
        x_FindBioseq_Info(id, CScope::eGetBioseq_Loaded);
    if ( bool(info)  &&  info->HasBioseq() ) {
        if ( info->GetTSE_Lock() == tse ) {
            ret.m_Bioseq_Info = info.GetPointer();
        }
    }
    else {
        // new bioseq - try to find it in source TSE
        {{
            // first try main id
            CSeqMatch_Info match(id, tse);
            CConstRef<CBioseq_Info> bioseq = match.GetBioseq_Info();
            if ( bioseq ) {
                _ASSERT(m_HeapScope);
                CBioseq_Handle bh =
                    GetBioseqHandle(id, CScope::eGetBioseq_Loaded);
                if ( bh && bh.GetTSE_Lock() == tse ) {
                    ret = bh;
                    return ret;
                }
            }
        }}
        
        CSeq_id_Mapper& mapper = *CSeq_id_Mapper::GetInstance();
        if ( mapper.HaveMatchingHandles(id) ) {
            // than try matching handles
            TSeq_id_HandleSet hset;
            mapper.GetMatchingHandles(id, hset);
            ITERATE ( TSeq_id_HandleSet, hit, hset ) {
                if ( *hit == id ) // already checked
                    continue;
                CSeqMatch_Info match(*hit, tse);
                CConstRef<CBioseq_Info> bioseq = match.GetBioseq_Info();
                if ( bioseq ) {
                    _ASSERT(m_HeapScope);
                    CBioseq_Handle bh = GetBioseqHandle(*hit,
                        CScope::eGetBioseq_Loaded);
                    if ( bh && bh.GetTSE_Lock() == tse ) {
                        ret = bh;
                        break;
                    }
                }
            }
        }
    }
    ret.m_Seq_id = id;
    ret.m_Scope.Set(m_HeapScope);
    return ret;
}


CBioseq_Handle CScope_Impl::GetBioseqHandle(const CSeq_id_Handle& id,
                                            int get_flag)
{
    CBioseq_Handle ret;
    if ( id )  {
        {{
            TReadLockGuard rguard(m_Scope_Conf_RWLock);
            ret.m_Bioseq_Info = x_GetBioseq_Info(id, get_flag).GetPointer();
        }}
        if ( bool(ret.m_Bioseq_Info) && !ret.x_GetScopeInfo().HasBioseq() ) {
            ret.m_Bioseq_Info.Reset();
        }
        ret.m_Seq_id = id;
    }
    ret.m_Scope.Set(m_HeapScope);
    return ret;
}


CBioseq_EditHandle CScope_Impl::GetEditHandle(const CBioseq_Handle& h)
{
    if ( !h ) {
        NCBI_THROW(CObjMgrException, eInvalidHandle,
                   "CScope::GetEditHandle: null handle");
    }
    
    if ( h.x_GetInfo().GetDataSource().GetDataLoader() ) {
        NCBI_THROW(CObjMgrException, eNotImplemented,
                   "CScope::GetEditHandle: detach is not implemented");
    }
    
    return CBioseq_EditHandle(h);
}


CSeq_entry_EditHandle CScope_Impl::GetEditHandle(const CSeq_entry_Handle& h)
{
    if ( !h ) {
        NCBI_THROW(CObjMgrException, eInvalidHandle,
                   "CScope::GetEditHandle: null handle");
    }
    
    if ( h.x_GetInfo().GetDataSource().GetDataLoader() ) {
        NCBI_THROW(CObjMgrException, eNotImplemented,
                   "CScope::GetEditHandle: detach is not implemented");
    }
    
    return CSeq_entry_EditHandle(h);
}


CSeq_annot_EditHandle CScope_Impl::GetEditHandle(const CSeq_annot_Handle& h)
{
    if ( !h ) {
        NCBI_THROW(CObjMgrException, eInvalidHandle,
                   "CScope::GetEditHandle: null handle");
    }
    
    if ( h.x_GetInfo().GetDataSource().GetDataLoader() ) {
        NCBI_THROW(CObjMgrException, eNotImplemented,
                   "CScope::GetEditHandle: detach is not implemented");
    }
    
    return CSeq_annot_EditHandle(h);
}


CBioseq_set_EditHandle CScope_Impl::GetEditHandle(const CBioseq_set_Handle& h)
{
    if ( !h ) {
        NCBI_THROW(CObjMgrException, eInvalidHandle,
                   "CScope::GetEditHandle: null handle");
    }
    
    if ( h.x_GetInfo().GetDataSource().GetDataLoader() ) {
        NCBI_THROW(CObjMgrException, eNotImplemented,
                   "CScope::GetEditHandle: detach is not implemented");
    }
    
    return CBioseq_set_EditHandle(h);
}


CBioseq_Handle CScope_Impl::GetBioseqHandle(const CSeq_id& id, int get_flag)
{
    return GetBioseqHandle(CSeq_id_Handle::GetHandle(id), get_flag);
}


CBioseq_Handle
CScope_Impl::GetBioseqHandleFromTSE(const CSeq_id_Handle& id,
                                    const CBioseq_Handle& bh)
{
    CBioseq_Handle ret;
    if ( bh ) {
        ret = x_GetBioseqHandleFromTSE(id, bh.GetTSE_Lock());
    }
    else {
        ret.m_Seq_id = id;
        ret.m_Scope.Set(m_HeapScope);
    }
    return ret;
}


CBioseq_Handle
CScope_Impl::GetBioseqHandleFromTSE(const CSeq_id_Handle& id,
                                    const CSeq_entry_Handle& seh)
{
    CBioseq_Handle ret;
    if ( seh ) {
        ret = x_GetBioseqHandleFromTSE(id, seh.GetTSE_Lock());
    }
    else {
        ret.m_Seq_id = id;
        ret.m_Scope.Set(m_HeapScope);
    }
    return ret;
}


CBioseq_Handle
CScope_Impl::GetBioseqHandleFromTSE(const CSeq_id& id,
                                    const CBioseq_Handle& bh)
{
    return GetBioseqHandleFromTSE(CSeq_id_Handle::GetHandle(id), bh);
}


CBioseq_Handle
CScope_Impl::GetBioseqHandleFromTSE(const CSeq_id& id,
                                    const CSeq_entry_Handle& seh)
{
    return GetBioseqHandleFromTSE(CSeq_id_Handle::GetHandle(id), seh);
}


CBioseq_Handle CScope_Impl::GetBioseqHandle(const CSeq_loc& loc, int get_flag)
{
    CBioseq_Handle bh;
    for (CSeq_loc_CI citer (loc); citer; ++citer) {
        bh = GetBioseqHandle(citer.GetSeq_id(), get_flag);
        if ( bh ) {
            break;
        }
    }
    return bh;
}


CBioseq_Handle CScope_Impl::GetBioseqHandle(const CBioseq_Info& seq,
                                            const TTSE_Lock& tse_lock)
{
    CBioseq_Handle ret;
    {{
        TReadLockGuard guard(m_Scope_Conf_RWLock);
        ret = x_GetBioseqHandle(seq, tse_lock);
    }}
    return ret;
}


CBioseq_Handle CScope_Impl::x_GetBioseqHandle(const CBioseq_Info& seq,
                                              const TTSE_Lock& tse_lock)
{
    CBioseq_Handle ret;
    ITERATE ( CBioseq_Info::TId, id, seq.GetId() ) {
        CBioseq_Handle bh = x_GetBioseqHandleFromTSE(*id, tse_lock);
        if ( bh && &bh.x_GetInfo() == &seq ) {
            ret = bh;
            break;
        }
    }
    return ret;
}


CDataSource_ScopeInfo*
CScope_Impl::x_FindBioseqInfo(const CPriorityTree& tree,
                              const CSeq_id_Handle& idh,
                              const TSeq_id_HandleSet* hset,
                              CSeqMatch_Info& match_info,
                              int get_flag)
{
    CDataSource_ScopeInfo* ret = 0;
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
        CDataSource_ScopeInfo* new_ret =
            x_FindBioseqInfo(mit->second, idh, hset, match_info, get_flag);
        if ( new_ret ) {
            ret = new_ret;
        }
    }
    return ret;
}


bool CScope_Impl::x_HaveResolvedSynonym(CSeqMatch_Info& info)
{
    const CBioseq_Info::TId& syns = info.GetBioseq_Info()->GetId();
    ITERATE(CBioseq_Info::TId, syn_it, syns) {
        CRef<CBioseq_ScopeInfo> res_info =
            x_FindBioseq_Info(*syn_it, CScope::eGetBioseq_Resolved);
        if (bool(res_info)  &&
            (&res_info->GetBioseq_Info() == info.GetBioseq_Info())) {
            return true;
        }
    }
    return false;
}


CDataSource_ScopeInfo*
CScope_Impl::x_FindBioseqInfo(CDataSource_ScopeInfo& ds_info,
                              const CSeq_id_Handle& main_idh,
                              const TSeq_id_HandleSet* hset,
                              CSeqMatch_Info& match_info,
                              int get_flag)
{
    // skip already matched CDataSource
    CDataSource& ds = ds_info.GetDataSource();
    if ( match_info && &match_info.GetTSE_Lock()->GetDataSource() == &ds ) {
        return 0;
    }
    CSeqMatch_Info info;
    {{
        CFastMutexGuard guard(ds_info.GetMutex());
        info = ds.HistoryResolve(main_idh, ds_info.GetTSESet());
        if ( !info && hset ) {
            ITERATE(TSeq_id_HandleSet, hit, *hset) {
                if ( *hit == main_idh ) // already checked
                    continue;
                if ( info  &&  info.GetIdHandle().IsBetter(*hit) ) // worse hit
                    continue;
                CSeqMatch_Info new_info =
                    ds.HistoryResolve(*hit, ds_info.GetTSESet());
                if ( !new_info )
                    continue;
                _ASSERT(&new_info.GetTSE_Lock()->GetDataSource() == &ds);
                if ( !info ) {
                    info = new_info;
                    continue;
                }
                CSeqMatch_Info* best_info =
                    ds.ResolveConflict(main_idh, info, new_info);
                if (best_info) {
                    info = *best_info;
                    continue;
                }
                x_ThrowConflict(eConflict_History, info, new_info);
            }
        }
    }}
    if ( !info  &&  get_flag == CScope::eGetBioseq_All ) {
        // Try to load the sequence from the data source
        info = ds_info.GetDataSource().BestResolve(main_idh);
    }
    if ( info ) {
        if ( match_info ) {
            bool resolved1 = x_HaveResolvedSynonym(match_info);
            bool resolved2 = x_HaveResolvedSynonym(info);
            if (resolved1  ==  resolved2) {
                x_ThrowConflict(eConflict_Live, match_info, info);
            }
            if ( resolved2 ) {
                // info has resolved synonym, replace match_info
                match_info = info;
            }
        }
        else {
            match_info = info;
        }
        return &ds_info;
    }
    return 0;
}


CDataSource_ScopeInfo*
CScope_Impl::x_FindBioseqInfo(const CPriorityNode& node,
                              const CSeq_id_Handle& idh,
                              const TSeq_id_HandleSet* hset,
                              CSeqMatch_Info& match_info,
                              int get_flag)
{
    if ( node.IsTree() ) {
        // Process sub-tree
        return x_FindBioseqInfo(node.GetTree(), idh, hset, match_info, get_flag);
    }
    else if ( node.IsLeaf() ) {
        return x_FindBioseqInfo(const_cast<CDataSource_ScopeInfo&>(node.GetLeaf()),
                                idh, hset, match_info, get_flag);
    }
    return 0;
}


void CScope_Impl::x_ResolveSeq_id(TSeq_idMapValue& id_info, int get_flag)
{
    // Use priority, do not scan all DSs - find the first one.
    // Protected by m_Scope_Conf_RWLock in upper-level functions
    CSeqMatch_Info match_info;
    auto_ptr<TSeq_id_HandleSet> hset;
    CSeq_id_Mapper& mapper = *CSeq_id_Mapper::GetInstance();
    if ( mapper.HaveMatchingHandles(id_info.first) ) {
        hset.reset(new TSeq_id_HandleSet);
        mapper.GetMatchingHandles(id_info.first, *hset);
        hset->erase(id_info.first);
        if ( hset->empty() )
            hset.reset();
    }
    CDataSource_ScopeInfo* ds_info =
        x_FindBioseqInfo(m_setDataSrc, id_info.first, hset.get(), match_info,
        get_flag);
    if ( !ds_info ) {
        // Map unresoved ids only if loading was requested
        if (get_flag == CScope::eGetBioseq_All) {
            _ASSERT(m_HeapScope);
            id_info.second.m_Bioseq_Info.Reset(new CBioseq_ScopeInfo(&id_info));
        }
    }
    else {
        {{
            CFastMutexGuard guard(ds_info->GetMutex());
            ds_info->AddTSE(match_info.GetTSE_Lock());
        }}

        CConstRef<CBioseq_Info> info = match_info.GetBioseq_Info();
        {{
            TReadLockGuard guard(m_BioseqMapLock);
            TBioseqMap::const_iterator bm_it = m_BioseqMap.find(&*info);
            if ( bm_it != m_BioseqMap.end() ) {
                id_info.second.m_Bioseq_Info = bm_it->second;
                return;
            }
        }}
        {{
            TWriteLockGuard guard(m_BioseqMapLock);
            pair<TBioseqMap::iterator, bool> ins = m_BioseqMap
                .insert(TBioseqMap::value_type(&*info,
                                               CRef<CBioseq_ScopeInfo>()));
            if ( ins.second ) { // new
                _ASSERT(m_HeapScope);
                ins.first->second.Reset(
                    new CBioseq_ScopeInfo(&id_info, info,
                                          match_info.GetTSE_Lock()));
            }
            id_info.second.m_Bioseq_Info = ins.first->second;
        }}
    }
}


void CScope_Impl::UpdateAnnotIndex(const CSeq_annot& annot)
{
    UpdateAnnotIndex(GetSeq_annotHandle(annot));
}


void CScope_Impl::UpdateAnnotIndex(const CSeq_annot_Handle& annot)
{
    TReadLockGuard rguard(m_Scope_Conf_RWLock);
    const CSeq_annot_Info& info = annot.x_GetInfo();
    info.GetDataSource().UpdateAnnotIndex(info);
}


CConstRef<CScope_Impl::TAnnotRefMap>
CScope_Impl::GetTSESetWithAnnots(const CSeq_id_Handle& idh)
{
    TReadLockGuard rguard(m_Scope_Conf_RWLock);
    TSeq_idMapValue& info = x_GetSeq_id_Info(idh);
    CRef<CBioseq_ScopeInfo> binfo = x_InitBioseq_Info(info,
        CScope::eGetBioseq_All);
    {{
        CInitGuard init(info.second.m_AllAnnotRef_Info, m_MutexPool);
        if ( init ) {
            CRef<TAnnotRefMap> ref_map(new TAnnotRefMap);
            TTSE_LockMap with_ref;
	    if ( binfo->HasBioseq() ) {
		const CBioseq_Info::TId& syns =
		    binfo->GetBioseq_Info().GetId();
		ITERATE(CBioseq_Info::TId, syn_it, syns) {
		    // Search current id
		    GetTSESetWithAnnotsRef(*binfo, *syn_it, *ref_map);
		}
	    }
	    else {
		GetTSESetWithAnnotsRef(*binfo, idh, *ref_map);
	    }
	    info.second.m_AllAnnotRef_Info = ref_map;
        }
    }}
    return info.second.m_AllAnnotRef_Info;
}


void
CScope_Impl::GetTSESetWithAnnotsRef(const CBioseq_ScopeInfo& binfo,
                                    const CSeq_id_Handle& idh,
                                    TAnnotRefMap& tse_map)
{
    TSeq_id_HandleSet idh_set;
    CSeq_id_Mapper& id_mapper = *CSeq_id_Mapper::GetInstance();
    if (id_mapper.HaveReverseMatch(idh)) {
        id_mapper.GetReverseMatchingHandles(idh, idh_set);
    }
    else {
        idh_set.insert(idh);
    }
    ITERATE(TSeq_id_HandleSet, match_it, idh_set) {
        if ( binfo.HasBioseq() ) {
            tse_map.GetData()[binfo.GetTSE_Lock()].insert(*match_it);
        }
        // Search in all data sources
        for (CPriority_I it(m_setDataSrc); it; ++it) {
            CFastMutexGuard guard(it->GetMutex());
            const TTSE_LockSet& history = it->GetTSESet();
            TTSE_LockSet tse_set =
                it->GetDataSource().GetTSESetWithAnnots(*match_it, history);
            ITERATE(TTSE_LockSet, ref_it, tse_set) {
                if ( (*ref_it)->IsDead() &&
                     history.find(*ref_it) == history.end() ) {
                    continue;
                }
                it->AddTSE(*ref_it);
                tse_map.GetData()[*ref_it].insert(*match_it);
            }
        }
    }
}


void CScope_Impl::x_ThrowConflict(EConflict conflict_type,
                                  const CSeqMatch_Info& info1,
                                  const CSeqMatch_Info& info2) const
{
    const char* msg_type =
        conflict_type == eConflict_History? "history": "live TSE";
    CNcbiOstrstream s;
    s << "CScope_Impl -- multiple " << msg_type << " matches: " <<
        info1.GetTSE_Lock()->GetDataSource().GetName() << "::" <<
        info1.GetIdHandle().AsString() <<
        " vs " <<
        info2.GetTSE_Lock()->GetDataSource().GetName() << "::" <<
        info2.GetIdHandle().AsString();
    string msg = CNcbiOstrstreamToString(s);
    NCBI_THROW(CObjMgrException, eFindConflict, msg);
}


void CScope_Impl::ResetHistory(void)
{
    TWriteLockGuard guard(m_Scope_Conf_RWLock);
    x_ResetHistory();
}


void CScope_Impl::x_ResetHistory(void)
{
    // 1. detach all CBbioseq_Handle objects from scope, and
    // 2. break circular link:
    // CBioseq_ScopeInfo-> CSynonymsSet-> SSeq_id_ScopeInfo-> CBioseq_ScopeInfo
    NON_CONST_ITERATE ( TSeq_idMap, it, m_Seq_idMap ) {
        if ( it->second.m_Bioseq_Info ) {
            it->second.m_Bioseq_Info->m_ScopeInfo = 0; // detaching from scope
            it->second.m_Bioseq_Info->m_SynCache.Reset(); // breaking the link
        }
    }
    m_Seq_idMap.clear();
    m_BioseqMap.clear();
    for (CPriority_I it(m_setDataSrc); it; ++it) {
        it->m_TSE_LockSet.clear();
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
            CBioseq_Handle bh = x_GetBioseqHandle(**iit, seh.GetTSE_Lock());
            if ( bh ) {
                handles.push_back(bh);
            }
        }
    }
}


CConstRef<CSynonymsSet> CScope_Impl::GetSynonyms(const CSeq_id& id)
{
    return GetSynonyms(CSeq_id_Handle::GetHandle(id));
}


CConstRef<CSynonymsSet> CScope_Impl::GetSynonyms(const CSeq_id_Handle& id)
{
    _ASSERT(id);
    TReadLockGuard rguard(m_Scope_Conf_RWLock);
    return x_GetSynonyms(*x_GetBioseq_Info(id, CScope::eGetBioseq_All));
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
        LOG_POST(Warning << "CScope::GetSynonyms: Bioseq["<<
                 info.GetBioseq_Info().IdString()<<"]: id "<<
                 idh.AsString()<<" is resolved to another Bioseq["<<
                 info2->GetBioseq_Info().IdString()<<"]");
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
                ITERATE(CBioseq_Info::TId, it, info.GetBioseq_Info().GetId()) {
                    CSeq_id_Mapper& mapper =
                        *CSeq_id_Mapper::GetInstance();
                    if ( mapper.HaveReverseMatch(*it) ) {
                        TSeq_id_HandleSet hset;
                        mapper.GetReverseMatchingHandles(*it, hset);
                        ITERATE(TSeq_id_HandleSet, mit, hset) {
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
        CFastMutexGuard guard(it->GetMutex());
        const TTSE_LockSet& history = it->GetTSESet();
        ITERATE(TTSE_LockSet, tse_it, history) {
            tses.push_back(CSeq_entry_Handle(*m_HeapScope, **tse_it, *tse_it));
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


void CScope_Impl::DebugDump(CDebugDumpContext ddc, unsigned int depth) const
{
    ddc.SetFrame("CScope_Impl");
    CObject::DebugDump( ddc, depth);

    ddc.Log("m_pObjMgr", m_pObjMgr,0);
    if (depth == 0) {
    } else {
        DebugDumpValue(ddc,"m_setDataSrc.type", "set<CDataSource*>");
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.22  2004/08/13 16:13:49  vasilche
* Use safe method to lock TSEs with fat annotations (from scope's history).
*
* Revision 1.21  2004/08/04 14:53:26  vasilche
* Revamped object manager:
* 1. Changed TSE locking scheme
* 2. TSE cache is maintained by CDataSource.
* 3. CObjectManager::GetInstance() doesn't hold CRef<> on the object manager.
* 4. Fixed processing of split data.
*
* Revision 1.20  2004/07/12 15:05:32  grichenk
* Moved seq-id mapper from xobjmgr to seq library
*
* Revision 1.19  2004/06/23 19:50:52  vasilche
* Fixed null pointer access in GetTSESetWithAnnots() when CBioseq is not found.
*
* Revision 1.18  2004/06/15 14:02:22  vasilche
* Commented out unused local variable.
*
* Revision 1.17  2004/06/10 16:21:27  grichenk
* Changed CSeq_id_Mapper singleton type to pointer, GetSeq_id_Mapper
* returns CRef<> which is locked by CObjectManager.
*
* Revision 1.16  2004/06/03 18:33:48  grichenk
* Modified annot collector to better resolve synonyms
* and matching IDs. Do not add id to scope history when
* collecting annots. Exclude TSEs with bioseqs from data
* source's annot index.
*
* Revision 1.15  2004/05/21 21:42:12  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.14  2004/04/16 13:31:47  grichenk
* Added data pre-fetching functions.
*
* Revision 1.13  2004/04/14 19:12:12  vasilche
* Added storing TSE in history when returning CSeq_entry or CSeq_annot handles.
* Added test for main id before matching handles in GetBioseqHandleFromTSE().
*
* Revision 1.12  2004/04/13 15:59:35  grichenk
* Added CScope::GetBioseqHandle() with id resolving flag.
*
* Revision 1.11  2004/04/12 19:35:59  grichenk
* Added locks in GetAllTSEs()
*
* Revision 1.10  2004/04/12 18:40:24  grichenk
* Added GetAllTSEs()
*
* Revision 1.9  2004/03/31 19:54:08  vasilche
* Fixed removal of bioseqs and bioseq-sets.
*
* Revision 1.8  2004/03/31 19:23:13  vasilche
* Fixed scope in CBioseq_Handle::GetEditHandle().
*
* Revision 1.7  2004/03/31 17:08:07  vasilche
* Implemented ConvertSeqToSet and ConvertSetToSeq.
*
* Revision 1.6  2004/03/29 20:51:19  vasilche
* Fixed ambiguity on MSVC.
*
* Revision 1.5  2004/03/29 20:13:06  vasilche
* Implemented whole set of methods to modify Seq-entry object tree.
* Added CBioseq_Handle::GetExactComplexityLevel().
*
* Revision 1.4  2004/03/25 19:27:44  vasilche
* Implemented MoveTo and CopyTo methods of handles.
*
* Revision 1.3  2004/03/24 18:30:30  vasilche
* Fixed edit API.
* Every *_Info object has its own shallow copy of original object.
*
* Revision 1.2  2004/03/16 18:09:10  vasilche
* Use GetPointer() to avoid ambiguity
*
* Revision 1.1  2004/03/16 15:47:28  vasilche
* Added CBioseq_set_Handle and set of EditHandles
*
* Revision 1.101  2004/02/19 17:23:01  vasilche
* Changed order of deletion of heap scope and scope impl objects.
* Reduce number of calls to x_ResetHistory().
*
* Revision 1.100  2004/02/10 21:15:16  grichenk
* Added reverse ID matching.
*
* Revision 1.99  2004/02/09 14:42:46  vasilche
* Temporary fix in GetSynonyms() to get accession without version.
*
* Revision 1.98  2004/02/02 14:46:43  vasilche
* Several performance fixed - do not iterate whole tse set in CDataSource.
*
* Revision 1.97  2004/01/29 20:33:28  vasilche
* Do not resolve any Seq-ids in CScope::GetSynonyms() -
* assume all not resolved Seq-id as synonym.
* Seq-id conflict messages made clearer.
*
* Revision 1.96  2004/01/28 20:50:49  vasilche
* Fixed NULL pointer exception in GetSynonyms() when matching Seq-id w/o version.
*
* Revision 1.95  2004/01/07 20:42:01  grichenk
* Fixed matching of accession to accession.version
*
* Revision 1.94  2003/12/18 16:38:07  grichenk
* Added CScope::RemoveEntry()
*
* Revision 1.93  2003/12/12 16:59:51  grichenk
* Fixed conflicts resolving (ask data source).
*
* Revision 1.92  2003/12/03 20:55:12  grichenk
* Check value returned by x_GetBioseq_Info()
*
* Revision 1.91  2003/11/21 20:33:03  grichenk
* Added GetBioseqHandleFromTSE(CSeq_id, CSeq_entry)
*
* Revision 1.90  2003/11/19 22:18:03  grichenk
* All exceptions are now CException-derived. Catch "exception" rather
* than "runtime_error".
*
* Revision 1.89  2003/11/12 15:49:39  vasilche
* Added loading annotations on non gi Seq-id.
*
* Revision 1.88  2003/10/23 13:47:27  vasilche
* Check CSeq_id_Handle for null in CScope::GetBioseqHandle().
*
* Revision 1.87  2003/10/22 14:08:15  vasilche
* Detach all CBbioseq_Handle objects from scope in CScope::ResetHistory().
*
* Revision 1.86  2003/10/22 13:54:36  vasilche
* All CScope::GetBioseqHandle() methods return 'null' CBioseq_Handle object
* instead of throwing an exception.
*
* Revision 1.85  2003/10/20 18:23:54  vasilche
* Make CScope::GetSynonyms() to skip conflicting ids.
*
* Revision 1.84  2003/10/09 13:58:21  vasilche
* Fixed conflict when the same datasource appears twice with equal priorities.
*
* Revision 1.83  2003/10/07 13:43:23  vasilche
* Added proper handling of named Seq-annots.
* Added feature search from named Seq-annots.
* Added configurable adaptive annotation search (default: gene, cds, mrna).
* Fixed selection of blobs for loading from GenBank.
* Added debug checks to CSeq_id_Mapper for easier finding lost CSeq_id_Handles.
* Fixed leaked split chunks annotation stubs.
* Moved some classes definitions in separate *.cpp files.
*
* Revision 1.82  2003/09/30 16:22:03  vasilche
* Updated internal object manager classes to be able to load ID2 data.
* SNP blobs are loaded as ID2 split blobs - readers convert them automatically.
* Scope caches results of requests for data to data loaders.
* Optimized CSeq_id_Handle for gis.
* Optimized bioseq lookup in scope.
* Reduced object allocations in annotation iterators.
* CScope is allowed to be destroyed before other objects using this scope are
* deleted (feature iterators, bioseq handles etc).
* Optimized lookup for matching Seq-ids in CSeq_id_Mapper.
* Added 'adaptive' option to objmgr_demo application.
*
* Revision 1.81  2003/09/05 20:50:26  grichenk
* +AddBioseq(CBioseq&)
*
* Revision 1.80  2003/09/05 17:29:40  grichenk
* Structurized Object Manager exceptions
*
* Revision 1.79  2003/09/03 20:00:02  grichenk
* Added sequence filtering by level (mains/parts/all)
*
* Revision 1.78  2003/08/04 17:03:01  grichenk
* Added constructors to iterate all annotations from a
* seq-entry or seq-annot.
*
* Revision 1.77  2003/07/25 15:25:25  grichenk
* Added CSeq_annot_CI class
*
* Revision 1.76  2003/07/17 20:07:56  vasilche
* Reduced memory usage by feature indexes.
* SNP data is loaded separately through PUBSEQ_OS.
* String compression for SNP data.
*
* Revision 1.75  2003/07/14 21:13:59  grichenk
* Added seq-loc dump in GetBioseqHandle(CSeqLoc)
*
* Revision 1.74  2003/06/30 18:42:10  vasilche
* CPriority_I made to use less memory allocations/deallocations.
*
* Revision 1.73  2003/06/24 14:25:18  vasilche
* Removed obsolete CTSE_Guard class.
* Used separate mutexes for bioseq and annot maps.
*
* Revision 1.72  2003/06/19 18:23:46  vasilche
* Added several CXxx_ScopeInfo classes for CScope related information.
* CBioseq_Handle now uses reference to CBioseq_ScopeInfo.
* Some fine tuning of locking in CScope.
*
* Revision 1.69  2003/05/27 19:44:06  grichenk
* Added CSeqVector_CI class
*
* Revision 1.68  2003/05/20 15:44:37  vasilche
* Fixed interaction of CDataSource and CDataLoader in multithreaded app.
* Fixed some warnings on WorkShop.
* Added workaround for memory leak on WorkShop.
*
* Revision 1.67  2003/05/14 18:39:28  grichenk
* Simplified TSE caching and filtering in CScope, removed
* some obsolete members and functions.
*
* Revision 1.66  2003/05/13 18:33:01  vasilche
* Fixed CScope::GetTSESetWithAnnots() conflict resolution.
*
* Revision 1.65  2003/05/12 19:18:29  vasilche
* Fixed locking of object manager classes in multi-threaded application.
*
* Revision 1.64  2003/05/09 20:28:03  grichenk
* Changed warnings to info
*
* Revision 1.63  2003/05/06 18:54:09  grichenk
* Moved TSE filtering from CDataSource to CScope, changed
* some filtering rules (e.g. priority is now more important
* than scope history). Added more caches to CScope.
*
* Revision 1.62  2003/04/29 19:51:13  vasilche
* Fixed interaction of Data Loader garbage collector and TSE locking mechanism.
* Made some typedefs more consistent.
*
* Revision 1.61  2003/04/24 16:12:38  vasilche
* Object manager internal structures are splitted more straightforward.
* Removed excessive header dependencies.
*
* Revision 1.60  2003/04/15 14:21:52  vasilche
* Removed unnecessary assignment.
*
* Revision 1.59  2003/04/14 21:32:18  grichenk
* Avoid passing CScope as an argument to CDataSource methods
*
* Revision 1.58  2003/04/09 16:04:32  grichenk
* SDataSourceRec replaced with CPriorityNode
* Added CScope::AddScope(scope, priority) to allow scope nesting
*
* Revision 1.57  2003/04/03 14:18:09  vasilche
* Added public GetSynonyms() method.
*
* Revision 1.56  2003/03/26 21:00:19  grichenk
* Added seq-id -> tse with annotation cache to CScope
*
* Revision 1.55  2003/03/24 21:26:45  grichenk
* Added support for CTSE_CI
*
* Revision 1.54  2003/03/21 19:22:51  grichenk
* Redesigned TSE locking, replaced CTSE_Lock with CRef<CTSE_Info>.
*
* Revision 1.53  2003/03/19 21:55:50  grichenk
* Avoid re-mapping TSEs in x_AddToHistory() if already indexed
*
* Revision 1.52  2003/03/18 14:52:59  grichenk
* Removed obsolete methods, replaced seq-id with seq-id handle
* where possible. Added argument to limit annotations update to
* a single seq-entry.
*
* Revision 1.51  2003/03/12 20:09:34  grichenk
* Redistributed members between CBioseq_Handle, CBioseq_Info and CTSE_Info
*
* Revision 1.50  2003/03/11 15:51:06  kuznets
* iterate -> ITERATE
*
* Revision 1.49  2003/03/11 14:15:52  grichenk
* +Data-source priority
*
* Revision 1.48  2003/03/10 16:55:17  vasilche
* Cleaned SAnnotSelector structure.
* Added shortcut when features are limited to one TSE.
*
* Revision 1.47  2003/03/05 20:55:29  vasilche
* Added cache cleaning in CScope::ResetHistory().
*
* Revision 1.46  2003/03/03 20:32:47  vasilche
* Removed obsolete method GetSynonyms().
*
* Revision 1.45  2003/02/28 21:54:18  grichenk
* +CSynonymsSet::empty(), removed _ASSERT() in CScope::GetSynonyms()
*
* Revision 1.44  2003/02/28 20:02:37  grichenk
* Added synonyms cache and x_GetSynonyms()
*
* Revision 1.43  2003/02/27 14:35:31  vasilche
* Splitted PopulateTSESet() by logically independent parts.
*
* Revision 1.42  2003/02/24 18:57:22  vasilche
* Make feature gathering in one linear pass using CSeqMap iterator.
* Do not use feture index by sub locations.
* Sort features at the end of gathering in one vector.
* Extracted some internal structures and classes in separate header.
* Delay creation of mapped features.
*
* Revision 1.41  2003/02/05 17:59:17  dicuccio
* Moved formerly private headers into include/objects/objmgr/impl
*
* Revision 1.40  2003/01/29 22:03:46  grichenk
* Use single static CSeq_id_Mapper instead of per-OM model.
*
* Revision 1.39  2003/01/22 20:11:54  vasilche
* Merged functionality of CSeqMapResolved_CI to CSeqMap_CI.
* CSeqMap_CI now supports resolution and iteration over sequence range.
* Added several caches to CScope.
* Optimized CSeqVector().
* Added serveral variants of CBioseqHandle::GetSeqVector().
* Tried to optimize annotations iterator (not much success).
* Rewritten CHandleRange and CHandleRangeMap classes to avoid sorting of list.
*
* Revision 1.38  2002/12/26 20:55:18  dicuccio
* Moved seq_id_mapper.hpp, tse_info.hpp, and bioseq_info.hpp -> include/ tree
*
* Revision 1.37  2002/12/26 16:39:24  vasilche
* Object manager class CSeqMap rewritten.
*
* Revision 1.36  2002/11/08 22:15:51  grichenk
* Added methods for removing/replacing annotations
*
* Revision 1.35  2002/11/08 19:43:35  grichenk
* CConstRef<> constructor made explicit
*
* Revision 1.34  2002/11/04 21:29:12  grichenk
* Fixed usage of const CRef<> and CRef<> constructor
*
* Revision 1.33  2002/11/01 05:34:32  kans
* added GetBioseqHandle taking CBioseq parameter
*
* Revision 1.32  2002/10/31 22:25:42  kans
* added GetBioseqHandle taking CSeq_loc parameter
*
* Revision 1.31  2002/10/18 19:12:40  grichenk
* Removed mutex pools, converted most static mutexes to non-static.
* Protected CSeqMap::x_Resolve() with mutex. Modified code to prevent
* dead-locks.
*
* Revision 1.30  2002/10/16 20:44:29  ucko
* *** empty log message ***
*
* Revision 1.29  2002/10/02 17:58:23  grichenk
* Added sequence type filter to CBioseq_CI
*
* Revision 1.28  2002/09/30 20:01:19  grichenk
* Added methods to support CBioseq_CI
*
* Revision 1.27  2002/08/09 14:59:00  ucko
* Restrict template <> to MIPSpro for now, as it also leads to link
* errors with Compaq's compiler.  (Sigh.)
*
* Revision 1.26  2002/08/08 19:51:24  ucko
* Omit EMPTY_TEMPLATE for GCC and KCC, as it evidently leads to link errors(!)
*
* Revision 1.25  2002/08/08 14:28:00  ucko
* Add EMPTY_TEMPLATE to explicit instantiations.
*
* Revision 1.24  2002/08/07 18:21:57  ucko
* Explicitly instantiate CMutexPool_Base<CScope>::sm_Pool
*
* Revision 1.23  2002/07/08 20:51:02  grichenk
* Moved log to the end of file
* Replaced static mutex (in CScope, CDataSource) with the mutex
* pool. Redesigned CDataSource data locking.
*
* Revision 1.22  2002/06/04 17:18:33  kimelman
* memory cleanup :  new/delete/Cref rearrangements
*
* Revision 1.21  2002/05/28 18:00:43  gouriano
* DebugDump added
*
* Revision 1.20  2002/05/14 20:06:26  grichenk
* Improved CTSE_Info locking by CDataSource and CDataLoader
*
* Revision 1.19  2002/05/06 03:28:47  vakatov
* OM/OM1 renaming
*
* Revision 1.18  2002/04/22 20:04:39  grichenk
* Fixed TSE dropping, removed commented code
*
* Revision 1.17  2002/04/17 21:09:40  grichenk
* Fixed annotations loading
*
* Revision 1.16  2002/03/28 14:02:31  grichenk
* Added scope history checks to CDataSource::x_FindBestTSE()
*
* Revision 1.15  2002/03/27 18:45:44  gouriano
* three functions made public
*
* Revision 1.14  2002/03/20 21:20:39  grichenk
* +CScope::ResetHistory()
*
* Revision 1.13  2002/02/28 20:53:32  grichenk
* Implemented attaching segmented sequence data. Fixed minor bugs.
*
* Revision 1.12  2002/02/25 21:05:29  grichenk
* Removed seq-data references caching. Increased MT-safety. Fixed typos.
*
* Revision 1.11  2002/02/21 19:27:06  grichenk
* Rearranged includes. Added scope history. Added searching for the
* best seq-id match in data sources and scopes. Updated tests.
*
* Revision 1.10  2002/02/07 21:27:35  grichenk
* Redesigned CDataSource indexing: seq-id handle -> TSE -> seq/annot
*
* Revision 1.9  2002/02/06 21:46:11  gouriano
* *** empty log message ***
*
* Revision 1.8  2002/02/05 21:46:28  gouriano
* added FindSeqid function, minor tuneup in CSeq_id_mapper
*
* Revision 1.7  2002/01/29 17:45:34  grichenk
* Removed debug output
*
* Revision 1.6  2002/01/28 19:44:49  gouriano
* changed the interface of BioseqHandle: two functions moved from Scope
*
* Revision 1.5  2002/01/23 21:59:31  grichenk
* Redesigned seq-id handles and mapper
*
* Revision 1.4  2002/01/18 17:06:29  gouriano
* renamed CScope::GetSequence to CScope::GetSeqVector
*
* Revision 1.3  2002/01/18 15:54:14  gouriano
* changed DropTopLevelSeqEntry()
*
* Revision 1.2  2002/01/16 16:25:57  gouriano
* restructured objmgr
*
* Revision 1.1  2002/01/11 19:06:22  gouriano
* restructured objmgr
*
*
* ===========================================================================
*/
