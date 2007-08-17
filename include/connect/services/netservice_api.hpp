#ifndef CONNECT_SERVICES___NETSERVICE_API__HPP
#define CONNECT_SERVICES___NETSERVICE_API__HPP

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
 * Authors:  Maxim Didenko
 *
 * File Description:
 *   Network client for ICache (NetCache).
 *
 */

#include <connect/services/srv_connections.hpp>

BEGIN_NCBI_SCOPE

class CNetSrvAuthenticator;
class NCBI_XCONNECT_EXPORT INetServiceAPI
{
public:
    INetServiceAPI(const string& service_name, const string& client_name);
    virtual ~INetServiceAPI();


    /// Connection management options
    enum EConnectionMode {
        /// Close connection after each call (default). 
        /// This mode frees server side resources, but reconnection can be
        /// costly because of the network overhead
        eCloseConnection,  

        /// Keep connection open.
        /// This mode occupies server side resources(session thread), 
        /// use this mode very carefully
        eKeepConnection    
    };
    /// Returns current connection mode
    /// @sa SetConnMode 
    EConnectionMode GetConnMode() const;

    /// Set connection mode
    /// @sa GetConnMode
    void SetConnMode(EConnectionMode conn_mode);

    const string& GetClientName() const { return m_ClientName; }
    const string& GetServiceName() const { return m_ServiceName; }

    void DiscoverLowPriorityServers(bool on_off);

    void SetRebalanceStrategy( IRebalanceStrategy* strategy, 
                               EOwnership owner = eTakeOwnership);

    string WaitResponse(CNetSrvConnector& conn) const;
    string SendCmdWaitResponse(CNetSrvConnector& conn, const string& cmd) const;

    CNetSrvConnectorPoll& GetPoll();
    CNetSrvConnectorPoll& GetPoll() const;

    void SetCommunicationTimeout(const STimeout& to) { GetPoll().SetCommunicationTimeout(to); }
    STimeout GetCommunicationTimeout() const { return GetPoll().GetCommunicationTimeout(); }

    void SetCreateSocketMaxRetries(unsigned int retries) { GetPoll().SetCreateSocketMaxRetries(retries); }
    unsigned int GetCreateSocketMaxRetries() const { return GetPoll().GetCreateSocketMaxRetries(); }

    //protected:
    void CheckServerOK(string& response) const;

    //protected:
    static void TrimErr(string& err_msg);

    
    class ISink 
    {
    public:
        virtual ~ISink() {};
        virtual CNcbiOstream& GetOstream(CNetSrvConnector& conn) = 0;
        virtual void EndOfData(CNetSrvConnector& conn) {}
    };
    enum EStreamCollectorType {
        eSendCmdWaitResponse,
        ePrintServerOut
    };
    void x_CollectStreamOutput(const string& cmd, ISink& sink, EStreamCollectorType type) const;

protected:
    enum ETrimErr {
        eNoTrimErr,
        eTrimErr
    };

    virtual void ProcessServerError(string& response, ETrimErr trim_err) const;

    void PrintServerOut(CNetSrvConnector& conn, CNcbiOstream& out) const;
    
    string GetConnectionInfo(CNetSrvConnector& conn) const;


private:
    friend class CNetServiceAuthenticator;
    void DoAuthenticate(CNetSrvConnector& conn) const {
        x_SendAuthetication(conn);
    }
    virtual void x_SendAuthetication(CNetSrvConnector& conn) const = 0;

    void x_CreatePoll();

    string m_ServiceName;
    string m_ClientName;

    auto_ptr<CNetSrvConnector::IEventListener> m_Authenticator;
    auto_ptr<CNetSrvConnectorPoll> m_Poll;
    EConnectionMode m_ConnMode;
    bool m_LPServices;
    IRebalanceStrategy* m_RebalanceStrategy;
    auto_ptr<IRebalanceStrategy> m_RebalanceStrategyGuard;

    INetServiceAPI(const INetServiceAPI&);
    INetServiceAPI& operator=(const INetServiceAPI&);

};


END_NCBI_SCOPE

#endif  /* CONNECT_SERVICES___NETSERVICE_API__HPP */
