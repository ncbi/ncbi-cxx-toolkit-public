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
 * Author: Eugene Vasilchenko, Aleksey Grichenko
 *
 * File Description: Event loop for PSG data loader
 *
 * ===========================================================================
 */

#include <ncbi_pch.hpp>
#include <objtools/data_loaders/genbank/impl/psg_blob_processor.hpp>
#include <objtools/data_loaders/genbank/impl/psg_cache.hpp>
#include <objtools/data_loaders/genbank/impl/psg_cdd.hpp>
#include <objtools/data_loaders/genbank/impl/wgsmaster.hpp>
#include <objtools/data_loaders/genbank/impl/psg_loader_impl.hpp>
#include <objmgr/impl/data_source.hpp>
#include <objmgr/impl/tse_split_info.hpp>
#include <objmgr/impl/tse_chunk_info.hpp>
#include <objmgr/impl/tse_loadlock.hpp>
#include <objmgr/impl/split_parser.hpp>
#include <serial/objistr.hpp>
#include <serial/serial.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqsplit/ID2S_Split_Info.hpp>
#include <objects/seqsplit/ID2S_Chunk.hpp>

#if defined(HAVE_PSG_LOADER)

BEGIN_NCBI_NAMESPACE;
BEGIN_NAMESPACE(objects);
BEGIN_NAMESPACE(psgl);


/////////////////////////////////////////////////////////////////////////////
// CPSGL_Blob_Processor
/////////////////////////////////////////////////////////////////////////////


struct CPSGL_Blob_Processor::SBlobSlot
{
    // retrieved information
    EPSG_Status m_BlobInfoStatus;
    EPSG_Status m_BlobDataStatus;
    shared_ptr<CPSG_BlobInfo> m_BlobInfo;
    shared_ptr<CPSG_BlobData> m_BlobData;
    
    bool IsReadyToDeserialize() const;
    
    // parsed object
    CRef<CSerialObject> m_BlobObject;
};


struct CPSGL_Blob_Processor::STSESlot : public CPSGL_Blob_Processor::SBlobSlot
{
    explicit STSESlot(const string& psg_blob_id)
        : m_PSG_Blob_id(psg_blob_id)
    {
    }
    
    string m_PSG_Blob_id;

    // parsed blob info
    shared_ptr<SPsgBlobInfo> m_PsgBlobInfo;
        
    // for TSE - retrieved skipped information
    shared_ptr<CPSG_SkippedBlob> m_Skipped; // maybe taken from split-info
    unique_ptr<CDeadline> m_SkippedWaitDeadline;

    // cross references
    SSplitSlot* m_SplitSlot = nullptr; // from TSE and chunk to their split info

    // attach destination without locking
    CTSE_Chunk_Info* m_LockedDelayedChunkInfo = nullptr;
    
    // for TSE - OM TSE lock
    CConstRef<CPsgBlobId> m_DL_Blob_id;
    CTSE_Lock m_TSE_Lock;
};


struct CPSGL_Blob_Processor::SChunkSlot : public CPSGL_Blob_Processor::SBlobSlot
{
    explicit
    SChunkSlot(SSplitSlot* split_slot)
        : m_SplitSlot(split_slot)
    {
    }
    
    // cross references
    SSplitSlot* m_SplitSlot; // from TSE and chunk to their split info
        
    // attach destination without locking
    CTSE_Chunk_Info* m_LockedChunkInfo = nullptr;
};


struct CPSGL_Blob_Processor::SSplitSlot : public CPSGL_Blob_Processor::SBlobSlot
{
    explicit SSplitSlot(const string& id2_info)
        : m_Id2Info(id2_info)
    {
    }
        
    string m_Id2Info;
        
    // for TSE - retrieved skipped information
    shared_ptr<CPSG_SkippedBlob> m_Skipped;
    unique_ptr<CDeadline> m_SkippedWaitDeadline;

    // cross references
    STSESlot* m_TSESlot = nullptr; // from split info and chunk to their TSE

    // chunks
    typedef map<TChunkId, SChunkSlot> TChunkMap;
    TChunkMap m_ChunkMap; // by id2_chunk
};


CPSGL_Blob_Processor::CPSGL_Blob_Processor(CDataSource* data_source,
                                           CPSGBlobMap* blob_info_cache,
                                           bool add_wgs_master)
    : m_AddWGSMasterDescr(add_wgs_master),
      m_DataSource(data_source),
      m_BlobInfoCache(blob_info_cache)
{
}


CPSGL_Blob_Processor::~CPSGL_Blob_Processor()
{
}


inline
bool CPSGL_Blob_Processor::SBlobSlot::IsReadyToDeserialize() const
{
    return m_BlobInfo && m_BlobData; // got blob data and info
}


void CPSGL_Blob_Processor::SetDLBlobId(const string& psg_blob_id,
                                       const CConstRef<CPsgBlobId>& dl_blob_id)
{
    _ASSERT(dl_blob_id);
    CFastMutexGuard guard(m_BlobProcessorMutex);
    auto tse_slot = SetTSESlot(psg_blob_id);
    _ASSERT(!tse_slot->m_DL_Blob_id);
    tse_slot->m_DL_Blob_id = dl_blob_id;
}


CConstRef<CPsgBlobId> CPSGL_Blob_Processor::GetDLBlobId(STSESlot* tse_slot)
{
    if ( !tse_slot->m_DL_Blob_id ) {
        tse_slot->m_DL_Blob_id = CreateDLBlobId(tse_slot);
    }
    return tse_slot->m_DL_Blob_id;
}


CConstRef<CPsgBlobId> CPSGL_Blob_Processor::CreateDLBlobId(STSESlot* tse_slot)
{
    if ( auto split_slot = tse_slot->m_SplitSlot ) {
        return ConstRef(new CPsgBlobId(tse_slot->m_PSG_Blob_id, split_slot->m_Id2Info));
    }
    else {
        return ConstRef(new CPsgBlobId(tse_slot->m_PSG_Blob_id));
    }
}


void CPSGL_Blob_Processor::SetLockedDelayedChunkInfo(const string& psg_blob_id,
                                                     CTSE_Chunk_Info& locked_chunk_info)
{
    _ASSERT(locked_chunk_info.GetChunkId() == kDelayedMain_ChunkId);
    CFastMutexGuard guard(m_BlobProcessorMutex);
    auto tse_slot = SetTSESlot(psg_blob_id);
    tse_slot->m_LockedDelayedChunkInfo = &locked_chunk_info;
}


void CPSGL_Blob_Processor::SetLockedChunkInfo(CTSE_Chunk_Info& locked_chunk_info)
{
    _ASSERT(locked_chunk_info.GetChunkId() != kSplitInfoChunkId);
    _ASSERT(locked_chunk_info.GetChunkId() != kDelayedMain_ChunkId);
    auto dl_blob_id = locked_chunk_info.GetBlobId();
    const CPsgBlobId& blob_id = dynamic_cast<const CPsgBlobId&>(*dl_blob_id);
    CFastMutexGuard guard(m_BlobProcessorMutex);
    auto chunk_slot = SetChunkSlot(blob_id.GetId2Info(), locked_chunk_info.GetChunkId());
    static_cast<SChunkSlot*>(chunk_slot)->m_LockedChunkInfo = &locked_chunk_info;
}


CPSGL_Blob_Processor::STSESlot*
CPSGL_Blob_Processor::GetTSESlot(const string& blob_id)
{
    auto iter = m_TSEBlobMap.find(blob_id);
    if ( iter != m_TSEBlobMap.end() ) {
        return &iter->second;
    }
    return 0;
}


CPSGL_Blob_Processor::STSESlot*
CPSGL_Blob_Processor::SetTSESlot(const string& blob_id)
{
    auto [ iter, inserted ] = m_TSEBlobMap.try_emplace(blob_id, blob_id);
    if ( inserted ) {
        _TRACE(Descr()<<": TSE slot for blob_id="<<blob_id);
    }
    _ASSERT(iter->second.m_PSG_Blob_id == blob_id);
    return &iter->second;
}


CPSGL_Blob_Processor::SSplitSlot*
CPSGL_Blob_Processor::GetSplitSlot(const string& id2_info)
{
    auto iter = m_SplitBlobMap.find(id2_info);
    if ( iter != m_SplitBlobMap.end() ) {
        return &iter->second;
    }
    return 0;
}


CPSGL_Blob_Processor::SSplitSlot*
CPSGL_Blob_Processor::SetSplitSlot(const string& id2_info)
{
    auto [ iter, inserted ] = m_SplitBlobMap.try_emplace(id2_info, id2_info);
    if ( inserted ) {
        _TRACE(Descr()<<": Split slot for id2_info="<<id2_info);
    }
    _ASSERT(iter->second.m_Id2Info == id2_info);
    return &iter->second;
}


CPSGL_Blob_Processor::SBlobSlot*
CPSGL_Blob_Processor::GetChunkSlot(const string& id2_info,
                                   TChunkId chunk_id)
{
    if ( auto split_slot = GetSplitSlot(id2_info) ) {
        if ( chunk_id == kSplitInfoChunkId ) {
            return split_slot;
        }
        auto iter2 = split_slot->m_ChunkMap.find(chunk_id);
        if ( iter2 != split_slot->m_ChunkMap.end() ) {
            return &iter2->second;
        }
    }
    return 0;
}


CPSGL_Blob_Processor::SBlobSlot*
CPSGL_Blob_Processor::SetChunkSlot(const string& id2_info,
                                   TChunkId chunk_id)
{
    auto split_slot = SetSplitSlot(id2_info);
    if ( chunk_id == kSplitInfoChunkId ) {
        return split_slot;
    }
    auto [ iter, inserted ] = split_slot->m_ChunkMap.try_emplace(chunk_id, split_slot);
    if ( inserted ) {
        _TRACE(Descr()<<": Blob slot for id2_info="<<id2_info<<" chunk="<<chunk_id);
    }
    return &iter->second;
}


inline
CPSGL_Blob_Processor::STSESlot*
CPSGL_Blob_Processor::GetTSESlot(const CPSG_BlobId& blob_id)
{
    return GetTSESlot(blob_id.GetId());
}


inline
CPSGL_Blob_Processor::STSESlot*
CPSGL_Blob_Processor::SetTSESlot(const CPSG_BlobId& blob_id)
{
    return SetTSESlot(blob_id.GetId());
}


inline
CPSGL_Blob_Processor::SBlobSlot*
CPSGL_Blob_Processor::GetChunkSlot(const CPSG_ChunkId& chunk_id)
{
    return GetChunkSlot(chunk_id.GetId2Info(), chunk_id.GetId2Chunk());
}


inline
CPSGL_Blob_Processor::SBlobSlot*
CPSGL_Blob_Processor::SetChunkSlot(const CPSG_ChunkId& chunk_id)
{
    return SetChunkSlot(chunk_id.GetId2Info(), chunk_id.GetId2Chunk());
}


CPSGL_Blob_Processor::SBlobSlot*
CPSGL_Blob_Processor::GetBlobSlot(const CPSG_BlobId* blob_id,
                                  const CPSG_ChunkId* chunk_id)
{
    if ( blob_id ) {
        return GetTSESlot(*blob_id);
    }
    else if ( chunk_id ) {
        return GetChunkSlot(*chunk_id);
    }
    return 0;
}


CPSGL_Blob_Processor::SBlobSlot*
CPSGL_Blob_Processor::SetBlobSlot(const CPSG_BlobId* blob_id,
                                  const CPSG_ChunkId* chunk_id)
{
    if ( blob_id ) {
        return SetTSESlot(*blob_id);
    }
    else if ( chunk_id ) {
        return SetChunkSlot(*chunk_id);
    }
    return 0;
}


int CPSGL_Blob_Processor::GetBlobInfoState(const string& psg_blob_id)
{
    int state = 0;
    if ( auto tse_slot = GetTSESlot(psg_blob_id) ) {
        if ( tse_slot->m_PsgBlobInfo ) {
            state |= tse_slot->m_PsgBlobInfo->blob_state_flags;
        }
    }
    return state;
}


inline
tuple<const CPSG_BlobId*, const CPSG_ChunkId*> CPSGL_Blob_Processor::ParseId(const CPSG_DataId* id)
{
    auto blob_id = dynamic_cast<const CPSG_BlobId*>(id);
    auto chunk_id = blob_id? nullptr: dynamic_cast<const CPSG_ChunkId*>(id);
    return make_tuple(blob_id, chunk_id);
}


static
string ToString(const CPSG_BlobId* blob_id)
{
    return blob_id->GetId();
}


static
string ToString(const CPSG_ChunkId* chunk_id)
{
    return chunk_id->GetId2Info()+"."+NStr::NumericToString(chunk_id->GetId2Chunk());
}


CPSGL_Processor::EProcessResult
CPSGL_Blob_Processor::ProcessItemFast(EPSG_Status status,
                                      const shared_ptr<CPSG_ReplyItem>& item)
{
    CFastMutexGuard guard(m_BlobProcessorMutex);
    switch (item->GetType()) {
    case CPSG_ReplyItem::eBlobInfo:
        // can get data to become ready for deserialization
        // also main TSE may get split_info link and be ready for giving to OM
        // in both cases we return eToNextStage for post-processing
        if ( auto blob_info = dynamic_pointer_cast<CPSG_BlobInfo>(item) ) {
            auto [ blob_id, chunk_id ] = ParseId(blob_info->GetId());
            bool ready_to_OM = false; // TSE is ready for giving to OM from split info
            SBlobSlot* slot = nullptr;
            if ( blob_id ) {
                auto tse_slot = SetTSESlot(blob_id->GetId());
                // only main blobs are cached
                auto parsed_info = make_shared<SPsgBlobInfo>(*blob_info);
                if ( m_BlobInfoCache ) {
                    m_BlobInfoCache->Add(parsed_info->blob_id_main, parsed_info);
                }
                // main blob may have linked split info
                if ( !parsed_info->id2_info.empty() ) {
                    auto split_slot = SetSplitSlot(parsed_info->id2_info);
                    if ( !split_slot->m_TSESlot ) {
                        _TRACE(Descr()<<": link TSE "<<blob_id->GetId()<<" to split "<<parsed_info->id2_info);
                        split_slot->m_TSESlot = tse_slot;
                        tse_slot->m_SplitSlot = split_slot;
                        tse_slot->m_Skipped = std::move(split_slot->m_Skipped);
                        tse_slot->m_SkippedWaitDeadline = std::move(split_slot->m_SkippedWaitDeadline);
                    }
                    _ASSERT(split_slot->m_TSESlot == tse_slot);
                    _ASSERT(tse_slot->m_SplitSlot == split_slot);
                    // we need both DL blob id and split info
                    ready_to_OM = GetDLBlobId(tse_slot) && split_slot->m_BlobObject;
                }
                slot = tse_slot;
                tse_slot->m_PsgBlobInfo = parsed_info;
            }
            else {
                slot = SetChunkSlot(*chunk_id);
            }
            if ( !slot->m_BlobInfo && status == EPSG_Status::eSuccess ) {
                slot->m_BlobInfoStatus = status;
                slot->m_BlobInfo = blob_info;
            }
            if ( ready_to_OM || slot->IsReadyToDeserialize() ) {
                // need post-processing - parse the object and give TSE to OM
                return eToNextStage;
            }
        }
        break;
    case CPSG_ReplyItem::eBlobData:
        // can get data to become ready for deserialization
        if ( status == EPSG_Status::eForbidden ) {
            m_GotForbidden = true;
            while ( true ) {
                string msg = item->GetNextMessage();
                if ( msg.empty() ) {
                    break;
                }
                _TRACE(Descr()<<": got forbidden: " << msg);
            }
        }
        if ( auto data = dynamic_pointer_cast<CPSG_BlobData>(item) ) {
            auto [ blob_id, chunk_id ] = ParseId(data->GetId());
            if ( auto slot = SetBlobSlot(blob_id, chunk_id) ) {
                if ( !slot->m_BlobData && status == EPSG_Status::eSuccess ) {
                    slot->m_BlobDataStatus = status;
                    slot->m_BlobData = data;
                }
                if ( slot->IsReadyToDeserialize() ) {
                    // need post-processing - parse the object and assign to TSE
                    return eToNextStage;
                }
            }
        }
        break;
    case CPSG_ReplyItem::eSkippedBlob:
        // Only main blob can be skipped
        if ( auto skipped = dynamic_pointer_cast<CPSG_SkippedBlob>(item) ) {
            // only main or split-info blobs have special processing if skipped
            auto [ blob_id, chunk_id ] = ParseId(skipped->GetId());
            if ( blob_id ) {
                if ( auto tse_slot = SetTSESlot(*blob_id) ) {
                    tse_slot->m_Skipped = skipped;
                    tse_slot->m_SkippedWaitDeadline = GetWaitDeadline(*skipped);
                    if ( ObtainSkippedTSE_Lock(tse_slot) != eProcessed ) {
                        // need post-processing - get a TSE lock or assign chunks
                        return eToNextStage;
                    }
                }
            }
            else if ( chunk_id && chunk_id->GetId2Chunk() == kSplitInfoChunkId ) {
                if ( auto split_slot = SetSplitSlot(chunk_id->GetId2Info()) ) {
                    if ( auto tse_slot = split_slot->m_TSESlot ) {
                        tse_slot->m_Skipped = skipped;
                        tse_slot->m_SkippedWaitDeadline = GetWaitDeadline(*skipped);
                        if ( ObtainSkippedTSE_Lock(tse_slot) != eProcessed ) {
                            // need post-processing - get a TSE lock or assign chunks
                            return eToNextStage;
                        }
                    }
                    else {
                        // no link yet, save 'skipped' info in the split_slot
                        split_slot->m_Skipped = skipped;
                        split_slot->m_SkippedWaitDeadline = GetWaitDeadline(*skipped);
                    }
                }
            }
            else {
                break;
            }
            
        }
        break;
    case CPSG_ReplyItem::eProcessor:
        if ( auto processor = dynamic_pointer_cast<CPSG_Processor>(item) ) {
            if ( processor->GetProgressStatus() == CPSG_Processor::eUnauthorized ) {
                m_GotUnauthorized = true;
            }
        }
        break;
    default:
        break;
    }
    return eProcessed;
}


CPSGL_Processor::EProcessResult
CPSGL_Blob_Processor::ProcessItemSlow(EPSG_Status status,
                                      const shared_ptr<CPSG_ReplyItem>& item)
{
    switch (item->GetType()) {
    case CPSG_ReplyItem::eBlobInfo:
        if ( auto blob_info = dynamic_pointer_cast<CPSG_BlobInfo>(item) ) {
            return PostProcessBlob(blob_info->GetId());
        }
        break;
    case CPSG_ReplyItem::eBlobData:
        if ( auto data = dynamic_pointer_cast<CPSG_BlobData>(item) ) {
            return PostProcessBlob(data->GetId());
        }
        break;
    case CPSG_ReplyItem::eSkippedBlob:
        // Only main blob can be skipped
        if ( auto skipped = dynamic_pointer_cast<CPSG_SkippedBlob>(item) ) {
            return PostProcessSkippedBlob(skipped->GetId());
        }
        break;
    default:
        break;
    }
    return eProcessed;
}


CPSGL_Processor::EProcessResult
CPSGL_Blob_Processor::PostProcessBlob(const CPSG_DataId* id)
{
    // either deserialization or giving TSE/chunk to OM
    _ASSERT(id);
    _TRACE(Descr()<<": PostProcessBlob("<<id->Repr()<<")");
    auto [ blob_id, chunk_id ] = ParseId(id);
    SBlobSlot* slot;
    bool ready_object;
    bool ready_data;
    {{
        CFastMutexGuard guard(m_BlobProcessorMutex);
        slot = GetBlobSlot(blob_id, chunk_id);
        _ASSERT(slot);
        ready_object = slot->m_BlobObject;
        ready_data = !ready_object && slot->IsReadyToDeserialize();
    }}

    if ( blob_id ) {
        if ( ready_data && !ParseTSE(blob_id, slot) ) {
            return x_Failed("cannot parse blob "+ToString(blob_id));
        }
        return TSE_ToOM(blob_id, chunk_id, slot);
    }
    else if ( chunk_id->GetId2Chunk() == kSplitInfoChunkId ) {
        if ( ready_data && !ParseSplitInfo(chunk_id, slot) ) {
            return x_Failed("cannot parse split "+ToString(chunk_id));
        }
        return TSE_ToOM(blob_id, chunk_id, slot);
    }
    else {
        _ASSERT(ready_object || ready_data);
        if ( ready_data && !ParseChunk(chunk_id, slot) ) {
            return x_Failed("cannot parse chunk "+ToString(chunk_id));
        }
        return Chunk_ToOM(chunk_id, static_cast<SChunkSlot*>(slot));
    }
}


bool CPSGL_Blob_Processor::ParseTSE(const CPSG_BlobId* blob_id,
                                    SBlobSlot* data_slot)
{
    _ASSERT(blob_id);
    _ASSERT(data_slot);
    shared_ptr<CPSG_BlobInfo> blob_info;
    shared_ptr<CPSG_BlobData> blob_data;
    {{
        CFastMutexGuard guard(m_BlobProcessorMutex);
        if ( data_slot->m_BlobObject || !data_slot->m_BlobInfo || !data_slot->m_BlobData ) {
            return false;
        }
        blob_info = data_slot->m_BlobInfo;
        blob_data = data_slot->m_BlobData;
    }}
    _TRACE(Descr()<<": ParseTSE("<<blob_id->GetId()<<")");
    // full TSE entry
    unique_ptr<CObjectIStream> in(GetBlobDataStream(*blob_info, *blob_data));
    if ( !in.get() ) {
        LOG_POST("PSGBlobProcessor("<<this<<"): cannot open data stream for "<<
                 blob_id->GetId());
        return false;
    }
    CRef<CSeq_entry> object(new CSeq_entry);
    *in >> *object;
    in.reset();
    {{
        CFastMutexGuard guard(m_BlobProcessorMutex);
        data_slot->m_BlobObject = std::move(object);
        data_slot->m_BlobData.reset();
    }}
    return true;
}


bool CPSGL_Blob_Processor::ParseSplitInfo(const CPSG_ChunkId* split_info_id,
                                          SBlobSlot* data_slot)
{
    _ASSERT(split_info_id);
    _ASSERT(split_info_id->GetId2Chunk() == kSplitInfoChunkId);
    _ASSERT(data_slot);
    shared_ptr<CPSG_BlobInfo> blob_info;
    shared_ptr<CPSG_BlobData> blob_data;
    {{
        CFastMutexGuard guard(m_BlobProcessorMutex);
        if ( data_slot->m_BlobObject || !data_slot->m_BlobInfo || !data_slot->m_BlobData ) {
            return false;
        }
        blob_info = data_slot->m_BlobInfo;
        blob_data = data_slot->m_BlobData;
    }}
    _TRACE(Descr()<<": ParseSplitInfo("<<split_info_id->GetId2Info()<<")");
    unique_ptr<CObjectIStream> in(GetBlobDataStream(*blob_info, *blob_data));
    if ( !in.get() ) {
        LOG_POST("PSGBlobProcessor("<<this<<"): cannot open data stream for "<<
                 split_info_id->GetId2Info());
        return false;
    }
    CRef<CID2S_Split_Info> object(new CID2S_Split_Info);
    *in >> *object;
    in.reset();
    {{
        CFastMutexGuard guard(m_BlobProcessorMutex);
        data_slot->m_BlobObject = std::move(object);
        data_slot->m_BlobData.reset();
    }}
    return true;
}


bool CPSGL_Blob_Processor::ParseChunk(const CPSG_ChunkId* chunk_id,
                                      SBlobSlot* data_slot)
{
    _ASSERT(chunk_id);
    _ASSERT(chunk_id->GetId2Chunk() != kSplitInfoChunkId);
    _ASSERT(data_slot);
    shared_ptr<CPSG_BlobInfo> blob_info;
    shared_ptr<CPSG_BlobData> blob_data;
    {{
        CFastMutexGuard guard(m_BlobProcessorMutex);
        if ( data_slot->m_BlobObject || !data_slot->m_BlobInfo || !data_slot->m_BlobData ) {
            return false;
        }
        blob_info = data_slot->m_BlobInfo;
        blob_data = data_slot->m_BlobData;
    }}
    _TRACE(Descr()<<": ParseChunk("<<chunk_id->GetId2Info()<<"/"<<chunk_id->GetId2Chunk()<<")");
    unique_ptr<CObjectIStream> in(GetBlobDataStream(*blob_info, *blob_data));
    if ( !in.get() ) {
        LOG_POST("PSGBlobProcessor("<<this<<"): cannot open data stream for "<<
                 chunk_id->GetId2Info()<<"/"<<chunk_id->GetId2Chunk());
        return false;
    }
    CRef<CID2S_Chunk> object(new CID2S_Chunk);
    *in >> *object;
    in.reset();
    {{
        CFastMutexGuard guard(m_BlobProcessorMutex);
        data_slot->m_BlobObject = std::move(object);
        data_slot->m_BlobData.reset();
    }}
    return true;
}


CPSGL_Processor::EProcessResult
CPSGL_Blob_Processor::TSE_ToOM(const CPSG_BlobId* blob_id,
                               const CPSG_ChunkId* split_info_id,
                               SBlobSlot* data_slot)
{
    _ASSERT(blob_id || split_info_id);
    _ASSERT(data_slot);
    STSESlot* tse_slot;
    SSplitSlot* split_slot;
    CRef<CSeq_entry> entry;
    CRef<CID2S_Split_Info> split_info;
    CConstRef<CPsgBlobId> dl_blob_id;
    {{
        _TRACE(Descr()<<": LoadTSE("<<(blob_id? blob_id->GetId(): split_info_id->GetId2Info())<<")");
        CFastMutexGuard guard(m_BlobProcessorMutex);
        if ( blob_id ) {
            // the data_slot is TSE slot
            // but the data may come from its m_SplitSlot instead because
            // the link appeared after split info was parsed
            _ASSERT(!split_info_id);
            _ASSERT(data_slot->m_BlobInfo);
            tse_slot = static_cast<STSESlot*>(data_slot);
            // there may be a link to ID2 split info
            split_slot = tse_slot->m_SplitSlot;
        }
        else {
            _ASSERT(split_info_id);
            _ASSERT(split_info_id->GetId2Chunk() == kSplitInfoChunkId);
            // the data_slot is split_info slot
            split_slot = static_cast<SSplitSlot*>(data_slot);
            tse_slot = split_slot->m_TSESlot;
        }
        if ( !tse_slot ) {
            // no link to TSE yet
            return eProcessed;
        }
        _ASSERT(tse_slot);
        dl_blob_id = GetDLBlobId(tse_slot);
        if ( !dl_blob_id ) {
            // internal OM blob id is not known yet
            return eProcessed;
        }
        entry = dynamic_cast<CSeq_entry*>(tse_slot->m_BlobObject.GetPointerOrNull());
        if ( !entry && split_slot ) {
            _ASSERT(tse_slot->m_SplitSlot == split_slot);
            _ASSERT(split_slot->m_TSESlot == tse_slot);
            _ASSERT(tse_slot->m_BlobInfo->GetId2Info() == split_slot->m_Id2Info);
            split_info = dynamic_cast<CID2S_Split_Info*>(split_slot->m_BlobObject.GetPointerOrNull());
        }
    }}
    if ( !entry && !split_info  ) {
        // no data yet
        return eProcessed;
    }
    if ( CPSGDataLoader_Impl::GetGetBlobByIdShouldFail() ) {
        return x_Failed("GetBlobByIdShouldFail=true for: "+dl_blob_id->ToString());
    }
    if ( s_GetDebugLevel() >= 6 ) {
        LOG_POST(Info<<"PSGBlobProcessor("<<this<<"): getting TSE load lock: "<<dl_blob_id->ToString());
    }
    CTSE_LoadLock load_lock = m_DataSource->GetTSE_LoadLock(CBlobIdKey(dl_blob_id));
    if ( s_GetDebugLevel() >= 6 ) {
        LOG_POST(Info<<"PSGBlobProcessor("<<this<<"): got TSE load lock: "<<dl_blob_id->ToString());
    }
    if ( !load_lock.IsLoaded() ) {
        load_lock->SetBlobVersion(tse_slot->m_PsgBlobInfo->GetBlobVersion());
        UpdateOMBlobId(load_lock, dl_blob_id);
        auto blob_state = tse_slot->m_PsgBlobInfo->blob_state_flags;
        if ( dl_blob_id->HasBioseqIsDead() ) {
            // the 'dead' state was set from bioseq
            blob_state &= ~CBioseq_Handle::fState_dead;
        }
        load_lock->SetBlobState(blob_state);
    }
    // even if the TSE is marked as loaded it may have no main entry yet because
    // it has a specifal 'delayed main' chunk
    // we need to check that possibility too
    CTSE_Chunk_Info* delayed_main_chunk = nullptr;
    AutoPtr<CInitGuard> delayed_main_chunk_load_lock;
    if ( load_lock.IsLoaded() && load_lock->x_NeedsDelayedMainChunk() ) {
        // check if we need to load the delayed main chunk
        delayed_main_chunk = tse_slot->m_LockedDelayedChunkInfo;
        if ( !delayed_main_chunk ) {
            delayed_main_chunk = &load_lock->GetSplitInfo().GetChunk(kDelayedMain_ChunkId);
            if ( !delayed_main_chunk->IsLoaded() ) {
                // obtain the load lock for the delayed main chunk
                delayed_main_chunk_load_lock = delayed_main_chunk->GetLoadInitGuard();
                if ( !delayed_main_chunk_load_lock.get() || !*delayed_main_chunk_load_lock.get() ) {
                    // already loaded
                    delayed_main_chunk_load_lock.reset();
                    delayed_main_chunk = nullptr;
                }
            }
        }
        if ( delayed_main_chunk && delayed_main_chunk->IsLoaded() ) {
            // it's already loaded - no need to load it again
            delayed_main_chunk_load_lock.reset();
            delayed_main_chunk = nullptr;
        }
    }
    if ( !load_lock.IsLoaded() || delayed_main_chunk ) {
        if ( entry ) {
            _ASSERT(!split_info);
            if ( s_GetDebugLevel() >= 8 ) {
                LOG_POST(Info<<"PSGBlobProcessor("<<this<<"): TSE "<<dl_blob_id->ToString()<<" "<<
                         MSerial_AsnText<<*entry);
            }
            load_lock->SetSeq_entry(*entry);
        }
        else {
            _ASSERT(split_info);
            if ( s_GetDebugLevel() >= 8 ) {
                LOG_POST(Info<<"PSGBlobProcessor("<<this<<"): TSE "<<dl_blob_id->ToString()<<" "<<
                         MSerial_AsnText<<*split_info);
            }
            const CPsgBlobId& blob_id = dynamic_cast<const CPsgBlobId&>(*load_lock->GetBlobId());
            if ( blob_id.GetId2Info().empty() ) {
                const_cast<CPsgBlobId&>(blob_id).SetId2Info(split_slot->m_Id2Info);
            }
            _ASSERT(blob_id.GetId2Info() == split_slot->m_Id2Info);
            CSplitParser::Attach(*load_lock, *split_info);
        }
        
        if ( m_AddWGSMasterDescr ) {
            CWGSMasterSupport::AddWGSMaster(load_lock);
        }
        
        if ( delayed_main_chunk ) {
            if ( s_GetDebugLevel() >= 6 ) {
                LOG_POST(Info<<"PSGBlobProcessor("<<this<<"): "
                         "calling delayed_main_chunk->SetLoaded() for "<<dl_blob_id->ToString());
            }
            delayed_main_chunk->SetLoaded();
            if ( s_GetDebugLevel() >= 6 ) {
                LOG_POST(Info<<"PSGBlobProcessor("<<this<<"): "
                         "delayed TSE chunk loaded: "<<dl_blob_id->ToString());
            }
        }
        else {
            if ( s_GetDebugLevel() >= 6 ) {
                LOG_POST(Info<<"PSGBlobProcessor("<<this<<"): "
                         "calling SetLoaded() for "<<dl_blob_id->ToString());
            }
            load_lock.SetLoaded();
            if ( s_GetDebugLevel() >= 6 ) {
                LOG_POST(Info<<"PSGBlobProcessor("<<this<<"): "
                         "TSE loaded: "<<dl_blob_id->ToString());
            }
        }
    }
    {{
        CFastMutexGuard guard(m_BlobProcessorMutex);
        tse_slot->m_TSE_Lock = load_lock;
        _ASSERT(tse_slot->m_TSE_Lock);
        tse_slot->m_BlobObject.Reset();
        if ( split_slot ) {
            split_slot->m_BlobObject.Reset();
        }
    }}
    return AssignChunks(tse_slot);
}


CPSGL_Processor::EProcessResult
CPSGL_Blob_Processor::Chunk_ToOM(const CPSG_ChunkId* chunk_id,
                                 SChunkSlot* chunk_slot)
{
    _ASSERT(chunk_id && chunk_id->GetId2Chunk() != kSplitInfoChunkId);
    _ASSERT(chunk_slot);
    CRef<CSerialObject> chunk;
    {{
        CFastMutexGuard guard(m_BlobProcessorMutex);
        swap(chunk, chunk_slot->m_BlobObject);
        if ( !chunk ) {
            // the chunk was already attached by another thread
            return eProcessed;
        }
    }}
    _ASSERT(chunk);
    CTSE_Chunk_Info* chunk_info = chunk_slot->m_LockedChunkInfo;
    AutoPtr<CInitGuard> chunk_load_lock;
    if ( !chunk_info ) {
        CTSE_Lock tse_lock;
        {{
            // we need to lock CTSE_Chunk_Info and we need TSE for that
            // check if we have TSE ready for the chunk
            CFastMutexGuard guard(m_BlobProcessorMutex);
            if ( auto split_slot = chunk_slot->m_SplitSlot ) {
                if ( auto tse_slot = split_slot->m_TSESlot ) {
                    tse_lock = tse_slot->m_TSE_Lock;
                }
            }
            if ( !tse_lock ) {
                // no TSE to attach to, yet
                // return the object back into slot
                chunk_slot->m_BlobObject = chunk;
                // the chunk will be processed with other items or at the end of reply
                return eProcessed;
            }
        }}
        _ASSERT(tse_lock->HasSplitInfo());
        CTSE_Split_Info& split_info = const_cast<CTSE_Split_Info&>(tse_lock->GetSplitInfo());
        chunk_info = &split_info.GetChunk(chunk_id->GetId2Chunk());
        chunk_load_lock = chunk_info->GetLoadInitGuard();
    }
    if ( !chunk_info->IsLoaded() ) {
        CSplitParser::Load(*chunk_info, static_cast<CID2S_Chunk&>(*chunk));
        chunk_info->SetLoaded();
    }
    return eProcessed;
}


CPSGL_Processor::EProcessResult
CPSGL_Blob_Processor::AssignChunks(STSESlot* tse_slot)
{
    _ASSERT(tse_slot);
    _ASSERT(tse_slot->m_TSE_Lock);
    if ( !tse_slot->m_TSE_Lock->HasSplitInfo() ) {
        return eProcessed;
    }
    // collect ready chunks
    vector<pair<TChunkId, CRef<CID2S_Chunk>>> ready_chunks;
    {{
        CFastMutexGuard guard(m_BlobProcessorMutex);
        if ( !tse_slot->m_SplitSlot ) {
            return eProcessed;
        }
        if ( auto split_slot = tse_slot->m_SplitSlot ) {
            for ( auto& [ chunk_id, chunk_slot ] : split_slot->m_ChunkMap ) {
                if ( chunk_slot.m_BlobObject ) {
                    CRef<CID2S_Chunk> chunk(dynamic_cast<CID2S_Chunk*>(chunk_slot.m_BlobObject.GetNCPointerOrNull()));
                    _ASSERT(chunk);
                    ready_chunks.push_back(make_pair(chunk_id, chunk));
                    chunk_slot.m_BlobObject = null;
                }
            }
        }
    }}
    // assign ready chunks
    CTSE_Split_Info& split_info = const_cast<CTSE_Split_Info&>(tse_slot->m_TSE_Lock->GetSplitInfo());
    for ( auto [ id, chunk ] : ready_chunks ) {
        CTSE_Chunk_Info& chunk_info = split_info.GetChunk(id);
        AutoPtr<CInitGuard> chunk_load_lock = chunk_info.GetLoadInitGuard();
        if ( chunk_load_lock.get() && *chunk_load_lock.get() ) {
            CSplitParser::Load(chunk_info, *chunk);
            chunk_info.SetLoaded();
        }
    }
    return eProcessed;
}


CPSGL_Processor::EProcessResult
CPSGL_Blob_Processor::PostProcessSkippedBlob(const CPSG_DataId* id)
{
    auto [ blob_id, chunk_id ] = ParseId(id);
    {{
        CFastMutexGuard guard(m_BlobProcessorMutex);
        STSESlot* tse_slot = nullptr;
        if ( blob_id ) {
            tse_slot = GetTSESlot(*blob_id);
        }
        else {
            auto split_slot = GetSplitSlot(chunk_id->GetId2Info());
            tse_slot = split_slot->m_TSESlot;
        }
        _ASSERT(tse_slot);
        _ASSERT(tse_slot->m_Skipped);
        if ( ObtainSkippedTSE_Lock(tse_slot) != eToNextStage ) {
            // either no lock yet or no chunks to assign
            // nothing else to do for now
            // the rest will be finished in ProcessReply
            return eProcessed;
        }
    }}
    return eProcessed;
}


bool CPSGL_Blob_Processor::HasChunksToAssign(const CTSE_Lock& tse)
{
    if ( !tse->HasSplitInfo() ) {
        return false;
    }
    // there could be a split info for master WGS descriptors
    auto& psg_blob_id = dynamic_cast<const CPsgBlobId&>(*tse->GetBlobId());
    if ( psg_blob_id.GetId2Info().empty() ) {
        return false;
    }
    if ( auto split_slot = GetSplitSlot(psg_blob_id.GetId2Info()) ) {
        for ( auto& [ chunk_id, chunk_slot ] : split_slot->m_ChunkMap ) {
            (void)chunk_id; // alas, cannot skip unused fields in structured binding
            if ( chunk_slot.m_BlobObject ) {
                return true;
            }
        }
    }
    return false;
}


CPSGL_Processor::EProcessResult
CPSGL_Blob_Processor::ObtainSkippedTSE_Lock(STSESlot* tse_slot,
                                            EWaitForLock wait_for_lock)
{
    _ASSERT(tse_slot->m_Skipped);
    if ( !tse_slot->m_TSE_Lock ) {
        auto dl_blob_id = GetDLBlobId(tse_slot);
        if ( !dl_blob_id ) {
            return eFailed;
        }
        CTSE_LoadLock load_lock;
        if ( wait_for_lock && tse_slot->m_SkippedWaitDeadline ) {
            if ( s_GetDebugLevel() >= 6 ) {
                LOG_POST(Info<<"PSGBlobProcessor("<<this<<"): "
                         "getting loaded TSE lock: "<<dl_blob_id->ToString()<<
                         " wait: "<<tse_slot->m_SkippedWaitDeadline->GetRemainingTime().GetAsDouble());
            }
            load_lock = m_DataSource->GetLoadedTSE_Lock(CBlobIdKey(dl_blob_id),
                                                        *tse_slot->m_SkippedWaitDeadline);
        }
        else {
            if ( s_GetDebugLevel() >= 6 ) {
                LOG_POST(Info<<"PSGBlobProcessor("<<this<<"): "
                         "getting loaded TSE lock: "<<dl_blob_id->ToString());
            }
            load_lock = m_DataSource->GetTSE_LoadLockIfLoaded(CBlobIdKey(dl_blob_id));
        }
        if ( !load_lock ) {
            // no TSE lock yet
            if ( s_GetDebugLevel() >= 6 ) {
                LOG_POST(Info<<"PSGBlobProcessor("<<this<<"): "
                         "didn't get loaded TSE lock: "<<dl_blob_id->ToString());
            }
            return eFailed;
        }
        if ( s_GetDebugLevel() >= 6 ) {
            LOG_POST(Info<<"PSGBlobProcessor("<<this<<"): "
                     "got loaded TSE lock: "<<dl_blob_id->ToString());
        }
        _ASSERT(load_lock.IsLoaded());
        tse_slot->m_TSE_Lock = load_lock;
    }
    _ASSERT(tse_slot->m_TSE_Lock);
    // now we might need to assign chunks (including split info) to this tse
    // but only in postprocessing
    // check if there are completed chunks
    if ( HasChunksToAssign(tse_slot->m_TSE_Lock) ) {
        return eToNextStage;
    }
    // nothing else to do
    return eProcessed;
}


unique_ptr<CDeadline> CPSGL_Blob_Processor::GetWaitDeadline(const CPSG_SkippedBlob& skipped) const
{
    double timeout = 0;
    switch ( skipped.GetReason() ) {
    case CPSG_SkippedBlob::eInProgress:
        timeout = 1;
        break;
    case CPSG_SkippedBlob::eSent:
        if ( skipped.GetTimeUntilResend().IsNull() ) {
            timeout = 0.2;
        }
        else {
            timeout = skipped.GetTimeUntilResend().GetValue();
        }
        break;
    default:
        return nullptr;
    }
    return make_unique<CDeadline>(CTimeout(timeout));
}


const char* CPSGL_Blob_Processor::GetSkippedType(const CPSG_SkippedBlob& skipped)
{
    switch ( skipped.GetReason() ) {
    case CPSG_SkippedBlob::eInProgress:
        return "in progress";
    case CPSG_SkippedBlob::eSent:
        return "sent";
    case CPSG_SkippedBlob::eExcluded:
        return "excluded";
    default:
        return "unknown";
    }
}


CPSGL_Processor::EProcessResult
CPSGL_Blob_Processor::ProcessTSE_Lock(const string& blob_id,
                                      CTSE_Lock& tse_lock,
                                      EWaitForLock wait_for_lock)
{
    if ( blob_id.empty() ) {
        // inconsistent reply - no blob id
        return x_Failed("ProcessReply(): empty blob id");
    }
    if ( tse_lock ) {
        _TRACE(Descr()<<": ProcessReply(): TSE lock: "<<tse_lock.GetPointerOrNull());
        return eProcessed;
    }
    auto tse_slot = GetTSESlot(blob_id);
    if ( !tse_slot ) {
        // got blob id without TSE
        _TRACE(Descr()<<": ProcessReply(): processed w/o TSE");
        return eProcessed;
    }
    if ( !tse_slot->m_TSE_Lock && tse_slot->m_Skipped ) {
        // try to get TSE lock with waiting
        auto r = ObtainSkippedTSE_Lock(tse_slot, wait_for_lock);
        if ( r == eFailed ) {
            // no lock even after a wait
            _TRACE(Descr()<<": ProcessReply(): couldn't get TSE lock");
            return wait_for_lock? eProcessed: eToNextStage;
        }
        _ASSERT(tse_slot->m_TSE_Lock);
    }
    if ( tse_slot->m_TSE_Lock ) {
        AssignChunks(tse_slot);
        tse_lock = tse_slot->m_TSE_Lock;
    }
    _TRACE(Descr()<<": ProcessReply(): TSE lock: "<<tse_lock.GetPointerOrNull());
    return eProcessed;
}


pair<CTSE_Chunk_Info*, CRef<CID2S_Chunk>>
CPSGL_Blob_Processor::GetNextLoadedChunk()
{
    CTSE_Chunk_Info* chunk_info = nullptr;
    CRef<CID2S_Chunk> chunk_object;
    while ( !m_SplitBlobMap.empty() ) {
        SSplitSlot& split_slot = m_SplitBlobMap.begin()->second;
        while ( !split_slot.m_ChunkMap.empty() ) {
            SChunkSlot& chunk_slot = split_slot.m_ChunkMap.begin()->second;
            if ( chunk_slot.m_LockedChunkInfo && chunk_slot.m_BlobObject ) {
                chunk_info = chunk_slot.m_LockedChunkInfo;
                chunk_object = dynamic_cast<CID2S_Chunk*>(chunk_slot.m_BlobObject.GetPointerOrNull());
            }
            split_slot.m_ChunkMap.erase(split_slot.m_ChunkMap.begin());
            if ( chunk_info && chunk_object ) {
                return make_pair(chunk_info, chunk_object);
            }
        }
        m_SplitBlobMap.erase(m_SplitBlobMap.begin());
    }
    return make_pair(nullptr, null);
}


END_NAMESPACE(psgl);
END_NAMESPACE(objects);
END_NCBI_NAMESPACE;

#endif // HAVE_PSG_LOADER
