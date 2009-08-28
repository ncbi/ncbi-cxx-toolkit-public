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
 * Author:  Anatoliy Kuznetsov, Dmitry Kazimirov
 *
 * File Description:
 *   Implementation of netcache ICache client.
 *
 */

#include <ncbi_pch.hpp>

#include "netservice_params.hpp"

#include <connect/services/neticache_client.hpp>
#include <connect/services/error_codes.hpp>
#include <connect/services/netcache_api_expt.hpp>

#include <connect/ncbi_conn_exception.hpp>
#include <connect/ncbi_conn_reader_writer.hpp>

#include <util/cache/icache_cf.hpp>
#include <util/transmissionrw.hpp>

#include <corelib/ncbitime.hpp>
#include <corelib/request_ctx.hpp>
#include <corelib/plugin_manager_impl.hpp>
#include <corelib/ncbi_system.hpp>
#include <corelib/ncbi_safe_static.hpp>

#include <memory>


#define NCBI_USE_ERRCODE_X   ConnServ_NetCache


BEGIN_NCBI_SCOPE


/// IReader/IWriter implementation
///
/// @internal
///
class CNetCacheSock_RW : public CSocketReaderWriter
{
public:
    typedef CSocketReaderWriter TParent;

    explicit CNetCacheSock_RW(CSocket* sock);
    explicit CNetCacheSock_RW(CSocket* sock, size_t blob_size);

    virtual ~CNetCacheSock_RW();

    /// Take socket ownership
    void OwnSocket();

    /// Set pointer on parent object responsible for socket pooling
    /// If parent set RW will return socket to its parent for reuse.
    /// If parent not set, socket is closed.
    void SetSocketParent(CNetICacheClient* parent);

    /// Access to CSocketReaderWriter::m_Sock
    CSocket& GetSocket() { _ASSERT(m_Sock); return *m_Sock; }

    virtual ERW_Result Read(void*   buf,
        size_t  count,
        size_t* bytes_read = 0);

    virtual ERW_Result PendingCount(size_t* count);

    void FinishTransmission();

    /// Set BLOB diagnostic comments
    void SetBlobComment(const string& comment)
    {
        m_BlobComment = comment;
    }

protected:
    CNetICacheClient*  m_Parent;
    /// Flag that EOF is controlled not on the socket level (sock close)
    /// but based on the BLOB size
    bool m_BlobSizeControl;
    /// Remaining BLOB size to be read
    size_t m_BlobBytesToRead;
    /// diagnostic comments (logging)
    string m_BlobComment;
};


CNetCacheSock_RW::CNetCacheSock_RW(CSocket* sock) :
    CSocketReaderWriter(sock),
    m_Parent(0),
    m_BlobSizeControl(false),
    m_BlobBytesToRead(0)
{
}

CNetCacheSock_RW::CNetCacheSock_RW(CSocket* sock, size_t blob_size) :
    CSocketReaderWriter(sock),
    m_Parent(0),
    m_BlobSizeControl(true),
    m_BlobBytesToRead(blob_size)
{
}

CNetCacheSock_RW::~CNetCacheSock_RW()
{
    try {
        if (m_BlobBytesToRead > 0) {
            m_BlobComment += ": remained bytes=";
            m_BlobComment += NStr::IntToString(m_BlobBytesToRead);
        }

        FinishTransmission();
    } catch(exception& ex) {
        ERR_POST_X(1, "Exception in CNetCacheSock_RW::~CNetCacheSock_RW():" << ex.what());
    } catch(...) {
        ERR_POST_X(2, "Unknown Exception in CNetCacheSock_RW::~CNetCacheSock_RW()");
    }
}

void CNetCacheSock_RW::FinishTransmission()
{
    if (m_Sock) {
        if (m_Parent) {
            m_Parent->ReturnSocket(m_Sock, m_BlobComment);
            m_Sock = 0;
        } else {
            if (m_Sock->GetStatus(eIO_Open) == eIO_Success)
                m_Sock->Close();
        }
    }
}

void CNetCacheSock_RW::OwnSocket()
{
    m_IsOwned = eTakeOwnership;
}

void CNetCacheSock_RW::SetSocketParent(CNetICacheClient* parent)
{
    m_Parent = parent;
}

ERW_Result CNetCacheSock_RW::PendingCount(size_t* count)
{
    if (m_BlobSizeControl && m_BlobBytesToRead == 0) {
        *count = 0;
        return eRW_Success;
    }

    return TParent::PendingCount(count);
}

ERW_Result CNetCacheSock_RW::Read(void* buf, size_t count, size_t* bytes_read)
{
    _ASSERT(m_BlobSizeControl);

    if (!m_BlobSizeControl)
        return TParent::Read(buf, count, bytes_read);

    ERW_Result res = eRW_Eof;

    if (m_BlobBytesToRead == 0) {
        if (bytes_read)
            *bytes_read = 0;

        return res;
    }

    if (m_BlobBytesToRead < count)
        count = m_BlobBytesToRead;

    size_t nn_read = 0;

    if (count)
        res = TParent::Read(buf, count, &nn_read);

    if (bytes_read)
        *bytes_read = nn_read;

    m_BlobBytesToRead -= nn_read;

    if (m_BlobBytesToRead == 0)
        FinishTransmission();

    return res;
}


/// IWriter with error checking
///
/// @internal
class CNetCache_WriterErrCheck : public CTransmissionWriter
{
public:
    typedef CTransmissionWriter TParent;

    CNetCache_WriterErrCheck(CNetCacheSock_RW* wrt,
        EOwnership own_writer,
        CTransmissionWriter::ESendEofPacket send_eof =
            CTransmissionWriter::eDontSendEofPacket);

    virtual ~CNetCache_WriterErrCheck();

    virtual ERW_Result Write(const void* buf,
        size_t count,
        size_t* bytes_written = 0);

    virtual ERW_Result Flush(void);

protected:
    void CheckInputMessage();

protected:
    CNetCacheSock_RW*  m_RW;
};

CNetCache_WriterErrCheck::CNetCache_WriterErrCheck(
        CNetCacheSock_RW* wrt,
        EOwnership own_writer,
        CTransmissionWriter::ESendEofPacket send_eof) :
    CTransmissionWriter(wrt, own_writer, send_eof),
    m_RW(wrt)
{
    _ASSERT(wrt);
}

CNetCache_WriterErrCheck::~CNetCache_WriterErrCheck()
{
}

ERW_Result CNetCache_WriterErrCheck::Write(
    const void* buf,
    size_t count,
    size_t* bytes_written)
{
    if (!m_RW) {  // error detected! block transmission
        if (bytes_written)
            *bytes_written = 0;

        return eRW_Error;
    }

    ERW_Result res = TParent::Write(buf, count, bytes_written);

    if (res == eRW_Success)
        CheckInputMessage();

    return res;
}


ERW_Result CNetCache_WriterErrCheck::Flush(void)
{
    if (!m_RW)
        return eRW_Error;

    ERW_Result res = TParent::Flush();

    if (res == eRW_Success)
        CheckInputMessage();

    return res;
}

void CNetCache_WriterErrCheck::CheckInputMessage()
{
    _ASSERT(m_RW);

    CSocket& sock = m_RW->GetSocket();

    string msg;

    STimeout to = {0, 0};
    EIO_Status io_st = sock.Wait(eIO_Read, &to);
    switch (io_st) {
    case eIO_Success:
        {
            io_st = sock.ReadLine(msg);
            if (io_st == eIO_Closed)
                goto closed_err;

            if (!msg.empty()) {
                if (msg.find("ERR:") == 0) {
                    msg.erase(0, 4);
                    msg = NStr::ParseEscapes(msg);
                }
                goto throw_err_msg;
            }
        }
        break;

    case eIO_Closed:
        goto closed_err;

    default:
        break;
    }
    return;

closed_err:
    msg = "Server closed communication channel (timeout?)";

throw_err_msg:
    m_RW = 0;

    NCBI_THROW(CNetServiceException, eCommunicationError, msg);
}


void s_MakeCommand(string* cmd)
{
    CRequestContext& req = CDiagContext::GetRequestContext();
    cmd->append(" \"");
    cmd->append(req.GetClientIP());
    cmd->append("\" \"");
    cmd->append(req.GetSessionID());
    cmd->append("\"");
}



CNetICacheClient::CNetICacheClient() :
    m_Sock(0),
    m_Host("localhost"),
    m_Port(9000),
    m_OwnSocket(eNoOwnership),
    m_ClientName("neticache_client"),
    m_Timeout(s_GetDefaultCommTimeout()),
    m_CacheName("default_cache"),
    m_Throttler(5000, CTimeSpan(60,0))
{
}


CNetICacheClient::CNetICacheClient(const string&  host,
                                   unsigned short port,
                                   const string&  cache_name,
                                   const string&  client_name) :
    m_Sock(0),
    m_Host(host),
    m_Port(port),
    m_OwnSocket(eNoOwnership),
    m_ClientName(client_name),
    m_Timeout(s_GetDefaultCommTimeout()),
    m_CacheName(cache_name),
    m_Throttler(5000, CTimeSpan(60,0))
{
}

CNetICacheClient::CNetICacheClient(
    const string& lb_service_name,
    const string& cache_name,
    const string& client_name,
    const string& lbsm_affinity_name) :
        m_Sock(0),
        m_OwnSocket(eNoOwnership),
        m_ClientName(client_name),
        m_Timeout(s_GetDefaultCommTimeout()),
        m_RebalanceStrategy(CreateDefaultRebalanceStrategy()),
        m_ServiceDiscovery(
            new CNetServiceDiscovery(lb_service_name, lbsm_affinity_name)),
        m_CacheName(cache_name),
        m_Throttler(5000, CTimeSpan(60,0))
{
}

void CNetICacheClient::SetConnectionParams(const string&  host,
                                           unsigned short port,
                                           const string&  cache_name,
                                           const string&  client_name)
{
    m_Host = host;
    m_Port = port;
    m_ServiceDiscovery = NULL;
    m_CacheName = cache_name;
    m_ClientName = client_name;
}

void CNetICacheClient::SetConnectionParams(
    CNetServiceDiscovery* service_discovery,
    IRebalanceStrategy* rebalance_strategy,
    const string& cache_name,
    const string& client_name)
{
    m_ServiceDiscovery = service_discovery;
    m_RebalanceStrategy = rebalance_strategy;
    m_CacheName = cache_name;
    m_ClientName = client_name;
}

CNetICacheClient::~CNetICacheClient()
{
    if (m_OwnSocket == eTakeOwnership)
        delete m_Sock;
}


void CNetICacheClient::ReturnSocket(CSocket* sock, const string& blob_comments)
{
    // check if socket has unattended input
    {{
        STimeout to = {0, 0};
        EIO_Status io_st = sock->Wait(eIO_Read, &to);
        string line;
        if (io_st == eIO_Success) {
            sock->ReadLine(line);
            ERR_POST_X(7, "ReturnSocket detected unread input for " <<
                       blob_comments << ": "<<line.size()<<" bytes:" << NStr::PrintableString(line));
            delete sock;
            return;
        }
    }}

    {{
        CFastMutexGuard guard(m_Lock);
        if (m_Sock == 0) {
            m_Sock = sock;
            return;
        }
    }}

    _ASSERT(sock);
    CFastMutexGuard guard(m_SockPool_Lock);
    m_SockPool.Put(sock);
}


void CNetICacheClient::SetCommunicationTimeout(const STimeout& to)
{
    m_Timeout = to;
    if (m_Sock) {
        m_Sock->SetTimeout(eIO_ReadWrite, &m_Timeout);
    }
}


STimeout CNetICacheClient::GetCommunicationTimeout() const
{
    return m_Timeout;
}


CSocket* CNetICacheClient::DetachSocket()
{
    CSocket* s = m_Sock; m_Sock = 0; return s;
}


bool CNetICacheClient::ReadStr(CSocket& sock, string* str)
{
    _ASSERT(str);

    EIO_Status io_st = sock.ReadLine(*str);
    switch (io_st)
    {
    case eIO_Success:
        return true;
    case eIO_Timeout:
        {
            string msg = "Communication timeout reading from server.";
            NCBI_THROW(CNetServiceException, eTimeout, msg);
        }
        break;
    default: // invalid socket or request, bailing out
        return false;
    }

    return true;
}


void CNetICacheClient::WriteStr(const char* str, size_t len)
{
    const char* buf_ptr = str;
    size_t size_to_write = len;
    while (size_to_write) {
        size_t n_written;
        EIO_Status io_st = m_Sock->Write(buf_ptr, size_to_write, &n_written);
        NCBI_IO_CHECK(io_st);
        size_to_write -= n_written;
        buf_ptr       += n_written;
    } // while
}


void CNetICacheClient::CreateSocket(const string& hostname,
                                     unsigned      port)
{
    EIO_Status io_st;
    if (m_Sock == 0) {
        m_Sock = new CSocket();
        if ( TServConn_UserLinger2::GetDefault() ) {
            STimeout zero = {0,0};
            m_Sock->SetTimeout(eIO_Close,&zero);
        }
        m_OwnSocket = eTakeOwnership;
    } //else {

    unsigned conn_repeats = 0;

    do {
        io_st = m_Sock->Connect(hostname, port, &m_Timeout, eOn);
        if (io_st != eIO_Success) {
            if (io_st == eIO_Unknown) {

                m_Sock->Close();

                // most likely this is an indication we have too many
                // open ports on the client side
                // (this kernel limitation manifests itself on Linux)
                //

                if (++conn_repeats > TServConn_ConnMaxRetries::GetDefault()) {
                    if ( io_st != eIO_Success) {
                        throw CIO_Exception(DIAG_COMPILE_INFO,
                            0, (CIO_Exception::EErrCode)io_st,
                            "IO error. Failed to connect to server.");
                    }
                    //NCBI_IO_CHECK(io_st);
                }
                // give system a chance to recover

                SleepMilliSec(1000 * conn_repeats);
            } else {
                NCBI_IO_CHECK(io_st);
            }
        } else {
            break;
        }

    } while (1);

    m_Sock->SetDataLogging(eDefault);

    //    }
    m_Sock->SetTimeout(eIO_ReadWrite, &m_Timeout);
    m_Sock->DisableOSSendDelay();

    m_Host = hostname;
    m_Port = port;
}


void CNetICacheClient::WaitForServer(unsigned wait_sec)
{
    STimeout to = {wait_sec, 0};
    if ( wait_sec == 0)
        to = m_Timeout;
    while (true) {
        EIO_Status io_st = m_Sock->Wait(eIO_Read, &to);
        if (io_st == eIO_Timeout) {
            NCBI_THROW(CNetServiceException, eTimeout,
                "No response from " + m_Host + ":" + NStr::IntToString(m_Port) +
                " for " + NStr::IntToString(to.sec) + " sec.");
        }
        else {
            break;
        }
    }
}


bool CNetICacheClient::CheckConnect()
{
    if (!m_Sock) {
        CFastMutexGuard guard(m_SockPool_Lock);
        m_Sock = m_SockPool.GetIfAvailable();
    }

    if (m_Sock && (eIO_Success == m_Sock->GetStatus(eIO_Open))) {
        // check if netcache session is in OK state
        // we have to do that, because if client program failed to
        // read the whole BLOB (deserialization error?) the network protocol
        // gets stuck in an incorrect state (needs to be closed)
        try {
            WriteStr("A?", 3);
            WaitForServer();
            if (!ReadStr(*m_Sock, &m_Tmp)) {
                delete m_Sock;m_Sock = 0;
                return CheckConnect();
            }
            if (m_Tmp[0] != 'O' || m_Tmp[1] != 'K') {
                delete m_Sock; m_Sock = 0;
                return CheckConnect();
            }
        }
        catch (exception& ) {
            delete m_Sock; m_Sock = 0;
            return CheckConnect();
        }

        return false; // we are connected, nothing to do
    }
    if (m_ServiceDiscovery) {
        m_RebalanceStrategy->OnResourceRequested();
        if (m_RebalanceStrategy->NeedRebalance() || m_Host.empty()) {
            TDiscoveredServers servers;
            m_ServiceDiscovery->QueryLoadBalancer(servers, false);
            if (servers.empty())
                m_Host.erase();
            else {
                m_Host = servers.front().host;
                m_Port = servers.front().port;
            }
        }
    }
    if (!m_Host.empty()) { // we can restore connection
//        m_Throttler.Approve(CRequestRateControl::eSleep);
        CSocketAPI::SetReuseAddress(eOn);
        CreateSocket(m_Host, m_Port);
        m_Sock->SetReuseAddress(eOn);
        return true;
    }
    NCBI_THROW(CNetServiceException, eCommunicationError,
        "Cannot establish connection with a server. Unknown host name.");
}


string CNetICacheClient::MakeCommandPacket(const string& cmd_str,
                                           bool          connected) const
{
    string out_str;

    if (m_ClientName.length() < 3) {
        NCBI_THROW(CNetCacheException,
                   eAuthenticationError, "Client name is too small or empty");
    }
    if (m_CacheName.empty()) {
        NCBI_THROW(CNetCacheException,
                   eAuthenticationError, "Cache name unknown");
    }

    if (connected) {
        // connection has been re-established,
        //   need to send authentication line
        out_str = m_ClientName;
        out_str.append("\r\n");
    }
    out_str.append("IC(");
    out_str.append(m_CacheName);
    out_str.append(") ");
    out_str.append(cmd_str);
    s_MakeCommand(&out_str);

    return out_str;
}


void CNetICacheClient::TrimPrefix(string* str) const
{
    CheckOK(str);
    str->erase(0, 3); // "OK:"
}


void CNetICacheClient::CheckOK(string* str) const
{
    if (str->find("OK:") != 0) {
        // Answer is not in "OK:....." format
        string msg = "Server error:";

        if (str->find("ERR:") == 0) {
            str->erase(0, 4);
            *str = NStr::ParseEscapes(*str);
        }

        if (str->find("Cache unknown") != string::npos) {
            NCBI_THROW(CNetCacheException, eUnknnownCache, *str);
        }

        msg += *str;
        NCBI_THROW(CNetServiceException, eCommunicationError, msg);
    }
}


void CNetICacheClient::RegisterSession(unsigned pid)
{
    CFastMutexGuard guard(m_Lock);
    if (CheckConnect()) {
        // connection reestablished
        WriteStr(m_ClientName.c_str(), m_ClientName.length() + 1);
    }

    _ASSERT(m_Sock);

    char hostname[256];
    string host;
    int status = SOCK_gethostname(hostname, sizeof(hostname));
    if (status != 0) {
        NCBI_THROW(CNetServiceException, eCommunicationError,
                   "Cannot get host name");
    }
    string cmd = "SMR ";
    cmd.append(hostname);
    cmd.push_back(' ');
    cmd.append(NStr::UIntToString(pid));
    s_MakeCommand(&cmd);
    WriteStr(cmd.c_str(), cmd.length() + 1);
    WaitForServer();
    if (!ReadStr(*m_Sock, &m_Tmp)) {
        NCBI_THROW(CNetServiceException, eCommunicationError,
                   "Communication error");
    }
    CheckOK(&m_Tmp);
}


void CNetICacheClient::UnRegisterSession(unsigned pid)
{
    CFastMutexGuard guard(m_Lock);
    if (CheckConnect()) {
        // connection reestablished
        WriteStr(m_ClientName.c_str(), m_ClientName.length() + 1);
    }

    _ASSERT(m_Sock);

    char hostname[256];
    string host;
    int status = SOCK_gethostname(hostname, sizeof(hostname));
    if (status != 0) {
        NCBI_THROW(CNetServiceException, eCommunicationError,
                   "Cannot get host name");
    }
    string cmd = "SMU ";
    cmd.append(hostname);
    cmd.push_back(' ');
    cmd.append(NStr::UIntToString(pid));
    s_MakeCommand(&cmd);
    WriteStr(cmd.c_str(), cmd.length() + 1);
    WaitForServer();
    if (!ReadStr(*m_Sock, &m_Tmp)) {
        NCBI_THROW(CNetServiceException, eCommunicationError,
                   "Communication error");
    }
    CheckOK(&m_Tmp);
}


void CNetICacheClient::SetTimeStampPolicy(TTimeStampFlags policy,
                                          unsigned int    timeout,
                                          unsigned int    max_timeout)
{
    CFastMutexGuard guard(m_Lock);

    CNetICacheClient* cl = const_cast<CNetICacheClient*>(this);
    bool reconnected = cl->CheckConnect();
    CSockGuard sg(*m_Sock);

    string cmd("STSP ");
    cmd.append(NStr::IntToString(policy));
    cmd.append(" ");
    cmd.append(NStr::IntToString(timeout));
    cmd.append(" ");
    cmd.append(NStr::IntToString(max_timeout));
    cmd = MakeCommandPacket(cmd, reconnected);

    WriteStr(cmd.c_str(), cmd.length() + 1);
    WaitForServer();
    if (!ReadStr(*m_Sock, &m_Tmp)) {
        NCBI_THROW(CNetServiceException, eCommunicationError,
                   "Communication error");
    }
    CheckOK(&m_Tmp);
}


ICache::TTimeStampFlags CNetICacheClient::GetTimeStampPolicy() const
{
    CFastMutexGuard guard(m_Lock);

    CNetICacheClient* cl = const_cast<CNetICacheClient*>(this);
    bool reconnected = cl->CheckConnect();
    CSockGuard sg(*m_Sock);

    string cmd("GTSP ");
    cmd = MakeCommandPacket(cmd, reconnected);

    cl->WriteStr(cmd.c_str(), cmd.length() + 1);
    cl->WaitForServer();
    if (!cl->ReadStr(*m_Sock, &cl->m_Tmp)) {
        NCBI_THROW(CNetServiceException, eCommunicationError,
                   "Communication error");
    }
    TrimPrefix(&cl->m_Tmp);

    ICache::TTimeStampFlags flags = atoi(m_Tmp.c_str());
    return flags;
}


int CNetICacheClient::GetTimeout() const
{
    CFastMutexGuard guard(m_Lock);

    CNetICacheClient* cl = const_cast<CNetICacheClient*>(this);
    bool reconnected = cl->CheckConnect();
    CSockGuard sg(*m_Sock);

    string cmd("GTOU ");
    MakeCommandPacket(cmd, reconnected);
    cl->WriteStr(cmd.c_str(), cmd.length() + 1);
    cl->WaitForServer();
    if (!cl->ReadStr(*m_Sock, &cl->m_Tmp)) {
        NCBI_THROW(CNetServiceException, eCommunicationError,
                   "Communication error");
    }
    TrimPrefix(&cl->m_Tmp);

    int to = atoi(m_Tmp.c_str());
    return to;
}


bool CNetICacheClient::IsOpen() const
{
    CFastMutexGuard guard(m_Lock);

    // need this trick to get over intrinsic non-const-ness of
    // the network protocol
    CNetICacheClient* cl = const_cast<CNetICacheClient*>(this);
    bool reconnected = cl->CheckConnect();
    CSockGuard sg(*m_Sock);

    string cmd("ISOP ");
    cmd = MakeCommandPacket(cmd, reconnected);
    cl->WriteStr(cmd.c_str(), cmd.length() + 1);
    cl->WaitForServer();
    string answer;
    if (!cl->ReadStr(*m_Sock, &answer)) {
        NCBI_THROW(CNetServiceException, eCommunicationError,
                   "Communication error");
    }
    TrimPrefix(&answer);

    int to = atoi(answer.c_str());
    return to != 0;

}


void CNetICacheClient::SetVersionRetention(EKeepVersions policy)
{
    CFastMutexGuard guard(m_Lock);

    bool reconnected = CheckConnect();
    CSockGuard sg(*m_Sock);
    string cmd("SVRP ");
    switch (policy) {
    case ICache::eKeepAll:
        cmd.append("KA");
        break;
    case ICache::eDropOlder:
        cmd.append("DO");
        break;
    case ICache::eDropAll:
        cmd.append("DA");
        break;
    default:
        _ASSERT(0);
    }
    cmd = MakeCommandPacket(cmd, reconnected);
    WriteStr(cmd.c_str(), cmd.length() + 1);
    WaitForServer();
    if (!ReadStr(*m_Sock, &m_Tmp)) {
        NCBI_THROW(CNetServiceException, eCommunicationError,
                   "Communication error");
    }
    CheckOK(&m_Tmp);
}


ICache::EKeepVersions CNetICacheClient::GetVersionRetention() const
{
    CFastMutexGuard guard(m_Lock);

    CNetICacheClient* cl = const_cast<CNetICacheClient*>(this);
    bool reconnected = cl->CheckConnect();
    CSockGuard sg(*m_Sock);

    string cmd("ISOP ");
    cmd = MakeCommandPacket(cmd, reconnected);
    cl->WriteStr(cmd.c_str(), cmd.length() + 1);
    cl->WaitForServer();
    string answer;
    if (!cl->ReadStr(*m_Sock, &answer)) {
        NCBI_THROW(CNetServiceException, eCommunicationError,
                   "Communication error");
    }
    TrimPrefix(&answer);
    int to = atoi(answer.c_str());

    return (ICache::EKeepVersions)to;
}


void CNetICacheClient::Store(const string&  key,
                             int            version,
                             const string&  subkey,
                             const void*    data,
                             size_t         size,
                             unsigned int   time_to_live,
                             const string&  owner)
{
    CFastMutexGuard guard(m_Lock);

    bool reconnected = CheckConnect();
//    CSockGuard sg(*m_Sock);
    string cmd("STRS ");
    cmd.append(NStr::UIntToString(time_to_live));
    cmd.push_back(' ');
    cmd.append(NStr::UIntToString(size));
    cmd.push_back(' ');
    AddKVS(&cmd, key, version, subkey);
    cmd = MakeCommandPacket(cmd, reconnected);

    WriteStr(cmd.c_str(), cmd.length() + 1);
    WaitForServer();
    if (!ReadStr(*m_Sock, &m_Tmp)) {
        NCBI_THROW(CNetServiceException, eCommunicationError,
                   "Communication error");
    }
    TrimPrefix(&m_Tmp);

    {{
    auto_ptr<CNetCacheSock_RW> wrt(new CNetCacheSock_RW(m_Sock));
    wrt->SetSocketParent(this);
    DetachSocket();
    guard.Release();
    auto_ptr<CNetCache_WriterErrCheck> writer(
       new CNetCache_WriterErrCheck(wrt.release(), eTakeOwnership));

    size_t     bytes_written;
    const char* ptr = (const char*) data;
    while (size) {
        ERW_Result res = writer->Write(ptr, size, &bytes_written);
        if (res != eRW_Success) {
            NCBI_THROW(CNetServiceException, eCommunicationError,
                       "Communication error");
        }
        ptr += bytes_written;
        size -= bytes_written;
    }
    }}

}


size_t CNetICacheClient::GetSize(const string&  key,
                                 int            version,
                                 const string&  subkey)
{
    CFastMutexGuard guard(m_Lock);

    bool reconnected = CheckConnect();

    string cmd("GSIZ ");
    AddKVS(&cmd, key, version, subkey);
    cmd = MakeCommandPacket(cmd, reconnected);

    WriteStr(cmd.c_str(), cmd.length() + 1);
    WaitForServer();
    if (!ReadStr(*m_Sock, &m_Tmp)) {
        NCBI_THROW(CNetServiceException, eCommunicationError,
                   "Communication error");
    }
    TrimPrefix(&m_Tmp);

    size_t sz = (size_t)atoi(m_Tmp.c_str());
    return sz;
}


void CNetICacheClient::GetBlobOwner(const string&  key,
                                    int            version,
                                    const string&  subkey,
                                    string*        owner)
{
    CFastMutexGuard guard(m_Lock);

    bool reconnected = CheckConnect();

    string cmd("GBLW ");
    AddKVS(&cmd, key, version, subkey);
    cmd = MakeCommandPacket(cmd, reconnected);

    WriteStr(cmd.c_str(), cmd.length() + 1);
    WaitForServer();
    if (!ReadStr(*m_Sock, &m_Tmp)) {
        NCBI_THROW(CNetServiceException, eCommunicationError,
                   "Communication error");
    }
    TrimPrefix(&m_Tmp);

    *owner = m_Tmp;
}


bool CNetICacheClient::Read(const string& key,
                            int           version,
                            const string& subkey,
                            void*         buf,
                            size_t        buf_size)
{
    size_t blob_size;
    auto_ptr<IReader> rdr;
    {{
        CFastMutexGuard guard(m_Lock);
        rdr.reset((GetReadStream_NoLock(key, version, subkey)));
        blob_size = m_BlobSize;
    }}


    if (rdr.get() == 0) {
        return false;
    }

    size_t x_read = 0;

    size_t buf_avail = buf_size;
    unsigned char* buf_ptr = (unsigned char*) buf;

    while (buf_avail && blob_size) {
        size_t bytes_read;
        ERW_Result rw_res =
            rdr->Read(buf_ptr, buf_avail, &bytes_read);
        switch (rw_res) {
        case eRW_Success:
            buf_avail -= bytes_read;
            buf_ptr   += bytes_read;
            blob_size -= bytes_read;
            x_read += bytes_read;
            break;
        case eRW_Eof:
            return (x_read == blob_size);
        case eRW_Timeout:
            break;
        default:
            return false;
        } // switch
    } // while

    return (x_read == blob_size);
}


void CNetICacheClient::GetBlobAccess(const string&     key,
                                     int               version,
                                     const string&     subkey,
                                     SBlobAccessDescr* blob_descr)
{
    size_t blob_size;
    {{
        CFastMutexGuard guard(m_Lock);
        blob_descr->reader.reset(GetReadStream_NoLock(key, version, subkey));
        blob_size = m_BlobSize;
    }}
    if (blob_descr->reader.get()) {
        blob_descr->blob_size = blob_size;
        blob_descr->blob_found = true;

        if (blob_descr->buf && blob_descr->buf_size > blob_size) {
            // read to buffer
            size_t to_read = blob_size;
            char* buf_ptr = blob_descr->buf;
            while (to_read) {
                size_t nn_read;
                ERW_Result rw_res =
                    blob_descr->reader->Read(buf_ptr, to_read, &nn_read);
                if (!nn_read || rw_res != eRW_Success) {
                    blob_descr->reader.reset(0);
                    NCBI_THROW(CNetServiceException, eCommunicationError,
                               "Communication error");
                }
                buf_ptr += nn_read;
                to_read -= nn_read;
            } // while
            blob_descr->reader.reset(0);
        }
    } else {
        blob_descr->blob_size = 0;
        blob_descr->blob_found = false;
    }
}


IWriter* CNetICacheClient::GetWriteStream(const string&    key,
                                          int              version,
                                          const string&    subkey,
                                          unsigned int     time_to_live,
                                          const string&    /*owner*/)
{
    CFastMutexGuard guard(m_Lock);

    bool reconnected = CheckConnect();
    CSockGuard sg(*m_Sock);
    string cmd("STOR ");
    cmd.append(NStr::UIntToString(time_to_live));
    cmd.push_back(' ');
    AddKVS(&cmd, key, version, subkey);
    cmd = MakeCommandPacket(cmd, reconnected);

    WriteStr(cmd.c_str(), cmd.length() + 1);
    WaitForServer();
    if (!ReadStr(*m_Sock, &m_Tmp)) {
        NCBI_THROW(CNetServiceException, eCommunicationError,
                   "Communication error");
    }
    TrimPrefix(&m_Tmp);

    CNetCacheSock_RW* writer = new CNetCacheSock_RW(m_Sock);
    DetachSocket();
    writer->OwnSocket();
    sg.Release();
    writer->SetSocketParent(this);
    CNetCache_WriterErrCheck* err_writer =
        new CNetCache_WriterErrCheck(writer, eTakeOwnership,
            CTransmissionWriter::eSendEofPacket);

    // Add BLOB stream comments (diagnostics):
    //      key-version-subkey[writer]
    //
    m_Tmp.erase();
    m_Tmp.append(key);
    m_Tmp.push_back('-');
    m_Tmp.append(NStr::IntToString(version));
    m_Tmp.push_back('-');
    m_Tmp.append(subkey);
    m_Tmp.append("[writer]");

    writer->SetBlobComment(m_Tmp);

    return err_writer;
}


void CNetICacheClient::Remove(const string& key)
{
    CFastMutexGuard guard(m_Lock);

    bool reconnected = CheckConnect();
    CSockGuard sg(*m_Sock);
    string cmd("REMK ");
    cmd.push_back('"');
    cmd.append(key);
    cmd.push_back('"');
    cmd = MakeCommandPacket(cmd, reconnected);
    WriteStr(cmd.c_str(), cmd.length() + 1);
    WaitForServer();
    if (!ReadStr(*m_Sock, &m_Tmp)) {
        NCBI_THROW(CNetServiceException, eCommunicationError,
                   "Communication error");
    }
    CheckOK(&m_Tmp);
}


void CNetICacheClient::Remove(const string&    key,
                              int              version,
                              const string&    subkey)
{
    CFastMutexGuard guard(m_Lock);

    bool reconnected = CheckConnect();
    CSockGuard sg(*m_Sock);
    string cmd("REMO ");
    AddKVS(&cmd, key, version, subkey);
    cmd = MakeCommandPacket(cmd, reconnected);

    WriteStr(cmd.c_str(), cmd.length() + 1);
    WaitForServer();
    if (!ReadStr(*m_Sock, &m_Tmp)) {
        NCBI_THROW(CNetServiceException, eCommunicationError,
                   "Communication error");
    }
    CheckOK(&m_Tmp);
}


time_t CNetICacheClient::GetAccessTime(const string&  key,
                                       int            version,
                                       const string&  subkey)
{
    CFastMutexGuard guard(m_Lock);

    bool reconnected = CheckConnect();
    CSockGuard sg(*m_Sock);
    string cmd("GACT ");
    AddKVS(&cmd, key, version, subkey);
    cmd = MakeCommandPacket(cmd, reconnected);

    WriteStr(cmd.c_str(), cmd.length() + 1);
    WaitForServer();
    if (!ReadStr(*m_Sock, &m_Tmp)) {
        NCBI_THROW(CNetServiceException, eCommunicationError,
                   "Communication error");
    }
    TrimPrefix(&m_Tmp);

    return NStr::StringToInt(m_Tmp);
}


bool CNetICacheClient::HasBlobs(const string&  key,
                                const string&  subkey)
{
    CFastMutexGuard guard(m_Lock);

    bool reconnected = CheckConnect();
    string cmd("HASB ");
    AddKVS(&cmd, key, 0, subkey);
    cmd = MakeCommandPacket(cmd, reconnected);

    WriteStr(cmd.c_str(), cmd.length() + 1);
    WaitForServer();

    string& answer = m_Tmp;
    if (!ReadStr(*m_Sock, &answer)) {
        NCBI_THROW(CNetServiceException, eCommunicationError,
                   "Communication error");
    }
    TrimPrefix(&answer);

    return NStr::StringToInt(answer) != 0;
}


void CNetICacheClient::Purge(time_t           access_timeout,
                             EKeepVersions    keep_last_version)
{
    CFastMutexGuard guard(m_Lock);

    bool reconnected = CheckConnect();
    string cmd("PRG1 ");
    cmd.append(NStr::IntToString((long) access_timeout));
    cmd.append(" ");
    cmd.append(NStr::IntToString((int)keep_last_version));
    cmd = MakeCommandPacket(cmd, reconnected);
    WriteStr(cmd.c_str(), cmd.length() + 1);
    WaitForServer();
    if (!ReadStr(*m_Sock, &m_Tmp)) {
        NCBI_THROW(CNetServiceException, eCommunicationError,
                   "Communication error");
    }
    CheckOK(&m_Tmp);
}


void CNetICacheClient::Purge(const string&    key,
                             const string&    subkey,
                             time_t           access_timeout,
                             EKeepVersions    keep_last_version)
{
}


IReader*
CNetICacheClient::GetReadStream_NoLock(const string&  key,
                                       int            version,
                                       const string&  subkey)
{
    bool reconnected = CheckConnect();
    CSockGuard sg(*m_Sock);
    string cmd("READ ");
    AddKVS(&cmd, key, version, subkey);
    cmd = MakeCommandPacket(cmd, reconnected);

    WriteStr(cmd.c_str(), cmd.length() + 1);
    WaitForServer();
    string& answer = m_Tmp;
    if (!ReadStr(*m_Sock, &answer)) {
        NCBI_THROW(CNetServiceException, eCommunicationError,
                   "Communication error");
    }

    bool blob_found = x_CheckErrTrim(answer);
    if (!blob_found) {
        sg.Release();
        return NULL;
    }
    string::size_type pos = answer.find("SIZE=");
    if (pos != string::npos) {
        const char* ch = answer.c_str() + pos + 5;
        m_BlobSize = (size_t)atoi(ch);
    } else {
        m_BlobSize = 0;
    }

    sg.Release();
    auto_ptr<CNetCacheSock_RW> rw(new CNetCacheSock_RW(m_Sock, m_BlobSize));

    rw->OwnSocket();
    DetachSocket();
    rw->SetSocketParent(this);

    // Add BLOB stream comments (diagnostics):
    //      key-version-subkey(BLOB size)
    //
    m_Tmp.erase();
    m_Tmp.append(key);
    m_Tmp.push_back('-');
    m_Tmp.append(NStr::IntToString(version));
    m_Tmp.push_back('-');
    m_Tmp.append(subkey);
    m_Tmp.push_back('(');
    m_Tmp.append(NStr::UIntToString(m_BlobSize));
    m_Tmp.push_back(')');

    rw->SetBlobComment(m_Tmp);

    return rw.release();
}


bool CNetICacheClient::x_CheckErrTrim(string& answer)
{
    if (NStr::strncmp(answer.c_str(), "OK:", 3) == 0)
        answer.erase(0, 3);
    else {
        string msg = "Server error:";

        if (NStr::strncmp(answer.c_str(), "ERR:", 4) == 0)
            answer.erase(0, 4);

        if (NStr::strncmp(answer.c_str(), "BLOB not found", 14) == 0)
            return false;

        msg += answer;

        if (NStr::strncmp(answer.c_str(), "BLOB locked", 11) == 0) {
            NCBI_THROW(CNetCacheException, eBlobLocked, msg);
        }

        NCBI_THROW(CNetCacheException, eServerError, msg);
    }

    return true;
}


IReader* CNetICacheClient::GetReadStream(const string&  key,
                                         int            version,
                                         const string&  subkey)
{
    CFastMutexGuard guard(m_Lock);
    return GetReadStream_NoLock(key, version, subkey);
}


void CNetICacheClient::AddKVS(string*          out_str,
                              const string&    key,
                              int              version,
                              const string&    subkey) const
{
    _ASSERT(out_str);

    out_str->push_back('"');
    out_str->append(key);
    out_str->push_back('"');
    out_str->append(" ");
    out_str->append(NStr::UIntToString(version));
    out_str->append(" ");
    out_str->push_back('"');
    out_str->append(subkey);
    out_str->push_back('"');
}



string CNetICacheClient::GetCacheName(void) const
{
    CFastMutexGuard guard(m_Lock);

    return m_CacheName;
}


bool CNetICacheClient::SameCacheParams(const TCacheParams* params) const
{
    CFastMutexGuard guard(m_Lock);

    return false;
}


const char* kNetICacheDriverName = "netcache";

/// Class factory for NetCache implementation of ICache
///
/// @internal
///
class CNetICacheCF : public CICacheCF<CNetICacheClient>
{
public:
    typedef CICacheCF<CNetICacheClient> TParent;
public:
    CNetICacheCF() : TParent(kNetICacheDriverName, 0)
    {
    }
    ~CNetICacheCF()
    {
    }

    virtual
    ICache* CreateInstance(
                   const string&    driver  = kEmptyStr,
                   CVersionInfo     version = NCBI_INTERFACE_VERSION(ICache),
                   const TPluginManagerParamTree* params = 0) const;

};


static const char* kCFParam_service         = "service";
static const char* kCFParam_server          = "server";
static const char* kCFParam_host            = "host";
static const char* kCFParam_port            = "port";
static const char* kCFParam_cache_name2     = "cache_name";
static const char* kCFParam_cache_name      = "name";
static const char* kCFParam_client          = "client";


ICache* CNetICacheCF::CreateInstance(
           const string&                  driver,
           CVersionInfo                   version,
           const TPluginManagerParamTree* params) const
{
    auto_ptr<CNetICacheClient> drv;
    if (driver.empty() || driver == m_DriverName) {
        if (version.Match(NCBI_INTERFACE_VERSION(ICache))
                            != CVersionInfo::eNonCompatible) {
            drv.reset(new CNetICacheClient());
        }
    } else {
        return 0;
    }

    if (!params)
        return drv.release();

    // cache client configuration
    string cache_name;
    try {
        cache_name =
            GetParam(params, kCFParam_cache_name, true);
    }
    catch (exception&)
    {
        cache_name =
            GetParam(params, kCFParam_cache_name2, true);
    }

    const string& client_name =
        GetParam(params, kCFParam_client, true);

    string service_name = GetParam(params, kCFParam_service, false);

    if (!service_name.empty()) {
        CConfig conf(params);

        CNetObjectRef<CNetServiceDiscovery> service_discovery(
            new CNetServiceDiscovery(service_name,
                conf.GetString(driver, "use_lbsm_affinity",
                    CConfig::eErr_NoThrow, kEmptyStr)));

        drv->SetConnectionParams(service_discovery,
            CreateSimpleRebalanceStrategy(conf, driver),
                cache_name, client_name);
    } else {
        string host;
        try {
            host = GetParam(params, kCFParam_server, true);
        }
        catch (exception&)
        {
            host = GetParam(params, kCFParam_host, true);
        }

        int port = GetParamInt(params, kCFParam_port, true, 9000);

        drv->SetConnectionParams(host, port, cache_name, client_name);
    }

    return drv.release();
}


void NCBI_EntryPoint_xcache_netcache(
     CPluginManager<ICache>::TDriverInfoList&   info_list,
     CPluginManager<ICache>::EEntryPointRequest method)
{
    CHostEntryPointImpl<CNetICacheCF>::NCBI_EntryPointImpl(info_list, method);
}


void Cache_RegisterDriver_NetCache(void)
{
    RegisterEntryPoint<ICache>( NCBI_EntryPoint_xcache_netcache );
}

END_NCBI_SCOPE
