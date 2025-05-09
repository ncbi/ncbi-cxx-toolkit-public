#ifndef PUBSEQ_GATEWAY_TYPES__HPP
#define PUBSEQ_GATEWAY_TYPES__HPP

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

#include <string>
#include <chrono>
using namespace std;

// Processor priority
// The higher the value the higher the priority is
typedef int     TProcessorPriority;
const int       kUnknownPriority = -1;

// steady_clock is guaranteed not to go back so it is most useful to measure
// the intervals.
// high_resolution_clock could be steady or not depending on platform or
// options
// system_clock is not steady, i.e. can go back
// Mostly PSG needs to calculate intervals so this type is used throughout the
// server code
using psg_clock_t = chrono::steady_clock;
using psg_time_point_t = psg_clock_t::time_point;


// For the future extensions
typedef string  TAuthToken;


// Error codes for the PSG protocol messages
enum EPSGS_PubseqGatewayErrorCode {
    ePSGS_UnknownSatellite                      = 300,
    ePSGS_BadURL                                = 301,
    ePSGS_NoUsefulCassandra                     = 302,
    ePSGS_MalformedParameter                    = 304,
    ePSGS_UnknownResolvedSatellite              = 305,
    ePSGS_NoBioseqInfo                          = 306,
    ePSGS_BioseqInfoError                       = 307,
    ePSGS_BadID2Info                            = 308,
    ePSGS_UnpackingError                        = 309,
    ePSGS_UnknownError                          = 310,
    ePSGS_NoBlobPropsError                      = 311,
    ePSGS_UnresolvedSeqId                       = 312,
    ePSGS_InsufficientArguments                 = 313,
    ePSGS_InvalidId2Info                        = 314,
    ePSGS_NoSplitHistoryError                   = 315,

    // whole bioseq_info record not found
    ePSGS_NoBioseqInfoForGiError                = 316,

    ePSGS_BioseqInfoMultipleRecords             = 318,
    ePSGS_ServerLogicError                      = 319,
    ePSGS_BioseqInfoAccessionAdjustmentError    = 320,
    ePSGS_NoProcessor                           = 321,
    ePSGS_ShuttingDown                          = 322,
    ePSGS_Unauthorised                          = 323,

    // Exceptions when handling certain requests
    ePSGS_TestIOError                           = 324,
    ePSGS_StatisticsError                       = 325,
    ePSGS_AckAlertError                         = 326,
    ePSGS_GetAlertsError                        = 327,
    ePSGS_ShutdownError                         = 328,
    ePSGS_StatusError                           = 329,
    ePSGS_InfoError                             = 330,
    ePSGS_ConfigError                           = 331,
    ePSGS_HealthError                           = 332,

    ePSGS_TooManyRequests                       = 333,
    ePSGS_RequestCancelled                      = 334,

    ePSGS_BlobRetrievalIsNotAuthorized          = 335,

    ePSGS_RequestTimeout                        = 336,
    ePSGS_NotFoundAndNotInstantiated            = 337,

    ePSGS_IPGNotFound                           = 338,
    ePSGS_IPGKeyspaceNotAvailable               = 339,

    ePSGS_NoWebCubbyUserCookieError             = 340,
    ePSGS_MyNCBIError                           = 341,
    ePSGS_MyNCBINotFound                        = 342,
    ePSGS_SecureSatUnauthorized                 = 343,
    ePSGS_CassConnectionError                   = 344,
    ePSGS_MyNCBIRequestInitiatorDestroyed       = 345,

    ePSGS_BlobChunkAfterFallbackRequested       = 346,
    ePSGS_NotFoundID2BlobPropWithFallback       = 347,
    ePSGS_ID2ChunkErrorWithFallback             = 348,
    ePSGS_ID2ChunkErrorAfterFallbackRequested   = 349,
    ePSGS_ID2InfoParseErrorFallback             = 350,

    ePSGS_ConnectionExceedsSoftLimit            = 351,

    ePSGS_GetSatMappingError                    = 352,

    ePSGS_DispatcherStatusError                 = 353,
    ePSGS_ConnectionsStatusError                = 354,
    ePSGS_HelloStatusError                      = 355
};


enum EPSGS_ResolutionResult {
    ePSGS_Si2csiCache,
    ePSGS_Si2csiDB,
    ePSGS_BioseqCache,
    ePSGS_BioseqDB,
    ePSGS_NotResolved,
    ePSGS_PostponedForDB
};


enum EPSGS_SeqIdParsingResult {
    ePSGS_ParsedOK,
    ePSGS_ParseFailed
};


enum EPSGS_DbFetchType {
    ePSGS_BlobBySeqIdFetch,
    ePSGS_BlobBySatSatKeyFetch,
    ePSGS_AnnotationFetch,
    ePSGS_AnnotationBlobFetch,
    ePSGS_TSEChunkFetch,
    ePSGS_BioseqInfoFetch,
    ePSGS_Si2csiFetch,
    ePSGS_SplitHistoryFetch,
    ePSGS_PublicCommentFetch,
    ePSGS_AccVerHistoryFetch,
    ePSGS_IPGResolveFetch,
    ePSGS_UnknownFetch
};


enum EPSGS_CacheLookupResult {
    ePSGS_CacheHit,
    ePSGS_CacheNotHit,
    ePSGS_CacheFailure          // LMDB may throw an exception
};


enum EPSGS_AccessionAdjustmentResult {
    ePSGS_NotRequired,
    ePSGS_AdjustedWithGi,
    ePSGS_AdjustedWithAny,
    ePSGS_SeqIdsEmpty,
    ePSGS_LogicError
};


enum EPSGS_ReplyMimeType {
    ePSGS_JsonMime,
    ePSGS_HtmlMime,
    ePSGS_BinaryMime,
    ePSGS_PlainTextMime,
    ePSGS_ImageMime,
    ePSGS_PSGMime,

    ePSGS_NotSet
};


enum EPSGS_BlobSkipReason {
    ePSGS_BlobExcluded,
    ePSGS_BlobInProgress,
    ePSGS_BlobSent
};


// At startup time the server does the following:
// - opens a database connection
// - reads mapping from the root keyspace
// - creates a cache for si2csi, bioseqinfo and blobprop
// If there is an error on any of these stages the server tries to recover
// periodically
enum EPSGS_StartupDataState {
    ePSGS_NoCassConnection,
    ePSGS_NoValidCassMapping,
    ePSGS_NoCassCache,
    ePSGS_StartupDataOK
};


enum EPSGS_LoggingFlag {
    ePSGS_NeedLogging,
    ePSGS_SkipLogging
};


enum EPSGS_StatusUpdateFlag {
    ePSGS_NeedStatusUpdate,
    ePSGS_SkipStatusUpdate
};

#endif

