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
*   DataSource for object manager
*
*/


#include <ncbi_pch.hpp>
#include <objmgr/impl/data_source.hpp>

#include <objmgr/impl/annot_object.hpp>
#include <objmgr/impl/handle_range_map.hpp>
#include <objmgr/impl/seq_entry_info.hpp>
#include <objmgr/impl/seq_annot_info.hpp>
#include <objmgr/impl/bioseq_info.hpp>
#include <objmgr/impl/bioseq_set_info.hpp>
#include <objmgr/impl/tse_info.hpp>
#include <objmgr/seqmatch_info.hpp>
#include <objmgr/data_loader.hpp>
#include <objmgr/seq_map.hpp>
#include <objmgr/objmgr_exception.hpp>
#include <objmgr/bioseq_ci.hpp> // for CBioseq_CI_Base

#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Seq_point.hpp>
#include <objects/seqloc/Seq_loc_equiv.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objects/seq/seqport_util.hpp>

#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>

#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqres/Seq_graph.hpp>

#include <objmgr/impl/prefetch_impl.hpp>
#include <corelib/ncbimtx.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


CTSE_LockingSet::CTSE_LockingSet(void)
    : m_LockCount(0)
{
}


CTSE_LockingSet::CTSE_LockingSet(const CTSE_LockingSet& tse_set)
    : m_LockCount(0), m_TSE_set(tse_set.m_TSE_set)
{
}


CTSE_LockingSet::~CTSE_LockingSet(void)
{
    _ASSERT(!x_Locked());
}


DEFINE_CLASS_STATIC_FAST_MUTEX(CTSE_LockingSet::sm_Mutex);

CTSE_LockingSet& CTSE_LockingSet::operator=(const CTSE_LockingSet& tse_set)
{
    CFastMutexGuard guard(sm_Mutex);
    _ASSERT(!x_Locked());
    m_TSE_set = tse_set.m_TSE_set;
    return *this;
}


bool CTSE_LockingSet::insert(CTSE_Info* tse)
{
    CFastMutexGuard guard(sm_Mutex);
    bool ins = m_TSE_set.insert(tse).second;
    if ( ins && x_Locked() ) {
        tse->AddReference();
    }
    return ins;
}


bool CTSE_LockingSet::erase(CTSE_Info* tse)
{
    CFastMutexGuard guard(sm_Mutex);
    if ( m_TSE_set.erase(tse) ) {
        if ( x_Locked() ) {
            tse->RemoveReference();
        }
        return true;
    }
    return false;
}


void CTSE_LockingSet::x_Lock(void)
{
    CFastMutexGuard guard(sm_Mutex);
    if ( m_LockCount++ == 0 ) {
        NON_CONST_ITERATE( TTSESet, it, m_TSE_set ) {
            (*it)->AddReference();
        }
    }
    _ASSERT(m_LockCount > 0);
}


void CTSE_LockingSet::x_Unlock(void)
{
    CFastMutexGuard guard(sm_Mutex);
    _ASSERT(m_LockCount > 0);
    if ( --m_LockCount == 0 ) {
        NON_CONST_ITERATE( TTSESet, it, m_TSE_set ) {
            (*it)->RemoveReference();
        }
    }
}


CDataSource::CDataSource(CDataLoader& loader, CObjectManager& objmgr)
    : m_Loader(&loader),
      m_ObjMgr(&objmgr),
      m_DefaultPriority(99)
{
    m_Loader->SetTargetDataSource(*this);
}


CDataSource::CDataSource(CSeq_entry& entry, CObjectManager& objmgr)
    : m_Loader(0),
      m_pTopEntry(&entry),
      m_ObjMgr(&objmgr),
      m_DefaultPriority(9)
{
    AddTSE(entry, false);
}


CDataSource::~CDataSource(void)
{
    if (m_PrefetchThread) {
        // Wait for the prefetch thread to stop
        m_PrefetchThread->Terminate();
        m_PrefetchThread->Join();
    }
    DropAllTSEs();
    m_Loader.Reset();
}


CConstRef<CTSE_Info> CDataSource::GetTopEntry_Info(void)
{
    CConstRef<CTSE_Info> ret;
    {{
        TMainReadLockGuard guard(m_DSMainLock);    
        ITERATE ( TTSE_Set, it, m_TSE_Set ) {
            if ( (*it)->GetSeq_entryCore() == m_pTopEntry ) {
                ret = *it;
                break;
            }
        }
    }}
    return ret;
}


void CDataSource::DropAllTSEs(void)
{
    // Lock indexes
    TMainWriteLockGuard guard(m_DSMainLock);    

    ITERATE ( TTSE_Set, it, m_TSE_Set ) {
        if ( (*it)->Locked() ) {
            ERR_POST("CDataSource::DropAllTSEs: tse is locked");
            NCBI_THROW(CObjMgrException, eOtherError,
                       "CDataSource::DropAllTSEs: tse is locked");
        }
    }

    if ( m_Loader ) {
        NON_CONST_ITERATE ( TTSE_Set, it, m_TSE_Set ) {
            m_Loader->DropTSE(**it);
        }
    }

    //m_Bioseq_InfoMap.clear();
    //m_Seq_annot_InfoMap.clear();
    //m_Seq_entry_InfoMap.clear();
    //m_TSE_InfoMap.clear();
    m_TSE_Set.clear();
    m_pTopEntry.Reset();
    m_TSE_seq.clear();

    {{
        TAnnotWriteLockGuard guard2(m_DSAnnotLock);
        m_TSE_annot.clear();
        m_DirtyAnnot_TSEs.clear();
    }}
}


TTSE_Lock CDataSource::x_FindBestTSE(const CSeq_id_Handle& handle) const
{
    TTSE_LockSet all_tse;
    size_t all_count = 0;
    {{
        TMainReadLockGuard guard(m_DSMainLock);
        TTSEMap::const_iterator tse_set = m_TSE_seq.find(handle);
        if ( tse_set == m_TSE_seq.end() ) {
            return TTSE_Lock();
        }
        ITERATE ( CTSE_LockingSet, it, tse_set->second ) {
            if ( all_tse.insert(TTSE_Lock(*it)).second )
                ++all_count;
        }
        if ( all_count == 0 ) {
            return TTSE_Lock();
        }
    }}
    if ( all_count == 1 ) {
        // There is only one TSE, no matter live or dead
        return TTSE_Lock(*all_tse.begin());
    }
    // The map should not contain empty entries
    _ASSERT(!all_tse.empty());
    TTSE_LockSet live_tse;
    size_t live_count = 0;
    ITERATE ( TTSE_LockSet, tse, all_tse ) {
        // Find live TSEs
        if ( !(*tse)->IsDead() ) {
            live_tse.insert(*tse);
            ++live_count;
        }
    }

    // Check live
    if ( live_count == 1 ) {
        // There is only one live TSE -- ok to use it
        return *live_tse.begin();
    }
    else if ( live_count == 0 ) {
        if ( m_Loader ) {
            TTSE_Lock best(GetDataLoader()->ResolveConflict(handle, all_tse));
            if ( best ) {
                return best;
            }
        }
        // No live TSEs -- try to select the best dead TSE
        NCBI_THROW(CObjMgrException, eFindConflict,
                   "Multiple seq-id matches found");
    }
    if ( m_Loader ) {
        // Multiple live TSEs - try to resolve the conflict (the status of some
        // TSEs may change)
        TTSE_Lock best(GetDataLoader()->ResolveConflict(handle, live_tse));
        if ( best ) {
            return best;
        }
    }
    NCBI_THROW(CObjMgrException, eFindConflict,
               "Multiple live entries found");
}


TTSE_Lock CDataSource::GetBlobById(const CSeq_id_Handle& idh)
{
    TMainReadLockGuard guard(m_DSMainLock);
    TTSEMap::iterator tse_set = m_TSE_seq.find(idh);
    if (tse_set == m_TSE_seq.end()) {
        // Request TSE-info from loader if any
        if ( m_Loader ) {
            //
        }
        else {
            // No such blob, no loader to call
            return TTSE_Lock();
        }
//###
    }
//###
    return TTSE_Lock();
}


CConstRef<CBioseq_Info> CDataSource::GetBioseq_Info(const CSeqMatch_Info& info)
{
    CRef<CBioseq_Info> ret;
    // The TSE is locked by the scope, so, it can not be deleted.
    CTSE_Info::TBioseqs::const_iterator found =
        info.GetTSE_Info().m_Bioseqs.find(info.GetIdHandle());
    if ( found != info.GetTSE_Info().m_Bioseqs.end() ) {
        ret = found->second;
    }
    return ret;
}


CRef<CTSE_Info> CDataSource::AddTSE(CSeq_entry& tse,
                                    bool dead,
                                    const CObject* blob_id)
{
    CRef<CTSE_Info> info(new CTSE_Info(tse, dead, blob_id));
    AddTSE(info);
    return info;
}


void CDataSource::AddTSE(CRef<CTSE_Info> info)
{
    TMainWriteLockGuard guard(m_DSMainLock);
    _VERIFY(m_TSE_Set.insert(info).second);
    info->x_DSAttach(*this);

}


bool CDataSource::DropTSE(CTSE_Info& info)
{
    TMainWriteLockGuard guard(m_DSMainLock);

    if ( info.Locked() ) {
        _TRACE("DropTSE: DS="<<this<<" TSE_Info="<<&info<<" - locked");
        return false; // Not really dropped, although found
    }
    CRef<CTSE_Info> info_lock(&info);
    x_DropTSE(info);
    return true;
}


void CDataSource::x_DropTSE(CTSE_Info& info)
{
    if ( m_Loader ) {
        m_Loader->DropTSE(info);
    }
    info.x_DSDetach(*this);
    _VERIFY(m_TSE_Set.erase(Ref(&info)));
    {{
        TAnnotWriteLockGuard guard2(m_DSAnnotLock);
        m_DirtyAnnot_TSEs.erase(&info);
    }}
}


template<class Map, class Ref, class Info>
inline
void x_MapObject(Map& info_map, const Ref& ref, const Info& info)
{
    typedef typename Map::value_type value_type;
    if ( !info_map.insert(value_type(ref, info)).second ) {
        NCBI_THROW(CObjMgrException, eOtherError,
                   "CDataSource::x_Map(): object already mapped");
    }
}


template<class Map, class Ref, class Info>
inline
void x_UnmapObject(Map& info_map, const Ref& ref, const Info& _DEBUG_ARG(info))
{
    typename Map::iterator iter = info_map.lower_bound(ref);
    if ( iter != info_map.end() && iter->first == ref ) {
        _ASSERT(iter->second == info);
        info_map.erase(iter);
    }
}


void CDataSource::x_Map(CConstRef<CSeq_entry> obj, CTSE_Info* info)
{
    x_MapObject(m_TSE_InfoMap, obj, info);
}


void CDataSource::x_Unmap(CConstRef<CSeq_entry> obj, CTSE_Info* info)
{
    x_UnmapObject(m_TSE_InfoMap, obj, info);
}


void CDataSource::x_Map(CConstRef<CSeq_entry> obj, CSeq_entry_Info* info)
{
    x_MapObject(m_Seq_entry_InfoMap, obj, info);
}


void CDataSource::x_Unmap(CConstRef<CSeq_entry> obj, CSeq_entry_Info* info)
{
    x_UnmapObject(m_Seq_entry_InfoMap, obj, info);
}


void CDataSource::x_Map(CConstRef<CSeq_annot> obj, CSeq_annot_Info* info)
{
    x_MapObject(m_Seq_annot_InfoMap, obj, info);
}


void CDataSource::x_Unmap(CConstRef<CSeq_annot> obj, CSeq_annot_Info* info)
{
    x_UnmapObject(m_Seq_annot_InfoMap, obj, info);
}


void CDataSource::x_Map(CConstRef<CBioseq> obj, CBioseq_Info* info)
{
    x_MapObject(m_Bioseq_InfoMap, obj, info);
}


void CDataSource::x_Unmap(CConstRef<CBioseq> obj, CBioseq_Info* info)
{
    x_UnmapObject(m_Bioseq_InfoMap, obj, info);
}


/////////////////////////////////////////////////////////////////////////////
//   mapping of various XXX_Info
// CDataSource must be guarded by mutex
/////////////////////////////////////////////////////////////////////////////

CConstRef<CTSE_Info>
CDataSource::FindTSEInfo(const CSeq_entry& tse)
{
    TMainReadLockGuard guard(m_DSMainLock);
    return x_FindTSE_Info(tse);
}


CConstRef<CSeq_entry_Info>
CDataSource::FindSeq_entry_Info(const CSeq_entry& entry)
{
    TMainReadLockGuard guard(m_DSMainLock);
    return x_FindSeq_entry_Info(entry);
}


CConstRef<CSeq_annot_Info>
CDataSource::FindSeq_annot_Info(const CSeq_annot& annot)
{
    TMainReadLockGuard guard(m_DSMainLock);
    return x_FindSeq_annot_Info(annot);
}


CConstRef<CBioseq_Info>
CDataSource::FindBioseq_Info(const CBioseq& bioseq)
{
    TMainReadLockGuard guard(m_DSMainLock);
    return x_FindBioseq_Info(bioseq);
}


CConstRef<CTSE_Info>
CDataSource::x_FindTSE_Info(const CSeq_entry& obj)
{
    CConstRef<CTSE_Info> ret;
    TTSE_InfoMap::iterator found = m_TSE_InfoMap.find(ConstRef(&obj));
    if ( found != m_TSE_InfoMap.end() ) {
        ret.Reset(found->second);
    }
    return ret;
}


CConstRef<CSeq_entry_Info>
CDataSource::x_FindSeq_entry_Info(const CSeq_entry& obj)
{
    CConstRef<CSeq_entry_Info> ret;
    TSeq_entry_InfoMap::iterator found =
        m_Seq_entry_InfoMap.find(ConstRef(&obj));
    if ( found != m_Seq_entry_InfoMap.end() ) {
        ret.Reset(found->second);
    }
    return ret;
}


CConstRef<CSeq_annot_Info>
CDataSource::x_FindSeq_annot_Info(const CSeq_annot& obj)
{
    CConstRef<CSeq_annot_Info> ret;
    TSeq_annot_InfoMap::iterator found =
        m_Seq_annot_InfoMap.find(ConstRef(&obj));
    if ( found != m_Seq_annot_InfoMap.end() ) {
        ret.Reset(found->second);
    }
    return ret;
}


CConstRef<CBioseq_Info>
CDataSource::x_FindBioseq_Info(const CBioseq& obj)
{
    CConstRef<CBioseq_Info> ret;
    TBioseq_InfoMap::iterator found =
        m_Bioseq_InfoMap.find(ConstRef(&obj));
    if ( found != m_Bioseq_InfoMap.end() ) {
        ret.Reset(found->second);
    }
    return ret;
}

/////////////////////////////////////////////////////////////////////////////


CRef<CSeq_entry_Info> CDataSource::AttachEntry(CBioseq_set_Info& parent,
                                               CSeq_entry& entry,
                                               int index)
{
    if ( m_Loader ) {
        NCBI_THROW(CObjMgrException, eModifyDataError,
                   "Can not remove a loaded entry");
    }
    TMainWriteLockGuard guard(m_DSMainLock);
    return parent.AddEntry(entry, index);
}


void CDataSource::RemoveEntry(CSeq_entry_Info& entry)
{
    if ( m_Loader ) {
        NCBI_THROW(CObjMgrException, eModifyDataError,
                   "Can not remove a loaded entry");
    }
    if ( !entry.HasParent_Info() ) {
        // Top level entry
        NCBI_THROW(CObjMgrException, eModifyDataError,
                   "Can not remove top level seq-entry from a data source");
    }

    TMainWriteLockGuard guard(m_DSMainLock);
    CBioseq_set_Info& parent = entry.GetParentBioseq_set_Info();
    parent.RemoveEntry(Ref(&entry));
}


CRef<CSeq_annot_Info> CDataSource::AttachAnnot(CSeq_entry_Info& entry_info,
                                               const CSeq_annot& annot)
{
    if ( m_Loader ) {
        NCBI_THROW(CObjMgrException, eModifyDataError,
                   "Can not modify a loaded entry");
    }

    TMainWriteLockGuard guard(m_DSMainLock);
    return entry_info.AddAnnot(annot);
}


CRef<CSeq_annot_Info> CDataSource::AttachAnnot(CBioseq_Base_Info& parent,
                                               const CSeq_annot& annot)
{
    if ( m_Loader ) {
        NCBI_THROW(CObjMgrException, eModifyDataError,
                   "Can not modify a loaded entry");
    }

    TMainWriteLockGuard guard(m_DSMainLock);
    return parent.AddAnnot(annot);
}


void CDataSource::RemoveAnnot(CSeq_annot_Info& annot)
{
    if ( m_Loader ) {
        NCBI_THROW(CObjMgrException, eModifyDataError,
                   "Can not modify a loaded entry");
    }

    TMainWriteLockGuard guard(m_DSMainLock);
    CBioseq_Base_Info& parent = annot.GetParentBioseq_Base_Info();
    parent.RemoveAnnot(Ref(&annot));
}


CRef<CSeq_annot_Info> CDataSource::ReplaceAnnot(CSeq_annot_Info& old_annot,
                                                const CSeq_annot& new_annot)
{
    if ( m_Loader ) {
        NCBI_THROW(CObjMgrException, eModifyDataError,
                   "Can not modify a loaded entry");
    }

    TMainWriteLockGuard guard(m_DSMainLock);
    CBioseq_Base_Info& parent = old_annot.GetParentBioseq_Base_Info();
    parent.RemoveAnnot(Ref(&old_annot));
    return parent.AddAnnot(new_annot);
}


void PrintSeqMap(const string& /*id*/, const CSeqMap& /*smap*/)
{
#if _DEBUG && 0
    _TRACE("CSeqMap("<<id<<"):");
    ITERATE ( CSeqMap, it, smap ) {
        switch ( it.GetType() ) {
        case CSeqMap::eSeqGap:
            _TRACE("    gap: "<<it.GetLength());
            break;
        case CSeqMap::eSeqData:
            _TRACE("    data: "<<it.GetLength());
            break;
        case CSeqMap::eSeqRef:
            _TRACE("    ref: "<<it.GetRefSeqid().AsString()<<' '<<
                   it.GetRefPosition()<<' '<<it.GetLength()<<' '<<
                   it.GetRefMinusStrand());
            break;
        default:
            _TRACE("    bad: "<<it.GetType()<<' '<<it.GetLength());
            break;
        }
    }
    _TRACE("end of CSeqMap "<<id);
#endif
}


void CDataSource::x_SetDirtyAnnotIndex(CTSE_Info& tse)
{
    _ASSERT(tse.x_DirtyAnnotIndex());
    TAnnotWriteLockGuard guard(m_DSAnnotLock);
    _VERIFY(m_DirtyAnnot_TSEs.insert(&tse).second);
}


void CDataSource::x_ResetDirtyAnnotIndex(CTSE_Info& tse)
{
    _ASSERT(!tse.x_DirtyAnnotIndex());
    _VERIFY(m_DirtyAnnot_TSEs.erase(&tse));
}


void CDataSource::UpdateAnnotIndex(void)
{
    TMainWriteLockGuard guard(m_DSMainLock);
    while ( !m_DirtyAnnot_TSEs.empty() ) {
        CTSE_Info* tse_info = *m_DirtyAnnot_TSEs.begin();
        tse_info->UpdateAnnotIndex();
        _ASSERT(m_DirtyAnnot_TSEs.empty() ||
                *m_DirtyAnnot_TSEs.begin() != tse_info);

    }
}


void CDataSource::UpdateAnnotIndex(const CSeq_entry_Info& entry_info)
{
    TMainWriteLockGuard guard(m_DSMainLock);
    entry_info.UpdateAnnotIndex();
}


void CDataSource::UpdateAnnotIndex(const CSeq_annot_Info& annot_info)
{
    TMainWriteLockGuard guard(m_DSMainLock);
    annot_info.UpdateAnnotIndex();
}


void CDataSource::GetTSESetWithAnnots(const CSeq_id_Handle& idh,
                                      TTSE_LockSet& with_ref)
{
    // load all relevant TSEs
    if ( m_Loader ) {
        m_Loader->GetRecords(idh, CDataLoader::eAnnot);
    }

    UpdateAnnotIndex();

    // collect all relevant TSEs
    TTSEMap::const_iterator rtse_it = m_TSE_annot.find(idh);
    if ( rtse_it != m_TSE_annot.end() ) {
        ITERATE(CTSE_LockingSet::TTSESet, tse, rtse_it->second) {
            if ( (*tse)->ContainsSeqid(idh) ) {
                // skip TSE containing sequence
                continue;
            }
            with_ref.insert(TTSE_Lock(*tse));
        }
    }
}


void CDataSource::GetSynonyms(const CSeq_id_Handle& main_idh,
                              set<CSeq_id_Handle>& syns)
{
    //### The TSE returns locked, unlock it
    CSeq_id_Handle idh = main_idh;
    TTSE_Lock tse_info(x_FindBestTSE(main_idh));
    if ( !tse_info ) {
        // Try to find the best matching id (not exactly equal)
        CSeq_id_Mapper& mapper = CSeq_id_Mapper::GetSeq_id_Mapper();
        if ( mapper.HaveMatchingHandles(idh) ) {
            TSeq_id_HandleSet hset;
            mapper.GetMatchingHandles(idh, hset);
            ITERATE(TSeq_id_HandleSet, hit, hset) {
                if ( *hit == main_idh ) // already checked
                    continue;
                if ( bool(tse_info)  &&  idh.IsBetter(*hit) ) // worse hit
                    continue;
                TTSE_Lock tmp_tse(x_FindBestTSE(*hit));
                if ( tmp_tse ) {
                    tse_info = tmp_tse;
                    idh = *hit;
                }
            }
        }
    }
    // At this point the tse_info (if not null) should be locked
    if ( tse_info ) {
        CTSE_Info::TBioseqs::const_iterator info =
            tse_info->m_Bioseqs.find(idh);
        if (info == tse_info->m_Bioseqs.end()) {
            // Just copy the existing id
            syns.insert(main_idh);
        }
        else {
            // Create range list for each synonym of a seq_id
            const CBioseq_Info::TId& syn = info->second->GetId();
            ITERATE ( CBioseq_Info::TId, syn_it, syn ) {
                syns.insert(*syn_it);
            }
        }
    }
    else {
        // Just copy the existing range map
        syns.insert(main_idh);
    }
}


void CDataSource::x_IndexTSE(TTSEMap& tse_map,
                             const CSeq_id_Handle& id,
                             CTSE_Info* tse_info)
{
    TTSEMap::iterator it = tse_map.lower_bound(id);
    if ( it == tse_map.end() || it->first != id ) {
        it = tse_map.insert(it, TTSEMap::value_type(id, CTSE_LockingSet()));
    }
    _ASSERT(it != tse_map.end() && it->first == id);
    it->second.insert(tse_info);
}


void CDataSource::x_UnindexTSE(TTSEMap& tse_map,
                               const CSeq_id_Handle& id,
                               CTSE_Info* tse_info)
{
    TTSEMap::iterator it = tse_map.find(id);
    if (it == tse_map.end() || it->first != id) {
        return;
    }
    it->second.erase(tse_info);
    if ( it->second.empty() ) {
        tse_map.erase(it);
    }
}


void CDataSource::x_IndexSeqTSE(const CSeq_id_Handle& id,
                                CTSE_Info* tse_info)
{
    // no need to lock as it's locked by callers
    x_IndexTSE(m_TSE_seq, id, tse_info);
}


void CDataSource::x_UnindexSeqTSE(const CSeq_id_Handle& id,
                                  CTSE_Info* tse_info)
{
    // no need to lock as it's locked by callers
    x_UnindexTSE(m_TSE_seq, id, tse_info);
}


void CDataSource::x_IndexAnnotTSE(const CSeq_id_Handle& id,
                                  CTSE_Info* tse_info)
{
    TAnnotWriteLockGuard guard(m_DSAnnotLock);
    if (tse_info) {
        // Filter out TSEs which contain the sequence
        CSeq_id_Mapper& mapper = CSeq_id_Mapper::GetSeq_id_Mapper();
        TSeq_id_HandleSet hset;
        if (mapper.HaveMatchingHandles(id)) {
            mapper.GetMatchingHandles(id, hset);
        }
        else {
            hset.insert(id);
        }
        ITERATE(TSeq_id_HandleSet, match_it, hset) {
            if ( tse_info->ContainsSeqid(*match_it) ) {
                return;
            }
        }
    }
    x_IndexTSE(m_TSE_annot, id, tse_info);
}


void CDataSource::x_UnindexAnnotTSE(const CSeq_id_Handle& id,
                                    CTSE_Info* tse_info)
{
    TAnnotWriteLockGuard guard(m_DSAnnotLock);
    x_UnindexTSE(m_TSE_annot, id, tse_info);
}


void CDataSource::x_IndexAnnotTSEs(CTSE_Info* tse_info)
{
    TAnnotWriteLockGuard guard(m_DSAnnotLock);
    ITERATE ( CTSE_Info::TSeqIdToNames, it, tse_info->m_SeqIdToNames ) {
        x_IndexTSE(m_TSE_annot, it->first, tse_info);
    }
    if ( tse_info->x_DirtyAnnotIndex() ) {
        _VERIFY(m_DirtyAnnot_TSEs.insert(tse_info).second);
    }
}


void CDataSource::x_UnindexAnnotTSEs(CTSE_Info* tse_info)
{
    TAnnotWriteLockGuard guard(m_DSAnnotLock);
    ITERATE ( CTSE_Info::TSeqIdToNames, it, tse_info->m_SeqIdToNames ) {
        x_UnindexTSE(m_TSE_annot, it->first, tse_info);
    }
}


void CDataSource::x_CleanupUnusedEntries(void)
{
    // Lock indexes
    TMainWriteLockGuard guard(m_DSMainLock);    

    bool broken = true;
    while ( broken ) {
        broken = false;
        NON_CONST_ITERATE( TTSE_Set, it, m_TSE_Set ) {
            if ( !(*it)->Locked() ) {
                //### Lock the entry and check again
                x_DropTSE(const_cast<CTSE_Info&>(**it));
                broken = true;
                break;
            }
        }
    }
}


void CDataSource::x_UpdateTSEStatus(CTSE_Info& tse, bool dead)
{
    tse.m_Dead = dead;
}


CSeqMatch_Info CDataSource::BestResolve(CSeq_id_Handle idh)
{
    //### Lock all TSEs found, unlock all filtered out in the end.
    CTSE_LockingSetLock lock;
    {{
        TMainWriteLockGuard guard(m_DSMainLock);
        lock.Lock(m_TSE_seq[idh]);
    }}
    if ( m_Loader ) {
        // Send request to the loader
        m_Loader->GetRecords(idh, CDataLoader::eBioseqCore);
    }
    CSeqMatch_Info match;
    TTSE_Lock tse(x_FindBestTSE(idh));
    if ( !tse ) {
        // Try to find the best matching id (not exactly equal)
        CSeq_id_Mapper& mapper = CSeq_id_Mapper::GetSeq_id_Mapper();
        if ( mapper.HaveMatchingHandles(idh) ) {
            TSeq_id_HandleSet hset;
            mapper.GetMatchingHandles(idh, hset);
            ITERATE(TSeq_id_HandleSet, hit, hset) {
                if ( *hit == idh ) // already checked
                    continue;
                if ( bool(tse)  &&  idh.IsBetter(*hit) ) // worse hit
                    continue;
                TTSE_Lock tmp_tse(x_FindBestTSE(*hit));
                if ( tmp_tse ) {
                    tse = tmp_tse;
                    idh = *hit;
                }
            }
        }
    }
    if ( tse ) {
        match = CSeqMatch_Info(idh, *tse);
    }
    return match;
}


CSeqMatch_Info CDataSource::HistoryResolve(CSeq_id_Handle idh,
                                           const TTSE_LockSet& history)
{
    CSeqMatch_Info match;
    {{
        TMainReadLockGuard guard(m_DSMainLock);
        TTSEMap::const_iterator tse_set = m_TSE_seq.find(idh);
        if ( tse_set != m_TSE_seq.end() ) {
            ITERATE ( CTSE_LockingSet, it, tse_set->second ) {
                if ( history.find(TTSE_Lock(*it)) == history.end() ) {
                    continue;
                }
                
                CSeqMatch_Info new_match(idh, **it);
                _ASSERT(new_match.GetBioseq_Info());
                if ( !match ) {
                    match = new_match;
                    continue;
                }

                CNcbiOstrstream s;
                s << "CDataSource("<<GetName()<<"): "
                    "multiple history matches: Seq-id="<<idh.AsString()<<
                    ": seq1=["<<match.GetBioseq_Info()->IdString()<<
                    "] seq2=["<<new_match.GetBioseq_Info()->IdString()<<"]";
                string msg = CNcbiOstrstreamToString(s);
                NCBI_THROW(CObjMgrException, eFindConflict, msg);
            }
        }
    }}
    return match;
}


CSeqMatch_Info* CDataSource::ResolveConflict(const CSeq_id_Handle& id,
                                             CSeqMatch_Info& info1,
                                             CSeqMatch_Info& info2)
{
    if (&info1.GetDataSource() != this  ||
        &info2.GetDataSource() != this) {
        // Can not compare TSEs from different data sources or
        // without a loader.
        return 0;
    }
    if (!m_Loader) {
        if ( info1.GetIdHandle().IsBetter(info2.GetIdHandle()) ) {
            return &info1;
        }
        if ( info2.GetIdHandle().IsBetter(info1.GetIdHandle()) ) {
            return &info2;
        }
        return 0;
    }
    TTSE_LockSet tse_set;
    tse_set.insert(TTSE_Lock(&info1.GetTSE_Info()));
    tse_set.insert(TTSE_Lock(&info2.GetTSE_Info()));
    CConstRef<CTSE_Info> tse = m_Loader->ResolveConflict(id, tse_set);
    if (tse == &info1.GetTSE_Info()) {
        return &info1;
    }
    if (tse == &info2.GetTSE_Info()) {
        return &info2;
    }
    return 0;
}


string CDataSource::GetName(void) const
{
    if ( m_Loader )
        return m_Loader->GetName();
    else
        return kEmptyStr;
}


void CDataSource::GetBioseqs(const CSeq_entry_Info& entry,
                             TBioseq_InfoSet& bioseqs,
                             CSeq_inst::EMol filter,
                             TBioseqLevelFlag level)
{
    // Find TSE_Info
    x_CollectBioseqs(entry, bioseqs, filter, level);
}


void CDataSource::x_CollectBioseqs(const CSeq_entry_Info& info,
                                   TBioseq_InfoSet& bioseqs,
                                   CSeq_inst::EMol filter,
                                   TBioseqLevelFlag level)
{
    // parts should be changed to all before adding bioseqs to the list
    if ( info.IsSeq() ) {
        const CBioseq_Info& seq = info.GetSeq();
        if ( level != CBioseq_CI_Base::eLevel_Parts &&
             (filter == CSeq_inst::eMol_not_set ||
              seq.GetInst_Mol() == filter) ) {
            bioseqs.push_back(ConstRef(&seq));
        }
    }
    else {
        const CBioseq_set_Info& set = info.GetSet();
        ITERATE( CBioseq_set_Info::TSeq_set, it, set.GetSeq_set() ) {
            const CSeq_entry_Info& sub_info = **it;
            TBioseqLevelFlag local_level = level;
            if ( sub_info.IsSet() &&
                 sub_info.GetSet().GetClass() == CBioseq_set::eClass_parts ) {
                switch (level) {
                case CBioseq_CI_Base::eLevel_Mains:
                    // Skip parts
                    continue;
                case CBioseq_CI_Base::eLevel_Parts:
                    // Allow adding bioseqs from lower levels
                    local_level = CBioseq_CI_Base::eLevel_All;
                    break;
                default:
                    break;
                }
            }
            x_CollectBioseqs(sub_info, bioseqs, filter, local_level);
        }
    }
}


void CDataSource::Prefetch(CPrefetchToken_Impl& token)
{
#if !defined(NCBI_NO_THREADS)
    if (!m_PrefetchThread) {
        CFastMutexGuard guard(m_PrefetchLock);
        // Check againi
        if (!m_PrefetchThread) {
            m_PrefetchThread.Reset(new CPrefetchThread(*this));
            m_PrefetchThread->Run();
        }
    }
    _ASSERT(m_PrefetchThread);
    m_PrefetchThread->AddRequest(token);
#endif
}


void CDataSource::DebugDump(CDebugDumpContext /*ddc*/,
                            unsigned int /*depth*/) const
{
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.134  2004/06/03 18:33:48  grichenk
* Modified annot collector to better resolve synonyms
* and matching IDs. Do not add id to scope history when
* collecting annots. Exclude TSEs with bioseqs from data
* source's annot index.
*
* Revision 1.133  2004/05/21 21:42:12  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.132  2004/04/16 13:31:47  grichenk
* Added data pre-fetching functions.
*
* Revision 1.131  2004/03/31 17:08:07  vasilche
* Implemented ConvertSeqToSet and ConvertSetToSeq.
*
* Revision 1.130  2004/03/24 18:30:29  vasilche
* Fixed edit API.
* Every *_Info object has its own shallow copy of original object.
*
* Revision 1.129  2004/03/16 15:47:27  vasilche
* Added CBioseq_set_Handle and set of EditHandles
*
* Revision 1.128  2004/02/25 19:23:46  shomrat
* Use of entry_info instead of tse_info to allow collection of bioseqs on non-tse entries
*
* Revision 1.127  2004/02/03 19:02:17  vasilche
* Fixed broken 'dirty annot index' state after RemoveEntry().
*
* Revision 1.126  2004/02/02 15:27:20  dicuccio
* +<algorithm> for find
*
* Revision 1.125  2004/02/02 14:46:43  vasilche
* Several performance fixed - do not iterate whole tse set in CDataSource.
*
* Revision 1.124  2003/12/18 16:38:06  grichenk
* Added CScope::RemoveEntry()
*
* Revision 1.123  2003/11/26 17:55:57  vasilche
* Implemented ID2 split in ID1 cache.
* Fixed loading of splitted annotations.
*
* Revision 1.122  2003/10/27 16:47:12  vasilche
* Fixed error:
* src/objmgr/data_source.cpp", line 913: Fatal: Assertion failed: (it != tse_map.end() && it->first == id)
*
* Revision 1.121  2003/10/07 13:43:23  vasilche
* Added proper handling of named Seq-annots.
* Added feature search from named Seq-annots.
* Added configurable adaptive annotation search (default: gene, cds, mrna).
* Fixed selection of blobs for loading from GenBank.
* Added debug checks to CSeq_id_Mapper for easier finding lost CSeq_id_Handles.
* Fixed leaked split chunks annotation stubs.
* Moved some classes definitions in separate *.cpp files.
*
* Revision 1.120  2003/09/30 16:22:02  vasilche
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
* Revision 1.119  2003/09/05 17:29:40  grichenk
* Structurized Object Manager exceptions
*
* Revision 1.118  2003/09/03 20:00:02  grichenk
* Added sequence filtering by level (mains/parts/all)
*
* Revision 1.117  2003/08/14 20:05:19  vasilche
* Simple SNP features are stored as table internally.
* They are recreated when needed using CFeat_CI.
*
* Revision 1.116  2003/08/04 17:04:31  grichenk
* Added default data-source priority assignment.
* Added support for iterating all annotations from a
* seq-entry or seq-annot.
*
* Revision 1.115  2003/07/17 22:51:31  vasilche
* Fixed unused variables warnings.
*
* Revision 1.114  2003/07/17 20:07:56  vasilche
* Reduced memory usage by feature indexes.
* SNP data is loaded separately through PUBSEQ_OS.
* String compression for SNP data.
*
* Revision 1.113  2003/07/09 17:54:29  dicuccio
* Fixed uninitialized variables in CDataSource and CSeq_annot_Info
*
* Revision 1.112  2003/07/01 18:01:08  vasilche
* Added check for null m_SeqMap pointer.
*
* Revision 1.111  2003/06/24 14:35:01  vasilche
* Fixed compilation in debug mode.
*
* Revision 1.110  2003/06/24 14:25:18  vasilche
* Removed obsolete CTSE_Guard class.
* Used separate mutexes for bioseq and annot maps.
*
* Revision 1.109  2003/06/19 18:23:45  vasilche
* Added several CXxx_ScopeInfo classes for CScope related information.
* CBioseq_Handle now uses reference to CBioseq_ScopeInfo.
* Some fine tuning of locking in CScope.
*
* Revision 1.104  2003/05/20 15:44:37  vasilche
* Fixed interaction of CDataSource and CDataLoader in multithreaded app.
* Fixed some warnings on WorkShop.
* Added workaround for memory leak on WorkShop.
*
* Revision 1.103  2003/05/14 18:39:28  grichenk
* Simplified TSE caching and filtering in CScope, removed
* some obsolete members and functions.
*
* Revision 1.102  2003/05/12 19:18:29  vasilche
* Fixed locking of object manager classes in multi-threaded application.
*
* Revision 1.101  2003/05/06 18:54:09  grichenk
* Moved TSE filtering from CDataSource to CScope, changed
* some filtering rules (e.g. priority is now more important
* than scope history). Added more caches to CScope.
*
* Revision 1.100  2003/05/05 20:59:47  vasilche
* Use one static mutex for all instances of CTSE_LockingSet.
*
* Revision 1.99  2003/04/29 19:51:13  vasilche
* Fixed interaction of Data Loader garbage collector and TSE locking mechanism.
* Made some typedefs more consistent.
*
* Revision 1.98  2003/04/24 16:12:38  vasilche
* Object manager internal structures are splitted more straightforward.
* Removed excessive header dependencies.
*
* Revision 1.97  2003/04/14 21:32:18  grichenk
* Avoid passing CScope as an argument to CDataSource methods
*
* Revision 1.96  2003/03/24 21:26:45  grichenk
* Added support for CTSE_CI
*
* Revision 1.95  2003/03/21 21:08:53  grichenk
* Fixed TTSE_Lock initialization
*
* Revision 1.93  2003/03/21 19:22:51  grichenk
* Redesigned TSE locking, replaced CTSE_Lock with CRef<CTSE_Info>.
*
* Revision 1.92  2003/03/18 14:52:59  grichenk
* Removed obsolete methods, replaced seq-id with seq-id handle
* where possible. Added argument to limit annotations update to
* a single seq-entry.
*
* Revision 1.91  2003/03/12 20:09:34  grichenk
* Redistributed members between CBioseq_Handle, CBioseq_Info and CTSE_Info
*
* Revision 1.90  2003/03/11 15:51:06  kuznets
* iterate -> ITERATE
*
* Revision 1.89  2003/03/11 14:15:52  grichenk
* +Data-source priority
*
* Revision 1.88  2003/03/10 16:55:17  vasilche
* Cleaned SAnnotSelector structure.
* Added shortcut when features are limited to one TSE.
*
* Revision 1.87  2003/03/05 20:56:43  vasilche
* SAnnotSelector now holds all parameters of annotation iterators.
*
* Revision 1.86  2003/03/03 20:31:08  vasilche
* Removed obsolete method PopulateTSESet().
*
* Revision 1.85  2003/02/27 14:35:31  vasilche
* Splitted PopulateTSESet() by logically independent parts.
*
* Revision 1.84  2003/02/25 20:10:40  grichenk
* Reverted to single total-range index for annotations
*
* Revision 1.83  2003/02/24 18:57:22  vasilche
* Make feature gathering in one linear pass using CSeqMap iterator.
* Do not use feture index by sub locations.
* Sort features at the end of gathering in one vector.
* Extracted some internal structures and classes in separate header.
* Delay creation of mapped features.
*
* Revision 1.82  2003/02/24 14:51:11  grichenk
* Minor improvements in annot indexing
*
* Revision 1.81  2003/02/13 14:34:34  grichenk
* Renamed CAnnotObject -> CAnnotObject_Info
* + CSeq_annot_Info and CAnnotObject_Ref
* Moved some members of CAnnotObject to CSeq_annot_Info
* and CAnnotObject_Ref.
* Added feat/align/graph to CAnnotObject_Info map
* to CDataSource.
*
* Revision 1.80  2003/02/05 17:59:17  dicuccio
* Moved formerly private headers into include/objects/objmgr/impl
*
* Revision 1.79  2003/02/04 22:01:26  grichenk
* Fixed annotations loading/indexing order
*
* Revision 1.78  2003/02/04 21:46:32  grichenk
* Added map of annotations by intervals (the old one was
* by total ranges)
*
* Revision 1.77  2003/01/29 22:03:46  grichenk
* Use single static CSeq_id_Mapper instead of per-OM model.
*
* Revision 1.76  2003/01/29 17:45:02  vasilche
* Annotaions index is split by annotation/feature type.
*
* Revision 1.75  2003/01/22 20:11:54  vasilche
* Merged functionality of CSeqMapResolved_CI to CSeqMap_CI.
* CSeqMap_CI now supports resolution and iteration over sequence range.
* Added several caches to CScope.
* Optimized CSeqVector().
* Added serveral variants of CBioseqHandle::GetSeqVector().
* Tried to optimize annotations iterator (not much success).
* Rewritten CHandleRange and CHandleRangeMap classes to avoid sorting of list.
*
* Revision 1.74  2002/12/26 16:39:24  vasilche
* Object manager class CSeqMap rewritten.
*
* Revision 1.73  2002/12/20 20:54:24  grichenk
* Added optional location/product switch to CFeat_CI
*
* Revision 1.72  2002/12/19 20:16:39  grichenk
* Fixed locations on minus strand
*
* Revision 1.71  2002/12/06 15:36:00  grichenk
* Added overlap type for annot-iterators
*
* Revision 1.70  2002/11/08 22:15:51  grichenk
* Added methods for removing/replacing annotations
*
* Revision 1.69  2002/11/08 21:03:30  ucko
* CConstRef<> now requires an explicit constructor.
*
* Revision 1.68  2002/11/04 21:29:12  grichenk
* Fixed usage of const CRef<> and CRef<> constructor
*
* Revision 1.67  2002/10/18 19:12:40  grichenk
* Removed mutex pools, converted most static mutexes to non-static.
* Protected CSeqMap::x_Resolve() with mutex. Modified code to prevent
* dead-locks.
*
* Revision 1.66  2002/10/16 20:44:16  ucko
* MIPSpro: use #pragma instantiate rather than template<>, as the latter
* ended up giving rld errors for CMutexPool_Base<*>::sm_Pool.  (Sigh.)
*
* Revision 1.65  2002/10/02 17:58:23  grichenk
* Added sequence type filter to CBioseq_CI
*
* Revision 1.64  2002/09/30 20:01:19  grichenk
* Added methods to support CBioseq_CI
*
* Revision 1.63  2002/09/16 19:59:36  grichenk
* Fixed getting reference to a gap on minus strand
*
* Revision 1.62  2002/09/10 19:55:49  grichenk
* Throw exception when unable to create resolved seq-map
*
* Revision 1.61  2002/08/09 14:58:50  ucko
* Restrict template <> to MIPSpro for now, as it also leads to link
* errors with Compaq's compiler.  (Sigh.)
*
* Revision 1.60  2002/08/08 19:51:16  ucko
* Omit EMPTY_TEMPLATE for GCC and KCC, as it evidently leads to link errors(!)
*
* Revision 1.59  2002/08/08 14:28:00  ucko
* Add EMPTY_TEMPLATE to explicit instantiations.
*
* Revision 1.58  2002/08/07 18:22:48  ucko
* Explicitly instantiate CMutexPool_Base<{CDataSource,TTSESet}>::sm_Pool
*
* Revision 1.57  2002/07/25 15:01:51  grichenk
* Replaced non-const GetXXX() with SetXXX()
*
* Revision 1.56  2002/07/08 20:51:01  grichenk
* Moved log to the end of file
* Replaced static mutex (in CScope, CDataSource) with the mutex
* pool. Redesigned CDataSource data locking.
*
* Revision 1.55  2002/07/01 15:44:51  grichenk
* Fixed typos
*
* Revision 1.54  2002/07/01 15:40:58  grichenk
* Fixed 'tse_set' warning.
* Removed duplicate call to tse_set.begin().
* Fixed version resolving for annotation iterators.
* Fixed strstream bug in KCC.
*
* Revision 1.53  2002/06/28 17:30:42  grichenk
* Duplicate seq-id: ERR_POST() -> THROW1_TRACE() with the seq-id
* list.
* Fixed x_CleanupUnusedEntries().
* Fixed a bug with not found sequences.
* Do not copy bioseq data in GetBioseqCore().
*
* Revision 1.52  2002/06/21 15:12:15  grichenk
* Added resolving seq-id to the best version
*
* Revision 1.51  2002/06/12 14:39:53  grichenk
* Performance improvements
*
* Revision 1.50  2002/06/04 17:18:33  kimelman
* memory cleanup :  new/delete/Cref rearrangements
*
* Revision 1.49  2002/05/31 17:53:00  grichenk
* Optimized for better performance (CTSE_Info uses atomic counter,
* delayed annotations indexing, no location convertions in
* CAnnot_Types_CI if no references resolution is required etc.)
*
* Revision 1.48  2002/05/29 21:21:13  gouriano
* added debug dump
*
* Revision 1.47  2002/05/28 18:00:43  gouriano
* DebugDump added
*
* Revision 1.46  2002/05/24 14:57:12  grichenk
* SerialAssign<>() -> CSerialObject::Assign()
*
* Revision 1.45  2002/05/21 18:57:25  grichenk
* Fixed annotations dropping
*
* Revision 1.44  2002/05/21 18:40:50  grichenk
* Fixed annotations droppping
*
* Revision 1.43  2002/05/14 20:06:25  grichenk
* Improved CTSE_Info locking by CDataSource and CDataLoader
*
* Revision 1.42  2002/05/13 15:28:27  grichenk
* Fixed seqmap for virtual sequences
*
* Revision 1.41  2002/05/09 14:18:15  grichenk
* More TSE conflict resolving rules for annotations
*
* Revision 1.40  2002/05/06 03:28:46  vakatov
* OM/OM1 renaming
*
* Revision 1.39  2002/05/03 21:28:09  ucko
* Introduce T(Signed)SeqPos.
*
* Revision 1.38  2002/05/02 20:42:37  grichenk
* throw -> THROW1_TRACE
*
* Revision 1.37  2002/04/22 20:05:08  grichenk
* Fixed minor bug in GetSequence()
*
* Revision 1.36  2002/04/19 18:02:47  kimelman
* add verify to catch coredump
*
* Revision 1.35  2002/04/17 21:09:14  grichenk
* Fixed annotations loading
* +IsSynonym()
*
* Revision 1.34  2002/04/11 18:45:39  ucko
* Pull in extra headers to make KCC happy.
*
* Revision 1.33  2002/04/11 12:08:21  grichenk
* Fixed GetResolvedSeqMap() implementation
*
* Revision 1.32  2002/04/05 21:23:08  grichenk
* More duplicate id warnings fixed
*
* Revision 1.31  2002/04/05 20:27:52  grichenk
* Fixed duplicate identifier warning
*
* Revision 1.30  2002/04/04 21:33:13  grichenk
* Fixed GetSequence() for sequences with unresolved segments
*
* Revision 1.29  2002/04/03 18:06:47  grichenk
* Fixed segmented sequence bugs (invalid positioning of literals
* and gaps). Improved CSeqVector performance.
*
* Revision 1.27  2002/03/28 14:02:31  grichenk
* Added scope history checks to CDataSource::x_FindBestTSE()
*
* Revision 1.26  2002/03/22 17:24:12  gouriano
* loader-related fix in DropTSE()
*
* Revision 1.25  2002/03/21 21:39:48  grichenk
* garbage collector bugfix
*
* Revision 1.24  2002/03/20 21:24:59  gouriano
* *** empty log message ***
*
* Revision 1.23  2002/03/15 18:10:08  grichenk
* Removed CRef<CSeq_id> from CSeq_id_Handle, added
* key to seq-id map th CSeq_id_Mapper
*
* Revision 1.22  2002/03/11 21:10:13  grichenk
* +CDataLoader::ResolveConflict()
*
* Revision 1.21  2002/03/07 21:25:33  grichenk
* +GetSeq_annot() in annotation iterators
*
* Revision 1.20  2002/03/06 19:37:19  grichenk
* Fixed minor bugs and comments
*
* Revision 1.19  2002/03/05 18:44:55  grichenk
* +x_UpdateTSEStatus()
*
* Revision 1.18  2002/03/05 16:09:10  grichenk
* Added x_CleanupUnusedEntries()
*
* Revision 1.17  2002/03/04 15:09:27  grichenk
* Improved MT-safety. Added live/dead flag to CDataSource methods.
*
* Revision 1.16  2002/02/28 20:53:31  grichenk
* Implemented attaching segmented sequence data. Fixed minor bugs.
*
* Revision 1.15  2002/02/25 21:05:29  grichenk
* Removed seq-data references caching. Increased MT-safety. Fixed typos.
*
* Revision 1.14  2002/02/21 19:27:05  grichenk
* Rearranged includes. Added scope history. Added searching for the
* best seq-id match in data sources and scopes. Updated tests.
*
* Revision 1.13  2002/02/20 20:23:27  gouriano
* corrected FilterSeqid()
*
* Revision 1.12  2002/02/15 20:35:38  gouriano
* changed implementation of HandleRangeMap
*
* Revision 1.11  2002/02/12 19:41:42  grichenk
* Seq-id handles lock/unlock moved to CSeq_id_Handle 'ctors.
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
* Revision 1.7  2002/01/30 22:09:28  gouriano
* changed CSeqMap interface
*
* Revision 1.6  2002/01/29 17:45:00  grichenk
* Added seq-id handles locking
*
* Revision 1.5  2002/01/28 19:44:49  gouriano
* changed the interface of BioseqHandle: two functions moved from Scope
*
* Revision 1.4  2002/01/23 21:59:31  grichenk
* Redesigned seq-id handles and mapper
*
* Revision 1.3  2002/01/18 15:56:23  gouriano
* changed TSeqMaps definition
*
* Revision 1.2  2002/01/16 16:25:57  gouriano
* restructured objmgr
*
* Revision 1.1  2002/01/11 19:06:18  gouriano
* restructured objmgr
*
*
* ===========================================================================
*/
