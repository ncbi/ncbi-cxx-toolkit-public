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


class CICacheHandler : public CNetCache_RequestHandler
{
public:
    CICacheHandler(CNetCache_RequestHandlerHost* host);

    /// Process ICache request
    virtual
    void ProcessRequest(CNCRequestParser&      parser,
                        SNetCache_RequestStat& stat);
    virtual
    bool ProcessTransmission(const char* buf, size_t buf_size,
                             ETransmission eot);
    virtual
    bool ProcessWrite();

private:
    typedef void (CICacheHandler::*TProcessRequestFunc)
                                        (ICache&                 ic,
                                         const CNCRequestParser& parser,
                                         SNetCache_RequestStat&  stat);

    // ICache request processing
    void Process_IC_SetTimeStampPolicy(ICache&                 ic,
                                       const CNCRequestParser& parser,
                                       SNetCache_RequestStat&  stat);

    void Process_IC_GetTimeStampPolicy(ICache&                 ic,
                                       const CNCRequestParser& parser,
                                       SNetCache_RequestStat&  stat);

    void Process_IC_SetVersionRetention(ICache&                 ic,
                                        const CNCRequestParser& parser,
                                        SNetCache_RequestStat&  stat);

    void Process_IC_GetVersionRetention(ICache&                 ic,
                                        const CNCRequestParser& parser,
                                        SNetCache_RequestStat&  stat);

    void Process_IC_GetTimeout(ICache&                 ic,
                               const CNCRequestParser& parser,
                               SNetCache_RequestStat&  stat);

    void Process_IC_IsOpen(ICache&                 ic,
                           const CNCRequestParser& parser,
                           SNetCache_RequestStat&  stat);

    void Process_IC_Store(ICache&                 ic,
                          const CNCRequestParser& parser,
                          SNetCache_RequestStat&  stat);
    void Process_IC_StoreBlob(ICache&                 ic,
                              const CNCRequestParser& parser,
                              SNetCache_RequestStat&  stat);

    void Process_IC_GetSize(ICache&                 ic,
                            const CNCRequestParser& parser,
                            SNetCache_RequestStat&  stat);

    void Process_IC_GetBlobOwner(ICache&                 ic,
                                 const CNCRequestParser& parser,
                                 SNetCache_RequestStat&  stat);

    void Process_IC_Read(ICache&                 ic,
                         const CNCRequestParser& parser,
                         SNetCache_RequestStat&  stat);

    void Process_IC_Remove(ICache&                 ic,
                           const CNCRequestParser& parser,
                           SNetCache_RequestStat&  stat);

    void Process_IC_RemoveKey(ICache&                 ic,
                              const CNCRequestParser& parser,
                              SNetCache_RequestStat&  stat);

    void Process_IC_GetAccessTime(ICache&                 ic,
                                  const CNCRequestParser& parser,
                                  SNetCache_RequestStat&  stat);

    void Process_IC_HasBlobs(ICache&                 ic,
                             const CNCRequestParser& parser,
                             SNetCache_RequestStat&  stat);

    void Process_IC_Purge1(ICache&                 ic,
                           const CNCRequestParser& parser,
                           SNetCache_RequestStat&  stat);

private:
    void x_ParseKeyVersion(const CNCRequestParser& parser,
                           size_t                  param_num,
                           SNetCache_RequestStat&  stat);

    struct SProcessorInfo;
    friend struct SProcessorInfo;

    struct SProcessorInfo
    {
        TProcessRequestFunc func;
        size_t              params_cnt;


        SProcessorInfo(TProcessRequestFunc _func, size_t _params_cnt)
            : func(_func), params_cnt(_params_cnt)
        {}

        SProcessorInfo(void)
            : func(NULL), params_cnt(0)
        {}
    };

    typedef map<string, SProcessorInfo> TProcessorsMap;


    TProcessorsMap    m_Processors;
    string            m_CacheName;
    string            m_Key;
    unsigned int      m_Version;
    string            m_SubKey;
    bool              m_SizeKnown;
    size_t            m_BlobSize;
    auto_ptr<IWriter> m_Writer;
    auto_ptr<IReader> m_Reader;
};

END_NCBI_SCOPE

#endif /* NETCACHE_IC_HANDLER__HPP */
