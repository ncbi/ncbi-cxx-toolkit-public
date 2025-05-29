#ifndef PUBSEQ_GATEWAY_STAT__HPP
#define PUBSEQ_GATEWAY_STAT__HPP

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
 * File Description:
 *
 */


#include <connect/services/json_over_uttp.hpp>
USING_NCBI_SCOPE;

class IPSGS_Processor;

class CPSGSCounters
{
    public:
        CPSGSCounters(const map<string, size_t> &  proc_group_to_index);
        ~CPSGSCounters();

    public:
        enum EPSGS_CounterType {
            ePSGS_BadUrlPath = 0,
            ePSGS_InsufficientArgs,
            ePSGS_MalformedArgs,
            ePSGS_ClientSatToSatNameError,
            ePSGS_ServerSatToSatNameError,
            ePSGS_BlobPropsNotFound,
            ePSGS_LMDBError,
            ePSGS_CassQueryTimeoutError,
            ePSGS_InvalidId2InfoError,
            ePSGS_SplitHistoryNotFound,
            ePSGS_PublicCommentNotFound,
            ePSGS_AccVerHistoryNotFound,
            ePSGS_IPGResolveNotFound,
            ePSGS_MaxHopsExceededError,
            ePSGS_TSEChunkSplitVersionCacheMatched,
            ePSGS_TSEChunkSplitVersionCacheNotMatched,
            ePSGS_AdminRequest,
            ePSGS_SatMappingRequest,
            ePSGS_ResolveRequest,
            ePSGS_GetBlobBySeqIdRequest,
            ePSGS_GetBlobBySatSatKeyRequest,
            ePSGS_GetNamedAnnotations,
            ePSGS_AccessionVersionHistory,
            ePSGS_IPGResolve,
            ePSGS_TestIORequest,
            ePSGS_GetTSEChunk,
            ePSGS_HealthRequest,
            ePSGS_DeepHealthRequest,
            ePSGS_ReadyZRequest,
            ePSGS_HealthZRequest,
            ePSGS_ReadyZCassandraRequest,
            ePSGS_ReadyZConnectionsRequest,
            ePSGS_ReadyZLMDBRequest,
            ePSGS_ReadyZWGSRequest,
            ePSGS_ReadyZCDDRequest,
            ePSGS_ReadyZSNPRequest,
            ePSGS_LiveZRequest,
            ePSGS_HelloRequest,
            ePSGS_Si2csiCacheHit,
            ePSGS_Si2csiCacheMiss,
            ePSGS_BioseqInfoCacheHit,
            ePSGS_BioseqInfoCacheMiss,
            ePSGS_BlobPropCacheHit,
            ePSGS_BlobPropCacheMiss,
            ePSGS_Si2csiNotFound,
            ePSGS_Si2csiFoundOne,
            ePSGS_Si2csiFoundMany,
            ePSGS_BioseqInfoNotFound,
            ePSGS_BioseqInfoFoundOne,
            ePSGS_BioseqInfoFoundMany,
            ePSGS_Si2csiError,
            ePSGS_BioseqInfoError,
            ePSGS_BackloggedRequests,
            ePSGS_TooManyRequests,
            ePSGS_DestroyedProcessorCallbacks,
            ePSGS_NonProtocolRequests,
            ePSGS_NoProcessorInstantiated,
            ePSGS_UvTcpInitFailure,
            ePSGS_AcceptFailure,
            ePSGS_NumConnHardLimitExceeded,
            ePSGS_NumConnSoftLimitExceeded,
            ePSGS_NumConnAlertLimitExceeded,
            ePSGS_NumReqRefusedDueToSoftLimit,
            ePSGS_NumConnBadToGoodMigration,
            ePSGS_FrameworkUnknownError,
            ePSGS_NoRequestStop,
            ePSGS_NoWebCubbyUserCookie,
            ePSGS_MyNCBIErrorCounter,
            ePSGS_MyNCBINotFoundCounter,
            ePSGS_SecureSatUnauthorizedCounter,
            ePSGS_FailureToGetCassConnectionCounter,
            ePSGS_MyNCBIOKCacheMiss,
            ePSGS_MyNCBIOKCacheHit,
            ePSGS_MyNCBINotFoundCacheMiss,
            ePSGS_MyNCBINotFoundCacheHit,
            ePSGS_MyNCBIErrorCacheMiss,
            ePSGS_MyNCBIErrorCacheHit,
            ePSGS_MyNCBIOKCacheWaitHit,
            ePSGS_IncludeHUPSetToNo,
            ePSGS_IncomingConnectionsCounter,

            // Request stop statuses
            ePSGS_100,
            ePSGS_101,
            ePSGS_200,
            ePSGS_201,
            ePSGS_202,
            ePSGS_203,
            ePSGS_204,
            ePSGS_205,
            ePSGS_206,
            ePSGS_299,
            ePSGS_300,
            ePSGS_301,
            ePSGS_302,
            ePSGS_303,
            ePSGS_304,
            ePSGS_305,
            ePSGS_307,
            ePSGS_400,
            ePSGS_401,
            ePSGS_402,
            ePSGS_403,
            ePSGS_404,
            ePSGS_405,
            ePSGS_406,
            ePSGS_407,
            ePSGS_408,
            ePSGS_409,
            ePSGS_410,
            ePSGS_411,
            ePSGS_412,
            ePSGS_413,
            ePSGS_414,
            ePSGS_415,
            ePSGS_416,
            ePSGS_417,
            ePSGS_422,
            ePSGS_499,
            ePSGS_500,
            ePSGS_501,
            ePSGS_502,
            ePSGS_503,
            ePSGS_504,
            ePSGS_505,
            ePSGS_xxx,  // unknown request stop status

            // Not pure monotonic counters;
            // They participate in the output though so the name and identifier will be
            // required for them too
            ePSGS_CassandraActiveStatements,
            ePSGS_NumberOfConnections,
            ePSGS_ActiveProcessorGroups,
            ePSGS_ShutdownRequested,
            ePSGS_GracefulShutdownExpiredInSec,
            ePSGS_SplitInfoCacheSize,
            ePSGS_MyNCBIOKCacheSize,
            ePSGS_MyNCBINotFoundCacheSize,
            ePSGS_MyNCBIErrorCacheSize,

            // Used to reserve an array for individual counters
            ePSGS_MaxIndividualCounter,

            // Counters which are per processor
            ePSGS_InputSeqIdNotResolved,
            ePSGS_AnnotationBlobNotFound,
            ePSGS_AnnotationNotFound,
            ePSGS_GetBlobNotFound,
            ePSGS_TSEChunkNotFound,
            ePSGS_ProcUnknownError,
            ePSGS_OpTooLong,

            // Used to calculate the number of counters per processor
            ePSGS_LastCounter
        };

    static EPSGS_CounterType StatusToCounterType(int  status);
    static bool IsPerProcessorCounter(EPSGS_CounterType  counter)
    {
        return counter > ePSGS_MaxIndividualCounter;
    }

    void Increment(IPSGS_Processor *  processor,
                   EPSGS_CounterType  counter);
    uint64_t GetValue(EPSGS_CounterType  counter);
    uint64_t GetFinishedRequestsCounter(void);
    void IncrementRequestStopCounter(int  status);
    void UpdateConfiguredNameDescription(
                            const map<string, tuple<string, string>> &  conf);
    void Reset(void);

    public:
        void PopulateDictionary(CJsonNode &  dict);
        void AppendValueNode(CJsonNode &  dict, const string &  id,
                             const string &  name, const string &  description,
                             uint64_t  value);
        void AppendValueNode(CJsonNode &  dict, const string &  id,
                             const string &  name, const string &  description,
                             bool  value);
        void AppendValueNode(CJsonNode &  dict, const string &  id,
                             const string &  name, const string &  description,
                             const string &  value);
        void AppendValueNode(CJsonNode &  dict, EPSGS_CounterType  counter_type,
                             uint64_t  value);
        void AppendValueNode(CJsonNode &  dict, EPSGS_CounterType  counter_type,
                             bool  value);
        void AppendValueNode(CJsonNode &  dict, EPSGS_CounterType  counter_type,
                             const string &  value);

        struct SCounterInfo
        {
            enum EPSGS_CounterType {
                ePSGS_Monotonic,    // Monotonically growing counter,
                                    // needs to be generically sent to the
                                    // client
                ePSGS_Arbitrary     // The value is set from outside at the moment
                                    // of sending a reply. Can float arbitrary.
                                    // Basically a storage for the identifier,
                                    // name, and description
            };

            string                  m_Identifier;
            string                  m_Name;
            string                  m_Description;
            EPSGS_CounterType       m_Type;

            atomic_uint_fast64_t    m_Value;

            SCounterInfo(const string &  identifier,
                         const string &  name,
                         const string &  description,
                         EPSGS_CounterType  counter_type = ePSGS_Monotonic) :
                m_Identifier(identifier), m_Name(name),
                m_Description(description), m_Type(counter_type),
                m_Value(0)
            {}
        };

    private:
        map<string, size_t>             m_ProcGroupToIndex;
        SCounterInfo *                  m_Counters[ePSGS_MaxIndividualCounter];

        // Introduced for convenience; there are individual counters for each
        // request stop status code and an alternative would be to sum all of
        // them. Easier however is to increment just one approprietly and get
        // the sum quickly when needed.
        atomic_uint_fast64_t            m_FinishedRequestsCounter;

        vector<vector<SCounterInfo *>>  m_PerProcessorCounters;
};



/*
void AppendValueNode(CJsonNode &  dict,
                     const string &  id,
                     bool  value);
*/
void AppendValueNode(CJsonNode &  dict,
                     const string &  id,
                     const string &  value);

#endif
