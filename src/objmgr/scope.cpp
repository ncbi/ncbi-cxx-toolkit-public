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

#include <objects/objmgr/scope.hpp>
#include <objects/objmgr/impl/data_source.hpp>
#include <objects/objmgr/impl/tse_info.hpp>
#include <objects/objmgr/impl/bioseq_info.hpp>
#include <objects/objmgr/impl/seq_annot_info.hpp>
#include <objects/objmgr/impl/priority.hpp>
#include <objects/objmgr/seqmatch_info.hpp>
#include <objects/objmgr/bioseq_handle.hpp>
#include <objects/objmgr/object_manager.hpp>
#include <objects/objmgr/impl/synonyms.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Delta_seq.hpp>
#include <objects/seq/Seq_literal.hpp>
#include <objects/seqloc/Seq_loc.hpp>

#if 0
// for extended diagnostics in x_ThrowConflict()
# include <serial/serial.hpp>
# include <serial/objostr.hpp>
# include <objects/seqset/Seq_entry.hpp>
#endif

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


////////////////////////////////////////////////////////////////////
//
//  CScope::
//


CScope::CScope(CObjectManager& objmgr)
    : m_pObjMgr(&objmgr), m_FindMode(eFirst)
{
    m_pObjMgr->RegisterScope(*this);
    m_setDataSrc.SetTree();
}


CScope::~CScope(void)
{
    x_DetachFromOM();
}

void CScope::AddDefaults(CPriorityNode::TPriority priority)
{
    CWriteLockGuard guard(m_Scope_Conf_RWLock);
    m_pObjMgr->AcquireDefaultDataSources(m_setDataSrc, priority);
    x_ClearCacheOnNewData();
}


void CScope::AddDataLoader (const string& loader_name,
                            CPriorityNode::TPriority priority)
{
    CWriteLockGuard guard(m_Scope_Conf_RWLock);
    m_pObjMgr->AddDataLoader(m_setDataSrc, loader_name, priority);
    x_ClearCacheOnNewData();
}


void CScope::AddTopLevelSeqEntry(CSeq_entry& top_entry,
                                 CPriorityNode::TPriority priority)
{
    CWriteLockGuard guard(m_Scope_Conf_RWLock);
    m_pObjMgr->AddTopLevelSeqEntry(m_setDataSrc, top_entry, priority);
    x_ClearCacheOnNewData();
}


void CScope::AddScope(CScope& scope, CPriorityNode::TPriority priority)
{
    CWriteLockGuard guard(m_Scope_Conf_RWLock);
    m_pObjMgr->AddScope(m_setDataSrc, scope, priority);
    x_ClearCacheOnNewData();
}


void CScope::x_ClearCacheOnNewData(void)
{
    // Clear unresolved bioseq handles
    for ( TCache::iterator it = m_Cache.begin(); it != m_Cache.end(); ) {
        TCache::iterator cur = it++;
        if ( !cur->second ) {
            m_Cache.erase(cur);
        }
    }
    // Clear annot cache
    m_AnnotCache.clear();
    if (!m_Cache.empty()) {
        LOG_POST(Info <<
            "CScope: -- "
            "adding new data to a scope with non-empty cache "
            "may cause the cached data to become inconsistent");
    }
    m_DS_Cache.clear();
}


bool CScope::AttachAnnot(CSeq_entry& entry, CSeq_annot& annot)
{
    CWriteLockGuard guard(m_Scope_Conf_RWLock);
    for (CPriority_I it(m_setDataSrc); it; ++it) {
        if ( it->AttachAnnot(entry, annot) ) {
            // Clear annot cache
            m_AnnotCache.clear();
            return true;
        }
    }
    return false;
}


bool CScope::RemoveAnnot(CSeq_entry& entry, CSeq_annot& annot)
{
    CWriteLockGuard guard(m_Scope_Conf_RWLock);
    for (CPriority_I it(m_setDataSrc); it; ++it) {
        if ( it->GetDataLoader() )
            continue; // can not modify loaded data
        if ( it->RemoveAnnot(entry, annot) ) {
            // Clear annot cache
            m_AnnotCache.clear();
            return true;
        }
    }
    return false;
}


bool CScope::ReplaceAnnot(CSeq_entry& entry,
                          CSeq_annot& old_annot,
                          CSeq_annot& new_annot)
{
    CWriteLockGuard guard(m_Scope_Conf_RWLock);
    for (CPriority_I it(m_setDataSrc); it; ++it) {
        if ( it->ReplaceAnnot(entry, old_annot, new_annot) ) {
            // Clear annot cache
            m_AnnotCache.clear();
            return true;
        }
    }
    return false;
}


bool CScope::AttachEntry(CSeq_entry& parent, CSeq_entry& entry)
{
    CWriteLockGuard guard(m_Scope_Conf_RWLock);
    for (CPriority_I it(m_setDataSrc); it; ++it) {
        if ( it->AttachEntry(parent, entry) ) {
            // Clear annot cache
            m_AnnotCache.clear();
            //### Take each seq-id from history with priority lower than
            //### the one of modified entry, check if there are any conflicts.
            if (!m_Cache.empty()) {
                LOG_POST(Info <<
                    "CScope::AttachEntry() -- "
                    "adding new data to a scope with non-empty cache "
                    "may cause the cached data to become inconsistent");
            }
            return true;
        }
    }
    return false;
}


bool CScope::AttachMap(CSeq_entry& bioseq, CSeqMap& seqmap)
{
    CWriteLockGuard guard(m_Scope_Conf_RWLock);
    for (CPriority_I it(m_setDataSrc); it; ++it) {
        if ( it->AttachMap(bioseq, seqmap) ) {
            return true;
        }
    }
    return false;
}


CBioseq_Handle CScope::GetBioseqHandle(const CSeq_id& id)
{
    return GetBioseqHandle(GetIdHandle(id));
}


CBioseq_Handle CScope::GetBioseqHandleFromTSE(const CSeq_id& id,
                                              const CBioseq_Handle& bh)
{
    return GetBioseqHandleFromTSE(GetIdHandle(id), bh);
}


CBioseq_Handle CScope::GetBioseqHandle(const CSeq_id_Handle& id)
{
    CReadLockGuard rguard(m_Scope_Conf_RWLock);
    {{
        CMutexGuard guard(m_Scope_Cache_Mtx);
        TCache::const_iterator it = m_Cache.find(id);
        if ( it != m_Cache.end() ) {
            return it->second;
        }
    }}
    CBioseq_Handle bh;
    CSeqMatch_Info match = x_BestResolve(id);
    {{
        CMutexGuard guard(m_Scope_Cache_Mtx);
        if (match) {
            CRef<CBioseq_Info> bsi =
                match.GetDataSource().GetBioseqHandle(match);
            _ASSERT(bsi);
            bh = CBioseq_Handle(match.GetIdHandle(), *this, *bsi);
            m_Cache.insert(TCache::value_type(id, bh));
            x_AddBioseqToCache(*bsi, &id);
        }
        else {
            m_Cache[id];
        }
    }}
    return bh;
}


CBioseq_Handle CScope::GetBioseqHandleFromTSE(const CSeq_id_Handle& id,
                                              const CBioseq_Handle& bh)
{
    CReadLockGuard rguard(m_Scope_Conf_RWLock);
    CMutexGuard guard(m_Scope_Cache_Mtx);
    TCache::const_iterator it = m_Cache.find(id);
    if ( it != m_Cache.end() && it->second ) {
        if ( &it->second.GetTopLevelSeqEntry() != &bh.GetTopLevelSeqEntry() ) {
            return CBioseq_Handle();
        }
        return it->second;
    }
    CBioseq_Handle ret;
    TSeq_id_HandleSet hset;
    x_GetIdMapper().GetMatchingHandles(id.GetSeqId(), hset);
    ITERATE ( TSeq_id_HandleSet, hit, hset ) {
        CSeqMatch_Info match(id, bh.m_Bioseq_Info->GetTSE_Info());
        CBioseq_Info* bsi = match.GetDataSource().GetBioseqHandle(match);
        if ( bsi ) {
            ret = CBioseq_Handle(match.GetIdHandle(), *this, *bsi);
            m_Cache[id] = ret;
            break;
        }
    }
    return ret;
}


CBioseq_Handle CScope::GetBioseqHandle(const CSeq_loc& loc)
{
    for (CSeq_loc_CI citer (loc); citer; ++citer) {
        return GetBioseqHandle(citer.GetSeq_id());
    }

    NCBI_THROW(CException, eUnknown, "GetBioseqHandle by location failed");
}


CBioseq_Handle CScope::GetBioseqHandle(const CBioseq& seq)
{
    ITERATE (CBioseq::TId, id, seq.GetId()) {
        return GetBioseqHandle (**id);
    }

    NCBI_THROW(CException, eUnknown, "GetBioseqHandle from bioseq failed");
}


void CScope::FindSeqid(set< CRef<const CSeq_id> >& setId,
                       const string& searchBy) const
{
    CReadLockGuard rguard(m_Scope_Conf_RWLock);
    CMutexGuard guard(m_Scope_Cache_Mtx);
    setId.clear();

    TSeq_id_HandleSet setSource, setResult;
    // find all
    x_GetIdMapper().GetMatchingHandlesStr(searchBy, setSource);
    // filter those which "belong" to my data sources
    for (CPriority_I it(m_setDataSrc); it; ++it) {
        it->FilterSeqid(setResult, setSource);
    }
    // create result
    ITERATE(TSeq_id_HandleSet, itSet, setResult) {
        setId.insert(CRef<const CSeq_id>
                     (&(x_GetIdMapper().GetSeq_id(*itSet))));
    }
}


void CScope::x_ResolveInNode(const CPriorityNode& node,
                             CSeq_id_Handle& idh,
                             TSeqMatchSet& sm_set)
{
    if ( node.IsTree() ) {
        // Process sub-tree
        NON_CONST_ITERATE(CPriorityNode::TPriorityMap, mit, node.GetTree()) {
            // Search in all nodes of the same priority regardless
            // of previous results
            NON_CONST_ITERATE(CPriorityNode::TPrioritySet, sit, mit->second) {
                x_ResolveInNode(*sit, idh, sm_set);
            }
            // Don't process lower priority nodes if something
            // was found
            if (!sm_set.empty())
                return;
        }
    }
    else if ( node.IsDataSource() ) {
        TTSE_LockSet& dsc = m_DS_Cache[&node.GetDataSource()];
        CSeqMatch_Info info;
        CSeqMatch_Info from_history;
        ITERATE(TTSE_LockSet, tse_it, dsc) {
            CTSE_Info::TBioseqMap::const_iterator seq =
                (*tse_it)->m_BioseqMap.find(idh);
            if (seq != (*tse_it)->m_BioseqMap.end()) {
                // Use cached TSE (same meaning as from history). If info
                // is set but not in the history just ignore it.
                info = CSeqMatch_Info(idh, **tse_it);
                if ( from_history ) {
                    // Both are in the history - can not resolve the conflict
                    x_ThrowConflict(eConflict_History, info, from_history);
                }
                from_history = info;
            }
        }
        if (!from_history) {
            // Try to load the sequence from the data source
            info = node.GetDataSource().BestResolve(idh);
        }
        if ( info ) {
            sm_set.insert(info);
        }
    }
}


CSeqMatch_Info CScope::x_BestResolve(CSeq_id_Handle idh)
{
    // Use priority, do not scan all DSs - find the first one.
    // Protected by m_Scope_Conf_RWLock in upper-level functions
    TSeqMatchSet bm_set;
    x_ResolveInNode(m_setDataSrc, idh, bm_set);
    if (bm_set.empty()) {
        return CSeqMatch_Info();
    }
    TSeqMatchSet::iterator bm_it = bm_set.begin();
    CSeqMatch_Info best = *bm_it;
    ++bm_it;
    if (bm_it == bm_set.end()) {
        CMutexGuard guard(m_Scope_Cache_Mtx);
        m_DS_Cache[&best.GetDataSource()].
            insert(TTSE_Lock(&best.GetTSE_Info()));
        return best;
    }
    // More than one TSE found in different data sources.
    CSeqMatch_Info over = *bm_it;
    x_ThrowConflict(eConflict_Live, best, over);
    return CSeqMatch_Info(); // Just to avoid compiler warnings.
}


void CScope::UpdateAnnotIndex(const CHandleRangeMap& loc,
                              const SAnnotTypeSelector& sel)
{
    CReadLockGuard rguard(m_Scope_Conf_RWLock);
    for (CPriority_I it(m_setDataSrc); it; ++it) {
        it->UpdateAnnotIndex(loc, sel);
    }
}


void CScope::UpdateAnnotIndex(const CHandleRangeMap& loc,
                              const SAnnotTypeSelector& sel,
                              const CSeq_entry& entry)
{
    CReadLockGuard rguard(m_Scope_Conf_RWLock);
    for (CPriority_I it(m_setDataSrc); it; ++it) {
        CConstRef<CSeq_entry_Info> entry_info = it->GetSeq_entry_Info(entry);
        if ( entry_info ) {
            it->UpdateAnnotIndex(loc, sel, *entry_info);
            break;
        }
    }
}


void CScope::UpdateAnnotIndex(const CHandleRangeMap& loc,
                              const SAnnotTypeSelector& sel,
                              const CSeq_annot& annot)
{
    CReadLockGuard rguard(m_Scope_Conf_RWLock);
    for (CPriority_I it(m_setDataSrc); it; ++it) {
        CConstRef<CSeq_annot_Info> annot_info = it->GetSeq_annot_Info(annot);
        if ( annot_info ) {
            it->UpdateAnnotIndex(loc, sel, *annot_info);
            break;
        }
    }
}


CConstRef<CScope::TAnnotRefSet>
CScope::GetTSESetWithAnnots(const CSeq_id_Handle& idh)
{
    CReadLockGuard rguard(m_Scope_Conf_RWLock);
    CMutexGuard guard(m_Scope_Cache_Mtx);
    TAnnotCache::iterator cached = m_AnnotCache.find(idh);
    if (cached != m_AnnotCache.end()) {
        return cached->second;
    }

    CBioseq_Handle bh = GetBioseqHandle(idh);
    // Create new entry for idh
    CRef<TAnnotRefSet>& tse_set_ref = m_AnnotCache[idh];
    tse_set_ref.Reset(new TAnnotRefSet);
    TTSE_LockSet& tse_set = *tse_set_ref;

    if ( bh ) {
        tse_set.insert(TTSE_Lock(&bh.m_Bioseq_Info->GetTSE_Info()));
    }

    TTSE_LockSet with_ref;
    for (CPriority_I it(m_setDataSrc); it; ++it) {
        it->GetTSESetWithAnnots(idh, with_ref);
        TTSE_LockSet& tse_cache = m_DS_Cache[&*it];
        NON_CONST_ITERATE(TTSE_LockSet, ref_it, with_ref) {
            if (tse_cache.find(*ref_it) != tse_cache.end()  ||
                !(*ref_it)->IsDead()) {
                tse_set.insert(*ref_it);
            }
        }
        with_ref.clear();
    }
    return tse_set_ref;
}


CScope::TTSE_Lock CScope::GetTSEInfo(const CSeq_entry* tse)
{
    CReadLockGuard rguard(m_Scope_Conf_RWLock);
    TTSE_Lock ret;
    for (CPriority_I it(m_setDataSrc); it; ++it) {
        ret = it->GetTSEInfo(*tse);
        if ( ret )
            break;
    }
    return ret;
}


const CSeq_entry& CScope::x_GetTSEFromInfo(const TTSE_Lock& tse)
{
    return tse->GetDataSource().GetTSEFromInfo(tse);
}


void CScope::SetFindMode(EFindMode mode)
{
    m_FindMode = mode;
}


CSeq_id_Mapper& CScope::x_GetIdMapper(void) const
{
    return CSeq_id_Mapper::GetSeq_id_Mapper();
}


CSeq_id_Handle CScope::GetIdHandle(const CSeq_id& id) const
{
    return x_GetIdMapper().GetHandle(id);
}


void CScope::x_AddBioseqToCache(CBioseq_Info& info, const CSeq_id_Handle* id)
{
    ITERATE(CBioseq_Info::TSynonyms, syn, info.m_Synonyms) {
        CBioseq_Handle bsh(*syn, *this, info);
        m_Cache[*syn] = bsh;
    }
    if ( id ) {
        CBioseq_Handle bsh(*id, *this, info);
        m_Cache[*id] = bsh;
    }
}


void CScope::x_ThrowConflict(EConflict conflict_type,
                             const CSeqMatch_Info& info1,
                             const CSeqMatch_Info& info2) const
{
    const char* msg =
        conflict_type == eConflict_History? "history": "live TSE";
#if 0
    ERR_POST(Error << "CScope -- multiple " << msg << " matches: " <<
             info1.GetDataSource().GetName() << "::" <<
             x_GetIdMapper().GetSeq_id(info1.GetIdHandle()).DumpAsFasta() <<
             " vs " <<
             info2.GetDataSource().GetName() << "::" <<
             x_GetIdMapper().GetSeq_id(info2.GetIdHandle()).DumpAsFasta());
    {{
        CNcbiOstrstream s;
        s <<
            "---------------------------------------------------\n" <<
            "CTSE_Info1: " << &info1.GetTSE_Info() <<
            "  TSE1: " << &info1.GetTSE_Info().GetTSE() <<
            "  dead: " << info1.GetTSE_Info().IsDead() << "\n" <<
            "---------------------------------------------------\n" <<
            "CTSE_Info2: " << &info2.GetTSE_Info() <<
            "  TSE2: " << &info2.GetTSE_Info().GetTSE() <<
            "  dead: " << info2.GetTSE_Info().IsDead() << "\n" <<
            "---------------------------------------------------\n";
        if ( &info1.GetTSE_Info().GetTSE() != &info2.GetTSE_Info().GetTSE() ) {
            auto_ptr<CObjectOStream> out(CObjectOStream::Open(eSerial_AsnText,
                                                              s));
            s << "-- Seq_entry 1 ----------------------------------\n";
            *out << info1.GetTSE_Info().GetTSE();
            out->Flush();
            s << "-- Seq_entry 2 ----------------------------------\n";
            *out << info2.GetTSE_Info().GetTSE();
            out->Flush();
            s << "-------------------------------------------------\n";
        }
        NcbiCerr << string(CNcbiOstrstreamToString(s));
    }}
#endif
    ERR_POST(Fatal << "CScope -- multiple " << msg << " matches: " <<
             info1.GetDataSource().GetName() << "::" <<
             x_GetIdMapper().GetSeq_id(info1.GetIdHandle()).DumpAsFasta() <<
             " vs " <<
             info2.GetDataSource().GetName() << "::" <<
             x_GetIdMapper().GetSeq_id(info2.GetIdHandle()).DumpAsFasta());
}


void CScope::ResetHistory(void)
{
    CReadLockGuard rguard(m_Scope_Conf_RWLock);
    CMutexGuard guard(m_Scope_Cache_Mtx);
    m_Cache.clear();
    m_SynCache.clear();
    m_AnnotCache.clear();
    m_DS_Cache.clear();
}


void CScope::x_PopulateBioseq_HandleSet(const CSeq_entry& tse,
                                        set<CBioseq_Handle>& handles,
                                        CSeq_inst::EMol filter)
{
    CReadLockGuard rguard(m_Scope_Conf_RWLock);
    CMutexGuard guard(m_Scope_Cache_Mtx);
    TTSE_Lock tse_lock(0);
    set<CBioseq_Info*> info_set;
    for (CPriority_I it(m_setDataSrc); it; ++it) {
        tse_lock = it->GetTSEHandles(tse, info_set, filter);
        if (tse_lock) {
            // Convert each bioseq info into bioseq handle
            ITERATE (set<CBioseq_Info*>, iit, info_set) {
                CBioseq_Handle h(*(*iit)->m_Synonyms.begin(), *this, **iit);
                handles.insert(h);
                x_AddBioseqToCache(**iit);
            }
            break;
        }
    }
}


const CSynonymsSet& CScope::GetSynonyms(const CSeq_id& id)
{
    return GetSynonyms(GetIdHandle(id));
}


const CSynonymsSet& CScope::GetSynonyms(const CSeq_id_Handle& id)
{
    const CSynonymsSet* ret = x_GetSynonyms(id);
    if ( !ret ) {
        NCBI_THROW(CException, eUnknown, "GetSynonyms failed");
    }
    return *ret;
}


const CSynonymsSet* CScope::x_GetSynonyms(const CSeq_id_Handle& id)
{
    CReadLockGuard rguard(m_Scope_Conf_RWLock);
    {{
        CMutexGuard guard(m_Scope_Cache_Mtx);
        TSynCache::const_iterator cached = m_SynCache.find(id);
        if (cached != m_SynCache.end()) {
            return cached->second;
        }
    }}
    // Temporary implementation. May change when datasources will
    // be able of getting synonyms without the whole sequences.
    // Then we will need to filter results like in x_BestResolve.
    CBioseq_Handle h = GetBioseqHandle(id);
    if ( !h ) {
        return 0;
    }
    // It's OK to use CRef, at least one copy should be kept
    // alive by the id cache (for the ID requested).
    {{
        CMutexGuard guard(m_Scope_Cache_Mtx);
        CRef<CSynonymsSet>& synset = m_SynCache[id];
        if ( !synset ) {
            ITERATE(CBioseq_Info::TSynonyms, it, h.m_Bioseq_Info->m_Synonyms) {
                // Check current ID for conflicts, add to the set.
                TCache::const_iterator seq = m_Cache.find(*it);
                if (seq != m_Cache.end()  &&  seq->second != h) {
                    // already cached for a different bioseq - ignore the id
                    continue;
                }
                CRef<CSynonymsSet>& synset2 = m_SynCache[*it];
                if ( !synset ) {
                    if ( !synset2 ) {
                        synset2.Reset(new CSynonymsSet);
                    }
                    synset = synset2;
                }
                else if ( !synset2 ) {
                    synset2 = synset;
                }
                _ASSERT(synset == synset2);
                synset2->AddSynonym(*it);
            }
            _ASSERT(synset);
        }
        return synset;
    }}
}


void
CScope::x_DetachFromOM(void)
{
    CWriteLockGuard rguard(m_Scope_Conf_RWLock);
    // Drop and release all TSEs
    ResetHistory();
    if(m_pObjMgr) {
        m_pObjMgr->ReleaseDataSources(m_setDataSrc);
        m_pObjMgr->RevokeScope(*this);
        m_pObjMgr=0;
    }
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
    DebugDumpValue(ddc,"m_FindMode", m_FindMode);
}


/////////////////////////////////////////////////////////////////////////////
// CSynonymsSet
/////////////////////////////////////////////////////////////////////////////

CSynonymsSet::CSynonymsSet(void)
{
}


CSynonymsSet::~CSynonymsSet(void)
{
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
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
