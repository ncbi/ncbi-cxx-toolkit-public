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
 * Authors: Aleksey Grichenko, Eugene Vasilchenko
 *
 * File Description: processor for data from WGS
 *
 */

#include <ncbi_pch.hpp>

#include "wgs_processor.hpp"
#include "wgs_client.hpp"
#include "pubseq_gateway.hpp"
#include "pubseq_gateway_convert_utils.hpp"
#include "pubseq_gateway_logging.hpp"
#include "pubseq_gateway_convert_utils.hpp"
#include "osg_getblob_base.hpp"
#include "osg_resolve_base.hpp"
#include "id2info.hpp"
#include <sra/readers/sra/wgsread.hpp>
#include <sra/readers/sra/wgsresolver.hpp>
#include <corelib/rwstream.hpp>
#include <serial/serial.hpp>
#include <serial/objostrasnb.hpp>
#include <util/thread_nonstop.hpp>
#include <util/limited_size_map.hpp>
#include <util/compress/zlib.hpp>
#include <objects/general/general__.hpp>
#include <objects/seqfeat/SeqFeatData.hpp>
#include <objects/seqset/seqset__.hpp>
#include <objects/id2/ID2_Blob_Id.hpp>
#include <objects/id2/ID2_Blob_State.hpp>
#include <objects/id2/ID2_Reply_Data.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/bioseq_info/record.hpp>


BEGIN_NCBI_NAMESPACE;
BEGIN_NAMESPACE(psg);
BEGIN_NAMESPACE(wgs);

USING_SCOPE(objects);

static const string kWGSProcessorName = "WGS";
static const string kWGSProcessorGroupName = "WGS";
static const string kWGSProcessorSection = "WGS_PROCESSOR";


/////////////////////////////////////////////////////////////////////////////
// Helper classes
/////////////////////////////////////////////////////////////////////////////

BEGIN_LOCAL_NAMESPACE;

class COSSWriter : public IWriter
{
public:
    typedef vector<char> TOctetString;
    typedef list<TOctetString*> TOctetStringSequence;

    COSSWriter(TOctetStringSequence& out)
        : m_Output(out)
        {
        }
    
    virtual ERW_Result Write(const void* buffer,
                             size_t  count,
                             size_t* written)
        {
            const char* data = static_cast<const char*>(buffer);
            m_Output.push_back(new TOctetString(data, data+count));
            if ( written ) {
                *written = count;
            }
            return eRW_Success;
        }
    virtual ERW_Result Flush(void)
        {
            return eRW_Success;
        }

private:
    TOctetStringSequence& m_Output;
};


class CSemaphoreGuard
{
public:
    CSemaphoreGuard(CSemaphore& sema) : m_Sema(sema) {}
    ~CSemaphoreGuard(void) { m_Sema.Post(); }
private:
    CSemaphore& m_Sema;
};


END_LOCAL_NAMESPACE;


/////////////////////////////////////////////////////////////////////////////
// CWGSProcessorRef
/////////////////////////////////////////////////////////////////////////////

CWGSProcessorRef::CWGSProcessorRef(CPSGS_WGSProcessor* processor)
    : m_ProcessorPtr(processor)
{
}


CWGSProcessorRef::~CWGSProcessorRef()
{
}


void CWGSProcessorRef::Detach()
{
    CFastMutexGuard guard(m_ProcessorPtrMutex);
    if ( m_ProcessorPtr ) {
        m_ProcessorPtr = 0;
    }
}


void CWGSProcessorRef::ResolveSeqId(shared_ptr<CWGSProcessorRef> ref)
{
    CRequestContextResetter context_resetter;
    CSemaphoreGuard gsema(*CPSGS_WGSProcessor::sm_ThreadSema);
    shared_ptr<CWGSClient> client;
    CRef<CSeq_id> seq_id;
    {{
        CFastMutexGuard guard(ref->m_ProcessorPtrMutex);
        if ( !ref->m_ProcessorPtr ) {
            return;
        }
        ref->m_ProcessorPtr->GetRequest()->SetRequestContext();
        client = ref->m_ProcessorPtr->m_Client;
        seq_id = ref->m_ProcessorPtr->m_SeqId;
    }}
    shared_ptr<SWGSData> data = client->ResolveSeqId(*seq_id);
    {{
        CFastMutexGuard guard(ref->m_ProcessorPtrMutex);
        if ( !ref->m_ProcessorPtr ) {
            return;
        }
        ref->m_ProcessorPtr->m_WGSData = data;
        ref->m_ProcessorPtr->GetUvLoopBinder().PostponeInvoke(
            OnResolvedSeqId, new shared_ptr<CWGSProcessorRef>(ref));
    }}
}


void CWGSProcessorRef::OnResolvedSeqId(void *data)
{
    shared_ptr<CWGSProcessorRef>& ref = *static_cast<shared_ptr<CWGSProcessorRef>*>(data);
    {{
        CFastMutexGuard guard(ref->m_ProcessorPtrMutex);
        if ( ref->m_ProcessorPtr ) {
           ref->m_ProcessorPtr->OnResolvedSeqId();
        }
    }}
    delete &ref;
}


void CWGSProcessorRef::GetBlobBySeqId(shared_ptr<CWGSProcessorRef> ref)
{
    CRequestContextResetter context_resetter;
    CSemaphoreGuard gsema(*CPSGS_WGSProcessor::sm_ThreadSema);
    shared_ptr<CWGSClient> client;
    CRef<CSeq_id> seq_id;
    CWGSClient::TBlobIds excluded;
    {{
        CFastMutexGuard guard(ref->m_ProcessorPtrMutex);
        if ( !ref->m_ProcessorPtr ) {
            return;
        }
        ref->m_ProcessorPtr->GetRequest()->SetRequestContext();
        client = ref->m_ProcessorPtr->m_Client;
        seq_id = ref->m_ProcessorPtr->m_SeqId;
        excluded = ref->m_ProcessorPtr->m_ExcludedBlobs;
    }}

    shared_ptr<SWGSData> data = client->GetBlobBySeqId(*seq_id, excluded);

    {{
        CFastMutexGuard guard(ref->m_ProcessorPtrMutex);
        if ( !ref->m_ProcessorPtr ) {
            return;
        }
        ref->m_ProcessorPtr->m_WGSData = data;
        ref->m_ProcessorPtr->GetUvLoopBinder().PostponeInvoke(
            OnGotBlobBySeqId, new shared_ptr<CWGSProcessorRef>(ref));
    }}
}


void CWGSProcessorRef::OnGotBlobBySeqId(void *data)
{
    shared_ptr<CWGSProcessorRef>& ref = *static_cast<shared_ptr<CWGSProcessorRef>*>(data);
    {{
        CFastMutexGuard guard(ref->m_ProcessorPtrMutex);
        if ( ref->m_ProcessorPtr ) {
            ref->m_ProcessorPtr->OnGotBlobBySeqId();
        }
    }}
    delete &ref;
}


void CWGSProcessorRef::GetBlobByBlobId(shared_ptr<CWGSProcessorRef> ref)
{
    CRequestContextResetter context_resetter;
    CSemaphoreGuard gsema(*CPSGS_WGSProcessor::sm_ThreadSema);
    shared_ptr<CWGSClient> client;
    string blob_id;
    {{
        CFastMutexGuard guard(ref->m_ProcessorPtrMutex);
        if ( !ref->m_ProcessorPtr ) {
            return;
        }
        ref->m_ProcessorPtr->GetRequest()->SetRequestContext();
        client = ref->m_ProcessorPtr->m_Client;
        blob_id = ref->m_ProcessorPtr->m_PSGBlobId;
    }}

    shared_ptr<SWGSData> data = client->GetBlobByBlobId(blob_id);

    {{
        CFastMutexGuard guard(ref->m_ProcessorPtrMutex);
        if ( !ref->m_ProcessorPtr ) {
            return;
        }
        ref->m_ProcessorPtr->m_WGSData = data;
        ref->m_ProcessorPtr->GetUvLoopBinder().PostponeInvoke(
            OnGotBlobByBlobId, new shared_ptr<CWGSProcessorRef>(ref));
    }}
}


void CWGSProcessorRef::OnGotBlobByBlobId(void *data)
{
    shared_ptr<CWGSProcessorRef>& ref = *static_cast<shared_ptr<CWGSProcessorRef>*>(data);
    {{
        CFastMutexGuard guard(ref->m_ProcessorPtrMutex);
        if ( ref->m_ProcessorPtr ) {
            ref->m_ProcessorPtr->OnGotBlobByBlobId();
        }
    }}
    delete &ref;
}


void CWGSProcessorRef::GetChunk(shared_ptr<CWGSProcessorRef> ref)
{
    CRequestContextResetter context_resetter;
    CSemaphoreGuard gsema(*CPSGS_WGSProcessor::sm_ThreadSema);
    shared_ptr<CWGSClient> client;
    string id2info;
    int64_t chunk_id;
    {{
        CFastMutexGuard guard(ref->m_ProcessorPtrMutex);
        if ( !ref->m_ProcessorPtr ) {
            return;
        }
        ref->m_ProcessorPtr->GetRequest()->SetRequestContext();
        client = ref->m_ProcessorPtr->m_Client;
        id2info = ref->m_ProcessorPtr->m_Id2Info;
        chunk_id = ref->m_ProcessorPtr->m_ChunkId;
    }}

    shared_ptr<SWGSData> data = client->GetChunk(id2info, chunk_id);

    {{
        CFastMutexGuard guard(ref->m_ProcessorPtrMutex);
        if ( !ref->m_ProcessorPtr ) {
            return;
        }
        ref->m_ProcessorPtr->m_WGSData = data;
        ref->m_ProcessorPtr->GetUvLoopBinder().PostponeInvoke(
            OnGotChunk, new shared_ptr<CWGSProcessorRef>(ref));
    }}
}


void CWGSProcessorRef::OnGotChunk(void* data)
{
    shared_ptr<CWGSProcessorRef>& ref = *static_cast<shared_ptr<CWGSProcessorRef>*>(data);
    {{
        CFastMutexGuard guard(ref->m_ProcessorPtrMutex);
        if ( ref->m_ProcessorPtr ) {
            ref->m_ProcessorPtr->OnGotChunk();
        }
    }}
    delete &ref;
}


/////////////////////////////////////////////////////////////////////////////
// CPSGS_WGSProcessor
/////////////////////////////////////////////////////////////////////////////


unique_ptr<CSemaphore> CPSGS_WGSProcessor::sm_ThreadSema;

#define PARAM_VDB_CACHE_SIZE "vdb_cache_size"
#define PARAM_INDEX_UPDATE_TIME "index_update_time"
#define PARAM_COMPRESS_DATA "compress_data"
#define PARAM_WORKER_THREADS "worker_threads"

#define DEFAULT_VDB_CACHE_SIZE 100
#define DEFAULT_INDEX_UPDATE_TIME 600
#define DEFAULT_COMPRESS_DATA SWGSProcessor_Config::eCompressData_some
#define DEFAULT_WORKER_THREADS 5


CPSGS_WGSProcessor::CPSGS_WGSProcessor(void)
    : m_Config(new SWGSProcessor_Config),
      m_Status(ePSGS_NotFound),
      m_Canceled(false),
      m_ChunkId(0),
      m_OutputFormat(SPSGS_ResolveRequest::ePSGS_NativeFormat)
{
    x_LoadConfig();
    sm_ThreadSema.reset(new CSemaphore(m_Config->m_WorkerThreads, m_Config->m_WorkerThreads));
}


CPSGS_WGSProcessor::CPSGS_WGSProcessor(
    const shared_ptr<CWGSClient>& client,
    shared_ptr<CPSGS_Request> request,
    shared_ptr<CPSGS_Reply> reply,
    TProcessorPriority priority)
    : m_Client(client),
      m_Status(ePSGS_InProgress),
      m_Canceled(false),
      m_ChunkId(0),
      m_OutputFormat(SPSGS_ResolveRequest::ePSGS_NativeFormat)
{
    m_Request = request;
    m_Reply = reply;
    m_Priority = priority;
}


CPSGS_WGSProcessor::~CPSGS_WGSProcessor(void)
{
    _ASSERT(m_Status != ePSGS_InProgress);
    if ( m_ProcessorRef ) {
        m_ProcessorRef->Detach();
    }
}


void CPSGS_WGSProcessor::x_LoadConfig(void)
{
    const CNcbiRegistry& registry = CPubseqGatewayApp::GetInstance()->GetConfig();
    m_Config->m_CacheSize = registry.GetInt(kWGSProcessorSection, PARAM_VDB_CACHE_SIZE, DEFAULT_VDB_CACHE_SIZE);

    int compress_data = registry.GetInt(kWGSProcessorSection, PARAM_COMPRESS_DATA, DEFAULT_COMPRESS_DATA);
    if ( compress_data >= SWGSProcessor_Config::eCompressData_never &&
         compress_data <= SWGSProcessor_Config::eCompressData_always ) {
        m_Config->m_CompressData = SWGSProcessor_Config::ECompressData(compress_data);
    }
    m_Config->m_UpdateDelay = registry.GetInt(kWGSProcessorSection, PARAM_INDEX_UPDATE_TIME, DEFAULT_INDEX_UPDATE_TIME);
    m_Config->m_WorkerThreads = registry.GetInt(kWGSProcessorSection, PARAM_WORKER_THREADS, DEFAULT_WORKER_THREADS);
    if (m_Config->m_WorkerThreads <= 0) m_Config->m_WorkerThreads = DEFAULT_WORKER_THREADS;
}


IPSGS_Processor*
CPSGS_WGSProcessor::CreateProcessor(shared_ptr<CPSGS_Request> request,
                                    shared_ptr<CPSGS_Reply> reply,
                                    TProcessorPriority priority) const
{
    if ( !x_IsEnabled(*request) ) return nullptr;
    x_InitClient();
    _ASSERT(m_Client);
    if ( !m_Client->CanProcessRequest(*request) ) return nullptr;

    return new CPSGS_WGSProcessor(m_Client, request, reply, priority);
}


string CPSGS_WGSProcessor::GetName() const
{
    return kWGSProcessorName;
}


string CPSGS_WGSProcessor::GetGroupName() const
{
    return kWGSProcessorGroupName;
}


void CPSGS_WGSProcessor::Process()
{
    CRequestContextResetter context_resetter;
    GetRequest()->SetRequestContext();

    auto req_type = GetRequest()->GetRequestType();
    switch (req_type) {
    case CPSGS_Request::ePSGS_ResolveRequest:
        x_ProcessResolveRequest();
        break;
    case CPSGS_Request::ePSGS_BlobBySeqIdRequest:
        x_ProcessBlobBySeqIdRequest();
        break;
    case CPSGS_Request::ePSGS_BlobBySatSatKeyRequest:
        x_ProcessBlobBySatSatKeyRequest();
        break;
    case CPSGS_Request::ePSGS_TSEChunkRequest:
        x_ProcessTSEChunkRequest();
        break;
    default:
        x_Finish(ePSGS_Error);
        break;
    }
}


bool CPSGS_WGSProcessor::x_IsEnabled(CPSGS_Request& request) const
{
    auto app = CPubseqGatewayApp::GetInstance();
    bool enabled = app->GetWGSProcessorsEnabled();
    if ( enabled ) {
        for (const auto& name : request.GetRequest<SPSGS_RequestBase>().m_DisabledProcessors ) {
            if ( NStr::EqualNocase(name, kWGSProcessorName) ) return false;
        }
    }
    else {
        for (const auto& name : request.GetRequest<SPSGS_RequestBase>().m_EnabledProcessors ) {
            if ( NStr::EqualNocase(name, kWGSProcessorName) ) return true;
        }
    }
    return enabled;
}


inline
void CPSGS_WGSProcessor::x_InitClient(void) const
{
    DEFINE_STATIC_FAST_MUTEX(s_ClientMutex);
    if ( m_Client ) return;
    CFastMutexGuard guard(s_ClientMutex);
    if ( !m_Client ) {
        m_Client = make_shared<CWGSClient>(*m_Config);
    }
}


void CPSGS_WGSProcessor::x_ProcessResolveRequest(void)
{
    SPSGS_ResolveRequest& resolve_request = GetRequest()->GetRequest<SPSGS_ResolveRequest>();
    m_OutputFormat = resolve_request.m_OutputFormat;
    m_SeqId.Reset(new CSeq_id());
    if (ParseInputSeqId(*m_SeqId, resolve_request.m_SeqId, resolve_request.m_SeqIdType) != ePSGS_ParsedOK) {
        PSG_WARNING("Bad seq-id: " << resolve_request.m_SeqId);
        x_Finish(ePSGS_Error);
        return;
    }

    if ( !x_WaitForIdleThread() ) return;
    m_ProcessorRef = make_shared<CWGSProcessorRef>(this);
    thread(bind(&CWGSProcessorRef::ResolveSeqId, m_ProcessorRef)).detach();
}


void CPSGS_WGSProcessor::x_ProcessBlobBySeqIdRequest(void)
{
    SPSGS_BlobBySeqIdRequest& get_request = GetRequest()->GetRequest<SPSGS_BlobBySeqIdRequest>();
    m_SeqId.Reset(new CSeq_id());
    if (ParseInputSeqId(*m_SeqId, get_request.m_SeqId, get_request.m_SeqIdType) != ePSGS_ParsedOK) {
        PSG_WARNING("Bad seq-id: " << get_request.m_SeqId);
        x_Finish(ePSGS_Error);
        return;
    }

    if ( !x_WaitForIdleThread() ) return;
    m_ProcessorRef = make_shared<CWGSProcessorRef>(this);
    if (get_request.m_TSEOption == SPSGS_BlobRequestBase::ePSGS_NoneTSE) {
        thread(bind(&CWGSProcessorRef::ResolveSeqId, m_ProcessorRef)).detach();
    }
    else {
        m_ExcludedBlobs = get_request.m_ExcludeBlobs;
        if (!m_ExcludedBlobs.empty()) {
            string s;
            for (const auto& bid : m_ExcludedBlobs) {
                s += bid + ";";
            }
        }
        thread(bind(&CWGSProcessorRef::GetBlobBySeqId, m_ProcessorRef)).detach();
    }
}


void CPSGS_WGSProcessor::x_ProcessBlobBySatSatKeyRequest(void)
{
    SPSGS_BlobBySatSatKeyRequest& blob_request = GetRequest()->GetRequest<SPSGS_BlobBySatSatKeyRequest>();
    m_PSGBlobId = blob_request.m_BlobId.GetId();

    if ( !x_WaitForIdleThread() ) return;
    m_ProcessorRef = make_shared<CWGSProcessorRef>(this);
    thread(bind(&CWGSProcessorRef::GetBlobByBlobId, m_ProcessorRef)).detach();
}


void CPSGS_WGSProcessor::x_ProcessTSEChunkRequest(void)
{
    SPSGS_TSEChunkRequest& chunk_request = GetRequest()->GetRequest<SPSGS_TSEChunkRequest>();
    m_Id2Info = chunk_request.m_Id2Info;
    m_ChunkId = chunk_request.m_Id2Chunk;

    if ( !x_WaitForIdleThread() ) return;
    m_ProcessorRef = make_shared<CWGSProcessorRef>(this);
    thread(bind(&CWGSProcessorRef::GetChunk, m_ProcessorRef)).detach();
}


CPSGS_WGSProcessor::EOutputFormat CPSGS_WGSProcessor::x_GetOutputFormat(void)
{
    if ( m_OutputFormat == SPSGS_ResolveRequest::ePSGS_NativeFormat ||
        m_OutputFormat == SPSGS_ResolveRequest::ePSGS_UnknownFormat ) {
        return SPSGS_ResolveRequest::ePSGS_JsonFormat;
    }
    return m_OutputFormat;
}


void s_SetBlobVersion(CBlobRecord& blob_props, const CID2_Blob_Id& blob_id)
{
    if ( blob_id.IsSetVersion() ) {
        blob_props.SetModified(int64_t(blob_id.GetVersion())*60000);
    }
}


void s_SetBlobState(CBlobRecord& blob_props, int id2_blob_state)
{
    if ( id2_blob_state & (1 << eID2_Blob_State_withdrawn) ) {
        blob_props.SetWithdrawn(true);
    }
    if ( id2_blob_state & ((1 << eID2_Blob_State_suppressed) |
        (1 << eID2_Blob_State_suppressed_temp)) ) {
        blob_props.SetSuppress(true);
    }
    if ( id2_blob_state & (1 << eID2_Blob_State_dead) ) {
        blob_props.SetDead(true);
    }
}


void s_SetBlobDataProps(CBlobRecord& blob_props, const CID2_Reply_Data& data)
{
    if ( data.GetData_compression() == data.eData_compression_gzip ) {
        blob_props.SetGzip(true);
    }
    blob_props.SetNChunks(data.GetData().size());
}


void CPSGS_WGSProcessor::x_SendResult(const string& data_to_send, EOutputFormat output_format)
{
    size_t item_id = GetReply()->GetItemId();
    GetReply()->PrepareBioseqData(item_id, GetName(), data_to_send, output_format);
    GetReply()->PrepareBioseqCompletion(item_id, GetName(), 2);
}


void CPSGS_WGSProcessor::x_SendBioseqInfo(void)
{
    EOutputFormat output_format = x_GetOutputFormat();
    string data_to_send;
    if ( output_format == SPSGS_ResolveRequest::ePSGS_JsonFormat ) {
        data_to_send = ToJsonString(
            *m_WGSData->m_BioseqInfo,
            m_WGSData->m_BioseqInfoFlags,
            m_WGSData->m_BlobId);
    } else {
        data_to_send = ToBioseqProtobuf(*m_WGSData->m_BioseqInfo);
    }

    x_SendResult(data_to_send, output_format);
}


void CPSGS_WGSProcessor::x_SendBlobProps(const string& psg_blob_id, CBlobRecord& blob_props)
{
    auto& reply = *GetReply();
    size_t item_id = reply.GetItemId();
    string data_to_send = ToJsonString(blob_props);
    reply.PrepareBlobPropData(item_id, GetName(), psg_blob_id, data_to_send);
    reply.PrepareBlobPropCompletion(item_id, GetName(), 2);
}


void CPSGS_WGSProcessor::x_SendBlobData(const string& psg_blob_id, const CID2_Reply_Data& data)
{
    size_t item_id = GetReply()->GetItemId();
    int chunk_no = 0;
    for ( auto& chunk : data.GetData() ) {
        GetReply()->PrepareBlobData(item_id, GetName(), psg_blob_id,
            (const unsigned char*)chunk->data(), chunk->size(), chunk_no++);
    }
    GetReply()->PrepareBlobCompletion(item_id, GetName(), chunk_no + 1);
}


void CPSGS_WGSProcessor::x_SendChunkBlobProps(
    const string& id2_info,
    TID2ChunkId chunk_id,
    CBlobRecord& blob_props)
{
    size_t item_id = GetReply()->GetItemId();
    string data_to_send = ToJsonString(blob_props);
    GetReply()->PrepareTSEBlobPropData(item_id, GetName(), chunk_id, id2_info, data_to_send);
    GetReply()->PrepareBlobPropCompletion(item_id, GetName(), 2);
}


void CPSGS_WGSProcessor::x_SendChunkBlobData(
    const string& id2_info,
    TID2ChunkId chunk_id,
    const CID2_Reply_Data& data)
{
    size_t item_id = GetReply()->GetItemId();
    int chunk_no = 0;
    for ( auto& chunk : data.GetData() ) {
        GetReply()->PrepareTSEBlobData(item_id, GetName(),
            (const unsigned char*)chunk->data(), chunk->size(), chunk_no++,
            chunk_id, id2_info);
    }
    GetReply()->PrepareTSEBlobCompletion(item_id, GetName(), chunk_no+1);
}


void CPSGS_WGSProcessor::x_SendSplitInfo(void)
{
    if (!m_WGSData->m_Id2BlobId) return;
    CID2_Blob_Id& id2_blob_id = *m_WGSData->m_Id2BlobId;
    string blob_id = m_WGSData->m_BlobId;

    string id2_info = osg::CPSGS_OSGGetBlobBase::GetPSGId2Info(id2_blob_id, 0);
    
    CBlobRecord main_blob_props;
    s_SetBlobVersion(main_blob_props, id2_blob_id);
    s_SetBlobState(main_blob_props, m_WGSData->GetID2BlobState());
    main_blob_props.SetId2Info(id2_info);
    x_SendBlobProps(blob_id, main_blob_props);
    
    CBlobRecord split_info_blob_props;
    CID2_Reply_Data data;
    x_WriteData(data, *m_WGSData->m_Data, m_WGSData->m_Compress);
    s_SetBlobDataProps(split_info_blob_props, data);
    x_SendChunkBlobProps(id2_info, kSplitInfoChunk, split_info_blob_props);
    x_SendChunkBlobData(id2_info, kSplitInfoChunk, data);
}


void CPSGS_WGSProcessor::x_SendMainEntry(void)
{
    _ASSERT(m_WGSData->m_Id2BlobId);
    CID2_Blob_Id& id2_blob_id = *m_WGSData->m_Id2BlobId;
    string main_blob_id = m_WGSData->m_BlobId;

    CID2_Reply_Data data;
    x_WriteData(data, *m_WGSData->m_Data, m_WGSData->m_Compress);

    CBlobRecord main_blob_props;
    s_SetBlobVersion(main_blob_props, id2_blob_id);
    s_SetBlobState(main_blob_props, m_WGSData->GetID2BlobState());
    s_SetBlobDataProps(main_blob_props, data);
    x_SendBlobProps(main_blob_id, main_blob_props);
    x_SendBlobData(main_blob_id, data);
}


void CPSGS_WGSProcessor::x_SendExcluded(void)
{
    size_t item_id = GetReply()->GetItemId();
    GetReply()->PrepareBlobExcluded(item_id, GetName(), m_WGSData->m_BlobId, ePSGS_BlobExcluded);
}


void CPSGS_WGSProcessor::x_SendBlob(void)
{
    _ASSERT(m_WGSData);
   if ( find (m_ExcludedBlobs.begin(), m_ExcludedBlobs.end(), m_WGSData->m_BlobId) != m_ExcludedBlobs.end()) {
        x_SendExcluded();
        return;
    }
    
    _ASSERT(m_WGSData->m_Data);
    if ( m_WGSData->m_Data->GetMainObject().GetThisTypeInfo() == CID2S_Split_Info::GetTypeInfo() ) {
        // split info
        x_SendSplitInfo();
    }
    else {
        x_SendMainEntry();
    }
}


void CPSGS_WGSProcessor::x_SendChunk(void)
{
    _ASSERT(m_WGSData  &&  m_WGSData->m_Data);
    CID2_Blob_Id& id2_blob_id = *m_WGSData->m_Id2BlobId;
    string id2_info = osg::CPSGS_OSGGetBlobBase::GetPSGId2Info(id2_blob_id, 0);
    
    CID2_Reply_Data data;
    x_WriteData(data, *m_WGSData->m_Data, m_WGSData->m_Compress);

    CBlobRecord chunk_blob_props;
    s_SetBlobDataProps(chunk_blob_props, data);
    x_SendChunkBlobProps(id2_info, m_ChunkId, chunk_blob_props);
    x_SendChunkBlobData(id2_info, m_ChunkId, data);
}


void CPSGS_WGSProcessor::x_WriteData(CID2_Reply_Data& data,
                                     const CAsnBinData& obj,
                                     bool compress) const
{
    data.SetData_format(CID2_Reply_Data::eData_format_asn_binary);
    COSSWriter writer(data.SetData());
    CWStream writer_stream(&writer);
    AutoPtr<CNcbiOstream> str;
    if ( compress ) {
        data.SetData_compression(CID2_Reply_Data::eData_compression_gzip);
        str.reset(new CCompressionOStream(writer_stream,
                                            new CZipStreamCompressor(ICompression::eLevel_Lowest),
                                            CCompressionIStream::fOwnProcessor));
    }
    else {
        data.SetData_compression(CID2_Reply_Data::eData_compression_none);
        str.reset(&writer_stream, eNoOwnership);
    }
    CObjectOStreamAsnBinary objstr(*str);
    obj.Serialize(objstr);
}


void CPSGS_WGSProcessor::OnResolvedSeqId(void)
{
    CRequestContextResetter context_resetter;
    GetRequest()->SetRequestContext();
    if ( x_IsCanceled() ) {
        return;
    }
    if ( !m_WGSData  ||  !m_WGSData->m_BioseqInfo ) {
        x_Finish(ePSGS_NotFound);
        return;
    }
    if ( !x_SignalStartProcessing() ) {
        return;
    }
    x_SendBioseqInfo();
    x_Finish(ePSGS_Done);
}


void CPSGS_WGSProcessor::OnGotBlobBySeqId(void)
{
    CRequestContextResetter context_resetter;
    GetRequest()->SetRequestContext();
    if ( x_IsCanceled() ) {
        return;
    }
    // NOTE: m_Data may be null if the blob was excluded.
    if ( !m_WGSData  ||  !m_WGSData->m_BioseqInfo ) {
        x_Finish(ePSGS_NotFound);
        return;
    }
    if ( !x_SignalStartProcessing() ) {
        return;
    }
    x_SendBioseqInfo();
    x_SendBlob();
    x_Finish(ePSGS_Done);
}


void CPSGS_WGSProcessor::OnGotBlobByBlobId(void)
{
    CRequestContextResetter context_resetter;
    GetRequest()->SetRequestContext();
    if ( x_IsCanceled() ) {
        return;
    }
    if ( !m_WGSData  ||  !m_WGSData->m_Data ) {
        x_Finish(ePSGS_NotFound);
        return;
    }
    if ( !x_SignalStartProcessing() ) {
        return;
    }
    x_SendBlob();
    x_Finish(ePSGS_Done);
}


void CPSGS_WGSProcessor::OnGotChunk(void)
{
    CRequestContextResetter context_resetter;
    GetRequest()->SetRequestContext();
    if ( x_IsCanceled() ) {
        return;
    }
    if ( !m_WGSData  ||  !m_WGSData->m_Data ) {
        x_Finish(ePSGS_NotFound);
        return;
    }
    if ( !x_SignalStartProcessing() ) {
        return;
    }
    x_SendChunk();
    x_Finish(ePSGS_Done);
}


void CPSGS_WGSProcessor::Cancel()
{
    m_Canceled = true;
    x_Finish(ePSGS_Canceled);
    if ( m_ProcessorRef ) {
        m_ProcessorRef->Detach();
    }
}


IPSGS_Processor::EPSGS_Status CPSGS_WGSProcessor::GetStatus()
{
    return m_Status;
}


bool CPSGS_WGSProcessor::x_IsCanceled()
{
    if ( m_Canceled ) {
        x_Finish(ePSGS_Canceled);
        return true;
    }
    return false;
}


bool CPSGS_WGSProcessor::x_SignalStartProcessing()
{
    if ( SignalStartProcessing() == ePSGS_Cancel ) {
        x_Finish(ePSGS_Canceled);
        return false;
    }
    return true;
}


void CPSGS_WGSProcessor::x_Finish(EPSGS_Status status)
{
    _ASSERT(status != ePSGS_InProgress);
    m_Status = status;
    SignalFinishProcessing();
}


bool CPSGS_WGSProcessor::x_WaitForIdleThread(void)
{
    sm_ThreadSema->Wait();
    if ( x_IsCanceled() ) {
        sm_ThreadSema->Post();
        return false;
    }
    return true;
}


END_NAMESPACE(wgs);
END_NAMESPACE(psg);
END_NCBI_NAMESPACE;
