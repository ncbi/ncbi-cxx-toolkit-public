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

#include <set>

BEGIN_NCBI_SCOPE

struct SCompareServerAddress {
    bool operator()(const SNetServerImpl* l, const SNetServerImpl* r) const
    {
        int cmp = l->m_Host.compare(r->m_Host);

        return cmp < 0 || (cmp == 0 && l->m_Port < r->m_Port);
    }
};

typedef set<SNetServerImpl*, SCompareServerAddress> TNetServerSet;

struct SNetServerGroupIteratorImpl : public CNetObject
{
    SNetServerGroupIteratorImpl(SNetServerGroupImpl* server_group_impl,
        TDiscoveredServers::const_iterator position);

    CNetServerGroup m_ServerGroup;

    TDiscoveredServers::const_iterator m_Position;
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

inline SNetServerGroupIteratorImpl::SNetServerGroupIteratorImpl(
    SNetServerGroupImpl* server_group_impl,
    TDiscoveredServers::const_iterator position) :
        m_ServerGroup(server_group_impl),
        m_Position(position)
{
}

struct SNetServiceImpl : public CNetObject
{
    // Construct a new object.
    SNetServiceImpl(
        const string& service_name,
        const string& client_name,
        const string& lbsm_affinity_name);

    // Set up connection event listening. In fact, this
    // listener implements the authentication part of both
    // NS and NC protocols.
    void SetListener(INetServerConnectionListener* listener);

    CNetServer GetServer(const string& host, unsigned int port);
    CNetServerConnection GetSingleServerConnection();

    // Utility method for commands that require single server (that is,
    // a host:port pair) to be specified (not a load-balanced service
    // name).
    CNetServerConnection RequireStandAloneServerSpec();

    void Monitor(CNcbiOstream& out, const string& cmd);

    virtual ~SNetServiceImpl();

    CNetObjectRef<CNetServiceDiscovery> m_ServiceDiscovery;
    string m_ClientName;

    CNetObjectRef<INetServerConnectionListener> m_Listener;
    CNetObjectRef<IRebalanceStrategy> m_RebalanceStrategy;

    TDiscoveredServers m_DiscoveredServers;
    CRWLock m_ServersLock;
    TNetServerSet m_Servers;
    CFastMutex m_ConnectionMutex;

    bool m_IsLoadBalanced;
    ESwitch m_DiscoverLowPriorityServers;

    STimeout m_Timeout;
    unsigned int m_MaxRetries;
    ESwitch m_PermanentConnection;
};

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
