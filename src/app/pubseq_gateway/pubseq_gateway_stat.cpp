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

    err_sum += m_BadUrlPath;
    dict.SetInteger("BadUrlPathCount", m_BadUrlPath);
    err_sum += m_InsufficientArguments;
    dict.SetInteger("InsufficientArgumentsCount", m_InsufficientArguments);
    err_sum += m_MalformedArguments;
    dict.SetInteger("MalformedArgumentsCount", m_MalformedArguments);
    err_sum += m_ResolveError;
    dict.SetInteger("ResolveErrorCount", m_ResolveError);
    err_sum += m_GetBlobNotFound;
    dict.SetInteger("GetBlobNotFoundCount", m_GetBlobNotFound);
    err_sum += m_GetBlobError;
    dict.SetInteger("GetBlobErrorCount", m_GetBlobError);
    err_sum += m_UnknownError;
    dict.SetInteger("UnknownErrorCount", m_UnknownError);
    err_sum += m_ClientSatToSatNameError;
    dict.SetInteger("ClientSatToSatNameErrorCount", m_ClientSatToSatNameError);
    err_sum += m_ServerSatToSatNameError;
    dict.SetInteger("ServerSatToSatNameErrorCount", m_ServerSatToSatNameError);
    err_sum += m_CanonicalSeqIdError;
    dict.SetInteger("CanonicalSeqIdErrorCount", m_CanonicalSeqIdError);
    err_sum += m_BioseqID2InfoError;
    dict.SetInteger("BioseqID2InfoErrorCount", m_BioseqID2InfoError);
    err_sum += m_BioseqInfoError;
    dict.SetInteger("BioseqInfoErrorCount", m_BioseqInfoError);
    err_sum += m_BlobPropsNotFoundError;
    dict.SetInteger("BlobPropsNotFoundErrorCount", m_BlobPropsNotFoundError);

    dict.SetInteger("TotalErrorCount", err_sum);
}


void CPubseqGatewayRequestCounters::PopulateDictionary(CJsonNode &  dict) const
{
    dict.SetInteger("ResolvedAsPrimaryOSLT", m_ResolvedAsPrimaryOSLT);
    dict.SetInteger("ResolvedAsSecondaryOSLT", m_ResolvedAsSecondaryOSLT);
    dict.SetInteger("ResolvedAsFallback", m_ResolvedAsFallback);
    dict.SetInteger("NotResolved", m_NotResolved);

    uint64_t    req_sum(0);

    req_sum += m_Admin;
    dict.SetInteger("AdminRequestCount", m_Admin);
    req_sum += m_Resolve;
    dict.SetInteger("ResolveRequestCount", m_Resolve);
    req_sum += m_GetBlobBySeqId;
    dict.SetInteger("GetBlobBySeqIdRequestCount", m_GetBlobBySeqId);
    req_sum += m_GetBlobBySatSatKey;
    dict.SetInteger("GetBlobBySatSatKeyRequestCount", m_GetBlobBySatSatKey);

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

