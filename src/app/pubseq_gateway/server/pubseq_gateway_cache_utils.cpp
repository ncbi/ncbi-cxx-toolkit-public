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
#include "insdc_utils.hpp"
#include "resolve_trace.hpp"

USING_NCBI_SCOPE;



ECacheLookupResult
CPSGCache::s_LookupBioseqInfo(SBioseqResolution &  bioseq_resolution,
                              bool                 need_trace)
{
    auto                    app = CPubseqGatewayApp::GetInstance();
    CPubseqGatewayCache *   cache = app->GetLookupCache();

    if (cache == nullptr)
        return eNotFound;

    auto    version = bioseq_resolution.m_BioseqInfo.GetVersion();
    auto    seq_id_type = bioseq_resolution.m_BioseqInfo.GetSeqIdType();
    auto    gi = bioseq_resolution.m_BioseqInfo.GetGI();

    CBioseqInfoFetchRequest     fetch_request;
    fetch_request.SetAccession(bioseq_resolution.m_BioseqInfo.GetAccession());
    if (version >= 0)
        fetch_request.SetVersion(version);
    if (seq_id_type >= 0)
        fetch_request.SetSeqIdType(seq_id_type);
    if (gi > 0)
        fetch_request.SetGI(gi);

    auto        start = chrono::high_resolution_clock::now();
    bool        cache_hit = false;

    COperationTiming &      timing = app->GetTiming();
    try {
        if (need_trace)
            ResolveTrace("Cache request: " + fetch_request.ToString());

        auto    results = cache->FetchBioseqInfo(fetch_request);
        switch (results.size()) {
            case 0:
                if (need_trace)
                    ResolveTrace("0 hits");
                if (IsINSDCSeqIdType(seq_id_type)) {
                    timing.Register(eLookupLmdbBioseqInfo, eOpStatusNotFound, start);
                    app->GetCacheCounters().IncBioseqInfoCacheMiss();
                    return CPSGCache::s_LookupINSDCBioseqInfo(bioseq_resolution,
                                                              need_trace);
                }
                cache_hit = false;
                break;
            case 1:
                if (need_trace)
                    ResolveTrace("1 hit");
                cache_hit = true;
                bioseq_resolution.m_BioseqInfo = std::move(results[0]);
                break;
            default:
                // More than one record; may be need to pick the latest version
                if (need_trace)
                    ResolveTrace(to_string(results.size()) + " hits");

                if (version < 0) {
                    auto    ver = results[0].GetVersion();
                    size_t  index_to_pick = 0;
                    for (size_t  k = 0; k < results.size(); ++k) {
                        if (results[k].GetVersion() > ver) {
                            index_to_pick = k;
                            ver = results[k].GetVersion();
                        }
                    }
                    cache_hit = true;
                    bioseq_resolution.m_BioseqInfo = std::move(results[index_to_pick]);

                    if (need_trace)
                        ResolveTrace("Record with max version " +
                                     to_string(ver) + " selected");
                } else {
                    cache_hit = false;

                    if (need_trace)
                        ResolveTrace("Consider as nothing was found");
                }
                break;
        }
    } catch (const exception &  exc) {
        if (need_trace)
            ResolveTrace("Cache fetch exception. Report failure.");
        ERR_POST(Critical << "Exception while bioseq info cache lookup: "
                          << exc.what());
        app->GetErrorCounters().IncLMDBError();
        return eFailure;
    } catch (...) {
        if (need_trace)
            ResolveTrace("Cache fetch exception. Report failure.");
        ERR_POST(Critical << "Unknown exception while bioseq info cache lookup");
        app->GetErrorCounters().IncLMDBError();
        return eFailure;
    }

    if (cache_hit) {
        if (need_trace)
            ResolveTrace("Report cache hit");
        timing.Register(eLookupLmdbBioseqInfo, eOpStatusFound, start);
        app->GetCacheCounters().IncBioseqInfoCacheHit();
        return eFound;
    }

    if (need_trace)
        ResolveTrace("Report cache no hit");
    timing.Register(eLookupLmdbBioseqInfo, eOpStatusNotFound, start);
    app->GetCacheCounters().IncBioseqInfoCacheMiss();
    return eNotFound;
}


ECacheLookupResult
CPSGCache::s_LookupINSDCBioseqInfo(SBioseqResolution &  bioseq_resolution,
                                   bool                 need_trace)
{
    auto                    app = CPubseqGatewayApp::GetInstance();
    CPubseqGatewayCache *   cache = app->GetLookupCache();

    auto    version = bioseq_resolution.m_BioseqInfo.GetVersion();
    auto    gi = bioseq_resolution.m_BioseqInfo.GetGI();

    CBioseqInfoFetchRequest     fetch_request;
    fetch_request.SetAccession(bioseq_resolution.m_BioseqInfo.GetAccession());
    if (version >= 0)
        fetch_request.SetVersion(version);
    if (gi > 0)
        fetch_request.SetGI(gi);

    auto        start = chrono::high_resolution_clock::now();
    bool        cache_hit = false;

    COperationTiming &      timing = app->GetTiming();
    try {
        if (need_trace)
            ResolveTrace("Cache for INSDC types: " + fetch_request.ToString());

        auto    results = cache->FetchBioseqInfo(fetch_request);
        SINSDCDecision  decision = DecideINSDC(results, version);

        if (need_trace)
            ResolveTrace("Cache for INSDC types: " +
                         to_string(results.size()) + " hits; decision status: " +
                         to_string(decision.status));

        switch (decision.status) {
            case CRequestStatus::e200_Ok:
                cache_hit = true;
                bioseq_resolution.m_BioseqInfo = std::move(results[decision.index]);
                break;
            case CRequestStatus::e404_NotFound:
                cache_hit = false;
                break;
            case CRequestStatus::e500_InternalServerError:
                // No suitable records
                cache_hit = false;
                break;
            default:
                // Impossible
                cache_hit = false;
                break;
        }
    } catch (const exception &  exc) {
        if (need_trace)
            ResolveTrace("Cache fetch for INSDC types exception. Report failure.");

        ERR_POST(Critical << "Exception while INSDC bioseq info cache lookup: "
                          << exc.what());
        app->GetErrorCounters().IncLMDBError();
        return eFailure;
    } catch (...) {
        if (need_trace)
            ResolveTrace("Cache fetch for INSDC types exception. Report failure.");

        ERR_POST(Critical << "Unknown exception while INSDC bioseq info cache lookup");
        app->GetErrorCounters().IncLMDBError();
        return eFailure;
    }

    if (cache_hit) {
        if (need_trace)
            ResolveTrace("Report cache for INSDC types hit");
        timing.Register(eLookupLmdbBioseqInfo, eOpStatusFound, start);
        app->GetCacheCounters().IncBioseqInfoCacheHit();
        return eFound;
    }

    if (need_trace)
        ResolveTrace("Report cache for INSDC types no hit");
    timing.Register(eLookupLmdbBioseqInfo, eOpStatusNotFound, start);
    app->GetCacheCounters().IncBioseqInfoCacheMiss();
    return eNotFound;
}


ECacheLookupResult
CPSGCache::s_LookupSi2csi(SBioseqResolution &  bioseq_resolution,
                          bool  need_trace)
{
    auto                    app = CPubseqGatewayApp::GetInstance();
    CPubseqGatewayCache *   cache = app->GetLookupCache();

    if (cache == nullptr)
        return eNotFound;

    auto    seq_id_type = bioseq_resolution.m_BioseqInfo.GetSeqIdType();

    CSi2CsiFetchRequest     fetch_request;
    fetch_request.SetSecSeqId(bioseq_resolution.m_BioseqInfo.GetAccession());
    if (seq_id_type >= 0)
        fetch_request.SetSecSeqIdType(seq_id_type);

    auto    start = chrono::high_resolution_clock::now();
    bool    cache_hit = false;

    try {
        if (need_trace)
            ResolveTrace("Cache request: " + fetch_request.ToString());

        auto    results = cache->FetchSi2Csi(fetch_request);
        switch (results.size()) {
            case 0:
                if (need_trace)
                    ResolveTrace("0 hits");
                cache_hit = false;
                break;
            case 1:
                if (need_trace)
                    ResolveTrace("1 hit");
                cache_hit = true;
                bioseq_resolution.m_BioseqInfo.SetAccession(results[0].GetAccession());
                bioseq_resolution.m_BioseqInfo.SetVersion(results[0].GetVersion());
                bioseq_resolution.m_BioseqInfo.SetSeqIdType(results[0].GetSeqIdType());
                bioseq_resolution.m_BioseqInfo.SetGI(results[0].GetGI());
                break;
            default:
                if (need_trace)
                    ResolveTrace(to_string(results.size()) + " hits. "
                                 "Cannot decide what to choose so treat as no hit");

                // More than one record: there is no basis to choose, so
                // say that there was no cache hit
                cache_hit = false;
                break;
        }
    } catch (const exception &  exc) {
        if (need_trace)
            ResolveTrace("Cache fetch exception. Report failure.");
        ERR_POST(Critical << "Exception while csi cache lookup: "
                          << exc.what());
        app->GetErrorCounters().IncLMDBError();
        return eFailure;
    } catch (...) {
        if (need_trace)
            ResolveTrace("Cache fetch exception. Report failure.");
        ERR_POST(Critical << "Unknown exception while csi cache lookup");
        app->GetErrorCounters().IncLMDBError();
        return eFailure;
    }

    COperationTiming &      timing = app->GetTiming();
    if (cache_hit) {
        if (need_trace)
            ResolveTrace("Report cache hit");
        timing.Register(eLookupLmdbSi2csi, eOpStatusFound, start);
        app->GetCacheCounters().IncSi2csiCacheHit();
        return eFound;
    }

    if (need_trace)
        ResolveTrace("Report cache no hit");
    timing.Register(eLookupLmdbSi2csi, eOpStatusNotFound, start);
    app->GetCacheCounters().IncSi2csiCacheMiss();
    return eNotFound;
}


ECacheLookupResult  CPSGCache::s_LookupBlobProp(int  sat,
                                                int  sat_key,
                                                int64_t &  last_modified,
                                                CBlobRecord &  blob_record)
{
    auto                    app = CPubseqGatewayApp::GetInstance();
    CPubseqGatewayCache *   cache = app->GetLookupCache();

    if (cache == nullptr)
        return eNotFound;

    CBlobFetchRequest       fetch_request;
    fetch_request.SetSat(sat);
    fetch_request.SetSatKey(sat_key);
    if (last_modified != INT64_MIN)
        fetch_request.SetLastModified(last_modified);

    auto    start = chrono::high_resolution_clock::now();
    bool    cache_hit = false;

    try {
        auto    results = cache->FetchBlobProp(fetch_request);
        switch (results.size()) {
            case 0:
                cache_hit = false;
                break;
            case 1:
                cache_hit = true;
                last_modified = results[0].GetModified();
                blob_record = std::move(results[0]);
                break;
            default:
                // More than one record: need to choose by last modified
                cache_hit = true;
                size_t      max_last_modified_index = 0;
                for (size_t  k = 0; k < results.size(); ++k) {
                    if (results[k].GetModified() >
                        results[max_last_modified_index].GetModified())
                        max_last_modified_index = k;
                }
                last_modified = results[max_last_modified_index].GetModified();
                blob_record = std::move(results[max_last_modified_index]);
                break;
        }
    } catch (const exception &  exc) {
        ERR_POST(Critical << "Exception while blob prop cache lookup: "
                          << exc.what());
        app->GetErrorCounters().IncLMDBError();
        return eFailure;
    } catch (...) {
        ERR_POST(Critical << "Unknown exception while blob prop cache lookup");
        app->GetErrorCounters().IncLMDBError();
        return eFailure;
    }

    COperationTiming &      timing = app->GetTiming();
    if (cache_hit) {
        timing.Register(eLookupLmdbBlobProp, eOpStatusFound, start);
        app->GetCacheCounters().IncBlobPropCacheHit();
        return eFound;
    }

    timing.Register(eLookupLmdbBlobProp, eOpStatusNotFound, start);
    app->GetCacheCounters().IncBlobPropCacheMiss();
    return eNotFound;
}

