#ifndef CONNECT_SERVICES__SRV_CONNECTIONS_HPP
#define CONNECT_SERVICES__SRV_CONNECTIONS_HPP

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

#include <util/resource_pool.hpp>

#include <vector>

BEGIN_NCBI_SCOPE

NCBI_XCONNECT_EXPORT 
void DiscoverLBServices(const string& service_name, 
                        vector<pair<string,unsigned short> >& services,
                        bool all_services = true);


/******************************************************************/
struct CSocketFactory
{
    static CSocket* Create(); 
    static void Delete(CSocket* v) { delete v; }    
};

class NCBI_XCONNECT_EXPORT CNetSrvConnector
{
public:
    CNetSrvConnector(const string& host, unsigned short port);
    ~CNetSrvConnector();

    class IEventListener {
    public:
        virtual ~IEventListener() {}

        virtual void OnConnected(CNetSrvConnector&) {};
        virtual void OnDisconnected(CNetSrvConnector&) {};
    };

    const string GetHost() const { return m_Host; }
    unsigned short GetPort() const { return m_Port; }

    void SetEventListener(IEventListener* listener)  { m_EventListener = listener; }


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
    void Telnet(CNcbiOstream& out, IStringProcessor* processor = NULL);

    // if socket is not NULL the temporary connector with the given socket
    // will be create and its disconnect method will be called. This is used
    // for detached socket
    void Disconnect(CSocket* socket = NULL);

    CSocket* DetachSocket();
    void AttachSocket(CSocket* socket);

    static void SetDefaultCommunicationTimeout(const STimeout& to);
    void SetCommunicationTimeout(const STimeout& to);
    STimeout GetCommunicationTimeout() const { return m_Timeout; }

    static void SetDefaultCreateSocketMaxReties(unsigned int retires);
    void SetCreateSocketMaxRetries(unsigned int retries) { m_MaxRetries = retries; }
    unsigned int GetCreateSocketMaxRetries() const { return m_MaxRetries; }

private:

    CNetSrvConnector(const CNetSrvConnector& other);
    CNetSrvConnector& operator=(const CNetSrvConnector&);

    bool x_IsConnected();
    void x_CheckConnect();

    string             m_Host;
    unsigned short     m_Port;
    auto_ptr<CSocket>  m_Socket;
    STimeout           m_Timeout;
    IEventListener*    m_EventListener;
    bool               m_ReleaseSocket;
    unsigned int       m_MaxRetries;

    typedef CResourcePool<CSocket, CNoLock, CSocketFactory> TResourcePool;
    TResourcePool m_SocketPool;

public:
    // !!Special perpose constructor. Use it only if you know what you
    // are doing. Copies all member but m_Socket from 'other' and sets m_Socket
    // with 'socket'. 'socket' will not be deleted after connector 
    // distruction
    CNetSrvConnector(const CNetSrvConnector& other, CSocket* socket);

};

class CNetSrvConnectorHolder
{
public:
    CNetSrvConnectorHolder(CNetSrvConnector& conn, bool disconnect) 
        : m_Conn(&conn), m_Disconnect(disconnect) {}
    ~CNetSrvConnectorHolder() { if (m_Disconnect) m_Conn->Disconnect(); }
 
    CNetSrvConnectorHolder(const CNetSrvConnectorHolder& other)
    {
        m_Conn = other.m_Conn;
        m_Disconnect = other.m_Disconnect;
        other.m_Disconnect = false;
    }
    CNetSrvConnectorHolder& operator=( const CNetSrvConnectorHolder& other)
    {
        if(this != &other) {
            if (m_Disconnect) m_Conn->Disconnect();
            m_Conn = other.m_Conn;
            m_Disconnect = other.m_Disconnect;
            other.m_Disconnect = false;            
        }
        return *this;
    }

    operator CNetSrvConnector& () const  { return *m_Conn; }
    CNetSrvConnector* operator->() const { return m_Conn; }

    bool NeedDisconnect() const { return m_Disconnect; }
private:
    CNetSrvConnector* m_Conn;
    mutable bool m_Disconnect;

};

class IRebalanceStrategy 
{
public:
    virtual ~IRebalanceStrategy() {}
    virtual bool NeedRebalance() = 0;
    virtual void OnResourceRequested( const CNetSrvConnector& ) = 0;
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
    virtual void OnResourceRequested( const CNetSrvConnector& ) {
        ++m_RequestCounter;
    }
    virtual void Reset() {
        m_RequestCounter = 0;
        m_LastRebalanceTime = 0;
        
    }
private:
    int     m_RebalanceRequests;
    int     m_RebalanceTime;
    int     m_RequestCounter;
    time_t  m_LastRebalanceTime;
};



class NCBI_XCONNECT_EXPORT CNetSrvConnectorPoll : virtual protected CConnIniter
{    
public:   
    class iterator
    {
    public:
        typedef CNetSrvConnector value_type;
        typedef CNetSrvConnectorHolder pointer;
        typedef CNetSrvConnectorHolder reference;

        reference operator*() const 
        { 
            unsigned index = (m_CurIndex-1) % m_Poll->m_Services.size();
            return CNetSrvConnectorHolder(*m_Poll->x_FindOrCreateConnector(m_Poll->m_Services[index]),
                                          !m_Poll->m_PermConn);
        }
        pointer operator->() const { return operator*(); }

        iterator& operator++() 
        { 
            if(m_CurIndex) {
                if(++m_CurIndex >= m_Pivot+m_Poll->m_Services.size())
                    m_CurIndex = 0;
            }
            return *this; 
        }
        iterator operator++(int) { iterator temp(*this); operator++(); return temp; }
        
        bool operator == (const iterator& other) const 
        { return other.m_Poll == m_Poll && other.m_CurIndex == m_CurIndex; }
        bool operator != (const iterator& other) const
        { return other.m_Poll != m_Poll || other.m_CurIndex != m_CurIndex; }
        
    private:
        const CNetSrvConnectorPoll* m_Poll;
        unsigned m_Pivot;
        unsigned m_CurIndex;

        friend class CNetSrvConnectorPoll;
        explicit iterator(const CNetSrvConnectorPoll& poll, bool last = false, bool random = false)
            : m_Poll(&poll)
        { 
            m_Pivot = random && !m_Poll->m_Services.empty()? (rand() %  m_Poll->m_Services.size())+1 : 1;
            m_CurIndex = last || m_Poll->m_Services.empty()? 0 : m_Pivot;
        }


    };

    typedef pair<string, unsigned short> TService;
    typedef map<TService, CNetSrvConnector*> TCont;
    typedef vector<TService> TServices;

    CNetSrvConnectorPoll(const string& service, 
                         CNetSrvConnector::IEventListener* event_listener,
                         IRebalanceStrategy* rebalance_strategy);
    ~CNetSrvConnectorPoll();

    CNetSrvConnectorHolder GetBest(const string& hit = "");
    CNetSrvConnectorHolder GetSpecific(const string& host, unsigned int port);

    // invalidates priveosly taken iterators
    iterator begin() { x_Rebalance(); return iterator(*this); }
    // invalidates priveosly taken iterators
    iterator random_begin() { x_Rebalance(); return iterator(*this,false,true); }
    iterator end() { return iterator(*this, true); }

    size_t GetServersNumber() const { return m_Services.size(); }

    template<class Pred>
    bool Find(Pred func) {
        x_Rebalance();
        // pick a random pivot element, so we do not always
        // fetch jobs using the same lookup order and some servers do 
        // not get equally "milked"
        // also get random list lookup direction
        unsigned serv_size = m_Services.size();
        unsigned pivot = rand() % serv_size;

        for (unsigned i = pivot; i < pivot + serv_size; ++i) {
            unsigned j = i % serv_size;
            const TService& srv = m_Services[j];
            CNetSrvConnectorHolder holder(*x_FindOrCreateConnector(srv), 
                                          !m_PermConn);
            CNetSrvConnector& conn = (CNetSrvConnector&)holder;
            if (func(conn))
                return true;
        }
        return false;
    }
    const string& GetServiceName() const { return m_ServiceName; }

    bool IsLoadBalanced() const { return m_IsLoadBalanced; }

    void UsePermanentConnection(bool of_on) { m_PermConn = of_on; }

    void DiscoverLowPriorityServers(bool on_off) { 
        m_DiscoverLowPriorityServers = on_off; 
        if (m_RebalanceStrategy) m_RebalanceStrategy->Reset();
    }

    void SetRebalanceStrategy(IRebalanceStrategy* strategy)
    {
        m_RebalanceStrategy = strategy;
    }

    void SetCommunicationTimeout(const STimeout& to);
    STimeout GetCommunicationTimeout() const { return m_Timeout; }

    void SetCreateSocketMaxRetries(unsigned int retries);
    unsigned int GetCreateSocketMaxRetries() const { return m_MaxRetries; }
    
private:
    friend class iterator;

    CNetSrvConnector* x_FindOrCreateConnector(const TService& srv) const;
    void x_Rebalance();

    string              m_ServiceName;
    TServices           m_Services;
    mutable TCont       m_Connections;
    IRebalanceStrategy* m_RebalanceStrategy;
    bool                m_IsLoadBalanced;
    bool                m_DiscoverLowPriorityServers;
    CNetSrvConnector::IEventListener* m_EventListener;
    bool                m_PermConn;
    STimeout            m_Timeout;
    unsigned int        m_MaxRetries; 

    CNetSrvConnectorPoll(const CNetSrvConnectorPoll&);
    CNetSrvConnectorPoll& operator=(const CNetSrvConnectorPoll&);

};



/// Net Service exception
///
class CNetSrvConnException : public CException
{
public:
    enum EErrCode {
        eReadTimeout,
        eResponseTimeout,
        eLBNameNotFound,
        eSrvListEmpty
    };

    virtual const char* GetErrCodeString(void) const
    {
        switch (GetErrCode())
        {
        case eReadTimeout:        return "eReadTimeout";
        case eResponseTimeout:    return "eResponseTimeout";
        case eLBNameNotFound:     return "eLBNameNotFound";
        case eSrvListEmpty:       return "eSrvListEmpty";
        default:                  return CException::GetErrCodeString();
        }
    }

    NCBI_EXCEPTION_DEFAULT(CNetSrvConnException, CException);
};


///////////////////////////////////////////////////////////////////////////
struct undefined_t {};

template<typename TDerived>
struct CServiceClientTraits {
    typedef undefined_t client_type;
};

template<typename TDerived>
struct Wrapper
{
    TDerived& derived()
    {
        return *static_cast<TDerived*>(this);
    }

    TDerived const& derived() const
    {
        return *static_cast<TDerived const*>(this);
    }
};

template<typename TDerived>
class CServiceConnections : public Wrapper<TDerived>
{
    typedef typename CServiceClientTraits<TDerived>::client_type client_t;
    typedef vector<client_t*> TConnections;
public:
    CServiceConnections(const string& service)
        : m_Service(service), m_Discovered(false)  {}

    ~CServiceConnections() 
    {
        ITERATE(typename TConnections, it, m_Connections) {
            delete *it;
        }
    }

    const string& GetService() const { return m_Service; }
    bool IsHostPortConfig() { x_DiscoverConnections(); return m_HostPort; }

    template<typename Func>
    void for_each(Func func) {
        typename TConnections::const_iterator it_b = GetCont().begin();
        typename TConnections::const_iterator it_e = GetCont().end();
        while( it_b != it_e ) {
            func( **it_b );
            ++it_b;
        }
    }

private:

    TConnections& GetCont() {
        x_DiscoverConnections();
        return m_Connections;
    }

    string m_Service;
    bool m_HostPort;
    bool m_Discovered;
    TConnections m_Connections;
    void x_DiscoverConnections()
    {
        if (m_Discovered) return;
        string sport, host;
        typedef vector<pair<string, unsigned short> > TSrvs;
        TSrvs srvs;
        if ( NStr::SplitInTwo(m_Service, ":", host, sport) ) {
            try {
                unsigned int port = NStr::StringToInt(sport);
                srvs.push_back(make_pair(host, (unsigned short)port));
                m_HostPort = true;
            } catch (...) {
            }
        } else {
            DiscoverLBServices(m_Service, srvs);
            m_HostPort = false;
        }
        if (srvs.empty())
            NCBI_THROW(CCoreException, eInvalidArg, "\"" +m_Service+ "\" is not a valid service name");

        ITERATE(TSrvs, it, srvs) {
            m_Connections.push_back(this->derived().CreateClient(it->first, it->second));
        }
        m_Discovered = true;
    }
};

template<typename TClient>
class ICtrlCmd
{
public:
    explicit ICtrlCmd(CNcbiOstream& os) : m_Os(os) {}
    virtual ~ICtrlCmd () {}

    CNcbiOstream& GetStream() { return m_Os;};
    
    virtual void Execute(TClient& cln) = 0;

private:
    CNcbiOstream& m_Os;
};

template<typename TClient, typename TConnections>
class CCtrlCmdRunner 
{
public:
    CCtrlCmdRunner(TConnections& connections, ICtrlCmd<TClient>& cmd)
        : m_Connections(&connections), m_Cmd(&cmd) {}

    void operator()(TClient& cln) {
        if ( !m_Connections->IsHostPortConfig() ) 
            m_Cmd->GetStream() << m_Connections->GetService() 
                              << "(" << cln.GetHost() << ":" << cln.GetPort() << "): ";
        m_Cmd->Execute(cln);
    }

private:
    TConnections* m_Connections;
    ICtrlCmd<TClient>* m_Cmd;
};



END_NCBI_SCOPE

#endif // CONNECT_SERVICES__SRV_CONNECTIONS_HPP
