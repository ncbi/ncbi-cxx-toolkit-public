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
#include <connect/services/json_over_uttp.hpp>
#include <chrono>
#include <string>
#include <vector>

#include "pubseq_gateway_exception.hpp"
#include "pubseq_gateway_types.hpp"

USING_NCBI_SCOPE;
USING_IDBLOB_SCOPE;


// Mostly for timing collection
typedef chrono::high_resolution_clock::time_point  TPSGS_HighResolutionTimePoint;


// Blob identifier consists of two integers: sat and sat key.
// The blob sat eventually needs to be resolved to a sat name.
struct SPSGS_BlobId
{
    SPSGS_BlobId()
    {}

    SPSGS_BlobId(const string &  blob_id) :
        m_Id(blob_id)
    {}

    SPSGS_BlobId(const SPSGS_BlobId &) = default;
    SPSGS_BlobId(SPSGS_BlobId &&) = default;
    SPSGS_BlobId &  operator=(const SPSGS_BlobId &) = default;
    SPSGS_BlobId &  operator=(SPSGS_BlobId &&) = default;

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
        ePSGS_ResolveRequest,
        ePSGS_BlobBySeqIdRequest,
        ePSGS_BlobBySatSatKeyRequest,
        ePSGS_AnnotationRequest,
        ePSGS_TSEChunkRequest,

        ePSGS_UnknownRequest
    };

    static string TypeToString(EPSGS_Type  req_type)
    {
        switch (req_type) {
            case ePSGS_ResolveRequest:          return "ResolveRequest";
            case ePSGS_BlobBySeqIdRequest:      return "BlobBySeqIdRequest";
            case ePSGS_BlobBySatSatKeyRequest:  return "BlobBySatSatKeyRequest";
            case ePSGS_AnnotationRequest:       return "AnnotationRequest";
            case ePSGS_TSEChunkRequest:         return "TSEChunkRequest";
            case ePSGS_UnknownRequest:          return "UnknownRequest";
            default: break;
        }
        return "UnknownRequestTypeValue";
    }

public:
    CPSGS_Request();
    CPSGS_Request(unique_ptr<SPSGS_RequestBase> req,
                  CRef<CRequestContext>  request_context);

    EPSGS_Type  GetRequestType(void) const;
    size_t  GetRequestId(void) const
    {
        return m_RequestId;
    }

    template<typename TRequest> TRequest& GetRequest(void)
    {
        if (m_Request) {
            TRequest*   req = dynamic_cast<TRequest *>(m_Request.get());
            if (req != nullptr)
                return *req;
        }

        NCBI_THROW(CPubseqGatewayException, eInvalidUserRequestType,
                   "User request type mismatch. Stored type: " +
                   x_RequestTypeToString(GetRequestType()));
    }

    CRef<CRequestContext>  GetRequestContext(void);
    void SetRequestContext(void);
    TPSGS_HighResolutionTimePoint GetStartTimestamp(void) const;
    bool NeedTrace(void);
    virtual string GetName(void) const;
    virtual CJsonNode Serialize(void) const;

    CPSGS_Request(const CPSGS_Request &) = default;
    CPSGS_Request(CPSGS_Request &&) = default;
    CPSGS_Request &  operator=(const CPSGS_Request &) = default;
    CPSGS_Request &  operator=(CPSGS_Request &&) = default;

private:
    string x_RequestTypeToString(EPSGS_Type  type) const;

private:
    unique_ptr<SPSGS_RequestBase>   m_Request;
    CRef<CRequestContext>           m_RequestContext;
    size_t                          m_RequestId;
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

    int                             m_Hops;
    EPSGS_Trace                     m_Trace;
    TPSGS_HighResolutionTimePoint   m_StartTimestamp;
    vector<string>                  m_EnabledProcessors;
    vector<string>                  m_DisabledProcessors;

    SPSGS_RequestBase() :
        m_Hops(0),
        m_Trace(ePSGS_NoTracing),
        m_StartTimestamp(chrono::high_resolution_clock::now())
    {}

    SPSGS_RequestBase(int  hops,
                      EPSGS_Trace  trace,
                      const vector<string> &  enabled_processors,
                      const vector<string> &  disabled_processors,
                      const TPSGS_HighResolutionTimePoint &  start) :
        m_Hops(hops), m_Trace(trace), m_StartTimestamp(start),
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

    virtual TPSGS_HighResolutionTimePoint GetStartTimestamp(void) const
    {
        return m_StartTimestamp;
    }

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

    SPSGS_ResolveRequest(const string &  seq_id,
                         int  seq_id_type,
                         TPSGS_BioseqIncludeData  include_data_flags,
                         EPSGS_OutputFormat  output_format,
                         EPSGS_CacheAndDbUse  use_cache,
                         EPSGS_AccSubstitutioOption  subst_option,
                         int  hops,
                         EPSGS_Trace  trace,
                         const vector<string> &  enabled_processors,
                         const vector<string> &  disabled_processors,
                         const TPSGS_HighResolutionTimePoint &  start_timestamp) :
        SPSGS_RequestBase(hops, trace,
                          enabled_processors, disabled_processors,
                          start_timestamp),
        m_SeqId(seq_id), m_SeqIdType(seq_id_type),
        m_IncludeDataFlags(include_data_flags),
        m_OutputFormat(output_format),
        m_UseCache(use_cache),
        m_AccSubstOption(subst_option)
    {}

    SPSGS_ResolveRequest() :
        m_SeqIdType(-1),
        m_IncludeDataFlags(0),
        m_OutputFormat(ePSGS_UnknownFormat),
        m_UseCache(ePSGS_UnknownUseCache),
        m_AccSubstOption(ePSGS_UnknownAccSubstitution)
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

    // Both cases: by seq_id/seq_id_type and by sat/sat_key store the
    // required blob id here.
    // When the seq_id/seq_id_type is resolved to sat/sat_key the m_BlobId
    // is populated
    SPSGS_BlobId            m_BlobId;

    // Processing fields: they are not coming from the client and used while
    // the request is in process.
    // Helps to avoid unnecessery cache updates;
    // - only the one who added will remove
    // - only the one who added will set completed once
    bool                    m_ExcludeBlobCacheAdded;
    bool                    m_ExcludeBlobCacheCompleted;

    SPSGS_BlobRequestBase(EPSGS_TSEOption  tse_option,
                          EPSGS_CacheAndDbUse  use_cache,
                          const string &  client_id,
                          int  hops,
                          EPSGS_Trace  trace,
                          const vector<string> &  enabled_processors,
                          const vector<string> &  disabled_processors,
                          const TPSGS_HighResolutionTimePoint &  start_timestamp) :
        SPSGS_RequestBase(hops, trace,
                          enabled_processors, disabled_processors,
                          start_timestamp),
        m_TSEOption(tse_option),
        m_UseCache(use_cache),
        m_ClientId(client_id),
        m_ExcludeBlobCacheAdded(false),
        m_ExcludeBlobCacheCompleted(false)
    {}

    SPSGS_BlobRequestBase() :
        m_TSEOption(ePSGS_UnknownTSE),
        m_UseCache(ePSGS_UnknownUseCache),
        m_ExcludeBlobCacheAdded(false),
        m_ExcludeBlobCacheCompleted(false)
    {}

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
    bool                            m_AutoBlobSkipping;

    SPSGS_BlobBySeqIdRequest(const string &  seq_id,
                             int  seq_id_type,
                             vector<string> &  exclude_blobs,
                             EPSGS_TSEOption  tse_option,
                             EPSGS_CacheAndDbUse  use_cache,
                             EPSGS_AccSubstitutioOption  subst_option,
                             bool  auto_blob_skipping,
                             const string &  client_id,
                             int  hops,
                             EPSGS_Trace  trace,
                             const vector<string> &  enabled_processors,
                             const vector<string> &  disabled_processors,
                             const TPSGS_HighResolutionTimePoint &  start_timestamp) :
        SPSGS_BlobRequestBase(tse_option, use_cache, client_id, hops, trace,
                              enabled_processors, disabled_processors,
                              start_timestamp),
        m_SeqId(seq_id),
        m_SeqIdType(seq_id_type),
        m_ExcludeBlobs(move(exclude_blobs)),
        m_AccSubstOption(subst_option),
        m_AutoBlobSkipping(auto_blob_skipping)
    {}

    SPSGS_BlobBySeqIdRequest() :
        m_SeqIdType(-1),
        m_AccSubstOption(ePSGS_UnknownAccSubstitution),
        m_AutoBlobSkipping(true)
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
                                 int  hops,
                                 EPSGS_Trace  trace,
                                 const vector<string> &  enabled_processors,
                                 const vector<string> &  disabled_processors,
                                 const TPSGS_HighResolutionTimePoint &  start_timestamp) :
        SPSGS_BlobRequestBase(tse_option, use_cache, client_id, hops, trace,
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


struct SPSGS_AnnotRequest : public SPSGS_RequestBase
{
    SPSGS_AnnotRequest(const string &  seq_id,
                       int  seq_id_type,
                       vector<string> &  names,
                       EPSGS_CacheAndDbUse  use_cache,
                       int  hops,
                       EPSGS_Trace  trace,
                       const vector<string> &  enabled_processors,
                       const vector<string> &  disabled_processors,
                       const TPSGS_HighResolutionTimePoint &  start_timestamp) :
        SPSGS_RequestBase(hops, trace,
                          enabled_processors, disabled_processors,
                          start_timestamp),
        m_SeqId(seq_id),
        m_SeqIdType(seq_id_type),
        m_Names(move(names)),
        m_UseCache(use_cache),
        m_Lock(false),
        m_ProcessedBioseqInfo(kUnknownPriority)
    {}

    SPSGS_AnnotRequest() :
        m_SeqIdType(-1),
        m_UseCache(ePSGS_UnknownUseCache),
        m_Lock(false),
        m_ProcessedBioseqInfo(kUnknownPriority)
    {}

    SPSGS_AnnotRequest(const SPSGS_AnnotRequest &) = default;
    SPSGS_AnnotRequest(SPSGS_AnnotRequest &&) = default;
    SPSGS_AnnotRequest &  operator=(const SPSGS_AnnotRequest &) = default;
    SPSGS_AnnotRequest &  operator=(SPSGS_AnnotRequest &&) = default;

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
    // The highest priority will be stored together with the name.
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

public:
    string                  m_SeqId;
    int                     m_SeqIdType;
    vector<string>          m_Names;
    EPSGS_CacheAndDbUse     m_UseCache;

private:
    // A list of names which have been already processed by some processors
    mutable atomic<bool>                        m_Lock;
    TProcessorPriority                          m_ProcessedBioseqInfo;
    vector<pair<TProcessorPriority, string>>    m_Processed;
};


struct SPSGS_TSEChunkRequest : public SPSGS_RequestBase
{
    int64_t                             m_Id2Chunk;
    string                              m_Id2Info;
    EPSGS_CacheAndDbUse                 m_UseCache;

    SPSGS_TSEChunkRequest(int64_t  id2_chunk,
                          const string &  id2_info,
                          EPSGS_CacheAndDbUse  use_cache,
                          int  hops,
                          EPSGS_Trace  trace,
                          const vector<string> &  enabled_processors,
                          const vector<string> &  disabled_processors,
                          const TPSGS_HighResolutionTimePoint &  start_timestamp) :
        SPSGS_RequestBase(hops, trace,
                          enabled_processors, disabled_processors,
                          start_timestamp),
        m_Id2Chunk(id2_chunk),
        m_Id2Info(id2_info),
        m_UseCache(use_cache)
    {}

    SPSGS_TSEChunkRequest() :
        m_Id2Chunk(INT64_MIN),
        m_UseCache(ePSGS_UnknownUseCache)
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


#endif  // PSGS_REQUEST__HPP

