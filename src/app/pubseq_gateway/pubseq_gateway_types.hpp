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

// Error codes for the PSG protocol messages
enum EPubseqGatewayErrorCode {
    eUnknownSatellite = 300,
    eBadURL,
    eMissingParameter,
    eMalformedParameter,
    eUnknownResolvedSatellite,
    eNoCanonicalTranslation,
    eNoBioseqInfo,
    eBadID2Info,
    eResolutionNotFound,
    eUnpackingError,
    eUnknownError,
    eBlobPropsNotFound
};


// The user may come with an accession or with a pair sat/sat key
// The requests are counted separately so the enumeration distinguish them.
enum EBlobIdentificationType {
    eBySeqId,
    eBySatAndSatKey
};


// Pretty much copied from the client; the justfication for copying is:
// "it will be decoupled with the client type"
enum EServIncludeData {
    fServNoTSE = (1 << 0),
    fServFastInfo = (1 << 1),
    fServWholeTSE = (1 << 2),
    fServOrigTSE = (1 << 3),
    fServCanonicalId = (1 << 4),
    fServSeqIds = (1 << 5),
    fServMoleculeType = (1 << 6),
    fServLength = (1 << 7),
    fServState = (1 << 8),
    fServBlobId = (1 << 9),
    fServTaxId = (1 << 10),
    fServHash = (1 << 11)
};

// Bit-set of EServIncludeData flags
typedef int TServIncludeData;


enum EOutputFormat {
    eProtobufFormat,
    eJsonFormat,
    eNativeFormat,

    eUnknownFormat
};


#endif
