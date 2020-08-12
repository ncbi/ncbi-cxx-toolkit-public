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
#include "stat_counters.hpp"

USING_NCBI_SCOPE;

static const string     kValue("value");
static const string     kName("name");
static const string     kDescription("description");

// counter id -> (name, description)
static map<string, tuple<string, string>>   s_IdToNameDescription =
{
    { kCassandraActiveStatementsCount,
        { "Cassandra active statements counter",
          "Number of the currently active Cassandra queries"
        }
    },
    { kNumberOfConnections,
        { "Cassandra connections counter",
          "Number of the connections to Cassandra"
        }
    },
    { kActiveRequestCount,
        { "Active requests counter",
          "Number of the currently active client requests"
        }
    },
    { kShutdownRequested,
        { "Shutdown requested flag",
          "Shutdown requested flag"
        }
    },
    { kGracefulShutdownExpiredInSec,
        { "Graceful shutdown expiration",
          "Graceful shutdown expiration in seconds from now"
        }
    },
    { kBadUrlPathCount,
        { "Unknown URL counter",
          "Number of times clients requested a path "
          "which is not served by the server"
        }
    },
    { kInsufficientArgumentsCount,
        { "Insufficient arguments counter",
          "Number of times clients did not supply all the "
          "requiried arguments"
        }
    },
    { kMalformedArgumentsCount,
        { "Malformed arguments counter",
          "Number of times clients supplied malformed arguments"
        }
    },
    { kGetBlobNotFoundCount,
        { "Blob not found counter",
          "Number of times clients requested a blob which was not found"
        }
    },
    { kUnknownErrorCount,
        { "Unknown error counter",
          "Number of times an unknown error has been detected"
        }
    },
    { kClientSatToSatNameErrorCount,
        { "Client provided sat to sat name mapping error counter",
          "Number of times a client provided sat "
          "could not be mapped to a sat name"
        }
    },
    { kServerSatToSatNameErrorCount,
        { "Server data sat to sat name mapping error counter",
          "Number of times a server data sat "
          "could not be mapped to a sat name"
        }
    },
    { kBlobPropsNotFoundErrorCount,
        { "Blob properties not found counter",
          "Number of times blob properties were not found"
        }
    },
    { kLMDBErrorCount,
        { "LMDB cache error count",
          "Number of times an error was detected "
          "while searching in the LMDB cache"
        }
    },
    { kCassQueryTimeoutErrorCount,
        { "Cassandra query timeout error counter",
          "Number of times a timeout error was detected "
          "while executing a Cassandra query"
        }
    },
    { kInvalidId2InfoErrorCount,
        { "Invalid bioseq info ID2 field error counter",
          "Number of times a malformed bioseq info ID2 field was detected"
        }
    },
    { kMaxHopsExceededErrorCount,
        { "Max hops exceeded error count",
          "Number of times the max number of hops was exceeded"
        }
    },
    { kTotalErrorCount,
        { "Total number of errors",
          "Total number of errors"
        }
    },
    { kInputSeqIdNotResolved,
        { "Seq id not resolved counter",
          "Number of times a client provided seq id could not be resolved"
        }
    },
    { kAdminRequestCount,
        { "Administrative requests counter",
          "Number of time a client requested administrative functionality"
        }
    },
    { kResolveRequestCount,
        { "Resolve requests counter",
          "Number of times a client requested resolve functionality"
        }
    },
    { kGetBlobBySeqIdRequestCount,
        { "Blob requests (by seq id) counter",
          "Number of times a client requested a blob by seq id"
        }
    },
    { kGetBlobBySatSatKeyRequestCount,
        { "Blob requests (by sat and sat key) counter",
          "Number of times a client requested a blob by sat and sat key"
        }
    },
    { kGetNamedAnnotationsCount,
        { "Named annotations requests counter",
          "Number of times a client requested named annotations"
        }
    },
    { kTestIORequestCount,
        { "Test input/output requests counter",
          "Number of times a client requested an input/output test"
        }
    },
    { kGetTSEChunkCount,
        { "TSE chunk requests counter",
          "Number of times a client requested a TSE chunk"
        }
    },
    { kTotalRequestCount,
        { "Total number of requests",
          "Total number of requests"
        }
    },
    { kSi2csiCacheHit,
        { "si2csi cache hit counter",
          "Number of times a si2csi LMDB cache lookup found a record"
        }
    },
    { kSi2csiCacheMiss,
        { "si2csi cache miss counter",
          "Number of times a si2csi LMDB cache lookup did not find a record"
        }
    },
    { kBioseqInfoCacheHit,
        { "bioseq info cache hit counter",
          "Number of times a bioseq info LMDB cache lookup found a record"
        }
    },
    { kBioseqInfoCacheMiss,
        { "bioseq info cache miss counter",
          "Number of times a bioseq info LMDB cache lookup did not find a record"
        }
    },
    { kBlobPropCacheHit,
        { "Blob properties cache hit counter",
          "Number of times a blob properties LMDB cache lookup found a record"
        }
    },
    { kBlobPropCacheMiss,
        { "Blob properties cache miss counter",
          "Number of times a blob properties LMDB cache lookup did not find a record"
        }
    },
    { kSi2csiNotFound,
        { "si2csi not found in Cassandra counter",
          "Number of times a Cassandra si2csi query resulted in no records"
        }
    },
    { kSi2csiFoundOne,
        { "si2csi found one record in Cassandra counter",
          "Number of times a Cassandra si2csi query resulted in exactly one record"
        }
    },
    { kSi2csiFoundMany,
        { "si2csi found more than one record in Cassandra counter",
          "Number of times a Cassandra si2csi query resulted in more than one record"
        }
    },
    { kBioseqInfoNotFound,
        { "bioseq info not found in Cassandra counter",
          "Number of times a Cassandra bioseq info query resulted in no records"
        }
    },
    { kBioseqInfoFoundOne,
        { "bioseq info found one record in Cassandra counter",
          "Number of times a Cassandra bioseq info query resulted in exactly one record"
        }
    },
    { kBioseqInfoFoundMany,
        { "bioseq info found more than one record in Cassandra counter",
          "Number of times a Cassandra bioseq info query resulted in more than one record"
        }
    },
    { kSi2csiError,
        { "si2csi Cassandra query execution error counter",
          "Number of time a Cassandra si2csi query resulted in an error"
        }
    },
    { kBioseqInfoError,
        { "bioseq info Cassandra query execution error counter",
          "Number of times a Cassandra bioseq info query resulted in an error"
        }
    }
};


// The configuration may overwrite the default values
void UpdateIdToNameDescription(const map<string, tuple<string, string>> &  conf)
{
    for (const auto &  conf_item : conf) {
        // Note: there is no checking if the value is present in
        // the map. This lets to have the configured value to be used
        // even if the code does not have a default value set
        s_IdToNameDescription[conf_item.first] = conf_item.second;
    }
}


void AppendValueNode(CJsonNode &  dict,
                     const string &  id,
                     uint64_t  value)
{
    CJsonNode   value_dict(CJsonNode::NewObjectNode());
    const auto  iter = s_IdToNameDescription.find(id);

    value_dict.SetInteger(kValue, value);
    if (iter != s_IdToNameDescription.end()) {
        value_dict.SetString(kName, get<0>(iter->second));
        value_dict.SetString(kDescription, get<1>(iter->second));
    } else {
        value_dict.SetString(kName, kEmptyStr);
        value_dict.SetString(kDescription, kEmptyStr);
    }

    dict.SetByKey(id, value_dict);
}

void AppendValueNode(CJsonNode &  dict,
                     const string &  id,
                     bool  value)
{
    CJsonNode   value_dict(CJsonNode::NewObjectNode());
    const auto  iter = s_IdToNameDescription.find(id);

    value_dict.SetBoolean(kValue, value);
    if (iter != s_IdToNameDescription.end()) {
        value_dict.SetString(kName, get<0>(iter->second));
        value_dict.SetString(kDescription, get<1>(iter->second));
    } else {
        value_dict.SetString(kName, kEmptyStr);
        value_dict.SetString(kDescription, kEmptyStr);
    }

    dict.SetByKey(id, value_dict);
}

void AppendValueNode(CJsonNode &  dict,
                     const string &  id,
                     const string &  value)
{
    CJsonNode   value_dict(CJsonNode::NewObjectNode());
    const auto  iter = s_IdToNameDescription.find(id);

    value_dict.SetString(kValue, value);
    if (iter != s_IdToNameDescription.end()) {
        value_dict.SetString(kName, get<0>(iter->second));
        value_dict.SetString(kDescription, get<1>(iter->second));
    } else {
        value_dict.SetString(kName, kEmptyStr);
        value_dict.SetString(kDescription, kEmptyStr);
    }

    dict.SetByKey(id, value_dict);
}


void CPubseqGatewayErrorCounters::PopulateDictionary(CJsonNode &  dict) const
{
    uint64_t    err_sum(0);
    uint64_t    value(0);

    value = m_BadUrlPath;
    err_sum += value;
    AppendValueNode(dict, kBadUrlPathCount, value);

    value = m_InsufficientArguments;
    err_sum += value;
    AppendValueNode(dict, kInsufficientArgumentsCount, value);

    value = m_MalformedArguments;
    err_sum += value;
    AppendValueNode(dict, kMalformedArgumentsCount, value);

    value = m_GetBlobNotFound;
    err_sum += value;
    AppendValueNode(dict, kGetBlobNotFoundCount, value);

    value = m_UnknownError;
    err_sum += value;
    AppendValueNode(dict, kUnknownErrorCount, value);

    value = m_ClientSatToSatNameError;
    err_sum += value;
    AppendValueNode(dict, kClientSatToSatNameErrorCount, value);

    value = m_ServerSatToSatNameError;
    err_sum += value;
    AppendValueNode(dict, kServerSatToSatNameErrorCount, value);

    value = m_BlobPropsNotFoundError;
    err_sum += value;
    AppendValueNode(dict, kBlobPropsNotFoundErrorCount, value);

    value = m_LMDBError;
    err_sum += value;
    AppendValueNode(dict, kLMDBErrorCount, value);

    value = m_CassQueryTimeoutError;
    err_sum += value;
    AppendValueNode(dict, kCassQueryTimeoutErrorCount, value);

    value = m_InvalidId2InfoError;
    err_sum += value;
    AppendValueNode(dict, kInvalidId2InfoErrorCount, value);

    value = m_MaxHopsExceededError;
    err_sum += value;
    AppendValueNode(dict, kMaxHopsExceededErrorCount, value);

    AppendValueNode(dict, kTotalErrorCount, err_sum);
}


void CPubseqGatewayRequestCounters::PopulateDictionary(CJsonNode &  dict) const
{
    AppendValueNode(dict, kInputSeqIdNotResolved, m_NotResolved);

    uint64_t    req_sum(0);
    uint64_t    value(0);

    value = m_Admin;
    req_sum += value;
    AppendValueNode(dict, kAdminRequestCount, value);

    value = m_Resolve;
    req_sum += value;
    AppendValueNode(dict, kResolveRequestCount, value);

    value = m_GetBlobBySeqId;
    req_sum += value;
    AppendValueNode(dict, kGetBlobBySeqIdRequestCount, value);

    value = m_GetBlobBySatSatKey;
    req_sum += value;
    AppendValueNode(dict, kGetBlobBySatSatKeyRequestCount, value);

    value = m_GetNA;
    req_sum += value;
    AppendValueNode(dict, kGetNamedAnnotationsCount, value);

    value = m_TestIO;
    req_sum += value;
    AppendValueNode(dict, kTestIORequestCount, value);

    value = m_GetTSEChunk;
    req_sum += value;
    AppendValueNode(dict, kGetTSEChunkCount, value);

    AppendValueNode(dict, kTotalRequestCount, req_sum);
}


void CPubseqGatewayCacheCounters::PopulateDictionary(CJsonNode &  dict) const
{
    AppendValueNode(dict, kSi2csiCacheHit, m_Si2csiCacheHit);
    AppendValueNode(dict, kSi2csiCacheMiss, m_Si2csiCacheMiss);
    AppendValueNode(dict, kBioseqInfoCacheHit, m_BioseqInfoCacheHit);
    AppendValueNode(dict, kBioseqInfoCacheMiss, m_BioseqInfoCacheMiss);
    AppendValueNode(dict, kBlobPropCacheHit, m_BlobPropCacheHit);
    AppendValueNode(dict, kBlobPropCacheMiss, m_BlobPropCacheMiss);
}


void CPubseqGatewayDBCounters::PopulateDictionary(CJsonNode &  dict) const
{
    AppendValueNode(dict, kSi2csiNotFound, m_Si2csiNotFound);
    AppendValueNode(dict, kSi2csiFoundOne, m_Si2csiFoundOne);
    AppendValueNode(dict, kSi2csiFoundMany, m_Si2csiFoundMany);
    AppendValueNode(dict, kBioseqInfoNotFound, m_BioseqInfoNotFound);
    AppendValueNode(dict, kBioseqInfoFoundOne, m_BioseqInfoFoundOne);
    AppendValueNode(dict, kBioseqInfoFoundMany, m_BioseqInfoFoundMany);
    AppendValueNode(dict, kSi2csiError, m_Si2csiError);
    AppendValueNode(dict, kBioseqInfoError, m_BioseqInfoError);
}

