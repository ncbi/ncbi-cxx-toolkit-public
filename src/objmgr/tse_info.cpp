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
*   TSE info -- entry for data source seq-id to TSE map
*
*/


#include <ncbi_pch.hpp>
#include <objmgr/impl/data_source.hpp>
#include <objmgr/impl/tse_info.hpp>
#include <objmgr/impl/tse_split_info.hpp>
#include <objmgr/impl/tse_chunk_info.hpp>
#include <objmgr/impl/bioseq_info.hpp>
#include <objmgr/impl/annot_object.hpp>
#include <objmgr/impl/seq_annot_info.hpp>
#include <objmgr/impl/snp_annot_info.hpp>
#include <objmgr/impl/annot_type_index.hpp>
#include <objmgr/impl/handle_range.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objmgr/objmgr_exception.hpp>
#include <algorithm>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


SIdAnnotObjs::SIdAnnotObjs(void)
{
}


SIdAnnotObjs::~SIdAnnotObjs(void)
{
    NON_CONST_ITERATE ( TAnnotSet, it, m_AnnotSet ) {
        delete *it;
        *it = 0;
    }
}


SIdAnnotObjs::TRangeMap& SIdAnnotObjs::x_GetRangeMap(size_t index)
{
    if ( index >= m_AnnotSet.size() ) {
        m_AnnotSet.resize(index+1);
    }
    TRangeMap*& slot = m_AnnotSet[index];
    if ( !slot ) {
        slot = new TRangeMap;
    }
    return *slot;
}


bool SIdAnnotObjs::x_CleanRangeMaps(void)
{
    while ( !m_AnnotSet.empty() ) {
        TRangeMap*& slot = m_AnnotSet.back();
        if ( slot ) {
            if ( !slot->empty() ) {
                return false;
            }
            delete slot;
            slot = 0;
        }
        m_AnnotSet.pop_back();
    }
    return true;
}


SIdAnnotObjs::SIdAnnotObjs(const SIdAnnotObjs& _DEBUG_ARG(objs))
{
    _ASSERT(objs.m_AnnotSet.empty());
    _ASSERT(objs.m_SNPSet.empty());
}


////////////////////////////////////////////////////////////////////
//
//  CTSE_Info::
//
//    General information and indexes for top level seq-entries
//


CTSE_Info::CTSE_Info(void)
{
    x_Initialize();

    x_TSEAttach(*this);
}


CTSE_Info::CTSE_Info(const TBlobId& blob_id,
                     TBlobVersion blob_version)
{
    x_Initialize();

    m_BlobId = blob_id;
    m_BlobVersion = blob_version;

    x_TSEAttach(*this);
}


CTSE_Info::CTSE_Info(CSeq_entry& entry,
                     TBlobState blob_state,
                     const TBlobId& blob_id,
                     TBlobVersion blob_version)
{
    x_Initialize();

    m_BlobId = blob_id;
    m_BlobVersion = blob_version;
    m_BlobState = blob_state;

    SetSeq_entry(entry);
    m_LoadState = eLoaded;

    x_TSEAttach(*this);
}


CTSE_Info::CTSE_Info(CSeq_entry& entry,
                     TBlobState blob_state)
{
    x_Initialize();

    m_BlobState = blob_state;

    SetSeq_entry(entry);
    m_LoadState = eLoaded;

    x_TSEAttach(*this);
}


CTSE_Info::CTSE_Info(const CTSE_Info& info)
    : TParent(info)
{
    x_Initialize();

    m_BlobState = info.m_BlobState;
    m_Name = info.m_Name;
    m_UsedMemory = info.m_UsedMemory;
    m_LoadState = eLoaded;
    m_Split = info.m_Split;

    x_TSEAttach(*this);
}


CTSE_Info::~CTSE_Info(void)
{
    _ASSERT(m_LockCounter.Get() == 0);
    _ASSERT(m_DataSource == 0);
}


void CTSE_Info::x_Initialize(void)
{
    m_DataSource = 0;
    m_BlobVersion = -1;
    m_BlobState = CBioseq_Handle::fState_none;
    m_UsedMemory = 0;
    m_LoadState = eNotLoaded;
    m_CacheState = eNotInCache;
    m_LockCounter.Set(0);
    m_AnnotIdsFlags = 0;
}


void CTSE_Info::SetBlobVersion(TBlobVersion version)
{
    _ASSERT(version >= 0);
    _ASSERT(m_LoadState == eNotLoaded || !m_Object ||
            m_BlobVersion < 0 || m_BlobVersion == version);
    m_BlobVersion = version;
}


void CTSE_Info::SetName(const CAnnotName& name)
{
    m_Name = name;
}


void CTSE_Info::SetSeq_entry(CSeq_entry& entry)
{
    entry.Parentize();
    if ( HasDataSource() ) {
        {{
            CDataSource::TMainLock::TWriteLockGuard guard
                (GetDataSource().m_DSMainLock);
            x_SetObject(entry);
        }}
        UpdateAnnotIndex();
    }
    else {
        x_SetObject(entry);
    }
}


void CTSE_Info::SetSeq_entry(CSeq_entry& entry, const TSNP_InfoMap& snps)
{
    m_SNP_InfoMap = snps;
    SetSeq_entry(entry);
    if ( !m_SNP_InfoMap.empty() ) {
        NCBI_THROW(CObjMgrException, eAddDataError,
                   "Unknown SNP annots");
    }
}


CRef<CSeq_annot_SNP_Info> CTSE_Info::x_GetSNP_Info(const TSNP_InfoKey& annot)
{
    CRef<CSeq_annot_SNP_Info> ret;
    TSNP_InfoMap::iterator iter = m_SNP_InfoMap.find(annot);
    if ( iter != m_SNP_InfoMap.end() ) {
        ret = iter->second;
        m_SNP_InfoMap.erase(iter);
    }
    return ret;
}


bool CTSE_Info::HasAnnot(const CAnnotName& name) const
{
    TAnnotLockReadGuard guard(GetAnnotLock());
    return m_NamedAnnotObjs.find(name) != m_NamedAnnotObjs.end();
}


bool CTSE_Info::HasNamedAnnot(const string& name) const
{
    return HasAnnot(CAnnotName(name));
}


bool CTSE_Info::HasUnnamedAnnot(void) const
{
    return HasAnnot(CAnnotName());
}


CConstRef<CSeq_entry> CTSE_Info::GetCompleteTSE(void) const
{
    return GetCompleteSeq_entry();
}


CConstRef<CSeq_entry> CTSE_Info::GetTSECore(void) const
{
    return GetSeq_entryCore();
}


void CTSE_Info::x_DSAttachContents(CDataSource& ds)
{
    _ASSERT(!m_DataSource);

    m_DataSource = &ds;
    TParent::x_DSAttachContents(ds);
    if ( m_Split ) {
        m_Split->x_DSAttach(ds);
    }
    ITERATE ( TBioseqs, it, m_Bioseqs ) {
        ds.x_IndexSeqTSE(it->first, this);
    }
    ds.x_IndexAnnotTSEs(this);
}


void CTSE_Info::x_DSDetachContents(CDataSource& ds)
{
    _ASSERT(m_DataSource == &ds);

    ITERATE ( TBioseqs, it, m_Bioseqs ) {
        ds.x_UnindexSeqTSE(it->first, this);
    }
    ds.x_UnindexAnnotTSEs(this);
    TParent::x_DSDetachContents(ds);
    m_DataSource = 0;
}


void CTSE_Info::x_DSMapObject(CConstRef<TObject> obj, CDataSource& ds)
{
    ds.x_Map(obj, this);
    TParent::x_DSMapObject(obj, ds);
}


void CTSE_Info::x_DSUnmapObject(CConstRef<TObject> obj, CDataSource& ds)
{
    ds.x_Unmap(obj, this);
    TParent::x_DSUnmapObject(obj, ds);
}


inline
void CTSE_Info::x_IndexSeqTSE(const CSeq_id_Handle& id)
{
    if ( HasDataSource() ) {
        GetDataSource().x_IndexSeqTSE(id, this);
    }
}


inline
void CTSE_Info::x_UnindexSeqTSE(const CSeq_id_Handle& id)
{
    if ( HasDataSource() ) {
        GetDataSource().x_UnindexSeqTSE(id, this);
    }
}


void CTSE_Info::x_IndexAnnotTSE(const CAnnotName& name,
                                const CSeq_id_Handle& id)
{
    if ( ContainsMatchingBioseq(id) ) {
        return;
    }
    TSeqIdToNames::iterator iter = m_SeqIdToNames.lower_bound(id);
    if ( iter == m_SeqIdToNames.end() || iter->first != id ) {
        iter = m_SeqIdToNames.insert(iter,
                                     TSeqIdToNames::value_type(id, TNames()));
        if ( HasDataSource() ) {
            GetDataSource().x_IndexAnnotTSE(id, this);
        }
    }
    _VERIFY(iter->second.insert(name).second);
}


void CTSE_Info::x_UnindexAnnotTSE(const CAnnotName& name,
                                  const CSeq_id_Handle& id)
{
    TSeqIdToNames::iterator iter = m_SeqIdToNames.lower_bound(id);
    if ( iter == m_SeqIdToNames.end() || iter->first != id ) {
        return;
    }
    _ASSERT(iter != m_SeqIdToNames.end() && iter->first == id);
    _VERIFY(iter->second.erase(name) == 1);
    if ( iter->second.empty() ) {
        m_SeqIdToNames.erase(iter);
        if ( HasDataSource() ) {
            GetDataSource().x_UnindexAnnotTSE(id, this);
        }
    }
}


void CTSE_Info::x_DoUpdate(TNeedUpdateFlags flags)
{
    if ( flags & (fNeedUpdate_core|fNeedUpdate_children_core) ) {
        if ( m_Split ) {
            m_Split->x_UpdateCore();
        }
    }
    TParent::x_DoUpdate(flags);
}


void CTSE_Info::GetBioseqsIds(TBioseqsIds& ids) const
{
    {{
        CDataSource::TMainLock::TReadLockGuard guard
            (GetDataSource().m_DSMainLock);
        ITERATE ( TBioseqs, it, m_Bioseqs ) {
            ids.push_back(it->first);
        }
    }}
    if ( m_Split ) {
        m_Split->GetBioseqsIds(ids);
        // after adding split bioseq Seq-ids the result may contain
        // duplicates and need to be sorted
        sort(ids.begin(), ids.end());
        ids.erase(unique(ids.begin(), ids.end()), ids.end());
    }
}


bool CTSE_Info::ContainsBioseq(const CSeq_id_Handle& id) const
{
    {{
        CDataSource::TMainLock::TReadLockGuard guard
            (GetDataSource().m_DSMainLock);
        if ( m_Bioseqs.find(id) != m_Bioseqs.end() ) {
            return true;
        }
    }}
    if ( m_Split && m_Split->ContainsBioseq(id) ) {
        return true;
    }
    return false;
}


bool CTSE_Info::ContainsMatchingBioseq(const CSeq_id_Handle& id) const
{
    if ( ContainsBioseq(id) ) {
        return true;
    }
    else if ( id.HaveMatchingHandles() ) {
        CSeq_id_Handle::TMatches ids;
        id.GetMatchingHandles(ids);
        ITERATE ( CSeq_id_Handle::TMatches, match_it, ids ) {
            if ( *match_it != id ) {
                if ( ContainsBioseq(*match_it) ) {
                    return true;
                }
            }
        }
    }
    return false;
}


CConstRef<CBioseq_Info> CTSE_Info::FindBioseq(const CSeq_id_Handle& id) const
{
    CConstRef<CBioseq_Info> ret;
    x_GetRecords(id, true);
    {{
        CDataSource::TMainLock::TReadLockGuard guard
            (GetDataSource().m_DSMainLock);
        TBioseqs::const_iterator it = m_Bioseqs.find(id);
        if ( it != m_Bioseqs.end() ) {
            ret = it->second;
        }
    }}
    return ret;
}


CConstRef<CBioseq_Info>
CTSE_Info::FindMatchingBioseq(const CSeq_id_Handle& id) const
{
    return GetSeqMatch(id).m_Bioseq;
}


SSeqMatch_TSE CTSE_Info::GetSeqMatch(const CSeq_id_Handle& id) const
{
    SSeqMatch_TSE ret;
    ret.m_Bioseq = FindBioseq(id);
    if ( ret.m_Bioseq ) {
        ret.m_Seq_id = id;
    }
    else if ( id.HaveMatchingHandles() ) {
        CSeq_id_Handle::TMatches ids;
        id.GetMatchingHandles(ids);
        ITERATE ( CSeq_id_Handle::TMatches, match_it, ids ) {
            if ( *match_it != id ) {
                ret.m_Bioseq = FindBioseq(*match_it);
                if ( ret.m_Bioseq ) {
                    ret.m_Seq_id = *match_it;
                    break;
                }
            }
        }
    }
    return ret;
}


void CTSE_Info::x_GetRecords(const CSeq_id_Handle& id, bool bioseq) const
{
    if ( m_Split ) {
        m_Split->x_GetRecords(id, bioseq);
    }
}


void CTSE_Info::x_LoadChunk(TChunkId chunk_id) const
{
    m_Split->x_LoadChunk(chunk_id);
}


void CTSE_Info::x_LoadChunks(const TChunkIds& chunk_ids) const
{
    m_Split->x_LoadChunks(chunk_ids);
}


void CTSE_Info::x_SetBioseqId(const CSeq_id_Handle& id,
                              CBioseq_Info* info)
{
    _ASSERT(info);
    pair<TBioseqs::iterator, bool> ins =
        m_Bioseqs.insert(TBioseqs::value_type(id, info));
    if ( ins.second ) {
        // register this TSE in data source as containing the sequence
        x_IndexSeqTSE(id);
    }
    else {
        // No duplicate bioseqs in the same TSE
        NCBI_THROW(CObjMgrException, eAddDataError,
                   "duplicate Bioseq id "+id.AsString()+" present in"+
                   "\n  seq1: " + ins.first->second->IdString()+
                   "\n  seq2: " + info->IdString());
    }
}


void CTSE_Info::x_ResetBioseqId(const CSeq_id_Handle& id,
                                CBioseq_Info* _DEBUG_ARG(info))
{
    TBioseqs::iterator iter = m_Bioseqs.lower_bound(id);
    if ( iter != m_Bioseqs.end() && iter->first == id ) {
        _ASSERT(iter->second == info);
        m_Bioseqs.erase(iter);
        x_UnindexSeqTSE(id);
    }
}


void CTSE_Info::x_SetBioseq_setId(int key,
                                  CBioseq_set_Info* info)
{
    pair<TBioseq_sets::iterator, bool> ins =
        m_Bioseq_sets.insert(TBioseq_sets::value_type(key, info));
    if ( ins.second ) {
        // everything is fine
    }
    else {
        // No duplicate bioseqs in the same TSE
        NCBI_THROW(CObjMgrException, eAddDataError,
                   " duplicate Bioseq_set id '"+NStr::IntToString(key));
    }
}


void CTSE_Info::x_ResetBioseq_setId(int key,
                                    CBioseq_set_Info* _DEBUG_ARG(info))
{
    TBioseq_sets::iterator iter = m_Bioseq_sets.lower_bound(key);
    if ( iter != m_Bioseq_sets.end() && iter->first == key ) {
        _ASSERT(iter->second == info);
        m_Bioseq_sets.erase(iter);
    }
}


void CTSE_Info::x_SetDirtyAnnotIndexNoParent(void)
{
    if ( HasDataSource() ) {
        GetDataSource().x_SetDirtyAnnotIndex(*this);
    }
}


void CTSE_Info::x_ResetDirtyAnnotIndexNoParent(void)
{
    if ( HasDataSource() ) {
        GetDataSource().x_ResetDirtyAnnotIndex(*this);
    }
}


void CTSE_Info::UpdateAnnotIndex(const CSeq_id_Handle& id) const
{
    x_GetRecords(id, false);
    const_cast<CTSE_Info*>(this)->UpdateAnnotIndex();
}


void CTSE_Info::UpdateAnnotIndex(void) const
{
    const_cast<CTSE_Info*>(this)->UpdateAnnotIndex();
}


void CTSE_Info::UpdateAnnotIndex(const CTSE_Info_Object& object) const
{
    const_cast<CTSE_Info*>(this)->
        UpdateAnnotIndex(const_cast<CTSE_Info_Object&>(object));
}


void CTSE_Info::UpdateAnnotIndex(void)
{
    CDataSource::TAnnotLockWriteGuard guard(GetDataSource());
    UpdateAnnotIndex(*this);
}


void CTSE_Info::UpdateAnnotIndex(CTSE_Info_Object& object)
{
    _ASSERT(&object.GetTSE_Info() == this);
    TAnnotLockWriteGuard guard(GetAnnotLock());
    object.x_UpdateAnnotIndex(*this);
    _ASSERT(!object.x_DirtyAnnotIndex());
}


void CTSE_Info::UpdateAnnotIndex(CTSE_Chunk_Info& chunk)
{
    TAnnotLockWriteGuard guard(GetAnnotLock());
    chunk.x_UpdateAnnotIndex(*this);
}


void CTSE_Info::x_UpdateAnnotIndexContents(CTSE_Info& tse)
{
    _ASSERT(this == &tse);
    TParent::x_UpdateAnnotIndexContents(tse);
}


CTSE_Info::TAnnotObjs& CTSE_Info::x_SetAnnotObjs(const CAnnotName& name)
{
    TNamedAnnotObjs::iterator iter = m_NamedAnnotObjs.lower_bound(name);
    if ( iter == m_NamedAnnotObjs.end() || iter->first != name ) {
        typedef TNamedAnnotObjs::value_type value_type;
        iter = m_NamedAnnotObjs.insert(iter, value_type(name, TAnnotObjs()));
    }
    return iter->second;
}


void CTSE_Info::x_RemoveAnnotObjs(const CAnnotName& name)
{
    m_NamedAnnotObjs.erase(name);
}


const CTSE_Info::TAnnotObjs*
CTSE_Info::x_GetAnnotObjs(const CAnnotName& name) const
{
    TNamedAnnotObjs::const_iterator iter = m_NamedAnnotObjs.lower_bound(name);
    if ( iter == m_NamedAnnotObjs.end() || iter->first != name ) {
        return 0;
    }
    return &iter->second;
}


const CTSE_Info::TAnnotObjs*
CTSE_Info::x_GetUnnamedAnnotObjs(void) const
{
    TNamedAnnotObjs::const_iterator iter = m_NamedAnnotObjs.begin();
    if ( iter == m_NamedAnnotObjs.end() || iter->first.IsNamed() ) {
        return 0;
    }
    return &iter->second;
}


SIdAnnotObjs& CTSE_Info::x_SetIdObjects(TAnnotObjs& objs,
                                        const CAnnotName& name,
                                        const CSeq_id_Handle& id)
{
    // repeat for more generic types of selector
    TAnnotObjs::iterator it = objs.lower_bound(id);
    if ( it == objs.end() || it->first != id ) {
        // new id
        it = objs.insert(it, TAnnotObjs::value_type(id, SIdAnnotObjs()));
        x_IndexAnnotTSE(name, id);
    }
    _ASSERT(it != objs.end() && it->first == id);
    return it->second;
}


SIdAnnotObjs& CTSE_Info::x_SetIdObjects(const CAnnotName& name,
                                        const CSeq_id_Handle& id)
{
    return x_SetIdObjects(x_SetAnnotObjs(name), name, id);
}


const SIdAnnotObjs* CTSE_Info::x_GetIdObjects(const TAnnotObjs& objs,
                                              const CSeq_id_Handle& idh) const
{
    TAnnotObjs::const_iterator it = objs.lower_bound(idh);
    if ( it == objs.end() || it->first != idh ) {
        return 0;
    }
    return &it->second;
}


const SIdAnnotObjs* CTSE_Info::x_GetIdObjects(const CAnnotName& name,
                                              const CSeq_id_Handle& idh) const
{
    const TAnnotObjs* objs = x_GetAnnotObjs(name);
    if ( !objs ) {
        return 0;
    }
    return x_GetIdObjects(*objs, idh);
}


const SIdAnnotObjs*
CTSE_Info::x_GetUnnamedIdObjects(const CSeq_id_Handle& idh) const
{
    const TAnnotObjs* objs = x_GetUnnamedAnnotObjs();
    if ( !objs ) {
        return 0;
    }
    return x_GetIdObjects(*objs, idh);
}


bool CTSE_Info::x_HasIdObjects(const CSeq_id_Handle& idh) const
{
    // tse annot index should be locked by TAnnotLockReadGuard
    ITERATE ( TNamedAnnotObjs, it, m_NamedAnnotObjs ) {
        if ( x_GetIdObjects(it->second, idh) ) {
            return true;
        }
    }
    return false;
}


inline
void CTSE_Info::x_MapAnnotObject(TRangeMap& rangeMap,
                                 const SAnnotObject_Key& key,
                                 const SAnnotObject_Index& index)
{
    _ASSERT(index.m_AnnotObject_Info == key.m_AnnotObject_Info);
    rangeMap.insert(TRangeMap::value_type(key.m_Range, index));
}


inline
bool CTSE_Info::x_UnmapAnnotObject(TRangeMap& rangeMap,
                                   const SAnnotObject_Key& key)
{
    for ( TRangeMap::iterator it = rangeMap.find(key.m_Range);
          it && it->first == key.m_Range; ++it ) {
        if ( it->second.m_AnnotObject_Info == key.m_AnnotObject_Info ) {
            rangeMap.erase(it);
            return rangeMap.empty();
        }
    }
    _ASSERT(0);
    return rangeMap.empty();
}


void CTSE_Info::x_MapAnnotObject(SIdAnnotObjs& objs,
                                 const SAnnotObject_Key& key,
                                 const SAnnotObject_Index& index)
{
    if ( key.m_AnnotObject_Info->IsLocs() ) {
        // Locs may contain multiple indexes
        CAnnotObject_Info::TTypeIndexSet idx_set;
        key.m_AnnotObject_Info->GetLocsTypes(idx_set);
        ITERATE(CAnnotObject_Info::TTypeIndexSet, idx_rg, idx_set) {
            for (size_t idx = idx_rg->first; idx < idx_rg->second; ++idx) {
                x_MapAnnotObject(objs.x_GetRangeMap(idx), key, index);
            }
        }
    }
    else {
        size_t idx = CAnnotType_Index::GetTypeIndex(key);
        x_MapAnnotObject(objs.x_GetRangeMap(idx), key, index);
    }
}


bool CTSE_Info::x_UnmapAnnotObject(SIdAnnotObjs& objs,
                                   const SAnnotObject_Key& key)
{
    size_t index = CAnnotType_Index::GetTypeIndex(key);
    _ASSERT(index < objs.x_GetRangeMapCount());
    if ( x_UnmapAnnotObject(objs.x_GetRangeMap(index), key) ) {
        if ( objs.x_CleanRangeMaps() ) {
            return objs.m_SNPSet.empty();
        }
    }
    return false;
}


void CTSE_Info::x_MapAnnotObject(TAnnotObjs& objs,
                                 const CAnnotName& name,
                                 const SAnnotObject_Key& key,
                                 const SAnnotObject_Index& index)
{
    x_MapAnnotObject(x_SetIdObjects(objs, name, key.m_Handle), key, index);
}


bool CTSE_Info::x_UnmapAnnotObject(TAnnotObjs& objs,
                                   const CAnnotName& name,
                                   const SAnnotObject_Key& key)
{
    TAnnotObjs::iterator it = objs.find(key.m_Handle);
    _ASSERT(it != objs.end() && it->first == key.m_Handle);
    if ( x_UnmapAnnotObject(it->second, key) ) {
        x_UnindexAnnotTSE(name, key.m_Handle);
        objs.erase(it);
        return objs.empty();
    }
    return false;
}


void CTSE_Info::x_MapSNP_Table(const CAnnotName& name,
                               const CSeq_id_Handle& key,
                               const CSeq_annot_SNP_Info& snp_info)
{
    SIdAnnotObjs& objs = x_SetIdObjects(name, key);
    objs.m_SNPSet.push_back(ConstRef(&snp_info));
}


void CTSE_Info::x_UnmapSNP_Table(const CAnnotName& name,
                                 const CSeq_id_Handle& key,
                                 const CSeq_annot_SNP_Info& snp_info)
{
    SIdAnnotObjs& objs = x_SetIdObjects(name, key);
    TSNPSet::iterator iter = find(objs.m_SNPSet.begin(),
                                  objs.m_SNPSet.end(),
                                  ConstRef(&snp_info));
    if ( iter != objs.m_SNPSet.end() ) {
        objs.m_SNPSet.erase(iter);
    }
}


void CTSE_Info::x_MapAnnotObjects(const SAnnotObjectsIndex& infos)
{
    size_t count = infos.GetKeys().size();
    if ( count == 0 ) {
        return;
    }
    _ASSERT(infos.GetIndices().size() == count);

    const CAnnotName& name = infos.GetName();
    TAnnotObjs& index = x_SetAnnotObjs(name);

    for ( size_t i = 0; i < count; ++i ) {
        x_MapAnnotObject(index, name, infos.GetKeys()[i],
                         infos.GetIndices()[i]);
    }
}


void CTSE_Info::x_UnmapAnnotObjects(const SAnnotObjectsIndex& infos)
{
    size_t count = infos.GetKeys().size();
    if ( count == 0 ) {
        return;
    }

    const CAnnotName& name = infos.GetName();
    TAnnotObjs& index = x_SetAnnotObjs(name);

    for ( size_t i = 0; i < count; ++i ) {
        x_UnmapAnnotObject(index, name, infos.GetKeys()[i]);
    }

    if ( index.empty() ) {
        x_RemoveAnnotObjs(name);
    }
}


CBioseq_set_Info& CTSE_Info::x_GetBioseq_set(int id)
{
    TBioseq_sets::iterator iter = m_Bioseq_sets.find(id);
    if ( iter == m_Bioseq_sets.end() ) {
        NCBI_THROW(CObjMgrException, eRegisterError,
                   "cannot find Bioseq-set by local id");
    }
    return *iter->second;
}


CBioseq_Info& CTSE_Info::x_GetBioseq(const CSeq_id_Handle& id)
{
    TBioseqs::iterator iter = m_Bioseqs.find(id);
    if ( iter == m_Bioseqs.end() ) {
        NCBI_THROW(CObjMgrException, eRegisterError,
                   "cannot find Bioseq by Seq-id "+id.AsString());
    }
    return *iter->second;
}


CTSE_Split_Info& CTSE_Info::GetSplitInfo(void)
{
    if ( !m_Split ) {
        m_Split = new CTSE_Split_Info;
        m_Split->x_TSEAttach(*this);
    }
    return *m_Split;
}


END_SCOPE(objects)
END_NCBI_SCOPE
