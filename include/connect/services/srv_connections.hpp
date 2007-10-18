#ifndef CONNECT_SERVICES__SERVER_CONN_HPP_1
#define CONNECT_SERVICES__SERVER_CONN_HPP_1

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
 * Authors:  Maxim Didneko,
 *
 * File Description:  
 *
 */

#include <corelib/ncbimisc.hpp>
#include <corelib/ncbistre.hpp>
#include <corelib/ncbistr.hpp>

#include <connect/ncbi_core_cxx.hpp>
#include <connect/connect_export.h>
#include <connect/ncbi_socket.hpp>

#include <corelib/ncbimtx.hpp>

#include <util/resource_pool.hpp>


BEGIN_NCBI_SCOPE

///////////////////////////////////////////////////////////////////////////
//
class CNetServerConnector;
class INetServerConnectorEventListener
{
public:
    virtual void OnConnected(CNetServerConnector&) {};
    virtual void OnDisconnected(CNetServerConnector&) {};

    virtual ~INetServerConnectorEventListener() {}
};

///////////////////////////////////////////////////////////////////////////
//
class CNetServerConnectors;
class CNetServerConnectorFactory;
class CNetServerConnectorHolder;
class NCBI_XCONNECT_EXPORT CNetServerConnector
{
public:
    ~CNetServerConnector();

    bool ReadStr(string& str);
    void WriteStr(const string& str);
    void WriteBuf(const void* buf, size_t len);
     // if wait_sec is set to 0 m_Timeout will be used
    void WaitForServer(unsigned int wait_sec = 0);

    class IStringProcessor {
    public:
        virtual ~IStringProcessor() {}
        // if returns false the telnet method will stop
        virtual bool Process(string& line) = 0;
    };
     // out and processor can be NULL
    void Telnet(CNcbiOstream* out,  IStringProcessor* processor);

    CSocket& GetSocket();
    void Disconnect();

    const string& GetHost() const;
    unsigned int GetPort() const;

private:

    friend class CNetServerConnectorFactory;
    explicit CNetServerConnector(CNetServerConnectors& parent);

    CNetServerConnectors& m_Parent;
    auto_ptr<CSocket>     m_Socket;
    bool                  m_WasConnected;

    bool x_IsConnected();
    void x_CheckConnect();
    friend class CNetServerConnectorHolder;
    void x_ReturnToParent();

    friend class CNetServerConnectors;
    void x_SetCommunicationTimeout(const STimeout& to)
    {
        if (m_Socket.get())
            m_Socket->SetTimeout(eIO_ReadWrite, &to);
    }

    CNetServerConnector(const CNetServerConnector&);
    CNetServerConnector& operator= (const CNetServerConnector&);
};

///////////////////////////////////////////////////////////////////////////
//
class CNetServerConnectorHolder
{
public:
    CNetServerConnectorHolder(CNetServerConnector& conn) 
        : m_Conn(&conn) {}

    ~CNetServerConnectorHolder() 
    { 
        if (m_Conn) m_Conn->x_ReturnToParent(); 
    }
 
    CNetServerConnectorHolder(const CNetServerConnectorHolder& other)
    {
        m_Conn = other.m_Conn;
        other.m_Conn = NULL;
    }
    CNetServerConnectorHolder& operator= (const CNetServerConnectorHolder& other)
    {
        if (this != &other) {
            if (m_Conn) m_Conn->x_ReturnToParent(); 
            m_Conn = other.m_Conn;
        }
        return *this;
    }

    operator CNetServerConnector& () const { _ASSERT(m_Conn); return *m_Conn; }
    CNetServerConnector* operator->() const { _ASSERT(m_Conn); return m_Conn; }

    void CheckConnect() const { m_Conn->x_CheckConnect(); }

    void reset()
    {
        if (m_Conn) {
            m_Conn->x_ReturnToParent(); 
            m_Conn = NULL;
        }
    }

private:
    mutable CNetServerConnector* m_Conn;
};

///////////////////////////////////////////////////////////////////////////
//
class CNetServerConnectorFactory
{
public:
    explicit CNetServerConnectorFactory(CNetServerConnectors& conns)
        : m_Connectors(&conns) {}

    CNetServerConnector* Create() const { return new CNetServerConnector(*m_Connectors); }
    void Delete(CNetServerConnector* conn) const { delete conn; }

private:
    CNetServerConnectors* m_Connectors;
};

///////////////////////////////////////////////////////////////////////////
//
class NCBI_XCONNECT_EXPORT CNetServerConnectors
{
public:
    CNetServerConnectors(const string& host, unsigned short port,
                         INetServerConnectorEventListener* listener = NULL);

    const string& GetHost() const { return m_Host; }
    unsigned short GetPort() const { return m_Port; }
    
    INetServerConnectorEventListener* GetEventListener() const { return m_EventListener; }

    static void SetDefaultCommunicationTimeout(const STimeout& to);
    void SetCommunicationTimeout(const STimeout& to);
    const STimeout& GetCommunicationTimeout() const { return m_Timeout; }
      
    static void SetDefaultCreateSocketMaxReties(unsigned int retires);
    void SetCreateSocketMaxRetries(unsigned int retries) { m_MaxRetries = retries; }
    unsigned int GetCreateSocketMaxRetries() const { return m_MaxRetries; }

    void PermanentConnection(ESwitch type) { m_PermanentConnection = type; }

    CNetServerConnectorHolder GetConnector();

private:
    string         m_Host;
    unsigned short m_Port;

    STimeout                          m_Timeout;
    INetServerConnectorEventListener* m_EventListener;
    unsigned int                      m_MaxRetries;

    CNetServerConnectorFactory m_ServerConnectorFactory;
    typedef CResourcePool_Base<CNetServerConnector, CRWLock, CNetServerConnectorFactory> TConnectorPool;
    auto_ptr<TConnectorPool> m_ServerConnectorPool;

    ESwitch m_PermanentConnection;

    friend class CNetServerConnector;
    void x_TakeConnector(CNetServerConnector*);

    CNetServerConnectors(const CNetServerConnectors&);
    CNetServerConnectors& operator= (const CNetServerConnectors&);
};


///////////////////////////////////////////////////////////////////////////
//
class IRebalanceStrategy 
{
public:
    virtual ~IRebalanceStrategy() {}
    virtual bool NeedRebalance() = 0;
    virtual void OnResourceRequested( const CNetServerConnectors& ) = 0;
    virtual void Reset() = 0;
};

class NCBI_XCONNECT_EXPORT CSimpleRebalanceStrategy : public IRebalanceStrategy 
{
public:
    CSimpleRebalanceStrategy(int rebalance_requests, int rebalance_time) 
        : m_RebalanceRequests(rebalance_requests), m_RebalanceTime(rebalance_time),
          m_RequestCounter(0), m_LastRebalanceTime(0) {}

    virtual ~CSimpleRebalanceStrategy() {}

    virtual bool NeedRebalance() {
        CFastMutexGuard g(m_Mutex);
        time_t curr = time(0);
        if ( !m_LastRebalanceTime || 
             (m_RebalanceTime && int(curr - m_LastRebalanceTime) >= m_RebalanceTime) ||
             (m_RebalanceRequests && (m_RequestCounter >= m_RebalanceRequests)) )  {
            m_RequestCounter = 0;
            m_LastRebalanceTime = curr;
            return true;
        }
        return false;
    }
    virtual void OnResourceRequested( const CNetServerConnectors& ) {
        CFastMutexGuard g(m_Mutex);
        ++m_RequestCounter;
    }
    virtual void Reset() {
        CFastMutexGuard g(m_Mutex);
        m_RequestCounter = 0;
        m_LastRebalanceTime = 0;
    }
    
private:
    int     m_RebalanceRequests;
    int     m_RebalanceTime;
    int     m_RequestCounter;
    time_t  m_LastRebalanceTime;
    CFastMutex m_Mutex;
};

///////////////////////////////////////////////////////////////////////////
//
class NCBI_XCONNECT_EXPORT CNetServiceConnector 
{
public:
    typedef pair<string, unsigned short> TServer;
    typedef map<TServer, CNetServerConnectors*> TCont;
    typedef vector<TServer> TServers;

    CNetServiceConnector(const string& service_name,
                         INetServerConnectorEventListener* listener = NULL);
    ~CNetServiceConnector();

    CNetServerConnectorHolder GetBest(const TServer* backup = NULL, const string& hit = "");
    CNetServerConnectorHolder GetSpecific(const string& host, unsigned int port);

    void SetCommunicationTimeout(const STimeout& to);
    const STimeout& GetCommunicationTimeout() const;
      
    void SetCreateSocketMaxRetries(unsigned int retries);
    unsigned int GetCreateSocketMaxRetries() const;

    void PermanentConnection(ESwitch type);

    bool IsLoadBalanced() const { return m_IsLoadBalanced; }

    template<class Pred>
    bool Find(Pred func) {
        x_Rebalance();
        CReadLockGuard g(m_ServersLock);
        TServers servers_copy(m_Servers);
        g.Release();
        // pick a random pivot element, so we do not always
        // fetch jobs using the same lookup order and some servers do 
        // not get equally "milked"
        // also get random list lookup direction
        unsigned servers_size = servers_copy.size();
        unsigned pivot = rand() % servers_size;
        unsigned pivot_plus_servers_size = pivot + servers_size;

        for (unsigned i = pivot; i < pivot_plus_servers_size; ++i) {
            unsigned j = i % servers_size;
            const TServer& srv = servers_copy[j];
            CNetServerConnectorHolder h = x_FindOrCreateConnector(srv).GetConnector(); 
            CNetServerConnector& conn = (CNetServerConnector&)h;
            if (func(conn, i == pivot_plus_servers_size - 1))
                return true;
        }
        return false;
    }

    template<class Func>
    void ForEach(Func func) {
        x_Rebalance();
        CReadLockGuard g(m_ServersLock);
        TServers servers_copy(m_Servers);
        g.Release();
        NON_CONST_ITERATE(TServers, it, servers_copy) {
            CNetServerConnectorHolder h = x_FindOrCreateConnector(*it).GetConnector(); 
            CNetServerConnector& conn = (CNetServerConnector&)h;
            func(conn);
        }
    }

    void DiscoverLowPriorityServers(ESwitch on_off); 
    void SetRebalanceStrategy(IRebalanceStrategy* strategy);

private: 
    string        m_ServiceName;
    TServers      m_Servers;
    CRWLock       m_ServersLock;
    mutable TCont      m_Connectors;
    mutable CFastMutex m_ConnectorsMutex;

    bool    m_IsLoadBalanced;
    ESwitch m_DiscoverLowPriorityServers;

    STimeout                          m_Timeout;
    INetServerConnectorEventListener* m_EventListener;
    unsigned int                      m_MaxRetries;
    ESwitch                           m_PermanentConnection;
    IRebalanceStrategy*               m_RebalanceStrategy;

    CNetServerConnectors& x_FindOrCreateConnector(const TServer& srv) const;
    void x_Rebalance();

    CNetServiceConnector(const CNetServiceConnector&);
    CNetServiceConnector& operator= (const CNetServiceConnector&);
};
///////////////////////////////////////////////////////////////////////////
inline 
const string& CNetServerConnector::GetHost() const
{
    return m_Parent.GetHost();
}
inline
unsigned int CNetServerConnector::GetPort() const
{
    return m_Parent.GetPort();
}

END_NCBI_SCOPE
 
#endif // CONNECT_SERVICES__SERVER_CONN_HPP
