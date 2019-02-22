#ifndef PUBSEQ_GATEWAY__HPP
#define PUBSEQ_GATEWAY__HPP

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
 * Authors: Dmitri Dmitrienko
 *
 * File Description:
 *
 */
#include <string>

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbi_system.hpp>

#include <objtools/pubseq_gateway/impl/cassandra/cass_blob_op.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/cass_factory.hpp>
#include <objtools/pubseq_gateway/cache/psg_cache.hpp>

#include "pending_operation.hpp"
#include "http_server_transport.hpp"
#include "pubseq_gateway_version.hpp"
#include "pubseq_gateway_stat.hpp"
#include "pubseq_gateway_utils.hpp"
#include "pubseq_gateway_types.hpp"


USING_NCBI_SCOPE;


class CPubseqGatewayApp: public CNcbiApplication
{
public:
    CPubseqGatewayApp();
    ~CPubseqGatewayApp();

    virtual void Init(void);
    void ParseArgs(void);
    void OpenCache(void);
    void OpenCass(void);
    void CloseCass(void);
    bool SatToSatName(size_t  sat, string &  sat_name);

    string GetBioseqKeyspace(void) const
    {
        return m_BioseqKeyspace;
    }

    vector<pair<string, int32_t>> GetBioseqNAKeyspaces(void) const
    {
        return m_BioseqNAKeyspaces;
    }

    CPubseqGatewayCache *  GetLookupCache(void)
    {
        return m_LookupCache.get();
    }

    int OnBadURL(HST::CHttpRequest &  req,
                 HST::CHttpReply<CPendingOperation> &  resp);

    int OnGet(HST::CHttpRequest &  req,
              HST::CHttpReply<CPendingOperation> &  resp);

    int OnGetBlob(HST::CHttpRequest &  req,
                  HST::CHttpReply<CPendingOperation> &  resp);

    int OnResolve(HST::CHttpRequest &  req,
                  HST::CHttpReply<CPendingOperation> &  resp);

    int OnGetNA(HST::CHttpRequest &  req,
                HST::CHttpReply<CPendingOperation> &  resp);

    int OnConfig(HST::CHttpRequest &  req,
                 HST::CHttpReply<CPendingOperation> &  resp);

    int OnInfo(HST::CHttpRequest &  req,
               HST::CHttpReply<CPendingOperation> &  resp);

    int OnStatus(HST::CHttpRequest &  req,
                 HST::CHttpReply<CPendingOperation> &  resp);

    virtual int Run(void);

    static CPubseqGatewayApp *  GetInstance(void);
    CPubseqGatewayErrorCounters &  GetErrorCounters(void);
    CPubseqGatewayRequestCounters &  GetRequestCounters(void);
    CPubseqGatewayCacheCounters &  GetCacheCounters(void);

private:
    struct SRequestParameter
    {
        bool            m_Found;
        CTempString     m_Value;

        SRequestParameter() : m_Found(false)
        {}
    };

    void x_SendUnknownClientSatelliteError(
        HST::CHttpReply<CPendingOperation> &  resp,
        const SBlobId &  blob_id, const string &  message);
    void x_SendMessageAndCompletionChunks(
        HST::CHttpReply<CPendingOperation> &  resp,  const string &  message,
        CRequestStatus::ECode  status, int  code, EDiagSev  severity);

    bool x_ProcessCommonGetAndResolveParams(
        HST::CHttpRequest &  req,
        HST::CHttpReply<CPendingOperation> &  resp,
        CTempString &  seq_id, int &  seq_id_type,
        ECacheAndCassandraUse &  use_cache, bool  use_psg_protocol);

private:
    void x_ValidateArgs(void);
    string  x_GetCmdLineArguments(void) const;
    CRef<CRequestContext>  x_CreateRequestContext(HST::CHttpRequest &  req) const;
    void x_PrintRequestStop(CRef<CRequestContext>  context, int  status);

    SRequestParameter  x_GetParam(HST::CHttpRequest &  req,
                                  const string &  name) const;
    ECacheAndCassandraUse x_GetUseCacheParameter(HST::CHttpRequest &  req,
                                                 string &  err_msg);
    bool x_IsBoolParamValid(const string &  param_name,
                            const CTempString &  param_value,
                            string &  err_msg) const;
    bool x_ConvertIntParameter(const string &  param_name,
                               const CTempString &  param_value,
                               int &  converted,
                               string &  err_msg) const;
    bool x_IsResolutionParamValid(const string &  param_name,
                                  const CTempString &  param_value,
                                  string &  err_msg) const;
    EOutputFormat x_GetOutputFormat(const string &  param_name,
                                    const CTempString &  param_value,
                                    string &  err_msg) const;
    ETSEOption x_GetTSEOption(const string &  param_name,
                              const CTempString &  param_value,
                              string &  err_msg) const;
    int x_PopulateSatToKeyspaceMap(void);

private:
    void x_MalformedArguments(HST::CHttpReply<CPendingOperation> &  resp,
                              CRef<CRequestContext> &  context,
                              const string &  err_msg);

private:
    string                              m_Si2csiDbFile;
    string                              m_BioseqInfoDbFile;
    string                              m_BlobPropDbFile;
    vector<string>                      m_SatNames;
    vector<pair<string, int32_t>>       m_BioseqNAKeyspaces;

    unsigned short                      m_HttpPort;
    unsigned short                      m_HttpWorkers;
    unsigned int                        m_ListenerBacklog;
    unsigned short                      m_TcpMaxConn;

    shared_ptr<CCassConnection>         m_CassConnection;
    shared_ptr<CCassConnectionFactory>  m_CassConnectionFactory;
    unsigned int                        m_TimeoutMs;
    unsigned int                        m_MaxRetries;

    CTime                               m_StartTime;
    string                              m_RootKeyspace;
    string                              m_BioseqKeyspace;

    unique_ptr<CPubseqGatewayCache>     m_LookupCache;
    unique_ptr<HST::CHttpDaemon<CPendingOperation>>
                                        m_TcpDaemon;

    // The server counters
    CPubseqGatewayErrorCounters         m_ErrorCounters;
    CPubseqGatewayRequestCounters       m_RequestCounters;
    CPubseqGatewayCacheCounters         m_CacheCounters;

private:
    static CPubseqGatewayApp *          sm_PubseqApp;
};


#endif
