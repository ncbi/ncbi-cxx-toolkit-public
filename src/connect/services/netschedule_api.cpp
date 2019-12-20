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

#include <connect/ncbi_socket.hpp>
#include <connect/ncbi_conn_exception.hpp>
#include <connect/ncbi_userhost.hpp>

#include <corelib/ncbi_system.hpp>
#include <corelib/plugin_manager_impl.hpp>

#include <array>
#include <memory>
#include <stdio.h>


#define COMPATIBLE_NETSCHEDULE_VERSION "4.10.0"


BEGIN_NCBI_SCOPE

namespace grid {
namespace netschedule {
namespace limits {

void ThrowIllegalChar(const string& name, const string& value, char c)
{
    NCBI_THROW_FMT(CConfigException, eInvalidParameter,
            "Invalid character '" << NStr::PrintableString(CTempString(&c, 1)) <<
            "' in the " << name << " \"" << NStr::PrintableString(value) << "\".");
}

}
}
}

using namespace grid::netschedule;

SNetScheduleNotificationThread::SNetScheduleNotificationThread(
        SNetScheduleAPIImpl* ns_api) :
    m_API(ns_api),
    m_StopThread(false)
{
}

SNetScheduleNotificationReceiver::SNetScheduleNotificationReceiver()
{
    STimeout rto;
    rto.sec = rto.usec = 0;
    socket.SetDataLogging(TServConn_ConnDataLogging::GetDefault() ? eOn : eOff);
    socket.SetTimeout(eIO_Read, &rto);

    EIO_Status status = socket.Bind(0);
    if (status != eIO_Success) {
        NCBI_THROW_FMT(CException, eUnknown,
            "Could not bind a UDP socket: " << IO_StatusStr(status));
    }

    port = socket.GetLocalPort(eNH_HostByteOrder);
}

void SNetScheduleNotificationThread::CmdAppendPortAndTimeout(
        string* cmd, unsigned remaining_seconds)
{
    if (remaining_seconds > 0) {
        *cmd += " port=";
        *cmd += NStr::UIntToString(GetPort());

        *cmd += " timeout=";
        *cmd += NStr::UIntToString(remaining_seconds);
    }
}

CNetScheduleNotificationHandler::CNetScheduleNotificationHandler()
{
}

CUrlArgs s_CreateCUrlArgs(const string& output)
try {
    return CUrlArgs(output);
}
catch (CUrlParserException&) {
    return CUrlArgs();
}

SNetScheduleOutputParser::SNetScheduleOutputParser(const string& output) :
        CUrlArgs(s_CreateCUrlArgs(output))
{
}

const string& SNetScheduleOutputParser::operator()(const string& param) const
{
    auto it = FindFirst(param);
    return it != GetArgs().end() ? it->value : kEmptyStr;
}

int g_ParseNSOutput(const string& attr_string, const char* const* attr_names,
        string* attr_values, size_t attr_count)
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

SNetScheduleNotificationThread::ENotificationType
        SNetScheduleNotificationThread::CheckNotification(string* ns_node)
{
    _ASSERT(ns_node);

    SNetScheduleOutputParser parser(m_Receiver.message);

    if (parser("queue") != m_API->m_Queue) return eNT_Unknown;

    *ns_node = parser("ns_node");

    const auto reason = parser("reason");

    if (reason.empty())
        return eNT_GetNotification;
    else if (NStr::CompareCase(reason, CTempString("get", 3)) == 0)
        return eNT_GetNotification;
    else if (NStr::CompareCase(reason, CTempString("read", 4)) == 0)
        return eNT_ReadNotification;
    else
        return eNT_Unknown;
}

void SServerNotifications::InterruptWait()
{
    CFastMutexGuard guard(m_Mutex);

    if (m_Interrupted)
        m_NotificationSemaphore.TryWait();
    else {
        m_Interrupted = true;
        if (!m_ReadyServers.empty())
            m_NotificationSemaphore.TryWait();
    }

    m_NotificationSemaphore.Post();
}

void SServerNotifications::RegisterServer(const string& ns_node)
{
    CFastMutexGuard guard(m_Mutex);

    if (!m_ReadyServers.empty())
        m_Interrupted = false;
    else {
        x_ClearInterruptFlag();
        m_NotificationSemaphore.Post();
    }

    m_ReadyServers.insert(ns_node);
}

bool SServerNotifications::GetNextNotification(string* ns_node)
{
    CFastMutexGuard guard(m_Mutex);

    x_ClearInterruptFlag();

    if (m_ReadyServers.empty())
        return false;

    TReadyServers::iterator next_server = m_ReadyServers.begin();
    *ns_node = *next_server;
    m_ReadyServers.erase(next_server);

    if (m_ReadyServers.empty())
        // Make sure the notification semaphore count is reset to zero.
        m_NotificationSemaphore.TryWait();

    return true;
}

void* SNetScheduleNotificationThread::Main()
{
    SetCurrentThreadName(
            (CNcbiApplication::Instance()->GetProgramDisplayName() +
                    "_nt").c_str());

    static const STimeout two_seconds = {2, 0};

    string server_host;

    while (!m_StopThread)
        if (m_Receiver.socket.Wait(&two_seconds) == eIO_Success) {
            if (m_StopThread)
                break;

            if (m_Receiver(&server_host)) {
                string ns_node;

                ENotificationType notif_type = CheckNotification(&ns_node);

                switch (notif_type) {
                case eNT_GetNotification:
                    m_GetNotifications.RegisterServer(ns_node);
                    break;
                case eNT_ReadNotification:
                    m_ReadNotifications.RegisterServer(ns_node);
                    break;
                default:
                    break;
                }
            }
        }
    return NULL;
}

bool SNetScheduleNotificationReceiver::operator()(string* server_host)
{
    array<char, 64 * 1024> buffer; // Enough to hold any UDP
    size_t msg_len;

    if (socket.Recv(buffer.data(), buffer.size(), &msg_len,
            server_host, NULL) != eIO_Success)
        return false;

    _ASSERT(buffer.size() > msg_len);
    buffer[msg_len] = '\0'; // Make it null-terminated in case it's not
    message.assign(buffer.data()); // Ignore everything after the first null character

    return true;
}

bool CNetScheduleNotificationHandler::ReceiveNotification(string* server_host)
{
    return m_Receiver(server_host);
}

bool CNetScheduleNotificationHandler::WaitForNotification(
        const CDeadline& deadline, string* server_host)
{
    STimeout timeout;

    for (;;) {
        deadline.GetRemainingTime().Get(&timeout.sec, &timeout.usec);

        if (timeout.sec == 0 && timeout.usec == 0)
            return false;

        switch (m_Receiver.socket.Wait(&timeout)) {
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

void SNetScheduleAPIImpl::AllocNotificationThread()
{
    CFastMutexGuard guard(m_NotificationThreadMutex);

    if (m_NotificationThread == NULL)
        m_NotificationThread = new SNetScheduleNotificationThread(this);
}

void SNetScheduleAPIImpl::StartNotificationThread()
{
    if (m_NotificationThreadStartStopCounter.Add(1) == 1)
        m_NotificationThread->Run();
}

SNetScheduleAPIImpl::~SNetScheduleAPIImpl()
{
    if (m_NotificationThreadStartStopCounter.Add(-1) == 0) {
        CFastMutexGuard guard(m_NotificationThreadMutex);

        if (m_NotificationThread != NULL) {
            m_NotificationThread->m_StopThread = true;
            CDatagramSocket().Send("INTERRUPT", sizeof("INTERRUPT"),
                    "127.0.0.1", m_NotificationThread->m_Receiver.port);
            m_NotificationThread->Join();
        }
    }
}

void SNetScheduleAPIImpl::x_ClearNode()
{
    string cmd("CLRN");
    g_AppendClientIPSessionIDHitID(cmd);

    for (CNetServiceIterator it =
            m_Service.Iterate(CNetService::eIncludePenalized); it; ++it) {
        CNetServer server = *it;

        try {
            CNetServer::SExecResult exec_result;
            server->ConnectAndExec(cmd, false, exec_result);
        } catch (CNetSrvConnException& e) {
            if (m_Service.IsLoadBalanced()) {
                ERR_POST(server->m_ServerInPool->m_Address.AsString() <<
                    ": " << e.what());
            }
        } catch (CNetServiceException& e) {
            if (e.GetErrCode() != CNetServiceException::eCommunicationError)
                throw;
            else {
                ERR_POST(server->m_ServerInPool->m_Address.AsString() <<
                    ": " << e.what());
            }
        }
    }
}

CTempString s_GetSection(bool ns_conf)
{
    return ns_conf ? "netschedule_conf_from_netschedule" : "netcache_conf_from_netschedule";
}

CNetScheduleConfigLoader::CNetScheduleConfigLoader(CSynRegistry& registry, SRegSynonyms& sections, bool ns_conf) :
    m_Registry(registry), m_Sections(sections), m_NsConf(ns_conf), m_Mode(eImplicit)
{
    sections.Insert(s_GetSection(m_NsConf));

    const auto param = "load_config_from_ns";

    if (m_Registry.Has(m_Sections, param)) {
        m_Mode = m_Registry.Get(m_Sections, param, true) ? eExplicit : eOff;
    }
}

bool CNetScheduleConfigLoader::Transform(const string& prefix, string& name) const
{
    if (m_NsConf) {
        // If it's "service to queue" special case (we do not know queue name)
        if (name == "queue_name") return true;

        // Queue parameter "timeout" determines the initial TTL of a submitted job.
        // Since "timeout" is too generic, replaced it with "job_ttl" on client side.
        if (name == "timeout") {
            name = "job_ttl";
            return true;
        }
    }

    // Do not load client_name from server
    if (name == "client_name") return false;

    // Only params starting with provided prefix are used
    if (NStr::StartsWith(name, prefix)) {
        name.erase(0, prefix.size());
        return true;
    }

    return false;
}

bool CNetScheduleConfigLoader::operator()(SNetScheduleAPIImpl* impl)
{
    _ASSERT(impl);

    if (m_Mode == eOff) return false;

    // Turn off any subsequent attempts
    const auto mode = m_Mode;
    m_Mode = eOff;

    // Errors could happen when we try to load config from servers that either
    // do not support "GETP2" command (introduced in 4.16.9)
    // or, do not support "QINF2 service=name" command (introduced in 4.17.0)
    // or, do not have "service to queue" mapping set
    // or, are not actually NetSchedule servers but worker nodes
    // or, are currently not reachable (behind some firewall)
    // and need cross connectivity which is usually enabled later
    //
    // This guard is set to suppress errors and avoid retries if config loading is not enabled explicitly
    const auto retry_mode = mode == eImplicit ?
        SNetServiceImpl::SRetry::eNoRetryNoErrors :
        SNetServiceImpl::SRetry::eDefault;
    auto retry_guard = impl->m_Service->CreateRetryGuard(retry_mode);

    CNetScheduleAPI::TQueueParams queue_params;

    try {
        impl->GetQueueParams(kEmptyStr, queue_params);
    }
    catch (...) {
        if (mode == eExplicit) throw;
        return false;
    }

    CRef<CMemoryRegistry> mem_registry(new CMemoryRegistry);
    const string prefix = m_NsConf ? "ns." : "nc.";
    const string section = s_GetSection(m_NsConf);

    for (auto& param : queue_params) {
        auto name = param.first;

        if (Transform(prefix, name)) {
            mem_registry->Set(section, name, param.second);
        }
    }

    if (mem_registry->Empty()) return false;

    m_Registry.Add(mem_registry.GetObject());
    return true;
}

string SNetScheduleAPIImpl::MakeAuthString()
{
    string auth(m_Service->MakeAuthString());

    const CVersionAPI* version = nullptr;
    string name;

    {{
        if (CNcbiApplicationGuard app = CNcbiApplication::InstanceGuard()) {
            version = &app->GetFullVersion();
            name = app->GetProgramDisplayName();
        }
    }}

    if (version && m_ProgramVersion.empty()) {
        m_ProgramVersion += name;
        auto package_name = version->GetPackageName();

        if (!package_name.empty()) {
            m_ProgramVersion += ": ";
            m_ProgramVersion += package_name;
            m_ProgramVersion += ' ';
            m_ProgramVersion += version->GetPackageVersion().Print();
            m_ProgramVersion += " built on ";
            m_ProgramVersion += version->GetBuildInfo().date;
        }
    }

    if (!m_ProgramVersion.empty()) {
        auth += " prog=\"";
        auth += m_ProgramVersion;
        auth += '\"';
    }

    switch (m_ClientType) {
    case CNetScheduleAPI::eCT_Admin:
        auth += " client_type=\"admin\"";
        break;

    case CNetScheduleAPI::eCT_Submitter:
        auth += " client_type=\"submitter\"";
        break;

    case CNetScheduleAPI::eCT_WorkerNode:
        auth += " client_type=\"worker node\"";
        break;

    case CNetScheduleAPI::eCT_Reader:
        auth += " client_type=\"reader\"";

    default: /* eCT_Auto */
        break;
    }

    if (!m_ClientNode.empty()) {
        auth += " client_node=\"";
        auth += m_ClientNode;
        auth += '\"';
    }

    if (!m_ClientSession.empty()) {
        auth += " client_session=\"";
        auth += m_ClientSession;
        auth += '\"';
    }

    if (version) {
        auth += " client_version=\"";
        auth += version->GetVersionInfo().Print();
        auth += '\"';
    }

    ITERATE(SNetScheduleAPIImpl::TAuthParams, it, m_AuthParams) {
        auth += it->second;
    }

    auth += " ns_compat_ver=\"" COMPATIBLE_NETSCHEDULE_VERSION "\""
        "\r\n";

    auth += m_Queue;

    // Make the auth token look like a command to be able to
    // check for potential authentication/initialization errors
    // like the "queue not found" error.
    if (m_Mode & fNonWnCompatible) {
        auth += "\r\nVERSION";
    }

    return auth;
}

INetServerConnectionListener::TPropCreator CNetScheduleServerListener::GetPropCreator() const
{
    return [] { return new SNetScheduleServerProperties; };
}

INetServerConnectionListener* CNetScheduleServerListener::Clone()
{
    return new CNetScheduleServerListener(*this);
}

void SNetScheduleAPIImpl::Init(CSynRegistry& registry, SRegSynonyms& sections)
{
    SetDiagUserAndHost();

    m_RetryOnException = registry.Get(sections, "enforce_retry_policy", false);

    if (!m_Queue.empty()) limits::Check<limits::SQueueName>(m_Queue);

    const string& user(GetDiagContext().GetUsername());
    m_ClientNode =
        m_Service->GetClientName() + "::" +
        (user.empty() ? kEmptyStr : user + '@') +
        GetDiagContext().GetHost();

    CNetScheduleConfigLoader loader(registry, sections);

    bool affinities_initialized = false;

    // There are two phases of Init in case we need to load config from server
    // 1) Setup as much as possible and try to get config from server
    // 2) Setup everything using received config from server
    do {
        if (m_Queue.empty()) {
            m_Queue = registry.Get(sections, "queue_name", "");
            if (!m_Queue.empty()) limits::Check<limits::SQueueName>(m_Queue);
        }

        m_UseEmbeddedStorage =   registry.Get(sections, { "use_embedded_storage", "use_embedded_input" }, true);
        m_JobGroup =             registry.Get(sections, "job_group", "");
        m_JobTtl =               registry.Get(sections, "job_ttl", 0);
        m_ClientNode =           registry.Get(sections, "client_node", m_ClientNode);
        GetListener()->Scope() = registry.Get(sections, "scope", "");

        if (!affinities_initialized && registry.Get(sections, "use_affinities", false)) {
            affinities_initialized = true;
            InitAffinities(registry, sections);
        }

        if (!m_ClientNode.empty()) {
            m_ClientSession =
                NStr::NumericToString(CDiagContext::GetPID()) + '@' +
                NStr::NumericToString(GetFastLocalTime().GetTimeT()) + ':' +
                GetDiagContext().GetStringUID();
        }

        GetListener()->SetAuthString(MakeAuthString());

        // If not working in WN compatible mode
        if (!(m_Mode & fConfigLoading)) break;
    } while (loader(this));
}

void CNetScheduleServerListener::OnConnected(CNetServerConnection& connection)
{
    if (m_NonWn) {
        string version_info(connection.Exec(m_Auth, false));

        CNetServerInfo server_info(new SNetServerInfoImpl(version_info));

        string attr_name, attr_value;
        string ns_node, ns_session;
        CVersionInfo version;

        while (server_info.GetNextAttribute(attr_name, attr_value))
            if (attr_name == "ns_node")
                ns_node = attr_value;
            else if (attr_name == "ns_session")
                ns_session = attr_value;
            else if (attr_name == "server_version")
                version = CVersionInfo(attr_value);

        // Usually, all attributes come together, so no need to check version
        if (!ns_node.empty() && !ns_session.empty()) {
            auto server_props = connection->m_Server->Get<SNetScheduleServerProperties>();

            // Version cannot change without session, so no need to compare, too
            if (server_props->ns_node != ns_node ||
                    server_props->ns_session != ns_session) {
                CFastMutexGuard guard(m_SharedData->m_ServerByNodeMutex);
                server_props->ns_node = ns_node;
                server_props->ns_session = ns_session;
                server_props->version = version;
                m_SharedData->m_ServerByNode[ns_node] = connection->m_Server->m_ServerInPool;
                server_props->affs_synced = false;
            }
        }

        if (!m_Scope.empty()) {
            string cmd("SETSCOPE " + m_Scope);
            g_AppendClientIPSessionIDHitID(cmd);
            connection.Exec(cmd, false);
        }
    } else
        connection->WriteLine(m_Auth);
}

void CNetScheduleServerListener::OnErrorImpl(
    const string& err_msg, CNetServer& server)
{
    string code;
    string msg;

    if (!NStr::SplitInTwo(err_msg, ":", code, msg)) {
        if (err_msg == "Job not found") {
            NCBI_THROW(CNetScheduleException, eJobNotFound, err_msg);
        }
        code = err_msg;
    }

    // Map code into numeric value
    CException::TErrCode err_code = CNetScheduleExceptionMap::GetCode(code);

    switch (err_code) {
    case CException::eInvalid:
        NCBI_THROW(CNetServiceException, eCommunicationError, err_msg);

    case CNetScheduleException::eGroupNotFound:
    case CNetScheduleException::eAffinityNotFound:
    case CNetScheduleException::eDuplicateName:
        // Convert these errors into warnings.
        OnWarning(msg, server);
        break;

    case CNetScheduleException::eJobNotFound:
        NCBI_THROW(CNetScheduleException, eJobNotFound, "Job not found");

    default:
        NCBI_THROW(CNetScheduleException, EErrCode(err_code), !msg.empty() ?
                msg : CNetScheduleException::GetErrCodeDescription(err_code));
    }
}

void CNetScheduleServerListener::OnWarningImpl(const string& warn_msg,
        CNetServer& server)
{
        ERR_POST(Warning << server->m_ServerInPool->m_Address.AsString() <<
                ": " << warn_msg);
}

const char* const kNetScheduleAPIDriverName = "netschedule_api";

SNetScheduleAPIImpl::SNetScheduleAPIImpl(CSynRegistryBuilder registry_builder, const string& section,
        const string& service_name, const string& client_name,
        const string& queue_name, bool wn, bool try_config) :
    m_Mode(GetMode(wn, try_config)),
    m_SharedData(new SNetScheduleSharedData),
    m_Queue(queue_name)
{
    SRegSynonyms sections{ section, kNetScheduleAPIDriverName };
    m_Service = SNetServiceImpl::Create("NetScheduleAPI", service_name, client_name,
            new CNetScheduleServerListener(m_Mode & fNonWnCompatible, m_SharedData),
            registry_builder, sections);
    Init(registry_builder, sections);
}

SNetScheduleAPIImpl::SNetScheduleAPIImpl(
        SNetServerInPool* server, SNetScheduleAPIImpl* parent) :
    m_Mode(parent->m_Mode),
    m_SharedData(parent->m_SharedData),
    m_RetryOnException(parent->m_RetryOnException),
    m_Service(SNetServiceImpl::Clone(server, parent->m_Service)),
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
    m_Impl(new SNetScheduleAPIImpl(nullptr, conf_section))
{
}

CNetScheduleAPI::CNetScheduleAPI(const IRegistry& reg,
        const string& conf_section) :
    m_Impl(new SNetScheduleAPIImpl(reg, conf_section))
{
}

CNetScheduleAPI::CNetScheduleAPI(CConfig* conf, const string& conf_section) :
    m_Impl(new SNetScheduleAPIImpl(conf, conf_section))
{
}

CNetScheduleAPI::CNetScheduleAPI(const string& service_name,
        const string& client_name, const string& queue_name) :
    m_Impl(new SNetScheduleAPIImpl(nullptr, kEmptyStr, service_name, client_name, queue_name))
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

#define EXTRACT_WARNING_TYPE(warning_type) \
    if (NStr::StartsWith(warn_msg, "e" #warning_type ":")) { \
        warn_msg.erase(0, sizeof("e" #warning_type ":") - 1); \
        return eWarn##warning_type; \
    }

CNetScheduleAPI::ENetScheduleWarningType
        CNetScheduleAPI::ExtractWarningType(string& warn_msg)
{
    EXTRACT_WARNING_TYPE(AffinityNotFound);
    EXTRACT_WARNING_TYPE(AffinityNotPreferred);
    EXTRACT_WARNING_TYPE(AffinityAlreadyPreferred);
    EXTRACT_WARNING_TYPE(GroupNotFound);
    EXTRACT_WARNING_TYPE(JobNotFound);
    EXTRACT_WARNING_TYPE(JobAlreadyCanceled);
    EXTRACT_WARNING_TYPE(JobAlreadyDone);
    EXTRACT_WARNING_TYPE(JobAlreadyFailed);
    EXTRACT_WARNING_TYPE(JobPassportOnlyMatch);
    EXTRACT_WARNING_TYPE(NoParametersChanged);
    EXTRACT_WARNING_TYPE(ConfigFileNotChanged);
    EXTRACT_WARNING_TYPE(AlertNotFound);
    EXTRACT_WARNING_TYPE(AlertAlreadyAcknowledged);
    EXTRACT_WARNING_TYPE(SubmitsDisabledForServer);
    EXTRACT_WARNING_TYPE(QueueAlreadyPaused);
    EXTRACT_WARNING_TYPE(QueueNotPaused);
    EXTRACT_WARNING_TYPE(CommandObsolete);
    EXTRACT_WARNING_TYPE(JobNotRead);
    return eWarnUnknown;
}

#define WARNING_TYPE_TO_STRING(warning_type) \
    case eWarn##warning_type: \
        return #warning_type;

const char* CNetScheduleAPI::WarningTypeToString(
        CNetScheduleAPI::ENetScheduleWarningType warning_type)
{
    switch (warning_type) {
    WARNING_TYPE_TO_STRING(AffinityNotFound);
    WARNING_TYPE_TO_STRING(AffinityNotPreferred);
    WARNING_TYPE_TO_STRING(AffinityAlreadyPreferred);
    WARNING_TYPE_TO_STRING(GroupNotFound);
    WARNING_TYPE_TO_STRING(JobNotFound);
    WARNING_TYPE_TO_STRING(JobAlreadyCanceled);
    WARNING_TYPE_TO_STRING(JobAlreadyDone);
    WARNING_TYPE_TO_STRING(JobAlreadyFailed);
    WARNING_TYPE_TO_STRING(JobPassportOnlyMatch);
    WARNING_TYPE_TO_STRING(NoParametersChanged);
    WARNING_TYPE_TO_STRING(ConfigFileNotChanged);
    WARNING_TYPE_TO_STRING(AlertNotFound);
    WARNING_TYPE_TO_STRING(AlertAlreadyAcknowledged);
    WARNING_TYPE_TO_STRING(SubmitsDisabledForServer);
    WARNING_TYPE_TO_STRING(QueueAlreadyPaused);
    WARNING_TYPE_TO_STRING(QueueNotPaused);
    WARNING_TYPE_TO_STRING(CommandObsolete);
    WARNING_TYPE_TO_STRING(JobNotRead);
    default:
        return "eWarnUnknown";
    }
}

CNetScheduleSubmitter CNetScheduleAPI::GetSubmitter()
{
    return new SNetScheduleSubmitterImpl(m_Impl);
}

CNetScheduleExecutor CNetScheduleAPI::GetExecutor()
{
    return new SNetScheduleExecutorImpl(m_Impl);
}

CNetScheduleJobReader CNetScheduleAPI::GetJobReader(const string& group,
        const string& affinity)
{
    m_Impl->AllocNotificationThread();
    return new SNetScheduleJobReaderImpl(m_Impl, group, affinity);
}

CNetScheduleAdmin CNetScheduleAPI::GetAdmin()
{
    return new SNetScheduleAdminImpl(m_Impl);
}

CNetService CNetScheduleAPI::GetService()
{
    return m_Impl->m_Service;
}

static void s_SetJobExpTime(time_t* job_exptime, const string& time_str)
{
    if (job_exptime != NULL)
        *job_exptime = (time_t) NStr::StringToUInt8(time_str,
                NStr::fConvErr_NoThrow);
}

static void s_SetPauseMode(ENetScheduleQueuePauseMode* pause_mode,
        const string& mode_str)
{
    if (pause_mode != NULL)
        *pause_mode = mode_str.empty() ? eNSQ_NoPause :
                mode_str == "pullback" ? eNSQ_WithPullback :
                        eNSQ_WithoutPullback;
}

CNetScheduleAPI::EJobStatus CNetScheduleAPI::GetJobDetails(
        CNetScheduleJob& job,
        time_t* job_exptime,
        ENetScheduleQueuePauseMode* pause_mode)
{
    string cmd("STATUS2 " + job.job_id);
    g_AppendClientIPSessionIDHitID(cmd);
    cmd += " need_progress_msg=1";
    auto response = m_Impl->ExecOnJobServer(job, cmd);

    SNetScheduleOutputParser parser(response);

    const auto status = StringToStatus(parser("job_status"));

    s_SetJobExpTime(job_exptime, parser("job_exptime"));
    s_SetPauseMode(pause_mode, parser("pause"));

    switch (status) {
    case ePending:
    case eRunning:
    case eCanceled:
    case eFailed:
    case eDone:
    case eReading:
    case eConfirmed:
    case eReadFailed:
        job.input = parser("input");
        job.output = parser("output");
        job.ret_code = NStr::StringToInt(parser("ret_code"), NStr::fConvErr_NoThrow);
        job.error_msg = parser("err_msg");
        break;

    default:
        job.input.erase();
        job.ret_code = 0;
        job.output.erase();
        job.error_msg.erase();
    }

    job.affinity.erase();
    job.mask = CNetScheduleAPI::eEmptyMask;
    job.progress_msg = parser("msg");

    return status;
}

CNetScheduleAPI::EJobStatus SNetScheduleAPIImpl::GetJobStatus(string cmd,
        const CNetScheduleJob& job, time_t* job_exptime,
        ENetScheduleQueuePauseMode* pause_mode)
{
    string response;

    try {
        cmd += ' ';
        cmd += job.job_id;
        g_AppendClientIPSessionIDHitID(cmd);
        response = ExecOnJobServer(job, cmd);
    }
    catch (CNetScheduleException& e) {
        if (e.GetErrCode() != CNetScheduleException::eJobNotFound)
            throw;

        if (job_exptime != NULL)
            *job_exptime = 0;

        return CNetScheduleAPI::eJobNotFound;
    }

    SNetScheduleOutputParser parser(response);

    s_SetJobExpTime(job_exptime, parser("job_exptime"));
    s_SetPauseMode(pause_mode, parser("pause"));

    return CNetScheduleAPI::StringToStatus(parser("job_status"));
}

bool SNetScheduleAPIImpl::GetServerByNode(const string& ns_node,
        CNetServer* server)
{
    SNetServerInPool* known_server; /* NCBI_FAKE_WARNING */

    {{
        CFastMutexGuard guard(m_SharedData->m_ServerByNodeMutex);

        auto server_props_it = m_SharedData->m_ServerByNode.find(ns_node);

        if (server_props_it == m_SharedData->m_ServerByNode.end())
            return false;

        known_server = server_props_it->second;
    }}

    *server = new SNetServerImpl(m_Service,
            m_Service->m_ServerPool->ReturnServer(known_server));

    return true;
}

const CNetScheduleAPI::SServerParams&
SNetScheduleAPIImpl::SServerParamsSync::operator()(CNetService& service, const string& queue)
{
    CFastMutexGuard g(m_FastMutex);

    if (m_AskCount-- > 0) return m_ServerParams;

    m_AskCount = kAskMaxCount;

    m_ServerParams.max_input_size = kNetScheduleMaxDBDataSize;
    m_ServerParams.max_output_size = kNetScheduleMaxDBDataSize;

    string cmd("QINF2 " + queue);
    g_AppendClientIPSessionIDHitID(cmd);

    CUrlArgs url_parser(service.FindServerAndExec(cmd, false).response);

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
                m_ServerParams.max_input_size =
                        NStr::StringToInt(field->value);
            } else if (field->name == "max_output_size") {
                field_bits |= (1 << eMaxOutputSize);
                m_ServerParams.max_output_size =
                        NStr::StringToInt(field->value);
            }
        }
        if (field_bits == (1 << eNumberOfSizeParams) - 1)
            break;
    }

    return m_ServerParams;
}

const CNetScheduleAPI::SServerParams& CNetScheduleAPI::GetServerParams()
{
    return m_Impl->GetServerParams();
}

void SNetScheduleAPIImpl::GetQueueParams(
        const string& queue_name, TQueueParams& queue_params)
{
    string cmd("QINF2 ");

    if (!queue_name.empty()) {
        limits::Check<limits::SQueueName>(queue_name);

        cmd += queue_name;
    } else if (!m_Queue.empty()) {
        cmd += m_Queue;
    } else {
        cmd += "service=" + m_Service->m_ServiceName;
    }

    g_AppendClientIPSessionIDHitID(cmd);

    CUrlArgs url_parser(m_Service.FindServerAndExec(cmd, false).response);

    ITERATE(CUrlArgs::TArgs, field, url_parser.GetArgs()) {
        queue_params[field->name] = field->value;
    }
}

void CNetScheduleAPI::GetQueueParams(
        const string& queue_name, TQueueParams& queue_params)
{
    return m_Impl->GetQueueParams(queue_name, queue_params);
}

void SNetScheduleAPIImpl::GetQueueParams(TQueueParams& queue_params)
{
    string cmd("GETP2");
    g_AppendClientIPSessionIDHitID(cmd);

    CUrlArgs url_parser(m_Service.FindServerAndExec(cmd,
            false).response);

    ITERATE(CUrlArgs::TArgs, field, url_parser.GetArgs()) {
        queue_params[field->name] = field->value;
    }
}

void CNetScheduleAPI::GetQueueParams(TQueueParams& queue_params)
{
    return m_Impl->GetQueueParams(queue_params);
}

void CNetScheduleAPI::GetProgressMsg(CNetScheduleJob& job)
{
    string cmd("MGET " + job.job_id);
    g_AppendClientIPSessionIDHitID(cmd);
    auto response = m_Impl->ExecOnJobServer(job, cmd);
    job.progress_msg = NStr::ParseEscapes(response);
}

void CNetScheduleAPI::SetClientNode(const string& client_node)
{
    // Cannot add this to limits::SClientNode due to CNetScheduleAPIExt allowing reset to empty values
    if (client_node.empty()) {
        NCBI_THROW_FMT(CConfigException, eParameterMissing,
                "'" << limits::SClientNode::Name() << "' cannot be empty");
    }

    limits::Check<limits::SClientNode>(client_node);

    m_Impl->m_ClientNode = client_node;

    m_Impl->UpdateAuthString();
}

void CNetScheduleAPI::SetClientSession(const string& client_session)
{
    // Cannot add this to limits::SClientSession due to CNetScheduleAPIExt allowing reset to empty values
    if (client_session.empty()) {
        NCBI_THROW_FMT(CConfigException, eParameterMissing,
                "'" << limits::SClientSession::Name() << "' cannot be empty");
    }

    limits::Check<limits::SClientSession>(client_session);

    m_Impl->m_ClientSession = client_session;

    m_Impl->UpdateAuthString();
}

void SNetScheduleAPIImpl::UpdateAuthString()
{
    m_Service->m_ServerPool->ResetServerConnections();

    GetListener()->SetAuthString(MakeAuthString());
}

void CNetScheduleAPI::SetClientType(CNetScheduleAPI::EClientType client_type)
{
    m_Impl->m_ClientType = client_type;

    m_Impl->UpdateAuthString();
}

void SNetScheduleAPIImpl::UseOldStyleAuth()
{
    m_Service->m_ServerPool->m_UseOldStyleAuth = true;

    UpdateAuthString();
}

void SNetScheduleAPIImpl::SetAuthParam(const string& param_name,
        const string& param_value)
{
    if (!param_value.empty()) {
        string auth_param(' ' + param_name);
        auth_param += "=\"";
        auth_param += NStr::PrintableString(param_value);
        auth_param += '"';
        m_AuthParams[param_name] = auth_param;
    } else
        m_AuthParams.erase(param_name);

    UpdateAuthString();
}

void SNetScheduleAPIImpl::InitAffinities(CSynRegistry& registry, const SRegSynonyms& sections)
{
    const bool claim_new_affinities = registry.Get(sections, "claim_new_affinities", false);
    const bool process_any_job =      registry.Get(sections, "process_any_job", false);
    const string affinity_list =      registry.Get(sections, "affinity_list", kEmptyStr);
    const string affinity_ladder =    registry.Get(sections, "affinity_ladder", kEmptyStr);

    if (affinity_ladder.empty()) {

        if (claim_new_affinities) {
            m_AffinityPreference = CNetScheduleExecutor::eClaimNewPreferredAffs;

        } else if (process_any_job) {
            m_AffinityPreference = CNetScheduleExecutor::ePreferredAffsOrAnyJob;

        } else {
            m_AffinityPreference = CNetScheduleExecutor::ePreferredAffinities;
        }

        if (affinity_list.empty()) return;

        NStr::Split(affinity_list, ", ", m_AffinityList,
                NStr::fSplit_MergeDelimiters | NStr::fSplit_Truncate);

        for (auto& affinity : m_AffinityList) {
            limits::Check<limits::SAffinity>(affinity);
        }

        return;
    }

    // Sanity checks
    if (claim_new_affinities) {
        NCBI_THROW(CConfigException, eInvalidParameter,
                "'affinity_ladder' cannot be used with 'claim_new_affinities'");
    }
    if (!affinity_list.empty()) {
        NCBI_THROW(CConfigException, eInvalidParameter,
                "'affinity_ladder' cannot be used with 'affinity_list'");
    }

    if (!process_any_job) {
        m_AffinityPreference = CNetScheduleExecutor::eExplicitAffinitiesOnly;
    }

    list<CTempString> affinities;
    NStr::Split(affinity_ladder, ", ", affinities,
            NStr::fSplit_MergeDelimiters | NStr::fSplit_Truncate);

    if (affinities.empty()) return;

    string affinity_step;

    for (auto& affinity : affinities) {
        limits::Check<limits::SAffinity>(affinity);

        if (!affinity_step.empty()) affinity_step += ',';
        affinity_step += affinity;
        m_AffinityLadder.emplace_back(affinity, affinity_step);
    }
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
            return new SNetScheduleAPIImpl(&config, m_DriverName);
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


void CNetScheduleAPIExt::AddToClientNode(const string& data)
{
    string& client_node(m_Impl->m_ClientNode);
    client_node += ':';
    client_node += data;
    UpdateAuthString();
}

void CNetScheduleAPIExt::UpdateAuthString()
{
    m_Impl->UpdateAuthString();
}

void CNetScheduleAPIExt::UseOldStyleAuth()
{
    m_Impl->UseOldStyleAuth();
}

CCompoundIDPool CNetScheduleAPIExt::GetCompoundIDPool()
{
    return m_Impl->m_CompoundIDPool;
}

CNetScheduleAPI CNetScheduleAPIExt::GetServer(CNetServer::TInstance server)
{
    return new SNetScheduleAPIImpl(server->m_ServerInPool, m_Impl);
}

void CNetScheduleAPIExt::ReSetClientNode(const string& client_node)
{
    limits::Check<limits::SClientNode>(client_node);
    m_Impl->m_ClientNode = client_node;
    m_Impl->UpdateAuthString();
}

void CNetScheduleAPIExt::ReSetClientSession(const string& client_session)
{
    limits::Check<limits::SClientSession>(client_session);
    m_Impl->m_ClientSession = client_session;
    m_Impl->UpdateAuthString();
}

CNetScheduleAPI::TInstance
CNetScheduleAPIExt::CreateWnCompat(const string& service_name,
        const string& client_name)
{
    return new SNetScheduleAPIImpl(nullptr, kEmptyStr, service_name, client_name, kEmptyStr, true, false);
}

CNetScheduleAPI::TInstance
CNetScheduleAPIExt::CreateNoCfgLoad(const string& service_name,
        const string& client_name, const string& queue_name)
{
    return new SNetScheduleAPIImpl(nullptr, kEmptyStr, service_name, client_name, queue_name, false, false);
}


END_NCBI_SCOPE
