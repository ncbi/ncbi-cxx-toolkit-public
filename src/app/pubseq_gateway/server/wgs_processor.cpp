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
#include "pubseq_gateway.hpp"
#include "pubseq_gateway_convert_utils.hpp"
#include "pubseq_gateway_logging.hpp"
#include "pubseq_gateway_convert_utils.hpp"
#include "id2info.hpp"
#include "cass_processor_base.hpp"
#include "psgs_thread_pool.hpp"
#include <sra/readers/sra/wgsread.hpp>
#include <sra/readers/sra/wgsresolver.hpp>
#include <corelib/rwstream.hpp>
#include <serial/serial.hpp>
#include <serial/objostrasnb.hpp>
#include <util/compress/zlib.hpp>
#include <util/thread_pool.hpp>
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


class CWGSThreadPoolTask_ResolveSeqId: public CThreadPool_Task
{
public:
    CWGSThreadPoolTask_ResolveSeqId(CPSGS_WGSProcessor& processor) : m_Processor(processor) {}

    virtual EStatus Execute(void) override
    {
        m_Processor.ResolveSeqId();
        return eCompleted;
    }

private:
    CPSGS_WGSProcessor& m_Processor;
};


class CWGSThreadPoolTask_GetBlobBySeqId: public CThreadPool_Task
{
public:
    CWGSThreadPoolTask_GetBlobBySeqId(CPSGS_WGSProcessor& processor) : m_Processor(processor) {}

    virtual EStatus Execute(void) override
    {
        m_Processor.GetBlobBySeqId();
        return eCompleted;
    }

private:
    CPSGS_WGSProcessor& m_Processor;
};


class CWGSThreadPoolTask_GetBlobByBlobId: public CThreadPool_Task
{
public:
    CWGSThreadPoolTask_GetBlobByBlobId(CPSGS_WGSProcessor& processor) : m_Processor(processor) {}

    virtual EStatus Execute(void) override
    {
        m_Processor.GetBlobByBlobId();
        return eCompleted;
    }

private:
    CPSGS_WGSProcessor& m_Processor;
};


class CWGSThreadPoolTask_GetChunk: public CThreadPool_Task
{
public:
    CWGSThreadPoolTask_GetChunk(CPSGS_WGSProcessor& processor) : m_Processor(processor) {}

    virtual EStatus Execute(void) override
    {
        m_Processor.GetChunk();
        return eCompleted;
    }

private:
    CPSGS_WGSProcessor& m_Processor;
};


END_LOCAL_NAMESPACE;


NCBI_PARAM_DECL(int, WGS_PROCESSOR, ERROR_RATE);
NCBI_PARAM_DEF(int, WGS_PROCESSOR, ERROR_RATE, 0);

static bool s_SimulateError()
{
    static int error_rate = NCBI_PARAM_TYPE(WGS_PROCESSOR, ERROR_RATE)::GetDefault();
    if ( error_rate > 0 ) {
        static int error_counter = 0;
        if ( ++error_counter >= error_rate ) {
            error_counter = 0;
            return true;
        }
    }
    return false;
}


static const char kSubSatSeparator = '/';

static void s_FormatBlobId(ostream& s, const CID2_Blob_Id& blob_id)
{
    s << blob_id.GetSat()
      << kSubSatSeparator << blob_id.GetSub_sat()
      << '.' << blob_id.GetSat_key();
}


/////////////////////////////////////////////////////////////////////////////
// CPSGS_WGSProcessor
/////////////////////////////////////////////////////////////////////////////


#define PARAM_VDB_CACHE_SIZE "vdb_cache_size"
#define PARAM_INDEX_UPDATE_TIME "index_update_time"
#define PARAM_FILE_REOPEN_TIME "file_reopen_time"
#define PARAM_FILE_RECHECK_TIME "file_recheck_time"
#define PARAM_COMPRESS_DATA "compress_data"

#define DEFAULT_VDB_CACHE_SIZE 100
#define DEFAULT_INDEX_UPDATE_TIME 600
#define DEFAULT_FILE_REOPEN_TIME 3600
#define DEFAULT_FILE_RECHECK_TIME 600
#define DEFAULT_COMPRESS_DATA SWGSProcessor_Config::eCompressData_some


CPSGS_WGSProcessor::CPSGS_WGSProcessor(void)
    : m_Config(new SWGSProcessor_Config),
      m_Status(ePSGS_NotFound),
      m_Canceled(false),
      m_ChunkId(0),
      m_OutputFormat(SPSGS_ResolveRequest::ePSGS_NativeFormat),
      m_Unlocked(true)
{
    x_LoadConfig();
}


CPSGS_WGSProcessor::CPSGS_WGSProcessor(
    const shared_ptr<CWGSClient>& client,
    shared_ptr<ncbi::CThreadPool> thread_pool,
    shared_ptr<CPSGS_Request> request,
    shared_ptr<CPSGS_Reply> reply,
    TProcessorPriority priority)
    : m_Client(client),
      m_Start(psg_clock_t::now()),
      m_Status(ePSGS_InProgress),
      m_Canceled(false),
      m_ChunkId(0),
      m_OutputFormat(SPSGS_ResolveRequest::ePSGS_NativeFormat),
      m_Unlocked(true),
      m_ThreadPool(thread_pool)
{
    m_Request = request;
    m_Reply = reply;
    m_Priority = priority;
}


CPSGS_WGSProcessor::~CPSGS_WGSProcessor(void)
{
    _ASSERT(m_Status != ePSGS_InProgress);
    x_UnlockRequest();
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
    m_Config->m_IndexUpdateDelay = registry.GetInt(kWGSProcessorSection, PARAM_INDEX_UPDATE_TIME, DEFAULT_INDEX_UPDATE_TIME);
    m_Config->m_FileReopenTime = registry.GetInt(kWGSProcessorSection, PARAM_FILE_REOPEN_TIME, DEFAULT_FILE_REOPEN_TIME);
    m_Config->m_FileRecheckTime = registry.GetInt(kWGSProcessorSection, PARAM_FILE_RECHECK_TIME, DEFAULT_FILE_RECHECK_TIME);

    unsigned int max_conn = registry.GetInt(kWGSProcessorSection, kParamMaxConn, kDefaultMaxConn);
    if (max_conn == 0) {
        max_conn = kDefaultMaxConn;
    }
    m_ThreadPool.reset(new CThreadPool(kMax_UInt,
                                       new CPSGS_ThreadPool_Controller(
                                           min(3u, max_conn), max_conn)));
}


bool CPSGS_WGSProcessor::CanProcess(shared_ptr<CPSGS_Request> request,
                                    shared_ptr<CPSGS_Reply> /*reply*/) const
{
    if ( !x_IsEnabled(*request) ) return false;
    x_InitClient();
    _ASSERT(m_Client);
    return m_Client->CanProcessRequest(*request);
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

    return new CPSGS_WGSProcessor(m_Client, m_ThreadPool, request, reply, priority);
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
    if ( x_IsCanceled() ) {
        return;
    }

    try {
        {
            CFastMutexGuard guard(m_Mutex);
            m_Unlocked = false;
        }
        if (m_Request) m_Request->Lock(kWGSProcessorEvent);
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
    catch (exception& exc) {
        x_SendError("Exception when handling a request: ", exc);
        x_Finish(ePSGS_Error);
    }
}


bool CPSGS_WGSProcessor::x_IsEnabled(CPSGS_Request& request) const
{
    auto app = CPubseqGatewayApp::GetInstance();
    bool enabled = app->Settings().m_WGSProcessorsEnabled;
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


static void s_OnResolvedSeqId(void* data)
{
    static_cast<CPSGS_WGSProcessor*>(data)->OnResolvedSeqId();
}


void CPSGS_WGSProcessor::x_ProcessResolveRequest(void)
{
    SPSGS_ResolveRequest& resolve_request = GetRequest()->GetRequest<SPSGS_ResolveRequest>();
    m_OutputFormat = resolve_request.m_OutputFormat;
    m_SeqId.Reset(new CSeq_id());
    string err;
    if (ParseInputSeqId(*m_SeqId, resolve_request.m_SeqId, resolve_request.m_SeqIdType, &err) != ePSGS_ParsedOK) {
        PSG_ERROR("Error parsing seq-id: " << (err.empty() ? resolve_request.m_SeqId : err));
        x_Finish(ePSGS_Error);
        return;
    }
    if ( GetRequest()->NeedTrace() ) {
        GetReply()->SendTrace(
            kWGSProcessorName + " processor is resolving seq-id " + m_SeqId->AsFastaString(),
            GetRequest()->GetStartTimestamp());
    }
    m_ThreadPool->AddTask(new CWGSThreadPoolTask_ResolveSeqId(*this));
}


void CPSGS_WGSProcessor::ResolveSeqId(void)
{
    CRequestContextResetter context_resetter;
    GetRequest()->SetRequestContext();
    try {
        m_WGSData = m_Client->ResolveSeqId(*m_SeqId);
        if ( GetRequest()->NeedTrace() ) {
            GetReply()->SendTrace(
                kWGSProcessorName + " processor finished resolving seq-id " + m_SeqId->AsFastaString() + ", waiting for other processors",
                GetRequest()->GetStartTimestamp());
        }
        x_WaitForOtherProcessors();
    }
    catch (exception& exc) {
        m_WGSDataError = "Exception when handling a request: "+string(exc.what());
        m_WGSData.reset();
    }
    PostponeInvoke(s_OnResolvedSeqId, this);
}


void CPSGS_WGSProcessor::OnResolvedSeqId(void)
{
    CRequestContextResetter context_resetter;
    GetRequest()->SetRequestContext();
    if ( x_IsCanceled() ) {
        return;
    }
    if ( s_SimulateError() ) {
        m_WGSDataError = "simulated WGS processor error";
        m_WGSData.reset();
    }
    if ( !m_WGSData  ||  !m_WGSData->m_BioseqInfo  ||  m_WGSData->m_GetResult == SWGSData::eResult_NotFound ) {
        if ( m_WGSDataError.empty() ) {
            if ( GetRequest()->NeedTrace() ) {
                GetReply()->SendTrace(
                    kWGSProcessorName + " processor could not find info for seq-id " + m_SeqId->AsFastaString(),
                    GetRequest()->GetStartTimestamp());
            }
            x_Finish(ePSGS_NotFound);
        }
        else {
            x_SendError(m_WGSDataError);
            x_Finish(ePSGS_Error);
        }
        return;
    }
    if ( !x_SignalStartProcessing() ) {
        return;
    }
    try {
        if ( GetRequest()->NeedTrace() ) {
            GetReply()->SendTrace(
                kWGSProcessorName + " processor resolved seq-id " + m_SeqId->AsFastaString() + " to blob-id " + m_WGSData->m_BlobId,
                GetRequest()->GetStartTimestamp());
        }
        x_SendBioseqInfo();
    }
    catch (exception& exc) {
        m_WGSDataError = "Exception when handling a request: "+string(exc.what());
        x_SendError(m_WGSDataError);
        x_Finish(ePSGS_Error);
        return;
    }
    x_Finish(ePSGS_Done);
}


static void s_OnGotBlobBySeqId(void* data)
{
    static_cast<CPSGS_WGSProcessor*>(data)->OnGotBlobBySeqId();
}


void CPSGS_WGSProcessor::x_ProcessBlobBySeqIdRequest(void)
{
    SPSGS_BlobBySeqIdRequest& get_request = GetRequest()->GetRequest<SPSGS_BlobBySeqIdRequest>();
    m_SeqId.Reset(new CSeq_id());
    m_ClientId = get_request.m_ClientId;
    string err;
    if (ParseInputSeqId(*m_SeqId, get_request.m_SeqId, get_request.m_SeqIdType, &err) != ePSGS_ParsedOK) {
        PSG_ERROR("Error parsing seq-id: " << (err.empty() ? get_request.m_SeqId : err));
        x_Finish(ePSGS_Error);
        return;
    }

    if (get_request.m_TSEOption == SPSGS_BlobRequestBase::ePSGS_NoneTSE) {
        if ( GetRequest()->NeedTrace() ) {
            GetReply()->SendTrace(
                kWGSProcessorName + " processor is getting info for seq-id " + m_SeqId->AsFastaString(),
                GetRequest()->GetStartTimestamp());
        }
        m_ThreadPool->AddTask(new CWGSThreadPoolTask_ResolveSeqId(*this));
    }
    else {
        if ( GetRequest()->NeedTrace() ) {
            GetReply()->SendTrace(
                kWGSProcessorName + " processor is getting blob for seq-id " + m_SeqId->AsFastaString(),
                GetRequest()->GetStartTimestamp());
        }
        m_ExcludedBlobs = get_request.m_ExcludeBlobs;
        m_ResendTimeoutMks = get_request.m_ResendTimeoutMks;
        m_ThreadPool->AddTask(new CWGSThreadPoolTask_GetBlobBySeqId(*this));
    }
}


void CPSGS_WGSProcessor::GetBlobBySeqId(void)
{
    CRequestContextResetter context_resetter;
    GetRequest()->SetRequestContext();
    try {
        CWGSClient::SWGSSeqInfo seq;
        m_WGSData = m_Client->GetSeqInfoBySeqId(*m_SeqId, seq, m_ExcludedBlobs);
        if ( m_WGSData ) {
            if (x_CheckExcludedCache()  &&  m_WGSData->m_GetResult == SWGSData::eResult_Found) {
                m_Client->GetWGSData(m_WGSData, seq);
            }
        }

        if ( GetRequest()->NeedTrace() ) {
            GetReply()->SendTrace(
                kWGSProcessorName + " processor finished getting blob for seq-id " + m_SeqId->AsFastaString() + ", waiting for other processors",
                GetRequest()->GetStartTimestamp());
        }
        x_WaitForOtherProcessors();
    }
    catch (exception& exc) {
        m_WGSDataError = "Exception when handling a request: "+string(exc.what());
        m_WGSData.reset();
    }
    PostponeInvoke(s_OnGotBlobBySeqId, this);
}


void CPSGS_WGSProcessor::OnGotBlobBySeqId(void)
{
    CRequestContextResetter context_resetter;
    GetRequest()->SetRequestContext();
    if ( x_IsCanceled() ) {
        return;
    }
    if ( s_SimulateError() ) {
        m_WGSDataError = "simulated WGS processor error";
        m_WGSData.reset();
    }
    // NOTE: m_Data may be null if the blob was excluded/skipped.
    if ( !m_WGSData  ||  !m_WGSData->m_BioseqInfo  ||  m_WGSData->m_GetResult == SWGSData::eResult_NotFound ) {
        if ( m_WGSDataError.empty() ) {
            if ( GetRequest()->NeedTrace() ) {
                GetReply()->SendTrace(
                    kWGSProcessorName + " processor could not find info for seq-id " + m_SeqId->AsFastaString(),
                    GetRequest()->GetStartTimestamp());
            }
            x_RegisterTimingNotFound(eBlobRetrieve);
            x_Finish(ePSGS_NotFound);
        }
        else {
            x_SendError(m_WGSDataError);
            x_Finish(ePSGS_Error);
        }
        return;
    }
    if ( !x_SignalStartProcessing() ) {
        return;
    }
    try {
        if ( GetRequest()->NeedTrace() ) {
            GetReply()->SendTrace(
                kWGSProcessorName + " processor resolved seq-id " + m_SeqId->AsFastaString() + " to blob-id " + m_WGSData->m_BlobId,
                GetRequest()->GetStartTimestamp());
        }
        x_SendBioseqInfo();
        x_SendBlob();
    }
    catch (exception& exc) {
        x_SendError("Exception when handling a request: ", exc);
        x_Finish(ePSGS_Error);
        return;
    }
    x_Finish(ePSGS_Done);
}


static void s_OnGotBlobByBlobId(void* data)
{
    static_cast<CPSGS_WGSProcessor*>(data)->OnGotBlobByBlobId();
}


void CPSGS_WGSProcessor::x_ProcessBlobBySatSatKeyRequest(void)
{
    SPSGS_BlobBySatSatKeyRequest& blob_request = GetRequest()->GetRequest<SPSGS_BlobBySatSatKeyRequest>();
    m_PSGBlobId = blob_request.m_BlobId.GetId();
    m_ClientId = blob_request.m_ClientId;
    if ( GetRequest()->NeedTrace() ) {
        GetReply()->SendTrace(
            kWGSProcessorName + " processor is fetching blob " + m_PSGBlobId,
            GetRequest()->GetStartTimestamp());
    }
    m_ThreadPool->AddTask(new CWGSThreadPoolTask_GetBlobByBlobId(*this));
}


void CPSGS_WGSProcessor::GetBlobByBlobId(void)
{
    CRequestContextResetter context_resetter;
    GetRequest()->SetRequestContext();
    try {
        m_WGSData = m_Client->GetBlobByBlobId(m_PSGBlobId);
        if ( GetRequest()->NeedTrace() ) {
            GetReply()->SendTrace(
                kWGSProcessorName + " processor finished fetching blob " + m_PSGBlobId,
                GetRequest()->GetStartTimestamp());
        }
    }
    catch (exception& exc) {
        m_WGSDataError = "Exception when handling a request: "+string(exc.what());
        m_WGSData.reset();
    }
    PostponeInvoke(s_OnGotBlobByBlobId, this);
}


void CPSGS_WGSProcessor::OnGotBlobByBlobId(void)
{
    CRequestContextResetter context_resetter;
    GetRequest()->SetRequestContext();
    if ( x_IsCanceled() ) {
        return;
    }
    if ( s_SimulateError() ) {
        m_WGSDataError = "simulated WGS processor error";
        m_WGSData.reset();
    }
    if ( !m_WGSData  ||  m_WGSData->m_GetResult == SWGSData::eResult_NotFound ) {
        if ( m_WGSDataError.empty() ) {
            if ( GetRequest()->NeedTrace() ) {
                GetReply()->SendTrace(
                    kWGSProcessorName + " processor could not find blob " + m_PSGBlobId,
                    GetRequest()->GetStartTimestamp());
            }
            x_RegisterTimingNotFound(eBlobRetrieve);
            x_Finish(ePSGS_NotFound);
        }
        else {
            x_SendError(m_WGSDataError);
            x_Finish(ePSGS_Error);
        }
        return;
    }
    if ( !x_SignalStartProcessing() ) {
        return;
    }
    try {
        if ( GetRequest()->NeedTrace() ) {
            GetReply()->SendTrace(
                kWGSProcessorName + " processor retrieved blob " + m_PSGBlobId,
                GetRequest()->GetStartTimestamp());
        }
        x_SendBlob();
    }
    catch (exception& exc) {
        x_SendError("Exception when handling a request: ", exc);
        x_Finish(ePSGS_Error);
        return;
    }
    x_Finish(ePSGS_Done);
}


static void s_OnGotChunk(void* data)
{
    static_cast<CPSGS_WGSProcessor*>(data)->OnGotChunk();
}


void CPSGS_WGSProcessor::x_ProcessTSEChunkRequest(void)
{
    SPSGS_TSEChunkRequest& chunk_request = GetRequest()->GetRequest<SPSGS_TSEChunkRequest>();
    m_Id2Info = chunk_request.m_Id2Info;
    m_ChunkId = chunk_request.m_Id2Chunk;
    if ( GetRequest()->NeedTrace() ) {
        GetReply()->SendTrace(
            kWGSProcessorName + " processor is fetching chunk " + m_Id2Info + "." + NStr::NumericToString(m_ChunkId),
            GetRequest()->GetStartTimestamp());
    }
    m_ThreadPool->AddTask(new CWGSThreadPoolTask_GetChunk(*this));
}


void CPSGS_WGSProcessor::GetChunk(void)
{
    CRequestContextResetter context_resetter;
    GetRequest()->SetRequestContext();
    try {
        m_WGSData = m_Client->GetChunk(m_Id2Info, m_ChunkId);
        if ( GetRequest()->NeedTrace() ) {
            GetReply()->SendTrace(
                kWGSProcessorName + " processor finished fetching chunk " + m_Id2Info + "." + NStr::NumericToString(m_ChunkId),
                GetRequest()->GetStartTimestamp());
        }
    }
    catch (exception& exc) {
        m_WGSDataError = "Exception when handling a request: "+string(exc.what());
        m_WGSData.reset();
    }
    PostponeInvoke(s_OnGotChunk, this);
}


void CPSGS_WGSProcessor::OnGotChunk(void)
{
    CRequestContextResetter context_resetter;
    GetRequest()->SetRequestContext();
    if ( x_IsCanceled() ) {
        return;
    }
    if ( s_SimulateError() ) {
        m_WGSDataError = "simulated WGS processor error";
        m_WGSData.reset();
    }
    if ( !m_WGSData  ||  m_WGSData->m_GetResult == SWGSData::eResult_NotFound ) {
        if ( m_WGSDataError.empty() ) {
            if ( GetRequest()->NeedTrace() ) {
                GetReply()->SendTrace(
                    kWGSProcessorName + " processor could not find chunk " + m_Id2Info + "." + NStr::NumericToString(m_ChunkId),
                    GetRequest()->GetStartTimestamp());
            }
            x_RegisterTimingNotFound(eTseChunkRetrieve);
            x_Finish(ePSGS_NotFound);
        }
        else {
            x_SendError(m_WGSDataError);
            x_Finish(ePSGS_Error);
        }
        return;
    }
    if ( !x_SignalStartProcessing() ) {
        return;
    }
    try {
        if ( m_WGSData->IsForbidden() ) {
            if ( GetRequest()->NeedTrace() ) {
                GetReply()->SendTrace(
                    kWGSProcessorName + " processor can not send forbidden chunk " + m_Id2Info + "." + NStr::NumericToString(m_ChunkId),
                    GetRequest()->GetStartTimestamp());
            }
            x_SendForbidden();
        }
        else {
            if ( GetRequest()->NeedTrace() ) {
                GetReply()->SendTrace(
                    kWGSProcessorName + " processor retrieved chunk " + m_Id2Info + "." + NStr::NumericToString(m_ChunkId),
                    GetRequest()->GetStartTimestamp());
            }
            x_SendChunk();
        }
    }
    catch (exception& exc) {
        x_SendError("Exception when handling a request: ", exc);
        x_Finish(ePSGS_Error);
        return;
    }
    x_Finish(ePSGS_Done);
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


void CPSGS_WGSProcessor::x_RegisterTiming(psg_time_point_t start,
                                          EPSGOperation operation,
                                          EPSGOperationStatus status,
                                          size_t blob_size)
{
    CPubseqGatewayApp::GetInstance()->
        GetTiming().Register(this, operation, status, start, blob_size);
}


void CPSGS_WGSProcessor::x_RegisterTimingFound(psg_time_point_t start,
                                               EPSGOperation operation,
                                               const CID2_Reply_Data& data)
{
    size_t blob_size = 0;
    for ( auto& chunk : data.GetData() ) {
        blob_size += chunk->size();
    }
    x_RegisterTiming(start, operation, eOpStatusFound, blob_size);
}


void CPSGS_WGSProcessor::x_RegisterTimingNotFound(EPSGOperation operation)
{
    x_RegisterTiming(m_Start, operation, eOpStatusNotFound, 0);
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


void CPSGS_WGSProcessor::x_SendBlobForbidden(const string& psg_blob_id)
{
    auto& reply = *GetReply();
    size_t item_id = reply.GetItemId();
    reply.PrepareBlobMessage(item_id, GetName(),
                             psg_blob_id,
                             "Blob retrieval is not authorized",
                             CRequestStatus::e403_Forbidden,
                             ePSGS_BlobRetrievalIsNotAuthorized,
                             eDiag_Error);
    reply.PrepareBlobCompletion(item_id, GetName(), 2);
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
    auto split_version = m_WGSData->m_SplitVersion;

    string id2_info = GetPSGId2Info(id2_blob_id, split_version);

    CBlobRecord main_blob_props;
    s_SetBlobVersion(main_blob_props, id2_blob_id);
    s_SetBlobState(main_blob_props, m_WGSData->GetID2BlobState());
    main_blob_props.SetId2Info(id2_info);
    x_SendBlobProps(blob_id, main_blob_props);
    
    CBlobRecord split_info_blob_props;
    CID2_Reply_Data data;
    x_WriteData(data, *m_WGSData->m_Data, m_WGSData->m_Compress);
    x_RegisterTimingFound(m_WGSData->m_Start, eBlobRetrieve, data);
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
    x_RegisterTimingFound(m_WGSData->m_Start, eBlobRetrieve, data);

    CBlobRecord main_blob_props;
    s_SetBlobVersion(main_blob_props, id2_blob_id);
    s_SetBlobState(main_blob_props, m_WGSData->GetID2BlobState());
    s_SetBlobDataProps(main_blob_props, data);
    x_SendBlobProps(main_blob_id, main_blob_props);
    x_SendBlobData(main_blob_id, data);
}


void CPSGS_WGSProcessor::x_SendForbidden(void)
{
    CID2_Blob_Id& id2_blob_id = *m_WGSData->m_Id2BlobId;
    const string& psg_blob_id = m_WGSData->m_BlobId;

    CBlobRecord blob_props;
    s_SetBlobVersion(blob_props, id2_blob_id);
    s_SetBlobState(blob_props, m_WGSData->GetID2BlobState());
    x_SendBlobProps(psg_blob_id, blob_props);
    x_SendBlobForbidden(psg_blob_id);
}


void CPSGS_WGSProcessor::x_SendBlob(void)
{
    if ( m_WGSData->IsForbidden() ) {
        x_SendForbidden();
        return;
    }
    switch (m_WGSData->m_GetResult) {
    case SWGSData::eResult_Excluded:
        GetReply()->PrepareBlobExcluded(m_WGSData->m_BlobId, GetName(), ePSGS_BlobExcluded);
        return;
    case SWGSData::eResult_InProgress:
        GetReply()->PrepareBlobExcluded(m_WGSData->m_BlobId, GetName(), ePSGS_BlobInProgress);
        return;
    case SWGSData::eResult_Sent:
        GetReply()->PrepareBlobExcluded(m_WGSData->m_BlobId, GetName(),
            m_SentMksAgo, m_ResendTimeoutMks - m_SentMksAgo);
        return;
    default:
        break;
    }
    if ( m_WGSData->m_Data->GetMainObject().GetThisTypeInfo() == CID2S_Split_Info::GetTypeInfo() ) {
        // split info
        x_SendSplitInfo();
    }
    else {
        x_SendMainEntry();
    }
    x_SetExcludedCacheCompleted();
}


void CPSGS_WGSProcessor::x_SendChunk(void)
{
    _ASSERT(m_WGSData  &&  m_WGSData->m_Data);
    CID2_Blob_Id& id2_blob_id = *m_WGSData->m_Id2BlobId;
    auto split_version = m_WGSData->m_SplitVersion;
    string id2_info = GetPSGId2Info(id2_blob_id, split_version);

    CID2_Reply_Data data;
    x_WriteData(data, *m_WGSData->m_Data, m_WGSData->m_Compress);
    x_RegisterTimingFound(m_WGSData->m_Start, eTseChunkRetrieve, data);

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


void CPSGS_WGSProcessor::x_UnlockRequest(void)
{
    {
        CFastMutexGuard guard(m_Mutex);
        if (m_Unlocked) return;
        m_Unlocked = true;
    }
    if (m_Request) m_Request->Unlock(kWGSProcessorEvent);
}


void CPSGS_WGSProcessor::x_WaitForOtherProcessors(void)
{
    if (m_Canceled) return;
    GetRequest()->WaitFor(kCassandraProcessorEvent);
}


void CPSGS_WGSProcessor::Cancel()
{
    m_Canceled = true;
    if (!IsUVThreadAssigned()) {
        m_Status = ePSGS_Canceled;
        x_Finish(ePSGS_Canceled);
    }
    else {
        x_UnlockRequest();
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
    if (m_AddedToExcludedCache) {
        // The blob was added to the cache but never completed and needs to be removed.
        x_RemoveFromExcludedCache();
    }
    m_Status = status;
    x_UnlockRequest();
    SignalFinishProcessing();
}


void CPSGS_WGSProcessor::x_SendError(shared_ptr<CPSGS_Reply> reply,
                                     const string& msg)
{
    reply->PrepareProcessorMessage(reply->GetItemId(), "WGS", msg,
                                   CRequestStatus::e500_InternalServerError,
                                   ePSGS_UnknownError,
                                   eDiag_Error);
}


void CPSGS_WGSProcessor::x_SendError(const string& msg)
{
    x_SendError(m_Reply, msg);
}


void CPSGS_WGSProcessor::x_SendError(shared_ptr<CPSGS_Reply> reply,
                                     const string& msg, const exception& exc)
{
    x_SendError(reply, msg+string(exc.what()));
}


void CPSGS_WGSProcessor::x_SendError(const string& msg, const exception& exc)
{
    x_SendError(m_Reply, msg+string(exc.what()));
}


string CPSGS_WGSProcessor::GetPSGId2Info(const CID2_Blob_Id& tse_id,
                                         CWGSClient::TID2SplitVersion split_version)
{
    ostringstream s;
    if ( CWGSClient::IsOSGBlob(tse_id) ) {
        s_FormatBlobId(s, tse_id);
        CWGSClient::TID2BlobVersion blob_version = tse_id.IsSetVersion()? tse_id.GetVersion(): 0;
        s << '.' << blob_version << '.' << split_version;
    }
    return s.str();
}


bool CPSGS_WGSProcessor::x_CheckExcludedCache(void)
{
    if (m_ClientId.empty()  ||  m_ResendTimeoutMks == 0 ||
        !m_WGSData  ||  m_WGSData->m_BlobId.empty()) 
        return true; // proceed sending data if any

    bool completed;
    psg_time_point_t completed_time;
    auto* app = CPubseqGatewayApp::GetInstance();
    auto  cache_result = app->GetExcludeBlobCache()->AddBlobId(
        m_ClientId, SExcludeBlobId(m_WGSData->m_BlobId),
        completed, completed_time);
    if (cache_result == ePSGS_Added) {
        m_AddedToExcludedCache = true;
        return true; // blob-id was not in the cache, ok to send data
    }
    // Blob is already in the cache - check the progress and resend timeout.
    if (!completed) {
        m_WGSData->m_GetResult = SWGSData::eResult_InProgress;
        return false;
    }
    m_SentMksAgo = GetTimespanToNowMks(completed_time);
    if (m_SentMksAgo < m_ResendTimeoutMks) {
        m_WGSData->m_GetResult = SWGSData::eResult_Sent;
        return false;
    }
    return true;
}


void CPSGS_WGSProcessor::x_RemoveFromExcludedCache(void)
{
    if (!m_AddedToExcludedCache  ||  m_ClientId.empty()  ||
        !m_WGSData  ||  m_WGSData->m_BlobId.empty())
        return;
    auto* app = CPubseqGatewayApp::GetInstance();
    app->GetExcludeBlobCache()->Remove(m_ClientId, SExcludeBlobId(m_WGSData->m_BlobId));
    m_AddedToExcludedCache = false;
}


void CPSGS_WGSProcessor::x_SetExcludedCacheCompleted(void)
{
    if (!m_AddedToExcludedCache  ||  m_ClientId.empty()  ||
        !m_WGSData  ||  m_WGSData->m_BlobId.empty())
        return;
    auto* app = CPubseqGatewayApp::GetInstance();
    app->GetExcludeBlobCache()->SetCompleted(m_ClientId, SExcludeBlobId(m_WGSData->m_BlobId), true);
    m_AddedToExcludedCache = false;
}


END_NAMESPACE(wgs);
END_NAMESPACE(psg);
END_NCBI_NAMESPACE;
