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
 * Authors:  Sergey Satskiy
 *
 * File Description: PSG server alerts
 *
 */
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include "timing.hpp"
#include "pubseq_gateway_utils.hpp"

static string           kTimeRangeStart("TimeRangeStart");
static string           kTimeRangeEnd("TimeRangeEnd");

const unsigned long     kMaxBlobSize = 1024L*1024L*1024L*8L;   // 8 GB


CJsonNode SerializeHistogram(const TOnePSGTiming &  histogram)
{
    static string   kBins("Bins");
    static string   kStart("Start");
    static string   kEnd("End");
    static string   kCount("Count");
    static string   kLowerAnomaly("LowerAnomaly");
    static string   kUpperAnomaly("UpperAnomaly");
    static string   kTotalCount("TotalCount");
    static string   kValueSum("ValueSum");

    CJsonNode       ret(CJsonNode::NewObjectNode());
    CJsonNode       bins(CJsonNode::NewArrayNode());

    size_t          bin_count =  histogram.GetNumberOfBins();
    size_t          last_bin_index = bin_count - 1;
    auto            starts = histogram.GetBinStarts();
    auto            counters = histogram.GetBinCounters();
    for (size_t  k = 0; k < bin_count; ++k) {
        CJsonNode   bin(CJsonNode::NewObjectNode());
        bin.SetInteger(kStart, starts[k]);
        if (k >= last_bin_index)
            bin.SetInteger(kEnd, histogram.GetMax());
        else
            bin.SetInteger(kEnd, starts[k+1] - 1);
        bin.SetInteger(kCount, counters[k]);

        bins.Append(bin);
    }
    ret.SetByKey(kBins, bins);

    // GetCount() does not include anomalies!
    auto    lower_anomalies = histogram.GetLowerAnomalyCount();
    auto    upper_anomalies = histogram.GetUpperAnomalyCount();
    ret.SetInteger(kLowerAnomaly, lower_anomalies);
    ret.SetInteger(kUpperAnomaly, upper_anomalies);
    ret.SetInteger(kTotalCount, histogram.GetCount() +
                                lower_anomalies + upper_anomalies);
    ret.SetInteger(kValueSum, histogram.GetSum());
    return ret;
}



CJsonNode CPSGTimingBase::SerializeSeries(int  most_ancient_time,
                                          int  most_recent_time,
                                          unsigned long  tick_span) const
{
    CJsonNode                   ret(CJsonNode::NewArrayNode());
    TPSGTiming::TTimeBins       bins = m_PSGTiming->GetHistograms();

    int64_t         histogram_start_time = 0;

    for (const auto &  bin : bins) {
        int64_t     histogram_cover = tick_span * bin.n_ticks;
        int64_t     histogram_end_time = histogram_start_time + histogram_cover - 1;

        if (most_recent_time >= 0) {
            // Most recent time defined
            if (most_recent_time > histogram_end_time) {
                // It is out of the requested range
                histogram_start_time = histogram_end_time + 1;
                continue;
            }
        }

        if (most_ancient_time >= 0) {
            // Most ancient time defined
            if (most_ancient_time < histogram_start_time) {
                // It is out of the requested range
                histogram_start_time = histogram_end_time + 1;
                continue;
            }
        }

        // Histogram is within the range. Take the counters.
        CJsonNode   slice = SerializeHistogram(bin.histogram);
        slice.SetInteger(kTimeRangeStart, histogram_start_time);
        slice.SetInteger(kTimeRangeEnd, histogram_end_time);
        ret.Append(slice);

        histogram_start_time = histogram_end_time + 1;
    }

    return ret;
}


CJsonNode CPSGTimingBase::SerializeCombined(int  most_ancient_time,
                                            int  most_recent_time,
                                            unsigned long  tick_span) const
{
    TPSGTiming::TTimeBins       bins = m_PSGTiming->GetHistograms();
    TOnePSGTiming               combined_histogram = bins.front().
                                    histogram.Clone(TOnePSGTiming::eCloneStructureOnly);

    int64_t         histogram_start_time = 0;
    int64_t         actual_recent_time = -1;    // undefined so far
    int64_t         actual_ancient_time = -1;   // undefined so far

    for (const auto &  bin : bins) {
        int64_t     histogram_cover = tick_span * bin.n_ticks;
        int64_t     histogram_end_time = histogram_start_time + histogram_cover - 1;

        if (most_recent_time >= 0) {
            // Most recent time defined
            if (most_recent_time > histogram_end_time) {
                // It is out of the requested range
                histogram_start_time = histogram_end_time + 1;
                continue;
            }
        }

        if (most_ancient_time >= 0) {
            // Most ancient time defined
            if (most_ancient_time < histogram_start_time) {
                // It is out of the requested range
                histogram_start_time = histogram_end_time + 1;
                continue;
            }
        }

        // Histogram is within the range. Take the counters.
        combined_histogram.AddCountersFrom(bin.histogram);

        // Update actual covered range if needed
        if (actual_recent_time == -1)
            actual_recent_time = histogram_start_time;
        actual_ancient_time = histogram_end_time;

        histogram_start_time = histogram_end_time + 1;
    }

    // The histograms were combined. Serialize them.
    if (actual_recent_time == -1 && actual_ancient_time == -1) {
        // Nothing fit the selected time range
        return CJsonNode::NewObjectNode();
    }

    CJsonNode   ret = SerializeHistogram(combined_histogram);
    ret.SetInteger(kTimeRangeStart, actual_recent_time);
    ret.SetInteger(kTimeRangeEnd, actual_ancient_time);
    return ret;
}


CLmdbCacheTiming::CLmdbCacheTiming(unsigned long  min_stat_value,
                                   unsigned long  max_stat_value,
                                   unsigned long  n_bins,
                                   TOnePSGTiming::EScaleType  stat_type,
                                   bool &  reset_to_default)
{
    reset_to_default = false;

    try {
        TOnePSGTiming       model_histogram(min_stat_value, max_stat_value,
                                            n_bins, stat_type);
        m_PSGTiming.reset(new TPSGTiming(model_histogram));
    } catch (...) {
        reset_to_default = true;
        TOnePSGTiming       model_histogram(kMinStatValue,
                                            kMaxStatValue,
                                            kNStatBins,
                                            TOnePSGTiming::eLog2);
        m_PSGTiming.reset(new TPSGTiming(model_histogram));
    }
}


CLmdbResolutionTiming::CLmdbResolutionTiming(unsigned long  min_stat_value,
                                             unsigned long  max_stat_value,
                                             unsigned long  n_bins,
                                             TOnePSGTiming::EScaleType  stat_type,
                                             bool &  reset_to_default)
{
    reset_to_default = false;

    try {
        TOnePSGTiming       model_histogram(min_stat_value, max_stat_value,
                                            n_bins, stat_type);
        m_PSGTiming.reset(new TPSGTiming(model_histogram));
    } catch (...) {
        reset_to_default = true;
        TOnePSGTiming       model_histogram(kMinStatValue,
                                            kMaxStatValue,
                                            kNStatBins,
                                            TOnePSGTiming::eLog2);
        m_PSGTiming.reset(new TPSGTiming(model_histogram));
    }
}


CCassTiming::CCassTiming(unsigned long  min_stat_value,
                         unsigned long  max_stat_value,
                         unsigned long  n_bins,
                         TOnePSGTiming::EScaleType  stat_type,
                         bool &  reset_to_default)
{
    reset_to_default = false;

    try {
        TOnePSGTiming       model_histogram(min_stat_value, max_stat_value,
                                            n_bins, stat_type);
        m_PSGTiming.reset(new TPSGTiming(model_histogram));
    } catch (...) {
        reset_to_default = true;
        TOnePSGTiming       model_histogram(kMinStatValue,
                                            kMaxStatValue,
                                            kNStatBins,
                                            TOnePSGTiming::eLog2);
        m_PSGTiming.reset(new TPSGTiming(model_histogram));
    }
}


CCassResolutionTiming::CCassResolutionTiming(unsigned long  min_stat_value,
                                             unsigned long  max_stat_value,
                                             unsigned long  n_bins,
                                             TOnePSGTiming::EScaleType  stat_type,
                                             bool &  reset_to_default)
{
    reset_to_default = false;

    try {
        TOnePSGTiming       model_histogram(min_stat_value, max_stat_value,
                                            n_bins, stat_type);
        m_PSGTiming.reset(new TPSGTiming(model_histogram));
    } catch (...) {
        reset_to_default = true;
        TOnePSGTiming       model_histogram(kMinStatValue,
                                            kMaxStatValue,
                                            kNStatBins,
                                            TOnePSGTiming::eLog2);
        m_PSGTiming.reset(new TPSGTiming(model_histogram));
    }
}


CBlobRetrieveTiming::CBlobRetrieveTiming(size_t  min_blob_size,
                                         size_t  max_blob_size,
                                         unsigned long  min_stat_value,
                                         unsigned long  max_stat_value,
                                         unsigned long  n_bins,
                                         TOnePSGTiming::EScaleType  stat_type,
                                         bool &  reset_to_default) :
    m_MinBlobSize(min_blob_size), m_MaxBlobSize(max_blob_size)
{
    reset_to_default = false;

    try {
        TOnePSGTiming       model_histogram(min_stat_value, max_stat_value,
                                            n_bins, stat_type);
        m_PSGTiming.reset(new TPSGTiming(model_histogram));
    } catch (...) {
        reset_to_default = true;
        TOnePSGTiming       model_histogram(kMinStatValue,
                                            kMaxStatValue,
                                            kNStatBins,
                                            TOnePSGTiming::eLog2);
        m_PSGTiming.reset(new TPSGTiming(model_histogram));
    }
}


static string   kStartBlobSize("MinBlobSize");
static string   kEndBlobSize("MaxBlobSize");

CJsonNode CBlobRetrieveTiming::SerializeCombined(int  most_ancient_time,
                                                 int  most_recent_time,
                                                 unsigned long  tick_span) const
{
    CJsonNode       timing = CPSGTimingBase::SerializeCombined(most_ancient_time,
                                                               most_recent_time,
                                                               tick_span);
    timing.SetInteger(kStartBlobSize, m_MinBlobSize);
    timing.SetInteger(kEndBlobSize, m_MaxBlobSize);
    return timing;
}


CJsonNode CBlobRetrieveTiming::SerializeSeries(int  most_ancient_time,
                                               int  most_recent_time,
                                               unsigned long  tick_span) const
{
    static string   kBins("Bins");

    CJsonNode   ret(CJsonNode::NewObjectNode());
    ret.SetByKey(kBins, CPSGTimingBase::SerializeSeries(most_ancient_time,
                                                        most_recent_time,
                                                        tick_span));
    ret.SetInteger(kStartBlobSize, m_MinBlobSize);
    ret.SetInteger(kEndBlobSize, m_MaxBlobSize);
    return ret;
}


CHugeBlobRetrieveTiming::CHugeBlobRetrieveTiming(
        unsigned long  min_stat_value,
        unsigned long  max_stat_value,
        unsigned long  n_bins,
        TOnePSGTiming::EScaleType  stat_type,
        bool &  reset_to_default)
{
    reset_to_default = false;

    try {
        TOnePSGTiming       model_histogram(min_stat_value, max_stat_value,
                                            n_bins, stat_type);
        m_PSGTiming.reset(new TPSGTiming(model_histogram));
    } catch (...) {
        reset_to_default = true;
        TOnePSGTiming       model_histogram(kMinStatValue,
                                            kMaxStatValue,
                                            kNStatBins,
                                            TOnePSGTiming::eLog2);
        m_PSGTiming.reset(new TPSGTiming(model_histogram));
    }
}


CNotFoundBlobRetrieveTiming::CNotFoundBlobRetrieveTiming(
        unsigned long  min_stat_value,
        unsigned long  max_stat_value,
        unsigned long  n_bins,
        TOnePSGTiming::EScaleType  stat_type,
        bool &  reset_to_default)
{
    reset_to_default = false;

    try {
        TOnePSGTiming       model_histogram(min_stat_value, max_stat_value,
                                            n_bins, stat_type);
        m_PSGTiming.reset(new TPSGTiming(model_histogram));
    } catch (...) {
        reset_to_default = true;
        TOnePSGTiming       model_histogram(kMinStatValue,
                                            kMaxStatValue,
                                            kNStatBins,
                                            TOnePSGTiming::eLog2);
        m_PSGTiming.reset(new TPSGTiming(model_histogram));
    }
}


CNARetrieveTiming::CNARetrieveTiming(unsigned long  min_stat_value,
                                     unsigned long  max_stat_value,
                                     unsigned long  n_bins,
                                     TOnePSGTiming::EScaleType  stat_type,
                                     bool &  reset_to_default)
{
    reset_to_default = false;

    try {
        TOnePSGTiming       model_histogram(min_stat_value, max_stat_value,
                                            n_bins, stat_type);
        m_PSGTiming.reset(new TPSGTiming(model_histogram));
    } catch (...) {
        reset_to_default = true;
        TOnePSGTiming       model_histogram(kMinStatValue,
                                            kMaxStatValue,
                                            kNStatBins,
                                            TOnePSGTiming::eLog2);
        m_PSGTiming.reset(new TPSGTiming(model_histogram));
    }
}


CSplitHistoryRetrieveTiming::CSplitHistoryRetrieveTiming(unsigned long  min_stat_value,
                                                         unsigned long  max_stat_value,
                                                         unsigned long  n_bins,
                                                         TOnePSGTiming::EScaleType  stat_type,
                                                         bool &  reset_to_default)
{
    reset_to_default = false;

    try {
        TOnePSGTiming       model_histogram(min_stat_value, max_stat_value,
                                            n_bins, stat_type);
        m_PSGTiming.reset(new TPSGTiming(model_histogram));
    } catch (...) {
        reset_to_default = true;
        TOnePSGTiming       model_histogram(kMinStatValue,
                                            kMaxStatValue,
                                            kNStatBins,
                                            TOnePSGTiming::eLog2);
        m_PSGTiming.reset(new TPSGTiming(model_histogram));
    }
}


CResolutionTiming::CResolutionTiming(unsigned long  min_stat_value,
                                     unsigned long  max_stat_value,
                                     unsigned long  n_bins,
                                     TOnePSGTiming::EScaleType  stat_type,
                                     bool &  reset_to_default)
{
    reset_to_default = false;

    try {
        TOnePSGTiming       model_histogram(min_stat_value, max_stat_value,
                                            n_bins, stat_type);
        m_PSGTiming.reset(new TPSGTiming(model_histogram));
    } catch (...) {
        reset_to_default = true;
        TOnePSGTiming       model_histogram(kMinStatValue,
                                            kMaxStatValue,
                                            kNStatBins,
                                            TOnePSGTiming::eLog2);
        m_PSGTiming.reset(new TPSGTiming(model_histogram));
    }
}



COperationTiming::COperationTiming(unsigned long  min_stat_value,
                                   unsigned long  max_stat_value,
                                   unsigned long  n_bins,
                                   const string &  stat_type,
                                   unsigned long  small_blob_size)
{
    auto        scale_type = TOnePSGTiming::eLog2;
    if (NStr::CompareNocase(stat_type, "linear") == 0)
        scale_type = TOnePSGTiming::eLinear;

    bool    reset_to_default = false;
    for (size_t  index = 0; index <= 1; ++index) {
        m_LookupLmdbSi2csiTiming.push_back(
            unique_ptr<CLmdbCacheTiming>(
                new CLmdbCacheTiming(min_stat_value, max_stat_value,
                                     n_bins, scale_type, reset_to_default)));
        m_LookupLmdbBioseqInfoTiming.push_back(
            unique_ptr<CLmdbCacheTiming>(
                new CLmdbCacheTiming(min_stat_value, max_stat_value,
                                     n_bins, scale_type, reset_to_default)));
        m_LookupLmdbBlobPropTiming.push_back(
            unique_ptr<CLmdbCacheTiming>(
                new CLmdbCacheTiming(min_stat_value, max_stat_value,
                                     n_bins, scale_type, reset_to_default)));
        m_LookupCassSi2csiTiming.push_back(
            unique_ptr<CCassTiming>(
                new CCassTiming(min_stat_value, max_stat_value,
                                n_bins, scale_type, reset_to_default)));
        m_LookupCassBioseqInfoTiming.push_back(
            unique_ptr<CCassTiming>(
                new CCassTiming(min_stat_value, max_stat_value,
                                n_bins, scale_type, reset_to_default)));
        m_LookupCassBlobPropTiming.push_back(
            unique_ptr<CCassTiming>(
                new CCassTiming(min_stat_value, max_stat_value,
                                n_bins, scale_type, reset_to_default)));

        m_ResolutionLmdbTiming.push_back(
            unique_ptr<CLmdbResolutionTiming>(
                new CLmdbResolutionTiming(min_stat_value, max_stat_value,
                                          n_bins, scale_type, reset_to_default)));
        m_ResolutionCassTiming.push_back(
            unique_ptr<CCassResolutionTiming>(
                new CCassResolutionTiming(min_stat_value, max_stat_value,
                                          n_bins, scale_type, reset_to_default)));

        m_NARetrieveTiming.push_back(
            unique_ptr<CNARetrieveTiming>(
                new CNARetrieveTiming(min_stat_value, max_stat_value,
                                      n_bins, scale_type, reset_to_default)));

        m_SplitHistoryRetrieveTiming.push_back(
            unique_ptr<CSplitHistoryRetrieveTiming>(
                new CSplitHistoryRetrieveTiming(min_stat_value, max_stat_value,
                                                n_bins, scale_type, reset_to_default)));
    }

    m_HugeBlobRetrievalTiming.reset(
        new CHugeBlobRetrieveTiming(min_stat_value, max_stat_value,
                                    n_bins, scale_type, reset_to_default));
    m_NotFoundBlobRetrievalTiming.reset(
        new CNotFoundBlobRetrieveTiming(min_stat_value, max_stat_value,
                                        n_bins, scale_type, reset_to_default));

    // Resolution timing
    m_ResolutionErrorTiming.reset(
        new CResolutionTiming(min_stat_value, max_stat_value,
                              n_bins, scale_type, reset_to_default));
    m_ResolutionNotFoundTiming.reset(
        new CResolutionTiming(min_stat_value, max_stat_value,
                              n_bins, scale_type, reset_to_default));
    m_ResolutionFoundInCacheTiming.reset(
        new CResolutionTiming(min_stat_value, max_stat_value,
                              n_bins, scale_type, reset_to_default));
    // 1, 2, 3, 4, 5+ trips to cassandra
    for (size_t  index = 0; index < 5; ++index) {
        m_ResolutionFoundCassandraTiming.push_back(
            unique_ptr<CResolutionTiming>(
                new CResolutionTiming(min_stat_value, max_stat_value,
                                      n_bins, scale_type, reset_to_default)));
    }


    reset_to_default |= x_SetupBlobSizeBins(min_stat_value, max_stat_value,
                                            n_bins, scale_type, small_blob_size);


    if (reset_to_default)
        ERR_POST("Invalid statistics parameters detected. Default parameters "
                 "were used");

    // fill the map between the histogram name and where it is stored
    m_NamesMap = {
        { "LookupLmdbSi2csiFound", m_LookupLmdbSi2csiTiming[0].get() },
        { "LookupLmdbSi2csiNotFound", m_LookupLmdbSi2csiTiming[1].get() },
        { "LookupLmdbBioseqInfoFound", m_LookupLmdbBioseqInfoTiming[0].get() },
        { "LookupLmdbBioseqInfoNotFound", m_LookupLmdbBioseqInfoTiming[1].get() },
        { "LookupLmdbBlobPropFound", m_LookupLmdbBlobPropTiming[0].get() },
        { "LookupLmdbBlobPropNotFound", m_LookupLmdbBlobPropTiming[1].get() },
        { "LookupCassSi2csiFound", m_LookupCassSi2csiTiming[0].get() },
        { "LookupCassSi2csiNotFound", m_LookupCassSi2csiTiming[1].get() },
        { "LookupCassBioseqInfoFound", m_LookupCassBioseqInfoTiming[0].get() },
        { "LookupCassBioseqInfoNotFound", m_LookupCassBioseqInfoTiming[1].get() },
        { "LookupCassBlobPropFound", m_LookupCassBlobPropTiming[0].get() },
        { "LookupCassBlobPropNotFound", m_LookupCassBlobPropTiming[1].get() },
        { "ResolutionLmdbFound", m_ResolutionLmdbTiming[0].get() },
        { "ResolutionLmdbNotFound", m_ResolutionLmdbTiming[1].get() },
        { "ResolutionCassFound", m_ResolutionCassTiming[0].get() },
        { "ResolutionCassNotFound", m_ResolutionCassTiming[1].get() },
        { "NARetrieveFound", m_NARetrieveTiming[0].get() },
        { "NARetrieveNotFound", m_NARetrieveTiming[1].get() },
        { "SplitHistoryRetrieveFound", m_SplitHistoryRetrieveTiming[0].get() },
        { "SplitHistoryRetrieveNotFound", m_SplitHistoryRetrieveTiming[1].get() },
        { "HugeBlobRetrieval", m_HugeBlobRetrievalTiming.get() },
        { "BlobRetrievalNotFound", m_NotFoundBlobRetrievalTiming.get() },
        { "ResolutionError", m_ResolutionErrorTiming.get() },
        { "ResolutionNotFound", m_ResolutionNotFoundTiming.get() },
        { "ResolutionFoundInCache", m_ResolutionFoundInCacheTiming.get() },
        { "ResolutionFoundCassandraIn1Try", m_ResolutionFoundCassandraTiming[0].get() },
        { "ResolutionFoundCassandraIn2Tries", m_ResolutionFoundCassandraTiming[1].get() },
        { "ResolutionFoundCassandraIn3Tries", m_ResolutionFoundCassandraTiming[2].get() },
        { "ResolutionFoundCassandraIn4Tries", m_ResolutionFoundCassandraTiming[3].get() },
        { "ResolutionFoundCassandraIn5OrMoreTries", m_ResolutionFoundCassandraTiming[4].get() }
    };

    for (auto & retieve_timing : m_BlobRetrieveTiming) {
        string      name = "BlobRetrievalFrom" +
                           to_string(retieve_timing->GetMinBlobSize()) + "To" +
                           to_string(retieve_timing->GetMaxBlobSize());
        m_NamesMap[name] = retieve_timing.get();
    }
}


bool COperationTiming::x_SetupBlobSizeBins(unsigned long  min_stat_value,
                                           unsigned long  max_stat_value,
                                           unsigned long  n_bins,
                                           TOnePSGTiming::EScaleType  stat_type,
                                           unsigned long  small_blob_size)
{
    bool    reset_to_default = false;

    m_BlobRetrieveTiming.push_back(
            unique_ptr<CBlobRetrieveTiming>(
                new CBlobRetrieveTiming(0, small_blob_size,
                                        min_stat_value, max_stat_value,
                                        n_bins, stat_type,
                                        reset_to_default)));
    m_Ends.push_back(small_blob_size);

    unsigned long   range_end = small_blob_size;
    unsigned long   range_start = range_end + 1;
    size_t          k = 0;

    for (;;) {
        range_end = (size_t(1) << k) - 1;
        if (range_end <= small_blob_size) {
            ++k;
            continue;
        }

        m_BlobRetrieveTiming.push_back(
                unique_ptr<CBlobRetrieveTiming>(
                    new CBlobRetrieveTiming(range_start, range_end,
                                            min_stat_value, max_stat_value,
                                            n_bins, stat_type,
                                            reset_to_default)));
        m_Ends.push_back(range_end);

        range_start = range_end + 1;
        if (range_start >= kMaxBlobSize)
            break;
        ++k;
    }

    return reset_to_default;
}


ssize_t COperationTiming::x_GetBlobRetrievalBinIndex(unsigned long  blob_size)
{
    if (blob_size >= kMaxBlobSize)
        return -1;

    for (size_t  index = 0; ; ++index) {
        if (m_Ends[index] >= blob_size)
            return index;
    }
    return -1;
}


void COperationTiming::Register(EPSGOperation  operation,
                                EPSGOperationStatus  status,
                                const THighResolutionTimePoint &  op_begin_ts,
                                unsigned long  blob_size)
{
    auto            now = chrono::high_resolution_clock::now();
    uint64_t        mks = chrono::duration_cast<chrono::microseconds>(now - op_begin_ts).count();

    size_t          index = 0;
    if (status == eOpStatusNotFound)
        index = 1;

    switch (operation) {
        case eLookupLmdbSi2csi:
            m_LookupLmdbSi2csiTiming[index]->Add(mks);
            break;
        case eLookupLmdbBioseqInfo:
            m_LookupLmdbBioseqInfoTiming[index]->Add(mks);
            break;
        case eLookupLmdbBlobProp:
            m_LookupLmdbBlobPropTiming[index]->Add(mks);
            break;
        case eLookupCassSi2csi:
            m_LookupCassSi2csiTiming[index]->Add(mks);
            break;
        case eLookupCassBioseqInfo:
            m_LookupCassBioseqInfoTiming[index]->Add(mks);
            break;
        case eLookupCassBlobProp:
            m_LookupCassBlobPropTiming[index]->Add(mks);
            break;
        case eResolutionLmdb:
            m_ResolutionLmdbTiming[index]->Add(mks);
            break;
        case eResolutionCass:
            m_ResolutionCassTiming[index]->Add(mks);
            break;
        case eBlobRetrieve:
            if (status == eOpStatusNotFound)
                m_NotFoundBlobRetrievalTiming->Add(mks);
            else {
                ssize_t     bin_index = x_GetBlobRetrievalBinIndex(blob_size);
                if (bin_index < 0)
                    m_HugeBlobRetrievalTiming->Add(mks);
                else
                    m_BlobRetrieveTiming[bin_index]->Add(mks);
            }
            break;
        case eNARetrieve:
            m_NARetrieveTiming[index]->Add(mks);
            break;
        case eSplitHistoryRetrieve:
            m_SplitHistoryRetrieveTiming[index]->Add(mks);
            break;
        case eResolutionError:
            m_ResolutionErrorTiming->Add(mks);
            break;
        case eResolutionNotFound:
            m_ResolutionNotFoundTiming->Add(mks);
            break;
        case eResolutionFoundInCache:
            m_ResolutionFoundInCacheTiming->Add(mks);
            break;
        case eResolutionFoundInCassandra:
            // The blob_size here is the number of queries of Cassandra
            index = blob_size - 1;
            if (index > 4)
                index = 4;
            m_ResolutionFoundCassandraTiming[index]->Add(mks);
            break;
    }
}


void COperationTiming::Rotate(void)
{
    lock_guard<mutex>   guard(m_Lock);

    for (size_t  k = 0; k <= 1; ++k) {
        m_LookupLmdbSi2csiTiming[k]->Rotate();
        m_LookupLmdbBioseqInfoTiming[k]->Rotate();
        m_LookupLmdbBlobPropTiming[k]->Rotate();
        m_LookupCassSi2csiTiming[k]->Rotate();
        m_LookupCassBioseqInfoTiming[k]->Rotate();
        m_LookupCassBlobPropTiming[k]->Rotate();
        m_ResolutionLmdbTiming[k]->Rotate();
        m_ResolutionCassTiming[k]->Rotate();
        m_NARetrieveTiming[k]->Rotate();
        m_SplitHistoryRetrieveTiming[k]->Rotate();
    }

    m_HugeBlobRetrievalTiming->Rotate();
    m_NotFoundBlobRetrievalTiming->Rotate();

    m_ResolutionErrorTiming->Rotate();
    m_ResolutionNotFoundTiming->Rotate();
    m_ResolutionFoundInCacheTiming->Rotate();
    for (auto &  item : m_ResolutionFoundCassandraTiming)
        item->Rotate();

    for (auto &  item : m_BlobRetrieveTiming)
        item->Rotate();
}


void COperationTiming::Reset(void)
{
    lock_guard<mutex>   guard(m_Lock);

    for (size_t  k = 0; k <= 1; ++k) {
        m_LookupLmdbSi2csiTiming[k]->Reset();
        m_LookupLmdbBioseqInfoTiming[k]->Reset();
        m_LookupLmdbBlobPropTiming[k]->Reset();
        m_LookupCassSi2csiTiming[k]->Reset();
        m_LookupCassBioseqInfoTiming[k]->Reset();
        m_LookupCassBlobPropTiming[k]->Reset();
        m_ResolutionLmdbTiming[k]->Reset();
        m_ResolutionCassTiming[k]->Reset();
        m_NARetrieveTiming[k]->Reset();
        m_SplitHistoryRetrieveTiming[k]->Reset();
    }

    m_HugeBlobRetrievalTiming->Reset();
    m_NotFoundBlobRetrievalTiming->Reset();

    m_ResolutionErrorTiming->Reset();
    m_ResolutionNotFoundTiming->Reset();
    m_ResolutionFoundInCacheTiming->Reset();
    for (auto &  item : m_ResolutionFoundCassandraTiming)
        item->Reset();

    for (auto &  item : m_BlobRetrieveTiming)
        item->Reset();
}


CJsonNode
COperationTiming::Serialize(int  most_ancient_time,
                            int  most_recent_time,
                            const vector<CTempString> &  histogram_names,
                            unsigned long  tick_span) const
{
    static string   kSecondsCovered("SecondsCovered");

    lock_guard<mutex>       guard(m_Lock);

    CJsonNode       ret(CJsonNode::NewObjectNode());

    // All the histograms have the same number of covered ticks
    ret.SetInteger(kSecondsCovered,
                   tick_span * m_ResolutionFoundInCacheTiming->GetNumberOfCoveredTicks());

    if (histogram_names.empty()) {
        for (const auto &  name_to_histogram : m_NamesMap) {
            ret.SetByKey(name_to_histogram.first,
                         name_to_histogram.second->SerializeCombined(most_ancient_time,
                                                                     most_recent_time,
                                                                     tick_span));
        }
    } else {
        for (const auto &  name : histogram_names) {
            string      histogram_name(name.data(), name.size());
            const auto  iter = m_NamesMap.find(histogram_name);
            if (iter != m_NamesMap.end()) {
                ret.SetByKey(
                    histogram_name,
                    iter->second->SerializeSeries(most_ancient_time,
                                                  most_recent_time,
                                                  tick_span));
            }
        }
    }

    return ret;
}

