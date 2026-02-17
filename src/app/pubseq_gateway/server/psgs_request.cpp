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
 * Authors: Sergey Satskiy
 *
 * File Description:
 *
 */
#include <ncbi_pch.hpp>

#include <corelib/ncbistr.hpp>

#include "psgs_request.hpp"
#include "pubseq_gateway_logging.hpp"
#include "pubseq_gateway_utils.hpp"

USING_NCBI_SCOPE;

static atomic<bool>     s_RequestIdLock(false);
static size_t           s_NextRequestId = 0;


size_t  GetNextRequestId(void)
{
    CSpinlockGuard      guard(&s_RequestIdLock);
    auto request_id = ++s_NextRequestId;
    return request_id;
}


SPSGS_BlobId::SPSGS_BlobId(int32_t  sat, int32_t  sat_key)
{
    char    buf[kPSGToStringBufferSize];
    m_Id.append(buf, PSGToString(sat, buf))
        .push_back('.');
    m_Id.append(buf, PSGToString(sat_key, buf));
}


CPSGS_Request::CPSGS_Request() :
    m_RequestId(0),
    m_BacklogTimeMks(0),
    m_ConcurrentProcessorCount(0)
{}


CPSGS_Request::~CPSGS_Request()
{
    for (auto it: m_Wait) {
        switch (it.second->m_State) {
            case SWaitData::ePSGS_Unlocked:
                delete it.second;
                break;
            case SWaitData::ePSGS_LockedNobodyWaits:
                // This is rather strange: a processor has not unlocked an
                // event and at the same time a request is going to be
                // destroyed.
                PSG_ERROR("Reply is going to be deleted when a processor has "
                          "not unlocked an event");
                delete it.second;
                break;
            case SWaitData::ePSGS_LockedSomebodyWaits:
                // This is rather strange: a processor is still waiting on the
                // condition variable and at the same time the request is going
                // to be destroyed.
                // Just in case, lets unlock the condition variable and then
                // delete the associated data
                PSG_ERROR("Reply is going to be deleted when a processor is "
                          "locked on a condition variable");
                it.second->m_WaitObject.notify_all();
                delete it.second;
                break;
        }
    }
}


CPSGS_Request::CPSGS_Request(const CHttpRequest &  http_request,
                             unique_ptr<SPSGS_RequestBase> req,
                             CRef<CRequestContext>  request_context) :
    m_HttpRequest(http_request),
    m_Request(std::move(req)),
    m_RequestContext(request_context),
    m_RequestId(GetNextRequestId()),
    m_BacklogTimeMks(0),
    m_ConcurrentProcessorCount(0)
{}


CPSGS_Request::EPSGS_Type  CPSGS_Request::GetRequestType(void) const
{
    if (m_Request)
        return m_Request->GetRequestType();
    return ePSGS_UnknownRequest;
}


void CPSGS_Request::Lock(const string &  event_name)
{
    if (m_ConcurrentProcessorCount < 2)
        return;     // No parallel processors so there is no point to wait

    unique_lock<mutex>   scope_lock(m_WaitLock);

    if (m_Wait.find(event_name) != m_Wait.end()) {
        // Double locking; it is rather an error
        NCBI_THROW(CPubseqGatewayException, eLogic,
                   "Multiple lock of the same event is not supported");
    }

    m_Wait[event_name] = new SWaitData();
    m_Wait[event_name]->m_State = SWaitData::ePSGS_LockedNobodyWaits;
}


void CPSGS_Request::Unlock(const string &  event_name)
{
    if (m_ConcurrentProcessorCount < 2)
        return;     // No parallel processors so there is no point to wait

    unique_lock<mutex>   scope_lock(m_WaitLock);

    auto it = m_Wait.find(event_name);
    if (it == m_Wait.end()) {
        // Unlocking something which was not locked
        return;
    }

    switch (it->second->m_State) {
        case SWaitData::ePSGS_LockedNobodyWaits:
            it->second->m_State = SWaitData::ePSGS_Unlocked;
            break;
        case SWaitData::ePSGS_Unlocked:
            // Double unlocking; it's OK
            break;
        case SWaitData::ePSGS_LockedSomebodyWaits:
            it->second->m_WaitCount = 0;
            it->second->m_State = SWaitData::ePSGS_Unlocked;
            it->second->m_WaitObject.notify_all();
            break;
    }
}


void CPSGS_Request::WaitFor(const string &  event_name, size_t  timeout_sec)
{
    if (m_ConcurrentProcessorCount < 2) {
        // No parallel processors so there is no point to wait
        return;
    }

    unique_lock<mutex>   scope_lock(m_WaitLock);

    auto it = m_Wait.find(event_name);
    if (it == m_Wait.end()) {
        // There was no Lock() call for that event
        return;
    }

    switch (it->second->m_State) {
        case SWaitData::ePSGS_LockedNobodyWaits:
        case SWaitData::ePSGS_LockedSomebodyWaits:
            // The logic to initiate waiting is below
            break;
        case SWaitData::ePSGS_Unlocked:
            // It has already been unlocked
            return;
    }

    ++it->second->m_WaitCount;
    it->second->m_State = SWaitData::ePSGS_LockedSomebodyWaits;
    auto    status = it->second->m_WaitObject.wait_for(scope_lock,
                                                       chrono::seconds(timeout_sec));
    if (status == cv_status::timeout) {
        // Note: decrementing the count only in case of a timeout. In case of a
        // normal Unlock() the count will be reset there.
        if (--it->second->m_WaitCount == 0)
            it->second->m_State = SWaitData::ePSGS_LockedNobodyWaits;

        string  message = "Timeout (" + to_string(timeout_sec) +
                          " seconds) waiting on event '" + event_name + "'";

        PSG_WARNING(message);
        NCBI_THROW(CPubseqGatewayException, eTimeout, message);
    }

    // Here:
    // - waiting has completed within the timeout
    // - there is no need to change the state because it is done in Unlock()
    //   by unlocking processor and the state here is ePSGS_Unlocked
}


// Provides the original request context
CRef<CRequestContext>  CPSGS_Request::GetRequestContext(void)
{
    return m_RequestContext;
}


// Sets the cloned request context so that many threads can produce messages
void CPSGS_Request::SetRequestContext(void)
{
    CDiagContext::SetRequestContext(m_RequestContext->Clone());
    CDiagContext::GetRequestContext().SetReadOnly(false);
}


psg_time_point_t CPSGS_Request::GetStartTimestamp(void) const
{
    if (m_Request)
        return m_Request->GetStartTimestamp();

    NCBI_THROW(CPubseqGatewayException, eLogic,
               "User request is not initialized");
}


bool CPSGS_Request::NeedTrace(void)
{
    if (m_Request)
        return m_Request->GetTrace() == SPSGS_RequestBase::ePSGS_WithTracing;

    NCBI_THROW(CPubseqGatewayException, eLogic,
               "User request is not initialized");
}


bool CPSGS_Request::NeedProcessorEvents(void)
{
    if (m_Request)
        return m_Request->GetProcessorEvents();

    NCBI_THROW(CPubseqGatewayException, eLogic,
               "User request is not initialized");
}


optional<bool> CPSGS_Request::GetIncludeHUP(void)
{
    if (m_Request) {
        switch (m_Request->GetRequestType()) {
            case ePSGS_BlobBySeqIdRequest:
            case ePSGS_BlobBySatSatKeyRequest:
            case ePSGS_AnnotationRequest:
                return GetRequest<SPSGS_BlobRequestBase>().m_IncludeHUP;
            case ePSGS_TSEChunkRequest:
                return GetRequest<SPSGS_TSEChunkRequest>().m_IncludeHUP;
            case ePSGS_ResolveRequest:
            case ePSGS_AccessionVersionHistoryRequest:
            case ePSGS_IPGResolveRequest:
                break;
            default:
                break;
        }

        return optional<bool>();
    }
    NCBI_THROW(CPubseqGatewayException, eLogic,
               "User request is not initialized");
}


int CPSGS_Request::GetHops(void)
{
    if (m_Request) {
        switch (m_Request->GetRequestType()) {
            case ePSGS_ResolveRequest:
                return GetRequest<SPSGS_ResolveRequest>().m_Hops;
            case ePSGS_BlobBySeqIdRequest:
            case ePSGS_BlobBySatSatKeyRequest:
            case ePSGS_AnnotationRequest:
                return GetRequest<SPSGS_BlobRequestBase>().m_Hops;
            case ePSGS_TSEChunkRequest:
                return GetRequest<SPSGS_TSEChunkRequest>().m_Hops;
            case ePSGS_AccessionVersionHistoryRequest:
                return GetRequest<SPSGS_AccessionVersionHistoryRequest>().m_Hops;
            case ePSGS_IPGResolveRequest:
                break;
            default:
                break;
        }

        return 0;
    }
    NCBI_THROW(CPubseqGatewayException, eLogic,
               "User request is not initialized");
}


string CPSGS_Request::GetName(void) const
{
    if (m_Request)
        return m_Request->GetName();
    return "unknown (request is not initialized)";
}


CJsonNode CPSGS_Request::Serialize(void) const
{
    CJsonNode       json;

    if (m_Request) {
        json = m_Request->Serialize();
    } else {
        json = CJsonNode::NewObjectNode();
        json.SetString("request name", GetName());
    }

    json.SetInteger("request id", m_RequestId);
    json.SetInteger("backlog time mks", m_BacklogTimeMks);
    json.SetInteger("concurrent processor count", m_ConcurrentProcessorCount);
    return json;
}


string CPSGS_Request::GetLimitedProcessorsMessage(void) const
{
    string      msg;

    for (auto &  item : m_LimitedProcessors) {
        if (!msg.empty())
            msg += "; ";
        msg += "processor: " + item.first +
               ", concurrency limit: " + to_string(item.second);
    }
    return msg;
}


string CPSGS_Request::GetThrottledProcessorsMessage(void) const
{
    string      msg;

    for (auto &  item : m_IPThrottledProcessors) {
        if (!msg.empty())
            msg += "; ";
        msg += "processor: " + item.first +
               ", throttling by ip limit: " + to_string(item.second);
    }
    return msg;
}


void SPSGS_RequestBase::AppendCommonParameters(CJsonNode &  json) const
{
    json.SetString("trace", TraceToString(m_Trace));
    json.SetBoolean("processor events", m_ProcessorEvents);

    auto            now = psg_clock_t::now();
    uint64_t        mks = chrono::duration_cast<chrono::microseconds>
                            (now - m_StartTimestamp).count();
    json.SetInteger("created ago mks", mks);

    CJsonNode   enabled_procs(CJsonNode::NewArrayNode());
    for (const auto &  name : m_EnabledProcessors) {
        enabled_procs.AppendString(name);
    }
    json.SetByKey("enabled processors", enabled_procs);

    CJsonNode   disabled_procs(CJsonNode::NewArrayNode());
    for (const auto &  name : m_DisabledProcessors) {
        disabled_procs.AppendString(name);
    }
    json.SetByKey("disabled processors", disabled_procs);
}


CJsonNode SPSGS_ResolveRequest::Serialize(void) const
{
    CJsonNode       json(CJsonNode::NewObjectNode());

    json.SetString("request name", GetName());
    json.SetString("seq id", m_SeqId);
    json.SetInteger("seq id type", m_SeqIdType);
    json.SetInteger("include data flags", m_IncludeDataFlags);
    json.SetString("output format", OutputFormatToString(m_OutputFormat));
    json.SetString("use cache", CacheAndDbUseToString(m_UseCache));
    json.SetString("subst option", AccSubstitutioOptionToString(m_AccSubstOption));
    json.SetInteger("hops", m_Hops);
    SPSGS_RequestBase::AppendCommonParameters(json);
    return json;
}


void SPSGS_BlobRequestBase::AppendCommonParameters(CJsonNode &  json) const
{
    json.SetString("tse option", TSEOptionToString(m_TSEOption));
    json.SetString("use cache", CacheAndDbUseToString(m_UseCache));
    json.SetString("client id", m_ClientId);
    json.SetInteger("send blob if small", m_SendBlobIfSmall);
    json.SetInteger("hops", m_Hops);
}


CJsonNode SPSGS_BlobBySeqIdRequest::Serialize(void) const
{
    CJsonNode       json(CJsonNode::NewObjectNode());

    json.SetString("request name", GetName());
    json.SetString("seq id", m_SeqId);
    json.SetInteger("seq id type", m_SeqIdType);

    CJsonNode   exclude_blobs(CJsonNode::NewArrayNode());
    for (const auto &  blob_id : m_ExcludeBlobs) {
        exclude_blobs.AppendString(blob_id);
    }
    json.SetByKey("exclude blobs", exclude_blobs);

    json.SetString("subst option", AccSubstitutioOptionToString(m_AccSubstOption));
    json.SetInteger("resend timeout mks", m_ResendTimeoutMks);

    SPSGS_BlobRequestBase::AppendCommonParameters(json);
    SPSGS_RequestBase::AppendCommonParameters(json);
    return json;
}


CJsonNode SPSGS_BlobBySatSatKeyRequest::Serialize(void) const
{
    CJsonNode       json(CJsonNode::NewObjectNode());

    json.SetString("request name", GetName());
    json.SetString("blob id", m_BlobId.GetId());
    json.SetInteger("last modified", m_LastModified);

    SPSGS_BlobRequestBase::AppendCommonParameters(json);
    SPSGS_RequestBase::AppendCommonParameters(json);
    return json;
}


CJsonNode SPSGS_AnnotRequest::Serialize(void) const
{
    CJsonNode       json(CJsonNode::NewObjectNode());

    json.SetString("request name", GetName());
    json.SetString("seq id", m_SeqId);
    json.SetInteger("seq id type", m_SeqIdType);

    CJsonNode   names(CJsonNode::NewArrayNode());
    for (const auto &  name : m_Names) {
        names.AppendString(name);
    }
    json.SetByKey("names", names);

    json.SetInteger("resend timeout mks", m_ResendTimeoutMks);

    CJsonNode   seq_ids(CJsonNode::NewArrayNode());
    for (const auto &  seq_id : m_SeqIds) {
        seq_ids.AppendString(seq_id);
    }
    json.SetByKey("seq ids", seq_ids);

    SPSGS_BlobRequestBase::AppendCommonParameters(json);
    SPSGS_RequestBase::AppendCommonParameters(json);
    return json;
}


CJsonNode SPSGS_AccessionVersionHistoryRequest::Serialize(void) const
{
    CJsonNode       json(CJsonNode::NewObjectNode());

    json.SetString("request name", GetName());
    json.SetString("seq id", m_SeqId);
    json.SetInteger("seq id type", m_SeqIdType);
    json.SetString("use cache", CacheAndDbUseToString(m_UseCache));
    json.SetInteger("hops", m_Hops);

    SPSGS_RequestBase::AppendCommonParameters(json);
    return json;
}


CJsonNode SPSGS_IPGResolveRequest::Serialize(void) const
{
    CJsonNode       json(CJsonNode::NewObjectNode());

    json.SetString("request name", GetName());
    if (m_Protein.has_value())
        json.SetString("protein", m_Protein.value());
    else
        json.SetString("protein", "<null>");
    json.SetInteger("ipg", m_IPG);
    if (m_Nucleotide.has_value())
        json.SetString("nucleotide", m_Nucleotide.value());
    else
        json.SetString("nucleotide", "<null>");
    json.SetString("use cache", CacheAndDbUseToString(m_UseCache));
    json.SetBoolean("seq id resolve", m_SeqIdResolve);

    SPSGS_RequestBase::AppendCommonParameters(json);
    return json;
}


// If the name has already been processed then it returns a priority of
// the processor which did it.
// If the name is new to the list then returns kUnknownPriority
TProcessorPriority
SPSGS_AnnotRequest::RegisterProcessedName(TProcessorPriority  priority,
                                          const string &  name)
{
    TProcessorPriority      ret = kUnknownPriority;
    CSpinlockGuard          guard(&m_Lock);

    for (auto &  item : m_Processed) {
        if (item.second == name) {
            ret = max(ret, item.first);
        }
    }

    // Add to the list regardless if it was in the list or not
    m_Processed.push_back(make_pair(priority, name));

    return ret;
}


TProcessorPriority
SPSGS_AnnotRequest::RegisterBioseqInfo(TProcessorPriority  priority)
{
    CSpinlockGuard          guard(&m_Lock);
    TProcessorPriority      ret = m_ProcessedBioseqInfo;
    m_ProcessedBioseqInfo = max(m_ProcessedBioseqInfo, priority);
    return ret;
}


// The names could be processed by the other processors which priority is
// higher (or equal) than the given. Those names should not be provided.
vector<string>
SPSGS_AnnotRequest::GetNotProcessedName(TProcessorPriority  priority)
{
    vector<string>      ret = m_Names;

    CSpinlockGuard      guard(&m_Lock);
    for (const auto &  item : m_Processed) {
        if (item.first >= priority) {
            auto    it = find(ret.begin(), ret.end(), item.second);
            if (it != ret.end()) {
                ret.erase(it);
            }
        }
    }
    return ret;
}


bool SPSGS_AnnotRequest::WasSent(const string &  annot_name) const
{
    for (const auto &  item : m_Processed) {
        if (item.second == annot_name)
            return true;
    }
    return false;
}


vector<pair<TProcessorPriority, string>>
SPSGS_AnnotRequest::GetProcessedNames(void) const
{
    CSpinlockGuard      guard(&m_Lock);
    return m_Processed;
}


void
SPSGS_AnnotRequest::ReportResultStatus(const string &  annot_name,
                                       EPSGS_ResultStatus  rs)
{
    CSpinlockGuard      guard(&m_Lock);

    switch (rs) {
        case ePSGS_RS_NotFound:
            m_NotFound.insert(annot_name);
            break;
        case ePSGS_RS_Error:
        case ePSGS_RS_Timeout:
        case ePSGS_RS_Unavailable:
            {
                auto it = m_ErrorAnnotations.find(annot_name);
                if (it == m_ErrorAnnotations.end()) {
                    m_ErrorAnnotations[annot_name] = int(rs);
                } else {
                    if (it->second < int(rs)) {
                        it->second = int(rs);
                    }
                }
            }
            break;
    }
}


void  SPSGS_AnnotRequest::ReportBlobError(TProcessorPriority  priority,
                                          EPSGS_ResultStatus  rs)
{
    if (m_Names.size() != 0) {
        // This kind of error report may happened only in the case when exactly
        // one annotation was requested
        return;
    }

    CSpinlockGuard      guard(&m_Lock);

    // Mark specifically this processor annotation as the one with an error
    // i.e. move it from 'ok' to the corresponding list
    for (auto  it = m_Processed.begin(); it != m_Processed.end(); ++it) {
        if (it->first == priority && it->second == m_Names[0]) {
            m_Processed.erase(it);

            switch (rs) {
                case ePSGS_RS_NotFound:
                    // Strictly speaking this must not happened.
                    // The blob error reporting is for errors.
                    m_NotFound.insert(m_Names[0]);
                    break;
                case ePSGS_RS_Error:
                case ePSGS_RS_Timeout:
                case ePSGS_RS_Unavailable:
                    // The ePSGS_RS_Unavailable should not really happened
                    // because it is for the infrastructure to report the
                    // status of a named annotation while this method is for
                    // the processors.
                    {
                        auto it = m_ErrorAnnotations.find(m_Names[0]);
                        if (it == m_ErrorAnnotations.end()) {
                            m_ErrorAnnotations[m_Names[0]] = int(rs);
                        } else {
                            if (it->second < int(rs)) {
                                it->second = int(rs);
                            }
                        }
                    }
                    break;
            }
            break;
        }
    }
}


CJsonNode SPSGS_TSEChunkRequest::Serialize(void) const
{
    CJsonNode       json(CJsonNode::NewObjectNode());

    json.SetString("request name", GetName());
    json.SetInteger("id2 chunk", m_Id2Chunk);
    json.SetString("id2 info", m_Id2Info);
    json.SetString("use cache", CacheAndDbUseToString(m_UseCache));

    SPSGS_RequestBase::AppendCommonParameters(json);
    return json;
}

