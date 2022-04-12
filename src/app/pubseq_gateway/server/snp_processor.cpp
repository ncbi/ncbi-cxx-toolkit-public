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
 * File Description: processor for data from SNP
 *
 */

#include <ncbi_pch.hpp>

#include "snp_processor.hpp"
#include "snp_client.hpp"
#include "pubseq_gateway.hpp"
#include "pubseq_gateway_convert_utils.hpp"
#include "pubseq_gateway_logging.hpp"
#include "pubseq_gateway_convert_utils.hpp"
#include "osg_getblob_base.hpp"
#include "osg_resolve_base.hpp"
#include "id2info.hpp"
#include "insdc_utils.hpp"
#include <corelib/rwstream.hpp>
#include <serial/serial.hpp>
#include <serial/objostrasnb.hpp>
#include <util/thread_nonstop.hpp>
#include <util/limited_size_map.hpp>
#include <util/compress/zlib.hpp>
#include <objects/seqfeat/SeqFeatData.hpp>
#include <objects/seqset/seqset__.hpp>
#include <objects/id2/ID2_Blob_Id.hpp>
#include <objects/id2/ID2_Blob_State.hpp>
#include <objects/id2/ID2_Reply_Data.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/bioseq_info/record.hpp>


BEGIN_NCBI_NAMESPACE;
BEGIN_NAMESPACE(psg);
BEGIN_NAMESPACE(snp);

USING_SCOPE(objects);

static const string kSNPProcessorName = "SNP";
static const string kSNPProcessorGroupName = "SNP";
static const string kSNPProcessorSection = "SNP_PROCESSOR";


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


void s_SetBlobDataProps(CBlobRecord& blob_props, const CID2_Reply_Data& data)
{
    if ( data.GetData_compression() == data.eData_compression_gzip ) {
        blob_props.SetGzip(true);
    }
    blob_props.SetNChunks(data.GetData().size());
}


END_LOCAL_NAMESPACE;


/////////////////////////////////////////////////////////////////////////////
// CPSGS_SNPProcessor
/////////////////////////////////////////////////////////////////////////////


#define PARAM_GC_CACHE_SIZE "gc_cache_size"
#define PARAM_MISSING_GC_SIZE "missing_gc_size"
#define PARAM_SPLIT "split"
#define PARAM_VDB_FILES "vdb_files"
#define PARAM_ANNOT_NAME "annot_name"
#define PARAM_ADD_PTIS "add_ptis"

#define DEFAULT_GC_CACHE_SIZE 10
#define DEFAULT_MISSING_GC_SIZE 10000
#define DEFAULT_SPLIT true
#define DEFAULT_VDB_FILES ""
#define DEFAULT_ANNOT_NAME ""
#define DEFAULT_ADD_PTIS true


CPSGS_SNPProcessor::CPSGS_SNPProcessor(void)
    : m_Config(new SSNPProcessor_Config),
      m_Unlocked(true)
{
    x_LoadConfig();
}


CPSGS_SNPProcessor::CPSGS_SNPProcessor(
    const shared_ptr<CSNPClient>& client,
    shared_ptr<CPSGS_Request> request,
    shared_ptr<CPSGS_Reply> reply,
    TProcessorPriority priority)
    : CPSGS_CassProcessorBase(request, reply, priority),
      CPSGS_ResolveBase(request, reply,
        bind(&CPSGS_SNPProcessor::x_OnSeqIdResolveFinished,
            this, placeholders::_1),
        bind(&CPSGS_SNPProcessor::x_OnSeqIdResolveError,
            this, placeholders::_1, placeholders::_2, placeholders::_3, placeholders::_4),
        bind(&CPSGS_SNPProcessor::x_OnResolutionGoodData, this)),
      m_Client(client),
      m_Status(ePSGS_InProgress),
      m_Unlocked(true)
{
}


CPSGS_SNPProcessor::~CPSGS_SNPProcessor(void)
{
    _ASSERT(m_Status != ePSGS_InProgress);
    x_UnlockRequest();
}


void CPSGS_SNPProcessor::x_LoadConfig(void)
{
    const CNcbiRegistry& registry = CPubseqGatewayApp::GetInstance()->GetConfig();
    m_Config->m_GCSize = registry.GetInt(kSNPProcessorSection, PARAM_GC_CACHE_SIZE, DEFAULT_GC_CACHE_SIZE);
    m_Config->m_MissingGCSize = registry.GetInt(kSNPProcessorSection, PARAM_MISSING_GC_SIZE, DEFAULT_MISSING_GC_SIZE);
    m_Config->m_Split = registry.GetBool(kSNPProcessorSection, PARAM_SPLIT, DEFAULT_SPLIT);
    m_Config->m_AnnotName = registry.GetString(kSNPProcessorSection, PARAM_ANNOT_NAME, DEFAULT_ANNOT_NAME);

    string vdb_files = registry.GetString(kSNPProcessorSection, PARAM_VDB_FILES, DEFAULT_VDB_FILES);
    NStr::Split(vdb_files, ",", m_Config->m_VDBFiles);

    m_Config->m_AddPTIS = registry.GetBool(kSNPProcessorSection, PARAM_ADD_PTIS, DEFAULT_ADD_PTIS);
    if (m_Config->m_AddPTIS && !CSnpPtisClient::IsEnabled()) {
        PSG_ERROR("CSNPClient: SNP primary track is disabled due to lack of GRPC support");
        m_Config->m_AddPTIS = false;
    }
}


bool CPSGS_SNPProcessor::x_IsEnabled(CPSGS_Request& request) const
{
    auto app = CPubseqGatewayApp::GetInstance();
    bool enabled = app->GetSNPProcessorsEnabled();
    if (enabled) {
        for (const auto& name : request.GetRequest<SPSGS_RequestBase>().m_DisabledProcessors) {
            if (NStr::EqualNocase(name, kSNPProcessorName)) return false;
        }
    }
    else {
        for (const auto& name : request.GetRequest<SPSGS_RequestBase>().m_EnabledProcessors) {
            if (NStr::EqualNocase(name, kSNPProcessorName)) return true;
        }
    }
    return enabled;
}


inline
void CPSGS_SNPProcessor::x_InitClient(void) const
{
    DEFINE_STATIC_FAST_MUTEX(s_InitMutex);
    if (m_Client) return;
    CFastMutexGuard guard(s_InitMutex);
    if (!m_Client) {
        m_Client = make_shared<CSNPClient>(*m_Config);
    }
}


IPSGS_Processor* CPSGS_SNPProcessor::CreateProcessor(
    shared_ptr<CPSGS_Request> request,
    shared_ptr<CPSGS_Reply> reply,
    TProcessorPriority priority) const
{
    if (!x_IsEnabled(*request)) return nullptr;
    x_InitClient();
    _ASSERT(m_Client);
    if (!m_Client->CanProcessRequest(*request, m_Priority)) return nullptr;

    return new CPSGS_SNPProcessor(m_Client, request, reply, priority);
}


string CPSGS_SNPProcessor::GetName() const
{
    return kSNPProcessorName;
}


string CPSGS_SNPProcessor::GetGroupName() const
{
    return kSNPProcessorGroupName;
}


void CPSGS_SNPProcessor::Process()
{
    CRequestContextResetter context_resetter;
    GetRequest()->SetRequestContext();

    try {
        m_Unlocked = false;
        if (m_Request) m_Request->Lock(kSNPProcessorEvent);
        auto req_type = GetRequest()->GetRequestType();
        switch (req_type) {
        case CPSGS_Request::ePSGS_AnnotationRequest:
            x_ProcessAnnotationRequest();
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
    catch (...) {
        x_Finish(ePSGS_Error);
        return;
    }
}


static void s_GetAnnotation(CPSGS_SNPProcessor* processor)
{
    processor->GetAnnotation();
}


static void s_OnGotAnnotation(void* data)
{
    static_cast<CPSGS_SNPProcessor*>(data)->OnGotAnnotation();
}


void CPSGS_SNPProcessor::x_ProcessAnnotationRequest(void)
{
    ResolveInputSeqId();
}


void CPSGS_SNPProcessor::GetAnnotation(void)
{
    CRequestContextResetter context_resetter;
    GetRequest()->SetRequestContext();
    try {
        SPSGS_AnnotRequest& annot_request = GetRequest()->GetRequest<SPSGS_AnnotRequest>();
        vector<string> names = annot_request.GetNotProcessedName(m_Priority);
        for (auto& id : m_SeqIds) {
            m_SNPData = m_Client->GetAnnotInfo(id, names);
            if (!m_SNPData.empty()) break;
        }
    }
    catch (...) {
        m_SNPData.clear();
    }
    GetUvLoopBinder().PostponeInvoke(s_OnGotAnnotation, this);
}


void CPSGS_SNPProcessor::OnGotAnnotation(void)
{
    CRequestContextResetter context_resetter;
    GetRequest()->SetRequestContext();
    if (x_IsCanceled()) {
        return;
    }
    if (m_SNPData.empty()) {
        x_Finish(ePSGS_NotFound);
        return;
    }
    if (!x_SignalStartProcessing()) {
        return;
    }
    try {
        x_SendAnnotInfo();
    }
    catch (...) {
        x_Finish(ePSGS_Error);
        return;
    }
    x_Finish(ePSGS_Done);
}


static void s_GetBlobByBlobId(CPSGS_SNPProcessor* processor)
{
    processor->GetBlobByBlobId();
}


static void s_OnGotBlobByBlobId(void* data)
{
    static_cast<CPSGS_SNPProcessor*>(data)->OnGotBlobByBlobId();
}


void CPSGS_SNPProcessor::x_ProcessBlobBySatSatKeyRequest(void)
{
    SPSGS_BlobBySatSatKeyRequest& blob_request = GetRequest()->GetRequest<SPSGS_BlobBySatSatKeyRequest>();
    m_PSGBlobId = blob_request.m_BlobId.GetId();
    thread(bind(&s_GetBlobByBlobId, this)).detach();
}


void CPSGS_SNPProcessor::GetBlobByBlobId(void)
{
    CRequestContextResetter context_resetter;
    GetRequest()->SetRequestContext();
    try {
        m_SNPData.push_back(m_Client->GetBlobByBlobId(m_PSGBlobId));
    }
    catch (...) {
        m_SNPData.clear();
    }
    GetUvLoopBinder().PostponeInvoke(s_OnGotBlobByBlobId, this);
}


void CPSGS_SNPProcessor::OnGotBlobByBlobId(void)
{
    CRequestContextResetter context_resetter;
    GetRequest()->SetRequestContext();
    if (x_IsCanceled()) {
        return;
    }
    if (m_SNPData.empty()) {
        x_Finish(ePSGS_NotFound);
        return;
    }
    if (!x_SignalStartProcessing()) {
        return;
    }
    try {
        x_SendBlob();
    }
    catch (...) {
        x_Finish(ePSGS_Error);
        return;
    }
    x_Finish(ePSGS_Done);
}


static void s_GetChunk(CPSGS_SNPProcessor* processor)
{
    processor->GetChunk();
}


static void s_OnGotChunk(void* data)
{
    static_cast<CPSGS_SNPProcessor*>(data)->OnGotChunk();
}


void CPSGS_SNPProcessor::x_ProcessTSEChunkRequest(void)
{
    SPSGS_TSEChunkRequest& chunk_request = GetRequest()->GetRequest<SPSGS_TSEChunkRequest>();
    m_Id2Info = chunk_request.m_Id2Info;
    m_ChunkId = chunk_request.m_Id2Chunk;
    thread(bind(&s_GetChunk, this)).detach();
}


void CPSGS_SNPProcessor::GetChunk(void)
{
    CRequestContextResetter context_resetter;
    GetRequest()->SetRequestContext();
    try {
        SSNPData data = m_Client->GetChunk(m_Id2Info, m_ChunkId);
        m_SNPData.push_back(data);
    }
    catch (...) {
        m_SNPData.clear();
    }
    GetUvLoopBinder().PostponeInvoke(s_OnGotChunk, this);
}


void CPSGS_SNPProcessor::OnGotChunk(void)
{
    CRequestContextResetter context_resetter;
    GetRequest()->SetRequestContext();
    if (x_IsCanceled()) {
        return;
    }
    if (m_SNPData.empty()) {
        x_Finish(ePSGS_NotFound);
        return;
    }
    if (!x_SignalStartProcessing()) {
        return;
    }
    try {
        x_SendChunk();
    }
    catch (...) {
        x_Finish(ePSGS_Error);
        return;
    }
    x_Finish(ePSGS_Done);
}


void CPSGS_SNPProcessor::x_WriteData(objects::CID2_Reply_Data& data, const CSerialObject& obj) const
{
    data.SetData_format(CID2_Reply_Data::eData_format_asn_binary);
    data.SetData_compression(CID2_Reply_Data::eData_compression_none);
    COSSWriter writer(data.SetData());
    CWStream writer_stream(&writer);
    CObjectOStreamAsnBinary objstr(writer_stream);
    objstr << obj;
}


void CPSGS_SNPProcessor::x_SendAnnotInfo(void)
{
    for (auto& data : m_SNPData) {
        CJsonNode json(CJsonNode::NewObjectNode());
        json.SetString("blob_id", data.m_BlobId);
        ostringstream annot_str;
        for (auto& it : data.m_AnnotInfo) {
            annot_str << MSerial_AsnBinary << *it;
        }
        json.SetString("seq_annot_info", NStr::Base64Encode(annot_str.str(), 0));
        GetReply()->PrepareNamedAnnotationData(data.m_Name, kSNPProcessorName, json.Repr(CJsonNode::fStandardJson));
        SPSGS_AnnotRequest& annot_request = GetRequest()->GetRequest<SPSGS_AnnotRequest>();
        annot_request.RegisterProcessedName(GetPriority(), data.m_Name);
    }
}


void CPSGS_SNPProcessor::x_SendBlob(void)
{
    for (auto& data : m_SNPData) {
        if (data.m_SplitInfo) x_SendSplitInfo(data);
        if (data.m_TSE) x_SendMainEntry(data);
    }
}


void CPSGS_SNPProcessor::x_SendChunk(void)
{
    for (auto& data : m_SNPData) {
        CID2_Reply_Data id2_data;
        x_WriteData(id2_data, *data.m_Chunk);
        CBlobRecord chunk_blob_props;
        s_SetBlobDataProps(chunk_blob_props, id2_data);
        x_SendChunkBlobProps(m_Id2Info, m_ChunkId, chunk_blob_props);
        x_SendChunkBlobData(m_Id2Info, m_ChunkId, id2_data);
    }

}


void CPSGS_SNPProcessor::x_SendSplitInfo(const SSNPData& data)
{
    const CID2S_Split_Info& info = *data.m_SplitInfo;
    string id2_info = m_PSGBlobId + ".0." + NStr::NumericToString(data.m_SplitVersion);
    
    CBlobRecord main_blob_props;
    main_blob_props.SetId2Info(id2_info);
    x_SendBlobProps(m_PSGBlobId, main_blob_props);
    
    CBlobRecord split_info_blob_props;
    CID2_Reply_Data id2_data;
    x_WriteData(id2_data, info);
    s_SetBlobDataProps(split_info_blob_props, id2_data);
    x_SendChunkBlobProps(id2_info, kSplitInfoChunk, split_info_blob_props);
    x_SendChunkBlobData(id2_info, kSplitInfoChunk, id2_data);
}


void CPSGS_SNPProcessor::x_SendMainEntry(const SSNPData& data)
{
    const CSeq_entry& entry = *data.m_TSE;
    string main_blob_id = m_PSGBlobId + ".0.0";
    CID2_Reply_Data id2_data;
    x_WriteData(id2_data, entry);

    CBlobRecord main_blob_props;
    s_SetBlobDataProps(main_blob_props, id2_data);
    x_SendBlobProps(main_blob_id, main_blob_props);
    x_SendBlobData(main_blob_id, id2_data);
}


void CPSGS_SNPProcessor::x_SendBlobProps(const string& psg_blob_id, CBlobRecord& blob_props)
{
    auto& reply = *GetReply();
    size_t item_id = reply.GetItemId();
    string data_to_send = ToJsonString(blob_props);
    reply.PrepareBlobPropData(item_id, GetName(), psg_blob_id, data_to_send);
    reply.PrepareBlobPropCompletion(item_id, GetName(), 2);
}


void CPSGS_SNPProcessor::x_SendBlobData(const string& psg_blob_id, const CID2_Reply_Data& data)
{
    size_t item_id = GetReply()->GetItemId();
    int chunk_no = 0;
    for ( auto& chunk : data.GetData() ) {
        GetReply()->PrepareBlobData(item_id, GetName(), psg_blob_id,
            (const unsigned char*)chunk->data(), chunk->size(), chunk_no++);
    }
    GetReply()->PrepareBlobCompletion(item_id, GetName(), chunk_no + 1);
}


void CPSGS_SNPProcessor::x_SendChunkBlobProps(const string& id2_info, int chunk_id, CBlobRecord& blob_props)
{
    size_t item_id = GetReply()->GetItemId();
    string data_to_send = ToJsonString(blob_props);
    GetReply()->PrepareTSEBlobPropData(item_id, GetName(), chunk_id, id2_info, data_to_send);
    GetReply()->PrepareBlobPropCompletion(item_id, GetName(), 2);
}


void CPSGS_SNPProcessor::x_SendChunkBlobData(const string& id2_info, int chunk_id, const CID2_Reply_Data& data)
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


void CPSGS_SNPProcessor::Cancel()
{
    m_Canceled = true;
    x_UnlockRequest();
}


IPSGS_Processor::EPSGS_Status CPSGS_SNPProcessor::GetStatus()
{
    return m_Status;
}


void CPSGS_SNPProcessor::x_UnlockRequest(void)
{
    if (m_Unlocked) return;
    m_Unlocked = true;
    if (m_Request) m_Request->Unlock(kSNPProcessorEvent);
}


bool CPSGS_SNPProcessor::x_IsCanceled()
{
    if ( m_Canceled ) {
        x_Finish(ePSGS_Canceled);
        return true;
    }
    return false;
}


bool CPSGS_SNPProcessor::x_SignalStartProcessing()
{
    if ( SignalStartProcessing() == ePSGS_Cancel ) {
        x_Finish(ePSGS_Canceled);
        return false;
    }
    return true;
}


void CPSGS_SNPProcessor::x_Finish(EPSGS_Status status)
{
    _ASSERT(status != ePSGS_InProgress);
    m_Status = status;
    x_UnlockRequest();
    SignalFinishProcessing();
}


void CPSGS_SNPProcessor::x_OnSeqIdResolveFinished(SBioseqResolution&& bioseq_resolution)
{
    CRequestContextResetter context_resetter;
    GetRequest()->SetRequestContext();
    if (x_IsCanceled()) {
        return;
    }

    auto& info = bioseq_resolution.GetBioseqInfo();
    CSeq_id id(CSeq_id::E_Choice(info.GetSeqIdType()),
        info.GetAccession(), info.GetName(), info.GetVersion());
    m_SeqIds.push_back(CSeq_id_Handle::GetHandle(id));
    for (auto& it : info.GetSeqIds()) {
        string err;
        if (ParseInputSeqId(id, get<1>(it), get<0>(it), &err) != ePSGS_ParsedOK) {
            PSG_ERROR("Error parsing seq-id: " << err);
            continue;
        }
        m_SeqIds.push_back(CSeq_id_Handle::GetHandle(id));
    }

    thread(bind(&s_GetAnnotation, this)).detach();
}


void CPSGS_SNPProcessor::x_OnSeqIdResolveError(
    CRequestStatus::ECode status,
    int code,
    EDiagSev severity,
    const string& message)
{
    CRequestContextResetter context_resetter;
    GetRequest()->SetRequestContext();
    if (x_IsCanceled()) {
        return;
    }
    x_Finish(status == CRequestStatus::e404_NotFound ? ePSGS_NotFound : ePSGS_Error);
}


void CPSGS_SNPProcessor::x_OnResolutionGoodData(void)
{
    CRequestContextResetter context_resetter;
    GetRequest()->SetRequestContext();
    if (x_IsCanceled()) {
        return;
    }
}


void CPSGS_SNPProcessor::ProcessEvent(void)
{
    x_Peek(true);
}


void CPSGS_SNPProcessor::x_Peek(bool  need_wait)
{
    if (x_IsCanceled()) {
        return;
    }
    if (m_InPeek) return;

    m_InPeek = true;

    // 1 -> call m_Loader->Wait1 to pick data
    // 2 -> check if we have ready-to-send buffers
    // 3 -> call reply->Send()  to send what we have if it is ready
    bool overall_final_state = false;

    while (true) {
        auto initial_size = m_FetchDetails.size();

        for (auto& details : m_FetchDetails) {
            if (details) {
                overall_final_state |= x_Peek(details, need_wait);
            }
        }

        if (initial_size == m_FetchDetails.size()) {
            break;
        }
    }

    // Blob specific: ready packets need to be sent right away
    if (GetReply()->IsOutputReady())
        GetReply()->Flush(CPSGS_Reply::ePSGS_SendAccumulated);

    // Blob specific: deal with exclude blob cache
    if (AreAllFinishedRead()) {
        for (auto& details : m_FetchDetails) {
            if (details) {
                // Update the cache records where needed
                details->SetExcludeBlobCacheCompleted();
            }
        }
    }

    m_InPeek = false;
}


bool CPSGS_SNPProcessor::x_Peek(unique_ptr<CCassFetch>& fetch_details, bool  need_wait)
{
    if (!fetch_details->GetLoader()) return true;

    bool final_state = false;
    if (need_wait)
        if (!fetch_details->ReadFinished()) {
            final_state = fetch_details->GetLoader()->Wait();
            if (final_state) fetch_details->SetReadFinished();
        }

    if (fetch_details->GetLoader()->HasError() &&
        GetReply()->IsOutputReady() && !GetReply()->IsFinished()) {
        // Send an error
        string error = fetch_details->GetLoader()->LastError();
        PSG_ERROR(error);

        // Mark finished
        UpdateOverallStatus(CRequestStatus::e500_InternalServerError);
        fetch_details->SetReadFinished();
        x_Finish(ePSGS_Error);
    }

    return final_state;
}


END_NAMESPACE(snp);
END_NAMESPACE(psg);
END_NCBI_NAMESPACE;
