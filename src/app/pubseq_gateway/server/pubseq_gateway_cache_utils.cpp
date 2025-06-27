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
#include "pending_operation.hpp"
#include "bioseq_info_record_selector.hpp"
#include "psgs_seq_id_utils.hpp"

USING_NCBI_SCOPE;



EPSGS_CacheLookupResult
CPSGCache::x_LookupBioseqInfo(IPSGS_Processor *  processor,
                              SBioseqResolution &  bioseq_resolution)
{
    auto                    app = CPubseqGatewayApp::GetInstance();
    CPubseqGatewayCache *   cache = app->GetLookupCache();

    if (cache == nullptr)
        return ePSGS_CacheNotHit;

    auto    version = bioseq_resolution.GetBioseqInfo().GetVersion();
    auto    seq_id_type = bioseq_resolution.GetBioseqInfo().GetSeqIdType();
    auto    gi = bioseq_resolution.GetBioseqInfo().GetGI();
    string  seq_id = StripTrailingVerticalBars(bioseq_resolution.GetBioseqInfo().GetAccession());

    CBioseqInfoFetchRequest     fetch_request;
    fetch_request.SetAccession(seq_id);
    if (version >= 0)
        fetch_request.SetVersion(version);
    if (seq_id_type >= 0)
        fetch_request.SetSeqIdType(seq_id_type);
    if (gi > 0)
        fetch_request.SetGI(gi);

    auto        start = psg_clock_t::now();
    bool        cache_hit = false;

    COperationTiming &      timing = app->GetTiming();
    try {
        if (m_NeedTrace) {
            m_Reply->SendTrace(
                "Cache request: " +
                ToJsonString(fetch_request),
                m_Request->GetStartTimestamp());
        }

        auto    records = cache->FetchBioseqInfo(fetch_request);

        if (m_NeedTrace) {
            string  msg = to_string(records.size()) + " hit(s)";
            for (const auto &  item : records) {
                msg += "\n" +
                       ToJsonString(item,
                                    SPSGS_ResolveRequest::fPSGS_AllBioseqFields);
            }
            m_Reply->SendTrace(msg, m_Request->GetStartTimestamp());
        }

        switch (records.size()) {
            case 0:
                if (IsINSDCSeqIdType(seq_id_type)) {
                    timing.Register(processor, eLookupLmdbBioseqInfo, eOpStatusNotFound, start);
                    app->GetCounters().Increment(
                            processor,
                            CPSGSCounters::ePSGS_BioseqInfoCacheMiss);
                    return CPSGCache::x_LookupINSDCBioseqInfo(processor, bioseq_resolution);
                }
                cache_hit = false;
                break;
            case 1:
                cache_hit = true;
                bioseq_resolution.SetBioseqInfo(records[0]);
                break;
            default:
                // More than one record; may be need to pick the latest version
                ssize_t     index_to_pick = SelectBioseqInfoRecord(records);
                if (index_to_pick < 0) {
                    // Many found and could not select one => treat as not
                    // found
                    if (m_NeedTrace) {
                        m_Reply->SendTrace(
                            to_string(records.size()) + " bioseq info records "
                            "were found in cache however it was impossible "
                            "to choose one of them. So treat it as not found",
                            m_Request->GetStartTimestamp());
                    }
                    cache_hit = false;
                    break;
                }

                if (m_NeedTrace) {
                    string      prefix;
                    if (records.size() == 1)
                        prefix = "Selected record:\n";
                    else
                        prefix = "Record with max date changed selected\n";
                    m_Reply->SendTrace(
                        prefix +
                        ToJsonString(records[index_to_pick],
                                     SPSGS_ResolveRequest::fPSGS_AllBioseqFields),
                        m_Request->GetStartTimestamp());
                }

                cache_hit = true;
                bioseq_resolution.SetBioseqInfo(records[index_to_pick]);

                break;
        }
    } catch (const exception &  exc) {
        if (m_NeedTrace)
            m_Reply->SendTrace("Cache fetch exception. Report failure.",
                               m_Request->GetStartTimestamp());
        ERR_POST(Critical << "Exception while bioseq info cache lookup: "
                          << exc.what());
        app->GetCounters().Increment(processor, CPSGSCounters::ePSGS_LMDBError);
        return ePSGS_CacheFailure;
    } catch (...) {
        if (m_NeedTrace)
            m_Reply->SendTrace("Cache fetch exception. Report failure.",
                               m_Request->GetStartTimestamp());
        ERR_POST(Critical << "Unknown exception while bioseq info cache lookup");
        app->GetCounters().Increment(processor, CPSGSCounters::ePSGS_LMDBError);
        return ePSGS_CacheFailure;
    }

    if (cache_hit) {
        if (m_NeedTrace)
            m_Reply->SendTrace("Report cache hit",
                               m_Request->GetStartTimestamp());
        timing.Register(processor, eLookupLmdbBioseqInfo, eOpStatusFound, start);
        app->GetCounters().Increment(processor, CPSGSCounters::ePSGS_BioseqInfoCacheHit);
        return ePSGS_CacheHit;
    }

    if (m_NeedTrace)
        m_Reply->SendTrace("Report cache no hit",
                           m_Request->GetStartTimestamp());
    timing.Register(processor, eLookupLmdbBioseqInfo, eOpStatusNotFound, start);
    app->GetCounters().Increment(processor, CPSGSCounters::ePSGS_BioseqInfoCacheMiss);
    return ePSGS_CacheNotHit;
}


EPSGS_CacheLookupResult
CPSGCache::x_LookupINSDCBioseqInfo(IPSGS_Processor *  processor,
                                   SBioseqResolution &  bioseq_resolution)
{
    auto                    app = CPubseqGatewayApp::GetInstance();
    CPubseqGatewayCache *   cache = app->GetLookupCache();

    auto    version = bioseq_resolution.GetBioseqInfo().GetVersion();
    auto    gi = bioseq_resolution.GetBioseqInfo().GetGI();
    string  seq_id = StripTrailingVerticalBars(bioseq_resolution.GetBioseqInfo().GetAccession());

    CBioseqInfoFetchRequest     fetch_request;
    fetch_request.SetAccession(seq_id);
    if (version >= 0)
        fetch_request.SetVersion(version);
    if (gi > 0)
        fetch_request.SetGI(gi);

    auto        start = psg_clock_t::now();
    bool        cache_hit = false;

    COperationTiming &      timing = app->GetTiming();
    try {
        if (m_NeedTrace) {
            m_Reply->SendTrace(
                    "Cache request for INSDC types: " +
                    ToJsonString(fetch_request),
                    m_Request->GetStartTimestamp());
        }

        auto    records = cache->FetchBioseqInfo(fetch_request);
        SINSDCDecision  decision = DecideINSDC(records, version);

        if (m_NeedTrace) {
            string  msg = to_string(records.size()) +
                          " hit(s); decision status: " + to_string(decision.status);
            for (const auto &  item : records) {
                msg += "\n" +
                       ToJsonString(item,
                                    SPSGS_ResolveRequest::fPSGS_AllBioseqFields);
            }
            m_Reply->SendTrace(msg, m_Request->GetStartTimestamp());
        }

        switch (decision.status) {
            case CRequestStatus::e200_Ok:
                cache_hit = true;
                bioseq_resolution.SetBioseqInfo(records[decision.index]);
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
        if (m_NeedTrace)
            m_Reply->SendTrace("Cache fetch for INSDC types exception. "
                               "Report failure.",
                               m_Request->GetStartTimestamp());

        ERR_POST(Critical << "Exception while INSDC bioseq info cache lookup: "
                          << exc.what());
        app->GetCounters().Increment(processor, CPSGSCounters::ePSGS_LMDBError);
        return ePSGS_CacheFailure;
    } catch (...) {
        if (m_NeedTrace)
            m_Reply->SendTrace("Cache fetch for INSDC types exception. "
                               "Report failure.",
                               m_Request->GetStartTimestamp());

        ERR_POST(Critical << "Unknown exception while INSDC bioseq info cache lookup");
        app->GetCounters().Increment(processor, CPSGSCounters::ePSGS_LMDBError);
        return ePSGS_CacheFailure;
    }

    if (cache_hit) {
        if (m_NeedTrace)
            m_Reply->SendTrace("Report cache for INSDC types hit",
                               m_Request->GetStartTimestamp());
        timing.Register(processor, eLookupLmdbBioseqInfo, eOpStatusFound, start);
        app->GetCounters().Increment(processor, CPSGSCounters::ePSGS_BioseqInfoCacheHit);
        return ePSGS_CacheHit;
    }

    if (m_NeedTrace)
        m_Reply->SendTrace("Report cache for INSDC types no hit",
                           m_Request->GetStartTimestamp());
    timing.Register(processor, eLookupLmdbBioseqInfo, eOpStatusNotFound, start);
    app->GetCounters().Increment(processor, CPSGSCounters::ePSGS_BioseqInfoCacheMiss);
    return ePSGS_CacheNotHit;
}


EPSGS_CacheLookupResult
CPSGCache::x_LookupSi2csi(IPSGS_Processor *  processor,
                          SBioseqResolution &  bioseq_resolution)
{
    auto                    app = CPubseqGatewayApp::GetInstance();
    CPubseqGatewayCache *   cache = app->GetLookupCache();

    if (cache == nullptr)
        return ePSGS_CacheNotHit;

    string  seq_id = StripTrailingVerticalBars(bioseq_resolution.GetBioseqInfo().GetAccession());
    auto    seq_id_type = bioseq_resolution.GetBioseqInfo().GetSeqIdType();

    CSi2CsiFetchRequest     fetch_request;
    fetch_request.SetSecSeqId(seq_id);
    if (seq_id_type >= 0)
        fetch_request.SetSecSeqIdType(seq_id_type);

    auto    start = psg_clock_t::now();
    bool    cache_hit = false;

    try {
        if (m_NeedTrace) {
            m_Reply->SendTrace(
                "Cache request: " +
                ToJsonString(fetch_request),
                m_Request->GetStartTimestamp());
        }

        auto    records = cache->FetchSi2Csi(fetch_request);

        if (m_NeedTrace) {
            string  msg = to_string(records.size()) + " hit(s)";
            for (const auto &  item : records) {
                msg += "\n" + ToJsonString(item);
            }
            m_Reply->SendTrace(msg, m_Request->GetStartTimestamp());
        }

        switch (records.size()) {
            case 0:
                cache_hit = false;
                break;
            case 1:
                cache_hit = true;
                {
                    CBioseqInfoRecord   bioseq_info;
                    bioseq_info.SetAccession(records[0].GetAccession());
                    bioseq_info.SetVersion(records[0].GetVersion());
                    bioseq_info.SetSeqIdType(records[0].GetSeqIdType());
                    bioseq_info.SetGI(records[0].GetGI());

                    bioseq_resolution.SetBioseqInfo(bioseq_info);
                }
                break;
            default:
                if (m_NeedTrace) {
                    m_Reply->SendTrace(
                        to_string(records.size()) + " hits. "
                        "Cannot decide what to choose so treat as no hit",
                        m_Request->GetStartTimestamp());
                }

                // More than one record: there is no basis to choose, so
                // say that there was no cache hit
                cache_hit = false;
                break;
        }
    } catch (const exception &  exc) {
        if (m_NeedTrace)
            m_Reply->SendTrace("Cache fetch exception. Report failure.",
                               m_Request->GetStartTimestamp());
        ERR_POST(Critical << "Exception while csi cache lookup: "
                          << exc.what());
        app->GetCounters().Increment(processor, CPSGSCounters::ePSGS_LMDBError);
        return ePSGS_CacheFailure;
    } catch (...) {
        if (m_NeedTrace)
            m_Reply->SendTrace("Cache fetch exception. Report failure.",
                               m_Request->GetStartTimestamp());
        ERR_POST(Critical << "Unknown exception while csi cache lookup");
        app->GetCounters().Increment(processor, CPSGSCounters::ePSGS_LMDBError);
        return ePSGS_CacheFailure;
    }

    COperationTiming &      timing = app->GetTiming();
    if (cache_hit) {
        if (m_NeedTrace)
            m_Reply->SendTrace("Report cache hit",
                               m_Request->GetStartTimestamp());
        timing.Register(processor, eLookupLmdbSi2csi, eOpStatusFound, start);
        app->GetCounters().Increment(processor, CPSGSCounters::ePSGS_Si2csiCacheHit);
        return ePSGS_CacheHit;
    }

    if (m_NeedTrace)
        m_Reply->SendTrace("Report cache no hit",
                           m_Request->GetStartTimestamp());
    timing.Register(processor, eLookupLmdbSi2csi, eOpStatusNotFound, start);
    app->GetCounters().Increment(processor, CPSGSCounters::ePSGS_Si2csiCacheMiss);
    return ePSGS_CacheNotHit;
}


EPSGS_CacheLookupResult  CPSGCache::x_LookupBlobProp(
                                            IPSGS_Processor *  processor,
                                            int  sat,
                                            int  sat_key,
                                            int64_t &  last_modified,
                                            CBlobRecord &  blob_record)
{
    auto                    app = CPubseqGatewayApp::GetInstance();
    CPubseqGatewayCache *   cache = app->GetLookupCache();

    if (cache == nullptr)
        return ePSGS_CacheNotHit;

    CBlobFetchRequest       fetch_request;
    fetch_request.SetSat(sat);
    fetch_request.SetSatKey(sat_key);
    if (last_modified != INT64_MIN)
        fetch_request.SetLastModified(last_modified);

    auto    start = psg_clock_t::now();
    bool    cache_hit = false;

    try {
        if (m_NeedTrace) {
            m_Reply->SendTrace(
                "Cache request: " +
                ToJsonString(fetch_request),
                m_Request->GetStartTimestamp());
        }

        auto    records = cache->FetchBlobProp(fetch_request);

        if (m_NeedTrace) {
            string  msg = to_string(records.size()) + " hit(s)";
            for (const auto &  item : records) {
                msg += "\n" + ToJsonString(item);
            }
            m_Reply->SendTrace(msg, m_Request->GetStartTimestamp());
        }

        switch (records.size()) {
            case 0:
                cache_hit = false;
                break;
            case 1:
                cache_hit = true;
                last_modified = records[0].GetModified();
                blob_record = std::move(records[0]);
                break;
            default:
                // More than one record: need to choose by last modified
                cache_hit = true;
                size_t      max_last_modified_index = 0;
                for (size_t  k = 0; k < records.size(); ++k) {
                    if (records[k].GetModified() >
                        records[max_last_modified_index].GetModified())
                        max_last_modified_index = k;
                }
                if (m_NeedTrace) {
                    m_Reply->SendTrace(
                        "Record with max last_modified selected\n" +
                        ToJsonString(records[max_last_modified_index]),
                        m_Request->GetStartTimestamp());
                }

                last_modified = records[max_last_modified_index].GetModified();
                blob_record = std::move(records[max_last_modified_index]);

                break;
        }
    } catch (const exception &  exc) {
        if (m_NeedTrace)
            m_Reply->SendTrace("Cache fetch exception. Report failure.",
                               m_Request->GetStartTimestamp());
        ERR_POST(Critical << "Exception while blob prop cache lookup: "
                          << exc.what());
        app->GetCounters().Increment(processor, CPSGSCounters::ePSGS_LMDBError);
        return ePSGS_CacheFailure;
    } catch (...) {
        if (m_NeedTrace)
            m_Reply->SendTrace("Cache fetch exception. Report failure.",
                               m_Request->GetStartTimestamp());
        ERR_POST(Critical << "Unknown exception while blob prop cache lookup");
        app->GetCounters().Increment(processor, CPSGSCounters::ePSGS_LMDBError);
        return ePSGS_CacheFailure;
    }

    COperationTiming &      timing = app->GetTiming();
    if (cache_hit) {
        if (m_NeedTrace)
            m_Reply->SendTrace("Report cache hit",
                               m_Request->GetStartTimestamp());
        timing.Register(processor, eLookupLmdbBlobProp, eOpStatusFound, start);
        app->GetCounters().Increment(processor, CPSGSCounters::ePSGS_BlobPropCacheHit);
        return ePSGS_CacheHit;
    }

    if (m_NeedTrace)
        m_Reply->SendTrace("Report cache no hit",
                           m_Request->GetStartTimestamp());
    timing.Register(processor, eLookupLmdbBlobProp, eOpStatusNotFound, start);
    app->GetCounters().Increment(processor, CPSGSCounters::ePSGS_BlobPropCacheMiss);
    return ePSGS_CacheNotHit;
}

