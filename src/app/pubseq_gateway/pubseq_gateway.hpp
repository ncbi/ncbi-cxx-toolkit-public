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

#include <objtools/pubseq_gateway/impl/diag/AppLog.hpp>
#include <objtools/pubseq_gateway/impl/diag/AppPerf.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/cass_blob_op.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/cass_factory.hpp>

#include <objtools/pubseq_gateway/impl/rpc/DdRpcDataPacker.hpp>

#include "acc_ver_cache_db.hpp"
#include "pending_operation.hpp"
#include "http_server_transport.hpp"
#include "pubseq_gateway_version.hpp"
#include "pubseq_gateway_stat.hpp"


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

    template<typename P>
    int OnBadURL(HST::CHttpRequest &  req, HST::CHttpReply<P> &  resp);

    template<typename P>
    int OnResolve(HST::CHttpRequest &  req, HST::CHttpReply<P> &  resp);

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
                    const char *  accession, size_t  accession_length,
                    string &  acc_bin_data);

    // Unpacks the accession binary data and picks sat and sat_key.
    // Returns true on success.
    //         false if there was a problem in unpacking the data
    template<typename P>
    bool  x_UnpackAccessionData(HST::CHttpReply<P> &  resp,
                                const string &  resolution_data,
                                int &  sat,
                                int &  sat_key);

private:
    void x_ValidateArgs(void);
    string  x_GetCmdLineArguments(void) const;

private:
    string                              m_DbPath;
    string                              m_LogFile;
    unsigned int                        m_LogLevel;
    unsigned int                        m_LogLevelFile;
    vector<string>                      m_SatNames;

    unsigned short                      m_HttpPort;
    unsigned short                      m_HttpWorkers;
    unsigned int                        m_ListenerBacklog;
    unsigned short                      m_TcpMaxConn;

    unique_ptr<CAccVerCacheDB>          m_Db;
    shared_ptr<CCassConnection>         m_CassConnection;
    shared_ptr<CCassConnectionFactory>  m_CassConnectionFactory;
    unsigned int                        m_TimeoutMs;

    CTime                               m_StartTime;

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
    m_ErrorCounters.IncBadUrlPath();
    resp.Send400("Unknown Request", "The provided URL is not recognized");
    return 0;
}


template<typename P>
int CPubseqGatewayApp::OnResolve(HST::CHttpRequest &  req,
                                 HST::CHttpReply<P> &  resp)
{
    const char *    accession;
    size_t          accession_len;

    if (req.GetParam("accession", sizeof("accession") - 1,
                     true, &accession, &accession_len)) {
        string          resolution_data;
        if (x_Resolve(resp, accession, accession_len, resolution_data)) {
            resp.SendOk(resolution_data.c_str(), resolution_data.length(),
                        false);
            m_RequestCounters.IncResolve();
        }
    } else {
        m_ErrorCounters.IncInsufficientArguments();
        resp.Send400("Missing Request Parameter",
                     "Expected to have the 'accession' parameter");
    }
    return 0;
}


template<typename P>
int CPubseqGatewayApp::OnGetBlob(HST::CHttpRequest &  req,
                                 HST::CHttpReply<P> &  resp)
{
    const char *    ssat;
    const char *    ssat_key;
    const char *    accession;
    size_t          sat_len;
    size_t          sat_key_len;
    size_t          accession_len;

    // Validate parameters first
    bool    sat_found = req.GetParam(
                "sat", sizeof("sat") - 1, true,
                &ssat, &sat_len);
    bool    sat_key_found = req.GetParam(
                "sat_key", sizeof("sat_key") - 1, true,
                &ssat_key, &sat_key_len);
    bool    accession_found = req.GetParam(
                "accession", sizeof("accession") - 1, true,
                &accession, &accession_len);

    if (sat_found && sat_key_found && accession_found) {
        m_ErrorCounters.IncMalformedArguments();
        resp.Send400("Conflicting Request Parameters",
                     "Expected to find 'sat' and 'sat_key' prameters "
                     "or 'accession' parameter. Found all three.");
        return 0;
    }

    if (sat_found && sat_key_found)
    {
        int         sat;
        try {
            NStr::StringToNumeric(ssat, &sat);
        } catch (...) {
            m_ErrorCounters.IncMalformedArguments();
            resp.Send400("Malformed Request Parameters",
                         "Malformed 'sat' parameter. "
                         "An integer is expected.");
            return 0;
        }

        string      sat_name;
        if (SatToSatName(sat, sat_name)) {
            int         sat_key;
            try {
                NStr::StringToNumeric(ssat_key, &sat_key);
            } catch (...) {
                m_ErrorCounters.IncMalformedArguments();
                resp.Send400("Malformed Request Parameters",
                             "Malformed 'sat_key' parameter. "
                             "An integer is expected.");
                return 0;
            }
            resp.Postpone(CPendingOperation(std::move(sat_name), sat_key,
                                            m_CassConnection, m_TimeoutMs));
            return 0;
        }

        m_ErrorCounters.IncGetBlobNotFound();
        string      msg = string("Unknown satellite number ") +
                          NStr::NumericToString(sat);
        resp.Send404("Not Found", msg.c_str());
    } else if (accession_found) {
        string          resolution_data;
        if (!x_Resolve(resp, accession, accession_len, resolution_data))
            return 0;   // resolution failed

        int             sat;
        int             sat_key;
        if (!x_UnpackAccessionData(resp, resolution_data, sat, sat_key))
            return 0;   // unpacking failed

        string      sat_name;
        if (SatToSatName(sat, sat_name)) {
            resp.Postpone(CPendingOperation(resolution_data,
                                            std::move(sat_name), sat_key,
                                            m_CassConnection, m_TimeoutMs));
            return 0;
        }

        m_ErrorCounters.IncGetBlobNotFound();
        string      msg = string("Unknown satellite number ") +
                          NStr::NumericToString(sat) + " after unpacking "
                          "accession data";
        resp.Send404("Not Found", msg.c_str());
    } else {
        m_ErrorCounters.IncInsufficientArguments();
        resp.Send400("Missing Request Parameters", "invalid request");
    }
    return 0;
}


template<typename P>
int CPubseqGatewayApp::OnConfig(HST::CHttpRequest &  req,
                                HST::CHttpReply<P> &  resp)
{
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
    return 0;
}


template<typename P>
int CPubseqGatewayApp::OnInfo(HST::CHttpRequest &  req,
                              HST::CHttpReply<P> &  resp)
{
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
    return 0;
}


template<typename P>
int CPubseqGatewayApp::OnStatus(HST::CHttpRequest &  req,
                                HST::CHttpReply<P> &  resp)
{
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
    return 0;
}


template<typename P>
bool CPubseqGatewayApp::x_Resolve(HST::CHttpReply<P> &  resp,
                                  const char *  accession,
                                  size_t  accession_length,
                                  string &  acc_bin_data)
{
    string      acc_str(accession, accession_length);
    if (m_Db->Storage().Get(acc_str, acc_bin_data))
        return true;

    m_ErrorCounters.IncResolveNotFound();
    string      msg = "Entry (" + acc_str + ") not found";
    resp.Send404("Not Found", msg.c_str());
    return false;
}


template<typename P>
bool CPubseqGatewayApp::x_UnpackAccessionData(HST::CHttpReply<P> &  resp,
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

        resp.Send502("Accession Data Unpacking Failure", msg.c_str());
        ERRLOG1((msg.c_str()));
        return false;
    } catch (const exception &  e) {
        m_ErrorCounters.IncResolveError();
        string  msg = string("Accession data unpacking error: ") +
                      e.what();

        resp.Send502("Accession Data Unpacking Failure", msg.c_str());
        ERRLOG1((msg.c_str()));
        return false;
    } catch (...) {
        m_ErrorCounters.IncResolveError();
        string  msg = "Unknown accession data unpacking error";

        resp.Send502("Accession Data Unpacking Failure", msg.c_str());
        ERRLOG1((msg.c_str()));
        return false;
    }
    return true;
}

#endif
