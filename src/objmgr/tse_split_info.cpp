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
#include <objmgr/impl/bioseq_info.hpp>
#include <objmgr/impl/bioseq_set_info.hpp>
#include <objmgr/data_loader.hpp>
#include <objmgr/objmgr_exception.hpp>
#include <objmgr/seq_map.hpp>
#include <objects/seq/Seq_literal.hpp>

#include <algorithm>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

/////////////////////////////////////////////////////////////////////////////
// CTSE_Chunk_Info
/////////////////////////////////////////////////////////////////////////////


CTSE_Split_Info::CTSE_Split_Info(void)
    : m_BlobVersion(-1),
      m_SplitVersion(-1),
      m_BioseqChunkId(-1),
      m_SeqIdToChunksSorted(false)
{
}


CTSE_Split_Info::~CTSE_Split_Info(void)
{
    NON_CONST_ITERATE ( TChunks, it, m_Chunks ) {
        it->second->x_DropAnnotObjects();
    }
}


// TSE/DS attach
void CTSE_Split_Info::x_TSEAttach(CTSE_Info& tse)
{
    if ( !m_BlobId ) {
        m_BlobId = tse.GetBlobId();
        _ASSERT(m_BlobId);
        m_BlobVersion = tse.GetBlobVersion();
    }
    m_TSE_Set.push_back(&tse);
    NON_CONST_ITERATE ( TChunks, it, m_Chunks ) {
        it->second->x_TSEAttach(tse);
    }
}


void CTSE_Split_Info::x_DSAttach(CDataSource& ds)
{
    if ( !m_DataLoader ) {
        m_DataLoader = ds.GetDataLoader();
        _ASSERT(m_DataLoader);
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


CDataLoader& CTSE_Split_Info::GetDataLoader(void)
{
    return *m_DataLoader;
}


// chunk attach
void CTSE_Split_Info::AddChunk(CTSE_Chunk_Info& chunk_info)
{
    _ASSERT(m_Chunks.find(chunk_info.GetChunkId()) == m_Chunks.end());
    m_Chunks[chunk_info.GetChunkId()].Reset(&chunk_info);
    chunk_info.x_SplitAttach(*this);
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
    ITERATE ( TTSE_Set, it, m_TSE_Set ) {
        x_AddDescInfo(**it, info, chunk_id);
    }
}


void CTSE_Split_Info::x_AddDescInfo(CTSE_Info& tse_info,
                                    const TDescInfo& info,
                                    TChunkId chunk_id)
{
    x_GetBase(tse_info, info.second).x_AddDescrChunkId(info.first, chunk_id);
}


void CTSE_Split_Info::x_AddAnnotPlace(const TPlace& place, TChunkId chunk_id)
{
    ITERATE ( TTSE_Set, it, m_TSE_Set ) {
        x_AddAnnotPlace(**it, place, chunk_id);
    }
}


void CTSE_Split_Info::x_AddAnnotPlace(CTSE_Info& tse_info,
                                      const TPlace& place, TChunkId chunk_id)
{
    x_GetBase(tse_info, place).x_AddAnnotChunkId(chunk_id);
}


void CTSE_Split_Info::x_AddBioseqPlace(TBioseq_setId place_id,
                                       TChunkId chunk_id)
{
    if ( place_id == 0 ) {
        _ASSERT(m_BioseqChunkId < 0);
        _ASSERT(chunk_id >= 0);
        m_BioseqChunkId = chunk_id;
    }
    ITERATE ( TTSE_Set, it, m_TSE_Set ) {
        x_AddBioseqPlace(**it, place_id, chunk_id);
    }
}


void CTSE_Split_Info::x_AddBioseqPlace(CTSE_Info& tse_info,
                                       TBioseq_setId place_id,
                                       TChunkId chunk_id)
{
    if ( place_id == 0 ) {
        tse_info.x_SetNeedUpdate(CTSE_Info::fNeedUpdate_core);
    }
    else {
        x_GetBioseq_set(tse_info, place_id).x_AddBioseqChunkId(chunk_id);
    }
}


void CTSE_Split_Info::x_AddSeq_data(const TLocationSet& location,
                                    CTSE_Chunk_Info& chunk)
{
    ITERATE ( TTSE_Set, it, m_TSE_Set ) {
        x_AddSeq_data(**it, location, chunk);
    }
}


void CTSE_Split_Info::x_AddSeq_data(CTSE_Info& tse_info,
                                    const TLocationSet& locations,
                                    CTSE_Chunk_Info& chunk)
{
    ITERATE ( TLocationSet, it, locations ) {
        CBioseq_Info& bioseq = x_GetBioseq(tse_info, it->first);
        bioseq.x_AddSeq_dataChunkId(chunk.GetChunkId());

        CSeqMap& seq_map = const_cast<CSeqMap&>(bioseq.GetSeqMap());
        seq_map.SetRegionInChunk(chunk,
                                 it->second.GetFrom(),
                                 it->second.GetLength());
    }
}


void CTSE_Split_Info::x_SetContainedId(const TBioseqId& id,
                                       TChunkId chunk_id)
{
    _ASSERT(!m_SeqIdToChunksSorted);
    m_SeqIdToChunks.push_back(pair<CSeq_id_Handle, TChunkId>(id, chunk_id));
    m_SeqIdToChunksSorted = false;
}


// annot index
void CTSE_Split_Info::x_UpdateAnnotIndex(CTSE_Chunk_Info& chunk)
{
    CFastMutexGuard guard(m_SeqIdToChunksMutex);
    if ( !chunk.m_AnnotIndexEnabled ) {
        ITERATE ( TTSE_Set, it, m_TSE_Set ) {
            CTSE_Info& tse_info = **it;
            CDataSource::TAnnotLockWriteGuard guard2(tse_info.GetDataSource());
            tse_info.UpdateAnnotIndex(chunk);
        }
        chunk.m_AnnotIndexEnabled = true;
    }
}


CTSE_Split_Info::TSeqIdToChunks::const_iterator
CTSE_Split_Info::x_FindChunk(const CSeq_id_Handle& id) const
{
    if ( !m_SeqIdToChunksSorted ) {
        CFastMutexGuard guard(m_SeqIdToChunksMutex);
        if ( !m_SeqIdToChunksSorted ) {
            sort(m_SeqIdToChunks.begin(), m_SeqIdToChunks.end());
            m_SeqIdToChunksSorted = true;
        }
    }
    return lower_bound(m_SeqIdToChunks.begin(),
                       m_SeqIdToChunks.end(),
                       pair<CSeq_id_Handle, TChunkId>(id, -1));
}

// load requests
void CTSE_Split_Info::x_GetRecords(const CSeq_id_Handle& id, bool bioseq) const
{
    for ( TSeqIdToChunks::const_iterator iter = x_FindChunk(id);
          iter != m_SeqIdToChunks.end() && iter->first == id; ++iter ) {
        GetChunk(iter->second).x_GetRecords(id, bioseq);
    }
}


void CTSE_Split_Info::GetBioseqsIds(TBioseqsIds& ids) const
{
    ITERATE ( TChunks, it, m_Chunks ) {
        it->second->GetBioseqsIds(ids);
    }
}


bool CTSE_Split_Info::ContainsBioseq(const CSeq_id_Handle& id) const
{
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
    GetChunk(chunk_id).Load();
}


void CTSE_Split_Info::x_LoadChunks(const TChunkIds& chunk_ids) const
{
    ITERATE ( TChunkIds, it, chunk_ids ) {
        x_LoadChunk(*it);
    }
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
    ITERATE ( TTSE_Set, it, m_TSE_Set ) {
        x_LoadDescr(**it, place, descr);
    }
}


void CTSE_Split_Info::x_LoadDescr(CTSE_Info& tse_info,
                                  const TPlace& place,
                                  const CSeq_descr& descr)
{
    x_GetBase(tse_info, place).AddSeq_descr(descr);
}


void CTSE_Split_Info::x_LoadAnnot(const TPlace& place,
                                  CRef<CSeq_annot_Info> annot)
{
    CRef<CSeq_annot_Info> add;
    ITERATE ( TTSE_Set, it, m_TSE_Set ) {
        if ( !add ) {
            add = annot;
        }
        else {
            add = new CSeq_annot_Info(*annot);
        }
        x_LoadAnnot(**it, place, add);
    }
}


void CTSE_Split_Info::x_LoadAnnot(CTSE_Info& tse_info,
                                  const TPlace& place,
                                  CRef<CSeq_annot_Info> annot)
{
    {{
        CDataSource::TMainLock::TWriteLockGuard guard
            (tse_info.GetDataSource().m_DSMainLock);
        x_GetBase(tse_info, place).AddAnnot(annot);
    }}
    {{
        CDataSource::TAnnotLockWriteGuard guard(tse_info.GetDataSource());
        tse_info.UpdateAnnotIndex(*annot);
    }}
}


void CTSE_Split_Info::x_LoadBioseq(const TPlace& place, const CBioseq& bioseq)
{
    CRef<CSeq_entry_Info> add;
    ITERATE ( TTSE_Set, it, m_TSE_Set ) {
        if ( !add ) {
            add = new CSeq_entry_Info(*new CSeq_entry);
            add->SelectSeq(const_cast<CBioseq&>(bioseq));
        }
        else {
            add = new CSeq_entry_Info(*add);
        }
        x_LoadBioseq(**it, place, add);
    }
}


void CTSE_Split_Info::x_LoadBioseq(CTSE_Info& tse_info,
                                   const TPlace& place,
                                   CRef<CSeq_entry_Info> entry)
{
    {{
        CDataSource::TMainLock::TWriteLockGuard guard
            (tse_info.GetDataSource().m_DSMainLock);
        x_GetBioseq_set(tse_info, place).AddEntry(entry);
    }}
}


void CTSE_Split_Info::x_LoadSequence(const TPlace& place, TSeqPos pos,
                                     const TSequence& sequence)
{
    ITERATE ( TTSE_Set, it, m_TSE_Set ) {
        x_LoadSequence(**it, place, pos, sequence);
    }
}


void CTSE_Split_Info::x_LoadSequence(CTSE_Info& tse_info,
                                     const TPlace& place, TSeqPos pos,
                                     const TSequence& sequence)
{
    CSeqMap& seq_map =
        const_cast<CSeqMap&>(x_GetBioseq(tse_info, place).GetSeqMap());
    ITERATE ( TSequence, it, sequence ) {
        const CSeq_literal& literal = **it;
        seq_map.LoadSeq_data(pos, literal.GetLength(), literal.GetSeq_data());
        pos += literal.GetLength();
    }
}


void CTSE_Split_Info::x_LoadAssembly(const TPlace& place,
                                     const TAssembly& assembly)
{
    ITERATE ( TTSE_Set, it, m_TSE_Set ) {
        x_LoadAssembly(**it, place, assembly);
    }
}


void CTSE_Split_Info::x_LoadAssembly(CTSE_Info& tse_info,
                                     const TPlace& place,
                                     const TAssembly& assembly)
{
    x_GetBioseq(tse_info, place).SetInst_Hist_Assembly(assembly);
}


/////////////////////////////////////////////////////////////////////////////
// get attach points
CBioseq_Info& CTSE_Split_Info::x_GetBioseq(CTSE_Info& tse_info,
                                           const TBioseqId& place_id)
{
    return tse_info.x_GetBioseq(place_id);
}


CBioseq_set_Info& CTSE_Split_Info::x_GetBioseq_set(CTSE_Info& tse_info,
                                                   TBioseq_setId place_id)
{
    return tse_info.x_GetBioseq_set(place_id);
}


CBioseq_Base_Info& CTSE_Split_Info::x_GetBase(CTSE_Info& tse_info,
                                              const TPlace& place)
{
    if ( place.first ) {
        return x_GetBioseq(tse_info, place.first);
    }
    else {
        return x_GetBioseq_set(tse_info, place.second);
    }
}


CBioseq_Info& CTSE_Split_Info::x_GetBioseq(CTSE_Info& tse_info,
                                           const TPlace& place)
{
    if ( place.first ) {
        return x_GetBioseq(tse_info, place.first);
    }
    else {
        NCBI_THROW(CObjMgrException, eOtherError,
                   "Bioseq-set id where gi is expected");
    }
}


CBioseq_set_Info& CTSE_Split_Info::x_GetBioseq_set(CTSE_Info& tse_info,
                                                   const TPlace& place)
{
    if ( place.first ) {
        NCBI_THROW(CObjMgrException, eOtherError,
                   "Gi where Bioseq-set id is expected");
    }
    else {
        return x_GetBioseq_set(tse_info, place.second);
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE

