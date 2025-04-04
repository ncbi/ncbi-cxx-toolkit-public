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
#include <objmgr/impl/tse_split_info.hpp>
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
#include <objmgr/error_codes.hpp>

#include <corelib/ncbimtx.hpp>
#include <corelib/ncbi_param.hpp>
#include <objmgr/scope.hpp>
#include <common/ncbi_sanitizers.h>

#include <algorithm>

#if 0 && defined(_DEBUG)
# define TSE_LOCK_NO_SWAP
# define TSE_LOCK_TRACE(x) _TRACE(x)
#else
# define TSE_LOCK_TRACE(x)
#endif

#define NCBI_USE_ERRCODE_X   ObjMgr_DataSource

BEGIN_NCBI_SCOPE

NCBI_DEFINE_ERR_SUBCODE_X(3);

BEGIN_SCOPE(objects)

class CSeq_entry;

NCBI_PARAM_DECL(unsigned, OBJMGR, BLOB_CACHE);
NCBI_PARAM_DEF_EX(unsigned, OBJMGR, BLOB_CACHE, 10,
                  eParam_NoThread, OBJMGR_BLOB_CACHE);

unsigned CDataSource::GetDefaultBlobCacheSizeLimit(void)
{
    static CSafeStatic<NCBI_PARAM_TYPE(OBJMGR, BLOB_CACHE)> sx_Value;
    return sx_Value->Get();
}


NCBI_PARAM_DECL(bool, OBJMGR, BULK_CHUNKS);
NCBI_PARAM_DEF_EX(bool, OBJMGR, BULK_CHUNKS, true,
                  eParam_NoThread, OBJMGR_BULK_CHUNKS);

static bool s_GetBulkChunks(void)
{
    static bool value = NCBI_PARAM_TYPE(OBJMGR, BULK_CHUNKS)::GetDefault();
    return value;
}


CDataSource::CDataSource(void)
    : m_DefaultPriority(CObjectManager::kPriority_Entry),
      m_Blob_Cache_Size(0),
      m_Blob_Cache_Size_Limit(GetDefaultBlobCacheSizeLimit()),
      m_StaticBlobCounter(0),
      m_TrackSplitSeq(false)
{
}


CDataSource::CDataSource(CDataLoader& loader)
    : m_Loader(&loader),
      m_DefaultPriority(loader.GetDefaultPriority()),
      m_Blob_Cache_Size(0),
      m_Blob_Cache_Size_Limit(min(GetDefaultBlobCacheSizeLimit(),
                                  loader.GetDefaultBlobCacheSizeLimit())),
      m_StaticBlobCounter(0),
      m_TrackSplitSeq(loader.GetTrackSplitSeq())
{
    m_Loader->SetTargetDataSource(*this);
}


CDataSource::CDataSource(const CObject& shared_object, const CSeq_entry& entry)
    : m_SharedObject(&shared_object),
      m_DefaultPriority(CObjectManager::kPriority_Entry),
      m_Blob_Cache_Size(0),
      m_Blob_Cache_Size_Limit(GetDefaultBlobCacheSizeLimit()),
      m_StaticBlobCounter(0),
      m_TrackSplitSeq(false)
{
    CTSE_Lock tse_lock = AddTSE(const_cast<CSeq_entry&>(entry));
    m_StaticBlobs.PutLock(tse_lock);
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


void CDataSource::RevokeDataLoader(void)
{
    if ( m_Loader ) {
        TMainLock::TWriteLockGuard guard(m_DSMainLock);
        m_Loader = null;
    }
}


void CDataSource::DropAllTSEs(void)
{
    // Lock indexes
    TMainLock::TWriteLockGuard guard(m_DSMainLock);

    // First clear all indices
    m_InfoMap.clear();
    
    {{
        TSeqLock::TWriteLockGuard guard2(m_DSSeqLock);
        m_TSE_seq.clear();
        m_TSE_split_seq.clear();
    }}
    
    {{
        TAnnotLock::TWriteLockGuard guard2(m_DSAnnotLock);
        m_TSE_seq_annot.clear();
        m_TSE_orphan_annot.clear();
        m_DirtyAnnot_TSEs.clear();
    }}

    // then drop all TSEs
    {{
        TCacheLock::TWriteLockGuard guard2(m_DSCacheLock);
        // check if any TSE is locked by user 
        ITERATE ( TBlob_Map, it, m_Blob_Map ) {
            CAtomicCounter::TValue lock_counter = it->second->m_LockCounter.Get();
            CAtomicCounter::TValue used_counter = m_StaticBlobs.FindLock(it->second)? 1: 0;
            if ( lock_counter != used_counter ) {
                ERR_POST_X(1, "CDataSource::DropAllTSEs: tse is locked");
            }
        }
        NON_CONST_ITERATE( TBlob_Map, it, m_Blob_Map ) {
            x_ForgetTSE(it->second);
        }
        m_StaticBlobs.Drop();
        m_Blob_Map.clear();
        m_Blob_Cache.clear();
        m_Blob_Cache_Size = 0;
        m_StaticBlobCounter = 0;
    }}
}


CDataSource::TTSE_Lock CDataSource::GetSharedTSE(void) const
{
    _ASSERT(GetSharedObject());
    _ASSERT(m_StaticBlobs.size() == 1);
    return m_StaticBlobs.begin()->second;
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


CDataSource::TTSE_Lock CDataSource::AddStaticTSE(CRef<CTSE_Info> info)
{
    TMainLock::TWriteLockGuard guard(m_DSMainLock);
    if ( info->m_BlobVersion == -1 ) {
        // assign fake version for conflict resolution
        info->m_BlobVersion = -1-(++m_StaticBlobCounter);
    }
    TTSE_Lock lock = AddTSE(info);
    m_StaticBlobs.AddLock(lock);
    return lock;
}


CDataSource::TTSE_Lock CDataSource::AddStaticTSE(CSeq_entry& se)
{
    return AddStaticTSE(Ref(new CTSE_Info(se)));
}


namespace {
    // local class for calling x_DSAttach()/x_DSDetach() without catch(...)
    class CDSDetachGuard
    {
    public:
        CDSDetachGuard(void)
            : m_DataSource(0),
              m_TSE_Info(0)
            {
            }
        ~CDSDetachGuard(void)
            {
                if ( m_TSE_Info ) {
                    m_TSE_Info->x_DSDetach(*m_DataSource);
                }
            }
        void Attach(CDataSource* ds, CTSE_Info* tse)
            {
                m_DataSource = ds;
                m_TSE_Info = tse;
                m_TSE_Info->x_DSAttach(*m_DataSource);
                m_TSE_Info = 0;
                m_DataSource = 0;
            }
    private:
        CDataSource* m_DataSource;
        CTSE_Info* m_TSE_Info;

    private:
        CDSDetachGuard(const CDSDetachGuard&);
        void operator=(const CDSDetachGuard&);
    };
}


CDataSource::TTSE_Lock CDataSource::AddTSE(CRef<CTSE_Info> info)
{
    _ASSERT(!m_SharedObject || m_StaticBlobs.empty());
    TTSE_Lock lock;
    _ASSERT(IsLoaded(*info));
    _ASSERT(!info->IsLocked());
    _ASSERT(!info->HasDataSource());
    TMainLock::TWriteLockGuard guard(m_DSMainLock);
    TCacheLock::TWriteLockGuard guard2(m_DSCacheLock);
    _ASSERT(!info->IsLocked());
    _ASSERT(!info->HasDataSource());
    TBlobId blob_id = info->GetBlobId();
    if ( !blob_id ) {
        // Set pointer to TSE itself as its BlobId.
        info->m_BlobId = blob_id = new CBlobIdPtr(info.GetPointer());
    }
    if ( !m_Blob_Map.insert(TBlob_Map::value_type(blob_id, info)).second ) {
        NCBI_THROW(CObjMgrException, eFindConflict,
                   "Duplicated Blob-id");
    }
    {{
        CDSDetachGuard detach_guard;
        detach_guard.Attach(this, info);
    }}
    x_SetLock(lock, info);
    _ASSERT(info->IsLocked());
    return lock;
}


bool CDataSource::DropStaticTSE(CTSE_Info& info)
{
    TMainLock::TWriteLockGuard guard(m_DSMainLock);
    m_StaticBlobs.RemoveLock(&info);
    return DropTSE(info);
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
    _ASSERT(&info.GetDataSource() == this);
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
        _ASSERT(blob_id);
        _VERIFY(m_Blob_Map.erase(blob_id));
    }}
    _ASSERT(!info->IsLocked());
    {{
        TAnnotLock::TWriteLockGuard guard2(m_DSAnnotLock);
        m_DirtyAnnot_TSEs.erase(info);
    }}
    _ASSERT(!info->IsLocked());
}


void CDataSource::x_Map(const CObject* obj, const CTSE_Info_Object* info)
{
    typedef TInfoMap::value_type value_type;
    pair<TInfoMap::iterator, bool> ins =
        m_InfoMap.insert(value_type(obj, info));
    if ( !ins.second ) {
        CNcbiOstrstream str;
        str << "CDataSource::x_Map(): object already mapped:" <<
            " " << typeid(*obj).name() <<
            " obj: " << obj <<
            " " << typeid(*info).name() <<
            " info: " << info <<
            " was: " << ins.first->second;
        NCBI_THROW(CObjMgrException, eOtherError,
                   CNcbiOstrstreamToString(str));
    }
}


void CDataSource::x_Unmap(const CObject* obj, const CTSE_Info_Object* info)
{
    TInfoMap::iterator iter = m_InfoMap.find(obj);
    if ( iter != m_InfoMap.end() && iter->second == info ) {
        m_InfoMap.erase(iter);
    }
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


static ostream& s_Format(ostream& out, const CSeq_id_Handle& seq_id)
{
    return out << seq_id;
}


static ostream& s_Format(ostream& out, const string& str)
{
    return out << str;
}


static ostream& s_Format(ostream& out, const pair<const string, int>& na_with_zoom)
{
    out << na_with_zoom.first;
    if ( na_with_zoom.second > 0 ) {
        out << NCBI_ANNOT_TRACK_ZOOM_LEVEL_SUFFIX << na_with_zoom.second;
    }
    return out;
}


template<class Value>
static ostream& s_Format(ostream& out, const pair<const CSeq_id_Handle, Value>& request_pair)
{
    return out << request_pair.first;
}


template<class Container>
static ostream& s_Format(ostream& out, const Container& container)
{
    char separator = '(';
    for ( auto& element : container ) {
        out << separator; separator = ',';
        s_Format(out, element);
    }
    return out << ')';
}


static string s_FormatCall(const char* method_name,
                           const CSeq_id_Handle& seq_id)
{
    ostringstream out;
    out << method_name << '(' << seq_id << ')';
    return out.str();
}


static string s_FormatCall(const char* method_name,
                           const CBioseq_Handle::TId& ids)
{
    ostringstream out;
    out << method_name << '(';
    s_Format(out, ids);
    out << ')';
    return out.str();
}


static string s_FormatCall(const char* method_name,
                           const CDataSource::TSeqIdSets& id_sets)
{
    ostringstream out;
    out << method_name << '(';
    s_Format(out, id_sets);
    out << ')';
    return out.str();
}


static string s_FormatCall(const char* method_name,
                           const CDataLoader::TTSE_LockSets& tse_set)
{
    ostringstream out;
    out << method_name << '(';
    s_Format(out, tse_set);
    out << ')';
    return out.str();
}


static string s_FormatCall(const char* method_name,
                           const CSeq_id_Handle& seq_id,
                           CDataLoader::EChoice choice)
{
    ostringstream out;
    out << method_name << '(' << seq_id << ')';
    return out.str();
}


static string s_FormatCall(const char* method_name,
                           const CDataLoader::TSeq_idSet& ids,
                           const SAnnotSelector* sel,
                           CDataLoader::TProcessedNAs* processed_nas)
{
    ostringstream out;
    out << method_name << '(';
    s_Format(out, ids);
    if ( sel && sel->IsIncludedAnyNamedAnnotAccession() ) {
        out << ',';
        s_Format(out, sel->GetNamedAnnotAccessions());
        if ( processed_nas && !processed_nas->empty() ) {
            out << '-';
            s_Format(out, *processed_nas);
        }
    }
    out << ')';
    return out.str();
}


static string s_FormatCall(const char* method_name,
                           const CBioseq_Info& bioseq,
                           const SAnnotSelector* sel,
                           CDataLoader::TProcessedNAs* processed_nas)
{
    ostringstream out;
    out << method_name << '(';
    s_Format(out, bioseq.GetId());
    if ( sel && sel->IsIncludedAnyNamedAnnotAccession() ) {
        out << ',';
        s_Format(out, sel->GetNamedAnnotAccessions());
        if ( processed_nas && !processed_nas->empty() ) {
            out << '-';
            s_Format(out, *processed_nas);
        }
    }
    out << ')';
    return out.str();
}


static string s_FormatCall(const char* method_name,
                           const CDataLoader::TBlobId& blob_id)
{
    ostringstream out;
    out << method_name << '(' << blob_id << ')';
    return out.str();
}


CDataSource::TSeq_entry_Lock
CDataSource::GetSeq_entry_Lock(const CBlobIdKey& blob_id)
{
    TSeq_entry_Lock ret;
    {{
        try {
            ret.second = m_Loader->GetBlobById(blob_id);
            ret.first = ret.second;
        }
        catch ( CLoaderException& exc ) {
            exc.SetFailedCall(s_FormatCall("GetBlobById", blob_id));
            throw;
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


CDataSource::TBioseq_set_Lock
CDataSource::FindBioseq_set_Lock(const CBioseq_set& seqset,
                                 const TTSE_LockSet& /*history*/) const
{
    TBioseq_set_Lock ret;
    {{
        TMainLock::TReadLockGuard guard(m_DSMainLock);
        ret.first = x_FindBioseq_set_Info(seqset);
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


CDataSource::TSeq_feat_Lock
CDataSource::FindSeq_feat_Lock(const CSeq_id_Handle& loc_id,
                               TSeqPos loc_pos,
                               const CSeq_feat& feat) const
{
    const_cast<CDataSource*>(this)->UpdateAnnotIndex();
    TSeq_feat_Lock ret;
    TAnnotLock::TReadLockGuard guard(m_DSAnnotLock);
    for ( int i = 0; i < 2; ++i ) {
        const TSeq_id2TSE_Set& index = i? m_TSE_seq_annot: m_TSE_orphan_annot;
        TSeq_id2TSE_Set::const_iterator it = index.find(loc_id);
        if ( it != index.end() ) {
            ITERATE ( TTSE_Set, it2, it->second ) {
                ret = (*it2)->x_FindSeq_feat(loc_id, loc_pos, feat);
                if ( ret.first.first ) {
                    x_SetLock(ret.first.second,
                              ConstRef(&ret.first.first->GetTSE_Info()));
                    return ret;
                }
            }
        }
    }
    return ret;
}


void CDataSource::x_CollectBlob_ids(const CSeq_id_Handle& idh,
                                    const TSeq_id2TSE_Set& index,
                                    TLoadedBlob_ids_Set& blob_ids)
{
    TSeq_id2TSE_Set::const_iterator tse_set = index.find(idh);
    if ( tse_set != index.end() ) {
        for ( auto& tse : tse_set->second ) {
            blob_ids.insert(tse->GetBlobId());
        }
    }
}


void CDataSource::x_CollectBlob_ids(const CSeq_id_Handle& idh,
                                    const TSeq_id2SplitInfoSet& index,
                                    TLoadedBlob_ids_Set& blob_ids)
{
    auto iter = index.find(idh);
    if ( iter != index.end() ) {
        for ( auto& split : iter->second ) {
            blob_ids.insert(split->GetBlobId());
        }
    }
}


void CDataSource::x_GetLoadedBlob_ids(const CSeq_id_Handle& idh,
                                      TLoadedTypes types,
                                      TLoadedBlob_ids_Set& blob_ids) const
{
    if ( types & (fLoaded_bioseqs|fSplit_bioseqs) ) {
        TSeqLock::TReadLockGuard guard(m_DSSeqLock);
        if ( types & fLoaded_bioseqs ) {
            x_CollectBlob_ids(idh, m_TSE_seq, blob_ids);
        }
        if ( x_IsTrackingSplitSeq() && (types & fSplit_bioseqs) ) {
            x_CollectBlob_ids(idh, m_TSE_split_seq, blob_ids);
        }
    }
    if ( types & fLoaded_annots ) {
        TAnnotLock::TReadLockGuard guard(m_DSAnnotLock);
        if ( types & fLoaded_bioseq_annots ) {
            x_CollectBlob_ids(idh, m_TSE_seq_annot, blob_ids);
        }
        if ( types & fLoaded_orphan_annots ) {
            x_CollectBlob_ids(idh, m_TSE_orphan_annot, blob_ids);
        }
    }
}


void CDataSource::GetLoadedBlob_ids(const CSeq_id_Handle& idh,
                                    TLoadedTypes types,
                                    TLoadedBlob_ids& blob_ids) const
{
    TLoadedBlob_ids_Set ids;
    if ( idh.HaveMatchingHandles() ) { 
        // Try to find the best matching id (not exactly equal)
        CSeq_id_Handle::TMatches hset;
        idh.GetMatchingHandles(hset, eAllowWeakMatch);
        ITERATE ( CSeq_id_Handle::TMatches, hit, hset ) {
            x_GetLoadedBlob_ids(*hit, types, ids);
        }
    }
    else {
        x_GetLoadedBlob_ids(idh, types, ids);
    }
    ITERATE(TLoadedBlob_ids_Set, it, ids) {
        blob_ids.push_back(*it);
    }
}


CConstRef<CTSE_Info>
CDataSource::x_FindTSE_Info(const CSeq_entry& obj) const
{
    CConstRef<CTSE_Info> ret;
    TInfoMap::const_iterator found = m_InfoMap.find(&obj);
    if ( found != m_InfoMap.end() ) {
        ret = dynamic_cast<const CTSE_Info*>(&*found->second);
    }
    return ret;
}


CConstRef<CSeq_entry_Info>
CDataSource::x_FindSeq_entry_Info(const CSeq_entry& obj) const
{
    CConstRef<CSeq_entry_Info> ret;
    TInfoMap::const_iterator found = m_InfoMap.find(&obj);
    if ( found != m_InfoMap.end() ) {
        ret = dynamic_cast<const CSeq_entry_Info*>(&*found->second);
    }
    return ret;
}


CConstRef<CSeq_annot_Info>
CDataSource::x_FindSeq_annot_Info(const CSeq_annot& obj) const
{
    CConstRef<CSeq_annot_Info> ret;
    TInfoMap::const_iterator found = m_InfoMap.find(&obj);
    if ( found != m_InfoMap.end() ) {
        ret = dynamic_cast<const CSeq_annot_Info*>(&*found->second);
    }
    return ret;
}


CConstRef<CBioseq_set_Info>
CDataSource::x_FindBioseq_set_Info(const CBioseq_set& obj) const
{
    CConstRef<CBioseq_set_Info> ret;
    TInfoMap::const_iterator found = m_InfoMap.find(&obj);
    if ( found != m_InfoMap.end() ) {
        ret = dynamic_cast<const CBioseq_set_Info*>(&*found->second);
    }
    return ret;
}


CConstRef<CBioseq_Info>
CDataSource::x_FindBioseq_Info(const CBioseq& obj) const
{
    CConstRef<CBioseq_Info> ret;
    TInfoMap::const_iterator found = m_InfoMap.find(&obj);
    if ( found != m_InfoMap.end() ) {
        ret = dynamic_cast<const CBioseq_Info*>(&*found->second);
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
                                               CSeq_annot& annot)
{
    if ( m_Loader ) {
        NCBI_THROW(CObjMgrException, eModifyDataError,
                   "Can not modify a loaded entry");
    }

    TMainLock::TWriteLockGuard guard(m_DSMainLock);
    return entry_info.AddAnnot(annot);
}


CRef<CSeq_annot_Info> CDataSource::AttachAnnot(CBioseq_Base_Info& parent,
                                               CSeq_annot& annot)
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
                                                CSeq_annot& new_annot)
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


CDSAnnotLockReadGuard::CDSAnnotLockReadGuard(EEmptyGuard)
    : m_MainGuard(eEmptyGuard),
      m_AnnotGuard(eEmptyGuard)
{
}

CDSAnnotLockReadGuard::CDSAnnotLockReadGuard(CDataSource& ds)
    : m_MainGuard(ds.m_DSMainLock),
      m_AnnotGuard(ds.m_DSAnnotLock)
{
}
void CDSAnnotLockReadGuard::Guard(CDataSource& ds)
{
    m_MainGuard.Guard(ds.m_DSMainLock);
    m_AnnotGuard.Guard(ds.m_DSAnnotLock);
}


CDSAnnotLockWriteGuard::CDSAnnotLockWriteGuard(EEmptyGuard)
    : m_MainGuard(eEmptyGuard),
      m_AnnotGuard(eEmptyGuard)
{
}
CDSAnnotLockWriteGuard::CDSAnnotLockWriteGuard(CDataSource& ds)
    : m_MainGuard(ds.m_DSMainLock),
      m_AnnotGuard(ds.m_DSAnnotLock)
{
}
void CDSAnnotLockWriteGuard::Guard(CDataSource& ds)
{
    m_MainGuard.Guard(ds.m_DSMainLock);
    m_AnnotGuard.Guard(ds.m_DSAnnotLock);
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
    TMainLock::TReadLockGuard guard(m_DSMainLock);
    entry_info.UpdateAnnotIndex();
}


void CDataSource::UpdateAnnotIndex(const CSeq_annot_Info& annot_info)
{
    TMainLock::TReadLockGuard guard(m_DSMainLock);
    annot_info.UpdateAnnotIndex();
}


CDataSource::TTSE_LockSet
CDataSource::x_GetRecords(const CSeq_id_Handle& idh,
                          CDataLoader::EChoice choice)
{
    TTSE_LockSet tse_set;
    if ( m_Loader ) {
        CDataLoader::TTSE_LockSet tse_set2;
        try {
            tse_set2 = m_Loader->GetRecords(idh, choice);
        }
        catch ( CLoaderException& exc ) {
            exc.SetFailedCall(s_FormatCall("GetRecords", idh, choice));
            throw;
        }
        ITERATE ( CDataLoader::TTSE_LockSet, it, tse_set2 ) {
            tse_set.AddLock(*it);
            (*it)->x_GetRecords(idh, choice == CDataLoader::eBioseqCore);
        }
    }
    return tse_set;
}


static inline void sx_AddAnnotMatch(CDataSource::TTSE_LockMatchSet& ret,
                                    const CTSE_Lock& tse_lock,
                                    const CSeq_id_Handle& id)
{
    if ( ret.empty() ||
         ret.back().second != id ||
         ret.back().first != tse_lock ) {
        ret.push_back(pair<CTSE_Lock, CSeq_id_Handle>(tse_lock, id));
    }
}


void CDataSource::x_AddTSEAnnots(TTSE_LockMatchSet& ret,
                                 const CSeq_id_Handle& id,
                                 const CTSE_Lock& tse_lock)
{
    //TSE annot index should be locked from modification by TAnnotLockReadGuard
    const CTSE_Info& tse = *tse_lock;
    if ( tse.HasMatchingAnnotIds() ) {
        // full check
        CSeq_id_Handle::TMatches ids;
        id.GetReverseMatchingHandles(ids);
        ITERATE ( CSeq_id_Handle::TMatches, id_it2, ids ) {
            if ( tse.x_HasIdObjects(*id_it2) ) {
                sx_AddAnnotMatch(ret, tse_lock, *id_it2);
            }
        }
    }
    else if ( id.IsGi() || !tse.OnlyGiAnnotIds()  ) {
        if ( tse.x_HasIdObjects(id) ) {
            sx_AddAnnotMatch(ret, tse_lock, id);
        }
    }
}


void CDataSource::x_AddTSEBioseqAnnots(TTSE_LockMatchSet& ret,
                                       const CBioseq_Info& bioseq,
                                       const CTSE_Lock& tse_lock)
{
    const CTSE_Info& tse = *tse_lock;
    ITERATE ( CBioseq_Info::TId, id_it, bioseq.GetId() ) {
        tse.x_GetRecords(*id_it, false);
    }
    UpdateAnnotIndex(tse);
    CTSE_Info::TAnnotLockReadGuard guard(tse.GetAnnotLock());
    ITERATE ( CBioseq_Info::TId, id_it, bioseq.GetId() ) {
        x_AddTSEAnnots(ret, *id_it, tse_lock);
    }
}


void CDataSource::x_AddTSEOrphanAnnots(TTSE_LockMatchSet& ret,
                                       const TSeq_idSet& ids,
                                       const CTSE_Lock& tse_lock)
{
    const CTSE_Info& tse = *tse_lock;
    ITERATE ( TSeq_idSet, id_it, ids ) {
        if ( tse.ContainsMatchingBioseq(*id_it) ) {
            // not orphan
            return;
        }
        tse.x_GetRecords(*id_it, false);
    }
    UpdateAnnotIndex(tse);
    CTSE_Info::TAnnotLockReadGuard guard(tse.GetAnnotLock());
    ITERATE ( TSeq_idSet, id_it, ids ) {
        x_AddTSEAnnots(ret, *id_it, tse_lock);
    }
}


void CDataSource::GetTSESetWithOrphanAnnots(const TSeq_idSet& ids,
                                            TTSE_LockMatchSet& ret,
                                            const SAnnotSelector* sel,
                                            CDataLoader::TProcessedNAs* processed_nas)
{
    if ( m_Loader ) {
        // with loader installed we look only in TSEs reported by loader.

        // collect set of TSEs with orphan annotations
        CDataLoader::TTSE_LockSet tse_set;
        try {
            tse_set = m_Loader->GetOrphanAnnotRecordsNA(ids, sel, processed_nas);
        }
        catch ( CLoaderException& exc ) {
            exc.SetFailedCall(s_FormatCall("GetOrphanAnnotRecordsNA(", ids, sel, processed_nas));
            throw;
        }

        ITERATE ( CDataLoader::TTSE_LockSet, tse_it, tse_set ) {
            x_AddTSEOrphanAnnots(ret, ids, *tse_it);
        }
    }
    else {
        // without loader we look only in static TSE if any
        if ( m_StaticBlobs.size() <= 10 ) {
            ITERATE ( TTSE_LockSet, tse_it, m_StaticBlobs ) {
                x_AddTSEOrphanAnnots(ret, ids, tse_it->second);
            }
        }
        else {
            UpdateAnnotIndex();
            TAnnotLock::TReadLockGuard guard(m_DSAnnotLock);
            ITERATE ( TSeq_idSet, id_it, ids ) {
                TSeq_id2TSE_Set::const_iterator tse_set =
                    m_TSE_orphan_annot.find(*id_it);
                if (tse_set != m_TSE_orphan_annot.end()) {
                    ITERATE(TTSE_Set, tse_it, tse_set->second) {
                        sx_AddAnnotMatch(ret,
                                         m_StaticBlobs.FindLock(*tse_it),
                                         *id_it);
                    }
                }
            }
        }
    }
    sort(ret.begin(), ret.end());
    ret.erase(unique(ret.begin(), ret.end()), ret.end());
}


void CDataSource::GetTSESetWithBioseqAnnots(const CBioseq_Info& bioseq,
                                            const TTSE_Lock& tse,
                                            TTSE_LockMatchSet& ret,
                                            const SAnnotSelector* sel,
                                            CDataLoader::TProcessedNAs* processed_nas,
                                            bool external_only)
{
    if ( !external_only ) {
        // add bioseq annotations
        x_AddTSEBioseqAnnots(ret, bioseq, tse);
    }

    if ( m_Loader ) {
        // with loader installed we look only in TSE with bioseq,
        // and TSEs with external annotations reported by the loader.

        // external annotations
        CDataLoader::TTSE_LockSet tse_set2;
        try {
            tse_set2 = m_Loader->GetExternalAnnotRecordsNA(bioseq, sel, processed_nas);
        }
        catch ( CLoaderException& exc ) {
            exc.SetFailedCall(s_FormatCall("GetExternalAnnotRecordsNA", bioseq, sel, processed_nas));
            throw;
        }
        ITERATE ( CDataLoader::TTSE_LockSet, tse_it, tse_set2 ) {
            x_AddTSEBioseqAnnots(ret, bioseq, *tse_it);
        }
    }
    else {
        // without loader bioseq TSE is the same as manually added TSE
        // and we already have added it
        size_t blob_count = m_StaticBlobs.size();
        if ( blob_count <= 1 ) {
            _ASSERT(m_StaticBlobs.FindLock(tse));
        }
        else {
            TSeq_idSet ids;
            ITERATE ( CBioseq_Info::TId, it, bioseq.GetId() ) {
                if ( it->HaveReverseMatch() ) {
                    CSeq_id_Handle::TMatches hset;
                    it->GetReverseMatchingHandles(ids);
                }
                else {
                    ids.insert(*it);
                }
            }
            if ( blob_count <= 10 ) {
                ITERATE ( TTSE_LockSet, it, m_StaticBlobs ) {
                    if ( it->second == tse ) {
                        continue;
                    }
                    x_AddTSEOrphanAnnots(ret, ids, it->second);
                }
            }
            else {
                UpdateAnnotIndex();
                TAnnotLock::TReadLockGuard guard(m_DSAnnotLock);
                ITERATE ( TSeq_idSet, id_it, ids ) {
                    TSeq_id2TSE_Set::const_iterator annot_it =
                        m_TSE_orphan_annot.find(*id_it);
                    if ( annot_it == m_TSE_orphan_annot.end() ) {
                        continue;
                    }
                    ITERATE ( TTSE_Set, tse_it, annot_it->second ) {
                        if ( *tse_it == tse ) {
                            continue;
                        }
                        sx_AddAnnotMatch(ret,
                                         m_StaticBlobs.FindLock(*tse_it),
                                         *id_it);
                    }
                }
            }
        }
    }
    sort(ret.begin(), ret.end());
    ret.erase(unique(ret.begin(), ret.end()), ret.end());
}


bool CDataSource::x_IndexTSE(TSeq_id2TSE_Set& tse_map,
                             const CSeq_id_Handle& id,
                             CTSE_Info* tse_info)
{
    TSeq_id2TSE_Set::iterator it = tse_map.lower_bound(id);
    if ( it == tse_map.end() || it->first != id ) {
        it = tse_map.insert(it, TSeq_id2TSE_Set::value_type(id, TTSE_Set()));
    }
    _ASSERT(it != tse_map.end() && it->first == id);
    return it->second.insert(Ref(tse_info)).second;
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


void CDataSource::x_IndexSplitInfo(TSeq_id2SplitInfoSet& split_map,
                                   const CSeq_id_Handle& id,
                                   CTSE_Split_Info* split_info)
{
    split_map[id].insert(Ref(split_info));
}


void CDataSource::x_UnindexSplitInfo(TSeq_id2SplitInfoSet& split_map,
                                     const CSeq_id_Handle& id,
                                     CTSE_Split_Info* split_info)
{
    auto it = split_map.find(id);
    if ( it != split_map.end() ) {
        it->second.erase(Ref(split_info));
        if ( it->second.empty() ) {
            split_map.erase(it);
        }
    }
}


void CDataSource::x_IndexSeqTSELocked(const CSeq_id_Handle& id,
                                      CTSE_Info* tse_info)
{
    if ( x_IndexTSE(m_TSE_seq, id, tse_info) &&
         x_IsTrackingSplitSeq() &&
         tse_info->HasSplitInfo() ) {
        x_UnindexSplitInfo(m_TSE_split_seq, id, &tse_info->GetSplitInfo());
    }
}


void CDataSource::x_IndexSeqTSE(const CSeq_id_Handle& id,
                                CTSE_Info* tse_info)
{
    TSeqLock::TWriteLockGuard guard(m_DSSeqLock);
    x_IndexSeqTSELocked(id, tse_info);
}


void CDataSource::x_IndexSplitInfo(const CSeq_id_Handle& id,
                                   CTSE_Split_Info* split_info)
{
    if ( x_IsTrackingSplitSeq() ) {
        TSeqLock::TWriteLockGuard guard(m_DSSeqLock);
        x_IndexSplitInfo(m_TSE_split_seq, id, split_info);
    }
}


void CDataSource::x_IndexSplitInfo(const vector<CSeq_id_Handle>& ids,
                                   CTSE_Split_Info* split_info)
{
    if ( x_IsTrackingSplitSeq() ) {
        TSeqLock::TWriteLockGuard guard(m_DSSeqLock);
        for ( auto& id : ids ) {
            x_IndexSplitInfo(m_TSE_split_seq, id, split_info);
        }
    }
}


void CDataSource::x_UnindexSplitInfo(const CSeq_id_Handle& id,
                                     CTSE_Split_Info* split_info)
{
    if ( x_IsTrackingSplitSeq() ) {
        TSeqLock::TWriteLockGuard guard(m_DSSeqLock);
        x_UnindexSplitInfo(m_TSE_split_seq, id, split_info);
    }
}


void CDataSource::x_UnindexSplitInfo(const vector<CSeq_id_Handle>& ids,
                                     CTSE_Split_Info* split_info)
{
    if ( x_IsTrackingSplitSeq() ) {
        TSeqLock::TWriteLockGuard guard(m_DSSeqLock);
        for ( auto& id : ids ) {
            x_UnindexSplitInfo(m_TSE_split_seq, id, split_info);
        }
    }
}


void CDataSource::x_IndexSeqTSE(const vector<CSeq_id_Handle>& ids,
                                CTSE_Info* tse_info)
{
    TSeqLock::TWriteLockGuard guard(m_DSSeqLock);
    for ( auto& id : ids ) {
        x_IndexSeqTSELocked(id, tse_info);
    }
}


void CDataSource::x_UnindexSeqTSE(const CSeq_id_Handle& id,
                                  CTSE_Info* tse_info)
{
    TSeqLock::TWriteLockGuard guard(m_DSSeqLock);
    x_UnindexTSE(m_TSE_seq, id, tse_info);
}


void CDataSource::x_IndexAnnotTSE(const CSeq_id_Handle& id,
                                  CTSE_Info* tse_info,
                                  bool orphan)
{
    TAnnotLock::TWriteLockGuard guard(m_DSAnnotLock);
    x_IndexTSE(orphan? m_TSE_orphan_annot: m_TSE_seq_annot, id, tse_info);
}


void CDataSource::x_UnindexAnnotTSE(const CSeq_id_Handle& id,
                                    CTSE_Info* tse_info,
                                    bool orphan)
{
    TAnnotLock::TWriteLockGuard guard(m_DSAnnotLock);
    x_UnindexTSE(orphan? m_TSE_orphan_annot: m_TSE_seq_annot, id, tse_info);
}


void CDataSource::x_IndexAnnotTSEs(CTSE_Info* tse_info)
{
    TAnnotLock::TWriteLockGuard guard(m_DSAnnotLock);
    ITERATE ( CTSE_Info::TIdAnnotInfoMap, it, tse_info->m_IdAnnotInfoMap ) {
        x_IndexTSE(it->second.m_Orphan? m_TSE_orphan_annot: m_TSE_seq_annot,
                   it->first, tse_info);
    }
    if ( tse_info->x_DirtyAnnotIndex() ) {
        _VERIFY(m_DirtyAnnot_TSEs.insert(Ref(tse_info)).second);
    }
}


void CDataSource::x_UnindexAnnotTSEs(CTSE_Info* tse_info)
{
    TAnnotLock::TWriteLockGuard guard(m_DSAnnotLock);
    ITERATE ( CTSE_Info::TIdAnnotInfoMap, it, tse_info->m_IdAnnotInfoMap ) {
        x_UnindexTSE(it->second.m_Orphan? m_TSE_orphan_annot: m_TSE_seq_annot,
                     it->first, tse_info);
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
        TSeqLock::TReadLockGuard guard(m_DSSeqLock);
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
                all_tse.PutLock(tse);
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
    NCBI_THROW_FMT(CObjMgrException, eFindConflict,
                   "Multiple seq-id matches found for "<<handle);
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
        idh.GetMatchingHandles(hset, eAllowWeakMatch);
        ITERATE ( CSeq_id_Handle::TMatches, hit, hset ) {
            if ( *hit == idh ) // already checked
                continue;
            if ( ret && ret.m_Seq_id.IsBetter(*hit) ) // worse hit
                continue;
            ITERATE ( TTSE_LockSet, it, locks ) {
                it->second->x_GetRecords(*hit, true);
            }
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


SSeqMatch_DS CDataSource::x_GetSeqMatch(const CSeq_id_Handle& idh)
{
    TTSE_LockSet locks;
    try {
        return x_GetSeqMatch(idh, locks);
    }
    catch ( CObjMgrException& exc ) {
        if ( !m_Loader || exc.GetErrCode() != exc.eFindConflict ) {
            throw;
        }
    }
    return SSeqMatch_DS();
}


SSeqMatch_DS CDataSource::BestResolve(const CSeq_id_Handle& idh)
{
    return x_GetSeqMatch(idh, x_GetRecords(idh, CDataLoader::eBioseqCore));
}


CDataSource::TSeqMatches CDataSource::GetMatches(const CSeq_id_Handle& idh,
                                                 const TTSE_LockSet& history)
{
    TSeqMatches ret;

    if ( !history.empty() ) {
        TSeqLock::TReadLockGuard guard(m_DSSeqLock);
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
    SSeqMatch_DS match = x_GetSeqMatch(idh);
    if ( match ) {
        ids = match.m_Bioseq->GetId();
        return;
    }
    // Bioseq not found - try to request ids from loader if any.
    if ( m_Loader ) {
        try {
            m_Loader->GetIds(idh, ids);
        }
        catch ( CLoaderException& exc ) {
            exc.SetFailedCall(s_FormatCall("GetIds", idh));
            throw;
        }
    }
}


CDataSource::SAccVerFound CDataSource::GetAccVer(const CSeq_id_Handle& idh)
{
    SAccVerFound ret;
    SSeqMatch_DS match = x_GetSeqMatch(idh);
    if ( match ) {
        ret.acc_ver = CScope::x_GetAccVer(match.m_Bioseq->GetId());
        ret.sequence_found = true;
    }
    else if ( m_Loader ) {
        try {
            ret = m_Loader->GetAccVerFound(idh);
        }
        catch ( CLoaderException& exc ) {
            exc.SetFailedCall(s_FormatCall("GetAccVer", idh));
            throw;
        }
    }
    return ret;
}


CDataSource::SGiFound CDataSource::GetGi(const CSeq_id_Handle& idh)
{
    SGiFound ret;
    SSeqMatch_DS match = x_GetSeqMatch(idh);
    if ( match ) {
        ret.gi = CScope::x_GetGi(match.m_Bioseq->GetId());
        ret.sequence_found = true;
    }
    else if ( m_Loader ) {
        try {
            ret = m_Loader->GetGiFound(idh);
        }
        catch ( CLoaderException& exc ) {
            exc.SetFailedCall(s_FormatCall("GetGi", idh));
            throw;
        }
    }
    return ret;
}


string CDataSource::GetLabel(const CSeq_id_Handle& idh)
{
    string ret;
    try {
        TTSE_LockSet locks;
        if ( SSeqMatch_DS match = x_GetSeqMatch(idh, locks) ) {
            ret = objects::GetLabel(match.m_Bioseq->GetId());
        }
    }
    catch ( CObjMgrException& /*exc*/ ) {
        if ( !m_Loader ) {
            throw;
        }
    }
    if ( m_Loader ) {
        try {
            ret = m_Loader->GetLabel(idh);
        }
        catch ( CLoaderException& exc ) {
            exc.SetFailedCall(s_FormatCall("GetLabel", idh));
            throw;
        }
    }
    return ret;
}


TTaxId CDataSource::GetTaxId(const CSeq_id_Handle& idh)
{
    TTaxId ret = INVALID_TAX_ID;
    SSeqMatch_DS match = x_GetSeqMatch(idh);
    if ( match ) {
        ret = match.m_Bioseq->GetTaxId();
    }
    else if ( m_Loader ) {
        try {
            ret = m_Loader->GetTaxId(idh);
        }
        catch ( CLoaderException& exc ) {
            exc.SetFailedCall(s_FormatCall("GetTaxId", idh));
            throw;
        }
    }
    return ret;
}


TSeqPos CDataSource::GetSequenceLength(const CSeq_id_Handle& idh)
{
    TSeqPos ret = kInvalidSeqPos;
    SSeqMatch_DS match = x_GetSeqMatch(idh);
    if ( match ) {
        ret = match.m_Bioseq->GetBioseqLength();
    }
    else if ( m_Loader ) {
        try {
            ret = m_Loader->GetSequenceLength(idh);
        }
        catch ( CLoaderException& exc ) {
            exc.SetFailedCall(s_FormatCall("GetSequenceLength", idh));
            throw;
        }
    }
    return ret;
}


CDataSource::STypeFound
CDataSource::GetSequenceType(const CSeq_id_Handle& idh)
{
    STypeFound ret;
    SSeqMatch_DS match = x_GetSeqMatch(idh);
    if ( match ) {
        ret.type = match.m_Bioseq->GetInst_Mol();
        ret.sequence_found = true;
    }
    else if ( m_Loader ) {
        try {
            ret = m_Loader->GetSequenceTypeFound(idh);
        }
        catch ( CLoaderException& exc ) {
            exc.SetFailedCall(s_FormatCall("GetSequenceType", idh));
            throw;
        }
    }
    return ret;
}


int CDataSource::GetSequenceState(const CSeq_id_Handle& idh)
{
    int ret = CBioseq_Handle::fState_not_found|CBioseq_Handle::fState_no_data;
    SSeqMatch_DS match = x_GetSeqMatch(idh);
    if ( match ) {
        ret = match.m_Bioseq->GetTSE_Info().GetBlobState();
    }
    else if ( m_Loader ) {
        try {
            ret = m_Loader->GetSequenceState(idh);
        }
        catch ( CLoaderException& exc ) {
            exc.SetFailedCall(s_FormatCall("GetSequenceState", idh));
            throw;
        }
    }
    return ret;
}


void CDataSource::GetBulkIds(const TIds& ids, TLoaded& loaded, TBulkIds& ret)
{
    size_t count = ids.size(), remaining = 0;
    _ASSERT(ids.size() == loaded.size());
    _ASSERT(ids.size() == ret.size());
    for ( size_t i = 0; i < count; ++i ) {
        if ( loaded[i] ) {
            continue;
        }
        SSeqMatch_DS match = x_GetSeqMatch(ids[i]);
        if ( match ) {
            ret[i] = match.m_Bioseq->GetId();
            loaded[i] = true;
        }
        else {
            ++remaining;
        }
    }
    if ( remaining && m_Loader ) {
        try {
            m_Loader->GetBulkIds(ids, loaded, ret);
        }
        catch ( CLoaderException& exc ) {
            exc.SetFailedCall(s_FormatCall("GetBulkIds", ids));
            throw;
        }
    }
}


void CDataSource::GetAccVers(const TIds& ids, TLoaded& loaded, TIds& ret)
{
    size_t count = ids.size(), remaining = 0;
    _ASSERT(ids.size() == loaded.size());
    _ASSERT(ids.size() == ret.size());
    for ( size_t i = 0; i < count; ++i ) {
        if ( loaded[i] ) {
            continue;
        }
        SSeqMatch_DS match = x_GetSeqMatch(ids[i]);
        if ( match ) {
            ret[i] = CScope::x_GetAccVer(match.m_Bioseq->GetId());
            loaded[i] = true;
        }
        else {
            ++remaining;
        }
    }
    if ( remaining && m_Loader ) {
        try {
            m_Loader->GetAccVers(ids, loaded, ret);
        }
        catch ( CLoaderException& exc ) {
            exc.SetFailedCall(s_FormatCall("GetAccVers", ids));
            throw;
        }
    }
}


void CDataSource::GetGis(const TIds& ids, TLoaded& loaded, TGis& ret)
{
    size_t count = ids.size(), remaining = 0;
    _ASSERT(ids.size() == loaded.size());
    _ASSERT(ids.size() == ret.size());
    for ( size_t i = 0; i < count; ++i ) {
        if ( loaded[i] ) {
            continue;
        }
        SSeqMatch_DS match = x_GetSeqMatch(ids[i]);
        if ( match ) {
            ret[i] = CScope::x_GetGi(match.m_Bioseq->GetId());
            loaded[i] = true;
        }
        else {
            ++remaining;
        }
    }
    if ( remaining && m_Loader ) {
        try {
            m_Loader->GetGis(ids, loaded, ret);
        }
        catch ( CLoaderException& exc ) {
            exc.SetFailedCall(s_FormatCall("GetGis", ids));
            throw;
        }
    }
}


void CDataSource::GetLabels(const TIds& ids, TLoaded& loaded, TLabels& ret)
{
    size_t count = ids.size(), remaining = 0;
    _ASSERT(ids.size() == loaded.size());
    _ASSERT(ids.size() == ret.size());
    for ( size_t i = 0; i < count; ++i ) {
        if ( loaded[i] ) {
            continue;
        }
        SSeqMatch_DS match = x_GetSeqMatch(ids[i]);
        if ( match ) {
            ret[i] = objects::GetLabel(match.m_Bioseq->GetId());
            loaded[i] = true;
        }
        else {
            ++remaining;
        }
    }
    if ( remaining && m_Loader ) {
        try {
            m_Loader->GetLabels(ids, loaded, ret);
        }
        catch ( CLoaderException& exc ) {
            exc.SetFailedCall(s_FormatCall("GetLabels", ids));
            throw;
        }
    }
}


void CDataSource::GetTaxIds(const TIds& ids, TLoaded& loaded, TTaxIds& ret)
{
    size_t count = ids.size(), remaining = 0;
    _ASSERT(ids.size() == loaded.size());
    _ASSERT(ids.size() == ret.size());
    for ( size_t i = 0; i < count; ++i ) {
        if ( loaded[i] ) {
            continue;
        }
        SSeqMatch_DS match = x_GetSeqMatch(ids[i]);
        if ( match ) {
            ret[i] = match.m_Bioseq->GetTaxId();
            loaded[i] = true;
        }
        else {
            ++remaining;
        }
    }
    if ( remaining && m_Loader ) {
        try {
            m_Loader->GetTaxIds(ids, loaded, ret);
        }
        catch ( CLoaderException& exc ) {
            exc.SetFailedCall(s_FormatCall("GetTaxIds", ids));
            throw;
        }
    }
}


void CDataSource::GetSequenceLengths(const TIds& ids, TLoaded& loaded,
                                     TSequenceLengths& ret)
{
    size_t count = ids.size(), remaining = 0;
    _ASSERT(ids.size() == loaded.size());
    _ASSERT(ids.size() == ret.size());
    for ( size_t i = 0; i < count; ++i ) {
        if ( loaded[i] ) {
            continue;
        }
        SSeqMatch_DS match = x_GetSeqMatch(ids[i]);
        if ( match ) {
            ret[i] = match.m_Bioseq->GetBioseqLength();
            loaded[i] = true;
        }
        else {
            ++remaining;
        }
    }
    if ( remaining && m_Loader ) {
        try {
            m_Loader->GetSequenceLengths(ids, loaded, ret);
        }
        catch ( CLoaderException& exc ) {
            exc.SetFailedCall(s_FormatCall("GetSequenceLengths", ids));
            throw;
        }
    }
}


void CDataSource::GetSequenceTypes(const TIds& ids, TLoaded& loaded,
                                   TSequenceTypes& ret)
{
    size_t count = ids.size(), remaining = 0;
    _ASSERT(ids.size() == loaded.size());
    _ASSERT(ids.size() == ret.size());
    for ( size_t i = 0; i < count; ++i ) {
        if ( loaded[i] ) {
            continue;
        }
        SSeqMatch_DS match = x_GetSeqMatch(ids[i]);
        if ( match ) {
            ret[i] = match.m_Bioseq->GetInst_Mol();
            loaded[i] = true;
        }
        else {
            ++remaining;
        }
    }
    if ( remaining && m_Loader ) {
        try {
            m_Loader->GetSequenceTypes(ids, loaded, ret);
        }
        catch ( CLoaderException& exc ) {
            exc.SetFailedCall(s_FormatCall("GetSequenceTypes", ids));
            throw;
        }
    }
}


void CDataSource::GetSequenceStates(const TIds& ids, TLoaded& loaded,
                                    TSequenceStates& ret)
{
    size_t count = ids.size(), remaining = 0;
    _ASSERT(ids.size() == loaded.size());
    _ASSERT(ids.size() == ret.size());
    for ( size_t i = 0; i < count; ++i ) {
        if ( loaded[i] ) {
            continue;
        }
        SSeqMatch_DS match = x_GetSeqMatch(ids[i]);
        if ( match ) {
            ret[i] = match.m_Bioseq->GetTSE_Info().GetBlobState();
            loaded[i] = true;
        }
        else {
            ++remaining;
        }
    }
    if ( remaining && m_Loader ) {
        try {
            m_Loader->GetSequenceStates(ids, loaded, ret);
        }
        catch ( CLoaderException& exc ) {
            exc.SetFailedCall(s_FormatCall("GetSequenceStates", ids));
            throw;
        }
    }
}


CDataSource::SHashFound
CDataSource::GetSequenceHash(const CSeq_id_Handle& idh)
{
    SHashFound ret;
    if ( m_Loader ) {
        try {
            ret = m_Loader->GetSequenceHashFound(idh);
        }
        catch ( CLoaderException& exc ) {
            exc.SetFailedCall(s_FormatCall("GetSequenceHash", idh));
            throw;
        }
    }
    return ret;
}


void CDataSource::GetSequenceHashes(const TIds& ids, TLoaded& loaded,
                                    TSequenceHashes& ret, THashKnown& known)
{
    if ( m_Loader ) {
        try {
            m_Loader->GetSequenceHashes(ids, loaded, ret, known);
        }
        catch ( CLoaderException& exc ) {
            exc.SetFailedCall(s_FormatCall("GetSequenceHashes", ids));
            throw;
        }
    }
}


void CDataSource::GetCDDAnnots(const TSeqIdSets& id_sets, TLoaded& loaded, TCDD_Locks& ret)
{
    if (!m_Loader) return;
#ifdef NCBI_USE_TSAN
    const size_t limit_cdds_request = 15;
#else
    const size_t limit_cdds_request = 200;
#endif
    _ASSERT(id_sets.size() == loaded.size());
    _ASSERT(id_sets.size() == ret.size());
    size_t current_size = min(limit_cdds_request, id_sets.size());
    for (size_t current_start = 0; current_start < id_sets.size(); current_start += current_size) {
        if (current_size > id_sets.size() - current_start) current_size = id_sets.size() - current_start;
        TSeqIdSets current_id_sets(current_size);
        size_t current_end = current_start + current_size;
        TLoaded current_loaded(current_id_sets.size());
        TCDD_Locks current_ret(current_id_sets.size());
        copy(id_sets.begin() + current_start, id_sets.begin() + current_end, current_id_sets.begin());
        copy(loaded.begin() + current_start, loaded.begin() + current_end, current_loaded.begin());
        copy(ret.begin() + current_start, ret.begin() + current_end, current_ret.begin());
        try {
            m_Loader->GetCDDAnnots(current_id_sets, current_loaded, current_ret);
            copy(current_loaded.begin(), current_loaded.end(), loaded.begin() + current_start);
            copy(current_ret.begin(), current_ret.end(), ret.begin() + current_start);
        }
        catch ( CLoaderException& exc ) {
            exc.SetFailedCall(s_FormatCall("GetCDDAnnots", current_id_sets));
            throw;
        }
    }
}


static void s_GetBlobs(CDataLoader* loader,
                       CDataLoader::TTSE_LockSets& all_tse_sets,
                       CDataLoader::TTSE_LockSets& current_tse_sets)
{
    try {
        loader->GetBlobs(current_tse_sets);
    }
    catch ( CLoaderException& exc ) {
        exc.SetFailedCall(s_FormatCall("GetBlobs", current_tse_sets));
        throw;
    }
    if ( all_tse_sets.empty() ) {
        swap(all_tse_sets, current_tse_sets);
    }
    else {
        all_tse_sets.insert(current_tse_sets.begin(), current_tse_sets.end());
        current_tse_sets.clear();
    }
}


void CDataSource::GetBlobs(TSeqMatchMap& match_map)
{
    if ( match_map.empty() ) {
        return;
    }
    if ( m_Loader ) {
        CDataLoader::TTSE_LockSets tse_sets;
        CDataLoader::TTSE_LockSets current_tse_sets;
        size_t current_tse_sets_size = 0;
#ifdef NCBI_USE_TSAN
        const size_t limit_blobs_request = 15;
#else
        const size_t limit_blobs_request = 200;
#endif
        ITERATE(TSeqMatchMap, match, match_map) {
            _ASSERT( !match->second );
            current_tse_sets.insert(
                CDataLoader::TTSE_LockSets::value_type(
                match->first, CDataLoader::TTSE_LockSet()));
            if ( ++current_tse_sets_size >= limit_blobs_request ) {
                s_GetBlobs(m_Loader, tse_sets, current_tse_sets);
                current_tse_sets_size = 0;
            }
        }
        if ( !current_tse_sets.empty() ) {
            s_GetBlobs(m_Loader, tse_sets, current_tse_sets);
        }
        if ( s_GetBulkChunks() ) {
            // bulk chunk loading
            vector<CConstRef<CTSE_Chunk_Info>> chunks;
            for ( auto& tse_set : tse_sets ) {
                auto& idh = tse_set.first;
                CSeq_id_Handle::TMatches hset;
                if ( idh.HaveMatchingHandles() ) {
                    idh.GetMatchingHandles(hset, eAllowWeakMatch);
                }
                for ( auto& tse_lock : tse_set.second ) {
                    if ( tse_lock->HasSplitInfo() ) {
                        auto& split_info = tse_lock->GetSplitInfo();
                        split_info.x_AddChunksForGetRecords(chunks, idh);
                        for ( auto& hit : hset ) {
                            if ( hit == idh ) // already checked
                                continue;
                            split_info.x_AddChunksForGetRecords(chunks, hit);
                        }
                    }
                }
            }
            if ( !chunks.empty() ) {
                CTSE_Split_Info::x_LoadChunks(m_Loader, chunks);
            }
        }
        ITERATE(CDataLoader::TTSE_LockSets, tse_set, tse_sets) {
            TTSE_LockSet locks;
            ITERATE(CDataLoader::TTSE_LockSet, it, tse_set->second) {
                locks.AddLock(*it);
                (*it)->x_GetRecords(tse_set->first, true);
            }
            TSeqMatchMap::iterator match = match_map.find(tse_set->first);
            _ASSERT(match != match_map.end()  &&  !match->second);
            match->second = x_GetSeqMatch(tse_set->first, locks);
        }
    }
    else {
        NON_CONST_ITERATE(TSeqMatchMap, it, match_map) {
            if ( !it->second ) {
                it->second = BestResolve(it->first);
            }
        }
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
void CDataSource::Prefetch(CPrefetchTokenOld_Impl& token)
{
    if (!m_PrefetchThread) {
        CFastMutexGuard guard(m_PrefetchLock);
        // Check againi
        if (!m_PrefetchThread) {
            m_PrefetchThread.Reset(new CPrefetchThreadOld(*this));
            m_PrefetchThread->Run();
        }
    }
    _ASSERT(m_PrefetchThread);
    m_PrefetchThread->AddRequest(token);
}
#else
void CDataSource::Prefetch(CPrefetchTokenOld_Impl& /* token */)
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
        ret = m_StaticBlobs.FindLock(&tse_info);
        if ( ret ) {
            return ret;
        }
    }
    if ( (flags & fLockNoThrow) == 0 ) {
        NCBI_THROW(CObjMgrException, eOtherError,
                   "CDataSource::x_LockTSE: cannot find in locks");
    }
    return ret;
}


class CTSE_LoadLockGuard : public CObject
{
public:
    CTSE_LoadLockGuard(CDataSource* ds, CTSE_Info::CLoadMutex* mutex)
        : m_DataSource(ds), m_Mutex(mutex), m_Guard(*mutex), m_Waiting(false)
        {
        }
    ~CTSE_LoadLockGuard(void)
        {
            Release();
        }

    void Release(void)
        {
            if ( m_Mutex ) {
                if ( !m_Waiting ) {
                    m_Mutex->m_LoadWait.SignalAll();
                }
                m_Guard.Release();
                m_Mutex.Reset();
            }
        }

    bool WaitForSignal(const CDeadline& deadline)
        {
            m_Waiting = true;
            return m_Mutex->m_LoadWait.WaitForSignal(*m_Mutex, deadline);
        }

    CRef<CDataSource> GetDataSource(void) const
        {
            return m_DataSource;
        }

private:
    CRef<CDataSource>   m_DataSource;
    CRef<CTSE_Info::CLoadMutex>  m_Mutex;
    CMutexGuard     m_Guard;
    bool m_Waiting;

private:
    CTSE_LoadLockGuard(const CTSE_LoadLockGuard&);
    CTSE_LoadLockGuard operator=(const CTSE_LoadLockGuard&);
};


CTSE_LoadLock CDataSource::GetTSE_LoadLock(const TBlobId& blob_id)
{
    _ASSERT(blob_id);
    CTSE_LoadLock ret;
    {{
        CTSE_Lock lock;
        CRef<CTSE_Info::CLoadMutex> load_mutex;
        {{
            TCacheLock::TWriteLockGuard guard(m_DSCacheLock);
            TTSE_Ref& slot = m_Blob_Map[blob_id];
            if ( !slot ) {
                slot.Reset(new CTSE_Info(blob_id));
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


CTSE_LoadLock CDataSource::GetTSE_LoadLockIfLoaded(const TBlobId& blob_id)
{
    _ASSERT(blob_id);
    CTSE_LoadLock ret;
    {{
        CTSE_Lock lock;
        {{
            TCacheLock::TWriteLockGuard guard(m_DSCacheLock);
            TBlob_Map::const_iterator iter = m_Blob_Map.find(blob_id);
            if ( iter == m_Blob_Map.end() || !iter->second ||
                 !IsLoaded(*iter->second) ) {
                return ret;
            }
            x_SetLock(lock, iter->second);
        }}
        _ASSERT(lock);
        _ASSERT(IsLoaded(*lock));
        ret.m_DataSource.Reset(this);
        TSE_LOCK_TRACE("TSE_LoadLock("<<&*lock<<") "<<&lock<<" lock");
        _VERIFY(lock->m_LockCounter.Add(1) > 1);
        ret.m_Info = const_cast<CTSE_Info*>(lock.GetNonNullPointer());
    }}
    return ret;
}


CTSE_LoadLock CDataSource::GetLoadedTSE_Lock(const TBlobId& blob_id, const CTimeout& timeout)
{
    return GetLoadedTSE_Lock(blob_id, CDeadline(timeout));
}


CTSE_LoadLock CDataSource::GetLoadedTSE_Lock(const TBlobId& blob_id, const CDeadline& deadline)
{
    CTSE_LoadLock lock = GetTSE_LoadLock(blob_id);
    if ( IsLoaded(*lock) ) {
        return lock;
    }
    while ( lock.x_GetGuard().WaitForSignal(deadline) ) {
        if ( IsLoaded(*lock) ) {
            return lock;
        }
    }
    if ( IsLoaded(*lock) ) {
        return lock;
    }
    return CTSE_LoadLock();
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
        CDSDetachGuard detach_guard;
        detach_guard.Attach(this, &*lock);
    }}
    {{
        TCacheLock::TWriteLockGuard guard2(m_DSCacheLock);
        lock->m_LoadState = CTSE_Info::eLoaded;
        lock->m_LoadMutex.Reset();
    }}
    lock.ReleaseLoadLock();
}


void CDataSource::x_ReleaseLastTSELock(CRef<CTSE_Info> tse)
{
    if ( !m_Loader ) {
        // keep in cache only when loader is used
        return;
    }
    _ASSERT(tse);
    vector<TTSE_Ref> to_delete;
    {{
        TCacheLock::TWriteLockGuard guard(m_DSCacheLock);
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
            m_Blob_Cache_Size += 1;
            _ASSERT(m_Blob_Cache_Size == m_Blob_Cache.size());
            tse->m_CacheState = CTSE_Info::eInCache;
        }
        _ASSERT(tse->m_CachePosition ==
                find(m_Blob_Cache.begin(), m_Blob_Cache.end(), tse));
        _ASSERT(m_Blob_Cache_Size == m_Blob_Cache.size());
        
        unsigned cache_size = m_Blob_Cache_Size_Limit;
        while ( m_Blob_Cache_Size > cache_size ) {
            CRef<CTSE_Info> del_tse = m_Blob_Cache.front();
            m_Blob_Cache.pop_front();
            m_Blob_Cache_Size -= 1;
            _ASSERT(m_Blob_Cache_Size == m_Blob_Cache.size());
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
    TSE_LOCK_TRACE("TSE_Lock("<<&*tse<<") "<<&lock<<" lock");
    if ( tse->m_LockCounter.Add(1) != 1 ) {
        return;
    }

    TCacheLock::TWriteLockGuard guard(m_DSCacheLock);
    if ( tse->m_CacheState == CTSE_Info::eInCache ) {
        _ASSERT(tse->m_CachePosition ==
                find(m_Blob_Cache.begin(), m_Blob_Cache.end(), tse));
        tse->m_CacheState = CTSE_Info::eNotInCache;
        m_Blob_Cache.erase(tse->m_CachePosition);
        m_Blob_Cache_Size -= 1;
        _ASSERT(m_Blob_Cache_Size == m_Blob_Cache.size());
    }
    
    _ASSERT(find(m_Blob_Cache.begin(), m_Blob_Cache.end(), tse) ==
            m_Blob_Cache.end());
}


void CDataSource::x_SetLoadLock(CTSE_LoadLock& load, CTSE_Lock& lock)
{
    _ASSERT(lock && !load);
    _ASSERT(lock->IsLocked());
    load.m_DataSource.Reset(this);
    load.m_Info.Reset(const_cast<CTSE_Info*>(lock.GetNonNullPointer()));
    TSE_LOCK_TRACE("TSE_LoadLock("<<&*load<<") "<<&load<<" lock");
    load.m_Info->m_LockCounter.Add(1);
    if ( !IsLoaded(*load) ) {
        _ASSERT(load->m_LoadMutex);
        CThread::GetSystemID(&load.m_LoadLockOwner);
        load.m_LoadLock.Reset(new CTSE_LoadLockGuard(this, load->m_LoadMutex));
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
    TSE_LOCK_TRACE("TSE_LoadLock("<<&tse<<") "<<&load<<" lock");
    _VERIFY(tse.m_LockCounter.Add(1) > 1);
    load.m_Info.Reset(&tse);
    if ( !IsLoaded(tse) ) {
        _ASSERT(load_mutex);
        CThread::GetSystemID(&load.m_LoadLockOwner);
        load.m_LoadLock.Reset(new CTSE_LoadLockGuard(this, load_mutex));
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
        m_LoadLockOwner = lock.m_LoadLockOwner;
        m_LoadLock = lock.m_LoadLock;
        if ( *this ) {
            TSE_LOCK_TRACE("TSE_LoadLock("<<&*lock<<") "<<this<<" lock");
            m_Info->m_LockCounter.Add(1);
        }
    }
    return *this;
}


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


void CTSE_LoadLock::Reset(void)
{
    ReleaseLoadLock();
    if ( *this ) {
        TSE_LOCK_TRACE("TSE_LoadLock("<<&**this<<") "<<this<<" unlock");
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
            TThreadSystemID current;
            CThread::GetSystemID(&current);
            if ( m_LoadLockOwner == current ) {
                x_GetGuard().Release();
            }
        }
        else {
            // reset TSE info into empty state
            m_Info->x_Reset();
        }
        m_LoadLock.Reset();
    }
}


void CTSE_Lock::x_Drop(void)
{
    _ASSERT(*this);
    const CTSE_Info* info = GetNonNullPointer();
    TSE_LOCK_TRACE("TSE_Lock("<<info<<") "<<this<<" drop");
    _VERIFY(info->m_LockCounter.Add(-1) == 0);
    m_Info.Reset();
}


void CTSE_Lock::Swap(CTSE_Lock& lock)
{
#ifdef TSE_LOCK_NO_SWAP
    CTSE_Lock tmp(lock);
    lock = *this;
    *this = tmp;
#else
    m_Info.Swap(lock.m_Info);
#endif
}


void CTSE_Lock::x_Unlock(void)
{
    _ASSERT(*this);
    const CTSE_Info* info = GetNonNullPointer();
    TSE_LOCK_TRACE("TSE_Lock("<<info<<") "<<this<<" unlock");
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
    TSE_LOCK_TRACE("TSE_Lock("<<info<<") "<<this<<" lock");
    return info->m_LockCounter.Add(1) == 1;
}


void CTSE_Lock::x_Relock(const CTSE_Info* info)
{
    _ASSERT(!*this && info);
    _ASSERT(info->m_LockCounter.Get() != 0);
    m_Info.Reset(info);
    TSE_LOCK_TRACE("TSE_Lock("<<info<<") "<<this<<" lock");
    info->m_LockCounter.Add(1);
}


void CTSE_LockSet::clear(void)
{
    m_TSE_LockSet.clear();
}


void CTSE_LockSet::Drop(void)
{
    NON_CONST_ITERATE ( TTSE_LockSet, it, m_TSE_LockSet ) {
        it->second.Drop();
    }
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
    m_TSE_LockSet[&*lock] = lock;
    return true;
}


bool CTSE_LockSet::PutLock(CTSE_Lock& lock)
{
    m_TSE_LockSet[&*lock].Swap(lock);
    return true;
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
