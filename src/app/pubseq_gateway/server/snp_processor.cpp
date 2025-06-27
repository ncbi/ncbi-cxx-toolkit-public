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
#include "id2info.hpp"
#include "insdc_utils.hpp"
#include "psgs_thread_pool.hpp"
#include <corelib/rwstream.hpp>
#include <serial/serial.hpp>
#include <serial/objostrasnb.hpp>
#include <util/compress/zlib.hpp>
#include <util/thread_pool.hpp>
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

static const string kParamMaxConn = "maxconn";
static const int kDefaultMaxConn = 64;


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


class CSNPThreadPoolTask_GetBlobByBlobId: public CThreadPool_Task
{
public:
    CSNPThreadPoolTask_GetBlobByBlobId(CPSGS_SNPProcessor& processor) : m_Processor(processor) {}

    virtual EStatus Execute(void) override
    {
        m_Processor.GetBlobByBlobId();
        return eCompleted;
    }

private:
    CPSGS_SNPProcessor& m_Processor;
};


class CSNPThreadPoolTask_GetChunk: public CThreadPool_Task
{
public:
    CSNPThreadPoolTask_GetChunk(CPSGS_SNPProcessor& processor) : m_Processor(processor) {}

    virtual EStatus Execute(void) override
    {
        m_Processor.GetChunk();
        return eCompleted;
    }

private:
    CPSGS_SNPProcessor& m_Processor;
};


class CSNPThreadPoolTask_GetAnnotation: public CThreadPool_Task
{
public:
    CSNPThreadPoolTask_GetAnnotation(CPSGS_SNPProcessor& processor) : m_Processor(processor) {}

    virtual EStatus Execute(void) override
    {
        m_Processor.GetAnnotation();
        return eCompleted;
    }

private:
    CPSGS_SNPProcessor& m_Processor;
};


END_LOCAL_NAMESPACE;


NCBI_PARAM_DECL(int, SNP_PROCESSOR, ERROR_RATE);
NCBI_PARAM_DEF(int, SNP_PROCESSOR, ERROR_RATE, 0);

static bool s_SimulateError()
{
    static int error_rate = NCBI_PARAM_TYPE(SNP_PROCESSOR, ERROR_RATE)::GetDefault();
    if ( error_rate > 0 ) {
        static int error_counter = 0;
        if ( ++error_counter >= error_rate ) {
            error_counter = 0;
            return true;
        }
    }
    return false;
}


/////////////////////////////////////////////////////////////////////////////
// CPSGS_SNPProcessor
/////////////////////////////////////////////////////////////////////////////


#define PARAM_GC_CACHE_SIZE "gc_cache_size"
#define PARAM_MISSING_GC_SIZE "missing_gc_size"
#define PARAM_FILE_REOPEN_TIME "file_reopen_time"
#define PARAM_FILE_RECHECK_TIME "file_recheck_time"
#define PARAM_FILE_OPEN_RETRY "file_open_retry"
#define PARAM_SPLIT "split"
#define PARAM_VDB_FILES "vdb_files"
#define PARAM_ANNOT_NAME "annot_name"
#define PARAM_ADD_PTIS "add_ptis"
#define PARAM_ALLOW_NON_REFSEQ "allow_non_refseq"
#define PARAM_SNP_SCALE_LIMIT "snp_scale_limit"

#define DEFAULT_GC_CACHE_SIZE 10
#define DEFAULT_MISSING_GC_SIZE 10000
#define DEFAULT_FILE_REOPEN_TIME 3600
#define DEFAULT_FILE_RECHECK_TIME 600
#define DEFAULT_FILE_OPEN_RETRY 3
#define DEFAULT_SPLIT true
#define DEFAULT_VDB_FILES ""
#define DEFAULT_ANNOT_NAME ""
#define DEFAULT_ADD_PTIS true
#define DEFAULT_ALLOW_NON_REFSEQ false
#define DEFAULT_SNP_SCALE_LIMIT ""


CPSGS_SNPProcessor::CPSGS_SNPProcessor(void)
    : m_Config(new SSNPProcessor_Config),
      m_Unlocked(true),
      m_PreResolving(false),
      m_ScaleLimit(CSeq_id::eSNPScaleLimit_Default)
{
    x_LoadConfig();
}


CPSGS_SNPProcessor::CPSGS_SNPProcessor(
    const CPSGS_SNPProcessor& parent,
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
      m_Config(parent.m_Config),
      m_Client(parent.m_Client),
      m_ThreadPool(parent.m_ThreadPool),
      m_Start(psg_clock_t::now()),
      m_Status(ePSGS_InProgress),
      m_Unlocked(true),
      m_PreResolving(false),
      m_ScaleLimit(CSeq_id::eSNPScaleLimit_Default)
{
    m_Request = request;
    m_Reply = reply;
    if ( request->GetRequestType() == CPSGS_Request::ePSGS_AnnotationRequest ) {
        m_ProcessNAs = m_Client->WhatNACanProcess(request->GetRequest<SPSGS_AnnotRequest>(), priority);
    }
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
    m_Config->m_FileReopenTime = registry.GetInt(kSNPProcessorSection, PARAM_FILE_REOPEN_TIME, DEFAULT_FILE_REOPEN_TIME);
    m_Config->m_FileRecheckTime = registry.GetInt(kSNPProcessorSection, PARAM_FILE_RECHECK_TIME, DEFAULT_FILE_RECHECK_TIME);
    m_Config->m_FileOpenRetry = registry.GetInt(kSNPProcessorSection, PARAM_FILE_OPEN_RETRY, DEFAULT_FILE_OPEN_RETRY);
    m_Config->m_Split = registry.GetBool(kSNPProcessorSection, PARAM_SPLIT, DEFAULT_SPLIT);
    m_Config->m_AnnotName = registry.GetString(kSNPProcessorSection, PARAM_ANNOT_NAME, DEFAULT_ANNOT_NAME);
    /*
    string vdb_files = registry.GetString(kSNPProcessorSection, PARAM_VDB_FILES, DEFAULT_VDB_FILES);
    NStr::Split(vdb_files, ",", m_Config->m_VDBFiles);
    */
    m_Config->m_AddPTIS = registry.GetBool(kSNPProcessorSection, PARAM_ADD_PTIS, DEFAULT_ADD_PTIS);
    if (m_Config->m_AddPTIS && !CSnpPtisClient::IsEnabled()) {
        PSG_ERROR("CSNPClient: SNP primary track is disabled due to lack of GRPC support");
        m_Config->m_AddPTIS = false;
    }
    m_Config->m_AllowNonRefSeq = registry.GetBool(kSNPProcessorSection, PARAM_ALLOW_NON_REFSEQ, DEFAULT_ALLOW_NON_REFSEQ);

    string scale_limit = registry.GetString(kSNPProcessorSection, PARAM_SNP_SCALE_LIMIT, DEFAULT_SNP_SCALE_LIMIT);
    m_Config->m_SNPScaleLimit = CSeq_id::GetSNPScaleLimit_Value(scale_limit);

    unsigned int max_conn = registry.GetInt(kSNPProcessorSection, kParamMaxConn, kDefaultMaxConn);
    if (max_conn == 0) {
        max_conn = kDefaultMaxConn;
    }
    m_ThreadPool.reset(new CThreadPool(kMax_UInt,
                                       new CPSGS_ThreadPool_Controller(
                                           min(3u, max_conn), max_conn)));
}


bool CPSGS_SNPProcessor::x_IsEnabled(CPSGS_Request& request) const
{
    auto app = CPubseqGatewayApp::GetInstance();
    bool enabled = app->Settings().m_SNPProcessorsEnabled;
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


vector<string> CPSGS_SNPProcessor::WhatCanProcess(shared_ptr<CPSGS_Request> request,
                                                  shared_ptr<CPSGS_Reply> reply) const
{
    try {
        if (!x_IsEnabled(*request)) return vector<string>();
        x_InitClient();
        _ASSERT(m_Client);
        return m_Client->WhatNACanProcess(request->GetRequest<SPSGS_AnnotRequest>());
    }
    catch ( exception& exc ) {
        x_SendError(reply, "Exception in WhatCanProcess: ", exc);
        throw;
    }
}


bool CPSGS_SNPProcessor::CanProcess(shared_ptr<CPSGS_Request> request,
                                    shared_ptr<CPSGS_Reply> reply) const
{
    try {
        if (!x_IsEnabled(*request)) return false;
        x_InitClient();
        _ASSERT(m_Client);
        if (!m_Client->CanProcessRequest(*request, 0)) return false;
        return true;
    }
    catch ( exception& exc ) {
        x_SendError(reply, "Exception in CanProcess: ", exc);
        throw;
    }
}


IPSGS_Processor* CPSGS_SNPProcessor::CreateProcessor(
    shared_ptr<CPSGS_Request> request,
    shared_ptr<CPSGS_Reply> reply,
    TProcessorPriority priority) const
{
    try {
        if (!x_IsEnabled(*request)) return nullptr;
        x_InitClient();
        _ASSERT(m_Client);
        if (!m_Client->CanProcessRequest(*request, m_Priority)) return nullptr;
        return new CPSGS_SNPProcessor(*this, request, reply, priority);
    }
    catch ( exception& exc ) {
        x_SendError(reply, "Exception in CreateProcessor: ", exc);
        throw;
    }
}


string CPSGS_SNPProcessor::GetName() const
{
    return kSNPProcessorName;
}


string CPSGS_SNPProcessor::GetGroupName() const
{
    return kSNPProcessorGroupName;
}


void CPSGS_SNPProcessor::x_SendError(shared_ptr<CPSGS_Reply> reply,
                                     const string& msg)
{
    reply->PrepareProcessorMessage(reply->GetItemId(), "SNP", msg,
                                   CRequestStatus::e500_InternalServerError,
                                   ePSGS_UnknownError,
                                   eDiag_Error);
}


void CPSGS_SNPProcessor::x_SendError(const string& msg)
{
    x_SendError(GetReply(), msg);
}


void CPSGS_SNPProcessor::x_SendError(shared_ptr<CPSGS_Reply> reply,
                                     const string& msg, const exception& exc)
{
    x_SendError(reply, msg+string(exc.what()));
}


void CPSGS_SNPProcessor::x_SendError(const string& msg, const exception& exc)
{
    x_SendError(GetReply(), msg+string(exc.what()));
}


void CPSGS_SNPProcessor::Process()
{
    CRequestContextResetter context_resetter;
    GetRequest()->SetRequestContext();

    try {
        {
            CFastMutexGuard guard(m_Mutex);
            m_Unlocked = false;
        }
        if (GetRequest()) GetRequest()->Lock(kSNPProcessorEvent);
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
    catch (exception& exc) {
        x_SendError("Exception when handling a request: ", exc);
        x_Finish(ePSGS_Error);
        return;
    }
}


static void s_OnGotAnnotation(void* data)
{
    static_cast<CPSGS_SNPProcessor*>(data)->OnGotAnnotation();
}


void CPSGS_SNPProcessor::x_ReportResultStatusForAllNA(SPSGS_AnnotRequest::EPSGS_ResultStatus status)
{
    SPSGS_AnnotRequest& annot_request = GetRequest()->GetRequest<SPSGS_AnnotRequest>();
    for ( auto& name : m_ProcessNAs ) {
        annot_request.ReportResultStatus(name, status);
    }
}


void CPSGS_SNPProcessor::x_ProcessAnnotationRequest(void)
{
    SPSGS_AnnotRequest& annot_request = GetRequest()->GetRequest<SPSGS_AnnotRequest>();
    if (!annot_request.m_SeqId.empty()) {
        try {
            m_SeqIds.push_back(CSeq_id_Handle::GetHandle(annot_request.m_SeqId));
        }
        catch (exception& e) {
            if ( GetRequest()->NeedTrace() ) {
                GetReply()->SendTrace(
                    kSNPProcessorName + " processor failed to parse seq-id " + annot_request.m_SeqId + ": " + e.what(),
                    GetRequest()->GetStartTimestamp());
            }
        }
    }
    for (auto& id : annot_request.m_SeqIds) {
        try {
            m_SeqIds.push_back(CSeq_id_Handle::GetHandle(id));
        }
        catch (exception& e) {
            if ( GetRequest()->NeedTrace() ) {
                GetReply()->SendTrace(
                    kSNPProcessorName + " processor failed to parse seq-id " + annot_request.m_SeqId + ": " + e.what(),
                    GetRequest()->GetStartTimestamp());
            }
        }
    }
    if (m_SeqIds.empty()) {
        if ( GetRequest()->NeedTrace() ) {
            GetReply()->SendTrace(
                kSNPProcessorName + " processor can not process any of the requested seq-ids",
                GetRequest()->GetStartTimestamp());
        }
        // no suitable Seq-ids in the request
        x_ReportResultStatusForAllNA(SPSGS_AnnotRequest::ePSGS_RS_NotFound);
        x_Finish(ePSGS_NotFound);
        return;
    }
    m_ScaleLimit = annot_request.m_SNPScaleLimit.value_or(CSeq_id::eSNPScaleLimit_Default);
    bool need_resolve = true;
    for (size_t i = 0; i < m_SeqIds.size(); ++i) {
        if (m_Client->IsValidSeqId(m_SeqIds[i])) {
            swap(m_SeqIds[0], m_SeqIds[i]);
            need_resolve = false;
            break;
        }
    }
    if (need_resolve) {
        if (annot_request.m_SeqIdResolve) {
            // No RefSeq id found in request, try to resolve it.
            if ( GetRequest()->NeedTrace() ) {
                GetReply()->SendTrace(
                    kSNPProcessorName + " processor is resolving the requested seq-id",
                    GetRequest()->GetStartTimestamp());
            }
            m_PreResolving = true;
            ResolveInputSeqId();
            return;
        }
        if (!m_Config->m_AllowNonRefSeq) {
            x_Finish(ePSGS_NotFound);
            return;
        }
    }
    m_ThreadPool->AddTask(new CSNPThreadPoolTask_GetAnnotation(*this));
}


void CPSGS_SNPProcessor::GetAnnotation(void)
{
    CRequestContextResetter context_resetter;
    GetRequest()->SetRequestContext();
    for ( auto& name : m_ProcessNAs ) {
        if (m_Canceled) {
            break;
        }
        try {
            for (auto& id : m_SeqIds) {
                if (!m_Client->IsValidSeqId(id)) continue;
                auto data = m_Client->GetAnnotInfo(id, name, m_ScaleLimit);
                m_SNPData.insert(m_SNPData.end(), data.begin(), data.end());
                if ( GetRequest()->NeedTrace() ) {
                    GetReply()->SendTrace(
                        kSNPProcessorName + " processor got annotation for " + id.AsString(),
                        GetRequest()->GetStartTimestamp());
                }
            }
        }
        catch (exception& exc) {
            SSNPData data;
            data.m_Name = name;
            data.m_Error = "Exception when handling get_na request: " + string(exc.what());
            m_SNPData.push_back(data);
        }
    }
    // Even if canceled, execute OnGotAnnotation to properly finish processing.
    PostponeInvoke(s_OnGotAnnotation, this);
}


void CPSGS_SNPProcessor::OnGotAnnotation(void)
{
    CRequestContextResetter context_resetter;
    GetRequest()->SetRequestContext();
    if (x_IsCanceled()) {
        return;
    }
    if ( s_SimulateError() ) {
        m_SNPDataError = "simulated SNP processor error";
        m_SNPData.clear();
    }
    for ( auto& s : m_SNPData ) {
        if ( s_SimulateError() ) {
            s.m_Error = "simulated SNP processor error";
        }
    }
    if (m_SNPData.empty()) {
        if ( m_SNPDataError.empty() ) {
            if ( GetRequest()->NeedTrace() ) {
                GetReply()->SendTrace(
                    kSNPProcessorName + " processor did not find the requested annotations",
                    GetRequest()->GetStartTimestamp());
            }
            x_RegisterTimingNotFound(eNAResolve);
            x_ReportResultStatusForAllNA(SPSGS_AnnotRequest::ePSGS_RS_NotFound);
            x_Finish(ePSGS_NotFound);
            return;
        }
        else {
            x_SendError(m_SNPDataError);
            x_ReportResultStatusForAllNA(SPSGS_AnnotRequest::ePSGS_RS_Error);
            x_Finish(ePSGS_Error);
            return;
        }
    }
    if (!x_SignalStartProcessing()) {
        return;
    }
    set<string> has_error;
    set<string> has_data;
    for (auto& data : m_SNPData) {
        if ( !data.m_Error.empty() ) {
            has_error.insert(data.m_Name);
            x_SendError(data.m_Error);
        }
        else {
            try {
                has_data.insert(data.m_Name);
                x_SendAnnotInfo(data);
            }
            catch (exception& exc) {
                has_error.insert(data.m_Name);
                m_SNPDataError = "Exception when handling a get_na request: " + string(exc.what());
                x_SendError(m_SNPDataError);
                x_ReportResultStatusForAllNA(SPSGS_AnnotRequest::ePSGS_RS_Error);
                x_Finish(ePSGS_Error);
                return;
            }
        }
    }
    SPSGS_AnnotRequest& annot_request = GetRequest()->GetRequest<SPSGS_AnnotRequest>();
    for ( auto& name : m_ProcessNAs ) {
        if ( has_error.count(name) ) {
            annot_request.ReportResultStatus(name, SPSGS_AnnotRequest::ePSGS_RS_Error);
        }
        else if ( !has_data.count(name) ) {
            annot_request.ReportResultStatus(name, SPSGS_AnnotRequest::ePSGS_RS_NotFound);
        }
    }
    x_Finish(!has_error.empty()? ePSGS_Error: (!has_data.empty()? ePSGS_Done: ePSGS_NotFound));
}


static void s_OnGotBlobByBlobId(void* data)
{
    static_cast<CPSGS_SNPProcessor*>(data)->OnGotBlobByBlobId();
}


void CPSGS_SNPProcessor::x_ProcessBlobBySatSatKeyRequest(void)
{
    SPSGS_BlobBySatSatKeyRequest& blob_request = GetRequest()->GetRequest<SPSGS_BlobBySatSatKeyRequest>();
    m_PSGBlobId = blob_request.m_BlobId.GetId();
    m_ThreadPool->AddTask(new CSNPThreadPoolTask_GetBlobByBlobId(*this));
}


void CPSGS_SNPProcessor::GetBlobByBlobId(void)
{
    CRequestContextResetter context_resetter;
    GetRequest()->SetRequestContext();
    try {
        if ( GetRequest()->NeedTrace() ) {
            GetReply()->SendTrace(
                kSNPProcessorName + " processor trying to get blob " + m_PSGBlobId,
                GetRequest()->GetStartTimestamp());
        }
        m_SNPData.push_back(m_Client->GetBlobByBlobId(m_PSGBlobId));
    }
    catch (...) {
        if ( GetRequest()->NeedTrace() ) {
            GetReply()->SendTrace(
                kSNPProcessorName + " processor failed to get blob " + m_PSGBlobId,
                GetRequest()->GetStartTimestamp());
        }
        m_SNPData.clear();
    }
    PostponeInvoke(s_OnGotBlobByBlobId, this);
}


void CPSGS_SNPProcessor::OnGotBlobByBlobId(void)
{
    CRequestContextResetter context_resetter;
    GetRequest()->SetRequestContext();
    if (x_IsCanceled()) {
        return;
    }
    if ( s_SimulateError() ) {
        m_SNPDataError = "simulated SNP processor error";
        m_SNPData.clear();
    }
    if (m_SNPData.empty()) {
        if ( m_SNPDataError.empty() ) {
            if ( GetRequest()->NeedTrace() ) {
                GetReply()->SendTrace(
                    kSNPProcessorName + " processor did not find the requested blob-id " + m_PSGBlobId,
                    GetRequest()->GetStartTimestamp());
            }
            x_RegisterTimingNotFound(eNARetrieve);
            x_Finish(ePSGS_NotFound);
            return;
        }
        else {
            x_SendError(m_SNPDataError);
            x_Finish(ePSGS_Error);
            return;
        }
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


static void s_OnGotChunk(void* data)
{
    static_cast<CPSGS_SNPProcessor*>(data)->OnGotChunk();
}


void CPSGS_SNPProcessor::x_ProcessTSEChunkRequest(void)
{
    SPSGS_TSEChunkRequest& chunk_request = GetRequest()->GetRequest<SPSGS_TSEChunkRequest>();
    m_Id2Info = chunk_request.m_Id2Info;
    m_ChunkId = chunk_request.m_Id2Chunk;
    m_ThreadPool->AddTask(new CSNPThreadPoolTask_GetChunk(*this));
}


void CPSGS_SNPProcessor::GetChunk(void)
{
    CRequestContextResetter context_resetter;
    GetRequest()->SetRequestContext();
    try {
        if ( GetRequest()->NeedTrace() ) {
            GetReply()->SendTrace(
                kSNPProcessorName + " processor trying to get chunk " + m_Id2Info + "." + NStr::NumericToString(m_ChunkId),
                GetRequest()->GetStartTimestamp());
        }
        SSNPData data = m_Client->GetChunk(m_Id2Info, m_ChunkId);
        m_SNPData.push_back(data);
    }
    catch (...) {
        m_SNPData.clear();
    }
    PostponeInvoke(s_OnGotChunk, this);
}


void CPSGS_SNPProcessor::OnGotChunk(void)
{
    CRequestContextResetter context_resetter;
    GetRequest()->SetRequestContext();
    if (x_IsCanceled()) {
        return;
    }
    if ( s_SimulateError() ) {
        m_SNPDataError = "simulated SNP processor error";
        m_SNPData.clear();
    }
    if (m_SNPData.empty()) {
        if ( m_SNPDataError.empty() ) {
            if ( GetRequest()->NeedTrace() ) {
                GetReply()->SendTrace(
                    kSNPProcessorName + " processor did not find chunk " + m_Id2Info + "." + NStr::NumericToString(m_ChunkId),
                    GetRequest()->GetStartTimestamp());
            }
            x_RegisterTimingNotFound(eTseChunkRetrieve);
            x_Finish(ePSGS_NotFound);
            return;
        }
        else {
            x_SendError(m_SNPDataError);
            x_Finish(ePSGS_Error);
            return;
        }
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


void CPSGS_SNPProcessor::x_RegisterTiming(psg_time_point_t start,
                                          EPSGOperation operation,
                                          EPSGOperationStatus status,
                                          size_t blob_size)
{
    CPubseqGatewayApp::GetInstance()->
        GetTiming().Register(this, operation, status, start, blob_size);
}


void CPSGS_SNPProcessor::x_RegisterTimingFound(psg_time_point_t start,
                                               EPSGOperation operation,
                                               const CID2_Reply_Data& data)
{
    size_t blob_size = 0;
    for ( auto& chunk : data.GetData() ) {
        blob_size += chunk->size();
    }
    x_RegisterTiming(start, operation, eOpStatusFound, blob_size);
}


void CPSGS_SNPProcessor::x_RegisterTimingNotFound(EPSGOperation operation)
{
    x_RegisterTiming(m_Start, operation, eOpStatusNotFound, 0);
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


void CPSGS_SNPProcessor::x_SendAnnotInfo(const SSNPData& data)
{
    SPSGS_AnnotRequest& annot_request = GetRequest()->GetRequest<SPSGS_AnnotRequest>();
    if ( annot_request.RegisterProcessedName(GetPriority(), data.m_Name) > GetPriority() ) {
        // higher priority processor already processed this request
        if ( GetRequest()->NeedTrace() ) {
            GetReply()->SendTrace(
                kSNPProcessorName + " processor stops processing request because a higher priority processor has already processed it",
                GetRequest()->GetStartTimestamp());
        }
        return;
    }
    x_RegisterTiming(data.m_Start, eNAResolve, eOpStatusFound, 0);
    CJsonNode json(CJsonNode::NewObjectNode());
    json.SetString("blob_id", data.m_BlobId);
    ostringstream annot_str;
    for (auto& it : data.m_AnnotInfo) {
        annot_str << MSerial_AsnBinary << *it;
    }
    json.SetString("seq_annot_info", NStr::Base64Encode(annot_str.str(), 0));
    GetReply()->PrepareNamedAnnotationData(data.m_Name, kSNPProcessorName, json.Repr(CJsonNode::fStandardJson));
}


void CPSGS_SNPProcessor::x_SendAnnotInfo(void)
{
    for (auto& data : m_SNPData) {
        x_SendAnnotInfo(data);
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
        x_RegisterTimingFound(data.m_Start, eTseChunkRetrieve, id2_data);
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
    x_RegisterTimingFound(data.m_Start, eNARetrieve, id2_data);
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
    x_RegisterTimingFound(data.m_Start, eNARetrieve, id2_data);

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
    CPSGS_CassProcessorBase::Cancel();
    m_Canceled = true;
    if (!IsUVThreadAssigned()) {
        m_Status = ePSGS_Canceled;
        x_Finish(ePSGS_Canceled);
    }
    else {
        x_UnlockRequest();
    }
}


IPSGS_Processor::EPSGS_Status CPSGS_SNPProcessor::GetStatus()
{
    return m_Status;
}


void CPSGS_SNPProcessor::x_UnlockRequest(void)
{
    {
        CFastMutexGuard guard(m_Mutex);
        if (m_Unlocked) return;
        m_Unlocked = true;
    }
    if (GetRequest()) GetRequest()->Unlock(kSNPProcessorEvent);
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
    if ( GetRequest()->GetRequestType() == CPSGS_Request::ePSGS_AnnotationRequest ) {
        // cannot register processed name before getting the data
    }
    else {
        if ( SignalStartProcessing() == ePSGS_Cancel ) {
            x_Finish(ePSGS_Canceled);
            return false;
        }
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
    m_PreResolving = false;
    if (x_IsCanceled()) {
        return;
    }

    if ( GetRequest()->NeedTrace() ) {
        GetReply()->SendTrace(
            kSNPProcessorName + " processor finished resolving the requested seq-id",
            GetRequest()->GetStartTimestamp());
    }
    try {
        auto& info = bioseq_resolution.GetBioseqInfo();
        if (m_Client->IsValidSeqId(info.GetAccession(), info.GetSeqIdType(), info.GetVersion())) {
            CSeq_id id(CSeq_id::E_Choice(info.GetSeqIdType()), info.GetAccession(), info.GetName(), info.GetVersion());
            m_SeqIds.push_back(CSeq_id_Handle::GetHandle(id));
        }
        CSeq_id id;
        for (auto& it : info.GetSeqIds()) {
            string err;
            if (!m_Client->IsValidSeqId(get<1>(it), get<0>(it))) continue;
            if (ParseInputSeqId(id, get<1>(it), get<0>(it), &err) != ePSGS_ParsedOK) {
                PSG_ERROR("Error parsing seq-id: " << (err.empty() ? get<1>(it) : err));
                continue;
            }
            m_SeqIds.push_back(CSeq_id_Handle::GetHandle(id));
        }

        m_ThreadPool->AddTask(new CSNPThreadPoolTask_GetAnnotation(*this));
    }
    catch (...) {
        x_Finish(ePSGS_Error);
    }
}


void CPSGS_SNPProcessor::x_OnSeqIdResolveError(
    CRequestStatus::ECode status,
    int code,
    EDiagSev severity,
    const string& message)
{
    CRequestContextResetter context_resetter;
    GetRequest()->SetRequestContext();
    m_PreResolving = false;
    if (x_IsCanceled()) {
        return;
    }
    if ( GetRequest()->NeedTrace() ) {
        GetReply()->SendTrace(
            kSNPProcessorName + " processor failed to resolve the requested seq-id",
            GetRequest()->GetStartTimestamp());
    }
    if ( status == CRequestStatus::e404_NotFound ) {
        x_ReportResultStatusForAllNA(SPSGS_AnnotRequest::ePSGS_RS_NotFound);
        x_Finish(ePSGS_NotFound);
    }
    else {
        x_ReportResultStatusForAllNA(SPSGS_AnnotRequest::ePSGS_RS_Error);
        x_Finish(ePSGS_Error);
    }
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
    if (!m_PreResolving) return;
    x_Peek(true);
}


void CPSGS_SNPProcessor::x_Peek(bool need_wait)
{
    if (x_IsCanceled()) {
        return;
    }

    // 1 -> call m_Loader->Wait1 to pick data
    // 2 -> check if we have ready-to-send buffers
    // 3 -> call reply->Send()  to send what we have if it is ready
    /* bool overall_final_state = false; */

    while (true) {
        auto initial_size = m_FetchDetails.size();

        for (auto& details : m_FetchDetails) {
            if (details) {
                if (details->InPeek()) {
                    continue;
                }
                details->SetInPeek(true);
                /* overall_final_state |= */ x_Peek(details, need_wait);
                details->SetInPeek(false);
            }
        }

        if (initial_size == m_FetchDetails.size()) {
            break;
        }
    }

    // Blob specific: ready packets need to be sent right away
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
        fetch_details->GetLoader()->ClearError();
        fetch_details->SetReadFinished();
        x_Finish(ePSGS_Error);
    }

    return final_state;
}


END_NAMESPACE(snp);
END_NAMESPACE(psg);
END_NCBI_NAMESPACE;
