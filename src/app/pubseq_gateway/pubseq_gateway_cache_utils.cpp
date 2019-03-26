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

#include "pubseq_gateway_cache_utils.hpp"
#include "pubseq_gateway.hpp"
#include "pubseq_gateway_convert_utils.hpp"

USING_NCBI_SCOPE;



ECacheLookupResult  CPSGCache::s_LookupBioseqInfo(
                                    CBioseqInfoRecord &  bioseq_info_record,
                                    string &  bioseq_info_cache_data)
{
    bool                    cache_hit = false;
    CPubseqGatewayCache *   cache = CPubseqGatewayApp::GetInstance()->
                                                            GetLookupCache();

    int     version = bioseq_info_record.GetVersion();
    int     seq_id_type = bioseq_info_record.GetSeqIdType();

    try {
        if (version >= 0) {
            if (seq_id_type >= 0) {
                cache_hit = cache->LookupBioseqInfoByAccessionVersionSeqIdType(
                                            bioseq_info_record.GetAccession(),
                                            version, seq_id_type,
                                            bioseq_info_cache_data);
            } else {
                // The cache and Cassandra DB have incompatible data types so
                // here are temporary value
                cache_hit = cache->LookupBioseqInfoByAccessionVersion(
                                            bioseq_info_record.GetAccession(),
                                            version,  bioseq_info_cache_data,
                                            seq_id_type);
                if (cache_hit) {
                    bioseq_info_record.SetSeqIdType(seq_id_type);
                }
            }
        } else {
            if (seq_id_type >= 0) {
                // The cache and Cassandra DB have incompatible data types so
                // here are temporary value
                cache_hit = cache->LookupBioseqInfoByAccessionVersionSeqIdType(
                                            bioseq_info_record.GetAccession(),
                                            -1, seq_id_type,
                                            bioseq_info_cache_data,
                                            version, seq_id_type);
                if (cache_hit) {
                    bioseq_info_record.SetVersion(version);
                    bioseq_info_record.SetSeqIdType(seq_id_type);
                }
            } else {
                // The cache and Cassandra DB have incompatible data types so
                // here are temporary value
                cache_hit = cache->LookupBioseqInfoByAccession(
                                            bioseq_info_record.GetAccession(),
                                            bioseq_info_cache_data,
                                            version, seq_id_type);
                if (cache_hit) {
                    bioseq_info_record.SetVersion(version);
                    bioseq_info_record.SetSeqIdType(seq_id_type);
                }
            }
        }
    } catch (const exception &  exc) {
        ERR_POST(Critical << "Exception while bioseq info cache lookup: "
                          << exc.what());
        CPubseqGatewayApp::GetInstance()->GetErrorCounters().IncLMDBError();
        return eFailure;
    } catch (...) {
        ERR_POST(Critical << "Unknown exception while bioseq info cache lookup");
        CPubseqGatewayApp::GetInstance()->GetErrorCounters().IncLMDBError();
        return eFailure;
    }

    if (cache_hit) {
        CPubseqGatewayApp::GetInstance()->GetCacheCounters().
                                                IncBioseqInfoCacheHit();
        return eFound;
    }

    CPubseqGatewayApp::GetInstance()->GetCacheCounters().
                                            IncBioseqInfoCacheMiss();
    return eNotFound;
}


ECacheLookupResult  CPSGCache::s_LookupSi2csi(CBioseqInfoRecord &  bioseq_info_record,
                                              string &  csi_cache_data)
{
    bool                    cache_hit = false;
    CPubseqGatewayCache *   cache = CPubseqGatewayApp::GetInstance()->
                                                            GetLookupCache();

    // The Cassandra DB data types are incompatible with the cache API
    // so this is a temporary value to retrieve the seq_id from cache
    int     seq_id_type = bioseq_info_record.GetSeqIdType();

    try {
        if (seq_id_type < 0) {
            cache_hit = cache->LookupCsiBySeqId(
                                    bioseq_info_record.GetAccession(),
                                    seq_id_type, csi_cache_data);
        } else {
            cache_hit = cache->LookupCsiBySeqIdSeqIdType(
                                    bioseq_info_record.GetAccession(),
                                    seq_id_type, csi_cache_data);
        }
    } catch (const exception &  exc) {
        ERR_POST(Critical << "Exception while csi cache lookup: "
                          << exc.what());
        CPubseqGatewayApp::GetInstance()->GetErrorCounters().IncLMDBError();
        return eFailure;
    } catch (...) {
        ERR_POST(Critical << "Unknown exception while csi cache lookup");
        CPubseqGatewayApp::GetInstance()->GetErrorCounters().IncLMDBError();
        return eFailure;
    }

    if (cache_hit) {
        bioseq_info_record.SetSeqIdType(seq_id_type);
        CPubseqGatewayApp::GetInstance()->GetCacheCounters().
                                                IncSi2csiCacheHit();
        return eFound;
    }

    CPubseqGatewayApp::GetInstance()->GetCacheCounters().
                                            IncSi2csiCacheMiss();
    return eNotFound;
}


ECacheLookupResult  CPSGCache::s_LookupBlobProp(int  sat,
                                                int  sat_key,
                                                int64_t &  last_modified,
                                                CBlobRecord &  blob_record)
{
    bool                    cache_hit = false;
    CPubseqGatewayCache *   cache = CPubseqGatewayApp::GetInstance()->
                                                        GetLookupCache();
    string                  blob_prop_cache_data;

    try {
        if (last_modified == INT64_MIN) {
            cache_hit = cache->LookupBlobPropBySatKey(
                                        sat, sat_key, last_modified,
                                        blob_prop_cache_data);
        } else {
            cache_hit = cache->LookupBlobPropBySatKeyLastModified(
                                        sat, sat_key, last_modified,
                                        blob_prop_cache_data);
        }
    } catch (const exception &  exc) {
        ERR_POST(Critical << "Exception while blob prop cache lookup: "
                          << exc.what());
        CPubseqGatewayApp::GetInstance()->GetErrorCounters().IncLMDBError();
        return eFailure;
    } catch (...) {
        ERR_POST(Critical << "Unknown exception while blob prop cache lookup");
        CPubseqGatewayApp::GetInstance()->GetErrorCounters().IncLMDBError();
        return eFailure;
    }

    if (cache_hit) {
        CPubseqGatewayApp::GetInstance()->GetCacheCounters().
                                                IncBlobPropCacheHit();
        ConvertBlobPropProtobufToBlobRecord(sat_key, last_modified,
                                            blob_prop_cache_data, blob_record);
        return eFound;
    }

    CPubseqGatewayApp::GetInstance()->GetCacheCounters().
                                                IncBlobPropCacheMiss();
    return eNotFound;
}

