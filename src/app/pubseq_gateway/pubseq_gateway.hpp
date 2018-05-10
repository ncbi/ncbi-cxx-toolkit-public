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

#include <connect/services/json_over_uttp.hpp>

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
    // Resolves an accession. If failed then sends a message to the client.
    // Returns true if succeeded (acc_bin_data is populated as well)
    //         false if failed (404 response is sent)
    template<typename P>
    bool  x_Resolve(HST::CHttpReply<P> &  resp,
                    const string &  accession,
                    string &  acc_bin_data);

    // Unpacks the accession binary data and picks sat and sat_key.
    // Returns true on success.
    //         false if there was a problem in unpacking the data
    template<typename P>
    bool  x_UnpackAccessionData(HST::CHttpReply<P> &  resp,
                                const string &  accession,
                                const string &  resolution_data,
                                int &  sat,
                                int &  sat_key);
    template<typename P>
    void  x_SendResolution(HST::CHttpReply<P> &  resp,
                           const string &  resolution_data,
                           bool  need_completion);
    template<typename P>
    void x_SendUnknownSatelliteError(HST::CHttpReply<P> &  resp,
                                     const SBlobId &  blob_id,
                                     const string &  message, int  err_code);

    template<typename P>
    void x_SendUnpackingError(HST::CHttpReply<P> &  resp,
                              const string &  accession,
                              const string &  message, int  err_code);

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

    unique_ptr<HST::CHttpDaemon<CPendingOperation>>
                                        m_TcpDaemon;

    // The server counters
    CPubseqGatewayErrorCounters         m_ErrorCounters;
    CPubseqGatewayRequestCounters       m_RequestCounters;

private:
    static CPubseqGatewayApp *          sm_PubseqApp;
};



template<typename P>
int CPubseqGatewayApp::OnBadURL(HST::CHttpRequest &  req,
                                HST::CHttpReply<P> &  resp)
{
    CRef<CRequestContext>   context = x_CreateRequestContext(req);

    m_ErrorCounters.IncBadUrlPath();
    resp.Send400("Unknown Request", "The provided URL is not recognized");

    x_PrintRequestStop(context, CRequestStatus::e400_BadRequest);
    return 0;
}


template<typename P>
int CPubseqGatewayApp::OnGet(HST::CHttpRequest &  req,
                             HST::CHttpReply<P> &  resp)
{
    CRequestContextResetter context_resetter;
    CRef<CRequestContext>   context = x_CreateRequestContext(req);

    SRequestParameter   accession_param = x_GetParam(req, "accession");
    SRequestParameter   resolution_param = x_GetParam(req, "resolution");
    SRequestParameter   main_blob_param = x_GetParam(req, "main_blob");
    SRequestParameter   prefer_non_split_param = x_GetParam(req, "prefer_non_split");
    SRequestParameter   named_annots_param = x_GetParam(req, "named_annots");
    SRequestParameter   external_annots_param = x_GetParam(req, "external_annots");

    // Check the mandatory parameters presence
    if (!accession_param.m_Found) {
        m_ErrorCounters.IncInsufficientArguments();
        resp.Send400("Missing Request Parameter",
                     "Expected to have the 'accession' parameter");
        x_PrintRequestStop(context, CRequestStatus::e400_BadRequest);
        return 0;
    }

    if (!resolution_param.m_Found) {
        m_ErrorCounters.IncInsufficientArguments();
        resp.Send400("Missing Request Parameter",
                     "Expected to have the 'resolution' parameter");
        x_PrintRequestStop(context, CRequestStatus::e400_BadRequest);
        return 0;
    }

    if (!main_blob_param.m_Found) {
        m_ErrorCounters.IncInsufficientArguments();
        resp.Send400("Missing Request Parameter",
                     "Expected to have the 'main_blob' parameter");
        x_PrintRequestStop(context, CRequestStatus::e400_BadRequest);
        return 0;
    }

    // Check the valid parmameter values
    string      err_msg;
    if (!x_IsResolutionParamValid("resolution",
                                  resolution_param.m_Value, err_msg)) {
        m_ErrorCounters.IncMalformedArguments();
        resp.Send400("Malformed Request Parameters", err_msg.c_str());
        x_PrintRequestStop(context, CRequestStatus::e400_BadRequest);
        return 0;
    }

    if (!x_IsBoolParamValid("main_blob", main_blob_param.m_Value, err_msg)) {
        m_ErrorCounters.IncMalformedArguments();
        resp.Send400("Malformed Request Parameters", err_msg.c_str());
        x_PrintRequestStop(context, CRequestStatus::e400_BadRequest);
        return 0;
    }

    if (prefer_non_split_param.m_Found) {
        if (!x_IsBoolParamValid("prefer_non_split",
                                prefer_non_split_param.m_Value, err_msg)) {
            m_ErrorCounters.IncMalformedArguments();
            resp.Send400("Malformed Request Parameters", err_msg.c_str());
            x_PrintRequestStop(context, CRequestStatus::e400_BadRequest);
            return 0;
        }
    }

    if (named_annots_param.m_Found) {
        if (!x_IsBoolParamValid("named_annots",
                                named_annots_param.m_Value, err_msg)) {
            m_ErrorCounters.IncMalformedArguments();
            resp.Send400("Malformed Request Parameters", err_msg.c_str());
            x_PrintRequestStop(context, CRequestStatus::e400_BadRequest);
            return 0;
        }
    }

    // Should depend on resolution but unconditional for now
    bool            need_main_blob = main_blob_param.m_Value == "yes";
    string          resolution_data;

    if (!x_Resolve(resp, accession_param.m_Value, resolution_data)) {
        x_PrintRequestStop(context, CRequestStatus::e404_NotFound);
        return 0;   // Failure; 404 has been sent
    }

    // Send the resolution right away...
    x_SendResolution(resp, resolution_data, !need_main_blob);

    if (main_blob_param.m_Value == "yes") {
        int     sat;
        int     sat_key;
        if (!x_UnpackAccessionData(resp, accession_param.m_Value,
                                   resolution_data, sat, sat_key))
            return 0;   // unpacking failed

        SBlobId     blob_id(sat, sat_key);
        if (SatToSatName(sat, blob_id.m_SatName)) {
            resp.Postpone(CPendingOperation(SBlobRequest(blob_id, eByAccession),
                                            1, m_CassConnection, m_TimeoutMs,
                                            m_MaxRetries,
                                            context));
            return 0;
        }

        m_ErrorCounters.IncGetBlobNotFound();
        string      msg = string("Unknown satellite number ") +
                          NStr::NumericToString(sat) + " after unpacking "
                          "accession data";
        x_SendUnknownSatelliteError(resp, blob_id, msg,
                                    eUnknownResolvedSatellite);
        x_PrintRequestStop(context, CRequestStatus::e404_NotFound);
        return 0;
    }

    x_PrintRequestStop(context, CRequestStatus::e200_Ok);
    return 0;
}


template<typename P>
int CPubseqGatewayApp::OnGetBlob(HST::CHttpRequest &  req,
                                 HST::CHttpReply<P> &  resp)
{
    CRequestContextResetter context_resetter;
    CRef<CRequestContext>   context = x_CreateRequestContext(req);

    SRequestParameter   blob_id_param = x_GetParam(req, "blob_id");

    if (blob_id_param.m_Found)
    {
        SBlobId     blob_id(blob_id_param.m_Value);

        if (!blob_id.IsValid()) {
            m_ErrorCounters.IncMalformedArguments();
            resp.Send400("Malformed Request Parameters",
                         "Malformed 'blob_id' parameter. "
                         "Expected format 'sat.sat_key' where both "
                         "'sat' and 'sat_key' are integers.");
            x_PrintRequestStop(context, CRequestStatus::e400_BadRequest);
            return 0;
        }

        if (SatToSatName(blob_id.m_Sat, blob_id.m_SatName)) {
            resp.Postpone(CPendingOperation(SBlobRequest(blob_id,
                                                         eBySatAndSatKey),
                                            0, m_CassConnection, m_TimeoutMs,
                                            m_MaxRetries, context));

            return 0;
        }

        m_ErrorCounters.IncGetBlobNotFound();
        string      msg = string("Unknown satellite number ") +
                          NStr::NumericToString(blob_id.m_Sat);
        x_SendUnknownSatelliteError(resp, blob_id, msg,
                                    eUnknownResolvedSatellite);
        x_PrintRequestStop(context, CRequestStatus::e404_NotFound);
        return 0;
    }

    m_ErrorCounters.IncInsufficientArguments();
    resp.Send400("Missing Request Parameters",
                 "Mandatory parameter 'blob_id' is not found.");
    x_PrintRequestStop(context, CRequestStatus::e400_BadRequest);
    return 0;
}


template<typename P>
int CPubseqGatewayApp::OnConfig(HST::CHttpRequest &  req,
                                HST::CHttpReply<P> &  resp)
{
    CRequestContextResetter context_resetter;
    CRef<CRequestContext>   context = x_CreateRequestContext(req);

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
    m_RequestCounters.IncAdmin();

    x_PrintRequestStop(context, CRequestStatus::e200_Ok);
    return 0;
}


template<typename P>
int CPubseqGatewayApp::OnInfo(HST::CHttpRequest &  req,
                              HST::CHttpReply<P> &  resp)
{
    CRequestContextResetter context_resetter;
    CRef<CRequestContext>   context = x_CreateRequestContext(req);

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
    m_RequestCounters.IncAdmin();

    x_PrintRequestStop(context, CRequestStatus::e200_Ok);
    return 0;
}


template<typename P>
int CPubseqGatewayApp::OnStatus(HST::CHttpRequest &  req,
                                HST::CHttpReply<P> &  resp)
{
    CRequestContextResetter context_resetter;
    CRef<CRequestContext>   context = x_CreateRequestContext(req);

    CJsonNode                       reply(CJsonNode::NewObjectNode());

    reply.SetInteger("CassandraActiveStatementsCount",
                     m_CassConnection->GetActiveStatements());
    reply.SetInteger("NumberOfConnections",
                     m_TcpDaemon->NumOfConnections());

    uint64_t                        err_count;
    uint64_t                        err_sum(0);

    err_count = m_ErrorCounters.GetBadUrlPath();
    err_sum += err_count;
    reply.SetInteger("BadUrlPathCount", err_count);
    err_count = m_ErrorCounters.GetInsufficientArguments();
    err_sum += err_count;
    reply.SetInteger("InsufficientArgumentsCount", err_count);
    err_count = m_ErrorCounters.GetMalformedArguments();
    err_sum += err_count;
    reply.SetInteger("MalformedArgumentsCount", err_count);
    err_count = m_ErrorCounters.GetResolveNotFound();
    err_sum += err_count;
    reply.SetInteger("ResolveNotFoundCount", err_count);
    err_count = m_ErrorCounters.GetResolveError();
    err_sum += err_count;
    reply.SetInteger("ResolveErrorCount", err_count);
    err_count = m_ErrorCounters.GetGetBlobNotFound();
    err_sum += err_count;
    reply.SetInteger("GetBlobNotFoundCount", err_count);
    err_count = m_ErrorCounters.GetGetBlobError();
    err_sum += err_count;
    reply.SetInteger("GetBlobErrorCount", err_count);
    err_count = m_ErrorCounters.GetUnknownError();
    err_sum += err_count;
    reply.SetInteger("UnknownErrorCount", err_count);
    reply.SetInteger("TotalErrorCount", err_sum);


    uint64_t                        req_count;
    uint64_t                        req_sum(0);

    req_count = m_RequestCounters.GetAdmin();
    req_sum += req_count;
    reply.SetInteger("AdminRequestCount", req_count);
    req_count = m_RequestCounters.GetResolve();
    req_sum += req_count;
    reply.SetInteger("ResolveRequestCount", req_count);
    req_count = m_RequestCounters.GetGetBlobByAccession();
    req_sum += req_count;
    reply.SetInteger("GetBlobByAccessionRequestCount", req_count);
    req_count = m_RequestCounters.GetGetBlobBySatSatKey();
    req_sum += req_count;
    reply.SetInteger("GetBlobBySatSatKeyRequestCount", req_count);
    reply.SetInteger("TotalSucceededRequestCount", req_sum);

    string      content = reply.Repr();

    resp.SetJsonContentType();
    resp.SetContentLength(content.size());
    resp.SendOk(content.c_str(), content.size(), false);

    m_RequestCounters.IncAdmin();

    x_PrintRequestStop(context, CRequestStatus::e200_Ok);
    return 0;
}


template<typename P>
bool CPubseqGatewayApp::x_Resolve(HST::CHttpReply<P> &  resp,
                                  const string &  accession,
                                  string &  acc_bin_data)
{
    if (m_Db->Storage().Get(accession, acc_bin_data))
        return true;

    m_ErrorCounters.IncResolveNotFound();
    string      msg = "Entry (" + accession + ") not found";

    vector<h2o_iovec_t>     chunks;
    string      header = GetResolutionErrorHeader(accession, msg.size(),
                                                  CRequestStatus::e404_NotFound,
                                                  eResolutionNotFound,
                                                  eDiag_Error);
    chunks.push_back(resp.PrepareChunk(
                (const unsigned char *)(header.data()), header.size()));

    chunks.push_back(resp.PrepareChunk(
                (const unsigned char *)(msg.data()), msg.size()));

    string  reply_completion = GetReplyCompletionHeader(2);
    chunks.push_back(resp.PrepareChunk(
                (const unsigned char *)(reply_completion.data()),
                reply_completion.size()));

    resp.Send(chunks, true);
    return false;
}


template<typename P>
void CPubseqGatewayApp::x_SendResolution(HST::CHttpReply<P> &  resp,
                                         const string &  resolution_data,
                                         bool  need_completion)
{
    vector<h2o_iovec_t>     chunks;

    // Add header
    string      header = GetResolutionHeader(resolution_data.size());
    chunks.push_back(resp.PrepareChunk(
                (const unsigned char *)(header.data()),
                header.size()));

    // Add binary data
    chunks.push_back(resp.PrepareChunk(
                (const unsigned char *)(resolution_data.data()),
                resolution_data.size()));

    // Add completion if needed, i.e if it is the only data to send
    if (need_completion) {
        string  footer = GetReplyCompletionHeader(2);
        chunks.push_back(resp.PrepareChunk(
                    (const unsigned char *)(footer.data()),
                    footer.size()));
    }

    resp.Send(chunks, true);
    m_RequestCounters.IncResolve();
}


template<typename P>
void CPubseqGatewayApp::x_SendUnpackingError(HST::CHttpReply<P> &  resp,
                                             const string &  accession,
                                             const string &  msg,
                                             int  err_code)
{
    vector<h2o_iovec_t>     chunks;

    string      header = GetResolutionErrorHeader(accession, msg.size(),
                                                  CRequestStatus::e404_NotFound,
                                                  err_code, eDiag_Error);
    chunks.push_back(resp.PrepareChunk(
                (const unsigned char *)(header.data()), header.size()));

    chunks.push_back(resp.PrepareChunk(
                (const unsigned char *)(msg.data()), msg.size()));

    string  reply_completion = GetReplyCompletionHeader(2);
    chunks.push_back(resp.PrepareChunk(
                (const unsigned char *)(reply_completion.data()),
                reply_completion.size()));

    resp.Send(chunks, true);
}


template<typename P>
void CPubseqGatewayApp::x_SendUnknownSatelliteError(
        HST::CHttpReply<P> &  resp, const SBlobId &  blob_id,
        const string &  message, int  err_code)
{
    vector<h2o_iovec_t>     chunks;

    // Add header
    string      header = GetBlobErrorHeader(blob_id, message.size(),
                                            CRequestStatus::e404_NotFound,
                                            err_code, eDiag_Error);
    chunks.push_back(resp.PrepareChunk(
                (const unsigned char *)(header.data()), header.size()));

    // Add the error message
    chunks.push_back(resp.PrepareChunk(
                (const unsigned char *)(message.data()), message.size()));

    // Add completion
    string  reply_completion = GetReplyCompletionHeader(2);
    chunks.push_back(resp.PrepareChunk(
                (const unsigned char *)(reply_completion.data()),
                reply_completion.size()));

    resp.Send(chunks, true);
}


template<typename P>
bool CPubseqGatewayApp::x_UnpackAccessionData(HST::CHttpReply<P> &  resp,
                                              const string &  accession,
                                              const string &  resolution_data,
                                              int &  sat,
                                              int &  sat_key)
{
    DDRPC::DataRow      rec;
    try {
        DDRPC::AccVerResolverUnpackData(rec, resolution_data);
        sat = rec[2].AsUint1;
        sat_key = rec[3].AsUint4;
    } catch (const DDRPC::EDdRpcException &  e) {
        m_ErrorCounters.IncResolveError();
        string  msg = string("Accession data unpacking error: ") +
                      e.what();
        x_SendUnpackingError(resp, accession, msg, eUnpackingError);
        ERR_POST(msg);
        return false;
    } catch (const exception &  e) {
        m_ErrorCounters.IncResolveError();
        string  msg = string("Accession data unpacking error: ") +
                      e.what();
        x_SendUnpackingError(resp, accession, msg, eUnpackingError);
        ERR_POST(msg);
        return false;
    } catch (...) {
        m_ErrorCounters.IncResolveError();
        string  msg = "Unknown accession data unpacking error";
        x_SendUnpackingError(resp, accession, msg, eUnpackingError);
        ERR_POST(msg);
        return false;
    }
    return true;
}

#endif
