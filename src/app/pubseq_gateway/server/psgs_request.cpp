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
    while (s_RequestIdLock.exchange(true)) {}   // acquire lock
    auto request_id = ++s_NextRequestId;
    s_RequestIdLock = false;                    // release lock
    return request_id;
}


SPSGS_BlobId::SPSGS_BlobId(int32_t  sat, int32_t  sat_key)
{
    char    buf[64];
    long    len;
    len = PSGToString(sat, buf);
    m_Id.append(buf, len)
        .append(1, '.');
    len = PSGToString(sat_key, buf);
    m_Id.append(buf, len);
}


CPSGS_Request::CPSGS_Request() :
    m_RequestId(0),
    m_ConcurrentProcessorCount(0)
{}


CPSGS_Request::~CPSGS_Request()
{
    for (auto it: m_Wait) {
        switch (it.second->m_State) {
            case SWaitData::ePSGS_Unlocked:
            case SWaitData::ePSGS_LockedNobodyWaits:
                delete it.second;
                break;
            case SWaitData::ePSGS_LockedOneWaits:
                // This is rather strange: a processor is still waiting on the
                // condition variable and at the same time the request is going
                // to be destroyed.
                // Just in case, lets unlock the condition variable and then
                // delete the associated data
                PSG_ERROR("Reply is going to be deleted when a processor is "
                          "locked on a condition variable");
                it.second->m_WaitObject.notify_one();
                delete it.second;
                break;
        }
    }
}


CPSGS_Request::CPSGS_Request(unique_ptr<SPSGS_RequestBase> req,
                             CRef<CRequestContext>  request_context) :
    m_Request(move(req)),
    m_RequestContext(request_context),
    m_RequestId(GetNextRequestId()),
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
        case SWaitData::ePSGS_LockedOneWaits:
            it->second->m_State = SWaitData::ePSGS_Unlocked;
            it->second->m_WaitObject.notify_one();
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
            // The logic to initiate waiting is below
            break;
        case SWaitData::ePSGS_Unlocked:
            // It has already been unlocked
            return;
        case SWaitData::ePSGS_LockedOneWaits:
            NCBI_THROW(CPubseqGatewayException, eLogic,
                       "Multiple waiting of the same event is not supported");
    }

    it->second->m_State = SWaitData::ePSGS_LockedOneWaits;
    auto    status = it->second->m_WaitObject.wait_for(scope_lock,
                                                       chrono::seconds(timeout_sec));
    if (status == cv_status::timeout) {
        // Still locked
        it->second->m_State = SWaitData::ePSGS_LockedNobodyWaits;

        string  message = "Timeout (" + to_string(timeout_sec) +
                          " seconds) waiting on event '" + event_name + "'";

        PSG_WARNING(message);
        NCBI_THROW(CPubseqGatewayException, eTimeout, message);
    }

    // Here:
    // - scope_lock mutex is released inside wait_for()
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
    if (m_RequestContext.NotNull()) {
        CDiagContext::SetRequestContext(m_RequestContext->Clone());
        CDiagContext::GetRequestContext().SetReadOnly(false);
    }
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


string CPSGS_Request::GetName(void) const
{
    if (m_Request)
        return m_Request->GetName();
    return "unknown (request is not initialized)";
}


CJsonNode CPSGS_Request::Serialize(void) const
{
    if (m_Request)
        return m_Request->Serialize();

    CJsonNode       json(CJsonNode::NewObjectNode());
    json.SetString("name", GetName());
    return json;
}


CJsonNode SPSGS_ResolveRequest::Serialize(void) const
{
    CJsonNode       json(CJsonNode::NewObjectNode());

    json.SetString("name", GetName());
    json.SetString("seq id", m_SeqId);
    json.SetInteger("seq id type", m_SeqIdType);
    json.SetBoolean("trace", m_Trace);
    json.SetInteger("include data flags", m_IncludeDataFlags);
    json.SetInteger("output format", m_OutputFormat);
    json.SetBoolean("use cache", m_UseCache);
    json.SetInteger("subst option", m_AccSubstOption);
    return json;
}


CJsonNode SPSGS_BlobBySeqIdRequest::Serialize(void) const
{
    CJsonNode       json(CJsonNode::NewObjectNode());

    json.SetString("name", GetName());
    json.SetString("seq id", m_SeqId);
    json.SetInteger("seq id type", m_SeqIdType);
    json.SetInteger("tse option", m_TSEOption);
    json.SetBoolean("use cache", m_UseCache);
    json.SetString("client id", m_ClientId);
    json.SetBoolean("trace", m_Trace);
    json.SetInteger("subst option", m_AccSubstOption);

    CJsonNode   exclude_blobs(CJsonNode::NewArrayNode());
    for (const auto &  blob_id : m_ExcludeBlobs) {
        exclude_blobs.AppendString(blob_id);
    }
    json.SetByKey("exclude blobs", exclude_blobs);

    return json;
}


CJsonNode SPSGS_BlobBySatSatKeyRequest::Serialize(void) const
{
    CJsonNode       json(CJsonNode::NewObjectNode());

    json.SetString("name", GetName());
    json.SetString("blob id", m_BlobId.GetId());
    json.SetInteger("tse option", m_TSEOption);
    json.SetBoolean("use cache", m_UseCache);
    json.SetString("client id", m_ClientId);
    json.SetBoolean("trace", m_Trace);
    json.SetInteger("last modified", m_LastModified);
    return json;
}


CJsonNode SPSGS_AnnotRequest::Serialize(void) const
{
    CJsonNode       json(CJsonNode::NewObjectNode());

    json.SetString("name", GetName());
    json.SetString("seq id", m_SeqId);
    json.SetInteger("seq id type", m_SeqIdType);
    json.SetBoolean("trace", m_Trace);
    json.SetBoolean("use cache", m_UseCache);
    json.SetInteger("tse option", m_TSEOption);

    CJsonNode   names(CJsonNode::NewArrayNode());
    for (const auto &  name : m_Names) {
        names.AppendString(name);
    }
    json.SetByKey("names", names);

    return json;
}


CJsonNode SPSGS_AccessionVersionHistoryRequest::Serialize(void) const
{
    CJsonNode       json(CJsonNode::NewObjectNode());

    json.SetString("name", GetName());
    json.SetString("seq id", m_SeqId);
    json.SetInteger("seq id type", m_SeqIdType);
    json.SetBoolean("trace", m_Trace);
    json.SetBoolean("use cache", m_UseCache);

    return json;
}


// If the name has already been processed then it returns a priority of
// the processor which did it.
// If the name is new to the list then returns kUnknownPriority
// The highest priority will be stored together with the name.
TProcessorPriority
SPSGS_AnnotRequest::RegisterProcessedName(TProcessorPriority  priority,
                                          const string &  name)
{
    TProcessorPriority      ret = kUnknownPriority;

    while (m_Lock.exchange(true)) {}        // acquire lock

    for (auto &  item : m_Processed) {
        if (item.second == name) {
            ret = item.first;
            item.first = max(item.first, priority);
            break;
        }
    }

    if (ret == kUnknownPriority) {
        // Not found => add
        m_Processed.push_back(make_pair(priority, name));
    }

    m_Lock = false;                         // release lock
    return ret;
}


TProcessorPriority
SPSGS_AnnotRequest::RegisterBioseqInfo(TProcessorPriority  priority)
{
    while (m_Lock.exchange(true)) {}        // acquire lock
    TProcessorPriority      ret = m_ProcessedBioseqInfo;
    m_ProcessedBioseqInfo = max(m_ProcessedBioseqInfo, priority);
    m_Lock = false;                         // release lock
    return ret;
}


// The names could be processed by the other processors which priority is
// higher (or equal) than the given. Those names should not be provided.
vector<string>
SPSGS_AnnotRequest::GetNotProcessedName(TProcessorPriority  priority)
{
    vector<string>      ret = m_Names;

    while (m_Lock.exchange(true)) {}        // acquire lock
    for (const auto &  item : m_Processed) {
        if (item.first >= priority) {
            auto    it = find(ret.begin(), ret.end(), item.second);
            if (it != ret.end()) {
                ret.erase(it);
            }
        }
    }
    m_Lock = false;                         // release lock
    return ret;
}


vector<pair<TProcessorPriority, string>>
SPSGS_AnnotRequest::GetProcessedNames(void) const
{
    while (m_Lock.exchange(true)) {}        // acquire lock
    auto    ret = m_Processed;
    m_Lock = false;                         // release lock
    return ret;
}


CJsonNode SPSGS_TSEChunkRequest::Serialize(void) const
{
    CJsonNode       json(CJsonNode::NewObjectNode());

    json.SetString("name", GetName());
    json.SetInteger("id2 chunk", m_Id2Chunk);
    json.SetString("id2 info", m_Id2Info);
    json.SetBoolean("use cache", m_UseCache);
    json.SetBoolean("trace", m_Trace);
    return json;
}

