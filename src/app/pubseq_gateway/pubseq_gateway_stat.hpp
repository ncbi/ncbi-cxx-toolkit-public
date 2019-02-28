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


// The class is used to collect information about errors
class CPubseqGatewayErrorCounters
{
public:
    CPubseqGatewayErrorCounters() :
        m_BadUrlPath(0), m_InsufficientArguments(0), m_MalformedArguments(0),
        m_ResolveError(0), m_GetBlobNotFound(0),
        m_GetBlobError(0), m_UnknownError(0), m_ClientSatToSatNameError(0),
        m_ServerSatToSatNameError(0), m_CanonicalSeqIdError(0),
        m_BioseqID2InfoError(0), m_BioseqInfoError(0),
        m_BlobPropsNotFoundError(0), m_LMDBError(0)
    {}

    void IncBadUrlPath(void)
    { ++m_BadUrlPath; }

    void IncInsufficientArguments(void)
    { ++m_InsufficientArguments; }

    void IncMalformedArguments(void)
    { ++m_MalformedArguments; }

    void IncResolveError(void)
    { ++m_ResolveError; }

    void IncGetBlobNotFound(void)
    { ++m_GetBlobNotFound; }

    void IncGetBlobError(void)
    { ++m_GetBlobError; }

    void IncUnknownError(void)
    { ++m_UnknownError; }

    void IncClientSatToSatName(void)
    { ++m_ClientSatToSatNameError; }

    void IncServerSatToSatName(void)
    { ++m_ServerSatToSatNameError; }

    void IncCanonicalSeqIdError(void)
    { ++m_CanonicalSeqIdError; }

    void IncBioseqInfoError(void)
    { ++m_BioseqInfoError; }

    void IncBioseqID2Info(void)
    { ++m_BioseqID2InfoError; }

    void IncBlobPropsNotFoundError(void)
    { ++m_BlobPropsNotFoundError; }

    void IncLMDBError(void)
    { ++m_LMDBError; }

    void PopulateDictionary(CJsonNode &  dict) const;

private:
    atomic_uint_fast64_t        m_BadUrlPath;
    atomic_uint_fast64_t        m_InsufficientArguments;
    atomic_uint_fast64_t        m_MalformedArguments;
    atomic_uint_fast64_t        m_ResolveError;         // 502
    atomic_uint_fast64_t        m_GetBlobNotFound;      // 404
    atomic_uint_fast64_t        m_GetBlobError;         // 502
    atomic_uint_fast64_t        m_UnknownError;         // 503
    atomic_uint_fast64_t        m_ClientSatToSatNameError;
    atomic_uint_fast64_t        m_ServerSatToSatNameError;
    atomic_uint_fast64_t        m_CanonicalSeqIdError;
    atomic_uint_fast64_t        m_BioseqID2InfoError;
    atomic_uint_fast64_t        m_BioseqInfoError;
    atomic_uint_fast64_t        m_BlobPropsNotFoundError;
    atomic_uint_fast64_t        m_LMDBError;
};



class CPubseqGatewayRequestCounters
{
public:
    CPubseqGatewayRequestCounters() :
        m_Admin(0), m_Resolve(0),
        m_GetBlobBySeqId(0), m_GetBlobBySatSatKey(0), m_GetNA(0),
        m_ResolvedAsPrimaryOSLT(0), m_ResolvedAsSecondaryOSLT(0),
        m_ResolvedAsPrimaryOSLTinDB(0), m_ResolvedAsSecondaryOSLTinDB(0),
        m_NotResolved(0)
    {}

    void IncAdmin(void)
    { ++m_Admin; }

    void IncResolve(void)
    { ++m_Resolve; }

    void IncGetBlobBySeqId(void)
    { ++m_GetBlobBySeqId; }

    void IncGetBlobBySatSatKey(void)
    { ++m_GetBlobBySatSatKey; }

    void IncGetNA(void)
    { ++m_GetNA; }

    void IncResolvedAsPrimaryOSLT(void)
    { ++m_ResolvedAsPrimaryOSLT; }

    void IncResolvedAsSecondaryOSLT(void)
    { ++m_ResolvedAsSecondaryOSLT; }

    void IncResolvedAsPrimaryOSLTinDB(void)
    { ++m_ResolvedAsPrimaryOSLTinDB; }

    void IncResolvedAsSecondaryOSLTinDB(void)
    { ++m_ResolvedAsSecondaryOSLTinDB; }

    void IncNotResolved(void)
    { ++m_NotResolved; }

    void PopulateDictionary(CJsonNode &  dict) const;

private:
    atomic_uint_fast64_t        m_Admin;
    atomic_uint_fast64_t        m_Resolve;
    atomic_uint_fast64_t        m_GetBlobBySeqId;
    atomic_uint_fast64_t        m_GetBlobBySatSatKey;
    atomic_uint_fast64_t        m_GetNA;

    atomic_uint_fast64_t        m_ResolvedAsPrimaryOSLT;
    atomic_uint_fast64_t        m_ResolvedAsSecondaryOSLT;
    atomic_uint_fast64_t        m_ResolvedAsPrimaryOSLTinDB;
    atomic_uint_fast64_t        m_ResolvedAsSecondaryOSLTinDB;
    atomic_uint_fast64_t        m_NotResolved;
};


class CPubseqGatewayCacheCounters
{
public:
    CPubseqGatewayCacheCounters() :
        m_Si2csiCacheHit(0), m_Si2csiCacheMiss(0),
        m_BioseqInfoCacheHit(0), m_BioseqInfoCacheMiss(0),
        m_BlobPropCacheHit(0), m_BlobPropCacheMiss(0)
    {}

    void IncSi2csiCacheHit(void)
    { ++m_Si2csiCacheHit; }

    void IncSi2csiCacheMiss(void)
    { ++m_Si2csiCacheMiss; }

    void IncBioseqInfoCacheHit(void)
    { ++m_BioseqInfoCacheHit; }

    void IncBioseqInfoCacheMiss(void)
    { ++m_BioseqInfoCacheMiss; }

    void IncBlobPropCacheHit(void)
    { ++m_BlobPropCacheHit; }

    void IncBlobPropCacheMiss(void)
    { ++m_BlobPropCacheMiss; }

    void PopulateDictionary(CJsonNode &  dict) const;

private:
    atomic_uint_fast64_t        m_Si2csiCacheHit;
    atomic_uint_fast64_t        m_Si2csiCacheMiss;
    atomic_uint_fast64_t        m_BioseqInfoCacheHit;
    atomic_uint_fast64_t        m_BioseqInfoCacheMiss;
    atomic_uint_fast64_t        m_BlobPropCacheHit;
    atomic_uint_fast64_t        m_BlobPropCacheMiss;
};


#endif
