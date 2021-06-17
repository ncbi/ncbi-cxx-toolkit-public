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
#include "pubseq_gateway.hpp"

static string           kTimeRangeStart("TimeRangeStart");
static string           kTimeRangeEnd("TimeRangeEnd");

const unsigned long     kMaxBlobSize = 1024L*1024L*1024L*8L;   // 8 GB


CJsonNode SerializeHistogram(const TOnePSGTiming &  histogram,
                             const string  &  name,
                             const string  &  description)
{
    static string   kBins("Bins");
    static string   kStart("Start");
    static string   kEnd("End");
    static string   kCount("Count");
    static string   kLowerAnomaly("LowerAnomaly");
    static string   kUpperAnomaly("UpperAnomaly");
    static string   kTotalCount("TotalCount");
    static string   kValueSum("ValueSum");
    static string   kName("name");
    static string   kDescription("description");

    CJsonNode       ret(CJsonNode::NewObjectNode());
    ret.SetString(kName, name);
    ret.SetString(kDescription, description);


    CJsonNode       bins(CJsonNode::NewArrayNode());

    size_t          bin_count =  histogram.GetNumberOfBins();
    size_t          last_bin_index = bin_count - 1;
    auto            starts = histogram.GetBinStartsPtr();
    auto            counters = histogram.GetBinCountersPtr();
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
                                          unsigned long  tick_span,
                                          const string &  name,
                                          const string &  description) const
{
    CJsonNode                   ret(CJsonNode::NewArrayNode());
    TPSGTiming::TTimeBins       bins = m_PSGTiming->GetHistograms();

    int64_t         histogram_start_time = 0;

    for (auto &  bin : bins) {
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
        CJsonNode   slice = SerializeHistogram(bin.histogram, name, description);
        slice.SetInteger(kTimeRangeStart, histogram_start_time);
        slice.SetInteger(kTimeRangeEnd, histogram_end_time);
        ret.Append(slice);

        histogram_start_time = histogram_end_time + 1;
    }

    return ret;
}


CJsonNode CPSGTimingBase::SerializeCombined(int  most_ancient_time,
                                            int  most_recent_time,
                                            unsigned long  tick_span,
                                            const string &  name,
                                            const string &  description) const
{
    TPSGTiming::TTimeBins       bins = m_PSGTiming->GetHistograms();
    TOnePSGTiming               combined_histogram = bins.front().
                                    histogram.Clone(TOnePSGTiming::eCloneStructureOnly);

    int64_t         histogram_start_time = 0;
    int64_t         actual_recent_time = -1;    // undefined so far
    int64_t         actual_ancient_time = -1;   // undefined so far

    for (auto &  bin : bins) {
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

    CJsonNode   ret = SerializeHistogram(combined_histogram, name, description);
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
                                                 unsigned long  tick_span,
                                                 const string &  name,
                                                 const string &  description) const
{
    CJsonNode       timing = CPSGTimingBase::SerializeCombined(most_ancient_time,
                                                               most_recent_time,
                                                               tick_span,
                                                               name,
                                                               description);
    timing.SetInteger(kStartBlobSize, m_MinBlobSize);
    timing.SetInteger(kEndBlobSize, m_MaxBlobSize);
    return timing;
}


CJsonNode CBlobRetrieveTiming::SerializeSeries(int  most_ancient_time,
                                               int  most_recent_time,
                                               unsigned long  tick_span,
                                               const string &  name,
                                               const string &  description) const
{
    static string   kBins("Bins");

    CJsonNode   ret(CJsonNode::NewObjectNode());
    ret.SetByKey(kBins, CPSGTimingBase::SerializeSeries(most_ancient_time,
                                                        most_recent_time,
                                                        tick_span,
                                                        name,
                                                        description));
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


CPublicCommentRetrieveTiming::CPublicCommentRetrieveTiming(unsigned long  min_stat_value,
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
                                   unsigned long  small_blob_size) :
    m_HugeBlobByteCounter(0)
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

        m_PublicCommentRetrieveTiming.push_back(
            unique_ptr<CPublicCommentRetrieveTiming>(
                new CPublicCommentRetrieveTiming(min_stat_value, max_stat_value,
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
    m_ResolutionFoundTiming.reset(
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
        { "LookupLmdbSi2csiFound",
          SInfo(m_LookupLmdbSi2csiTiming[0].get(),
                "si2csi LMDB cache found",
                "The timing of si2csi LMDB cache lookup "
                "when a record was found"
               )
        },
        { "LookupLmdbSi2csiNotFound",
          SInfo(m_LookupLmdbSi2csiTiming[1].get(),
                "si2csi LMDB cache not found",
                "The timing of si2csi LMDB cache lookup "
                "when there was no record found"
               )
        },
        { "LookupLmdbBioseqInfoFound",
          SInfo(m_LookupLmdbBioseqInfoTiming[0].get(),
                "bioseq info LMDB cache found",
                "The timing of bioseq info LMDB cache lookup "
                "when a record was found"
               )
        },
        { "LookupLmdbBioseqInfoNotFound",
          SInfo(m_LookupLmdbBioseqInfoTiming[1].get(),
                "bioseq info LMDB cache not found",
                "The timing of bioseq info LMDB cache lookup "
                "when there was no record found"
               )
        },
        { "LookupLmdbBlobPropFound",
          SInfo(m_LookupLmdbBlobPropTiming[0].get(),
                "blob properties LMDB cache found",
                "The timing of blob properties LMDB cache lookup "
                "when a record was found"
               )
        },
        { "LookupLmdbBlobPropNotFound",
          SInfo(m_LookupLmdbBlobPropTiming[1].get(),
                "blob properties LMDB cache not found",
                "The timing of blob properties LMDB cache lookup "
                "when there was no record found"
               )
        },
        { "LookupCassSi2csiFound",
          SInfo(m_LookupCassSi2csiTiming[0].get(),
                "si2csi Cassandra found",
                "The timing of si2csi Cassandra lookup "
                "when a record was found"
               )
        },
        { "LookupCassSi2csiNotFound",
          SInfo(m_LookupCassSi2csiTiming[1].get(),
                "si2csi Cassandra not found",
                "The timing of si2csi Cassandra lookup "
                "when there was no record found"
               )
        },
        { "LookupCassBioseqInfoFound",
          SInfo(m_LookupCassBioseqInfoTiming[0].get(),
                "bioseq info Cassandra found",
                "The timing of bioseq info Cassandra lookup "
                "when a record was found"
               )
        },
        { "LookupCassBioseqInfoNotFound",
          SInfo(m_LookupCassBioseqInfoTiming[1].get(),
                "bioseq info Cassandra not found",
                "The timing of bioseq info Cassandra lookup "
                "when there was no record found"
               )
        },
        { "LookupCassBlobPropFound",
          SInfo(m_LookupCassBlobPropTiming[0].get(),
                "blob properties Cassandra found",
                "The timing of blob properties Cassandra lookup "
                "when a record was found"
               )
        },
        { "LookupCassBlobPropNotFound",
          SInfo(m_LookupCassBlobPropTiming[1].get(),
                "blob properties Cassandra not found",
                "The timing of blob properties Cassandra lookup "
                "when there was no record found"
               )
        },
        { "ResolutionLmdbFound",
          SInfo(m_ResolutionLmdbTiming[0].get(),
                "LMDB resolution succeeded",
                "The timing of a seq id successful resolution "
                "in LMDB cache (start: request is received)"
               )
        },
        { "ResolutionLmdbNotFound",
          SInfo(m_ResolutionLmdbTiming[1].get(),
                "LMDB resolution not found",
                "The timing of a seq id unsuccessful resolution "
                "when all the tries in LMDB cache led to nothing "
                "(start: request is received)"
               )
        },
        { "ResolutionCassFound",
          SInfo(m_ResolutionCassTiming[0].get(),
                "Cassandra resolution succeeded",
                "The timing of a seq id successful resolution "
                "in Cassandra regardless how many queries were "
                "made to Cassandra (start: first Cassandra query)"
               )
        },
        { "ResolutionCassNotFound",
          SInfo(m_ResolutionCassTiming[1].get(),
                "Cassandra resolution not found",
                "The timing of a seq id unsuccessful resolution "
                "when all the tries in Cassandra led to nothing "
                "start: first Cassandra query)"
               )
        },
        { "NARetrieveFound",
          SInfo(m_NARetrieveTiming[0].get(),
                "Named annotations found",
                "The timing of named annotations successful retrieval"
               )
        },
        { "NARetrieveNotFound",
          SInfo(m_NARetrieveTiming[1].get(),
                "Named annotations not found",
                "The timing of named annotations retrieval "
                "when nothing was found"
               )
        },
        { "SplitHistoryRetrieveFound",
          SInfo(m_SplitHistoryRetrieveTiming[0].get(),
                "Split history found",
                "The timing of split history successful retrieval"
               )
        },
        { "SplitHistoryRetrieveNotFound",
          SInfo(m_SplitHistoryRetrieveTiming[1].get(),
                "Split history not found",
                "The timing of split history retrieval "
                "when nothing was found"
               )
        },
        { "PublicCommentRetrieveFound",
          SInfo(m_PublicCommentRetrieveTiming[0].get(),
                "Public comment found",
                "The timing of a public comment successful retrieval"
               )
        },
        { "PublicCommentRetrieveNotFound",
          SInfo(m_PublicCommentRetrieveTiming[1].get(),
                "Public comment not found",
                "The timing of public comment retrieval "
                "when nothing was found"
               )
        },
        { "HugeBlobRetrieval",
          SInfo(m_HugeBlobRetrievalTiming.get(),
                "Huge blob retrieval",
                "The timing of the very large blob retrieval",
                &m_HugeBlobByteCounter,
                "HugeBlobByteCounter",
                "Huge blob bytes counter",
                "The number of bytes transferred to the user as very large blobs"
               )
        },
        { "BlobRetrievalNotFound",
          SInfo(m_NotFoundBlobRetrievalTiming.get(),
                "Blob retrieval not found",
                "The timing of blob retrieval when a blob was not found"
               )
        },
        { "ResolutionError",
          SInfo(m_ResolutionErrorTiming.get(),
                "Resolution error",
                "The timing of a case when an error was detected while "
                "resolving seq id regardless it was cache, Cassandra or both "
                "(start: request is received)"
               )
        },
        { "ResolutionNotFound",
          SInfo(m_ResolutionNotFoundTiming.get(),
                "Resolution not found",
                "The timing of a case when resolution of a seq id did not succeed "
                "regardless it was cache, Cassandra or both "
                "(start: request is received)"
               )
        },
        { "ResolutionFound",
          SInfo(m_ResolutionFoundTiming.get(),
                "Resolution succeeded",
                "The timing of a seq id successful resolution regardless it "
                "was cache, Cassandra or both "
                "(start: request is received)"
               )
        },
        { "ResolutionFoundCassandraIn1Try",
          SInfo(m_ResolutionFoundCassandraTiming[0].get(),
                "Resolution succeeded via Cassandra (1 try)",
                "The timing of a seq id resolution in Cassandra when "
                "1 try was required (start: first Cassandra query)"
               )
        },
        { "ResolutionFoundCassandraIn2Tries",
          SInfo(m_ResolutionFoundCassandraTiming[1].get(),
                "Resolution succeeded via Cassandra (2 tries)",
                "The timing of a seq id resolution in Cassandra when "
                "2 tries were required (start: first Cassandra query)"
               )
        },
        { "ResolutionFoundCassandraIn3Tries",
          SInfo(m_ResolutionFoundCassandraTiming[2].get(),
                "Resolution succeeded via Cassandra (3 tries)",
                "The timing of a seq id resolution in Cassandra when "
                "3 tries were required (start: first Cassandra query)"
               )
        },
        { "ResolutionFoundCassandraIn4Tries",
          SInfo(m_ResolutionFoundCassandraTiming[3].get(),
                "Resolution succeeded via Cassandra (4 tries)",
                "The timing of a seq id resolution in Cassandra when "
                "4 tries were required (start: first Cassandra query)"
               )
        },
        { "ResolutionFoundCassandraIn5OrMoreTries",
          SInfo(m_ResolutionFoundCassandraTiming[4].get(),
                "Resolution succeeded via Cassandra (5 tries or more)",
                "The timing of a seq id resolution in Cassandra when "
                "5 or more tries were required (start: first Cassandra query)"
               )
        }
    };

    size_t      index = 0;
    for (auto & retieve_timing : m_BlobRetrieveTiming) {
        string      min_size_str = to_string(retieve_timing->GetMinBlobSize());
        string      max_size_str = to_string(retieve_timing->GetMaxBlobSize());
        string      id = "BlobRetrievalFrom" + min_size_str + "To" + max_size_str;
        string      name = "Blob retrieval (size: " +
                        min_size_str + " to " + max_size_str + ")";
        string      description = "The timing of a blob retrieval when "
                        "the blob size is between " + min_size_str +
                        " and " + max_size_str + " bytes";

        string      counter_id = "BlobByteCounterFrom" + min_size_str +
                                 "To" + max_size_str;
        string      counter_name = "Blob byte counter (blob size: " +
                                   min_size_str + " to " + max_size_str + ")";
        string      counter_description = "The number of bytes transferred to "
                                          "the user as blobs size between " +
                                          min_size_str + " and " +
                                          max_size_str;
        m_NamesMap[id] = SInfo(retieve_timing.get(), name, description,
                               &m_BlobByteCounters[index],
                               counter_id, counter_name, counter_description);
        ++index;
    }

    // Overwrite the default names and descriptions with what came from
    // the configuration
    auto        app = CPubseqGatewayApp::GetInstance();
    auto        id_to_name_and_desc = app->GetIdToNameAndDescriptionMap();

    for (const auto &  item : id_to_name_and_desc) {
        if (m_NamesMap.find(item.first) != m_NamesMap.end()) {
            m_NamesMap[item.first].m_Name = get<0>(item.second);
            m_NamesMap[item.first].m_Description = get<1>(item.second);
        } else {
            // May need to overwrite the associated counters names and
            // descriptions
            for (auto &  info : m_NamesMap) {
                if (info.second.m_CounterId == item.first) {
                    info.second.m_CounterName = get<0>(item.second);
                    info.second.m_CounterDescription = get<1>(item.second);
                    break;
                }
            }
        }
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
    m_BlobByteCounters.push_back(0);
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
        m_BlobByteCounters.push_back(0);
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
                                const TPSGS_HighResolutionTimePoint &  op_begin_ts,
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
                if (bin_index < 0) {
                    m_HugeBlobByteCounter += blob_size;
                    m_HugeBlobRetrievalTiming->Add(mks);
                } else {
                    m_BlobByteCounters[bin_index] += blob_size;
                    m_BlobRetrieveTiming[bin_index]->Add(mks);
                }
            }
            break;
        case eNARetrieve:
            m_NARetrieveTiming[index]->Add(mks);
            break;
        case eSplitHistoryRetrieve:
            m_SplitHistoryRetrieveTiming[index]->Add(mks);
            break;
        case ePublicCommentRetrieve:
            m_PublicCommentRetrieveTiming[index]->Add(mks);
            break;
        case eResolutionError:
            m_ResolutionErrorTiming->Add(mks);
            break;
        case eResolutionNotFound:
            m_ResolutionNotFoundTiming->Add(mks);
            break;
        case eResolutionFound:
            m_ResolutionFoundTiming->Add(mks);
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
        m_PublicCommentRetrieveTiming[k]->Rotate();
    }

    m_HugeBlobRetrievalTiming->Rotate();
    m_NotFoundBlobRetrievalTiming->Rotate();

    m_ResolutionErrorTiming->Rotate();
    m_ResolutionNotFoundTiming->Rotate();
    m_ResolutionFoundTiming->Rotate();
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
        m_PublicCommentRetrieveTiming[k]->Reset();
    }

    m_HugeBlobRetrievalTiming->Reset();
    m_NotFoundBlobRetrievalTiming->Reset();

    m_ResolutionErrorTiming->Reset();
    m_ResolutionNotFoundTiming->Reset();
    m_ResolutionFoundTiming->Reset();
    for (auto &  item : m_ResolutionFoundCassandraTiming)
        item->Reset();

    for (auto &  item : m_BlobRetrieveTiming)
        item->Reset();

    for (auto &  item : m_BlobByteCounters)
        item = 0;
    m_HugeBlobByteCounter = 0;
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
                   tick_span * m_ResolutionFoundTiming->GetNumberOfCoveredTicks());

    if (histogram_names.empty()) {
        for (const auto &  name_to_histogram : m_NamesMap) {
            ret.SetByKey(name_to_histogram.first,
                         name_to_histogram.second.m_Timing->SerializeCombined(
                             most_ancient_time,
                             most_recent_time,
                             tick_span,
                             name_to_histogram.second.m_Name,
                             name_to_histogram.second.m_Description));
            if (name_to_histogram.second.m_Counter != nullptr) {
                CJsonNode       bytes_counter(CJsonNode::NewObjectNode());
                bytes_counter.SetString("name",
                                        name_to_histogram.second.m_CounterName);
                bytes_counter.SetString("description",
                                        name_to_histogram.second.m_CounterDescription);
                bytes_counter.SetInteger("bytes",
                                         *name_to_histogram.second.m_Counter);
                ret.SetByKey(name_to_histogram.second.m_CounterId,
                             bytes_counter);
            }
        }
    } else {
        for (const auto &  name : histogram_names) {
            string      histogram_name(name.data(), name.size());
            const auto  iter = m_NamesMap.find(histogram_name);
            if (iter != m_NamesMap.end()) {
                ret.SetByKey(
                    histogram_name,
                    iter->second.m_Timing->SerializeSeries(most_ancient_time,
                                                           most_recent_time,
                                                           tick_span,
                                                           iter->second.m_Name,
                                                           iter->second.m_Description));
                if (iter->second.m_Counter != nullptr) {
                    CJsonNode       bytes_counter(CJsonNode::NewObjectNode());
                    bytes_counter.SetString("name",
                                            iter->second.m_CounterName);
                    bytes_counter.SetString("description",
                                            iter->second.m_CounterDescription);
                    bytes_counter.SetInteger("bytes",
                                             *(iter->second.m_Counter));
                    ret.SetByKey(iter->second.m_CounterId,
                                 bytes_counter);
                }
            }
        }
    }

    return ret;
}
