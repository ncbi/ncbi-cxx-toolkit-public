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

static const string     kValue("value");
static const string     kName("name");
static const string     kDescription("description");

void AppendValueNode(CJsonNode &  dict,
                     const string &  id,
                     uint64_t  value,
                     const string &  name,
                     const string &  description)
{
    CJsonNode   value_dict(CJsonNode::NewObjectNode());
    value_dict.SetInteger(kValue, value);
    value_dict.SetString(kName, name);
    value_dict.SetString(kDescription, description);

    dict.SetByKey(id, value_dict);
}

void AppendValueNode(CJsonNode &  dict,
                     const string &  id,
                     bool  value,
                     const string &  name,
                     const string &  description)
{
    CJsonNode   value_dict(CJsonNode::NewObjectNode());
    value_dict.SetBoolean(kValue, value);
    value_dict.SetString(kName, name);
    value_dict.SetString(kDescription, description);

    dict.SetByKey(id, value_dict);
}

void AppendValueNode(CJsonNode &  dict,
                     const string &  id,
                     const string &  value,
                     const string &  name,
                     const string &  description)
{
    CJsonNode   value_dict(CJsonNode::NewObjectNode());
    value_dict.SetString(kValue, value);
    value_dict.SetString(kName, name);
    value_dict.SetString(kDescription, description);

    dict.SetByKey(id, value_dict);
}


void CPubseqGatewayErrorCounters::PopulateDictionary(CJsonNode &  dict) const
{
    uint64_t    err_sum(0);
    uint64_t    value(0);

    value = m_BadUrlPath;
    err_sum += value;
    AppendValueNode(dict, "BadUrlPathCount", value,
                    "Unknown URL counter",
                    "Number of times clients requested a path "
                    "which is not served by the server");

    value = m_InsufficientArguments;
    err_sum += value;
    AppendValueNode(dict, "InsufficientArgumentsCount", value,
                    "Insufficient arguments counter",
                    "Number of times clients did not supply all the "
                    "requeried arguments");

    value = m_MalformedArguments;
    err_sum += value;
    AppendValueNode(dict, "MalformedArgumentsCount", value,
                    "Malformed arguments counter",
                    "Number of times clients supplied malformed arguments");

    value = m_GetBlobNotFound;
    err_sum += value;
    AppendValueNode(dict, "GetBlobNotFoundCount", value,
                    "Blob not found counter",
                    "Number of times clients requested a blob "
                    "which was not found");

    value = m_UnknownError;
    err_sum += value;
    AppendValueNode(dict, "UnknownErrorCount", value,
                    "Unknown error counter",
                    "Number of times an unknown error has been detected");

    value = m_ClientSatToSatNameError;
    err_sum += value;
    AppendValueNode(dict, "ClientSatToSatNameErrorCount", value,
                    "Client provided sat to sat name mapping error counter",
                    "Number of times a client provided sat "
                    "could not be mapped to a sat name");

    value = m_ServerSatToSatNameError;
    err_sum += value;
    AppendValueNode(dict, "ServerSatToSatNameErrorCount", value,
                    "Server data sat to sat name mapping error counter",
                    "Number of times a server data sat "
                    "could not be mapped to a sat name");

    value = m_BlobPropsNotFoundError;
    err_sum += value;
    AppendValueNode(dict, "BlobPropsNotFoundErrorCount", value,
                    "Blob properties not found counter",
                    "Number of times blob properties were not found");

    value = m_LMDBError;
    err_sum += value;
    AppendValueNode(dict, "LMDBErrorCount", value,
                    "LMDB cache error count",
                    "Number of times an error was detected "
                    "while searching in the LMDB cache");

    value = m_CassQueryTimeoutError;
    err_sum += value;
    AppendValueNode(dict, "CassQueryTimeoutErrorCount", value,
                    "Cassandra query timeout error counter",
                    "Number of times a timeout error was detected "
                    "while executing a Cassandra query");

    value = m_InvalidId2InfoError;
    err_sum += value;
    AppendValueNode(dict, "InvalidId2InfoErrorCount", value,
                    "Invalid bioseq info ID2 field error counter",
                    "Number of times a malformed bioseq info ID2 "
                    "field was detected");

    value = m_SplitHistoryNotFoundError;
    err_sum += value;
    AppendValueNode(dict, "SplitHistoryNotFoundErrorCount", value,
                    "Split history not found error count",
                    "Number of time a split history was not found");

    AppendValueNode(dict, "TotalErrorCount", err_sum,
                    "Total number of errors",
                    "Total number of errors");
}


void CPubseqGatewayRequestCounters::PopulateDictionary(CJsonNode &  dict) const
{
    AppendValueNode(dict, "InputSeqIdNotResolved", m_NotResolved,
                    "Seq id not resolved counter",
                    "Number of times a client provided seq id could not be resolved");
    AppendValueNode(dict, "TSEChunkSplitVersionCacheMatched",
                    m_TSEChunkSplitVersionCacheMatched,
                    "Requested TSE chunk split version matched the cached one counter",
                    "Number of times a client requested TSE chunk split version "
                    "matched the cached version");
    AppendValueNode(dict, "TSEChunkSplitVersionCacheNotMatched",
                    m_TSEChunkSplitVersionCacheNotMatched,
                    "Requested TSE chunk split version did not match the cached one counter",
                    "Number of times a client requested TSE chunk split version "
                    "did not match the cached version");

    uint64_t    req_sum(0);
    uint64_t    value(0);

    value = m_Admin;
    req_sum += value;
    AppendValueNode(dict, "AdminRequestCount", value,
                    "Administrative requests counter",
                    "Number of time a client requested administrative functionality");

    value = m_Resolve;
    req_sum += value;
    AppendValueNode(dict, "ResolveRequestCount", value,
                    "Resolve requests counter",
                    "Number of times a client requested resolve functionality");

    value = m_GetBlobBySeqId;
    req_sum += value;
    AppendValueNode(dict, "GetBlobBySeqIdRequestCount", value,
                    "Blob requests (by seq id) counter",
                    "Number of times a client requested a blob by seq id");

    value = m_GetBlobBySatSatKey;
    req_sum += value;
    AppendValueNode(dict, "GetBlobBySatSatKeyRequestCount", value,
                    "Blob requests (by sat and sat key) counter",
                    "Number of times a client requested a blob by sat and sat key");

    value = m_GetNA;
    req_sum += value;
    AppendValueNode(dict, "GetNamedAnnotationsCount", value,
                    "Named annotations requests counter",
                    "Number of times a client requested named annotations");

    value = m_TestIO;
    req_sum += value;
    AppendValueNode(dict, "TestIORequestCount", value,
                    "Test input/output requests counter",
                    "Number of times a client requested an input/output test");

    value = m_GetTSEChunk;
    req_sum += value;
    AppendValueNode(dict, "GetTSEChunkCount", value,
                    "TSE chunk requests counter",
                    "Number of times a client requested a TSE chunk");

    AppendValueNode(dict, "TotalRequestCount", req_sum,
                    "Total number of requests",
                    "Total number of requests");
}


void CPubseqGatewayCacheCounters::PopulateDictionary(CJsonNode &  dict) const
{
    AppendValueNode(dict, "Si2csiCacheHit", m_Si2csiCacheHit,
                    "si2csi cache hit counter",
                    "Number of times a si2csi LMDB cache lookup found a record");
    AppendValueNode(dict, "Si2csiCacheMiss", m_Si2csiCacheMiss,
                    "si2csi cache miss counter",
                    "Number of times a si2csi LMDB cache lookup did not find a record");
    AppendValueNode(dict, "BioseqInfoCacheHit", m_BioseqInfoCacheHit,
                    "bioseq info cache hit counter",
                    "Number of times a bioseq info LMDB cache lookup found a record");
    AppendValueNode(dict, "BioseqInfoCacheMiss", m_BioseqInfoCacheMiss,
                    "Number of times a bioseq info LMDB cache lookup did not find a record",
                    "bioseq info cache miss counter");
    AppendValueNode(dict, "BlobPropCacheHit", m_BlobPropCacheHit,
                    "Blob properties cache hit counter",
                    "Number of times a blob properties LMDB cache lookup found a record");
    AppendValueNode(dict, "BlobPropCacheMiss", m_BlobPropCacheMiss,
                    "Blob properties cache miss counter",
                    "Number of times a blob properties LMDB cache lookup did not find a record");
}


void CPubseqGatewayDBCounters::PopulateDictionary(CJsonNode &  dict) const
{
    AppendValueNode(dict, "Si2csiNotFound", m_Si2csiNotFound,
                    "si2csi not found in Cassandra counter",
                    "Number of times a Cassandra si2csi query resulted no records");
    AppendValueNode(dict, "Si2csiFoundOne", m_Si2csiFoundOne,
                    "si2csi found one record in Cassandra counter",
                    "Number of times a Cassandra si2csi query resulted exactly one record");
    AppendValueNode(dict, "Si2csiFoundMany", m_Si2csiFoundMany,
                    "si2csi found more than one records in Cassandra counter",
                    "Number of times a Cassandra si2csi query resulted more than one record");
    AppendValueNode(dict, "BioseqInfoNotFound", m_BioseqInfoNotFound,
                    "bioseq info not found in Cassandra counter",
                    "Number of times a Cassandra bioseq info query resulted no records");
    AppendValueNode(dict, "BioseqInfoFoundOne", m_BioseqInfoFoundOne,
                    "bioseq info found one record in Cassandra counter",
                    "Number of times a Cassandra bioseq info query resulted exactly one record");
    AppendValueNode(dict, "BioseqInfoFoundMany", m_BioseqInfoFoundMany,
                    "bioseq info found more than one records in Cassandra counter",
                    "Number of times a Cassandra bioseq info query resulted more than one record");
    AppendValueNode(dict, "Si2csiError", m_Si2csiError,
                    "si2csi Cassandra query execution error counter",
                    "Number of time a Cassandra si2csi query resulted an error");
    AppendValueNode(dict, "BioseqInfoError", m_BioseqInfoError,
                    "bioseq info Cassandra query execution error counter",
                    "Number of times a Cassandra bioseq info query resulted an error");
}

