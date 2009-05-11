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
    SNetServerGroupImpl(SNetServerGroupImpl** origin,
        unsigned discovery_iteration);

    // Releases a reference to the parent service object,
    // and if that was the last reference, the service object
    // will be deleted.
    virtual void Delete();

    // A list of servers discovered by the load balancer.
    TDiscoveredServers m_Servers;

    // A smart pointer to the NetService object
    // that contains this NetServerGroup.
    CNetService m_Service;

    // A pointer to the SNetServiceImpl::m_ServerGroups array
    // element where this server group object belongs.
    SNetServerGroupImpl** m_Origin;

    unsigned m_DiscoveryIteration;
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
    CNetServer GetServer(const TServerAddress& server_address);
    CNetServerConnection GetSingleServerConnection();

    // Utility method for commands that require single server (that is,
    // a host:port pair) to be specified (not a load-balanced service
    // name).
    CNetServerConnection RequireStandAloneServerSpec();

    SNetServerGroupImpl* CreateServerGroup(
        CNetService::EDiscoveryMode discovery_mode);

    SNetServerGroupImpl* DiscoverServers(
        CNetService::EDiscoveryMode discovery_mode);

    void Monitor(CNcbiOstream& out, const string& cmd);

    virtual ~SNetServiceImpl();

    string m_ServiceName;
    string m_ClientName;

    CNetObjectRef<INetServerConnectionListener> m_Listener;
    CNetObjectRef<IRebalanceStrategy> m_RebalanceStrategy;

    string m_LBSMAffinityName;
    const char* m_LBSMAffinityValue;
    unsigned m_LatestDiscoveryIteration;

    union {
        SNetServerGroupImpl* m_SignleServer;
        SNetServerGroupImpl* m_ServerGroups[
            CNetService::eNumberOfDiscoveryModes];
    };
    CFastMutex m_ServerGroupMutex;

    TNetServerSet m_DiscoveredServers;
    CFastMutex m_ServerMutex;

    STimeout m_Timeout;
    ESwitch m_PermanentConnection;

    enum EServiceType {
        eNotDefined,
        eLoadBalanced,
        eSingleServer
    } m_ServiceType;
};

inline SNetServerGroupImpl::SNetServerGroupImpl(SNetServerGroupImpl** origin,
    unsigned discovery_iteration) :
        m_Origin(origin),
        m_DiscoveryIteration(discovery_iteration)
{
}

inline void SNetServiceImpl::SetListener(INetServerConnectionListener* listener)
{
    m_Listener = listener;
}

inline CNetServer SNetServiceImpl::GetServer(
    const TServerAddress& server_address)
{
    return GetServer(server_address.first, server_address.second);
}

END_NCBI_SCOPE

#endif  /* CONNECT_SERVICES___NETSERVICE_API_IMPL__HPP */
