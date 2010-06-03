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
#include <corelib/request_ctx.hpp>
#include <corelib/plugin_manager_impl.hpp>
#include <corelib/ncbi_safe_static.hpp>
#include <corelib/rwstream.hpp>

#include <memory>


#define NCBI_USE_ERRCODE_X  ConnServ_NetCache

BEGIN_NCBI_SCOPE

const string s_InputBlobCachePrefix = ".nc_cache_input.";
const string s_OutputBlobCachePrefix = ".nc_cache_output.";

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
          host = CSocketAPI::ntoa(CSocketAPI::gethostbyname(host));
          s_FallbackServer->reset(new SServerAddress(host, port));
       }
    } catch (...) {
    }
    s_FallbackServer_Initialized = true;
    return s_FallbackServer->get();
}

void CNetCacheServerListener::OnInit(CNetObject* api_impl,
    CConfig* config, const string& config_section)
{
    SNetCacheAPIImpl* nc_impl = static_cast<SNetCacheAPIImpl*>(api_impl);

    m_Auth = nc_impl->m_Service->m_ClientName;

    if (m_Auth.length() < 3) {
        NCBI_THROW(CNetCacheException,
            eAuthenticationError, "Client name is too short or empty");
    }

    string temp_dir = config->GetString(config_section,
        "tmp_dir", CConfig::eErr_NoThrow, kEmptyStr);

    string default_temp_dir = ".";

    if (temp_dir.empty())
        temp_dir = config->GetString(config_section,
            "tmp_path", CConfig::eErr_NoThrow, default_temp_dir);

    nc_impl->m_TempDir = temp_dir.empty() ? default_temp_dir : temp_dir;

    nc_impl->m_CacheInput = config->GetBool(config_section,
        "cache_input", CConfig::eErr_NoThrow, false);

    nc_impl->m_CacheOutput = config->GetBool(config_section,
        "cache_output", CConfig::eErr_NoThrow, false);
}

void CNetCacheServerListener::OnConnected(CNetServerConnection::TInstance conn)
{
    conn->WriteLine(m_Auth);
}

void CNetCacheServerListener::OnError(
    const string& err_msg, SNetServerImpl* server)
{
    string message = server->m_Address.AsString();

    message += ": ";
    message += err_msg;

    static const char s_BlobNotFoundMsg[] = "BLOB not found";
    if (NStr::strncmp(err_msg.c_str(), s_BlobNotFoundMsg,
        sizeof(s_BlobNotFoundMsg) - 1) == 0)
        NCBI_THROW(CNetCacheException, eBlobNotFound, message);

    static const char s_UnknownCommandMsg[] = "Unknown command";
    if (NStr::strncmp(err_msg.c_str(), s_UnknownCommandMsg,
        sizeof(s_UnknownCommandMsg) - 1) == 0)
        NCBI_THROW(CNetCacheException, eUnknownCommand, message);

    if (err_msg.find("Cache unknown") != string::npos)
        NCBI_THROW(CNetCacheException, eUnknnownCache, message);

    NCBI_THROW(CNetServiceException, eCommunicationError, message);
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
        const string& lbsm_affinity_name) :
    m_Service(new SNetServiceImpl(service, client_name,
        new CNetCacheServerListener, lbsm_affinity_name))
{
    m_Service->Init(this, config, section, s_NetCacheConfigSections);
}

CNetServer SNetCacheAPIImpl::GetServer(const string& bid)
{
    CNetCacheKey key(bid);
    return m_Service->GetServer(key.GetHost(), key.GetPort());
}

void SNetCacheAPIImpl::AppendClientIPSessionIDPassword(string* cmd)
{
    CRequestContext& req = CDiagContext::GetRequestContext();
    cmd->append(" \"");
    cmd->append(req.GetClientIP());
    cmd->append("\" \"");
    cmd->append(req.GetSessionID());
    if (!m_Password.empty()) {
        cmd->append("\" pass=\"");
        cmd->append(m_Password);
    }
    cmd->append("\"");
}

string SNetCacheAPIImpl::MakeCmd(const char* cmd)
{
    string result(cmd);
    AppendClientIPSessionIDPassword(&result);
    return result;
}

string SNetCacheAPIImpl::MakeCmd(const char* cmd_base, const string& key)
{
    string result(cmd_base + key);
    AppendClientIPSessionIDPassword(&result);
    return result;
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
        m_NetCacheAPI->m_Password = password;
    }
}

CNetCacheAPI::CNetCacheAPI(CNetCacheAPI::EAppRegistry /* use_app_reg */,
        const string& conf_section /* = kEmptyStr */) :
    m_Impl(new SNetCacheAPIImpl(NULL, conf_section,
        kEmptyStr, kEmptyStr, kEmptyStr))
{
}

CNetCacheAPI::CNetCacheAPI(const IRegistry& reg, const string& conf_section)
{
    CConfig conf(reg);
    m_Impl = new SNetCacheAPIImpl(&conf, conf_section,
        kEmptyStr, kEmptyStr, kEmptyStr);
}

CNetCacheAPI::CNetCacheAPI(CConfig* conf, const string& conf_section) :
    m_Impl(new SNetCacheAPIImpl(conf, conf_section,
        kEmptyStr, kEmptyStr, kEmptyStr))
{
}

CNetCacheAPI::CNetCacheAPI(const string& client_name) :
    m_Impl(new SNetCacheAPIImpl(NULL, kEmptyStr,
        kEmptyStr, client_name, kEmptyStr))
{
}

CNetCacheAPI::CNetCacheAPI(const string& service_name,
        const string& client_name) :
    m_Impl(new SNetCacheAPIImpl(NULL, kEmptyStr,
        service_name, client_name, kEmptyStr))
{
}

CNetCacheAPI::CNetCacheAPI(const string& service_name,
        const string& client_name,
        const string& lbsm_affinity_name) :
    m_Impl(new SNetCacheAPIImpl(NULL, kEmptyStr,
        service_name, client_name, lbsm_affinity_name))
{
}

string CNetCacheAPI::PutData(const void*  buf,
                                size_t       size,
                                unsigned int time_to_live)
{
    return PutData(kEmptyStr, buf, size, time_to_live);
}


CNetServerConnection SNetCacheAPIImpl::InitiatePutCmd(
    string* key, unsigned time_to_live)
{
    _ASSERT(key);

    string request = "PUT3 ";
    request += NStr::IntToString(time_to_live);
    request += " ";

    CNetServer::SExecResult exec_result;

    if (!key->empty()) {
        request += *key;

        AppendClientIPSessionIDPassword(&request);

        exec_result = GetServer(*key).ExecWithRetry(request);
    } else {
        AppendClientIPSessionIDPassword(&request);

        try {
            exec_result = m_Service.FindServerAndExec(request);
        } catch (CNetSrvConnException& e) {
            SServerAddress* backup = s_GetFallbackServer();

            if (backup == NULL) {
                NCBI_THROW(CNetCacheException, eUnknnownCache,
                    "Fallback server address is not configured.");
            }

            ERR_POST_X(3, "Could not connect to " <<
                m_Service.GetServiceName() << ":" << e.what() <<
                ". Connecting to backup server " << backup->AsString() << ".");

            exec_result = m_Service->GetServer(*backup).ExecWithRetry(request);
        }
    }

    if (NStr::FindCase(exec_result.response, "ID:") != 0) {
        // Answer is not in "ID:....." format
        string msg = "Unexpected server response:";
        msg += exec_result.response;
        NCBI_THROW(CNetServiceException, eCommunicationError, msg);
    }
    exec_result.response.erase(0, 3);

    if (exec_result.response.empty()) {
        NCBI_THROW(CNetServiceException, eCommunicationError,
                   "Invalid server response. Empty key.");
    }

    *key = exec_result.response;

    return exec_result.conn;
}


string  CNetCacheAPI::PutData(const string& key,
                              const void*   buf,
                              size_t        size,
                              unsigned int  time_to_live)
{
    string k(key);

    m_Impl->WriteBuffer(m_Impl->InitiatePutCmd(&k, time_to_live),
        CNetCacheWriter::eNetCache_Wait, (const char*) buf, size);

    return k;
}

CNcbiOstream* CNetCacheAPI::CreateOStream(string& key, unsigned time_to_live)
{
    return new CWStream(PutData(&key, time_to_live), 0, NULL,
        CRWStreambuf::fOwnWriter | CRWStreambuf::fLogExceptions);

/*
    if (!m_Impl->m_CacheOutput) {
        auto_ptr<IWriter> writer(PutData(&key));
        m_OStream.reset( new CWStream(writer.release(), 0,0,
            CRWStreambuf::fOwnWriter));
        if (!m_OStream.get() || !m_OStream->good()) {
            m_OStream.reset();
            NCBI_THROW(CBlobStorageException, eWriter,
                "Writer couldn't create an ouput stream. BlobKey: " + key);
        }

    } else {
        if (key.empty() || !IsKeyValid(key))
            key = CreateEmptyBlob();
        m_CreatedBlobId = key;
        m_OStream.reset(CFile::CreateTmpFileEx(m_NCClient->m_TempDir,
            s_OutputBlobCachePrefix));
        if (!m_OStream.get() || !m_OStream->good()) {
            m_OStream.reset();
            NCBI_THROW(CBlobStorageException, eWriter,
                "Writer couldn't create a temporary file in the \"" +
                m_NCClient->m_TempDir +
                "\" directory. Check the \"tmp_dir\" parameter "
                "in the netcache_api configuration section.");
        }
    }
    return *m_OStream;

    ---------------------------------------

    try {
        if (m_NCClient->m_CacheOutput &&
                m_OStream.get() &&
                !m_CreatedBlobId.empty()) {
            auto_ptr<IWriter> writer;
            int try_count = 0;
            for (;;) {
                try {
                    writer.reset(m_NCClient.PutData(&m_CreatedBlobId));
                    break;
                }
                catch (CNetServiceException& ex) {
                    if (ex.GetErrCode() != CNetServiceException::eTimeout)
                        throw;
                    ERR_POST_XX(ConnServ_NetCache, 6,
                        "Communication Error : " << ex.what());
                    if (++try_count >= 2)
                        throw;
                    SleepMilliSec(1000 + try_count*2000);
                }
            }
            if (!writer.get()) {
                NCBI_THROW(CBlobStorageException,
                    eWriter, "Writer couldn't be created.");
            }
            fstream* fstr = dynamic_cast<fstream*>(m_OStream.get());
            if (fstr) {
                fstr->flush();
                fstr->seekg(0);
                char buf[1024];
                while (fstr->good()) {
                    fstr->read(buf, sizeof(buf));
                    if (writer->Write(buf, fstr->gcount()) != eRW_Success)
                        NCBI_THROW(CBlobStorageException,
                            eWriter, "Couldn't write to Writer.");
                }
            } else {
                NCBI_THROW(CBlobStorageException,
                    eWriter, "Wrong cast.");
            }
            m_CreatedBlobId = "";
        }

        if (m_OStream.get() && !m_NCClient->m_CacheOutput) {
            m_OStream->flush();
            if (!m_OStream->good()) {
                NCBI_THROW(CBlobStorageException, eWriter, " ");
            }
        }
    }
    catch (...) {
        m_OStream.reset();
        throw;
    }
*/



}

void SNetCacheAPIImpl::WriteBuffer(SNetServerConnectionImpl* conn_impl,
    CNetCacheWriter::EServerResponseType response_type,
    const char* buf_ptr, size_t buf_size)
{
    string error_message;
    {
        CNetCacheWriter writer(conn_impl, response_type, &error_message);

        size_t bytes_written;

        while (buf_size > 0) {
            ERW_Result io_res = writer.Write(buf_ptr, buf_size, &bytes_written);
            if (io_res != eRW_Success) {
                NCBI_THROW(CNetServiceException, eCommunicationError,
                    "Communication error");
            }
            buf_ptr += bytes_written;
            buf_size -= bytes_written;
        }
        writer.Flush();
    }
    if (!error_message.empty()) {
        NCBI_THROW(CNetServiceException, eCommunicationError, error_message);
    }
}


IWriter* CNetCacheAPI::PutData(string* key, unsigned int time_to_live,
    string* error_message)
{
    return new CNetCacheWriter(m_Impl->InitiatePutCmd(key, time_to_live),
        CNetCacheWriter::eNetCache_Wait, error_message);
}


bool CNetCacheAPI::HasBlob(const string& key)
{
    CNetServer srv = m_Impl->GetServer(key);

    try {
        return srv.ExecWithRetry(
            m_Impl->MakeCmd("HASB ", key)).response[0] == '1';
    } catch (CNetServiceException& e) {
        if (!TCGI_NetCacheUseHasbFallback::GetDefault() ||
                e.GetErrCode() != CNetServiceException::eCommunicationError ||
                e.GetMsg() != "Unknown request")
            throw;
        return !GetOwner(key).empty();
    }
}


size_t CNetCacheAPI::GetBlobSize(const string& key)
{
    return (size_t) NStr::StringToULong(m_Impl->GetServer(key).ExecWithRetry(
        m_Impl->MakeCmd("GSIZ ", key)).response);
}


void CNetCacheAPI::Remove(const string& key)
{
    CNetServer srv = m_Impl->GetServer(key);
    try {
        srv.ExecWithRetry(m_Impl->MakeCmd("RMV2 ", key));
    } catch (std::exception& e) {
        ERR_POST("Could not remove blob \"" << key << "\": " << e.what());
    } catch (...) {
        ERR_POST("Could not remove blob \"" << key << "\"");
    }
}


bool CNetCacheAPI::IsLocked(const string& key)
{
    CNetServer srv = m_Impl->GetServer(key);

    try {
        return srv.ExecWithRetry(
            m_Impl->MakeCmd("ISLK ", key)).response[0] == '1';
    } catch (CNetCacheException& e) {
        if (e.GetErrCode() == CNetCacheException::eBlobNotFound)
            return false;
        throw;
    }
}


string CNetCacheAPI::GetOwner(const string& key)
{
    CNetServer srv = m_Impl->GetServer(key);
    try {
        return srv.ExecWithRetry(m_Impl->MakeCmd("GBOW ", key)).response;
    } catch (CNetCacheException& e) {
        if (e.GetErrCode() == CNetCacheException::eBlobNotFound)
            return kEmptyStr;
        throw;
    }
}

IReader* CNetCacheAPI::GetReader(const string& key, size_t* blob_size)
{
    string cmd = "GET2 " + key;

    m_Impl->AppendClientIPSessionIDPassword(&cmd);

    return m_Impl->GetReadStream(
        m_Impl->GetServer(key).ExecWithRetry(cmd), blob_size);
}

void CNetCacheAPI::ReadData(const string& key, string& buffer)
{
    size_t blob_size;
    auto_ptr<IReader> reader(GetReader(key, &blob_size));

    buffer.resize(blob_size);

    m_Impl->ReadBuffer(*reader, const_cast<char*>(buffer.data()),
        blob_size, NULL, blob_size);
}

IReader* CNetCacheAPI::GetData(const string& key, size_t* blob_size)
{
    try {
        return GetReader(key, blob_size);
    } catch (CNetCacheException& e) {
        if (e.GetErrCode() == CNetCacheException::eBlobNotFound)
            return NULL;
        throw;
    }
}

IReader* SNetCacheAPIImpl::GetReadStream(
    const CNetServer::SExecResult& exec_result, size_t* blob_size)
{
    string::size_type pos = exec_result.response.find("SIZE=");

    if (pos == string::npos) {
        NCBI_THROW(CNetCacheException, eInvalidServerResponse,
            "No SIZE field in reply to a blob reading command");
    }

    size_t bsize = (size_t) atoi(
        exec_result.response.c_str() + pos + sizeof("SIZE=") - 1);

    if (blob_size)
        *blob_size = bsize;

    return new CNetCacheReader(exec_result.conn, bsize);
}

CNetCacheAPI::EReadResult
CNetCacheAPI::GetData(const string&  key,
                      void*          buf,
                      size_t         buf_size,
                      size_t*        n_read,
                      size_t*        blob_size)
{
    _ASSERT(buf && buf_size);

    size_t x_blob_size = 0;

    auto_ptr<IReader> reader(GetData(key, &x_blob_size));
    if (reader.get() == 0)
        return eNotFound;

    if (blob_size)
        *blob_size = x_blob_size;

    return m_Impl->ReadBuffer(*reader,
        (char*) buf, buf_size, n_read, x_blob_size);
}

CNetCacheAPI::EReadResult
CNetCacheAPI::GetData(const string& key, CSimpleBuffer& buffer)
{
    size_t x_blob_size = 0;

    auto_ptr<IReader> reader(GetData(key, &x_blob_size));
    if (reader.get() == 0)
        return eNotFound;

    buffer.resize_mem(x_blob_size);
    return m_Impl->ReadBuffer(*reader,
        (char*) buffer.data(), x_blob_size, NULL, x_blob_size);
}

CNcbiIstream* CNetCacheAPI::GetIStream(const string& key, size_t* blob_size)
{
    return new CRStream(GetReader(key, blob_size), 0, NULL,
        CRWStreambuf::fOwnReader | CRWStreambuf::fLogExceptions);

    /*if (blob_size) *blob_size = 0;
    size_t b_size = 0;
    auto_ptr<IReader> reader();

    if (blob_size)
        *blob_size = b_size;

    if (m_NCClient->m_CacheInput) {
        auto_ptr<fstream> fstr(CFile::CreateTmpFileEx(m_NCClient->m_TempDir,
            s_InputBlobCachePrefix));

        if (!fstr.get() || !fstr->good()) {
            fstr.reset();
            NCBI_THROW(CBlobStorageException, eReader,
                "Reader couldn't create a temporary file. BlobKey: " + key);
        }

        char buf[1024];
        size_t bytes_read = 0;

        while (reader->Read(buf, sizeof(buf), &bytes_read) == eRW_Success)
            fstr->write(buf, bytes_read);

        fstr->flush();
        fstr->seekg(0);
        m_IStream.reset(fstr.release());
    } else {
        m_IStream.reset(new CRStream(reader.release(), 0, 0,
            CRWStreambuf::fOwnReader | CRWStreambuf::fLogExceptions));

        if (!m_IStream.get() || !m_IStream->good()) {
            m_IStream.reset();
            NCBI_THROW(CBlobStorageException, eReader,
                "Reader couldn't create a input stream. BlobKey: " + key);
        }

    }

    return *m_IStream;*/
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
