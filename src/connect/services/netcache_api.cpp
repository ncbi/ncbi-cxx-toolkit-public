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
 *   Implementation of net cache client.
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

#define MAX_NETCACHE_PASSWORD_LENGTH 64


BEGIN_NCBI_SCOPE

static bool s_FallbackServer_Initialized = false;
static CSafeStaticPtr<auto_ptr<SServerAddress> > s_FallbackServer;

static SServerAddress* s_GetFallbackServer()
{
    if (s_FallbackServer_Initialized)
        return s_FallbackServer->get();
    try {
       string sport, host, hostport;
       hostport = TCGI_NetCacheFallbackServer::GetDefault();
       if (NStr::SplitInTwo(hostport, ":", host, sport)) {
          unsigned int port = NStr::StringToInt(sport);
          host = g_NetService_gethostip(host);
          s_FallbackServer->reset(new SServerAddress(host, port));
       }
    } catch (...) {
    }
    s_FallbackServer_Initialized = true;
    return s_FallbackServer->get();
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

    static const string default_temp_dir = ".";

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

        string enable_mirroring(config->GetString(config_section,
                "enable_mirroring", CConfig::eErr_NoThrow, "false"));

        if (NStr::CompareNocase(enable_mirroring, "on_read") == 0 ||
                NStr::CompareNocase(enable_mirroring, "onread") == 0)
            nc_impl->m_MirroringMode = SNetCacheAPIImpl::MirroredRead;
        else
            nc_impl->m_MirroringMode = NStr::StringToBool(enable_mirroring) ?
                    SNetCacheAPIImpl::eMirroringEnabled :
                    SNetCacheAPIImpl::eMirroringDisabled;
    } else {
        nc_impl->m_TempDir = default_temp_dir;
        nc_impl->m_CacheInput = false;
        nc_impl->m_CacheOutput = false;
        nc_impl->m_MirroringMode = SNetCacheAPIImpl::eMirroringDisabled;
    }
}

void CNetCacheServerListener::OnConnected(CNetServerConnection::TInstance conn)
{
    conn->WriteLine(m_Auth);
}

void CNetCacheServerListener::OnError(
    const string& err_msg, SNetServerImpl* server)
{
    string message = server->m_ServerInPool->m_Address.AsString();

    message += ": ";
    message += err_msg;

    static const char s_BlobNotFoundMsg[] = "BLOB not found";
    if (NStr::strncmp(err_msg.c_str(), s_BlobNotFoundMsg,
        sizeof(s_BlobNotFoundMsg) - 1) == 0)
        NCBI_THROW(CNetCacheException, eBlobNotFound, message);

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
    LOG_POST(Warning << server->m_ServerInPool->m_Address.AsString() <<
            ": " << warn_msg);
}

const char* const kNetCacheAPIDriverName = "netcache_api";

static const char* const s_NetCacheConfigSections[] = {
    kNetCacheAPIDriverName,
    "netcache_client",
    "netcache",
    NULL
};

static const string s_NetCacheAPIName("NetCacheAPI");

SNetCacheAPIImpl::SNetCacheAPIImpl(CConfig* config, const string& section,
        const string& service, const string& client_name) :
    m_Service(new SNetServiceImpl(s_NetCacheAPIName, client_name,
        new CNetCacheServerListener))
{
    m_Service->Init(this, service, config, section, s_NetCacheConfigSections);
}

void SNetCacheAPIImpl::SetPassword(const string& password)
{
    if (password.empty())
        m_Password = kEmptyStr;
    else {
        string encoded_password(NStr::PrintableString(password));

        if (encoded_password.length() > MAX_NETCACHE_PASSWORD_LENGTH) {
            NCBI_THROW(CNetCacheException,
                eAuthenticationError, "Password is too long");
        }

        m_Password.assign(" pass=\"");
        m_Password.append(encoded_password);
        m_Password.append("\"");
    }
}

void SNetCacheAPIImpl::AppendClientIPSessionIDPassword(string* cmd)
{
    CRequestContext& req = CDiagContext::GetRequestContext();
    cmd->append(" \"");
    cmd->append(req.GetClientIP());
    cmd->append("\" \"");
    cmd->append(req.GetSessionID());
    cmd->append("\"");
    if (!m_Password.empty())
        cmd->append(m_Password);
}

string SNetCacheAPIImpl::MakeCmd(const char* cmd)
{
    string result(cmd);
    AppendClientIPSessionIDPassword(&result);
    return result;
}

string SNetCacheAPIImpl::MakeCmd(const char* cmd_base, const CNetCacheKey& key)
{
    string result(cmd_base + key.StripKeyExtensions());
    AppendClientIPSessionIDPassword(&result);
    return result;
}

class SNCMirrorIterationBeginner : public IIterationBeginner
{
public:
    SNCMirrorIterationBeginner(CNetService service,
            CNetServer::TInstance primary_server) :
        m_Service(service),
        m_PrimaryServer(primary_server)
    {
    }

    virtual CNetServiceIterator BeginIteration();

    CNetService m_Service;
    CNetServer::TInstance m_PrimaryServer;
};

CNetServiceIterator SNCMirrorIterationBeginner::BeginIteration()
{
    return m_Service.Iterate(m_PrimaryServer);
}

CNetService SNetCacheAPIImpl::FindOrCreateService(const string& service_name)
{
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
    SNetCacheAPIImpl::ECmdType cmd_type,
    SNetServiceImpl::EServerErrorHandling error_handling)
{
    CNetServer primary_server(GetServer(key));

    if (!key.HasExtensions() || (m_MirroringMode != eMirroringEnabled &&
            (m_MirroringMode != MirroredRead || cmd_type != eReadCmd)))
        return primary_server.ExecWithRetry(cmd);

    CNetServer::SExecResult exec_result;

    SNCMirrorIterationBeginner iteration_beginner(key.HasExtensions() &&
                key.GetServiceName() != m_Service.GetServiceName() ?
            FindOrCreateService(key.GetServiceName()) : m_Service,
            primary_server);

    m_Service->IterateUntilExecOK(cmd, exec_result,
            &iteration_beginner, error_handling);

    return exec_result;
}

CNetCachePasswordGuard::CNetCachePasswordGuard(CNetCacheAPI::TInstance nc_api,
    const string& password)
{
    if (!nc_api->m_Password.empty()) {
        NCBI_THROW(CNetCacheException, eAuthenticationError,
            "Cannot reuse a password-protected NetCache API object.");
    }
    if (password.empty())
        m_NetCacheAPI = nc_api;
    else {
        m_NetCacheAPI = new SNetCacheAPIImpl(*nc_api);
        m_NetCacheAPI->SetPassword(password);
    }
}

CNetCacheAPI::CNetCacheAPI(CNetCacheAPI::EAppRegistry /* use_app_reg */,
        const string& conf_section /* = kEmptyStr */) :
    m_Impl(new SNetCacheAPIImpl(NULL, conf_section, kEmptyStr, kEmptyStr))
{
}

CNetCacheAPI::CNetCacheAPI(const IRegistry& reg, const string& conf_section)
{
    CConfig conf(reg);
    m_Impl = new SNetCacheAPIImpl(&conf, conf_section, kEmptyStr, kEmptyStr);
}

CNetCacheAPI::CNetCacheAPI(CConfig* conf, const string& conf_section) :
    m_Impl(new SNetCacheAPIImpl(conf, conf_section, kEmptyStr, kEmptyStr))
{
}

CNetCacheAPI::CNetCacheAPI(const string& client_name) :
    m_Impl(new SNetCacheAPIImpl(NULL, kEmptyStr, kEmptyStr, client_name))
{
}

CNetCacheAPI::CNetCacheAPI(const string& service_name,
        const string& client_name) :
    m_Impl(new SNetCacheAPIImpl(NULL, kEmptyStr, service_name, client_name))
{
}

string CNetCacheAPI::PutData(const void* buf, size_t size,
    unsigned time_to_live)
{
    return PutData(kEmptyStr, buf, size, time_to_live);
}

CNetServerConnection SNetCacheAPIImpl::InitiateWriteCmd(
    CNetCacheWriter* nc_writer)
{
    string cmd("PUT3 ");
    cmd.append(NStr::IntToString(nc_writer->GetTimeToLive()));

    const string& blob_id(nc_writer->GetBlobID());

    bool write_existing_blob = !blob_id.empty();
    bool add_extensions;
    CNetCacheKey key;
    string stripped_blob_id;

    if (write_existing_blob) {
        key.Assign(blob_id);
        cmd.push_back(' ');
        stripped_blob_id = (add_extensions = !key.HasExtensions()) ?
            blob_id : key.StripKeyExtensions();
        cmd.append(stripped_blob_id);
        if (m_MirroringMode != eMirroringEnabled)
            add_extensions = false;
    } else
        add_extensions = m_MirroringMode == eMirroringEnabled;

    AppendClientIPSessionIDPassword(&cmd);

    CNetServer::SExecResult exec_result;

    if (write_existing_blob)
        exec_result = ExecMirrorAware(key, cmd, eWriteCmd,
                SNetServiceImpl::eIgnoreServerErrors);
    else {
        try {
            exec_result = m_Service.FindServerAndExec(cmd);
        } catch (CNetSrvConnException& e) {
            SServerAddress* backup = s_GetFallbackServer();

            if (backup == NULL) {
                ERR_POST("Fallback server address is not configured.");
                throw;
            }

            ERR_POST_X(3, "Could not connect to " <<
                m_Service.GetServiceName() << ":" << e.what() <<
                ". Connecting to backup server " << backup->AsString() << ".");

            exec_result = m_Service->GetServer(*backup).ExecWithRetry(cmd);
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
        if (add_extensions && m_Service.IsLoadBalanced())
            CNetCacheKey::AddExtensions(exec_result.response,
                m_Service.GetServiceName());

        nc_writer->SetBlobID(exec_result.response);
    }

    return exec_result.conn;
}


string  CNetCacheAPI::PutData(const string& key,
                              const void*   buf,
                              size_t        size,
                              unsigned int  time_to_live)
{
    string actual_key(key);

    CNetCacheWriter writer(m_Impl, &actual_key, time_to_live,
        eNetCache_Wait, CNetCacheAPI::eCaching_Disable);

    writer.WriteBufferAndClose(reinterpret_cast<const char*>(buf), size);

    return actual_key;
}

CNcbiOstream* CNetCacheAPI::CreateOStream(string& key, unsigned time_to_live)
{
    return new CWStream(PutData(&key, time_to_live), 0, NULL,
        CRWStreambuf::fOwnWriter | CRWStreambuf::fLeakExceptions);
}

IEmbeddedStreamWriter* CNetCacheAPI::PutData(string* key,
    unsigned time_to_live, ECachingMode caching_mode)
{
    return new CNetCacheWriter(m_Impl, key, time_to_live,
        eNetCache_Wait, caching_mode);
}


bool CNetCacheAPI::HasBlob(const string& blob_id)
{
    CNetCacheKey key(blob_id);

    try {
        return m_Impl->ExecMirrorAware(key, m_Impl->MakeCmd("HASB ", key),
                SNetCacheAPIImpl::eReadCmd).response[0] == '1';
    } catch (CNetServiceException& e) {
        if (!TCGI_NetCacheUseHasbFallback::GetDefault() ||
                e.GetErrCode() != CNetServiceException::eCommunicationError ||
                e.GetMsg() != "Unknown request")
            throw;
        return !GetOwner(blob_id).empty();
    }
}


size_t CNetCacheAPI::GetBlobSize(const string& blob_id)
{
    CNetCacheKey key(blob_id);
    return CheckBlobSize(NStr::StringToUInt8(m_Impl->ExecMirrorAware(key,
        m_Impl->MakeCmd("GSIZ ", key), SNetCacheAPIImpl::eReadCmd).response));
}


void CNetCacheAPI::Remove(const string& blob_id)
{
    CNetCacheKey key(blob_id);
    try {
        m_Impl->ExecMirrorAware(key, m_Impl->MakeCmd("RMV2 ", key),
                SNetCacheAPIImpl::eWriteCmd);
    } catch (std::exception& e) {
        ERR_POST("Could not remove blob \"" << blob_id << "\": " << e.what());
    } catch (...) {
        ERR_POST("Could not remove blob \"" << blob_id << "\"");
    }
}


void CNetCacheAPI::PrintBlobInfo(const string& blob_id)
{
    string cmd("GETMETA " + blob_id);
    cmd.append(m_Impl->m_Service->m_ServerPool->m_EnforcedServerHost.empty() ?
            " 0" : " 1");

    CNetCacheKey key(blob_id);

    CNetServerMultilineCmdOutput output(m_Impl->ExecMirrorAware(key, cmd,
            SNetCacheAPIImpl::eReadCmd));

    output->SetNetCacheCompatMode();

    string line;

    if (output.ReadLine(line)) {
        if (!NStr::StartsWith(line, "SIZE="))
            printf("%s\n", line.c_str());
        while (output.ReadLine(line))
            printf("%s\n", line.c_str());
    }
}


string CNetCacheAPI::GetOwner(const string& blob_id)
{
    CNetCacheKey key(blob_id);
    try {
        return m_Impl->ExecMirrorAware(key, m_Impl->MakeCmd("GBOW ", key),
                SNetCacheAPIImpl::eReadCmd).response;
    } catch (CNetCacheException& e) {
        if (e.GetErrCode() == CNetCacheException::eBlobNotFound)
            return kEmptyStr;
        throw;
    }
}

IReader* CNetCacheAPI::GetReader(const string& key, size_t* blob_size,
    CNetCacheAPI::ECachingMode caching_mode)
{
    return m_Impl->GetPartReader(key, 0, 0, blob_size, caching_mode);
}

IReader* CNetCacheAPI::GetPartReader(const string& key,
    size_t offset, size_t part_size, size_t* blob_size,
    ECachingMode caching_mode)
{
    return m_Impl->GetPartReader(key, offset, part_size,
        blob_size, caching_mode);
}

void CNetCacheAPI::ReadData(const string& key, string& buffer)
{
    ReadPart(key, 0, 0, buffer);
}

void CNetCacheAPI::ReadPart(const string& key,
    size_t offset, size_t part_size, string& buffer)
{
    size_t blob_size;
    auto_ptr<IReader> reader(GetPartReader(key, offset, part_size,
        &blob_size, eCaching_Disable));

    buffer.resize(blob_size);

    m_Impl->ReadBuffer(*reader, const_cast<char*>(buffer.data()),
        blob_size, NULL, blob_size);
}

IReader* CNetCacheAPI::GetData(const string& key, size_t* blob_size,
    CNetCacheAPI::ECachingMode caching_mode)
{
    try {
        return GetReader(key, blob_size, caching_mode);
    } catch (CNetCacheException& e) {
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
    size_t* blob_size_ptr, CNetCacheAPI::ECachingMode caching_mode)
{
    CNetCacheKey key(blob_id);

    string stripped_blob_id(key.StripKeyExtensions());

    string cmd(offset == 0 && part_size == 0 ?
        "GET2 " + stripped_blob_id :
        "GETPART " + stripped_blob_id + ' ' +
            NStr::UInt8ToString((Uint8) offset) + ' ' +
            NStr::UInt8ToString((Uint8) part_size));

    AppendClientIPSessionIDPassword(&cmd);

    CNetServer::SExecResult exec_result(ExecMirrorAware(key, cmd,
            SNetCacheAPIImpl::eReadCmd));

    return new CNetCacheReader(this, blob_id,
        exec_result, blob_size_ptr, caching_mode);
}

CNetCacheAPI::EReadResult CNetCacheAPI::GetData(const string&  key,
                      void*          buf,
                      size_t         buf_size,
                      size_t*        n_read,
                      size_t*        blob_size)
{
    _ASSERT(buf && buf_size);

    size_t x_blob_size = 0;

    auto_ptr<IReader> reader(GetData(key, &x_blob_size, eCaching_Disable));
    if (reader.get() == 0)
        return eNotFound;

    if (blob_size)
        *blob_size = x_blob_size;

    return m_Impl->ReadBuffer(*reader,
        (char*) buf, buf_size, n_read, x_blob_size);
}

CNetCacheAPI::EReadResult CNetCacheAPI::GetData(
    const string& key, CSimpleBuffer& buffer)
{
    size_t x_blob_size = 0;

    auto_ptr<IReader> reader(GetData(key, &x_blob_size, eCaching_Disable));
    if (reader.get() == 0)
        return eNotFound;

    buffer.resize_mem(x_blob_size);
    return m_Impl->ReadBuffer(*reader,
        (char*) buffer.data(), x_blob_size, NULL, x_blob_size);
}

CNcbiIstream* CNetCacheAPI::GetIStream(const string& key, size_t* blob_size)
{
    return new CRStream(GetReader(key, blob_size), 0, NULL,
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
                kEmptyStr, kEmptyStr);
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
    return CNetCacheKey::IsValidKey(str);
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
