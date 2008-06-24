#ifndef NETCACHE_IC_HANDLER__HPP
#define NETCACHE_IC_HANDLER__HPP

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
#include <util/cache/icache.hpp>
#include "netcached.hpp"

BEGIN_NCBI_SCOPE

/// IC requests
///
/// @internal
typedef enum {
    eIC_Error,
    eIC_SetTimeStampPolicy,
    eIC_GetTimeStampPolicy,
    eIC_SetVersionRetention,
    eIC_GetVersionRetention,
    eIC_GetTimeout,
    eIC_IsOpen,
    eIC_Store,
    eIC_StoreBlob,
    eIC_GetSize,
    eIC_GetBlobOwner,
    eIC_Read,
    eIC_Remove,
    eIC_RemoveKey,
    eIC_GetAccessTime,
    eIC_HasBlobs,
    eIC_Purge1
} EIC_RequestType;



/// Parsed IC request
///
/// @internal 
struct SIC_Request
{
    EIC_RequestType req_type;
    string          cache_name;
    string          key;
    int             version;
    string          subkey;
    string          err_msg;

    unsigned        i0;
    unsigned        i1;
    unsigned        i2;

    SIC_Request() : req_type(eIC_Error), version(0), i0(0), i1(0), i2(0)
    { }
};

class CICacheHandler : public CNetCache_RequestHandler
{
public:
    CICacheHandler(CNetCache_RequestHandlerHost* host);

    // Transitional
    void ParseRequest(const string& reqstr, SIC_Request* req);

    /// Process ICache request
    virtual
    void ProcessRequest(string&                request,
                        SNetCache_RequestStat& stat);
    virtual
    bool ProcessTransmission(const char* buf, size_t buf_size,
                             ETransmission eot);
    virtual
    bool ProcessWrite();

private:
    // ICache request processing
    void Process_IC_SetTimeStampPolicy(ICache&               ic,
                                       CSocket&              sock, 
                                       SIC_Request&          req);

    void Process_IC_GetTimeStampPolicy(ICache&               ic,
                                       CSocket&              sock, 
                                       SIC_Request&          req);

    void Process_IC_SetVersionRetention(ICache&              ic,
                                       CSocket&              sock, 
                                       SIC_Request&          req);

    void Process_IC_GetVersionRetention(ICache&              ic,
                                       CSocket&              sock, 
                                       SIC_Request&          req);

    void Process_IC_GetTimeout(ICache&              ic,
                               CSocket&             sock, 
                               SIC_Request&         req);

    void Process_IC_IsOpen(ICache&              ic,
                           CSocket&             sock, 
                           SIC_Request&         req);

    void Process_IC_Store(ICache&              ic,
                          CSocket&             sock, 
                          SIC_Request&         req);
    void Process_IC_StoreBlob(ICache&              ic,
                              CSocket&             sock,
                              SIC_Request&         req);

    void Process_IC_GetSize(ICache&              ic,
                            CSocket&             sock, 
                            SIC_Request&         req);

    void Process_IC_GetBlobOwner(ICache&              ic,
                                 CSocket&             sock, 
                                 SIC_Request&         req);

    void Process_IC_Read(ICache&              ic,
                         CSocket&             sock, 
                         SIC_Request&         req);

    void Process_IC_Remove(ICache&              ic,
                           CSocket&             sock, 
                           SIC_Request&         req);

    void Process_IC_RemoveKey(ICache&              ic,
                              CSocket&             sock, 
                              SIC_Request&         req);

    void Process_IC_GetAccessTime(ICache&              ic,
                                  CSocket&             sock, 
                                  SIC_Request&         req);

    void Process_IC_HasBlobs(ICache&              ic,
                             CSocket&             sock, 
                             SIC_Request&         req);

    void Process_IC_Purge1(ICache&              ic,
                            CSocket&            sock, 
                            SIC_Request&        req);

private:
    void x_GetKeyVersionSubkey(const char* s, SIC_Request* req);
    bool              m_SizeKnown;
    size_t            m_BlobSize;
    auto_ptr<IWriter> m_Writer;
    auto_ptr<IReader> m_Reader;
};

END_NCBI_SCOPE

#endif /* NETCACHE_IC_HANDLER__HPP */
