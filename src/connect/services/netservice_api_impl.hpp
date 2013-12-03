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

#include <map>

BEGIN_NCBI_SCOPE

typedef pair<SNetServerInPool*, double> TServerRate;
typedef vector<TServerRate> TNetServerList;
typedef map<SServerAddress, SNetServerInPool*> TNetServerByAddress;

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
    SNetServerPoolImpl(const string& api_name, const string& client_name,
            INetServerConnectionListener* listener);

    void Init(CConfig* config, const string& section,
            INetServerConnectionListener* listener);

    SNetServerInPool* FindOrCreateServerImpl(
        unsigned host, unsigned short port);
    CRef<SNetServerInPool> ReturnServer(SNetServerInPool* server_impl);

    virtual ~SNetServerPoolImpl();

    string m_APIName;
    string m_ClientName;
    CRef<INetServerConnectionListener> m_Listener;

    unsigned m_EnforcedServerHost;
    unsigned short m_EnforcedServerPort;

    CRef<CSimpleRebalanceStrategy> m_RebalanceStrategy;

    string m_LBSMAffinityName;
    const char* m_LBSMAffinityValue;

    TNetServerByAddress m_Servers;
    CFastMutex m_ServerMutex;

    STimeout m_ConnTimeout;
    STimeout m_CommTimeout;
    STimeout m_FirstServerTimeout;

    ESwitch m_PermanentConnection;

    int m_MaxConsecutiveIOFailures;
    int m_IOFailureThresholdNumerator;
    int m_IOFailureThresholdDenominator;
    int m_ServerThrottlePeriod;
    int m_MaxConnectionTime;
    bool m_ThrottleUntilDiscoverable;

    bool m_UseOldStyleAuth;
};

struct SNetServiceIteratorImpl : public CObject
{
    SNetServiceIteratorImpl(SDiscoveredServers* server_group_impl) :
        m_ServerGroup(server_group_impl),
        m_Position(server_group_impl->m_Servers.begin())
    {
    }

    virtual bool Next();
    virtual bool Prev();

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
    virtual bool Prev();

    typedef vector<TNetServerList::const_iterator> TRandomIterators;

    TRandomIterators m_RandomIterators;
    TRandomIterators::const_iterator m_RandomIterator;
};

struct SNetServiceIterator_Circular : public SNetServiceIteratorImpl
{
    SNetServiceIterator_Circular(SDiscoveredServers* server_group_impl,
            TNetServerList::const_iterator pivot) :
        SNetServiceIteratorImpl(server_group_impl, pivot),
        m_Pivot(pivot)
    {
    }

    virtual bool Next();
    virtual bool Prev();

    TNetServerList::const_iterator m_Pivot;
};

class IServiceTraversal
{
public:
    virtual CNetServer BeginIteration() = 0;
    virtual CNetServer NextServer() = 0;

    virtual ~IServiceTraversal() {}
};

struct NCBI_XCONNECT_EXPORT SNetServiceImpl : public CObject
{
    // Construct a new object.
    SNetServiceImpl(const string& api_name, const string& client_name,
            INetServerConnectionListener* listener) :
        m_Listener(listener),
        m_ServerPool(new SNetServerPoolImpl(api_name, client_name, listener)),
#ifdef NCBI_GRID_XSITE_CONN_SUPPORT
        m_AllowXSiteConnections(false),
#endif
        m_UseSmartRetries(true)
    {
        ZeroInit();
    }

    // Constructors for 'spawning'.
    SNetServiceImpl(SNetServerInPool* server, SNetServiceImpl* parent) :
        m_Listener(parent->m_Listener),
        m_ServerPool(parent->m_ServerPool),
        m_ServiceName(server->m_Address.AsString()),
#ifdef NCBI_GRID_XSITE_CONN_SUPPORT
        m_ColoNetwork(parent->m_ColoNetwork),
        m_AllowXSiteConnections(parent->m_AllowXSiteConnections),
#endif
        m_UseSmartRetries(parent->m_UseSmartRetries)
    {
        ZeroInit();
        Construct(server);
    }
    SNetServiceImpl(const string& service_name, SNetServiceImpl* parent) :
        m_Listener(parent->m_Listener),
        m_ServerPool(parent->m_ServerPool),
        m_ServiceName(service_name),
#ifdef NCBI_GRID_XSITE_CONN_SUPPORT
        m_ColoNetwork(parent->m_ColoNetwork),
        m_AllowXSiteConnections(parent->m_AllowXSiteConnections),
#endif
        m_UseSmartRetries(parent->m_UseSmartRetries)
    {
        ZeroInit();
        Construct();
    }

    void ZeroInit();

    void Construct(SNetServerInPool* server);
    void Construct();

    void Init(CObject* api_impl, const string& service_name,
        CConfig* config, const string& config_section,
        const char* const* default_config_sections);

    string MakeAuthString();

    void DiscoverServersIfNeeded();
    void GetDiscoveredServers(CRef<SDiscoveredServers>& discovered_servers);

    bool IsInService(CNetServer::TInstance server);

    enum EServerErrorHandling {
        eRethrowServerErrors,
        eIgnoreServerErrors
    };

    void IterateUntilExecOK(const string& cmd,
        CNetServer::SExecResult& exec_result,
        IServiceTraversal* service_traversal,
        EServerErrorHandling error_handling);

    SDiscoveredServers* AllocServerGroup(unsigned discovery_iteration);

#ifdef NCBI_GRID_XSITE_CONN_SUPPORT
    void AllowXSiteConnections();

    bool IsColoAddr(unsigned int ip) const
    {
        return (SOCK_NetToHostLong(ip) >> 16) == m_ColoNetwork;
    }
#endif

    virtual ~SNetServiceImpl();

    // Connection event listening. This listener implements
    // the authentication part of both NS and NC protocols.
    CRef<INetServerConnectionListener> m_Listener;

    CNetServerPool m_ServerPool;

    string m_ServiceName;
    CNetService::EServiceType m_ServiceType;

    CFastMutex m_DiscoveryMutex;
    SDiscoveredServers* m_DiscoveredServers;
    SDiscoveredServers* m_ServerGroupPool;
    unsigned m_LatestDiscoveryIteration;

#ifdef NCBI_GRID_XSITE_CONN_SUPPORT
    unsigned int m_ColoNetwork;
    bool m_AllowXSiteConnections;
#endif

    bool m_UseSmartRetries;
};

END_NCBI_SCOPE

#endif  /* CONNECT_SERVICES___NETSERVICE_API_IMPL__HPP */
