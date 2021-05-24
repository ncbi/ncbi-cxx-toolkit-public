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
 * Authors: Dmitri Dmitrienko
 *
 * File Description:
 *
 */
#include <ncbi_pch.hpp>

#include <corelib/ncbithr.hpp>
#include <corelib/ncbidiag.hpp>
#include <corelib/request_ctx.hpp>

#include <objtools/pubseq_gateway/impl/cassandra/blob_storage.hpp>

#include "pubseq_gateway.hpp"
#include "pubseq_gateway_exception.hpp"
#include "resolve_processor.hpp"

#include "shutdown_data.hpp"
extern SShutdownData    g_ShutdownData;


USING_NCBI_SCOPE;


static string  kFmtParam = "fmt";
static string  kBlobIdParam = "blob_id";
static string  kLastModifiedParam = "last_modified";
static string  kTSELastModifiedParam = "tse_last_modified";
static string  kSeqIdParam = "seq_id";
static string  kSeqIdTypeParam = "seq_id_type";
static string  kTSEParam = "tse";
static string  kUseCacheParam = "use_cache";
static string  kNamesParam = "names";
static string  kExcludeBlobsParam = "exclude_blobs";
static string  kClientIdParam = "client_id";
static string  kAuthTokenParam = "auth_token";
static string  kTimeoutParam = "timeout";
static string  kDataSizeParam = "return_data_size";
static string  kLogParam = "log";
static string  kUsernameParam = "username";
static string  kAlertParam = "alert";
static string  kResetParam = "reset";
static string  kTSEIdParam = "tse_id";
static string  kId2ChunkParam = "id2_chunk";
static string  kId2InfoParam = "id2_info";
static string  kAccSubstitutionParam = "acc_substitution";
static string  kAutoBlobSkippingParam = "auto_blob_skipping";
static string  kTraceParam = "trace";
static string  kMostRecentTimeParam = "most_recent_time";
static string  kMostAncientTimeParam = "most_ancient_time";
static string  kHistogramNamesParam = "histogram_names";
static string  kSendBlobIfSmallParam = "send_blob_if_small";
static string  kNA = "n/a";

static vector<pair<string, SPSGS_ResolveRequest::EPSGS_BioseqIncludeData>>
    kResolveFlagParams =
{
    make_pair("all_info", SPSGS_ResolveRequest::fPSGS_AllBioseqFields),   // must be first

    make_pair("canon_id", SPSGS_ResolveRequest::fPSGS_CanonicalId),
    make_pair("seq_ids", SPSGS_ResolveRequest::fPSGS_SeqIds),
    make_pair("mol_type", SPSGS_ResolveRequest::fPSGS_MoleculeType),
    make_pair("length", SPSGS_ResolveRequest::fPSGS_Length),
    make_pair("state", SPSGS_ResolveRequest::fPSGS_State),
    make_pair("blob_id", SPSGS_ResolveRequest::fPSGS_BlobId),
    make_pair("tax_id", SPSGS_ResolveRequest::fPSGS_TaxId),
    make_pair("hash", SPSGS_ResolveRequest::fPSGS_Hash),
    make_pair("date_changed", SPSGS_ResolveRequest::fPSGS_DateChanged),
    make_pair("gi", SPSGS_ResolveRequest::fPSGS_Gi),
    make_pair("name", SPSGS_ResolveRequest::fPSGS_Name),
    make_pair("seq_state", SPSGS_ResolveRequest::fPSGS_SeqState)
};
static string  kBadUrlMessage = "Unknown request, the provided URL "
                                "is not recognized: ";


int CPubseqGatewayApp::OnBadURL(CHttpRequest &  req,
                                shared_ptr<CPSGS_Reply>  reply)
{
    CRequestContextResetter context_resetter;
    CRef<CRequestContext>   context = x_CreateRequestContext(req);

    if (x_IsShuttingDown(reply)) {
        x_PrintRequestStop(context, CRequestStatus::e503_ServiceUnavailable);
        return 0;
    }

    auto    http_reply = reply->GetHttpReply();
    if (req.GetPath() == "/") {
        // Special case: no path at all so provide a help message
        try {
            http_reply->SetContentType(ePSGS_JsonMime);
            http_reply->SetContentLength(m_HelpMessage.length());
            http_reply->SendOk(m_HelpMessage.c_str(), m_HelpMessage.length(), false);
            x_PrintRequestStop(context, CRequestStatus::e200_Ok);
        } catch (const exception &  exc) {
            string      msg = "Exception when handling no path URL event: " +
                              string(exc.what());
            x_SendMessageAndCompletionChunks(reply, msg,
                                             CRequestStatus::e500_InternalServerError,
                                             ePSGS_BadURL, eDiag_Error);
            PSG_ERROR(msg);
            x_PrintRequestStop(context, CRequestStatus::e500_InternalServerError);
        } catch (...) {
            string      msg = "Unknown exception when handling no path URL event";
            x_SendMessageAndCompletionChunks(reply, msg,
                                             CRequestStatus::e500_InternalServerError,
                                             ePSGS_BadURL, eDiag_Error);
            PSG_ERROR(msg);
            x_PrintRequestStop(context, CRequestStatus::e500_InternalServerError);
        }
    } else {
        try {
            m_Counters.Increment(CPSGSCounters::ePSGS_BadUrlPath);

            string      bad_url = req.GetPath();
            x_SendMessageAndCompletionChunks(reply, kBadUrlMessage + bad_url,
                                             CRequestStatus::e400_BadRequest,
                                             ePSGS_BadURL,
                                             eDiag_Error);
            auto    bad_url_size = bad_url.size();
            if (bad_url_size > 128) {
                bad_url.resize(128);
                bad_url += "... (truncated; original length: " +
                           to_string(bad_url_size) + ")";
            }

            PSG_WARNING(kBadUrlMessage + bad_url);
            x_PrintRequestStop(context, CRequestStatus::e400_BadRequest);
        } catch (const exception &  exc) {
            string      msg = "Exception when handling a bad URL event: " +
                              string(exc.what());
            x_SendMessageAndCompletionChunks(reply, msg,
                                             CRequestStatus::e500_InternalServerError,
                                             ePSGS_BadURL, eDiag_Error);
            PSG_ERROR(msg);
            x_PrintRequestStop(context, CRequestStatus::e500_InternalServerError);
        } catch (...) {
            string  msg = "Unknown exception when handling a bad URL event";
            x_SendMessageAndCompletionChunks(reply, msg,
                                             CRequestStatus::e500_InternalServerError,
                                             ePSGS_BadURL, eDiag_Error);
            PSG_ERROR(msg);
            x_PrintRequestStop(context, CRequestStatus::e500_InternalServerError);
        }
    }
    return 0;
}


int CPubseqGatewayApp::OnGet(CHttpRequest &  req,
                             shared_ptr<CPSGS_Reply>  reply)
{
    auto                    now = chrono::high_resolution_clock::now();
    CRequestContextResetter context_resetter;
    CRef<CRequestContext>   context = x_CreateRequestContext(req);

    if (x_IsShuttingDown(reply)) {
        x_PrintRequestStop(context, CRequestStatus::e503_ServiceUnavailable);
        return 0;
    }

    int     hops;
    if (!x_GetHops(req, reply, hops)) {
        x_PrintRequestStop(context, CRequestStatus::e400_BadRequest);
        return 0;
    }

    try {
        CTempString                             seq_id;
        int                                     seq_id_type;
        SPSGS_RequestBase::EPSGS_CacheAndDbUse  use_cache = SPSGS_RequestBase::ePSGS_CacheAndDb;

        if (!x_ProcessCommonGetAndResolveParams(req, reply, seq_id,
                                                seq_id_type, use_cache)) {
            x_PrintRequestStop(context, CRequestStatus::e400_BadRequest);
            return 0;
        }

        string                                  err_msg;
        SPSGS_BlobRequestBase::EPSGS_TSEOption  tse_option = SPSGS_BlobRequestBase::ePSGS_OrigTSE;
        SRequestParameter                       tse_param = x_GetParam(req, kTSEParam);
        if (tse_param.m_Found) {
            tse_option = x_GetTSEOption(kTSEParam, tse_param.m_Value, err_msg);
            if (tse_option == SPSGS_BlobRequestBase::ePSGS_UnknownTSE) {
                x_MalformedArguments(reply, context, err_msg);
                return 0;
            }
        }

        vector<string>      exclude_blobs;
        SRequestParameter   exclude_blobs_param = x_GetParam(req,
                                                         kExcludeBlobsParam);
        if (exclude_blobs_param.m_Found) {
            exclude_blobs = x_GetExcludeBlobs(kExcludeBlobsParam,
                                              exclude_blobs_param.m_Value);
        }

        SPSGS_RequestBase::EPSGS_AccSubstitutioOption
                                subst_option = SPSGS_RequestBase::ePSGS_DefaultAccSubstitution;
        SRequestParameter       subst_param = x_GetParam(req, kAccSubstitutionParam);
        if (subst_param.m_Found) {
            subst_option = x_GetAccessionSubstitutionOption(kAccSubstitutionParam,
                                                            subst_param.m_Value,
                                                            err_msg);
            if (subst_option == SPSGS_RequestBase::ePSGS_UnknownAccSubstitution) {
                x_MalformedArguments(reply, context, err_msg);
                return 0;
            }
        }

        SRequestParameter   client_id_param = x_GetParam(req, kClientIdParam);

        SPSGS_RequestBase::EPSGS_Trace  trace = SPSGS_RequestBase::ePSGS_NoTracing;
        if (!x_GetTraceParameter(req, kTraceParam, trace, err_msg)) {
            x_SendMessageAndCompletionChunks(reply, err_msg,
                                             CRequestStatus::e400_BadRequest,
                                             ePSGS_MalformedParameter, eDiag_Error);
            x_PrintRequestStop(context, CRequestStatus::e400_BadRequest);
            return 0;
        }

        bool                auto_blob_skipping = true;  // default
        SRequestParameter   auto_blob_skipping_param = x_GetParam(req, kAutoBlobSkippingParam);
        if (auto_blob_skipping_param.m_Found) {
            if (!x_IsBoolParamValid(kAutoBlobSkippingParam,
                                    auto_blob_skipping_param.m_Value,
                                    err_msg)) {
                m_Counters.Increment(CPSGSCounters::ePSGS_MalformedArgs);
                x_SendMessageAndCompletionChunks(reply, err_msg,
                                                 CRequestStatus::e400_BadRequest,
                                                 ePSGS_MalformedParameter,
                                                 eDiag_Error);
                x_PrintRequestStop(context, CRequestStatus::e400_BadRequest);
                return 0;
            }
            auto_blob_skipping = auto_blob_skipping_param.m_Value == "yes";
        }

        vector<string>      enabled_processors;
        vector<string>      disabled_processors;
        if (!x_GetEnabledAndDisabledProcessors(req, reply, enabled_processors,
                                               disabled_processors))
            return 0;

        int     send_blob_if_small = x_GetSendBlobIfSmallParameter(req, reply);
        if (send_blob_if_small < 0)
            return 0;

        m_Counters.Increment(CPSGSCounters::ePSGS_GetBlobBySeqIdRequest);
        unique_ptr<SPSGS_RequestBase>
            req(new SPSGS_BlobBySeqIdRequest(
                        string(seq_id.data(), seq_id.size()),
                        seq_id_type, exclude_blobs,
                        tse_option, use_cache, subst_option,
                        auto_blob_skipping,
                        string(client_id_param.m_Value.data(),
                               client_id_param.m_Value.size()),
                        send_blob_if_small, hops, trace,
                        enabled_processors, disabled_processors,
                        now));
        shared_ptr<CPSGS_Request>
            request(new CPSGS_Request(move(req), context));

        x_DispatchRequest(request, reply);
    } catch (const exception &  exc) {
        string      msg = "Exception when handling a get request: " +
                          string(exc.what());
        x_SendMessageAndCompletionChunks(reply, msg,
                                         CRequestStatus::e500_InternalServerError,
                                         ePSGS_UnknownError, eDiag_Error);
        x_PrintRequestStop(context, CRequestStatus::e500_InternalServerError);
    } catch (...) {
        string      msg = "Unknown exception when handling a get request";
        x_SendMessageAndCompletionChunks(reply, msg,
                                         CRequestStatus::e500_InternalServerError,
                                         ePSGS_UnknownError, eDiag_Error);
        x_PrintRequestStop(context, CRequestStatus::e500_InternalServerError);
    }
    return 0;
}


int CPubseqGatewayApp::OnGetBlob(CHttpRequest &  req,
                                 shared_ptr<CPSGS_Reply>  reply)
{
    auto                    now = chrono::high_resolution_clock::now();
    CRequestContextResetter context_resetter;
    CRef<CRequestContext>   context = x_CreateRequestContext(req);

    if (x_IsShuttingDown(reply)) {
        x_PrintRequestStop(context, CRequestStatus::e503_ServiceUnavailable);
        return 0;
    }

    int     hops;
    if (!x_GetHops(req, reply, hops)) {
        x_PrintRequestStop(context, CRequestStatus::e400_BadRequest);
        return 0;
    }

    try {
        string              err_msg;
        SPSGS_BlobRequestBase::EPSGS_TSEOption
                            tse_option = SPSGS_BlobRequestBase::ePSGS_OrigTSE;
        SRequestParameter   tse_param = x_GetParam(req, kTSEParam);
        if (tse_param.m_Found) {
            tse_option = x_GetTSEOption(kTSEParam, tse_param.m_Value, err_msg);
            if (tse_option == SPSGS_BlobRequestBase::ePSGS_UnknownTSE) {
                x_MalformedArguments(reply, context, err_msg);
                return 0;
            }
        }

        SRequestParameter   last_modified_param = x_GetParam(req, kLastModifiedParam);
        int64_t             last_modified_value = INT64_MIN;
        if (last_modified_param.m_Found) {
            try {
                last_modified_value = NStr::StringToLong(
                                                last_modified_param.m_Value);
            } catch (...) {
                x_MalformedArguments(reply, context,
                                     "Malformed '" + kLastModifiedParam +
                                     "' parameter. Expected an integer");
                return 0;
            }
        }

        SPSGS_RequestBase::EPSGS_CacheAndDbUse  use_cache = x_GetUseCacheParameter(req, err_msg);
        if (!err_msg.empty()) {
            x_MalformedArguments(reply, context, err_msg);
            return 0;
        }

        SPSGS_RequestBase::EPSGS_Trace  trace = SPSGS_RequestBase::ePSGS_NoTracing;
        if (!x_GetTraceParameter(req, kTraceParam, trace, err_msg)) {
            x_SendMessageAndCompletionChunks(reply, err_msg,
                                             CRequestStatus::e400_BadRequest,
                                             ePSGS_MalformedParameter, eDiag_Error);
            x_PrintRequestStop(context, CRequestStatus::e400_BadRequest);
            return 0;
        }

        SRequestParameter   blob_id_param = x_GetParam(req, kBlobIdParam);
        if (blob_id_param.m_Found)
        {
            SPSGS_BlobId    blob_id(blob_id_param.m_Value);

            if (blob_id.GetId().empty()) {
                x_MalformedArguments(reply, context,
                                     "The '" + kBlobIdParam +
                                     "' parameter value has not been supplied");
                return 0;
            }

            SRequestParameter   client_id_param = x_GetParam(req, kClientIdParam);

            vector<string>      enabled_processors;
            vector<string>      disabled_processors;
            if (!x_GetEnabledAndDisabledProcessors(req, reply, enabled_processors,
                                                   disabled_processors))
                return 0;

            int     send_blob_if_small = x_GetSendBlobIfSmallParameter(req, reply);
            if (send_blob_if_small < 0)
                return 0;

            m_Counters.Increment(CPSGSCounters::ePSGS_GetBlobBySatSatKeyRequest);
            unique_ptr<SPSGS_RequestBase>
                    req(new SPSGS_BlobBySatSatKeyRequest(
                                blob_id, last_modified_value,
                                tse_option, use_cache,
                                string(client_id_param.m_Value.data(),
                                       client_id_param.m_Value.size()),
                                send_blob_if_small, hops, trace,
                                enabled_processors, disabled_processors, now));
            shared_ptr<CPSGS_Request>
                    request(new CPSGS_Request(move(req), context));

            x_DispatchRequest(request, reply);
            return 0;
        }

        x_InsufficientArguments(reply, context, "Mandatory parameter "
                                "'" + kBlobIdParam + "' is not found.");
    } catch (const exception &  exc) {
        string      msg = "Exception when handling a getblob request: " +
                          string(exc.what());
        x_SendMessageAndCompletionChunks(reply, msg,
                                         CRequestStatus::e500_InternalServerError,
                                         ePSGS_UnknownError, eDiag_Error);
        x_PrintRequestStop(context, CRequestStatus::e500_InternalServerError);
    } catch (...) {
        string      msg = "Unknown exception when handling a getblob request";
        x_SendMessageAndCompletionChunks(reply, msg,
                                         CRequestStatus::e500_InternalServerError,
                                         ePSGS_UnknownError, eDiag_Error);
        x_PrintRequestStop(context, CRequestStatus::e500_InternalServerError);
    }
    return 0;
}


int CPubseqGatewayApp::OnResolve(CHttpRequest &  req,
                                 shared_ptr<CPSGS_Reply>  reply)
{
    auto                    now = chrono::high_resolution_clock::now();
    CRequestContextResetter context_resetter;
    CRef<CRequestContext>   context = x_CreateRequestContext(req);

    if (x_IsShuttingDown(reply)) {
        x_PrintRequestStop(context, CRequestStatus::e503_ServiceUnavailable);
        return 0;
    }

    int     hops;
    if (!x_GetHops(req, reply, hops)) {
        x_PrintRequestStop(context, CRequestStatus::e400_BadRequest);
        return 0;
    }

    try {
        CTempString                             seq_id;
        int                                     seq_id_type;
        SPSGS_RequestBase::EPSGS_CacheAndDbUse  use_cache = SPSGS_RequestBase::ePSGS_CacheAndDb;

        if (!x_ProcessCommonGetAndResolveParams(req, reply, seq_id,
                                                seq_id_type, use_cache)) {
            x_PrintRequestStop(context, CRequestStatus::e400_BadRequest);
            return 0;
        }

        string              err_msg;
        SPSGS_ResolveRequest::EPSGS_OutputFormat
                            output_format = SPSGS_ResolveRequest::ePSGS_NativeFormat;
        SRequestParameter   fmt_param = x_GetParam(req, kFmtParam);
        if (fmt_param.m_Found) {
            output_format = x_GetOutputFormat(kFmtParam, fmt_param.m_Value, err_msg);
            if (output_format == SPSGS_ResolveRequest::ePSGS_UnknownFormat) {
                m_Counters.Increment(CPSGSCounters::ePSGS_MalformedArgs);
                x_SendMessageAndCompletionChunks(reply, err_msg,
                                                 CRequestStatus::e400_BadRequest,
                                                 ePSGS_MalformedParameter,
                                                 eDiag_Error);
                PSG_WARNING(err_msg);
                x_PrintRequestStop(context, CRequestStatus::e400_BadRequest);
                return 0;
            }
        }

        SPSGS_ResolveRequest::TPSGS_BioseqIncludeData
                                include_data_flags = 0;
        SRequestParameter       request_param;
        for (const auto &  flag_param: kResolveFlagParams) {
            request_param = x_GetParam(req, flag_param.first);
            if (request_param.m_Found) {
                if (!x_IsBoolParamValid(flag_param.first,
                                        request_param.m_Value, err_msg)) {
                    m_Counters.Increment(CPSGSCounters::ePSGS_MalformedArgs);
                    x_SendMessageAndCompletionChunks(reply, err_msg,
                                                     CRequestStatus::e400_BadRequest,
                                                     ePSGS_MalformedParameter,
                                                     eDiag_Error);
                    PSG_WARNING(err_msg);
                    x_PrintRequestStop(context, CRequestStatus::e400_BadRequest);
                    return 0;
                }
                if (request_param.m_Value == "yes") {
                    include_data_flags |= flag_param.second;
                } else {
                    include_data_flags &= ~flag_param.second;
                }
            }
        }

        SPSGS_RequestBase::EPSGS_AccSubstitutioOption
                                subst_option = SPSGS_RequestBase::ePSGS_DefaultAccSubstitution;
        SRequestParameter       subst_param = x_GetParam(req, kAccSubstitutionParam);
        if (subst_param.m_Found) {
            subst_option = x_GetAccessionSubstitutionOption(kAccSubstitutionParam,
                                                            subst_param.m_Value,
                                                            err_msg);
            if (subst_option == SPSGS_RequestBase::ePSGS_UnknownAccSubstitution) {
                m_Counters.Increment(CPSGSCounters::ePSGS_MalformedArgs);
                x_SendMessageAndCompletionChunks(reply, err_msg,
                                                 CRequestStatus::e400_BadRequest,
                                                 ePSGS_MalformedParameter,
                                                 eDiag_Error);
                PSG_WARNING(err_msg);
                x_PrintRequestStop(context, CRequestStatus::e400_BadRequest);
                return 0;
            }
        }

        SPSGS_RequestBase::EPSGS_Trace  trace = SPSGS_RequestBase::ePSGS_NoTracing;
        if (!x_GetTraceParameter(req, kTraceParam, trace, err_msg)) {
            x_SendMessageAndCompletionChunks(reply, err_msg,
                                             CRequestStatus::e400_BadRequest,
                                             ePSGS_MalformedParameter,
                                             eDiag_Error);
            x_PrintRequestStop(context, CRequestStatus::e400_BadRequest);
            return 0;
        }

        vector<string>      enabled_processors;
        vector<string>      disabled_processors;
        if (!x_GetEnabledAndDisabledProcessors(req, reply, enabled_processors,
                                               disabled_processors))
            return 0;

        // Parameters processing has finished
        m_Counters.Increment(CPSGSCounters::ePSGS_ResolveRequest);
        unique_ptr<SPSGS_RequestBase>
            req(new SPSGS_ResolveRequest(
                        string(seq_id.data(), seq_id.size()),
                        seq_id_type, include_data_flags, output_format,
                        use_cache, subst_option, hops, trace,
                        enabled_processors, disabled_processors, now));
        shared_ptr<CPSGS_Request>
            request(new CPSGS_Request(move(req), context));

        x_DispatchRequest(request, reply);
    } catch (const exception &  exc) {
        string      msg = "Exception when handling a resolve request: " +
                          string(exc.what());
        x_SendMessageAndCompletionChunks(reply, msg,
                                         CRequestStatus::e500_InternalServerError,
                                         ePSGS_UnknownError, eDiag_Error);
        x_PrintRequestStop(context, CRequestStatus::e500_InternalServerError);
    } catch (...) {
        string      msg = "Unknown exception when handling a resolve request";
        x_SendMessageAndCompletionChunks(reply, msg,
                                         CRequestStatus::e500_InternalServerError,
                                         ePSGS_UnknownError, eDiag_Error);
        x_PrintRequestStop(context, CRequestStatus::e500_InternalServerError);
    }
    return 0;
}


int CPubseqGatewayApp::OnGetTSEChunk(CHttpRequest &  req,
                                     shared_ptr<CPSGS_Reply>  reply)
{
    auto                    now = chrono::high_resolution_clock::now();
    CRequestContextResetter context_resetter;
    CRef<CRequestContext>   context = x_CreateRequestContext(req);

    if (x_IsShuttingDown(reply)) {
        x_PrintRequestStop(context, CRequestStatus::e503_ServiceUnavailable);
        return 0;
    }

    int     hops;
    if (!x_GetHops(req, reply, hops)) {
        x_PrintRequestStop(context, CRequestStatus::e400_BadRequest);
        return 0;
    }

    try {
        // Mandatory parameter id2_chunk
        SRequestParameter   id2_chunk_param = x_GetParam(req, kId2ChunkParam);
        int64_t             id2_chunk_value = INT64_MIN;
        if (!id2_chunk_param.m_Found) {
            x_InsufficientArguments(reply, context, "Mandatory parameter "
                                    "'" + kId2ChunkParam + "' is not found.");
            return 0;
        }

        try {
            id2_chunk_value = NStr::StringToLong(id2_chunk_param.m_Value);
            if (id2_chunk_value < 0) {
                x_MalformedArguments(reply, context,
                                     "Invalid '" + kId2ChunkParam +
                                     "' parameter. Expected >= 0");
                return 0;
            }
        } catch (...) {
            x_MalformedArguments(reply, context,
                                 "Malformed '" + kId2ChunkParam + "' parameter. "
                                 "Expected an integer");
            return 0;
        }

        SRequestParameter   id2_info_param = x_GetParam(req, kId2InfoParam);
        if (!id2_info_param.m_Found)
        {
            x_InsufficientArguments(reply, context, "Mandatory parameter '" +
                                    kId2InfoParam + "' is not found.");
            return 0;
        }

        string                                  err_msg;
        SPSGS_RequestBase::EPSGS_CacheAndDbUse  use_cache = x_GetUseCacheParameter(req, err_msg);
        if (!err_msg.empty()) {
            x_MalformedArguments(reply, context, err_msg);
            return 0;
        }

        SPSGS_RequestBase::EPSGS_Trace  trace = SPSGS_RequestBase::ePSGS_NoTracing;
        if (!x_GetTraceParameter(req, kTraceParam, trace, err_msg)) {
            x_SendMessageAndCompletionChunks(reply, err_msg,
                                             CRequestStatus::e400_BadRequest,
                                             ePSGS_MalformedParameter, eDiag_Error);
            x_PrintRequestStop(context, CRequestStatus::e400_BadRequest);
            return 0;
        }

        vector<string>      enabled_processors;
        vector<string>      disabled_processors;
        if (!x_GetEnabledAndDisabledProcessors(req, reply, enabled_processors,
                                               disabled_processors))
            return 0;

        // All parameters are good
        m_Counters.Increment(CPSGSCounters::ePSGS_GetTSEChunk);
        unique_ptr<SPSGS_RequestBase>
            req(new SPSGS_TSEChunkRequest(
                        id2_chunk_value, id2_info_param.m_Value,
                        use_cache, hops, trace,
                        enabled_processors, disabled_processors, now));
        shared_ptr<CPSGS_Request>
            request(new CPSGS_Request(move(req), context));

        x_DispatchRequest(request, reply);
    } catch (const exception &  exc) {
        string      msg = "Exception when handling a get_tse_chunk request: " +
                          string(exc.what());
        x_SendMessageAndCompletionChunks(reply, msg,
                                         CRequestStatus::e500_InternalServerError,
                                         ePSGS_UnknownError, eDiag_Error);
        x_PrintRequestStop(context, CRequestStatus::e500_InternalServerError);
    } catch (...) {
        string      msg = "Unknown exception when handling a get_tse_chunk request";
        x_SendMessageAndCompletionChunks(reply, msg,
                                         CRequestStatus::e500_InternalServerError,
                                         ePSGS_UnknownError, eDiag_Error);
        x_PrintRequestStop(context, CRequestStatus::e500_InternalServerError);
    }
    return 0;
}


int CPubseqGatewayApp::OnGetNA(CHttpRequest &  req,
                               shared_ptr<CPSGS_Reply>  reply)
{
    auto                    now = chrono::high_resolution_clock::now();
    CRequestContextResetter context_resetter;
    CRef<CRequestContext>   context = x_CreateRequestContext(req);

    if (x_IsShuttingDown(reply)) {
        x_PrintRequestStop(context, CRequestStatus::e503_ServiceUnavailable);
        return 0;
    }

    int     hops;
    if (!x_GetHops(req, reply, hops)) {
        x_PrintRequestStop(context, CRequestStatus::e400_BadRequest);
        return 0;
    }

    try {
        CTempString                             seq_id;
        int                                     seq_id_type;
        SPSGS_RequestBase::EPSGS_CacheAndDbUse  use_cache = SPSGS_RequestBase::ePSGS_CacheAndDb;

        if (!x_ProcessCommonGetAndResolveParams(req, reply, seq_id,
                                                seq_id_type, use_cache)) {
            x_PrintRequestStop(context, CRequestStatus::e400_BadRequest);
            return 0;
        }

        // At the moment the only json format is supported (native translates
        // to json). See CXX-10258
        string              err_msg;
        SRequestParameter   fmt_param = x_GetParam(req, kFmtParam);
        if (fmt_param.m_Found) {
            auto        output_format = x_GetOutputFormat(kFmtParam,
                                                          fmt_param.m_Value,
                                                          err_msg);
            if (output_format == SPSGS_ResolveRequest::ePSGS_ProtobufFormat ||
                output_format == SPSGS_ResolveRequest::ePSGS_UnknownFormat) {
                x_MalformedArguments(
                        reply, context,
                        "Invalid '" + kFmtParam + "' parameter value. The 'get_na' "
                        "request supports 'json' and 'native' values");
                return 0;
            }
        }


        // Get the annotation names
        SRequestParameter   names_param = x_GetParam(req, kNamesParam);
        if (!names_param.m_Found) {
            x_MalformedArguments(reply, context,
                                 "The mandatory '" + kNamesParam +
                                 "' parameter is not found");
            return 0;
        }

        vector<string>          names;
        NStr::Split(names_param.m_Value, ",", names);
        if (names.empty()) {
            x_MalformedArguments(reply, context,
                                 "Named annotation names are not found in the request");
            return 0;
        }

        SPSGS_RequestBase::EPSGS_Trace  trace = SPSGS_RequestBase::ePSGS_NoTracing;
        if (!x_GetTraceParameter(req, kTraceParam, trace, err_msg)) {
            x_SendMessageAndCompletionChunks(reply, err_msg,
                                             CRequestStatus::e400_BadRequest,
                                             ePSGS_MalformedParameter,
                                             eDiag_Error);
            x_PrintRequestStop(context, CRequestStatus::e400_BadRequest);
            return 0;
        }

        vector<string>      enabled_processors;
        vector<string>      disabled_processors;
        if (!x_GetEnabledAndDisabledProcessors(req, reply, enabled_processors,
                                               disabled_processors))
            return 0;

        SPSGS_BlobRequestBase::EPSGS_TSEOption
                            tse_option = SPSGS_BlobRequestBase::ePSGS_NoneTSE;
        SRequestParameter   tse_param = x_GetParam(req, kTSEParam);
        if (tse_param.m_Found) {
            tse_option = x_GetTSEOption(kTSEParam, tse_param.m_Value, err_msg);
            if (tse_option == SPSGS_BlobRequestBase::ePSGS_UnknownTSE) {
                x_MalformedArguments(reply, context, err_msg);
                return 0;
            }
        }

        SRequestParameter   client_id_param = x_GetParam(req, kClientIdParam);

        int     send_blob_if_small = x_GetSendBlobIfSmallParameter(req, reply);
        if (send_blob_if_small < 0)
            return 0;

        bool                auto_blob_skipping = true;  // default
        SRequestParameter   auto_blob_skipping_param = x_GetParam(req, kAutoBlobSkippingParam);
        if (auto_blob_skipping_param.m_Found) {
            if (!x_IsBoolParamValid(kAutoBlobSkippingParam,
                                    auto_blob_skipping_param.m_Value,
                                    err_msg)) {
                m_Counters.Increment(CPSGSCounters::ePSGS_MalformedArgs);
                x_SendMessageAndCompletionChunks(reply, err_msg,
                                                 CRequestStatus::e400_BadRequest,
                                                 ePSGS_MalformedParameter,
                                                 eDiag_Error);
                x_PrintRequestStop(context, CRequestStatus::e400_BadRequest);
                return 0;
            }
            auto_blob_skipping = auto_blob_skipping_param.m_Value == "yes";
        }

        // Parameters processing has finished
        m_Counters.Increment(CPSGSCounters::ePSGS_GetNamedAnnotations);
        unique_ptr<SPSGS_RequestBase>
            req(new SPSGS_AnnotRequest(
                        string(seq_id.data(), seq_id.size()),
                        seq_id_type, names, use_cache,
                        auto_blob_skipping,
                        string(client_id_param.m_Value.data(),
                               client_id_param.m_Value.size()),
                        tse_option, send_blob_if_small,
                        hops, trace,
                        enabled_processors, disabled_processors,
                        now));
        shared_ptr<CPSGS_Request>
            request(new CPSGS_Request(move(req), context));

        x_DispatchRequest(request, reply);
    } catch (const exception &  exc) {
        string      msg = "Exception when handling a get_na request: " +
                          string(exc.what());
        x_SendMessageAndCompletionChunks(reply, msg,
                                         CRequestStatus::e500_InternalServerError,
                                         ePSGS_UnknownError, eDiag_Error);
        x_PrintRequestStop(context, CRequestStatus::e500_InternalServerError);
    } catch (...) {
        string      msg = "Unknown exception when handling a get_na request";
        x_SendMessageAndCompletionChunks(reply, msg,
                                         CRequestStatus::e500_InternalServerError,
                                         ePSGS_UnknownError, eDiag_Error);
        x_PrintRequestStop(context, CRequestStatus::e500_InternalServerError);
    }
    return 0;
}


int CPubseqGatewayApp::OnAccessionBlobHistory(CHttpRequest &  req,
                                              shared_ptr<CPSGS_Reply>  reply)
{
    auto                    now = chrono::high_resolution_clock::now();
    CRequestContextResetter context_resetter;
    CRef<CRequestContext>   context = x_CreateRequestContext(req);

    if (x_IsShuttingDown(reply)) {
        x_PrintRequestStop(context, CRequestStatus::e503_ServiceUnavailable);
        return 0;
    }

    int     hops;
    if (!x_GetHops(req, reply, hops)) {
        x_PrintRequestStop(context, CRequestStatus::e400_BadRequest);
        return 0;
    }

    try {
        CTempString                             seq_id;
        int                                     seq_id_type;
        SPSGS_RequestBase::EPSGS_CacheAndDbUse  use_cache = SPSGS_RequestBase::ePSGS_CacheAndDb;

        if (!x_ProcessCommonGetAndResolveParams(req, reply, seq_id,
                                                seq_id_type, use_cache)) {
            x_PrintRequestStop(context, CRequestStatus::e400_BadRequest);
            return 0;
        }

        SPSGS_RequestBase::EPSGS_Trace  trace = SPSGS_RequestBase::ePSGS_NoTracing;
        string                          err_msg;
        if (!x_GetTraceParameter(req, kTraceParam, trace, err_msg)) {
            x_SendMessageAndCompletionChunks(reply, err_msg,
                                             CRequestStatus::e400_BadRequest,
                                             ePSGS_MalformedParameter,
                                             eDiag_Error);
            x_PrintRequestStop(context, CRequestStatus::e400_BadRequest);
            return 0;
        }

        vector<string>      enabled_processors;
        vector<string>      disabled_processors;
        if (!x_GetEnabledAndDisabledProcessors(req, reply, enabled_processors,
                                               disabled_processors))
            return 0;

        // Parameters processing has finished
        m_Counters.Increment(CPSGSCounters::ePSGS_AccessionBlobHistory);
        unique_ptr<SPSGS_RequestBase>
            req(new SPSGS_AccessionBlobHistoryRequest(
                        string(seq_id.data(), seq_id.size()),
                        seq_id_type, use_cache, hops, trace,
                        enabled_processors, disabled_processors,
                        now));
        shared_ptr<CPSGS_Request>
            request(new CPSGS_Request(move(req), context));

        x_DispatchRequest(request, reply);
    } catch (const exception &  exc) {
        string      msg = "Exception when handling an accession_blob_history request: " +
                          string(exc.what());
        x_SendMessageAndCompletionChunks(reply, msg,
                                         CRequestStatus::e500_InternalServerError,
                                         ePSGS_UnknownError, eDiag_Error);
        x_PrintRequestStop(context, CRequestStatus::e500_InternalServerError);
    } catch (...) {
        string      msg = "Unknown exception when handling an accession_blob_history request";
        x_SendMessageAndCompletionChunks(reply, msg,
                                         CRequestStatus::e500_InternalServerError,
                                         ePSGS_UnknownError, eDiag_Error);
        x_PrintRequestStop(context, CRequestStatus::e500_InternalServerError);
    }

    return 0;
}


int CPubseqGatewayApp::OnHealth(CHttpRequest &  req,
                                shared_ptr<CPSGS_Reply>  reply)
{
    static string   separator = "==============================================";
    static string   prefix = "PSG_HEALTH_ERROR: ";

    m_Counters.Increment(CPSGSCounters::ePSGS_HealthRequest);

    auto                    now = chrono::high_resolution_clock::now();
    CRequestContextResetter context_resetter;
    CRef<CRequestContext>   context = x_CreateRequestContext(req);

    auto    startup_data_state = GetStartupDataState();
    if (startup_data_state != ePSGS_StartupDataOK) {
        // Something is wrong with access to Cassandra
        // Check if there are any active alerts
        auto        active_alerts = m_Alerts.SerializeActive();
        string      msg = separator + "\n" +
                          prefix + "CASSANDRA" "\n" +
                          GetCassStartupDataStateMessage(startup_data_state) + "\n" +
                          separator + "\n" +
                          prefix + "ALERTS" "\n";
        if (active_alerts.GetSize() == 0) {
            msg += "There are no active alerts";
        } else {
            msg += "Active alerts are:" "\n" +
                   active_alerts.Repr(CJsonNode::fStandardJson);
        }

        reply->GetHttpReply()->SetContentType(ePSGS_PlainTextMime);
        reply->GetHttpReply()->Send500(msg.c_str());
        PSG_WARNING("Cassandra is not available or is in non-working state");
        x_PrintRequestStop(context, CRequestStatus::e500_InternalServerError);
        return 0;
    }

    if (m_TestSeqId.empty()) {
        // seq_id for a health test is not configured so skip the test
        auto    http_reply = reply->GetHttpReply();
        http_reply->SetContentType(ePSGS_PlainTextMime);
        http_reply->SetContentLength(0);
        http_reply->SendOk(nullptr, 0, false);
        PSG_WARNING("Test seq_id resolution skipped (configured as an empty string)");
        x_PrintRequestStop(context, CRequestStatus::e200_Ok);
        return 0;
    }

    if (m_Si2csiDbFile.empty() || m_BioseqInfoDbFile.empty()) {
        // Cache is not configured so skip the test
        auto    http_reply = reply->GetHttpReply();
        http_reply->SetContentType(ePSGS_PlainTextMime);
        http_reply->SetContentLength(0);
        http_reply->SendOk(nullptr, 0, false);
        PSG_WARNING("Test seq_id resolution skipped (cache is not configured)");
        x_PrintRequestStop(context, CRequestStatus::e200_Ok);
        return 0;
    }

    // If cache is configured then we can give it a try to resolve the
    // configured seq_id in cache
    try {
        vector<string>      enabled_processors;
        vector<string>      disabled_processors;

        unique_ptr<SPSGS_RequestBase>
            req(new SPSGS_ResolveRequest(m_TestSeqId, -1,
                                         SPSGS_ResolveRequest::fPSGS_CanonicalId,
                                         SPSGS_ResolveRequest::ePSGS_JsonFormat,
                                         SPSGS_RequestBase::ePSGS_CacheOnly,
                                         SPSGS_RequestBase::ePSGS_NeverAccSubstitute,
                                         0, SPSGS_RequestBase::ePSGS_NoTracing,
                                         enabled_processors, disabled_processors,
                                         now));
        shared_ptr<CPSGS_Request>
            request(new CPSGS_Request(move(req), CRef<CRequestContext>()));


        CPSGS_ResolveProcessor  resolve_processor(request, reply, 0);
        auto    resolution = resolve_processor.ResolveTestInputSeqId();

        if (!resolution.IsValid()) {
            if (!m_TestSeqIdIgnoreError) {
                string  msg = separator + "\n" +
                              prefix + "RESOLUTION" "\n";
                if (resolution.m_Error.HasError()) {
                    msg += resolution.m_Error.m_ErrorMessage;
                } else {
                    msg += "Cannot resolve '" + m_TestSeqId + "' seq_id";
                }
                reply->GetHttpReply()->SetContentType(ePSGS_PlainTextMime);
                reply->GetHttpReply()->Send500(msg.c_str());
                PSG_WARNING("Cannot resolve test seq_id '" + m_TestSeqId + "'");
                x_PrintRequestStop(context, CRequestStatus::e500_InternalServerError);
                return 0;
            }
            PSG_WARNING("Cannot resolve test seq_id '" + m_TestSeqId +
                        "', however the configuration is to ignore test errors");
        }
    } catch (const exception &  exc) {
        if (!m_TestSeqIdIgnoreError) {
            string  msg = separator + "\n" +
                          prefix + "RESOLUTION" "\n" +
                          exc.what();
            reply->GetHttpReply()->SetContentType(ePSGS_PlainTextMime);
            reply->GetHttpReply()->Send500(msg.c_str());
            PSG_WARNING("Cannot resolve test seq_id '" + m_TestSeqId + "'");
            x_PrintRequestStop(context, CRequestStatus::e500_InternalServerError);
            return 0;
        }
        PSG_WARNING("Cannot resolve test seq_id '" + m_TestSeqId +
                    "', however the configuration is to ignore test errors");
    } catch (...) {
        if (!m_TestSeqIdIgnoreError) {
            string  msg = separator + "\n" +
                          prefix + "RESOLUTION" "\n"
                          "Unknown '" + m_TestSeqId + "' resolution error";
            reply->GetHttpReply()->SetContentType(ePSGS_PlainTextMime);
            reply->GetHttpReply()->Send500(msg.c_str());
            PSG_WARNING("Cannot resolve test seq_id '" + m_TestSeqId + "'");
            x_PrintRequestStop(context, CRequestStatus::e500_InternalServerError);
            return 0;
        }
        PSG_WARNING("Cannot resolve test seq_id '" + m_TestSeqId +
                    "', however the configuration is to ignore test errors");
    }

    // Here: all OK or errors are ignored
    auto    http_reply = reply->GetHttpReply();
    http_reply->SetContentType(ePSGS_PlainTextMime);
    http_reply->SetContentLength(0);
    http_reply->SendOk(nullptr, 0, false);
    x_PrintRequestStop(context, CRequestStatus::e200_Ok);
    return 0;
}


int CPubseqGatewayApp::OnConfig(CHttpRequest &  req,
                                shared_ptr<CPSGS_Reply>  reply)
{
    static string   kConfigurationFilePath = "ConfigurationFilePath";
    static string   kConfiguration = "Configuration";

    // NOTE: expected to work regardless of the shutdown request

    CRequestContextResetter context_resetter;
    CRef<CRequestContext>   context = x_CreateRequestContext(req);

    auto    http_reply = reply->GetHttpReply();
    try {
        m_Counters.Increment(CPSGSCounters::ePSGS_AdminRequest);

        CNcbiOstrstream             conf;
        CNcbiOstrstreamToString     converter(conf);

        GetConfig().Write(conf);

        CJsonNode   conf_info(CJsonNode::NewObjectNode());
        conf_info.SetString(kConfigurationFilePath, GetConfigPath());
        conf_info.SetString(kConfiguration, string(converter));
        string      content = conf_info.Repr(CJsonNode::fStandardJson);

        http_reply->SetContentType(ePSGS_JsonMime);
        http_reply->SetContentLength(content.length());
        http_reply->SendOk(content.c_str(), content.length(), false);

        x_PrintRequestStop(context, CRequestStatus::e200_Ok);
    } catch (const exception &  exc) {
        string      msg = "Exception when handling a config request: " +
                          string(exc.what());
        x_SendMessageAndCompletionChunks(reply, msg,
                                         CRequestStatus::e500_InternalServerError,
                                         ePSGS_ConfigError, eDiag_Error);
        PSG_ERROR(msg);
        x_PrintRequestStop(context, CRequestStatus::e500_InternalServerError);
    } catch (...) {
        string      msg = "Unknown exception when handling a config request";
        x_SendMessageAndCompletionChunks(reply, msg,
                                         CRequestStatus::e500_InternalServerError,
                                         ePSGS_ConfigError, eDiag_Error);
        PSG_ERROR(msg);
        x_PrintRequestStop(context, CRequestStatus::e500_InternalServerError);
    }
    return 0;
}


int CPubseqGatewayApp::OnInfo(CHttpRequest &  req,
                              shared_ptr<CPSGS_Reply>  reply)
{
    static string   kPID = "PID";
    static string   kExecutablePath = "ExecutablePath";
    static string   kCommandLineArguments = "CommandLineArguments";
    static string   kStartupDataState = "StartupDataState";
    static string   kRealTime = "RealTime";
    static string   kUserTime = "UserTime";
    static string   kSystemTime = "SystemTime";
    static string   kPhysicalMemory = "PhysicalMemory";
    static string   kMemoryUsedTotal = "MemoryUsedTotal";
    static string   kMemoryUsedTotalPeak = "MemoryUsedTotalPeak";
    static string   kMemoryUsedResident = "MemoryUsedResident";
    static string   kMemoryUsedResidentPeak = "MemoryUsedResidentPeak";
    static string   kMemoryUsedShared = "MemoryUsedShared";
    static string   kMemoryUsedData = "MemoryUsedData";
    static string   kMemoryUsedStack = "MemoryUsedStack";
    static string   kMemoryUsedText = "MemoryUsedText";
    static string   kMemoryUsedLib = "MemoryUsedLib";
    static string   kMemoryUsedSwap = "MemoryUsedSwap";
    static string   kProcFDSoftLimit = "ProcFDSoftLimit";
    static string   kProcFDHardLimit = "ProcFDHardLimit";
    static string   kProcFDUsed = "ProcFDUsed";
    static string   kCPUCount = "CPUCount";
    static string   kProcThreadCount = "ProcThreadCount";
    static string   kVersion = "Version";
    static string   kBuildDate = "BuildDate";
    static string   kStartedAt = "StartedAt";
    static string   kExcludeBlobCacheUserCount = "ExcludeBlobCacheUserCount";

    // NOTE: expected to work regardless of the shutdown request

    CRequestContextResetter context_resetter;
    CRef<CRequestContext>   context = x_CreateRequestContext(req);

    auto                    app = CPubseqGatewayApp::GetInstance();
    auto                    http_reply = reply->GetHttpReply();
    try {
        m_Counters.Increment(CPSGSCounters::ePSGS_AdminRequest);

        CJsonNode   info(CJsonNode::NewObjectNode());

        info.SetInteger(kPID, CDiagContext::GetPID());
        info.SetString(kExecutablePath, GetProgramExecutablePath());
        info.SetString(kCommandLineArguments, x_GetCmdLineArguments());
        info.SetString(kStartupDataState,
                       GetCassStartupDataStateMessage(app->GetStartupDataState()));


        double      real_time;
        double      user_time;
        double      system_time;
        bool        process_time_result = CCurrentProcess::GetTimes(&real_time,
                                                                    &user_time,
                                                                    &system_time);
        if (process_time_result) {
            info.SetDouble(kRealTime, real_time);
            info.SetDouble(kUserTime, user_time);
            info.SetDouble(kSystemTime, system_time);
        } else {
            info.SetString(kRealTime, kNA);
            info.SetString(kUserTime, kNA);
            info.SetString(kSystemTime, kNA);
        }

        Uint8       physical_memory = CSystemInfo::GetTotalPhysicalMemorySize();
        if (physical_memory > 0)
            info.SetInteger(kPhysicalMemory, physical_memory);
        else
            info.SetString(kPhysicalMemory, kNA);

        CProcessBase::SMemoryUsage      mem_usage;
        bool                            mem_used_result = CCurrentProcess::GetMemoryUsage(mem_usage);
        if (mem_used_result) {
            if (mem_usage.total > 0)
                info.SetInteger(kMemoryUsedTotal, mem_usage.total);
            else
                info.SetString(kMemoryUsedTotal, kNA);

            if (mem_usage.total_peak > 0)
                info.SetInteger(kMemoryUsedTotalPeak, mem_usage.total_peak);
            else
                info.SetString(kMemoryUsedTotalPeak, kNA);

            if (mem_usage.resident > 0)
                info.SetInteger(kMemoryUsedResident, mem_usage.resident);
            else
                info.SetString(kMemoryUsedResident, kNA);

            if (mem_usage.resident_peak > 0)
                info.SetInteger(kMemoryUsedResidentPeak, mem_usage.resident_peak);
            else
                info.SetString(kMemoryUsedResidentPeak, kNA);

            if (mem_usage.shared > 0)
                info.SetInteger(kMemoryUsedShared, mem_usage.shared);
            else
                info.SetString(kMemoryUsedShared, kNA);

            if (mem_usage.data > 0)
                info.SetInteger(kMemoryUsedData, mem_usage.data);
            else
                info.SetString(kMemoryUsedData, kNA);

            if (mem_usage.stack > 0)
                info.SetInteger(kMemoryUsedStack, mem_usage.stack);
            else
                info.SetString(kMemoryUsedStack, kNA);

            if (mem_usage.text > 0)
                info.SetInteger(kMemoryUsedText, mem_usage.text);
            else
                info.SetString(kMemoryUsedText, kNA);

            if (mem_usage.lib > 0)
                info.SetInteger(kMemoryUsedLib, mem_usage.lib);
            else
                info.SetString(kMemoryUsedLib, kNA);

            if (mem_usage.swap > 0)
                info.SetInteger(kMemoryUsedSwap, mem_usage.swap);
            else
                info.SetString(kMemoryUsedSwap, kNA);
        } else {
            info.SetString(kMemoryUsedTotal, kNA);
            info.SetString(kMemoryUsedTotalPeak, kNA);
            info.SetString(kMemoryUsedResident, kNA);
            info.SetString(kMemoryUsedResidentPeak, kNA);
            info.SetString(kMemoryUsedShared, kNA);
            info.SetString(kMemoryUsedData, kNA);
            info.SetString(kMemoryUsedStack, kNA);
            info.SetString(kMemoryUsedText, kNA);
            info.SetString(kMemoryUsedLib, kNA);
            info.SetString(kMemoryUsedSwap, kNA);
        }

        int         proc_fd_soft_limit;
        int         proc_fd_hard_limit;
        int         proc_fd_used =
                CCurrentProcess::GetFileDescriptorsCount(&proc_fd_soft_limit,
                                                         &proc_fd_hard_limit);

        if (proc_fd_soft_limit >= 0)
            info.SetInteger(kProcFDSoftLimit, proc_fd_soft_limit);
        else
            info.SetString(kProcFDSoftLimit, kNA);

        if (proc_fd_hard_limit >= 0)
            info.SetInteger(kProcFDHardLimit, proc_fd_hard_limit);
        else
            info.SetString(kProcFDHardLimit, kNA);

        if (proc_fd_used >= 0)
            info.SetInteger(kProcFDUsed, proc_fd_used);
        else
            info.SetString(kProcFDUsed, kNA);

        info.SetInteger(kCPUCount, CSystemInfo::GetCpuCount());

        int         proc_thread_count = CCurrentProcess::GetThreadCount();
        if (proc_thread_count >= 1)
            info.SetInteger(kProcThreadCount, proc_thread_count);
        else
            info.SetString(kProcThreadCount, kNA);


        info.SetString(kVersion, PUBSEQ_GATEWAY_VERSION);
        info.SetString(kBuildDate, PUBSEQ_GATEWAY_BUILD_DATE);
        info.SetString(kStartedAt, m_StartTime.AsString());

        info.SetInteger(kExcludeBlobCacheUserCount,
                        app->GetExcludeBlobCache()->Size());

        string      content = info.Repr(CJsonNode::fStandardJson);

        http_reply->SetContentType(ePSGS_JsonMime);
        http_reply->SetContentLength(content.length());
        http_reply->SendOk(content.c_str(), content.length(), false);

        x_PrintRequestStop(context, CRequestStatus::e200_Ok);
    } catch (const exception &  exc) {
        string      msg = "Exception when handling an info request: " +
                          string(exc.what());
        x_SendMessageAndCompletionChunks(reply, msg,
                                         CRequestStatus::e500_InternalServerError,
                                         ePSGS_InfoError, eDiag_Error);
        PSG_ERROR(msg);
        x_PrintRequestStop(context, CRequestStatus::e500_InternalServerError);
    } catch (...) {
        string      msg = "Unknown exception when handling an info request";
        x_SendMessageAndCompletionChunks(reply, msg,
                                         CRequestStatus::e500_InternalServerError,
                                         ePSGS_InfoError, eDiag_Error);
        PSG_ERROR(msg);
        x_PrintRequestStop(context, CRequestStatus::e500_InternalServerError);
    }
    return 0;
}


int CPubseqGatewayApp::OnStatus(CHttpRequest &  req,
                                shared_ptr<CPSGS_Reply>  reply)
{
    // NOTE: expected to work regardless of the shutdown request

    CRequestContextResetter context_resetter;
    CRef<CRequestContext>   context = x_CreateRequestContext(req);

    auto        http_reply = reply->GetHttpReply();
    try {
        m_Counters.Increment(CPSGSCounters::ePSGS_AdminRequest);

        CJsonNode                       status(CJsonNode::NewObjectNode());

        m_Counters.AppendValueNode(
            status, CPSGSCounters::ePSGS_CassandraActiveStatements,
            static_cast<uint64_t>(m_CassConnection->GetActiveStatements()));
        m_Counters.AppendValueNode(
            status, CPSGSCounters::ePSGS_NumberOfConnections,
            static_cast<uint64_t>(m_TcpDaemon->NumOfConnections()));
        m_Counters.AppendValueNode(
            status, CPSGSCounters::ePSGS_ActiveRequest,
            static_cast<uint64_t>(g_ShutdownData.m_ActiveRequestCount.load()));
        m_Counters.AppendValueNode(
            status, CPSGSCounters::ePSGS_ShutdownRequested,
            g_ShutdownData.m_ShutdownRequested);

        if (g_ShutdownData.m_ShutdownRequested) {
            auto        now = chrono::steady_clock::now();
            uint64_t    sec = std::chrono::duration_cast<std::chrono::seconds>
                                (g_ShutdownData.m_Expired - now).count();
            m_Counters.AppendValueNode(
                status, CPSGSCounters::ePSGS_GracefulShutdownExpiredInSec, sec);
        } else {
            m_Counters.AppendValueNode(
                status, CPSGSCounters::ePSGS_GracefulShutdownExpiredInSec, kNA);
        }

        m_Counters.PopulateDictionary(status);

        string      content = status.Repr(CJsonNode::fStandardJson);

        http_reply->SetContentType(ePSGS_JsonMime);
        http_reply->SetContentLength(content.length());
        http_reply->SendOk(content.c_str(), content.length(), false);

        x_PrintRequestStop(context, CRequestStatus::e200_Ok);
    } catch (const exception &  exc) {
        string      msg = "Exception when handling a status request: " +
                          string(exc.what());
        x_SendMessageAndCompletionChunks(reply, msg,
                                         CRequestStatus::e500_InternalServerError,
                                         ePSGS_StatusError, eDiag_Error);
        PSG_ERROR(msg);
        x_PrintRequestStop(context, CRequestStatus::e500_InternalServerError);
    } catch (...) {
        string      msg = "Unknown exception when handling a status request";
        x_SendMessageAndCompletionChunks(reply, msg,
                                         CRequestStatus::e500_InternalServerError,
                                         ePSGS_StatusError, eDiag_Error);
        PSG_ERROR(msg);
        x_PrintRequestStop(context, CRequestStatus::e500_InternalServerError);
    }
    return 0;
}


int CPubseqGatewayApp::OnShutdown(CHttpRequest &  req,
                                  shared_ptr<CPSGS_Reply>  reply)
{
    // NOTE: expected to work regardless of the shutdown request

    static const char *     s_ImmediateShutdown = "Immediate shutdown request accepted";
    static const char *     s_GracefulShutdown = "Graceful shutdown request accepted";
    static size_t           s_ImmediateShutdownSize = strlen(s_ImmediateShutdown);
    static size_t           s_GracefulShutdownSize = strlen(s_GracefulShutdown);

    CRequestContextResetter context_resetter;
    CRef<CRequestContext>   context = x_CreateRequestContext(req);

    try {
        string              msg;
        string              username;
        SRequestParameter   username_param = x_GetParam(req, kUsernameParam);
        if (username_param.m_Found) {
            username = string(username_param.m_Value.data(),
                              username_param.m_Value.size());
        }

        if (!m_AuthToken.empty()) {
            SRequestParameter   auth_token_param = x_GetParam(req, kAuthTokenParam);

            bool    auth_good = false;
            if (auth_token_param.m_Found) {
                auth_good = m_AuthToken == string(auth_token_param.m_Value.data(),
                                                  auth_token_param.m_Value.size());
            }

            if (!auth_good) {
                msg = "Unauthorized shutdown request: invalid authorization token. ";
                if (username.empty())
                    msg += "Unknown user";
                else
                    msg += "User: " + username;
                PSG_MESSAGE(msg);

                x_SendMessageAndCompletionChunks(
                        reply, "Invalid authorization token",
                        CRequestStatus::e401_Unauthorized,
                        ePSGS_Unauthorised, eDiag_Error);
                x_PrintRequestStop(context, CRequestStatus::e401_Unauthorized);
                return 0;
            }
        }

        int                 timeout = 10; // Default: 10 sec
        SRequestParameter   timeout_param = x_GetParam(req, kTimeoutParam);
        if (timeout_param.m_Found) {
            try {
                timeout = stoi(string(timeout_param.m_Value.data(),
                                      timeout_param.m_Value.size()));
            } catch (...) {
                msg = "Invalid shutdown request: cannot convert timeout to an integer. ";
                if (username.empty())
                    msg += "Unknown user";
                else
                    msg += "User: " + username;
                PSG_MESSAGE(msg);

                x_SendMessageAndCompletionChunks(
                        reply, "Invalid timeout (must be a positive integer)",
                        CRequestStatus::e400_BadRequest,
                        ePSGS_MalformedParameter, eDiag_Error);
                x_PrintRequestStop(context, CRequestStatus::e400_BadRequest);
                return 0;
            }

            if (timeout < 0) {
                msg = "Invalid shutdown request: timeout must be >= 0. ";
                if (username.empty())
                    msg += "Unknown user";
                else
                    msg += "User: " + username;
                PSG_MESSAGE(msg);

                x_SendMessageAndCompletionChunks(
                        reply, "Invalid timeout (must be a positive integer)",
                        CRequestStatus::e400_BadRequest,
                        ePSGS_MalformedParameter, eDiag_Error);
                x_PrintRequestStop(context, CRequestStatus::e400_BadRequest);
                return 0;
            }
        }

        auto        http_reply = reply->GetHttpReply();
        http_reply->SetContentType(ePSGS_PlainTextMime);
        if (timeout == 0) {
            // Immediate shutdown is requested
            msg = "Immediate shutdown request received from ";
            if (username.empty())
                msg += "an unknown user";
            else
                msg += "user " + username;
            PSG_MESSAGE(msg);

            http_reply->Send202(s_ImmediateShutdown, s_ImmediateShutdownSize);
            x_PrintRequestStop(context, CRequestStatus::e202_Accepted);
            exit(0);
        }

        msg = "Graceful shutdown request received from ";
        if (username.empty())
            msg += "an unknown user";
        else
            msg += "user " + username;

        auto        now = chrono::steady_clock::now();
        auto        expiration = now + chrono::seconds(timeout);
        if (g_ShutdownData.m_ShutdownRequested) {
            // Consequest shutdown request
            if (expiration >= g_ShutdownData.m_Expired) {
                msg += ". The previous shutdown expiration is shorter "
                       "than this one. Ignored.";
                PSG_MESSAGE(msg);
                http_reply->Send409(msg.c_str());
                x_PrintRequestStop(context, CRequestStatus::e409_Conflict);
                return 0;
            }
        }

        // New shutdown request or a shorter expiration request
        PSG_MESSAGE(msg);

        http_reply->Send202(s_GracefulShutdown, s_GracefulShutdownSize);
        x_PrintRequestStop(context, CRequestStatus::e202_Accepted);

        g_ShutdownData.m_Expired = expiration;
        g_ShutdownData.m_ShutdownRequested = true;
    } catch (const exception &  exc) {
        string      msg = "Exception when handling a shutdown request: " +
                          string(exc.what());
        x_SendMessageAndCompletionChunks(reply, msg,
                                         CRequestStatus::e500_InternalServerError,
                                         ePSGS_ShutdownError, eDiag_Error);
        PSG_ERROR(msg);
        x_PrintRequestStop(context, CRequestStatus::e500_InternalServerError);
    } catch (...) {
        string      msg = "Unknown exception when handling a shutdown request";
        x_SendMessageAndCompletionChunks(reply, msg,
                                         CRequestStatus::e500_InternalServerError,
                                         ePSGS_ShutdownError, eDiag_Error);
        PSG_ERROR(msg);
        x_PrintRequestStop(context, CRequestStatus::e500_InternalServerError);
    }
    return 0;
}


int CPubseqGatewayApp::OnGetAlerts(CHttpRequest &  req,
                                   shared_ptr<CPSGS_Reply>  reply)
{
    // NOTE: expected to work regardless of the shutdown request

    CRequestContextResetter context_resetter;
    CRef<CRequestContext>   context = x_CreateRequestContext(req);

    auto        http_reply = reply->GetHttpReply();
    try {
        m_Counters.Increment(CPSGSCounters::ePSGS_AdminRequest);

        string      content = m_Alerts.Serialize().Repr(CJsonNode::fStandardJson);

        http_reply->SetContentType(ePSGS_JsonMime);
        http_reply->SetContentLength(content.length());
        http_reply->SendOk(content.c_str(), content.length(), false);

        x_PrintRequestStop(context, CRequestStatus::e200_Ok);
    } catch (const exception &  exc) {
        string      msg = "Exception when handling a get alerts request: " +
                          string(exc.what());
        x_SendMessageAndCompletionChunks(reply, msg,
                                         CRequestStatus::e500_InternalServerError,
                                         ePSGS_GetAlertsError, eDiag_Error);
        PSG_ERROR(msg);
        x_PrintRequestStop(context, CRequestStatus::e500_InternalServerError);
    } catch (...) {
        string      msg = "Unknown exception when handling a get alerts request";
        x_SendMessageAndCompletionChunks(reply, msg,
                                         CRequestStatus::e500_InternalServerError,
                                         ePSGS_GetAlertsError, eDiag_Error);
        PSG_ERROR(msg);
        x_PrintRequestStop(context, CRequestStatus::e500_InternalServerError);
    }
    return 0;
}


int CPubseqGatewayApp::OnAckAlert(CHttpRequest &  req,
                                  shared_ptr<CPSGS_Reply>  reply)
{
    CRequestContextResetter context_resetter;
    CRef<CRequestContext>   context = x_CreateRequestContext(req);

    if (x_IsShuttingDown(reply)) {
        x_PrintRequestStop(context, CRequestStatus::e503_ServiceUnavailable);
        return 0;
    }

    string                  msg;

    try {
        SRequestParameter   alert_param = x_GetParam(req, kAlertParam);
        if (!alert_param.m_Found) {
            msg = "Missing " + kAlertParam + " parameter";
            x_SendMessageAndCompletionChunks(
                    reply, msg, CRequestStatus::e400_BadRequest,
                    ePSGS_InsufficientArguments, eDiag_Error);
            PSG_ERROR(msg);
            x_PrintRequestStop(context, CRequestStatus::e400_BadRequest);
            return 0;
        }

        SRequestParameter   username_param = x_GetParam(req, kUsernameParam);
        if (!username_param.m_Found) {
            msg = "Missing " + kUsernameParam + " parameter";
            x_SendMessageAndCompletionChunks(
                    reply, msg, CRequestStatus::e400_BadRequest,
                    ePSGS_InsufficientArguments, eDiag_Error);
            PSG_ERROR(msg);
            x_PrintRequestStop(context, CRequestStatus::e400_BadRequest);
            return 0;
        }

        string  alert(alert_param.m_Value.data(), alert_param.m_Value.size());
        string  username(username_param.m_Value.data(), username_param.m_Value.size());
        auto    http_reply = reply->GetHttpReply();

        switch (m_Alerts.Acknowledge(alert, username)) {
            case ePSGS_AlertNotFound:
                msg = "Alert " + alert + " is not found";
                x_SendMessageAndCompletionChunks(
                        reply, msg,
                        CRequestStatus::e404_NotFound,
                        ePSGS_MalformedParameter, eDiag_Error);
                PSG_ERROR(msg);
                x_PrintRequestStop(context, CRequestStatus::e404_NotFound);
                break;
            case ePSGS_AlertAlreadyAcknowledged:
                http_reply->SetContentType(ePSGS_PlainTextMime);
                msg = "Alert " + alert + " has already been acknowledged";
                http_reply->SendOk(msg.c_str(), msg.size(), false);
                x_PrintRequestStop(context, CRequestStatus::e200_Ok);
                break;
            case ePSGS_AlertAcknowledged:
                http_reply->SetContentType(ePSGS_PlainTextMime);
                http_reply->SendOk(nullptr, 0, true);
                x_PrintRequestStop(context, CRequestStatus::e200_Ok);
                break;
        }
    } catch (const exception &  exc) {
        string      msg = "Exception when handling an acknowledge alert request: " +
                          string(exc.what());
        x_SendMessageAndCompletionChunks(reply, msg,
                                         CRequestStatus::e500_InternalServerError,
                                         ePSGS_AckAlertError,
                                         eDiag_Error);
        PSG_ERROR(msg);
        x_PrintRequestStop(context, CRequestStatus::e500_InternalServerError);
    } catch (...) {
        string      msg = "Unknown exception when handling an acknowledge alert request";
        x_SendMessageAndCompletionChunks(reply, msg,
                                         CRequestStatus::e500_InternalServerError,
                                         ePSGS_AckAlertError,
                                         eDiag_Error);
        PSG_ERROR(msg);
        x_PrintRequestStop(context, CRequestStatus::e500_InternalServerError);
    }
    return 0;
}

int CPubseqGatewayApp::OnStatistics(CHttpRequest &  req,
                                    shared_ptr<CPSGS_Reply>  reply)
{
    CRequestContextResetter context_resetter;
    CRef<CRequestContext>   context = x_CreateRequestContext(req);
    auto                    http_reply = reply->GetHttpReply();

    if (x_IsShuttingDown(reply)) {
        x_PrintRequestStop(context, CRequestStatus::e503_ServiceUnavailable);
        return 0;
    }

    try {
        m_Counters.Increment(CPSGSCounters::ePSGS_AdminRequest);

        // /ADMIN/statistics[?reset=yes(dflt=no)][&most_recent_time=<time>][&most_ancient_time=<time>][histogram_names=name1,name2,...]
        // /ADMIN/statistics[?reset=yes(dflt=no)][&most_recent_time=<time>][&most_ancient_time=<time>]


        bool                    reset = false;
        SRequestParameter       reset_param = x_GetParam(req, kResetParam);
        if (reset_param.m_Found) {
            string      err_msg;
            if (!x_IsBoolParamValid(kResetParam, reset_param.m_Value, err_msg)) {
                x_SendMessageAndCompletionChunks(
                        reply, err_msg, CRequestStatus::e400_BadRequest,
                        ePSGS_MalformedParameter, eDiag_Error);
                PSG_ERROR(err_msg);
                x_PrintRequestStop(context, CRequestStatus::e400_BadRequest);
                return 0;
            }
            reset = reset_param.m_Value == "yes";
        }

        if (reset) {
            m_Timing->Reset();
            http_reply->SetContentType(ePSGS_PlainTextMime);
            http_reply->SendOk(nullptr, 0, true);
            x_PrintRequestStop(context, CRequestStatus::e200_Ok);
            return 0;
        }

        int                 most_recent_time = INT_MIN;
        SRequestParameter   most_recent_time_param = x_GetParam(req, kMostRecentTimeParam);
        if (most_recent_time_param.m_Found) {
            string          err_msg;
            try {
                most_recent_time = NStr::StringToInt8(most_recent_time_param.m_Value);
                if (most_recent_time < 0)
                    err_msg = "Invalid " + kMostRecentTimeParam +
                              " value (" + most_recent_time_param.m_Value +
                              "). It must be >= 0.";
            } catch (...) {
                err_msg = "Invalid " + kMostRecentTimeParam +
                          " value (" + most_recent_time_param.m_Value +
                          "). It must be an integer >= 0.";
            }

            if (!err_msg.empty()) {
                x_SendMessageAndCompletionChunks(
                        reply, err_msg, CRequestStatus::e400_BadRequest,
                        ePSGS_MalformedParameter, eDiag_Error);
                PSG_ERROR(err_msg);
                x_PrintRequestStop(context, CRequestStatus::e400_BadRequest);
                return 0;
            }
        }

        int                 most_ancient_time = INT_MIN;
        SRequestParameter   most_ancient_time_param = x_GetParam(req, kMostAncientTimeParam);
        if (most_ancient_time_param.m_Found) {
            string          err_msg;
            try {
                most_ancient_time = NStr::StringToInt8(most_ancient_time_param.m_Value);
                if (most_ancient_time < 0)
                    err_msg = "Invalid " + kMostAncientTimeParam +
                              " value (" + most_ancient_time_param.m_Value +
                              "). It must be >= 0.";
            } catch (...) {
                err_msg = "Invalid " + kMostAncientTimeParam +
                          " value (" + most_ancient_time_param.m_Value +
                          "). It must be an integer >= 0.";
            }

            if (!err_msg.empty()) {
                x_SendMessageAndCompletionChunks(
                        reply, err_msg, CRequestStatus::e400_BadRequest,
                        ePSGS_MalformedParameter, eDiag_Error);
                PSG_ERROR(err_msg);
                x_PrintRequestStop(context, CRequestStatus::e400_BadRequest);
                return 0;
            }
        }

        if (most_ancient_time >= 0 && most_recent_time >= 0) {
            // auto reorder the time if needed
            if (most_recent_time > most_ancient_time) {
                swap(most_recent_time, most_ancient_time);
            }
        }

        vector<CTempString> histogram_names;
        SRequestParameter   histogram_names_param = x_GetParam(req, kHistogramNamesParam);
        if (histogram_names_param.m_Found) {
            NStr::Split(histogram_names_param.m_Value, ",", histogram_names);
        }


        CJsonNode   timing(m_Timing->Serialize(most_ancient_time,
                                               most_recent_time,
                                               histogram_names,
                                               m_TickSpan));
        string      content = timing.Repr(CJsonNode::fStandardJson);

        http_reply->SetContentType(ePSGS_JsonMime);
        http_reply->SetContentLength(content.length());
        http_reply->SendOk(content.c_str(), content.length(), false);

        x_PrintRequestStop(context, CRequestStatus::e200_Ok);
    } catch (const exception &  exc) {
        string      msg = "Exception when handling a statistics request: " +
                          string(exc.what());
        x_SendMessageAndCompletionChunks(reply, msg,
                                         CRequestStatus::e500_InternalServerError,
                                         ePSGS_StatisticsError,
                                         eDiag_Error);
        PSG_ERROR(msg);
        x_PrintRequestStop(context, CRequestStatus::e500_InternalServerError);
    } catch (...) {
        string  msg = "Unknown exception when handling a statistics request";
        x_SendMessageAndCompletionChunks(reply, msg,
                                         CRequestStatus::e500_InternalServerError,
                                         ePSGS_StatisticsError,
                                         eDiag_Error);
        PSG_ERROR(msg);
        x_PrintRequestStop(context, CRequestStatus::e500_InternalServerError);
    }
    return 0;
}


int CPubseqGatewayApp::OnTestIO(CHttpRequest &  req,
                                shared_ptr<CPSGS_Reply>  reply)
{
    bool                    need_log = false;   // default
    CRequestContextResetter context_resetter;
    CRef<CRequestContext>   context;
    auto                    http_reply = reply->GetHttpReply();

    if (x_IsShuttingDown(reply)) {
        x_PrintRequestStop(context, CRequestStatus::e503_ServiceUnavailable);
        return 0;
    }

    try {
        string                  err_msg;

        SRequestParameter       log_param = x_GetParam(req, kLogParam);
        if (log_param.m_Found) {
            if (!x_IsBoolParamValid(kLogParam, log_param.m_Value, err_msg)) {
                x_SendMessageAndCompletionChunks(
                        reply, err_msg, CRequestStatus::e400_BadRequest,
                        ePSGS_MalformedParameter, eDiag_Error);
                PSG_ERROR(err_msg);
                return 0;
            }
            need_log = log_param.m_Value == "yes";
        }

        if (need_log)
            context = x_CreateRequestContext(req);

        // Read the return data size
        SRequestParameter   data_size_param = x_GetParam(req, kDataSizeParam);
        long                data_size = 0;
        if (data_size_param.m_Found) {
            data_size = NStr::StringToLong(data_size_param.m_Value);
            if (data_size < 0 || data_size > kMaxTestIOSize) {
                err_msg = "Invalid range of the '" + kDataSizeParam +
                          "' parameter. Accepted values are 0..." +
                          to_string(kMaxTestIOSize);
                x_SendMessageAndCompletionChunks(
                        reply, err_msg, CRequestStatus::e400_BadRequest,
                        ePSGS_MalformedParameter, eDiag_Error);
                if (need_log) {
                    PSG_WARNING(err_msg);
                    x_PrintRequestStop(context,
                                       CRequestStatus::e400_BadRequest);
                }
                return 0;
            }
        } else {
            err_msg = "The '" + kDataSizeParam + "' must be provided";
            x_SendMessageAndCompletionChunks(
                    reply, err_msg, CRequestStatus::e400_BadRequest,
                    ePSGS_InsufficientArguments, eDiag_Error);
            if (need_log) {
                PSG_WARNING(err_msg);
                x_PrintRequestStop(context, CRequestStatus::e400_BadRequest);
            }
            return 0;
        }

        m_Counters.Increment(CPSGSCounters::ePSGS_TestIORequest);

        http_reply->SetContentType(ePSGS_BinaryMime);
        http_reply->SetContentLength(data_size);

        // true: persistent
        http_reply->SendOk(m_IOTestBuffer.get(), data_size, true);

        if (need_log)
            x_PrintRequestStop(context, CRequestStatus::e200_Ok);
    } catch (const exception &  exc) {
        string      msg = "Exception when handling a test io request: " +
                          string(exc.what());
        x_SendMessageAndCompletionChunks(reply, msg,
                                         CRequestStatus::e500_InternalServerError,
                                         ePSGS_TestIOError,
                                         eDiag_Error);
        PSG_ERROR(msg);
        if (need_log)
            x_PrintRequestStop(context, CRequestStatus::e500_InternalServerError);
    } catch (...) {
        string  msg = "Unknown exception when handling a test io request";
        x_SendMessageAndCompletionChunks(reply, msg,
                                         CRequestStatus::e500_InternalServerError,
                                         ePSGS_TestIOError,
                                         eDiag_Error);
        PSG_ERROR(msg);
        if (need_log)
            x_PrintRequestStop(context, CRequestStatus::e500_InternalServerError);
    }
    return 0;
}


bool CPubseqGatewayApp::x_ProcessCommonGetAndResolveParams(
        CHttpRequest &  req,
        shared_ptr<CPSGS_Reply>  reply,
        CTempString &  seq_id,
        int &  seq_id_type,
        SPSGS_RequestBase::EPSGS_CacheAndDbUse &  use_cache)
{
    SRequestParameter   seq_id_type_param;
    string              err_msg;

    // Check the mandatory parameter presence
    SRequestParameter   seq_id_param = x_GetParam(req, kSeqIdParam);
    if (!seq_id_param.m_Found) {
        err_msg = "Missing the '" + kSeqIdParam + "' parameter";
        m_Counters.Increment(CPSGSCounters::ePSGS_InsufficientArgs);
    }
    else if (seq_id_param.m_Value.empty()) {
        err_msg = "Missing value of the '" + kSeqIdParam + "' parameter";
        m_Counters.Increment(CPSGSCounters::ePSGS_MalformedArgs);
    }

    if (err_msg.empty()) {
        use_cache = x_GetUseCacheParameter(req, err_msg);
    }

    if (!err_msg.empty()) {
        x_SendMessageAndCompletionChunks(reply, err_msg,
                                         CRequestStatus::e400_BadRequest,
                                         ePSGS_MissingParameter, eDiag_Error);
        PSG_WARNING(err_msg);
        return false;
    }
    seq_id = seq_id_param.m_Value;

    seq_id_type_param = x_GetParam(req, kSeqIdTypeParam);
    if (seq_id_type_param.m_Found) {
        if (!x_ConvertIntParameter(kSeqIdTypeParam, seq_id_type_param.m_Value,
                                   seq_id_type, err_msg)) {
            m_Counters.Increment(CPSGSCounters::ePSGS_MalformedArgs);
            x_SendMessageAndCompletionChunks(reply, err_msg,
                                             CRequestStatus::e400_BadRequest,
                                             ePSGS_MalformedParameter,
                                             eDiag_Error);
            PSG_WARNING(err_msg);
            return false;
        }

        if (seq_id_type < 0 || seq_id_type >= CSeq_id::e_MaxChoice) {
            err_msg = "The '" + kSeqIdTypeParam +
                      "' value must be >= 0 and less than " +
                      to_string(CSeq_id::e_MaxChoice);
            m_Counters.Increment(CPSGSCounters::ePSGS_MalformedArgs);
            x_SendMessageAndCompletionChunks(reply, err_msg,
                                             CRequestStatus::e400_BadRequest,
                                             ePSGS_MalformedParameter,
                                             eDiag_Error);
            PSG_WARNING(err_msg);
            return false;
        }
    } else {
        seq_id_type = -1;
    }

    return true;
}


SPSGS_RequestBase::EPSGS_CacheAndDbUse
CPubseqGatewayApp::x_GetUseCacheParameter(CHttpRequest &  req,
                                          string &  err_msg)
{
    SRequestParameter   use_cache_param = x_GetParam(req, kUseCacheParam);

    if (use_cache_param.m_Found) {
        if (!x_IsBoolParamValid(kUseCacheParam, use_cache_param.m_Value,
                                err_msg)) {
            m_Counters.Increment(CPSGSCounters::ePSGS_MalformedArgs);
            return SPSGS_RequestBase::ePSGS_UnknownUseCache;
        }
        if (use_cache_param.m_Value == "yes")
            return SPSGS_RequestBase::ePSGS_CacheOnly;
        return SPSGS_RequestBase::ePSGS_DbOnly;
    }
    return SPSGS_RequestBase::ePSGS_CacheAndDb;
}


int
CPubseqGatewayApp::x_GetSendBlobIfSmallParameter(CHttpRequest &  req,
                                                 shared_ptr<CPSGS_Reply>  reply)
{
    int                 send_blob_if_small_value = 0;   // default
    SRequestParameter   send_blob_if_small_param = x_GetParam(req, kSendBlobIfSmallParam);

    if (send_blob_if_small_param.m_Found) {
        string      err_msg;
        if (!x_ConvertIntParameter(kSendBlobIfSmallParam,
                                   send_blob_if_small_param.m_Value,
                                   send_blob_if_small_value, err_msg)) {
            m_Counters.Increment(CPSGSCounters::ePSGS_MalformedArgs);
            x_SendMessageAndCompletionChunks(reply, err_msg,
                                             CRequestStatus::e400_BadRequest,
                                             ePSGS_MalformedParameter,
                                             eDiag_Error);
            PSG_WARNING(err_msg);
            return -1;
        }

        if (send_blob_if_small_value < 0) {
            m_Counters.Increment(CPSGSCounters::ePSGS_MalformedArgs);
            err_msg = "Invalid " + kSendBlobIfSmallParam +
                      " value. It must be an integer >= 0";
            x_SendMessageAndCompletionChunks(reply, err_msg,
                                             CRequestStatus::e400_BadRequest,
                                             ePSGS_MalformedParameter,
                                             eDiag_Error);
            PSG_WARNING(err_msg);
            return -1;
        }
    }

    return send_blob_if_small_value;
}


bool CPubseqGatewayApp::x_IsShuttingDown(shared_ptr<CPSGS_Reply>  reply)
{
    if (g_ShutdownData.m_ShutdownRequested) {
        string      msg = "The server is in process of shutting down";
        x_SendMessageAndCompletionChunks(reply, msg,
                                         CRequestStatus::e503_ServiceUnavailable,
                                         ePSGS_ShuttingDown,
                                         eDiag_Error);
        PSG_ERROR(msg);
        return true;
    }
    return false;
}


void CPubseqGatewayApp::x_DispatchRequest(shared_ptr<CPSGS_Request>  request,
                                          shared_ptr<CPSGS_Reply>  reply)
{
    // The dispatcher works in terms of processors while the infrastructure
    // works in terms of pending operation. So, lets create the corresponding
    // pending operations.
    // Note: the case when no processors are available is handled in the
    //       dispatcher (reply is sent to the client).

    list<unique_ptr<CPendingOperation>>     pending_ops;
    for (auto processor: m_RequestDispatcher.DispatchRequest(request, reply)) {
        pending_ops.push_back(
            move(
                unique_ptr<CPendingOperation>(
                    new CPendingOperation(request, reply, processor))));
    }

    if (!pending_ops.empty()) {
        auto    http_conn = reply->GetHttpReply()->GetHttpConnection();
        http_conn->Postpone(move(pending_ops), reply);
    }
}

