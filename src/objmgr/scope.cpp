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

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


////////////////////////////////////////////////////////////////////
//
//  CScope::
//


CScope::CScope(CObjectManager& objmgr)
    : m_pObjMgr(0)
{
    TWriteLockGuard guard(m_Scope_Conf_RWLock);
    x_AttachToOM(objmgr);
}


CScope::~CScope(void)
{
    TWriteLockGuard guard(m_Scope_Conf_RWLock);
    x_DetachFromOM();
}

void CScope::x_AttachToOM(CObjectManager& objmgr)
{
    if ( m_pObjMgr != &objmgr ) {
        x_DetachFromOM();
        
        m_pObjMgr = &objmgr;
        m_pObjMgr->RegisterScope(*this);
    }
}


void CScope::x_DetachFromOM(void)
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


void CScope::AddDefaults(CPriorityTree::TPriority priority)
{
    CObjectManager::TDataSourcesLock ds_set;
    m_pObjMgr->AcquireDefaultDataSources(ds_set);

    TWriteLockGuard guard(m_Scope_Conf_RWLock);
    NON_CONST_ITERATE( CObjectManager::TDataSourcesLock, it, ds_set ) {
        m_setDataSrc.Insert(const_cast<CDataSource&>(**it), priority);
    }
    x_ClearCacheOnNewData();
}


void CScope::AddDataLoader (const string& loader_name,
                            CPriorityTree::TPriority priority)
{
    CRef<CDataSource> ds = m_pObjMgr->AcquireDataLoader(loader_name);

    TWriteLockGuard guard(m_Scope_Conf_RWLock);
    m_setDataSrc.Insert(*ds, priority);
    x_ClearCacheOnNewData();
}


void CScope::AddTopLevelSeqEntry(CSeq_entry& top_entry,
                                 CPriorityTree::TPriority priority)
{
    CRef<CDataSource> ds = m_pObjMgr->AcquireTopLevelSeqEntry(top_entry);

    TWriteLockGuard guard(m_Scope_Conf_RWLock);
    m_setDataSrc.Insert(*ds, priority);
    x_ClearCacheOnNewData();
}


void CScope::AddScope(CScope& scope, CPriorityTree::TPriority priority)
{
    TReadLockGuard src_guard(scope.m_Scope_Conf_RWLock);
    CPriorityTree tree(scope.m_setDataSrc);
    src_guard.Release();
    
    TWriteLockGuard guard(m_Scope_Conf_RWLock);
    m_setDataSrc.Insert(tree, priority);
    x_ClearCacheOnNewData();
}


void CScope::x_ClearCacheOnNewData(void)
{
    // Clear unresolved bioseq handles
    // Clear annot cache
    if (!m_Seq_idMap.empty()) {
        LOG_POST(Info <<
            "CScope: -- "
            "adding new data to a scope with non-empty cache "
            "may cause the cached data to become inconsistent");
    }
    NON_CONST_ITERATE ( TSeq_idMap, it, m_Seq_idMap ) {
        if ( it->second.m_Bioseq_Info &&
             !it->second.m_Bioseq_Info->HasBioseq() ) {
            it->second.m_Bioseq_Info.Reset();
        }
        it->second.m_AnnotRef_Info.Reset();
    }
}


void CScope::x_ClearAnnotCache(void)
{
    // Clear annot cache
    NON_CONST_ITERATE ( TSeq_idMap, it, m_Seq_idMap ) {
        it->second.m_AnnotRef_Info.Reset();
    }
}


bool CScope::AttachAnnot(CSeq_entry& entry, CSeq_annot& annot)
{
    TWriteLockGuard guard(m_Scope_Conf_RWLock);
    for (CPriority_I it(m_setDataSrc); it; ++it) {
        if ( it->GetDataSource().AttachAnnot(entry, annot) ) {
            // Clear annot cache
            x_ClearAnnotCache();
            return true;
        }
    }
    return false;
}


bool CScope::RemoveAnnot(CSeq_entry& entry, CSeq_annot& annot)
{
    TWriteLockGuard guard(m_Scope_Conf_RWLock);
    for (CPriority_I it(m_setDataSrc); it; ++it) {
        if ( it->GetDataLoader() )
            continue; // can not modify loaded data
        if ( it->GetDataSource().RemoveAnnot(entry, annot) ) {
            // Clear annot cache
            x_ClearAnnotCache();
            return true;
        }
    }
    return false;
}


bool CScope::ReplaceAnnot(CSeq_entry& entry,
                          CSeq_annot& old_annot,
                          CSeq_annot& new_annot)
{
    TWriteLockGuard guard(m_Scope_Conf_RWLock);
    for (CPriority_I it(m_setDataSrc); it; ++it) {
        if ( it->GetDataSource().ReplaceAnnot(entry, old_annot, new_annot) ) {
            // Clear annot cache
            x_ClearAnnotCache();
            return true;
        }
    }
    return false;
}


bool CScope::AttachEntry(CSeq_entry& parent, CSeq_entry& entry)
{
    TWriteLockGuard guard(m_Scope_Conf_RWLock);
    for (CPriority_I it(m_setDataSrc); it; ++it) {
        if ( it->GetDataSource().AttachEntry(parent, entry) ) {
            // Clear annot cache
            x_ClearCacheOnNewData();
            return true;
        }
    }
    return false;
}


bool CScope::AttachMap(CSeq_entry& bioseq, CSeqMap& seqmap)
{
    TWriteLockGuard guard(m_Scope_Conf_RWLock);
    for (CPriority_I it(m_setDataSrc); it; ++it) {
        if ( it->GetDataSource().AttachMap(bioseq, seqmap) ) {
            return true;
        }
    }
    return false;
}


CScope::TSeq_idMapValue& CScope::x_GetSeq_id_Info(const CSeq_id_Handle& id)
{
    {{
        TReadLockGuard guard(m_Seq_idMapLock);
        TSeq_idMap::iterator it = m_Seq_idMap.find(id);
        if ( it != m_Seq_idMap.end() )
            return *it;
    }}
    {{
        TWriteLockGuard guard(m_Seq_idMapLock);
        return *m_Seq_idMap.insert(TSeq_idMapValue(id,
                                                   SSeq_id_ScopeInfo())).first;
    }}
}


CScope::TSeq_idMapValue* CScope::x_FindSeq_id_Info(const CSeq_id_Handle& id)
{
    TReadLockGuard guard(m_Seq_idMapLock);
    TSeq_idMap::iterator it = m_Seq_idMap.find(id);
    if ( it == m_Seq_idMap.end() )
        return 0;
    else
        return &*it;
}


CRef<CBioseq_ScopeInfo> CScope::x_InitBioseq_Info(TSeq_idMapValue& info)
{
    {{
        CInitGuard init(info.second.m_Bioseq_Info, m_MutexPool);
        if ( init ) {
            info.second.m_Bioseq_Info = x_ResolveSeq_id(info.first);
        }
    }}
    _ASSERT(info.second.m_Bioseq_Info);
    return info.second.m_Bioseq_Info;
}


CRef<CBioseq_ScopeInfo> CScope::x_GetBioseq_Info(const CSeq_id_Handle& id)
{
    return x_InitBioseq_Info(x_GetSeq_id_Info(id));
}


CRef<CBioseq_ScopeInfo> CScope::x_FindBioseq_Info(const CSeq_id_Handle& id)
{
    CRef<CBioseq_ScopeInfo> ret;
    TSeq_idMapValue* info = x_FindSeq_id_Info(id);
    if ( info ) {
        ret = x_InitBioseq_Info(*info);
    }
    return ret;
}


CBioseq_Handle CScope::x_GetBioseqHandleFromTSE(const CSeq_id_Handle& id,
                                                const CTSE_Info& tse)
{
    CBioseq_Handle ret;
    TReadLockGuard rguard(m_Scope_Conf_RWLock);
    CRef<CBioseq_ScopeInfo> info = x_FindBioseq_Info(id);
    if ( info ) {
        if ( &info->GetTSE_Info() == &tse ) {
            ret.m_Bioseq_Info = info;
        }
    }
    else {
        // new bioseq - try to find it in source TSE
        TSeq_id_HandleSet hset;
        GetIdMapper().GetMatchingHandles(id, hset);
        ITERATE ( TSeq_id_HandleSet, hit, hset ) {
            CSeqMatch_Info match(id, tse);
            CConstRef<CBioseq_Info> bioseq = match.GetBioseq_Info();
            if ( bioseq ) {
                ret.m_Bioseq_Info = new CBioseq_ScopeInfo(this, bioseq);
                break;
            }
        }
    }
    ret.m_Seq_id = id;
    return ret;
}


CBioseq_Handle CScope::GetBioseqHandle(const CSeq_id_Handle& id)
{
    CBioseq_Handle ret;
    {{
        TReadLockGuard rguard(m_Scope_Conf_RWLock);
        ret.m_Bioseq_Info = x_GetBioseq_Info(id);
    }}
    if ( !ret.m_Bioseq_Info->HasBioseq() ) {
        ret.m_Bioseq_Info.Reset();
    }
    ret.m_Seq_id = id;
    return ret;
}


CBioseq_Handle CScope::GetBioseqHandle(const CSeq_id& id)
{
    return GetBioseqHandle(GetIdHandle(id));
}


CBioseq_Handle CScope::GetBioseqHandleFromTSE(const CSeq_id_Handle& id,
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


CBioseq_Handle CScope::GetBioseqHandleFromTSE(const CSeq_id& id,
                                              const CBioseq_Handle& bh)
{
    return GetBioseqHandleFromTSE(GetIdHandle(id), bh);
}


CBioseq_Handle CScope::GetBioseqHandle(const CSeq_loc& loc)
{
    for (CSeq_loc_CI citer (loc); citer; ++citer) {
        CBioseq_Handle bh = GetBioseqHandle(citer.GetSeq_id());
        if ( bh ) {
            return bh;
        }
    }

    string label;
    loc.GetLabel(&label);
    NCBI_THROW(CException, eUnknown, "GetBioseqHandle by location failed: "
        "seq-loc = " + label);
}


CBioseq_Handle CScope::GetBioseqHandle(const CBioseq& seq)
{
    ITERATE (CBioseq::TId, id, seq.GetId()) {
        CBioseq_Handle bh = GetBioseqHandle(**id);
        if ( bh && &bh.GetBioseq() == &seq ) {
            return bh;
        }
    }

    NCBI_THROW(CException, eUnknown, "GetBioseqHandle from bioseq failed");
}


void CScope::FindSeqid(set< CRef<const CSeq_id> >& setId,
                       const string& searchBy)
{
    TReadLockGuard rguard(m_Scope_Conf_RWLock);
    setId.clear();

    TSeq_id_HandleSet setSource, setResult;
    // find all
    GetIdMapper().GetMatchingHandlesStr(searchBy, setSource);
    // filter those which "belong" to my data sources
    for (CPriority_I it(m_setDataSrc); it; ++it) {
        it->GetDataSource().FilterSeqid(setResult, setSource);
    }
    // create result
    ITERATE(TSeq_id_HandleSet, itSet, setResult) {
        setId.insert(CRef<const CSeq_id>
                     (&(GetIdMapper().GetSeq_id(*itSet))));
    }
}


void CScope::x_FindBioseqInfo(const CPriorityTree& tree,
                              const CSeq_id_Handle& idh,
                              TSeqMatchSet& sm_set)
{
    // Process sub-tree
    CPriorityTree::TPriority last_priority = 0;
    ITERATE( CPriorityTree::TPriorityMap, mit, tree.GetTree() ) {
        // Search in all nodes of the same priority regardless
        // of previous results
        CPriorityTree::TPriority new_priority = mit->first;
        if ( new_priority != last_priority ) {
            if ( !sm_set.empty() )
                return;
            last_priority = new_priority;
        }
        x_FindBioseqInfo(mit->second, idh, sm_set);
        // Don't process lower priority nodes if something
        // was found
    }
}


void CScope::x_FindBioseqInfo(CDataSource_ScopeInfo& ds_info,
                              const CSeq_id_Handle& idh,
                              TSeqMatchSet& sm_set)
{
    CSeqMatch_Info info;
    {{
        CFastMutexGuard guard(ds_info.GetMutex());
        ITERATE(TTSE_LockSet, tse_it, ds_info.GetTSESet()) {
            CTSE_Info::TBioseqs::const_iterator seq =
                (*tse_it)->m_Bioseqs.find(idh);
            if (seq != (*tse_it)->m_Bioseqs.end()) {
                // Use cached TSE (same meaning as from history). If info
                // is set but not in the history just ignore it.
                if ( info ) {
                    CSeqMatch_Info new_info(idh, **tse_it);
                    // Both are in the history -
                    // can not resolve the conflict
                    x_ThrowConflict(eConflict_History, info, new_info);
                }
                info = CSeqMatch_Info(idh, **tse_it);
            }
        }
    }}
    if ( !info ) {
        // Try to load the sequence from the data source
        info = ds_info.GetDataSource().BestResolve(idh);
    }
    if ( info ) {
        sm_set.insert(TSeqMatchSet::value_type(info, &ds_info));
    }
}

void CScope::x_FindBioseqInfo(const CPriorityNode& node,
                              const CSeq_id_Handle& idh,
                              TSeqMatchSet& sm_set)
{
    if ( node.IsTree() ) {
        // Process sub-tree
        x_FindBioseqInfo(node.GetTree(), idh, sm_set);
    }
    else if ( node.IsLeaf() ) {
        x_FindBioseqInfo(const_cast<CDataSource_ScopeInfo&>(node.GetLeaf()),
                         idh, sm_set);
    }
}


CRef<CBioseq_ScopeInfo> CScope::x_ResolveSeq_id(const CSeq_id_Handle& idh)
{
    CRef<CBioseq_ScopeInfo> ret;
    // Use priority, do not scan all DSs - find the first one.
    // Protected by m_Scope_Conf_RWLock in upper-level functions
    TSeqMatchSet bm_set;
    x_FindBioseqInfo(m_setDataSrc, idh, bm_set);
    if (bm_set.empty()) {
        ret.Reset(new CBioseq_ScopeInfo(this));
    }
    else {
        TSeqMatchSet::iterator bm_begin = bm_set.begin();
        {{
            TSeqMatchSet::iterator bm_it = bm_begin;
            if ( ++bm_it != bm_set.end() ) {
                // More than one TSE found in different data sources.
                x_ThrowConflict(eConflict_Live, bm_begin->first, bm_it->first);
            }
        }}
        const CSeqMatch_Info& smi = bm_begin->first;
        {{
            CFastMutexGuard guard(bm_begin->second->GetMutex());
            bm_begin->second->AddTSE(smi.GetTSE_Info());
        }}

        CConstRef<CBioseq_Info> info = smi.GetBioseq_Info();
        const CBioseq* key = &info->GetBioseq();
        {{
            TReadLockGuard guard(m_BioseqMapLock);
            TBioseqMap::const_iterator bm_it = m_BioseqMap.find(key);
            if ( bm_it != m_BioseqMap.end() ) {
                ret = bm_it->second;
                return ret;
            }
        }}
        {{
            TWriteLockGuard guard(m_BioseqMapLock);
            pair<TBioseqMap::iterator, bool> ins =
                m_BioseqMap.insert(TBioseqMap::value_type(key, ret));
            if ( ins.second ) { // new
                ins.first->second.Reset(new CBioseq_ScopeInfo(this, info));
            }
            ret = ins.first->second;
        }}
        _TRACE("x_ResolveSeq_id: "<<idh.AsString()<<" -> "<<&info->GetTSE());
    }
    return ret; // Just to avoid compiler warnings.
}


void CScope::UpdateAnnotIndex(const CHandleRangeMap& loc,
                              const SAnnotSelector& sel)
{
    TReadLockGuard rguard(m_Scope_Conf_RWLock);
    for (CPriority_I it(m_setDataSrc); it; ++it) {
        it->GetDataSource().UpdateAnnotIndex(loc, sel);
    }
}


void CScope::UpdateAnnotIndex(const CHandleRangeMap& loc,
                              const SAnnotSelector& sel,
                              const CSeq_entry& entry)
{
    TReadLockGuard rguard(m_Scope_Conf_RWLock);
    for (CPriority_I it(m_setDataSrc); it; ++it) {
        CConstRef<CSeq_entry_Info> entry_info =
            it->GetDataSource().GetSeq_entry_Info(entry);
        if ( entry_info ) {
            it->GetDataSource().UpdateAnnotIndex(loc, sel, *entry_info);
            break;
        }
    }
}


void CScope::UpdateAnnotIndex(const CHandleRangeMap& loc,
                              const SAnnotSelector& sel,
                              const CSeq_annot& annot)
{
    TReadLockGuard rguard(m_Scope_Conf_RWLock);
    for (CPriority_I it(m_setDataSrc); it; ++it) {
        CConstRef<CSeq_annot_Info> annot_info =
            it->GetDataSource().GetSeq_annot_Info(annot);
        if ( annot_info ) {
            it->GetDataSource().UpdateAnnotIndex(loc, sel, *annot_info);
            break;
        }
    }
}


CConstRef<CScope::TAnnotRefSet>
CScope::GetTSESetWithAnnots(const CSeq_id_Handle& idh)
{
    TReadLockGuard rguard(m_Scope_Conf_RWLock);
    TSeq_idMapValue& info = x_GetSeq_id_Info(idh);
    CRef<CBioseq_ScopeInfo> binfo = x_InitBioseq_Info(info);
    {{
        CInitGuard init(info.second.m_AnnotRef_Info, m_MutexPool);
        if ( init ) {
            CRef<TAnnotRefSet> ref_set(new TAnnotRefSet);
            TTSE_LockSet& tse_set = *ref_set;

            if ( binfo->HasBioseq() ) {
                _TRACE("GetTSESetWithAnnots: "<<idh.AsString()<<" main TSE: " <<&binfo->GetBioseq_Info().GetTSE());
                tse_set.insert(TTSE_Lock(&binfo->GetTSE_Info()));
            }

            TTSE_LockSet with_ref;
            for (CPriority_I it(m_setDataSrc); it; ++it) {
                it->GetDataSource().GetTSESetWithAnnots(idh, with_ref);
                CFastMutexGuard guard(it->GetMutex());
                const TTSE_LockSet& tse_cache = it->GetTSESet();
                ITERATE(TTSE_LockSet, ref_it, with_ref) {
                    if (tse_cache.find(*ref_it) != tse_cache.end()  ||
                        !(*ref_it)->IsDead()) {
                        _TRACE("GetTSESetWithAnnots: "<<idh.AsString()<<" ref TSE: " <<&(*ref_it)->GetTSE());
                        tse_set.insert(*ref_it);
                    }
                }
                with_ref.clear();
            }
            info.second.m_AnnotRef_Info = ref_set;
        }
    }}
    return info.second.m_AnnotRef_Info;
}


CScope::TTSE_Lock CScope::GetTSEInfo(const CSeq_entry* tse)
{
    TReadLockGuard rguard(m_Scope_Conf_RWLock);
    TTSE_Lock ret;
    for (CPriority_I it(m_setDataSrc); it; ++it) {
        ret = it->GetDataSource().GetTSEInfo(*tse);
        if ( ret )
            break;
    }
    return ret;
}


const CSeq_entry& CScope::x_GetTSEFromInfo(const TTSE_Lock& tse)
{
    return tse->GetDataSource().GetTSEFromInfo(tse);
}


void CScope::x_ThrowConflict(EConflict conflict_type,
                             const CSeqMatch_Info& info1,
                             const CSeqMatch_Info& info2) const
{
    const char* msg_type =
        conflict_type == eConflict_History? "history": "live TSE";
    CNcbiOstrstream s;
    s << "CScope -- multiple " << msg_type << " matches: " <<
        info1.GetDataSource().GetName() << "::" <<
        info1.GetIdHandle().AsString() <<
        " vs " <<
        info2.GetDataSource().GetName() << "::" <<
        info2.GetIdHandle().AsString();
    string msg = CNcbiOstrstreamToString(s);
    THROW1_TRACE(runtime_error, msg);
}


void CScope::ResetHistory(void)
{
    TWriteLockGuard guard(m_Scope_Conf_RWLock);
    x_ResetHistory();
}


void CScope::x_ResetHistory(void)
{
    m_Seq_idMap.clear();
    m_BioseqMap.clear();
    for (CPriority_I it(m_setDataSrc); it; ++it) {
        it->m_TSE_LockSet.clear();
    }
}


void CScope::x_PopulateBioseq_HandleSet(const CSeq_entry& tse,
                                        set<CBioseq_Handle>& handles,
                                        CSeq_inst::EMol filter)
{
    TReadLockGuard rguard(m_Scope_Conf_RWLock);
    TTSE_Lock tse_lock(0);
    set< CConstRef<CBioseq_Info> > info_set;
    for (CPriority_I it(m_setDataSrc); it; ++it) {
        tse_lock = it->GetDataSource().GetTSEHandles(tse, info_set, filter);
        if (tse_lock) {
            // Convert each bioseq info into bioseq handle
            ITERATE (set< CConstRef<CBioseq_Info> >, iit, info_set) {
                CBioseq_Handle bh = GetBioseqHandle((*iit)->GetBioseq());
                if ( bh ) {
                    handles.insert(bh);
                }
            }
            break;
        }
    }
}


CConstRef<CSynonymsSet> CScope::GetSynonyms(const CSeq_id& id)
{
    return GetSynonyms(GetIdHandle(id));
}


CConstRef<CSynonymsSet> CScope::GetSynonyms(const CSeq_id_Handle& id)
{
    TReadLockGuard rguard(m_Scope_Conf_RWLock);
    return x_GetSynonyms(x_GetBioseq_Info(id));
}


CConstRef<CSynonymsSet> CScope::GetSynonyms(const CBioseq_Handle& bh)
{
    if ( !bh ) {
        return CConstRef<CSynonymsSet>();
    }
    TReadLockGuard rguard(m_Scope_Conf_RWLock);
    return x_GetSynonyms(bh.m_Bioseq_Info);
}


CConstRef<CSynonymsSet> CScope::x_GetSynonyms(CRef<CBioseq_ScopeInfo> info)
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
                    // Check current ID for conflicts, add to the set.
                    CRef<CBioseq_ScopeInfo> info2 = x_GetBioseq_Info(*it);
                    if ( info2 == info ) {
                        // the same bioseq - add synonym
                        syn_set->AddSynonym(*it);
                    }
                }
            }
            info->m_SynCache = syn_set;
        }
    }}
    return info->m_SynCache;
}


CConstRef<CSeq_entry_Info> CScope::x_GetSeq_entry_Info(const CSeq_entry& entry)
{
    CConstRef<CSeq_entry_Info> info;
    for (CPriority_I it(m_setDataSrc); it; ++it) {
        info = it->GetDataSource().GetSeq_entry_Info(entry);
        if ( info ) {
            break;
        }
    }
    return info;
}


void CScope::DebugDump(CDebugDumpContext ddc, unsigned int depth) const
{
    ddc.SetFrame("CScope");
    CObject::DebugDump( ddc, depth);

    ddc.Log("m_pObjMgr", m_pObjMgr,0);
    if (depth == 0) {
        //DebugDumpValue(ddc,"m_setDataSrc.size()", m_setDataSrc.size());
        //DebugDumpValue(ddc,"m_History.size()", m_History.size());
    } else {
        DebugDumpValue(ddc,"m_setDataSrc.type", "set<CDataSource*>");
        /*
        DebugDumpRangePtr(ddc,"m_setDataSrc",
            m_setDataSrc.begin(), m_setDataSrc.end(), depth);

        DebugDumpValue(ddc,"m_History.type", "set<CConstRef<CTSE_Info>>");
        DebugDumpRangeCRef(ddc,"m_History",
            m_History.begin(), m_History.end(), depth);
        */
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
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
