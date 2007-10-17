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
 *   Government have not placed any restriction on its use or reproduction.
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
 * Authors:  Maxim Didenko
 *
 * File Description:
 *
 */

#include <ncbi_pch.hpp>
#include <connect/ncbi_socket.hpp>
#include <corelib/ncbi_param.hpp>
#include <connect/ncbi_conn_exception.hpp>
#include <corelib/ncbi_system.hpp>
#include <connect/services/srv_connections_expt.hpp>
#include <connect/services/srv_connections.hpp>
#include <connect/services/error_codes.hpp>
#include <connect/ncbi_service.h>
 

#define NCBI_USE_ERRCODE_X   ConnServ_Connection

BEGIN_NCBI_SCOPE

void NCBI_XCONNECT_EXPORT DiscoverLBServices(const string& service_name, 
                                             vector<pair<string,unsigned short> >& services,
                                             bool all_services)
{
    //    cout << "DiscoverLBServicesDiscoverLBServices" <<endl;
    SConnNetInfo* net_info = ConnNetInfo_Create(service_name.c_str());
    TSERV_Type stype = fSERV_Any;
    if (all_services)
        stype |= fSERV_IncludeSuppressed;

    SERV_ITER srv_it = SERV_Open(service_name.c_str(), stype, 0, net_info);
    ConnNetInfo_Destroy(net_info);

    if (srv_it != 0) {
        const SSERV_Info* sinfo;
        while ((sinfo = SERV_GetNextInfoEx(srv_it, 0)) != 0) {
            
            // string host = CSocketAPI::gethostbyaddr(sinfo->host);
            string host = CSocketAPI::ntoa(sinfo->host);
            //cout << "DiscoverLBServices: " << host <<endl;
            // string::size_type pos = host.find_first_of(".");
            // if (pos != string::npos) {
            //    host.erase(pos, host.size());
            // }
            services.push_back(pair<string,unsigned short>(host, sinfo->port));
        } // while
        SERV_Close(srv_it);
    } else {
        NCBI_THROW(CNetSrvConnException, eLBNameNotFound, 
                   "Load balancer cannot find service name " + service_name + ".");
    }
}

/************************************************************************/

NCBI_PARAM_DECL(double,  service_connector, communication_timeout);
NCBI_PARAM_DEF (double,  service_connector, communication_timeout, 12.0);
typedef NCBI_PARAM_TYPE(service_connector, communication_timeout) TServConn_CommTimeout;

NCBI_PARAM_DECL(bool,   service_connector, use_linger2);
NCBI_PARAM_DEF (bool,   service_connector, use_linger2, false);
typedef NCBI_PARAM_TYPE(service_connector, use_linger2) TServConn_UserLinger2;

NCBI_PARAM_DECL(unsigned int, service_connector, connection_max_retries);
NCBI_PARAM_DEF (unsigned int, service_connector, connection_max_retries, 10);
typedef NCBI_PARAM_TYPE(      service_connector, connection_max_retries) TServConn_ConnMaxRetries;

NCBI_PARAM_DECL(int,    service_connector, max_find_lbname_retries);
NCBI_PARAM_DEF (int,    service_connector, max_find_lbname_retries, 3);
typedef NCBI_PARAM_TYPE(service_connector, max_find_lbname_retries) TServConn_MaxFineLBNameRetries;

NCBI_PARAM_DECL(int,    service_connector, max_connectors_pool_size);
NCBI_PARAM_DEF (int,    service_connector, max_connectors_pool_size, 0); // unlimited 
typedef NCBI_PARAM_TYPE(service_connector, max_connectors_pool_size) TServConn_MaxConnPoolSize;

static bool s_DefaultCommTimeout_Initialized = false;
static STimeout s_DefaultCommTimeout;

static STimeout s_GetDefaultCommTimeout()
{
   if (s_DefaultCommTimeout_Initialized)
       return s_DefaultCommTimeout;
   double ftm = TServConn_CommTimeout::GetDefault();
   NcbiMsToTimeout(&s_DefaultCommTimeout, (unsigned long)(ftm * 1000.0 + 0.5));
   s_DefaultCommTimeout_Initialized = true;
   return s_DefaultCommTimeout;
}
static void s_SetDefaultCommTimeout(const STimeout& tm) 
{
    s_DefaultCommTimeout = tm; 
    s_DefaultCommTimeout_Initialized = true;
}

/*************************************************************************/

CNetServerConnector::~CNetServerConnector()
{
    try {
        Disconnect();
    } catch (...) {
    }
}

bool CNetServerConnector::ReadStr(string& str)
{
    if (!x_IsConnected())
        return false;

    EIO_Status io_st = m_Socket->ReadLine(str);
    switch (io_st) 
    {
    case eIO_Success:
        return true;
    case eIO_Timeout:
        NCBI_THROW(CNetSrvConnException, eReadTimeout, 
                   "Communication timeout reading from server " + m_Parent.GetHost() + ":" 
                   + NStr::UIntToString(m_Parent.GetPort()) + ".");
        break;
    default: // invalid socket or request, bailing out
        return false;
    }
    return true;    
}
void CNetServerConnector::WriteStr(const string& str)
{
    WriteBuf(str.data(), str.size());    
}
    
void CNetServerConnector::WriteBuf(const void* buf, size_t len)
{
    x_CheckConnect();
    const char* buf_ptr = (const char*)buf;
    size_t size_to_write = len;
    while (size_to_write) {
        size_t n_written;
        EIO_Status io_st = m_Socket->Write(buf_ptr, size_to_write, &n_written);
        if ( io_st != eIO_Success) {
            CIO_Exception io_ex(DIAG_COMPILE_INFO,  0, (CIO_Exception::EErrCode)io_st,  "IO error.");
            NCBI_THROW(CNetSrvConnException, eWriteFailure, 
                        "Failed to write to server " + m_Parent.GetHost() + ":" 
                        + NStr::UIntToString(m_Parent.GetPort()) + 
                        ". Reason: " + string(io_ex.what()));
        }
        size_to_write -= n_written;
        buf_ptr       += n_written;
    } // while
}
     // if wait_sec is set to 0 m_Timeout will be used
void CNetServerConnector::WaitForServer(unsigned int wait_sec)
{
    _ASSERT(x_IsConnected());

    STimeout to = {wait_sec, 0};
    if (wait_sec == 0)
        to = m_Parent.GetCommunicationTimeout();
    while (true) {
        EIO_Status io_st = m_Socket->Wait(eIO_Read, &to);
        if (io_st == eIO_Timeout) {
            NCBI_THROW(CNetSrvConnException, eResponseTimeout, 
                       "No response from the server " + m_Parent.GetHost() + ":" 
                       + NStr::UIntToString(m_Parent.GetPort()) + ".");
        }
        else {
            break;
        }
    }
}


CSocket& CNetServerConnector::GetSocket()
{
    _ASSERT(x_IsConnected());    
    return *m_Socket;
}

CNetServerConnector::CNetServerConnector(CNetServerConnectors& parent)
    :m_Parent(parent), m_WasConnected(false)
{
}

void CNetServerConnector::x_CheckConnect()
{
    INetServerConnectorEventListener* listener = m_Parent.GetEventListener();
    if (x_IsConnected())
        return;

    if (m_WasConnected && listener ) listener->OnDisconnected(*this);
    m_WasConnected = false;
 
    if (!m_Socket.get()) {
        m_Socket.reset(new CSocket);
        if ( TServConn_UserLinger2::GetDefault() ) {
            STimeout zero = {0,0};
            m_Socket->SetTimeout(eIO_Close,&zero);
        }
    }

    unsigned conn_repeats = 0;
   
    do {
        EIO_Status io_st = m_Socket->Connect(m_Parent.GetHost(), m_Parent.GetPort(), 
                                             &m_Parent.GetCommunicationTimeout(), eOn);
        if (io_st != eIO_Success) {
            if (io_st == eIO_Unknown) {

                m_Socket->Close();
                
                // most likely this is an indication we have too many 
                // open ports on the client side 
                // (this kernel limitation manifests itself on Linux)
                //
                
                if (++conn_repeats > m_Parent.GetCreateSocketMaxRetries()) {
                    if ( io_st != eIO_Success) {
                        CIO_Exception io_ex(DIAG_COMPILE_INFO,  0, (CIO_Exception::EErrCode)io_st,  "IO error.");
                        NCBI_THROW(CNetSrvConnException, eConnectionFailure, 
                                "Failed to connect to server " + m_Parent.GetHost() + ":" 
                                + NStr::UIntToString(m_Parent.GetPort()) + 
                                ". Reason: " + string(io_ex.what()));
                    }
                }
                // give system a chance to recover
                
                SleepMilliSec(1000 * conn_repeats);
            } else {
                CIO_Exception io_ex(DIAG_COMPILE_INFO,  0, (CIO_Exception::EErrCode)io_st,  "IO error.");
                NCBI_THROW(CNetSrvConnException, eConnectionFailure, 
                          "Failed to connect to server " + m_Parent.GetHost() + ":" 
                          + NStr::UIntToString(m_Parent.GetPort()) + 
                          ". Reason: " + string(io_ex.what()));
            }
        } else {
            break;
        }
         
    } while (1);

    m_Socket->SetDataLogging(eDefault);
    m_Socket->SetTimeout(eIO_ReadWrite, &m_Parent.GetCommunicationTimeout());
    m_Socket->DisableOSSendDelay();
//    m_Socket->SetReuseAddress(eOn);

    if (listener)
        listener->OnConnected(*this);
    m_WasConnected = true;
}

bool CNetServerConnector::x_IsConnected()
{
    if (!m_Socket.get()) return false;
    EIO_Status st = m_Socket->GetStatus(eIO_Open);
    if (st != eIO_Success)
        return false;

    STimeout zero = {0, 0}, tmo;
    const STimeout* tmp = m_Socket->GetTimeout(eIO_Read);
    if (tmp) {
        tmo = *tmp;
        tmp = &tmo;
    }
    if (m_Socket->SetTimeout(eIO_Read, &zero) != eIO_Success)
        return false;

    EIO_Status io_st = m_Socket->Read(0,1,0,eIO_ReadPeek);
    bool ret = false;
    switch (io_st) {
    case eIO_Closed:
        m_Socket->Close();
        return false;
    case eIO_Success:
        if (m_Socket->GetStatus(eIO_Read) != eIO_Success) 
            break;
    case eIO_Timeout:
        ret = true;
    default:        
        break;
    }
    if (m_Socket->SetTimeout(eIO_Read, tmp) != eIO_Success)
        return false;
    return ret;
}

void CNetServerConnector::x_ReturnToParent()
{
    m_Parent.x_TakeConnector(this);
}

void CNetServerConnector::Disconnect()
{
    if (x_IsConnected())
        m_Socket->Close();

    INetServerConnectorEventListener* listener = m_Parent.GetEventListener();
    if (listener && m_WasConnected) listener->OnDisconnected(*this);
    m_WasConnected = false;
}

void CNetServerConnector::Telnet(CNcbiOstream* out,  CNetServerConnector::IStringProcessor* processor)
{
    _ASSERT(x_IsConnected());

    STimeout rto = {1, 0};
    m_Socket->SetTimeout(eIO_Read, &rto);

    string line;
    while (1) {
        EIO_Status st = m_Socket->ReadLine(line);       
        if (st == eIO_Success) {
            if (processor && !processor->Process(line))
                break;
            if (out) *out << line << "\n" << flush;
        } else {
            EIO_Status st = m_Socket->GetStatus(eIO_Open);
            if (st != eIO_Success) {
                break;
            }
        }
    }
    Disconnect();
}

/*************************************************************************/
CNetServerConnectors::CNetServerConnectors(const string& host, unsigned short port,
                      INetServerConnectorEventListener* listener)
    : m_Host(host), m_Port(port), m_Timeout(s_GetDefaultCommTimeout()), 
      m_EventListener(listener),
      m_MaxRetries(TServConn_ConnMaxRetries::GetDefault()), m_ServerConnectorFactory(*this),
      m_PermanentConnection(eDefault)
{
    m_ServerConnectorPool.reset(new TConnectorPool(TServConn_MaxConnPoolSize::GetDefault(), m_ServerConnectorFactory));
}
void CNetServerConnectors::x_TakeConnector(CNetServerConnector* connector)
{
    _ASSERT(connector);
    if (m_PermanentConnection != eOn) 
        connector->Disconnect();
    else {
       // we need to check if all data has been read.
       // if it has not then we have to close the connection
        STimeout to = {0, 0};
        CSocket* socket = connector->m_Socket.get();
        if (socket && socket->Wait(eIO_Read, &to) == eIO_Success)
            connector->Disconnect();
    }
    m_ServerConnectorPool->Put(connector);
}

CNetServerConnectorHolder CNetServerConnectors::GetConnector()
{
    return CNetServerConnectorHolder(*m_ServerConnectorPool->Get());
}
void CNetServerConnectors::SetCommunicationTimeout(const STimeout& to)
{
    m_Timeout = to;
    //
    //NON_CONST_ITERATE(TConnectorPool::TPoolList, it, m_ServerConnectorPool->GetFreeList()) {
    //    (*it)->x_SetCommunicationTimeout(m_Timeout);       
    //}
}

/* static */
void CNetServerConnectors::SetDefaultCommunicationTimeout(const STimeout& to)
{
    s_SetDefaultCommTimeout(to);
}
/* static */
void CNetServerConnectors::SetDefaultCreateSocketMaxReties(unsigned int retries)
{
    TServConn_ConnMaxRetries::SetDefault(retries);
}


/*************************************************************************/


CNetServiceConnector::CNetServiceConnector(const string& service,
                                 INetServerConnectorEventListener* listener)
    : m_ServiceName(service), m_IsLoadBalanced(false), 
      m_DiscoverLowPriorityServers(eOff), m_Timeout(s_GetDefaultCommTimeout()),
      m_EventListener(listener), 
      m_MaxRetries(TServConn_ConnMaxRetries::GetDefault()), 
      m_PermanentConnection(eOff)
{
    if (service.empty())
        return;

    string sport, host;
    if (NStr::SplitInTwo(service, ":", host, sport)) {
       unsigned int port = NStr::StringToInt(sport);
       host = CSocketAPI::ntoa(CSocketAPI::gethostbyname(host));
       CWriteLockGuard g(m_ServersLock);
       m_Servers.push_back(TServer(host, port));
    } else {
       m_IsLoadBalanced = true;
    }
}
CNetServiceConnector::~CNetServiceConnector()
{
    CFastMutexGuard g(m_ConnectorsMutex);
    ITERATE( TCont, it, m_Connectors) {
        delete it->second;
    }
}

CNetServerConnectorHolder CNetServiceConnector::GetBest(const TServer* backup, const string& hit)
{
    if (!m_IsLoadBalanced) {
        CReadLockGuard g(m_ServersLock);
        if (m_Servers.empty()) 
            NCBI_THROW(CNetSrvConnException, eSrvListEmpty, "The service is not set.");
        return x_FindOrCreateConnector(m_Servers[0]).GetConnector();
    }
    try {
        x_Rebalance();
    } catch (CNetSrvConnException& ex) {
        if (ex.GetErrCode() != CNetSrvConnException::eLBNameNotFound || !backup)
            throw;
        ERR_POST_X(1, "Connecting to backup server " << backup->first << ":" << backup->second << ".");
        return x_FindOrCreateConnector(*backup).GetConnector();
    }
    CReadLockGuard g(m_ServersLock);
    TServers servers_copy(m_Servers);
    g.Release();
    ITERATE(TServers, it, servers_copy) {
        CNetServerConnectorHolder conn = x_FindOrCreateConnector(*it).GetConnector();
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
        ERR_POST_X(3, "Couldn't find any availbale servers for " << m_ServiceName 
                   << " service. Connecting to backup server " << backup->first << ":" << backup->second << ".");
        return x_FindOrCreateConnector(*backup).GetConnector();
    }
    NCBI_THROW(CNetSrvConnException, eSrvListEmpty, "Couldn't find any availbale servers for " 
                                                    + m_ServiceName + " service.");
}

CNetServerConnectorHolder CNetServiceConnector::GetSpecific(const string& host, unsigned int port)
{
    string x_host = CSocketAPI::ntoa(CSocketAPI::gethostbyname(host));
    return x_FindOrCreateConnector(TServer(x_host,port)).GetConnector();
}

void CNetServiceConnector::SetCommunicationTimeout(const STimeout& to)
{
    CFastMutexGuard g(m_ConnectorsMutex);
    m_Timeout = to;
    NON_CONST_ITERATE( TCont, it, m_Connectors) {
        it->second->SetCommunicationTimeout(to);
    }
}
const STimeout& CNetServiceConnector::GetCommunicationTimeout() const 
{ 
    return m_Timeout; 
}
      
void CNetServiceConnector::SetCreateSocketMaxRetries(unsigned int retries) 
{ 
    CFastMutexGuard g(m_ConnectorsMutex);
    m_MaxRetries = retries;
    NON_CONST_ITERATE( TCont, it, m_Connectors) {
        it->second->SetCreateSocketMaxRetries(retries);
    }
}

unsigned int CNetServiceConnector::GetCreateSocketMaxRetries() const 
{ 
    return m_MaxRetries; 
}

void CNetServiceConnector::PermanentConnection(ESwitch type) 
{ 
    CFastMutexGuard g(m_ConnectorsMutex);
    m_PermanentConnection = type; 
    NON_CONST_ITERATE( TCont, it, m_Connectors) {
        it->second->PermanentConnection(type);
    }
}
void CNetServiceConnector::DiscoverLowPriorityServers(ESwitch on_off) 
{ 
    m_DiscoverLowPriorityServers = on_off; 
    if (m_RebalanceStrategy) m_RebalanceStrategy->Reset();
}

void CNetServiceConnector::SetRebalanceStrategy(IRebalanceStrategy* strategy)
{
    m_RebalanceStrategy = strategy;
}

CNetServerConnectors& CNetServiceConnector::x_FindOrCreateConnector(const TServer& srv) const
{
    CFastMutexGuard g(m_ConnectorsMutex);
    TCont::iterator it = m_Connectors.find(srv);
    CNetServerConnectors* conn = NULL;
    if(it != m_Connectors.end())
        conn = it->second;
    if( conn == NULL ) {
        conn = new CNetServerConnectors(srv.first,srv.second, m_EventListener);
        conn->SetCommunicationTimeout(m_Timeout);
        conn->PermanentConnection(m_PermanentConnection);
        m_Connectors[srv] = conn;
    }
    g.Release();
    if( m_RebalanceStrategy ) m_RebalanceStrategy->OnResourceRequested(*conn);
    return *conn;
}

void CNetServiceConnector::x_Rebalance()
{
    if (!m_IsLoadBalanced)
        return;
    if( !m_RebalanceStrategy || m_RebalanceStrategy->NeedRebalance()) {
        CWriteLockGuard g(m_ServersLock);
        m_Servers.clear();
        int try_count = 0;
        while(true) {
            try {
                DiscoverLBServices(m_ServiceName, m_Servers, m_DiscoverLowPriorityServers == eOn);
                break;
            } catch (CNetSrvConnException& ex) {
                if (ex.GetErrCode() != CNetSrvConnException::eLBNameNotFound)
                    throw;
                ERR_POST_X(4, "Communication Error : " << ex.what());
                if (++try_count > TServConn_MaxFineLBNameRetries::GetDefault())
                    throw;
                SleepMilliSec(1000 + try_count*2000);
            }
        }
    }
}

END_NCBI_SCOPE

