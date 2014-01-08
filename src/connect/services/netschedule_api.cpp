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
#include <corelib/request_ctx.hpp>

#include <util/ncbi_url.hpp>

#include <memory>
#include <stdio.h>


#define COMPATIBLE_NETSCHEDULE_VERSION "4.10.0"


BEGIN_NCBI_SCOPE

void g_AppendClientIPAndSessionID(string& cmd, const string* default_session)
{
    CRequestContext& req = CDiagContext::GetRequestContext();

    if (req.IsSetClientIP()) {
        cmd += " ip=\"";
        cmd += req.GetClientIP();
        cmd += '"';
    }

    if (req.IsSetSessionID() || default_session != NULL) {
        cmd += " sid=\"";
        cmd += NStr::PrintableString(default_session != NULL ?
                *default_session : req.GetSessionID());
        cmd += '"';
    }
}

CNetScheduleNotificationHandler::CNetScheduleNotificationHandler()
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

int g_ParseNSOutput(const string& attr_string, const char* const* attr_names,
        string* attr_values, int attr_count)
{
    try {
        CUrlArgs attr_parser(attr_string);
        const CUrlArgs::TArgs& attr_list = attr_parser.GetArgs();

        int found_attrs = 0;

        CUrlArgs::const_iterator attr_it;

        do {
            if ((attr_it = attr_parser.FindFirst(*attr_names)) !=
                    attr_list.end()) {
                *attr_values = attr_it->value;
                ++found_attrs;
            }
            ++attr_names;
            ++attr_values;
        } while (--attr_count > 0);

        return found_attrs;
    }
    catch (CUrlParserException&) {
    }

    return -1;
}

bool CNetScheduleNotificationHandler::ReceiveNotification(string* server_host)
{
    size_t msg_len;

    if (m_UDPSocket.Recv(m_Buffer, sizeof(m_Buffer), &msg_len,
            server_host, NULL) != eIO_Success)
        return false;

    while (msg_len > 0 && m_Buffer[msg_len - 1] == '\0')
        --msg_len;
    m_Message.assign(m_Buffer, msg_len);

    return true;
}

bool CNetScheduleNotificationHandler::WaitForNotification(
        const CDeadline& deadline, string* server_host)
{
    STimeout timeout;

    for (;;) {
        deadline.GetRemainingTime().Get(&timeout.sec, &timeout.usec);

        if (timeout.sec == 0 && timeout.usec == 0)
            return false;

        switch (m_UDPSocket.Wait(&timeout)) {
        case eIO_Timeout:
            return false;

        case eIO_Success:
            if (ReceiveNotification(server_host))
                return true;
            /* FALL THROUGH */

        default:
            break;
        }
    }

    return false;
}

void CNetScheduleNotificationHandler::PrintPortNumber()
{
    printf("Using UDP port %hu\n", GetPort());
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

    CNcbiApplication* app = CNcbiApplication::Instance();
    if (app != NULL) {
        auth += " client_version=\"";
        auth += app->GetFullVersion().GetVersionInfo().Print();
        auth += '\"';
    }

    ITERATE(SNetScheduleAPIImpl::TAuthParams, it, impl->m_AuthParams) {
        auth += it->second;
    }

    auth += " ns_compat_ver=\"" COMPATIBLE_NETSCHEDULE_VERSION "\""
        "\r\n";

    auth += impl->m_Queue;

    // Make the auth token look like a command to be able to
    // check for potential authentication/initialization errors
    // like the "queue not found" error.
    if (!m_WorkerNodeCompatMode) {
        auth += "\r\nVERSION";
        g_AppendClientIPAndSessionID(auth,
                impl->m_ClientSession.empty() ? NULL : &impl->m_ClientSession);
    }

    m_Auth = auth;
}

bool CNetScheduleServerListener::NeedToSubmitAffinities(
        SNetServerImpl* server_impl)
{
    return !x_GetServerProperties(server_impl)->affs_synced;
}

void CNetScheduleServerListener::SetAffinitiesSynced(
        SNetServerImpl* server_impl, bool affs_synced)
{
    x_GetServerProperties(server_impl)->affs_synced = affs_synced;
}

CRef<SNetScheduleServerProperties> CNetScheduleServerListener::x_GetServerProperties(
        SNetServerImpl* server_impl)
{
    return CRef<SNetScheduleServerProperties>(
            static_cast<SNetScheduleServerProperties*>(
                    server_impl->m_ServerInPool->
                            m_ServerProperties.GetPointerOrNull()));
}

CRef<INetServerProperties> CNetScheduleServerListener::AllocServerProperties()
{
    return CRef<INetServerProperties>(new SNetScheduleServerProperties);
}

CConfig* CNetScheduleServerListener::LoadConfigFromAltSource(
    CObject* /*api_impl*/, string* /*new_section_name*/)
{
    return NULL;
}

void CNetScheduleServerListener::OnInit(
    CObject* api_impl, CConfig* config, const string& config_section)
{
    SNetScheduleAPIImpl* ns_impl = static_cast<SNetScheduleAPIImpl*>(api_impl);

    if (!ns_impl->m_Queue.empty())
        SNetScheduleAPIImpl::VerifyQueueNameAlphabet(ns_impl->m_Queue);
    else if (config == NULL)
        ns_impl->m_Queue = "noname";
    else {
        ns_impl->m_Queue = config->GetString(config_section,
            "queue_name", CConfig::eErr_NoThrow, kEmptyStr);
        if (!ns_impl->m_Queue.empty())
            SNetScheduleAPIImpl::VerifyQueueNameAlphabet(ns_impl->m_Queue);
        else
            ns_impl->m_Queue = "noname";
    }
    if (config == NULL) {
        ns_impl->m_AffinityPreference = CNetScheduleExecutor::eAnyJob;
        ns_impl->m_UseEmbeddedStorage = true;
    } else {
        try {
            ns_impl->m_UseEmbeddedStorage = config->GetBool(config_section,
                "use_embedded_storage", CConfig::eErr_Throw, true);
        }
        catch (CConfigException&) {
            ns_impl->m_UseEmbeddedStorage = config->GetBool(config_section,
                "use_embedded_input", CConfig::eErr_NoThrow, true);
        }

        bool use_affinities = config->GetBool(config_section,
                "use_affinities", CConfig::eErr_NoThrow, false);
        bool claim_new_affinities = config->GetBool(config_section,
                "claim_new_affinities", CConfig::eErr_NoThrow, false);

        ns_impl->m_AffinityPreference =
                !use_affinities && !claim_new_affinities ?
                        CNetScheduleExecutor::eAnyJob :
                claim_new_affinities ?
                        CNetScheduleExecutor::eClaimNewPreferredAffs :
                        CNetScheduleExecutor::ePreferredAffsOrAnyJob;

        ns_impl->m_ClientNode = config->GetString(config_section,
            "client_node", CConfig::eErr_NoThrow, kEmptyStr);

        if (!ns_impl->m_ClientNode.empty())
            ns_impl->m_ClientSession = GetDiagContext().GetStringUID();
    }

    SetAuthString(ns_impl);
}

void CNetScheduleServerListener::OnConnected(
        CNetServerConnection::TInstance conn_impl)
{
    if (!m_WorkerNodeCompatMode) {
        CNetServerConnection conn(conn_impl);

        string version_info(conn.Exec(m_Auth));

        CNetServerInfo server_info(new SNetServerInfoImpl(version_info));

        string attr_name, attr_value;
        string ns_node, ns_session;

        while (server_info.GetNextAttribute(attr_name, attr_value))
            if (attr_name == "ns_node")
                ns_node = attr_value;
            else if (attr_name == "ns_session")
                ns_session = attr_value;

        if (!ns_node.empty() && !ns_session.empty()) {
            CRef<SNetScheduleServerProperties> server_props =
                x_GetServerProperties(conn_impl->m_Server);

            if (server_props->ns_node != ns_node ||
                    server_props->ns_session != ns_session) {
                CFastMutexGuard guard(m_ServerByNodeMutex);

                server_props->ns_node = ns_node;
                server_props->ns_session = ns_session;
                m_ServerByNode[ns_node] = conn_impl->m_Server->m_ServerInPool;
                server_props->affs_synced = false;
            }
        }
    } else
        conn_impl->WriteLine(m_Auth);
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
    } else if (msg.empty())
        msg = code;

    // Map code into numeric value
    CException::TErrCode err_code = CNetScheduleExceptionMap::GetCode(code);

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

SNetScheduleAPIImpl::SNetScheduleAPIImpl(
        CConfig* config, const string& section,
        const string& service_name, const string& client_name,
        const string& queue_name) :
    m_Service(new SNetServiceImpl("NetScheduleAPI",
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
    m_AffinityPreference(parent->m_AffinityPreference),
    m_UseEmbeddedStorage(parent->m_UseEmbeddedStorage)
{
}

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

    UpdateAuthString();
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
    case eDeleted:     return "Deleted";

    default: _ASSERT(0);
    }
    return kEmptyStr;
}

CNetScheduleAPI::EJobStatus
CNetScheduleAPI::StringToStatus(const CTempString& status_str)
{
    if (NStr::CompareNocase(status_str, "Pending") == 0)
        return ePending;
    if (NStr::CompareNocase(status_str, "Running") == 0)
        return eRunning;
    if (NStr::CompareNocase(status_str, "Canceled") == 0)
        return eCanceled;
    if (NStr::CompareNocase(status_str, "Failed") == 0)
        return eFailed;
    if (NStr::CompareNocase(status_str, "Done") == 0)
        return eDone;
    if (NStr::CompareNocase(status_str, "Reading") == 0)
        return eReading;
    if (NStr::CompareNocase(status_str, "Confirmed") == 0)
        return eConfirmed;
    if (NStr::CompareNocase(status_str, "ReadFailed") == 0)
        return eReadFailed;
    if (NStr::CompareNocase(status_str, "Deleted") == 0)
        return eDeleted;

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
    CNetScheduleAPI::GetJobDetails(CNetScheduleJob& job, time_t* job_exptime)
{
    string resp = m_Impl->x_ExecOnce("STATUS2", job.job_id);

    job.input.erase();
    job.affinity.erase();
    job.mask = CNetScheduleAPI::eEmptyMask;
    job.ret_code = 0;
    job.output.erase();
    job.error_msg.erase();
    job.progress_msg.erase();

    static const char* const s_JobStatusAttrNames[] = {
            "job_status",       // 0
            "job_exptime",      // 1
            "input",            // 2
            "output",           // 3
            "ret_code",         // 4
            "err_msg"};         // 5

#define NUMBER_OF_STATUS_ATTRS (sizeof(s_JobStatusAttrNames) / \
    sizeof(*s_JobStatusAttrNames))

    string attr_values[NUMBER_OF_STATUS_ATTRS];

    g_ParseNSOutput(resp, s_JobStatusAttrNames,
            attr_values, NUMBER_OF_STATUS_ATTRS);

    EJobStatus status = StringToStatus(attr_values[0]);

    if (job_exptime != NULL)
        *job_exptime = (time_t) NStr::StringToUInt8(attr_values[1],
                NStr::fConvErr_NoThrow);

    switch (status) {
    case ePending:
    case eRunning:
    case eCanceled:
    case eFailed:
    case eDone:
    case eReading:
    case eConfirmed:
    case eReadFailed:
        job.input = attr_values[2];
        job.output = attr_values[3];
        job.ret_code = NStr::StringToInt(attr_values[4],
                NStr::fConvErr_NoThrow);
        job.error_msg = attr_values[5];

        /* FALL THROUGH */

    default:
        return status;
    }
}

CNetScheduleAPI::EJobStatus SNetScheduleAPIImpl::GetJobStatus(const string& cmd,
        const string& job_key, time_t* job_exptime)
{
    string response;

    try {
        response = x_ExecOnce(cmd, job_key);
    }
    catch (CNetScheduleException& e) {
        if (e.GetErrCode() != CNetScheduleException::eJobNotFound)
            throw;

        if (job_exptime != NULL)
            *job_exptime = 0;

        return CNetScheduleAPI::eJobNotFound;
    }

    static const char* const s_AttrNames[] = {
        "job_status",       // 0
        "job_exptime"       // 1
    };

#define NUMBER_OF_ATTRS (sizeof(s_AttrNames) / sizeof(*s_AttrNames))

    string attr_values[NUMBER_OF_ATTRS];

    g_ParseNSOutput(response, s_AttrNames, attr_values, NUMBER_OF_ATTRS);

    if (job_exptime != NULL)
        *job_exptime = (time_t) NStr::StringToUInt8(attr_values[1],
                NStr::fConvErr_NoThrow);

    return CNetScheduleAPI::StringToStatus(attr_values[0]);
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

    m_ServerParams->max_input_size = kNetScheduleMaxDBDataSize;
    m_ServerParams->max_output_size = kNetScheduleMaxDBDataSize;

    string cmd("QINF2 " + m_Queue);
    g_AppendClientIPAndSessionID(cmd);

    CUrlArgs url_parser(m_Service.FindServerAndExec(cmd).response);

    enum {
        eMaxInputSize,
        eMaxOutputSize,
        eNumberOfSizeParams
    };

    int field_bits = 0;

    ITERATE(CUrlArgs::TArgs, field, url_parser.GetArgs()) {
        if (field->name[0] == 'm') {
            if (field->name == "max_input_size") {
                field_bits |= (1 << eMaxInputSize);
                m_ServerParams->max_input_size =
                        NStr::StringToInt(field->value);
            } else if (field->name == "max_output_size") {
                field_bits |= (1 << eMaxOutputSize);
                m_ServerParams->max_output_size =
                        NStr::StringToInt(field->value);
            }
        }
        if (field_bits == (1 << eNumberOfSizeParams) - 1)
            break;
    }

    return *m_ServerParams;
}

const CNetScheduleAPI::SServerParams& CNetScheduleAPI::GetServerParams()
{
    return m_Impl->GetServerParams();
}

void CNetScheduleAPI::GetQueueParams(
        const string& queue_name, TQueueParams& queue_params)
{
    string cmd;

    if (queue_name.empty())
        cmd = "QINF2 " + m_Impl->m_Queue;
    else {
        SNetScheduleAPIImpl::VerifyQueueNameAlphabet(queue_name);

        cmd = "QINF2 " + queue_name;
    }

    g_AppendClientIPAndSessionID(cmd);

    CUrlArgs url_parser(m_Impl->m_Service.FindServerAndExec(cmd).response);

    ITERATE(CUrlArgs::TArgs, field, url_parser.GetArgs()) {
        queue_params[field->name] = field->value;
    }
}

void CNetScheduleAPI::GetQueueParams(
        CNetScheduleAPI::TQueueParams& queue_params)
{
    string cmd("GETP2");
    g_AppendClientIPAndSessionID(cmd);

    CUrlArgs url_parser(m_Impl->m_Service.FindServerAndExec(cmd).response);

    ITERATE(CUrlArgs::TArgs, field, url_parser.GetArgs()) {
        queue_params[field->name] = field->value;
    }
}

void CNetScheduleAPI::GetProgressMsg(CNetScheduleJob& job)
{
    job.progress_msg = NStr::ParseEscapes(
            m_Impl->x_ExecOnce("MGET", job.job_id));
}

void SNetScheduleAPIImpl::VerifyQueueNameAlphabet(const string& queue_name)
{
    if (queue_name.empty()) {
        NCBI_THROW_FMT(CConfigException, eParameterMissing,
                "Queue name cannot be empty.");
    }
    if (queue_name[0] == '_') {
        NCBI_THROW_FMT(CConfigException, eParameterMissing,
                "Queue name cannot start with an underscore character.");
    }
    g_VerifyAlphabet(queue_name, "queue name", eCC_BASE64URL);
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

    UpdateAuthString();
}

void CNetScheduleAPI::SetClientSession(const string& client_session)
{
    s_VerifyClientCredentialString(client_session, "client session ID");

    m_Impl->m_ClientSession = client_session;

    UpdateAuthString();
}

void CNetScheduleAPI::UpdateAuthString()
{
    m_Impl->m_Service->m_ServerPool->ResetServerConnections();

    m_Impl->GetListener()->SetAuthString(m_Impl);
}

void CNetScheduleAPI::EnableWorkerNodeCompatMode()
{
    m_Impl->GetListener()->m_WorkerNodeCompatMode = true;

    UpdateAuthString();
}

void CNetScheduleAPI::UseOldStyleAuth()
{
    m_Impl->m_Service->m_ServerPool->m_UseOldStyleAuth = true;

    UpdateAuthString();
}

CNetScheduleAPI CNetScheduleAPI::GetServer(CNetServer::TInstance server)
{
    return new SNetScheduleAPIImpl(server->m_ServerInPool, m_Impl);
}

void CNetScheduleAPI::SetEventHandler(INetEventHandler* event_handler)
{
    m_Impl->GetListener()->m_EventHandler = event_handler;
}

void CNetScheduleAPI::SetAuthParam(const string& param_name,
        const string& param_value)
{
    if (!param_value.empty()) {
        string auth_param(' ' + param_name);
        auth_param += "=\"";
        auth_param += NStr::PrintableString(param_value);
        auth_param += '"';
        m_Impl->m_AuthParams[param_name] = auth_param;
    } else
        m_Impl->m_AuthParams.erase(param_name);

    UpdateAuthString();
}

CCompoundIDPool CNetScheduleAPI::GetCompoundIDPool()
{
    return m_Impl->m_CompoundIDPool;
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
