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
#include <corelib/ncbi_config_value.hpp>

#include <algorithm>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

CDataSource::CDataSource(void)
    : m_Loader(0),
      m_ObjMgr(0),
      m_DefaultPriority(9)
{
}


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


CDataSource::TTSE_Lock CDataSource::GetTopEntry_Info(void)
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
        TMainLock::TWriteLockGuard guard2(m_DSCacheLock);
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
            //_TRACE("TSE_Lock("<<&*m_ManualBlob<<") "<<&m_ManualBlob<<" unlock");
            _VERIFY(m_ManualBlob->m_LockCounter.Add(-1) == 0);
            m_ManualBlob.m_Info.Reset();
        }
        m_Blob_Map.clear();
        m_Blob_Cache.clear();
    }}
}


CDataSource::TTSE_Lock CDataSource::AddTSE(CSeq_entry& tse,
                                           CTSE_Info::TBlobState state)
{
    CRef<CTSE_Info> info(new CTSE_Info(tse, state));
    return AddTSE(info);
}


CDataSource::TTSE_Lock CDataSource::AddTSE(CSeq_entry& tse,
                                           bool dead)
{
    return AddTSE(tse, dead ?
        CBioseq_Handle::fState_dead : CBioseq_Handle::fState_none);
}


CDataSource::TTSE_Lock CDataSource::AddTSE(CRef<CTSE_Info> info)
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
    if ( m_Loader ) {
        m_Loader->DropTSE(info);
    }
    info->m_CacheState = CTSE_Info::eNotInCache;
    info->m_DataSource = 0;
}


void CDataSource::x_DropTSE(CRef<CTSE_Info> info)
{
    _ASSERT(!info->IsLocked());
    if ( m_Loader ) {
        m_Loader->DropTSE(info);
    }
    _ASSERT(!info->IsLocked());
    info->x_DSDetach(*this);
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
}


template<class Map, class Ref, class Info>
inline
void x_MapObject(Map& info_map, const Ref& ref, const Info& info)
{
    _ASSERT(ref);
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
    typename Map::iterator iter = info_map.lower_bound(ref);
    if ( iter != info_map.end() && iter->first == ref ) {
        _ASSERT(iter->second.GetPointer() == info);
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
    //LOG_POST(Warning << "CDataSource::x_MapTSE: " << m_TSE_InfoMap.size());
}


void CDataSource::x_Unmap(CConstRef<CSeq_entry> obj, CTSE_Info* info)
{
    x_UnmapObject(m_TSE_InfoMap, obj, info);
    //LOG_POST(Warning << "CDataSource::x_UnmapTSE: " << m_TSE_InfoMap.size());
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

CDataSource::TTSE_Lock
CDataSource::FindTSE_Lock(const CSeq_entry& tse,
                          const TTSE_LockSet& /*history*/) const
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
                                const TTSE_LockSet& /*history*/) const
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
                                const TTSE_LockSet& /*history*/) const
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
                             const TTSE_LockSet& /*history*/) const
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


void CDataSource::x_SetDirtyAnnotIndex(CTSE_Info& tse)
{
    TAnnotLock::TWriteLockGuard guard(m_DSAnnotLock);
    _ASSERT(tse.x_DirtyAnnotIndex());
    _VERIFY(m_DirtyAnnot_TSEs.insert(Ref(&tse)).second);
}


void CDataSource::x_ResetDirtyAnnotIndex(CTSE_Info& tse)
{
    _ASSERT(!tse.x_DirtyAnnotIndex());
    _VERIFY(m_DirtyAnnot_TSEs.erase(Ref(&tse)));
}


CDSAnnotLockReadGuard::CDSAnnotLockReadGuard(CDataSource& ds)
    : m_MainGuard(ds.m_DSMainLock),
      m_AnnotGuard(ds.m_DSAnnotLock)
{
}


CDSAnnotLockWriteGuard::CDSAnnotLockWriteGuard(CDataSource& ds)
    : m_MainGuard(ds.m_DSMainLock),
      m_AnnotGuard(ds.m_DSAnnotLock)
{
}


void CDataSource::UpdateAnnotIndex(void)
{
    TAnnotLockWriteGuard guard(*this);
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
CDataSource::x_GetRecords(const CSeq_id_Handle& idh,
                          CDataLoader::EChoice choice)
{
    TTSE_LockSet tse_set;
    if ( m_Loader ) {
        CDataLoader::TTSE_LockSet tse_set2 = m_Loader->GetRecords(idh, choice);
        ITERATE ( CDataLoader::TTSE_LockSet, it, tse_set2 ) {
            tse_set.AddLock(*it);
            (*it)->x_GetRecords(idh, choice == CDataLoader::eBioseqCore);
        }
    }
    return tse_set;
}


CDataSource::TTSE_LockMatchSet
CDataSource::GetTSESetWithOrphanAnnots(const TSeq_idSet& ids)
{
    TTSE_LockMatchSet ret;
    // load all relevant TSEs
    TTSE_LockSet load_locks;
    if ( m_Loader ) {
        CDataLoader::TTSE_LockSet tse_set;
        ITERATE ( TSeq_idSet, id_it, ids ) {
            CDataLoader::TTSE_LockSet tse_set2 =
                m_Loader->GetRecords(*id_it, CDataLoader::eOrphanAnnot);
            if ( tse_set.empty() ) {
                tse_set.swap(tse_set2);
            }
            else {
                tse_set.insert(tse_set2.begin(), tse_set2.end());
            }
        }
        ITERATE ( CDataLoader::TTSE_LockSet, tse_it, tse_set ) {
            load_locks.AddLock(*tse_it);
            ITERATE ( TSeq_idSet, id_it2, ids ) {
                (*tse_it)->x_GetRecords(*id_it2, false);
            }
        }
    }

    UpdateAnnotIndex();

    TAnnotLockReadGuard guard(*this);
#ifdef DEBUG_MAPS
    debug::CReadGuard<TSeq_id2TSE_Set> g1(m_TSE_annot);
#endif
    ITERATE ( TSeq_idSet, id_it, ids ) {
        TSeq_id2TSE_Set::const_iterator rtse_it = m_TSE_annot.find(*id_it);
        if ( rtse_it != m_TSE_annot.end() ) {
#ifdef DEBUG_MAPS
            debug::CReadGuard<TTSE_Set> g2(rtse_it->second);
#endif
            ITERATE ( TTSE_Set, tse, rtse_it->second ) {
                if ( (*tse)->ContainsMatchingBioseq(*id_it) ) {
                    continue;
                }
                TTSE_Lock lock = x_LockTSE(**tse, load_locks, fLockNoThrow);
                if ( lock ) {
                    ret[lock].insert(*id_it);
                    continue;
                }
            }
        }
    }
    return ret;
}


CDataSource::TTSE_LockMatchSet
CDataSource::GetTSESetWithBioseqAnnots(const CBioseq_Info& bioseq,
                                       const TTSE_Lock& tse)
{
    TTSE_LockMatchSet ret;

    CSeq_id_Handle::TMatches ids;
    ITERATE ( CBioseq_Info::TId, id_it, bioseq.GetId() ) {
        id_it->GetReverseMatchingHandles(ids);
    }

    // add bioseq TSE annots
    {{
        TSeq_idSet& dst = ret[tse];
        ITERATE ( TSeq_idSet, id_it, ids ) {
            tse->x_GetRecords(*id_it, false);
            dst.insert(*id_it);
        }
    }}

    // load all relevant TSEs
    TTSE_LockSet load_locks;
    if ( m_Loader ) {
        CDataLoader::TTSE_LockSet tse_set2 =
            m_Loader->GetExternalRecords(bioseq);
        ITERATE ( CDataLoader::TTSE_LockSet, tse_it, tse_set2 ) {
            load_locks.AddLock(*tse_it);
            ITERATE ( TSeq_idSet, id_it, ids ) {
                (*tse_it)->x_GetRecords(*id_it, false);
            }
        }
    }

    UpdateAnnotIndex();

    TAnnotLockReadGuard guard(*this);
#ifdef DEBUG_MAPS
    debug::CReadGuard<TSeq_id2TSE_Set> g1(m_TSE_annot);
#endif
    ITERATE ( TSeq_idSet, id_it, ids ) {
        TSeq_id2TSE_Set::const_iterator rtse_it = m_TSE_annot.find(*id_it);
        if ( rtse_it != m_TSE_annot.end() ) {
#ifdef DEBUG_MAPS
            debug::CReadGuard<TTSE_Set> g2(rtse_it->second);
#endif
            ITERATE ( TTSE_Set, tse_it, rtse_it->second ) {
                if ( (*tse_it)->ContainsMatchingBioseq(*id_it) ) {
                    continue;
                }
                TTSE_Lock lock = x_LockTSE(**tse_it, load_locks, fLockNoThrow);
                if ( lock ) {
                    ret[lock].insert(*id_it);
                    continue;
                }
            }
        }
    }
    return ret;
}


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



CDataSource::TTSE_Lock
CDataSource::x_FindBestTSE(const CSeq_id_Handle& handle,
                           const TTSE_LockSet& load_locks)
{
    TTSE_LockSet all_tse;
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
                all_tse.AddLock(tse);
            }
        }
    }}
    CDataLoader::TTSE_LockSet best_set = all_tse.GetBestTSEs();
    if ( best_set.empty() ) {
        // No TSE matches
        return TTSE_Lock();
    }
    CDataLoader::TTSE_LockSet::const_iterator it = best_set.begin();
    _ASSERT(it != best_set.end());
    if ( ++it == best_set.end() ) {
        // Only one TSE matches
        return *best_set.begin();
    }

    // We have multiple best TSEs
    if ( m_Loader ) {
        TTSE_Lock best = GetDataLoader()->ResolveConflict(handle, best_set);
        if ( best ) {
            // conflict resolved by data loader
            return best;
        }
    }
    // Cannot resolve conflict
    NCBI_THROW(CObjMgrException, eFindConflict,
               "Multiple seq-id matches found");
}


SSeqMatch_DS CDataSource::x_GetSeqMatch(const CSeq_id_Handle& idh,
                                        const TTSE_LockSet& locks)
{
    SSeqMatch_DS ret;
    ret.m_TSE_Lock = x_FindBestTSE(idh, locks);
    if ( ret.m_TSE_Lock ) {
        ret.m_Seq_id = idh;
        ret.m_Bioseq = ret.m_TSE_Lock->FindBioseq(ret.m_Seq_id);
        _ASSERT(ret);
    }
    else if ( idh.HaveMatchingHandles() ) { 
        // Try to find the best matching id (not exactly equal)
        CSeq_id_Handle::TMatches hset;
        idh.GetMatchingHandles(hset);
        ITERATE ( CSeq_id_Handle::TMatches, hit, hset ) {
            if ( *hit == idh ) // already checked
                continue;
            if ( ret && ret.m_Seq_id.IsBetter(*hit) ) // worse hit
                continue;
            TTSE_Lock new_tse = x_FindBestTSE(*hit, locks);
            if ( new_tse ) {
                ret.m_TSE_Lock = new_tse;
                ret.m_Seq_id = *hit;
                ret.m_Bioseq = ret.m_TSE_Lock->FindBioseq(ret.m_Seq_id);
                _ASSERT(ret);
            }
        }
    }
    return ret;
}


SSeqMatch_DS CDataSource::BestResolve(CSeq_id_Handle idh)
{
    return x_GetSeqMatch(idh, x_GetRecords(idh, CDataLoader::eBioseqCore));
}


CDataSource::TSeqMatches CDataSource::GetMatches(CSeq_id_Handle idh,
                                                 const TTSE_LockSet& history)
{
    TSeqMatches ret;

    if ( !history.IsEmpty() ) {
        TMainLock::TReadLockGuard guard(m_DSMainLock);
        TSeq_id2TSE_Set::const_iterator tse_set = m_TSE_seq.find(idh);
        if ( tse_set != m_TSE_seq.end() ) {
            ITERATE ( TTSE_Set, it, tse_set->second ) {
                TTSE_Lock tse_lock = history.FindLock(*it);
                if ( !tse_lock ) {
                    continue;
                }
                SSeqMatch_DS match(tse_lock, idh);
                _ASSERT(match);
                ret.push_back(match);
            }
        }
    }

    return ret;
}


void CDataSource::GetIds(const CSeq_id_Handle& idh, TIds& ids)
{
    TTSE_LockSet locks;
    SSeqMatch_DS match = x_GetSeqMatch(idh, locks);
    if ( match ) {
        ids = match.m_Bioseq->GetId();
        return;
    }
    // Bioseq not found - try to request ids from loader if any.
    if ( m_Loader ) {
        m_Loader->GetIds(idh, ids);
    }
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
        if ( level != CBioseq_CI::eLevel_Parts &&
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
                case CBioseq_CI::eLevel_Mains:
                    // Skip parts
                    continue;
                case CBioseq_CI::eLevel_Parts:
                    // Allow adding bioseqs from lower levels
                    local_level = CBioseq_CI::eLevel_All;
                    break;
                default:
                    break;
                }
            }
            x_CollectBioseqs(sub_info, bioseqs, filter, local_level);
        }
    }
}


#if !defined(NCBI_NO_THREADS)
void CDataSource::Prefetch(CPrefetchToken_Impl& token)
{
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
}
#else
void CDataSource::Prefetch(CPrefetchToken_Impl& /* token */)
{
}
#endif


CDataSource::TTSE_Lock CDataSource::x_LockTSE(const CTSE_Info& tse_info,
                                              const TTSE_LockSet& locks,
                                              TLockFlags flags)
{
    TTSE_Lock ret;
    _ASSERT(tse_info.Referenced());
    if ( (flags & fLockNoHistory) == 0 ) {
        ret = locks.FindLock(&tse_info);
        if ( ret ) {
            return ret;
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

static unsigned s_GetCacheSize(void)
{
    static unsigned sx_Value = kMax_UInt;
    unsigned value = sx_Value;
    if ( value == kMax_UInt ) {
        value = GetConfigInt("OBJMGR", "BLOB_CACHE", 10);
        if ( value == kMax_UInt ) {
            --value;
        }
        sx_Value = value;
    }
    return value;
}

void CDataSource::x_ReleaseLastTSELock(CRef<CTSE_Info> tse)
{
    _ASSERT(tse);
    vector<TTSE_Ref> to_delete;
    {{
        TMainLock::TWriteLockGuard guard(m_DSCacheLock);
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
        

        unsigned cache_size = s_GetCacheSize();
        while ( m_Blob_Cache.size() > cache_size ) {
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
    //_TRACE("TSE_Lock("<<&*tse<<") "<<&lock<<" lock");
    if ( tse->m_LockCounter.Add(1) != 1 ) {
        return;
    }

    TMainLock::TWriteLockGuard guard(m_DSCacheLock);
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
        }
    ~CTSE_LoadLockGuard(void)
        {
        }

    void Release(void)
        {
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
    //_TRACE("TSE_Lock("<<info<<") "<<this<<" unlock");
    CDataSource* ds = info->HasDataSource() ? &info->GetDataSource() : 0;
    if ( info->m_LockCounter.Add(-1) == 0 ) {
        _ASSERT(ds);
        ds->x_ReleaseLastLock(*this);
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
    //_TRACE("TSE_Lock("<<info<<") "<<this<<" lock");
    return info->m_LockCounter.Add(1) == 1;
}


void CTSE_Lock::x_Relock(const CTSE_Info* info)
{
    _ASSERT(!*this && info);
    _ASSERT(info->m_LockCounter.Get() != 0);
    m_Info.Reset(info);
    //_TRACE("TSE_Lock("<<info<<") "<<this<<" lock");
    info->m_LockCounter.Add(1);
}


bool CTSE_LockSet::IsEmpty(void) const
{
    return m_TSE_LockSet.empty();
}


void CTSE_LockSet::Clear(void)
{
    m_TSE_LockSet.clear();
}


CTSE_Lock CTSE_LockSet::FindLock(const CTSE_Info* info) const
{
    TTSE_LockSet::const_iterator it = m_TSE_LockSet.find(info);
    if ( it == m_TSE_LockSet.end() ) {
        return CTSE_Lock();
    }
    return it->second;
}


bool CTSE_LockSet::AddLock(const CTSE_Lock& lock)
{
    return m_TSE_LockSet.insert(TTSE_LockSet::value_type(&*lock, lock)).second;
}


bool CTSE_LockSet::RemoveLock(const CTSE_Lock& lock)
{
    return m_TSE_LockSet.erase(&*lock) != 0;
}


bool CTSE_LockSet::RemoveLock(const CTSE_Info* info)
{
    return m_TSE_LockSet.erase(info) != 0;
}


CDataLoader::TTSE_LockSet CTSE_LockSet::GetBestTSEs(void) const
{
    CDataLoader::TTSE_LockSet ret;
    ITERATE ( TTSE_LockSet, it, m_TSE_LockSet ) {
        if ( !ret.empty() ) {
            if ( IsBetter(**ret.begin(), *it->first) ) {
                continue;
            }
            else if ( IsBetter(*it->first, **ret.begin()) ) {
                ret.clear();
            }
        }
        ret.insert(it->second);
    }
    return ret;
}


bool CTSE_LockSet::IsBetter(const CTSE_Info& tse1,
                            const CTSE_Info& tse2)
{
    return tse1.GetBlobOrder() < tse2.GetBlobOrder();
}



END_SCOPE(objects)
END_NCBI_SCOPE
