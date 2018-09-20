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

#include <corelib/ncbi_system.hpp>

#include <sstream>

#ifdef NCBI_OS_LINUX
# include <sys/socket.h>
# include <netinet/in.h>
# include <netinet/tcp.h>
#endif


#define NCBI_USE_ERRCODE_X   ConnServ_Connection

#define END_OF_MULTILINE_OUTPUT "END"

BEGIN_NCBI_SCOPE

static const STimeout s_ZeroTimeout = {0, 0};

///////////////////////////////////////////////////////////////////////////
SNetServerMultilineCmdOutputImpl::~SNetServerMultilineCmdOutputImpl()
{
    if (!m_ReadCompletely)
        m_Connection->Close();
}

bool SNetServerMultilineCmdOutputImpl::ReadLine(string& output)
{
    _ASSERT(!m_ReadCompletely);

    if (!m_FirstLineConsumed) {
        output = m_FirstOutputLine;
        m_FirstOutputLine = kEmptyStr;
        m_FirstLineConsumed = true;
    } else if (!m_NetCacheCompatMode)
        m_Connection->ReadCmdOutputLine(output, true);
    else {
        try {
            m_Connection->ReadCmdOutputLine(output, true);
        }
        catch (CNetSrvConnException& e) {
            if (e.GetErrCode() != CNetSrvConnException::eConnClosedByServer)
                throw;

            m_ReadCompletely = true;
            return false;
        }
    }

    if (output != END_OF_MULTILINE_OUTPUT)
        return true;
    else {
        m_ReadCompletely = true;
        return false;
    }
}

bool CNetServerMultilineCmdOutput::ReadLine(string& output)
{
    return m_Impl->ReadLine(output);
}

INetServerConnectionListener::TPropCreator INetServerConnectionListener::GetPropCreator() const
{
    return [] { return new INetServerProperties; };
}

void INetServerConnectionListener::OnError(const string& err_msg, CNetServer& server)
{
    if (m_ErrorHandler && m_ErrorHandler(err_msg, server)) return;

    OnErrorImpl(err_msg, server);
}

void INetServerConnectionListener::OnWarning(const string& warn_msg, CNetServer& server)
{
    if (m_WarningHandler && m_WarningHandler(warn_msg, server)) return;

    OnWarningImpl(warn_msg, server);
}

INetServerConnectionListener::INetServerConnectionListener(const INetServerConnectionListener&)
{
    // No m_ErrorHandler/m_WarningHandler sharing
}

INetServerConnectionListener& INetServerConnectionListener::operator=(const INetServerConnectionListener&)
{
    // No m_ErrorHandler/m_WarningHandler sharing
    return *this;
}

void INetServerConnectionListener::SetErrorHandler(TEventHandler error_handler)
{
    // Event handlers are not allowed to be changed, only to be set or reset
    _ASSERT(!(m_ErrorHandler && error_handler));
    m_ErrorHandler = error_handler;
}

void INetServerConnectionListener::SetWarningHandler(TEventHandler warning_handler)
{
    // Event handlers are not allowed to be changed, only to be set or reset
    _ASSERT(!(m_WarningHandler && warning_handler));
    m_WarningHandler = warning_handler;
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

        int upper_limit = TServConn_MaxConnPoolSize::GetDefault();

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
        bool multiline_output)
{
    switch (m_Socket.ReadLine(result))
    {
    case eIO_Success:
        break;
    case eIO_Timeout:
        Abort();
        CONNSERV_THROW_FMT(CNetSrvConnException, eReadTimeout, m_Server,
                "Communication timeout while reading"
                " (timeout=" << NcbiTimeoutToMs(
                        m_Socket.GetTimeout(eIO_Read)) / 1000.0l << "s)");
        break;
    case eIO_Closed:
        Abort();
        CONNSERV_THROW_FMT(CNetSrvConnException, eConnClosedByServer, m_Server,
                "Connection closed");
        break;
    default: // invalid socket or request, bailing out
        Abort();
        CONNSERV_THROW_FMT(CNetSrvConnException, eCommunicationError, m_Server,
                "Communication error while reading");
    }

    auto& conn_listener = m_Server->m_Service->m_Listener;

    if (NStr::StartsWith(result, "OK:")) {
        const char* reply = result.c_str() + STRING_LEN("OK:");
        size_t reply_len = result.length() - STRING_LEN("OK:");
        while (reply_len >= WARNING_PREFIX_LEN &&
                memcmp(reply, WARNING_PREFIX, WARNING_PREFIX_LEN) == 0) {
            reply += WARNING_PREFIX_LEN;
            reply_len -= WARNING_PREFIX_LEN;
            const char* semicolon = strchr(reply, ';');
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
        conn_listener->OnError(result, m_Server);
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

            CONNSERV_THROW_FMT(CNetSrvConnException, eWriteFailure,
                m_Server, "Failed to write: " << IO_StatusStr(io_st));
        }
        len -= n_written;
        buf += n_written;
    }
}

string CNetServerConnection::Exec(const string& cmd,
        bool multiline_output,
        STimeout* timeout)
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

    m_Impl->ReadCmdOutputLine(output, multiline_output);

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

    m_ThrottleStats.ResetThrottlingParameters();

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

bool SNetServerInfoImpl::GetNextAttribute(string& attr_name, string& attr_value)
{
    if (m_NextAttribute == m_Attributes->end())
        return false;

    attr_name = m_NextAttribute->name;
    attr_value = m_NextAttribute->value;
    ++m_NextAttribute;
    return true;
}

bool CNetServerInfo::GetNextAttribute(string& attr_name, string& attr_value)
{
    return m_Impl->GetNextAttribute(attr_name, attr_value);
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


CNetServerConnection SNetServerInPool::GetConnectionFromPool(SNetServerImpl* server)
{
    for (;;) {
        TFastMutexGuard guard(m_FreeConnectionListLock);

        if (m_FreeConnectionListSize == 0)
            return NULL;

        // Get an existing connection object from the connection pool.
        CNetServerConnection conn = m_FreeConnectionListHead;

        m_FreeConnectionListHead = conn->m_NextFree;
        --m_FreeConnectionListSize;
        conn->m_Server = server;

        guard.Release();

        // Check if the socket is already connected.
        if (conn->m_Socket.GetStatus(eIO_Open) != eIO_Success ||
                conn->m_Generation !=
                        m_CurrentConnectionGeneration.Get())
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

struct SNetServerImpl::SConnectDeadline
{
    SConnectDeadline(const STimeout& conn_timeout) :
        try_timeout(Min(conn_timeout , kMaxTryTimeout)),
        total_timeout(conn_timeout.sec, conn_timeout.usec),
        deadline(total_timeout)
    {}

    const STimeout* GetRemaining() const { return &try_timeout; }

    bool IsExpired()
    {
        CNanoTimeout remaining(deadline.GetRemainingTime());

        if (remaining.IsZero()) return true;

        remaining.Get(&try_timeout.sec, &try_timeout.usec);
        try_timeout = Min(try_timeout, kMaxTryTimeout);
        return false;
    }

    CTimeout GetTotal() const { return total_timeout; }

private:
    static STimeout Min(const STimeout& t1, const STimeout& t2)
    {
        if (t1.sec  < t2.sec)  return t1;
        if (t1.sec  > t2.sec)  return t2;
        if (t1.usec < t2.usec) return t1;
        return t2;
    }

    STimeout try_timeout;
    CTimeout total_timeout;
    CDeadline deadline;

    static const STimeout kMaxTryTimeout;
};

#ifndef NCBI_OS_MSWIN
const STimeout SNetServerImpl::SConnectDeadline::kMaxTryTimeout = {0, 250 * 1000};
#else
const STimeout SNetServerImpl::SConnectDeadline::kMaxTryTimeout = {1, 250 * 1000};
#endif

CNetServerConnection SNetServerInPool::Connect(SNetServerImpl* server, STimeout* timeout)
{
    CNetServerConnection conn = new SNetServerConnectionImpl(server);

    SNetServerImpl::SConnectDeadline deadline(timeout ? *timeout : m_ServerPool->m_ConnTimeout);
    auto& socket = conn->m_Socket;

    SNetServiceXSiteAPI::ConnectXSite(socket, deadline, m_Address, server->m_Service->m_ServiceName);

    AdjustThrottlingParameters(SThrottleStats::eCOR_Success);

    socket.SetDataLogging(TServConn_ConnDataLogging::GetDefault() ? eOn : eOff);
    socket.SetTimeout(eIO_ReadWrite, timeout ? timeout :
                              &m_ServerPool->m_CommTimeout);
    socket.DisableOSSendDelay();
    socket.SetReuseAddress(eOn);

    server->m_Service->m_Listener->OnConnected(conn);

    if (timeout) socket.SetTimeout(eIO_ReadWrite, &m_ServerPool->m_CommTimeout);

    return conn;
}

void SNetServerImpl::ConnectImpl(CSocket& socket, SConnectDeadline& deadline,
        const SServerAddress& actual, const SServerAddress& original)
{
    EIO_Status io_st;

    do {
        io_st = socket.Connect(CSocketAPI::ntoa(actual.host), actual.port,
                deadline.GetRemaining(), fSOCK_LogOff | fSOCK_KeepAlive);

    } while (io_st == eIO_Timeout && !deadline.IsExpired());

    if (io_st == eIO_Success) return;

    socket.Close();

    ostringstream os;
    os << original.AsString() << ": Could not connect: " << IO_StatusStr(io_st);

    if (io_st == eIO_Timeout) os << " (" << deadline.GetTotal().GetAsDouble() << "s)";

    NCBI_THROW(CNetSrvConnException, eConnectionFailure, os.str());
}

void SNetServerInPool::TryExec(SNetServerImpl* server, INetServerExecHandler& handler,
        STimeout* timeout)
{
    CheckIfThrottled();

    CNetServerConnection conn;

    // Silently reconnect if the connection was taken
    // from the pool and it was closed by the server
    // due to inactivity.
    while ((conn = GetConnectionFromPool(server)) != NULL) {
        try {
            handler.Exec(conn, timeout);
            return;
        }
        catch (CNetSrvConnException& e) {
            CException::TErrCode err_code = e.GetErrCode();
            if (err_code != CNetSrvConnException::eWriteFailure &&
                err_code != CNetSrvConnException::eConnClosedByServer)
            {
                AdjustThrottlingParameters(SThrottleStats::eCOR_Failure);
                throw;
            }
        }
    }

    try {
        handler.Exec(Connect(server, timeout), timeout);
    }
    catch (CNetSrvConnException&) {
        AdjustThrottlingParameters(SThrottleStats::eCOR_Failure);
        throw;
    }
}

void SNetServerImpl::TryExec(INetServerExecHandler& handler, STimeout* timeout)
{
    return m_ServerInPool->TryExec(this, handler, timeout);
}

class CNetServerExecHandler : public INetServerExecHandler
{
public:
    CNetServerExecHandler(const string& cmd,
            bool multiline_output,
            CNetServer::SExecResult& exec_result,
            INetServerExecListener* exec_listener) :
        m_Cmd(cmd),
        m_MultilineOutput(multiline_output),
        m_ExecResult(exec_result),
        m_ExecListener(exec_listener)
    {
    }

    virtual void Exec(CNetServerConnection::TInstance conn_impl,
            STimeout* timeout);

    string m_Cmd;
    bool m_MultilineOutput;
    CNetServer::SExecResult& m_ExecResult;
    INetServerExecListener* m_ExecListener;
};

void CNetServerExecHandler::Exec(CNetServerConnection::TInstance conn_impl,
        STimeout* timeout)
{
    m_ExecResult.conn = conn_impl;

    if (m_ExecListener != NULL)
        m_ExecListener->OnExec(m_ExecResult.conn, m_Cmd);

    m_ExecResult.response = m_ExecResult.conn.Exec(m_Cmd,
            m_MultilineOutput,
            timeout);
}

void SNetServerImpl::ConnectAndExec(const string& cmd,
        bool multiline_output,
        CNetServer::SExecResult& exec_result, STimeout* timeout,
        INetServerExecListener* exec_listener)
{
    CNetServerExecHandler exec_handler(cmd, multiline_output,
            exec_result, exec_listener);

    TryExec(exec_handler, timeout);
}

void SNetServerInPool::AdjustThrottlingParameters(SThrottleStats::EConnOpResult op_result)
{
    const auto& params = m_ServerPool->GetThrottleParams();

    if (params.m_ServerThrottlePeriod <= 0)
        return;

    m_ThrottleStats.AdjustThrottlingParameters(params, op_result);
}

void SThrottleStats::AdjustThrottlingParameters(const SThrottleParams& params, EConnOpResult op_result)
{
    CFastMutexGuard guard(m_ThrottleLock);

    if (params.m_MaxConsecutiveIOFailures > 0) {
        if (op_result != eCOR_Success)
            ++m_NumberOfConsecutiveIOFailures;
        else if (m_NumberOfConsecutiveIOFailures < params.m_MaxConsecutiveIOFailures)
            m_NumberOfConsecutiveIOFailures = 0;
    }

    if (params.m_IOFailureThresholdNumerator > 0) {
        if (m_IOFailureRegister[m_IOFailureRegisterIndex] != op_result) {
            if ((m_IOFailureRegister[m_IOFailureRegisterIndex] =
                    op_result) == eCOR_Success)
                --m_IOFailureCounter;
            else
                ++m_IOFailureCounter;
        }

        if (++m_IOFailureRegisterIndex >= params.m_IOFailureThresholdDenominator)
            m_IOFailureRegisterIndex = 0;
    }
}

void SNetServerInPool::CheckIfThrottled()
{
    const auto& params = m_ServerPool->GetThrottleParams();

    if (params.m_ServerThrottlePeriod <= 0)
        return;

    m_ThrottleStats.CheckIfThrottled(params, m_Address);
}

void SThrottleStats::CheckIfThrottled(const SThrottleParams& params, const SServerAddress& address)
{
    CFastMutexGuard guard(m_ThrottleLock);

    if (m_Throttled) {
        CTime current_time(GetFastLocalTime());
        auto duration = current_time - m_ThrottledUntil;

        if ((duration >= CTimeSpan(0, 0)) &&
                (!params.m_ThrottleUntilDiscoverable || m_DiscoveredAfterThrottling)) {
            duration += CTimeSpan(params.m_ServerThrottlePeriod, 0);
            ResetThrottlingParameters();
            LOG_POST(Warning << "Disabling throttling for server " << address.AsString() <<
                    " after " << duration.AsString() << " seconds wait" << 
                    (params.m_ThrottleUntilDiscoverable ? " and rediscovery" : ""));
            return;
        }
        NCBI_THROW(CNetSrvConnException, eServerThrottle, m_ThrottleMessage);
    }

    if (params.m_MaxConsecutiveIOFailures > 0 &&
        m_NumberOfConsecutiveIOFailures >= params.m_MaxConsecutiveIOFailures) {
        m_Throttled = true;
        m_DiscoveredAfterThrottling = false;
        m_ThrottleMessage = "Server " + address.AsString();
        m_ThrottleMessage += " has reached the maximum number "
            "of connection failures in a row";
    }

    if (params.m_IOFailureThresholdNumerator > 0 &&
            m_IOFailureCounter >= params.m_IOFailureThresholdNumerator) {
        m_Throttled = true;
        m_DiscoveredAfterThrottling = false;
        m_ThrottleMessage = "Connection to server " + address.AsString();
        m_ThrottleMessage += " aborted as it is considered bad/overloaded";
    }

    if (m_Throttled) {
        m_ThrottledUntil.SetCurrent();
        m_ThrottledUntil.AddSecond(params.m_ServerThrottlePeriod);
        NCBI_THROW(CNetSrvConnException, eServerThrottle, m_ThrottleMessage);
    }
}

void SThrottleStats::ResetThrottlingParameters()
{
    m_NumberOfConsecutiveIOFailures = 0;
    memset(m_IOFailureRegister, 0, sizeof(m_IOFailureRegister));
    m_IOFailureCounter = m_IOFailureRegisterIndex = 0;
    m_Throttled = false;
}

void SThrottleStats::DiscoveredAfterThrottling()
{
    CFastMutexGuard guard(m_ThrottleLock);
    m_DiscoveredAfterThrottling = true;
}

CNetServer::SExecResult CNetServer::ExecWithRetry(const string& cmd,
        bool multiline_output)
{
    CNetServer::SExecResult exec_result;

    const CTimeout& max_total_time = m_Impl->m_ServerInPool->m_ServerPool->m_MaxTotalTime;
    CDeadline deadline(max_total_time);

    unsigned attempt = 0;
    auto& service = m_Impl->m_Service;

    const auto max_retries = service->GetConnectionMaxRetries();
    const auto retry_delay = service->GetConnectionRetryDelay();

    for (;;) {
        string warning;

        try {
            m_Impl->ConnectAndExec(cmd, multiline_output,
                    exec_result);
            return exec_result;
        }
        catch (CNetSrvConnException& e) {
            if (++attempt > max_retries ||
                    e.GetErrCode() == CNetSrvConnException::eServerThrottle)
                throw;

            if (deadline.IsExpired()) {
                LOG_POST(Error << "Timeout (max_connection_time=" <<
                    max_total_time.GetAsMilliSeconds() << "); cmd=" << cmd << "; exception=" << e.GetMsg());
                throw;
            }

            warning = e.GetMsg();
        }
        catch (CNetScheduleException& e) {
            if (++attempt > max_retries ||
                    e.GetErrCode() != CNetScheduleException::eTryAgain)
                throw;

            if (deadline.IsExpired()) {
                LOG_POST(Error << "Timeout (max_connection_time=" <<
                    max_total_time.GetAsMilliSeconds() << "); cmd=" << cmd << "; exception=" << e.GetMsg());
                throw;
            }

            warning = e.GetMsg();
        }

        warning += ", reconnecting: attempt ";
        warning += NStr::NumericToString(attempt);
        warning += " of ";
        warning += NStr::NumericToString(max_retries);

        service->m_Listener->OnWarning(warning, *this);

        SleepMilliSec(retry_delay);
    }
}

CNetServerInfo CNetServer::GetServerInfo()
{
    string cmd("VERSION");
    g_AppendClientIPSessionIDHitID(cmd);
    auto response = ExecWithRetry(cmd, false).response;

    // Keep these two lines separate to let the Exec method
    // above succeed before constructing the object below.

    return new SNetServerInfoImpl(response);
}

CNetServerInfo g_ServerInfoFromString(const string& server_info)
{
    return new SNetServerInfoImpl(server_info);
}

void SThrottleParams::Init(CSynRegistry& registry, const SRegSynonyms& sections)
{
    m_ServerThrottlePeriod = registry.Get(sections, "throttle_relaxation_period", 0);

    if (m_ServerThrottlePeriod <= 0) return;

    m_MaxConsecutiveIOFailures = registry.Get(sections,
            { "throttle_by_consecutive_connection_failures", "throttle_by_subsequent_connection_failures" }, 0);

    m_ThrottleUntilDiscoverable = registry.Get(sections, "throttle_hold_until_active_in_lb", false);

    // These values must correspond to each other
    const auto default_error_rate = "0/1";
    m_IOFailureThresholdNumerator = 0;
    m_IOFailureThresholdDenominator = 1;

    const string error_rate = registry.Get(sections, "throttle_by_connection_error_rate", default_error_rate);

    if (error_rate == default_error_rate || error_rate.empty()) return;

    string numerator_str, denominator_str;

    if (!NStr::SplitInTwo(error_rate, "/", numerator_str, denominator_str)) return;

    const auto flags = NStr::fConvErr_NoThrow | NStr::fAllowLeadingSpaces | NStr::fAllowTrailingSpaces;

    int numerator = NStr::StringToInt(numerator_str, flags);
    int denominator = NStr::StringToInt(denominator_str, flags);

    if (numerator < 0) numerator = 0;

    if (denominator < 1) {
        denominator = 1;

    } else if (denominator > CONNECTION_ERROR_HISTORY_MAX) {
        numerator = (numerator * CONNECTION_ERROR_HISTORY_MAX) / denominator;
        denominator = CONNECTION_ERROR_HISTORY_MAX;
    }

    m_IOFailureThresholdNumerator = numerator;
    m_IOFailureThresholdDenominator = denominator;
}

SServerAddress::SServerAddress(unsigned h, unsigned short p) :
    host(h),
    port(p),
    name(host, {})
{
}

SServerAddress::SServerAddress(string n, unsigned short p) :
    host(g_NetService_gethostbyname(n)),
    port(p),
    name(host, move(n))
{
}

bool operator==(const SServerAddress& lhs, const SServerAddress& rhs)
{
    return lhs.host == rhs.host && lhs.port == rhs.port;
}

bool operator< (const SServerAddress& lhs, const SServerAddress& rhs)
{
    if (lhs.host != rhs.host) return lhs.host < rhs.host;

    return lhs.port < rhs.port;
}

string SServerAddress::AsString() const
{
    // Name was not looked up yet or host changed
    if (name.second.empty() || name.first != host) {
        name.first = host;
        name.second = g_NetService_gethostnamebyaddr(host);
    }

    return name.second + ':' + NStr::UIntToString(port);
}

END_NCBI_SCOPE
