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
    CRequestContextResetter context_resetter;
    CRef<CRequestContext>   context = x_CreateRequestContext(req);

    try {
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
    } catch (...) {
        resp.SetContentType(ePlainTextMime);
        resp.Send500("Internal Server Error",
                     "Unknown exception when handling a bad URL event");
    }
    return 0;
}


int CPubseqGatewayApp::OnGet(HST::CHttpRequest &  req,
                             HST::CHttpReply<CPendingOperation> &  resp)
{
    CRequestContextResetter context_resetter;
    CRef<CRequestContext>   context = x_CreateRequestContext(req);

    try {
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

        m_RequestCounters.IncGetBlobBySeqId();
        resp.Postpone(
                CPendingOperation(
                    SBlobRequest(seq_id, seq_id_type, tse_option, use_cache),
                    0, m_CassConnection, m_TimeoutMs,
                    m_MaxRetries, context));
    } catch (const exception &  exc) {
        string      msg = "Exception when handling a get request: " +
                          string(exc.what());
        x_SendMessageAndCompletionChunks(resp, msg,
                                         CRequestStatus::e500_InternalServerError,
                                         eUnknownError, eDiag_Error);
    } catch (...) {
        string      msg = "Unknown exception when handling a get request";
        x_SendMessageAndCompletionChunks(resp, msg,
                                         CRequestStatus::e500_InternalServerError,
                                         eUnknownError, eDiag_Error);
    }
    return 0;
}


int CPubseqGatewayApp::OnGetBlob(HST::CHttpRequest &  req,
                                 HST::CHttpReply<CPendingOperation> &  resp)
{
    CRequestContextResetter context_resetter;
    CRef<CRequestContext>   context = x_CreateRequestContext(req);

    try {
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
                m_RequestCounters.IncGetBlobBySatSatKey();
                resp.Postpone(
                        CPendingOperation(
                            SBlobRequest(blob_id, last_modified_value,
                                         tse_option, use_cache),
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

        string  message = "Mandatory parameter 'blob_id' is not found.";
        m_ErrorCounters.IncInsufficientArguments();
        x_SendMessageAndCompletionChunks(resp, message,
                                         CRequestStatus::e400_BadRequest,
                                         eMalformedParameter, eDiag_Error);
        PSG_WARNING(message);
        x_PrintRequestStop(context, CRequestStatus::e400_BadRequest);
    } catch (const exception &  exc) {
        string      msg = "Exception when handling a getblob request: " +
                          string(exc.what());
        x_SendMessageAndCompletionChunks(resp, msg,
                                         CRequestStatus::e500_InternalServerError,
                                         eUnknownError, eDiag_Error);
    } catch (...) {
        string      msg = "Unknown exception when handling a getblob request";
        x_SendMessageAndCompletionChunks(resp, msg,
                                         CRequestStatus::e500_InternalServerError,
                                         eUnknownError, eDiag_Error);
    }
    return 0;
}


int CPubseqGatewayApp::OnResolve(HST::CHttpRequest &  req,
                                 HST::CHttpReply<CPendingOperation> &  resp)
{
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
                resp.Send400("Bad Request", err_msg.c_str());
                PSG_WARNING(err_msg);
                return 0;
            }
            use_psg_protocol = psg_protocol_param.m_Value == "yes";
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
                if (use_psg_protocol)
                    x_SendMessageAndCompletionChunks(resp, err_msg,
                                                     CRequestStatus::e400_BadRequest,
                                                     eMalformedParameter, eDiag_Error);
                else
                    resp.Send400("Bad Request", err_msg.c_str());

                PSG_WARNING(err_msg);
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
                    if (use_psg_protocol)
                        x_SendMessageAndCompletionChunks(resp, err_msg,
                                                         CRequestStatus::e400_BadRequest,
                                                         eMalformedParameter, eDiag_Error);
                    else
                        resp.Send400("Bad Request", err_msg.c_str());

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
                                    output_format, use_cache, use_psg_protocol),
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
    }
    return 0;
}


int CPubseqGatewayApp::OnGetNA(HST::CHttpRequest &  req,
                               HST::CHttpReply<CPendingOperation> &  resp)
{
    CRequestContextResetter context_resetter;
    CRef<CRequestContext>   context = x_CreateRequestContext(req);

    // psg_protocol parameter must be present and must be set to true.
    // At least for now, see CXX-10258
    bool                use_psg_protocol = true;

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
                    SAnnotRequest(seq_id, seq_id_type, names, use_cache),
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
    }
    return 0;
}


int CPubseqGatewayApp::OnConfig(HST::CHttpRequest &  req,
                                HST::CHttpReply<CPendingOperation> &  resp)
{
    CRequestContextResetter context_resetter;
    CRef<CRequestContext>   context = x_CreateRequestContext(req);

    try {
        m_RequestCounters.IncAdmin();

        CNcbiOstrstream             conf;
        CNcbiOstrstreamToString     converter(conf);

        CNcbiApplication::Instance()->GetConfig().Write(conf);

        CJsonNode   reply(CJsonNode::NewObjectNode());
        reply.SetString("ConfigurationFilePath",
                        CNcbiApplication::Instance()->GetConfigPath());
        reply.SetString("Configuration", string(converter));
        string      content = reply.Repr();

        resp.SetContentType(eJsonMime);
        resp.SetContentLength(content.length());
        resp.SendOk(content.c_str(), content.length(), false);

        x_PrintRequestStop(context, CRequestStatus::e200_Ok);
    } catch (const exception &  exc) {
        string      msg = "Exception when handling a config request: " +
                          string(exc.what());
        resp.SetContentType(ePlainTextMime);
        resp.Send500("Internal Server Error", msg.c_str());
    } catch (...) {
        resp.SetContentType(ePlainTextMime);
        resp.Send500("Internal Server Error",
                     "Unknown exception when handling a config request");
    }
    return 0;
}


int CPubseqGatewayApp::OnInfo(HST::CHttpRequest &  req,
                              HST::CHttpReply<CPendingOperation> &  resp)
{
    CRequestContextResetter context_resetter;
    CRef<CRequestContext>   context = x_CreateRequestContext(req);

    try {
        m_RequestCounters.IncAdmin();

        CJsonNode   reply(CJsonNode::NewObjectNode());

        reply.SetInteger("PID", CDiagContext::GetPID());
        reply.SetString("ExecutablePath",
                        CNcbiApplication::Instance()->
                                                GetProgramExecutablePath());
        reply.SetString("CommandLineArguments", x_GetCmdLineArguments());


        double      user_time;
        double      system_time;
        bool        process_time_result = GetCurrentProcessTimes(&user_time,
                                                                 &system_time);
        if (process_time_result) {
            reply.SetDouble("UserTime", user_time);
            reply.SetDouble("SystemTime", system_time);
        } else {
            reply.SetString("UserTime", "n/a");
            reply.SetString("SystemTime", "n/a");
        }

        Uint8       physical_memory = GetPhysicalMemorySize();
        if (physical_memory > 0)
            reply.SetInteger("PhysicalMemory", physical_memory);
        else
            reply.SetString("PhysicalMemory", "n/a");

        size_t      mem_used_total;
        size_t      mem_used_resident;
        size_t      mem_used_shared;
        bool        mem_used_result = GetMemoryUsage(&mem_used_total,
                                                     &mem_used_resident,
                                                     &mem_used_shared);
        if (mem_used_result) {
            reply.SetInteger("MemoryUsedTotal", mem_used_total);
            reply.SetInteger("MemoryUsedResident", mem_used_resident);
            reply.SetInteger("MemoryUsedShared", mem_used_shared);
        } else {
            reply.SetString("MemoryUsedTotal", "n/a");
            reply.SetString("MemoryUsedResident", "n/a");
            reply.SetString("MemoryUsedShared", "n/a");
        }

        int         proc_fd_soft_limit;
        int         proc_fd_hard_limit;
        int         proc_fd_used = GetProcessFDCount(&proc_fd_soft_limit,
                                                     &proc_fd_hard_limit);

        if (proc_fd_soft_limit >= 0)
            reply.SetInteger("ProcFDSoftLimit", proc_fd_soft_limit);
        else
            reply.SetString("ProcFDSoftLimit", "n/a");

        if (proc_fd_hard_limit >= 0)
            reply.SetInteger("ProcFDHardLimit", proc_fd_hard_limit);
        else
            reply.SetString("ProcFDHardLimit", "n/a");

        if (proc_fd_used >= 0)
            reply.SetInteger("ProcFDUsed", proc_fd_used);
        else
            reply.SetString("ProcFDUsed", "n/a");

        int         proc_thread_count = GetProcessThreadCount();
        reply.SetInteger("CPUCount", GetCpuCount());
        if (proc_thread_count >= 1)
            reply.SetInteger("ProcThreadCount", proc_thread_count);
        else
            reply.SetString("ProcThreadCount", "n/a");


        reply.SetString("Version", PUBSEQ_GATEWAY_VERSION);
        reply.SetString("BuildDate", PUBSEQ_GATEWAY_BUILD_DATE);
        reply.SetString("StartedAt", m_StartTime.AsString());

        string      content = reply.Repr();

        resp.SetContentType(eJsonMime);
        resp.SetContentLength(content.length());
        resp.SendOk(content.c_str(), content.length(), false);

        x_PrintRequestStop(context, CRequestStatus::e200_Ok);
    } catch (const exception &  exc) {
        string      msg = "Exception when handling an info request: " +
                          string(exc.what());
        resp.SetContentType(ePlainTextMime);
        resp.Send500("Internal Server Error", msg.c_str());
    } catch (...) {
        resp.SetContentType(ePlainTextMime);
        resp.Send500("Internal Server Error",
                     "Unknown exception when handling an info request");
    }
    return 0;
}


int CPubseqGatewayApp::OnStatus(HST::CHttpRequest &  req,
                                HST::CHttpReply<CPendingOperation> &  resp)
{
    CRequestContextResetter context_resetter;
    CRef<CRequestContext>   context = x_CreateRequestContext(req);

    try {
        m_RequestCounters.IncAdmin();

        CJsonNode                       reply(CJsonNode::NewObjectNode());

        reply.SetInteger("CassandraActiveStatementsCount",
                         m_CassConnection->GetActiveStatements());
        reply.SetInteger("NumberOfConnections",
                         m_TcpDaemon->NumOfConnections());

        m_ErrorCounters.PopulateDictionary(reply);
        m_RequestCounters.PopulateDictionary(reply);
        m_CacheCounters.PopulateDictionary(reply);

        string      content = reply.Repr();

        resp.SetContentType(eJsonMime);
        resp.SetContentLength(content.length());
        resp.SendOk(content.c_str(), content.length(), false);

        x_PrintRequestStop(context, CRequestStatus::e200_Ok);
    } catch (const exception &  exc) {
        string      msg = "Exception when handling a status request: " +
                          string(exc.what());
        resp.SetContentType(ePlainTextMime);
        resp.Send500("Internal Server Error", msg.c_str());
    } catch (...) {
        resp.SetContentType(ePlainTextMime);
        resp.Send500("Internal Server Error",
                     "Unknown exception when handling a status request");
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
        if (use_psg_protocol)
            x_SendMessageAndCompletionChunks(resp, err_msg,
                                             CRequestStatus::e400_BadRequest,
                                             eMissingParameter, eDiag_Error);
        else
            resp.Send400("Bad Request", err_msg.c_str());

        PSG_WARNING(err_msg);
        return false;
    }
    seq_id = seq_id_param.m_Value;

    seq_id_type_param = x_GetParam(req, kSeqIdTypeParam);
    if (seq_id_type_param.m_Found) {
        if (!x_ConvertIntParameter(kSeqIdTypeParam, seq_id_type_param.m_Value,
                                   seq_id_type, err_msg)) {
            m_ErrorCounters.IncMalformedArguments();
            if (use_psg_protocol)
                x_SendMessageAndCompletionChunks(resp, err_msg,
                                                 CRequestStatus::e400_BadRequest,
                                                 eMalformedParameter, eDiag_Error);
            else
                resp.Send400("Bad Request", err_msg.c_str());

            PSG_WARNING(err_msg);
            return false;
        }

        if (seq_id_type < 0) {
            err_msg = "seq_id_type value must be >= 0";
            m_ErrorCounters.IncMalformedArguments();
            if (use_psg_protocol)
                x_SendMessageAndCompletionChunks(resp, err_msg,
                                                CRequestStatus::e400_BadRequest,
                                                eMalformedParameter, eDiag_Error);
            else
                resp.Send400("Bad Request", err_msg.c_str());

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

