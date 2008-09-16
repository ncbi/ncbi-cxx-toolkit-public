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


class CNetCacheHandler : public CNetCache_RequestHandler
{
public:
    CNetCacheHandler(CNetCache_RequestHandlerHost* host);

    /// Process request
    virtual
    void ProcessRequest(CNCRequestParser&      parser,
                        SNetCache_RequestStat& stat);
    virtual
    bool ProcessTransmission(const char* buf, size_t buf_size,
                             ETransmission eot);
    virtual
    bool ProcessWrite();

    bool ProcessWriteAndReport(unsigned blob_size, const string* req_id = 0);

    // NetCache request processing

private:
    typedef void (CNetCacheHandler::*TProcessRequestFunc)
                                        (const CNCRequestParser& parser,
                                         SNetCache_RequestStat&  stat);

    /// Process "PUT" request
    void ProcessPut(const CNCRequestParser& parser,
                    SNetCache_RequestStat&  stat);

    /// Process "PUT2" request
    void ProcessPut2(const CNCRequestParser& parser,
                     SNetCache_RequestStat&  stat);

    /// Process "PUT3" request
    void ProcessPut3(const CNCRequestParser& parser,
                     SNetCache_RequestStat&  stat);

    /// Process "GET" request
    void ProcessGet(const CNCRequestParser& parser,
                    SNetCache_RequestStat&  stat);

    /// Process "VERSION" request
    void ProcessVersion(const CNCRequestParser& parser,
                        SNetCache_RequestStat&  stat);

    /// Process "LOG" request
    void ProcessLog(const CNCRequestParser& parser,
                    SNetCache_RequestStat&  stat);

    /// Process "STAT" request
    void ProcessStat(const CNCRequestParser& parser,
                     SNetCache_RequestStat&  stat);

    /// Process "REMOVE" request
    void ProcessRemove(const CNCRequestParser& parser,
                       SNetCache_RequestStat&  stat);

    /// Process "REMOVE2" request
    void ProcessRemove2(const CNCRequestParser& parser,
                        SNetCache_RequestStat&  stat);

    /// Process "SHUTDOWN" request
    void ProcessShutdown(const CNCRequestParser& parser,
                         SNetCache_RequestStat&  stat);

    /// Process "GETCONF" request
    void ProcessGetConfig(const CNCRequestParser& parser,
                          SNetCache_RequestStat&  stat);

    /// Process "DROPSTAT" request
    void ProcessDropStat(const CNCRequestParser& parser,
                         SNetCache_RequestStat&  stat);

     /// Process "HASB" request
    void ProcessHasBlob(const CNCRequestParser& parser,
                        SNetCache_RequestStat&  stat);

    /// Process "GBOW" request
    void ProcessGetBlobOwner(const CNCRequestParser& parser,
                             SNetCache_RequestStat&  stat);

    /// Process "GETSTAT" request
    void ProcessGetStat(const CNCRequestParser& parser,
                        SNetCache_RequestStat&  stat);

    /// Process "ISLK" request
    void ProcessIsLock(const CNCRequestParser& parser,
                       SNetCache_RequestStat&  stat);

    /// Process "GSIZ" request
    void ProcessGetSize(const CNCRequestParser& parser,
                        SNetCache_RequestStat&  stat);

private:
    void x_CheckBlobIdParam(const string& req_id);

    struct SProcessorInfo;
    friend struct SProcessorInfo;

    struct SProcessorInfo
    {
        TProcessRequestFunc func;
        size_t              params_cnt;


        SProcessorInfo(CNetCacheHandler _func, size_t _params_cnt)
            : func(_func), params_cnt(_params_cnt)
        {}

        SProcessorInfo(void)
            : func(NULL), params_cnt(0)
        {}
    };

    typedef map<string, SProcessorInfo> TProcessorsMap;


    TProcessorsMap    m_Processors;
    bool              m_PutOK;
    auto_ptr<IWriter> m_Writer;
    auto_ptr<IReader> m_Reader;
};

END_NCBI_SCOPE

#endif /* NETCACHE_NC_HANDLER__HPP */
