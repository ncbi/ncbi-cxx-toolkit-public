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
//NCBI_PARAM_DEF (double,  service_connector, communication_timeout, 12.0);
typedef NCBI_PARAM_TYPE(service_connector, communication_timeout) TServConn_CommTimeout;

NCBI_PARAM_DECL(bool,   service_connector, use_linger2);
//NCBI_PARAM_DEF (bool,   service_connector, use_linger2, false);
typedef NCBI_PARAM_TYPE(service_connector, use_linger2) TServConn_UserLinger2;

NCBI_PARAM_DECL(unsigned int, service_connector, connection_max_retries);
//NCBI_PARAM_DEF (unsigned int, service_connector, connection_max_retries, 10);
typedef NCBI_PARAM_TYPE(      service_connector, connection_max_retries) TServConn_ConnMaxRetries;

NCBI_PARAM_DECL(int,    service_connector, max_find_lbname_retries);
NCBI_PARAM_DEF (int,    service_connector, max_find_lbname_retries, 3);
typedef NCBI_PARAM_TYPE(service_connector, max_find_lbname_retries) TServConn_MaxFineLBNameRetries;

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

/************************************************************************/

/* static */
CSocket* CSocketFactory::Create() 
{
    auto_ptr<CSocket> socket( new CSocket );
    if ( TServConn_UserLinger2::GetDefault() ) {
        STimeout zero = {0,0};
        socket->SetTimeout(eIO_Close,&zero);
    }
    return socket.release(); 
}

CNetSrvConnector::CNetSrvConnector(const string& host, unsigned short port)
    : m_Host(host), m_Port(port), 
      m_Timeout(s_GetDefaultCommTimeout()), 
      m_EventListener(NULL),
      m_ReleaseSocket(false),
      m_MaxRetries(TServConn_ConnMaxRetries::GetDefault())	
{
}

CNetSrvConnector::CNetSrvConnector(const CNetSrvConnector& other, CSocket* socket) 
    : m_Host(other.m_Host),  m_Port(other.m_Port), 
      m_Timeout(other.m_Timeout),
      m_EventListener(other.m_EventListener), 
      m_ReleaseSocket(true),
      m_MaxRetries(other.m_MaxRetries)
{
    m_Socket.reset(socket);
}

CNetSrvConnector::~CNetSrvConnector()
{
    if (m_ReleaseSocket)
        m_Socket.release();
}


/* static */
void CNetSrvConnector::SetDefaultCommunicationTimeout(const STimeout& to)
{
    s_SetDefaultCommTimeout(to);
}
/* static */
void CNetSrvConnector::SetDefaultCreateSocketMaxReties(unsigned int retries)
{
    TServConn_ConnMaxRetries::SetDefault(retries);
}

bool CNetSrvConnector::ReadStr(string& str)
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
                   "Communication timeout reading from server " + GetHost() + ":" + NStr::UIntToString(GetPort()) + ".");
        break;
    default: // invalid socket or request, bailing out
        return false;
    }
    return true;    
}

void CNetSrvConnector::WriteStr(const string& str)
{
    WriteBuf(str.data(), str.size());
}

void  CNetSrvConnector::WriteBuf(const void* buf, size_t len)
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
                        "Failed to write to server " + GetHost() + ":" + NStr::UIntToString(GetPort()) + 
                        ". Reason: " + string(io_ex.what()));
        }
        size_to_write -= n_written;
        buf_ptr       += n_written;
    } // while
}

void CNetSrvConnector::WaitForServer(unsigned wait_sec)
{
    _ASSERT(x_IsConnected());

    STimeout to = {wait_sec, 0};
    if (wait_sec == 0)
        to = m_Timeout;
    while (true) {
        EIO_Status io_st = m_Socket->Wait(eIO_Read, &to);
        if (io_st == eIO_Timeout) {
            NCBI_THROW(CNetSrvConnException, eResponseTimeout, 
                       "No response from the server " + GetHost() + ":" + NStr::UIntToString(GetPort()) + ".");
        }
        else {
            break;
        }
    }
}

void CNetSrvConnector::Telnet( CNcbiOstream& out,  CNetSrvConnector::IStringProcessor* processor)
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
            out << line << "\n" << flush;
        } else {
            EIO_Status st = m_Socket->GetStatus(eIO_Open);
            if (st != eIO_Success) {
                break;
            }
        }
    }

}

void CNetSrvConnector::x_CheckConnect()
{
    if (x_IsConnected())
        return;

    if (!m_Socket.get()) 
        m_Socket.reset(m_SocketPool.Get());

    unsigned conn_repeats = 0;
   
    do {
        EIO_Status io_st = m_Socket->Connect(m_Host, m_Port, &m_Timeout, eOn);
        if (io_st != eIO_Success) {
            if (io_st == eIO_Unknown) {

                m_Socket->Close();
                
                // most likely this is an indication we have too many 
                // open ports on the client side 
                // (this kernel limitation manifests itself on Linux)
                //
                
                if (++conn_repeats > m_MaxRetries) {
                    if ( io_st != eIO_Success) {
                        CIO_Exception io_ex(DIAG_COMPILE_INFO,  0, (CIO_Exception::EErrCode)io_st,  "IO error.");
                        NCBI_THROW(CNetSrvConnException, eConnectionFailure, 
                                "Failed to connect to server " + GetHost() + ":" + NStr::UIntToString(GetPort()) + 
                                ". Reason: " + string(io_ex.what()));
                    }
                }
                // give system a chance to recover
                
                SleepMilliSec(1000 * conn_repeats);
            } else {
                CIO_Exception io_ex(DIAG_COMPILE_INFO,  0, (CIO_Exception::EErrCode)io_st,  "IO error.");
                NCBI_THROW(CNetSrvConnException, eConnectionFailure, 
                          "Failed to connect to server " + GetHost() + ":" + NStr::UIntToString(GetPort()) + 
                          ". Reason: " + string(io_ex.what()));
            }
        } else {
            break;
        }
        
    } while (1);

    m_Socket->SetDataLogging(eDefault);
    m_Socket->SetTimeout(eIO_ReadWrite, &m_Timeout);
    m_Socket->DisableOSSendDelay();
//    m_Socket->SetReuseAddress(eOn);

    if (m_EventListener)
        m_EventListener->OnConnected(*this);
}

bool CNetSrvConnector::x_IsConnected()
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


CSocket* CNetSrvConnector::DetachSocket() 
{ 
    _ASSERT(!m_ReleaseSocket);
    x_CheckConnect();
    return m_Socket.release(); 
}
void CNetSrvConnector::AttachSocket(CSocket* socket) 
{ 
    _ASSERT(socket);
    _ASSERT(!m_ReleaseSocket);
    if (!m_Socket.get())
        m_Socket.reset(socket);
    else
        m_SocketPool.Put(socket);
}

void CNetSrvConnector::Disconnect(CSocket* socket)
{
    if (socket) {
        CNetSrvConnector tmp(*this, socket);
        tmp.Disconnect();
    } else {
        if (x_IsConnected()) {
            if (m_EventListener)
                m_EventListener->OnDisconnected(*this);
            m_Socket->Close();
        }
    }
}


void CNetSrvConnector::SetCommunicationTimeout(const STimeout& to)
{
    m_Timeout = to;
    if (m_Socket.get()) {
        m_Socket->SetTimeout(eIO_ReadWrite, &m_Timeout);
    }
    NON_CONST_ITERATE(TResourcePool::TPoolList, it, m_SocketPool.GetFreeList()) {
        (*it)->SetTimeout(eIO_ReadWrite, &m_Timeout);       
    }
}


/////////////////////////////////////////////////////////////////////////////////////////
//
CNetSrvConnectorPoll::CNetSrvConnectorPoll(const string& service, 
                                           CNetSrvConnector::IEventListener* event_listener,
                                           IRebalanceStrategy* rebalance_strategy)
    : m_ServiceName(service), m_RebalanceStrategy(rebalance_strategy), 
      m_IsLoadBalanced(false), m_DiscoverLowPriorityServers(false), 
      m_EventListener(event_listener), m_PermConn(false), m_Timeout(s_GetDefaultCommTimeout()),
      m_MaxRetries(TServConn_ConnMaxRetries::GetDefault())

{
    if (service.empty())
        return;

    string sport, host;
    if ( NStr::SplitInTwo(service, ":", host, sport) ) {
        unsigned int port = NStr::StringToInt(sport);
        host = CSocketAPI::ntoa(CSocketAPI::gethostbyname(host));
        m_Services.push_back(TService(host, port));
    } else {
        //DiscoverLBServices(service, m_Services, m_DiscoverLowPriorityServers);
        m_IsLoadBalanced = true;
    }
}

CNetSrvConnectorPoll::~CNetSrvConnectorPoll()
{
    ITERATE( TCont, it, m_Connections) {
        delete it->second;
    }
}

CNetSrvConnector* CNetSrvConnectorPoll::x_FindOrCreateConnector(const TService& srv) const
{
    TCont::iterator it = m_Connections.find(srv);
    CNetSrvConnector* conn = NULL;
    if(it != m_Connections.end())
        conn = it->second;
    if( conn == NULL ) {
        conn = new CNetSrvConnector(srv.first,srv.second);
        conn->SetEventListener(m_EventListener);
        conn->SetCommunicationTimeout(m_Timeout);
        m_Connections[srv] = conn;
    }
    if( m_RebalanceStrategy ) m_RebalanceStrategy->OnResourceRequested(*conn);
    return conn;
}

CNetSrvConnectorHolder CNetSrvConnectorPoll::GetBest(const TService* backup, const string& hit)
{
    if (!m_IsLoadBalanced) {
        if (m_Services.empty()) 
            NCBI_THROW(CNetSrvConnException, eSrvListEmpty, "The service is not set.");
        return CNetSrvConnectorHolder(*x_FindOrCreateConnector(m_Services[0]), !m_PermConn);
    }
    try {
        x_Rebalance();
    } catch (CNetSrvConnException& ex) {
        if (ex.GetErrCode() != CNetSrvConnException::eLBNameNotFound || !backup)
            throw;
        ERR_POST_X(1, "Connecting to backup server " << backup->first << ":" << backup->second << ".");
        return CNetSrvConnectorHolder(*x_FindOrCreateConnector(*backup), !m_PermConn);
    }
    ITERATE(TServices, it, m_Services) {
        CNetSrvConnector* conn = x_FindOrCreateConnector(*it);
        try {
            conn->x_CheckConnect();
            return CNetSrvConnectorHolder(*conn, !m_PermConn);
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
        return CNetSrvConnectorHolder(*x_FindOrCreateConnector(*backup), !m_PermConn);
    }
    NCBI_THROW(CNetSrvConnException, eSrvListEmpty, "Couldn't find any availbale servers for " 
                                                    + m_ServiceName + " service.");
}

CNetSrvConnectorHolder CNetSrvConnectorPoll::GetSpecific(const string& host, unsigned int port)
{
    string x_host = CSocketAPI::ntoa(CSocketAPI::gethostbyname(host));
    return CNetSrvConnectorHolder(*x_FindOrCreateConnector(TService(x_host,port)), !m_PermConn);
}

void CNetSrvConnectorPoll::x_Rebalance()
{
    if (!m_IsLoadBalanced)
        return;
    if( !m_RebalanceStrategy || m_RebalanceStrategy->NeedRebalance()) {
        m_Services.clear();
        int try_count = 0;
        while(true) {
            try {
                DiscoverLBServices(m_ServiceName, m_Services, m_DiscoverLowPriorityServers);
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

void CNetSrvConnectorPoll::SetCommunicationTimeout(const STimeout& to)
{
    m_Timeout = to;
    NON_CONST_ITERATE(TCont, it, m_Connections) {
        it->second->SetCommunicationTimeout(m_Timeout);
    }

}

void CNetSrvConnectorPoll::SetCreateSocketMaxRetries(unsigned int retries)
{
    m_MaxRetries = retries;
    NON_CONST_ITERATE(TCont, it, m_Connections) {
        it->second->SetCreateSocketMaxRetries(retries);
    }

}


END_NCBI_SCOPE
