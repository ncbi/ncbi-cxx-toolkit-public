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
using namespace std;

// Processor priority
// The higher the value the higher the priority is
typedef int     TProcessorPriority;
const int       kUnknownPriority = -1;


// For the future extensions
typedef string  TAuthToken;


// Error codes for the PSG protocol messages
enum EPSGS_PubseqGatewayErrorCode {
    ePSGS_UnknownSatellite = 300,
    ePSGS_BadURL,
    ePSGS_NoUsefulCassandra,
    ePSGS_MissingParameter,
    ePSGS_MalformedParameter,
    ePSGS_UnknownResolvedSatellite,
    ePSGS_NoBioseqInfo,
    ePSGS_BioseqInfoError,
    ePSGS_BadID2Info,
    ePSGS_UnpackingError,
    ePSGS_UnknownError,
    ePSGS_BlobPropsNotFound,
    ePSGS_UnresolvedSeqId,
    ePSGS_InsufficientArguments,
    ePSGS_InvalidId2Info,
    ePSGS_SplitHistoryNotFound,
    ePSGS_BioseqInfoNotFoundForGi,       // whole bioseq_info record not found
    ePSGS_BioseqInfoGiNotFoundForGi,     // gi is not found in in the seq_ids field
                                         // in the bioseq_info record
    ePSGS_BioseqInfoMultipleRecords,
    ePSGS_ServerLogicError,
    ePSGS_BioseqInfoAccessionAdjustmentError,
    ePSGS_NoProcessor,
    ePSGS_ShuttingDown,
    ePSGS_Unauthorised,

    ePSGS_TestIOError,          // Exceptions when handling certain requests
    ePSGS_StatisticsError,      //
    ePSGS_AckAlertError,        //
    ePSGS_GetAlertsError,       //
    ePSGS_ShutdownError,        //
    ePSGS_StatusError,          //
    ePSGS_InfoError,            //
    ePSGS_ConfigError,          //
    ePSGS_HealthError,          //

    ePSGS_TooManyRequests,
    ePSGS_RequestCancelled,

    ePSGS_BlobRetrievalIsNotAuthorized
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

#endif

