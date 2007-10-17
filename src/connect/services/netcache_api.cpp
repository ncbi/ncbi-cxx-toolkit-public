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
 *   Implementation of net cache client.
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbitime.hpp>
#include <corelib/plugin_manager_impl.hpp>
#include <connect/ncbi_conn_exception.hpp>
#include <connect/services/netcache_api.hpp>
#include <connect/services/netcache_rw.hpp>
#include <connect/services/netcache_key.hpp>
#include <memory>

BEGIN_NCBI_SCOPE


/****************************************************************/
NCBI_PARAM_DECL(string, netcache_api, fallback_server); 
typedef NCBI_PARAM_TYPE(netcache_api, fallback_server) TCGI_NetCacheFallbackServer;
NCBI_PARAM_DEF(string, netcache_api, fallback_server, kEmptyStr);

static bool s_FallbackServer_Initialized = false;
static auto_ptr<CNetServiceConnector::TServer> s_FallbackServer;
static CNetServiceConnector::TServer* s_GetFallbackServer()
{
    if (s_FallbackServer_Initialized)
        return s_FallbackServer.get();
    try {
       string sport, host, hostport;
       hostport = TCGI_NetCacheFallbackServer::GetDefault(); 
       if ( NStr::SplitInTwo(hostport, ":", host, sport) ) {
          unsigned int port = NStr::StringToInt(sport);
          host = CSocketAPI::ntoa(CSocketAPI::gethostbyname(host));
          s_FallbackServer.reset(new CNetServiceConnector::TServer(host, port));
       }
    } catch (...) {
    }
    s_FallbackServer_Initialized = true;
    return s_FallbackServer.get();
}
/****************************************************************/

CNetServerConnectorHolder CNetCacheAPI::x_GetConnector(const string& bid) const
{
    if (bid.empty())
        return GetConnector().GetBest(s_GetFallbackServer());

    CNetCacheKey key(bid);
    return GetConnector().GetSpecific(key.host, key.port);
}

void CNetCacheAPI::x_SendAuthetication(CNetServerConnector& conn) const
{
    string auth = GetClientName();
    conn.WriteStr(auth + "\r\n");
}


CNetCacheAPI::CNetCacheAPI(const string&  client_name)
    : INetServiceAPI(kEmptyStr, client_name)
{
}


CNetCacheAPI::CNetCacheAPI(const string&  service,
                           const string&  client_name)
    : INetServiceAPI(service, client_name)
{
}



CNetCacheAPI::~CNetCacheAPI()
{
}


string CNetCacheAPI::PutData(const void*  buf,
                                size_t       size,
                                unsigned int time_to_live) const
{
    return PutData(kEmptyStr, buf, size, time_to_live);
}



CNetServerConnectorHolder CNetCacheAPI::x_PutInitiate(string*   key, 
                                                      unsigned  time_to_live) const
{
    _ASSERT(key);

    string request = "PUT3 ";   
    request += NStr::IntToString(time_to_live);
    request += " ";
    request += *key;

    CNetServerConnectorHolder holder = x_GetConnector(*key);
    *key = SendCmdWaitResponse(holder, request);   

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
    return holder;
}


string  CNetCacheAPI::PutData(const string& key,
                              const void*   buf,
                              size_t        size,
                              unsigned int  time_to_live) const
{
    string k(key);
    CNetServerConnectorHolder holder = x_PutInitiate(&k, time_to_live);
    CNetCacheWriter writer(*this, holder, CTransmissionWriter::eSendEofPacket);
    
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


IWriter* CNetCacheAPI::PutData(string* key, unsigned int time_to_live) const
{
    CNetServerConnectorHolder holder = x_PutInitiate(key, time_to_live);
    return new CNetCacheWriter(*this, holder, CTransmissionWriter::eSendEofPacket);
}

void CNetCacheAPI::Remove(const string& key) const
{
    string request = "RMV2 " + key;

    CNetServerConnectorHolder holder = x_GetConnector(key);
    try {
        SendCmdWaitResponse(holder, request);
    } catch (...) {
        // ignore the error???
    }

}



bool CNetCacheAPI::IsLocked(const string& key) const
{

    string request = "ISLK " + key;

    CNetServerConnectorHolder holder = x_GetConnector(key);

    try {
        string answer = SendCmdWaitResponse(holder, request);  
        if ( answer[0] == '1' )
            return true;
        return false;
    } catch (CNetCacheException& e) {
        if (e.GetErrCode() == CNetCacheException::eBlobNotFound)
            return false;
        throw;
    }
}


string CNetCacheAPI::GetOwner(const string& key) const
{
    string request = "GBOW " + key;

    CNetServerConnectorHolder holder = x_GetConnector(key);
    try {
        string answer = SendCmdWaitResponse(holder, request);
        return answer;
    } catch (CNetCacheException& e) {
        if (e.GetErrCode() == CNetCacheException::eBlobNotFound)
            return kEmptyStr;
        throw;
    }
}

IReader* CNetCacheAPI::GetData(const string& key, 
                                  size_t*       blob_size,
                                  ELockMode     lock_mode) const
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
    CNetServerConnectorHolder holder = x_GetConnector(key);

    string answer;
    try {
        answer = SendCmdWaitResponse(holder, request);   
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
    return new CNetCacheReader(holder, bsize);
}


CNetCacheAPI::EReadResult
CNetCacheAPI::GetData(const string&  key,
                      void*          buf, 
                      size_t         buf_size, 
                      size_t*        n_read,
                      size_t*        blob_size) const
{
    _ASSERT(buf && buf_size);

    size_t x_blob_size = 0;

    auto_ptr<IReader> reader(GetData(key, &x_blob_size)); 
    if (reader.get() == 0)
        return eNotFound;
    
    if (blob_size) 
        *blob_size = x_blob_size;

    return CNetCacheAPI::x_ReadBuffer(*reader, buf, buf_size, n_read, x_blob_size);
}

CNetCacheAPI::EReadResult 
CNetCacheAPI::GetData(const string& key, CSimpleBuffer& buffer) const
{
    size_t x_blob_size = 0;

    auto_ptr<IReader> reader(GetData(key, &x_blob_size)); 
    if (reader.get() == 0)
        return eNotFound;
    
    buffer.resize_mem(x_blob_size);
    return CNetCacheAPI::x_ReadBuffer(*reader, buffer.data(), x_blob_size, NULL, x_blob_size);
    
}

/* static */
CNetCacheAPI::EReadResult 
CNetCacheAPI::x_ReadBuffer( IReader& reader,
                            void*          buf, 
                            size_t         buf_size, 
                            size_t*        n_read,
                            size_t         blob_size)
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

    if (x_read == blob_size) {
        return eReadComplete; 
    }

    return eReadPart;
}

void CNetCacheAPI::ProcessServerError(string& response, ETrimErr trim_err) const
{
    if (trim_err == eTrimErr)
        INetServiceAPI::TrimErr(response);

    if (NStr::strncmp(response.c_str(), "BLOB not found", 14) == 0) 
        NCBI_THROW(CNetCacheException, eBlobNotFound, response);

    if (NStr::strncmp(response.c_str(), "BLOB locked", 11) == 0) 
        NCBI_THROW(CNetCacheException, eBlobLocked, "Server error:" + response);

    NCBI_THROW(CNetServiceException, eCommunicationError, response);

}



///////////////////////////////////////////////////////////////////////////////
const char* kNetCacheAPIDriverName = "netcache_api";

/// @internal
class CNetCacheAPICF : public IClassFactory<CNetCacheAPI>
{
public:

    typedef CNetCacheAPI                 TDriver;
    typedef CNetCacheAPI                 IFace;
    typedef IFace                        TInterface;
    typedef IClassFactory<CNetCacheAPI>  TParent;
    typedef TParent::SDriverInfo   TDriverInfo;
    typedef TParent::TDriverList   TDriverList;

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
                unsigned int rebalance_time = conf.GetInt(m_DriverName, 
                                                          "rebalance_time",
                                                          CConfig::eErr_NoThrow, 10);
                unsigned int rebalance_requests = conf.GetInt(m_DriverName,
                                                              "rebalance_requests",
                                                              CConfig::eErr_NoThrow, 100);
                unsigned int communication_timeout = conf.GetInt(m_DriverName,
                                                              "communication_timeout",
                                                              CConfig::eErr_NoThrow, 12);
                drv = new CNetCacheAPI(service, client_name);

                STimeout tm = { communication_timeout, 0 };
                drv->SetCommunicationTimeout(tm);

                drv->SetRebalanceStrategy(
                               new CSimpleRebalanceStrategy(rebalance_requests,
                                                            rebalance_time),
                               eTakeOwnership);

                //bool permanent_conntction =
                //   conf.GetBool(m_DriverName, "use_permanent_connection",  
                //                 CConfig::eErr_NoThrow, true);

                //if( permanent_conntction )
                drv->SetConnMode(INetServiceAPI::eKeepConnection);

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
     CPluginManager<CNetCacheAPI>::TDriverInfoList&   info_list,
     CPluginManager<CNetCacheAPI>::EEntryPointRequest method)
{
       CHostEntryPointImpl<CNetCacheAPICF>::
       NCBI_EntryPointImpl(info_list, method);
}


END_NCBI_SCOPE
