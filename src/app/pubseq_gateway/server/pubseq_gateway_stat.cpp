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

#include <ncbi_pch.hpp>

#include "pubseq_gateway_stat.hpp"
#include "pubseq_gateway_logging.hpp"
#include "ipsgs_processor.hpp"

USING_NCBI_SCOPE;

static const string     kValue("value");
static const string     kName("name");
static const string     kDescription("description");


CPSGSCounters::CPSGSCounters(const map<string, size_t> &  proc_group_to_index) :
    m_ProcGroupToIndex(proc_group_to_index),
    m_FinishedRequestsCounter(0)
{
    // Monotonic counters
    m_Counters[ePSGS_BadUrlPath] =
        new SCounterInfo(
            "BadUrlPathCount", "Unknown URL counter",
            "Number of times clients requested a path "
            "which is not served by the server");
    m_Counters[ePSGS_NonProtocolRequests] =
        new SCounterInfo(
            "NonProtocolRequests", "Bad requests counter",
            "The number of requests which do not conform to the PSG protocol");
    m_Counters[ePSGS_NoProcessorInstantiated] =
        new SCounterInfo(
            "NoProcessorInstantiated", "No processor instantiated counter",
            "The number of requests when no processors were instantiated");
    m_Counters[ePSGS_UvTcpInitFailure] =
        new SCounterInfo(
            "UvTcpInitFailure", "libuv failure to init a tcp handler counter",
            "The number of times uv_tcp_init() call failed");
    m_Counters[ePSGS_AcceptFailure] =
        new SCounterInfo(
            "AcceptFailure", "TCP socket accept failure",
            "The number of times a TCP accept failed");
    m_Counters[ePSGS_NumConnHardLimitExceeded] =
        new SCounterInfo(
            "NumConnHardLimitExceeded", "Connection exceeded hard limit counter",
            "The number of times a new connection established when a connection hard limit is already reached.");
    m_Counters[ePSGS_NumConnSoftLimitExceeded] =
        new SCounterInfo(
            "NumConnSoftLimitExceeded", "Connection exceeded soft limit counter",
            "The number of times a new connection established when a connection soft limit is already reached.");
    m_Counters[ePSGS_NumConnAlertLimitExceeded] =
        new SCounterInfo(
            "NumConnAlertLimitExceeded", "Connection exceeded alert limit counter",
            "The number of times a new connection established when a connection alert limit is already reached.");
    m_Counters[ePSGS_NumReqRefusedDueToSoftLimit] =
        new SCounterInfo(
            "NumReqRefusedDueToSoftLimit", "Requests refused due to connection soft limit",
            "The number of times a request was refused (503) due to it came over a connection which was established when a soft limit is already reached.");
    m_Counters[ePSGS_NumConnBadToGoodMigration] =
        new SCounterInfo(
            "NumConnBadToGoodMigration", "Connection migration from 'bad' to 'good' counter",
            "The number of times a connection migrated from 'bad' to 'good'");
    m_Counters[ePSGS_FrameworkUnknownError] =
        new SCounterInfo(
            "FrameworkUnknownError", "Framework unknown error counter",
            "Number of times a framework unknown error has been detected");
    m_Counters[ePSGS_NoRequestStop] =
        new SCounterInfo(
            "NoRequestStop", "Missed request stop error counter",
            "Number of times a processor group was erased when there was no request stop for that request");
    m_Counters[ePSGS_InsufficientArgs] =
        new SCounterInfo(
            "InsufficientArgumentsCount", "Insufficient arguments counter",
            "Number of times clients did not supply all the requiried arguments");
    m_Counters[ePSGS_MalformedArgs] =
        new SCounterInfo(
            "MalformedArgumentsCount", "Malformed arguments counter",
            "Number of times clients supplied malformed arguments");
    m_Counters[ePSGS_ClientSatToSatNameError] =
        new SCounterInfo(
            "ClientSatToSatNameErrorCount",
            "Client provided sat to sat name mapping error counter",
            "Number of times a client provided sat could not be mapped to a sat name");
    m_Counters[ePSGS_ServerSatToSatNameError] =
        new SCounterInfo(
            "ServerSatToSatNameErrorCount",
            "Server data sat to sat name mapping error counter",
            "Number of times a server data sat could not be mapped to a sat name");
    m_Counters[ePSGS_BlobPropsNotFound] =
        new SCounterInfo(
            "BlobPropsNotFoundCount", "Blob properties not found counter",
            "Number of times blob properties were not found");
    m_Counters[ePSGS_LMDBError] =
        new SCounterInfo(
            "LMDBErrorCount", "LMDB cache error count",
            "Number of times an error was detected while searching in the LMDB cache");
    m_Counters[ePSGS_CassQueryTimeoutError] =
        new SCounterInfo(
            "CassQueryTimeoutErrorCount", "Cassandra query timeout error counter",
            "Number of times a timeout error was detected while executing a Cassandra query");
    m_Counters[ePSGS_InvalidId2InfoError] =
        new SCounterInfo(
            "InvalidId2InfoErrorCount", "Invalid bioseq info ID2 field error counter",
            "Number of times a malformed bioseq info ID2 field was detected");
    m_Counters[ePSGS_SplitHistoryNotFound] =
        new SCounterInfo(
            "SplitHistoryNotFoundCount", "Split history not found counter",
            "Number of times a split history was not found");
    m_Counters[ePSGS_PublicCommentNotFound] =
        new SCounterInfo(
            "PublicCommentNotFoundCount", "Public comment not found counter",
            "Number of times a public comment was not found");
    m_Counters[ePSGS_AccVerHistoryNotFound] =
        new SCounterInfo(
            "AccessionVersionHistoryNotFoundCount", "Accession version history not found counter",
            "Number of times an accession version history was not found");
    m_Counters[ePSGS_IPGResolveNotFound] =
        new SCounterInfo(
            "IPGResolveNotFoundCount", "IPG resolution not found counter",
            "Number of times an IPG resolution was not found");
    m_Counters[ePSGS_MaxHopsExceededError] =
        new SCounterInfo(
            "MaxHopsExceededErrorCount", "Max hops exceeded error count",
            "Number of times the max number of hops was exceeded");
    m_Counters[ePSGS_TSEChunkSplitVersionCacheMatched] =
        new SCounterInfo(
            "TSEChunkSplitVersionCacheMatched",
            "Requested TSE chunk split version matched the cached one counter",
            "Number of times a client requested TSE chunk split version "
            "matched the cached version");
    m_Counters[ePSGS_TSEChunkSplitVersionCacheNotMatched] =
        new SCounterInfo(
            "TSEChunkSplitVersionCacheNotMatched",
            "Requested TSE chunk split version did not match the cached one counter",
            "Number of times a client requested TSE chunk split version "
            "did not match the cached version");
    m_Counters[ePSGS_AdminRequest] =
        new SCounterInfo(
            "AdminRequestCount", "Administrative requests counter",
            "Number of time a client requested administrative functionality");
    m_Counters[ePSGS_SatMappingRequest] =
        new SCounterInfo(
            "GetSatMappingRequestCount", "Get sat mapping request counter",
            "Number of times a client requested sat mapping");
    m_Counters[ePSGS_ResolveRequest] =
        new SCounterInfo(
            "ResolveRequestCount", "Resolve requests counter",
            "Number of times a client requested resolve functionality");
    m_Counters[ePSGS_GetBlobBySeqIdRequest] =
        new SCounterInfo(
            "GetBlobBySeqIdRequestCount", "Blob requests (by seq id) counter",
            "Number of times a client requested a blob by seq id");
    m_Counters[ePSGS_GetBlobBySatSatKeyRequest] =
        new SCounterInfo(
            "GetBlobBySatSatKeyRequestCount", "Blob requests (by sat and sat key) counter",
            "Number of times a client requested a blob by sat and sat key");
    m_Counters[ePSGS_GetNamedAnnotations] =
        new SCounterInfo(
            "GetNamedAnnotationsCount", "Named annotations requests counter",
            "Number of times a client requested named annotations");
    m_Counters[ePSGS_AccessionVersionHistory] =
        new SCounterInfo(
            "AccessionVersionHistoryCount", "Accession version history requests counter",
            "Number of times a client requested accession version history");
    m_Counters[ePSGS_IPGResolve] =
        new SCounterInfo(
            "IPGResolve", "IPG resolve request counter",
            "Number of times a client requested IPG resolution");
    m_Counters[ePSGS_TestIORequest] =
        new SCounterInfo(
            "TestIORequestCount", "Test input/output requests counter",
            "Number of times a client requested an input/output test");
    m_Counters[ePSGS_GetTSEChunk] =
        new SCounterInfo(
            "GetTSEChunkCount", "TSE chunk requests counter",
            "Number of times a client requested a TSE chunk");
    m_Counters[ePSGS_HealthRequest] =
        new SCounterInfo(
            "HealthRequestCount", "Health requests counter",
            "Number of times a client requested health status (obsolete)");
    m_Counters[ePSGS_DeepHealthRequest] =
        new SCounterInfo(
            "DeepHealthRequestCount", "Deep health requests counter",
            "Number of times a client requested deep-health status (obsolete)");
    m_Counters[ePSGS_ReadyZRequest] =
        new SCounterInfo(
            "ReadyZRequestCount", "readyz requests counter",
            "Number of times a client requested ready status");
    m_Counters[ePSGS_HealthZRequest] =
        new SCounterInfo(
            "HealthZRequestCount", "healthz requests counter",
            "Number of times a client requested health status");
    m_Counters[ePSGS_ReadyZCassandraRequest] =
        new SCounterInfo(
            "ReadyZCassandraRequestCount", "readyz/cassandra requests counter",
            "Number of times a client requested readyz/cassandra status");
    m_Counters[ePSGS_ReadyZConnectionsRequest] =
        new SCounterInfo(
            "ReadyZConnectionsRequestCount", "readyz/connections requests counter",
            "Number of times a client requested readyz/cconnections status");
    m_Counters[ePSGS_ReadyZLMDBRequest] =
        new SCounterInfo(
            "ReadyZLMDBRequestCount", "readyz/lmdb requests counter",
            "Number of times a client requested readyz/lmdb status");
    m_Counters[ePSGS_ReadyZWGSRequest] =
        new SCounterInfo(
            "ReadyZWGSRequestCount", "readyz/wgs requests counter",
            "Number of times a client requested readyz/wgs status");
    m_Counters[ePSGS_ReadyZCDDRequest] =
        new SCounterInfo(
            "ReadyZCDDRequestCount", "readyz/cdd requests counter",
            "Number of times a client requested readyz/cdd status");
    m_Counters[ePSGS_ReadyZSNPRequest] =
        new SCounterInfo(
            "ReadyZSNPRequestCount", "readyz/snp requests counter",
            "Number of times a client requested readyz/snp status");
    m_Counters[ePSGS_LiveZRequest] =
        new SCounterInfo(
            "LiveZRequestCount", "livez requests counter",
            "Number of times a client requested live status");
    m_Counters[ePSGS_HelloRequest] =
        new SCounterInfo(
            "HelloRequestCount", "hello requests counter",
            "Number of times a client sent hello");
    m_Counters[ePSGS_Si2csiCacheHit] =
        new SCounterInfo(
            "Si2csiCacheHit", "si2csi cache hit counter",
            "Number of times a si2csi LMDB cache lookup found a record");
    m_Counters[ePSGS_Si2csiCacheMiss] =
        new SCounterInfo(
            "Si2csiCacheMiss", "si2csi cache miss counter",
            "Number of times a si2csi LMDB cache lookup did not find a record");
    m_Counters[ePSGS_BioseqInfoCacheHit] =
        new SCounterInfo(
            "BioseqInfoCacheHit", "bioseq info cache hit counter",
            "Number of times a bioseq info LMDB cache lookup found a record");
    m_Counters[ePSGS_BioseqInfoCacheMiss] =
        new SCounterInfo(
            "BioseqInfoCacheMiss", "bioseq info cache miss counter",
            "Number of times a bioseq info LMDB cache lookup did not find a record");
    m_Counters[ePSGS_BlobPropCacheHit] =
        new SCounterInfo(
            "BlobPropCacheHit", "Blob properties cache hit counter",
            "Number of times a blob properties LMDB cache lookup found a record");
    m_Counters[ePSGS_BlobPropCacheMiss] =
        new SCounterInfo(
            "BlobPropCacheMiss", "Blob properties cache miss counter",
            "Number of times a blob properties LMDB cache lookup did not find a record");
    m_Counters[ePSGS_Si2csiNotFound] =
        new SCounterInfo(
            "Si2csiNotFound", "si2csi not found in Cassandra counter",
            "Number of times a Cassandra si2csi query resulted in no records");
    m_Counters[ePSGS_Si2csiFoundOne] =
        new SCounterInfo(
            "Si2csiFoundOne", "si2csi found one record in Cassandra counter",
            "Number of times a Cassandra si2csi query resulted in exactly one record");
    m_Counters[ePSGS_Si2csiFoundMany] =
        new SCounterInfo(
            "Si2csiFoundMany", "si2csi found more than one record in Cassandra counter",
            "Number of times a Cassandra si2csi query resulted in more than one record");
    m_Counters[ePSGS_BioseqInfoNotFound] =
        new SCounterInfo(
            "BioseqInfoNotFound", "bioseq info not found in Cassandra counter",
            "Number of times a Cassandra bioseq info query resulted in no records");
    m_Counters[ePSGS_BioseqInfoFoundOne] =
        new SCounterInfo(
            "BioseqInfoFoundOne", "bioseq info found one record in Cassandra counter",
            "Number of times a Cassandra bioseq info query resulted in exactly one record");
    m_Counters[ePSGS_BioseqInfoFoundMany] =
        new SCounterInfo(
            "BioseqInfoFoundMany", "bioseq info found more than one record in Cassandra counter",
            "Number of times a Cassandra bioseq info query resulted in more than one record");
    m_Counters[ePSGS_Si2csiError] =
        new SCounterInfo(
            "Si2csiError", "si2csi Cassandra query execution error counter",
            "Number of time a Cassandra si2csi query resulted in an error");
    m_Counters[ePSGS_BioseqInfoError] =
        new SCounterInfo(
            "BioseqInfoError", "bioseq info Cassandra query execution error counter",
            "Number of times a Cassandra bioseq info query resulted in an error");
    m_Counters[ePSGS_BackloggedRequests] =
        new SCounterInfo(
            "BackloggedRequests", "Backlogged requests counter",
            "Number of times a request has been put into a backlog list");
    m_Counters[ePSGS_TooManyRequests] =
        new SCounterInfo(
            "TooManyRequests", "Too many requests counter",
            "Number of times a request has been rejected because currently there are too many in serving");
    m_Counters[ePSGS_DestroyedProcessorCallbacks] =
        new SCounterInfo(
            "DestroyedProcessorCallbacks", "Destroyed processor callback counter",
            "Number of times a postponed callback was going to be invoked when a processor has already been destroyed");
    m_Counters[ePSGS_NoWebCubbyUserCookie] =
        new SCounterInfo(
            "NoWebCubbyUserCookieCount", "No web cubby user cookie found counter",
            "Number of times a web cubby user cookie was not found for a secure keyspace");
    m_Counters[ePSGS_FailureToGetCassConnectionCounter] =
        new SCounterInfo(
            "FailureToGetCassConnectionCount", "Getting Cassandra connection failure counter",
            "Number of times there was a failure to get a Cassandra connection (secure or unsecure)");
    m_Counters[ePSGS_MyNCBIErrorCounter] =
        new SCounterInfo(
            "MyNCBIErrorCount", "My NCBI error counter",
            "Number of times my NCBI service replied with an error");
    m_Counters[ePSGS_MyNCBINotFoundCounter] =
        new SCounterInfo(
            "MyNCBINotFoundCount", "My NCBI not found counter",
            "Number of times my NCBI service replied with 'not found'");
    m_Counters[ePSGS_SecureSatUnauthorizedCounter] =
        new SCounterInfo(
            "SecureSatUnauthorizedCount", "Secure satellite unauthorized counter",
            "Number of times a secure satellite refused authorization");
    m_Counters[ePSGS_MyNCBIOKCacheMiss] =
        new SCounterInfo(
            "MyNCBIOKCacheMissCount", "My NCBI user info OK cache miss counter",
            "Number of times a lookup in the my NCBI user info OK cache found no record");
    m_Counters[ePSGS_MyNCBIOKCacheHit] =
        new SCounterInfo(
            "MyNCBIOKCacheHitCount", "My NCBI user info OK cache hit counter",
            "Number of times a lookup in the my NCBI user info OK cache found a record");
    m_Counters[ePSGS_MyNCBINotFoundCacheMiss] =
        new SCounterInfo(
            "MyNCBINotFoundCacheMissCount", "My NCBI not found cache miss counter",
            "Number of times a lookup in the my NCBI not found cache found no record");
    m_Counters[ePSGS_MyNCBINotFoundCacheHit] =
        new SCounterInfo(
            "MyNCBINotFoundCacheHitCount", "My NCBI not found cache hit counter",
            "Number of times a lookup in the my NCBI not found cache found a record");
    m_Counters[ePSGS_MyNCBIErrorCacheMiss] =
        new SCounterInfo(
            "MyNCBIErrorCacheMissCount", "My NCBI error cache miss counter",
            "Number of times a lookup in the my NCBI error cache found no record");
    m_Counters[ePSGS_MyNCBIErrorCacheHit] =
        new SCounterInfo(
            "MyNCBIErrorCacheHitCount", "My NCBI error cache hit counter",
            "Number of times a lookup in the my NCBI error cache found a record");
    m_Counters[ePSGS_MyNCBIOKCacheWaitHit] =
        new SCounterInfo(
            "MyNCBIOKCacheWaitHitCount", "My NCBI OK cache hit counter when resolution is in progress",
            "Number of times a lookup in the my NCBI user info OK cache found a record in an in progress state");
    m_Counters[ePSGS_IncludeHUPSetToNo] =
        new SCounterInfo(
            "IncludeHUPSetToNo", "Include HUP set to 'no' when a blob in a secure keyspace counter",
            "Number of times a secure blob was going to be retrieved when include HUP option is explicitly set to 'no'");
    m_Counters[ePSGS_IncomingConnectionsCounter] =
        new SCounterInfo(
            "IncomingConnectionsCounter", "Total number of incoming connections",
            "Total number of incoming connections (lifetime)");
    m_Counters[ePSGS_NewConnThrottled] =
        new SCounterInfo(
            "NewConnThrottled", "Number of times a new connection was closed due to throttling",
            "Number of times a new connection was closed due to throttling");
    m_Counters[ePSGS_OldConnThrottled] =
        new SCounterInfo(
            "OldConnThrottled", "Number of times an old connection was closed due to throttling",
            "Number of times an old connection was closed due to throttling");
    m_Counters[ePSGS_CancelRunning] =
        new SCounterInfo(
            "CancelRunning", "Number of times a running request was canceled due to a closed connection",
            "Number of times a running request was canceled due to a closed connection");
    m_Counters[ePSGS_CancelBacklogged] =
        new SCounterInfo(
            "CancelBacklogged", "Number of times a backlogged request was canceled due to a closed connection",
            "Number of times a backlogged request was canceled due to a closed connection");
    m_Counters[ePSGS_100] =
        new SCounterInfo(
            "RequestStop100", "Request stop counter with status 100",
            "Number of times a request finished with status 100");
    m_Counters[ePSGS_101] =
        new SCounterInfo(
            "RequestStop101", "Request stop counter with status 101",
            "Number of times a request finished with status 101");
    m_Counters[ePSGS_200] =
        new SCounterInfo(
            "RequestStop200", "Request stop counter with status 200",
            "Number of times a request finished with status 200");
    m_Counters[ePSGS_201] =
        new SCounterInfo(
            "RequestStop201", "Request stop counter with status 201",
            "Number of times a request finished with status 201");
    m_Counters[ePSGS_202] =
        new SCounterInfo(
            "RequestStop202", "Request stop counter with status 202",
            "Number of times a request finished with status 202");
    m_Counters[ePSGS_203] =
        new SCounterInfo(
            "RequestStop203", "Request stop counter with status 203",
            "Number of times a request finished with status 203");
    m_Counters[ePSGS_204] =
        new SCounterInfo(
            "RequestStop204", "Request stop counter with status 204",
            "Number of times a request finished with status 204");
    m_Counters[ePSGS_205] =
        new SCounterInfo(
            "RequestStop205", "Request stop counter with status 205",
            "Number of times a request finished with status 205");
    m_Counters[ePSGS_206] =
        new SCounterInfo(
            "RequestStop206", "Request stop counter with status 206",
            "Number of times a request finished with status 206");
    m_Counters[ePSGS_299] =
        new SCounterInfo(
            "RequestStop299", "Request stop counter with status 299",
            "Number of times a request finished with status 299");
    m_Counters[ePSGS_300] =
        new SCounterInfo(
            "RequestStop300", "Request stop counter with status 300",
            "Number of times a request finished with status 300");
    m_Counters[ePSGS_301] =
        new SCounterInfo(
            "RequestStop301", "Request stop counter with status 301",
            "Number of times a request finished with status 301");
    m_Counters[ePSGS_302] =
        new SCounterInfo(
            "RequestStop302", "Request stop counter with status 302",
            "Number of times a request finished with status 302");
    m_Counters[ePSGS_303] =
        new SCounterInfo(
            "RequestStop303", "Request stop counter with status 303",
            "Number of times a request finished with status 303");
    m_Counters[ePSGS_304] =
        new SCounterInfo(
            "RequestStop304", "Request stop counter with status 304",
            "Number of times a request finished with status 304");
    m_Counters[ePSGS_305] =
        new SCounterInfo(
            "RequestStop305", "Request stop counter with status 305",
            "Number of times a request finished with status 305");
    m_Counters[ePSGS_307] =
        new SCounterInfo(
            "RequestStop307", "Request stop counter with status 307",
            "Number of times a request finished with status 307");
    m_Counters[ePSGS_400] =
        new SCounterInfo(
            "RequestStop400", "Request stop counter with status 400",
            "Number of times a request finished with status 400");
    m_Counters[ePSGS_401] =
        new SCounterInfo(
            "RequestStop401", "Request stop counter with status 401",
            "Number of times a request finished with status 401");
    m_Counters[ePSGS_402] =
        new SCounterInfo(
            "RequestStop402", "Request stop counter with status 402",
            "Number of times a request finished with status 402");
    m_Counters[ePSGS_403] =
        new SCounterInfo(
            "RequestStop403", "Request stop counter with status 403",
            "Number of times a request finished with status 403");
    m_Counters[ePSGS_404] =
        new SCounterInfo(
            "RequestStop404", "Request stop counter with status 404",
            "Number of times a request finished with status 404");
    m_Counters[ePSGS_405] =
        new SCounterInfo(
            "RequestStop405", "Request stop counter with status 405",
            "Number of times a request finished with status 405");
    m_Counters[ePSGS_406] =
        new SCounterInfo(
            "RequestStop406", "Request stop counter with status 406",
            "Number of times a request finished with status 406");
    m_Counters[ePSGS_407] =
        new SCounterInfo(
            "RequestStop407", "Request stop counter with status 407",
            "Number of times a request finished with status 407");
    m_Counters[ePSGS_408] =
        new SCounterInfo(
            "RequestStop408", "Request stop counter with status 408",
            "Number of times a request finished with status 408");
    m_Counters[ePSGS_409] =
        new SCounterInfo(
            "RequestStop409", "Request stop counter with status 409",
            "Number of times a request finished with status 409");
    m_Counters[ePSGS_410] =
        new SCounterInfo(
            "RequestStop410", "Request stop counter with status 410",
            "Number of times a request finished with status 410");
    m_Counters[ePSGS_411] =
        new SCounterInfo(
            "RequestStop411", "Request stop counter with status 411",
            "Number of times a request finished with status 411");
    m_Counters[ePSGS_412] =
        new SCounterInfo(
            "RequestStop412", "Request stop counter with status 412",
            "Number of times a request finished with status 412");
    m_Counters[ePSGS_413] =
        new SCounterInfo(
            "RequestStop413", "Request stop counter with status 413",
            "Number of times a request finished with status 413");
    m_Counters[ePSGS_414] =
        new SCounterInfo(
            "RequestStop414", "Request stop counter with status 414",
            "Number of times a request finished with status 414");
    m_Counters[ePSGS_415] =
        new SCounterInfo(
            "RequestStop415", "Request stop counter with status 415",
            "Number of times a request finished with status 415");
    m_Counters[ePSGS_416] =
        new SCounterInfo(
            "RequestStop416", "Request stop counter with status 416",
            "Number of times a request finished with status 416");
    m_Counters[ePSGS_417] =
        new SCounterInfo(
            "RequestStop417", "Request stop counter with status 417",
            "Number of times a request finished with status 417");
    m_Counters[ePSGS_422] =
        new SCounterInfo(
            "RequestStop422", "Request stop counter with status 422",
            "Number of times a request finished with status 422");
    m_Counters[ePSGS_499] =
        new SCounterInfo(
            "RequestStop499", "Request stop counter with status 499",
            "Number of times a request finished with status 499");
    m_Counters[ePSGS_500] =
        new SCounterInfo(
            "RequestStop500", "Request stop counter with status 500",
            "Number of times a request finished with status 500");
    m_Counters[ePSGS_501] =
        new SCounterInfo(
            "RequestStop501", "Request stop counter with status 501",
            "Number of times a request finished with status 501");
    m_Counters[ePSGS_502] =
        new SCounterInfo(
            "RequestStop502", "Request stop counter with status 502",
            "Number of times a request finished with status 502");
    m_Counters[ePSGS_503] =
        new SCounterInfo(
            "RequestStop503", "Request stop counter with status 503",
            "Number of times a request finished with status 503");
    m_Counters[ePSGS_504] =
        new SCounterInfo(
            "RequestStop504", "Request stop counter with status 504",
            "Number of times a request finished with status 504");
    m_Counters[ePSGS_505] =
        new SCounterInfo(
            "RequestStop505", "Request stop counter with status 505",
            "Number of times a request finished with status 505");
    m_Counters[ePSGS_xxx] =
        new SCounterInfo(
            "RequestStopXXX", "Request stop counter with unknown status",
            "Number of times a request finished with unknown status");

    // The counters below are for the sake of an identifier, name and
    // description. The name and description can be overwritten by the
    // configuration values

    m_Counters[ePSGS_CassandraActiveStatements] =
        new SCounterInfo(
            "CassandraActiveStatementsCount", "Cassandra active statements counter",
            "Number of the currently active Cassandra queries",
            SCounterInfo::ePSGS_Arbitrary);
    m_Counters[ePSGS_NumberOfConnections] =
        new SCounterInfo(
            "NumberOfTCPConnections", "Client TCP connections counter",
            "Number of the client TCP connections",
            SCounterInfo::ePSGS_Arbitrary);
    m_Counters[ePSGS_ActiveProcessorGroups] =
        new SCounterInfo(
            "ActiveProcessorGroupCount", "Active processor groups counter",
            "Number of processor groups currently active in the dispatcher",
            SCounterInfo::ePSGS_Arbitrary);
    m_Counters[ePSGS_SplitInfoCacheSize] =
        new SCounterInfo(
            "SplitInfoCacheSize", "Split info cache size",
            "Number of records in the split info cache",
            SCounterInfo::ePSGS_Arbitrary);
    m_Counters[ePSGS_MyNCBIOKCacheSize] =
        new SCounterInfo(
            "MyNCBIOKCacheSize", "My NCBI user info OK cache size",
            "Number of records in the my NCBI user info OK cache",
            SCounterInfo::ePSGS_Arbitrary);
    m_Counters[ePSGS_MyNCBINotFoundCacheSize] =
        new SCounterInfo(
            "MyNCBINotFoundCacheSize", "My NCBI not found cache size",
            "Number of records in the my NCBI not found cache",
            SCounterInfo::ePSGS_Arbitrary);
    m_Counters[ePSGS_MyNCBIErrorCacheSize] =
        new SCounterInfo(
            "MyNCBIErrorCacheSize", "My NCBI error cache size",
            "Number of records in the my NCBI error cache",
            SCounterInfo::ePSGS_Arbitrary);
    m_Counters[ePSGS_ShutdownRequested] =
        new SCounterInfo(
            "ShutdownRequested", "Shutdown requested flag",
            "Shutdown requested flag",
            SCounterInfo::ePSGS_Arbitrary);
    m_Counters[ePSGS_GracefulShutdownExpiredInSec] =
        new SCounterInfo(
            "GracefulShutdownExpiredInSec", "Graceful shutdown expiration",
            "Graceful shutdown expiration in seconds from now",
            SCounterInfo::ePSGS_Arbitrary);


    // Counters below must be per processor
    size_t  counter = static_cast<size_t>(ePSGS_LastCounter) - 1;
    while (counter > static_cast<size_t>(ePSGS_MaxIndividualCounter)) {
        m_PerProcessorCounters.push_back(vector<SCounterInfo *>());
        --counter;
    }

    string      proc_group_names[m_ProcGroupToIndex.size()];
    for (const auto &  item : m_ProcGroupToIndex) {
        proc_group_names[item.second] = item.first;
    }

    size_t      index;
    for (const auto &  proc_group : proc_group_names) {
        index = static_cast<size_t>(ePSGS_InputSeqIdNotResolved) -
                static_cast<size_t>(ePSGS_MaxIndividualCounter) - 1;

        m_PerProcessorCounters[index].push_back(
            new SCounterInfo(
                proc_group + "_InputSeqIdNotResolved",
                "Seq id not resolved counter (" + proc_group + ")",
                "Number of times a client provided seq id could not be resolved by " + proc_group));


        index = static_cast<size_t>(ePSGS_AnnotationBlobNotFound) -
                static_cast<size_t>(ePSGS_MaxIndividualCounter) - 1;

        m_PerProcessorCounters[index].push_back(
            new SCounterInfo(
                proc_group + "_AnnotationBlobNotFoundCount",
                "Annotation blob not found counter (" + proc_group + ")",
                "Number of times an annotation blob was not found by " + proc_group));


        index = static_cast<size_t>(ePSGS_AnnotationNotFound) -
                static_cast<size_t>(ePSGS_MaxIndividualCounter) - 1;

        m_PerProcessorCounters[index].push_back(
            new SCounterInfo(
                proc_group + "_AnnotationNotFoundCount",
                "Annotation not found counter (" + proc_group + ")",
                "Number of times an annotation was not found by " + proc_group));


        index = static_cast<size_t>(ePSGS_GetBlobNotFound) -
                static_cast<size_t>(ePSGS_MaxIndividualCounter) - 1;

        m_PerProcessorCounters[index].push_back(
            new SCounterInfo(
                proc_group + "_GetBlobNotFoundCount",
                "Blob not found counter (" + proc_group + ")",
                "Number of times clients requested a blob which was not found by " + proc_group));


        index = static_cast<size_t>(ePSGS_TSEChunkNotFound) -
                static_cast<size_t>(ePSGS_MaxIndividualCounter) - 1;

        m_PerProcessorCounters[index].push_back(
            new SCounterInfo(
                proc_group + "_TSEChunkNotFoundCount",
                "TSE chunk not found counter (" + proc_group + ")",
                "Number of times a TSE chunk was not found by " + proc_group));


        index = static_cast<size_t>(ePSGS_ProcUnknownError) -
                static_cast<size_t>(ePSGS_MaxIndividualCounter) - 1;

        m_PerProcessorCounters[index].push_back(
            new SCounterInfo(
                proc_group + "_ProcUnknownErrorCount",
                "Processor unknown error counter (" + proc_group + ")",
                "Number of times an unknown error has been detected by " + proc_group));


        index = static_cast<size_t>(ePSGS_OpTooLong) -
                static_cast<size_t>(ePSGS_MaxIndividualCounter) - 1;

        m_PerProcessorCounters[index].push_back(
            new SCounterInfo(
                proc_group + "_OperationTooLong",
                "Operation took too long (" + proc_group + ")",
                "The number of times an operation (like a backend retrieval) "
                "took longer than a configurable time by " + proc_group));
    }
}


CPSGSCounters::~CPSGSCounters()
{
    for (size_t  k=0; k < ePSGS_MaxIndividualCounter; ++k) {
        delete m_Counters[k];
    }

    for (size_t  k=0; k < m_PerProcessorCounters.size(); ++k) {
        for (size_t  m=0; m < m_PerProcessorCounters[k].size(); ++m) {
            delete m_PerProcessorCounters[k][m];
        }
    }
}


void CPSGSCounters::Increment(IPSGS_Processor *  processor,
                              EPSGS_CounterType  counter)
{
    if (counter >= ePSGS_LastCounter) {
        PSG_ERROR("Invalid counter id " + to_string(counter) +
                  ". Ignore and continue.");
        return;
    }

    if (IsPerProcessorCounter(counter)) {
        if (processor == nullptr) {
            PSG_ERROR("Invalid nullptr processor for the per processor counter id " +
                      to_string(counter) + ". Ignore and continue.");
            return;
        }

        size_t  cnt_index = static_cast<size_t>(counter) -
                            static_cast<size_t>(ePSGS_MaxIndividualCounter) - 1;
        size_t  proc_index = m_ProcGroupToIndex[processor->GetGroupName()];

        ++(m_PerProcessorCounters[cnt_index][proc_index]->m_Value);
        return;
    }

    if (counter >= ePSGS_MaxIndividualCounter) {
        PSG_ERROR("Invalid counter id " + to_string(counter) +
                  " (non per-processor). Ignore and continue.");
        return;
    }

    ++(m_Counters[counter]->m_Value);
}


uint64_t CPSGSCounters::GetValue(EPSGS_CounterType  counter)
{
    if (counter >= ePSGS_MaxIndividualCounter) {
        PSG_ERROR("Invalid counter id " + to_string(counter) +
                  ". Only non per-processor counters support value retrieval. "
                  "Ignore and continue.");
        return 0;
    }

    return m_Counters[counter]->m_Value;
}

uint64_t CPSGSCounters::GetFinishedRequestsCounter(void)
{
    return m_FinishedRequestsCounter;
}


CPSGSCounters::EPSGS_CounterType
CPSGSCounters::StatusToCounterType(int  status)
{
    switch (status) {
        case 100:   return ePSGS_100;
        case 101:   return ePSGS_101;
        case 200:   return ePSGS_200;
        case 201:   return ePSGS_201;
        case 202:   return ePSGS_202;
        case 203:   return ePSGS_203;
        case 204:   return ePSGS_204;
        case 205:   return ePSGS_205;
        case 206:   return ePSGS_206;
        case 299:   return ePSGS_299;
        case 300:   return ePSGS_300;
        case 301:   return ePSGS_301;
        case 302:   return ePSGS_302;
        case 303:   return ePSGS_303;
        case 304:   return ePSGS_304;
        case 305:   return ePSGS_305;
        case 307:   return ePSGS_307;
        case 400:   return ePSGS_400;
        case 401:   return ePSGS_401;
        case 402:   return ePSGS_402;
        case 403:   return ePSGS_403;
        case 404:   return ePSGS_404;
        case 405:   return ePSGS_405;
        case 406:   return ePSGS_406;
        case 407:   return ePSGS_407;
        case 408:   return ePSGS_408;
        case 409:   return ePSGS_409;
        case 410:   return ePSGS_410;
        case 411:   return ePSGS_411;
        case 412:   return ePSGS_412;
        case 413:   return ePSGS_413;
        case 414:   return ePSGS_414;
        case 415:   return ePSGS_415;
        case 416:   return ePSGS_416;
        case 417:   return ePSGS_417;
        case 422:   return ePSGS_422;
        case 499:   return ePSGS_499;
        case 500:   return ePSGS_500;
        case 501:   return ePSGS_501;
        case 502:   return ePSGS_502;
        case 503:   return ePSGS_503;
        case 504:   return ePSGS_504;
        case 505:   return ePSGS_505;
    }
    return ePSGS_xxx;
}


void CPSGSCounters::IncrementRequestStopCounter(int  status)
{
    ++m_FinishedRequestsCounter;
    Increment(nullptr, StatusToCounterType(status));
}


void CPSGSCounters::UpdateConfiguredNameDescription(
                            const map<string, tuple<string, string>> &  conf)
{
    for (auto const & conf_item : conf) {
        for (size_t  k=0; k < ePSGS_MaxIndividualCounter; ++k) {
            if (m_Counters[k]->m_Identifier == conf_item.first) {
                m_Counters[k]->m_Name = get<0>(conf_item.second);
                m_Counters[k]->m_Description = get<1>(conf_item.second);
                break;
            }
        }

        for (size_t  k=0; k < m_PerProcessorCounters.size(); ++k) {
            for (size_t  m=0; m < m_PerProcessorCounters[k].size(); ++m) {
                if (m_PerProcessorCounters[k][m]->m_Identifier == conf_item.first) {
                    m_PerProcessorCounters[k][m]->m_Name = get<0>(conf_item.second);
                    m_PerProcessorCounters[k][m]->m_Description = get<1>(conf_item.second);
                    break;
                }
            }
        }
    }
}


void CPSGSCounters::PopulateDictionary(CJsonNode &  dict)
{
    uint64_t    value(0);

    for (size_t  k=0; k < ePSGS_MaxIndividualCounter; ++k) {
        if (m_Counters[k]->m_Type == SCounterInfo::ePSGS_Monotonic) {
            value = m_Counters[k]->m_Value;
            AppendValueNode(dict, m_Counters[k]->m_Identifier,
                            m_Counters[k]->m_Name, m_Counters[k]->m_Description,
                            value);
        }
    }

    for (size_t  k=0; k < m_PerProcessorCounters.size(); ++k) {
        for (size_t  m=0; m < m_PerProcessorCounters[k].size(); ++m) {
            value = m_PerProcessorCounters[k][m]->m_Value;
            AppendValueNode(dict, m_PerProcessorCounters[k][m]->m_Identifier,
                            m_PerProcessorCounters[k][m]->m_Name,
                            m_PerProcessorCounters[k][m]->m_Description,
                            value);
        }
    }
}


void CPSGSCounters::Reset(void)
{
    for (size_t  k=0; k < ePSGS_MaxIndividualCounter; ++k) {
        if (m_Counters[k]->m_Type == SCounterInfo::ePSGS_Monotonic) {
            m_Counters[k]->m_Value = 0;
        }
    }

    for (size_t  k=0; k < m_PerProcessorCounters.size(); ++k) {
        for (size_t  m=0; m < m_PerProcessorCounters[k].size(); ++m) {
            m_PerProcessorCounters[k][m]->m_Value = 0;
        }
    }
}


void CPSGSCounters::AppendValueNode(CJsonNode &  dict, const string &  id,
                                    const string &  name, const string &  description,
                                    uint64_t  value)
{
    CJsonNode   value_dict(CJsonNode::NewObjectNode());

    value_dict.SetInteger(kValue, value);
    value_dict.SetString(kName, name);
    value_dict.SetString(kDescription, description);
    dict.SetByKey(id, value_dict);
}


void CPSGSCounters::AppendValueNode(CJsonNode &  dict, const string &  id,
                                    const string &  name, const string &  description,
                                    bool  value)
{
    CJsonNode   value_dict(CJsonNode::NewObjectNode());

    value_dict.SetBoolean(kValue, value);
    value_dict.SetString(kName, name);
    value_dict.SetString(kDescription, description);
    dict.SetByKey(id, value_dict);

}


void CPSGSCounters::AppendValueNode(CJsonNode &  dict, const string &  id,
                                    const string &  name, const string &  description,
                                    const string &  value)
{
    CJsonNode   value_dict(CJsonNode::NewObjectNode());

    value_dict.SetString(kValue, value);
    value_dict.SetString(kName, name);
    value_dict.SetString(kDescription, description);
    dict.SetByKey(id, value_dict);

}


void CPSGSCounters::AppendValueNode(CJsonNode &  dict,
                                    EPSGS_CounterType  counter_type,
                                    uint64_t  value)
{
    AppendValueNode(dict,
                    m_Counters[counter_type]->m_Identifier,
                    m_Counters[counter_type]->m_Name,
                    m_Counters[counter_type]->m_Description,
                    value);
}


void CPSGSCounters::AppendValueNode(CJsonNode &  dict,
                                    EPSGS_CounterType  counter_type,
                                    bool  value)
{
    AppendValueNode(dict,
                    m_Counters[counter_type]->m_Identifier,
                    m_Counters[counter_type]->m_Name,
                    m_Counters[counter_type]->m_Description,
                    value);
}


void CPSGSCounters::AppendValueNode(CJsonNode &  dict,
                                    EPSGS_CounterType  counter_type,
                                    const string &  value)
{
    AppendValueNode(dict,
                    m_Counters[counter_type]->m_Identifier,
                    m_Counters[counter_type]->m_Name,
                    m_Counters[counter_type]->m_Description,
                    value);
}

