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
 * Author:  Maxim Didenko
 *
 * File Description:
 */

#include <ncbi_pch.hpp>
#include <connect/services/netservice_api.hpp>
#include <connect/services/netservice_api_expt.hpp>


BEGIN_NCBI_SCOPE

class CNetServiceAuthenticator : public CNetSrvConnector::IEventListener
{
public:
    explicit CNetServiceAuthenticator(const INetServiceAPI& net_srv_client)
        : m_NetSrvClient(net_srv_client) {}
    
    virtual void OnConnected(CNetSrvConnector& conn) {
        m_NetSrvClient.DoAuthenticate(conn);
    }
    
private:            
    const INetServiceAPI& m_NetSrvClient;
};


INetServiceAPI::INetServiceAPI(const string& service_name, const string& client_name)
    : m_ServiceName(service_name), m_ClientName(client_name), 
      m_ConnMode(eCloseConnection), m_LPServices(false), m_RebalanceStrategy(NULL)
{
}

INetServiceAPI::~INetServiceAPI()
{
}
INetServiceAPI::EConnectionMode INetServiceAPI::GetConnMode() const
{
    return m_ConnMode;
}

void INetServiceAPI::SetConnMode(EConnectionMode conn_mode)
{
    if (m_ConnMode == conn_mode )
        return;
    m_ConnMode = conn_mode;
    if( m_Poll.get())
        m_Poll->UsePermanentConnection(conn_mode == eKeepConnection);
}

void INetServiceAPI::DiscoverLowPriorityServers(bool on_off)
{
    if (m_LPServices == on_off)
        return;
    m_LPServices = on_off;
    if ( m_Poll.get())
        m_Poll->DiscoverLowPriorityServers(on_off);
}

void INetServiceAPI::SetRebalanceStrategy(IRebalanceStrategy* strategy, EOwnership owner)
{
    m_RebalanceStrategy = strategy;
    if (owner == eTakeOwnership)
        m_RebalanceStrategyGuard.reset(m_RebalanceStrategy);
    else
        m_RebalanceStrategyGuard.reset();
    if (m_Poll.get()) 
        m_Poll->SetRebalanceStrategy(m_RebalanceStrategy);    
}

void  INetServiceAPI::x_CreatePoll()
{
    if (!m_Authenticator.get())
        m_Authenticator.reset(new CNetServiceAuthenticator(*this));
    if (!m_RebalanceStrategy ) {
        m_RebalanceStrategyGuard.reset(new CSimpleRebalanceStrategy(50,10));
        m_RebalanceStrategy = m_RebalanceStrategyGuard.get();
    }
    m_Poll.reset(new CNetSrvConnectorPoll(m_ServiceName, 
                                          m_Authenticator.get(), 
                                          m_RebalanceStrategy));
    m_Poll->UsePermanentConnection(m_ConnMode == eKeepConnection);
    m_Poll->DiscoverLowPriorityServers(m_LPServices);
}

CNetSrvConnectorPoll& INetServiceAPI::GetPoll() 
{
    if (!m_Poll.get()) {
        x_CreatePoll();
    }
    return *m_Poll;
}
CNetSrvConnectorPoll& INetServiceAPI::GetPoll() const
{
    return const_cast<INetServiceAPI&>(*this).GetPoll();
}


string INetServiceAPI::GetConnectionInfo(CNetSrvConnector& conn) const
{
    if( !GetPoll().IsLoadBalanced())
        return GetServiceName();
    else
        return conn.GetHost() + ':' + NStr::UIntToString(conn.GetPort()) 
            + '(' + GetServiceName() + ')';
}

/* static */
void INetServiceAPI::TrimErr(string& err_msg)
{
    if (err_msg.find("ERR:") == 0) {
        err_msg.erase(0, 4);
        err_msg = NStr::ParseEscapes(err_msg);
    }
}


void INetServiceAPI::PrintServerOut(CNetSrvConnector& conn, CNcbiOstream& out) const
{
    conn.WaitForServer();
    string response;
    while (1) {
        if (!conn.ReadStr(response))
            break;
        CheckServerOK(response);
        if (response == "END")
            break;
        out << response << "\n";
    }
}


void INetServiceAPI::CheckServerOK(string& response) const
{
    if (NStr::StartsWith(response, "OK:")) {
        response.erase(0, 3); // "OK:"
    } else if (NStr::StartsWith(response, "ERR:")) {
        ProcessServerError(response, eTrimErr);
    }
}


void INetServiceAPI::ProcessServerError(string& response, ETrimErr trim_err) const
{
    if (trim_err == eTrimErr)
        TrimErr(response);
    NCBI_THROW(CNetServiceException, eCommunicationError, response);
}

string INetServiceAPI::SendCmdWaitResponse(CNetSrvConnector& conn, const string& cmd) const
{
    conn.WriteStr(cmd + "\r\n");
    return WaitResponse(conn);
}

string INetServiceAPI::WaitResponse(CNetSrvConnector& conn) const
{
    conn.WaitForServer();
    string tmp;
    if (!conn.ReadStr(tmp)) {
        NCBI_THROW(CNetServiceException, eCommunicationError, 
                   "Communication error");
    }
    CheckServerOK(tmp);
    return tmp;
    
}

void INetServiceAPI::x_CollectStreamOutput(const string& cmd, ISink& sink, INetServiceAPI::EStreamCollectorType type) const
{
    for (CNetSrvConnectorPoll::iterator it = GetPoll().begin(); 
         it != GetPoll().end(); ++it) {
        CNetSrvConnectorHolder ch = *it;
        if ( type == eSendCmdWaitResponse ) {  
            CNcbiOstream& os = sink.GetOstream(ch);
            os << SendCmdWaitResponse(ch, cmd);
        } else if ( type == ePrintServerOut ) {
            CNcbiOstream& os = sink.GetOstream(ch);
            ch->WriteStr(cmd + "\r\n");
            PrintServerOut(ch, os);
        } else {
            _ASSERT(false);
        }
        sink.EndOfData(ch);
    }        
}


END_NCBI_SCOPE
