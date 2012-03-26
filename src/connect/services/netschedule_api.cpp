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
 * Author:  Anatoliy Kuznetsov, Maxim Didenko, Dmitry Kazimirov
 *
 * File Description:
 *   Implementation of NetSchedule API.
 *
 */

#include <ncbi_pch.hpp>

#include "netschedule_api_impl.hpp"

#include <connect/ncbi_conn_exception.hpp>

#include <corelib/ncbi_system.hpp>
#include <corelib/plugin_manager_impl.hpp>
#include <corelib/request_control.hpp>

#include <util/ncbi_url.hpp>

#include <memory>
#include <stdio.h>


BEGIN_NCBI_SCOPE

CNetScheduleNotificationHandler::CNetScheduleNotificationHandler(
        CNetScheduleJob& job, unsigned wait_time) :
    m_Job(job),
    m_Timeout(wait_time, 0)
{
    m_UDPSocket.SetReuseAddress(eOn);

    STimeout rto;
    rto.sec = rto.usec = 0;
    m_UDPSocket.SetTimeout(eIO_Read, &rto);

    EIO_Status status = m_UDPSocket.Bind(0);
    if (status != eIO_Success) {
        NCBI_THROW_FMT(CException, eUnknown,
            "Could not bind a UDP socket: " << IO_StatusStr(status));
    }

    m_UDPPort = m_UDPSocket.GetLocalPort(eNH_HostByteOrder);
}

bool CNetScheduleNotificationHandler::ParseNotification(
        const CTempString& expected_prefix,
        const string& attr_name,
        string* attr_value)
{
    CTempString prefix;
    CTempString attrs;

    if (NStr::SplitInTwo(m_Message, " ", prefix, attrs, NStr::eMergeDelims) &&
            prefix == expected_prefix)
        try {
            CUrlArgs attr_parser(attrs);
            const CUrlArgs::TArgs& attr_list = attr_parser.GetArgs();
            CUrlArgs::const_iterator attr_it = attr_parser.FindFirst(attr_name);
            if (attr_it != attr_list.end()) {
                *attr_value = attr_it->value;
                return true;
            }
        }
        catch (CUrlParserException&) {
        }

    return false;
}

bool CNetScheduleNotificationHandler::WaitForNotification()
{
    STimeout timeout;
    size_t msg_len;

    for (;;) {
        m_Timeout.GetRemainingTime().Get(&timeout.sec, &timeout.usec);

        if (timeout.sec == 0 && timeout.usec == 0)
            return false;

        switch (m_UDPSocket.Wait(&timeout)) {
        case eIO_Timeout:
            return false;

        case eIO_Success:
            if (m_UDPSocket.Recv(m_Buffer, sizeof(m_Buffer), &msg_len,
                    &m_ServerHost, &m_ServerPort) == eIO_Success) {
                m_Message.assign(m_Buffer, msg_len);

                return true;
            }
            /* FALL THROUGH */

        default:
            break;
        }
    }

    return false;
}

/**********************************************************************/

void CNetScheduleServerListener::SetAuthString(SNetScheduleAPIImpl* impl)
{
    string auth(impl->m_Service->MakeAuthString());

    if (!impl->m_ProgramVersion.empty()) {
        auth += " prog=\"";
        auth += impl->m_ProgramVersion;
        auth += '\"';
    }

    if (!impl->m_ClientNode.empty()) {
        auth += " client_node=\"";
        auth += impl->m_ClientNode;
        auth += '\"';
    }

    if (!impl->m_ClientSession.empty()) {
        auth += " client_session=\"";
        auth += impl->m_ClientSession;
        auth += '\"';
    }

    auth += "\r\n";

    auth += impl->m_Queue;

    // Make the auth token look like a command to be able to
    // check for potential authentication/initialization errors
    // like the "queue not found" error.
    if (!m_WorkerNodeCompatMode)
        auth += "\r\nVERSION";

    m_Auth = auth;
}

void CNetScheduleServerListener::OnInit(
    CObject* api_impl, CConfig* config, const string& config_section)
{
    SNetScheduleAPIImpl* ns_impl = static_cast<SNetScheduleAPIImpl*>(api_impl);

    if (ns_impl->m_Queue.empty()) {
        if (config == NULL) {
            NCBI_THROW(CConfigException, eParameterMissing,
                "Could not get queue name");
        }
        ns_impl->m_Queue = config->GetString(config_section,
            "queue_name", CConfig::eErr_Throw, "noname");
    }
    if (config == NULL)
        ns_impl->m_UseEmbeddedStorage = true;
    else {
        try {
            ns_impl->m_UseEmbeddedStorage = config->GetBool(config_section,
                "use_embedded_storage", CConfig::eErr_Throw, true);
        }
        catch (CConfigException&) {
            ns_impl->m_UseEmbeddedStorage = config->GetBool(config_section,
                "use_embedded_input", CConfig::eErr_NoThrow, true);
        }

        ns_impl->m_ClientNode = config->GetString(config_section,
            "client_node", CConfig::eErr_NoThrow, kEmptyStr);

        if (!ns_impl->m_ClientNode.empty())
            ns_impl->m_ClientSession = GetDiagContext().GetStringUID();
    }

    SetAuthString(ns_impl);
}

void CNetScheduleServerListener::OnConnected(
    CNetServerConnection::TInstance conn)
{
    CNetServerConnection conn_object(conn);

    if (!m_WorkerNodeCompatMode)
        conn_object.Exec(m_Auth);
    else
        conn->WriteLine(m_Auth);
}

void CNetScheduleServerListener::OnError(
    const string& err_msg, SNetServerImpl* /*server*/)
{
    string code;
    string msg;

    if (!NStr::SplitInTwo(err_msg, ":", code, msg)) {
        if (err_msg == "Job not found") {
            NCBI_THROW(CNetScheduleException, eJobNotFound, err_msg);
        }
        code = err_msg;
        msg = err_msg;
    }

    // Map code into numeric value
    CException::TErrCode err_code =
        SNetScheduleAPIImpl::sm_ExceptionMap.GetCode(code);

    switch (err_code) {
    case CException::eInvalid:
        NCBI_THROW(CNetServiceException, eCommunicationError, err_msg);

    case CNetScheduleException::eGroupNotFound:
        // Ignore this error.
        break;

    case CNetScheduleException::eJobNotFound:
        msg = "Job not found";
        /* FALL THROUGH */

    default:
        NCBI_THROW(CNetScheduleException, EErrCode(err_code), msg);
    }
}

void CNetScheduleServerListener::OnWarning(const string& warn_msg,
        SNetServerImpl* server)
{
    if (m_EventHandler)
        m_EventHandler->OnWarning(warn_msg, server);
    else {
        LOG_POST(Warning << server->m_ServerInPool->m_Address.AsString() <<
                ": " << warn_msg);
    }
}

const char* const kNetScheduleAPIDriverName = "netschedule_api";

static const char* const s_NetScheduleConfigSections[] = {
    kNetScheduleAPIDriverName,
    NULL
};

static const string s_NetScheduleAPIName("NetScheduleAPI");

SNetScheduleAPIImpl::SNetScheduleAPIImpl(
        CConfig* config, const string& section,
        const string& service_name, const string& client_name,
        const string& queue_name) :
    m_Service(new SNetServiceImpl(s_NetScheduleAPIName,
        client_name, new CNetScheduleServerListener)),
    m_Queue(queue_name)
{
    m_Service->Init(this, service_name,
        config, section, s_NetScheduleConfigSections);
}

SNetScheduleAPIImpl::SNetScheduleAPIImpl(
        SNetServerInPool* server, SNetScheduleAPIImpl* parent) :
    m_Service(new SNetServiceImpl(server, parent->m_Service)),
    m_Queue(parent->m_Queue),
    m_ProgramVersion(parent->m_ProgramVersion),
    m_ClientNode(parent->m_ClientNode),
    m_ClientSession(parent->m_ClientSession),
    m_UseEmbeddedStorage(parent->m_UseEmbeddedStorage)
{
}

CNetScheduleExceptionMap SNetScheduleAPIImpl::sm_ExceptionMap;

CNetScheduleAPI::CNetScheduleAPI(CNetScheduleAPI::EAppRegistry /*use_app_reg*/,
        const string& conf_section /* = kEmptyStr */) :
    m_Impl(new SNetScheduleAPIImpl(NULL, conf_section,
        kEmptyStr, kEmptyStr, kEmptyStr))
{
}

CNetScheduleAPI::CNetScheduleAPI(const IRegistry& reg,
        const string& conf_section)
{
    CConfig conf(reg);
    m_Impl = new SNetScheduleAPIImpl(&conf, conf_section,
        kEmptyStr, kEmptyStr, kEmptyStr);
}

CNetScheduleAPI::CNetScheduleAPI(CConfig* conf, const string& conf_section) :
    m_Impl(new SNetScheduleAPIImpl(conf, conf_section,
        kEmptyStr, kEmptyStr, kEmptyStr))
{
}

CNetScheduleAPI::CNetScheduleAPI(const string& service_name,
        const string& client_name, const string& queue_name) :
    m_Impl(new SNetScheduleAPIImpl(NULL, kEmptyStr,
        service_name, client_name, queue_name))
{
}

void CNetScheduleAPI::SetProgramVersion(const string& pv)
{
    m_Impl->m_ProgramVersion = pv;

    m_Impl->UpdateAuthString();
}

const string& CNetScheduleAPI::GetProgramVersion() const
{
    return m_Impl->m_ProgramVersion;
}

const string& CNetScheduleAPI::GetQueueName() const
{
    return m_Impl->m_Queue;
}

string CNetScheduleAPI::StatusToString(EJobStatus status)
{
    switch(status) {
    case eJobNotFound: return "NotFound";
    case ePending:     return "Pending";
    case eRunning:     return "Running";
    case eCanceled:    return "Canceled";
    case eFailed:      return "Failed";
    case eDone:        return "Done";
    case eReading:     return "Reading";
    case eConfirmed:   return "Confirmed";
    case eReadFailed:  return "ReadFailed";

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
    if (NStr::CompareNocase(status_str, "Canceled") == 0) {
        return eCanceled;
    }
    if (NStr::CompareNocase(status_str, "Failed") == 0) {
        return eFailed;
    }
    if (NStr::CompareNocase(status_str, "Done") == 0) {
        return eDone;
    }
    if (NStr::CompareNocase(status_str, "Reading") == 0) {
        return eReading;
    }
    if (NStr::CompareNocase(status_str, "Confirmed") == 0) {
        return eConfirmed;
    }
    if (NStr::CompareNocase(status_str, "ReadFailed") == 0) {
        return eReadFailed;
    }


    // check acceptable synonyms

    if (NStr::CompareNocase(status_str, "Pend") == 0) {
        return ePending;
    }
    if (NStr::CompareNocase(status_str, "Run") == 0) {
        return eRunning;
    }
    if (NStr::CompareNocase(status_str, "Cancel") == 0) {
        return eCanceled;
    }
    if (NStr::CompareNocase(status_str, "Fail") == 0) {
        return eFailed;
    }

    return eJobNotFound;
}

CNetScheduleSubmitter CNetScheduleAPI::GetSubmitter()
{
    return new SNetScheduleSubmitterImpl(m_Impl);
}

CNetScheduleExecutor CNetScheduleAPI::GetExecutor()
{
    return new SNetScheduleExecutorImpl(m_Impl);
}

CNetScheduleAdmin CNetScheduleAPI::GetAdmin()
{
    return new SNetScheduleAdminImpl(m_Impl);
}

CNetService CNetScheduleAPI::GetService()
{
    return m_Impl->m_Service;
}

#define SKIP_SPACE(ptr) \
    while (isspace((unsigned char) *(ptr))) \
        ++ptr;

CNetScheduleAPI::EJobStatus
    CNetScheduleAPI::GetJobDetails(CNetScheduleJob& job)
{
    job.input.erase();
    job.affinity.erase();
    job.mask = CNetScheduleAPI::eEmptyMask;
    job.ret_code = 0;
    job.output.erase();
    job.error_msg.erase();
    job.progress_msg.erase();

    string resp = m_Impl->x_SendJobCmdWaitResponse("STATUS" , job.job_id);

    const char* str = resp.c_str();

    EJobStatus status = (EJobStatus) atoi(str);

    size_t field_len;

    switch (status) {
    case ePending:
    case eRunning:
    case eCanceled:
    case eFailed:
    case eDone:
    case eReading:
    case eConfirmed:
    case eReadFailed:
        while (*str && !isspace((unsigned char) *str))
            ++str;
        SKIP_SPACE(str);

        job.ret_code = atoi(str);

        while (*str && !isspace((unsigned char) *str))
            ++str;
        SKIP_SPACE(str);

        if (*str) {
            job.output = NStr::ParseQuoted(str, &field_len);

            str += field_len;
            SKIP_SPACE(str);

            if (*str) {
                job.error_msg = NStr::ParseQuoted(str, &field_len);

                str += field_len;
                SKIP_SPACE(str);

                if (*str)
                    job.input = NStr::ParseQuoted(str);
            }
        }

        /* FALL THROUGH */

    default:
        return status;
    }
}

CNetScheduleAPI::EJobStatus SNetScheduleAPIImpl::x_GetJobStatus(
    const string& job_key, bool submitter)
{
    return (CNetScheduleAPI::EJobStatus) atoi(
        x_SendJobCmdWaitResponse(submitter ? "SST" : "WST", job_key).c_str());
}

const CNetScheduleAPI::SServerParams& SNetScheduleAPIImpl::GetServerParams()
{
    CFastMutexGuard g(m_FastMutex);

    if (!m_ServerParams.get())
        m_ServerParams.reset(new CNetScheduleAPI::SServerParams);
    else
        if (m_ServerParamsAskCount-- > 0)
            return *m_ServerParams;

    m_ServerParamsAskCount = SERVER_PARAMS_ASK_MAX_COUNT;

    m_ServerParams->max_input_size = kMax_UInt;
    m_ServerParams->max_output_size = kMax_UInt;
    m_ServerParams->fast_status = true;

    bool was_called = false;

    for (CNetServiceIterator it = m_Service.Iterate(); it; ++it) {
        was_called = true;

        string resp;
        try {
            resp = (*it).ExecWithRetry("GETP").response;
        } catch (CNetScheduleException& ex) {
            if (ex.GetErrCode() != CNetScheduleException::eProtocolSyntaxError)
                throw;
        } catch (...) {
        }
        list<string> spars;
        NStr::Split(resp, ";", spars);
        bool fast_status = false;
        ITERATE(list<string>, param, spars) {
            string n,v;
            NStr::SplitInTwo(*param,"=",n,v);
            if (n == "max_input_size") {
                size_t val = NStr::StringToInt(v) / 4;
                if (m_ServerParams->max_input_size > val)
                    m_ServerParams->max_input_size = val ;
            } else if (n == "max_output_size") {
                size_t val = NStr::StringToInt(v) / 4;
                if (m_ServerParams->max_output_size > val)
                    m_ServerParams->max_output_size = val;
            } else if (n == "fast_status" && v == "1") {
                fast_status = true;
            }
        }
        if (m_ServerParams->fast_status)
            m_ServerParams->fast_status = fast_status;
    }

    if (m_ServerParams->max_input_size == kMax_UInt)
        m_ServerParams->max_input_size = kNetScheduleMaxDBDataSize / 4;
    if (m_ServerParams->max_output_size == kMax_UInt)
        m_ServerParams->max_output_size = kNetScheduleMaxDBDataSize / 4;
    if (!was_called)
        m_ServerParams->fast_status = false;

    return *m_ServerParams;
}

const CNetScheduleAPI::SServerParams& CNetScheduleAPI::GetServerParams()
{
    return m_Impl->GetServerParams();
}

void CNetScheduleAPI::GetProgressMsg(CNetScheduleJob& job)
{
    string resp = m_Impl->x_SendJobCmdWaitResponse("MGET", job.job_id);
    job.progress_msg = NStr::ParseEscapes(resp);
}

static void s_VerifyClientCredentialString(const string& str,
    const CTempString& param_name)
{
    if (str.empty()) {
        NCBI_THROW_FMT(CConfigException, eParameterMissing,
            "'" << param_name << "' cannot be empty");
    }

    g_VerifyAlphabet(str, param_name, eCC_RelaxedId);
}

void CNetScheduleAPI::SetClientNode(const string& client_node)
{
    s_VerifyClientCredentialString(client_node, "client node ID");

    m_Impl->m_ClientNode = client_node;

    m_Impl->UpdateAuthString();
}

void CNetScheduleAPI::SetClientSession(const string& client_session)
{
    s_VerifyClientCredentialString(client_session, "client session ID");

    m_Impl->m_ClientSession = client_session;

    m_Impl->UpdateAuthString();
}

void CNetScheduleAPI::EnableWorkerNodeCompatMode()
{
    CNetScheduleServerListener* listener =
            static_cast<CNetScheduleServerListener*>(
            m_Impl->m_Service->m_Listener.GetPointer());

    listener->m_WorkerNodeCompatMode = true;
    listener->SetAuthString(m_Impl);
}

void CNetScheduleAPI::UseOldStyleAuth()
{
    m_Impl->m_Service->m_ServerPool->m_UseOldStyleAuth = true;

    m_Impl->UpdateAuthString();
}

CNetScheduleAPI CNetScheduleAPI::GetServer(CNetServer::TInstance server)
{
    return new SNetScheduleAPIImpl(server->m_ServerInPool, m_Impl);
}

void CNetScheduleAPI::SetEventHandler(IEventHandler* event_handler)
{
    static_cast<CNetScheduleServerListener*>(m_Impl->m_Service->
            m_Listener.GetPointer())->m_EventHandler = event_handler;
}

///////////////////////////////////////////////////////////////////////////////

/// @internal
class CNetScheduleAPICF : public IClassFactory<SNetScheduleAPIImpl>
{
public:

    typedef SNetScheduleAPIImpl TDriver;
    typedef SNetScheduleAPIImpl IFace;
    typedef IFace TInterface;
    typedef IClassFactory<SNetScheduleAPIImpl> TParent;
    typedef TParent::SDriverInfo TDriverInfo;
    typedef TParent::TDriverList TDriverList;

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
        if (params && (driver.empty() || driver == m_DriverName) &&
                version.Match(NCBI_INTERFACE_VERSION(IFace)) !=
                    CVersionInfo::eNonCompatible) {
            CConfig config(params);
            return new SNetScheduleAPIImpl(&config, m_DriverName,
                kEmptyStr, kEmptyStr, kEmptyStr);
        }
        return NULL;
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
     CPluginManager<SNetScheduleAPIImpl>::TDriverInfoList&   info_list,
     CPluginManager<SNetScheduleAPIImpl>::EEntryPointRequest method)
{
       CHostEntryPointImpl<CNetScheduleAPICF>::
           NCBI_EntryPointImpl(info_list, method);

}


END_NCBI_SCOPE
