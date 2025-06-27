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
 * Authors: Aleksey Grichenko
 *
 * File Description: processor for data from CDD
 *
 */

#include <ncbi_pch.hpp>

#include "cdd_processor.hpp"
#include "pubseq_gateway.hpp"
#include "pubseq_gateway_convert_utils.hpp"
#include "pubseq_gateway_logging.hpp"
#include "psgs_thread_pool.hpp"
#include <objtools/data_loaders/cdd/cdd_access/cdd_client.hpp>
#include <objtools/data_loaders/cdd/cdd_access/CDD_Rep_Get_Blob_By_Seq_Id.hpp>
#include <objtools/data_loaders/cdd/cdd_access/CDD_Reply_Get_Blob_Id.hpp>
#include <objects/id2/ID2_Blob_Id.hpp>
#include <objects/seqsplit/ID2S_Seq_annot_Info.hpp>
#include <objects/seqsplit/ID2S_Feat_type_Info.hpp>
#include <objects/seqsplit/ID2S_Seq_loc.hpp>
#include <objects/seqfeat/SeqFeatData.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seqset/seqset__.hpp>
#include <util/thread_pool.hpp>


BEGIN_NCBI_NAMESPACE;
BEGIN_NAMESPACE(psg);
BEGIN_NAMESPACE(cdd);

USING_SCOPE(objects);

static const string kCDDAnnotName = "CDD";
static const string kCDDProcessorGroupName = "CDD";
static const string kCDDProcessorName = "CDD";
static const string kCDDProcessorSection = "CDD_PROCESSOR";
const CID2_Blob_Id::TSat kCDDSat = 8087;

static const string kParamMaxConn = "maxconn";
static const int kDefaultMaxConn = 64;

static const string kParamCDDBackendTimeout = "backend_timeout";
static const double kDefaultCDDBackendTimeout = 5.0;


/////////////////////////////////////////////////////////////////////////////
// Helper classes
/////////////////////////////////////////////////////////////////////////////

BEGIN_LOCAL_NAMESPACE;

class CCDDThreadPoolTask_GetBlobId : public CThreadPool_Task
{
public:
    CCDDThreadPoolTask_GetBlobId(CPSGS_CDDProcessor& processor) : m_Processor(processor) {}

    virtual EStatus Execute(void) override
    {
        m_Processor.GetBlobId();
        return eCompleted;
    }

private:
    CPSGS_CDDProcessor& m_Processor;
};


class CCDDThreadPoolTask_GetBlobBySeqId : public CThreadPool_Task
{
public:
    CCDDThreadPoolTask_GetBlobBySeqId(CPSGS_CDDProcessor& processor) : m_Processor(processor) {}

    virtual EStatus Execute(void) override
    {
        m_Processor.GetBlobBySeqId();
        return eCompleted;
    }

private:
    CPSGS_CDDProcessor& m_Processor;
};


class CCDDThreadPoolTask_GetBlobByBlobId : public CThreadPool_Task
{
public:
    CCDDThreadPoolTask_GetBlobByBlobId(CPSGS_CDDProcessor& processor) : m_Processor(processor) {}

    virtual EStatus Execute(void) override
    {
        m_Processor.GetBlobByBlobId();
        return eCompleted;
    }

private:
    CPSGS_CDDProcessor& m_Processor;
};


END_LOCAL_NAMESPACE;


NCBI_PARAM_DECL(int, CDD_PROCESSOR, ERROR_RATE);
NCBI_PARAM_DEF(int, CDD_PROCESSOR, ERROR_RATE, 0);

static bool s_SimulateError()
{
    static int error_rate = NCBI_PARAM_TYPE(CDD_PROCESSOR, ERROR_RATE)::GetDefault();
    if ( error_rate > 0 ) {
        static int error_counter = 0;
        if ( ++error_counter >= error_rate ) {
            error_counter = 0;
            return true;
        }
    }
    return false;
}


CPSGS_CDDProcessor::CPSGS_CDDProcessor(void)
    : m_ClientPool(new CCDDClientPool()),
      m_Status(ePSGS_NotFound),
      m_Canceled(false),
      m_Unlocked(true)
{
    const CNcbiRegistry& registry = CPubseqGatewayApp::GetInstance()->GetConfig();
    double timeout = registry.GetDouble(kCDDProcessorSection, kParamCDDBackendTimeout, kDefaultCDDBackendTimeout);
    try {
        m_ClientPool->SetClientTimeout(CTimeout(timeout));
    }
    catch (CTimeException&) {
        m_ClientPool->SetClientTimeout(CTimeout(CTimeout::eDefault));
    }
    unsigned int max_conn = registry.GetInt(kCDDProcessorSection, kParamMaxConn, kDefaultMaxConn);
    if (max_conn == 0) {
        max_conn = kDefaultMaxConn;
    }
    m_ThreadPool.reset(new CThreadPool(kMax_UInt,
                                       new CPSGS_ThreadPool_Controller(
                                           min(3u, max_conn), max_conn)));
}

CPSGS_CDDProcessor::CPSGS_CDDProcessor(
    shared_ptr<CCDDClientPool> client_pool,
    shared_ptr<CThreadPool> thread_pool,
    shared_ptr<CPSGS_Request> request,
    shared_ptr<CPSGS_Reply> reply,
    TProcessorPriority priority)
    : m_ClientPool(client_pool),
      m_Start(psg_clock_t::now()),
      m_Status(ePSGS_InProgress),
      m_Canceled(false),
      m_Unlocked(true),
      m_ThreadPool(thread_pool)
{
    m_Request = request;
    m_Reply = reply;
    m_Priority = priority;
}

CPSGS_CDDProcessor::~CPSGS_CDDProcessor(void)
{
    _ASSERT(m_Status != ePSGS_InProgress);
    x_UnlockRequest();
}


bool CPSGS_CDDProcessor::x_IsEnabled(CPSGS_Request& request) const
{
    // first check global setting
    bool enabled = CPubseqGatewayApp::GetInstance()->Settings().m_CDDProcessorsEnabled;
    if ( enabled ) {
        // check if disabled in request
        for (const auto& name : request.GetRequest<SPSGS_RequestBase>().m_DisabledProcessors ) {
            if ( NStr::EqualNocase(name, kCDDProcessorName) ) {
                enabled = false;
                break;
            }
        }
    }
    else {
        // check if enabled in request
        for (const auto& name : request.GetRequest<SPSGS_RequestBase>().m_EnabledProcessors ) {
            if ( NStr::EqualNocase(name, kCDDProcessorName) ) {
                enabled = true;
                break;
            }
        }
    }
    return enabled;
}


vector<string> CPSGS_CDDProcessor::WhatCanProcess(shared_ptr<CPSGS_Request> request,
                                                  shared_ptr<CPSGS_Reply> reply) const
{
    try {
        vector<string> can_process;
        if ( x_IsEnabled(*request) ) {
            SPSGS_AnnotRequest& annot_request = request->GetRequest<SPSGS_AnnotRequest>();
            if ( x_CanProcessAnnotRequestIds(annot_request) ) {
                for ( auto& name : annot_request.m_Names ) {
                    if ( name == kCDDAnnotName ) {
                        can_process.push_back(name);
                        break;
                    }
                }
            }
        }
        return can_process;
    }
    catch ( exception& exc ) {
        x_SendError(reply, "Exception in WhatCanProcess: ", exc);
        throw;
    }
}


bool CPSGS_CDDProcessor::CanProcess(shared_ptr<CPSGS_Request> request,
                                    shared_ptr<CPSGS_Reply> reply) const
{
    try {
        auto req_type = request->GetRequestType();
        if (req_type != CPSGS_Request::ePSGS_AnnotationRequest &&
            req_type != CPSGS_Request::ePSGS_BlobBySatSatKeyRequest) {
            return false;
        }

        if ( !x_IsEnabled(*request) ) {
            return false;
        }

        if (req_type == CPSGS_Request::ePSGS_AnnotationRequest &&
            !x_CanProcessAnnotRequest(request->GetRequest<SPSGS_AnnotRequest>(), 0)) {
            return false;
        }
        if (req_type == CPSGS_Request::ePSGS_BlobBySatSatKeyRequest &&
            !x_CanProcessBlobRequest(request->GetRequest<SPSGS_BlobBySatSatKeyRequest>())) {
            return false;
        }
    
        return true;
    }
    catch ( exception& exc ) {
        x_SendError(reply, "Exception in CanProcess: ", exc);
        throw;
    }
}


IPSGS_Processor*
CPSGS_CDDProcessor::CreateProcessor(shared_ptr<CPSGS_Request> request,
                                    shared_ptr<CPSGS_Reply> reply,
                                    TProcessorPriority priority) const
{
    try {
        auto req_type = request->GetRequestType();
        if (req_type != CPSGS_Request::ePSGS_AnnotationRequest &&
            req_type != CPSGS_Request::ePSGS_BlobBySatSatKeyRequest) {
            return nullptr;
        }

        if ( !x_IsEnabled(*request) ) {
            return nullptr;
        }

        if (req_type == CPSGS_Request::ePSGS_AnnotationRequest &&
            !x_CanProcessAnnotRequest(request->GetRequest<SPSGS_AnnotRequest>(), priority)) {
            return nullptr;
        }
        if (req_type == CPSGS_Request::ePSGS_BlobBySatSatKeyRequest &&
            !x_CanProcessBlobRequest(request->GetRequest<SPSGS_BlobBySatSatKeyRequest>())) {
            return nullptr;
        }

        return new CPSGS_CDDProcessor(m_ClientPool, m_ThreadPool, request, reply, priority);
    }
    catch ( exception& exc ) {
        x_SendError(reply, "Exception in CreateProcessor: ", exc);
        throw;
    }
}


string CPSGS_CDDProcessor::GetName() const
{
    return kCDDProcessorName;
}


string CPSGS_CDDProcessor::GetGroupName() const
{
    return kCDDProcessorGroupName;
}


void CPSGS_CDDProcessor::x_SendError(shared_ptr<CPSGS_Reply> reply,
                                     const string& msg)
{
    reply->PrepareProcessorMessage(reply->GetItemId(), kCDDProcessorName, msg,
                                   CRequestStatus::e500_InternalServerError,
                                   ePSGS_UnknownError,
                                   eDiag_Error);
}


void CPSGS_CDDProcessor::x_SendError(const string& msg)
{
    x_SendError(GetReply(), msg);
}


void CPSGS_CDDProcessor::x_SendError(shared_ptr<CPSGS_Reply> reply,
                                     const string& msg, const exception& exc)
{
    x_SendError(reply, msg+string(exc.what()));
}


void CPSGS_CDDProcessor::x_SendError(const string& msg, const exception& exc)
{
    x_SendError(GetReply(), msg+string(exc.what()));
}


void CPSGS_CDDProcessor::x_ReportResultStatus(SPSGS_AnnotRequest::EPSGS_ResultStatus status)
{
    SPSGS_AnnotRequest& annot_request = GetRequest()->GetRequest<SPSGS_AnnotRequest>();
    annot_request.ReportResultStatus(kCDDAnnotName, status);
}


void CPSGS_CDDProcessor::Process()
{
    _ASSERT(GetRequest());
    CRequestContextResetter     context_resetter;
    GetRequest()->SetRequestContext();

    try {
        {
            CFastMutexGuard guard(m_Mutex);
            m_Unlocked = false;
        }
        GetRequest()->Lock(kCDDProcessorEvent);
        auto req_type = GetRequest()->GetRequestType();
        switch (req_type) {
        case CPSGS_Request::ePSGS_AnnotationRequest:
            x_ProcessResolveRequest();
            break;
        case CPSGS_Request::ePSGS_BlobBySatSatKeyRequest:
            x_ProcessGetBlobRequest();
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


static void s_OnGotBlobId(void* data)
{
    static_cast<CPSGS_CDDProcessor*>(data)->OnGotBlobId();
}


static void s_OnGotBlobBySeqId(void* data)
{
    static_cast<CPSGS_CDDProcessor*>(data)->OnGotBlobBySeqId();
}


void CPSGS_CDDProcessor::x_ProcessResolveRequest(void)
{
    SPSGS_AnnotRequest& annot_request = GetRequest()->GetRequest<SPSGS_AnnotRequest>();
    if ( !x_NameIncluded(annot_request.GetNotProcessedName(m_Priority)) ) {
        x_ReportResultStatus(SPSGS_AnnotRequest::ePSGS_RS_NotFound);
        x_Finish(ePSGS_NotFound);
        return;
    }
    if (!annot_request.m_SeqId.empty() && x_CanProcessSeq_id(annot_request.m_SeqId)) {
        m_SeqIds.push_back(CSeq_id_Handle::GetHandle(annot_request.m_SeqId));
    }
    for (auto& id : annot_request.m_SeqIds) {
        if (x_CanProcessSeq_id(id)) {
            m_SeqIds.push_back(CSeq_id_Handle::GetHandle(id));
        }
    }

    if (annot_request.m_TSEOption == SPSGS_BlobRequestBase::EPSGS_TSEOption::ePSGS_SmartTSE ||
        annot_request.m_TSEOption == SPSGS_BlobRequestBase::EPSGS_TSEOption::ePSGS_WholeTSE ||
        annot_request.m_TSEOption == SPSGS_BlobRequestBase::EPSGS_TSEOption::ePSGS_OrigTSE) {
        // Send whole TSE.
        m_ThreadPool->AddTask(new CCDDThreadPoolTask_GetBlobBySeqId(*this));
    }
    else {
        // Send annot info only.
        m_ThreadPool->AddTask(new CCDDThreadPoolTask_GetBlobId(*this));
    }
}


static void s_OnGotBlobByBlobId(void* data)
{
    static_cast<CPSGS_CDDProcessor*>(data)->OnGotBlobByBlobId();
}


void CPSGS_CDDProcessor::x_ProcessGetBlobRequest(void)
{
    SPSGS_BlobBySatSatKeyRequest blob_request =
        GetRequest()->GetRequest<SPSGS_BlobBySatSatKeyRequest>();
    m_BlobId = CCDDClientPool::StringToBlobId(blob_request.m_BlobId.GetId());
    if ( !m_BlobId ) {
        x_Finish(ePSGS_NotFound);
        return;
    }
    m_ThreadPool->AddTask(new CCDDThreadPoolTask_GetBlobByBlobId(*this));
}


void CPSGS_CDDProcessor::GetBlobId(void)
{
    CRequestContextResetter context_resetter;
    GetRequest()->SetRequestContext();
    for (auto id : m_SeqIds) {
        try {
            if ( GetRequest()->NeedTrace() ) {
                GetReply()->SendTrace(
                    kCDDProcessorName + " processor trying to get blob-id by seq-id " + id.AsString(),
                    GetRequest()->GetStartTimestamp());
            }
            m_CDDBlob.info = m_ClientPool->GetBlobIdBySeq_id(id);
            if (m_CDDBlob.info) break;
        }
        catch (exception& exc) {
            if ( GetRequest()->NeedTrace() ) {
                GetReply()->SendTrace(
                    kCDDProcessorName + " processor failed to get blob-id by seq-id, exception: " + exc.what(),
                    GetRequest()->GetStartTimestamp());
            }
            m_Error = "Exception when handling get_na request: " + string(exc.what());
            m_CDDBlob.info.Reset();
            m_CDDBlob.data.Reset();
        }
    }
    PostponeInvoke(s_OnGotBlobId, this);
}


void CPSGS_CDDProcessor::GetBlobBySeqId(void)
{
    CRequestContextResetter context_resetter;
    GetRequest()->SetRequestContext();
    for (auto id : m_SeqIds) {
        try {
            if ( GetRequest()->NeedTrace() ) {
                GetReply()->SendTrace(
                    kCDDProcessorName + " processor trying to get blob by seq-id " + id.AsString(),
                    GetRequest()->GetStartTimestamp());
            }
            m_CDDBlob = m_ClientPool->GetBlobBySeq_id(id);
            if (m_CDDBlob.info && m_CDDBlob.data) break;
        }
        catch (exception& exc) {
            if ( GetRequest()->NeedTrace() ) {
                GetReply()->SendTrace(
                    kCDDProcessorName + " processor failed to get blob by seq-id, exception: " + exc.what(),
                    GetRequest()->GetStartTimestamp());
            }
            m_Error = "Exception when handling get_na request: " + string(exc.what());
            m_CDDBlob.info.Reset();
            m_CDDBlob.data.Reset();
        }
    }
    PostponeInvoke(s_OnGotBlobBySeqId, this);
}


void CPSGS_CDDProcessor::GetBlobByBlobId(void)
{
    CRequestContextResetter context_resetter;
    GetRequest()->SetRequestContext();
    try {
        if ( GetRequest()->NeedTrace() ) {
            GetReply()->SendTrace(
                kCDDProcessorName + " processor trying to get blob by blob-id " + CCDDClientPool::BlobIdToString(*m_BlobId),
                GetRequest()->GetStartTimestamp());
        }
        m_CDDBlob.data = m_ClientPool->GetBlobByBlobId(*m_BlobId);
    }
    catch (exception& exc) {
        if ( GetRequest()->NeedTrace() ) {
            GetReply()->SendTrace(
                kCDDProcessorName + " processor failed to get blob by blob-id, exception: " + exc.what(),
                GetRequest()->GetStartTimestamp());
        }
        m_Error = "Exception when handling getblob request: " + string(exc.what());
        m_CDDBlob.info.Reset();
        m_CDDBlob.data.Reset();
    }
    PostponeInvoke(s_OnGotBlobByBlobId, this);
}


void CPSGS_CDDProcessor::OnGotBlobId(void)
{
    CRequestContextResetter context_resetter;
    GetRequest()->SetRequestContext();
    if ( x_IsCanceled() ) {
        return;
    }
    if ( s_SimulateError() ) {
        m_Error = "simulated CDD processor error";
        m_CDDBlob.info.Reset();
        m_CDDBlob.data.Reset();
    }
    if ( !m_CDDBlob.info ) {
        if ( !m_Error.empty() ) {
            x_SendError(m_Error);
            x_ReportResultStatus(SPSGS_AnnotRequest::ePSGS_RS_Error);
            x_Finish(ePSGS_Error);
        }
        else {
            if ( GetRequest()->NeedTrace() ) {
                GetReply()->SendTrace(
                    kCDDProcessorName + " processor did not find the requested blob-id",
                    GetRequest()->GetStartTimestamp());
            }
            x_RegisterTimingNotFound(eNAResolve);
            x_ReportResultStatus(SPSGS_AnnotRequest::ePSGS_RS_NotFound);
            x_Finish(ePSGS_NotFound);
        }
        return;
    }
    if ( !x_SignalStartProcessing() ) {
        return;
    }
    try {
        x_SendAnnotInfo(*m_CDDBlob.info);
    }
    catch (exception& exc) {
        m_Error = "Exception when sending get_na reply: " + string(exc.what());
        x_SendError(m_Error);
        x_ReportResultStatus(SPSGS_AnnotRequest::ePSGS_RS_Error);
        x_Finish(ePSGS_Error);
        return;
    }
    x_Finish(ePSGS_Done);
}


void CPSGS_CDDProcessor::OnGotBlobBySeqId(void)
{
    CRequestContextResetter context_resetter;
    GetRequest()->SetRequestContext();
    if ( x_IsCanceled() ) {
        return;
    }
    if ( s_SimulateError() ) {
        m_Error = "simulated CDD processor error";
        m_CDDBlob.info.Reset();
        m_CDDBlob.data.Reset();
    }
    if ( !m_CDDBlob.info  ||  !m_CDDBlob.data ) {
        if ( !m_Error.empty() ) {
            x_SendError(m_Error);
            x_ReportResultStatus(SPSGS_AnnotRequest::ePSGS_RS_Error);
            x_Finish(ePSGS_Error);
        }
        else {
            if ( GetRequest()->NeedTrace() ) {
                GetReply()->SendTrace(
                    kCDDProcessorName + " processor did not find the requested blob",
                    GetRequest()->GetStartTimestamp());
            }
            x_RegisterTimingNotFound(eNAResolve);
            x_ReportResultStatus(SPSGS_AnnotRequest::ePSGS_RS_NotFound);
            x_Finish(ePSGS_NotFound);
        }
        return;
    }
    if ( !x_SignalStartProcessing() ) {
        return;
    }
    try {
        x_SendAnnotInfo(*m_CDDBlob.info);
        x_SendAnnot(m_CDDBlob.info->GetBlob_id(), m_CDDBlob.data);
    }
    catch (exception& exc) {
        m_Error = "Exception when sending get_na reply: " + string(exc.what());
        x_SendError(m_Error);
        x_ReportResultStatus(SPSGS_AnnotRequest::ePSGS_RS_Error);
        x_Finish(ePSGS_Error);
        return;
    }
    x_Finish(ePSGS_Done);
}


void CPSGS_CDDProcessor::OnGotBlobByBlobId(void)
{
    CRequestContextResetter context_resetter;
    GetRequest()->SetRequestContext();
    if ( x_IsCanceled() ) {
        return;
    }
    if ( s_SimulateError() ) {
        m_Error = "simulated CDD processor error";
        m_CDDBlob.info.Reset();
        m_CDDBlob.data.Reset();
    }
    if ( !m_CDDBlob.data ) {
        if ( !m_Error.empty() ) {
            x_SendError(m_Error);
            x_Finish(ePSGS_Error);
        }
        else {
            if ( GetRequest()->NeedTrace() ) {
                GetReply()->SendTrace(
                    kCDDProcessorName + " processor did not find the requested blob",
                    GetRequest()->GetStartTimestamp());
            }
            x_RegisterTimingNotFound(eNARetrieve);
            x_Finish(ePSGS_NotFound);
        }
        return;
    }
    if ( !x_SignalStartProcessing() ) {
        return;
    }
    try {
        x_SendAnnot(*m_BlobId, m_CDDBlob.data);
    }
    catch (exception& exc) {
        m_Error = "Exception when sending getblob reply: " + string(exc.what());
        x_SendError(m_Error);
        x_Finish(ePSGS_Error);
        return;
    }
    x_Finish(ePSGS_Done);
}


void CPSGS_CDDProcessor::x_RegisterTiming(EPSGOperation operation,
                                          EPSGOperationStatus status,
                                          size_t blob_size)
{
    CPubseqGatewayApp::GetInstance()->
        GetTiming().Register(this, operation, status, m_Start, blob_size);
}


void CPSGS_CDDProcessor::x_RegisterTimingNotFound(EPSGOperation operation)
{
    x_RegisterTiming(operation, eOpStatusNotFound, 0);
}


void CPSGS_CDDProcessor::x_SendAnnotInfo(const CCDD_Reply_Get_Blob_Id& blob_info)
{
    SPSGS_AnnotRequest& annot_request = GetRequest()->GetRequest<SPSGS_AnnotRequest>();
    if ( annot_request.RegisterProcessedName(GetPriority(), kCDDAnnotName) > GetPriority() ) {
        // higher priority processor already processed this request
        if ( GetRequest()->NeedTrace() ) {
            GetReply()->SendTrace(
                kCDDProcessorName + " processor stops sending annot-info because a higher priority processor has already sent it",
                GetRequest()->GetStartTimestamp());
        }
        x_Finish(ePSGS_Canceled);
        return;
    }

    x_RegisterTiming(eNAResolve, eOpStatusFound, 0);
    
    const CID2_Blob_Id& blob_id = blob_info.GetBlob_id();
    CJsonNode       json(CJsonNode::NewObjectNode());
    json.SetString("blob_id", CCDDClientPool::BlobIdToString(blob_id));
    if ( blob_id.IsSetVersion() ) {
        json.SetInteger("last_modified", blob_id.GetVersion()*60000);
    }

    CRef<CID2S_Seq_annot_Info> annot_info(new CID2S_Seq_annot_Info);
    annot_info->SetName(kCDDAnnotName);
    CRef<CID2S_Feat_type_Info> feat_info(new CID2S_Feat_type_Info);
    feat_info->SetType(CSeqFeatData::e_Region);
    feat_info->SetSubtypes().push_back(CSeqFeatData::eSubtype_region);
    annot_info->SetFeat().push_back(feat_info);
    feat_info.Reset(new CID2S_Feat_type_Info);
    feat_info->SetType(CSeqFeatData::e_Site);
    feat_info->SetSubtypes().push_back(CSeqFeatData::eSubtype_site);
    annot_info->SetFeat().push_back(feat_info);

    const CSeq_id& annot_id = blob_info.GetSeq_id();
    if ( annot_id.IsGi() ) {
        annot_info->SetSeq_loc().SetWhole_gi(annot_id.GetGi());
    }
    else {
        annot_info->SetSeq_loc().SetWhole_seq_id().Assign(annot_id);
    }

    ostringstream annot_str;
    annot_str << MSerial_AsnBinary << *annot_info;
    json.SetString("seq_annot_info", NStr::Base64Encode(annot_str.str(), 0));

    GetReply()->PrepareNamedAnnotationData(kCDDAnnotName, kCDDProcessorName,
        json.Repr(CJsonNode::fStandardJson));
}


void CPSGS_CDDProcessor::x_SendAnnot(const CID2_Blob_Id& id2_blob_id, CRef<CSeq_annot>& annot)
{
    string psg_blob_id = CCDDClientPool::BlobIdToString(id2_blob_id);
    CRef<CSeq_entry> entry(new CSeq_entry);
    entry->SetSet().SetSeq_set();
    entry->SetAnnot().push_back(annot);
    ostringstream blob_str;
    blob_str << MSerial_AsnBinary << *entry;
    string blob_data = blob_str.str();

    x_RegisterTiming(eNARetrieve, eOpStatusFound, blob_data.size());
    
    CBlobRecord blob_props;
    if (id2_blob_id.IsSetVersion()) {
        blob_props.SetModified(int64_t(id2_blob_id.GetVersion()*60000));
    }
    blob_props.SetNChunks(1);
    size_t item_id = GetReply()->GetItemId();
    GetReply()->PrepareBlobPropData(
        item_id,
        kCDDProcessorName,
        psg_blob_id,
        ToJsonString(blob_props));
    GetReply()->PrepareBlobPropCompletion(item_id, kCDDProcessorName, 2);

    item_id = GetReply()->GetItemId();
    GetReply()->PrepareBlobData(
        item_id,
        kCDDProcessorName,
        psg_blob_id,
        (const unsigned char*)blob_data.data(), blob_data.size(), 0);
    GetReply()->PrepareBlobCompletion(item_id, kCDDProcessorName, 2);
}


void CPSGS_CDDProcessor::Cancel()
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


IPSGS_Processor::EPSGS_Status CPSGS_CDDProcessor::GetStatus()
{
    return m_Status;
}


void CPSGS_CDDProcessor::x_UnlockRequest(void)
{
    {
        CFastMutexGuard guard(m_Mutex);
        if (m_Unlocked) return;
        m_Unlocked = true;
    }
    if (GetRequest()) GetRequest()->Unlock(kCDDProcessorEvent);
}


bool CPSGS_CDDProcessor::x_IsCanceled()
{
    if ( m_Canceled ) {
        x_Finish(ePSGS_Canceled);
        return true;
    }
    return false;
}


bool CPSGS_CDDProcessor::x_SignalStartProcessing()
{
    if ( GetRequest()->GetRequestType() == CPSGS_Request::ePSGS_AnnotationRequest ) {
        SPSGS_AnnotRequest& annot_request = GetRequest()->GetRequest<SPSGS_AnnotRequest>();
        if ( annot_request.RegisterProcessedName(GetPriority(), kCDDAnnotName) > GetPriority() ) {
            // higher priority processor already processed this request
            if ( GetRequest()->NeedTrace() ) {
                GetReply()->SendTrace(
                    kCDDProcessorName + " processor stops processing request because a higher priority processor has already processed it",
                    GetRequest()->GetStartTimestamp());
            }
            x_Finish(ePSGS_Canceled);
            return false;
        }
    }
    else {
        if ( SignalStartProcessing() == ePSGS_Cancel ) {
            x_Finish(ePSGS_Canceled);
            return false;
        }
    }
    return true;
}


bool CPSGS_CDDProcessor::x_CanProcessSeq_id(const string& psg_id) const
{
    try {
        CSeq_id id(psg_id);
        if (!id.IsGi() && !id.GetTextseq_Id()) return false;
        if (!m_ClientPool->IsValidId(id)) return false;
    }
    catch (exception& e) {
        return false;
    }
    return true;
}


bool CPSGS_CDDProcessor::x_CanProcessAnnotRequestIds(SPSGS_AnnotRequest& annot_request) const
{
    if (!annot_request.m_SeqId.empty() && x_CanProcessSeq_id(annot_request.m_SeqId)) return true;
    for (const auto& id: annot_request.m_SeqIds) {
        if (x_CanProcessSeq_id(id)) return true;
    }
    return false;
}


bool CPSGS_CDDProcessor::x_CanProcessAnnotRequest(SPSGS_AnnotRequest& annot_request,
                                                  TProcessorPriority priority) const
{
    if (!x_NameIncluded(annot_request.GetNotProcessedName(priority))) {
        return false;
    }
    if (!x_CanProcessAnnotRequestIds(annot_request)) {
        return false;
    }
    return true;
}


bool CPSGS_CDDProcessor::x_CanProcessBlobRequest(SPSGS_BlobBySatSatKeyRequest& blob_request) const
{
    CRef<CCDDClientPool::TBlobId> blob_id =
        CCDDClientPool::StringToBlobId(blob_request.m_BlobId.GetId());
    return blob_id && blob_id->GetSat() == kCDDSat;
}


bool CPSGS_CDDProcessor::x_NameIncluded(const vector<string>& names) const
{
    for ( auto& name : names ) {
        if ( name == kCDDAnnotName ) return true;
    }
    return false;
}


void CPSGS_CDDProcessor::x_Finish(EPSGS_Status status)
{
    _ASSERT(status != ePSGS_InProgress);
    m_Status = status;
    x_UnlockRequest();
    SignalFinishProcessing();
}


END_NAMESPACE(cdd);
END_NAMESPACE(psg);
END_NCBI_NAMESPACE;
