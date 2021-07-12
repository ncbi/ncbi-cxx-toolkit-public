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

USING_NCBI_SCOPE;

static const string     kValue("value");
static const string     kName("name");
static const string     kDescription("description");


CPSGSCounters::CPSGSCounters()
{
    m_Counters[ePSGS_BadUrlPath] =
        new SCounterInfo(
            "BadUrlPathCount", "Unknown URL counter",
            "Number of times clients requested a path "
            "which is not served by the server",
            true, true, false);
    m_Counters[ePSGS_InsufficientArgs] =
        new SCounterInfo(
            "InsufficientArgumentsCount", "Insufficient arguments counter",
            "Number of times clients did not supply all the requiried arguments",
            true, true, false);
    m_Counters[ePSGS_MalformedArgs] =
        new SCounterInfo(
            "MalformedArgumentsCount", "Malformed arguments counter",
            "Number of times clients supplied malformed arguments",
            true, true, false);
    m_Counters[ePSGS_GetBlobNotFound] =
        new SCounterInfo(
            "GetBlobNotFoundCount", "Blob not found counter",
            "Number of times clients requested a blob which was not found",
            true, true, false);
    m_Counters[ePSGS_UnknownError] =
        new SCounterInfo(
            "UnknownErrorCount", "Unknown error counter",
            "Number of times an unknown error has been detected",
            true, true, false);
    m_Counters[ePSGS_ClientSatToSatNameError] =
        new SCounterInfo(
            "ClientSatToSatNameErrorCount",
            "Client provided sat to sat name mapping error counter",
            "Number of times a client provided sat could not be mapped to a sat name",
            true, true, false);
    m_Counters[ePSGS_ServerSatToSatNameError] =
        new SCounterInfo(
            "ServerSatToSatNameErrorCount",
            "Server data sat to sat name mapping error counter",
            "Number of times a server data sat could not be mapped to a sat name",
            true, true, false);
    m_Counters[ePSGS_BlobPropsNotFoundError] =
        new SCounterInfo(
            "BlobPropsNotFoundErrorCount", "Blob properties not found counter",
            "Number of times blob properties were not found",
            true, true, false);
    m_Counters[ePSGS_LMDBError] =
        new SCounterInfo(
            "LMDBErrorCount", "LMDB cache error count",
            "Number of times an error was detected while searching in the LMDB cache",
            true, true, false);
    m_Counters[ePSGS_CassQueryTimeoutError] =
        new SCounterInfo(
            "CassQueryTimeoutErrorCount", "Cassandra query timeout error counter",
            "Number of times a timeout error was detected while executing a Cassandra query",
            true, true, false);
    m_Counters[ePSGS_InvalidId2InfoError] =
        new SCounterInfo(
            "InvalidId2InfoErrorCount", "Invalid bioseq info ID2 field error counter",
            "Number of times a malformed bioseq info ID2 field was detected",
            true, true, false);
    m_Counters[ePSGS_SplitHistoryNotFoundError] =
        new SCounterInfo(
            "SplitHistoryNotFoundErrorCount", "Split history not found error count",
            "Number of times a split history was not found",
            true, true, false);
    m_Counters[ePSGS_MaxHopsExceededError] =
        new SCounterInfo(
            "MaxHopsExceededErrorCount", "Max hops exceeded error count",
            "Number of times the max number of hops was exceeded",
            true, true, false);
    m_Counters[ePSGS_InputSeqIdNotResolved] =
        new SCounterInfo(
            "InputSeqIdNotResolved", "Seq id not resolved counter",
            "Number of times a client provided seq id could not be resolved",
            true, false, false);
    m_Counters[ePSGS_TSEChunkSplitVersionCacheMatched] =
        new SCounterInfo(
            "TSEChunkSplitVersionCacheMatched",
            "Requested TSE chunk split version matched the cached one counter",
            "Number of times a client requested TSE chunk split version "
            "matched the cached version",
            true, false, false);
    m_Counters[ePSGS_TSEChunkSplitVersionCacheNotMatched] =
        new SCounterInfo(
            "TSEChunkSplitVersionCacheNotMatched",
            "Requested TSE chunk split version did not match the cached one counter",
            "Number of times a client requested TSE chunk split version "
            "did not match the cached version",
            true, false, false);
    m_Counters[ePSGS_AdminRequest] =
        new SCounterInfo(
            "AdminRequestCount", "Administrative requests counter",
            "Number of time a client requested administrative functionality",
            true, false, true);
    m_Counters[ePSGS_ResolveRequest] =
        new SCounterInfo(
            "ResolveRequestCount", "Resolve requests counter",
            "Number of times a client requested resolve functionality",
            true, false, true);
    m_Counters[ePSGS_GetBlobBySeqIdRequest] =
        new SCounterInfo(
            "GetBlobBySeqIdRequestCount", "Blob requests (by seq id) counter",
            "Number of times a client requested a blob by seq id",
            true, false, true);
    m_Counters[ePSGS_GetBlobBySatSatKeyRequest] =
        new SCounterInfo(
            "GetBlobBySatSatKeyRequestCount", "Blob requests (by sat and sat key) counter",
            "Number of times a client requested a blob by sat and sat key",
            true, false, true);
    m_Counters[ePSGS_GetNamedAnnotations] =
        new SCounterInfo(
            "GetNamedAnnotationsCount", "Named annotations requests counter",
            "Number of times a client requested named annotations",
            true, false, true);
    m_Counters[ePSGS_AccessionVersionHistory] =
        new SCounterInfo(
            "AccessionVersionHistoryCount", "Accession version history requests counter",
            "Number of times a client requested accession version history",
            true, false, true);
    m_Counters[ePSGS_TestIORequest] =
        new SCounterInfo(
            "TestIORequestCount", "Test input/output requests counter",
            "Number of times a client requested an input/output test",
            true, false, true);
    m_Counters[ePSGS_GetTSEChunk] =
        new SCounterInfo(
            "GetTSEChunkCount", "TSE chunk requests counter",
            "Number of times a client requested a TSE chunk",
            true, false, true);
    m_Counters[ePSGS_HealthRequest] =
        new SCounterInfo(
            "HealthRequestCount", "Health requests counter",
            "Number of times a client requested health or deep-health status",
            true, false, true);
    m_Counters[ePSGS_Si2csiCacheHit] =
        new SCounterInfo(
            "Si2csiCacheHit", "si2csi cache hit counter",
            "Number of times a si2csi LMDB cache lookup found a record",
            true, false, false);
    m_Counters[ePSGS_Si2csiCacheMiss] =
        new SCounterInfo(
            "Si2csiCacheMiss", "si2csi cache miss counter",
            "Number of times a si2csi LMDB cache lookup did not find a record",
            true, false, false);
    m_Counters[ePSGS_BioseqInfoCacheHit] =
        new SCounterInfo(
            "BioseqInfoCacheHit", "bioseq info cache hit counter",
            "Number of times a bioseq info LMDB cache lookup found a record",
            true, false, false);
    m_Counters[ePSGS_BioseqInfoCacheMiss] =
        new SCounterInfo(
            "BioseqInfoCacheMiss", "bioseq info cache miss counter",
            "Number of times a bioseq info LMDB cache lookup did not find a record",
            true, false, false);
    m_Counters[ePSGS_BlobPropCacheHit] =
        new SCounterInfo(
            "BlobPropCacheHit", "Blob properties cache hit counter",
            "Number of times a blob properties LMDB cache lookup found a record",
            true, false, false);
    m_Counters[ePSGS_BlobPropCacheMiss] =
        new SCounterInfo(
            "BlobPropCacheMiss", "Blob properties cache miss counter",
            "Number of times a blob properties LMDB cache lookup did not find a record",
            true, false, false);
    m_Counters[ePSGS_Si2csiNotFound] =
        new SCounterInfo(
            "Si2csiNotFound", "si2csi not found in Cassandra counter",
            "Number of times a Cassandra si2csi query resulted in no records",
            true, false, false);
    m_Counters[ePSGS_Si2csiFoundOne] =
        new SCounterInfo(
            "Si2csiFoundOne", "si2csi found one record in Cassandra counter",
            "Number of times a Cassandra si2csi query resulted in exactly one record",
            true, false, false);
    m_Counters[ePSGS_Si2csiFoundMany] =
        new SCounterInfo(
            "Si2csiFoundMany", "si2csi found more than one record in Cassandra counter",
            "Number of times a Cassandra si2csi query resulted in more than one record",
            true, false, false);
    m_Counters[ePSGS_BioseqInfoNotFound] =
        new SCounterInfo(
            "BioseqInfoNotFound", "bioseq info not found in Cassandra counter",
            "Number of times a Cassandra bioseq info query resulted in no records",
            true, false, false);
    m_Counters[ePSGS_BioseqInfoFoundOne] =
        new SCounterInfo(
            "BioseqInfoFoundOne", "bioseq info found one record in Cassandra counter",
            "Number of times a Cassandra bioseq info query resulted in exactly one record",
            true, false, false);
    m_Counters[ePSGS_BioseqInfoFoundMany] =
        new SCounterInfo(
            "BioseqInfoFoundMany", "bioseq info found more than one record in Cassandra counter",
            "Number of times a Cassandra bioseq info query resulted in more than one record",
            true, false, false);
    m_Counters[ePSGS_Si2csiError] =
        new SCounterInfo(
            "Si2csiError", "si2csi Cassandra query execution error counter",
            "Number of time a Cassandra si2csi query resulted in an error",
            true, true, false);
    m_Counters[ePSGS_BioseqInfoError] =
        new SCounterInfo(
            "BioseqInfoError", "bioseq info Cassandra query execution error counter",
            "Number of times a Cassandra bioseq info query resulted in an error",
            true, true, false);

    // The counters below are for the sake of an identifier, name and
    // description. The name and description can be overwritten by the
    // configuration values

    m_Counters[ePSGS_TotalRequest] =
        new SCounterInfo(
            "TotalRequestCount", "Total number of requests",
            "Total number of requests",
            false, false, false);
    m_Counters[ePSGS_TotalError] =
        new SCounterInfo(
            "TotalErrorCount", "Total number of errors",
            "Total number of errors",
            false, false, false);
    m_Counters[ePSGS_CassandraActiveStatements] =
        new SCounterInfo(
            "CassandraActiveStatementsCount", "Cassandra active statements counter",
            "Number of the currently active Cassandra queries",
            false, false, false);
    m_Counters[ePSGS_NumberOfConnections] =
        new SCounterInfo(
            "NumberOfConnections", "Cassandra connections counter",
            "Number of the connections to Cassandra",
            false, false, false);
    m_Counters[ePSGS_ActiveRequest] =
        new SCounterInfo(
            "ActiveRequestCount", "Active requests counter",
            "Number of the currently active client requests",
            false, false, false);
    m_Counters[ePSGS_ShutdownRequested] =
        new SCounterInfo(
            "ShutdownRequested", "Shutdown requested flag",
            "Shutdown requested flag",
            false, false, false);
    m_Counters[ePSGS_GracefulShutdownExpiredInSec] =
        new SCounterInfo(
            "GracefulShutdownExpiredInSec", "Graceful shutdown expiration",
            "Graceful shutdown expiration in seconds from now",
            false, false, false);
}


CPSGSCounters::~CPSGSCounters()
{
    for (auto & item: m_Counters) {
        delete item.second;
    }
}


void CPSGSCounters::Increment(EPSGS_CounterType  counter)
{
    auto    it = m_Counters.find(counter);
    if (it == m_Counters.end()) {
        PSG_ERROR("There is no information about the counter with id " +
                  to_string(counter) + ". Nothing was incremented.");
        return;
    }

    ++(it->second->m_Value);
}


void CPSGSCounters::UpdateConfiguredNameDescription(
                            const map<string, tuple<string, string>> &  conf)
{
    for (auto const & conf_item : conf) {
        for (auto & counter: m_Counters) {
            if (counter.second->m_Identifier == conf_item.first) {
                counter.second->m_Name = get<0>(conf_item.second);
                counter.second->m_Description = get<1>(conf_item.second);
                break;
            }
        }
    }
}


void CPSGSCounters::PopulateDictionary(CJsonNode &  dict)
{
    uint64_t    err_sum(0);
    uint64_t    req_sum(0);
    uint64_t    value(0);

    for (auto const & item: m_Counters) {
        if (!item.second->m_IsMonotonicCounter)
            continue;

        value = item.second->m_Value;
        if (item.second->m_IsErrorCounter) {
            err_sum += value;
        } else {
            if (item.second->m_IsRequestCounter) {
                req_sum += value;
            }
        }
        AppendValueNode(dict, item.second->m_Identifier,
                        item.second->m_Name, item.second->m_Description,
                        value);
    }

    AppendValueNode(dict,
                    m_Counters[ePSGS_TotalRequest]->m_Identifier,
                    m_Counters[ePSGS_TotalRequest]->m_Name,
                    m_Counters[ePSGS_TotalRequest]->m_Description,
                    req_sum);
    AppendValueNode(dict,
                    m_Counters[ePSGS_TotalError]->m_Identifier,
                    m_Counters[ePSGS_TotalError]->m_Name,
                    m_Counters[ePSGS_TotalError]->m_Description,
                    err_sum);
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

