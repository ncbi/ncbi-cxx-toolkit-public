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

#include <objmgr/scope.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/seqmatch_info.hpp>
#include <objmgr/objmgr_exception.hpp>

#include <objmgr/impl/data_source.hpp>
#include <objmgr/impl/tse_info.hpp>
#include <objmgr/impl/bioseq_info.hpp>
#include <objmgr/impl/seq_annot_info.hpp>
#include <objmgr/impl/priority.hpp>
#include <objmgr/impl/synonyms.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Delta_seq.hpp>
#include <objects/seq/Seq_literal.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqset/Seq_entry.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


/////////////////////////////////////////////////////////////////////////////
//
// CScope
//
/////////////////////////////////////////////////////////////////////////////


CScope::CScope(CObjectManager& objmgr)
{
    if ( CanBeDeleted() ) {
        // this CScope object is allocated in heap
        m_Impl.Reset(new CScope_Impl(objmgr));
        m_Impl->m_HeapScope = this;
    }
    else {
        // allocate heap CScope object
        m_HeapScope.Reset(new CScope(objmgr));
        _ASSERT(m_HeapScope->CanBeDeleted());
        m_Impl = m_HeapScope->m_Impl;
        _ASSERT(m_Impl);
    }
}


CScope::~CScope(void)
{
    if ( bool(m_Impl) && m_Impl->m_HeapScope == this ) {
        m_Impl->m_HeapScope = 0;
    }
}


CConstRef<CSynonymsSet> CScope::GetSynonyms(const CSeq_id& id)
{
    return m_Impl->GetSynonyms(id);
}


CConstRef<CSynonymsSet> CScope::GetSynonyms(const CSeq_id_Handle& id)
{
    return m_Impl->GetSynonyms(id);
}


CConstRef<CSynonymsSet> CScope::GetSynonyms(const CBioseq_Handle& bh)
{
    return m_Impl->GetSynonyms(bh);
}


/////////////////////////////////////////////////////////////////////////////
//
//  CScope::
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
    // Drop and release all TSEs
    x_ResetHistory();
    if ( m_pObjMgr ) {
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


void CScope_Impl::AddTopLevelSeqEntry(CSeq_entry& top_entry,
                                      TPriority priority)
{
    CRef<CDataSource> ds = m_pObjMgr->AcquireTopLevelSeqEntry(top_entry);
    TWriteLockGuard guard(m_Scope_Conf_RWLock);
    m_setDataSrc.Insert(*ds, (priority == kPriority_NotSet) ?
        ds->GetDefaultPriority() : priority);
    x_ClearCacheOnNewData();
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
    AddTopLevelSeqEntry(*entry, priority);
    return GetBioseqHandle(bioseq);
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


void CScope_Impl::x_ClearCacheOnRemoveData(CSeq_entry_Info& info)
{
    x_ClearAnnotCache();
    if (info.m_Bioseq) {
        // Clear bioseq from the scope cache
        ITERATE(CBioseq_Info::TSynonyms, si, info.GetBioseq_Info().GetSynonyms()) {
            TSeq_idMap::iterator bs_id = m_Seq_idMap.find(*si);
            if (bs_id != m_Seq_idMap.end()) {
                if ( bs_id->second.m_Bioseq_Info ) {
                    bs_id->second.m_Bioseq_Info->m_ScopeInfo = 0; // detaching from scope
                    bs_id->second.m_Bioseq_Info->m_SynCache.Reset(); // breaking the link
                }
            m_Seq_idMap.erase(bs_id);
            }
            TBioseqMap::iterator bs = m_BioseqMap.find(&info.GetBioseq_Info().GetBioseq());
            if (bs != m_BioseqMap.end()) {
                m_BioseqMap.erase(bs);
            }
        }
    }
    else {
        NON_CONST_ITERATE(CSeq_entry_Info::TEntries, ei, info.m_Entries) {
            x_ClearCacheOnRemoveData(**ei);
        }
    }
}


CRef<CTSE_Info> CScope_Impl::x_GetTSE_Info(CSeq_entry& tse)
{
    for (CPriority_I it(m_setDataSrc); it; ++it) {
        CRef<CTSE_Info> info = it->GetDataSource().FindTSEInfo(tse);
        if ( info )
            return info;
    }
    NCBI_THROW(CObjMgrException, eFindFailed,
               "CScope_Impl::x_GetTSE_Info: entry is not attached");
}


CConstRef<CTSE_Info> CScope_Impl::x_GetTSE_Info(const CSeq_entry& tse)
{
    return x_GetTSE_Info(const_cast<CSeq_entry&>(tse));
}


CRef<CSeq_entry_Info> CScope_Impl::x_GetSeq_entry_Info(CSeq_entry& entry)
{
    for (CPriority_I it(m_setDataSrc); it; ++it) {
        CRef<CSeq_entry_Info> info =
            it->GetDataSource().FindSeq_entry_Info(entry);
        if ( info ) {
            return info;
        }
    }
    NCBI_THROW(CObjMgrException, eFindFailed,
               "CScope_Impl::x_GetSeq_entry_Info: entry is not attached");
}


CConstRef<CSeq_entry_Info>
CScope_Impl::x_GetSeq_entry_Info(const CSeq_entry& entry)
{
    return x_GetSeq_entry_Info(const_cast<CSeq_entry&>(entry));
}


CRef<CSeq_annot_Info>
CScope_Impl::x_GetSeq_annot_Info(CSeq_annot& annot)
{
    for (CPriority_I it(m_setDataSrc); it; ++it) {
        CRef<CSeq_annot_Info> info =
            it->GetDataSource().FindSeq_annot_Info(annot);
        if ( info ) {
            return info;
        }
    }
    NCBI_THROW(CObjMgrException, eFindFailed,
               "CScope_Impl::x_GetSeq_annot_Info: annot is not attached");
}


CConstRef<CSeq_annot_Info>
CScope_Impl::x_GetSeq_annot_Info(const CSeq_annot& annot)
{
    return x_GetSeq_annot_Info(const_cast<CSeq_annot&>(annot));
}


CScope_Impl::TTSE_Lock
CScope_Impl::GetTSEInfo(const CSeq_entry* tse)
{
    TTSE_Lock ret;
    {
        TReadLockGuard rguard(m_Scope_Conf_RWLock);
        try {
            ret = x_GetTSE_Info(*tse);
        }
        catch ( exception& /*exc*/ ) {
        }
    }
    return ret;
}


CScope_Impl::TTSE_Lock
CScope_Impl::GetTSE_Info(const CSeq_entry& tse)
{
    TTSE_Lock ret;
    {
        TReadLockGuard rguard(m_Scope_Conf_RWLock);
        try {
            ret = x_GetTSE_Info(tse);
        }
        catch ( exception& /*exc*/ ) {
        }
    }
    return ret;
}


CScope_Impl::TTSE_Lock
CScope_Impl::GetTSE_Info_WithSeq_entry(const CSeq_entry& entry)
{
    TTSE_Lock ret;
    {
        TReadLockGuard rguard(m_Scope_Conf_RWLock);
        try {
            CConstRef<CSeq_entry_Info> info = x_GetSeq_entry_Info(entry);
            ret.Reset(&info->GetTSE_Info());
        }
        catch ( exception& /*exc*/ ) {
        }
    }
    return ret;
}


CScope_Impl::TTSE_Lock
CScope_Impl::GetTSE_Info_WithSeq_annot(const CSeq_annot& annot)
{
    TTSE_Lock ret;
    {
        TReadLockGuard rguard(m_Scope_Conf_RWLock);
        try {
            CConstRef<CSeq_annot_Info> info = x_GetSeq_annot_Info(annot);
            ret.Reset(&info->GetTSE_Info());
        }
        catch ( exception& /*exc*/ ) {
        }
    }
    return ret;
}


CConstRef<CSeq_entry_Info>
CScope_Impl::GetSeq_entry_Info(const CSeq_entry& entry)
{
    CConstRef<CSeq_entry_Info> ret;
    {
        TReadLockGuard rguard(m_Scope_Conf_RWLock);
        try {
            ret = x_GetSeq_entry_Info(entry);
        }
        catch ( exception& /*exc*/ ) {
        }
    }
    return ret;
}


CConstRef<CSeq_annot_Info>
CScope_Impl::GetSeq_annot_Info(const CSeq_annot& annot)
{
    CConstRef<CSeq_annot_Info> ret;
    {
        TReadLockGuard rguard(m_Scope_Conf_RWLock);
        try {
            ret = x_GetSeq_annot_Info(annot);
        }
        catch ( exception& /*exc*/ ) {
        }
    }
    return ret;
}


void CScope_Impl::AttachAnnot(CSeq_entry& entry, CSeq_annot& annot)
{
    TWriteLockGuard guard(m_Scope_Conf_RWLock);
    CRef<CSeq_entry_Info> info = x_GetSeq_entry_Info(entry);
    info->GetDataSource().AttachAnnot(*info, annot);
    x_ClearAnnotCache();
}


void CScope_Impl::RemoveAnnot(CSeq_entry& entry, CSeq_annot& annot)
{
    TWriteLockGuard guard(m_Scope_Conf_RWLock);
    CRef<CSeq_entry_Info> info = x_GetSeq_entry_Info(entry);
    info->GetDataSource().RemoveAnnot(*info, annot);
    x_ClearAnnotCache();
}


void CScope_Impl::ReplaceAnnot(CSeq_entry& entry,
                               CSeq_annot& old_annot,
                               CSeq_annot& new_annot)
{
    TWriteLockGuard guard(m_Scope_Conf_RWLock);
    CRef<CSeq_entry_Info> info = x_GetSeq_entry_Info(entry);
    info->GetDataSource().ReplaceAnnot(*info, old_annot, new_annot);
    x_ClearAnnotCache();
}


void CScope_Impl::AttachEntry(CSeq_entry& parent, CSeq_entry& entry)
{
    TWriteLockGuard guard(m_Scope_Conf_RWLock);
    CRef<CSeq_entry_Info> info = x_GetSeq_entry_Info(parent);
    info->GetDataSource().AttachEntry(*info, entry);
    x_ClearCacheOnNewData();
}


void CScope_Impl::RemoveEntry(CSeq_entry& entry)
{
    TWriteLockGuard guard(m_Scope_Conf_RWLock);
    CSeq_entry_Info& info = *x_GetSeq_entry_Info(entry);
    x_ClearCacheOnRemoveData(info);
    if ( info.GetParentSeq_entry_Info() ) {
        info.GetDataSource().RemoveEntry(info);
    }
    else {
        CDataSource_ScopeInfo* ds_info = 0;
        for (CPriority_I it(m_setDataSrc); it; ++it) {
            if (&it->GetDataSource() == &info.GetDataSource())
            {
                ds_info = &*it;
                break;
            }
        }
        if ( ds_info ) {
            m_setDataSrc.Erase(*ds_info);
        }
    }
}


void CScope_Impl::AttachMap(CSeq_entry& bioseq, CSeqMap& seqmap)
{
    TWriteLockGuard guard(m_Scope_Conf_RWLock);
    CRef<CSeq_entry_Info> info = x_GetSeq_entry_Info(bioseq);
    info->GetDataSource().AttachMap(*info, seqmap);
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
    TSeq_idMapValue* info = bh.x_GetBioseq_ScopeInfo().m_ScopeInfo;
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
CScope_Impl::x_InitBioseq_Info(TSeq_idMapValue& info)
{
    {{
        CInitGuard init(info.second.m_Bioseq_Info, m_MutexPool);
        if ( init ) {
            x_ResolveSeq_id(info);
        }
    }}
    _ASSERT(info.second.m_Bioseq_Info);
    return info.second.m_Bioseq_Info;
}


CRef<CBioseq_ScopeInfo>
CScope_Impl::x_GetBioseq_Info(const CSeq_id_Handle& id)
{
    return x_InitBioseq_Info(x_GetSeq_id_Info(id));
}


CRef<CBioseq_ScopeInfo>
CScope_Impl::x_FindBioseq_Info(const CSeq_id_Handle& id)
{
    CRef<CBioseq_ScopeInfo> ret;
    TSeq_idMapValue* info = x_FindSeq_id_Info(id);
    if ( info ) {
        ret = x_InitBioseq_Info(*info);
    }
    return ret;
}


CBioseq_Handle CScope_Impl::x_GetBioseqHandleFromTSE(const CSeq_id_Handle& id,
                                                     const CTSE_Info& tse)
{
    CBioseq_Handle ret;
    TReadLockGuard rguard(m_Scope_Conf_RWLock);
    CRef<CBioseq_ScopeInfo> info = x_FindBioseq_Info(id);
    if ( bool(info)  &&  info->HasBioseq() ) {
        if ( &info->GetTSE_Info() == &tse ) {
            ret.m_Bioseq_Info = info;
        }
    }
    else {
        // new bioseq - try to find it in source TSE
        CSeq_id_Mapper& mapper = CSeq_id_Mapper::GetSeq_id_Mapper();
        if ( mapper.HaveMatchingHandles(id) ) {
            TSeq_id_HandleSet hset;
            mapper.GetMatchingHandles(id, hset);
            ITERATE ( TSeq_id_HandleSet, hit, hset ) {
                CSeqMatch_Info match(*hit, tse);
                CConstRef<CBioseq_Info> bioseq = match.GetBioseq_Info();
                if ( bioseq ) {
                    _ASSERT(m_HeapScope);
                    CBioseq_Handle bh = GetBioseqHandle(*hit);
                    if ( bh && &bh.x_GetTSE_Info() == &tse ) {
                        ret = bh;
                        break;
                    }
                }
            }
        }
        else {
            CSeqMatch_Info match(id, tse);
            CConstRef<CBioseq_Info> bioseq = match.GetBioseq_Info();
            if ( bioseq ) {
                CBioseq_Handle bh = GetBioseqHandle(id);
                if ( bh && &bh.x_GetTSE_Info() == &tse ) {
                    ret = bh;
                }
            }
        }
    }
    ret.m_Seq_id = id;
    return ret;
}


CBioseq_Handle CScope_Impl::GetBioseqHandle(const CSeq_id_Handle& id)
{
    CBioseq_Handle ret;
    if ( id )  {
        {{
            TReadLockGuard rguard(m_Scope_Conf_RWLock);
            ret.m_Bioseq_Info = x_GetBioseq_Info(id);
        }}
        if ( !ret.m_Bioseq_Info->HasBioseq() ) {
            ret.m_Bioseq_Info.Reset();
        }
        ret.m_Seq_id = id;
    }
    return ret;
}


CBioseq_Handle CScope_Impl::GetBioseqHandle(const CSeq_id& id)
{
    return GetBioseqHandle(CSeq_id_Handle::GetHandle(id));
}


CBioseq_Handle CScope_Impl::GetBioseqHandleFromTSE(const CSeq_id_Handle& id,
                                                   const CBioseq_Handle& bh)
{
    CBioseq_Handle ret;
    if ( bh ) {
        ret = x_GetBioseqHandleFromTSE(id, bh.m_Bioseq_Info->GetTSE_Info());
    }
    else {
        ret.m_Seq_id = id;
    }
    return ret;
}


CBioseq_Handle CScope_Impl::GetBioseqHandleFromTSE(const CSeq_id& id,
                                                   const CBioseq_Handle& bh)
{
    return GetBioseqHandleFromTSE(CSeq_id_Handle::GetHandle(id), bh);
}


CBioseq_Handle CScope_Impl::GetBioseqHandleFromTSE(const CSeq_id& id,
                                                   const CSeq_entry& tse)
{
    CBioseq_Handle ret;
    ret.m_Seq_id = CSeq_id_Handle::GetHandle(id);
    TTSE_Lock tse_lock = GetTSEInfo(&tse);
    if ( tse_lock ) {
        ret = x_GetBioseqHandleFromTSE(ret.m_Seq_id, *tse_lock);
    }
    return ret;
}


CBioseq_Handle CScope_Impl::GetBioseqHandle(const CSeq_loc& loc)
{
    CBioseq_Handle bh;
    for (CSeq_loc_CI citer (loc); citer; ++citer) {
        bh = GetBioseqHandle(citer.GetSeq_id());
        if ( bh ) {
            break;
        }
    }
    return bh;
}


CBioseq_Handle CScope_Impl::GetBioseqHandle(const CBioseq& seq)
{
    ITERATE (CBioseq::TId, id, seq.GetId()) {
        CBioseq_Handle bh = GetBioseqHandle(**id);
        if ( bh && &bh.GetBioseq() == &seq ) {
            return bh;
        }
    }
    return CBioseq_Handle();
}


void CScope_Impl::FindSeqid(set< CConstRef<CSeq_id> >& setId,
                            const string& searchBy)
{
    TReadLockGuard rguard(m_Scope_Conf_RWLock);
    setId.clear();

    TSeq_id_HandleSet setSource, setResult;
    // find all
    CSeq_id_Mapper& mapper = CSeq_id_Mapper::GetSeq_id_Mapper();
    mapper.GetMatchingHandlesStr(searchBy, setSource);
    // filter those which "belong" to my data sources
    for (CPriority_I it(m_setDataSrc); it; ++it) {
        it->GetDataSource().FilterSeqid(setResult, setSource);
    }
    // create result
    ITERATE(TSeq_id_HandleSet, itSet, setResult) {
        setId.insert(itSet->GetSeqId());
    }
}


CDataSource_ScopeInfo*
CScope_Impl::x_FindBioseqInfo(const CPriorityTree& tree,
                              const CSeq_id_Handle& idh,
                              CSeqMatch_Info& match_info)
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
            x_FindBioseqInfo(mit->second, idh, match_info);
        if ( new_ret ) {
            _ASSERT(!ret); // should be checked by match_info already
            ret = new_ret;
        }
    }
    return ret;
}


CDataSource_ScopeInfo*
CScope_Impl::x_FindBioseqInfo(CDataSource_ScopeInfo& ds_info,
                              const CSeq_id_Handle& idh,
                              CSeqMatch_Info& match_info)
{
    // skip already matched CDataSource
    if ( match_info &&
         &match_info.GetDataSource() == &ds_info.GetDataSource() ) {
        return 0;
    }
    CSeqMatch_Info info;
    {{
        CFastMutexGuard guard(ds_info.GetMutex());
        ITERATE(TTSE_LockSet, tse_it, ds_info.GetTSESet()) {
            CSeq_id_Handle found_id = idh;
            CTSE_Info::TBioseqs::const_iterator seq =
                (*tse_it)->m_Bioseqs.find(idh);
            if (seq == (*tse_it)->m_Bioseqs.end()) {
                TSeq_id_HandleSet hset;
                CSeq_id_Mapper::GetSeq_id_Mapper().GetMatchingHandles(idh, hset);
                ITERATE(TSeq_id_HandleSet, hit, hset) {
                    seq = (*tse_it)->m_Bioseqs.find(*hit);
                    if (seq != (*tse_it)->m_Bioseqs.end()) {
                        found_id = *hit;
                        break;
                    }
                }
            }
            if (seq != (*tse_it)->m_Bioseqs.end()) {
                // Use cached TSE (same meaning as from history). If info
                // is set but not in the history just ignore it.
                if ( info ) {
                    CSeqMatch_Info new_info(found_id, **tse_it);
                    // Both are in the history -
                    // can not resolve the conflict
                    if (&info.GetDataSource() == &new_info.GetDataSource()) {
                        CSeqMatch_Info* best_info =
                            info.GetDataSource().ResolveConflict(
                                found_id, info, new_info);
                        if (best_info) {
                            info = *best_info;
                            continue;
                        }
                    }
                    x_ThrowConflict(eConflict_History, info, new_info);
                }
                info = CSeqMatch_Info(found_id, **tse_it);
            }
        }
    }}
    if ( !info ) {
        // Try to load the sequence from the data source
        info = ds_info.GetDataSource().BestResolve(idh);
    }
    if ( info ) {
        if ( match_info ) {
            x_ThrowConflict(eConflict_Live, match_info, info);
        }
        match_info = info;
        return &ds_info;
    }
    return 0;
}


CDataSource_ScopeInfo*
CScope_Impl::x_FindBioseqInfo(const CPriorityNode& node,
                              const CSeq_id_Handle& idh,
                              CSeqMatch_Info& match_info)
{
    if ( node.IsTree() ) {
        // Process sub-tree
        return x_FindBioseqInfo(node.GetTree(), idh, match_info);
    }
    else if ( node.IsLeaf() ) {
        return x_FindBioseqInfo(const_cast<CDataSource_ScopeInfo&>(node.GetLeaf()),
                                idh, match_info);
    }
    return 0;
}


void CScope_Impl::x_ResolveSeq_id(TSeq_idMapValue& id_info)
{
    // Use priority, do not scan all DSs - find the first one.
    // Protected by m_Scope_Conf_RWLock in upper-level functions
    CSeqMatch_Info match_info;
    CDataSource_ScopeInfo* ds_info =
        x_FindBioseqInfo(m_setDataSrc, id_info.first, match_info);
    if ( !ds_info ) {
        _ASSERT(m_HeapScope);
        id_info.second.m_Bioseq_Info.Reset(new CBioseq_ScopeInfo(&id_info));
    }
    else {
        {{
            CFastMutexGuard guard(ds_info->GetMutex());
            ds_info->AddTSE(match_info.GetTSE_Info());
        }}

        CConstRef<CBioseq_Info> info = match_info.GetBioseq_Info();
        const CBioseq* key = &info->GetBioseq();
        {{
            TReadLockGuard guard(m_BioseqMapLock);
            TBioseqMap::const_iterator bm_it = m_BioseqMap.find(key);
            if ( bm_it != m_BioseqMap.end() ) {
                id_info.second.m_Bioseq_Info = bm_it->second;
                return;
            }
        }}
        {{
            TWriteLockGuard guard(m_BioseqMapLock);
            pair<TBioseqMap::iterator, bool> ins =
                m_BioseqMap.insert(TBioseqMap::value_type(key, CRef<CBioseq_ScopeInfo>()));
            if ( ins.second ) { // new
                _ASSERT(m_HeapScope);
                ins.first->second.Reset(new CBioseq_ScopeInfo(&id_info, info));
            }
            id_info.second.m_Bioseq_Info = ins.first->second;
        }}
    }
}


void CScope_Impl::UpdateAnnotIndex(const CSeq_annot& annot)
{
    TReadLockGuard rguard(m_Scope_Conf_RWLock);
    CConstRef<CSeq_annot_Info> info = x_GetSeq_annot_Info(annot);
    info->GetDataSource().UpdateAnnotIndex(*info);
}


CConstRef<CScope_Impl::TAnnotRefSet>
CScope_Impl::GetTSESetWithAnnots(const CSeq_id_Handle& idh)
{
    TReadLockGuard rguard(m_Scope_Conf_RWLock);
    TSeq_idMapValue& info = x_GetSeq_id_Info(idh);
    CRef<CBioseq_ScopeInfo> binfo = x_InitBioseq_Info(info);
    {{
        CInitGuard init(info.second.m_AllAnnotRef_Info, m_MutexPool);
        if ( init ) {
            CRef<TAnnotRefSet> ref_set(new TAnnotRefSet);
            TTSE_LockSet& tse_set = *ref_set;

            if ( binfo->HasBioseq() ) {
                _TRACE("GetTSESetWithAnnots: "<<idh.AsString()
                       <<" main TSE: " <<&binfo->GetBioseq_Info().GetTSE());
                TTSE_Lock tse(&binfo->GetTSE_Info());
                
                tse_set.insert(tse);
            }

            TTSE_LockSet with_ref;
            for (CPriority_I it(m_setDataSrc); it; ++it) {
                it->GetDataSource().GetTSESetWithAnnots(idh, with_ref);
                CFastMutexGuard guard(it->GetMutex());
                const TTSE_LockSet& tse_cache = it->GetTSESet();
                ITERATE(TTSE_LockSet, ref_it, with_ref) {
                    if ( (*ref_it)->IsDead() &&
                         tse_cache.find(*ref_it) == tse_cache.end() ) {
                        continue;
                    }

                    _TRACE("GetTSESetWithAllAnnots: "<<idh.AsString()
                           <<" ref TSE: " <<&(*ref_it)->GetTSE());
                    tse_set.insert(*ref_it);
                }
                with_ref.clear();
            }
            info.second.m_AllAnnotRef_Info = ref_set;
        }
    }}
    return info.second.m_AllAnnotRef_Info;
}


CConstRef<CScope_Impl::TAnnotRefSet>
CScope_Impl::GetTSESetWithAnnots(const CBioseq_Handle& bh)
{
    TReadLockGuard rguard(m_Scope_Conf_RWLock);
    TSeq_idMapValue& info = x_GetSeq_id_Info(bh);
    _ASSERT(info.second.m_Bioseq_Info);
    CRef<CBioseq_ScopeInfo> binfo = info.second.m_Bioseq_Info;
    {{
        CInitGuard init(info.second.m_AllAnnotRef_Info, m_MutexPool);
        if ( init ) {
            CRef<TAnnotRefSet> ref_set(new TAnnotRefSet);
            TTSE_LockSet& tse_set = *ref_set;

            if ( binfo->HasBioseq() ) {
                TTSE_Lock tse(&binfo->GetTSE_Info());
                
                tse_set.insert(tse);
            }

            TTSE_LockSet with_ref;
            for (CPriority_I it(m_setDataSrc); it; ++it) {
                it->GetDataSource().GetTSESetWithAnnots(info.first, with_ref);
                CFastMutexGuard guard(it->GetMutex());
                const TTSE_LockSet& tse_cache = it->GetTSESet();
                ITERATE(TTSE_LockSet, ref_it, with_ref) {
                    if ( (*ref_it)->IsDead() &&
                         tse_cache.find(*ref_it) == tse_cache.end() ) {
                        continue;
                    }

                    tse_set.insert(*ref_it);
                }
                with_ref.clear();
            }
            info.second.m_AllAnnotRef_Info = ref_set;
        }
    }}
    return info.second.m_AllAnnotRef_Info;
}


const CSeq_entry& CScope_Impl::x_GetTSEFromInfo(const TTSE_Lock& tse)
{
    return tse->GetDataSource().GetTSEFromInfo(tse);
}


void CScope_Impl::x_ThrowConflict(EConflict conflict_type,
                                  const CSeqMatch_Info& info1,
                                  const CSeqMatch_Info& info2) const
{
    const char* msg_type =
        conflict_type == eConflict_History? "history": "live TSE";
    CNcbiOstrstream s;
    s << "CScope_Impl -- multiple " << msg_type << " matches: " <<
        info1.GetDataSource().GetName() << "::" <<
        info1.GetIdHandle().AsString() <<
        " vs " <<
        info2.GetDataSource().GetName() << "::" <<
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


void CScope_Impl::x_PopulateBioseq_HandleSet(const CSeq_entry& tse,
                                             TBioseq_HandleSet& handles,
                                             CSeq_inst::EMol filter,
                                             CBioseq_CI_Base::EBioseqLevelFlag level)
{
    TReadLockGuard rguard(m_Scope_Conf_RWLock);
    TTSE_Lock tse_lock(0);
    CDataSource::TBioseq_InfoSet info_set;
    for (CPriority_I it(m_setDataSrc); it; ++it) {
        tse_lock = it->GetDataSource().GetTSEHandles(
            tse, info_set, filter, level);
        if (tse_lock) {
            // Convert each bioseq info into bioseq handle
            ITERATE (CDataSource::TBioseq_InfoSet, iit, info_set) {
                CBioseq_Handle bh = GetBioseqHandle((*iit)->GetBioseq());
                if ( bh ) {
                    handles.push_back(bh);
                }
            }
            break;
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
    return x_GetSynonyms(x_GetBioseq_Info(id));
}


CConstRef<CSynonymsSet> CScope_Impl::GetSynonyms(const CBioseq_Handle& bh)
{
    if ( !bh ) {
        return CConstRef<CSynonymsSet>();
    }
    TReadLockGuard rguard(m_Scope_Conf_RWLock);
    return x_GetSynonyms(bh.m_Bioseq_Info);
}


CConstRef<CSynonymsSet>
CScope_Impl::x_GetSynonyms(CRef<CBioseq_ScopeInfo> info)
{
    {{
        CInitGuard init(info->m_SynCache, m_MutexPool);
        if ( init ) {
            // It's OK to use CRef, at least one copy should be kept
            // alive by the id cache (for the ID requested).
            CRef<CSynonymsSet> syn_set(new CSynonymsSet);
            //syn_set->AddSynonym(id);
            if ( info->HasBioseq() ) {
                ITERATE(CBioseq_Info::TSynonyms, it,
                        info->GetBioseq_Info().m_Synonyms) {
                    TSeq_id_HandleSet hset;
                    CSeq_id_Mapper::GetSeq_id_Mapper().
                        GetMatchingHandles(*it, hset);
                    ITERATE(TSeq_id_HandleSet, mit, hset) {
                        // Check current ID for conflicts, add to the set.
                        try {
                            TSeq_idMapValue& seq_id_info =
                                x_GetSeq_id_Info(*mit);
                            CRef<CBioseq_ScopeInfo> info2 =
                                x_InitBioseq_Info(seq_id_info);
                            if ( info2 == info ) {
                                // the same bioseq - add synonym
                                syn_set->AddSynonym(&seq_id_info);
                            }
                            else {
                                LOG_POST(Warning << "CScope::GetSynonyms: "
                                         "Bioseq["<<info->GetBioseq_Info().IdsString()<<"]: id "<<mit->AsString()<<" is resolved to another Bioseq ["<<info2->GetBioseq_Info().IdsString()<<"]");
                            }
                        }
                        catch ( exception& exc ) {
                            LOG_POST(Warning << "CScope::GetSynonyms: "
                                     "Bioseq["<<info->GetBioseq_Info().IdsString()<<"]: id "<<mit->AsString()<<" cannot be resolved: "<<
                                     exc.what());
                        }
                    }
                }
            }
            info->m_SynCache = syn_set;
        }
    }}
    return info->m_SynCache;
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
