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

#include "netcache_api_impl.hpp"
#include "netservice_params.hpp"

#include <connect/services/srv_connections_expt.hpp>
#include <connect/services/error_codes.hpp>

#include <connect/ncbi_conn_exception.hpp>

#include <corelib/ncbitime.hpp>
#include <corelib/request_ctx.hpp>
#include <corelib/plugin_manager_impl.hpp>
#include <corelib/ncbi_safe_static.hpp>

#include <memory>


#define NCBI_USE_ERRCODE_X  ConnServ_NetCache

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
       if ( NStr::SplitInTwo(hostport, ":", host, sport) ) {
          unsigned int port = NStr::StringToInt(sport);
          host = CSocketAPI::ntoa(CSocketAPI::gethostbyname(host));
          s_FallbackServer->reset(new SServerAddress(host, port));
       }
    } catch (...) {
    }
    s_FallbackServer_Initialized = true;
    return s_FallbackServer->get();
}


CNetServerConnection SNetCacheAPIImpl::x_GetConnection(const string& bid)
{
    CNetCacheKey key(bid);
    return m_Service->GetServer(key.GetHost(), key.GetPort()).Connect();
}

SNetCacheAPIImpl::CNetCacheServerListener::CNetCacheServerListener(
    const string& client_name)
{
    m_Auth = client_name;
}

void SNetCacheAPIImpl::CNetCacheServerListener::OnConnected(
    CNetServerConnection::TInstance conn)
{
    conn->WriteLine(m_Auth);
}

string SNetCacheAPIImpl::x_MakeCommand(const string& cmd) const
{
    string command(cmd);

    CRequestContext& req = CDiagContext::GetRequestContext();
    command += " \"";
    command += req.GetClientIP();
    command += "\" \"";
    command += req.GetSessionID();
    command += "\"";

    return command;
}

void SNetCacheAPIImpl::CNetCacheServerListener::OnError(
    const string& err_msg, SNetServerImpl* server)
{
    string message = server->m_Address.AsString();

    message += ": ";
    message += err_msg;

    if (NStr::strncmp(err_msg.c_str(), "BLOB not found", 14) == 0)
        NCBI_THROW(CNetCacheException, eBlobNotFound, message);

    if (NStr::strncmp(err_msg.c_str(), "BLOB locked", 11) == 0)
        NCBI_THROW(CNetCacheException, eBlobLocked, message);

    static const char s_UnknownCommandMsg[] = "Unknown command";
    if (NStr::strncmp(err_msg.c_str(), s_UnknownCommandMsg,
        sizeof(s_UnknownCommandMsg) - 1) == 0)
        NCBI_THROW(CNetCacheException, eUnknownCommand, message);

    NCBI_THROW(CNetServiceException, eCommunicationError, message);
}

CNetCacheAPI::CNetCacheAPI(const string& client_name) :
    m_Impl(new SNetCacheAPIImpl(kEmptyStr, client_name, kEmptyStr))
{
}


CNetCacheAPI::CNetCacheAPI(const string& service_name,
    const string& client_name,
    const string& lbsm_affinity_name) :
    m_Impl(new SNetCacheAPIImpl(service_name, client_name, lbsm_affinity_name))
{
}



string CNetCacheAPI::PutData(const void*  buf,
                                size_t       size,
                                unsigned int time_to_live)
{
    return PutData(kEmptyStr, buf, size, time_to_live);
}



CNetServerConnection SNetCacheAPIImpl::x_PutInitiate(
    string*   key, unsigned  time_to_live)
{
    _ASSERT(key);

    string request = "PUT3 ";
    request += NStr::IntToString(time_to_live);
    request += " ";

    CNetServerConnection conn;

    if (!key->empty()) {
        conn = x_GetConnection(*key);

        request += *key;
    } else {
        try {
            conn = m_Service.GetBestConnection();
        } catch (CNetSrvConnException& e) {
            SServerAddress* backup = s_GetFallbackServer();

            if (backup == NULL) {
                NCBI_THROW(CNetCacheException, eUnknnownCache,
                    "Fallback server address is not configured.");
            }

            ERR_POST_X(3, "Could not connect to " <<
                m_Service.GetServiceName() << ":" << e.what() <<
                ". Connecting to backup server " << backup->AsString() << ".");

            conn = m_Service->GetServer(*backup).Connect();
        }
    }

    *key = conn.Exec(x_MakeCommand(request));

    if (NStr::FindCase(*key, "ID:") != 0) {
        // Answer is not in "ID:....." format
        string msg = "Unexpected server response:";
        msg += *key;
        NCBI_THROW(CNetServiceException, eCommunicationError, msg);
    }
    key->erase(0, 3);

    if (key->empty()) {
        NCBI_THROW(CNetServiceException, eCommunicationError,
                   "Invalid server response. Empty key.");
    }
    return conn;
}


string  CNetCacheAPI::PutData(const string& key,
                              const void*   buf,
                              size_t        size,
                              unsigned int  time_to_live)
{
    string k(key);

    CNetServerConnection conn = m_Impl->x_PutInitiate(&k, time_to_live);
    CNetCacheWriter writer(*this, conn, CTransmissionWriter::eSendEofPacket);

    const char* buf_ptr = (const char*)buf;
    size_t size_to_write = size;
    while (size_to_write) {
        size_t n_written;
        ERW_Result io_res =
            writer.Write(buf_ptr, size_to_write, &n_written);
        NCBI_IO_CHECK_RW(io_res);

        size_to_write -= n_written;
        buf_ptr       += n_written;
    } // while
    writer.Flush();
    writer.Close();

    return k;
}


IWriter* CNetCacheAPI::PutData(string* key, unsigned int time_to_live)
{
    CNetServerConnection conn = m_Impl->x_PutInitiate(key, time_to_live);
    return new CNetCacheWriter(*this, conn, CTransmissionWriter::eSendEofPacket);
}


bool CNetCacheAPI::HasBlob(const string& key)
{
    if (m_Impl->m_NoHasBlob)
        return !GetOwner(key).empty();
    string request = "HASB " + key;
    try {
        return m_Impl->x_GetConnection(key).
            Exec(m_Impl->x_MakeCommand(request))[0] == '1';
    } catch (CNetServiceException& e) {
        if (e.GetErrCode() != CNetServiceException::eCommunicationError ||
            e.GetMsg() != "Unknown request")
            throw;
        m_Impl->m_NoHasBlob = true;
        return !GetOwner(key).empty();
    }
}


size_t CNetCacheAPI::GetBlobSize(const string& key)
{
    return (size_t) NStr::StringToULong(m_Impl->x_GetConnection(key).
        Exec(m_Impl->x_MakeCommand("GSIZ " + key)));
}


void CNetCacheAPI::Remove(const string& key)
{
    string request = "RMV2 " + key;

    CNetServerConnection conn = m_Impl->x_GetConnection(key);
    try {
        conn.Exec(m_Impl->x_MakeCommand(request));
    } catch (...) {
        // TODO log the error
    }
}


bool CNetCacheAPI::IsLocked(const string& key)
{

    string request = "ISLK " + key;

    CNetServerConnection conn = m_Impl->x_GetConnection(key);

    try {
        return conn.Exec(m_Impl->x_MakeCommand(request))[0] == '1';
    } catch (CNetCacheException& e) {
        if (e.GetErrCode() == CNetCacheException::eBlobNotFound)
            return false;
        throw;
    }
}


string CNetCacheAPI::GetOwner(const string& key)
{
    string request = "GBOW " + key;

    CNetServerConnection conn = m_Impl->x_GetConnection(key);
    try {
        return conn.Exec(m_Impl->x_MakeCommand(request));
    } catch (CNetCacheException& e) {
        if (e.GetErrCode() == CNetCacheException::eBlobNotFound)
            return kEmptyStr;
        throw;
    }
}

IReader* CNetCacheAPI::GetData(const string& key,
                                  size_t*       blob_size,
                                  ELockMode     lock_mode)
{
    string request = "GET2 " + key;

    switch (lock_mode) {
    case eLockNoWait:
        request += " NW";  // no-wait mode
        break;
    case eLockWait:
        break;
    default:
        _ASSERT(0);
        break;
    }

    size_t bsize = 0;
    CNetServerConnection conn = m_Impl->x_GetConnection(key);

    string answer;
    try {
        answer = conn.Exec(m_Impl->x_MakeCommand(request));
    } catch (CNetCacheException& e) {
        if (e.GetErrCode() == CNetCacheException::eBlobNotFound)
            return NULL;
        throw;
    }

    string::size_type pos = answer.find("SIZE=");
    if (pos != string::npos) {
        const char* ch = answer.c_str() + pos + 5;
        bsize = (size_t)atoi(ch);

        if (blob_size) {
            *blob_size = bsize;
        }
    }
    return new CNetCacheReader(conn, bsize);
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

    return m_Impl->x_ReadBuffer(*reader,
        buf, buf_size, n_read, x_blob_size);
}

CNetCacheAPI::EReadResult
CNetCacheAPI::GetData(const string& key, CSimpleBuffer& buffer)
{
    size_t x_blob_size = 0;

    auto_ptr<IReader> reader(GetData(key, &x_blob_size));
    if (reader.get() == 0)
        return eNotFound;

    buffer.resize_mem(x_blob_size);
    return m_Impl->x_ReadBuffer(*reader,
        buffer.data(), x_blob_size, NULL, x_blob_size);
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
CNetCacheAPI::EReadResult SNetCacheAPIImpl::x_ReadBuffer(
    IReader& reader,
    void* buf,
    size_t buf_size,
    size_t* n_read,
    size_t blob_size)
{
    size_t x_read = 0;
    size_t buf_avail = buf_size;
    unsigned char* buf_ptr = (unsigned char*) buf;

    size_t bytes_read;
    while (buf_avail) {
        ERW_Result rw_res = reader.Read(buf_ptr, buf_avail, &bytes_read);
        switch (rw_res) {
        case eRW_Success:
            x_read += bytes_read;
            buf_avail -= bytes_read;
            buf_ptr   += bytes_read;
            break;
        case eRW_Eof:
            buf_avail = 0; // stop the loop
            break;
        default:
            NCBI_THROW(CNetServiceException, eCommunicationError,
                       "Error while reading BLOB");
        } // switch
    } // while

    if (n_read)
        *n_read = x_read;

    return x_read == blob_size ?
        CNetCacheAPI::eReadComplete : CNetCacheAPI::eReadPart;
}


///////////////////////////////////////////////////////////////////////////////
const char* kNetCacheAPIDriverName = "netcache_api";

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
        TDriver* drv = 0;
        if (params && (driver.empty() || driver == m_DriverName)) {
            if (version.Match(NCBI_INTERFACE_VERSION(IFace))
                                != CVersionInfo::eNonCompatible) {

                CConfig conf(params);
                string client_name =
                    conf.GetString(m_DriverName,
                                   "client_name", CConfig::eErr_Throw, "noname");
                string service =
                    conf.GetString(m_DriverName,
                                   "service", CConfig::eErr_NoThrow, kEmptyStr);
                NStr::TruncateSpacesInPlace(service);
                unsigned int communication_timeout = conf.GetInt(m_DriverName,
                                                              "communication_timeout",
                                                              CConfig::eErr_NoThrow, 12);

                drv = new SNetCacheAPIImpl(service, client_name,
                    conf.GetString(m_DriverName, "use_lbsm_affinity",
                        CConfig::eErr_NoThrow, kEmptyStr));

                STimeout tm = { communication_timeout, 0 };
                drv->m_Service.SetCommunicationTimeout(tm);

                drv->m_Service.SetRebalanceStrategy(
                    CreateSimpleRebalanceStrategy(conf, m_DriverName));
            }
        }
        return drv;
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


END_NCBI_SCOPE
