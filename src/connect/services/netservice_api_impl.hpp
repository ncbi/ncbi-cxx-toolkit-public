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

typedef pair<SNetServerImpl*, double> TServerRate;
typedef vector<TServerRate> TNetServerList;
typedef set<SNetServerImpl*, SCompareServerAddress> TNetServerSet;

struct SDiscoveredServers : public CObject
{
    SDiscoveredServers(unsigned discovery_iteration) :
        m_NextGroupInPool(NULL),
        m_DiscoveryIteration(discovery_iteration)
    {
    }

    void Reset(unsigned discovery_iteration)
    {
        m_NextGroupInPool = NULL;
        m_Servers.clear();
        m_DiscoveryIteration = discovery_iteration;
    }

    // Releases a reference to the parent service object,
    // and if that was the last reference, the service object
    // will be deleted.
    virtual void DeleteThis();

    SDiscoveredServers* m_NextGroupInPool;

    // A list of servers discovered by the load balancer.
    // The structure of this array is as follows:
    // index: begin()            m_SuppressedBegin              end()
    //        |                  |                              |
    // value: regular_srv_0...   suppressed_0... suppressed_last
    TNetServerList m_Servers;
    TNetServerList::const_iterator m_SuppressedBegin;

    // A smart pointer to the SNetServiceImpl object
    // that contains this object.
    CNetService m_Service;

    unsigned m_DiscoveryIteration;
};

struct NCBI_XCONNECT_EXPORT SNetServerPoolImpl : public CObject
{
    // Construct a new object.
    SNetServerPoolImpl(
        const string& api_name,
        const string& client_name,
        INetServerConnectionListener* listener);

    void Init(CObject* api_impl, CConfig* config, const string& section);

    SNetServerImpl* FindOrCreateServerImpl(
        const string& host, unsigned short port);
    CNetServer ReturnServer(SNetServerImpl* server_impl);
    CNetServer GetServer(const string& host, unsigned int port);
    CNetServer GetServer(const SServerAddress& server_address);

    virtual ~SNetServerPoolImpl();

    string m_APIName;
    string m_ClientName;

    string m_EnforcedServerHost;
    unsigned m_EnforcedServerPort;

    // Connection event listening. This listener implements
    // the authentication part of both NS and NC protocols.
    CRef<INetServerConnectionListener> m_Listener;

    CRef<CSimpleRebalanceStrategy> m_RebalanceStrategy;

    string m_LBSMAffinityName;
    const char* m_LBSMAffinityValue;

    TNetServerSet m_Servers;
    CFastMutex m_ServerMutex;

    STimeout m_ConnTimeout;
    STimeout m_CommTimeout;
    ESwitch m_PermanentConnection;

    int m_MaxConsecutiveIOFailures;
    int m_IOFailureThresholdNumerator;
    int m_IOFailureThresholdDenominator;
    int m_ServerThrottlePeriod;
    int m_MaxConnectionTime;
    bool m_ThrottleUntilDiscoverable;

    bool m_UseOldStyleAuth;
};

inline CNetServer SNetServerPoolImpl::GetServer(
    const SServerAddress& server_address)
{
    return GetServer(server_address.host, server_address.port);
}

struct SNetServiceIteratorImpl : public CObject
{
    SNetServiceIteratorImpl(SDiscoveredServers* server_group_impl) :
        m_ServerGroup(server_group_impl),
        m_Position(server_group_impl->m_Servers.begin())
    {
    }

    virtual bool Next();

    CRef<SDiscoveredServers> m_ServerGroup;

    TNetServerList::const_iterator m_Position;

protected:
    // For use by SNetServiceIterator_RandomPivot
    SNetServiceIteratorImpl(SDiscoveredServers* server_group_impl,
            TNetServerList::const_iterator position) :
        m_ServerGroup(server_group_impl), m_Position(position)
    {
    }
};

struct SNetServiceIterator_OmitPenalized : public SNetServiceIteratorImpl
{
    SNetServiceIterator_OmitPenalized(SDiscoveredServers* server_group_impl) :
        SNetServiceIteratorImpl(server_group_impl)
    {
    }

    virtual bool Next();
};

struct SNetServiceIterator_RandomPivot : public SNetServiceIteratorImpl
{
    SNetServiceIterator_RandomPivot(SDiscoveredServers* server_group_impl,
            TNetServerList::const_iterator pivot) :
        SNetServiceIteratorImpl(server_group_impl, pivot)
    {
    }

    SNetServiceIterator_RandomPivot(SDiscoveredServers* server_group_impl);

    virtual bool Next();

    typedef vector<TNetServerList::const_iterator> TRandomIterators;

    TRandomIterators m_RandomIterators;
    TRandomIterators::const_iterator m_RandomIterator;
};

class IIterationBeginner
{
public:
    virtual CNetServiceIterator BeginIteration() = 0;

    virtual ~IIterationBeginner() {}
};

enum EServiceType {
    eServiceNotDefined,
    eLoadBalancedService,
    eSingleServerService
};

struct SNetServiceImpl : public CObject
{
    // Construct a new object.
    SNetServiceImpl(const string& api_name, const string& client_name,
            INetServerConnectionListener* listener) :
        m_ServerPool(new SNetServerPoolImpl(api_name, client_name, listener))
    {
    }

    void Construct(SNetServerImpl* server);
    void Construct();

    // Constructors for 'spawning'.
    SNetServiceImpl(SNetServerImpl* server, SNetServiceImpl* parent) :
        m_ServerPool(parent->m_ServerPool)
    {
        Construct(server);
    }
    SNetServiceImpl(const string& service_name, SNetServiceImpl* parent) :
        m_ServerPool(parent->m_ServerPool)
    {
        Construct();
    }

    void Init(CObject* api_impl, const string& service_name,
        CConfig* config, const string& config_section,
        const char* const* default_config_sections);

    SNetServiceImpl(const string& service_name) : m_ServiceName(service_name)
    {
    }

    string MakeAuthString();

    void DiscoverServersIfNeeded();
    void GetDiscoveredServers(CRef<SDiscoveredServers>& discovered_servers);

    void IterateAndExec(const string& cmd,
        CNetServer::SExecResult& exec_result,
        IIterationBeginner* iteration_beginner);

    CNetServer GetSingleServer(const string& cmd);
    // Utility method for commands that require a single server (that is,
    // a host:port pair) to be specified (not a load-balanced service name).
    CNetServer RequireStandAloneServerSpec(const string& cmd);

    void Monitor(CNcbiOstream& out, const string& cmd);

    SDiscoveredServers* AllocServerGroup(unsigned discovery_iteration);

    virtual ~SNetServiceImpl();

    CNetServerPool m_ServerPool;

    string m_ServiceName;
    EServiceType m_ServiceType;

    CFastMutex m_DiscoveryMutex;
    SDiscoveredServers* m_DiscoveredServers;
    SDiscoveredServers* m_ServerGroupPool;
    unsigned m_LatestDiscoveryIteration;
};

END_NCBI_SCOPE

#endif  /* CONNECT_SERVICES___NETSERVICE_API_IMPL__HPP */
