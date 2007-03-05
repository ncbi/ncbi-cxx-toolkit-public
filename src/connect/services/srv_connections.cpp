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
#include <connect/ncbi_conn_exception.hpp>
#include <corelib/ncbi_system.hpp>
#include <connect/services/srv_connections.hpp>

#include <connect/ncbi_service.h>

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
    }
}

/************************************************************************/

static STimeout s_DefaultCommTimeout = {12, 0};

CNetSrvConnector::CNetSrvConnector(const string& host, unsigned short port)
    : m_Host(host), m_Port(port), m_Timeout(s_DefaultCommTimeout), m_EventListener(NULL)
{
}
CNetSrvConnector::~CNetSrvConnector()
{
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
                   "Communication timeout reading from server.");
        break;
    default: // invalid socket or request, bailing out
        return false;
    }
    return true;    
}

void CNetSrvConnector::WriteStr(const string& str)
{
    WriteBuf(str.data(), str.size());
    /*
    x_CheckConnect();
    const char* buf_ptr = &str[0];
    size_t size_to_write = str.size();
    while (size_to_write) {
        size_t n_written;
        EIO_Status io_st = m_Socket->Write(buf_ptr, size_to_write, &n_written);
        NCBI_IO_CHECK(io_st);
        size_to_write -= n_written;
        buf_ptr       += n_written;
    } // while
    */
}

void  CNetSrvConnector::WriteBuf(const void* buf, size_t len)
{
    x_CheckConnect();
    const char* buf_ptr = (const char*)buf;
    size_t size_to_write = len;
    while (size_to_write) {
        size_t n_written;
        EIO_Status io_st = m_Socket->Write(buf_ptr, size_to_write, &n_written);
        NCBI_IO_CHECK(io_st);
        size_to_write -= n_written;
        buf_ptr       += n_written;
    } // while
}

void CNetSrvConnector::WaitForServer(unsigned wait_sec)
{
    _ASSERT(x_IsConnected());

    STimeout to = {wait_sec, 0};
    while (true) {
        EIO_Status io_st = m_Socket->Wait(eIO_Read, &to);
        if (io_st == eIO_Timeout) {
            NCBI_THROW(CNetSrvConnException, eResponseTimeout, 
                       "No response from the server");
        }
        else {
            break;
        }
    }
}

void CNetSrvConnector::Telnet( CNcbiOstream& out, const string& stop_string)
{
    _ASSERT(x_IsConnected());

    STimeout rto;
    rto.sec = 1;
    rto.usec = 0;
    m_Socket->SetTimeout(eIO_Read, &rto);

    string line;
    while (1) {

        EIO_Status st = m_Socket->ReadLine(line);       
        if (st == eIO_Success) {
            if (!stop_string.empty() && line == stop_string)
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
        m_Socket.reset(new CSocket);

    unsigned conn_repeats = 0;
    const unsigned max_repeats = 10;
   
    do {
        EIO_Status io_st = m_Socket->Connect(m_Host, m_Port, &m_Timeout, eOn);
        if (io_st != eIO_Success) {
            if (io_st == eIO_Unknown) {

                m_Socket->Close();
                
                // most likely this is an indication we have too many 
                // open ports on the client side 
                // (this kernel limitation manifests itself on Linux)
                //
                
                if (++conn_repeats > max_repeats) {
                    if ( io_st != eIO_Success) {
                        throw CIO_Exception(DIAG_COMPILE_INFO,
                        0, (CIO_Exception::EErrCode)io_st, 
                            "IO error. Failed to connect to server.");
                    }
                }
                // give system a chance to recover
                
                SleepMilliSec(1000 * conn_repeats);
            } else {
                NCBI_IO_CHECK(io_st);
            }
        } else {
            break;
        }
        
    } while (1);

    m_Socket->SetDataLogging(eDefault);
    m_Socket->SetTimeout(eIO_ReadWrite, &m_Timeout);
    m_Socket->DisableOSSendDelay();

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

void CNetSrvConnector::Disconnect()
{
    if (x_IsConnected()) {
        m_Socket->Close();
        if (m_EventListener)
            m_EventListener->OnDisconnected(*this);
    }
}


CNetSrvConnectorPoll::CNetSrvConnectorPoll(const string& service, 
                                           CNetSrvConnector::IEventListener* event_listener,
                                           IRebalanceStrategy* rebalance_strategy)
    : m_ServiceName(service), m_RebalanceStrategy(rebalance_strategy), m_IsLoadBalanced(false),
      m_DiscoverLowPriorityServers(false), m_EventListener(event_listener),
      m_PermConn(false)
{
    string sport, host;
    if ( NStr::SplitInTwo(service, ":", host, sport) ) {
        unsigned int port = NStr::StringToInt(sport);
        m_Services.push_back(TService(host, port));
        m_IsLoadBalanced = false;
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
        m_Connections[srv] = conn;
    }
    if( m_RebalanceStrategy ) m_RebalanceStrategy->OnResourceRequested(*conn);
    return conn;
}

CNetSrvConnectorHolder CNetSrvConnectorPoll::GetBest(const string& hit)
{
    x_Rebalance();
    const TService& srv = m_Services[0];
    return CNetSrvConnectorHolder(*x_FindOrCreateConnector(srv), !m_PermConn);
}

CNetSrvConnectorHolder CNetSrvConnectorPoll::GetSpecific(const string& host, unsigned int port)
{
    return CNetSrvConnectorHolder(*x_FindOrCreateConnector(TService(host,port)), !m_PermConn);
}

void CNetSrvConnectorPoll::x_Rebalance()
{
    if (!m_IsLoadBalanced)
        return;
    if( !m_RebalanceStrategy || m_RebalanceStrategy->NeedRebalance()) {
        m_Services.clear();
        DiscoverLBServices(m_ServiceName, m_Services, m_DiscoverLowPriorityServers);               
    }
}


END_NCBI_SCOPE
