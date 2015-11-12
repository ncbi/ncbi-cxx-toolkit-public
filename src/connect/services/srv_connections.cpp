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
 *   Government have not placed any restriction on its use or reproduction.
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
 * Authors:  Maxim Didenko, Dmitry Kazimirov
 *
 * File Description:
 *
 */

#include <ncbi_pch.hpp>

#include "netservice_api_impl.hpp"

#include <connect/services/netschedule_api_expt.hpp>
#include <connect/services/error_codes.hpp>

#include <connect/ncbi_conn_stream.hpp>
#include <connect/ncbi_conn_exception.hpp>

#include <corelib/ncbi_system.hpp>

#include "../ncbi_comm.h"

#ifdef NCBI_OS_LINUX
# include <sys/socket.h>
# include <netinet/in.h>
# include <netinet/tcp.h>
#endif


#define NCBI_USE_ERRCODE_X   ConnServ_Connection

#define END_OF_MULTILINE_OUTPUT "END"

BEGIN_NCBI_SCOPE

static const STimeout s_ZeroTimeout = {0, 0};
#ifndef NCBI_OS_MSWIN
static const STimeout s_InternalConnectTimeout = {0, 250 * 1000};
#else
static const STimeout s_InternalConnectTimeout = {1, 250 * 1000};
#endif

///////////////////////////////////////////////////////////////////////////
SNetServerMultilineCmdOutputImpl::~SNetServerMultilineCmdOutputImpl()
{
    if (!m_ReadCompletely)
        m_Connection->Close();
}

bool CNetServerMultilineCmdOutput::ReadLine(string& output)
{
    _ASSERT(!m_Impl->m_ReadCompletely);

    if (!m_Impl->m_FirstLineConsumed) {
        output = m_Impl->m_FirstOutputLine;
        m_Impl->m_FirstOutputLine = kEmptyStr;
        m_Impl->m_FirstLineConsumed = true;
    } else if (!m_Impl->m_NetCacheCompatMode)
        m_Impl->m_Connection->ReadCmdOutputLine(output, true);
    else {
        try {
            m_Impl->m_Connection->ReadCmdOutputLine(output, true);
        }
        catch (CNetSrvConnException& e) {
            if (e.GetErrCode() != CNetSrvConnException::eConnClosedByServer)
                throw;

            m_Impl->m_ReadCompletely = true;
            return false;
        }
    }

    if (output != END_OF_MULTILINE_OUTPUT)
        return true;
    else {
        m_Impl->m_ReadCompletely = true;
        return false;
    }
}

inline SNetServerConnectionImpl::SNetServerConnectionImpl(
        SNetServerImpl* server) :
    m_Server(server),
    m_Generation(server->m_ServerInPool->m_CurrentConnectionGeneration.Get()),
    m_NextFree(NULL)
{
    if (TServConn_UserLinger2::GetDefault())
        m_Socket.SetTimeout(eIO_Close, &s_ZeroTimeout);
}

void SNetServerConnectionImpl::DeleteThis()
{
    // Return this connection to the pool.
    if (m_Server->m_ServerInPool->m_CurrentConnectionGeneration.Get() ==
            m_Generation && m_Socket.GetStatus(eIO_Open) == eIO_Success) {
        TFastMutexGuard guard(
                m_Server->m_ServerInPool->m_FreeConnectionListLock);

        //int upper_limit = m_Server.m_Impl->m_MaxConnPoolSize;
        int upper_limit = TServConn_MaxConnPoolSize::GetDefault();
        //LOG_POST("pool size: "<<upper_limit);

        if (upper_limit == 0 || m_Server->m_ServerInPool->
                m_FreeConnectionListSize < upper_limit) {
            m_NextFree = m_Server->m_ServerInPool->m_FreeConnectionListHead;
            m_Server->m_ServerInPool->m_FreeConnectionListHead = this;
            ++m_Server->m_ServerInPool->m_FreeConnectionListSize;
            m_Server = NULL;
            return;
        }
    }

    // Could not return the connection to the pool, delete it.
    delete this;
}

#define STRING_LEN(str) (sizeof(str) - 1)
#define WARNING_PREFIX "WARNING:"
#define WARNING_PREFIX_LEN STRING_LEN(WARNING_PREFIX)

void SNetServerConnectionImpl::ReadCmdOutputLine(string& result,
        bool multiline_output,
        INetServerConnectionListener* conn_listener)
{
    switch (m_Socket.ReadLine(result))
    {
    case eIO_Success:
        break;
    case eIO_Timeout:
        Abort();
        NCBI_THROW_FMT(CNetSrvConnException, eReadTimeout,
                "Communication timeout reading from " <<
                m_Server->m_ServerInPool->m_Address.AsString() <<
                " (timeout=" << NcbiTimeoutToMs(
                        m_Socket.GetTimeout(eIO_Read)) / 1000.0 << "s)");
        break;
    case eIO_Closed:
        Abort();
        NCBI_THROW_FMT(CNetSrvConnException, eConnClosedByServer,
                "Connection closed by " <<
                m_Server->m_ServerInPool->m_Address.AsString());
        break;
    default: // invalid socket or request, bailing out
        Abort();
        NCBI_THROW_FMT(CNetSrvConnException, eCommunicationError,
                "Communication error reading from " <<
                m_Server->m_ServerInPool->m_Address.AsString());
    }

    if (NStr::StartsWith(result, "OK:")) {
        const char* reply = result.c_str() + STRING_LEN("OK:");
        size_t reply_len = result.length() - STRING_LEN("OK:");
        while (reply_len >= WARNING_PREFIX_LEN &&
                memcmp(reply, WARNING_PREFIX, WARNING_PREFIX_LEN) == 0) {
            reply += WARNING_PREFIX_LEN;
            reply_len -= WARNING_PREFIX_LEN;
            const char* semicolon = strchr(reply, ';');
            if (conn_listener == NULL)
                conn_listener = m_Server->m_Service->m_Listener;
            if (semicolon == NULL) {
                conn_listener->OnWarning(string(reply, reply + reply_len),
                        m_Server);
                reply_len = 0;
                break;
            }
            conn_listener->OnWarning(string(reply, semicolon), m_Server);
            reply_len -= semicolon - reply + 1;
            reply = semicolon + 1;
        }
        result.erase(0, result.length() - reply_len);
    } else if (NStr::StartsWith(result, "ERR:")) {
        result.erase(0, STRING_LEN("ERR:"));
        result = NStr::ParseEscapes(result);
        (conn_listener != NULL ? conn_listener :
                m_Server->m_Service->m_Listener)->OnError(result, m_Server);
        result = multiline_output ? string(END_OF_MULTILINE_OUTPUT) : kEmptyStr;
    }
}


void SNetServerConnectionImpl::Close()
{
    m_Socket.Close();
}

void SNetServerConnectionImpl::Abort()
{
    m_Socket.Abort();
}

SNetServerConnectionImpl::~SNetServerConnectionImpl()
{
    Close();
}

void SNetServerConnectionImpl::WriteLine(const string& line)
{
    // TODO change to "\n" when no old NS/NC servers remain.
    string str(line + "\r\n");

    const char* buf = str.data();
    size_t len = str.size();

    while (len > 0) {
        size_t n_written;

        EIO_Status io_st = m_Socket.Write(buf, len, &n_written);

        if (io_st != eIO_Success) {
            Abort();

            NCBI_THROW_FMT(CNetSrvConnException, eWriteFailure,
                "Failed to write to " <<
                m_Server->m_ServerInPool->m_Address.AsString() << ": " <<
                IO_StatusStr(io_st));
        }
        len -= n_written;
        buf += n_written;
    }
}

CConfig* INetServerConnectionListener::PreInit(CObject* api_impl,
    CConfig* config, string* section)
{
    _ASSERT(section);

    if (config) {
        m_ClientName = config->GetString(*section, "client_name",
                CConfig::eErr_NoThrow);

        if (m_ClientName.empty()) {
            m_ClientName = config->GetString(*section, "client",
                    CConfig::eErr_NoThrow);
        }
    }

    return OnPreInit(api_impl, config, section);
}

namespace {
    class CTimeoutKeeper
    {
    public:
        CTimeoutKeeper(CSocket* sock, STimeout* timeout)
        {
            if (timeout == NULL)
                m_Socket = NULL;
            else {
                m_Socket = sock;
                m_ReadTimeout = *sock->GetTimeout(eIO_Read);
                m_WriteTimeout = *sock->GetTimeout(eIO_Write);
                sock->SetTimeout(eIO_ReadWrite, timeout);
            }
        }

        ~CTimeoutKeeper()
        {
            if (m_Socket != NULL) {
                m_Socket->SetTimeout(eIO_Read, &m_ReadTimeout);
                m_Socket->SetTimeout(eIO_Write, &m_WriteTimeout);
            }
        }

        CSocket* m_Socket;
        STimeout m_ReadTimeout;
        STimeout m_WriteTimeout;
    };
}

string CNetServerConnection::Exec(const string& cmd,
        bool multiline_output,
        STimeout* timeout,
        INetServerConnectionListener* conn_listener)
{
    CTimeoutKeeper timeout_keeper(&m_Impl->m_Socket, timeout);

    m_Impl->WriteLine(cmd);

    m_Impl->m_Socket.SetCork(false);
#ifdef NCBI_OS_LINUX
    int fd = 0, val = 1;
    m_Impl->m_Socket.GetOSHandle(&fd, sizeof(fd));
    setsockopt(fd, IPPROTO_TCP, TCP_QUICKACK, &val, sizeof(val));
#endif

    string output;

    m_Impl->ReadCmdOutputLine(output, multiline_output, conn_listener);

    return output;
}

CNetServerMultilineCmdOutput::CNetServerMultilineCmdOutput(
    const CNetServer::SExecResult& exec_result) :
        m_Impl(new SNetServerMultilineCmdOutputImpl(exec_result))
{
}

/*************************************************************************/
SNetServerInPool::SNetServerInPool(unsigned host, unsigned short port,
        INetServerProperties* server_properties) :
    m_Address(host, port),
    m_ServerProperties(server_properties)
{
    m_CurrentConnectionGeneration.Set(0);

    m_FreeConnectionListHead = NULL;
    m_FreeConnectionListSize = 0;

    ResetThrottlingParameters();

    m_RankBase = 1103515245 *
            // XOR the network prefix bytes of the IP address with the port
            // number (in network byte order) and convert the result
            // to host order so that it can be used in arithmetic operations.
            CSocketAPI::NetToHostLong(host ^ CSocketAPI::HostToNetShort(port)) +
            12345;
}

void SNetServerInPool::DeleteThis()
{
    CNetServerPool server_pool(m_ServerPool);

    if (!server_pool)
        return;

    // Before resetting the m_Service pointer, verify that no other object
    // has acquired a reference to this server object yet (between
    // the time the reference counter went to zero, and the
    // current moment when m_Service is about to be reset).
    CFastMutexGuard g(server_pool->m_ServerMutex);

    server_pool = NULL;

    if (!Referenced())
        m_ServerPool = NULL;
}

SNetServerInPool::~SNetServerInPool()
{
    // No need to lock the mutex in the destructor.
    SNetServerConnectionImpl* impl = m_FreeConnectionListHead;
    while (impl != NULL) {
        SNetServerConnectionImpl* next_impl = impl->m_NextFree;
        delete impl;
        impl = next_impl;
    }
}

SNetServerInfoImpl::SNetServerInfoImpl(const string& version_string)
{
    try {
        m_URLParser.reset(new CUrlArgs(version_string));
        m_Attributes = &m_URLParser->GetArgs();
    }
    catch (CUrlParserException&) {
        m_Attributes = &m_FreeFormVersionAttributes;

        const char* version = version_string.c_str();
        const char* prev_part_end = version;

        string attr_name, attr_value;

        for (; *version != '\0'; ++version) {
            if ((*version == 'v' || *version == 'V') &&
                    memcmp(version + 1, "ersion", 6) == 0) {
                const char* version_number = version + 7;
                attr_name.assign(prev_part_end, version_number);
                // Bring the 'v' in 'version' to lower case.
                attr_name[attr_name.size() - 7] = 'v';
                while (isspace(*version_number) ||
                        *version_number == ':' || *version_number == '=')
                    ++version_number;
                const char* version_number_end = version_number;
                while (isdigit(*version_number_end) ||
                        *version_number_end == '.')
                    ++version_number_end;
                attr_value.assign(version_number, version_number_end);
                m_FreeFormVersionAttributes.push_back(
                    CUrlArgs::TArg(attr_name, attr_value));
                prev_part_end = version_number_end;
                while (isspace(*prev_part_end) || *prev_part_end == '&')
                    ++prev_part_end;
            } else if ((*version == 'b' || *version == 'B') &&
                    memcmp(version + 1, "uil", 3) == 0 &&
                        (version[4] == 'd' || version[4] == 't')) {
                const char* build = version + 5;
                attr_name.assign(version, build);
                // Bring the 'b' in 'build' to upper case.
                attr_name[attr_name.size() - 5] = 'B';
                while (isspace(*build) || *build == ':' || *build == '=')
                    ++build;
                attr_value.assign(build);
                m_FreeFormVersionAttributes.push_back(
                    CUrlArgs::TArg(attr_name, attr_value));
                break;
            }
        }

        if (prev_part_end < version) {
            while (isspace(version[-1]))
                if (--version == prev_part_end)
                    break;

            if (prev_part_end < version)
                m_FreeFormVersionAttributes.push_back(CUrlArgs::TArg(
                    "Details", string(prev_part_end, version)));
        }
    }

    m_NextAttribute = m_Attributes->begin();
}

bool CNetServerInfo::GetNextAttribute(string& attr_name, string& attr_value)
{
    if (m_Impl->m_NextAttribute == m_Impl->m_Attributes->end())
        return false;

    attr_name = m_Impl->m_NextAttribute->name;
    attr_value = m_Impl->m_NextAttribute->value;
    ++m_Impl->m_NextAttribute;
    return true;
}

unsigned CNetServer::GetHost() const
{
    return m_Impl->m_ServerInPool->m_Address.host;
}

unsigned short CNetServer::GetPort() const
{
    return m_Impl->m_ServerInPool->m_Address.port;
}

string CNetServer::GetServerAddress() const
{
    return m_Impl->m_ServerInPool->m_Address.AsString();
}

CNetServerConnection SNetServerImpl::GetConnectionFromPool()
{
    for (;;) {
        TFastMutexGuard guard(m_ServerInPool->m_FreeConnectionListLock);

        if (m_ServerInPool->m_FreeConnectionListSize == 0)
            return NULL;

        // Get an existing connection object from the connection pool.
        CNetServerConnection conn = m_ServerInPool->m_FreeConnectionListHead;

        m_ServerInPool->m_FreeConnectionListHead = conn->m_NextFree;
        --m_ServerInPool->m_FreeConnectionListSize;
        conn->m_Server = this;

        guard.Release();

        // Check if the socket is already connected.
        if (conn->m_Socket.GetStatus(eIO_Open) != eIO_Success ||
                conn->m_Generation !=
                        m_ServerInPool->m_CurrentConnectionGeneration.Get())
            continue;

        SSOCK_Poll conn_socket_poll_struct = {
            /* sock     */ conn->m_Socket.GetSOCK(),
            /* event    */ eIO_ReadWrite
            /* revent   */ // [out]
        };

        if (SOCK_Poll(1, &conn_socket_poll_struct, &s_ZeroTimeout, NULL) ==
                eIO_Success && conn_socket_poll_struct.revent == eIO_Write)
            return conn;

        conn->m_Socket.Close();
    }
}

#ifdef NCBI_GRID_XSITE_CONN_SUPPORT
const char SNetServerImpl::kXSiteFwd[] = "XSITEFWD";
#endif

inline static bool operator <(const STimeout& t1, const STimeout& t2)
{
    return t1.sec == t2.sec ? t1.usec < t2.usec : t1.sec < t2.sec;
}

inline ostream& operator<<(ostream& out, const STimeout& tm)
{
    return out << (tm.sec+tm.usec*1e-6);
}

CNetServerConnection SNetServerImpl::Connect(STimeout* timeout,
        INetServerConnectionListener* conn_listener)
{
    CNetServerConnection conn = new SNetServerConnectionImpl(this);

    EIO_Status io_st;
    const STimeout& conn_timeout = timeout != NULL ? *timeout :
            m_ServerInPool->m_ServerPool->m_ConnTimeout;
    CDeadline deadline(conn_timeout.sec, conn_timeout.usec * 1000);
    STimeout internal_timeout = conn_timeout < s_InternalConnectTimeout ?
            conn_timeout : s_InternalConnectTimeout;

    //LOG_POST("internal timeout: "<<internal_timeout);

    SServerAddress server_address(m_ServerInPool->m_Address);

#ifdef NCBI_GRID_XSITE_CONN_SUPPORT
    ticket_t ticket = 0;

    if (m_Service->m_AllowXSiteConnections &&
            m_Service->IsColoAddr(m_ServerInPool->m_Address.host)) {
        union {
            SFWDRequestReply rq;
            char buffer[FWD_MAX_RR_SIZE + 1];
        };

        memset(buffer, 0, sizeof(buffer));
        rq.host =                     m_ServerInPool->m_Address.host;
        rq.port = SOCK_HostToNetShort(m_ServerInPool->m_Address.port);
        rq.flag = SOCK_HostToNetShort(1);
        _ASSERT(offsetof(SFWDRequestReply, text) +
                sizeof(kXSiteFwd) < sizeof(buffer));
        memcpy(rq.text, kXSiteFwd, sizeof(kXSiteFwd));

        size_t len = 0;

        CConn_ServiceStream svc(kXSiteFwd);
        if (svc.write((const char*) &rq.ticket/*0*/, sizeof(rq.ticket)) &&
                svc.write(buffer, offsetof(SFWDRequestReply, text) +
                        sizeof(kXSiteFwd))) {
            svc.read(buffer, sizeof(buffer) - 1);
            len = (size_t) svc.gcount();
            _ASSERT(len < sizeof(buffer));
        }

        memset(buffer + len, 0, sizeof(buffer) - len);

        if (len < offsetof(SFWDRequestReply, text) ||
            (rq.flag & 0xF0F0) || rq.port == 0) {
            const char* err;
            if (len == 0)
                err = "Connection refused";
            else if (len < offsetof(SFWDRequestReply, text))
                err = "Short response received";
            else if (!(rq.flag & 0xF0F0))
                err = rq.flag & 0x0F0F ? "Client rejected" : "Unknown error";
            else if (NStr::strncasecmp(buffer, "NCBI", 4) == 0)
                err = buffer;
            else if (rq.text[0])
                err = rq.text;
            else
                err = "Unspecified error";
            NCBI_THROW_FMT(CNetSrvConnException, eConnectionFailure,
                           "Error while acquiring an auth ticket from a "
                           "cross-site connection proxy: " << err);
        }

        if (rq.ticket != 0) {
            server_address.host =                     rq.host;
            server_address.port = SOCK_NetToHostShort(rq.port);
        } else {
            SOCK sock;
            server_address.port = 0;
            io_st = CONN_GetSOCK(svc.GetCONN(), &sock);
            if (sock != NULL)
                SOCK_CreateOnTop(sock, 0, &sock);
            if (io_st != eIO_Success  ||  sock == NULL) {
                NCBI_THROW(CNetSrvConnException, eConnectionFailure,
                           "Error while connecting to proxy.");
            }
            conn->m_Socket.Reset(sock, eTakeOwnership, eCopyTimeoutsToSOCK);
        }
        ticket = rq.ticket;
    }

    if (server_address.port != 0) {
#endif

        STimeout remaining_timeout;

        do {
            io_st = conn->m_Socket.Connect(CSocketAPI::ntoa(
                    server_address.host), server_address.port,
                    &internal_timeout, fSOCK_LogOff | fSOCK_KeepAlive);

            deadline.GetRemainingTime().Get(&remaining_timeout.sec,
                                            &remaining_timeout.usec);

            if (remaining_timeout < internal_timeout)
                internal_timeout = remaining_timeout;

        } while (io_st == eIO_Timeout && (remaining_timeout.usec > 0 ||
                                          remaining_timeout.sec > 0));

        if (io_st != eIO_Success) {
            conn->m_Socket.Close();

            string message = "Could not connect to " +
                m_ServerInPool->m_Address.AsString();
            message += ": ";
            message += IO_StatusStr(io_st);

            NCBI_THROW(CNetSrvConnException, eConnectionFailure, message);
        }

#ifdef NCBI_GRID_XSITE_CONN_SUPPORT
    }

    if (ticket != 0 &&
            conn->m_Socket.Write(&ticket, sizeof(ticket)) != eIO_Success) {
        NCBI_THROW(CNetSrvConnException, eConnectionFailure,
                "Error while sending proxy auth ticket.");
    }
#endif

    m_ServerInPool->AdjustThrottlingParameters(SNetServerInPool::eCOR_Success);

    //LOG_POST("comm timeout: "<<*(timeout != NULL ? timeout: &m_ServerInPool->m_ServerPool->m_CommTimeout));
    conn->m_Socket.SetDataLogging(
            TServConn_ConnDataLogging::GetDefault() ? eOn : eOff);
    conn->m_Socket.SetTimeout(eIO_ReadWrite, timeout != NULL ? timeout :
                              &m_ServerInPool->m_ServerPool->m_CommTimeout);
    conn->m_Socket.DisableOSSendDelay();
    conn->m_Socket.SetReuseAddress(eOn);

    (conn_listener != NULL ? conn_listener :
            m_Service->m_Listener)->OnConnected(conn);

    if (timeout != NULL)
        conn->m_Socket.SetTimeout(eIO_ReadWrite,
                &m_ServerInPool->m_ServerPool->m_CommTimeout);

    return conn;
}

void SNetServerImpl::TryExec(INetServerExecHandler& handler,
        STimeout* timeout, INetServerConnectionListener* conn_listener)
{
    m_ServerInPool->CheckIfThrottled();

    CNetServerConnection conn;

    // Silently reconnect if the connection was taken
    // from the pool and it was closed by the server
    // due to inactivity.
    while ((conn = GetConnectionFromPool()) != NULL) {
        try {
            handler.Exec(conn, timeout);
            return;
        }
        catch (CNetSrvConnException& e) {
            CException::TErrCode err_code = e.GetErrCode();
            if (err_code != CNetSrvConnException::eWriteFailure &&
                err_code != CNetSrvConnException::eConnClosedByServer)
            {
                m_ServerInPool->AdjustThrottlingParameters(
                        SNetServerInPool::eCOR_Failure);
                throw;
            }
        }
    }

    try {
        handler.Exec(Connect(timeout, conn_listener), timeout);
    }
    catch (CNetSrvConnException&) {
        m_ServerInPool->AdjustThrottlingParameters(
                SNetServerInPool::eCOR_Failure);
        throw;
    }
}

class CNetServerExecHandler : public INetServerExecHandler
{
public:
    CNetServerExecHandler(const string& cmd,
            bool multiline_output,
            CNetServer::SExecResult& exec_result,
            INetServerExecListener* exec_listener,
            INetServerConnectionListener* conn_listener) :
        m_Cmd(cmd),
        m_MultilineOutput(multiline_output),
        m_ExecResult(exec_result),
        m_ExecListener(exec_listener),
        m_ConnListener(conn_listener)
    {
    }

    virtual void Exec(CNetServerConnection::TInstance conn_impl,
            STimeout* timeout);

    string m_Cmd;
    bool m_MultilineOutput;
    CNetServer::SExecResult& m_ExecResult;
    INetServerExecListener* m_ExecListener;
    INetServerConnectionListener* m_ConnListener;
};

void CNetServerExecHandler::Exec(CNetServerConnection::TInstance conn_impl,
        STimeout* timeout)
{
    m_ExecResult.conn = conn_impl;

    if (m_ExecListener != NULL)
        m_ExecListener->OnExec(m_ExecResult.conn, m_Cmd);

    m_ExecResult.response = m_ExecResult.conn.Exec(m_Cmd,
            m_MultilineOutput,
            timeout, m_ConnListener);
}

void SNetServerImpl::ConnectAndExec(const string& cmd,
        bool multiline_output,
        CNetServer::SExecResult& exec_result, STimeout* timeout,
        INetServerExecListener* exec_listener,
        INetServerConnectionListener* conn_listener)
{
    if (conn_listener == NULL)
        conn_listener = m_Service->m_Listener;

    CNetServerExecHandler exec_handler(cmd, multiline_output,
            exec_result, exec_listener, conn_listener);

    TryExec(exec_handler, timeout, conn_listener);
}

void SNetServerInPool::AdjustThrottlingParameters(EConnOpResult op_result)
{
    if (m_ServerPool->m_ServerThrottlePeriod <= 0)
        return;

    CFastMutexGuard guard(m_ThrottleLock);

    if (m_ServerPool->m_MaxConsecutiveIOFailures > 0) {
        if (op_result != eCOR_Success)
            ++m_NumberOfConsecutiveIOFailures;
        else if (m_NumberOfConsecutiveIOFailures <
                m_ServerPool->m_MaxConsecutiveIOFailures)
            m_NumberOfConsecutiveIOFailures = 0;
    }

    if (m_ServerPool->m_IOFailureThresholdNumerator > 0) {
        if (m_IOFailureRegister[m_IOFailureRegisterIndex] != op_result) {
            if ((m_IOFailureRegister[m_IOFailureRegisterIndex] =
                    op_result) == eCOR_Success)
                --m_IOFailureCounter;
            else
                ++m_IOFailureCounter;
        }

        if (++m_IOFailureRegisterIndex >=
                m_ServerPool->m_IOFailureThresholdDenominator)
            m_IOFailureRegisterIndex = 0;
    }
}

void SNetServerInPool::CheckIfThrottled()
{
    if (m_ServerPool->m_ServerThrottlePeriod <= 0)
        return;

    CFastMutexGuard guard(m_ThrottleLock);

    if (m_Throttled) {
        CTime current_time(GetFastLocalTime());
        if (current_time >= m_ThrottledUntil &&
                (!m_ServerPool->m_ThrottleUntilDiscoverable ||
                        m_DiscoveredAfterThrottling)) {
            ResetThrottlingParameters();
            return;
        }
        NCBI_THROW(CNetSrvConnException, eServerThrottle, m_ThrottleMessage);
    }

    if (m_ServerPool->m_MaxConsecutiveIOFailures > 0 &&
        m_NumberOfConsecutiveIOFailures >=
            m_ServerPool->m_MaxConsecutiveIOFailures) {
        m_Throttled = true;
        m_DiscoveredAfterThrottling = false;
        m_ThrottleMessage = "Server " + m_Address.AsString();
        m_ThrottleMessage += " has reached the maximum number "
            "of connection failures in a row";
    }

    if (m_ServerPool->m_IOFailureThresholdNumerator > 0 &&
            m_IOFailureCounter >= m_ServerPool->m_IOFailureThresholdNumerator) {
        m_Throttled = true;
        m_DiscoveredAfterThrottling = false;
        m_ThrottleMessage = "Connection to server " + m_Address.AsString();
        m_ThrottleMessage += " aborted as it is considered bad/overloaded";
    }

    if (m_Throttled) {
        m_ThrottledUntil.SetCurrent();
        m_ThrottledUntil.AddSecond(m_ServerPool->m_ServerThrottlePeriod);
        NCBI_THROW(CNetSrvConnException, eServerThrottle, m_ThrottleMessage);
    }
}

void SNetServerInPool::ResetThrottlingParameters()
{
    m_NumberOfConsecutiveIOFailures = 0;
    memset(m_IOFailureRegister, 0, sizeof(m_IOFailureRegister));
    m_IOFailureCounter = m_IOFailureRegisterIndex = 0;
    m_Throttled = false;
}

CNetServer::SExecResult CNetServer::ExecWithRetry(const string& cmd,
        bool multiline_output,
        INetServerConnectionListener* conn_listener)
{
    CNetServer::SExecResult exec_result;

    CTime max_connection_time(GetFastLocalTime());
    max_connection_time.AddNanoSecond(
        m_Impl->m_ServerInPool->m_ServerPool->m_MaxConnectionTime * 1000000);

    unsigned attempt = 0;

    if (conn_listener == NULL)
        conn_listener = m_Impl->m_Service->m_Listener;

    for (;;) {
        string warning;

        try {
            m_Impl->ConnectAndExec(cmd, multiline_output,
                    exec_result, NULL, NULL, conn_listener);
            return exec_result;
        }
        catch (CNetSrvConnException& e) {
            if (++attempt > TServConn_ConnMaxRetries::GetDefault() ||
                    e.GetErrCode() == CNetSrvConnException::eServerThrottle)
                throw;

            if (m_Impl->m_ServerInPool->m_ServerPool->m_MaxConnectionTime > 0 &&
                    max_connection_time <= GetFastLocalTime()) {
                LOG_POST(Error << "Timeout (max_connection_time=" <<
                    m_Impl->m_ServerInPool->m_ServerPool->m_MaxConnectionTime <<
                    "); cmd=" << cmd <<
                    "; exception=" << e.GetMsg());
                throw;
            }

            warning = e.GetMsg();
        }
        catch (CNetScheduleException& e) {
            if (++attempt > TServConn_ConnMaxRetries::GetDefault() ||
                    e.GetErrCode() != CNetScheduleException::eTryAgain)
                throw;

            if (m_Impl->m_ServerInPool->m_ServerPool->m_MaxConnectionTime > 0 &&
                    max_connection_time <= GetFastLocalTime()) {
                LOG_POST(Error << "Timeout (max_connection_time=" <<
                    m_Impl->m_ServerInPool->m_ServerPool->m_MaxConnectionTime <<
                    "); cmd=" << cmd <<
                    "; exception=" << e.GetMsg());
                throw;
            }

            warning = e.GetMsg();
        }

        warning += ", reconnecting: attempt ";
        warning += NStr::NumericToString(attempt);
        warning += " of ";
        warning += NStr::NumericToString(
                TServConn_ConnMaxRetries::GetDefault());

        conn_listener->OnWarning(warning, *this);

        SleepMilliSec(s_GetRetryDelay());
    }
}

CNetServerInfo CNetServer::GetServerInfo()
{
    string response(ExecWithRetry("VERSION", false).response);

    // Keep these two lines separate to let the Exec method
    // above succeed before constructing the object below.

    return new SNetServerInfoImpl(response);
}

CNetServerInfo g_ServerInfoFromString(const string& server_info)
{
    return new SNetServerInfoImpl(server_info);
}

END_NCBI_SCOPE
