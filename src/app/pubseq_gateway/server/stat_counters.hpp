#ifndef PUBSEQ_GATEWAY_STAT_COUNTERS__HPP
#define PUBSEQ_GATEWAY_STAT_COUNTERS__HPP

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


// All the statistics counter IDs

static const string     kBadUrlPathCount("BadUrlPathCount");
static const string     kInsufficientArgumentsCount("InsufficientArgumentsCount");
static const string     kCassandraActiveStatementsCount("CassandraActiveStatementsCount");
static const string     kNumberOfConnections("NumberOfConnections");
static const string     kActiveRequestCount("ActiveRequestCount");
static const string     kShutdownRequested("ShutdownRequested");
static const string     kGracefulShutdownExpiredInSec("GracefulShutdownExpiredInSec");
static const string     kMalformedArgumentsCount("MalformedArgumentsCount");
static const string     kGetBlobNotFoundCount("GetBlobNotFoundCount");
static const string     kUnknownErrorCount("UnknownErrorCount");
static const string     kClientSatToSatNameErrorCount("ClientSatToSatNameErrorCount");
static const string     kServerSatToSatNameErrorCount("ServerSatToSatNameErrorCount");
static const string     kBlobPropsNotFoundErrorCount("BlobPropsNotFoundErrorCount");
static const string     kLMDBErrorCount("LMDBErrorCount");
static const string     kCassQueryTimeoutErrorCount("CassQueryTimeoutErrorCount");
static const string     kInvalidId2InfoErrorCount("InvalidId2InfoErrorCount");
static const string     kSplitHistoryNotFoundErrorCount("SplitHistoryNotFoundErrorCount");
static const string     kMaxHopsExceededErrorCount("MaxHopsExceededErrorCount");
static const string     kTotalErrorCount("TotalErrorCount");
static const string     kInputSeqIdNotResolved("InputSeqIdNotResolved");
static const string     kTSEChunkSplitVersionCacheMatched("TSEChunkSplitVersionCacheMatched");
static const string     kTSEChunkSplitVersionCacheNotMatched("TSEChunkSplitVersionCacheNotMatched");
static const string     kAdminRequestCount("AdminRequestCount");
static const string     kResolveRequestCount("ResolveRequestCount");
static const string     kGetBlobBySeqIdRequestCount("GetBlobBySeqIdRequestCount");
static const string     kGetBlobBySatSatKeyRequestCount("GetBlobBySatSatKeyRequestCount");
static const string     kGetNamedAnnotationsCount("GetNamedAnnotationsCount");
static const string     kTestIORequestCount("TestIORequestCount");
static const string     kGetTSEChunkCount("GetTSEChunkCount");
static const string     kHealthRequestCount("HealthRequestCount");
static const string     kTotalRequestCount("TotalRequestCount");
static const string     kSi2csiCacheHit("Si2csiCacheHit");
static const string     kSi2csiCacheMiss("Si2csiCacheMiss");
static const string     kBioseqInfoCacheHit("BioseqInfoCacheHit");
static const string     kBioseqInfoCacheMiss("BioseqInfoCacheMiss");
static const string     kBlobPropCacheHit("BlobPropCacheHit");
static const string     kBlobPropCacheMiss("BlobPropCacheMiss");
static const string     kSi2csiNotFound("Si2csiNotFound");
static const string     kSi2csiFoundOne("Si2csiFoundOne");
static const string     kSi2csiFoundMany("Si2csiFoundMany");
static const string     kBioseqInfoNotFound("BioseqInfoNotFound");
static const string     kBioseqInfoFoundOne("BioseqInfoFoundOne");
static const string     kBioseqInfoFoundMany("BioseqInfoFoundMany");
static const string     kSi2csiError("Si2csiError");
static const string     kBioseqInfoError("BioseqInfoError");

#endif
