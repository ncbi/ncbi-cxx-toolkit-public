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
 * Author:  Anatoliy Kuznetsov
 *
 * File Description:
 *   Implementation of net cache client.
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbitime.hpp>
#include <corelib/plugin_manager_impl.hpp>
#include <connect/ncbi_socket.hpp>
#include <connect/ncbi_conn_exception.hpp>
#include <connect/ncbi_conn_reader_writer.hpp>
#include <connect/services/netcache_client.hpp>
#include <util/transmissionrw.hpp>
#include <memory>


BEGIN_NCBI_SCOPE


const string kNetCache_KeyPrefix = "NCID";

void CNetCache_ParseBlobKey(CNetCache_Key* key, const string& key_str)
{
    _ASSERT(key);

    // NCID_01_1_MYHOST_9000

    const char* ch = key_str.c_str();
    key->hostname = key->prefix = kEmptyStr;

    // prefix

    for (;*ch && *ch != '_'; ++ch) {
        key->prefix += *ch;
    }
    if (*ch == 0) {
        NCBI_THROW(CNetCacheException, eKeyFormatError, "Key syntax error.");
    }
    ++ch;

    if (key->prefix != kNetCache_KeyPrefix) {
        NCBI_THROW(CNetCacheException, eKeyFormatError, 
                                       "Key syntax error. Invalid prefix.");
    }

    // version
    key->version = atoi(ch);
    while (*ch && *ch != '_') {
        ++ch;
    }
    if (*ch == 0) {
        NCBI_THROW(CNetCacheException, eKeyFormatError, "Key syntax error.");
    }
    ++ch;

    // id
    key->id = atoi(ch);
    while (*ch && *ch != '_') {
        ++ch;
    }
    if (*ch == 0) {
        NCBI_THROW(CNetCacheException, eKeyFormatError, "Key syntax error.");
    }
    ++ch;


    // hostname
    for (;*ch && *ch != '_'; ++ch) {
        key->hostname += *ch;
    }
    if (*ch == 0) {
        NCBI_THROW(CNetCacheException, eKeyFormatError, "Key syntax error.");
    }
    ++ch;

    // port
    key->port = atoi(ch);
}


unsigned CNetCache_GetBlobId(const string& key_str)
{
    unsigned id = 0;

    const char* ch = key_str.c_str();

    if (*ch == 0) {
err_throw:
        NCBI_THROW(CNetCacheException, eKeyFormatError, "Key syntax error.");
    }

    // prefix
    int prsum = (ch[0] - 'N') +
                (ch[1] - 'C') +
                (ch[2] - 'I') +
                (ch[3] - 'D');
    if (prsum != 0) {
        goto err_throw;
    }
    ch += 4;
    if (*ch != '_') {
        goto err_throw;
    }

    // version
    ch += 3;
    if (*ch != '_') {
        goto err_throw;
    }
    ++ch;

    // id
    id = (unsigned) atoi(ch);
    return id;
}


void CNetCache_GenerateBlobKey(string*        key, 
                               unsigned       id, 
                               const string&  host, 
                               unsigned short port)
{
    string tmp;
    *key = "NCID_01";    // NetCacheId prefix plus version

    NStr::IntToString(tmp, id);
    *key += "_";
    *key += tmp;

    *key += "_";
    *key += host;    

    NStr::IntToString(tmp, port);
    *key += "_";
    *key += tmp;

    NStr::IntToString(tmp, time(0));
    *key += "_";
    *key += tmp;
}




CNetCacheClient::CNetCacheClient(const string&  client_name)
    : CNetServiceClient(client_name),
      m_PutVersion(0)
{
}


CNetCacheClient::CNetCacheClient(const string&  host,
                                 unsigned short port,
                                 const string&  client_name)
    : CNetServiceClient(host, port, client_name),
      m_PutVersion(0)
{
}


CNetCacheClient::CNetCacheClient(CSocket*      sock, 
                                 const string& client_name)
    : CNetServiceClient(sock, client_name),
      m_PutVersion(0)
{
}


CNetCacheClient::~CNetCacheClient()
{
}



void CNetCacheClient::CheckConnect(const string& key)
{
    if (m_Sock && (eIO_Success == m_Sock->GetStatus(eIO_Open))) {
        return; // we are connected, nothing to do
    }

    // not connected

    if (!m_Host.empty()) { // we can restore connection
        CreateSocket(m_Host, m_Port);
        return;
    }

    // no primary host information

    if (key.empty()) {
        NCBI_THROW(CNetServiceException, eCommunicationError,
           "Cannot establish connection with a server. Unknown host name.");
    }

    CNetCache_Key blob_key;
    CNetCache_ParseBlobKey(&blob_key, key);
    CreateSocket(blob_key.hostname, blob_key.port);
}


string CNetCacheClient::PutData(const void*  buf,
                                size_t       size,
                                unsigned int time_to_live)
{
    return CNetCacheClient::PutData(kEmptyStr, buf, size, time_to_live);
}

void CNetCacheClient::PutInitiate(string*   key, 
                                  unsigned  time_to_live,
                                  unsigned  put_version)
{
    _ASSERT(key);

    string& request = m_Tmp;
    string put_str = "PUT";
    if (put_version) {
        put_str += NStr::IntToString(put_version);
    }
    put_str.push_back(' ');

    MakeCommandPacket(&request, put_str);
    
    request += NStr::IntToString(time_to_live);
    request += " ";
    request += *key;
   
    WriteStr(request.c_str(), request.length() + 1);
    WaitForServer();

    // Read BLOB_ID answer from the server
    if (!ReadStr(*m_Sock, key)) {
        NCBI_THROW(CNetServiceException, eCommunicationError, 
                   "Communication error");
    }

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
}



string  CNetCacheClient::PutData(const string& key,
                                 const void*   buf,
                                 size_t        size,
                                 unsigned int  time_to_live)
{
    _ASSERT(m_PutVersion == 0 || m_PutVersion == 2);

    CheckConnect(key);
    CSockGuard sg(*m_Sock);

    string k(key);
    PutInitiate(&k, time_to_live , m_PutVersion);

    m_Sock->DisableOSSendDelay(false);
    
    if (m_PutVersion == 2) {
        TransmitBuffer((const char*) buf, size);
    } else {
        WriteStr((const char*) buf, size);
    }

    return k;
}

IWriter* CNetCacheClient::PutData(string* key, unsigned int  time_to_live)
{
    _ASSERT(key);

    CheckConnect(*key);

    PutInitiate(key, time_to_live, 2);

    m_Sock->DisableOSSendDelay(false);

    IWriter* writer = new CNetCacheSock_RW(m_Sock);
    CTransmissionWriter* twriter = 
        new CTransmissionWriter(writer, eTakeOwnership);
    return twriter;
}


bool CNetCacheClient::IsAlive()
{
    string version = ServerVersion();
    return !version.empty();
}

string CNetCacheClient::ServerVersion()
{
    CheckConnect(kEmptyStr);
    CSockGuard sg(*m_Sock);

    string& request = m_Tmp;
    string version;

    MakeCommandPacket(&request, "VERSION ");
    
    WriteStr(request.c_str(), request.length() + 1);
    WaitForServer();

    // Read BLOB_ID answer from the server
    ReadStr(*m_Sock, &version);
    if (NStr::FindCase(version, "OK:") != 0) {
        // error message in version string
        string msg = version;
        if (msg.empty()) {
            msg = "Empty version string.";
        }
        NCBI_THROW(CNetServiceException, eCommunicationError, msg);
    }

    version.erase(0, 3);
    return version;
}



void CNetCacheClient::SendClientName()
{
    unsigned client_len = m_ClientName.length();
    if (client_len < 3) {
        NCBI_THROW(CNetCacheException, 
                   eAuthenticationError, "Client name too small or empty");
    }
    const char* client = m_ClientName.c_str();
    WriteStr(client, client_len + 1);
}

void CNetCacheClient::MakeCommandPacket(string* out_str, 
                                        const string& cmd_str)
{
    _ASSERT(out_str);

    if (m_ClientName.length() < 3) {
        NCBI_THROW(CNetCacheException, 
                   eAuthenticationError, "Client name is too small or empty");
    }

    *out_str = m_ClientName;
    const string& client_name_comment = GetClientNameComment();
    if (!client_name_comment.empty()) {
        out_str->append(" ");
        out_str->append(client_name_comment);
    }
    out_str->append("\r\n");
    out_str->append(cmd_str);
}

void CNetCacheClient::Remove(const string& key)
{
    CheckConnect(key);
    CSockGuard sg(*m_Sock);

    string& request = m_Tmp;
    MakeCommandPacket(&request, "REMOVE ");    
    request += key;
    WriteStr(request.c_str(), request.length() + 1);
}



IReader* CNetCacheClient::GetData(const string& key, size_t* blob_size)
{
    CheckConnect(key);

    string& request = m_Tmp;
    MakeCommandPacket(&request, "GET ");
    
    request += key;
    WriteStr(request.c_str(), request.length() + 1);

    WaitForServer();
    unsigned int bs = 0;

    string answer;
    bool res = ReadStr(*m_Sock, &answer);
    if (res) {
        if (NStr::strncmp(answer.c_str(), "OK:", 3) != 0) {
            string msg = "Server error:";
            msg += answer;
            NCBI_THROW(CNetCacheException, eServerError, msg);
        }
        if (blob_size) {
            string::size_type pos = answer.find("SIZE=");
            if (pos != string::npos) {
                const char* ch = answer.c_str() + pos + 5;
                bs = atoi(ch);
                *blob_size = (size_t) bs;
            } else {
                *blob_size = 0;
            }
        }
    }

    IReader* reader = new CNetCacheSock_RW(m_Sock);
    return reader;
}


CNetCacheClient::EReadResult
CNetCacheClient::GetData(const string&  key,
                         void*          buf, 
                         size_t         buf_size, 
                         size_t*        n_read,
                         size_t*        blob_size)
{
    _ASSERT(buf && buf_size);

    CheckConnect(key);
    CSockGuard sg(*m_Sock);

    size_t x_blob_size = 0;
    size_t x_read = 0;


    try {

        auto_ptr<IReader> reader(GetData(key, &x_blob_size));
        if (blob_size)
            *blob_size = x_blob_size;

        if (reader.get() == 0)
            return CNetCacheClient::eNotFound;

        size_t buf_avail = buf_size;
        unsigned char* buf_ptr = (unsigned char*) buf;

        if (n_read)
            *n_read = 0;

        while (buf_avail) {
            size_t bytes_read;
            ERW_Result rw_res = reader->Read(buf_ptr, buf_avail, &bytes_read);
            switch (rw_res) {
            case eRW_Success:
                x_read += bytes_read;
                if (n_read)
                    *n_read += bytes_read;
                buf_avail -= bytes_read;
                buf_ptr   += bytes_read;
                break;
            case eRW_Eof:
                if (x_read == 0)
                    return CNetCacheClient::eNotFound;
                if (n_read)
                    *n_read = x_read;
                return CNetCacheClient::eReadComplete;
            case eRW_Timeout:
                break;
            default:
                return CNetCacheClient::eNotFound;
            } // switch
        } // while

    } 
    catch (CNetCacheException& ex)
    {
        const string& str = ex.what();
        if (str.find_first_of("BLOB not found") > 0) {
            return CNetCacheClient::eNotFound;
        }
        throw;
    }

    if (x_read == x_blob_size) {
        return CNetCacheClient::eReadComplete;
    }

    return CNetCacheClient::eReadPart;
}


void CNetCacheClient::ShutdownServer()
{
    CheckConnect(kEmptyStr);
    CSockGuard sg(*m_Sock);

    SendClientName();

    const char command[] = "SHUTDOWN";
    WriteStr(command, sizeof(command));
}

void CNetCacheClient::Logging(bool on_off)
{
    CheckConnect(kEmptyStr);
    CSockGuard sg(*m_Sock);

    SendClientName();
    const char* command;
    if (on_off)
        command = "LOG ON";
    else
        command = "LOG OFF";

    WriteStr(command, strlen(command));
}




bool CNetCacheClient::IsError(const char* str)
{
    int cmp = NStr::strncasecmp(str, "ERR:", 4);
    return cmp == 0;
}


void CNetCacheClient::TransmitBuffer(const char* buf, size_t len)
{
    _ASSERT(m_Sock);

    CSocketReaderWriter  wrt(m_Sock, eNoOwnership);
    CTransmissionWriter twrt(&wrt, eNoOwnership);

    const char* buf_ptr = buf;
    size_t size_to_write = len;
    while (size_to_write) {
        size_t n_written;
        ERW_Result io_res =
            twrt.Write(buf_ptr, size_to_write, &n_written);
        NCBI_IO_CHECK_RW(io_res);

        size_to_write -= n_written;
        buf_ptr       += n_written;
    } // while
}



CNetCacheSock_RW::CNetCacheSock_RW(CSocket* sock) 
: CSocketReaderWriter(sock) 
{}

CNetCacheSock_RW::~CNetCacheSock_RW() 
{ 
    if (m_Sock) m_Sock->Close(); 
}

void CNetCacheSock_RW::OwnSocket() 
{ 
    m_IsOwned = eTakeOwnership; 
}


///////////////////////////////////////////////////////////////////////////////
const char* kNetCacheDriverName = "netcache_client";

/// @internal
class CNetCacheClientCF : public IClassFactory<CNetCacheClient>
{
public:

    typedef CNetCacheClient                 TDriver;
    typedef CNetCacheClient                 IFace;
    typedef IFace                           TInterface;
    typedef IClassFactory<CNetCacheClient>  TParent;
    typedef TParent::SDriverInfo   TDriverInfo;
    typedef TParent::TDriverList   TDriverList;

    /// Construction
    ///
    /// @param driver_name
    ///   Driver name string
    /// @param patch_level
    ///   Patch level implemented by the driver.
    ///   By default corresponds to interface patch level.
    CNetCacheClientCF(const string& driver_name = kNetCacheDriverName, 
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
            const string& client_name = 
                conf.GetString(m_DriverName, 
                               "client_name", CConfig::eErr_Throw, "noname");
            const string& service = 
                conf.GetString(m_DriverName, 
                               "service", CConfig::eErr_NoThrow, "");
            if (!service.empty()) {
                unsigned int rebalance_time = conf.GetInt(m_DriverName, 
                                                "rebalance_time",
                                                CConfig::eErr_NoThrow, 10);
                unsigned int rebalance_requests = conf.GetInt(m_DriverName,
                                                "rebalance_requests",
                                                CConfig::eErr_NoThrow, 100);
                unsigned int rebalance_bytes = conf.GetInt(m_DriverName,
                                                "rebalance_bytes",
                                                CConfig::eErr_NoThrow, 5*1024000);
                drv = new CNetCacheClient_LB(client_name, service,
                                             rebalance_time, 
                                             rebalance_requests,
                                             rebalance_bytes );
            } else { // non lb client
                const string& host = 
                    conf.GetString(m_DriverName, 
                                  "host", CConfig::eErr_Throw, "");
                unsigned int port = conf.GetInt(m_DriverName,
                                               "port",
                                               CConfig::eErr_Throw, 9001);

                drv = new CNetCacheClient(host, port, client_name);
            }
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

void NCBI_XCONNECT_EXPORT NCBI_EntryPoint_xnetcache(
     CPluginManager<CNetCacheClient>::TDriverInfoList&   info_list,
     CPluginManager<CNetCacheClient>::EEntryPointRequest method)
{
       CHostEntryPointImpl<CNetCacheClientCF>::
       NCBI_EntryPointImpl(info_list, method);

}

END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.39  2005/04/19 14:17:48  kuznets
 * Alternative put version
 *
 * Revision 1.38  2005/04/13 13:37:10  didenko
 * Changed NetCache PluginManager driver name to netcache_client
 *
 * Revision 1.37  2005/03/22 18:54:07  kuznets
 * Changed project tree layout
 *
 * Revision 1.36  2005/03/22 14:59:22  kuznets
 * Enable TCP/IP buffering when using IWriter
 *
 * Revision 1.35  2005/03/21 16:46:35  didenko
 * + creating from PluginManager
 *
 * Revision 1.34  2005/02/07 13:01:03  kuznets
 * Part of functionality moved to netservice_client.hpp(cpp)
 *
 * Revision 1.33  2005/01/28 19:25:22  kuznets
 * Exception method inlined
 *
 * Revision 1.32  2005/01/28 14:55:14  kuznets
 * GetCommunicatioTimeout() declared const
 *
 * Revision 1.31  2005/01/28 14:46:58  kuznets
 * Code clean-up, added PutData returning IWriter
 *
 * Revision 1.30  2005/01/19 12:21:47  kuznets
 * Fixed bug in restoring host name fron connection
 *
 * Revision 1.29  2005/01/03 19:11:30  kuznets
 * Code cleanup: polishing resource management
 *
 * Revision 1.28  2004/12/27 16:30:42  kuznets
 * + logging control function
 *
 * Revision 1.27  2004/12/20 13:14:10  kuznets
 * +NetcacheClient::SetSocket()
 *
 * Revision 1.26  2004/12/16 17:43:50  kuznets
 * + methods to change comm.timeouts
 *
 * Revision 1.25  2004/11/16 17:01:35  kuznets
 * Close connection after receiving server version
 *
 * Revision 1.24  2004/11/16 14:00:04  kuznets
 * Code cleanup: made use of CSocket::ReadLine() instead of ad-hoc code
 *
 * Revision 1.23  2004/11/10 18:47:30  kuznets
 * fixed connection in IsAlive()
 *
 * Revision 1.22  2004/11/10 13:37:21  kuznets
 * Code cleanup
 *
 * Revision 1.21  2004/11/10 12:40:30  kuznets
 * Bug fix: missing return
 *
 * Revision 1.20  2004/11/09 20:04:47  kuznets
 * Fixed logical errors in GetData
 *
 * Revision 1.19  2004/11/09 19:07:30  kuznets
 * Return not found code in GetData instead of exception
 *
 * Revision 1.18  2004/11/04 21:50:42  kuznets
 * CheckConnect() in Shutdown()
 *
 * Revision 1.17  2004/11/02 17:30:15  kuznets
 * Implemented reconnection mode and no-default server mode
 *
 * Revision 1.16  2004/11/02 13:51:42  kuznets
 * Improved diagnostics
 *
 * Revision 1.15  2004/11/01 16:02:26  kuznets
 * GetData now returns BLOB size
 *
 * Revision 1.14  2004/11/01 14:39:45  kuznets
 * Implemented BLOB update
 *
 * Revision 1.13  2004/10/28 16:16:20  kuznets
 * +CNetCacheClient::Remove()
 *
 * Revision 1.12  2004/10/27 14:16:59  kuznets
 * BLOB key parser moved from netcached
 *
 * Revision 1.11  2004/10/25 14:36:39  kuznets
 * New methods IsAlive(), ServerVersion()
 *
 * Revision 1.10  2004/10/21 15:52:56  kuznets
 * removed unused variable
 *
 * Revision 1.9  2004/10/20 14:51:05  kuznets
 * Optimization in networking
 *
 * Revision 1.8  2004/10/20 13:46:22  kuznets
 * Optimized networking
 *
 * Revision 1.7  2004/10/13 14:46:38  kuznets
 * Optimization in the networking
 *
 * Revision 1.6  2004/10/08 12:30:35  lavr
 * Cosmetics
 *
 * Revision 1.5  2004/10/06 16:49:10  ucko
 * CNetCacheClient::ReadStr: clear str with erase(), since clear() isn't
 * 100% portable.
 *
 * Revision 1.4  2004/10/06 15:27:24  kuznets
 * Removed unused variable
 *
 * Revision 1.3  2004/10/05 19:02:05  kuznets
 * Implemented ShutdownServer()
 *
 * Revision 1.2  2004/10/05 18:18:46  kuznets
 * +GetData, fixed bugs in protocol
 *
 * Revision 1.1  2004/10/04 18:44:59  kuznets
 * Initial revision
 *
 * ===========================================================================
 */
