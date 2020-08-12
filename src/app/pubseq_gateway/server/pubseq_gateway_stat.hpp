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

void UpdateIdToNameDescription(const map<string, tuple<string, string>> &  conf);

void AppendValueNode(CJsonNode &  dict,
                     const string &  id,
                     uint64_t  value);
void AppendValueNode(CJsonNode &  dict,
                     const string &  id,
                     bool  value);
void AppendValueNode(CJsonNode &  dict,
                     const string &  id,
                     const string &  value);


// The class is used to collect information about errors
class CPubseqGatewayErrorCounters
{
public:
    CPubseqGatewayErrorCounters() :
        m_BadUrlPath(0), m_InsufficientArguments(0), m_MalformedArguments(0),
        m_GetBlobNotFound(0),
        m_UnknownError(0), m_ClientSatToSatNameError(0),
        m_ServerSatToSatNameError(0),
        m_BlobPropsNotFoundError(0), m_LMDBError(0),
        m_CassQueryTimeoutError(0),
        m_InvalidId2InfoError(0),
        m_MaxHopsExceededError(0)
    {}

    void IncBadUrlPath(void)
    { ++m_BadUrlPath; }

    void IncInsufficientArguments(void)
    { ++m_InsufficientArguments; }

    void IncMalformedArguments(void)
    { ++m_MalformedArguments; }

    void IncGetBlobNotFound(void)
    { ++m_GetBlobNotFound; }

    void IncUnknownError(void)
    { ++m_UnknownError; }

    void IncClientSatToSatName(void)
    { ++m_ClientSatToSatNameError; }

    void IncServerSatToSatName(void)
    { ++m_ServerSatToSatNameError; }

    void IncBlobPropsNotFoundError(void)
    { ++m_BlobPropsNotFoundError; }

    void IncLMDBError(void)
    { ++m_LMDBError; }

    void IncCassQueryTimeoutError(void)
    { ++m_CassQueryTimeoutError; }

    void IncInvalidId2InfoError(void)
    { ++m_InvalidId2InfoError; }

    void IncMaxHopsExceededError(void)
    { ++m_MaxHopsExceededError; }

    void PopulateDictionary(CJsonNode &  dict) const;

private:
    atomic_uint_fast64_t        m_BadUrlPath;
    atomic_uint_fast64_t        m_InsufficientArguments;
    atomic_uint_fast64_t        m_MalformedArguments;
    atomic_uint_fast64_t        m_GetBlobNotFound;      // 404
    atomic_uint_fast64_t        m_UnknownError;         // 503
    atomic_uint_fast64_t        m_ClientSatToSatNameError;
    atomic_uint_fast64_t        m_ServerSatToSatNameError;
    atomic_uint_fast64_t        m_BlobPropsNotFoundError;
    atomic_uint_fast64_t        m_LMDBError;
    atomic_uint_fast64_t        m_CassQueryTimeoutError;
    atomic_uint_fast64_t        m_InvalidId2InfoError;
    atomic_uint_fast64_t        m_MaxHopsExceededError;
};



class CPubseqGatewayRequestCounters
{
public:
    CPubseqGatewayRequestCounters() :
        m_Admin(0), m_Resolve(0), m_GetBlobBySeqId(0), m_GetBlobBySatSatKey(0),
        m_TestIO(0), m_GetNA(0), m_GetTSEChunk(0),
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

    void IncTestIO(void)
    { ++m_TestIO; }

    void IncGetNA(void)
    { ++m_GetNA; }

    void IncGetTSEChunk(void)
    { ++m_GetTSEChunk; }

    void IncNotResolved(void)
    { ++m_NotResolved; }

    void PopulateDictionary(CJsonNode &  dict) const;

private:
    atomic_uint_fast64_t        m_Admin;
    atomic_uint_fast64_t        m_Resolve;
    atomic_uint_fast64_t        m_GetBlobBySeqId;
    atomic_uint_fast64_t        m_GetBlobBySatSatKey;
    atomic_uint_fast64_t        m_TestIO;
    atomic_uint_fast64_t        m_GetNA;
    atomic_uint_fast64_t        m_GetTSEChunk;

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


class CPubseqGatewayDBCounters
{
public:
    CPubseqGatewayDBCounters() :
        m_Si2csiNotFound(0), m_Si2csiFoundOne(0),
        m_Si2csiFoundMany(0), m_BioseqInfoNotFound(0),
        m_BioseqInfoFoundOne(0), m_BioseqInfoFoundMany(0),
        m_Si2csiError(0), m_BioseqInfoError(0)
    {}

    void IncSi2csiNotFound(void)
    { ++m_Si2csiNotFound; }

    void IncSi2csiFoundOne(void)
    { ++m_Si2csiFoundOne; }

    void IncSi2csiFoundMany(void)
    { ++m_Si2csiFoundMany; }

    void IncBioseqInfoNotFound(void)
    { ++m_BioseqInfoNotFound; }

    void IncBioseqInfoFoundOne(void)
    { ++m_BioseqInfoFoundOne; }

    void IncBioseqInfoFoundMany(void)
    { ++m_BioseqInfoFoundMany; }

    void IncSi2csiError(void)
    { ++m_Si2csiError; }

    void IncBioseqInfoError(void)
    { ++m_BioseqInfoError; }

    void PopulateDictionary(CJsonNode &  dict) const;

private:
    atomic_uint_fast64_t        m_Si2csiNotFound;
    atomic_uint_fast64_t        m_Si2csiFoundOne;
    atomic_uint_fast64_t        m_Si2csiFoundMany;
    atomic_uint_fast64_t        m_BioseqInfoNotFound;
    atomic_uint_fast64_t        m_BioseqInfoFoundOne;
    atomic_uint_fast64_t        m_BioseqInfoFoundMany;

    atomic_uint_fast64_t        m_Si2csiError;
    atomic_uint_fast64_t        m_BioseqInfoError;
};

#endif
