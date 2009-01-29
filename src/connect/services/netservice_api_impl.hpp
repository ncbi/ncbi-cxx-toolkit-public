#ifndef CONNECT_SERVICES___NETSERVICE_API_IMPL__HPP
#define CONNECT_SERVICES___NETSERVICE_API_IMPL__HPP

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
 * Author:  Dmitry Kazimirov
 *
 * File Description:
 */

#include "srv_connections_impl.hpp"

#include <connect/services/netservice_api.hpp>

BEGIN_NCBI_SCOPE

typedef map<TServerAddress,
    CNetServerConnectionPool> TServerAddressToConnectionPool;

struct SNetServerImpl : public CNetObject
{
    SNetServerImpl(const TServerAddress& address,
        SNetServiceImpl* service_impl);

    // The host:port pair that identifies this server.
    TServerAddress m_Address;

    // A smart pointer to the NetService object
    // that contains this NetServer.
    CNetService m_Service;
};

struct SNetServerGroupImpl : public CNetObject
{
    SNetServerGroupImpl(SNetServiceImpl* service_impl);

    // A list of servers discovered by the load balancer.
    TDiscoveredServers m_Servers;

    // A smart pointer to the NetService object
    // that contains this NetServerGroup.
    CNetService m_Service;
};

class INetServerFinder : public CNetObject
{
public:
    virtual bool Consider(CNetServer server) = 0;
};

enum ECmdOutputType {
    eSingleLineOutput,
    eMultilineOutput,
    eMultilineOutput_NetCacheStyle
};

struct SNetServiceImpl : public CNetObject
{
    // Construct a new object. If needed, this constructor
    // can be 'published' in the CNetService component (as
    // the CNetService::Create() method).
    SNetServiceImpl(
        const std::string& service_name,
        const std::string& client_name,
        const std::string& lbsm_affinity_name);

    // Set up connection event listening. In fact, this
    // listener implements the authentication part of both
    // NS and NC protocols.
    void SetListener(INetServerConnectionListener* listener);

    CNetServerConnection GetBestConnection();
    CNetServerConnection GetConnection(const TServerAddress& srv);

    void DiscoverServers(TDiscoveredServers& servers,
        CNetService::EServerSortMode sort_mode = CNetService::eSortByLoad);

    bool FindServer(const CNetObjectRef<INetServerFinder>& finder,
        CNetService::EServerSortMode sort_mode = CNetService::eRandomize);

    void PrintCmdOutput(
        const string& cmd,
        CNcbiOstream& output_stream,
        ECmdOutputType output_type);

    template<class Func>
    Func ForEach(Func func) {
        TDiscoveredServers servers;

        DiscoverServers(servers);

        ITERATE(TDiscoveredServers, it, servers) {
            func(GetConnection(*it));
        }

        return func;
    }

    CNetObjectRef<CNetServiceDiscovery> m_ServiceDiscovery;
    string m_ClientName;

    CNetObjectRef<INetServerConnectionListener> m_Listener;
    CNetObjectRef<IRebalanceStrategy> m_RebalanceStrategy;

    TDiscoveredServers m_Servers;
    CRWLock m_ServersLock;
    TServerAddressToConnectionPool m_ServerAddressToConnectionPool;
    CFastMutex m_ConnectionMutex;

    bool m_IsLoadBalanced;
    ESwitch m_DiscoverLowPriorityServers;

    STimeout m_Timeout;
    unsigned int m_MaxRetries;
    ESwitch m_PermanentConnection;
};

inline SNetServerImpl::SNetServerImpl(const TServerAddress& address,
    SNetServiceImpl* service_impl) : m_Address(address), m_Service(service_impl)
{
}

inline SNetServerGroupImpl::SNetServerGroupImpl(SNetServiceImpl* service_impl) :
    m_Service(service_impl)
{
}

inline void SNetServiceImpl::SetListener(INetServerConnectionListener* listener)
{
    m_Listener = listener;
}

END_NCBI_SCOPE

#endif  /* CONNECT_SERVICES___NETSERVICE_API_IMPL__HPP */
