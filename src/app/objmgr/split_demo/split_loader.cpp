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
*  Author: Aleksey Grichenko
*
*  File Description:
*   Sample split data loader
*
*/

#include <ncbi_pch.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seq/Seq_literal.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/general/Object_id.hpp>
#include <serial/objistr.hpp>
#include <serial/serial.hpp>
#include <objmgr/impl/tse_info.hpp>
#include <objmgr/impl/seq_annot_info.hpp>
#include <objmgr/impl/tse_split_info.hpp>
#include <objmgr/impl/tse_loadlock.hpp>

#include "split_loader.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


CSplitDataLoader::TRegisterLoaderInfo CSplitDataLoader::RegisterInObjectManager(
    CObjectManager& om,
    const string& data_file,
    CObjectManager::EIsDefault is_default,
    CObjectManager::TPriority priority)
{
    TSplitLoaderMaker maker(data_file);
    CDataLoader::RegisterInObjectManager(om, maker, is_default, priority);
    return maker.GetRegisterInfo();
}


string CSplitDataLoader::GetLoaderNameFromArgs(const string& data_file)
{
    return "SPLIT_LOADER_DEMO:" + data_file;
}


CSplitDataLoader::CSplitDataLoader(const string& loader_name,
                                   const string& data_file)
  : CDataLoader(loader_name),
    m_DataFile(data_file),
    m_NextSeqsetId(0),
    m_NextChunkId(0)
{
}


CSplitDataLoader::~CSplitDataLoader(void)
{
}


void CSplitDataLoader::DropTSE(CRef<CTSE_Info> tse_info)
{
    // Release loaded data, reset chunk mappings
    m_TSE.Reset();
    m_AnnotChunks.clear();
    m_DescrChunks.clear();
    m_SeqChunks.clear();
    m_NextSeqsetId = 0;
    m_NextChunkId = 0;
}


CSplitDataLoader::TTSE_LockSet
CSplitDataLoader::GetRecords(const CSeq_id_Handle& idh,
                             EChoice choice)
{
    // Split information is filled by x_LoadData()
    CTSE_LoadLock lock = x_LoadData();
    _ASSERT(m_TSE);
    TTSE_LockSet locks;
    locks.insert(TTSE_Lock(lock));
    return locks;
}


void CSplitDataLoader::GetChunk(TChunk chunk)
{
    // Check if already loaded
    if ( chunk->IsLoaded() ) {
        return;
    }
    // Find annotations related to the chunk
    TAnnotChunks::iterator annot_chunks =
        m_AnnotChunks.find(chunk->GetChunkId());
    if (annot_chunks != m_AnnotChunks.end()) {
        // Attach all related annotations
        ITERATE(TAnnots, annot, annot_chunks->second) {
            CRef<CSeq_annot_Info> info(new CSeq_annot_Info(*annot->m_Annot));
            chunk->x_LoadAnnot(annot->m_Place, info);
        }
    }
    // Find descriptors related to the chunk
    TDescrChunks::iterator descr = m_DescrChunks.find(chunk->GetChunkId());
    if (descr != m_DescrChunks.end()) {
        // Attach related descriptor
        chunk->x_LoadDescr(descr->second.m_Place, *descr->second.m_Descr);
    }
    // Load sequence data
    TSequenceChunks::iterator seq = m_SeqChunks.find(chunk->GetChunkId());
    if (seq != m_SeqChunks.end()) {
        // Attach seq-data
        CTSE_Chunk_Info::TSequence sequence;
        sequence.push_back(seq->second.m_Literal);
        chunk->x_LoadSequence(seq->second.m_Place, seq->second.m_Pos,
            sequence);
    }
    // Mark chunk as loaded
    chunk->SetLoaded();
}


CSplitDataLoader::TBlobId
CSplitDataLoader::GetBlobId(const CSeq_id_Handle& idh)
{
    // Check if the id is known
    if (m_Ids.find(idh) != m_Ids.end()) {
        return TBlobId(m_TSE.GetPointer());
    }
    return TBlobId(0);
}


bool CSplitDataLoader::CanGetBlobById(void) const
{
    return true;
}


CSplitDataLoader::TTSE_Lock
CSplitDataLoader::GetBlobById(const TBlobId& blob_id)
{
    // Load data, get the lock
    CTSE_LoadLock lock = x_LoadData();
    if (blob_id == TBlobId(m_TSE)) {
        return TTSE_Lock(lock);
    }
    return TTSE_Lock();
}


CTSE_LoadLock CSplitDataLoader::x_LoadData(void)
{
    if ( m_TSE ) {
        // Already loaded, get and return the lock
        return GetDataSource()->GetTSE_LoadLock(TBlobId(&*m_TSE));
    }
    m_TSE.Reset(new CSeq_entry);
    m_NextSeqsetId = 0;
    m_NextChunkId = 0;
    {{
        // Load the seq-entry
        auto_ptr<CObjectIStream> is(CObjectIStream::Open(m_DataFile,
            eSerial_AsnText));
        *is >> *m_TSE;
    }}
    TChunks chunks;
    // Split data
    if ( m_TSE->IsSeq() ) {
        x_SplitSeq(chunks, m_TSE->SetSeq());
    }
    else {
        x_SplitSet(chunks, m_TSE->SetSet());
    }
    // Fill TSE info
    CTSE_LoadLock load_lock =
        GetDataSource()->GetTSE_LoadLock(TBlobId(&*m_TSE));
    CTSE_Info& info = *load_lock;
    info.SetSeq_entry(*m_TSE);
    // Attach all chunks to the TSE info
    NON_CONST_ITERATE(TChunks, it, chunks) {
        info.GetSplitInfo().AddChunk(**it);
    }
    // Mark TSE info as loaded
    load_lock.SetLoaded();
    return load_lock;
}


void CSplitDataLoader::x_SplitSeq(TChunks& chunks, CBioseq& bioseq)
{
    // Split and remove annots
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*bioseq.GetFirstId());
    if ( bioseq.IsSetAnnot() ) {
        x_SplitAnnot(chunks, TPlace(idh, 0), bioseq.SetAnnot());
        bioseq.SetAnnot().clear();
    }
    // Split and remove descrs
    if ( bioseq.IsSetDescr() ) {
        x_SplitDescr(chunks, TPlace(idh, 0), bioseq.SetDescr());
        bioseq.ResetDescr();
    }
    // Split and remove data
    x_SplitSeqData(chunks, idh, bioseq);
}


void CSplitDataLoader::x_SplitSet(TChunks& chunks, CBioseq_set& seqset)
{
    NON_CONST_ITERATE(CBioseq_set::TSeq_set, it, seqset.SetSeq_set()) {
        if ( (*it)->IsSeq() ) {
            x_SplitSeq(chunks, (*it)->SetSeq());
        }
        else {
            x_SplitSet(chunks, (*it)->SetSet());
        }
    }
    TPlace place;
    seqset.SetId().SetId(++m_NextSeqsetId);
    place.second = seqset.GetId().GetId();
    // Split and remove annots
    if ( seqset.IsSetAnnot() ) {
        x_SplitAnnot(chunks, place, seqset.SetAnnot());
        seqset.SetAnnot().clear();
    }
    // Split and remove descrs
    if ( seqset.IsSetDescr() ) {
        x_SplitDescr(chunks, place, seqset.SetDescr());
        seqset.ResetDescr();
    }
}


void CSplitDataLoader::x_SplitSeqData(TChunks& chunks,
                                      CSeq_id_Handle idh,
                                      CBioseq& bioseq)
{
    if ( !bioseq.GetInst().IsSetSeq_data() ) {
        return;
    }
    // Prepare internal data
    SSeqData data;
    data.m_Pos = 0;
    data.m_Place.first = idh;
    data.m_Literal.Reset(new CSeq_literal);
    data.m_Literal->SetLength(bioseq.GetInst().IsSetLength() ?
        bioseq.GetInst().GetLength() : 0);
    data.m_Literal->SetSeq_data(bioseq.SetInst().SetSeq_data());
    bioseq.SetInst().ResetSeq_data();

    // Create location for the chunk
    CTSE_Chunk_Info::TLocationSet loc_set;
    CTSE_Chunk_Info::TLocationRange rg =
        CTSE_Chunk_Info::TLocationRange::GetWhole();
    CTSE_Chunk_Info::TLocation loc(idh, rg);
    loc_set.push_back(loc);

    // Create new chunk for the data
    CRef<CTSE_Chunk_Info> chunk(new CTSE_Chunk_Info(++m_NextChunkId));
    // Add seq-data
    chunk->x_AddSeq_data(loc_set);
    // Store data locally, remember place and chunk id
    m_SeqChunks[m_NextChunkId] = data;
    chunks.push_back(chunk);
}


void CSplitDataLoader::x_SplitDescr(TChunks& chunks,
                                    TPlace place,
                                    CSeq_descr& descr)
{
    // Create new chunk for each descr
    CRef<CTSE_Chunk_Info> chunk(new CTSE_Chunk_Info(++m_NextChunkId));
    // Add descr info using bioseq id or bioseq-set id
    // Descr type mask includes everything for simplicity
    if ( place.first ) {
        chunk->x_AddDescInfo(0xffff, place.first);
    }
    else {
        chunk->x_AddDescInfo(0xffff, place.second);
    }
    // Store data locally, remember place and chunk id
    SDescrData data;
    data.m_Place = place;
    data.m_Descr.Reset(&descr);
    m_DescrChunks[m_NextChunkId] = data;
    chunks.push_back(chunk);
}


void CSplitDataLoader::x_SplitAnnot(TChunks& chunks,
                                    TPlace place,
                                    CBioseq::TAnnot& annots)
{
    // Create new chunk for each set of annots
    CRef<CTSE_Chunk_Info> chunk(new CTSE_Chunk_Info(++m_NextChunkId));
    // Register attachment place for annots
    chunk->x_AddAnnotPlace(place);
    SAnnotData data;
    data.m_Place = place;
    TAnnots& annot_chunks = m_AnnotChunks[m_NextChunkId];
    // Process each annot
    NON_CONST_ITERATE(CBioseq::TAnnot, ait, annots) {
        CSeq_annot& annot = **ait;
        data.m_Annot = &annot;
        annot_chunks.push_back(data);
        switch ( annot.GetData().Which() ) {
        case CSeq_annot::C_Data::e_Ftable:
            x_SplitFeats(*chunk, annot);
            break;
        case CSeq_annot::C_Data::e_Align:
            x_SplitAligns(*chunk, annot);
            break;
        case CSeq_annot::C_Data::e_Graph:
            x_SplitGraphs(*chunk, annot);
            break;
        default:
            // ignore other annotations
            continue;
        }
    }
    chunks.push_back(chunk);
}


void CSplitDataLoader::x_SplitFeats(CTSE_Chunk_Info& chunk,
                                    const CSeq_annot& annot)
{
    _ASSERT(annot.GetData().IsFtable());
    ITERATE(CSeq_annot::C_Data::TFtable, it, annot.GetData().GetFtable()) {
        const CSeq_feat& feat = **it;
        // Get type and all referenced seq-ids for each feature
        SAnnotTypeSelector sel(feat.GetData().Which());
        set<CSeq_id_Handle> handles;
        for (CSeq_loc_CI loc_it(feat.GetLocation()); loc_it; ++loc_it) {
            handles.insert(loc_it.GetSeq_id_Handle());
        }
        if ( feat.IsSetProduct() ) {
            for (CSeq_loc_CI loc_it(feat.GetProduct()); loc_it; ++loc_it) {
                handles.insert(loc_it.GetSeq_id_Handle());
            }
        }
        // Register each referenced seq-id and feature type
        ITERATE(set<CSeq_id_Handle>, idh, handles) {
            chunk.x_AddAnnotType(CAnnotName(), sel, *idh);
        }
    }
}


void CSplitDataLoader::x_SplitAligns(CTSE_Chunk_Info& chunk,
                                     const CSeq_annot& annot)
{
    _ASSERT(annot.GetData().IsAlign());
    SAnnotTypeSelector sel(CSeq_annot::C_Data::e_Align);
    ITERATE(CSeq_annot::C_Data::TAlign, it, annot.GetData().GetAlign()) {
        const CSeq_align& align = **it;
        // Collect all referenced seq-ids
        set<CSeq_id_Handle> handles;
        CSeq_align::TDim dim = align.CheckNumRows();
        for (CSeq_align::TDim row = 0; row < dim; row++) {
            handles.insert(CSeq_id_Handle::GetHandle(align.GetSeq_id(row)));
        }
        // Register each referenced seq-id and annotation type
        ITERATE(set<CSeq_id_Handle>, idh, handles) {
            chunk.x_AddAnnotType(CAnnotName(), sel, *idh);
        }
    }
}


void CSplitDataLoader::x_SplitGraphs(CTSE_Chunk_Info& chunk,
                                     const CSeq_annot& annot)
{
    _ASSERT(annot.GetData().IsGraph());
    SAnnotTypeSelector sel(CSeq_annot::C_Data::e_Graph);
    ITERATE(CSeq_annot::C_Data::TGraph, it, annot.GetData().GetGraph()) {
        const CSeq_graph& graph = **it;
        // Collect all referenced seq-ids
        set<CSeq_id_Handle> handles;
        for (CSeq_loc_CI loc_it(graph.GetLoc()); loc_it; ++loc_it) {
            handles.insert(loc_it.GetSeq_id_Handle());
        }
        // Register each referenced seq-id and annotation type
        ITERATE(set<CSeq_id_Handle>, idh, handles) {
            chunk.x_AddAnnotType(CAnnotName(), sel, *idh);
        }
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE
