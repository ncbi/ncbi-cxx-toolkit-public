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
#include "exclude_blob_cache.hpp"
#include "alerts.hpp"
#include "timing.hpp"


USING_NCBI_SCOPE;


const long  kMaxTestIOSize = 1000000000;


// Cassandra mapping needs to be switched atomically so it is first read into
// the corresponding structure and then switched.
struct SCassMapping
{
    string                              m_BioseqKeyspace;
    vector<pair<string, int32_t>>       m_BioseqNAKeyspaces;

    bool operator==(const SCassMapping &  rhs) const
    {
        return m_BioseqKeyspace == rhs.m_BioseqKeyspace &&
               m_BioseqNAKeyspaces == rhs.m_BioseqNAKeyspaces;
    }

    bool operator!=(const SCassMapping &  rhs) const
    { return !this->operator==(rhs); }

    vector<string> validate(const string &  root_keyspace) const
    {
        vector<string>      errors;

        if (m_BioseqKeyspace.empty())
            errors.push_back("Cannot find the resolver keyspace (where SI2CSI "
                             "and BIOSEQ_INFO tables reside) in the " +
                             root_keyspace + ".SAT2KEYSPACE table.");

        if (m_BioseqNAKeyspaces.empty())
            errors.push_back("No bioseq named annotation keyspaces found "
                             "in the " + root_keyspace + " keyspace.");

        return errors;
    }

    void clear(void)
    {
        m_BioseqKeyspace.clear();
        m_BioseqNAKeyspaces.clear();
    }
};


class CPubseqGatewayApp: public CNcbiApplication
{
public:
    CPubseqGatewayApp();
    ~CPubseqGatewayApp();

    virtual void Init(void);
    void ParseArgs(void);
    void OpenCache(void);
    bool OpenCass(void);
    bool PopulateCassandraMapping(bool  need_accept_alert=false);
    void CheckCassMapping(void);
    void CloseCass(void);
    bool SatToSatName(size_t  sat, string &  sat_name);

    string GetBioseqKeyspace(void) const
    {
        return m_CassMapping[m_MappingIndex].m_BioseqKeyspace;
    }

    vector<pair<string, int32_t>> GetBioseqNAKeyspaces(void) const
    {
        return m_CassMapping[m_MappingIndex].m_BioseqNAKeyspaces;
    }

    CPubseqGatewayCache *  GetLookupCache(void)
    {
        return m_LookupCache.get();
    }

    CExcludeBlobCache *  GetExcludeBlobCache(void)
    {
        return m_ExcludeBlobCache.get();
    }

    unsigned long GetSlimMaxBlobSize(void) const
    {
        return m_SlimMaxBlobSize;
    }

    int OnBadURL(HST::CHttpRequest &  req,
                 HST::CHttpReply<CPendingOperation> &  resp);

    int OnGet(HST::CHttpRequest &  req,
              HST::CHttpReply<CPendingOperation> &  resp);

    int OnGetBlob(HST::CHttpRequest &  req,
                  HST::CHttpReply<CPendingOperation> &  resp);

    int OnResolve(HST::CHttpRequest &  req,
                  HST::CHttpReply<CPendingOperation> &  resp);

    int OnGetTSEChunk(HST::CHttpRequest &  req,
                      HST::CHttpReply<CPendingOperation> &  resp);

    int OnGetNA(HST::CHttpRequest &  req,
                HST::CHttpReply<CPendingOperation> &  resp);

    int OnConfig(HST::CHttpRequest &  req,
                 HST::CHttpReply<CPendingOperation> &  resp);

    int OnInfo(HST::CHttpRequest &  req,
               HST::CHttpReply<CPendingOperation> &  resp);

    int OnStatus(HST::CHttpRequest &  req,
                 HST::CHttpReply<CPendingOperation> &  resp);

    int OnShutdown(HST::CHttpRequest &  req,
                   HST::CHttpReply<CPendingOperation> &  resp);

    int OnGetAlerts(HST::CHttpRequest &  req,
                    HST::CHttpReply<CPendingOperation> &  resp);

    int OnAckAlert(HST::CHttpRequest &  req,
                   HST::CHttpReply<CPendingOperation> &  resp);

    int OnStatistics(HST::CHttpRequest &  req,
                     HST::CHttpReply<CPendingOperation> &  resp);

    int OnTestIO(HST::CHttpRequest &  req,
                 HST::CHttpReply<CPendingOperation> &  resp);

    virtual int Run(void);

    static CPubseqGatewayApp *  GetInstance(void);
    CPubseqGatewayErrorCounters &  GetErrorCounters(void);
    CPubseqGatewayRequestCounters &  GetRequestCounters(void);
    CPubseqGatewayCacheCounters &  GetCacheCounters(void);
    CPubseqGatewayDBCounters &  GetDBCounters(void);

    COperationTiming & GetTiming(void)
    {
        return *m_Timing.get();
    }

    EPSGS_StartupDataState GetStartupDataState(void) const
    {
        return m_StartupDataState;
    }

    map<string, tuple<string, string>>  GetIdToNameAndDescriptionMap(void) const
    {
        return m_IdToNameAndDescription;
    }

    shared_ptr<CCassConnection> GetCassandraConnection(void)
    {
        return m_CassConnection;
    }

    unsigned int GetCassandraTimeout(void) const
    {
        return m_TimeoutMs;
    }

    unsigned int GetCassandraMaxRetries(void) const
    {
        return m_MaxRetries;
    }

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
        const SPSGS_BlobId &  blob_id, const string &  message);
    void x_SendMessageAndCompletionChunks(
        HST::CHttpReply<CPendingOperation> &  resp,  const string &  message,
        CRequestStatus::ECode  status, int  code, EDiagSev  severity);

    bool x_ProcessCommonGetAndResolveParams(
        HST::CHttpRequest &  req,
        HST::CHttpReply<CPendingOperation> &  resp,
        CTempString &  seq_id, int &  seq_id_type,
        SPSGS_RequestBase::EPSGS_CacheAndDbUse &  use_cache,
        bool  use_psg_protocol);

private:
    void x_ValidateArgs(void);
    string  x_GetCmdLineArguments(void) const;
    CRef<CRequestContext>  x_CreateRequestContext(HST::CHttpRequest &  req) const;
    void x_PrintRequestStop(CRef<CRequestContext>  context, int  status);

    SRequestParameter  x_GetParam(HST::CHttpRequest &  req,
                                  const string &  name) const;
    SPSGS_RequestBase::EPSGS_CacheAndDbUse x_GetUseCacheParameter(
                                                HST::CHttpRequest &  req,
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
    SPSGS_ResolveRequest::EPSGS_OutputFormat x_GetOutputFormat(
                                    const string &  param_name,
                                    const CTempString &  param_value,
                                    string &  err_msg) const;
    SPSGS_BlobRequestBase::EPSGS_TSEOption x_GetTSEOption(
                              const string &  param_name,
                              const CTempString &  param_value,
                              string &  err_msg) const;
    vector<SPSGS_BlobId> x_GetExcludeBlobs(const string &  param_name,
                                           const CTempString &  param_value,
                                           string &  err_msg) const;
    unsigned long x_GetDataSize(const IRegistry &  reg,
                                const string &  section,
                                const string &  entry,
                                unsigned long  default_val);
    SPSGS_RequestBase::EPSGS_AccSubstitutioOption x_GetAccessionSubstitutionOption(
                                            const string &  param_name,
                                            const CTempString &  param_value,
                                            string &  err_msg) const;
    bool x_GetTraceParameter(HST::CHttpRequest &  req,
                             const string &  param_name,
                             SPSGS_RequestBase::EPSGS_Trace &  trace,
                             string &  err_msg);

private:
    void x_InsufficientArguments(HST::CHttpReply<CPendingOperation> &  resp,
                                 CRef<CRequestContext> &  context,
                                 const string &  err_msg);
    void x_MalformedArguments(HST::CHttpReply<CPendingOperation> &  resp,
                              CRef<CRequestContext> &  context,
                              const string &  err_msg);
    bool x_IsShuttingDown(HST::CHttpRequest &  req,
                          HST::CHttpReply<CPendingOperation> &  resp);
    bool x_IsDBOK(HST::CHttpRequest &  req,
                  HST::CHttpReply<CPendingOperation> &  resp);
    void x_ReadIdToNameAndDescriptionConfiguration(const IRegistry &  reg,
                                                   const string &  section);

private:
    string                              m_Si2csiDbFile;
    string                              m_BioseqInfoDbFile;
    string                              m_BlobPropDbFile;

    // Bioseq and named annotations keyspaces can be updated dynamically.
    // The index controls the active set.
    // The sat names cannot be updated dynamically - they are read once.
    size_t                              m_MappingIndex;
    SCassMapping                        m_CassMapping[2];
    vector<string>                      m_SatNames;

    unsigned short                      m_HttpPort;
    unsigned short                      m_HttpWorkers;
    unsigned int                        m_ListenerBacklog;
    unsigned short                      m_TcpMaxConn;

    shared_ptr<CCassConnection>         m_CassConnection;
    shared_ptr<CCassConnectionFactory>  m_CassConnectionFactory;
    unsigned int                        m_TimeoutMs;
    unsigned int                        m_MaxRetries;

    unsigned int                        m_ExcludeCacheMaxSize;
    unsigned int                        m_ExcludeCachePurgePercentage;
    unsigned int                        m_ExcludeCacheInactivityPurge;
    unsigned long                       m_SmallBlobSize;
    unsigned long                       m_MinStatValue;
    unsigned long                       m_MaxStatValue;
    unsigned long                       m_NStatBins;
    string                              m_StatScaleType;
    unsigned long                       m_TickSpan;

    CTime                               m_StartTime;
    string                              m_RootKeyspace;
    string                              m_AuthToken;

    bool                                m_AllowIOTest;
    unique_ptr<char []>                 m_IOTestBuffer;

    unsigned long                       m_SlimMaxBlobSize;

    unique_ptr<CPubseqGatewayCache>     m_LookupCache;
    unique_ptr<HST::CHttpDaemon<CPendingOperation>>
                                        m_TcpDaemon;

    // The server counters
    CPubseqGatewayErrorCounters         m_ErrorCounters;
    CPubseqGatewayRequestCounters       m_RequestCounters;
    CPubseqGatewayCacheCounters         m_CacheCounters;
    CPubseqGatewayDBCounters            m_DBCounters;

    unique_ptr<CExcludeBlobCache>       m_ExcludeBlobCache;

    CPSGAlerts                          m_Alerts;
    unique_ptr<COperationTiming>        m_Timing;

    EPSGS_StartupDataState              m_StartupDataState;

    // Serialized JSON introspection message
    string                              m_HelpMessage;

    // Configured counter/statistics ID to name/description
    map<string, tuple<string, string>>  m_IdToNameAndDescription;

private:
    static CPubseqGatewayApp *          sm_PubseqApp;
};


#endif
