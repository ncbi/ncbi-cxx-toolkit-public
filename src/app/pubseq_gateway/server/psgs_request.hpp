#ifndef PSGS_REQUEST__HPP
#define PSGS_REQUEST__HPP

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
 * Authors: Sergey Satskiy
 *
 * File Description: PSG server requests
 *
 */

#include <corelib/request_ctx.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/bioseq_info/record.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/blob_task/fetch_split_history.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <connect/services/json_over_uttp.hpp>
#include <string>
#include <vector>
#include <mutex>
#include <optional>
#include <condition_variable>

#include "pubseq_gateway_exception.hpp"
#include "pubseq_gateway_types.hpp"
#include "http_request.hpp"

USING_NCBI_SCOPE;
USING_IDBLOB_SCOPE;
USING_SCOPE(objects);


// Blob identifier consists of two integers: sat and sat key.
// The blob sat eventually needs to be resolved to a sat name.
struct SPSGS_BlobId
{
    SPSGS_BlobId()
    {}

    SPSGS_BlobId(const string &  blob_id) :
        m_Id(blob_id)
    {}

    SPSGS_BlobId(int32_t  sat, int32_t  sat_key);

    SPSGS_BlobId(const SPSGS_BlobId &) = default;
    SPSGS_BlobId(SPSGS_BlobId &&) = default;
    SPSGS_BlobId &  operator=(const SPSGS_BlobId &) = default;
    SPSGS_BlobId &  operator=(SPSGS_BlobId &&) = default;

    void SetId(const string &  blob_id)
    {
        m_Id = blob_id;
    }

    string GetId(void) const
    {
        return m_Id;
    }

    bool operator < (const SPSGS_BlobId &  other) const
    {
        return m_Id < other.m_Id;
    }

    bool operator == (const SPSGS_BlobId &  other) const
    {
        return m_Id == other.m_Id;
    }

    bool operator != (const SPSGS_BlobId &  other) const
    {
        return !this->operator==(other);
    }

    string      m_Id;
};


// Forward declaration
// CPSGS_Request uses unique_ptr to SPSGS_RequestBase and
// SPSGS_RequestBase uses the request type from CPSGS_Request
struct SPSGS_RequestBase;

class CPSGS_Request
{
public:
    enum EPSGS_Type {
        ePSGS_ResolveRequest = 0,
        ePSGS_BlobBySeqIdRequest,
        ePSGS_BlobBySatSatKeyRequest,
        ePSGS_AnnotationRequest,
        ePSGS_TSEChunkRequest,
        ePSGS_AccessionVersionHistoryRequest,
        ePSGS_IPGResolveRequest,

        ePSGS_UnknownRequest
    };

    static string TypeToString(EPSGS_Type  req_type)
    {
        switch (req_type) {
            case ePSGS_ResolveRequest:                  return "ResolveRequest";
            case ePSGS_BlobBySeqIdRequest:              return "BlobBySeqIdRequest";
            case ePSGS_BlobBySatSatKeyRequest:          return "BlobBySatSatKeyRequest";
            case ePSGS_AnnotationRequest:               return "AnnotationRequest";
            case ePSGS_TSEChunkRequest:                 return "TSEChunkRequest";
            case ePSGS_AccessionVersionHistoryRequest:  return "AccessionVersionHistoryRequest";
            case ePSGS_IPGResolveRequest:               return "IPGResolveRequest";
            case ePSGS_UnknownRequest:                  return "UnknownRequest";
            default: break;
        }
        return "UnknownRequestTypeValue (" + to_string(req_type) + ")";
    }

public:
    CPSGS_Request();
    virtual ~CPSGS_Request();
    CPSGS_Request(const CHttpRequest &  http_request,
                  unique_ptr<SPSGS_RequestBase> req,
                  CRef<CRequestContext>  request_context);

    EPSGS_Type  GetRequestType(void) const;
    size_t  GetRequestId(void) const
    {
        return m_RequestId;
    }

    void  SetBacklogTime(uint64_t  val)
    {
        m_BacklogTimeMks = val;
    }

    uint64_t  GetBacklogTime(void) const
    {
        return m_BacklogTimeMks;
    }

    void  SetConcurrentProcessorCount(size_t  cnt)
    {
        m_ConcurrentProcessorCount = cnt;
    }

    size_t  GetConcurrentProcessorCount(void) const
    {
        return m_ConcurrentProcessorCount;
    }

    // The Lock() call makes it possible for the other processors to wait for
    // the event name
    void Lock(const string &  event_name);
    // The Unlock() call signals for the waiting processors that they can
    // continue
    void Unlock(const string &  event_name);
    // The WaitFor() call blocks the processor till the event is unlocked
    void WaitFor(const string &  event_name, size_t  timeout_sec = 10);

    template<typename TRequest> TRequest& GetRequest(void)
    {
        if (m_Request) {
            TRequest*   req = dynamic_cast<TRequest *>(m_Request.get());
            if (req != nullptr)
                return *req;
        }

        NCBI_THROW(CPubseqGatewayException, eInvalidUserRequestType,
                   "User request type mismatch. Stored type: " +
                   TypeToString(GetRequestType()));
    }

    CRef<CRequestContext>  GetRequestContext(void);
    void SetRequestContext(void);
    psg_time_point_t GetStartTimestamp(void) const;
    bool NeedTrace(void);
    bool NeedProcessorEvents(void);
    int GetHops(void);
    optional<bool> GetIncludeHUP(void);
    virtual string GetName(void) const;
    virtual CJsonNode Serialize(void) const;

    optional<string> GetWebCubbyUser(void)
    {
        return m_HttpRequest.GetWebCubbyUser();
    }

    size_t GetLimitedProcessorCount(void) const
    {
        return m_LimitedProcessors.size();
    }

    void AddLimitedProcessor(const string &  name, size_t  limit)
    {
        m_LimitedProcessors.push_back(pair<string, size_t>(name, limit));
    }

    size_t GetIPThrottledProcessorCount(void) const
    {
        return m_IPThrottledProcessors.size();
    }

    void AddIPThrottledProcessor(const string &  name, size_t  limit)
    {
        m_IPThrottledProcessors.push_back(pair<string, size_t>(name, limit));
    }

    string GetLimitedProcessorsMessage(void) const;
    string GetThrottledProcessorsMessage(void) const;

    CPSGS_Request(const CPSGS_Request &) = delete;
    CPSGS_Request(CPSGS_Request &&) = delete;
    CPSGS_Request &  operator=(const CPSGS_Request &) = delete;
    CPSGS_Request &  operator=(CPSGS_Request &&) = delete;

private:
    CHttpRequest                    m_HttpRequest;
    unique_ptr<SPSGS_RequestBase>   m_Request;
    CRef<CRequestContext>           m_RequestContext;
    size_t                          m_RequestId;
    uint64_t                        m_BacklogTimeMks;

private:
    struct SWaitData
    {
        enum EPSGS_WaitObjectState {
            ePSGS_LockedNobodyWaits,
            ePSGS_LockedSomebodyWaits,
            ePSGS_Unlocked
        };

        SWaitData() :
            m_State(ePSGS_LockedNobodyWaits), m_WaitCount(0)
        {}

        EPSGS_WaitObjectState   m_State;
        size_t                  m_WaitCount;
        condition_variable      m_WaitObject;
    };

    // Number of processors serving the request
    size_t                          m_ConcurrentProcessorCount;
    mutex                           m_WaitLock;
    map<string, SWaitData *>        m_Wait;

    // Processors which have not been instantiated due to a concurrency limit
    // together with the actual limit
    vector<pair<string, size_t>>    m_LimitedProcessors;

    // Processors which have not been instantiated due to throttling by IP
    // together with the actual limit
    vector<pair<string, size_t>>    m_IPThrottledProcessors;
};



// Base struct for all requests: any request can be traceable and has a start
// time
struct SPSGS_RequestBase
{
    // Use cache option comes from the user (the URL 'use_cache' parameter)
    enum EPSGS_CacheAndDbUse {
        ePSGS_CacheOnly,
        ePSGS_DbOnly,
        ePSGS_CacheAndDb,       // Default

        ePSGS_UnknownUseCache
    };

    static string  CacheAndDbUseToString(EPSGS_CacheAndDbUse  option)
    {
        switch (option) {
            case ePSGS_CacheOnly:       return "CacheOnly";
            case ePSGS_DbOnly:          return "DbOnly";
            case ePSGS_CacheAndDb:      return "CacheAndDb";
            case ePSGS_UnknownUseCache: return "UnknownUseCache";
            default: break;
        }
        return "UnknownCacheAndDbUseOptionValue";
    }

    // The accession substitution option comes from the user (the URL
    // 'acc_substitution' parameter)
    enum EPSGS_AccSubstitutioOption {
        ePSGS_DefaultAccSubstitution,       // Default
        ePSGS_LimitedAccSubstitution,
        ePSGS_NeverAccSubstitute,

        ePSGS_UnknownAccSubstitution
    };

    static string AccSubstitutioOptionToString(EPSGS_AccSubstitutioOption option)
    {
        switch (option) {
            case ePSGS_DefaultAccSubstitution:  return "DefaultAccSubstitution";
            case ePSGS_LimitedAccSubstitution:  return "LimitedAccSubstitution";
            case ePSGS_NeverAccSubstitute:      return "NeverAccSubstitute";
            case ePSGS_UnknownAccSubstitution:  return "UnknownAccSubstitution";
            default: break;
        }
        return "UnknownAccSubstitutioOptionValue";
    }

    enum EPSGS_Trace {
        ePSGS_NoTracing,
        ePSGS_WithTracing
    };

    static string TraceToString(EPSGS_Trace  trace)
    {
        switch (trace) {
            case ePSGS_NoTracing:   return "NoTracing";
            case ePSGS_WithTracing: return "WithTracing";
            default:    break;
        }
        return "UnknownTraceOptionValue";
    }

    EPSGS_Trace                     m_Trace;
    bool                            m_ProcessorEvents;
    psg_time_point_t                m_StartTimestamp;
    vector<string>                  m_EnabledProcessors;
    vector<string>                  m_DisabledProcessors;

    SPSGS_RequestBase() :
        m_Trace(ePSGS_NoTracing),
        m_ProcessorEvents(false),
        m_StartTimestamp(psg_clock_t::now())
    {}

    SPSGS_RequestBase(EPSGS_Trace  trace,
                      bool  processor_events,
                      const vector<string> &  enabled_processors,
                      const vector<string> &  disabled_processors,
                      const psg_time_point_t &  start) :
        m_Trace(trace), m_ProcessorEvents(processor_events),
        m_StartTimestamp(start),
        m_EnabledProcessors(enabled_processors),
        m_DisabledProcessors(disabled_processors)
    {}

    virtual ~SPSGS_RequestBase() {}

    virtual CPSGS_Request::EPSGS_Type GetRequestType(void) const = 0;
    virtual string GetName(void) const = 0;
    virtual CJsonNode Serialize(void) const = 0;

    virtual EPSGS_Trace GetTrace(void) const
    {
        return m_Trace;
    }

    virtual bool GetProcessorEvents(void) const
    {
        return m_ProcessorEvents;
    }

    virtual psg_time_point_t GetStartTimestamp(void) const
    {
        return m_StartTimestamp;
    }

    void AppendCommonParameters(CJsonNode &  json) const;

    SPSGS_RequestBase(const SPSGS_RequestBase &) = default;
    SPSGS_RequestBase(SPSGS_RequestBase &&) = default;
    SPSGS_RequestBase &  operator=(const SPSGS_RequestBase &) = default;
    SPSGS_RequestBase &  operator=(SPSGS_RequestBase &&) = default;
};


// Resolve request parsed parameters
struct SPSGS_ResolveRequest : public SPSGS_RequestBase
{
    // The output format may come from the user (the URL 'fmt' parameter)
    enum EPSGS_OutputFormat {
        ePSGS_ProtobufFormat,
        ePSGS_JsonFormat,
        ePSGS_NativeFormat,         // Default: the server decides between
                                    // protobuf and json

        ePSGS_UnknownFormat
    };

    static string OutputFormatToString(EPSGS_OutputFormat  format)
    {
        switch (format) {
            case ePSGS_ProtobufFormat:  return "ProtobufFormat";
            case ePSGS_JsonFormat:      return "JsonFormat";
            case ePSGS_NativeFormat:    return "NativeFormat";
            case ePSGS_UnknownFormat:   return "UnknownFormat";
            default: break;
        }
        return "UnknownFormatOptionValue";
    }

    // The user can specify what fields of the bioseq_info should be included
    // into the server response.
    // Pretty much copied from the client; the justfication for copying is:
    // "it will be decoupled with the client type"
    enum EPSGS_BioseqIncludeData {
        fPSGS_CanonicalId = (1 << 1),
        fPSGS_SeqIds = (1 << 2),
        fPSGS_MoleculeType = (1 << 3),
        fPSGS_Length = (1 << 4),
        fPSGS_State = (1 << 5),
        fPSGS_BlobId = (1 << 6),
        fPSGS_TaxId = (1 << 7),
        fPSGS_Hash = (1 << 8),
        fPSGS_DateChanged = (1 << 9),
        fPSGS_Gi = (1 << 10),
        fPSGS_Name = (1 << 11),
        fPSGS_SeqState = (1 << 12),

        fPSGS_AllBioseqFields = fPSGS_CanonicalId | fPSGS_SeqIds |
                                fPSGS_MoleculeType | fPSGS_Length |
                                fPSGS_State | fPSGS_BlobId | fPSGS_TaxId |
                                fPSGS_Hash | fPSGS_DateChanged | fPSGS_Gi |
                                fPSGS_Name | fPSGS_SeqState,
        fPSGS_BioseqKeyFields = fPSGS_CanonicalId | fPSGS_Gi
    };

    // Bit-set of EPSGS_BioseqIncludeData flags
    typedef int TPSGS_BioseqIncludeData;


    string                      m_SeqId;
    int                         m_SeqIdType;
    TPSGS_BioseqIncludeData     m_IncludeDataFlags;
    EPSGS_OutputFormat          m_OutputFormat;
    EPSGS_CacheAndDbUse         m_UseCache;
    EPSGS_AccSubstitutioOption  m_AccSubstOption;
    bool                        m_SeqIdResolve;
    int                         m_Hops;

    SPSGS_ResolveRequest(const string &  seq_id,
                         int  seq_id_type,
                         TPSGS_BioseqIncludeData  include_data_flags,
                         EPSGS_OutputFormat  output_format,
                         EPSGS_CacheAndDbUse  use_cache,
                         EPSGS_AccSubstitutioOption  subst_option,
                         bool seq_id_resolve,
                         int  hops,
                         EPSGS_Trace  trace,
                         bool  processor_events,
                         const vector<string> &  enabled_processors,
                         const vector<string> &  disabled_processors,
                         const psg_time_point_t &  start_timestamp) :
        SPSGS_RequestBase(trace, processor_events,
                          enabled_processors, disabled_processors,
                          start_timestamp),
        m_SeqId(seq_id), m_SeqIdType(seq_id_type),
        m_IncludeDataFlags(include_data_flags),
        m_OutputFormat(output_format),
        m_UseCache(use_cache),
        m_AccSubstOption(subst_option),
        m_SeqIdResolve(seq_id_resolve),
        m_Hops(hops)
    {}

    SPSGS_ResolveRequest() :
        m_SeqIdType(-1),
        m_IncludeDataFlags(0),
        m_OutputFormat(ePSGS_UnknownFormat),
        m_UseCache(ePSGS_UnknownUseCache),
        m_AccSubstOption(ePSGS_UnknownAccSubstitution),
        m_SeqIdResolve(true),   // default
        m_Hops(0)
    {}

    virtual CPSGS_Request::EPSGS_Type GetRequestType(void) const
    {
        return CPSGS_Request::ePSGS_ResolveRequest;
    }

    virtual string GetName(void) const
    {
        return "ID/resolve";
    }

    virtual CJsonNode Serialize(void) const;

    SPSGS_ResolveRequest(const SPSGS_ResolveRequest &) = default;
    SPSGS_ResolveRequest(SPSGS_ResolveRequest &&) = default;
    SPSGS_ResolveRequest &  operator=(const SPSGS_ResolveRequest &) = default;
    SPSGS_ResolveRequest &  operator=(SPSGS_ResolveRequest &&) = default;
};


struct SPSGS_BlobRequestBase : public SPSGS_RequestBase
{
    // The TSE option comes from the user (the URL 'tse' parameter)
    enum EPSGS_TSEOption {
        ePSGS_NoneTSE,
        ePSGS_SlimTSE,
        ePSGS_SmartTSE,
        ePSGS_WholeTSE,
        ePSGS_OrigTSE,      // Default value

        ePSGS_UnknownTSE
    };

    static string TSEOptionToString(EPSGS_TSEOption  option)
    {
        switch (option) {
            case ePSGS_NoneTSE:     return "NoneTSE";
            case ePSGS_SlimTSE:     return "SlimTSE";
            case ePSGS_SmartTSE:    return "SmartTSE";
            case ePSGS_WholeTSE:    return "WholeTSE";
            case ePSGS_OrigTSE:     return "OrigTSE";
            case ePSGS_UnknownTSE:  return "UnknownTSE";
            default: break;
        }
        return "UnknownOptionValue";
    }


    EPSGS_TSEOption         m_TSEOption;
    EPSGS_CacheAndDbUse     m_UseCache;
    string                  m_ClientId;
    unsigned long           m_SendBlobIfSmall;  // 0 - means do not send
                                                // only for slim and smart

    // Both cases: by seq_id/seq_id_type and by sat/sat_key store the
    // required blob id here.
    // When the seq_id/seq_id_type is resolved to sat/sat_key the m_BlobId
    // is populated
    SPSGS_BlobId            m_BlobId;

    int                     m_Hops;
    optional<bool>          m_IncludeHUP;

    SPSGS_BlobRequestBase(EPSGS_TSEOption  tse_option,
                          EPSGS_CacheAndDbUse  use_cache,
                          const string &  client_id,
                          int  send_blob_if_small,
                          int  hops,
                          const optional<bool> &  include_hup,
                          EPSGS_Trace  trace,
                          bool  processor_events,
                          const vector<string> &  enabled_processors,
                          const vector<string> &  disabled_processors,
                          const psg_time_point_t &  start_timestamp) :
        SPSGS_RequestBase(trace, processor_events,
                          enabled_processors, disabled_processors,
                          start_timestamp),
        m_TSEOption(tse_option),
        m_UseCache(use_cache),
        m_ClientId(client_id),
        m_SendBlobIfSmall(send_blob_if_small),
        m_Hops(hops), m_IncludeHUP(include_hup)
    {}

    SPSGS_BlobRequestBase() :
        m_TSEOption(ePSGS_UnknownTSE),
        m_UseCache(ePSGS_UnknownUseCache),
        m_SendBlobIfSmall(0),
        m_Hops(0)
    {}

    void AppendCommonParameters(CJsonNode &  json) const;

    SPSGS_BlobRequestBase(const SPSGS_BlobRequestBase &) = default;
    SPSGS_BlobRequestBase(SPSGS_BlobRequestBase &&) = default;
    SPSGS_BlobRequestBase &  operator=(const SPSGS_BlobRequestBase &) = default;
    SPSGS_BlobRequestBase &  operator=(SPSGS_BlobRequestBase &&) = default;
};


// Blob by seq_id request (eBlobBySeqIdRequest)
struct SPSGS_BlobBySeqIdRequest : public SPSGS_BlobRequestBase
{
    string                          m_SeqId;
    int                             m_SeqIdType;
    vector<string>                  m_ExcludeBlobs;
    EPSGS_AccSubstitutioOption      m_AccSubstOption;
    unsigned long                   m_ResendTimeoutMks;
    bool                            m_SeqIdResolve;

    SPSGS_BlobBySeqIdRequest(const string &  seq_id,
                             int  seq_id_type,
                             vector<string> &  exclude_blobs,
                             EPSGS_TSEOption  tse_option,
                             EPSGS_CacheAndDbUse  use_cache,
                             EPSGS_AccSubstitutioOption  subst_option,
                             double  resend_timeout,
                             const string &  client_id,
                             int  send_blob_if_small,
                             bool seq_id_resolve,
                             int  hops,
                             const optional<bool> &  include_hup,
                             EPSGS_Trace  trace,
                             bool  processor_events,
                             const vector<string> &  enabled_processors,
                             const vector<string> &  disabled_processors,
                             const psg_time_point_t &  start_timestamp) :
        SPSGS_BlobRequestBase(tse_option, use_cache, client_id, send_blob_if_small,
                              hops, include_hup, trace, processor_events,
                              enabled_processors, disabled_processors,
                              start_timestamp),
        m_SeqId(seq_id),
        m_SeqIdType(seq_id_type),
        m_ExcludeBlobs(std::move(exclude_blobs)),
        m_AccSubstOption(subst_option),
        m_ResendTimeoutMks((unsigned long)(resend_timeout * 1000000)),
        m_SeqIdResolve(seq_id_resolve)
    {}

    SPSGS_BlobBySeqIdRequest() :
        m_SeqIdType(-1),
        m_AccSubstOption(ePSGS_UnknownAccSubstitution),
        m_ResendTimeoutMks(0),
        m_SeqIdResolve(true)    // default
    {}

    virtual CPSGS_Request::EPSGS_Type GetRequestType(void) const
    {
        return CPSGS_Request::ePSGS_BlobBySeqIdRequest;
    }

    virtual string GetName(void) const
    {
        return "ID/get";
    }

    virtual CJsonNode Serialize(void) const;

    SPSGS_BlobBySeqIdRequest(const SPSGS_BlobBySeqIdRequest &) = default;
    SPSGS_BlobBySeqIdRequest(SPSGS_BlobBySeqIdRequest &&) = default;
    SPSGS_BlobBySeqIdRequest &  operator=(const SPSGS_BlobBySeqIdRequest &) = default;
    SPSGS_BlobBySeqIdRequest &  operator=(SPSGS_BlobBySeqIdRequest &&) = default;
};


// Blob by sat/sat_key request (eBlobBySatSatKeyRequest)
struct SPSGS_BlobBySatSatKeyRequest : public SPSGS_BlobRequestBase
{
    CBlobRecord::TTimestamp         m_LastModified;

    SPSGS_BlobBySatSatKeyRequest(const SPSGS_BlobId &  blob_id,
                                 CBlobRecord::TTimestamp  last_modified,
                                 EPSGS_TSEOption  tse_option,
                                 EPSGS_CacheAndDbUse  use_cache,
                                 const string &  client_id,
                                 int  send_blob_if_small,
                                 int  hops,
                                 const optional<bool> &  include_hup,
                                 EPSGS_Trace  trace,
                                 bool  processor_events,
                                 const vector<string> &  enabled_processors,
                                 const vector<string> &  disabled_processors,
                                 const psg_time_point_t &  start_timestamp) :
        SPSGS_BlobRequestBase(tse_option, use_cache, client_id, send_blob_if_small,
                              hops, include_hup, trace, processor_events,
                              enabled_processors, disabled_processors,
                              start_timestamp),
        m_LastModified(last_modified)
    {
        m_BlobId = blob_id;
    }

    SPSGS_BlobBySatSatKeyRequest() :
        m_LastModified(INT64_MIN)
    {}

    virtual CPSGS_Request::EPSGS_Type GetRequestType(void) const
    {
        return CPSGS_Request::ePSGS_BlobBySatSatKeyRequest;
    }

    virtual string GetName(void) const
    {
        return "ID/getblob";
    }

    virtual CJsonNode Serialize(void) const;

    SPSGS_BlobBySatSatKeyRequest(const SPSGS_BlobBySatSatKeyRequest &) = default;
    SPSGS_BlobBySatSatKeyRequest(SPSGS_BlobBySatSatKeyRequest &&) = default;
    SPSGS_BlobBySatSatKeyRequest &  operator=(const SPSGS_BlobBySatSatKeyRequest &) = default;
    SPSGS_BlobBySatSatKeyRequest &  operator=(SPSGS_BlobBySatSatKeyRequest &&) = default;
};


struct SPSGS_AnnotRequest : public SPSGS_BlobRequestBase
{
    string                              m_SeqId;
    int                                 m_SeqIdType;
    vector<string>                      m_Names;
    unsigned long                       m_ResendTimeoutMks;
    vector<string>                      m_SeqIds;
    bool                                m_SeqIdResolve;
    optional<CSeq_id::ESNPScaleLimit>   m_SNPScaleLimit;

    SPSGS_AnnotRequest(const string &  seq_id,
                       int  seq_id_type,
                       vector<string> &  names,
                       EPSGS_CacheAndDbUse  use_cache,
                       double  resend_timeout,
                       vector<string> &  seq_ids,
                       const string &  client_id,
                       SPSGS_BlobRequestBase::EPSGS_TSEOption  tse_option,
                       int  send_blob_if_small,
                       bool seq_id_resolve,
                       optional<CSeq_id::ESNPScaleLimit> &  snp_scale_limit,
                       int  hops,
                       const optional<bool> &  include_hup,
                       EPSGS_Trace  trace,
                       bool  processor_events,
                       const vector<string> &  enabled_processors,
                       const vector<string> &  disabled_processors,
                       const psg_time_point_t &  start_timestamp) :
        SPSGS_BlobRequestBase(tse_option, use_cache, client_id, send_blob_if_small,
                              hops, include_hup, trace, processor_events,
                              enabled_processors, disabled_processors,
                              start_timestamp),
        m_SeqId(seq_id),
        m_SeqIdType(seq_id_type),
        m_Names(std::move(names)),
        m_ResendTimeoutMks((unsigned long)(resend_timeout * 1000000)),
        m_SeqIds(std::move(seq_ids)),
        m_SeqIdResolve(seq_id_resolve),
        m_SNPScaleLimit(snp_scale_limit),
        m_Lock(false),
        m_ProcessedBioseqInfo(kUnknownPriority)
    {}

    SPSGS_AnnotRequest() :
        m_SeqIdType(-1),
        m_ResendTimeoutMks(0),
        m_SeqIdResolve(true),   // default
        m_Lock(false),
        m_ProcessedBioseqInfo(kUnknownPriority)
    {}

    SPSGS_AnnotRequest(const SPSGS_AnnotRequest &) = delete;
    SPSGS_AnnotRequest(SPSGS_AnnotRequest &&) = delete;
    SPSGS_AnnotRequest &  operator=(const SPSGS_AnnotRequest &) = delete;
    SPSGS_AnnotRequest &  operator=(SPSGS_AnnotRequest &&) = delete;

    virtual CPSGS_Request::EPSGS_Type GetRequestType(void) const
    {
        return CPSGS_Request::ePSGS_AnnotationRequest;
    }

    virtual string GetName(void) const
    {
        return "ID/get_na";
    }

    virtual CJsonNode Serialize(void) const;

    // Facilities to work with the list of already processed names

    // If the given name is already in the list then the priority
    // of the processor which has registered it before is returned
    // If the given name is not in the list then kUnknownPriority constant is
    // returned.
    TProcessorPriority RegisterProcessedName(TProcessorPriority  priority,
                                             const string &  name);


    // If bioseq info has already been sent then the priority of the processor
    // which sent it will be returned. Otherwise kUnknownPriority constant is
    // returned.
    // The highest priority of all the calls will be stored.
    TProcessorPriority RegisterBioseqInfo(TProcessorPriority  priority);

    // Names which have not been processed by a processor which priority
    // is higher than given
    vector<string> GetNotProcessedName(TProcessorPriority  priority);

    vector<pair<TProcessorPriority, string>>  GetProcessedNames(void) const;

    // Used to track a status of each individual annotation
    enum EPSGS_ResultStatus {
        // ePSGS_RS_Success     = 200,      implicitly memorized when a
        //                                  processor checks priority before
        //                                  sending
        ePSGS_RS_NotFound       = 404,      // Used by a processor
        ePSGS_RS_Error          = 500,      // Used by a processor
        ePSGS_RS_Unavailable    = 503,      // This is for the case when a
                                            // processor was not instantiated.
                                            // Used by the framework
        ePSGS_RS_Timeout        = 504       // Used by a processor
    };

    void  ReportResultStatus(const string &  annot_name, EPSGS_ResultStatus  rs);

    // In case of exactly one annotation requested it is possible that blob
    // props and blob chunks are need to be sent to the client. Thus it is
    // possible that there is an error during the blob prop or blob chunks
    // stage. This method should be called by the corresponding processor if
    // such an error encountered.
    // The rs is expected to be here a timeout or an error
    void  ReportBlobError(TProcessorPriority  priority,
                          EPSGS_ResultStatus  rs);

    set<string>  GetNotFoundNames(void) const
    {
        return m_NotFound;
    }

    map<string, int>  GetErrorNames(void) const
    {
        return m_ErrorAnnotations;
    }

    bool WasSent(const string &  annot_name) const;

private:
    // A list of names which have been already processed by some processors
    mutable atomic<bool>                        m_Lock;
    TProcessorPriority                          m_ProcessedBioseqInfo;
    vector<pair<TProcessorPriority, string>>    m_Processed;

    // Processor reported not found annotations
    set<string>                                 m_NotFound;

    // This map includes limited, error and timeout annotations and stores only
    // the highest reported code. This is because the highest error will be
    // reported to the client.
    map<string, int>                            m_ErrorAnnotations;
};


struct SPSGS_TSEChunkRequest : public SPSGS_RequestBase
{
    int64_t                             m_Id2Chunk;
    string                              m_Id2Info;
    EPSGS_CacheAndDbUse                 m_UseCache;
    int                                 m_Hops;
    optional<bool>                      m_IncludeHUP;

    SPSGS_TSEChunkRequest(int64_t  id2_chunk,
                          const string &  id2_info,
                          EPSGS_CacheAndDbUse  use_cache,
                          int  hops,
                          const optional<bool> &  include_hup,
                          EPSGS_Trace  trace,
                          bool  processor_events,
                          const vector<string> &  enabled_processors,
                          const vector<string> &  disabled_processors,
                          const psg_time_point_t &  start_timestamp) :
        SPSGS_RequestBase(trace, processor_events,
                          enabled_processors, disabled_processors,
                          start_timestamp),
        m_Id2Chunk(id2_chunk),
        m_Id2Info(id2_info),
        m_UseCache(use_cache),
        m_Hops(hops),
        m_IncludeHUP(include_hup)
    {}

    SPSGS_TSEChunkRequest() :
        m_Id2Chunk(INT64_MIN),
        m_UseCache(ePSGS_UnknownUseCache),
        m_Hops(0)
    {}

    virtual CPSGS_Request::EPSGS_Type GetRequestType(void) const
    {
        return CPSGS_Request::ePSGS_TSEChunkRequest;
    }

    virtual string GetName(void) const
    {
        return "ID/get_tse_chunk";
    }

    virtual CJsonNode Serialize(void) const;

    SPSGS_TSEChunkRequest(const SPSGS_TSEChunkRequest &) = default;
    SPSGS_TSEChunkRequest(SPSGS_TSEChunkRequest &&) = default;
    SPSGS_TSEChunkRequest &  operator=(const SPSGS_TSEChunkRequest &) = default;
    SPSGS_TSEChunkRequest &  operator=(SPSGS_TSEChunkRequest &&) = default;
};


struct SPSGS_AccessionVersionHistoryRequest : public SPSGS_RequestBase
{
    string                      m_SeqId;
    int                         m_SeqIdType;
    EPSGS_CacheAndDbUse         m_UseCache;
    int                         m_Hops;

    SPSGS_AccessionVersionHistoryRequest(
                         const string &  seq_id,
                         int  seq_id_type,
                         EPSGS_CacheAndDbUse  use_cache,
                         int  hops,
                         EPSGS_Trace  trace,
                         bool  processor_events,
                         const vector<string> &  enabled_processors,
                         const vector<string> &  disabled_processors,
                         const psg_time_point_t &  start_timestamp) :
        SPSGS_RequestBase(trace, processor_events,
                          enabled_processors, disabled_processors,
                          start_timestamp),
        m_SeqId(seq_id), m_SeqIdType(seq_id_type),
        m_UseCache(use_cache),
        m_Hops(hops)
    {}

    SPSGS_AccessionVersionHistoryRequest() :
        m_SeqIdType(-1),
        m_UseCache(ePSGS_UnknownUseCache),
        m_Hops(0)
    {}

    virtual CPSGS_Request::EPSGS_Type GetRequestType(void) const
    {
        return CPSGS_Request::ePSGS_AccessionVersionHistoryRequest;
    }

    virtual string GetName(void) const
    {
        return "ID/accession_version_history";
    }

    virtual CJsonNode Serialize(void) const;

    SPSGS_AccessionVersionHistoryRequest(const SPSGS_AccessionVersionHistoryRequest &) = default;
    SPSGS_AccessionVersionHistoryRequest(SPSGS_AccessionVersionHistoryRequest &&) = default;
    SPSGS_AccessionVersionHistoryRequest &  operator=(const SPSGS_AccessionVersionHistoryRequest &) = default;
    SPSGS_AccessionVersionHistoryRequest &  operator=(SPSGS_AccessionVersionHistoryRequest &&) = default;
};


struct SPSGS_IPGResolveRequest : public SPSGS_RequestBase
{
    optional<string>        m_Protein;
    int64_t                 m_IPG;
    optional<string>        m_Nucleotide;
    EPSGS_CacheAndDbUse     m_UseCache;
    bool                    m_SeqIdResolve;

    SPSGS_IPGResolveRequest(const optional<string> &  protein,
                            int64_t  ipg,
                            const optional<string> &  nucleotide,
                            EPSGS_CacheAndDbUse  use_cache,
                            bool seq_id_resolve,
                            EPSGS_Trace  trace,
                            bool  processor_events,
                            const vector<string> &  enabled_processors,
                            const vector<string> &  disabled_processors,
                            const psg_time_point_t &  start_timestamp) :
        SPSGS_RequestBase(trace, processor_events,
                          enabled_processors, disabled_processors,
                          start_timestamp),
        m_Protein(protein), m_IPG(ipg), m_Nucleotide(nucleotide),
        m_UseCache(use_cache), m_SeqIdResolve(seq_id_resolve)
    {}

    SPSGS_IPGResolveRequest() :
        m_IPG(-1),
        m_UseCache(ePSGS_UnknownUseCache),
        m_SeqIdResolve(true)    // default
    {}

    virtual CPSGS_Request::EPSGS_Type GetRequestType(void) const
    {
        return CPSGS_Request::ePSGS_IPGResolveRequest;
    }

    virtual string GetName(void) const
    {
        return "IPG/resolve";
    }

    virtual CJsonNode Serialize(void) const;

    SPSGS_IPGResolveRequest(const SPSGS_IPGResolveRequest &) = default;
    SPSGS_IPGResolveRequest(SPSGS_IPGResolveRequest &&) = default;
    SPSGS_IPGResolveRequest &  operator=(const SPSGS_IPGResolveRequest &) = default;
    SPSGS_IPGResolveRequest &  operator=(SPSGS_IPGResolveRequest &&) = default;
};

#endif  // PSGS_REQUEST__HPP

