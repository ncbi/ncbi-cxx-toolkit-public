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
#include <connect/ncbi_conn_exception.hpp>
#include <connect/services/netschedule_api.hpp>
#include <util/request_control.hpp>
#include <memory>
#include <stdio.h>


BEGIN_NCBI_SCOPE


CNetScheduleKey::CNetScheduleKey(const string& key_str)
{

    // JSID_01_1_MYHOST_9000

    const char* ch = key_str.c_str();
    string prefix;

    // prefix

    for (;*ch && *ch != '_'; ++ch) {
        prefix += *ch;
    }
    if (*ch == 0) {
        NCBI_THROW(CNetScheduleException, eKeyFormatError, "Key syntax error.");
    }
    ++ch;

    if (prefix != kNetScheduleKeyPrefix) {
        NCBI_THROW(CNetScheduleException, eKeyFormatError, 
                                       "Key syntax error. Invalid prefix.");
    }

    // version
    version = atoi(ch);
    while (*ch && *ch != '_') {
        ++ch;
    }
    if (*ch == 0) {
        NCBI_THROW(CNetScheduleException, eKeyFormatError, "Key syntax error.");
    }
    ++ch;

    // id
    id = (unsigned) atoi(ch);
    while (*ch && *ch != '_') {
        ++ch;
    }
    if (*ch == 0 || id == 0) {
        NCBI_THROW(CNetScheduleException, eKeyFormatError, "Key syntax error.");
    }
    ++ch;


    // hostname
    for (;*ch && *ch != '_'; ++ch) {
        host += *ch;
    }
    if (*ch == 0) {
        NCBI_THROW(CNetScheduleException, eKeyFormatError, "Key syntax error.");
    }
    ++ch;

    // port
    port = atoi(ch);
}

CNetScheduleKey::operator string() const 
{
    string tmp;
    string key = "JSID_01";  

    NStr::IntToString(tmp, id);
    key += "_";
    key += tmp;

    key += "_";
    key += host;    

    NStr::IntToString(tmp, port);
    key += "_";
    key += tmp;
    return key;
}

CNetScheduleExceptionMap::CNetScheduleExceptionMap()
{
    m_Map["eInternalError"] = CNetScheduleException::eInternalError;
    m_Map["eProtocolSyntaxError"] = CNetScheduleException::eProtocolSyntaxError;
    m_Map["eAuthenticationError"] = CNetScheduleException::eAuthenticationError;
    m_Map["eKeyFormatError"] = CNetScheduleException::eKeyFormatError;
    m_Map["eInvalidJobStatus"] = CNetScheduleException::eInvalidJobStatus;
    m_Map["eUnknownQueue"] = CNetScheduleException::eUnknownQueue;
    m_Map["eUnknownQueueClass"] = CNetScheduleException::eUnknownQueueClass;
    m_Map["eTooManyPendingJobs"] = CNetScheduleException::eTooManyPendingJobs;
    m_Map["eDataTooLong"] = CNetScheduleException::eDataTooLong;
    m_Map["eInvalidClientOrVersion"] = CNetScheduleException::eInvalidClientOrVersion;
    m_Map["eOperationAccessDenied"] = CNetScheduleException::eOperationAccessDenied;
    m_Map["eDuplicateName"] = CNetScheduleException::eDuplicateName;
}

CException::TErrCode CNetScheduleExceptionMap::GetCode(const string& name)
{
    TMap::iterator it = m_Map.find(name);
    if (it == m_Map.end())
        return CException::eInvalid;
    return it->second;
}



/**********************************************************************/

CNetScheduleExceptionMap CNetScheduleAPI::sm_ExceptionMap;

CNetScheduleAPI::CNetScheduleAPI(const string& service_name,
                                 const string& client_name,
                                 const string& queue_name)
    : INetServiceAPI(service_name,client_name),
      m_Queue(queue_name)
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



CNetScheduleAPI::EJobStatus 
CNetScheduleAPI::GetJobDetails(const string& job_key,
                               string*    input,
                               string*    progress_msg,
                               string*    affinity,
                               TJobMask*  job_mask,
                               TJobTags*  tags,
                               int*       ret_code,
                               string*    output,
                               string*    err_msg) const
{
    _ASSERT(ret_code);
    _ASSERT(output);
    
    /// These attributes are not supported yet
    if (progress_msg) *progress_msg = "";
    if (affinity) *affinity = "";
    if (job_mask) *job_mask = 0;
    if (tags) tags->clear();
    ///

    string resp = x_SendJobCmdWaitResponse("STATUS" , job_key); 

    const char* str = resp.c_str();

    int st = atoi(str);
    EJobStatus status = (EJobStatus) st;

    if (status == eDone || status == eFailed 
        || status == eRunning || status == ePending 
        || status == eCanceled || status == eReturned
        || status == eFailed) {
        //cerr << str <<endl;
        for ( ;*str && isdigit((unsigned char)(*str)); ++str) {
        }

        for ( ; *str && isspace((unsigned char)(*str)); ++str) {
        }

        *ret_code = atoi(str);

        for ( ;*str && isdigit((unsigned char)(*str)); ++str) {
        }

        output->erase();

        for ( ; *str && isspace((unsigned char)(*str)); ++str) {
        }

        if (*str && *str == '"') {
            ++str;
            for( ;*str && *str; ++str) {
                if (*str == '"' && *(str-1) != '\\') break;
                output->push_back(*str);
            }
            /*
            for( ;*str && *str != '"'; ++str) {
                output->push_back(*str);
            }
            */
        }
        *output = NStr::ParseEscapes(*output);
        if (err_msg || input) {
            if (err_msg) err_msg->erase();
            if (!*str)
                return status;

            for (++str; *str && isspace((unsigned char)(*str)); ++str) {
            }
            
            if (!*str)
                return status;

            if (*str && *str == '"') {
                ++str;
                for( ;*str && *str; ++str) {
                    if (*str == '"' && *(str-1) != '\\') break;
                    if (err_msg)
                        err_msg->push_back(*str);
                }
            }

            if (err_msg) *err_msg = NStr::ParseEscapes(*err_msg);
        }

        if (input) {
            input->erase();
            if (!*str)
                return status;

            for (++str; *str && isspace((unsigned char)(*str)); ++str) {
            }
            
            if (!*str)
                return status;

            if (*str && *str == '"') {
                ++str;
                for( ;*str && *str; ++str) {
                    if (*str == '"' && *(str-1) != '\\') break;
                    input->push_back(*str);
                }
            }
            *input = NStr::ParseEscapes(*input);
        }

    } // if status done or failed

    return status;
}

CNetScheduleAPI::EJobStatus 
CNetScheduleAPI::GetJobStatus(const string& job_key) const
{
    string resp = x_SendJobCmdWaitResponse("STATUS" , job_key); 
    const char* str = resp.c_str();
    int st = atoi(str);
    return (EJobStatus) st;
}

void CNetScheduleAPI::ProcessServerError(string& response, ETrimErr trim_err) const
{
    if (trim_err == eTrimErr)
        INetServiceAPI::TrimErr(response);
    string code;
    string msg;
    if (NStr::SplitInTwo(response, ":", code, msg)) {
        // Map code into numeric value
        CException::TErrCode n_code = sm_ExceptionMap.GetCode(code);
        if (n_code != CException::eInvalid) {
            NCBI_THROW(CNetScheduleException, EErrCode(n_code), msg);
        }
    }
    INetServiceAPI::ProcessServerError(response, eNoTrimErr);
}

///////////////////????????????????????????/////////////////////////
void CNetScheduleAPI::GetServerVersion(CNetScheduleAPI::ISink& sink) const
{ 
    string cmd = "VERSION";
    for (CNetSrvConnectorPoll::iterator it = GetPoll().begin(); it != GetPoll().end(); ++it) {
        CNcbiOstream& os = sink.GetOstream(GetConnectionInfo(*it));
        os << SendCmdWaitResponse(*it, cmd);
    }        
}



void CNetScheduleAPI::DumpQueue(CNetScheduleAPI::ISink& sink) const
{
    string cmd = "DUMP";        
    for (CNetSrvConnectorPoll::iterator it = GetPoll().begin(); it != GetPoll().end(); ++it) {
        CNcbiOstream& os = sink.GetOstream(GetConnectionInfo(*it));
        it->WriteStr(cmd);
        PrintServerOut(*it, os);
    }
}


void CNetScheduleAPI::PrintQueue(CNetScheduleAPI::ISink& sink, EJobStatus status) const
{
    string cmd = "QPRT " + CNetScheduleAPI::StatusToString(status);;        
    for (CNetSrvConnectorPoll::iterator it = GetPoll().begin(); it != GetPoll().end(); ++it) {
        CNcbiOstream& os = sink.GetOstream(GetConnectionInfo(*it));
        it->WriteStr(cmd);
        PrintServerOut(*it, os);
    }
}



void CNetScheduleAPI::GetServerStatistics(CNetScheduleAPI::ISink& sink, 
                                          EStatisticsOptions opt) const
{
    string cmd = "STAT";
    if (opt == eStatisticsAll) {
        cmd += " ALL";
    }
    for(CNetSrvConnectorPoll::iterator it = GetPoll().begin(); it != GetPoll().end(); ++it) {
        CNcbiOstream& os = sink.GetOstream(GetConnectionInfo(*it));
        it->WriteStr(cmd);
        PrintServerOut(*it, os);
    }
}


void CNetScheduleAPI::Monitor(CNcbiOstream & out) const
{
    /*
    CheckConnect(kEmptyStr);
    CSockGuard sg(*m_Sock);

    MakeCommandPacket(&m_Tmp, "MONI ");
    WriteStr(m_Tmp.c_str(), m_Tmp.length() + 1);
    m_Tmp = "QUIT";
    WriteStr(m_Tmp.c_str(), m_Tmp.length() + 1);

    STimeout rto;
    rto.sec = 1;
    rto.usec = 0;
    m_Sock->SetTimeout(eIO_Read, &rto);

    string line;
    while (1) {

        EIO_Status st = m_Sock->ReadLine(line);       
        if (st == eIO_Success) {
            if (m_Tmp == "END")
                break;
            out << line << "\n" << flush;
        } else {
            EIO_Status st = m_Sock->GetStatus(eIO_Open);
            if (st != eIO_Success) {
                break;
            }
        }
    }
    */
}


void CNetScheduleAPI::GetQueueList(CNetScheduleAPI::ISink& sink) const
{
    string cmd = "QLIST";
    for(CNetSrvConnectorPoll::iterator it = GetPoll().begin(); it != GetPoll().end(); ++it) {
        CNcbiOstream& os = sink.GetOstream(GetConnectionInfo(*it));
        os << SendCmdWaitResponse(*it, cmd);
    }
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

            if (service.empty()) {
                /*
                unsigned int rebalance_time = conf.GetInt(m_DriverName, 
                                                          "rebalance_time",
                                                          CConfig::eErr_NoThrow, 10);
                unsigned int rebalance_requests = conf.GetInt(m_DriverName,
                                                              "rebalance_requests",
                                                              CConfig::eErr_NoThrow, 100);
                */
                drv.reset(new CNetScheduleAPI(service,
                                              client_name, 
                                              queue_name) );
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


/*
 * ===========================================================================
 * $Log$
 * Revision 6.2  2007/01/09 16:05:02  didenko
 * Moved CNetScheduleExceptions to the new NetSchedule API
 *
 * Revision 6.1  2007/01/09 15:29:55  didenko
 * Added new API for NetSchedule service
 *
 * ===========================================================================
 */
