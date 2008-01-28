#ifndef NETCACHE_REQUEST_HANDLER__HPP
#define NETCACHE_REQUEST_HANDLER__HPP
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
 * Authors:  Victor Joukov
 *
 * File Description: Network cache daemon
 *
 */
#include "netcached.hpp"

BEGIN_NCBI_SCOPE

const size_t kNetworkBufferSize = 64 * 1024;

// NetCache request handler host
class CNetCache_RequestHandlerHost
{
public:
    virtual ~CNetCache_RequestHandlerHost() {}
    virtual void BeginReadTransmission() = 0;
    virtual void BeginDelayedWrite() = 0;
    virtual CNetCacheServer* GetServer() = 0;
    virtual SNetCache_RequestStat* GetStat() = 0;
    virtual const string* GetAuth() = 0;
};

// NetCache request handler
class CNetCache_RequestHandler
{
public:
    enum ETransmission {
        eTransmissionMoreBuffers = 0,
        eTransmissionLastBuffer
    };
    CNetCache_RequestHandler(CNetCache_RequestHandlerHost* host);
    virtual ~CNetCache_RequestHandler() {}
    /// Process request
    virtual
    void ProcessRequest(string&                request,
                        SNetCache_RequestStat& stat) = 0;
    // Optional for transmission reader, to start reading
    // transmission call m_Host->BeginReadTransmission()
    virtual bool ProcessTransmission(const char* buf, size_t buf_size,
        ETransmission eot) { return false; }
    // Optional for delayed write handler, to start writing
    // call m_Host->BeginDelayedWrite()
    virtual bool ProcessWrite() { return false; }

    // Service for subclasses
    CSocket& GetSocket(void) { return *m_Socket; }
    void SetSocket(CSocket *socket) { m_Socket = socket; }
protected:
    // Service for specific imlemetations
    static
    void WriteMsg(CSocket& sock, 
                  const string& prefix, const string& msg)
    {
        CNetCacheServer::WriteMsg(sock, prefix, msg);
    }
protected:
    CNetCache_RequestHandlerHost* m_Host;
    CNetCacheServer*              m_Server;
    SNetCache_RequestStat*        m_Stat;
    // Transitional - do not hold ownership
    const string*    m_Auth;
private:
    CSocket* m_Socket;
};

END_NCBI_SCOPE

#endif /* NETCACHE_REQUEST_HANDLER__HPP */
