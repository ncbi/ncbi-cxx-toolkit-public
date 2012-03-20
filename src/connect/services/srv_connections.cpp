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

#include <connect/ncbi_conn_exception.hpp>

#include <corelib/ncbi_system.hpp>

#ifdef NCBI_OS_LINUX
# include <sys/socket.h>
# include <netinet/in.h>
# include <netinet/tcp.h>
#endif


#define NCBI_USE_ERRCODE_X   ConnServ_Connection


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
    if (!m_ReadCompletely) {
        m_Connection->Close();
        ERR_POST_X(7, "Multiline command output was not completely read");
    }
}

bool CNetServerMultilineCmdOutput::ReadLine(string& output)
{
    _ASSERT(!m_Impl->m_ReadCompletely);

    if (!m_Impl->m_FirstLineConsumed) {
        output = m_Impl->m_FirstOutputLine;
        m_Impl->m_FirstOutputLine = kEmptyStr;
        m_Impl->m_FirstLineConsumed = true;
    } else if (!m_Impl->m_NetCacheCompatMode)
        m_Impl->m_Connection->ReadCmdOutputLine(output);
    else {
        try {
            m_Impl->m_Connection->ReadCmdOutputLine(output);
        }
        catch (CNetSrvConnException& e) {
            if (e.GetErrCode() != CNetSrvConnException::eConnClosedByServer)
                throw;

            m_Impl->m_ReadCompletely = true;
            return false;
        }
    }

    if (output != "END")
        return true;
    else {
        m_Impl->m_ReadCompletely = true;
        return false;
    }
}

inline SNetServerConnectionImpl::SNetServerConnectionImpl(
        SNetServerImpl* server) :
    m_Server(server),
    m_NextFree(NULL)
{
    if (TServConn_UserLinger2::GetDefault())
        m_Socket.SetTimeout(eIO_Close, &s_ZeroTimeout);
}

void SNetServerConnectionImpl::DeleteThis()
{
    // Return this connection to the pool.
    if (m_Server->m_ServerInPool->m_ServerPool->m_PermanentConnection != eOff &&
            m_Socket.GetStatus(eIO_Open) == eIO_Success) {
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

void SNetServerConnectionImpl::ReadCmdOutputLine(string& result)
{
    switch (m_Socket.ReadLine(result))
    {
    case eIO_Success:
        break;
    case eIO_Timeout:
        Abort();
        NCBI_THROW_FMT(CNetSrvConnException, eReadTimeout,
                "Communication timeout reading from " <<
                m_Server->m_ServerInPool->m_Address.AsString());
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
        result.erase(0, sizeof("OK:") - 1);
        if (NStr::StartsWith(result, "WARNING:")) {
            string::size_type semicolon_pos =
                result.find(';', sizeof("WARNING:") - 1);
            if (semicolon_pos != string::npos) {
                m_Server->m_Service->m_Listener->OnWarning(string(
                        result.begin() + sizeof("WARNING:") - 1,
                        result.begin() + semicolon_pos), m_Server);
                result.erase(0, semicolon_pos + 1);
            } else {
                m_Server->m_Service->m_Listener->OnWarning(string(
                        result.begin() + sizeof("WARNING:") - 1,
                        result.end()), m_Server);
                result.clear();
            }
        }
    } else if (NStr::StartsWith(result, "ERR:")) {
        result.erase(0, sizeof("ERR:") - 1);
        result = NStr::ParseEscapes(result);
        m_Server->m_Service->m_Listener->OnError(result, m_Server);
        result = kEmptyStr;
    }
}


void SNetServerConnectionImpl::Close()
{
    static const STimeout zero_timeout = {0, 0};

    m_Socket.SetTimeout(eIO_Close, &zero_timeout);

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

            CIO_Exception io_ex(DIAG_COMPILE_INFO, 0,
                (CIO_Exception::EErrCode) io_st, "IO error.");

            NCBI_THROW_FMT(CNetSrvConnException, eWriteFailure,
                "Failed to write to " <<
                m_Server->m_ServerInPool->m_Address.AsString() << ": " <<
                string(io_ex.what()));
        }
        len -= n_written;
        buf += n_written;
    }
}

string CNetServerConnection::Exec(const string& cmd)
{
    m_Impl->WriteLine(cmd);

    m_Impl->m_Socket.SetCork(false);
#ifdef NCBI_OS_LINUX
    int fd = 0, val = 1;
    m_Impl->m_Socket.GetOSHandle(&fd, sizeof(fd));
    setsockopt(fd, IPPROTO_TCP, TCP_QUICKACK, &val, sizeof(val));
#endif

    string output;
    m_Impl->ReadCmdOutputLine(output);

    return output;
}

CNetServerMultilineCmdOutput::CNetServerMultilineCmdOutput(
    const CNetServer::SExecResult& exec_result) :
        m_Impl(new SNetServerMultilineCmdOutputImpl(exec_result))
{
}

/*************************************************************************/
SNetServerInPool::SNetServerInPool(const string& host, unsigned short port) :
    m_Address(host, port)
{
    m_FreeConnectionListHead = NULL;
    m_FreeConnectionListSize = 0;

    ResetThrottlingParameters();
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

string CNetServer::GetHost() const
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
        if (conn->m_Socket.GetStatus(eIO_Open) != eIO_Success)
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

CNetServerConnection SNetServerImpl::Connect()
{
    CNetServerConnection conn = new SNetServerConnectionImpl(this);

    EIO_Status io_st;
    STimeout internal_timeout = s_InternalConnectTimeout;
    STimeout remaining_timeout = m_ServerInPool->m_ServerPool->m_ConnTimeout;

    do {
        if (remaining_timeout.sec == s_InternalConnectTimeout.sec ?
                remaining_timeout.usec > s_InternalConnectTimeout.usec :
                remaining_timeout.sec > s_InternalConnectTimeout.sec) {
            remaining_timeout.sec -= s_InternalConnectTimeout.sec;
            if (remaining_timeout.usec < s_InternalConnectTimeout.usec) {
                --remaining_timeout.sec;
                remaining_timeout.usec += 1000 * 1000;
            }
            remaining_timeout.usec -= s_InternalConnectTimeout.usec;
        } else {
            internal_timeout = remaining_timeout;
            remaining_timeout.sec = remaining_timeout.usec = 0;
        }

        io_st = conn->m_Socket.Connect(
                m_ServerInPool->m_Address.host, m_ServerInPool->m_Address.port,
                &internal_timeout, fSOCK_LogOff | fSOCK_KeepAlive);
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

    m_ServerInPool->AdjustThrottlingParameters(SNetServerInPool::eCOR_Success);

    conn->m_Socket.SetDataLogging(eOff);
    conn->m_Socket.SetTimeout(eIO_ReadWrite,
            &m_ServerInPool->m_ServerPool->m_CommTimeout);
    conn->m_Socket.DisableOSSendDelay();
    conn->m_Socket.SetReuseAddress(eOn);

    m_Service->m_Listener->OnConnected(conn);

    return conn;
}

void SNetServerImpl::ConnectAndExec(const string& cmd,
    CNetServer::SExecResult& exec_result)
{
    m_ServerInPool->CheckIfThrottled();

    // Silently reconnect if the connection was taken
    // from the pool and it was closed by the server
    // due to inactivity.
    while ((exec_result.conn = GetConnectionFromPool()) != NULL) {
        try {
            exec_result.response = exec_result.conn.Exec(cmd);
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
        exec_result.conn = Connect();
        exec_result.response = exec_result.conn.Exec(cmd);
    }
    catch (CNetSrvConnException&) {
        m_ServerInPool->AdjustThrottlingParameters(
                SNetServerInPool::eCOR_Failure);
        throw;
    }
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
        NCBI_THROW(CNetSrvConnException, eServerThrottle,
            m_ThrottleMessage);
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
        NCBI_THROW(CNetSrvConnException, eServerThrottle,
            m_ThrottleMessage);
    }
}

void SNetServerInPool::ResetThrottlingParameters()
{
    m_NumberOfConsecutiveIOFailures = 0;
    memset(m_IOFailureRegister, 0, sizeof(m_IOFailureRegister));
    m_IOFailureCounter = m_IOFailureRegisterIndex = 0;
    m_Throttled = false;
}

CNetServer::SExecResult CNetServer::ExecWithRetry(const string& cmd)
{
    CNetServer::SExecResult exec_result;

    CTime max_connection_time(GetFastLocalTime());
    max_connection_time.AddNanoSecond(
        m_Impl->m_ServerInPool->m_ServerPool->m_MaxConnectionTime * 1000000);

    unsigned attempt = 0;

    for (;;) {
        try {
            m_Impl->ConnectAndExec(cmd, exec_result);
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

            LOG_POST(Warning << e.what() << ", reconnecting: attempt " <<
                attempt << " of " << TServConn_ConnMaxRetries::GetDefault());
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

            LOG_POST(Warning << e.what() << ", reconnecting: attempt " <<
                attempt << " of " << TServConn_ConnMaxRetries::GetDefault());
        }

        SleepMilliSec(s_GetRetryDelay());
    }
}

CNetServerInfo CNetServer::GetServerInfo()
{
    return new SNetServerInfoImpl(ExecWithRetry("VERSION").response);
}

END_NCBI_SCOPE
