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
#include <objmgr/impl/tse_loadlock.hpp>
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

#include <algorithm>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


CDataSource::CDataSource(CDataLoader& loader, CObjectManager& objmgr)
    : m_Loader(&loader),
      m_ObjMgr(&objmgr),
      m_DefaultPriority(99),
      m_Blob_Map(SBlobIdComp(&loader))
{
    m_Loader->SetTargetDataSource(*this);
}


CDataSource::CDataSource(CSeq_entry& entry, CObjectManager& objmgr)
    : m_Loader(0),
      m_ObjMgr(&objmgr),
      m_DefaultPriority(9)
{
    m_ManualBlob = AddTSE(entry);
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


TTSE_Lock CDataSource::GetTopEntry_Info(void)
{
    return m_ManualBlob;
}


void CDataSource::DropAllTSEs(void)
{
    // Lock indexes
    TMainLock::TWriteLockGuard guard(m_DSMainLock);

    // First clear all indices
    m_Bioseq_InfoMap.clear();
    m_Annot_InfoMap.clear();
    m_Entry_InfoMap.clear();
    m_TSE_InfoMap.clear();
    
    m_TSE_seq.clear();

    {{
        TAnnotLock::TWriteLockGuard guard2(m_DSAnnotLock);
        m_TSE_annot.clear();
        m_DirtyAnnot_TSEs.clear();
    }}

    // then drop all TSEs
    {{
        TMainLock::TWriteLockGuard guard(m_DSCacheLock);
        // check if any TSE is locked by user 
        ITERATE ( TBlob_Map, it, m_Blob_Map ) {
            int lock_counter = it->second->m_LockCounter.Get();
            int used_counter = it->second == m_ManualBlob;
            if ( lock_counter != used_counter ) {
                ERR_POST("CDataSource::DropAllTSEs: tse is locked");
            }
        }
        NON_CONST_ITERATE( TBlob_Map, it, m_Blob_Map ) {
            x_ForgetTSE(it->second);
        }
        if ( m_ManualBlob ) {
            _VERIFY(m_ManualBlob->m_LockCounter.Add(-1) == 0);
            m_ManualBlob.m_Info.Reset();
        }
        m_Blob_Map.clear();
        m_Blob_Cache.clear();
    }}
}


TTSE_Lock CDataSource::x_FindBestTSE(const CSeq_id_Handle& handle,
                                     const TTSE_LockSet& load_locks)
{
    TTSE_LockSet all_tse;
    size_t all_count = 0;
    {{
        TMainLock::TReadLockGuard guard(m_DSMainLock);
#ifdef DEBUG_MAPS
        debug::CReadGuard<TSeq_id2TSE_Set> g1(m_TSE_seq);
#endif
        TSeq_id2TSE_Set::const_iterator tse_set = m_TSE_seq.find(handle);
        if ( tse_set == m_TSE_seq.end() ) {
            return TTSE_Lock();
        }
#ifdef DEBUG_MAPS
        debug::CReadGuard<TTSE_Set> g2(tse_set->second);
#endif
        ITERATE ( TTSE_Set, it, tse_set->second ) {
            TTSE_Lock tse = x_LockTSE(**it, load_locks, fLockNoThrow);
            if ( tse ) {
                if ( all_tse.insert(tse).second ) {
                    ++all_count;
                }
            }
        }
        if ( all_count == 0 ) {
            return TTSE_Lock();
        }
    }}
    if ( all_count == 1 ) {
        // There is only one TSE, no matter live or dead
        return *all_tse.begin();
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
            TTSE_Lock best = GetDataLoader()->ResolveConflict(handle, all_tse);
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
        TTSE_Lock best = GetDataLoader()->ResolveConflict(handle, live_tse);
        if ( best ) {
            return best;
        }
    }
    NCBI_THROW(CObjMgrException, eFindConflict,
               "Multiple live entries found");
}


#if 0
TTSE_Lock CDataSource::GetBlobById(const CSeq_id_Handle& idh)
{
    TMainLock::TReadLockGuard guard(m_DSMainLock);
#ifdef DEBUG_MAPS
    debug::CReadGuard<TSeq_id2TSE_Set> g1(m_TSE_seq);
#endif
    TSeq_id2TSE_Set::iterator tse_set = m_TSE_seq.find(idh);
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
#endif

TTSE_Lock CDataSource::AddTSE(CSeq_entry& tse,
                              CTSE_Info::ESuppression_Level level)
{
    CRef<CTSE_Info> info(new CTSE_Info(tse, level));
    return AddTSE(info);
}


TTSE_Lock CDataSource::AddTSE(CSeq_entry& tse,
                              bool dead)
{
    CTSE_Info::ESuppression_Level level = 
        dead? CTSE_Info::eSuppression_dead: CTSE_Info::eSuppression_none;
    return AddTSE(tse, level);
}


TTSE_Lock CDataSource::AddTSE(CRef<CTSE_Info> info)
{
    TTSE_Lock lock;
    _ASSERT(IsLoaded(*info));
    _ASSERT(!info->IsLocked());
    _ASSERT(!info->HasDataSource());
    TMainLock::TWriteLockGuard guard(m_DSMainLock);
    TMainLock::TWriteLockGuard guard2(m_DSCacheLock);
    _ASSERT(!info->IsLocked());
    _ASSERT(!info->HasDataSource());
    TBlobId blob_id = info->GetBlobId();
    if ( !blob_id ) {
        blob_id = info;
    }
    _VERIFY(m_Blob_Map.insert(TBlob_Map::value_type(blob_id, info)).second);
    info->x_DSAttach(*this);
    x_SetLock(lock, info);
    _ASSERT(info->IsLocked());
    return lock;
}


bool CDataSource::DropTSE(CTSE_Info& info)
{
    TMainLock::TWriteLockGuard guard(m_DSMainLock);
    CRef<CTSE_Info> ref(&info);

    if ( info.IsLocked() ) {
        _TRACE("DropTSE: DS="<<this<<" TSE_Info="<<&info<<" locked");
        return false; // Not really dropped, although found
    }
    if ( !info.HasDataSource() ) {
        _TRACE("DropTSE: DS="<<this<<" TSE_Info="<<&info<<" already dropped");
        return false; // Not really dropped, although found
    }
    _ASSERT(info.m_UsedMemory == 0 &&& info.GetDataSource() == this);
    info.m_UsedMemory = 1;
    _ASSERT(!info.IsLocked());
    x_DropTSE(ref);
    _ASSERT(!info.IsLocked());
    _ASSERT(!info.HasDataSource());
    return true;
}


void CDataSource::x_ForgetTSE(CRef<CTSE_Info> info)
{
    _TRACE("x_ForgetTSE("<<&*info<<")");
    if ( m_Loader ) {
        m_Loader->DropTSE(info);
    }
    info->m_CacheState = CTSE_Info::eNotInCache;
    info->m_DataSource = 0;
    _TRACE("x_ForgetTSE("<<&*info<<")");
}


void CDataSource::x_DropTSE(CRef<CTSE_Info> info)
{
    _TRACE("x_DropTSE("<<&*info<<")");
    _ASSERT(!info->IsLocked());
    if ( m_Loader ) {
        m_Loader->DropTSE(info);
    }
    _TRACE("x_DropTSE("<<&*info<<")");
    _ASSERT(!info->IsLocked());
    info->x_DSDetach(*this);
    _TRACE("x_DropTSE("<<&*info<<")");
    _ASSERT(!info->IsLocked());
    {{
        TCacheLock::TWriteLockGuard guard(m_DSCacheLock);
        TBlobId blob_id = info->GetBlobId();
        if ( !blob_id ) {
            blob_id = info;
        }
        _VERIFY(m_Blob_Map.erase(blob_id));
    }}
    _ASSERT(!info->IsLocked());
    {{
        TAnnotLock::TWriteLockGuard guard2(m_DSAnnotLock);
        m_DirtyAnnot_TSEs.erase(info);
    }}
    _ASSERT(!info->IsLocked());
    _TRACE("x_DropTSE("<<&*info<<")");
}


template<class Map, class Ref, class Info>
inline
void x_MapObject(Map& info_map, const Ref& ref, const Info& info)
{
    _ASSERT(ref);
    _TRACE("x_Map():" <<
           " " << typeid(typename Ref::TObjectType).name() <<
           " " << ref.GetPointerOrNull() <<
           " " << typeid(typename Info::TObjectType).name() <<
           " " << info.GetPointerOrNull());
    typedef typename Map::value_type value_type;
    pair<typename Map::iterator, bool> ins =
        info_map.insert(value_type(ref, info));
    if ( !ins.second ) {
        CNcbiOstrstream str;
        str << "CDataSource::x_Map(): object already mapped:" <<
            " " << typeid(typename Ref::TObjectType).name() <<
            " ref: " << ref.GetPointerOrNull() <<
            " " << typeid(typename Info::TObjectType).name() <<
            " ptr: " << info.GetPointerOrNull() <<
            " was: " << ins.first->second.GetPointerOrNull();
        NCBI_THROW(CObjMgrException, eOtherError,
                   CNcbiOstrstreamToString(str));
    }
}


template<class Map, class Ref, class Info>
inline
void x_UnmapObject(Map& info_map, const Ref& ref, const Info& _DEBUG_ARG(info))
{
    _ASSERT(ref);
    _TRACE("x_Unmap():" <<
           " " << typeid(*ref).name() <<
           " " << ref.GetPointerOrNull() <<
           " " << typeid(*info).name() <<
           " " << info);
    typename Map::iterator iter = info_map.lower_bound(ref);
    if ( iter != info_map.end() && iter->first == ref ) {
        _ASSERT(iter->second == info);
        info_map.erase(iter);
    }
    else {
        _ASSERT(iter != info_map.end() && iter->first == ref);
        abort();
    }
}


void CDataSource::x_Map(CConstRef<CSeq_entry> obj, CTSE_Info* info)
{
    x_MapObject(m_TSE_InfoMap, obj, Ref(info));
}


void CDataSource::x_Unmap(CConstRef<CSeq_entry> obj, CTSE_Info* info)
{
    x_UnmapObject(m_TSE_InfoMap, obj, info);
}


void CDataSource::x_Map(CConstRef<CSeq_entry> obj, CSeq_entry_Info* info)
{
    x_MapObject(m_Entry_InfoMap, obj, Ref(info));
}


void CDataSource::x_Unmap(CConstRef<CSeq_entry> obj, CSeq_entry_Info* info)
{
    x_UnmapObject(m_Entry_InfoMap, obj, info);
}


void CDataSource::x_Map(CConstRef<CSeq_annot> obj, CSeq_annot_Info* info)
{
    x_MapObject(m_Annot_InfoMap, obj, Ref(info));
}


void CDataSource::x_Unmap(CConstRef<CSeq_annot> obj, CSeq_annot_Info* info)
{
    x_UnmapObject(m_Annot_InfoMap, obj, info);
}


void CDataSource::x_Map(CConstRef<CBioseq> obj, CBioseq_Info* info)
{
    x_MapObject(m_Bioseq_InfoMap, obj, Ref(info));
}


void CDataSource::x_Unmap(CConstRef<CBioseq> obj, CBioseq_Info* info)
{
    x_UnmapObject(m_Bioseq_InfoMap, obj, info);
}


/////////////////////////////////////////////////////////////////////////////
//   mapping of various XXX_Info
// CDataSource must be guarded by mutex
/////////////////////////////////////////////////////////////////////////////

TTSE_Lock CDataSource::FindTSE_Lock(const CSeq_entry& tse,
                                    const TTSE_LockSet& history) const
{
    TTSE_Lock ret;
    {{
        TMainLock::TReadLockGuard guard(m_DSMainLock);
        CConstRef<CTSE_Info> info = x_FindTSE_Info(tse);
        if ( info ) {
            x_SetLock(ret, info);
        }
    }}
    return ret;
}


CDataSource::TSeq_entry_Lock
CDataSource::FindSeq_entry_Lock(const CSeq_entry& entry,
                                const TTSE_LockSet& history) const
{
    TSeq_entry_Lock ret;
    {{
        TMainLock::TReadLockGuard guard(m_DSMainLock);
        ret.first = x_FindSeq_entry_Info(entry);
        if ( ret.first ) {
            x_SetLock(ret.second, ConstRef(&ret.first->GetTSE_Info()));
        }
    }}
    return ret;
}


CDataSource::TSeq_annot_Lock
CDataSource::FindSeq_annot_Lock(const CSeq_annot& annot,
                                const TTSE_LockSet& history) const
{
    TSeq_annot_Lock ret;
    {{
        TMainLock::TReadLockGuard guard(m_DSMainLock);
        ret.first = x_FindSeq_annot_Info(annot);
        if ( ret.first ) {
            x_SetLock(ret.second, ConstRef(&ret.first->GetTSE_Info()));
        }
    }}
    return ret;
}


CDataSource::TBioseq_Lock
CDataSource::FindBioseq_Lock(const CBioseq& bioseq,
                             const TTSE_LockSet& history) const
{
    TBioseq_Lock ret;
    {{
        TMainLock::TReadLockGuard guard(m_DSMainLock);
        ret.first = x_FindBioseq_Info(bioseq);
        if ( ret.first ) {
            x_SetLock(ret.second, ConstRef(&ret.first->GetTSE_Info()));
        }
    }}
    return ret;
}


CConstRef<CTSE_Info>
CDataSource::x_FindTSE_Info(const CSeq_entry& obj) const
{
    CConstRef<CTSE_Info> ret;
    TTSE_InfoMap::const_iterator found = m_TSE_InfoMap.find(ConstRef(&obj));
    if ( found != m_TSE_InfoMap.end() ) {
        ret = found->second;
    }
    return ret;
}


CConstRef<CSeq_entry_Info>
CDataSource::x_FindSeq_entry_Info(const CSeq_entry& obj) const
{
    CConstRef<CSeq_entry_Info> ret;
    TEntry_InfoMap::const_iterator found =
        m_Entry_InfoMap.find(ConstRef(&obj));
    if ( found != m_Entry_InfoMap.end() ) {
        ret = found->second;
    }
    return ret;
}


CConstRef<CSeq_annot_Info>
CDataSource::x_FindSeq_annot_Info(const CSeq_annot& obj) const
{
    CConstRef<CSeq_annot_Info> ret;
    TAnnot_InfoMap::const_iterator found =
        m_Annot_InfoMap.find(ConstRef(&obj));
    if ( found != m_Annot_InfoMap.end() ) {
        ret = found->second;
    }
    return ret;
}


CConstRef<CBioseq_Info>
CDataSource::x_FindBioseq_Info(const CBioseq& obj) const
{
    CConstRef<CBioseq_Info> ret;
    TBioseq_InfoMap::const_iterator found =
        m_Bioseq_InfoMap.find(ConstRef(&obj));
    if ( found != m_Bioseq_InfoMap.end() ) {
        ret = found->second;
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
    TMainLock::TWriteLockGuard guard(m_DSMainLock);
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

    TMainLock::TWriteLockGuard guard(m_DSMainLock);
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

    TMainLock::TWriteLockGuard guard(m_DSMainLock);
    return entry_info.AddAnnot(annot);
}


CRef<CSeq_annot_Info> CDataSource::AttachAnnot(CBioseq_Base_Info& parent,
                                               const CSeq_annot& annot)
{
    if ( m_Loader ) {
        NCBI_THROW(CObjMgrException, eModifyDataError,
                   "Can not modify a loaded entry");
    }

    TMainLock::TWriteLockGuard guard(m_DSMainLock);
    return parent.AddAnnot(annot);
}


void CDataSource::RemoveAnnot(CSeq_annot_Info& annot)
{
    if ( m_Loader ) {
        NCBI_THROW(CObjMgrException, eModifyDataError,
                   "Can not modify a loaded entry");
    }

    TMainLock::TWriteLockGuard guard(m_DSMainLock);
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

    TMainLock::TWriteLockGuard guard(m_DSMainLock);
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
    _TRACE("x_SetDirtyAnnotIndex("<<&tse<<")");
    TAnnotLock::TWriteLockGuard guard(m_DSAnnotLock);
    _ASSERT(tse.x_DirtyAnnotIndex());
    _VERIFY(m_DirtyAnnot_TSEs.insert(Ref(&tse)).second);
}


void CDataSource::x_ResetDirtyAnnotIndex(CTSE_Info& tse)
{
    _TRACE("x_ResetDirtyAnnotIndex("<<&tse<<")");
    _ASSERT(!tse.x_DirtyAnnotIndex());
    _VERIFY(m_DirtyAnnot_TSEs.erase(Ref(&tse)));
}


void CDataSource::UpdateAnnotIndex(void)
{
    TMainLock::TWriteLockGuard guard(m_DSMainLock);
    TAnnotLock::TWriteLockGuard guard2(m_DSAnnotLock);
    while ( !m_DirtyAnnot_TSEs.empty() ) {
        CRef<CTSE_Info> tse_info = *m_DirtyAnnot_TSEs.begin();
        tse_info->UpdateAnnotIndex();
        _ASSERT(m_DirtyAnnot_TSEs.empty() ||
                *m_DirtyAnnot_TSEs.begin() != tse_info);

    }
}


void CDataSource::UpdateAnnotIndex(const CSeq_entry_Info& entry_info)
{
    TMainLock::TWriteLockGuard guard(m_DSMainLock);
    entry_info.UpdateAnnotIndex();
}


void CDataSource::UpdateAnnotIndex(const CSeq_annot_Info& annot_info)
{
    TMainLock::TWriteLockGuard guard(m_DSMainLock);
    annot_info.UpdateAnnotIndex();
}


CDataSource::TTSE_LockSet
CDataSource::GetTSESetWithAnnots(const CSeq_id_Handle& idh,
                                 const TTSE_LockSet& history)
{
    TTSE_LockSet annot_locks;
    // load all relevant TSEs
    TTSE_LockSet load_locks;
    if ( m_Loader ) {
        load_locks = m_Loader->GetRecords(idh, CDataLoader::eExtAnnot);
    }
    load_locks.insert(history.begin(), history.end());

    UpdateAnnotIndex();

    TMainLock::TReadLockGuard main_guard(m_DSMainLock);
    // collect all relevant TSEs
    TAnnotLock::TReadLockGuard guard(m_DSAnnotLock);
#ifdef DEBUG_MAPS
    debug::CReadGuard<TSeq_id2TSE_Set> g1(m_TSE_annot);
#endif
    TSeq_id2TSE_Set::const_iterator rtse_it = m_TSE_annot.find(idh);
    if ( rtse_it != m_TSE_annot.end() ) {
#ifdef DEBUG_MAPS
        debug::CReadGuard<TTSE_Set> g2(rtse_it->second);
#endif
        ITERATE ( TTSE_Set, tse, rtse_it->second ) {
            if ( (*tse)->ContainsSeqid(idh) ) {
                // skip TSE containing sequence
                continue;
            }
            TTSE_Lock lock = x_LockTSE(**tse, load_locks, fLockNoThrow);
            if ( lock ) {
                annot_locks.insert(lock);
            }
        }
    }
    //ERR_POST("GetTSESetWithAnnots("<<idh.AsString()<<").size() = "<<annot_locks.size());
    return annot_locks;
}

/*
void CDataSource::GetSynonyms(const CSeq_id_Handle& main_idh,
                              set<CSeq_id_Handle>& syns)
{
    //### The TSE returns locked, unlock it
    CSeq_id_Handle idh = main_idh;
    TTSE_Lock tse_info(x_FindBestTSE(main_idh));
    if ( !tse_info ) {
        // Try to find the best matching id (not exactly equal)
        CSeq_id_Mapper& mapper = *CSeq_id_Mapper::GetInstance();
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
*/

void CDataSource::x_IndexTSE(TSeq_id2TSE_Set& tse_map,
                             const CSeq_id_Handle& id,
                             CTSE_Info* tse_info)
{
    TSeq_id2TSE_Set::iterator it = tse_map.lower_bound(id);
    if ( it == tse_map.end() || it->first != id ) {
        it = tse_map.insert(it, TSeq_id2TSE_Set::value_type(id, TTSE_Set()));
    }
    _ASSERT(it != tse_map.end() && it->first == id);
    it->second.insert(Ref(tse_info));
}


void CDataSource::x_UnindexTSE(TSeq_id2TSE_Set& tse_map,
                               const CSeq_id_Handle& id,
                               CTSE_Info* tse_info)
{
    TSeq_id2TSE_Set::iterator it = tse_map.find(id);
    if ( it == tse_map.end() ) {
        return;
    }
    it->second.erase(Ref(tse_info));
    if ( it->second.empty() ) {
        tse_map.erase(it);
    }
}


void CDataSource::x_IndexSeqTSE(const CSeq_id_Handle& id,
                                CTSE_Info* tse_info)
{
    // no need to lock as it's locked by callers
    TMainLock::TWriteLockGuard guard(m_DSMainLock);
    x_IndexTSE(m_TSE_seq, id, tse_info);
}


void CDataSource::x_UnindexSeqTSE(const CSeq_id_Handle& id,
                                  CTSE_Info* tse_info)
{
    // no need to lock as it's locked by callers
    TMainLock::TWriteLockGuard guard(m_DSMainLock);
    x_UnindexTSE(m_TSE_seq, id, tse_info);
}


void CDataSource::x_IndexAnnotTSE(const CSeq_id_Handle& id,
                                  CTSE_Info* tse_info)
{
    TAnnotLock::TWriteLockGuard guard(m_DSAnnotLock);
    if ( tse_info ) {
        // Filter out TSEs which contain the sequence
        CSeq_id_Mapper& mapper = *CSeq_id_Mapper::GetInstance();
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
    TAnnotLock::TWriteLockGuard guard(m_DSAnnotLock);
    x_UnindexTSE(m_TSE_annot, id, tse_info);
}


void CDataSource::x_IndexAnnotTSEs(CTSE_Info* tse_info)
{
    TAnnotLock::TWriteLockGuard guard(m_DSAnnotLock);
    ITERATE ( CTSE_Info::TSeqIdToNames, it, tse_info->m_SeqIdToNames ) {
        x_IndexTSE(m_TSE_annot, it->first, tse_info);
    }
    if ( tse_info->x_DirtyAnnotIndex() ) {
        _TRACE("x_IndexAnnotTSEs("<<tse_info<<")");
        _VERIFY(m_DirtyAnnot_TSEs.insert(Ref(tse_info)).second);
    }
}


void CDataSource::x_UnindexAnnotTSEs(CTSE_Info* tse_info)
{
    TAnnotLock::TWriteLockGuard guard(m_DSAnnotLock);
    ITERATE ( CTSE_Info::TSeqIdToNames, it, tse_info->m_SeqIdToNames ) {
        x_UnindexTSE(m_TSE_annot, it->first, tse_info);
    }
}


void CDataSource::x_CleanupUnusedEntries(void)
{
}


CSeqMatch_Info CDataSource::BestResolve(CSeq_id_Handle idh)
{
    //### Lock all TSEs found, unlock all filtered out in the end.
    TTSE_LockSet load_locks;
    if ( m_Loader ) {
        // Send request to the loader
        load_locks = m_Loader->GetRecords(idh, CDataLoader::eBioseqCore);
    }
    CSeqMatch_Info match;
    TTSE_Lock tse = x_FindBestTSE(idh, load_locks);
    if ( !tse ) {
        // Try to find the best matching id (not exactly equal)
        CSeq_id_Mapper& mapper = *CSeq_id_Mapper::GetInstance();
        if ( mapper.HaveMatchingHandles(idh) ) {
            TSeq_id_HandleSet hset;
            mapper.GetMatchingHandles(idh, hset);
            ITERATE(TSeq_id_HandleSet, hit, hset) {
                if ( *hit == idh ) // already checked
                    continue;
                if ( bool(tse)  &&  idh.IsBetter(*hit) ) // worse hit
                    continue;
                TTSE_Lock tmp_tse = x_FindBestTSE(*hit, load_locks);
                if ( tmp_tse ) {
                    tse = tmp_tse;
                    idh = *hit;
                }
            }
        }
    }
    if ( tse ) {
        match = CSeqMatch_Info(idh, tse);
    }
    return match;
}


CSeqMatch_Info CDataSource::HistoryResolve(CSeq_id_Handle idh,
                                           const TTSE_LockSet& history)
{
    CSeqMatch_Info match;
    if ( !history.empty() ) {
        TTSE_LockSet locks;
        TMainLock::TReadLockGuard guard(m_DSMainLock);
#ifdef DEBUG_MAPS
        debug::CReadGuard<TSeq_id2TSE_Set> g1(m_TSE_seq);
#endif
        TSeq_id2TSE_Set::const_iterator tse_set = m_TSE_seq.find(idh);
        if ( tse_set != m_TSE_seq.end() ) {
#ifdef DEBUG_MAPS
            debug::CReadGuard<TTSE_Set> g2(tse_set->second);
#endif
            ITERATE ( TTSE_Set, it, tse_set->second ) {
                TTSE_Lock tse = x_LockTSE(**it, history, 
                                          fLockNoManual | fLockNoThrow);
                if ( !tse ) {
                    continue;
                }
                
                CSeqMatch_Info new_match(idh, tse);
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
    }
    return match;
}


CSeqMatch_Info* CDataSource::ResolveConflict(const CSeq_id_Handle& id,
                                             CSeqMatch_Info& info1,
                                             CSeqMatch_Info& info2)
{
    _ASSERT(info1.GetTSE_Lock() && info2.GetTSE_Lock());
    if (&info1.GetTSE_Lock()->GetDataSource() != this  ||
        &info2.GetTSE_Lock()->GetDataSource() != this) {
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
    tse_set.insert(info1.GetTSE_Lock());
    tse_set.insert(info2.GetTSE_Lock());
    TTSE_Lock tse = m_Loader->ResolveConflict(id, tse_set);
    if ( tse == info1.GetTSE_Lock() ) {
        return &info1;
    }
    if ( tse == info2.GetTSE_Lock() ) {
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


TTSE_Lock CDataSource::x_LockTSE(const CTSE_Info& tse_info,
                                 const TTSE_LockSet& locks,
                                 TLockFlags flags)
{
    TTSE_Lock ret;
    _ASSERT(tse_info.Referenced());
    if ( (flags & fLockNoHistory) == 0 ) {
        ITERATE ( TTSE_LockSet, it, locks ) {
            if ( it->GetPointerOrNull() == &tse_info ) {
                ret = *it;
                return ret;
            }
        }
    }
    if ( (flags & fLockNoManual) == 0 ) {
        if ( m_ManualBlob.GetPointerOrNull() == &tse_info ) {
            ret = m_ManualBlob;
            return ret;
        }
    }
    if ( (flags & fLockNoThrow) == 0 ) {
        NCBI_THROW(CObjMgrException, eOtherError,
                   "CDataSource::x_LockTSE: cannot find in locks");
    }
    return ret;
}


CTSE_LoadLock CDataSource::GetTSE_LoadLock(const TBlobId& blob_id)
{
    CTSE_LoadLock ret;
    {{
        CTSE_Lock lock;
        CRef<CTSE_Info::CLoadMutex> load_mutex;
        {{
            TMainLock::TWriteLockGuard guard(m_DSCacheLock);
            TTSE_Ref& slot = m_Blob_Map[blob_id];
            if ( !slot ) {
                slot.Reset(new CTSE_Info(blob_id));
                _TRACE("CDataSource::CTSE_Info = "<<&*slot<<" id="<<&blob_id);
                _ASSERT(!IsLoaded(*slot));
                _ASSERT(!slot->m_LoadMutex);
                slot->m_LoadMutex.Reset(new CTSE_Info::CLoadMutex);
            }
            x_SetLock(lock, slot);
            load_mutex = lock->m_LoadMutex;
        }}
        x_SetLoadLock(ret, const_cast<CTSE_Info&>(*lock), load_mutex);
    }}
    return ret;
}


bool CDataSource::IsLoaded(const CTSE_Info& tse) const
{
    return tse.m_LoadState != CTSE_Info::eNotLoaded;
}


void CDataSource::SetLoaded(CTSE_LoadLock& lock)
{
    {{
        TMainLock::TWriteLockGuard guard(m_DSMainLock);
        _ASSERT(lock);
        _ASSERT(lock.m_Info);
        _ASSERT(!IsLoaded(*lock));
        _ASSERT(lock.m_LoadLock);
        _ASSERT(!lock->HasDataSource());
        lock->x_DSAttach(*this);
    }}
    {{
        TMainLock::TWriteLockGuard guard2(m_DSCacheLock);
        lock->m_LoadState = CTSE_Info::eLoaded;
        lock->m_LoadMutex.Reset();
    }}
    lock.ReleaseLoadLock();
}


static int sx_GetIntVar(const char* var_name, int def_value)
{
    int ret = def_value;
    const char* value = getenv(var_name);
    if ( value ) {
        try {
            ret = NStr::StringToInt(value);
        }
        catch (...) {
            ret = def_value;
        }
    }
    return ret;
}

static unsigned sx_CacheSize = sx_GetIntVar("OBJMGR_BLOB_CACHE", 10);


void CDataSource::x_ReleaseLastTSELock(CRef<CTSE_Info> tse)
{
    _ASSERT(tse);
    vector<TTSE_Ref> to_delete;
    {{
        TMainLock::TWriteLockGuard guard(m_DSCacheLock);
        _TRACE("x_ReleaseLastTSELock("<<tse.GetPointerOrNull()<<")");
        if ( tse->IsLocked() ) { // already locked again
            return;
        }
        if ( !IsLoaded(*tse) ) { // not loaded yet
            _ASSERT(!tse->HasDataSource());
            return;
        }
        if ( !tse->HasDataSource() ) { // already released
            return;
        }
        _ASSERT(&tse->GetDataSource() == this);
        
        if ( tse->m_CacheState != CTSE_Info::eInCache ) {
            _ASSERT(find(m_Blob_Cache.begin(), m_Blob_Cache.end(), tse) ==
                    m_Blob_Cache.end());
            tse->m_CachePosition = m_Blob_Cache.insert(m_Blob_Cache.end(),tse);
            tse->m_CacheState = CTSE_Info::eInCache;
        }
        _ASSERT(tse->m_CachePosition ==
                find(m_Blob_Cache.begin(), m_Blob_Cache.end(), tse));
        
        while ( m_Blob_Cache.size() > sx_CacheSize ) {
            CRef<CTSE_Info> del_tse = m_Blob_Cache.front();
            m_Blob_Cache.pop_front();
            del_tse->m_CacheState = CTSE_Info::eNotInCache;
            to_delete.push_back(del_tse);
            _VERIFY(DropTSE(*del_tse));
        }
    }}
}


void CDataSource::x_SetLock(CTSE_Lock& lock, CConstRef<CTSE_Info> tse) const
{
    _ASSERT(!lock);
    _ASSERT(tse);
    lock.m_Info.Reset(&*tse);
    if ( tse->m_LockCounter.Add(1) != 1 ) {
        return;
    }

    TMainLock::TWriteLockGuard guard(m_DSCacheLock);
    _TRACE("x_SetLock("<<tse.GetPointerOrNull()<<")");
    if ( tse->m_CacheState == CTSE_Info::eInCache ) {
        _ASSERT(tse->m_CachePosition ==
                find(m_Blob_Cache.begin(), m_Blob_Cache.end(), tse));
        tse->m_CacheState = CTSE_Info::eNotInCache;
        m_Blob_Cache.erase(tse->m_CachePosition);
    }
    
    _ASSERT(find(m_Blob_Cache.begin(), m_Blob_Cache.end(), tse) ==
            m_Blob_Cache.end());
}


class CTSE_LoadLockGuard : public CObject
{
public:
    explicit CTSE_LoadLockGuard(CDataSource* ds,
                                const CObject* lock,
                                CMutex& mutex)
        : m_DataSource(ds), m_Lock(lock), m_Guard(mutex)
        {
            _TRACE("TSE_LoadLock locked mutex "<<&mutex<<" by guard "<<this);
        }
    ~CTSE_LoadLockGuard(void)
        {
            _TRACE("TSE_LoadLock destructing guard "<<this);
        }

    void Release(void)
        {
            _TRACE("TSE_LoadLock releasing by guard "<<this);
            m_Guard.Release();
            m_Lock.Reset();
        }

    CRef<CDataSource> GetDataSource(void) const
        {
            return m_DataSource;
        }

private:
    CRef<CDataSource>   m_DataSource;
    CConstRef<CObject>  m_Lock;
    CMutexGuard     m_Guard;

private:
    CTSE_LoadLockGuard(const CTSE_LoadLockGuard&);
    CTSE_LoadLockGuard operator=(const CTSE_LoadLockGuard&);
};


void CDataSource::x_SetLoadLock(CTSE_LoadLock& load, CTSE_Lock& lock)
{
    _ASSERT(lock && !load);
    _ASSERT(lock->IsLocked());
    load.m_DataSource.Reset(this);
    load.m_Info.Reset(const_cast<CTSE_Info*>(lock.GetNonNullPointer()));
    load.m_Info->m_LockCounter.Add(1);
    if ( !IsLoaded(*load) ) {
        _ASSERT(load->m_LoadMutex);
        load.m_LoadLock.Reset(new CTSE_LoadLockGuard(this,
                                                     load->m_LoadMutex,
                                                     *load->m_LoadMutex));
        if ( IsLoaded(*load) ) {
            load.ReleaseLoadLock();
        }
    }
}


void CDataSource::x_SetLoadLock(CTSE_LoadLock& load,
                                CTSE_Info& tse,
                                CRef<CTSE_Info::CLoadMutex> load_mutex)
{
    _ASSERT(!load);
    _ASSERT(tse.IsLocked());
    load.m_DataSource.Reset(this);
    _VERIFY(tse.m_LockCounter.Add(1) > 1);
    load.m_Info.Reset(&tse);
    if ( !IsLoaded(tse) ) {
        _ASSERT(load_mutex);
        load.m_LoadLock.Reset(new CTSE_LoadLockGuard(this,
                                                     load_mutex,
                                                     *load_mutex));
        if ( IsLoaded(tse) ) {
            load.ReleaseLoadLock();
        }
    }
}


void CDataSource::x_ReleaseLastLock(CTSE_Lock& lock)
{
    CRef<CTSE_Info> info(const_cast<CTSE_Info*>(lock.GetNonNullPointer()));
    lock.m_Info.Reset();
    x_ReleaseLastTSELock(info);
}


void CDataSource::x_ReleaseLastLoadLock(CTSE_LoadLock& lock)
{
    CRef<CTSE_Info> info = lock.m_Info;
    lock.m_Info.Reset();
    lock.m_DataSource.Reset();
    x_ReleaseLastTSELock(info);
}


void CTSE_Lock::x_Assign(const CTSE_LoadLock& load_lock)
{
    _ASSERT(load_lock);
    _ASSERT(load_lock.IsLoaded());
    x_Relock(&*load_lock);
}


CTSE_LoadLock& CTSE_LoadLock::operator=(const CTSE_LoadLock& lock)
{
    if ( this != &lock ) {
        Reset();
        m_Info = lock.m_Info;
        m_DataSource = lock.m_DataSource;
        m_LoadLock = lock.m_LoadLock;
        if ( *this ) {
            m_Info->m_LockCounter.Add(1);
        }
    }
    return *this;
}

/*
void CTSE_LoadLock::x_SetLoadLock(CDataSource* ds, CTSE_Info* info)
{
    _ASSERT(info && !m_Info);
    _ASSERT(ds && !m_DataSource);
    m_Info.Reset(info);
    m_DataSource.Reset(ds);
    info->m_LockCounter.Add(1);
    if ( !IsLoaded() ) {
        _ASSERT(info->m_LoadMutex);
        m_LoadLock.Reset(new CTSE_LoadLockGuard(ds,
                                                info->m_LoadMutex,
                                                *info->m_LoadMutex));
        if ( IsLoaded() ) {
            ReleaseLoadLock();
        }
    }
}
*/

bool CTSE_LoadLock::IsLoaded(void) const
{
    return m_DataSource->IsLoaded(**this);
}


void CTSE_LoadLock::SetLoaded(void)
{
    _ASSERT(m_LoadLock);
    _ASSERT(!IsLoaded());
    _ASSERT(m_Info->m_LoadMutex);
    m_DataSource->SetLoaded(*this);
}


CTSE_LoadLock::operator CTSE_Lock(void) const
{
    _ASSERT(*this);
    _ASSERT(IsLoaded());
    CTSE_Lock lock(*this);
    return lock;
}


void CTSE_LoadLock::Reset(void)
{
    ReleaseLoadLock();
    if ( *this ) {
        if ( m_Info->m_LockCounter.Add(-1) == 0 ) {
            m_DataSource->x_ReleaseLastLoadLock(*this);
            _ASSERT(!*this);
            _ASSERT(!m_DataSource);
        }
        else {
            m_Info.Reset();
            m_DataSource.Reset();
        }
    }
}


void CTSE_LoadLock::ReleaseLoadLock(void)
{
    if ( m_LoadLock ) {
        if ( IsLoaded() ) {
            static_cast<CTSE_LoadLockGuard&>(*m_LoadLock).Release();
        }
        m_LoadLock.Reset();
    }
}


void CTSE_Lock::x_Unlock(void)
{
    _ASSERT(*this);
    const CTSE_Info* info = GetNonNullPointer();
    if ( info->m_LockCounter.Add(-1) == 0 ) {
        info->GetDataSource().x_ReleaseLastLock(*this);
        _ASSERT(!*this);
    }
    else {
        m_Info.Reset();
    }
}


bool CTSE_Lock::x_Lock(const CTSE_Info* info)
{
    _ASSERT(!*this && info);
    m_Info.Reset(info);
    return info->m_LockCounter.Add(1) == 1;
}


void CTSE_Lock::x_Relock(const CTSE_Info* info)
{
    _ASSERT(!*this && info);
    _ASSERT(info->m_LockCounter.Get() != 0);
    m_Info.Reset(info);
    info->m_LockCounter.Add(1);
}


END_SCOPE(objects)
END_NCBI_SCOPE
