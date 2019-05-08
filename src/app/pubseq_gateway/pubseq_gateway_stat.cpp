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
USING_NCBI_SCOPE;


void CPubseqGatewayErrorCounters::PopulateDictionary(CJsonNode &  dict) const
{
    uint64_t    err_sum(0);
    uint64_t    value(0);

    value = m_BadUrlPath;
    err_sum += value;
    dict.SetInteger("BadUrlPathCount", value);
    value = m_InsufficientArguments;
    err_sum += value;
    dict.SetInteger("InsufficientArgumentsCount", value);
    value = m_MalformedArguments;
    err_sum += value;
    dict.SetInteger("MalformedArgumentsCount", value);
    value = m_GetBlobNotFound;
    err_sum += value;
    dict.SetInteger("GetBlobNotFoundCount", value);
    value = m_UnknownError;
    err_sum += value;
    dict.SetInteger("UnknownErrorCount", value);
    value = m_ClientSatToSatNameError;
    err_sum += value;
    dict.SetInteger("ClientSatToSatNameErrorCount", value);
    value = m_ServerSatToSatNameError;
    err_sum += value;
    dict.SetInteger("ServerSatToSatNameErrorCount", value);
    value = m_BioseqID2InfoError;
    err_sum += value;
    dict.SetInteger("BioseqID2InfoErrorCount", value);
    value = m_BlobPropsNotFoundError;
    err_sum += value;
    dict.SetInteger("BlobPropsNotFoundErrorCount", value);
    value = m_LMDBError;
    err_sum += value;
    dict.SetInteger("LMDBErrorCount", value);
    value = m_CassQueryTimeoutError;
    err_sum += value;
    dict.SetInteger("CassQueryTimeoutErrorCount", value);

    dict.SetInteger("TotalErrorCount", err_sum);
}


void CPubseqGatewayRequestCounters::PopulateDictionary(CJsonNode &  dict) const
{
    dict.SetInteger("InputSeqIdNotResolved", m_NotResolved);

    uint64_t    req_sum(0);
    uint64_t    value(0);

    value = m_Admin;
    req_sum += value;
    dict.SetInteger("AdminRequestCount", value);
    value = m_Resolve;
    req_sum += value;
    dict.SetInteger("ResolveRequestCount", value);
    value = m_GetBlobBySeqId;
    req_sum += value;
    dict.SetInteger("GetBlobBySeqIdRequestCount", value);
    value = m_GetBlobBySatSatKey;
    req_sum += value;
    dict.SetInteger("GetBlobBySatSatKeyRequestCount", value);
    value = m_GetNA;
    req_sum += value;
    dict.SetInteger("GetNamedAnnotationsCount", value);
    value = m_TestIO;
    req_sum += value;
    dict.SetInteger("TestIORequestCount", value);

    dict.SetInteger("TotalRequestCount", req_sum);
}


void CPubseqGatewayCacheCounters::PopulateDictionary(CJsonNode &  dict) const
{
    dict.SetInteger("Si2csiCacheHit", m_Si2csiCacheHit);
    dict.SetInteger("Si2csiCacheMiss", m_Si2csiCacheMiss);
    dict.SetInteger("BioseqInfoCacheHit", m_BioseqInfoCacheHit);
    dict.SetInteger("BioseqInfoCacheMiss", m_BioseqInfoCacheMiss);
    dict.SetInteger("BlobPropCacheHit", m_BlobPropCacheHit);
    dict.SetInteger("BlobPropCacheMiss", m_BlobPropCacheMiss);
}


void CPubseqGatewayDBCounters::PopulateDictionary(CJsonNode &  dict) const
{
    dict.SetInteger("Si2csiNotFound", m_Si2csiNotFound);
    dict.SetInteger("Si2csiFoundOne", m_Si2csiFoundOne);
    dict.SetInteger("Si2csiFoundMany", m_Si2csiFoundMany);
    dict.SetInteger("BioseqInfoNotFound", m_BioseqInfoNotFound);
    dict.SetInteger("BioseqInfoFoundOne", m_BioseqInfoFoundOne);
    dict.SetInteger("BioseqInfoFoundMany", m_BioseqInfoFoundMany);
    dict.SetInteger("Si2csiError", m_Si2csiError);
    dict.SetInteger("BioseqInfoError", m_BioseqInfoError);
}

