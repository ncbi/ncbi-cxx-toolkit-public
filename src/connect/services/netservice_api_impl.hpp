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
#include "balancing.hpp"

#include <set>

BEGIN_NCBI_SCOPE

struct SCompareServerAddress {
    bool operator()(const SNetServerImpl* l, const SNetServerImpl* r) const
    {
        return l->m_Address < r->m_Address;
    }
};

typedef vector<SNetServerImpl*> TNetServerList;
typedef set<SNetServerImpl*, SCompareServerAddress> TNetServerSet;

struct SNetServerGroupIteratorImpl : public CObject
{
    SNetServerGroupIteratorImpl(SNetServerGroupImpl* server_group_impl,
        TNetServerList::const_iterator position);

    CNetServerGroup m_ServerGroup;

    TNetServerList::const_iterator m_Position;
};

struct SNetServerGroupImpl : public CObject
{
    void Reset(CNetService::EDiscoveryMode discovery_mode,
        unsigned discovery_iteration)
    {
        m_NextGroupInPool = NULL;
        m_Servers.clear();
        m_DiscoveryMode = discovery_mode;
        m_DiscoveryIteration = discovery_iteration;
    }

    // Releases a reference to the parent service object,
    // and if that was the last reference, the service object
    // will be deleted.
    virtual void DeleteThis();

    SNetServerGroupImpl* m_NextGroupInPool;

    // A list of servers discovered by the load balancer.
    TNetServerList m_Servers;

    // A smart pointer to the NetService object
    // that contains this NetServerGroup.
    CNetService m_Service;

    CNetService::EDiscoveryMode m_DiscoveryMode;
    unsigned m_DiscoveryIteration;
};

inline SNetServerGroupIteratorImpl::SNetServerGroupIteratorImpl(
    SNetServerGroupImpl* server_group_impl,
    TNetServerList::const_iterator position) :
        m_ServerGroup(server_group_impl),
        m_Position(position)
{
}

struct NCBI_XCONNECT_EXPORT SNetServiceImpl : public CObject
{
    // Construct a new object.
    SNetServiceImpl(
        const string& api_name,
        const string& service_name,
        const string& client_name,
        INetServerConnectionListener* listener);

    void Init(CObject* api_impl,
        CConfig* config, const string& config_section,
        const char* const* default_config_sections);

    string MakeAuthString();

    SNetServerImpl* FindOrCreateServerImpl(
        const string& host, unsigned short port);
    CNetServer ReturnServer(SNetServerImpl* server_impl);
    CNetServer GetServer(const string& host, unsigned int port);
    CNetServer GetServer(const SServerAddress& server_address);
    CNetServer GetSingleServer(const string& cmd);

    // Utility method for commands that require single server (that is,
    // a host:port pair) to be specified (not a load-balanced service
    // name).
    CNetServer RequireStandAloneServerSpec(const string& cmd);

    SNetServerGroupImpl* AllocServerGroup(
        CNetService::EDiscoveryMode discovery_mode);

    SNetServerGroupImpl* DiscoverServers(
        CNetService::EDiscoveryMode discovery_mode);

    void Monitor(CNcbiOstream& out, const string& cmd);

    virtual ~SNetServiceImpl();

    string m_APIName;
    string m_ServiceName;
    string m_ClientName;

    string m_EnforcedServerHost;
    unsigned m_EnforcedServerPort;

    // Connection event listening. In fact, this listener implements
    // the authentication part of both NS and NC protocols.
    CRef<INetServerConnectionListener> m_Listener;
    CRef<CSimpleRebalanceStrategy> m_RebalanceStrategy;

    string m_LBSMAffinityName;
    const char* m_LBSMAffinityValue;
    unsigned m_LatestDiscoveryIteration;

    SNetServerGroupImpl* m_ServerGroupPool;

    union {
        SNetServerGroupImpl* m_SingleServerGroup;
        SNetServerGroupImpl* m_ServerGroups[
            CNetService::eNumberOfDiscoveryModes];
    };
    CFastMutex m_ServerGroupMutex;

    TNetServerSet m_Servers;
    CFastMutex m_ServerMutex;

    STimeout m_Timeout;
    ESwitch m_PermanentConnection;

    enum EServiceType {
        eNotDefined,
        eLoadBalanced,
        eSingleServer
    } m_ServiceType;

    int m_MaxSubsequentConnectionFailures;
    int m_ReconnectionFailureThresholdNumerator;
    int m_ReconnectionFailureThresholdDenominator;
    int m_ServerThrottlePeriod;
    int m_MaxQueryTime;
    bool m_ThrottleUntilDiscoverable;
    int m_ForceRebalanceAfterThrottleWithin;
};

inline CNetServer SNetServiceImpl::GetServer(
    const SServerAddress& server_address)
{
    return GetServer(server_address.host, server_address.port);
}

END_NCBI_SCOPE

#endif  /* CONNECT_SERVICES___NETSERVICE_API_IMPL__HPP */
