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

#include "pubseq_gateway.hpp"
#include "http_request.hpp"


CPubseqGatewayApp::SRequestParameter
CPubseqGatewayApp::x_GetParam(CHttpRequest &  req, string_view  name) const
{
    SRequestParameter       param;
    const char *            value;
    size_t                  value_size;

    param.m_Found = req.GetParam(name, &value, &value_size);
    if (param.m_Found)
        param.m_Value = string_view(value, value_size);
    return param;
}


constexpr string_view  kYes = "yes";
constexpr string_view  kNo = "no";

bool CPubseqGatewayApp::x_IsBoolParamValid(string_view  param_name,
                                           string_view  param_value,
                                           string &  err_msg) const
{
    if (param_value != kYes && param_value != kNo) {
        err_msg = "Malformed '" + param_name +
                  "' parameter. Acceptable values are 'yes' and 'no'.";
        return false;
    }
    return true;
}


bool CPubseqGatewayApp::x_ConvertIntParameter(string_view  param_name,
                                              string_view  param_value,
                                              int &  converted,
                                              string &  err_msg) const
{
    try {
        converted = NStr::StringToInt(param_value);
    } catch (...) {
        err_msg = "Error converting '" + param_name + "' parameter "
                  "to integer (received value: '" +
                  SanitizeInputValue(param_value) + "')";
        return false;
    }
    return true;
}


bool CPubseqGatewayApp::x_ConvertIntParameter(string_view  param_name,
                                              string_view  param_value,
                                              int64_t &  converted,
                                              string &  err_msg) const
{
    try {
        converted = NStr::StringToLong(param_value);
    } catch (...) {
        err_msg = "Error converting '" + param_name + "' parameter "
                  "to integer (received value: '" +
                  SanitizeInputValue(param_value) + "')";
        return false;
    }
    return true;
}


bool CPubseqGatewayApp::x_ConvertDoubleParameter(string_view  param_name,
                                                 string_view  param_value,
                                                 double &  converted,
                                                 string &  err_msg) const
{
    try {
        converted = NStr::StringToDouble(param_value);
    } catch (...) {
        err_msg = "Error converting '" + param_name + "' parameter "
                  "to double (received value: '" +
                  SanitizeInputValue(param_value) + "')";
        return false;
    }
    return true;
}


void CPubseqGatewayApp::x_MalformedArguments(
                                shared_ptr<CPSGS_Reply>  reply,
                                const psg_time_point_t &  now,
                                string_view  err_msg)
{
    m_Counters->Increment(nullptr, CPSGSCounters::ePSGS_NonProtocolRequests);
    m_Counters->Increment(nullptr, CPSGSCounters::ePSGS_MalformedArgs);
    x_SendMessageAndCompletionChunks(reply, now, err_msg,
                                     CRequestStatus::e400_BadRequest,
                                     ePSGS_MalformedParameter, eDiag_Error);
    PSG_WARNING(err_msg);
}


void CPubseqGatewayApp::x_InsufficientArguments(
                                shared_ptr<CPSGS_Reply>  reply,
                                const psg_time_point_t &  now,
                                string_view  err_msg)
{
    m_Counters->Increment(nullptr, CPSGSCounters::ePSGS_NonProtocolRequests);
    m_Counters->Increment(nullptr, CPSGSCounters::ePSGS_InsufficientArgs);
    x_SendMessageAndCompletionChunks(reply, now, err_msg,
                                     CRequestStatus::e400_BadRequest,
                                     ePSGS_InsufficientArguments, eDiag_Error);
    PSG_WARNING(err_msg);
}


// Used in case of exceptions while handling requests
void CPubseqGatewayApp::x_Finish500(shared_ptr<CPSGS_Reply>  reply,
                                    const psg_time_point_t &  now,
                                    EPSGS_PubseqGatewayErrorCode  code,
                                    string_view  err_msg)
{
    x_SendMessageAndCompletionChunks(reply, now, err_msg,
                                     CRequestStatus::e500_InternalServerError,
                                     code, eDiag_Error);
    PSG_ERROR(err_msg);
}


constexpr string_view  kTraceParam = "trace";

bool
CPubseqGatewayApp::x_GetTraceParameter(CHttpRequest &  req,
                                       shared_ptr<CPSGS_Reply>  reply,
                                       const psg_time_point_t &  now,
                                       SPSGS_RequestBase::EPSGS_Trace &  trace)
{
    trace = SPSGS_RequestBase::ePSGS_NoTracing;     // default
    SRequestParameter   trace_protocol_param = x_GetParam(req, kTraceParam);

    if (trace_protocol_param.m_Found) {
        string      err_msg;

        if (!x_IsBoolParamValid(kTraceParam,
                                trace_protocol_param.m_Value, err_msg)) {
            x_MalformedArguments(reply, now, err_msg);
            return false;
        }
        if (trace_protocol_param.m_Value == kYes)
            trace = SPSGS_RequestBase::ePSGS_WithTracing;
        else
            trace = SPSGS_RequestBase::ePSGS_NoTracing;
    }
    return true;
}


constexpr string_view  kProcessorEventsParam = "processor_events";

bool
CPubseqGatewayApp::x_GetProcessorEventsParameter(CHttpRequest &  req,
                                                 shared_ptr<CPSGS_Reply>  reply,
                                                 const psg_time_point_t &  now,
                                                 bool &  processor_events)
{
    processor_events = false;   // default
    SRequestParameter   processor_events_param = x_GetParam(req, kProcessorEventsParam);

    if (processor_events_param.m_Found) {
        string      err_msg;

        if (!x_IsBoolParamValid(kProcessorEventsParam,
                                processor_events_param.m_Value, err_msg)) {
            x_MalformedArguments(reply, now, err_msg);
            return false;
        }
        processor_events = processor_events_param.m_Value == kYes;
    }
    return true;
}


constexpr string_view  kIncludeHUPParam = "include_hup";

bool
CPubseqGatewayApp::x_GetIncludeHUPParameter(CHttpRequest &  req,
                                            shared_ptr<CPSGS_Reply>  reply,
                                            const psg_time_point_t &  now,
                                            optional<bool> &  include_hup)
{
    SRequestParameter   include_hup_param = x_GetParam(req, kIncludeHUPParam);

    if (include_hup_param.m_Found) {
        string      err_msg;

        if (!x_IsBoolParamValid(kIncludeHUPParam,
                                include_hup_param.m_Value, err_msg)) {
            x_MalformedArguments(reply, now, err_msg);
            return false;
        }
        include_hup = include_hup_param.m_Value == kYes;
    }

    // Here: two cases, the optional is set to true/false or not set at all
    return true;
}


constexpr string_view  kResendTimeoutParam = "resend_timeout";

bool
CPubseqGatewayApp::x_GetResendTimeout(CHttpRequest &  req,
                                      shared_ptr<CPSGS_Reply>  reply,
                                      const psg_time_point_t &  now,
                                      double &  resend_timeout)
{
    resend_timeout = m_Settings.m_ResendTimeoutSec;
    SRequestParameter   resend_timeout_param = x_GetParam(req, kResendTimeoutParam);

    if (resend_timeout_param.m_Found) {
        string      err_msg;
        if (!x_ConvertDoubleParameter(kResendTimeoutParam,
                                      resend_timeout_param.m_Value,
                                      resend_timeout, err_msg)) {
            x_MalformedArguments(reply, now, err_msg);
            return false;
        }

        if (resend_timeout < 0.0) {
            err_msg = "Invalid '" + kResendTimeoutParam + "' parameter value " +
                      to_string(resend_timeout) + ". It must be >= 0.0";
            x_MalformedArguments(reply, now, err_msg);
            return false;
        }
    }
    return true;
}


constexpr string_view kSeqIdResolveParam = "seq_id_resolve";

bool
CPubseqGatewayApp::x_GetSeqIdResolveParameter(CHttpRequest &  req,
                                              shared_ptr<CPSGS_Reply>  reply,
                                              const psg_time_point_t &  now,
                                              bool &  seq_id_resolve)
{
    seq_id_resolve = true;      // default
    SRequestParameter   seq_id_resolve_param = x_GetParam(req, kSeqIdResolveParam);

    if (seq_id_resolve_param.m_Found) {
        string      err_msg;
        if (!x_IsBoolParamValid(kSeqIdResolveParam,
                                seq_id_resolve_param.m_Value, err_msg)) {
            x_MalformedArguments(reply, now, err_msg);
            return false;
        }
        seq_id_resolve = seq_id_resolve_param.m_Value == kYes;
    }
    return true;
}


constexpr string_view  kHopsParam = "hops";

bool
CPubseqGatewayApp::x_GetHops(CHttpRequest &  req,
                             shared_ptr<CPSGS_Reply>  reply,
                             const psg_time_point_t &  now,
                             int &  hops)
{
    hops = 0;   // Default value
    SRequestParameter   hops_param = x_GetParam(req, kHopsParam);

    if (hops_param.m_Found) {
        string      err_msg;
        if (!x_ConvertIntParameter(kHopsParam, hops_param.m_Value,
                                   hops, err_msg)) {
            x_MalformedArguments(reply, now, err_msg);
            return false;
        }

        if (hops < 0) {
            err_msg = "Invalid '" + kHopsParam + "' parameter value " +
                      to_string(hops) + ". It must be > 0.";
            x_MalformedArguments(reply, now, err_msg);
            return false;
        }

        if (hops > m_Settings.m_MaxHops) {
            err_msg = "The '" + kHopsParam + "' parameter value " + to_string(hops) +
                      " exceeds the server configured value " +
                      to_string(m_Settings.m_MaxHops) + ".";
            m_Counters->Increment(nullptr, CPSGSCounters::ePSGS_MaxHopsExceededError);
            x_SendMessageAndCompletionChunks(reply, now, err_msg,
                                             CRequestStatus::e400_BadRequest,
                                             ePSGS_MalformedParameter,
                                             eDiag_Error);
            PSG_WARNING(err_msg);
            return false;
        }
    }
    return true;
}


constexpr string_view  kLastModifiedParam = "last_modified";

bool
CPubseqGatewayApp::x_GetLastModified(CHttpRequest &  req,
                                     shared_ptr<CPSGS_Reply>  reply,
                                     const psg_time_point_t &  now,
                                     int64_t &  last_modified)
{
    last_modified = INT64_MIN;

    SRequestParameter   last_modified_param = x_GetParam(req, kLastModifiedParam);
    if (last_modified_param.m_Found) {
        string      err_msg;
        if (!x_ConvertIntParameter(kLastModifiedParam, last_modified_param.m_Value,
                                   last_modified, err_msg)) {
            x_MalformedArguments(reply, now, err_msg);
            return false;
        }
    }
    return true;
}


constexpr string_view  kBlobIdParam = "blob_id";
constexpr string_view  kBlobIdNotFoundMsg = "Mandatory 'blob_id' parameter is not found.";
constexpr string_view  kBlobIdValNotFoundMsg = "The 'blob_id' parameter value has not been supplied";

bool
CPubseqGatewayApp::x_GetBlobId(CHttpRequest &  req,
                               shared_ptr<CPSGS_Reply>  reply,
                               const psg_time_point_t &  now,
                               SPSGS_BlobId &  blob_id)
{
    SRequestParameter   blob_id_param = x_GetParam(req, kBlobIdParam);
    if (!blob_id_param.m_Found) {
        x_InsufficientArguments(reply, now, kBlobIdNotFoundMsg);
        return false;
    }

    blob_id.SetId(blob_id_param.m_Value);
    if (blob_id.GetId().empty()) {
        x_MalformedArguments(reply, now, kBlobIdValNotFoundMsg);
        return false;
    }
    return true;
}


using LookupPair = pair<string_view, SPSGS_ResolveRequest::EPSGS_BioseqIncludeData>;
constexpr auto kResolveFlagParams = array
{
    LookupPair{"all_info", SPSGS_ResolveRequest::fPSGS_AllBioseqFields},   // must be first

    LookupPair{"canon_id", SPSGS_ResolveRequest::fPSGS_CanonicalId},
    LookupPair{"seq_ids", SPSGS_ResolveRequest::fPSGS_SeqIds},
    LookupPair{"mol_type", SPSGS_ResolveRequest::fPSGS_MoleculeType},
    LookupPair{"length", SPSGS_ResolveRequest::fPSGS_Length},
    LookupPair{"state", SPSGS_ResolveRequest::fPSGS_State},
    LookupPair{"blob_id", SPSGS_ResolveRequest::fPSGS_BlobId},
    LookupPair{"tax_id", SPSGS_ResolveRequest::fPSGS_TaxId},
    LookupPair{"hash", SPSGS_ResolveRequest::fPSGS_Hash},
    LookupPair{"date_changed", SPSGS_ResolveRequest::fPSGS_DateChanged},
    LookupPair{"gi", SPSGS_ResolveRequest::fPSGS_Gi},
    LookupPair{"name", SPSGS_ResolveRequest::fPSGS_Name},
    LookupPair{"seq_state", SPSGS_ResolveRequest::fPSGS_SeqState}
};


bool
CPubseqGatewayApp::x_GetResolveFlags(CHttpRequest &  req,
                                     shared_ptr<CPSGS_Reply>  reply,
                                     const psg_time_point_t &  now,
                                     SPSGS_ResolveRequest::TPSGS_BioseqIncludeData &  include_data_flags)
{
    include_data_flags = 0;     // default

    SRequestParameter       request_param;
    for (const auto &  flag_param: kResolveFlagParams) {
        request_param = x_GetParam(req, flag_param.first);
        if (request_param.m_Found) {
            string  err_msg;
            if (!x_IsBoolParamValid(flag_param.first,
                                    request_param.m_Value, err_msg)) {
                x_MalformedArguments(reply, now, err_msg);
                return false;
            }
            if (request_param.m_Value == "yes") {
                include_data_flags |= flag_param.second;
            } else {
                include_data_flags &= ~flag_param.second;
            }
        }
    }
    return true;
}


constexpr string_view  kId2InfoParam = "id2_info";
constexpr string_view  kId2InfoNotFoundMsg = "Mandatory parameter 'id2_info' is not found.";

bool
CPubseqGatewayApp::x_GetId2Info(CHttpRequest &  req,
                                shared_ptr<CPSGS_Reply>  reply,
                                const psg_time_point_t &  now,
                                string &  id2_info)
{
    SRequestParameter   id2_info_param = x_GetParam(req, kId2InfoParam);
    if (!id2_info_param.m_Found)
    {
        x_InsufficientArguments(reply, now, kId2InfoNotFoundMsg);
        return false;
    }

    id2_info = id2_info_param.m_Value;
    return true;
}


constexpr string_view  kId2ChunkParam = "id2_chunk";
constexpr string_view  kId2ChunkNotFoundMsg = "Mandatory 'id2_chunk' parameter is not found.";
constexpr string_view  kId2ValInvalidMsg = "Invalid 'id2_chunk' parameter value. It must be >= 0";

bool
CPubseqGatewayApp::x_GetId2Chunk(CHttpRequest &  req,
                                 shared_ptr<CPSGS_Reply>  reply,
                                 const psg_time_point_t &  now,
                                 int64_t &  id2_chunk)
{
    id2_chunk = INT64_MIN;
    SRequestParameter   id2_chunk_param = x_GetParam(req, kId2ChunkParam);
    if (!id2_chunk_param.m_Found) {
        x_InsufficientArguments(reply, now, kId2ChunkNotFoundMsg);
        return false;
    }

    string      err_msg;
    if (!x_ConvertIntParameter(kId2ChunkParam,
                               id2_chunk_param.m_Value, id2_chunk, err_msg)) {
        x_MalformedArguments(reply, now, err_msg);
        return false;
    }

    if (id2_chunk < 0) {
        x_MalformedArguments(reply, now, kId2ValInvalidMsg);
        return false;
    }

    return true;
}


constexpr string_view  kExcludeBlobsParam = "exclude_blobs";

vector<string>
CPubseqGatewayApp::x_GetExcludeBlobs(CHttpRequest &  req) const
{
    vector<string>      result;
    SRequestParameter   exclude_blobs_param = x_GetParam(req, kExcludeBlobsParam);
    if (exclude_blobs_param.m_Found) {
        vector<string>      blob_ids;
        NStr::Split(exclude_blobs_param.m_Value, ",", blob_ids);

        size_t      empty_count = 0;
        for (const auto &  item : blob_ids) {
            if (item.empty())
                ++empty_count;
            else
                result.push_back(item);
        }

        if (empty_count > 0)
            PSG_WARNING("Found " << empty_count << " empty blob id(s) in the '" <<
                        kExcludeBlobsParam << "' list (empty blob ids are ignored)");
    }

    return result;
}


constexpr string_view  kEnableProcessor = "enable_processor";
constexpr string_view  kDisableProcessor = "disable_processor";

bool
CPubseqGatewayApp::x_GetEnabledAndDisabledProcessors(
                                        CHttpRequest &  req,
                                        shared_ptr<CPSGS_Reply>  reply,
                                        const psg_time_point_t &  now,
                                        vector<string> &  enabled_processors,
                                        vector<string> &  disabled_processors)
{
    req.GetMultipleValuesParam(kEnableProcessor, enabled_processors);
    req.GetMultipleValuesParam(kDisableProcessor, disabled_processors);

    enabled_processors.erase(
        remove_if(enabled_processors.begin(), enabled_processors.end(),
                  [](string const & s) { return s.empty(); }),
        enabled_processors.end());
    disabled_processors.erase(
        remove_if(disabled_processors.begin(), disabled_processors.end(),
                  [](string const & s) { return s.empty(); }),
        disabled_processors.end());

    for (const auto & en_processor : enabled_processors) {
        for (const auto &  dis_processor : disabled_processors) {
            if (NStr::CompareNocase(en_processor, dis_processor) == 0) {
                x_MalformedArguments(reply, now,
                                     "The same processor name is found "
                                     "in both 'enable_processor' "
                                     "(has it as " + SanitizeInputValue(en_processor) +
                                     ") and 'disable_processor' "
                                     "(has it as " + SanitizeInputValue(dis_processor) +
                                     ") lists");
                return false;
            }
        }
    }

    return true;
}


constexpr string_view  kTSEParam = "tse";
constexpr string_view  kNone = "none";
constexpr string_view  kWhole = "whole";
constexpr string_view  kOrig = "orig";
constexpr string_view  kSmart = "smart";
constexpr string_view  kSlim = "slim";
constexpr string_view  kTSEMalformedMsg = "Malformed 'tse' parameter. "
                                          "Acceptable values are "
                                          "'none', 'whole', 'orig', 'smart' and 'slim'.";

bool CPubseqGatewayApp::x_GetTSEOption(CHttpRequest &  req,
                                       shared_ptr<CPSGS_Reply>  reply,
                                       const psg_time_point_t &  now,
                                       SPSGS_BlobRequestBase::EPSGS_TSEOption &  tse_option)
{
    SRequestParameter       tse_param = x_GetParam(req, kTSEParam);
    if (tse_param.m_Found) {
        if (tse_param.m_Value == kNone) {
            tse_option = SPSGS_BlobRequestBase::ePSGS_NoneTSE;
            return true;
        }
        if (tse_param.m_Value == kWhole) {
            tse_option = SPSGS_BlobRequestBase::ePSGS_WholeTSE;
            return true;
        }
        if (tse_param.m_Value == kOrig) {
            tse_option = SPSGS_BlobRequestBase::ePSGS_OrigTSE;
            return true;
        }
        if (tse_param.m_Value == kSmart) {
            tse_option = SPSGS_BlobRequestBase::ePSGS_SmartTSE;
            return true;
        }
        if (tse_param.m_Value == kSlim) {
            tse_option = SPSGS_BlobRequestBase::ePSGS_SlimTSE;
            return true;
        }

        x_MalformedArguments(reply, now, kTSEMalformedMsg);
        return false;
    }

    return true;
}


constexpr string_view  kAccSubstitutionParam = "acc_substitution";
constexpr string_view  kDefaultOption = "default";
constexpr string_view  kLimitedOption = "limited";
constexpr string_view  kNeverOption = "never";
constexpr string_view  kMalformedAccSubstMsg = "Malformed 'acc_substitution' parameter. "
                                               "Acceptable values are 'default', 'limited', 'never'.";


bool
CPubseqGatewayApp::x_GetAccessionSubstitutionOption(
                                        CHttpRequest &  req,
                                        shared_ptr<CPSGS_Reply>  reply,
                                        const psg_time_point_t &  now,
                                        SPSGS_RequestBase::EPSGS_AccSubstitutioOption & acc_subst_option)
{
    SRequestParameter   subst_param = x_GetParam(req, kAccSubstitutionParam);
    if (subst_param.m_Found) {
        if (subst_param.m_Value == kDefaultOption) {
            acc_subst_option = SPSGS_RequestBase::ePSGS_DefaultAccSubstitution;
            return true;
        }
        if (subst_param.m_Value == kLimitedOption) {
            acc_subst_option = SPSGS_RequestBase::ePSGS_LimitedAccSubstitution;
            return true;
        }
        if (subst_param.m_Value == kNeverOption) {
            acc_subst_option = SPSGS_RequestBase::ePSGS_NeverAccSubstitute;
            return true;
        }

        x_MalformedArguments(reply, now, kMalformedAccSubstMsg);
        return false;
    }
    return true;
}


constexpr string_view  kFmtParam = "fmt";
constexpr string_view  kJson = "json";
constexpr string_view  kHtml = "html";
constexpr string_view  kMalformedIntroFormatMsg = "Malformed 'fmt' parameter. "
                                                  "Acceptable values are 'html' and 'json'";

bool
CPubseqGatewayApp::x_GetIntrospectionFormat(CHttpRequest &  req,
                                            string &  fmt,
                                            string &  err_msg)
{
    SRequestParameter   fmt_param = x_GetParam(req, kFmtParam);
    if (fmt_param.m_Found) {
        if (fmt_param.m_Value == kHtml) {
            fmt = kHtml;
            return true;
        }
        if (fmt_param.m_Value == kJson) {
            fmt = kJson;
            return true;
        }

        err_msg = kMalformedIntroFormatMsg;
        return false;
    }

    fmt = kHtml;     // default
    return true;
}



constexpr string_view  kProtobuf = "protobuf";
constexpr string_view  kNative = "native";
constexpr string_view  kMalformedFmtMsg = "Malformed 'fmt' parameter. "
                                          "Acceptable values are "
                                          "'protobuf' and 'json' and 'native'.";

bool
CPubseqGatewayApp::x_GetOutputFormat(CHttpRequest &  req,
                                     shared_ptr<CPSGS_Reply>  reply,
                                     const psg_time_point_t &  now,
                                     SPSGS_ResolveRequest::EPSGS_OutputFormat &  output_format)
{
    SRequestParameter   fmt_param = x_GetParam(req, kFmtParam);
    if (fmt_param.m_Found) {
        if (fmt_param.m_Value == kProtobuf) {
            output_format = SPSGS_ResolveRequest::ePSGS_ProtobufFormat;
            return true;
        }
        if (fmt_param.m_Value == kJson) {
            output_format = SPSGS_ResolveRequest::ePSGS_JsonFormat;
            return true;
        }
        if (fmt_param.m_Value == kNative) {
            output_format = SPSGS_ResolveRequest::ePSGS_NativeFormat;
            return true;
        }

        x_MalformedArguments(reply, now, kMalformedFmtMsg);
        return false;
    }
    return true;
}


constexpr string_view  kUseCacheParam = "use_cache";

bool
CPubseqGatewayApp::x_GetUseCacheParameter(CHttpRequest &  req,
                                          shared_ptr<CPSGS_Reply>  reply,
                                          const psg_time_point_t &  now,
                                          SPSGS_RequestBase::EPSGS_CacheAndDbUse &  use_cache)
{
    use_cache = SPSGS_RequestBase::ePSGS_CacheAndDb;    // default

    SRequestParameter   use_cache_param = x_GetParam(req, kUseCacheParam);
    if (use_cache_param.m_Found) {
        string      err_msg;
        if (!x_IsBoolParamValid(kUseCacheParam,
                                use_cache_param.m_Value, err_msg)) {
            x_MalformedArguments(reply, now, err_msg);
            return false;
        }

        if (use_cache_param.m_Value == "yes") {
            use_cache = SPSGS_RequestBase::ePSGS_CacheOnly;
        } else {
            use_cache = SPSGS_RequestBase::ePSGS_DbOnly;
        }
    }
    return true;
}


constexpr string_view  kSendBlobIfSmallParam = "send_blob_if_small";
constexpr string_view  kSendBlobIfSmallInvalidMsg = "Invalid 'send_blob_if_small' "
                                                    "parameter value. It must be an integer >= 0";

bool
CPubseqGatewayApp::x_GetSendBlobIfSmallParameter(CHttpRequest &  req,
                                                 shared_ptr<CPSGS_Reply>  reply,
                                                 const psg_time_point_t &  now,
                                                 int &  send_blob_if_small)
{
    send_blob_if_small = 0;   // default

    SRequestParameter   send_blob_if_small_param = x_GetParam(req, kSendBlobIfSmallParam);
    if (send_blob_if_small_param.m_Found) {
        string      err_msg;
        if (!x_ConvertIntParameter(kSendBlobIfSmallParam,
                                   send_blob_if_small_param.m_Value,
                                   send_blob_if_small, err_msg)) {
            x_MalformedArguments(reply, now, err_msg);
            return false;
        }

        if (send_blob_if_small < 0) {
            x_MalformedArguments(reply, now, kSendBlobIfSmallInvalidMsg);
            return false;
        }
    }
    return true;
}


constexpr string_view   kNamesParam = "names";
constexpr string_view   kNamesNotFoundMsg = "The mandatory 'names' "
                                            "parameter is not found";
constexpr string_view   kNoNamesValueMsg = "Named annotation names are not found in the request";

bool
CPubseqGatewayApp::x_GetNames(CHttpRequest &  req,
                              shared_ptr<CPSGS_Reply>  reply,
                              const psg_time_point_t &  now,
                              vector<string> &  names)
{
    SRequestParameter   names_param = x_GetParam(req, kNamesParam);
    if (!names_param.m_Found) {
        x_MalformedArguments(reply, now, kNamesNotFoundMsg);
        return false;
    }

    names.clear();

    vector<string>      raw_names;
    NStr::Split(names_param.m_Value, ", ", raw_names, NStr::fSplit_Tokenize);

    // Filter out those names which repeated more than once
    for (const auto &  raw_item : raw_names) {
        if (!raw_item.empty()) {
            if (find(names.begin(), names.end(), raw_item) != names.end()) {
                PSG_WARNING("The annotation name " + SanitizeInputValue(raw_item) +
                            " is found in the list of names more than once "
                            "(case sensitive). The duplicates are ignored.");
            } else {
                names.push_back(raw_item);
            }
        }
    }

    if (names.empty()) {
        x_MalformedArguments(reply, now, kNoNamesValueMsg);
        return false;
    }
    return true;
}


bool
CPubseqGatewayApp::x_GetCommonIDRequestParams(
                                        CHttpRequest &  req,
                                        shared_ptr<CPSGS_Reply>  reply,
                                        const psg_time_point_t &  now,
                                        SPSGS_RequestBase::EPSGS_Trace &  trace,
                                        int &  hops,
                                        vector<string> &  enabled_processors,
                                        vector<string> &  disabled_processors,
                                        bool &  processor_events)
{
    trace = SPSGS_RequestBase::ePSGS_NoTracing;     // default
    if (!x_GetTraceParameter(req, reply, now, trace)) {
        return false;
    }

    hops = 0;   // default
    if (!x_GetHops(req, reply, now, hops)) {
        return false;
    }

    enabled_processors.clear();     // default
    disabled_processors.clear();    // default
    if (!x_GetEnabledAndDisabledProcessors(req, reply, now, enabled_processors,
                                           disabled_processors)) {
        return false;
    }

    processor_events = false;   // default
    if (!x_GetProcessorEventsParameter(req, reply, now, processor_events)) {
        return false;
    }
    return true;
}


constexpr string_view  kSeqIdParam = "seq_id";
constexpr string_view  kSeqIdTypeParam = "seq_id_type";
constexpr string_view  kSeqIdMissingMsg = "Missing the 'seq_id' parameter";
constexpr string_view  kSeqIdMissingValMsg = "Missing value of the 'seq_id' parameter";
static string          kSeqIdTypeBadValMsg = "The 'seq_id_type' value must be >= 0 and < " +
                                             to_string(CSeq_id::e_MaxChoice);

bool
CPubseqGatewayApp::x_ProcessCommonGetAndResolveParams(CHttpRequest &  req,
                                                      shared_ptr<CPSGS_Reply>  reply,
                                                      const psg_time_point_t &  now,
                                                      CTempString &  seq_id,
                                                      int &  seq_id_type,
                                                      SPSGS_RequestBase::EPSGS_CacheAndDbUse &  use_cache,
                                                      bool  seq_id_is_optional)
{
    SRequestParameter   seq_id_type_param;

    // Check the mandatory parameter presence
    SRequestParameter   seq_id_param = x_GetParam(req, kSeqIdParam);
    if (!seq_id_param.m_Found) {
        if (!seq_id_is_optional) {
            x_InsufficientArguments(reply, now, kSeqIdMissingMsg);
            return false;
        }
    }
    else if (seq_id_param.m_Value.empty()) {
        if (!seq_id_is_optional) {
            x_MalformedArguments(reply, now, kSeqIdMissingValMsg);
            return false;
        }
    }

    if (seq_id_param.m_Found) {
        if (!seq_id_param.m_Value.empty()) {
            seq_id = seq_id_param.m_Value;
        }
    }

    use_cache = SPSGS_RequestBase::ePSGS_CacheAndDb;
    if (!x_GetUseCacheParameter(req, reply, now, use_cache)) {
        return false;
    }

    seq_id_type_param = x_GetParam(req, kSeqIdTypeParam);
    if (seq_id_type_param.m_Found) {
        string              err_msg;
        if (!x_ConvertIntParameter(kSeqIdTypeParam, seq_id_type_param.m_Value,
                                   seq_id_type, err_msg)) {
            x_MalformedArguments(reply, now, err_msg);
            return false;
        }

        if (seq_id_type < 0 || seq_id_type >= CSeq_id::e_MaxChoice) {
            x_MalformedArguments(reply, now, kSeqIdTypeBadValMsg);
            return false;
        }
    } else {
        seq_id_type = -1;
    }

    return true;
}


constexpr string_view  kProteinParam = "protein";

bool
CPubseqGatewayApp::x_GetProtein(CHttpRequest &  req,
                                shared_ptr<CPSGS_Reply>  reply,
                                const psg_time_point_t &  now,
                                optional<string> &  protein)
{
    SRequestParameter   protein_param = x_GetParam(req, kProteinParam);
    if (protein_param.m_Found) {
        // Note: it is necessary to distinguish if the url value is "" or not
        //       provided. So here there is an assignment even if the parameter
        //       has zero length
        protein = protein_param.m_Value;
    }
    return true;
}


constexpr string_view  kIPGParam = "ipg";
constexpr string_view  kIPGBadValMsg = "The 'ipg' parameter value must be > 0";

bool
CPubseqGatewayApp::x_GetIPG(CHttpRequest &  req,
                            shared_ptr<CPSGS_Reply>  reply,
                            const psg_time_point_t &  now,
                            int64_t &  ipg)
{
    SRequestParameter   ipg_param = x_GetParam(req, kIPGParam);
    if (ipg_param.m_Found) {
        string              err_msg;
        if (!x_ConvertIntParameter(kIPGParam, ipg_param.m_Value,
                                   ipg, err_msg)) {
            x_MalformedArguments(reply, now, err_msg);
            return false;
        }

        if (ipg <= 0) {
            x_MalformedArguments(reply, now, kIPGBadValMsg);
            return false;
        }
    } else {
        ipg = -1;
    }

    return true;
}


constexpr string_view  kNucleotideParam = "nucleotide";

bool
CPubseqGatewayApp::x_GetNucleotide(CHttpRequest &  req,
                                   shared_ptr<CPSGS_Reply>  reply,
                                   const psg_time_point_t &  now,
                                   optional<string> &  nucleotide)
{
    SRequestParameter   nucleotide_param = x_GetParam(req, kNucleotideParam);
    if (nucleotide_param.m_Found) {
        // Note: it is necessary to distinguish if the url value is "" or not
        //       provided. So here there is an assignment even if the parameter
        //       has zero length
        nucleotide = nucleotide_param.m_Value;
    }
    return true;
}


constexpr string_view  kSNPScaleLimitParam = "snp_scale_limit";
constexpr string_view  kChromosome = "chromosome";
constexpr string_view  kContig = "contig";
constexpr string_view  kSupercontig = "supercontig";
constexpr string_view  kUnit = "unit";
constexpr string_view  kBadSNPScaleValMsg = "Malformed 'snp_scale_limit' parameter. "
                                            "Acceptable values are 'chromosome' and 'contig' and 'supercontig' and 'unit'.";

bool
CPubseqGatewayApp::x_GetSNPScaleLimit(CHttpRequest &  req,
                                      shared_ptr<CPSGS_Reply>  reply,
                                      const psg_time_point_t &  now,
                                      optional<CSeq_id::ESNPScaleLimit> &  snp_scale_limit)
{
    SRequestParameter   snp_scale_limit_param = x_GetParam(req, kSNPScaleLimitParam);
    if (snp_scale_limit_param.m_Found) {
        if (snp_scale_limit_param.m_Value.empty()) {
            // Empty string is OK. There is no assignment so the processor will
            // see it as 'not set'
            return true;
        }
        if (snp_scale_limit_param.m_Value == kChromosome) {
            snp_scale_limit = CSeq_id::eSNPScaleLimit_Chromosome;
            return true;
        }
        if (snp_scale_limit_param.m_Value == kContig) {
            snp_scale_limit = CSeq_id::eSNPScaleLimit_Contig;
            return true;
        }
        if (snp_scale_limit_param.m_Value == kSupercontig) {
            snp_scale_limit = CSeq_id::eSNPScaleLimit_Supercontig;
            return true;
        }
        if (snp_scale_limit_param.m_Value == kUnit) {
            snp_scale_limit = CSeq_id::eSNPScaleLimit_Unit;
            return true;
        }

        x_MalformedArguments(reply, now, kBadSNPScaleValMsg);
        return false;
    }

    // This is the case when there was no parameter in the incoming URL
    // There is no assignment so the processors will see it as 'not set'
    return true;
}


// It is like "1:59 5:1439 60:"
// Should return {{1, 59}, {5, 1439}, {60, numeric_limits<int>::max()}}
constexpr string_view           kTimeSeriesParam = "time_series";
static vector<pair<int, int>>   kDefaultTimeSeries = {{1, 59}, {5, 1439},
                                                      {60, numeric_limits<int>::max()}};

bool
CPubseqGatewayApp::x_GetTimeSeries(CHttpRequest &  req,
                                   shared_ptr<CPSGS_Reply>  reply,
                                   const psg_time_point_t &  now,
                                   vector<pair<int, int>> &  time_series)
{
    SRequestParameter   time_series_param = x_GetParam(req, kTimeSeriesParam);
    if (time_series_param.m_Found) {
        time_series.clear();

        if (time_series_param.m_Value.empty()) {
            x_MalformedArguments(reply, now, "The 'time_series' "
                                 "parameter is empty. Expected at leas one "
                                 "space separated pair of integers "
                                 "<aggregation mins>:<last minute> or 'no'"sv);
            return false;
        }

        if (time_series_param.m_Value == "no") {
            // Special value: 'no' means there will be no time series
            // The caller knows that it is a special value by an empty
            // time_series container
            return true;
        }

        bool                last = false;
        int                 previous = -1;
        vector<string>      parts;
        NStr::Split(time_series_param.m_Value, " ", parts);
        for (auto &  item : parts) {
            if (last) {
                x_MalformedArguments(reply, now, "The 'time_series' "
                                     "parameter is malformed. Another item is found "
                                     "after the one which describes the rest "
                                     "of the time series."sv);

                return false;
            }

            vector<string>  vals;
            NStr::Split(item, ":", vals);
            if (vals.size() != 2) {
                x_MalformedArguments(reply, now, "The 'time_series' "
                                     "parameter is malformed. One or more items "
                                     "do not have a second value."sv);
                return false;
            }

            int     aggregation;
            try {
                aggregation = NStr::StringToInt(vals[0]);
            } catch (...) {
                x_MalformedArguments(reply, now, "The 'time_series' "
                                     "parameter is malformed. Cannot convert one or more "
                                     "aggregation mins into an integer"sv);
                return false;
            }
            if (aggregation <= 0) {
                x_MalformedArguments(reply, now, "The 'time_series' "
                                     "paremeter is malformed. One or more "
                                     "aggregation mins is <= 0 while it must be > 0."sv);
                return false;
            }

            int     last_minute;
            if (vals[1].empty()) {
                last_minute = numeric_limits<int>::max();
                last = true;
            } else {
                try {
                    last_minute = NStr::StringToInt(vals[1]);
                } catch (...) {
                    x_MalformedArguments(reply, now, "The 'time_series' "
                                         "parameter is malformed. Cannot convert one or more "
                                         "last minute into an integer"sv);
                    return false;
                }

                if (last_minute <= previous) {
                    x_MalformedArguments(reply, now, "The 'time_series' "
                                         "parameter is malformed. One or more last minute "
                                         "<= than the previous one"sv);
                    return false;
                }

                // Check divisibility
                int     start = 0;
                if (previous != -1) {
                    start = previous + 1;
                }
                if ((last_minute - start + 1) % aggregation != 0) {
                   x_MalformedArguments(reply, now, "The 'time_series' "
                                         "parameter is malformed. The range " +
                                         to_string(start) + "-" +
                                         to_string(last_minute) +
                                         " is not divisable by aggregation of " +
                                         to_string(aggregation));
                    return false;
                }

                previous = last_minute;
            }

            time_series.push_back(make_pair(aggregation, last_minute));
        }

        if (!last) {
            x_MalformedArguments(reply, now, "The 'time_series' "
                                 "parameter is malformed. The item which descibes the "
                                 "rest of the series is not found."sv);
            return false;
        }

        return true;
    }

    // The user did not provide the parameter
    time_series = kDefaultTimeSeries;
    return true;
}


constexpr string_view  kVerboseParam = "verbose";

bool
CPubseqGatewayApp::x_GetVerboseParameter(CHttpRequest &  req,
                                         shared_ptr<CPSGS_Reply>  reply,
                                         const psg_time_point_t &  now,
                                         bool &  verbose)
{
    verbose = false;     // default
    SRequestParameter   verbose_protocol_param = x_GetParam(req, kVerboseParam);

    // It is a flag. There is no checking if a value is provided

    verbose = verbose_protocol_param.m_Found;
    return true;
}


constexpr string_view  kExclude = "exclude";

bool CPubseqGatewayApp::x_GetExcludeChecks(CHttpRequest &  req,
                                           shared_ptr<CPSGS_Reply>  reply,
                                           const psg_time_point_t &  now,
                                           vector<string> &  exclude_checks)
{
    req.GetMultipleValuesParam(kExclude, exclude_checks);

    exclude_checks.erase(
        remove_if(exclude_checks.begin(), exclude_checks.end(),
                  [](string const & s) { return s.empty(); }),
        exclude_checks.end());

    return true;
}


static string  kWhitespaceChars = " \t\n\r\f\v";

constexpr string_view  kPeerIdParam = "peer_id";
constexpr string_view  kPeerIdNotFoundMsg = "Mandatory 'peer_id' parameter is not found.";
constexpr string_view  kPeerIdEmptyMsg = "The 'peer_id' parameter cannot be empty.";
constexpr string_view  kPeerIdSpaceCharsMsg = "The 'peer_id' parameter cannot contain whitespace characters.";

bool CPubseqGatewayApp::x_GetPeerIdParameter(CHttpRequest &  req,
                                             shared_ptr<CPSGS_Reply>  reply,
                                             const psg_time_point_t &  now,
                                             string  &  peer_id)
{
    peer_id = "";
    string      err_msg;

    SRequestParameter   peer_id_param = x_GetParam(req, kPeerIdParam);
    if (peer_id_param.m_Found) {
        peer_id = peer_id_param.m_Value;
        if (!peer_id.empty()) {
            size_t      char_pos = peer_id.find_first_of(kWhitespaceChars);
            if (char_pos == string::npos) {
                return true;
            }
            err_msg = kPeerIdSpaceCharsMsg;
        } else {
            err_msg = kPeerIdEmptyMsg;
        }
        m_Counters->Increment(nullptr, CPSGSCounters::ePSGS_MalformedArgs);
    } else {
        err_msg = kPeerIdNotFoundMsg;
        m_Counters->Increment(nullptr, CPSGSCounters::ePSGS_InsufficientArgs);
    }

    m_Counters->Increment(nullptr, CPSGSCounters::ePSGS_NonProtocolRequests);
    reply->SetContentType(ePSGS_PlainTextMime);
    reply->SetContentLength(err_msg.size());
    reply->Send400(err_msg.c_str());
    PSG_WARNING(err_msg);
    return false;
}


constexpr string_view  kPeerUserAgentParam = "peer_user_agent";
constexpr string_view  kPeerUserAgentNotFoundMsg = "Mandatory 'peer_user_agent' parameter is not found.";
constexpr string_view  kPeerUserAgentEmptyMsg = "The 'peer_user_agent' parameter cannot be empty.";
constexpr string_view  kPeerUserAgentSpaceCharsMsg = "The 'peer_user_agent' parameter cannot contain "
                                             "whitespace characters.";
bool CPubseqGatewayApp::x_GetPeerUserAgentParameter(CHttpRequest &  req,
                                                    shared_ptr<CPSGS_Reply>  reply,
                                                    const psg_time_point_t &  now,
                                                    string &  peer_user_agent)
{
    peer_user_agent = "";
    string      err_msg;

    SRequestParameter   peer_user_agent_param = x_GetParam(req, kPeerUserAgentParam);
    if (peer_user_agent_param.m_Found) {
        peer_user_agent = peer_user_agent_param.m_Value;
        if (!peer_user_agent.empty()) {
            size_t      char_pos = peer_user_agent.find_first_of(kWhitespaceChars);
            if (char_pos == string::npos) {
                return true;
            }
            err_msg = kPeerUserAgentSpaceCharsMsg;
        } else {
            err_msg = kPeerUserAgentEmptyMsg;
        }
        m_Counters->Increment(nullptr, CPSGSCounters::ePSGS_MalformedArgs);
    } else {
        err_msg = kPeerUserAgentNotFoundMsg;
        m_Counters->Increment(nullptr, CPSGSCounters::ePSGS_InsufficientArgs);
    }

    m_Counters->Increment(nullptr, CPSGSCounters::ePSGS_NonProtocolRequests);
    reply->SetContentType(ePSGS_PlainTextMime);
    reply->SetContentLength(err_msg.size());
    reply->Send400(err_msg.c_str());
    PSG_WARNING(err_msg);
    return false;
}

