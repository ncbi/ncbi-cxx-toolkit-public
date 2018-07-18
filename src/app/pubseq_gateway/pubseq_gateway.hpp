#ifndef PUBSEQ_GATEWAY__HPP
#define PUBSEQ_GATEWAY__HPP

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
#include <string>

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbi_system.hpp>

#include <objtools/pubseq_gateway/impl/cassandra/cass_blob_op.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/cass_factory.hpp>

#include <objtools/pubseq_gateway/impl/rpc/DdRpcDataPacker.hpp>

#include "acc_ver_cache_db.hpp"
#include "pending_operation.hpp"
#include "http_server_transport.hpp"
#include "pubseq_gateway_version.hpp"
#include "pubseq_gateway_stat.hpp"
#include "pubseq_gateway_utils.hpp"


USING_NCBI_SCOPE;


class CPubseqGatewayApp: public CNcbiApplication
{
public:
    CPubseqGatewayApp();
    ~CPubseqGatewayApp();

    virtual void Init(void);
    void ParseArgs(void);
    void OpenDb(bool  initialize, bool  readonly);
    void OpenCass(void);
    void CloseCass(void);
    bool SatToSatName(size_t  sat, string &  sat_name);

    bool IsLog(void) const
    {
        return m_Log;
    }

    string GetBioseqKeyspace(void) const
    {
        return m_BioseqKeyspace;
    }

    template<typename P>
    int OnBadURL(HST::CHttpRequest &  req, HST::CHttpReply<P> &  resp);

    template<typename P>
    int OnGet(HST::CHttpRequest &  req, HST::CHttpReply<P> &  resp);

    template<typename P>
    int OnGetBlob(HST::CHttpRequest &  req, HST::CHttpReply<P> &  resp);

    template<typename P>
    int OnConfig(HST::CHttpRequest &  req, HST::CHttpReply<P> &  resp);

    template<typename P>
    int OnInfo(HST::CHttpRequest &  req, HST::CHttpReply<P> &  resp);

    template<typename P>
    int OnStatus(HST::CHttpRequest &  req, HST::CHttpReply<P> &  resp);

    virtual int Run(void);

    static CPubseqGatewayApp *  GetInstance(void);
    CPubseqGatewayErrorCounters &  GetErrorCounters(void);
    CPubseqGatewayRequestCounters &  GetRequestCounters(void);

private:
    template<typename P>
    void x_SendUnknownClientSatelliteError(HST::CHttpReply<P> &  resp,
                                           const SBlobId &  blob_id,
                                           const string &  message);
    template<typename P>
    void x_SendMessageAndCompletionChunks(
        HST::CHttpReply<P> &  resp,  const string &  message,
        CRequestStatus::ECode  status, int  code, EDiagSev  severity);

private:
    void x_ValidateArgs(void);
    string  x_GetCmdLineArguments(void) const;
    CRef<CRequestContext>  x_CreateRequestContext(HST::CHttpRequest &  req) const;
    void x_PrintRequestStop(CRef<CRequestContext>  context, int  status);

    struct SRequestParameter
    {
        bool        m_Found;
        string      m_Value;

        SRequestParameter() : m_Found(false)
        {}
    };
    SRequestParameter  x_GetParam(HST::CHttpRequest &  req,
                                  const string &  name) const;
    bool x_IsBoolParamValid(const string &  param_name,
                            const string &  param_value,
                            string &  err_msg) const;
    bool x_IsResolutionParamValid(const string &  param_name,
                                  const string &  param_value,
                                  string &  err_msg) const;
    int x_PopulateSatToKeyspaceMap(void);

private:
    string                              m_DbPath;
    vector<string>                      m_SatNames;

    unsigned short                      m_HttpPort;
    unsigned short                      m_HttpWorkers;
    unsigned int                        m_ListenerBacklog;
    unsigned short                      m_TcpMaxConn;

    unique_ptr<CAccVerCacheDB>          m_Db;
    shared_ptr<CCassConnection>         m_CassConnection;
    shared_ptr<CCassConnectionFactory>  m_CassConnectionFactory;
    unsigned int                        m_TimeoutMs;
    unsigned int                        m_MaxRetries;

    CTime                               m_StartTime;
    bool                                m_Log;
    string                              m_BioseqKeyspace;

    unique_ptr<HST::CHttpDaemon<CPendingOperation>>
                                        m_TcpDaemon;

    // The server counters
    CPubseqGatewayErrorCounters         m_ErrorCounters;
    CPubseqGatewayRequestCounters       m_RequestCounters;

private:
    static CPubseqGatewayApp *          sm_PubseqApp;
};


// Prepares the chunks for the case when it is a client error so only two
// chunks are required:
// - a message chunk
// - a reply completion chunk
template<typename P>
void  CPubseqGatewayApp::x_SendMessageAndCompletionChunks(
        HST::CHttpReply<P> &  resp,  const string &  message,
        CRequestStatus::ECode  status, int  code, EDiagSev  severity)
{
    vector<h2o_iovec_t>     chunks;
    string                  header = GetReplyMessageHeader(message.size(),
                                                           status, code,
                                                           severity);
    chunks.push_back(resp.PrepareChunk(
                (const unsigned char *)(header.data()), header.size()));

    // Add the error message
    chunks.push_back(resp.PrepareChunk(
                (const unsigned char *)(message.data()), message.size()));

    // Add reply completion
    string  reply_completion = GetReplyCompletionHeader(2);
    chunks.push_back(resp.PrepareChunk(
                (const unsigned char *)(reply_completion.data()),
                reply_completion.size()));

    resp.Send(chunks, true);
}


template<typename P>
int CPubseqGatewayApp::OnBadURL(HST::CHttpRequest &  req,
                                HST::CHttpReply<P> &  resp)
{
    CRequestContextResetter context_resetter;
    CRef<CRequestContext>   context = x_CreateRequestContext(req);
    string                  message = "Unknown request, the provided URL "
                                      "is not recognized";

    m_ErrorCounters.IncBadUrlPath();
    x_SendMessageAndCompletionChunks(resp, message,
                                     CRequestStatus::e400_BadRequest, eBadURL,
                                     eDiag_Error);

    ERR_POST(Warning << message);
    x_PrintRequestStop(context, CRequestStatus::e400_BadRequest);
    return 0;
}


template<typename P>
int CPubseqGatewayApp::OnGet(HST::CHttpRequest &  req,
                             HST::CHttpReply<P> &  resp)
{
    CRequestContextResetter context_resetter;
    CRef<CRequestContext>   context = x_CreateRequestContext(req);

    // Check the mandatory parameters presence
    SRequestParameter   seq_id_param = x_GetParam(req, "seq_id");
    if (!seq_id_param.m_Found) {
        string      message = "Missing the 'seq_id' parameter";

        m_ErrorCounters.IncInsufficientArguments();
        x_SendMessageAndCompletionChunks(resp, message,
                                         CRequestStatus::e400_BadRequest,
                                         eMissingParameter, eDiag_Error);

        ERR_POST(Warning << message);
        x_PrintRequestStop(context, CRequestStatus::e400_BadRequest);
        return 0;
    }

    int                 seq_id_type = 1;    // default
    SRequestParameter   seq_id_type_param = x_GetParam(req, "seq_id_type");
    if (seq_id_type_param.m_Found) {
        try {
            seq_id_type = NStr::StringToInt(seq_id_type_param.m_Value);
        } catch (const exception &  exc) {
            string      message = "Error converting seq_id_type parameter "
                                  "to integer: " + string(exc.what());

            m_ErrorCounters.IncMalformedArguments();
            x_SendMessageAndCompletionChunks(resp, message,
                                             CRequestStatus::e400_BadRequest,
                                             eMalformedParameter, eDiag_Error);

            ERR_POST(Warning << message);
            x_PrintRequestStop(context, CRequestStatus::e400_BadRequest);
            return 0;
        } catch (...) {
            string      message = "Unknown error converting "
                                  "seq_id_type parameter to integer";

            m_ErrorCounters.IncMalformedArguments();
            x_SendMessageAndCompletionChunks(resp, message,
                                             CRequestStatus::e400_BadRequest,
                                             eMalformedParameter, eDiag_Error);

            ERR_POST(Warning << message);
            x_PrintRequestStop(context, CRequestStatus::e400_BadRequest);
            return 0;
        }
    }

    map<string,
        pair<SRequestParameter,
             CPendingOperation::EServIncludeData>>    flag_params = {
        {"no_tse", make_pair(SRequestParameter(),
                             CPendingOperation::fServNoTSE)},
        {"fast_info", make_pair(SRequestParameter(),
                                CPendingOperation::fServFastInfo)},
        {"whole_tse", make_pair(SRequestParameter(),
                                CPendingOperation::fServWholeTSE)},
        {"orig_tse", make_pair(SRequestParameter(),
                               CPendingOperation::fServOrigTSE)},
        {"canon_id", make_pair(SRequestParameter(),
                               CPendingOperation::fServCanonicalId)},
        {"other_ids", make_pair(SRequestParameter(),
                                CPendingOperation::fServOtherIds)},
        {"mol_type", make_pair(SRequestParameter(),
                               CPendingOperation::fServMoleculeType)},
        {"length", make_pair(SRequestParameter(),
                             CPendingOperation::fServLength)},
        {"state", make_pair(SRequestParameter(),
                            CPendingOperation::fServState)},
        {"blob_id", make_pair(SRequestParameter(),
                              CPendingOperation::fServBlobId)},
        {"tax_id", make_pair(SRequestParameter(),
                             CPendingOperation::fServTaxId)},
        {"hash", make_pair(SRequestParameter(),
                           CPendingOperation::fServHash)}
    };


    TServIncludeData        include_data_flags = 0;
    for (auto &  flag_param: flag_params) {
        flag_param.second.first = x_GetParam(req, flag_param.first);
        if (flag_param.second.first.m_Found) {
            string      err_msg;

            if (!x_IsBoolParamValid(flag_param.first,
                                    flag_param.second.first.m_Value, err_msg)) {
                m_ErrorCounters.IncMalformedArguments();
                x_SendMessageAndCompletionChunks(resp, err_msg,
                                                 CRequestStatus::e400_BadRequest,
                                                 eMalformedParameter, eDiag_Error);

                ERR_POST(Warning << err_msg);
                x_PrintRequestStop(context, CRequestStatus::e400_BadRequest);
                return 0;
            }
            if (flag_param.second.first.m_Value == "yes") {
                include_data_flags |= flag_param.second.second;
            }
        }
    }

    resp.Postpone(
            CPendingOperation(
                SBlobRequest(seq_id_param.m_Value,
                             seq_id_type, include_data_flags),
                0, m_CassConnection, m_TimeoutMs,
                m_MaxRetries, context));
    return 0;
}


template<typename P>
int CPubseqGatewayApp::OnGetBlob(HST::CHttpRequest &  req,
                                 HST::CHttpReply<P> &  resp)
{
    CRequestContextResetter context_resetter;
    CRef<CRequestContext>   context = x_CreateRequestContext(req);

    SRequestParameter   blob_id_param = x_GetParam(req, "blob_id");
    SRequestParameter   last_modified_param = x_GetParam(req, "last_modified");

    if (blob_id_param.m_Found)
    {
        SBlobId     blob_id(blob_id_param.m_Value);

        if (!blob_id.IsValid()) {
            string  message = "Malformed 'blob_id' parameter. "
                              "Expected format 'sat.sat_key' where both "
                              "'sat' and 'sat_key' are integers.";

            m_ErrorCounters.IncMalformedArguments();
            x_SendMessageAndCompletionChunks(resp, message,
                                             CRequestStatus::e400_BadRequest,
                                             eMalformedParameter, eDiag_Error);
            ERR_POST(Warning << message);
            x_PrintRequestStop(context, CRequestStatus::e400_BadRequest);
            return 0;
        }

        if (SatToSatName(blob_id.m_Sat, blob_id.m_SatName)) {
            resp.Postpone(
                    CPendingOperation(
                        SBlobRequest(blob_id, last_modified_param.m_Value),
                        0, m_CassConnection, m_TimeoutMs,
                        m_MaxRetries, context));

            return 0;
        }

        m_ErrorCounters.IncClientSatToSatName();
        string      message = string("Unknown satellite number ") +
                              NStr::NumericToString(blob_id.m_Sat);
        x_SendUnknownClientSatelliteError(resp, blob_id, message);
        ERR_POST(Warning << message);
        x_PrintRequestStop(context, CRequestStatus::e404_NotFound);
        return 0;
    }

    string  message = "Mandatory parameter 'blob_id' is not found.";
    m_ErrorCounters.IncInsufficientArguments();
    x_SendMessageAndCompletionChunks(resp, message,
                                     CRequestStatus::e400_BadRequest,
                                     eMalformedParameter, eDiag_Error);
    ERR_POST(Warning << message);
    x_PrintRequestStop(context, CRequestStatus::e400_BadRequest);
    return 0;
}


template<typename P>
int CPubseqGatewayApp::OnConfig(HST::CHttpRequest &  req,
                                HST::CHttpReply<P> &  resp)
{
    CRequestContextResetter context_resetter;
    CRef<CRequestContext>   context = x_CreateRequestContext(req);

    m_RequestCounters.IncAdmin();

    CNcbiOstrstream             conf;
    CNcbiOstrstreamToString     converter(conf);

    CNcbiApplication::Instance()->GetConfig().Write(conf);

    CJsonNode   reply(CJsonNode::NewObjectNode());
    reply.SetString("ConfigurationFilePath",
                    CNcbiApplication::Instance()->GetConfigPath());
    reply.SetString("Configuration", string(converter));
    string      content = reply.Repr();

    resp.SetJsonContentType();
    resp.SetContentLength(content.length());
    resp.SendOk(content.c_str(), content.length(), false);

    x_PrintRequestStop(context, CRequestStatus::e200_Ok);
    return 0;
}


template<typename P>
int CPubseqGatewayApp::OnInfo(HST::CHttpRequest &  req,
                              HST::CHttpReply<P> &  resp)
{
    CRequestContextResetter context_resetter;
    CRef<CRequestContext>   context = x_CreateRequestContext(req);

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

    resp.SetJsonContentType();
    resp.SetContentLength(content.length());
    resp.SendOk(content.c_str(), content.length(), false);

    x_PrintRequestStop(context, CRequestStatus::e200_Ok);
    return 0;
}


template<typename P>
int CPubseqGatewayApp::OnStatus(HST::CHttpRequest &  req,
                                HST::CHttpReply<P> &  resp)
{
    CRequestContextResetter context_resetter;
    CRef<CRequestContext>   context = x_CreateRequestContext(req);

    m_RequestCounters.IncAdmin();

    CJsonNode                       reply(CJsonNode::NewObjectNode());

    reply.SetInteger("CassandraActiveStatementsCount",
                     m_CassConnection->GetActiveStatements());
    reply.SetInteger("NumberOfConnections",
                     m_TcpDaemon->NumOfConnections());

    m_ErrorCounters.PopulateDictionary(reply);
    m_RequestCounters.PopulateDictionary(reply);

    string      content = reply.Repr();

    resp.SetJsonContentType();
    resp.SetContentLength(content.size());
    resp.SendOk(content.c_str(), content.size(), false);

    x_PrintRequestStop(context, CRequestStatus::e200_Ok);
    return 0;
}


// Sends an unknown satellite error for the case when the satellite is provided
// by the user in the incoming URL. I.e. this error is treated as a client
// error (opposite to a server data inconsistency)
template<typename P>
void CPubseqGatewayApp::x_SendUnknownClientSatelliteError(
        HST::CHttpReply<P> &  resp,
        const SBlobId &  blob_id,
        const string &  message)
{
    vector<h2o_iovec_t>     chunks;

    // Add header
    string      header = GetBlobMessageHeader(1, blob_id, message.size(),
                                              CRequestStatus::e404_NotFound,
                                              eUnknownResolvedSatellite,
                                              eDiag_Error);
    chunks.push_back(resp.PrepareChunk(
                (const unsigned char *)(header.data()), header.size()));

    // Add the error message
    chunks.push_back(resp.PrepareChunk(
                (const unsigned char *)(message.data()), message.size()));

    // Add meta with n_chunks
    string      meta = GetBlobCompletionHeader(1, blob_id, 2);
    chunks.push_back(resp.PrepareChunk(
                (const unsigned char *)(meta.data()), meta.size()));

    // Add reply completion
    string  reply_completion = GetReplyCompletionHeader(3);
    chunks.push_back(resp.PrepareChunk(
                (const unsigned char *)(reply_completion.data()),
                reply_completion.size()));

    resp.Send(chunks, true);
}

#endif
