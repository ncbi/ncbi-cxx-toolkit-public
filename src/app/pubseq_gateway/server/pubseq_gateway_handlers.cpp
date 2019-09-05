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

#include "shutdown_data.hpp"
extern SShutdownData    g_ShutdownData;


USING_NCBI_SCOPE;


static string  kPsgProtocolParam = "psg_protocol";
static string  kFmtParam = "fmt";
static string  kBlobIdParam = "blob_id";
static string  kLastModifiedParam = "last_modified";
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
static string  kChunkParam = "chunk";
static string  kSplitVersionParam = "split_version";
static vector<pair<string, EServIncludeData>>   kResolveFlagParams =
{
    make_pair("all_info", fServAllBioseqFields),   // must be first

    make_pair("canon_id", fServCanonicalId),
    make_pair("seq_ids", fServSeqIds),
    make_pair("mol_type", fServMoleculeType),
    make_pair("length", fServLength),
    make_pair("state", fServState),
    make_pair("blob_id", fServBlobId),
    make_pair("tax_id", fServTaxId),
    make_pair("hash", fServHash),
    make_pair("date_changed", fServDateChanged)
};
static string  kBadUrlMessage = "Unknown request, the provided URL "
                                "is not recognized";


int CPubseqGatewayApp::OnBadURL(HST::CHttpRequest &  req,
                                HST::CHttpReply<CPendingOperation> &  resp)
{
    if (x_IsShuttingDown(req, resp))
        return 0;

    CRequestContextResetter context_resetter;
    CRef<CRequestContext>   context = x_CreateRequestContext(req);

    try {
        // Reply should use PSG protocol
        resp.SetContentType(ePSGMime);

        m_ErrorCounters.IncBadUrlPath();
        x_SendMessageAndCompletionChunks(resp, kBadUrlMessage,
                                         CRequestStatus::e400_BadRequest, eBadURL,
                                         eDiag_Error);

        PSG_WARNING(kBadUrlMessage);
        x_PrintRequestStop(context, CRequestStatus::e400_BadRequest);
    } catch (const exception &  exc) {
        string      msg = "Exception when handling a bad URL event: " +
                          string(exc.what());
        resp.SetContentType(ePlainTextMime);
        resp.Send500("Internal Server Error", msg.c_str());
        x_PrintRequestStop(context, CRequestStatus::e500_InternalServerError);
    } catch (...) {
        resp.SetContentType(ePlainTextMime);
        resp.Send500("Internal Server Error",
                     "Unknown exception when handling a bad URL event");
        x_PrintRequestStop(context, CRequestStatus::e500_InternalServerError);
    }
    return 0;
}


int CPubseqGatewayApp::OnGet(HST::CHttpRequest &  req,
                             HST::CHttpReply<CPendingOperation> &  resp)
{
    if (x_IsShuttingDown(req, resp))
        return 0;

    if (!x_IsDBOK(req, resp))
        return 0;

    auto                    now = chrono::high_resolution_clock::now();
    CRequestContextResetter context_resetter;
    CRef<CRequestContext>   context = x_CreateRequestContext(req);

    try {
        // Reply should use PSG protocol
        resp.SetContentType(ePSGMime);

        CTempString             seq_id;
        int                     seq_id_type;
        ECacheAndCassandraUse   use_cache;

        if (!x_ProcessCommonGetAndResolveParams(req, resp, seq_id,
                                                seq_id_type, use_cache, true)) {
            x_PrintRequestStop(context, CRequestStatus::e400_BadRequest);
            return 0;
        }

        string              err_msg;
        ETSEOption          tse_option = eOrigTSE;
        SRequestParameter   tse_param = x_GetParam(req, kTSEParam);
        if (tse_param.m_Found) {
            tse_option = x_GetTSEOption(kTSEParam, tse_param.m_Value, err_msg);
            if (tse_option == eUnknownTSE) {
                x_MalformedArguments(resp, context, err_msg);
                return 0;
            }
        }

        vector<SBlobId>     exclude_blobs;
        SRequestParameter   exclude_blobs_param = x_GetParam(req,
                                                             kExcludeBlobsParam);
        if (exclude_blobs_param.m_Found) {
            exclude_blobs = x_GetExcludeBlobs(kExcludeBlobsParam,
                                              exclude_blobs_param.m_Value, err_msg);
            if (!err_msg.empty()) {
                x_MalformedArguments(resp, context, err_msg);
                return 0;
            }
        }

        SRequestParameter   client_id_param = x_GetParam(req, kClientIdParam);

        m_RequestCounters.IncGetBlobBySeqId();
        resp.Postpone(
                CPendingOperation(
                    SBlobRequest(seq_id, seq_id_type, exclude_blobs,
                                 tse_option, use_cache,
                                 string(client_id_param.m_Value.data(),
                                        client_id_param.m_Value.size()),
                                 now),
                    0, m_CassConnection, m_TimeoutMs,
                    m_MaxRetries, context));
    } catch (const exception &  exc) {
        string      msg = "Exception when handling a get request: " +
                          string(exc.what());
        x_SendMessageAndCompletionChunks(resp, msg,
                                         CRequestStatus::e500_InternalServerError,
                                         eUnknownError, eDiag_Error);
        x_PrintRequestStop(context, CRequestStatus::e500_InternalServerError);
    } catch (...) {
        string      msg = "Unknown exception when handling a get request";
        x_SendMessageAndCompletionChunks(resp, msg,
                                         CRequestStatus::e500_InternalServerError,
                                         eUnknownError, eDiag_Error);
        x_PrintRequestStop(context, CRequestStatus::e500_InternalServerError);
    }
    return 0;
}


int CPubseqGatewayApp::OnGetBlob(HST::CHttpRequest &  req,
                                 HST::CHttpReply<CPendingOperation> &  resp)
{
    if (x_IsShuttingDown(req, resp))
        return 0;

    if (!x_IsDBOK(req, resp))
        return 0;

    CRequestContextResetter context_resetter;
    CRef<CRequestContext>   context = x_CreateRequestContext(req);

    try {
        // Reply should use PSG protocol
        resp.SetContentType(ePSGMime);

        string              err_msg;
        ETSEOption          tse_option = eOrigTSE;
        SRequestParameter   tse_param = x_GetParam(req, kTSEParam);
        if (tse_param.m_Found) {
            tse_option = x_GetTSEOption(kTSEParam, tse_param.m_Value, err_msg);
            if (tse_option == eUnknownTSE) {
                x_MalformedArguments(resp, context, err_msg);
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
                x_MalformedArguments(resp, context,
                                     "Malformed 'last_modified' parameter. "
                                     "Expected an integer");
                return 0;
            }
        }

        ECacheAndCassandraUse   use_cache = x_GetUseCacheParameter(req, err_msg);
        if (!err_msg.empty()) {
            x_MalformedArguments(resp, context, err_msg);
            return 0;
        }

        SRequestParameter   blob_id_param = x_GetParam(req, kBlobIdParam);
        if (blob_id_param.m_Found)
        {
            SBlobId     blob_id(blob_id_param.m_Value);

            if (!blob_id.IsValid()) {
                x_MalformedArguments(resp, context,
                                     "Malformed 'blob_id' parameter. "
                                     "Expected format 'sat.sat_key' where both "
                                     "'sat' and 'sat_key' are integers.");
                return 0;
            }

            if (SatToSatName(blob_id.m_Sat, blob_id.m_SatName)) {
                SRequestParameter   client_id_param = x_GetParam(req,
                                                                 kClientIdParam);

                m_RequestCounters.IncGetBlobBySatSatKey();
                resp.Postpone(
                        CPendingOperation(
                            SBlobRequest(blob_id, last_modified_value,
                                         tse_option, use_cache,
                                         string(client_id_param.m_Value.data(),
                                                client_id_param.m_Value.size())),
                            0, m_CassConnection, m_TimeoutMs,
                            m_MaxRetries, context));

                return 0;
            }

            m_ErrorCounters.IncClientSatToSatName();
            string      message = string("Unknown satellite number ") +
                                  NStr::NumericToString(blob_id.m_Sat);
            x_SendUnknownClientSatelliteError(resp, blob_id, message);
            PSG_WARNING(message);
            x_PrintRequestStop(context, CRequestStatus::e404_NotFound);
            return 0;
        }

        x_InsufficientArguments(resp, context, "Mandatory parameter "
                                "'blob_id' is not found.");
    } catch (const exception &  exc) {
        string      msg = "Exception when handling a getblob request: " +
                          string(exc.what());
        x_SendMessageAndCompletionChunks(resp, msg,
                                         CRequestStatus::e500_InternalServerError,
                                         eUnknownError, eDiag_Error);
        x_PrintRequestStop(context, CRequestStatus::e500_InternalServerError);
    } catch (...) {
        string      msg = "Unknown exception when handling a getblob request";
        x_SendMessageAndCompletionChunks(resp, msg,
                                         CRequestStatus::e500_InternalServerError,
                                         eUnknownError, eDiag_Error);
        x_PrintRequestStop(context, CRequestStatus::e500_InternalServerError);
    }
    return 0;
}


int CPubseqGatewayApp::OnResolve(HST::CHttpRequest &  req,
                                 HST::CHttpReply<CPendingOperation> &  resp)
{
    if (x_IsShuttingDown(req, resp))
        return 0;

    if (!x_IsDBOK(req, resp))
        return 0;

    auto                    now = chrono::high_resolution_clock::now();
    CRequestContextResetter context_resetter;
    CRef<CRequestContext>   context = x_CreateRequestContext(req);

    bool                use_psg_protocol = false;   // default

    try {
        SRequestParameter   psg_protocol_param = x_GetParam(req, kPsgProtocolParam);

        if (psg_protocol_param.m_Found) {
            string      err_msg;
            if (!x_IsBoolParamValid(kPsgProtocolParam,
                                    psg_protocol_param.m_Value, err_msg)) {
                m_ErrorCounters.IncMalformedArguments();
                resp.SetContentType(ePlainTextMime);
                resp.Send400("Bad Request", err_msg.c_str());
                PSG_WARNING(err_msg);
                x_PrintRequestStop(context, CRequestStatus::e400_BadRequest);
                return 0;
            }
            use_psg_protocol = psg_protocol_param.m_Value == "yes";
            if (use_psg_protocol)
                resp.SetContentType(ePSGMime);
        }


        CTempString             seq_id;
        int                     seq_id_type;
        ECacheAndCassandraUse   use_cache;

        if (!x_ProcessCommonGetAndResolveParams(req, resp, seq_id,
                                                seq_id_type, use_cache,
                                                use_psg_protocol)) {
            x_PrintRequestStop(context, CRequestStatus::e400_BadRequest);
            return 0;
        }

        string              err_msg;
        EOutputFormat       output_format = eNativeFormat;
        SRequestParameter   fmt_param = x_GetParam(req, kFmtParam);
        if (fmt_param.m_Found) {
            output_format = x_GetOutputFormat(kFmtParam, fmt_param.m_Value, err_msg);
            if (output_format == eUnknownFormat) {
                m_ErrorCounters.IncMalformedArguments();
                if (use_psg_protocol) {
                    x_SendMessageAndCompletionChunks(resp, err_msg,
                                                     CRequestStatus::e400_BadRequest,
                                                     eMalformedParameter, eDiag_Error);
                } else {
                    resp.SetContentType(ePlainTextMime);
                    resp.Send400("Bad Request", err_msg.c_str());
                }

                PSG_WARNING(err_msg);
                x_PrintRequestStop(context, CRequestStatus::e400_BadRequest);
                return 0;
            }
        }

        TServIncludeData        include_data_flags = 0;
        SRequestParameter       request_param;
        for (const auto &  flag_param: kResolveFlagParams) {
            request_param = x_GetParam(req, flag_param.first);
            if (request_param.m_Found) {
                if (!x_IsBoolParamValid(flag_param.first,
                                        request_param.m_Value, err_msg)) {
                    m_ErrorCounters.IncMalformedArguments();
                    if (use_psg_protocol) {
                        x_SendMessageAndCompletionChunks(resp, err_msg,
                                                         CRequestStatus::e400_BadRequest,
                                                         eMalformedParameter, eDiag_Error);
                    } else {
                        resp.SetContentType(ePlainTextMime);
                        resp.Send400("Bad Request", err_msg.c_str());
                    }

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

        // Parameters processing has finished
        m_RequestCounters.IncResolve();
        resp.Postpone(
                CPendingOperation(
                    SResolveRequest(seq_id, seq_id_type, include_data_flags,
                                    output_format, use_cache, use_psg_protocol,
                                    now),
                    0, m_CassConnection, m_TimeoutMs,
                    m_MaxRetries, context));
    } catch (const exception &  exc) {
        string      msg = "Exception when handling a resolve request: " +
                          string(exc.what());
        if (use_psg_protocol) {
            x_SendMessageAndCompletionChunks(resp, msg,
                                             CRequestStatus::e500_InternalServerError,
                                             eUnknownError, eDiag_Error);
        } else {
            resp.SetContentType(ePlainTextMime);
            resp.Send500("Internal Server Error", msg.c_str());
        }
        x_PrintRequestStop(context, CRequestStatus::e500_InternalServerError);
    } catch (...) {
        string      msg = "Unknown exception when handling a resolve request";
        if (use_psg_protocol) {
            x_SendMessageAndCompletionChunks(resp, msg,
                                             CRequestStatus::e500_InternalServerError,
                                             eUnknownError, eDiag_Error);
        } else {
            resp.SetContentType(ePlainTextMime);
            resp.Send500("Internal Server Error", msg.c_str());
        }
        x_PrintRequestStop(context, CRequestStatus::e500_InternalServerError);
    }
    return 0;
}


int CPubseqGatewayApp::OnGetTSEChunk(HST::CHttpRequest &  req,
                                     HST::CHttpReply<CPendingOperation> &  resp)
{
    if (x_IsShuttingDown(req, resp))
        return 0;

    if (!x_IsDBOK(req, resp))
        return 0;

    CRequestContextResetter context_resetter;
    CRef<CRequestContext>   context = x_CreateRequestContext(req);

    try {
        // Reply should use PSG protocol
        resp.SetContentType(ePSGMime);

        // tse_id is in fact a blob_id...
        SRequestParameter   tse_id_param = x_GetParam(req, kTSEIdParam);
        if (!tse_id_param.m_Found)
        {
            x_InsufficientArguments(resp, context, "Mandatory parameter "
                                    "'tse_id' is not found.");
            return 0;
        }

        SBlobId     tse_id(tse_id_param.m_Value);
        if (!tse_id.IsValid()) {
            x_MalformedArguments(resp, context,
                                 "Malformed 'tse_id' parameter. "
                                 "Expected format 'sat.sat_key' where both "
                                 "'sat' and 'sat_key' are integers.");
            return 0;
        }

        if (!SatToSatName(tse_id.m_Sat, tse_id.m_SatName)) {
            m_ErrorCounters.IncClientSatToSatName();
            string      message = string("Unknown satellite number ") +
                                  NStr::NumericToString(tse_id.m_Sat);
            x_SendUnknownClientSatelliteError(resp, tse_id, message);
            PSG_WARNING(message);
            x_PrintRequestStop(context, CRequestStatus::e404_NotFound);
            return 0;
        }

        SRequestParameter   chunk_param = x_GetParam(req, kChunkParam);
        int64_t             chunk_value = INT64_MIN;
        if (!chunk_param.m_Found)
        {
            x_InsufficientArguments(resp, context, "Mandatory parameter "
                                    "'chunk' is not found.");
            return 0;
        }

        try {
            chunk_value = NStr::StringToLong(chunk_param.m_Value);
            if (chunk_value <= 0) {
                x_MalformedArguments(resp, context,
                                     "Invalid 'chunk' parameter. "
                                     "Expected > 0");
                return 0;
            }
        } catch (...) {
            x_MalformedArguments(resp, context,
                                 "Malformed 'chunk' parameter. "
                                 "Expected an integer");
            return 0;
        }

        SRequestParameter   split_version_param = x_GetParam(req, kSplitVersionParam);
        int64_t             split_version_value = INT64_MIN;
        if (!split_version_param.m_Found)
        {
            x_InsufficientArguments(resp, context, "Mandatory parameter "
                                    "'split_version' is not found.");
            return 0;
        }

        try {
            split_version_value = NStr::StringToLong(split_version_param.m_Value);
        } catch (...) {
            x_MalformedArguments(resp, context,
                                 "Malformed 'split_version' parameter. "
                                 "Expected an integer");
            return 0;
        }

        string                  err_msg;
        ECacheAndCassandraUse   use_cache = x_GetUseCacheParameter(req, err_msg);
        if (!err_msg.empty()) {
            x_MalformedArguments(resp, context, err_msg);
            return 0;
        }

        // All parameters are good
        m_RequestCounters.IncGetTSEChunk();
        resp.Postpone(
                CPendingOperation(
                    STSEChunkRequest(tse_id, chunk_value,
                                     split_version_value, use_cache),
                    0, m_CassConnection, m_TimeoutMs,
                    m_MaxRetries, context));
    } catch (const exception &  exc) {
        string      msg = "Exception when handling a get_tse_chunk request: " +
                          string(exc.what());
        x_SendMessageAndCompletionChunks(resp, msg,
                                         CRequestStatus::e500_InternalServerError,
                                         eUnknownError, eDiag_Error);
        x_PrintRequestStop(context, CRequestStatus::e500_InternalServerError);
    } catch (...) {
        string      msg = "Unknown exception when handling a get_tse_chunk request";
        x_SendMessageAndCompletionChunks(resp, msg,
                                         CRequestStatus::e500_InternalServerError,
                                         eUnknownError, eDiag_Error);
        x_PrintRequestStop(context, CRequestStatus::e500_InternalServerError);
    }
    return 0;
}


int CPubseqGatewayApp::OnGetNA(HST::CHttpRequest &  req,
                               HST::CHttpReply<CPendingOperation> &  resp)
{
    if (x_IsShuttingDown(req, resp))
        return 0;

    if (!x_IsDBOK(req, resp))
        return 0;

    auto                    now = chrono::high_resolution_clock::now();
    CRequestContextResetter context_resetter;
    CRef<CRequestContext>   context = x_CreateRequestContext(req);

    // psg_protocol parameter must be present and must be set to true.
    // At least for now, see CXX-10258
    bool                use_psg_protocol = true;
    resp.SetContentType(ePSGMime);

    try {
        bool                psg_protocol_ok = false;
        SRequestParameter   psg_protocol_param = x_GetParam(req, kPsgProtocolParam);
        string              err_msg;

        if (psg_protocol_param.m_Found) {
            psg_protocol_ok = psg_protocol_param.m_Value == "yes";
        }

        if (!psg_protocol_ok) {
            x_MalformedArguments(resp, context,
                                 "Invalid '" + kPsgProtocolParam + "' parameter. "
                                 "It must be present and must be set to yes.");
            return 0;
        }

        CTempString             seq_id;
        int                     seq_id_type;
        ECacheAndCassandraUse   use_cache;

        if (!x_ProcessCommonGetAndResolveParams(req, resp, seq_id,
                                                seq_id_type, use_cache,
                                                true)) {
            x_PrintRequestStop(context, CRequestStatus::e400_BadRequest);
            return 0;
        }

        // At the moment the only json format is supported (native translates
        // to json). See CXX-10258
        SRequestParameter   fmt_param = x_GetParam(req, kFmtParam);
        if (fmt_param.m_Found) {
            EOutputFormat       output_format = x_GetOutputFormat(kFmtParam,
                                                                  fmt_param.m_Value,
                                                                  err_msg);
            if (output_format == eProtobufFormat || output_format == eUnknownFormat) {
                x_MalformedArguments(resp, context,
                                     "Invalid 'fmt' parameter value. The 'get_na' "
                                     "command supports 'json' and 'native' values");
                return 0;
            }
        }


        // Get the annotation names
        SRequestParameter   names_param = x_GetParam(req, kNamesParam);
        if (!names_param.m_Found) {
            x_MalformedArguments(resp, context,
                                 "The mandatory '" + kNamesParam +
                                 "' parameter is not found");
            return 0;
        }

        vector<CTempString>       names;

        NStr::Split(names_param.m_Value, ",", names);
        if (names.empty()) {
            x_MalformedArguments(resp, context,
                                 "Named annotation names are not found in the request");
            return 0;
        }

        // Parameters processing has finished
        m_RequestCounters.IncGetNA();
        resp.Postpone(
                CPendingOperation(
                    SAnnotRequest(seq_id, seq_id_type, names, use_cache, now),
                    0, m_CassConnection, m_TimeoutMs,
                    m_MaxRetries, context));
    } catch (const exception &  exc) {
        string      msg = "Exception when handling a get_na request: " +
                          string(exc.what());
        if (use_psg_protocol) {
            x_SendMessageAndCompletionChunks(resp, msg,
                                             CRequestStatus::e500_InternalServerError,
                                             eUnknownError, eDiag_Error);
        } else {
            resp.SetContentType(ePlainTextMime);
            resp.Send500("Internal Server Error", msg.c_str());
        }
        x_PrintRequestStop(context, CRequestStatus::e500_InternalServerError);
    } catch (...) {
        string      msg = "Unknown exception when handling a get_na request";
        if (use_psg_protocol) {
            x_SendMessageAndCompletionChunks(resp, msg,
                                             CRequestStatus::e500_InternalServerError,
                                             eUnknownError, eDiag_Error);
        } else {
            resp.SetContentType(ePlainTextMime);
            resp.Send500("Internal Server Error", msg.c_str());
        }
        x_PrintRequestStop(context, CRequestStatus::e500_InternalServerError);
    }
    return 0;
}


int CPubseqGatewayApp::OnConfig(HST::CHttpRequest &  req,
                                HST::CHttpReply<CPendingOperation> &  resp)
{
    // NOTE: expected to work regardless of the shutdown request

    CRequestContextResetter context_resetter;
    CRef<CRequestContext>   context = x_CreateRequestContext(req);

    try {
        m_RequestCounters.IncAdmin();

        CNcbiOstrstream             conf;
        CNcbiOstrstreamToString     converter(conf);

        GetConfig().Write(conf);

        CJsonNode   reply(CJsonNode::NewObjectNode());
        reply.SetString("ConfigurationFilePath", GetConfigPath());
        reply.SetString("Configuration", string(converter));
        string      content = reply.Repr(CJsonNode::fStandardJson);

        resp.SetContentType(eJsonMime);
        resp.SetContentLength(content.length());
        resp.SendOk(content.c_str(), content.length(), false);

        x_PrintRequestStop(context, CRequestStatus::e200_Ok);
    } catch (const exception &  exc) {
        string      msg = "Exception when handling a config request: " +
                          string(exc.what());
        resp.SetContentType(ePlainTextMime);
        resp.Send500("Internal Server Error", msg.c_str());
        x_PrintRequestStop(context, CRequestStatus::e500_InternalServerError);
    } catch (...) {
        resp.SetContentType(ePlainTextMime);
        resp.Send500("Internal Server Error",
                     "Unknown exception when handling a config request");
        x_PrintRequestStop(context, CRequestStatus::e500_InternalServerError);
    }
    return 0;
}


int CPubseqGatewayApp::OnInfo(HST::CHttpRequest &  req,
                              HST::CHttpReply<CPendingOperation> &  resp)
{
    // NOTE: expected to work regardless of the shutdown request

    static string   kNA = "n/a";

    CRequestContextResetter context_resetter;
    CRef<CRequestContext>   context = x_CreateRequestContext(req);

    auto                    app = CPubseqGatewayApp::GetInstance();
    try {
        m_RequestCounters.IncAdmin();

        CJsonNode   reply(CJsonNode::NewObjectNode());

        reply.SetInteger("PID", CDiagContext::GetPID());
        reply.SetString("ExecutablePath", GetProgramExecutablePath());
        reply.SetString("CommandLineArguments", x_GetCmdLineArguments());
        reply.SetString("StartupDataState",
                        GetCassStartupDataStateMessage(app->GetStartupDataState()));


        double      real_time;
        double      user_time;
        double      system_time;
        bool        process_time_result = CCurrentProcess::GetTimes(&real_time,
                                                                    &user_time,
                                                                    &system_time);
        if (process_time_result) {
            reply.SetDouble("RealTime", real_time);
            reply.SetDouble("UserTime", user_time);
            reply.SetDouble("SystemTime", system_time);
        } else {
            reply.SetString("RealTime", kNA);
            reply.SetString("UserTime", kNA);
            reply.SetString("SystemTime", kNA);
        }

        Uint8       physical_memory = CSystemInfo::GetTotalPhysicalMemorySize();
        if (physical_memory > 0)
            reply.SetInteger("PhysicalMemory", physical_memory);
        else
            reply.SetString("PhysicalMemory", kNA);

        CProcessBase::SMemoryUsage      mem_usage;
        bool                            mem_used_result = CCurrentProcess::GetMemoryUsage(mem_usage);
        if (mem_used_result) {
            if (mem_usage.total > 0)
                reply.SetInteger("MemoryUsedTotal", mem_usage.total);
            else
                reply.SetString("MemoryUsedTotal", kNA);

            if (mem_usage.total_peak > 0)
                reply.SetInteger("MemoryUsedTotalPeak", mem_usage.total_peak);
            else
                reply.SetString("MemoryUsedTotalPeak", kNA);

            if (mem_usage.resident > 0)
                reply.SetInteger("MemoryUsedResident", mem_usage.resident);
            else
                reply.SetString("MemoryUsedResident", kNA);

            if (mem_usage.resident_peak > 0)
                reply.SetInteger("MemoryUsedResidentPeak", mem_usage.resident_peak);
            else
                reply.SetString("MemoryUsedResidentPeak", kNA);

            if (mem_usage.shared > 0)
                reply.SetInteger("MemoryUsedShared", mem_usage.shared);
            else
                reply.SetString("MemoryUsedShared", kNA);

            if (mem_usage.data > 0)
                reply.SetInteger("MemoryUsedData", mem_usage.data);
            else
                reply.SetString("MemoryUsedData", kNA);

            if (mem_usage.stack > 0)
                reply.SetInteger("MemoryUsedStack", mem_usage.stack);
            else
                reply.SetString("MemoryUsedStack", kNA);

            if (mem_usage.text > 0)
                reply.SetInteger("MemoryUsedText", mem_usage.text);
            else
                reply.SetString("MemoryUsedText", kNA);

            if (mem_usage.lib > 0)
                reply.SetInteger("MemoryUsedLib", mem_usage.lib);
            else
                reply.SetString("MemoryUsedLib", kNA);

            if (mem_usage.swap > 0)
                reply.SetInteger("MemoryUsedSwap", mem_usage.swap);
            else
                reply.SetString("MemoryUsedSwap", kNA);
        } else {
            reply.SetString("MemoryUsedTotal", kNA);
            reply.SetString("MemoryUsedTotalPeak", kNA);
            reply.SetString("MemoryUsedResident", kNA);
            reply.SetString("MemoryUsedResidentPeak", kNA);
            reply.SetString("MemoryUsedShared", kNA);
            reply.SetString("MemoryUsedData", kNA);
            reply.SetString("MemoryUsedStack", kNA);
            reply.SetString("MemoryUsedText", kNA);
            reply.SetString("MemoryUsedLib", kNA);
            reply.SetString("MemoryUsedSwap", kNA);
        }

        int         proc_fd_soft_limit;
        int         proc_fd_hard_limit;
        int         proc_fd_used =
                CCurrentProcess::GetFileDescriptorsCount(&proc_fd_soft_limit,
                                                         &proc_fd_hard_limit);

        if (proc_fd_soft_limit >= 0)
            reply.SetInteger("ProcFDSoftLimit", proc_fd_soft_limit);
        else
            reply.SetString("ProcFDSoftLimit", kNA);

        if (proc_fd_hard_limit >= 0)
            reply.SetInteger("ProcFDHardLimit", proc_fd_hard_limit);
        else
            reply.SetString("ProcFDHardLimit", kNA);

        if (proc_fd_used >= 0)
            reply.SetInteger("ProcFDUsed", proc_fd_used);
        else
            reply.SetString("ProcFDUsed", kNA);

        reply.SetInteger("CPUCount", CSystemInfo::GetCpuCount());

        int         proc_thread_count = CCurrentProcess::GetThreadCount();
        if (proc_thread_count >= 1)
            reply.SetInteger("ProcThreadCount", proc_thread_count);
        else
            reply.SetString("ProcThreadCount", kNA);


        reply.SetString("Version", PUBSEQ_GATEWAY_VERSION);
        reply.SetString("BuildDate", PUBSEQ_GATEWAY_BUILD_DATE);
        reply.SetString("StartedAt", m_StartTime.AsString());

        reply.SetInteger("ExcludeBlobCacheUserCount",
                         app->GetExcludeBlobCache()->Size());

        string      content = reply.Repr(CJsonNode::fStandardJson);

        resp.SetContentType(eJsonMime);
        resp.SetContentLength(content.length());
        resp.SendOk(content.c_str(), content.length(), false);

        x_PrintRequestStop(context, CRequestStatus::e200_Ok);
    } catch (const exception &  exc) {
        string      msg = "Exception when handling an info request: " +
                          string(exc.what());
        resp.SetContentType(ePlainTextMime);
        resp.Send500("Internal Server Error", msg.c_str());
        x_PrintRequestStop(context, CRequestStatus::e500_InternalServerError);
    } catch (...) {
        resp.SetContentType(ePlainTextMime);
        resp.Send500("Internal Server Error",
                     "Unknown exception when handling an info request");
        x_PrintRequestStop(context, CRequestStatus::e500_InternalServerError);
    }
    return 0;
}


int CPubseqGatewayApp::OnStatus(HST::CHttpRequest &  req,
                                HST::CHttpReply<CPendingOperation> &  resp)
{
    // NOTE: expected to work regardless of the shutdown request

    CRequestContextResetter context_resetter;
    CRef<CRequestContext>   context = x_CreateRequestContext(req);

    try {
        m_RequestCounters.IncAdmin();

        CJsonNode                       reply(CJsonNode::NewObjectNode());

        reply.SetInteger("CassandraActiveStatementsCount",
                         m_CassConnection->GetActiveStatements());
        reply.SetInteger("NumberOfConnections",
                         m_TcpDaemon->NumOfConnections());
        reply.SetInteger("ActiveRequestCount",
                         g_ShutdownData.m_ActiveRequestCount);
        reply.SetBoolean("ShutdownRequested",
                         g_ShutdownData.m_ShutdownRequested);
        if (g_ShutdownData.m_ShutdownRequested) {
            auto        now = chrono::steady_clock::now();
            reply.SetInteger("GracefulShutdownExpiredInSec",
                              std::chrono::duration_cast<std::chrono::seconds>
                                (g_ShutdownData.m_Expired - now).count());
        } else {
            reply.SetString("GracefulShutdownExpiredInSec", "n/a");
        }

        m_ErrorCounters.PopulateDictionary(reply);
        m_RequestCounters.PopulateDictionary(reply);
        m_CacheCounters.PopulateDictionary(reply);
        m_DBCounters.PopulateDictionary(reply);

        string      content = reply.Repr(CJsonNode::fStandardJson);

        resp.SetContentType(eJsonMime);
        resp.SetContentLength(content.length());
        resp.SendOk(content.c_str(), content.length(), false);

        x_PrintRequestStop(context, CRequestStatus::e200_Ok);
    } catch (const exception &  exc) {
        string      msg = "Exception when handling a status request: " +
                          string(exc.what());
        resp.SetContentType(ePlainTextMime);
        resp.Send500("Internal Server Error", msg.c_str());
        x_PrintRequestStop(context, CRequestStatus::e500_InternalServerError);
    } catch (...) {
        resp.SetContentType(ePlainTextMime);
        resp.Send500("Internal Server Error",
                     "Unknown exception when handling a status request");
        x_PrintRequestStop(context, CRequestStatus::e500_InternalServerError);
    }
    return 0;
}


int CPubseqGatewayApp::OnShutdown(HST::CHttpRequest &  req,
                                  HST::CHttpReply<CPendingOperation> &  resp)
{
    // NOTE: expected to work regardless of the shutdown request

    static const char *     s_ImmediateShutdown = "Immediate shutdown request accepted";
    static const char *     s_GracefulShutdown = "Graceful shutdown request accepted";
    static size_t           s_ImmediateShutdownSize = strlen(s_ImmediateShutdown);
    static size_t           s_GracefulShutdownSize = strlen(s_GracefulShutdown);

    CRequestContextResetter context_resetter;
    CRef<CRequestContext>   context = x_CreateRequestContext(req);

    try {
        // Reply should use plain http with [potentially] a plain text in the body
        resp.SetContentType(ePlainTextMime);

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

                resp.Send401("Unauthorized", "Invalid authorization token");
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

                resp.Send400("Bad request", "Invalid timeout (must be a positive integer)");
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

                resp.Send400("Bad request", "Invalid timeout (must be a positive integer)");
                x_PrintRequestStop(context, CRequestStatus::e400_BadRequest);
                return 0;
            }
        }

        if (timeout == 0) {
            // Immediate shutdown is requested
            msg = "Immediate shutdown request received from ";
            if (username.empty())
                msg += "an unknown user";
            else
                msg += "user " + username;
            PSG_MESSAGE(msg);

            resp.Send202(s_ImmediateShutdown, s_ImmediateShutdownSize);
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
                resp.Send409("Conflict", msg.c_str());
                x_PrintRequestStop(context, CRequestStatus::e409_Conflict);
                return 0;
            }
        }

        // New shutdown request or a shorter expiration request
        PSG_MESSAGE(msg);

        resp.Send202(s_GracefulShutdown, s_GracefulShutdownSize);
        x_PrintRequestStop(context, CRequestStatus::e202_Accepted);

        g_ShutdownData.m_Expired = expiration;
        g_ShutdownData.m_ShutdownRequested = true;
    } catch (const exception &  exc) {
        string      msg = "Exception when handling a shutdown request: " +
                          string(exc.what());
        resp.Send500("Internal Server Error", msg.c_str());
        x_PrintRequestStop(context, CRequestStatus::e500_InternalServerError);
    } catch (...) {
        resp.Send500("Internal Server Error",
                     "Unknown exception when handling a shutdown request");
        x_PrintRequestStop(context, CRequestStatus::e500_InternalServerError);
    }
    return 0;
}


int CPubseqGatewayApp::OnGetAlerts(HST::CHttpRequest &  req,
                                   HST::CHttpReply<CPendingOperation> &  resp)
{
    // NOTE: expected to work regardless of the shutdown request

    CRequestContextResetter context_resetter;
    CRef<CRequestContext>   context = x_CreateRequestContext(req);

    try {
        m_RequestCounters.IncAdmin();

        string      content = m_Alerts.Serialize().Repr(CJsonNode::fStandardJson);

        resp.SetContentType(eJsonMime);
        resp.SetContentLength(content.length());
        resp.SendOk(content.c_str(), content.length(), false);

        x_PrintRequestStop(context, CRequestStatus::e200_Ok);
    } catch (const exception &  exc) {
        string      msg = "Exception when handling a get alerts request: " +
                          string(exc.what());
        resp.SetContentType(ePlainTextMime);
        resp.Send500("Internal Server Error", msg.c_str());
        x_PrintRequestStop(context, CRequestStatus::e500_InternalServerError);
    } catch (...) {
        resp.SetContentType(ePlainTextMime);
        resp.Send500("Internal Server Error",
                     "Unknown exception when handling a get alerts request");
        x_PrintRequestStop(context, CRequestStatus::e500_InternalServerError);
    }
    return 0;
}


int CPubseqGatewayApp::OnAckAlert(HST::CHttpRequest &  req,
                                  HST::CHttpReply<CPendingOperation> &  resp)
{
    if (x_IsShuttingDown(req, resp))
        return 0;

    CRequestContextResetter context_resetter;
    CRef<CRequestContext>   context = x_CreateRequestContext(req);
    string                  msg;

    resp.SetContentType(ePlainTextMime);
    try {
        SRequestParameter   alert_param = x_GetParam(req, kAlertParam);
        if (!alert_param.m_Found) {
            msg = "Missing " + kAlertParam + " parameter";
            resp.Send400("Bad request", msg.c_str());
            x_PrintRequestStop(context, CRequestStatus::e400_BadRequest);
            return 0;
        }

        SRequestParameter   username_param = x_GetParam(req, kUsernameParam);
        if (!username_param.m_Found) {
            msg = "Missing " + kUsernameParam + " parameter";
            resp.Send400("Bad request", msg.c_str());
            x_PrintRequestStop(context, CRequestStatus::e400_BadRequest);
            return 0;
        }

        string  alert(alert_param.m_Value.data(), alert_param.m_Value.size());
        string  username(username_param.m_Value.data(), username_param.m_Value.size());

        switch (m_Alerts.Acknowledge(alert, username)) {
            case eAlertNotFound:
                msg = "Alert " + alert + " is not found";
                resp.Send404("Not Found", msg.c_str());
                x_PrintRequestStop(context, CRequestStatus::e404_NotFound);
                break;
            case eAlertAlreadyAcknowledged:
                msg = "Alert " + alert + " has already been acknowledged";
                resp.SendOk(msg.c_str(), msg.size(), false);
                x_PrintRequestStop(context, CRequestStatus::e200_Ok);
                break;
            case eAlertAcknowledged:
                resp.SendOk(nullptr, 0, true);
                x_PrintRequestStop(context, CRequestStatus::e200_Ok);
                break;
        }
    } catch (const exception &  exc) {
        string      msg = "Exception when handling an acknowledge alert request: " +
                          string(exc.what());
        resp.SetContentType(ePlainTextMime);
        resp.Send500("Internal Server Error", msg.c_str());
        x_PrintRequestStop(context, CRequestStatus::e500_InternalServerError);
    } catch (...) {
        resp.SetContentType(ePlainTextMime);
        resp.Send500("Internal Server Error",
                     "Unknown exception when handling an acknowledge alert request");
        x_PrintRequestStop(context, CRequestStatus::e500_InternalServerError);
    }
    return 0;
}

int CPubseqGatewayApp::OnStatistics(HST::CHttpRequest &  req,
                                    HST::CHttpReply<CPendingOperation> &  resp)
{
    if (x_IsShuttingDown(req, resp))
        return 0;

    CRequestContextResetter context_resetter;
    CRef<CRequestContext>   context = x_CreateRequestContext(req);

    try {
        m_RequestCounters.IncAdmin();

        bool                    reset = false;
        SRequestParameter       reset_param = x_GetParam(req, kResetParam);
        if (reset_param.m_Found) {
            string      err_msg;
            if (!x_IsBoolParamValid(kResetParam, reset_param.m_Value, err_msg)) {
                resp.SetContentType(ePlainTextMime);
                resp.Send400("Bad Request", err_msg.c_str());
                PSG_WARNING(err_msg);
                x_PrintRequestStop(context, CRequestStatus::e400_BadRequest);
                return 0;
            }
            reset = reset_param.m_Value == "yes";
        }

        if (reset) {
            m_Timing->Reset();
            resp.SetContentType(ePlainTextMime);
            resp.SendOk(nullptr, 0, true);
            x_PrintRequestStop(context, CRequestStatus::e200_Ok);
            return 0;
        }

        CJsonNode   reply(m_Timing->Serialize());
        string      content = reply.Repr(CJsonNode::fStandardJson);

        resp.SetContentType(eJsonMime);
        resp.SetContentLength(content.length());
        resp.SendOk(content.c_str(), content.length(), false);

        x_PrintRequestStop(context, CRequestStatus::e200_Ok);
    } catch (const exception &  exc) {
        string      msg = "Exception when handling a statistics request: " +
                          string(exc.what());
        resp.SetContentType(ePlainTextMime);
        resp.Send500("Internal Server Error", msg.c_str());
        x_PrintRequestStop(context, CRequestStatus::e500_InternalServerError);
    } catch (...) {
        resp.SetContentType(ePlainTextMime);
        resp.Send500("Internal Server Error",
                     "Unknown exception when handling a statistics request");
        x_PrintRequestStop(context, CRequestStatus::e500_InternalServerError);
    }
    return 0;
}


int CPubseqGatewayApp::OnTestIO(HST::CHttpRequest &  req,
                                HST::CHttpReply<CPendingOperation> &  resp)
{
    if (x_IsShuttingDown(req, resp))
        return 0;

    bool                    need_log = false;   // default
    CRequestContextResetter context_resetter;
    CRef<CRequestContext>   context;

    try {
        string                  err_msg;

        SRequestParameter       log_param = x_GetParam(req, kLogParam);
        if (log_param.m_Found) {
            if (!x_IsBoolParamValid(kLogParam, log_param.m_Value, err_msg)) {
                resp.SetContentType(ePlainTextMime);
                resp.Send400("Bad Request", err_msg.c_str());
                PSG_WARNING(err_msg);
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
                err_msg = "Invalid range of the " + kDataSizeParam +
                          " parameter. Accepted values are 0..." +
                          NStr::NumericToString(kMaxTestIOSize);
                resp.SetContentType(ePlainTextMime);
                resp.Send400("Bad Request", err_msg.c_str());
                if (need_log) {
                    PSG_ERROR(err_msg);
                    x_PrintRequestStop(context,
                                       CRequestStatus::e400_BadRequest);
                }
                return 0;
            }
        } else {
            err_msg = "The " + kDataSizeParam + " must be provided";
            resp.SetContentType(ePlainTextMime);
            resp.Send400("Bad Request", err_msg.c_str());
            if (need_log) {
                PSG_ERROR(err_msg);
                x_PrintRequestStop(context, CRequestStatus::e400_BadRequest);
            }
            return 0;
        }

        m_RequestCounters.IncTestIO();

        resp.SetContentType(eBinaryMime);
        resp.SetContentLength(data_size);

        // true: persistent
        resp.SendOk(m_IOTestBuffer.get(), data_size, true);

        if (need_log)
            x_PrintRequestStop(context, CRequestStatus::e200_Ok);
    } catch (const exception &  exc) {
        string      msg = "Exception when handling a test io request: " +
                          string(exc.what());
        resp.SetContentType(ePlainTextMime);
        resp.Send500("Internal Server Error", msg.c_str());
        if (need_log)
            x_PrintRequestStop(context, CRequestStatus::e500_InternalServerError);
    } catch (...) {
        resp.SetContentType(ePlainTextMime);
        resp.Send500("Internal Server Error",
                     "Unknown exception when handling a test io request");
        if (need_log)
            x_PrintRequestStop(context, CRequestStatus::e500_InternalServerError);
    }
    return 0;
}


bool CPubseqGatewayApp::x_ProcessCommonGetAndResolveParams(
        HST::CHttpRequest &  req,
        HST::CHttpReply<CPendingOperation> &  resp,
        CTempString &  seq_id,
        int &  seq_id_type,
        ECacheAndCassandraUse &  use_cache,
        bool  use_psg_protocol)
{
    SRequestParameter   seq_id_type_param;
    string              err_msg;

    // Check the mandatory parameter presence
    SRequestParameter   seq_id_param = x_GetParam(req, kSeqIdParam);
    if (!seq_id_param.m_Found) {
        err_msg = "Missing the 'seq_id' parameter";
        m_ErrorCounters.IncInsufficientArguments();
    }
    else if (seq_id_param.m_Value.empty()) {
        err_msg = "Missing value of the 'seq_id' parameter";
        m_ErrorCounters.IncMalformedArguments();
    }

    if (err_msg.empty()) {
        use_cache = x_GetUseCacheParameter(req, err_msg);
    }

    if (!err_msg.empty()) {
        if (use_psg_protocol) {
            x_SendMessageAndCompletionChunks(resp, err_msg,
                                             CRequestStatus::e400_BadRequest,
                                             eMissingParameter, eDiag_Error);
        } else {
            resp.SetContentType(ePlainTextMime);
            resp.Send400("Bad Request", err_msg.c_str());
        }

        PSG_WARNING(err_msg);
        return false;
    }
    seq_id = seq_id_param.m_Value;

    seq_id_type_param = x_GetParam(req, kSeqIdTypeParam);
    if (seq_id_type_param.m_Found) {
        if (!x_ConvertIntParameter(kSeqIdTypeParam, seq_id_type_param.m_Value,
                                   seq_id_type, err_msg)) {
            m_ErrorCounters.IncMalformedArguments();
            if (use_psg_protocol) {
                x_SendMessageAndCompletionChunks(resp, err_msg,
                                                 CRequestStatus::e400_BadRequest,
                                                 eMalformedParameter, eDiag_Error);
            } else {
                resp.SetContentType(ePlainTextMime);
                resp.Send400("Bad Request", err_msg.c_str());
            }

            PSG_WARNING(err_msg);
            return false;
        }

        if (seq_id_type < 0) {
            err_msg = "seq_id_type value must be >= 0";
            m_ErrorCounters.IncMalformedArguments();
            if (use_psg_protocol) {
                x_SendMessageAndCompletionChunks(resp, err_msg,
                                                CRequestStatus::e400_BadRequest,
                                                eMalformedParameter, eDiag_Error);
            } else {
                resp.SetContentType(ePlainTextMime);
                resp.Send400("Bad Request", err_msg.c_str());
            }

            PSG_WARNING(err_msg);
            return false;
        }
    } else {
        seq_id_type = -1;
    }

    return true;
}


ECacheAndCassandraUse
CPubseqGatewayApp::x_GetUseCacheParameter(HST::CHttpRequest &  req,
                                          string &  err_msg)
{
    SRequestParameter   use_cache_param = x_GetParam(req, kUseCacheParam);

    if (use_cache_param.m_Found) {
        if (!x_IsBoolParamValid(kUseCacheParam, use_cache_param.m_Value,
                                err_msg)) {
            m_ErrorCounters.IncMalformedArguments();
            return eUnknownUseCache;
        }
        if (use_cache_param.m_Value == "yes")
            return eCacheOnly;
        return eCassandraOnly;
    }
    return eCacheAndCassandra;
}


bool CPubseqGatewayApp::x_IsShuttingDown(HST::CHttpRequest &  /* req */,
                                         HST::CHttpReply<CPendingOperation> &  resp)
{
    if (g_ShutdownData.m_ShutdownRequested) {
        resp.Send503("Service Unavailable", nullptr);
        return true;
    }
    return false;
}


bool CPubseqGatewayApp::x_IsDBOK(HST::CHttpRequest &  /* req */,
                                 HST::CHttpReply<CPendingOperation> &  resp)
{
    auto    startup_data_state = GetStartupDataState();
    if (startup_data_state != eStartupDataOK) {
        resp.Send502("Bad Gateway",
                     GetCassStartupDataStateMessage(startup_data_state).c_str());
        return false;
    }
    return true;
}

