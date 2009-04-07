#ifndef CONNECT_SERVICES___SRV_CONNECTIONS_IMPL__HPP
#define CONNECT_SERVICES___SRV_CONNECTIONS_IMPL__HPP

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
 * Authors:  Dmitry Kazimirov
 *
 * File Description:
 *
 */

#include <connect/services/netservice_api.hpp>

#include <corelib/ncbimtx.hpp>

BEGIN_NCBI_SCOPE

struct SNetServerCmdOutputImpl : public CNetObject
{
    SNetServerCmdOutputImpl(SNetServerConnectionImpl* connection_impl);

    virtual ~SNetServerCmdOutputImpl();

    void SetNetCacheCompatMode() {m_NetCacheCompatMode = true;}

    CNetServerConnection m_Connection;

    bool m_NetCacheCompatMode;
    bool m_ReadCompletely;
};

struct SNetServerImpl;

struct SNetServerConnectionImpl : public CNetObject
{
    SNetServerConnectionImpl(SNetServerImpl* pool);

    virtual void Delete();

    std::string ReadCmdOutputLine();

    void WriteLine(const string& line);

     // if wait_sec is set to 0 m_Timeout will be used
    void WaitForServer(unsigned int wait_sec = 0);

    void Close();
    void Abort();

    virtual ~SNetServerConnectionImpl();

    // The server this connection is connected to.
    CNetObjectRef<SNetServerImpl> m_Server;
    SNetServerConnectionImpl* m_NextFree;

    CSocket m_Socket;
};

inline SNetServerCmdOutputImpl::SNetServerCmdOutputImpl(
    SNetServerConnectionImpl* connection_impl) :
        m_Connection(connection_impl),
        m_NetCacheCompatMode(false),
        m_ReadCompletely(false)
{
}


class INetServerConnectionListener : public CNetObject
{
public:
    virtual void OnConnected(CNetServerConnection::TInstance) = 0;
    virtual void OnError(const string& err_msg,
        SNetServerImpl* pool) = 0;
};


struct SNetServerImpl : public CNetObject
{
    // Special constructor for making search images
    // for searching in TNetServerSet.
    SNetServerImpl(const string& host, unsigned short port) :
        m_Host(host), m_Port(port)
    {
    }

    void DeleteConnection(SNetServerConnectionImpl* impl);
    void ReturnToPool(SNetServerConnectionImpl* impl);

    std::string GetAddressAsString() const;

    // A smart pointer to the NetService object
    // that contains this NetServer.
    CNetService m_Service;

    string m_Host;
    unsigned short m_Port;

    STimeout m_Timeout;

    SNetServerConnectionImpl* m_FreeConnectionListHead;
    int m_FreeConnectionListSize;
    CFastMutex m_FreeConnectionListLock;
};

struct SNetServerImplReal : public SNetServerImpl
{
    SNetServerImplReal(const string& host,
        unsigned short port,
        SNetServiceImpl* service_impl,
        const STimeout& timeout);

    virtual ~SNetServerImplReal();
};

END_NCBI_SCOPE

#endif  /* CONNECT_SERVICES___SRV_CONNECTIONS_IMPL__HPP */
