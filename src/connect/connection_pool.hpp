#ifndef CONNECT___CONNECTION_POOL__HPP
#define CONNECT___CONNECTION_POOL__HPP

/* $Id$
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
 * Authors:  Aaron Ucko, Victor Joukov
 *
 */

/// @file connection_pool.hpp
/// Internal header for threaded server connection pools.


#include <connect/impl/server_connection.hpp>


/** @addtogroup ThreadedServer
 *
 * @{
 */


BEGIN_NCBI_SCOPE


class CServer_ConnectionPool
{
public:
    CServer_ConnectionPool(unsigned max_connections);
    ~CServer_ConnectionPool();

    typedef IServer_ConnectionBase TConnBase;
    typedef CServer_Connection     TConnection;
    typedef CServer_Listener       TListener;

    void SetMaxConnections(unsigned max_connections) {
        m_MaxConnections = max_connections;
    }

    bool Add(TConnBase* conn, EServerConnType type);
    void Remove(TConnBase* conn);
    bool RemoveListener(unsigned short  port);
    void PingControlConnection(void);

    /// Guard connection from out-of-order packet processing by
    /// pulling eActiveSocket's from poll vector
    /// Resets the expiration time as a bonus.
    void SetConnType(TConnBase* conn, EServerConnType type);
    void SetAllActive(const vector<CSocketAPI::SPoll>& polls);
    void SetAllActive(const vector<IServer_ConnectionBase*>& conns);

    /// Close connection as if it was initiated by server (not by client).
    void CloseConnection(TConnBase* conn);

    /// Erase all connections
    void Erase(void);

    bool GetPollAndTimerVec(vector<CSocketAPI::SPoll>& polls,
                            vector<IServer_ConnectionBase*>& timer_requests,
                            STimeout* timer_timeout,
                            vector<IServer_ConnectionBase*>& revived_conns,
                            vector<IServer_ConnectionBase*>& to_close_conns,
                            vector<IServer_ConnectionBase*>& to_delete_conns);

    void StartListening(void);
    void StopListening(void);

    /// Provides a list of ports on which the server is listening
    /// @return
    ///  currently listened ports
    vector<unsigned short>  GetListenerPorts(void);

private:
    void x_UpdateExpiration(TConnBase* conn);


    typedef set<TConnBase*> TData;

    TData               m_Data;
    mutable CMutex      m_Mutex;
    unsigned int        m_MaxConnections;
    mutable CTrigger    m_ControlTrigger;

private:
    // A list of ports on which the listeners should be stopped.
    // A storage for the ports is needed because the listener deletion could
    // not be done synchronously - the listener needs to be taken out of a poll
    // vector.
    // The access to the container is protected with m_Mutex
    vector<unsigned short>  m_ListenerPortsToStop;
    bool                    m_ListeningStarted;
};


END_NCBI_SCOPE


/* @} */

#endif  /* CONNECT___CONNECTION_POOL__HPP */
