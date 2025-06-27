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
 * File Description: base class for processors which may generate cassandra
 *                   fetches
 *
 */

#include <ncbi_pch.hpp>

#include <corelib/request_status.hpp>
#include <corelib/ncbidiag.hpp>

#include "cass_processor_base.hpp"
#include "pubseq_gateway.hpp"


CPSGS_CassProcessorBase::CPSGS_CassProcessorBase() :
    m_Canceled(false),
    m_Unlocked(false),
    // e100_Continue means that the processor has not updated the status yet
    // explicitly
    m_Status(CRequestStatus::e100_Continue)
{}


CPSGS_CassProcessorBase::CPSGS_CassProcessorBase(
                                            shared_ptr<CPSGS_Request> request,
                                            shared_ptr<CPSGS_Reply> reply,
                                            TProcessorPriority  priority) :
    m_Canceled(false),
    m_Unlocked(false),
    // e100_Continue means that the processor has not updated the status yet
    // explicitly
    m_Status(CRequestStatus::e100_Continue)
{
    IPSGS_Processor::m_Request = request;
    IPSGS_Processor::m_Reply = reply;
    IPSGS_Processor::m_Priority = priority;
}


CPSGS_CassProcessorBase::~CPSGS_CassProcessorBase()
{
    UnlockWaitingProcessor();
}


void CPSGS_CassProcessorBase::Cancel(void)
{
    // The other processors may wait on a lock
    UnlockWaitingProcessor();

    m_Canceled = true;
    CancelLoaders();
}


NCBI_PARAM_DECL(int, CASSANDRA_PROCESSOR, ERROR_RATE);
NCBI_PARAM_DEF(int, CASSANDRA_PROCESSOR, ERROR_RATE, 0);

void CPSGS_CassProcessorBase::SignalFinishProcessing(void)
{
    // It is safe to unlock the request many times
    UnlockWaitingProcessor();

    if (!AreAllFinishedRead() || !IsMyNCBIFinished()) {
        // Cannot really report finish because there could be still not
        // finished cassandra fetches. In this case there will be an async
        // event which will lead to another SignalFinishProcessing() call and
        // it will go further.
        return;
    }

    static int error_rate = NCBI_PARAM_TYPE(CASSANDRA_PROCESSOR, ERROR_RATE)::GetDefault();
    if ( error_rate > 0 && !m_FinishSignalled ) {
        static int error_counter = 0;
        if ( ++error_counter >= error_rate ) {
            error_counter = 0;
            m_Status = CRequestStatus::e500_InternalServerError;
        }
    }

    if (m_Status == CRequestStatus::e100_Continue) {
        // That means the processor has not updated the status explicitly and
        // it actually means everything is fine, i.e. 200.
        // Otherwise the processor would update the status explicitly using the
        // UpdateOverallStatus() method.
        // So to report the finishing status properly in response to the
        // GetStatus() call, the status needs to be updated to 200
        m_Status = CRequestStatus::e200_Ok;
    }
    IPSGS_Processor::SignalFinishProcessing();
}


void CPSGS_CassProcessorBase::UnlockWaitingProcessor(void)
{
    if (!m_Unlocked) {
        m_Unlocked = true;
        if (IPSGS_Processor::m_Request) {
            IPSGS_Processor::m_Request->Unlock(kCassandraProcessorEvent);
        }
    }
}


IPSGS_Processor::EPSGS_Status CPSGS_CassProcessorBase::GetStatus(void)
{
    if (m_Canceled)
        return IPSGS_Processor::ePSGS_Canceled;

    if (m_Status == CRequestStatus::e100_Continue)
        return IPSGS_Processor::ePSGS_InProgress;

    if (m_Status == CRequestStatus::e504_GatewayTimeout)
        return IPSGS_Processor::ePSGS_Timeout;

    // 300 never actually happens
    if (m_Status < 300)
        return IPSGS_Processor::ePSGS_Done;
    if (m_Status < 500) {
        if (m_Status == CRequestStatus::e401_Unauthorized) {
            return IPSGS_Processor::ePSGS_Unauthorized;
        }
        return IPSGS_Processor::ePSGS_NotFound;
    }
    return IPSGS_Processor::ePSGS_Error;
}


void CPSGS_CassProcessorBase::EnforceWait(void) const
{
    if (!m_FetchDetails.empty()) {
        for (const auto &  details: m_FetchDetails) {
            if (details) {
                if (details->Canceled()) {
                } else {
                    if (details->ReadFinished()) {
                    } else {
                        details->GetLoader()->Wait();
                    }
                }
            } else {
            }
        }
    }
}

string CPSGS_CassProcessorBase::GetVerboseFetches(void) const
{
    string      ret;
    ret = "Total fetches: " + to_string(m_FetchDetails.size());

    if (!m_FetchDetails.empty()) {
        for (const auto &  details: m_FetchDetails) {
            ret += "\n";
            if (details) {
                ret += details->Serialize() + " ";
                if (details->Canceled()) {
                    ret += "Canceled";
                } else {
                    if (details->ReadFinished()) {
                        ret += "Read has finished";
                    } else {
                        ret += "Read has not finished";
                    }
                }
            } else {
                ret += "No more details available";
            }
        }
    }
    return ret;
}


bool CPSGS_CassProcessorBase::AreAllFinishedRead(void) const
{
    for (const auto &  details: m_FetchDetails) {
        if (details) {
            if (!details->Canceled()) {
                if (!details->ReadFinished()) {
                    return false;
                }
            }
        }
    }
    return true;
}


void call_on_data_cb(void *  user_data)
{
    CPSGS_CassProcessorBase *   proc = (CPSGS_CassProcessorBase*)(user_data);
    proc->CallOnData();
}


void CPSGS_CassProcessorBase::CallOnData(void)
{
    auto p = IPSGS_Processor::m_Reply->GetDataReadyCB();
    p->OnData();
}


void call_processor_process_event_cb(void *  user_data)
{
    CPSGS_CassProcessorBase *   proc = (CPSGS_CassProcessorBase*)(user_data);
    proc->ProcessEvent();
}


void CPSGS_CassProcessorBase::CancelLoaders(void)
{
    for (auto &  loader: m_FetchDetails) {
        if (loader) {
            if (!loader->ReadFinished()) {
                loader->Cancel();
                loader->RemoveFromExcludeBlobCache();

                // Imitate the Cassandra data ready event. This will eventually
                // lead to a pending operation Peek() call which in turn calls
                // Wait() for the loader. The Wait() return value should say in
                // this case that there will be nothing else.
                // However, do not do that synchronously. Do the call in the
                // next iteration of libuv loop associated with the processor
                // so that it is safe even if Cancel() is done from another
                // loop
                PostponeInvoke(call_on_data_cb, (void*)(this));
            }
        }
    }

    // Sometimes there are no loaders which have not finished or the OnData()
    // does not lead to a process event call of the processor due to a data
    // trigger.
    // This may lead to a not called SignalFinishProcessing() for a processor.
    // On the other hand the ProcessEvent() is innocent in terms of data
    // processing however will check the cancel status as needed.
    // So shedule ProcessEvent() for later.
    PostponeInvoke(call_processor_process_event_cb, (void*)(this));
}


void
CPSGS_CassProcessorBase::UpdateOverallStatus(CRequestStatus::ECode  status)
{
    m_Status = max(status, m_Status);
}


static string   kCassandra = "cassandra";

bool
CPSGS_CassProcessorBase::IsCassandraProcessorEnabled(
                                    shared_ptr<CPSGS_Request> request) const
{
    // CXX-11549: for the time being the processor name does not participate in
    //            the decision. Only the hardcoded "cassandra" is searched in
    //            the enable/disable list.
    //            Also, what list is consulted depends on a configuration file
    //            setting which tells whether the cassandra processors are
    //            enabled or not. This leads to a nice case when looking at the
    //            request it is impossible to say if a processor was enabled or
    //            disabled at the time of request if enable/disable options are
    //            not in the request.

    auto *      app = CPubseqGatewayApp::GetInstance();
    bool        enabled = app->Settings().m_CassandraProcessorsEnabled;

    if (enabled) {
        for (const auto &  dis_processor :
                request->GetRequest<SPSGS_RequestBase>().m_DisabledProcessors) {
            if (NStr::CompareNocase(dis_processor, kCassandra) == 0) {
                return false;
            }
        }
    } else {
        for (const auto &  en_processor :
                request->GetRequest<SPSGS_RequestBase>().m_EnabledProcessors) {
            if (NStr::CompareNocase(en_processor, kCassandra) == 0) {
                return true;
            }
        }
    }

    return enabled;
}



SCass_BlobId
CPSGS_CassProcessorBase::TranslateSatToKeyspace(CBioseqInfoRecord::TSat  sat,
                                                CBioseqInfoRecord::TSatKey  sat_key,
                                                const string &  seq_id)
{
    SCass_BlobId    blob_id(sat, sat_key);

    if (blob_id.MapSatToKeyspace()) {
        // All good, the translation has been done successfully
        return blob_id;
    }

    // No translation => send an error message
    auto *      app = CPubseqGatewayApp::GetInstance();
    size_t      item_id = IPSGS_Processor::m_Reply->GetItemId();
    string      msg = "Unknown satellite number " + to_string(blob_id.m_Sat) +
                      " for bioseq info with seq_id '" +
                      seq_id + "'";
    app->GetCounters().Increment(this,
                                 CPSGSCounters::ePSGS_ServerSatToSatNameError);

    IPSGS_Processor::m_Reply->PrepareBlobPropMessage(
        item_id, GetName(), msg, CRequestStatus::e500_InternalServerError,
        ePSGS_UnknownResolvedSatellite, eDiag_Error);
    IPSGS_Processor::m_Reply->PrepareBlobPropCompletion(item_id, GetName(), 2);

    UpdateOverallStatus(CRequestStatus::e500_InternalServerError);
    PSG_ERROR(msg);

    CPSGS_CassProcessorBase::SignalFinishProcessing();

    // Return invalid blob id
    return SCass_BlobId();
}


bool CPSGS_CassProcessorBase::IsTimeoutError(const string &  msg) const
{
    string      msg_lower = msg;
    NStr::ToLower(msg_lower);

    return msg_lower.find("timeout") != string::npos;
}


bool CPSGS_CassProcessorBase::IsTimeoutError(int  code) const
{
    return code == CCassandraException::eQueryTimeout;
}


bool CPSGS_CassProcessorBase::IsError(EDiagSev  severity) const
{
    return (severity == eDiag_Error ||
            severity == eDiag_Critical ||
            severity == eDiag_Fatal);
}


CRequestStatus::ECode
CPSGS_CassProcessorBase::CountError(CCassFetch *  fetch_details,
                                    CRequestStatus::ECode  status,
                                    int  code,
                                    EDiagSev  severity,
                                    const string &  message,
                                    EPSGS_LoggingFlag  logging_flag,
                                    EPSGS_StatusUpdateFlag  status_update_flag)
{
    // Note: the 'code' may come from cassandra error handlers and from the PSG
    // internal logic code. It is safe to compare the codes against particular
    // values because they range quite different. The Cassandra codes start
    // from 2000 and the PSG codes start from 300

    EPSGS_DbFetchType       fetch_type = ePSGS_UnknownFetch;
    string                  message_prefix;
    if (fetch_details != nullptr) {
        fetch_type = fetch_details->GetFetchType();
        message_prefix = "Fetch context: " + fetch_details->Serialize() + "\n";
    }


    // It could be a message or an error
    bool    is_error = IsError(severity);

    if (logging_flag == ePSGS_NeedLogging) {
        if (is_error) {
            PSG_ERROR(message_prefix + message);
        } else {
            PSG_WARNING(message_prefix + message);
        }
    }

    if (m_Request->NeedTrace()) {
        m_Reply->SendTrace(message_prefix +
                           "Error detected. Status: " + to_string(status) +
                           " Code: " + to_string(code) +
                           " Severity: " + string(CNcbiDiag::SeverityName(severity)) +
                           " Message: " + message,
                           m_Request->GetStartTimestamp());
    }

    auto *  app = CPubseqGatewayApp::GetInstance();
    if (IsTimeoutError(code)) {
        app->GetCounters().Increment(this,
                                     CPSGSCounters::ePSGS_CassQueryTimeoutError);
        if (status_update_flag == ePSGS_NeedStatusUpdate) {
            UpdateOverallStatus(CRequestStatus::e504_GatewayTimeout);
        }
        return CRequestStatus::e504_GatewayTimeout;
    }

    if (status == CRequestStatus::e404_NotFound) {
        switch (fetch_type) {
            case ePSGS_BlobBySeqIdFetch:
                app->GetCounters().Increment(this,
                                             CPSGSCounters::ePSGS_GetBlobNotFound);
                break;
            case ePSGS_BlobBySatSatKeyFetch:
                app->GetCounters().Increment(this,
                                             CPSGSCounters::ePSGS_GetBlobNotFound);
                break;
            case ePSGS_AnnotationFetch:
                app->GetCounters().Increment(this,
                                             CPSGSCounters::ePSGS_AnnotationNotFound);
                break;
            case ePSGS_AnnotationBlobFetch:
                app->GetCounters().Increment(this,
                                             CPSGSCounters::ePSGS_AnnotationBlobNotFound);
                break;
            case ePSGS_TSEChunkFetch:
                app->GetCounters().Increment(this,
                                             CPSGSCounters::ePSGS_TSEChunkNotFound);
                break;
            case ePSGS_BioseqInfoFetch:
                app->GetCounters().Increment(this,
                                             CPSGSCounters::ePSGS_BioseqInfoNotFound);
                break;
            case ePSGS_Si2csiFetch:
                app->GetCounters().Increment(this,
                                             CPSGSCounters::ePSGS_Si2csiNotFound);
                break;
            case ePSGS_SplitHistoryFetch:
                app->GetCounters().Increment(this,
                                             CPSGSCounters::ePSGS_SplitHistoryNotFound);
                break;
            case ePSGS_PublicCommentFetch:
                app->GetCounters().Increment(this,
                                             CPSGSCounters::ePSGS_PublicCommentNotFound);
                break;
            case ePSGS_AccVerHistoryFetch:
                app->GetCounters().Increment(this,
                                             CPSGSCounters::ePSGS_AccVerHistoryNotFound);
                break;
            case ePSGS_IPGResolveFetch:
                app->GetCounters().Increment(this,
                                             CPSGSCounters::ePSGS_IPGResolveNotFound);
                break;
            case ePSGS_UnknownFetch:
                break;
        }

        if (status_update_flag == ePSGS_NeedStatusUpdate) {
            UpdateOverallStatus(CRequestStatus::e404_NotFound);
        }
        return CRequestStatus::e404_NotFound;
    }

    if (is_error) {
        if (fetch_type != ePSGS_UnknownFetch) {
            app->GetCounters().Increment(this,
                                         CPSGSCounters::ePSGS_ProcUnknownError);
        }
        if (status_update_flag == ePSGS_NeedStatusUpdate) {
            UpdateOverallStatus(status);
        }
    }

    return status;
}


bool CPSGS_CassProcessorBase::IsMyNCBIFinished(void) const
{
    if (m_WhoAmIRequest.get() == nullptr)
        return true;    // Not even started

    return m_WhoAmIRequest->GetResponseStatus() != EPSG_MyNCBIResponseStatus::eInProgress;
}



extern mutex    g_MyNCBICacheLock;

CPSGS_CassProcessorBase::EPSGS_MyNCBILookupResult
CPSGS_CassProcessorBase::PopulateMyNCBIUser(TMyNCBIDataCB  data_cb,
                                            TMyNCBIErrorCB  error_cb)
{
    if (m_UserName.has_value()) {
        // It has already been retrieved one way or another.
        // There could be only one user per request so no need to do anything
        return ePSGS_FoundInOKCache;
    }

    bool    new_client = m_Request->GetIncludeHUP().has_value();
    if (new_client) {
        if (!m_Request->GetIncludeHUP().value()) {
            // The include_hup explicitly tells 'no'
            ReportExplicitIncludeHUPSetToNo();
            return ePSGS_IncludeHUPSetToNo;
        }
    }


    // Check for the cookie
    m_MyNCBICookie = IPSGS_Processor::m_Request->GetWebCubbyUser();
    if (!m_MyNCBICookie.has_value()) {
        ReportNoWebCubbyUser();
        return ePSGS_CookieNotPresent;
    }

    // Cookie is here. Check all three caches (error, not found and ok)

    string  cookie = m_MyNCBICookie.value();
    bool    need_trace = m_Request->NeedTrace();
    auto    app = CPubseqGatewayApp::GetInstance();

    // Since there are 3 caches to check, that should be done under a mutex
    g_MyNCBICacheLock.lock();

    // Check first the error cache
    auto    err_cache_ret = app->GetMyNCBIErrorCache()->GetError(cookie);
    if (err_cache_ret.has_value()) {
        g_MyNCBICacheLock.unlock();

        // Found in error cache; invoke a callback for errors and return saying
        // no further actions are required.
        if (need_trace) {
            m_Reply->SendTrace("MyNCBI cookie was found in the error cache: " +
                               cookie + ". Call the error callback right away.",
                               m_Request->GetStartTimestamp());
        }
        error_cb(cookie, err_cache_ret.value().m_Status,
                 err_cache_ret.value().m_Code,
                 err_cache_ret.value().m_Severity,
                 err_cache_ret.value().m_Message);

        return ePSGS_FoundInErrorCache;
    }

    auto  not_found_cache_ret = app->GetMyNCBINotFoundCache()->IsNotFound(cookie);
    if (not_found_cache_ret) {
        g_MyNCBICacheLock.unlock();

        // Found in not found cache; invoke a callback for not found and return saying
        // no further actions are required.
        if (need_trace) {
            m_Reply->SendTrace("MyNCBI cookie was found in the not found cache: " +
                               cookie + ". Call the error callback right away.",
                               m_Request->GetStartTimestamp());
        }

        // Error callback is used for not found case as well. The 404 status is
        // the differentiating factor
        error_cb(cookie, CRequestStatus::e404_NotFound,
                 -1, eDiag_Error, "");

        return ePSGS_FoundInNotFoundCache;
    }

    // Check the OK cache. If found then the item can be in READY or INPROGRESS
    // state.

    // Note: the passed callbacks are used for the wait list if the request is
    // in progress. The callbacks are the same for both cases:
    // - called by my ncbi wrapper in responce to the request
    // - called from the cache when a wait list is checked
    // In both cases callbacks are called asynchronously.
    auto  user_info = app->GetMyNCBIOKCache()->GetUserInfo(this, cookie,
                                                           data_cb, error_cb);
    g_MyNCBICacheLock.unlock();

    if (user_info.has_value()) {
        if (user_info.value().m_Status == SMyNCBIOKCacheItem::ePSGS_Ready) {
            // The data are ready to use
            if (need_trace) {
                m_Reply->SendTrace("MyNCBI user info found in OK cache "
                                   "in Ready state for cookie: " + cookie +
                                   ". User name: " + user_info.value().m_UserInfo.username,
                                   m_Request->GetStartTimestamp());
            }

            m_UserName = user_info.value().m_UserInfo.username;
            return ePSGS_FoundInOKCache;
        }

        // Here: status == SMyNCBIOKCacheItem::ePSGS_InProgress
        // That means the caller is put into a wait list. Thus there is nothing
        // else to do; just wait for a callback
        if (need_trace) {
            m_Reply->SendTrace("MyNCBI cookie found in OK cache "
                               "in InProgress state for cookie: " + cookie +
                               ". We are in a wait list now.",
                               m_Request->GetStartTimestamp());
        }
        return ePSGS_AddedToWaitlist;
    }


    // Cookie has not been found in any of the caches so initiate a my ncbi request
    uv_loop_t *     loop = app->GetUVLoop();

    if (need_trace) {
        m_Reply->SendTrace("Initiating MyNCBI request for cookie: " + cookie,
                           m_Request->GetStartTimestamp());
    }

    m_WhoAmIRequest =
        app->GetMyNCBIFactory()->CreateWhoAmI(loop, cookie,
                                              CMyNCBIDataCallback(this, data_cb, cookie),
                                              CMyNCBIErrorCallback(this, error_cb, cookie));
    return ePSGS_RequestInitiated;
}


void CPSGS_CassProcessorBase::ReportNoWebCubbyUser(void)
{
    auto    app = CPubseqGatewayApp::GetInstance();
    app->GetCounters().Increment(this,
                                 CPSGSCounters::ePSGS_NoWebCubbyUserCookie);

    string  err_msg = GetName() + " processor: ";
    if (IPSGS_Processor::m_Request->GetIncludeHUP().has_value()) {
        err_msg += "HUP retrieval is allowed (explicitly set to 'yes') but auth token is missing";
    } else {
        err_msg += "HUP retrieval is allowed (implicitly) but auth token is missing";
    }

    IPSGS_Processor::m_Reply->PrepareProcessorMessage(
            IPSGS_Processor::m_Reply->GetItemId(), GetName(),
            err_msg, CRequestStatus::e401_Unauthorized,
            ePSGS_NoWebCubbyUserCookieError, eDiag_Warning);
    UpdateOverallStatus(CRequestStatus::e401_Unauthorized);
    PSG_NOTE(err_msg);
}


void CPSGS_CassProcessorBase::ReportExplicitIncludeHUPSetToNo(void)
{
    auto    app = CPubseqGatewayApp::GetInstance();
    app->GetCounters().Increment(this,
                                 CPSGSCounters::ePSGS_IncludeHUPSetToNo);

    string  err_msg = GetName() + " processor: ";
    if (IPSGS_Processor::m_Request->GetWebCubbyUser().has_value()) {
        err_msg += "HUP retrieval is disallowed but auth token is present";
    } else {
        err_msg += "HUP retrieval is disallowed and auth token is missing";
    }

    IPSGS_Processor::m_Reply->PrepareProcessorMessage(
            IPSGS_Processor::m_Reply->GetItemId(), GetName(),
            err_msg, CRequestStatus::e401_Unauthorized,
            ePSGS_NoWebCubbyUserCookieError, eDiag_Warning);
    UpdateOverallStatus(CRequestStatus::e401_Unauthorized);
    PSG_NOTE(err_msg);
}


void CPSGS_CassProcessorBase::ReportMyNCBIError(CRequestStatus::ECode  status,
                                                const string &  my_ncbi_message)
{
    auto    app = CPubseqGatewayApp::GetInstance();
    app->GetCounters().Increment(this,
                                 CPSGSCounters::ePSGS_MyNCBIErrorCounter);

    string  err_msg = GetName() + " processor received an error from MyNCBI "
                      "while checking permissions for a Cassandra secure keyspace. Message: " +
                      my_ncbi_message;
    IPSGS_Processor::m_Reply->PrepareProcessorMessage(
            IPSGS_Processor::m_Reply->GetItemId(), GetName(),
            err_msg, status,
            ePSGS_MyNCBIError, eDiag_Error);
    UpdateOverallStatus(status);
    PSG_ERROR(err_msg);
}


void CPSGS_CassProcessorBase::ReportMyNCBINotFound(void)
{
    auto    app = CPubseqGatewayApp::GetInstance();
    app->GetCounters().Increment(this,
                                 CPSGSCounters::ePSGS_MyNCBINotFoundCounter);

    string  err_msg = GetName() + " processor received a not found report from MyNCBI "
                      "while checking permissions for a Cassandra secure keyspace.";
    IPSGS_Processor::m_Reply->PrepareProcessorMessage(
            IPSGS_Processor::m_Reply->GetItemId(), GetName(),
            err_msg, CRequestStatus::e401_Unauthorized,
            ePSGS_MyNCBINotFound, eDiag_Warning);
    UpdateOverallStatus(CRequestStatus::e401_Unauthorized);
    PSG_NOTE(err_msg);
}


void CPSGS_CassProcessorBase::ReportFailureToGetCassConnection(void)
{
    auto    app = CPubseqGatewayApp::GetInstance();
    app->GetCounters().Increment(this,
                                 CPSGSCounters::ePSGS_FailureToGetCassConnectionCounter);

    string  err_msg = GetName() +
                      " processor failed to get a Cassandra connection because of an unknown reason.";
    IPSGS_Processor::m_Reply->PrepareProcessorMessage(
            IPSGS_Processor::m_Reply->GetItemId(), GetName(),
            err_msg, CRequestStatus::e500_InternalServerError,
            ePSGS_CassConnectionError, eDiag_Error);
    UpdateOverallStatus(CRequestStatus::e500_InternalServerError);
    PSG_ERROR(err_msg);
}


void CPSGS_CassProcessorBase::ReportFailureToGetCassConnection(const string &  message)
{
    auto    app = CPubseqGatewayApp::GetInstance();
    app->GetCounters().Increment(this,
                                 CPSGSCounters::ePSGS_FailureToGetCassConnectionCounter);

    string  err_msg = GetName() +
                      " processor failed to get a Cassandra connection. Message: " + message;
    IPSGS_Processor::m_Reply->PrepareProcessorMessage(
            IPSGS_Processor::m_Reply->GetItemId(), GetName(),
            err_msg, CRequestStatus::e500_InternalServerError,
            ePSGS_CassConnectionError, eDiag_Error);
    UpdateOverallStatus(CRequestStatus::e500_InternalServerError);
    PSG_ERROR(err_msg);
}


void CPSGS_CassProcessorBase::CleanupMyNCBICache(void)
{
    // It is possible that:
    // - the processor is destroyed because of the abrupt connection dropping and
    // - the processor has initiated a myncbi request or
    // - in the wait list of the my ncbi reply
    // So the my NCBI cache must be cleaned

    // If cookie is present => it is a waiter or initiator
    if (!m_MyNCBICookie.has_value()) {
        // The processor did not do anything with my NCBI
        return;
    }

    auto    app = CPubseqGatewayApp::GetInstance();
    if (m_WhoAmIRequest.get() == nullptr) {
        // Possibly this is a processor in the wait list
        app->GetMyNCBIOKCache()->ClearWaitingProcessor(m_MyNCBICookie.value(),
                                                       this);
    } else {
        // Possibly this is a processor which initiated the request
        app->GetMyNCBIOKCache()->ClearInitiatedRequest(m_MyNCBICookie.value());
    }
}


void CPSGS_CassProcessorBase::ReportSecureSatUnauthorized(const string &  user_name)
{
    auto    app = CPubseqGatewayApp::GetInstance();
    app->GetCounters().Increment(this,
                                 CPSGSCounters::ePSGS_SecureSatUnauthorizedCounter);

    string  err_msg = GetName() + " processor: user '" + user_name + 
                      "' is not authorized to access "
                      "Cassandra secure satellite.";
    IPSGS_Processor::m_Reply->PrepareProcessorMessage(
            IPSGS_Processor::m_Reply->GetItemId(), GetName(),
            err_msg, CRequestStatus::e401_Unauthorized,
            ePSGS_SecureSatUnauthorized, eDiag_Error);
    UpdateOverallStatus(CRequestStatus::e401_Unauthorized);
    PSG_ERROR(err_msg);
}

