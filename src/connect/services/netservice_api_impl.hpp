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
#include <corelib/request_ctx.hpp>
#include <connect/ncbi_connutil.h>

#include <map>
#include <atomic>
#include <memory>

BEGIN_NCBI_SCOPE

#define CONNSERV_THROW_FMT(exception_class, err_code, server, message) \
    NCBI_THROW(exception_class, err_code, \
            FORMAT(server->m_ServerInPool->m_Address.AsString() << \
            ": " << message))

typedef pair<SNetServerInPool*, double> TServerRate;
typedef vector<TServerRate> TNetServerList;
typedef map<SSocketAddress, SNetServerInPool*> TNetServerByAddress;
typedef map<string, CNetService> TNetServiceByName;

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
    SNetServerPoolImpl(INetServerConnectionListener* listener);

    void Init(CSynRegistry& registry, const SRegSynonyms& sections);

    SNetServerInPool* FindOrCreateServerImpl(SSocketAddress server_address);
    CRef<SNetServerInPool> ReturnServer(SNetServerInPool* server_impl);
    CNetServer GetServer(SNetServiceImpl* service, SSocketAddress server_address);

    void ResetServerConnections();
    const SThrottleParams& GetThrottleParams() const { return m_ThrottleParams; }

    virtual ~SNetServerPoolImpl();

private:
    INetServerConnectionListener::TPropCreator m_PropCreator;

public:
    SSocketAddress m_EnforcedServer;

    // LBSM affinity name and value
    using TLBSMAffinity = pair<string, const char*>;
    TLBSMAffinity m_LBSMAffinity;

    TNetServerByAddress m_Servers;
    CFastMutex m_ServerMutex;

    // Connection timeout, which is used as the eIO_Open timeout in CSocket::Connect.
    STimeout m_ConnTimeout;

    // Communication timeout, upon reaching which the connection is closed on the client side.
    STimeout m_CommTimeout;

    STimeout m_FirstServerTimeout;

    CTimeout m_MaxTotalTime;
    bool m_UseOldStyleAuth;

private:
    SThrottleParams m_ThrottleParams;
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

    CNetServer GetServer();
    double GetRate() const { return m_Position->second; }

protected:
    CRef<SDiscoveredServers> m_ServerGroup;

    TNetServerList::const_iterator m_Position;

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

struct SNetServiceIterator_Weighted : public SNetServiceIteratorImpl
{
    SNetServiceIterator_Weighted(SDiscoveredServers* server_group_impl,
            Uint4 key_crc32);

    virtual bool Next();
    virtual bool Prev();

    Uint4 m_KeyCRC32;

    struct SServerRank {
        TNetServerList::const_iterator m_ServerListIter;
        Uint4 m_ServerRank;
        SServerRank(TNetServerList::const_iterator server_list_iter,
                Uint4 server_rank) :
            m_ServerListIter(server_list_iter),
            m_ServerRank(server_rank)
        {
        }
        bool operator <(const SServerRank& that) const
        {
            return m_ServerRank < that.m_ServerRank ||
                    (m_ServerRank == that.m_ServerRank &&
                    m_ServerListIter->first->m_Address <
                            that.m_ServerListIter->first->m_Address);
        }
    };

    SServerRank x_GetServerRank(TNetServerList::const_iterator server) const
    {
        Uint4 rank = (1103515245 *
                (server->first->m_RankBase ^ m_KeyCRC32) + 12345) & 0x7FFFFFFF;
        return SServerRank(server, rank);
    }

    bool m_SingleServer;

    vector<SServerRank> m_ServerRanks;
    vector<SServerRank>::const_iterator m_CurrentServerRank;
};

class IServiceTraversal
{
public:
    virtual CNetServer BeginIteration() = 0;
    virtual CNetServer NextServer() = 0;

    virtual ~IServiceTraversal() {}
};

struct SNetServiceXSiteAPI : public CObject
{
    static void InitXSite(CSynRegistry& registry, const SRegSynonyms& sections);
    static void ConnectXSite(CSocket&, SNetServerImpl::SConnectDeadline&,
            const SSocketAddress&, const string&);

#ifdef NCBI_GRID_XSITE_CONN_SUPPORT
    static bool IsUsingXSiteProxy();
    static void AllowXSiteConnections();

private:
    static bool IsForeignAddr(unsigned int ip);
    static int GetDomain(unsigned int ip);

    static atomic<int> m_LocalDomain;
    static atomic<bool> m_AllowXSiteConnections;
#endif
};

struct NCBI_XCONNECT_EXPORT SNetServiceImpl : SNetServiceXSiteAPI
{
    enum EServiceType {
        eServiceNotDefined,
        eLoadBalancedService,
        eSingleServerService
    };

    struct SRetry
    {
        enum EType {
            eDefault,
            eNoRetry,
            eNoRetryNoErrors
        };

    protected:
        void Swap(SNetServiceImpl& impl, unsigned& n) { swap(impl.m_ConnectionMaxRetries, n); }
    };

    static SNetServiceImpl* Create(const string& api_name, const string& service_name, const string& client_name,
            INetServerConnectionListener* listener,
            CSynRegistry& registry, SRegSynonyms& sections, const string& ns_client_name = kEmptyStr);

    static SNetServiceImpl* Clone(SNetServerInPool* server, SNetServiceImpl* prototype);
    static SNetServiceImpl* Clone(const string& service_name, SNetServiceImpl* prototype);

private:
    // Construct a new object.
    SNetServiceImpl(const string& api_name, const string& service_name, const string& client_name,
            INetServerConnectionListener* listener, CSynRegistry& registry, const SRegSynonyms& sections);

    // Constructors for 'spawning'.
    SNetServiceImpl(SNetServerInPool* server, SNetServiceImpl* prototype);
    SNetServiceImpl(const string& service_name, SNetServiceImpl* prototype);

    void Construct(SNetServerInPool* server);
    void Construct();

    void Init(CSynRegistry& registry, SRegSynonyms& sections, const string& ns_client_name);

public:
    string MakeAuthString();

    CNetServer::SExecResult FindServerAndExec(const string& cmd,
        bool multiline_output);
    void DiscoverServersIfNeeded();
    void GetDiscoveredServers(CRef<SDiscoveredServers>& discovered_servers);

    bool IsInService(CNetServer::TInstance server);

    enum EServerErrorHandling {
        eRethrowServerErrors, // This ignores two errors, eBlobNotFound and eSubmitsDisabled
        eRethrowAllServerErrors,
        eIgnoreServerErrors
    };

    void IterateUntilExecOK(const string& cmd,
        bool multiline_output,
        CNetServer::SExecResult& exec_result,
        IServiceTraversal* service_traversal,
        EServerErrorHandling error_handling);

    SNetServiceIteratorImpl* Iterate(CNetServer::TInstance priority_server);

    SDiscoveredServers* AllocServerGroup(unsigned discovery_iteration);
    CNetServer GetServer(SSocketAddress server_address);

    const string& GetClientName() const { return m_ClientName; }

    unsigned GetConnectionMaxRetries() const { return m_ConnectionMaxRetries; }
    unsigned long GetConnectionRetryDelay() const { return m_ConnectionRetryDelay; }
    bool IsLoadBalanced() const { return m_ServiceType == eLoadBalancedService; }
    shared_ptr<void> CreateRetryGuard(SRetry::EType type);

    virtual ~SNetServiceImpl();

    // Connection event listening. This listener implements
    // the authentication part of both NS and NC protocols.
    CRef<INetServerConnectionListener> m_Listener;

    CNetServerPool m_ServerPool;

    string m_ServiceName;
    EServiceType m_ServiceType = eServiceNotDefined;

    CFastMutex m_DiscoveryMutex;
    SDiscoveredServers* m_DiscoveredServers = nullptr;
    SDiscoveredServers* m_ServerGroupPool = nullptr;
    unsigned m_LatestDiscoveryIteration = 0;
    CSimpleRebalanceStrategy m_RebalanceStrategy;
    atomic<size_t> m_RoundRobin;

private:
    string m_APIName;
    string m_ClientName;

    // connection parameters from config
    bool m_UseSmartRetries;
    unsigned m_ConnectionMaxRetries;
    unsigned long m_ConnectionRetryDelay;

    shared_ptr<void> m_NetInfo;
};

struct SNetServiceMap {
    CFastMutex m_ServiceMapMutex;
    TNetServiceByName m_ServiceByName;

    SNetServiceMap() {}
    SNetServiceMap(const SNetServiceMap& source) :
        m_ServiceByName(source.m_ServiceByName)
    {
    }

    CNetService GetServiceByName(const string& service_name,
            SNetServiceImpl* prototype);

    void Restrict() { m_Restricted = true; }
    bool IsAllowed(const string& service_name) const;
    bool IsAllowed(CNetServer::TInstance server, SNetServiceImpl* prototype);
    void AddToAllowed(const string& service_name);

private:
    CNetService GetServiceByNameImpl(const string&, SNetServiceImpl*);

    bool m_Restricted = false;
    set<string, PNocase> m_Allowed;
};

inline
void g_AppendHitID(string& cmd, CRequestContext& req)
{
    cmd += " ncbi_phid=\"";
    cmd += req.GetNextSubHitID();
    cmd += '"';
}

inline
void g_AppendClientIPAndSessionID(string& cmd, const CRequestContext& req)
{
    if (req.IsSetClientIP()) {
        cmd += " ip=\"";
        cmd += req.GetClientIP();
        cmd += '"';
    }

    cmd += " sid=\"";
    cmd += req.GetSessionID();
    cmd += '"';
}

END_NCBI_SCOPE

#endif  /* CONNECT_SERVICES___NETSERVICE_API_IMPL__HPP */
