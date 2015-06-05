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
* Author: Eugene Vasilchenko
*
* File Description:
*   Split TSE info
*
*/


#include <ncbi_pch.hpp>

#include <objmgr/impl/tse_split_info.hpp>
#include <objmgr/impl/tse_info.hpp>
#include <objmgr/impl/tse_chunk_info.hpp>
#include <objmgr/impl/data_source.hpp>
#include <objmgr/impl/seq_annot_info.hpp>
//#include <objmgr/impl/bioseq_info.hpp>
//#include <objmgr/impl/bioseq_set_info.hpp>
#include <objmgr/impl/tse_assigner.hpp>
#include <objmgr/data_loader.hpp>
#include <objmgr/objmgr_exception.hpp>
#include <objmgr/seq_map.hpp>
#include <objmgr/prefetch_manager.hpp>
#include <objects/seq/Seq_literal.hpp>

#include <algorithm>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


/////////////////////////////////////////////////////////////////////////////
// CTSE_Chunk_Info
/////////////////////////////////////////////////////////////////////////////


CTSE_Split_Info::CTSE_Split_Info(void)
    : m_DataLoader(0),
      m_BlobVersion(-1),
      m_SplitVersion(-1),
      m_BioseqChunkId(-1),
      m_SeqIdToChunksSorted(false),
      m_ContainsBioseqs(false)
{
}

CTSE_Split_Info::CTSE_Split_Info(TBlobId blob_id, TBlobVersion blob_ver)
    : m_DataLoader(0),
      m_BlobId(blob_id),
      m_BlobVersion(blob_ver),
      m_SplitVersion(-1),
      m_BioseqChunkId(-1),
      m_SeqIdToChunksSorted(false),
      m_ContainsBioseqs(false)
{
}


CTSE_Split_Info::~CTSE_Split_Info(void)
{
    NON_CONST_ITERATE ( TChunks, it, m_Chunks ) {
        it->second->x_DropAnnotObjects();
    }
}


// TSE/DS attach

void CTSE_Split_Info::x_TSEAttach(CTSE_Info& tse, CRef<ITSE_Assigner>& lsnr)
{
    m_TSE_Set.insert(TTSE_Set::value_type(&tse, lsnr));
    NON_CONST_ITERATE ( TChunks, it, m_Chunks ) {
        it->second->x_TSEAttach(tse, *lsnr);
    }
}

void CTSE_Split_Info::x_TSEDetach(CTSE_Info& tse_info)
{
    m_TSE_Set.erase(&tse_info);
}


CRef<ITSE_Assigner> CTSE_Split_Info::GetAssigner(const CTSE_Info& tse)
{
    CRef<ITSE_Assigner> ret;
    TTSE_Set::const_iterator it = m_TSE_Set.find(const_cast<CTSE_Info*>(&tse));
    if( it != m_TSE_Set.end() )
        return it->second;
    
    return CRef<ITSE_Assigner>();
}


void CTSE_Split_Info::x_DSAttach(CDataSource& ds)
{
    if ( !m_DataLoader && ds.GetDataLoader() ) {
        m_DataLoader = &ds;
        _ASSERT(m_DataLoader);
    }
}


void CTSE_Split_Info::x_DSDetach(CDataSource& ds)
{
    if ( m_DataLoader == &ds ) {
        m_DataLoader = 0;
    }
}


// identification
CTSE_Split_Info::TBlobId CTSE_Split_Info::GetBlobId(void) const
{
    _ASSERT(m_BlobId);
    return m_BlobId;
}


CTSE_Split_Info::TBlobVersion CTSE_Split_Info::GetBlobVersion(void) const
{
    return m_BlobVersion;
}


CTSE_Split_Info::TSplitVersion CTSE_Split_Info::GetSplitVersion(void) const
{
    _ASSERT(m_SplitVersion >= 0);
    return m_SplitVersion;
}


void CTSE_Split_Info::SetSplitVersion(TSplitVersion version)
{
    _ASSERT(m_SplitVersion < 0);
    _ASSERT(version >= 0);
    m_SplitVersion = version;
}


CInitMutexPool& CTSE_Split_Info::GetMutexPool(void)
{
    return m_MutexPool;
}


CDataLoader& CTSE_Split_Info::GetDataLoader(void) const
{
    _ASSERT(m_DataLoader);
    CDataLoader* loader = m_DataLoader->GetDataLoader();
    _ASSERT(loader);
    return *loader;
}


bool CTSE_Split_Info::x_HasDelayedMainChunk(void) const
{
    TChunks::const_iterator iter = m_Chunks.end(), begin = m_Chunks.begin();
    return iter != begin && (--iter)->first == kMax_Int;
}


bool CTSE_Split_Info::x_NeedsDelayedMainChunk(void) const
{
    TChunks::const_iterator iter = m_Chunks.end(), begin = m_Chunks.begin();
    if ( iter == begin || (--iter)->first != kMax_Int ) {
        return false;
    }
    return iter == begin || ((--iter)->first == kMax_Int-1 && iter == begin);
}


// chunk attach
void CTSE_Split_Info::AddChunk(CTSE_Chunk_Info& chunk_info)
{
    CMutexGuard guard(m_SeqIdToChunksMutex);
    _ASSERT(m_Chunks.find(chunk_info.GetChunkId()) == m_Chunks.end());
    _ASSERT(m_Chunks.empty() || chunk_info.GetChunkId() != kMax_Int);
    bool need_update = x_HasDelayedMainChunk();
    m_Chunks[chunk_info.GetChunkId()].Reset(&chunk_info);
    chunk_info.x_SplitAttach(*this);
    if ( need_update ) {
        chunk_info.x_EnableAnnotIndex();
    }
}


CTSE_Chunk_Info& CTSE_Split_Info::GetChunk(TChunkId chunk_id)
{
    TChunks::iterator iter = m_Chunks.find(chunk_id);
    if ( iter == m_Chunks.end() ) {
        NCBI_THROW(CObjMgrException, eAddDataError,
                   "invalid chunk id: "+NStr::IntToString(chunk_id));
    }
    return *iter->second;
}


const CTSE_Chunk_Info& CTSE_Split_Info::GetChunk(TChunkId chunk_id) const
{
    TChunks::const_iterator iter = m_Chunks.find(chunk_id);
    if ( iter == m_Chunks.end() ) {
        NCBI_THROW(CObjMgrException, eAddDataError,
                   "invalid chunk id: "+NStr::IntToString(chunk_id));
    }
    return *iter->second;
}


CTSE_Chunk_Info& CTSE_Split_Info::GetSkeletonChunk(void)
{
    TChunks::iterator iter = m_Chunks.find(0);
    if ( iter != m_Chunks.end() ) {
        return *iter->second;
    }
    
    CRef<CTSE_Chunk_Info> chunk(new CTSE_Chunk_Info(0));
    AddChunk(*chunk);
    _ASSERT(chunk == &GetChunk(0));

    return *chunk;
}


// split info
void CTSE_Split_Info::x_AddDescInfo(const TDescInfo& info, TChunkId chunk_id)
{
    NON_CONST_ITERATE ( TTSE_Set, it, m_TSE_Set ) {
        CTSE_Info& tse = *it->first;
        ITSE_Assigner& listener = *it->second;
        listener.AddDescInfo(tse, info, chunk_id);
    }
}

void CTSE_Split_Info::x_AddAssemblyInfo(const TAssemblyInfo& info,
                                        TChunkId chunk_id)
{
    NON_CONST_ITERATE ( TTSE_Set, it, m_TSE_Set ) {
        CTSE_Info& tse = *it->first;
        ITSE_Assigner& listener = *it->second;
        listener.AddAssemblyInfo(tse, info, chunk_id);
    }
}

void CTSE_Split_Info::x_AddAnnotPlace(const TPlace& place, TChunkId chunk_id)
{
    NON_CONST_ITERATE ( TTSE_Set, it, m_TSE_Set ) {
        CTSE_Info& tse = *it->first;
        ITSE_Assigner& listener = *it->second;
        listener.AddAnnotPlace(tse, place, chunk_id);
    }
}

void CTSE_Split_Info::x_AddBioseqPlace(TBioseq_setId place_id,
                                       TChunkId chunk_id)
{
    if ( place_id == kTSE_Place_id ) {
        _ASSERT(m_BioseqChunkId < 0);
        _ASSERT(chunk_id >= 0);
        m_BioseqChunkId = chunk_id;
    }
    NON_CONST_ITERATE ( TTSE_Set, it, m_TSE_Set ) {
        CTSE_Info& tse = *it->first;
        ITSE_Assigner& listener = *it->second;
        listener.AddBioseqPlace(tse, place_id, chunk_id);
    }
}

void CTSE_Split_Info::x_AddSeq_data(const TLocationSet& location,
                                    CTSE_Chunk_Info& chunk)
{
    NON_CONST_ITERATE ( TTSE_Set, it, m_TSE_Set ) {
        CTSE_Info& tse = *it->first;
        ITSE_Assigner& listener = *it->second;
        listener.AddSeq_data(tse, location, chunk);
    }
}

void CTSE_Split_Info::x_SetContainedId(const TBioseqId& id,
                                       TChunkId chunk_id,
                                       bool bioseq)
{
    m_SeqIdToChunksSorted = false;
    if ( bioseq && !m_ContainsBioseqs ) {
        m_ContainsBioseqs = true;
    }
    m_SeqIdToChunks.push_back(pair<CSeq_id_Handle, TChunkId>(id, chunk_id));
}


bool CTSE_Split_Info::x_CanAddBioseq(const TBioseqId& id) const
{
    ITERATE ( TTSE_Set, it, m_TSE_Set ) {
        if ( it->first->ContainsBioseq(id) ) {
            return false;
        }
    }
    return true;
}


// annot index
void CTSE_Split_Info::x_UpdateFeatIdIndex(CSeqFeatData::E_Choice type,
                                          EFeatIdType id_type)
{
    NON_CONST_ITERATE ( TChunks, it, m_Chunks ) {
        CTSE_Chunk_Info& chunk = *it->second;
        if ( !chunk.IsLoaded() && !chunk.m_AnnotIndexEnabled &&
             chunk.x_ContainsFeatIds(type, id_type) ) {
            x_UpdateAnnotIndex(chunk);
        }
    }
}


void CTSE_Split_Info::x_UpdateFeatIdIndex(CSeqFeatData::ESubtype subtype,
                                          EFeatIdType id_type)
{
    NON_CONST_ITERATE ( TChunks, it, m_Chunks ) {
        CTSE_Chunk_Info& chunk = *it->second;
        if ( !chunk.IsLoaded() && !chunk.m_AnnotIndexEnabled &&
             chunk.x_ContainsFeatIds(subtype, id_type) ) {
            x_UpdateAnnotIndex(chunk);
        }
    }
}


void CTSE_Split_Info::x_UpdateAnnotIndex(void)
{
    NON_CONST_ITERATE ( TChunks, it, m_Chunks ) {
        x_UpdateAnnotIndex(*it->second);
    }
}


void CTSE_Split_Info::x_UpdateAnnotIndex(CTSE_Chunk_Info& chunk)
{
    if ( !chunk.IsLoaded() && !chunk.m_AnnotIndexEnabled ) {
        NON_CONST_ITERATE ( TTSE_Set, it, m_TSE_Set ) {
            CTSE_Info& tse = *it->first;
            ITSE_Assigner& listener = *it->second;
            listener.UpdateAnnotIndex(tse, chunk);
        }
        chunk.m_AnnotIndexEnabled = true;
    }
}


CTSE_Split_Info::TSeqIdToChunks::const_iterator
CTSE_Split_Info::x_FindChunk(const CSeq_id_Handle& id) const
{
    if ( !m_SeqIdToChunksSorted ) {
        TSeqIdToChunks(m_SeqIdToChunks).swap(m_SeqIdToChunks);
        sort(m_SeqIdToChunks.begin(), m_SeqIdToChunks.end());
        m_SeqIdToChunksSorted = true;
    }
    return lower_bound(m_SeqIdToChunks.begin(),
                       m_SeqIdToChunks.end(),
                       pair<CSeq_id_Handle, TChunkId>(id, -1));
}

// load requests
void CTSE_Split_Info::x_GetRecords(const CSeq_id_Handle& id, bool bioseq) const
{
    if ( bioseq && !m_ContainsBioseqs ) {
        // shortcut - this TSE doesn't contain any Bioseqs
        return;
    }
    vector< CConstRef<CTSE_Chunk_Info> > chunks;
    {{
        CMutexGuard guard(m_SeqIdToChunksMutex);
        for ( TSeqIdToChunks::const_iterator iter = x_FindChunk(id);
              iter != m_SeqIdToChunks.end() && iter->first == id; ++iter ) {
            const CTSE_Chunk_Info& chunk = GetChunk(iter->second);
            if ( !chunk.IsLoaded() ) {
                chunks.push_back(ConstRef(&chunk));
            }
        }
    }}
    ITERATE ( vector< CConstRef<CTSE_Chunk_Info> >, it, chunks ) {
        (*it)->x_GetRecords(id, bioseq);
    }
}


void CTSE_Split_Info::GetBioseqsIds(TSeqIds& ids) const
{
    ITERATE ( TChunks, it, m_Chunks ) {
        it->second->GetBioseqsIds(ids);
    }
}


bool CTSE_Split_Info::ContainsBioseq(const CSeq_id_Handle& id) const
{
    if ( !m_ContainsBioseqs ) {
        // shortcut - this TSE doesn't contain any Bioseqs
        return false;
    }
    CMutexGuard guard(m_SeqIdToChunksMutex);
    for ( TSeqIdToChunks::const_iterator iter = x_FindChunk(id);
          iter != m_SeqIdToChunks.end() && iter->first == id; ++iter ) {
        if ( GetChunk(iter->second).ContainsBioseq(id) ) {
            return true;
        }
    }
    return false;
}


void CTSE_Split_Info::x_LoadChunk(TChunkId chunk_id) const
{
    CPrefetchManager::IsActive();
    GetChunk(chunk_id).Load();
}


void CTSE_Split_Info::x_LoadChunks(const TChunkIds& chunk_ids) const
{
    if ( CPrefetchManager::IsActive() ) {
        ITERATE ( TChunkIds, it, chunk_ids ) {
            LoadChunk(*it);
        }
        return;
    }

    CTSE_Split_Info& info_nc = const_cast<CTSE_Split_Info&>(*this);
    typedef vector< CRef<CTSE_Chunk_Info> > TChunkRefs;
    typedef vector< AutoPtr<CInitGuard> >   TInitGuards;
    TChunkIds sorted_ids = chunk_ids;
    sort(sorted_ids.begin(), sorted_ids.end());
    sorted_ids.erase(unique(sorted_ids.begin(), sorted_ids.end()),
        sorted_ids.end());
    TChunkRefs chunks;
    chunks.reserve(sorted_ids.size());
    TInitGuards guards;
    guards.reserve(sorted_ids.size());
    // Collect and lock all chunks to be loaded
    ITERATE(TChunkIds, id, sorted_ids) {
        CRef<CTSE_Chunk_Info> chunk(&info_nc.GetChunk(*id));
        AutoPtr<CInitGuard> guard(
            new CInitGuard(chunk->m_LoadLock, info_nc.GetMutexPool()));
        if ( !(*guard.get()) ) {
            continue;
        }
        chunks.push_back(chunk);
        guards.push_back(guard);
    }
    // Load chunks
    info_nc.GetDataLoader().GetChunks(chunks);
}


void CTSE_Split_Info::x_UpdateCore(void)
{
    if ( m_BioseqChunkId >= 0 ) {
        GetChunk(m_BioseqChunkId).Load();
    }
}


// load results
void CTSE_Split_Info::x_LoadDescr(const TPlace& place,
                                  const CSeq_descr& descr)
{
    NON_CONST_ITERATE ( TTSE_Set, it, m_TSE_Set ) {
        CTSE_Info& tse = *it->first;
        ITSE_Assigner& listener = *it->second;
        listener.LoadDescr(tse, place, descr);
    }
}


void CTSE_Split_Info::x_LoadAnnot(const TPlace& place,
                                  const CSeq_annot& annot,
                                  TIntId chunk_id)
{
    CRef<CSeq_annot> add;
    NON_CONST_ITERATE ( TTSE_Set, it, m_TSE_Set ) {
        CTSE_Info& tse = *it->first;
        ITSE_Assigner& listener = *it->second;
        if ( !add ) {
            add.Reset(const_cast<CSeq_annot*>(&annot));
        }
        else {
            CRef<CSeq_annot> tmp(add);
            add.Reset(new CSeq_annot);
            add->Assign(*tmp);
        }
        listener.LoadAnnot(tse, place, add, chunk_id);
    }
}

void CTSE_Split_Info::x_LoadBioseq(const TPlace& place, const CBioseq& bioseq)
{
    CRef<CSeq_entry> add;
    NON_CONST_ITERATE ( TTSE_Set, it, m_TSE_Set ) {
        CTSE_Info& tse = *it->first;
        ITSE_Assigner& listener = *it->second;
        if ( !add ) {
            add = new CSeq_entry;
            add->SetSeq(const_cast<CBioseq&>(bioseq));
        }
        else {
            CRef<CSeq_entry> tmp(add);
            add.Reset(new CSeq_entry);
            add->Assign(*tmp);
        }
        listener.LoadBioseq(tse, place, add);
    }
}


void CTSE_Split_Info::x_LoadSequence(const TPlace& place, TSeqPos pos,
                                     const TSequence& sequence)
{
    NON_CONST_ITERATE ( TTSE_Set, it, m_TSE_Set ) {
        CTSE_Info& tse = *it->first;
        ITSE_Assigner& listener = *it->second;
        listener.LoadSequence(tse, place, pos, sequence);
    }
}


void CTSE_Split_Info::x_LoadAssembly(const TBioseqId& seq_id,
                                     const TAssembly& assembly)
{
    NON_CONST_ITERATE ( TTSE_Set, it, m_TSE_Set ) {
        CTSE_Info& tse = *it->first;
        ITSE_Assigner& listener = *it->second;
        listener.LoadAssembly(tse, seq_id, assembly);
    }
}


void CTSE_Split_Info::x_LoadSeq_entry(CSeq_entry& entry,
                                      CTSE_SetObjectInfo* set_info)
{
    CRef<CSeq_entry> add;
    NON_CONST_ITERATE ( TTSE_Set, it, m_TSE_Set ) {
        CTSE_Info& tse = *it->first;
        ITSE_Assigner& listener = *it->second;
        if ( !add ) {
            add = &entry;
        }
        else {
            add = new CSeq_entry;
            add->Assign(entry);
            set_info = 0;
        }
        listener.LoadSeq_entry(tse, *add, set_info);
    }
}


void CTSE_Split_Info::x_SetBioseqUpdater(CRef<CBioseqUpdater> updater)
{
    NON_CONST_ITERATE ( TTSE_Set, it, m_TSE_Set ) {
        CTSE_Info& tse = *it->first;
        tse.SetBioseqUpdater(updater);
   }
}


END_SCOPE(objects)
END_NCBI_SCOPE

