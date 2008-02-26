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
 * Author:  Anatoliy Kuznetsov, Maxim Didenko
 *
 * File Description:
 *   Implementation of NetSchedule API.
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbitime.hpp>
#include <corelib/plugin_manager_impl.hpp>
#include <corelib/ncbi_system.hpp>
#include <connect/ncbi_conn_exception.hpp>
#include <connect/services/netschedule_api.hpp>
#include <util/request_control.hpp>
#include <memory>
#include <stdio.h>


BEGIN_NCBI_SCOPE


#define SERVER_PARAMS_ASK_MAX_COUNT 100
/**********************************************************************/

CNetScheduleExceptionMap CNetScheduleAPI::sm_ExceptionMap;

CNetScheduleAPI::CNetScheduleAPI(const string& service_name,
                                 const string& client_name,
                                 const string& queue_name)
    : CNetServiceAPI_Base(service_name,client_name),
      m_Queue(queue_name),
      m_ServerParamsAskCount(SERVER_PARAMS_ASK_MAX_COUNT)
{
}


CNetScheduleAPI::~CNetScheduleAPI()
{
}


string CNetScheduleAPI::StatusToString(EJobStatus status)
{
    switch(status) {
    case eJobNotFound: return "NotFound";
    case ePending:     return "Pending";
    case eRunning:     return "Running";
    case eReturned:    return "Returned";
    case eCanceled:    return "Canceled";
    case eFailed:      return "Failed";
    case eDone:        return "Done";
    default: _ASSERT(0);
    }
    return kEmptyStr;
}

CNetScheduleAPI::EJobStatus
CNetScheduleAPI::StringToStatus(const string& status_str)
{
    if (NStr::CompareNocase(status_str, "Pending") == 0) {
        return ePending;
    }
    if (NStr::CompareNocase(status_str, "Running") == 0) {
        return eRunning;
    }
    if (NStr::CompareNocase(status_str, "Returned") == 0) {
        return eReturned;
    }
    if (NStr::CompareNocase(status_str, "Canceled") == 0) {
        return eCanceled;
    }
    if (NStr::CompareNocase(status_str, "Failed") == 0) {
        return eFailed;
    }
    if (NStr::CompareNocase(status_str, "Done") == 0) {
        return eDone;
    }

    // check acceptable synonyms

    if (NStr::CompareNocase(status_str, "Pend") == 0) {
        return ePending;
    }
    if (NStr::CompareNocase(status_str, "Run") == 0) {
        return eRunning;
    }
    if (NStr::CompareNocase(status_str, "Return") == 0) {
        return eReturned;
    }
    if (NStr::CompareNocase(status_str, "Cancel") == 0) {
        return eCanceled;
    }
    if (NStr::CompareNocase(status_str, "Fail") == 0) {
        return eFailed;
    }

    return eJobNotFound;
}

void CNetScheduleAPI::x_SendAuthetication(CNetServerConnection& conn) const
{
    string auth = GetClientName();
    if (!m_ProgramVersion.empty()) {
        auth += " prog='" + m_ProgramVersion + '\'';
    }
    conn.WriteStr(auth + "\r\n");
    conn.WriteStr(m_Queue + "\r\n");
}


CNetScheduleAPI::EJobStatus
CNetScheduleAPI::GetJobDetails(CNetScheduleJob& job)
{

    /// These attributes are not supported yet
    job.progress_msg.erase();
    job.affinity.erase();
    job.mask = 0;
    job.tags.clear();
    ///

    string resp = x_SendJobCmdWaitResponse("STATUS" , job.job_id);

    const char* str = resp.c_str();

    int st = atoi(str);
    EJobStatus status = (EJobStatus) st;

    if (status == eDone || status == eFailed
        || status == eRunning || status == ePending
        || status == eCanceled || status == eReturned
        || status == eFailed) {
        //cerr << str <<endl;
        for ( ;*str && isdigit((unsigned char)(*str)); ++str) {}

        for ( ; *str && isspace((unsigned char)(*str)); ++str) {}

        job.ret_code = atoi(str);

        for ( ;*str && isdigit((unsigned char)(*str)); ++str) {}

        job.output.erase();

        for ( ; *str && isspace((unsigned char)(*str)); ++str) {}

        if (*str && *str == '"') {
            ++str;
            for( ;*str && *str; ++str) {
                if (*str == '"' && *(str-1) != '\\') break;
                job.output.push_back(*str);
            }
            /*
            for( ;*str && *str != '"'; ++str) {
                output->push_back(*str);
            }
            */
        }
        job.output = NStr::ParseEscapes(job.output);

        job.input.erase();
        job.error_msg.erase();
        if (!*str)
            return status;

        for (++str; *str && isspace((unsigned char)(*str)); ++str) {}

        if (!*str)
            return status;

        if (*str && *str == '"') {
            ++str;
            for( ;*str && *str; ++str) {
                if (*str == '"' && *(str-1) != '\\') break;
                job.error_msg.push_back(*str);
            }
        }
        job.error_msg = NStr::ParseEscapes(job.error_msg);

        if (!*str)
            return status;

        for (++str; *str && isspace((unsigned char)(*str)); ++str) {}

        if (!*str)
            return status;

        if (*str && *str == '"') {
            ++str;
            for( ;*str && *str; ++str) {
                if (*str == '"' && *(str-1) != '\\') break;
                job.input.push_back(*str);
            }
        }
        job.input = NStr::ParseEscapes(job.input);

    } // if status done or failed

    return status;
}

CNetScheduleAPI::EJobStatus
CNetScheduleAPI::x_GetJobStatus(const string& job_key, bool submitter)
{
    string cmd = "STATUS";
    if (GetServerParams().fast_status) {
        if (submitter)  cmd = "SST";
        else cmd = "WST";
    }
    string resp = x_SendJobCmdWaitResponse(cmd, job_key);
    const char* str = resp.c_str();
    int st = atoi(str);
    return (EJobStatus) st;
}

struct SNetScheduleParamsGetter
{
    SNetScheduleParamsGetter(const CNetScheduleAPI& api, CNetScheduleAPI::SServerParams& params)
        : m_API(api), m_Params(params), m_WasCalled(false)
    {
        m_Params.max_input_size = kMax_UInt;
        m_Params.max_output_size = kMax_UInt;
        m_Params.fast_status = true;
    }
    ~SNetScheduleParamsGetter()
    {
        if (m_Params.max_input_size == kMax_UInt)
            m_Params.max_input_size = kNetScheduleMaxDBDataSize / 4;
        if (m_Params.max_output_size == kMax_UInt)
            m_Params.max_output_size = kNetScheduleMaxDBDataSize / 4;
        if (!m_WasCalled)
            m_Params.fast_status = false;
    }
    void operator()(CNetServerConnection conn)
    {
        if (!m_WasCalled) m_WasCalled = true;
        string resp;
        try {
            resp = m_API.SendCmdWaitResponse(conn, "GETP");
        } catch (CNetScheduleException& ex) {
            if (ex.GetErrCode() != CNetScheduleException::eProtocolSyntaxError)
                throw;
        } catch (...) {
        }
        list<string> spars;
        NStr::Split(resp, ";", spars);
        bool fast_status = false;
        ITERATE(list<string>, it, spars) {
            string n,v;
            NStr::SplitInTwo(*it,"=",n,v);
            if (n == "max_input_size") {
                size_t val = NStr::StringToInt(v) / 4;
                if (m_Params.max_input_size > val)
                    m_Params.max_input_size = val ;
            } else if (n == "max_output_size") {
                size_t val = NStr::StringToInt(v) / 4;
                if (m_Params.max_output_size > val)
                    m_Params.max_output_size = val;
            } else if (n == "fast_status" && v == "1") {
                fast_status = true;
            }
        }
        if (m_Params.fast_status) m_Params.fast_status = fast_status;
    }
    const CNetScheduleAPI& m_API;
    CNetScheduleAPI::SServerParams& m_Params;
    bool m_WasCalled;
};

const CNetScheduleAPI::SServerParams& CNetScheduleAPI::GetServerParams()
{
    CFastMutexGuard g(m_ServerParamsMutex);
    if (m_ServerParams.get() && m_ServerParamsAskCount-- > 0) {
        return *m_ServerParams;
    }
    if (!m_ServerParams.get())
        m_ServerParams.reset(new SServerParams);
    m_ServerParamsAskCount = SERVER_PARAMS_ASK_MAX_COUNT;

    ForEach(SNetScheduleParamsGetter(*this, *m_ServerParams));

    return *m_ServerParams;
}


void CNetScheduleAPI::GetProgressMsg(CNetScheduleJob& job)
{
    string resp = x_SendJobCmdWaitResponse("MGET", job.job_id);
    job.progress_msg = NStr::ParseEscapes(resp);
}

void CNetScheduleAPI::ProcessServerError(string& response, ETrimErr trim_err) const
{
    if (trim_err == eTrimErr)
        CNetServiceAPI_Base::TrimErr(response);
    string code;
    string msg;
    if (NStr::SplitInTwo(response, ":", code, msg)) {
        // Map code into numeric value
        CException::TErrCode n_code = sm_ExceptionMap.GetCode(code);
        if (n_code != CException::eInvalid) {
            NCBI_THROW(CNetScheduleException, EErrCode(n_code), msg);
        }
    }
    CNetServiceAPI_Base::ProcessServerError(response, eNoTrimErr);
}


///////////////////////////////////////////////////////////////////////////////

const char* kNetScheduleAPIDriverName = "netschedule_api";

/// @internal
class CNetScheduleAPICF : public IClassFactory<CNetScheduleAPI>
{
public:

    typedef CNetScheduleAPI                 TDriver;
    typedef CNetScheduleAPI                 IFace;
    typedef IFace                           TInterface;
    typedef IClassFactory<CNetScheduleAPI>  TParent;
    typedef TParent::SDriverInfo             TDriverInfo;
    typedef TParent::TDriverList             TDriverList;

    /// Construction
    ///
    /// @param driver_name
    ///   Driver name string
    /// @param patch_level
    ///   Patch level implemented by the driver.
    ///   By default corresponds to interface patch level.
    CNetScheduleAPICF(const string& driver_name = kNetScheduleAPIDriverName,
                      int patch_level = -1)
        : m_DriverVersionInfo
        (ncbi::CInterfaceVersion<IFace>::eMajor,
         ncbi::CInterfaceVersion<IFace>::eMinor,
         patch_level >= 0 ?
            patch_level : ncbi::CInterfaceVersion<IFace>::ePatchLevel),
          m_DriverName(driver_name)
    {
        _ASSERT(!m_DriverName.empty());
    }

    /// Create instance of TDriver
    virtual TInterface*
    CreateInstance(const string& driver  = kEmptyStr,
                   CVersionInfo version = NCBI_INTERFACE_VERSION(IFace),
                   const TPluginManagerParamTree* params = 0) const
    {
        auto_ptr<TDriver> drv;
        if (params && (driver.empty() || driver == m_DriverName)) {
            if (version.Match(NCBI_INTERFACE_VERSION(IFace))
                                != CVersionInfo::eNonCompatible) {

            CConfig conf(params);
            string client_name =
                conf.GetString(m_DriverName,
                               "client_name", CConfig::eErr_Throw, "noname");
            string queue_name =
                conf.GetString(m_DriverName,
                               "queue_name", CConfig::eErr_Throw, "noname");
            string service =
                conf.GetString(m_DriverName,
                               "service", CConfig::eErr_NoThrow, "");
            NStr::TruncateSpacesInPlace(service);

            if (!service.empty()) {
                unsigned int communication_timeout = conf.GetInt(m_DriverName,
                                                              "communication_timeout",
                                                              CConfig::eErr_NoThrow, 12);

                drv.reset(new CNetScheduleAPI(service,
                                              client_name,
                                              queue_name) );
                STimeout tm = { communication_timeout, 0 };
                drv->SetCommunicationTimeout(tm);

                drv->SetRebalanceStrategy(
                    CreateSimpleRebalanceStrategy(conf, m_DriverName),
                               eTakeOwnership);

                //bool permanent_conntction =
                //   conf.GetBool(m_DriverName, "use_permanent_connection",
                //                 CConfig::eErr_NoThrow, true);

                //if( permanent_conntction )
                drv->SetConnMode(CNetServiceAPI_Base::eKeepConnection);


                //                                                rebalance_time,
                //                              rebalance_requests);
                /*
                unsigned max_retry = conf.GetInt(m_DriverName,
                                                 "max_retry",
                                                 CConfig::eErr_NoThrow, 0);
                if (max_retry) {
                    lb_drv->SetMaxRetry(max_retry);
                }

                bool discover_lp_servers =
                    conf.GetBool(m_DriverName, "discover_low_priority_servers",
                                CConfig::eErr_NoThrow, false);

                lb_drv->DiscoverLowPriorityServers(discover_lp_servers);

                string services_list = conf.GetString(m_DriverName,
                                                "sevices_list",
                                                 CConfig::eErr_NoThrow, kEmptyStr);
                vector<string> services;
                NStr::Tokenize(services_list, ",;", services);
                for(vector<string>::const_iterator it = services.begin();
                                                   it != services.end(); ++it) {
                    string host, sport;
                    if (NStr::SplitInTwo(*it,":",host,sport)) {
                        try {
                            unsigned int port = NStr::StringToUInt(sport);
                            lb_drv->AddServiceAddress(host, port);
                        } catch(CStringException&) {}
                    }
                }
                */
            }
            }
        }
        return drv.release();
    }

    void GetDriverVersions(TDriverList& info_list) const
    {
        info_list.push_back(TDriverInfo(m_DriverName, m_DriverVersionInfo));
    }
protected:
    CVersionInfo  m_DriverVersionInfo;
    string        m_DriverName;

};


void NCBI_XCONNECT_EXPORT NCBI_EntryPoint_xnetscheduleapi(
     CPluginManager<CNetScheduleAPI>::TDriverInfoList&   info_list,
     CPluginManager<CNetScheduleAPI>::EEntryPointRequest method)
{
       CHostEntryPointImpl<CNetScheduleAPICF>::
           NCBI_EntryPointImpl(info_list, method);

}


END_NCBI_SCOPE
