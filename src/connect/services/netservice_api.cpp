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
 * Author:  Maxim Didenko
 *
 * File Description:
 */

#include <ncbi_pch.hpp>

#include <connect/services/netservice_api.hpp>
#include <connect/services/error_codes.hpp>
#include <connect/services/srv_connections_expt.hpp>
#include <connect/services/netservice_api_expt.hpp>
#include <connect/services/netservice_params.hpp>

#include <corelib/ncbi_system.hpp>

#define NCBI_USE_ERRCODE_X   ConnServ_Connection

BEGIN_NCBI_SCOPE


class CNetServiceAuthenticator : public INetServerConnectionListener
{
public:
    explicit CNetServiceAuthenticator(const CNetServiceAPI_Base& net_srv_client)
        : m_NetSrvClient(net_srv_client) {}

    virtual void OnConnected(CNetServerConnection conn);
    virtual void OnDisconnected(CNetServerConnectionPool* pool);

private:
    const CNetServiceAPI_Base& m_NetSrvClient;
};

void CNetServiceAuthenticator::OnConnected(CNetServerConnection conn)
{
    m_NetSrvClient.DoAuthenticate(conn);
}

void CNetServiceAuthenticator::OnDisconnected(CNetServerConnectionPool*)
{
}


CNetServiceAPI_Base::CNetServiceAPI_Base(
    const string& service_name,
    const string& client_name) :
    m_ServiceName(service_name),
    m_ClientName(client_name),
    m_ConnMode(eCloseConnection),
    m_RebalanceStrategy(NULL),
    m_IsLoadBalanced(false),
    m_DiscoverLowPriorityServers(eOff),
    m_Timeout(s_GetDefaultCommTimeout()),
    m_MaxRetries(TServConn_ConnMaxRetries::GetDefault()),
    m_PermanentConnection(eOff)
{
    if (!m_Authenticator.get())
        m_Authenticator.reset(new CNetServiceAuthenticator(*this));

    if (!m_RebalanceStrategy) {
        m_RebalanceStrategyGuard.reset(CreateDefaultRebalanceStrategy());
        m_RebalanceStrategy = m_RebalanceStrategyGuard.get();
    }

    string sport, host;

    if (NStr::SplitInTwo(service_name, ":", host, sport)) {
       unsigned int port = NStr::StringToInt(sport);
       host = CSocketAPI::ntoa(CSocketAPI::gethostbyname(host));
       CWriteLockGuard g(m_ServersLock);
       m_Servers.push_back(TServerAddress(host, port));
    } else {
       m_IsLoadBalanced = true;
    }

    SetRebalanceStrategy(m_RebalanceStrategy);
    PermanentConnection(m_ConnMode == eKeepConnection ? eOn : eOff);
}

CNetServiceAPI_Base::~CNetServiceAPI_Base()
{
    CFastMutexGuard g(m_ConnectionMutex);
    ITERATE(TServerAddressToConnectionPool, it,
            m_ServerAddressToConnectionPool) {
        delete it->second;
    }
}

CNetServiceAPI_Base::EConnectionMode CNetServiceAPI_Base::GetConnMode() const
{
    return m_ConnMode;
}

void CNetServiceAPI_Base::SetConnMode(EConnectionMode conn_mode)
{
    if (m_ConnMode == conn_mode)
        return;

    m_ConnMode = conn_mode;

    PermanentConnection(conn_mode == eKeepConnection ? eOn : eOff);
}

void CNetServiceAPI_Base::DiscoverLowPriorityServers(ESwitch on_off)
{
    if (m_DiscoverLowPriorityServers != on_off) {
        m_DiscoverLowPriorityServers = on_off;

        if (m_RebalanceStrategy)
            m_RebalanceStrategy->Reset();
    }
}

void CNetServiceAPI_Base::SetRebalanceStrategy(IRebalanceStrategy* strategy, EOwnership owner)
{
    m_RebalanceStrategy = strategy;
    if (owner == eTakeOwnership)
        m_RebalanceStrategyGuard.reset(m_RebalanceStrategy);
    else
        m_RebalanceStrategyGuard.reset();
}

/* static */
void CNetServiceAPI_Base::TrimErr(string& err_msg)
{
    if (err_msg.find("ERR:") == 0) {
        err_msg.erase(0, 4);
        err_msg = NStr::ParseEscapes(err_msg);
    }
}


void CNetServiceAPI_Base::PrintServerOut(
    CNetServerConnection conn, CNcbiOstream& out) const
{
    conn.WaitForServer();
    string response;
    for (;;) {
        if (!conn.ReadStr(response))
            break;
        CheckServerOK(response);
        if (response == "END")
            break;
        out << response << "\n";
    }
}


void CNetServiceAPI_Base::CheckServerOK(string& response) const
{
    if (NStr::StartsWith(response, "OK:")) {
        response.erase(0, 3); // "OK:"
    } else if (NStr::StartsWith(response, "ERR:")) {
        ProcessServerError(response, eTrimErr);
    }
}


void CNetServiceAPI_Base::ProcessServerError(string& response, ETrimErr trim_err) const
{
    if (trim_err == eTrimErr)
        TrimErr(response);
    NCBI_THROW(CNetServiceException, eCommunicationError, response);
}

string CNetServiceAPI_Base::SendCmdWaitResponse(
    CNetServerConnection conn, const string& cmd) const
{
    conn.WriteStr(cmd + "\r\n");
    return WaitResponse(conn);
}

string CNetServiceAPI_Base::WaitResponse(CNetServerConnection conn) const
{
    conn.WaitForServer();
    string tmp;
    if (!conn.ReadStr(tmp)) {
        conn.Abort();
        NCBI_THROW(CNetServiceException, eCommunicationError,
                   "Communication error reading from server " +
                   GetHostDNSName(conn.GetHost()) + ":" +
                   NStr::UIntToString(conn.GetPort()) + ".");
    }
    CheckServerOK(tmp);
    return tmp;
}

struct SNetServiceStreamCollector
{
    SNetServiceStreamCollector(const CNetServiceAPI_Base& api, const string& cmd,
                               CNetServiceAPI_Base::ISink& sink,
                               CNetServiceAPI_Base::EStreamCollectorType type)
        : m_API(api), m_Cmd(cmd), m_Sink(sink), m_Type(type)
    {}

    void operator()(CNetServerConnection conn)
    {
        if (m_Type == CNetServiceAPI_Base::eSendCmdWaitResponse) {
            CNcbiOstream& os = m_Sink.GetOstream(conn);
            os << m_API.SendCmdWaitResponse(conn, m_Cmd);
        } else if (m_Type == CNetServiceAPI_Base::ePrintServerOut) {
            CNcbiOstream& os = m_Sink.GetOstream(conn);
            conn.WriteStr(m_Cmd + "\r\n");
            m_API.PrintServerOut(conn, os);
        } else {
            _ASSERT(false);
        }
        m_Sink.EndOfData(conn);
    }

    const CNetServiceAPI_Base& m_API;
    const string& m_Cmd;
    CNetServiceAPI_Base::ISink& m_Sink;
    CNetServiceAPI_Base::EStreamCollectorType m_Type;

};

void CNetServiceAPI_Base::x_CollectStreamOutput(const string& cmd, ISink& sink, CNetServiceAPI_Base::EStreamCollectorType type)
{
    ForEach(SNetServiceStreamCollector(*this, cmd, sink, type));
}

CNetServerConnection CNetServiceAPI_Base::GetBest(
    const TServerAddress* backup, const string& hit)
{
    if (!m_IsLoadBalanced) {
        CReadLockGuard g(m_ServersLock);
        if (m_Servers.empty())
            NCBI_THROW(CNetSrvConnException, eSrvListEmpty, "The service is not set.");
        return GetConnection(m_Servers[0]);
    }
    TDiscoveredServers servers;
    try {
        DiscoverServers(servers);
    } catch (CNetSrvConnException& ex) {
        if (ex.GetErrCode() != CNetSrvConnException::eLBNameNotFound || !backup)
            throw;
        ERR_POST_X(5, "Connecting to backup server " <<
            backup->first << ":" <<
            backup->second << ".");
        return GetConnection(*backup);
    }
    ITERATE(TDiscoveredServers, it, servers) {
        CNetServerConnection conn = GetConnection(*it);
        try {
            conn.CheckConnect();
            return conn;
        } catch (CNetSrvConnException& ex) {
            if (ex.GetErrCode() == CNetSrvConnException::eConnectionFailure) {
                ERR_POST_X(2, ex.what());
            }
            else
                throw;
        }
    }
    if (backup) {
        ERR_POST_X(3, "Couldn't find any availbale servers for " <<
            m_ServiceName << " service. Connecting to backup server " <<
            backup->first << ":" << backup->second << ".");
        return GetConnection(*backup);
    }
    NCBI_THROW(CNetSrvConnException, eSrvListEmpty,
        "Couldn't find any availbale servers for " +
        m_ServiceName + " service.");
}

CNetServerConnection CNetServiceAPI_Base::GetSpecific(const string& host, unsigned int port)
{
    return GetConnection(TServerAddress(
        CSocketAPI::ntoa(CSocketAPI::gethostbyname(host)), port));
}

void CNetServiceAPI_Base::SetCommunicationTimeout(const STimeout& to)
{
    CFastMutexGuard g(m_ConnectionMutex);
    m_Timeout = to;
    NON_CONST_ITERATE(TServerAddressToConnectionPool,
        it, m_ServerAddressToConnectionPool) {
        it->second->SetCommunicationTimeout(to);
    }
}
const STimeout& CNetServiceAPI_Base::GetCommunicationTimeout() const
{
    return m_Timeout;
}

void CNetServiceAPI_Base::SetCreateSocketMaxRetries(unsigned int retries)
{
    CFastMutexGuard g(m_ConnectionMutex);
    m_MaxRetries = retries;
    NON_CONST_ITERATE(TServerAddressToConnectionPool,
        it, m_ServerAddressToConnectionPool) {
        it->second->SetCreateSocketMaxRetries(retries);
    }
}

unsigned int CNetServiceAPI_Base::GetCreateSocketMaxRetries() const
{
    return m_MaxRetries;
}

void CNetServiceAPI_Base::PermanentConnection(ESwitch type)
{
    CFastMutexGuard g(m_ConnectionMutex);
    m_PermanentConnection = type;
    NON_CONST_ITERATE(TServerAddressToConnectionPool,
        it, m_ServerAddressToConnectionPool) {
        it->second->PermanentConnection(type);
    }
}

CNetServerConnection
    CNetServiceAPI_Base::GetConnection(const TServerAddress& srv)
{
    CFastMutexGuard g(m_ConnectionMutex);
    TServerAddressToConnectionPool::iterator it =
        m_ServerAddressToConnectionPool.find(srv);
    CNetServerConnectionPool* pool = NULL;
    if (it != m_ServerAddressToConnectionPool.end())
        pool = it->second;
    if (pool == NULL) {
        pool = new CNetServerConnectionPool(srv.first,
            srv.second, m_Authenticator.get());
        pool->SetCommunicationTimeout(m_Timeout);
        pool->PermanentConnection(m_PermanentConnection);
        m_ServerAddressToConnectionPool[srv] = pool;
    }
    g.Release();
    if (m_RebalanceStrategy)
        m_RebalanceStrategy->OnResourceRequested(*pool);
    return pool->GetConnection();
}

void CNetServiceAPI_Base::DiscoverServers(TDiscoveredServers& servers)
{
    if (m_IsLoadBalanced &&
        (!m_RebalanceStrategy || m_RebalanceStrategy->NeedRebalance())) {
        CWriteLockGuard g(m_ServersLock);
        m_Servers.clear();
        int try_count = 0;
        for (;;) {
            try {
                QueryLoadBalancer(m_ServiceName, m_Servers,
                    m_DiscoverLowPriorityServers == eOn);
                break;
            } catch (CNetSrvConnException& ex) {
                if (ex.GetErrCode() != CNetSrvConnException::eLBNameNotFound)
                    throw;
                ERR_POST_X(4, "Communication Error : " << ex.what());
                if (++try_count > TServConn_MaxFineLBNameRetries::GetDefault())
                    throw;
                SleepMilliSec(1000 + try_count * 2000);
            }
        }
    }

    servers.clear();
    CReadLockGuard g(m_ServersLock);
    servers.insert(servers.begin(), m_Servers.begin(), m_Servers.end());
}


END_NCBI_SCOPE
