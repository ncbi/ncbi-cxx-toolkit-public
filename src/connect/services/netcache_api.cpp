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
 * Authors:  Anatoliy Kuznetsov, Maxim Didenko, Dmitry Kazimirov
 *
 * File Description:
 *   Implementation of the NetCache client API.
 *
 */

#include <ncbi_pch.hpp>

#ifdef _MSC_VER
// Disable warning C4191: 'type cast' :
//     unsafe conversion from 'ncbi::CDll::FEntryPoint'
//     to 'void (__cdecl *)(...)' [comes from plugin_manager.hpp]
#pragma warning (disable: 4191)
#endif

#include "netcache_api_impl.hpp"

#include <connect/services/srv_connections_expt.hpp>
#include <connect/services/error_codes.hpp>

#include <connect/ncbi_conn_exception.hpp>

#include <corelib/ncbitime.hpp>
#include <corelib/ncbi_system.hpp>
#include <corelib/request_ctx.hpp>
#include <corelib/plugin_manager_impl.hpp>
#include <corelib/ncbi_safe_static.hpp>
#include <corelib/rwstream.hpp>

#include <memory>


#define NCBI_USE_ERRCODE_X  ConnServ_NetCache


BEGIN_NCBI_SCOPE

static bool s_FallbackServer_Initialized = false;
static CSafeStatic<auto_ptr<SServerAddress> > s_FallbackServer;

static SServerAddress* s_GetFallbackServer()
{
    if (s_FallbackServer_Initialized)
        return s_FallbackServer->get();
    try {
        string host, port;
        if (NStr::SplitInTwo(TCGI_NetCacheFallbackServer::GetDefault(),
                ":", host, port)) {
            s_FallbackServer->reset(new SServerAddress(
                    g_NetService_gethostbyname(host),
                            (unsigned short) NStr::StringToInt(port)));
        }
    } catch (...) {
    }
    s_FallbackServer_Initialized = true;
    return s_FallbackServer->get();
}

CRef<INetServerProperties> CNetCacheServerListener::AllocServerProperties()
{
    return CRef<INetServerProperties>(new SNetCacheServerProperties);
}

CConfig* CNetCacheServerListener::LoadConfigFromAltSource(CObject* api_impl,
        string* new_section_name)
{
    SNetCacheAPIImpl* nc_impl = static_cast<SNetCacheAPIImpl*>(api_impl);

    auto_ptr<CConfig::TParamTree> result;

    if (nc_impl->m_NetScheduleAPI) {
        CNetScheduleAPI::TQueueParams queue_params;

        nc_impl->m_NetScheduleAPI.GetQueueParams(
                nc_impl->m_NetScheduleAPI.GetQueueName(), queue_params);

        ITERATE(CNetScheduleAPI::TQueueParams, param_it, queue_params) {
            if (NStr::StartsWith(param_it->first, "nc.")) {
                string param_name(param_it->first);
                param_name.erase(0, sizeof("nc.") - 1);
                if (result.get() == NULL) {
                    result.reset(new CConfig::TParamTree);
                    *new_section_name = "netcache_conf_from_netschedule";
                }
                result->AddNode(CConfig::TParamValue(param_name, param_it->second));
            }
        }

        if (result.get() == NULL)
            try {
                nc_impl->m_NetScheduleAPI.GetQueueParams(queue_params);

                ITERATE(CNetScheduleAPI::TQueueParams, param_it, queue_params) {
                    if (NStr::StartsWith(param_it->first, "nc::")) {
                        string param_name(param_it->first);
                        param_name.erase(0, sizeof("nc::") - 1);
                        if (result.get() == NULL) {
                            result.reset(new CConfig::TParamTree);
                            *new_section_name =
                                    "netcache_conf_from_netschedule_GETP2";
                        }
                        result->AddNode(CConfig::TParamValue(param_name,
                                param_it->second));
                    }
                }
            }
            catch (CNetScheduleException&) {
            }
    }

    if (result.get() == NULL)
        return NULL;

    CConfig* config = new CConfig(result.get());

    result.release();

    return config;
}

void CNetCacheServerListener::OnInit(CObject* api_impl,
    CConfig* config, const string& config_section)
{
    SNetCacheAPIImpl* nc_impl = static_cast<SNetCacheAPIImpl*>(api_impl);

    m_Auth = nc_impl->m_Service->MakeAuthString();

    if (nc_impl->m_Service->m_ServerPool->m_ClientName.length() < 3) {
        NCBI_THROW(CNetCacheException,
            eAuthenticationError, "Client name is too short or empty");
    }

    const string default_temp_dir(".");

    if (config != NULL) {
        string temp_dir = config->GetString(config_section,
            "tmp_dir", CConfig::eErr_NoThrow, kEmptyStr);

        if (temp_dir.empty())
            temp_dir = config->GetString(config_section,
                "tmp_path", CConfig::eErr_NoThrow, default_temp_dir);

        nc_impl->m_TempDir = temp_dir.empty() ? default_temp_dir : temp_dir;

        nc_impl->m_CacheInput = config->GetBool(config_section,
            "cache_input", CConfig::eErr_NoThrow, false);

        nc_impl->m_CacheOutput = config->GetBool(config_section,
            "cache_output", CConfig::eErr_NoThrow, false);

        nc_impl->m_DefaultParameters.SetMirroringMode(
            config->GetString(config_section, "enable_mirroring",
                CConfig::eErr_NoThrow, kEmptyStr));

        nc_impl->m_DefaultParameters.SetServerCheck(
            config->GetString(config_section,
                "server_check", CConfig::eErr_NoThrow, kEmptyStr));

        nc_impl->m_DefaultParameters.SetServerCheckHint(
            config->GetString(config_section,
                "server_check_hint", CConfig::eErr_NoThrow, kEmptyStr));

        if (config->GetBool(config_section,
                "use_compound_id", CConfig::eErr_NoThrow, false))
            nc_impl->m_DefaultParameters.SetUseCompoundID(true);
    } else {
        nc_impl->m_TempDir = default_temp_dir;
        nc_impl->m_CacheInput = false;
        nc_impl->m_CacheOutput = false;
    }
}

void CNetCacheServerListener::OnConnected(
        CNetServerConnection::TInstance conn_impl)
{
    CRef<SNetCacheServerProperties> server_props(
            x_GetServerProperties(conn_impl->m_Server));

    CFastMutexGuard guard(server_props->m_Mutex);

    if (server_props->mirroring_checked) {
        guard.Release();
        conn_impl->WriteLine(m_Auth);
    } else {
        CNetServerConnection conn(conn_impl);
        try {
            string version_info(conn.Exec(m_Auth + "\r\nVERSION"));

            server_props->mirroring_checked = true;

            CUrlArgs url_parser(version_info);

            ITERATE(CUrlArgs::TArgs, field, url_parser.GetArgs()) {
                if (field->name == "mirrored" && field->value == "true")
                    server_props->mirrored = true;
            }
        }
        catch (CException&) {
        }
    }
}

void CNetCacheServerListener::OnError(
    const string& err_msg, SNetServerImpl* server)
{
    string message = server->m_ServerInPool->m_Address.AsString();

    message += ": ";
    message += err_msg;

    static const char s_BlobNotFoundMsg[] = "BLOB not found";
    if (NStr::strncmp(err_msg.c_str(), s_BlobNotFoundMsg,
            sizeof(s_BlobNotFoundMsg) - 1) == 0) {
        if (strstr(err_msg.c_str(), "AGE=") != NULL) {
            NCBI_THROW(CNetCacheBlobTooOldException, eBlobTooOld, message);
        } else {
            NCBI_THROW(CNetCacheException, eBlobNotFound, message);
        }
    }

    static const char s_AccessDenied[] = "Access denied";
    if (NStr::strncmp(err_msg.c_str(), s_AccessDenied,
        sizeof(s_AccessDenied) - 1) == 0)
        NCBI_THROW(CNetCacheException, eAccessDenied, message);

    static const char s_UnknownCommandMsg[] = "Unknown command";
    if (NStr::strncmp(err_msg.c_str(), s_UnknownCommandMsg,
        sizeof(s_UnknownCommandMsg) - 1) == 0)
        NCBI_THROW(CNetCacheException, eUnknownCommand, message);

    NCBI_THROW(CNetCacheException, eServerError, message);
}

void CNetCacheServerListener::OnWarning(const string& warn_msg,
        SNetServerImpl* server)
{
    if (m_EventHandler)
        m_EventHandler->OnWarning(warn_msg, server);
    else {
        LOG_POST(Warning << server->m_ServerInPool->m_Address.AsString() <<
                ": " << warn_msg);
    }
}

CRef<SNetCacheServerProperties> CNetCacheServerListener::x_GetServerProperties(
        SNetServerImpl* server_impl)
{
    return CRef<SNetCacheServerProperties>(
            static_cast<SNetCacheServerProperties*>(
                    server_impl->m_ServerInPool->
                            m_ServerProperties.GetPointerOrNull()));
}

const char* const kNetCacheAPIDriverName = "netcache_api";

static const char* const s_NetCacheConfigSections[] = {
    kNetCacheAPIDriverName,
    "netcache_client",
    "netcache",
    NULL
};

SNetCacheAPIImpl::SNetCacheAPIImpl(CConfig* config, const string& section,
        const string& service, const string& client_name,
        CNetScheduleAPI::TInstance ns_api) :
    m_Service(new SNetServiceImpl("NetCacheAPI", client_name,
        new CNetCacheServerListener)),
    m_NetScheduleAPI(ns_api),
    m_DefaultParameters(eVoid)
{
    m_Service->Init(this, service, config, section, s_NetCacheConfigSections);
}

SNetCacheAPIImpl::SNetCacheAPIImpl(SNetServerInPool* server,
        SNetCacheAPIImpl* parent) :
    m_Service(new SNetServiceImpl(server, parent->m_Service)),
    m_ServicesFromKeys(parent->m_ServicesFromKeys),
    m_TempDir(parent->m_TempDir),
    m_CacheInput(parent->m_CacheInput),
    m_CacheOutput(parent->m_CacheOutput),
    m_NetScheduleAPI(parent->m_NetScheduleAPI),
    m_DefaultParameters(parent->m_DefaultParameters)
{
}

void SNetCacheAPIImpl::AppendClientIPSessionID(string* cmd)
{
    CRequestContext& req = CDiagContext::GetRequestContext();
    cmd->append(" \"");
    cmd->append(req.GetClientIP());
    cmd->append("\" \"");
    cmd->append(req.GetSessionID());
    cmd->append("\"");
}

void SNetCacheAPIImpl::AppendClientIPSessionIDPassword(string* cmd,
        const CNetCacheAPIParameters* parameters)
{
    AppendClientIPSessionID(cmd);

    string password(parameters->GetPassword());
    if (!password.empty())
        cmd->append(password);
}

string SNetCacheAPIImpl::MakeCmd(const char* cmd)
{
    string result(cmd);
    AppendClientIPSessionID(&result);
    return result;
}

string SNetCacheAPIImpl::MakeCmd(const char* cmd,
        const CNetCacheAPIParameters* parameters)
{
    string result(cmd);
    AppendClientIPSessionIDPassword(&result, parameters);
    return result;
}

string SNetCacheAPIImpl::MakeCmd(const char* cmd_base, const CNetCacheKey& key,
        const CNetCacheAPIParameters* parameters)
{
    string result(cmd_base + key.StripKeyExtensions());
    AppendClientIPSessionIDPassword(&result, parameters);
    return result;
}

unsigned SNetCacheAPIImpl::x_ExtractBlobAge(
        const CNetServer::SExecResult& exec_result, const char* cmd_name)
{
    string::size_type pos = exec_result.response.find("AGE=");

    if (pos == string::npos) {
        NCBI_THROW_FMT(CNetCacheException, eInvalidServerResponse,
                "No AGE field in " << cmd_name << " output");
    }

    return NStr::StringToUInt(exec_result.response.c_str() + pos +
            sizeof("AGE=") - 1, NStr::fAllowTrailingSymbols);
}

class SNetCacheMirrorTraversal : public IServiceTraversal
{
public:
    SNetCacheMirrorTraversal(CNetService::TInstance service,
            CNetServer::TInstance primary_server, ESwitch server_check) :
        m_Service(service),
        m_PrimaryServer(primary_server),
        m_PrimaryServerCheck(server_check != eOff)
    {
    }

    virtual CNetServer BeginIteration();
    virtual CNetServer NextServer();

    CNetService m_Service;
    CNetServiceIterator m_Iterator;
    CNetServer::TInstance m_PrimaryServer;
    bool m_PrimaryServerCheck;
};

CNetServer SNetCacheMirrorTraversal::BeginIteration()
{
    if (m_PrimaryServerCheck)
        return *(m_Iterator = m_Service.Iterate(m_PrimaryServer));

    return m_PrimaryServer;
}

CNetServer SNetCacheMirrorTraversal::NextServer()
{
    if (m_PrimaryServerCheck)
        return ++m_Iterator ? *m_Iterator : CNetServer();

    m_PrimaryServerCheck = true;
    return *(m_Iterator = m_Service.Iterate(m_PrimaryServer));
}

CNetService SNetCacheAPIImpl::FindOrCreateService(const CNetCacheKey& key)
{
    const string& service_name(key.GetServiceName());

    if (service_name == m_Service.GetServiceName())
        return m_Service;

    pair<TNetServiceByName::iterator, bool> loc(m_ServicesFromKeys.insert(
            TNetServiceByName::value_type(service_name, CNetService())));

    if (!loc.second)
        return loc.first->second;

    CNetService new_service(new SNetServiceImpl(service_name, m_Service));

    loc.first->second = new_service;

    return new_service;
}

CNetServer::SExecResult SNetCacheAPIImpl::ExecMirrorAware(
    const CNetCacheKey& key, const string& cmd,
    const CNetCacheAPIParameters* parameters,
    SNetServiceImpl::EServerErrorHandling error_handling)
{
    CNetServer primary_server(GetServer(key));

    bool service_name_is_defined = !key.GetServiceName().empty();

    bool key_is_mirrored = service_name_is_defined &&
            !key.GetFlag(CNetCacheKey::fNCKey_SingleServer);

    ESwitch server_check = eDefault;
    parameters->GetServerCheck(&server_check);
    if (server_check == eDefault)
        server_check = key.GetFlag(CNetCacheKey::fNCKey_NoServerCheck) ?
                eOff : eOn;

    if (!key_is_mirrored || parameters->GetMirroringMode() ==
            CNetCacheAPI::eMirroringDisabled) {
        if (service_name_is_defined && server_check != eOff &&
                !FindOrCreateService(key)->IsInService(primary_server)) {
            NCBI_THROW_FMT(CNetSrvConnException, eServerNotInService,
                    key.GetKey() << ": NetCache server " <<
                    primary_server.GetServerAddress() << " could not be "
                    "accessed because it is not registered for the service.");
        }
        return primary_server.ExecWithRetry(cmd);
    } else {
        CNetServer::SExecResult exec_result;

        SNetCacheMirrorTraversal mirror_traversal(FindOrCreateService(key),
                primary_server, server_check);

        m_Service->IterateUntilExecOK(cmd, exec_result,
                &mirror_traversal, error_handling);

        return exec_result;
    }
}

CNetCachePasswordGuard::CNetCachePasswordGuard(CNetCacheAPI::TInstance nc_api,
    const string& password)
{
    if (!nc_api->m_DefaultParameters.GetPassword().empty()) {
        NCBI_THROW(CNetCacheException, eAuthenticationError,
            "Cannot reuse a password-protected NetCache API object.");
    }
    if (password.empty())
        m_NetCacheAPI = nc_api;
    else {
        m_NetCacheAPI = new SNetCacheAPIImpl(*nc_api);
        m_NetCacheAPI->m_DefaultParameters.SetPassword(password);
    }
}

CNetCacheAPI::CNetCacheAPI(CNetCacheAPI::EAppRegistry /* use_app_reg */,
        const string& conf_section /* = kEmptyStr */,
        CNetScheduleAPI::TInstance ns_api) :
    m_Impl(new SNetCacheAPIImpl(NULL, conf_section,
            kEmptyStr, kEmptyStr, ns_api))
{
}

CNetCacheAPI::CNetCacheAPI(const IRegistry& reg, const string& conf_section,
        CNetScheduleAPI::TInstance ns_api)
{
    CConfig conf(reg);
    m_Impl = new SNetCacheAPIImpl(&conf, conf_section,
            kEmptyStr, kEmptyStr, ns_api);
}

CNetCacheAPI::CNetCacheAPI(CConfig* conf, const string& conf_section,
        CNetScheduleAPI::TInstance ns_api) :
    m_Impl(new SNetCacheAPIImpl(conf, conf_section,
            kEmptyStr, kEmptyStr, ns_api))
{
}

CNetCacheAPI::CNetCacheAPI(const string& client_name,
        CNetScheduleAPI::TInstance ns_api) :
    m_Impl(new SNetCacheAPIImpl(NULL,
            kEmptyStr, kEmptyStr, client_name, ns_api))
{
}

CNetCacheAPI::CNetCacheAPI(const string& service_name,
        const string& client_name, CNetScheduleAPI::TInstance ns_api) :
    m_Impl(new SNetCacheAPIImpl(NULL,
            kEmptyStr, service_name, client_name, ns_api))
{
}

string CNetCacheAPI::PutData(const void* buf, size_t size,
        const CNamedParameterList* optional)
{
    return PutData(kEmptyStr, buf, size, optional);
}

CNetServerConnection SNetCacheAPIImpl::InitiateWriteCmd(
    CNetCacheWriter* nc_writer, const CNetCacheAPIParameters* parameters)
{
    string cmd("PUT3 ");
    cmd.append(NStr::IntToString(parameters->GetTTL()));

    const string& blob_id(nc_writer->GetBlobID());

    const bool write_existing_blob = !blob_id.empty();
    CNetCacheKey key;
    string stripped_blob_id;

    if (write_existing_blob) {
        key.Assign(blob_id, m_CompoundIDPool);
        cmd.push_back(' ');
        stripped_blob_id = key.StripKeyExtensions();
        cmd.append(stripped_blob_id);
    }

    AppendClientIPSessionIDPassword(&cmd, parameters);

    CNetServer::SExecResult exec_result;

    if (write_existing_blob)
        exec_result = ExecMirrorAware(key, cmd, parameters,
                SNetServiceImpl::eIgnoreServerErrors);
    else {
        try {
            exec_result = m_Service.FindServerAndExec(cmd);
        } catch (CNetSrvConnException& e) {
            SServerAddress* backup = s_GetFallbackServer();

            if (backup == NULL) {
                LOG_POST(Info << "Fallback server address is not configured.");
                throw;
            }

            ERR_POST_X(3, "Could not connect to " <<
                m_Service.GetServiceName() << ":" << e.what() <<
                ". Connecting to backup server " << backup->AsString() << ".");

            exec_result = m_Service.GetServer(*backup).ExecWithRetry(cmd);
        }
    }

    if (NStr::FindCase(exec_result.response, "ID:") != 0) {
        // Answer is not in the "ID:....." format
        exec_result.conn->Abort();
        string msg = "Unexpected server response: ";
        msg += exec_result.response;
        NCBI_THROW(CNetServiceException, eCommunicationError, msg);
    }
    exec_result.response.erase(0, 3);

    if (exec_result.response.empty()) {
        exec_result.conn->Abort();
        NCBI_THROW(CNetServiceException, eCommunicationError,
            "Invalid server response. Empty key.");
    }

    if (write_existing_blob) {
        if (exec_result.response != stripped_blob_id) {
            exec_result.conn->Abort();
            NCBI_THROW_FMT(CNetCacheException, eInvalidServerResponse,
                "Server created " << exec_result.response <<
                " in response to PUT3 \"" << stripped_blob_id << "\"");
        }
    } else {
        if (m_Service.IsLoadBalanced()) {
            CNetCacheKey::TNCKeyFlags key_flags = 0;

            switch (parameters->GetMirroringMode()) {
            case CNetCacheAPI::eMirroringDisabled:
                key_flags |= CNetCacheKey::fNCKey_SingleServer;
                break;
            case CNetCacheAPI::eMirroringEnabled:
                break;
            default:
                if (!CNetCacheServerListener::x_GetServerProperties(
                        exec_result.conn->m_Server)->mirrored)
                    key_flags |= CNetCacheKey::fNCKey_SingleServer;
            }

            bool server_check_hint = true;
            parameters->GetServerCheckHint(&server_check_hint);
            if (!server_check_hint)
                key_flags |= CNetCacheKey::fNCKey_NoServerCheck;

            CNetCacheKey::AddExtensions(exec_result.response,
                    m_Service.GetServiceName(), key_flags);
        }

        if (parameters->GetUseCompoundID())
            exec_result.response = CNetCacheKey::KeyToCompoundID(
                    exec_result.response, m_CompoundIDPool);

        nc_writer->SetBlobID(exec_result.response);
    }

    return exec_result.conn;
}


string  CNetCacheAPI::PutData(const string& key,
                              const void*   buf,
                              size_t        size,
                              const CNamedParameterList* optional)
{
    string actual_key(key);

    CNetCacheAPIParameters parameters(&m_Impl->m_DefaultParameters);

    parameters.LoadNamedParameters(optional);
    parameters.SetCachingMode(eCaching_Disable);

    CNetCacheWriter writer(m_Impl, &actual_key, eNetCache_Wait, &parameters);

    writer.WriteBufferAndClose(reinterpret_cast<const char*>(buf), size);

    return actual_key;
}

CNcbiOstream* CNetCacheAPI::CreateOStream(string& key,
        const CNamedParameterList* optional)
{
    return new CWStream(PutData(&key, optional), 0, NULL,
        CRWStreambuf::fOwnWriter | CRWStreambuf::fLeakExceptions);
}

IEmbeddedStreamWriter* CNetCacheAPI::PutData(string* key,
        const CNamedParameterList* optional)
{
    CNetCacheAPIParameters parameters(&m_Impl->m_DefaultParameters);

    parameters.LoadNamedParameters(optional);

    return new CNetCacheWriter(m_Impl, key, eNetCache_Wait, &parameters);
}


bool CNetCacheAPI::HasBlob(const string& blob_id,
        const CNamedParameterList* optional)
{
    CNetCacheKey key(blob_id, m_Impl->m_CompoundIDPool);

    CNetCacheAPIParameters parameters(&m_Impl->m_DefaultParameters);

    parameters.LoadNamedParameters(optional);

    try {
        return m_Impl->ExecMirrorAware(key, m_Impl->MakeCmd("HASB ",
                key, &parameters), &parameters).response[0] == '1';
    }
    catch (CNetCacheException& e) {
        if (e.GetErrCode() == CNetCacheException::eBlobNotFound)
            return false;
        throw;
    }
}


size_t CNetCacheAPI::GetBlobSize(const string& blob_id,
        const CNamedParameterList* optional)
{
    CNetCacheKey key(blob_id, m_Impl->m_CompoundIDPool);

    CNetCacheAPIParameters parameters(&m_Impl->m_DefaultParameters);

    parameters.LoadNamedParameters(optional);

    return CheckBlobSize(NStr::StringToUInt8(m_Impl->ExecMirrorAware(key,
        m_Impl->MakeCmd("GSIZ ", key, &parameters), &parameters).response));
}


void CNetCacheAPI::Remove(const string& blob_id,
        const CNamedParameterList* optional)
{
    CNetCacheAPIParameters parameters(&m_Impl->m_DefaultParameters);

    parameters.LoadNamedParameters(optional);

    CNetCacheKey key(blob_id, m_Impl->m_CompoundIDPool);

    try {
        m_Impl->ExecMirrorAware(key, m_Impl->MakeCmd("RMV2 ",
                key, &parameters), &parameters);
    } catch (std::exception& e) {
        ERR_POST("Could not remove blob \"" << blob_id << "\": " << e.what());
    } catch (...) {
        ERR_POST("Could not remove blob \"" << blob_id << "\"");
    }
}

void CNetCacheAPI::SetDefaultParameters(const CNamedParameterList* parameters)
{
    m_Impl->m_DefaultParameters.LoadNamedParameters(parameters);
}

CNetServerMultilineCmdOutput CNetCacheAPI::GetBlobInfo(const string& blob_id,
        const CNamedParameterList* optional)
{
    CNetCacheKey key(blob_id, m_Impl->m_CompoundIDPool);

    string cmd("GETMETA " + key.StripKeyExtensions());
    cmd.append(m_Impl->m_Service->m_ServerPool->m_EnforcedServerHost == 0 ?
            " 0" : " 1");

    CNetCacheAPIParameters parameters(&m_Impl->m_DefaultParameters);

    parameters.LoadNamedParameters(optional);

    CNetServerMultilineCmdOutput output(m_Impl->ExecMirrorAware(key,
            cmd, &parameters));

    output->SetNetCacheCompatMode();

    return output;
}

void CNetCacheAPI::PrintBlobInfo(const string& blob_key,
        const CNamedParameterList* optional)
{
    CNetServerMultilineCmdOutput output(GetBlobInfo(blob_key, optional));

    string line;

    if (output.ReadLine(line)) {
        if (!NStr::StartsWith(line, "SIZE="))
            NcbiCout << line << NcbiEndl;
        while (output.ReadLine(line))
            NcbiCout << line << NcbiEndl;
    }
}

void CNetCacheAPI::ProlongBlobLifetime(const string& blob_key, unsigned ttl,
        const CNamedParameterList* optional)
{
    CNetCacheKey key_obj(blob_key, m_Impl->m_CompoundIDPool);

    string cmd("PROLONG \"\" " + key_obj.StripKeyExtensions());

    cmd += " \"\" ttl=";
    cmd += NStr::NumericToString(ttl);

    CNetCacheAPIParameters parameters(&m_Impl->m_DefaultParameters);

    parameters.LoadNamedParameters(optional);

    m_Impl->AppendClientIPSessionIDPassword(&cmd, &parameters);

    m_Impl->ExecMirrorAware(key_obj, cmd, &parameters);
}

IReader* CNetCacheAPI::GetReader(const string& key, size_t* blob_size,
    const CNamedParameterList* optional)
{
    return m_Impl->GetPartReader(key, 0, 0, blob_size, optional);
}

IReader* CNetCacheAPI::GetPartReader(const string& key,
    size_t offset, size_t part_size, size_t* blob_size,
    const CNamedParameterList* optional)
{
    return m_Impl->GetPartReader(key, offset, part_size,
        blob_size, optional);
}

void CNetCacheAPI::ReadData(const string& key, string& buffer,
        const CNamedParameterList* optional)
{
    ReadPart(key, 0, 0, buffer, optional);
}

void CNetCacheAPI::ReadPart(const string& key,
    size_t offset, size_t part_size, string& buffer,
    const CNamedParameterList* optional)
{
    size_t blob_size;
    auto_ptr<IReader> reader(GetPartReader(key, offset, part_size,
        &blob_size, optional));

    buffer.resize(blob_size);

    m_Impl->ReadBuffer(*reader, const_cast<char*>(buffer.data()),
        blob_size, NULL, blob_size);
}

IReader* CNetCacheAPI::GetData(const string& key, size_t* blob_size,
    const CNamedParameterList* optional)
{
    try {
        return GetReader(key, blob_size, optional);
    }
    catch (CNetCacheBlobTooOldException&) {
        return NULL;
    }
    catch (CNetCacheException& e) {
        switch (e.GetErrCode()) {
        case CNetCacheException::eBlobNotFound:
        case CNetCacheException::eAccessDenied:
            return NULL;
        }
        throw;
    }
}

IReader* SNetCacheAPIImpl::GetPartReader(const string& blob_id,
    size_t offset, size_t part_size,
    size_t* blob_size_ptr, const CNamedParameterList* optional)
{
    CNetCacheKey key(blob_id, m_CompoundIDPool);

    string stripped_blob_id(key.StripKeyExtensions());

    const char* cmd_name;
    string cmd;

    if (offset == 0 && part_size == 0) {
        cmd_name = "GET2 ";
        cmd = cmd_name + stripped_blob_id;
    } else {
        cmd_name = "GETPART ";
        cmd = cmd_name + stripped_blob_id + ' ' +
            NStr::UInt8ToString((Uint8) offset) + ' ' +
            NStr::UInt8ToString((Uint8) part_size);
    }

    CNetCacheAPIParameters parameters(&m_DefaultParameters);

    parameters.LoadNamedParameters(optional);

    AppendClientIPSessionIDPassword(&cmd, &parameters);

    unsigned max_age = parameters.GetMaxBlobAge();
    if (max_age > 0) {
        cmd += " age=";
        cmd += NStr::NumericToString(max_age);
    }

    CNetServer::SExecResult exec_result(ExecMirrorAware(key, cmd, &parameters));

    unsigned* actual_age_ptr = parameters.GetActualBlobAgePtr();
    if (max_age > 0 && actual_age_ptr != NULL)
        *actual_age_ptr = x_ExtractBlobAge(exec_result, cmd_name);

    return new CNetCacheReader(this, blob_id,
            exec_result, blob_size_ptr, &parameters);
}

CNetCacheAPI::EReadResult CNetCacheAPI::GetData(const string&  key,
                      void*          buf,
                      size_t         buf_size,
                      size_t*        n_read,
                      size_t*        blob_size,
                      const CNamedParameterList* optional)
{
    _ASSERT(buf && buf_size);

    size_t x_blob_size = 0;

    auto_ptr<IReader> reader(GetData(key, &x_blob_size, optional));
    if (reader.get() == 0)
        return eNotFound;

    if (blob_size)
        *blob_size = x_blob_size;

    return m_Impl->ReadBuffer(*reader,
        (char*) buf, buf_size, n_read, x_blob_size);
}

CNetCacheAPI::EReadResult CNetCacheAPI::GetData(
    const string& key, CSimpleBuffer& buffer,
    const CNamedParameterList* optional)
{
    size_t x_blob_size = 0;

    auto_ptr<IReader> reader(GetData(key, &x_blob_size, optional));
    if (reader.get() == 0)
        return eNotFound;

    buffer.resize_mem(x_blob_size);
    return m_Impl->ReadBuffer(*reader,
        (char*) buffer.data(), x_blob_size, NULL, x_blob_size);
}

CNcbiIstream* CNetCacheAPI::GetIStream(const string& key, size_t* blob_size,
        const CNamedParameterList* optional)
{
    return new CRStream(GetReader(key, blob_size, optional), 0, NULL,
        CRWStreambuf::fOwnReader | CRWStreambuf::fLeakExceptions);
}

CNetCacheAdmin CNetCacheAPI::GetAdmin()
{
    return new SNetCacheAdminImpl(m_Impl);
}

CNetService CNetCacheAPI::GetService()
{
    return m_Impl->m_Service;
}

CNetCacheAPI CNetCacheAPI::GetServer(CNetServer::TInstance server)
{
    return new SNetCacheAPIImpl(server->m_ServerInPool, m_Impl);
}

void CNetCacheAPI::SetEventHandler(INetEventHandler* event_handler)
{
    m_Impl->GetListener()->m_EventHandler = event_handler;
}

CCompoundIDPool CNetCacheAPI::GetCompoundIDPool()
{
    return m_Impl->m_CompoundIDPool;
}

/* static */
CNetCacheAPI::EReadResult SNetCacheAPIImpl::ReadBuffer(
    IReader& reader,
    char* buf_ptr,
    size_t buf_size,
    size_t* n_read,
    size_t blob_size)
{
    size_t bytes_read;
    size_t total_bytes_read = 0;

    while (buf_size > 0) {
        ERW_Result rw_res = reader.Read(buf_ptr, buf_size, &bytes_read);
        if (rw_res == eRW_Success) {
            total_bytes_read += bytes_read;
            buf_ptr += bytes_read;
            buf_size -= bytes_read;
        } else if (rw_res == eRW_Eof) {
            break;
        } else {
            NCBI_THROW(CNetServiceException, eCommunicationError,
                       "Error while reading BLOB");
        }
    }

    if (n_read != NULL)
        *n_read = total_bytes_read;

    return total_bytes_read == blob_size ?
        CNetCacheAPI::eReadComplete : CNetCacheAPI::eReadPart;
}


///////////////////////////////////////////////////////////////////////////////

/// @internal
class CNetCacheAPICF : public IClassFactory<SNetCacheAPIImpl>
{
public:

    typedef SNetCacheAPIImpl TDriver;
    typedef SNetCacheAPIImpl IFace;
    typedef IFace TInterface;
    typedef IClassFactory<SNetCacheAPIImpl> TParent;
    typedef TParent::SDriverInfo TDriverInfo;
    typedef TParent::TDriverList TDriverList;

    /// Construction
    ///
    /// @param driver_name
    ///   Driver name string
    /// @param patch_level
    ///   Patch level implemented by the driver.
    ///   By default corresponds to interface patch level.
    CNetCacheAPICF(const string& driver_name = kNetCacheAPIDriverName,
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
            return new SNetCacheAPIImpl(&config, m_DriverName,
                kEmptyStr, kEmptyStr, NULL);
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


void NCBI_XCONNECT_EXPORT NCBI_EntryPoint_xnetcacheapi(
     CPluginManager<SNetCacheAPIImpl>::TDriverInfoList&   info_list,
     CPluginManager<SNetCacheAPIImpl>::EEntryPointRequest method)
{
    CHostEntryPointImpl<CNetCacheAPICF>::NCBI_EntryPointImpl(info_list, method);
}

//////////////////////////////////////////////////////////////////////////////
//

CBlobStorage_NetCache::CBlobStorage_NetCache()
{
    _ASSERT(0 && "Cannot create an unitialized CBlobStorage_NetCache object.");
}


CBlobStorage_NetCache::~CBlobStorage_NetCache()
{
    try {
        Reset();
    }
    NCBI_CATCH_ALL("CBlobStorage_NetCache_Impl::~CBlobStorage_NetCache()");
}

bool CBlobStorage_NetCache::IsKeyValid(const string& str)
{
    return CNetCacheKey::IsValidKey(str, m_NCClient.GetCompoundIDPool());
}

CNcbiIstream& CBlobStorage_NetCache::GetIStream(const string& key,
                                             size_t* blob_size,
                                             ELockMode /*lockMode*/)
{
    m_IStream.reset(m_NCClient.GetIStream(key, blob_size));
    return *m_IStream;
}

string CBlobStorage_NetCache::GetBlobAsString(const string& data_id)
{
    string buf;
    m_NCClient.ReadData(data_id, buf);
    return buf;
}

CNcbiOstream& CBlobStorage_NetCache::CreateOStream(string& key,
                                                   ELockMode /*lockMode*/)
{
    m_OStream.reset(m_NCClient.CreateOStream(key));
    return *m_OStream;
}

string CBlobStorage_NetCache::CreateEmptyBlob()
{
    return m_NCClient.PutData((const void*) NULL, 0);
}

void CBlobStorage_NetCache::DeleteBlob(const string& data_id)
{
    m_NCClient.Remove(data_id);
}

void CBlobStorage_NetCache::Reset()
{
    m_IStream.reset();
    m_OStream.reset();
}

END_NCBI_SCOPE
