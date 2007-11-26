#ifndef NETCACHE_NC_HANDLER__HPP
#define NETCACHE_NC_HANDLER__HPP

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
#include "request_handler.hpp"

BEGIN_NCBI_SCOPE

/// NC requests
///
/// @internal
typedef enum {
    eError,
    ePut,
    ePut2,   ///< PUT v.2 transmission protocol
    ePut3,   ///< PUT v.2 transmission protocol with commit confirmation
    eGet,
    eShutdown,
    eVersion,
    eRemove,
    eRemove2, ///< Remove with confirmation
    eLogging,
    eGetConfig,
    eGetStat,
    eIsLock,
    eGetBlobOwner,
    eDropStat,
    eHasBlob
} ENC_RequestType;


/// Parsed NetCache request 
///
/// @internal
struct SNC_Request
{
    ENC_RequestType req_type;
    unsigned int    timeout;
    string          req_id;
    string          err_msg;
    bool            no_lock;

    SNC_Request() : req_type(eError), timeout(0), no_lock(false)
    { }
};

class CNetCacheHandler : public CNetCache_RequestHandler
{
public:
    CNetCacheHandler(CNetCache_RequestHandlerHost* host);

    // Transitional
    void ParseRequest(const string& reqstr, SNC_Request* req);

    /// Process request
    virtual
    void ProcessRequest(string&               request,
                        const string&         auth,
                        NetCache_RequestStat& stat,
                        NetCache_RequestInfo* info);
    virtual
    bool ProcessTransmission(const char* buf, size_t buf_size,
                             ETransmission eot);
    virtual
    bool ProcessWrite();

    // NetCache request processing

    /// Process "PUT" request
    void ProcessPut(CSocket&              sock, 
                    SNC_Request&          req,
                    NetCache_RequestStat& stat);

    /// Process "PUT2" request
    void ProcessPut2(CSocket&              sock, 
                     SNC_Request&          req,
                     NetCache_RequestStat& stat);

    /// Process "PUT3" request
    void ProcessPut3(CSocket&              sock, 
                     SNC_Request&          req,
                     NetCache_RequestStat& stat);

    /// Process "GET" request
    void ProcessGet(CSocket&              sock, 
                    const SNC_Request&    req,
                    NetCache_RequestStat& stat);

    /// Process "GET2" request
    void ProcessGet2(CSocket&              sock,
                     const SNC_Request&    req,
                     NetCache_RequestStat& stat);

    /// Process "VERSION" request
    void ProcessVersion(CSocket& sock, const SNC_Request& req);

    /// Process "LOG" request
    void ProcessLog(CSocket& sock, const SNC_Request& req);

    /// Process "REMOVE" request
    void ProcessRemove(CSocket& sock, const SNC_Request& req);

    /// Process "REMOVE2" request
    void ProcessRemove2(CSocket& sock, const SNC_Request& req);

    /// Process "SHUTDOWN" request
    void ProcessShutdown(CSocket& sock);

    /// Process "GETCONF" request
    void ProcessGetConfig(CSocket& sock);

    /// Process "DROPSTAT" request
    void ProcessDropStat(CSocket& sock);

     /// Process "HASB" request
    void ProcessHasBlob(CSocket& sock, const SNC_Request& req);

    /// Process "GBOW" request
    void ProcessGetBlobOwner(CSocket& sock, const SNC_Request& req);

    /// Process "GETSTAT" request
    void ProcessGetStat(CSocket& sock, const SNC_Request& req);

    /// Process "ISLK" request
    void ProcessIsLock(CSocket& sock, const SNC_Request& req);
private:
    bool x_CheckBlobId(CSocket&       sock,
                       CNetCache_Key* blob_id, 
                       const string&  blob_key);
    bool m_PutOK;
    auto_ptr<IWriter> m_Writer;
    auto_ptr<IReader> m_Reader;
};

END_NCBI_SCOPE

#endif /* NETCACHE_NC_HANDLER__HPP */
