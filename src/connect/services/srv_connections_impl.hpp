#ifndef CONNECT_SERVICES___SRV_CONNECTIONS_IMPL__HPP
#define CONNECT_SERVICES___SRV_CONNECTIONS_IMPL__HPP

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
 * Authors:  Dmitry Kazimirov
 *
 * File Description:
 *
 */

#include "netservice_params.hpp"

#include <connect/services/netservice_api.hpp>

#include <corelib/ncbimtx.hpp>

#include <util/ncbi_url.hpp>

#include <bitset>

BEGIN_NCBI_SCOPE

struct SNetServerMultilineCmdOutputImpl : public CObject
{
    SNetServerMultilineCmdOutputImpl(
        const CNetServer::SExecResult& exec_result) :
            m_Connection(exec_result.conn),
            m_FirstOutputLine(exec_result.response),
            m_FirstLineConsumed(false),
            m_NetCacheCompatMode(false),
            m_ReadCompletely(false)
    {
    }

    virtual ~SNetServerMultilineCmdOutputImpl();

    void SetNetCacheCompatMode() { m_NetCacheCompatMode = true; }
    bool ReadLine(string& output);

private:
    CNetServerConnection m_Connection;

    string m_FirstOutputLine;
    bool m_FirstLineConsumed;

    bool m_NetCacheCompatMode;
    bool m_ReadCompletely;
};

class INetServerProperties : public CObject
{
};

struct NCBI_XCONNECT_EXPORT INetServerConnectionListener : CObject
{
    using TPropCreator = function<INetServerProperties*()>;
    using TEventHandler = CNetService::TEventHandler;

    virtual TPropCreator GetPropCreator() const;
    virtual INetServerConnectionListener* Clone() = 0;

    // Event handlers.
    virtual void OnConnected(CNetServerConnection& connection) = 0;

    void OnError(const string& err_msg, CNetServer& server);
    void OnWarning(const string& warn_msg, CNetServer& server);

    void SetErrorHandler(TEventHandler error_handler);
    void SetWarningHandler(TEventHandler warning_handler);

protected:
    INetServerConnectionListener() = default;
    INetServerConnectionListener(const INetServerConnectionListener&);
    INetServerConnectionListener& operator=(const INetServerConnectionListener&);

private:
    virtual void OnErrorImpl(const string& err_msg, CNetServer& server) = 0;
    virtual void OnWarningImpl(const string& warn_msg, CNetServer& server) = 0;

    TEventHandler m_ErrorHandler;
    TEventHandler m_WarningHandler;
};

struct SNetServerConnectionImpl : public CObject
{
    SNetServerConnectionImpl(SNetServerImpl* pool);

    // Return this connection to the pool.
    virtual void DeleteThis();

    void WriteLine(const string& line);
    void ReadCmdOutputLine(string& result,
            bool multiline_output);

    void Close();
    void Abort();

    virtual ~SNetServerConnectionImpl();

    // The server this connection is connected to.
    CNetServer m_Server;
    CAtomicCounter::TValue m_Generation;
    SNetServerConnectionImpl* m_NextFree;

    CSocket m_Socket;
};

class INetServerExecHandler
{
public:
    virtual ~INetServerExecHandler() {}

    virtual void Exec(CNetServerConnection::TInstance conn_impl,
            const STimeout* timeout) = 0;
};

class INetServerExecListener
{
public:
    virtual ~INetServerExecListener() {}

    virtual void OnExec(CNetServerConnection::TInstance conn_impl,
            const string& cmd) = 0;
};

struct SThrottleParams
{
    // The parameters below describe different conditions that trigger server throttling.
    int max_consecutive_io_failures;

    // Connection failure rate (numerator/denominator), which is when reached, triggers server throttling.
    struct SIOFailureThreshold
    {
        size_t numerator = 0;
        size_t denominator = 1;
        constexpr static size_t kMaxDenominator = 128;

        void Init(CSynRegistry& registry, const SRegSynonyms& sections);
    } io_failure_threshold;

    // How many seconds the API should wait before attempting to connect to a misbehaving server again.
    // Throttling is off if period is less or equal to zero.
    int throttle_period;

    // Whether to check with LBSMD before re-enabling the server.
    bool throttle_until_discoverable;

    // Whether to throttle on connection failures only
    bool connect_failures_only;

    void Init(CSynRegistry& registry, const SRegSynonyms& sections);
};

struct SThrottleStats
{
    SThrottleStats(SThrottleParams params) : m_Params(move(params)) { Reset(); }

    void Adjust(SNetServerImpl* server_impl, int err_code);
    void Check(SNetServerImpl* server_impl);
    void Discover();

private:
    void Reset();

    const SThrottleParams m_Params;
    int m_NumberOfConsecutiveIOFailures;
    pair<bitset<SThrottleParams::SIOFailureThreshold::kMaxDenominator>, size_t> m_IOFailureRegister;
    bool m_Throttled;
    bool m_DiscoveredAfterThrottling;
    string m_ThrottleMessage;
    CTime m_ThrottledUntil;
    CFastMutex m_ThrottleLock;
};

struct SNetServerInPool : public CObject
{
    SNetServerInPool(SSocketAddress address,
            INetServerProperties* server_properties, SThrottleParams throttle_params);

    // Releases a reference to the parent service object,
    // and if that was the last reference, the service object
    // will be deleted, which in turn will lead to this server
    // object being deleted too (along with all other server
    // objects that the parent service object may contain).
    virtual void DeleteThis();

    virtual ~SNetServerInPool();

    void TryExec(SNetServerImpl* server, INetServerExecHandler& handler, const STimeout* timeout);

    CNetServerConnection GetConnectionFromPool(SNetServerImpl* server);
    CNetServerConnection Connect(SNetServerImpl* server, const STimeout* timeout);

public:
    // A smart pointer to the server pool object that contains
    // this NetServer. Valid only when this object is returned
    // to an outside caller via ReturnServer() or GetServer().
    CNetServerPool m_ServerPool;

    SSocketAddress m_Address;

    // API-specific server properties.
    CRef<INetServerProperties> m_ServerProperties;

    CAtomicCounter m_CurrentConnectionGeneration;
    SNetServerConnectionImpl* m_FreeConnectionListHead;
    int m_FreeConnectionListSize;
    CFastMutex m_FreeConnectionListLock;

    SThrottleStats m_ThrottleStats;
    Uint4 m_RankBase;
};

struct SNetServerInfoImpl : public CObject
{
    SNetServerInfoImpl(const string& version_string);
    bool GetNextAttribute(string& attr_name, string& attr_value);

private:
    typedef CUrlArgs::TArgs TAttributes;

    TAttributes m_Attributes;
    TAttributes::const_iterator m_NextAttribute;
};

struct SNetServerImpl : public CObject
{
    struct SConnectDeadline;

    SNetServerImpl(CNetService::TInstance service,
            SNetServerInPool* server_in_pool) :
        m_Service(service),
        m_ServerInPool(server_in_pool)
    {
    }

    void TryExec(INetServerExecHandler& handler,
            const STimeout* timeout = NULL);

    CNetServer::SExecResult ConnectAndExec(const string& cmd,
            bool multiline_output,
            bool retry_on_exception = false);

    void ConnectAndExec(const string& cmd,
            bool multiline_output,
            CNetServer::SExecResult& exec_result,
            const STimeout* timeout = NULL,
            INetServerExecListener* exec_listener = NULL);

    static void ConnectImpl(CSocket&, SConnectDeadline&, const SSocketAddress&,
            const SSocketAddress&);

    template <class TProperties>
    CRef<TProperties> Get()
    {
        auto properties = m_ServerInPool->m_ServerProperties.GetPointerOrNull();
        return CRef<TProperties>(static_cast<TProperties*>(properties));
    }

    CNetService m_Service;
    CRef<SNetServerInPool> m_ServerInPool;
};

class CTimeoutKeeper
{
public:
    CTimeoutKeeper(CSocket* sock, const STimeout* timeout)
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

END_NCBI_SCOPE

#endif  /* CONNECT_SERVICES___SRV_CONNECTIONS_IMPL__HPP */
