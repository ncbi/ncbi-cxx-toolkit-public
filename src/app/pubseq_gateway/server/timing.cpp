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
#include "ipsgs_processor.hpp"

static string           kTimeRangeStart("TimeRangeStart");
static string           kTimeRangeEnd("TimeRangeEnd");

const unsigned long     kMaxBlobSize = 1024L*1024L*1024L*8L;   // 8 GB


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
static string   kMaxValue("MaxValue");


CJsonNode SerializeHistogram(const TOnePSGTiming &  histogram,
                             const string  &  name,
                             const string  &  description,
                             uint64_t  max_value)
{
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
    ret.SetInteger(kMaxValue, max_value);
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
        CJsonNode   slice = SerializeHistogram(bin.histogram, name, description,
                                               GetMaxValue());
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

    CJsonNode   ret = SerializeHistogram(combined_histogram, name, description,
                                         GetMaxValue());
    ret.SetInteger(kTimeRangeStart, actual_recent_time);
    ret.SetInteger(kTimeRangeEnd, actual_ancient_time);
    return ret;
}


#define TIMING_CLASS_DEF(class_name)                                                \
    class_name::class_name(unsigned long  min_stat_value,                           \
                           unsigned long  max_stat_value,                           \
                           unsigned long  n_bins,                                   \
                           TOnePSGTiming::EScaleType  stat_type,                    \
                           bool &  reset_to_default)                                \
    {                                                                               \
        reset_to_default = false;                                                   \
        try {                                                                       \
            TOnePSGTiming       model_histogram(min_stat_value, max_stat_value,     \
                                                n_bins, stat_type);                 \
            m_PSGTiming.reset(new TPSGTiming(model_histogram));                     \
        } catch (...) {                                                             \
            reset_to_default = true;                                                \
            TOnePSGTiming       model_histogram(kMinStatValue,                      \
                                                kMaxStatValue,                      \
                                                kNStatBins,                         \
                                                TOnePSGTiming::eLog2);              \
            m_PSGTiming.reset(new TPSGTiming(model_histogram));                     \
        }                                                                           \
    }

TIMING_CLASS_DEF(CLmdbCacheTiming);
TIMING_CLASS_DEF(CLmdbResolutionTiming);
TIMING_CLASS_DEF(CCassTiming);
TIMING_CLASS_DEF(CMyNCBITiming);
TIMING_CLASS_DEF(CCassResolutionTiming);
TIMING_CLASS_DEF(CHugeBlobRetrieveTiming);
TIMING_CLASS_DEF(CNotFoundBlobRetrieveTiming);
TIMING_CLASS_DEF(CNARetrieveTiming);
TIMING_CLASS_DEF(CSplitHistoryRetrieveTiming);
TIMING_CLASS_DEF(CPublicCommentRetrieveTiming);
TIMING_CLASS_DEF(CAccVerHistoryRetrieveTiming);
TIMING_CLASS_DEF(CIPGResolveRetrieveTiming);
TIMING_CLASS_DEF(CTSEChunkRetrieveTiming);
TIMING_CLASS_DEF(CNAResolveTiming);
TIMING_CLASS_DEF(CVDBOpenTiming);
TIMING_CLASS_DEF(CSNPPTISLookupTiming);
TIMING_CLASS_DEF(CWGSVDBLookupTiming);
TIMING_CLASS_DEF(CResolutionTiming);
TIMING_CLASS_DEF(CBacklogTiming);
TIMING_CLASS_DEF(CProcessorPerformanceTiming);


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


COperationTiming::COperationTiming(unsigned long  min_stat_value,
                                   unsigned long  max_stat_value,
                                   unsigned long  n_bins,
                                   const string &  stat_type,
                                   unsigned long  small_blob_size,
                                   const string &  only_for_processor,
                                   size_t  log_timing_threshold,
                                   const map<string, size_t> &  proc_group_to_index) :
    m_OnlyForProcessor(only_for_processor),
    m_LogTimingThresholdMks(log_timing_threshold * 1000),
    m_ProcGroupToIndex(proc_group_to_index)
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
        m_RetrieveMyNCBITiming.push_back(
            unique_ptr<CMyNCBITiming>(
                new CMyNCBITiming(min_stat_value, max_stat_value,
                                  n_bins, scale_type, reset_to_default)));

        m_ResolutionLmdbTiming.push_back(
            unique_ptr<CLmdbResolutionTiming>(
                new CLmdbResolutionTiming(min_stat_value, max_stat_value,
                                          n_bins, scale_type, reset_to_default)));
        m_ResolutionCassTiming.push_back(
            unique_ptr<CCassResolutionTiming>(
                new CCassResolutionTiming(min_stat_value, max_stat_value,
                                          n_bins, scale_type, reset_to_default)));

        m_SplitHistoryRetrieveTiming.push_back(
            unique_ptr<CSplitHistoryRetrieveTiming>(
                new CSplitHistoryRetrieveTiming(min_stat_value, max_stat_value,
                                                n_bins, scale_type, reset_to_default)));

        m_PublicCommentRetrieveTiming.push_back(
            unique_ptr<CPublicCommentRetrieveTiming>(
                new CPublicCommentRetrieveTiming(min_stat_value, max_stat_value,
                                                 n_bins, scale_type, reset_to_default)));

        m_AccVerHistoryRetrieveTiming.push_back(
            unique_ptr<CAccVerHistoryRetrieveTiming>(
                new CAccVerHistoryRetrieveTiming(min_stat_value, max_stat_value,
                                                 n_bins, scale_type, reset_to_default)));

        m_IPGResolveRetrieveTiming.push_back(
            unique_ptr<CIPGResolveRetrieveTiming>(
                new CIPGResolveRetrieveTiming(min_stat_value, max_stat_value,
                                              n_bins, scale_type, reset_to_default)));

        m_VDBOpenTiming.push_back(
            unique_ptr<CVDBOpenTiming>(
                new CVDBOpenTiming(min_stat_value, max_stat_value,
                                   n_bins, scale_type, reset_to_default)));

        m_SNPPTISLookupTiming.push_back(
            unique_ptr<CSNPPTISLookupTiming>(
                new CSNPPTISLookupTiming(min_stat_value, max_stat_value,
                                         n_bins, scale_type, reset_to_default)));

        m_WGSVDBLookupTiming.push_back(
            unique_ptr<CWGSVDBLookupTiming>(
                new CWGSVDBLookupTiming(min_stat_value, max_stat_value,
                                        n_bins, scale_type, reset_to_default)));

    }

    m_RetrieveMyNCBIErrorTiming.reset(
        new CMyNCBITiming(min_stat_value, max_stat_value,
                          n_bins, scale_type, reset_to_default));

    m_BacklogTiming.reset(
        new CBacklogTiming(min_stat_value, max_stat_value,
                           n_bins, scale_type, reset_to_default));


    // 1, 2, 3, 4, 5+ trips to cassandra
    for (size_t  index = 0; index < 5; ++index) {
        m_ResolutionFoundCassandraTiming.push_back(
            unique_ptr<CResolutionTiming>(
                new CResolutionTiming(min_stat_value, max_stat_value,
                                      n_bins, scale_type, reset_to_default)));
    }


    for (size_t  k = 0; k < m_ProcGroupToIndex.size(); ++k) {
        m_BlobRetrieveTiming.push_back(vector<unique_ptr<CBlobRetrieveTiming>>());
        m_HugeBlobByteCounter.push_back(0);
        m_BlobByteCounters.push_back(vector<uint64_t>());
    }
    reset_to_default |= x_SetupBlobSizeBins(min_stat_value, max_stat_value,
                                            n_bins, scale_type, small_blob_size);


    for (size_t  req_index = 0;
         req_index < static_cast<size_t>(CPSGS_Request::ePSGS_UnknownRequest);
         ++req_index) {

        m_DoneProcPerformance.push_back(vector<unique_ptr<CProcessorPerformanceTiming>>());
        m_NotFoundProcPerformance.push_back(vector<unique_ptr<CProcessorPerformanceTiming>>());
        m_TimeoutProcPerformance.push_back(vector<unique_ptr<CProcessorPerformanceTiming>>());
        m_ErrorProcPerformance.push_back(vector<unique_ptr<CProcessorPerformanceTiming>>());

        for (size_t  proc_index = 0; proc_index < m_ProcGroupToIndex.size(); ++proc_index) {
            m_DoneProcPerformance.back().push_back(
                unique_ptr<CProcessorPerformanceTiming>(
                    new CProcessorPerformanceTiming(min_stat_value, max_stat_value,
                                                    n_bins, scale_type, reset_to_default)));
            m_NotFoundProcPerformance.back().push_back(
                unique_ptr<CProcessorPerformanceTiming>(
                    new CProcessorPerformanceTiming(min_stat_value, max_stat_value,
                                                    n_bins, scale_type, reset_to_default)));
            m_TimeoutProcPerformance.back().push_back(
                unique_ptr<CProcessorPerformanceTiming>(
                    new CProcessorPerformanceTiming(min_stat_value, max_stat_value,
                                                    n_bins, scale_type, reset_to_default)));
            m_ErrorProcPerformance.back().push_back(
                unique_ptr<CProcessorPerformanceTiming>(
                    new CProcessorPerformanceTiming(min_stat_value, max_stat_value,
                                                    n_bins, scale_type, reset_to_default)));
        }
    }


    if (reset_to_default)
        PSG_ERROR("Invalid statistics parameters detected. Default parameters "
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
        { "RetrieveMyNCBIFound",
          SInfo(m_RetrieveMyNCBITiming[0].get(),
                "retrieve MyNCBI found",
                "The timing of the retrieve data from MyNCBI when a record "
                "was found"
               )
        },
        { "RetrieveMyNCBIError",
          SInfo(m_RetrieveMyNCBIErrorTiming.get(),
                "MyNCBI retrieve timing in case of errors",
                "The time MyNCBI replied within when there was an error"
               )
        },
        { "RetrieveMyNCBINotFound",
          SInfo(m_RetrieveMyNCBITiming[1].get(),
                "retrieve MyNCBI not found",
                "The timing of the retrieve data from MyNCBI when there was no "
                "record found"
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
                "CassandraDB resolution succeeded",
                "The timing of a seq id successful resolution "
                "in Cassandra regardless how many queries were "
                "made to Cassandra (start: first Cassandra query)"
               )
        },
        { "ResolutionCassNotFound",
          SInfo(m_ResolutionCassTiming[1].get(),
                "CassandraDB resolution not found",
                "The timing of a seq id unsuccessful resolution "
                "when all the tries in Cassandra led to nothing "
                "start: first Cassandra query)"
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
                "The timing of a public comment retrieval "
                "when nothing was found"
               )
        },
        { "AccessionVersionHistoryRetrieveFound",
          SInfo(m_AccVerHistoryRetrieveTiming[0].get(),
                "Accession version history found",
                "The timing of an accession version history successful retrieval"
               )
        },
        { "AccessionVersionHistoryRetrieveNotFound",
          SInfo(m_AccVerHistoryRetrieveTiming[1].get(),
                "Accession version history not found",
                "The timing of an accession version history retrieval "
                "when nothing was found"
               )
        },
        { "IPGResolveRetrieveFound",
          SInfo(m_IPGResolveRetrieveTiming[0].get(),
                "IPG resolve found",
                "The timing of an ipg successful retrieval"
               )
        },
        { "IPGResolveRetrieveNotFound",
          SInfo(m_IPGResolveRetrieveTiming[1].get(),
                "IPG resolve not found",
                "The timing of an ipg resolve retrieval "
                "when nothing was found"
               )
        },
        { "VDBOpenSuccess",
          SInfo(m_VDBOpenTiming[0].get(),
                "VDB opening success",
                "The timing of the successful VDB opening"
               )
        },
        { "VDBOpenFailed",
          SInfo(m_VDBOpenTiming[1].get(),
                "VDB opening failure",
                "The timing of the failed VDB opening"
               )
        },
        { "SNPPTISLookupFound",
          SInfo(m_SNPPTISLookupTiming[0].get(),
                "SNP PTIS lookup found",
                "The timing of the SNP PTIS lookup success"
               )
        },
        { "SNPPTISLookupNotFound",
          SInfo(m_SNPPTISLookupTiming[1].get(),
                "SNP PTIS lookup not found",
                "The timing of the SNP PTIS lookup when nothing was found"
               )
        },
        { "WGSVDBLookupFound",
          SInfo(m_WGSVDBLookupTiming[0].get(),
                "WGS VDB lookup found",
                "The timing of the WGS VDB lookup success"
               )
        },
        { "WGSVDBLookupNotFound",
          SInfo(m_WGSVDBLookupTiming[1].get(),
                "WGS VDB lookup not found",
                "The timing of the WGS VDB lookup when nothing was found"
               )
        },
        { "Backlog",
          SInfo(m_BacklogTiming.get(),
                "Backlog time",
                "The time a request spent in a backlog before processing"
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

    // Per processor timing
    string      proc_group_names[m_ProcGroupToIndex.size()];
    for (const auto &  item : m_ProcGroupToIndex) {
        proc_group_names[item.second] = item.first;
    }

    for (size_t  req_index = 0;
         req_index < static_cast<size_t>(CPSGS_Request::ePSGS_UnknownRequest);
         ++req_index) {

        for (auto & item : m_ProcGroupToIndex) {
            size_t      proc_index = item.second;
            string      req_name = CPSGS_Request::TypeToString(
                                        static_cast<CPSGS_Request::EPSGS_Type>(req_index));
            string      prefix = item.first + "_" +
                                 NStr::Replace(req_name, "Request", "") + "_";
            string      suffix = "_ProcessorPerformance";

            m_NamesMap[prefix + "Done" + suffix] =
                SInfo(m_DoneProcPerformance[req_index][proc_index].get(),
                      item.first + " Done performance for " + req_name,
                      "The timing of the " + item.first +
                      " processor when it finished with status Done for " + req_name);

            m_NamesMap[prefix + "NotFound" + suffix] =
                SInfo(m_NotFoundProcPerformance[req_index][proc_index].get(),
                      item.first + " NotFound performance for " + req_name,
                      "The timing of the " + item.first +
                      " processor when it finished with status NotFound for " + req_name);

            m_NamesMap[prefix + "Timeout" + suffix] =
                SInfo(m_TimeoutProcPerformance[req_index][proc_index].get(),
                      item.first + " Timeout performance for " + req_name,
                      "The timing of the " + item.first +
                      " processor when it finished with status Timeout for " + req_name);

            m_NamesMap[prefix + "Error" + suffix] =
                SInfo(m_ErrorProcPerformance[req_index][proc_index].get(),
                      item.first + " Error performance for " + req_name,
                      "The timing of the " + item.first +
                      " processor when it finished with status Error for " + req_name);
        }
    }

    for (const auto &  proc_group : proc_group_names) {
        string      id = proc_group + "ResolutionError";
        m_ResolutionErrorTiming.push_back(
            unique_ptr<CResolutionTiming>(
                new CResolutionTiming(min_stat_value, max_stat_value,
                                      n_bins, scale_type, reset_to_default)));

        m_NamesMap[id] = SInfo(m_ResolutionErrorTiming.back().get(),
                               proc_group + " resolution error",
                               "The timing of a case when an error was detected while "
                               "resolving seq id (by " + proc_group +
                               ") regardless of the resolution path "
                               "(start: request is received)");

        id = proc_group + "ResolutionNotFound";
        m_ResolutionNotFoundTiming.push_back(
            unique_ptr<CResolutionTiming>(
                new CResolutionTiming(min_stat_value, max_stat_value,
                                      n_bins, scale_type, reset_to_default)));
        m_NamesMap[id] = SInfo(m_ResolutionNotFoundTiming.back().get(),
                               proc_group + " resolution not found",
                               "The timing of a case when resolution of a seq id did not succeed "
                               "(by " + proc_group + ") regardless of the resolution path "
                               "(start: request is received)");

        id = proc_group + "ResolutionFound";
        m_ResolutionFoundTiming.push_back(
            unique_ptr<CResolutionTiming>(
                new CResolutionTiming(min_stat_value, max_stat_value,
                                      n_bins, scale_type, reset_to_default)));
        m_NamesMap[id] = SInfo(m_ResolutionFoundTiming.back().get(),
                               proc_group + " resolution succeeded",
                               "The timing of a seq id successful resolution (by " +
                               proc_group + ") regardless of the resolution path "
                               "(start: request is received)");

        m_NARetrieveTiming.push_back(vector<unique_ptr<CNARetrieveTiming>>());
        m_NARetrieveTiming.back().push_back(
            unique_ptr<CNARetrieveTiming>(
                new CNARetrieveTiming(min_stat_value, max_stat_value,
                                      n_bins, scale_type, reset_to_default)));
        id = proc_group + "NARetrieveFound";
        m_NamesMap[id] = SInfo(m_NARetrieveTiming.back().back().get(),
                               proc_group + " named annotations found",
                               "The timing of named annotations successful retrieval (by " +
                               proc_group + ")");

        id = proc_group + "NARetrieveNotFound";
        m_NARetrieveTiming.back().push_back(
            unique_ptr<CNARetrieveTiming>(
                new CNARetrieveTiming(min_stat_value, max_stat_value,
                                      n_bins, scale_type, reset_to_default)));
        m_NamesMap[id] = SInfo(m_NARetrieveTiming.back().back().get(),
                               proc_group + " named annotations not found",
                               "The timing of named annotations retrieval "
                               "when nothing was found (by " + proc_group + ")");

        m_TSEChunkRetrieveTiming.push_back(vector<unique_ptr<CTSEChunkRetrieveTiming>>());
        m_TSEChunkRetrieveTiming.back().push_back(
            unique_ptr<CTSEChunkRetrieveTiming>(
                new CTSEChunkRetrieveTiming(min_stat_value, max_stat_value,
                                            n_bins, scale_type, reset_to_default)));
        id = proc_group + "TSEChunkRetrieveFound";
        m_NamesMap[id] = SInfo(m_TSEChunkRetrieveTiming.back().back().get(),
                               proc_group + " TSE chunk found",
                               "The timing of a TSE chunk retrieval (by " + proc_group + ")");

        m_TSEChunkRetrieveTiming.back().push_back(
            unique_ptr<CTSEChunkRetrieveTiming>(
                new CTSEChunkRetrieveTiming(min_stat_value, max_stat_value,
                                            n_bins, scale_type, reset_to_default)));
        id = proc_group + "TSEChunkRetrieveNotFound";
        m_NamesMap[id] = SInfo(m_TSEChunkRetrieveTiming.back().back().get(),
                               proc_group + " TSE chunk not found",
                               "The timing of a TSE chunk retrieval "
                               "when nothing was found (by " + proc_group + ")");

        m_NAResolveTiming.push_back(vector<unique_ptr<CNAResolveTiming>>());
        m_NAResolveTiming.back().push_back(
            unique_ptr<CNAResolveTiming>(
                new CNAResolveTiming(min_stat_value, max_stat_value,
                                     n_bins, scale_type, reset_to_default)));
        id = proc_group + "NAResolveFound";

        // NA resolve timing is only for CDD and SNP processors
        if (proc_group == "CDD" || proc_group == "SNP") {
            m_NamesMap[id] = SInfo(m_NAResolveTiming.back().back().get(),
                                   proc_group + " NA resolution found",
                                   "The timing of the NA resolution (by " + proc_group + ")");
        } else {
            m_NamesMap[id] = SInfo(nullptr, "uninitialized", "uninitialized");
        }

        m_NAResolveTiming.back().push_back(
            unique_ptr<CNAResolveTiming>(
                new CNAResolveTiming(min_stat_value, max_stat_value,
                                     n_bins, scale_type, reset_to_default)));
        id = proc_group + "NAResolveNotFound";

        // NA resolve timing is only for CDD and SNP processors
        if (proc_group == "CDD" || proc_group == "SNP") {
            m_NamesMap[id] = SInfo(m_NAResolveTiming.back().back().get(),
                                   proc_group + " NA resolution not found",
                                   "The timing of the NA resolution "
                                   "when nothing was found (by " + proc_group + ")");
        } else {
            m_NamesMap[id] = SInfo(nullptr, "uninitialized", "uninitialized");
        }

        m_HugeBlobRetrievalTiming.push_back(
            unique_ptr<CHugeBlobRetrieveTiming>(
                new CHugeBlobRetrieveTiming(min_stat_value, max_stat_value,
                                            n_bins, scale_type, reset_to_default)));
        id = proc_group + "HugeBlobRetrieval";
        m_NamesMap[id] = SInfo(m_HugeBlobRetrievalTiming.back().get(),
                               proc_group + " huge blob retrieval",
                               "The timing of the very large blob retrieval",
                               &m_HugeBlobByteCounter[m_ProcGroupToIndex[proc_group]],
                               proc_group + "HugeBlobByteCounter",
                               proc_group + " huge blob bytes counter",
                               "The number of bytes transferred to the user as very large blobs by " + proc_group
                              );

        m_NotFoundBlobRetrievalTiming.push_back(
            unique_ptr<CNotFoundBlobRetrieveTiming>(
                new CNotFoundBlobRetrieveTiming(min_stat_value, max_stat_value,
                                                n_bins, scale_type, reset_to_default)));
        id = proc_group + "BlobRetrievalNotFound";
        m_NamesMap[id] = SInfo(m_NotFoundBlobRetrievalTiming.back().get(),
                               proc_group + " blob retrieval not found",
                               "The timing of blob retrieval when a blob was not found by " + proc_group);

        size_t      index = 0;
        for (auto & retieve_timing : m_BlobRetrieveTiming[m_ProcGroupToIndex[proc_group]]) {
            string      min_size_str = to_string(retieve_timing->GetMinBlobSize());
            string      max_size_str = to_string(retieve_timing->GetMaxBlobSize());
            string      id = proc_group + "BlobRetrievalFrom" + min_size_str + "To" + max_size_str;
            string      name = proc_group + " Blob retrieval (size: " +
                            min_size_str + " to " + max_size_str + ")";
            string      description = "The timing of a blob retrieval when "
                            "the blob size is between " + min_size_str +
                            " and " + max_size_str + " bytes";

            string      counter_id = proc_group + "BlobByteCounterFrom" + min_size_str +
                                     "To" + max_size_str;
            string      counter_name = "Blob byte counter (blob size: " +
                                       min_size_str + " to " + max_size_str + ") by " + proc_group;
            string      counter_description = "The number of bytes transferred to "
                                              "the user as blobs size between " +
                                              min_size_str + " and " +
                                              max_size_str + " by " + proc_group;
            m_NamesMap[id] = SInfo(retieve_timing.get(), name, description,
                                   &m_BlobByteCounters[m_ProcGroupToIndex[proc_group]][index],
                                   counter_id, counter_name, counter_description);
            ++index;
        }

        // time series by processor
        m_IdGetDoneByProc.push_back(unique_ptr<CProcessorRequestTimeSeries>(new CProcessorRequestTimeSeries()));
        m_IdGetblobDoneByProc.push_back(unique_ptr<CProcessorRequestTimeSeries>(new CProcessorRequestTimeSeries()));
        m_IdResolveDoneByProc.push_back(unique_ptr<CProcessorRequestTimeSeries>(new CProcessorRequestTimeSeries()));
        m_IdGetTSEChunkDoneByProc.push_back(unique_ptr<CProcessorRequestTimeSeries>(new CProcessorRequestTimeSeries()));
        m_IdGetNADoneByProc.push_back(unique_ptr<CProcessorRequestTimeSeries>(new CProcessorRequestTimeSeries()));
    }


    // Overwrite the default names and descriptions with what came from
    // the configuration
    auto        app = CPubseqGatewayApp::GetInstance();
    auto        id_to_name_and_desc = app->Settings().m_IdToNameAndDescription;

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

    // Initialize the applog identifiers for the case when an operation is too
    // long
    m_TooLongIDs[eLookupLmdbSi2csi] = "lookup_lmdb_si2csi_too_long";
    m_TooLongIDs[eLookupLmdbBioseqInfo] = "lookup_lmdb_bioseq_info_too_long";
    m_TooLongIDs[eLookupLmdbBlobProp] = "lookup_lmdb_blob_prop_too_long";
    m_TooLongIDs[eLookupCassSi2csi] = "lookup_cass_si2csi_too_long";
    m_TooLongIDs[eLookupCassBioseqInfo] = "lookup_cass_bioseq_info_too_long";
    m_TooLongIDs[eLookupCassBlobProp] = "lookup_cass_blob_prop_too_long";
    m_TooLongIDs[eMyNCBIRetrieve] = "retrieve_my_ncbi_too_long";
    m_TooLongIDs[eMyNCBIRetrieveError] = "error_retrieve_my_ncbi_too_long";
    m_TooLongIDs[eResolutionLmdb] = "resolution_lmdb_too_long";
    m_TooLongIDs[eResolutionCass] = "resolution_cass_too_long";
    m_TooLongIDs[eResolutionError] = "resolution_error_too_long";
    m_TooLongIDs[eResolutionNotFound] = "resolution_not_found_too_long";
    m_TooLongIDs[eResolutionFound] = "resolution_found_too_long";
    m_TooLongIDs[eResolutionFoundInCassandra] = "resolution_found_in_cass_too_long";
    m_TooLongIDs[eBlobRetrieve] = "blob_retrieve_too_long";
    m_TooLongIDs[eNARetrieve] = "na_retrieve_too_long";
    m_TooLongIDs[eSplitHistoryRetrieve] = "split_history_retrieve_too_long";
    m_TooLongIDs[ePublicCommentRetrieve] = "public_comment_retrieve_too_long";
    m_TooLongIDs[eAccVerHistRetrieve] = "acc_ver_hist_retrieve_too_long";
    m_TooLongIDs[eIPGResolveRetrieve] = "iog_resolve_retrieve_too_long";
    m_TooLongIDs[eTseChunkRetrieve] = "tse_chunk_retrieve_too_long";
    m_TooLongIDs[eNAResolve] = "na_resolve_too_long";
    m_TooLongIDs[eVDBOpen] = "vdb_open_too_long";
    m_TooLongIDs[eBacklog] = "backlog_too_long";
    m_TooLongIDs[eSNP_PTISLookup] = "snp_ptis_lookup_too_long";
    m_TooLongIDs[eWGS_VDBLookup] = "wgs_vdb_lookup_too_long";
}


bool COperationTiming::x_SetupBlobSizeBins(unsigned long  min_stat_value,
                                           unsigned long  max_stat_value,
                                           unsigned long  n_bins,
                                           TOnePSGTiming::EScaleType  stat_type,
                                           unsigned long  small_blob_size)
{
    bool    reset_to_default = false;

    for (size_t  j = 0; j < m_ProcGroupToIndex.size(); ++j) {
        m_BlobRetrieveTiming[j].push_back(
                unique_ptr<CBlobRetrieveTiming>(
                    new CBlobRetrieveTiming(0, small_blob_size,
                                            min_stat_value, max_stat_value,
                                            n_bins, stat_type,
                                            reset_to_default)));
        m_BlobByteCounters[j].push_back(0);

        if (j == 0)
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

            m_BlobRetrieveTiming[j].push_back(
                    unique_ptr<CBlobRetrieveTiming>(
                        new CBlobRetrieveTiming(range_start, range_end,
                                                min_stat_value, max_stat_value,
                                                n_bins, stat_type,
                                                reset_to_default)));
            m_BlobByteCounters[j].push_back(0);

            if (j == 0)
                m_Ends.push_back(range_end);

            range_start = range_end + 1;
            if (range_start >= kMaxBlobSize)
                break;
            ++k;
        }

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


uint64_t COperationTiming::Register(IPSGS_Processor *  processor,
                                    EPSGOperation  operation,
                                    EPSGOperationStatus  status,
                                    const psg_time_point_t &  op_begin_ts,
                                    unsigned long  blob_size)
{
    if (!m_OnlyForProcessor.empty()) {
        // May need to skip timing collection
        if (processor != nullptr) {
            if (m_OnlyForProcessor != processor->GetGroupName()) {
                // Skipping because it is not the configured group
                return 0;
            }
        }
    }


    auto            now = psg_clock_t::now();
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
        case eMyNCBIRetrieve:
            m_RetrieveMyNCBITiming[index]->Add(mks);
            break;
        case eMyNCBIRetrieveError:
            m_RetrieveMyNCBIErrorTiming->Add(mks);
            break;
        case eResolutionLmdb:
            m_ResolutionLmdbTiming[index]->Add(mks);
            break;
        case eResolutionCass:
            m_ResolutionCassTiming[index]->Add(mks);
            break;
        case eBlobRetrieve:
            if (processor) {
                size_t      proc_index = m_ProcGroupToIndex[processor->GetGroupName()];
                if (status == eOpStatusNotFound)
                    m_NotFoundBlobRetrievalTiming[proc_index]->Add(mks);
                else {
                    ssize_t     bin_index = x_GetBlobRetrievalBinIndex(blob_size);
                    if (bin_index < 0) {
                        m_HugeBlobByteCounter[proc_index] += blob_size;
                        m_HugeBlobRetrievalTiming[proc_index]->Add(mks);
                    } else {
                        m_BlobByteCounters[proc_index][bin_index] += blob_size;
                        m_BlobRetrieveTiming[proc_index][bin_index]->Add(mks);
                    }
                }
            } else {
                PSG_ERROR("No processor for per processor timing (" +
                          to_string(eBlobRetrieve) + "). Ignore and continue.");
            }
            break;
        case eNARetrieve:
            if (processor)
                m_NARetrieveTiming[m_ProcGroupToIndex[processor->GetGroupName()]][index]->Add(mks);
            else
                PSG_ERROR("No processor for per processor timing (" +
                          to_string(eNARetrieve) + "). Ignore and continue.");
            break;
        case eSplitHistoryRetrieve:
            m_SplitHistoryRetrieveTiming[index]->Add(mks);
            break;
        case ePublicCommentRetrieve:
            m_PublicCommentRetrieveTiming[index]->Add(mks);
            break;
        case eAccVerHistRetrieve:
            m_AccVerHistoryRetrieveTiming[index]->Add(mks);
            break;
        case eIPGResolveRetrieve:
            m_IPGResolveRetrieveTiming[index]->Add(mks);
            break;
        case eResolutionError:
            if (processor)
                m_ResolutionErrorTiming[m_ProcGroupToIndex[processor->GetGroupName()]]->Add(mks);
            else
                PSG_ERROR("No processor for per processor timing (" +
                          to_string(eResolutionError) + "). Ignore and continue.");
            break;
        case eResolutionNotFound:
            if (processor)
                m_ResolutionNotFoundTiming[m_ProcGroupToIndex[processor->GetGroupName()]]->Add(mks);
            else
                PSG_ERROR("No processor for per processor timing (" +
                          to_string(eResolutionNotFound) + "). Ignore and continue.");
            break;
        case eResolutionFound:
            if (processor)
                m_ResolutionFoundTiming[m_ProcGroupToIndex[processor->GetGroupName()]]->Add(mks);
            else
                PSG_ERROR("No processor for per processor timing (" +
                          to_string(eResolutionFound) + "). Ignore and continue.");
            break;
        case eBacklog:
            m_BacklogTiming->Add(mks);
            break;
        case eResolutionFoundInCassandra:
            // The blob_size here is the number of queries of Cassandra
            index = blob_size - 1;
            if (index > 4)
                index = 4;
            m_ResolutionFoundCassandraTiming[index]->Add(mks);
            break;
        case eTseChunkRetrieve:
            if (processor)
                m_TSEChunkRetrieveTiming[m_ProcGroupToIndex[processor->GetGroupName()]][index]->Add(mks);
            else
                PSG_ERROR("No processor for per processor timing (" +
                          to_string(eTseChunkRetrieve) + "). Ignore and continue.");
            break;
        case eNAResolve:
            if (processor) {
                string      group_name = processor->GetGroupName();
                if (group_name == "CDD" || group_name == "SNP") {
                    m_NAResolveTiming[m_ProcGroupToIndex[group_name]][index]->Add(mks);
                } else {
                    PSG_ERROR("NA resolve timing is supported for "
                              "CDD and SNP only. Received from: " + group_name);
                }
            } else {
                PSG_ERROR("No processor for per processor timing (" +
                          to_string(eNAResolve) + "). Ignore and continue.");
            }
            break;
        case eVDBOpen:
            m_VDBOpenTiming[index]->Add(mks);
            break;
        case eSNP_PTISLookup:
            m_SNPPTISLookupTiming[index]->Add(mks);
            break;
        case eWGS_VDBLookup:
            m_WGSVDBLookupTiming[index]->Add(mks);
            break;
        default:
            break;
    }

    // NOTE: backlog time is not going to be logged. The reason is that the
    // processor pointer is not available at the moment of registering the
    // backlog time. Thus 'backlog_time_mks' is registerd outside of this
    // method at CHttpConnection::x_MaintainBacklog and it will be logged (if
    // log is switched on) regardless of the threshold.
    if (m_LogTimingThresholdMks > 0) {
        if (mks > m_LogTimingThresholdMks) {
            if (processor != nullptr) {
                auto        app = CPubseqGatewayApp::GetInstance();
                app->GetCounters().Increment(processor,
                                             CPSGSCounters::ePSGS_OpTooLong);

                auto    request = processor->GetRequest();

                CRequestContextResetter     context_resetter;
                request->SetRequestContext();

                GetDiagContext().Extra().Print("op_too_long", mks)
                                        .Print(m_TooLongIDs[operation], mks)
                                        .Print("processor", processor->GetGroupName());
            }
        }
    }

    return mks;
}


void COperationTiming::RegisterForTimeSeries(
                            CPSGS_Request::EPSGS_Type  request_type,
                            CRequestStatus::ECode  status)
{
    if (status == CRequestStatus::e500_InternalServerError ||
        status == CRequestStatus::e501_NotImplemented ||
        status == CRequestStatus::e502_BadGateway ||
        status == CRequestStatus::e503_ServiceUnavailable ||
        status == CRequestStatus::e504_GatewayTimeout ||
        status == CRequestStatus::e505_HTTPVerNotSupported) {
        // The same condition GRID Dashboard uses
        m_ErrorTimeSeries.Add();
    }

    if (request_type == CPSGS_Request::ePSGS_UnknownRequest)
        return;

    auto                counter = CRequestTimeSeries::RequestStatusToCounter(status);

    switch (request_type) {
        case CPSGS_Request::ePSGS_ResolveRequest:
            m_IdResolveStat.Add(counter);
            break;
        case CPSGS_Request::ePSGS_BlobBySeqIdRequest:
            m_IdGetStat.Add(counter);
            break;
        case CPSGS_Request::ePSGS_BlobBySatSatKeyRequest:
            m_IdGetblobStat.Add(counter);
            break;
        case CPSGS_Request::ePSGS_AnnotationRequest:
            m_IdGetNAStat.Add(counter);
            break;
        case CPSGS_Request::ePSGS_TSEChunkRequest:
            m_IdGetTSEChunkStat.Add(counter);
            break;
        case CPSGS_Request::ePSGS_AccessionVersionHistoryRequest:
            m_IdAccVerHistStat.Add(counter);
            break;
        case CPSGS_Request::ePSGS_IPGResolveRequest:
            m_IpgResolveStat.Add(counter);
            break;
        default:
            break;
    }
}


void COperationTiming::RegisterProcessorDone(CPSGS_Request::EPSGS_Type  request_type,
                                             IPSGS_Processor *  processor)
{
    switch (request_type) {
        case CPSGS_Request::ePSGS_ResolveRequest:
            m_IdResolveDoneByProc[m_ProcGroupToIndex[processor->GetGroupName()]]->Add();
            break;
        case CPSGS_Request::ePSGS_BlobBySeqIdRequest:
            m_IdGetDoneByProc[m_ProcGroupToIndex[processor->GetGroupName()]]->Add();
            break;
        case CPSGS_Request::ePSGS_BlobBySatSatKeyRequest:
            m_IdGetblobDoneByProc[m_ProcGroupToIndex[processor->GetGroupName()]]->Add();
            break;
        case CPSGS_Request::ePSGS_AnnotationRequest:
            m_IdGetNADoneByProc[m_ProcGroupToIndex[processor->GetGroupName()]]->Add();
            break;
        case CPSGS_Request::ePSGS_TSEChunkRequest:
            m_IdGetTSEChunkDoneByProc[m_ProcGroupToIndex[processor->GetGroupName()]]->Add();
            break;

        // The requests below are specific for cassandra so there is no
        // separate collection
        case CPSGS_Request::ePSGS_AccessionVersionHistoryRequest:
        case CPSGS_Request::ePSGS_IPGResolveRequest:
        default:
            break;
    }
}


void COperationTiming::RegisterProcessorPerformance(IPSGS_Processor *  processor,
                                                    IPSGS_Processor::EPSGS_Status  proc_finish_status)
{
    bool                valid;
    psg_time_point_t    start_timestamp = processor->GetProcessInvokeTimestamp(valid);

    if (!valid)
        return;     // Should not really happened
                    // the start timestamp is memorized unconditionally

    size_t  request_index = static_cast<size_t>(processor->GetRequest()->GetRequestType());
    size_t  proc_index = m_ProcGroupToIndex[processor->GetGroupName()];


    auto            now = psg_clock_t::now();
    uint64_t        mks = chrono::duration_cast<chrono::microseconds>(now - start_timestamp).count();

    switch (proc_finish_status) {
        case IPSGS_Processor::ePSGS_InProgress:
            // Cannot happened; the processor has not finished yet while this
            // method is for the moment when a processor is done.
            break;
        case IPSGS_Processor::ePSGS_Done:
            m_DoneProcPerformance[request_index][proc_index]->Add(mks);
            break;
        case IPSGS_Processor::ePSGS_NotFound:
            m_NotFoundProcPerformance[request_index][proc_index]->Add(mks);
            break;
        case IPSGS_Processor::ePSGS_Canceled:
            // Not collected
            break;
        case IPSGS_Processor::ePSGS_Timeout:
            m_TimeoutProcPerformance[request_index][proc_index]->Add(mks);
            break;
        case IPSGS_Processor::ePSGS_Error:
            m_ErrorProcPerformance[request_index][proc_index]->Add(mks);
            break;
        case IPSGS_Processor::ePSGS_Unauthorized:
            // Not collected
            break;
        default:
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
        m_RetrieveMyNCBITiming[k]->Rotate();
        m_ResolutionLmdbTiming[k]->Rotate();
        m_ResolutionCassTiming[k]->Rotate();
        m_SplitHistoryRetrieveTiming[k]->Rotate();
        m_PublicCommentRetrieveTiming[k]->Rotate();
        m_AccVerHistoryRetrieveTiming[k]->Rotate();
        m_IPGResolveRetrieveTiming[k]->Rotate();
        m_VDBOpenTiming[k]->Rotate();
        m_SNPPTISLookupTiming[k]->Rotate();
        m_WGSVDBLookupTiming[k]->Rotate();
    }

    for (size_t  k = 0; k < m_NARetrieveTiming.size(); ++k) {
        for (size_t  m = 0; m < m_NARetrieveTiming[k].size(); ++m) {
            m_NARetrieveTiming[k][m]->Rotate();
        }
    }

    for (size_t  k = 0; k < m_TSEChunkRetrieveTiming.size(); ++k) {
        for (size_t  m = 0; m < m_TSEChunkRetrieveTiming[k].size(); ++m) {
            m_TSEChunkRetrieveTiming[k][m]->Rotate();
        }
    }

    for (size_t  k = 0; k < m_NAResolveTiming.size(); ++k) {
        for (size_t  m = 0; m < m_NAResolveTiming[k].size(); ++m) {
            m_NAResolveTiming[k][m]->Rotate();
        }
    }

    for (auto &  item : m_HugeBlobRetrievalTiming)
        item->Rotate();

    for (auto &  item : m_NotFoundBlobRetrievalTiming)
        item->Rotate();

    for (auto &  item : m_ResolutionErrorTiming)
        item->Rotate();

    for (auto &  item : m_ResolutionNotFoundTiming)
        item->Rotate();

    for (auto &  item : m_ResolutionFoundTiming)
        item->Rotate();

    m_BacklogTiming->Rotate();
    m_RetrieveMyNCBIErrorTiming->Rotate();

    for (auto &  item : m_ResolutionFoundCassandraTiming)
        item->Rotate();

    for (size_t  k=0; k < m_BlobRetrieveTiming.size(); ++k) {
        for (auto &  item : m_BlobRetrieveTiming[k]) {
            item->Rotate();
        }
    }

    for (size_t  req_index = 0;
         req_index < static_cast<size_t>(CPSGS_Request::ePSGS_UnknownRequest);
         ++req_index) {
        for (size_t  proc_index = 0; proc_index < m_ProcGroupToIndex.size(); ++proc_index) {
            m_DoneProcPerformance[req_index][proc_index]->Rotate();
            m_NotFoundProcPerformance[req_index][proc_index]->Rotate();
            m_TimeoutProcPerformance[req_index][proc_index]->Rotate();
            m_ErrorProcPerformance[req_index][proc_index]->Rotate();
        }
    }
}


void COperationTiming::RotateRequestStat(void)
{
    // Once per minute rotate
    m_IdGetStat.Rotate();
    m_IdGetblobStat.Rotate();
    m_IdResolveStat.Rotate();
    m_IdAccVerHistStat.Rotate();
    m_IdGetTSEChunkStat.Rotate();
    m_IdGetNAStat.Rotate();
    m_IpgResolveStat.Rotate();

    for (auto &  item : m_IdGetDoneByProc)
        item->Rotate();
    for (auto &  item : m_IdGetblobDoneByProc)
        item->Rotate();
    for (auto &  item : m_IdResolveDoneByProc)
        item->Rotate();
    for (auto &  item : m_IdGetTSEChunkDoneByProc)
        item->Rotate();
    for (auto &  item : m_IdGetNADoneByProc)
        item->Rotate();

    m_TCPConnectionsStat.Rotate();
    m_ActiveRequestsStat.Rotate();
    m_BacklogStat.Rotate();
    m_ErrorTimeSeries.Rotate();
}


void COperationTiming::RotateAvgPerfTimeSeries(void)
{
    lock_guard<mutex>   guard(m_Lock);

    for (size_t  k = 0; k <= 1; ++k) {
        m_LookupLmdbSi2csiTiming[k]->RotateAvgTimeSeries();
        m_LookupLmdbBioseqInfoTiming[k]->RotateAvgTimeSeries();
        m_LookupLmdbBlobPropTiming[k]->RotateAvgTimeSeries();
        m_LookupCassSi2csiTiming[k]->RotateAvgTimeSeries();
        m_LookupCassBioseqInfoTiming[k]->RotateAvgTimeSeries();
        m_LookupCassBlobPropTiming[k]->RotateAvgTimeSeries();
        m_RetrieveMyNCBITiming[k]->RotateAvgTimeSeries();
        m_ResolutionLmdbTiming[k]->RotateAvgTimeSeries();
        m_ResolutionCassTiming[k]->RotateAvgTimeSeries();
        m_SplitHistoryRetrieveTiming[k]->RotateAvgTimeSeries();
        m_PublicCommentRetrieveTiming[k]->RotateAvgTimeSeries();
        m_AccVerHistoryRetrieveTiming[k]->RotateAvgTimeSeries();
        m_IPGResolveRetrieveTiming[k]->RotateAvgTimeSeries();
        m_VDBOpenTiming[k]->RotateAvgTimeSeries();
        m_SNPPTISLookupTiming[k]->RotateAvgTimeSeries();
        m_WGSVDBLookupTiming[k]->RotateAvgTimeSeries();
    }

    for (size_t  k = 0; k < m_NARetrieveTiming.size(); ++k) {
        for (size_t  m = 0; m < m_NARetrieveTiming[k].size(); ++m) {
            m_NARetrieveTiming[k][m]->RotateAvgTimeSeries();
        }
    }

    for (size_t  k = 0; k < m_TSEChunkRetrieveTiming.size(); ++k) {
        for (size_t  m = 0; m < m_TSEChunkRetrieveTiming[k].size(); ++m) {
            m_TSEChunkRetrieveTiming[k][m]->RotateAvgTimeSeries();
        }
    }

    for (size_t  k = 0; k < m_NAResolveTiming.size(); ++k) {
        for (size_t  m = 0; m < m_NAResolveTiming[k].size(); ++m) {
            m_NAResolveTiming[k][m]->RotateAvgTimeSeries();
        }
    }

    for (auto &  item : m_HugeBlobRetrievalTiming)
        item->RotateAvgTimeSeries();

    for (auto &  item : m_NotFoundBlobRetrievalTiming)
        item->RotateAvgTimeSeries();

    for (auto &  item : m_ResolutionErrorTiming)
        item->RotateAvgTimeSeries();

    for (auto &  item : m_ResolutionNotFoundTiming)
        item->RotateAvgTimeSeries();

    for (auto &  item : m_ResolutionFoundTiming)
        item->RotateAvgTimeSeries();

    m_BacklogTiming->RotateAvgTimeSeries();
    m_RetrieveMyNCBIErrorTiming->RotateAvgTimeSeries();

    for (auto &  item : m_ResolutionFoundCassandraTiming)
        item->RotateAvgTimeSeries();

    for (size_t  k=0; k < m_BlobRetrieveTiming.size(); ++k) {
        for (auto &  item : m_BlobRetrieveTiming[k]) {
            item->RotateAvgTimeSeries();
        }
    }

    for (size_t  req_index = 0;
         req_index < static_cast<size_t>(CPSGS_Request::ePSGS_UnknownRequest);
         ++req_index) {
        for (size_t  proc_index = 0; proc_index < m_ProcGroupToIndex.size(); ++proc_index) {
            m_DoneProcPerformance[req_index][proc_index]->RotateAvgTimeSeries();
            m_NotFoundProcPerformance[req_index][proc_index]->RotateAvgTimeSeries();
            m_TimeoutProcPerformance[req_index][proc_index]->RotateAvgTimeSeries();
            m_ErrorProcPerformance[req_index][proc_index]->RotateAvgTimeSeries();
        }
    }
}


void COperationTiming::CollectMomentousStat(size_t  tcp_conn_count,
                                            size_t  active_request_count,
                                            size_t  backlog_count)
{
    // Once per 5 seconds collect
    m_TCPConnectionsStat.Add(tcp_conn_count);
    m_ActiveRequestsStat.Add(active_request_count);
    m_BacklogStat.Add(backlog_count);
}


void COperationTiming::Reset(void)
{
    {
        // The code is in the block to unlock a bit earlier
        lock_guard<mutex>   guard(m_Lock);

        for (size_t  k = 0; k <= 1; ++k) {
            m_LookupLmdbSi2csiTiming[k]->Reset();
            m_LookupLmdbBioseqInfoTiming[k]->Reset();
            m_LookupLmdbBlobPropTiming[k]->Reset();
            m_LookupCassSi2csiTiming[k]->Reset();
            m_LookupCassBioseqInfoTiming[k]->Reset();
            m_LookupCassBlobPropTiming[k]->Reset();
            m_RetrieveMyNCBITiming[k]->Reset();
            m_ResolutionLmdbTiming[k]->Reset();
            m_ResolutionCassTiming[k]->Reset();
            m_SplitHistoryRetrieveTiming[k]->Reset();
            m_PublicCommentRetrieveTiming[k]->Reset();
            m_AccVerHistoryRetrieveTiming[k]->Reset();
            m_IPGResolveRetrieveTiming[k]->Reset();
            m_VDBOpenTiming[k]->Reset();
            m_SNPPTISLookupTiming[k]->Reset();
            m_WGSVDBLookupTiming[k]->Reset();
        }

        for (size_t  k = 0; k < m_NARetrieveTiming.size(); ++k) {
            for (size_t  m = 0; m < m_NARetrieveTiming[k].size(); ++ m) {
                m_NARetrieveTiming[k][m]->Reset();
            }
        }

        for (size_t  k = 0; k < m_TSEChunkRetrieveTiming.size(); ++k) {
            for (size_t  m = 0; m < m_TSEChunkRetrieveTiming[k].size(); ++m) {
                m_TSEChunkRetrieveTiming[k][m]->Reset();
            }
        }

        for (size_t  k = 0; k < m_NAResolveTiming.size(); ++k) {
            for (size_t  m = 0; m < m_NAResolveTiming[k].size(); ++m) {
                m_NAResolveTiming[k][m]->Reset();
            }
        }

        for (auto &  item : m_HugeBlobRetrievalTiming)
            item->Reset();

        for (auto &  item : m_NotFoundBlobRetrievalTiming)
            item->Reset();

        for (auto &  item : m_ResolutionErrorTiming)
            item->Reset();

        for (auto &  item : m_ResolutionNotFoundTiming)
            item->Reset();

        for (auto &  item : m_ResolutionFoundTiming)
            item->Reset();

        m_BacklogTiming->Reset();
        m_RetrieveMyNCBIErrorTiming->Reset();

        for (auto &  item : m_ResolutionFoundCassandraTiming)
            item->Reset();

        for (size_t  k=0; k < m_BlobRetrieveTiming.size(); ++k) {
            for (auto &  item : m_BlobRetrieveTiming[k]) {
                item->Reset();
            }
        }

        for (size_t  k=0; k < m_BlobByteCounters.size(); ++k) {
            for (auto &  item : m_BlobByteCounters[k]) {
                item = 0;
            }
        }

        for (size_t  k=0; k < m_HugeBlobByteCounter.size(); ++k)
            m_HugeBlobByteCounter[k] = 0;
    }

    m_IdGetStat.Reset();
    m_IdGetblobStat.Reset();
    m_IdResolveStat.Reset();
    m_IdAccVerHistStat.Reset();
    m_IdGetTSEChunkStat.Reset();
    m_IdGetNAStat.Reset();
    m_IpgResolveStat.Reset();

    m_TCPConnectionsStat.Reset();
    m_ActiveRequestsStat.Reset();
    m_BacklogStat.Reset();
    m_ErrorTimeSeries.Reset();

    for (auto &  item : m_IdGetDoneByProc)
        item->Reset();
    for (auto &  item : m_IdGetblobDoneByProc)
        item->Reset();
    for (auto &  item : m_IdResolveDoneByProc)
        item->Reset();
    for (auto &  item : m_IdGetTSEChunkDoneByProc)
        item->Reset();
    for (auto &  item : m_IdGetNADoneByProc)
        item->Reset();

    for (size_t  req_index = 0;
         req_index < static_cast<size_t>(CPSGS_Request::ePSGS_UnknownRequest);
         ++req_index) {
        for (size_t  proc_index = 0; proc_index < m_ProcGroupToIndex.size(); ++proc_index) {
            m_DoneProcPerformance[req_index][proc_index]->Reset();
            m_NotFoundProcPerformance[req_index][proc_index]->Reset();
            m_TimeoutProcPerformance[req_index][proc_index]->Reset();
            m_ErrorProcPerformance[req_index][proc_index]->Reset();
        }
    }
}


static string   kSecondsCovered("SecondsCovered");

CJsonNode
COperationTiming::Serialize(int  most_ancient_time,
                            int  most_recent_time,
                            const vector<CTempString> &  histogram_names,
                            const vector<pair<int, int>> &  time_series,
                            unsigned long  tick_span) const
{
    CJsonNode       ret(CJsonNode::NewObjectNode());

    // The collection of data switches to the next slot almost
    // synchronously so it is possible to get the current index and
    // loop from one of them
    bool        loop;
    size_t      current_index;
    m_IdGetStat.GetLoopAndIndex(loop, current_index);

    // All the histograms have the same number of covered ticks
    ret.SetInteger(kSecondsCovered,
                   tick_span * m_BacklogTiming->GetNumberOfCoveredTicks());

    if (histogram_names.empty()) {
        {
            // The code is in a block to unlock a bit earlier
            lock_guard<mutex>       guard(m_Lock);

            for (const auto &  name_to_histogram : m_NamesMap) {
                if (name_to_histogram.second.m_Timing == nullptr) {
                    // Some items need to skip, e.g. NA resolve is collected
                    // only for CDD and SNP processors
                    continue;
                }
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

                if (!time_series.empty()) {
                    ret.SetByKey(
                        name_to_histogram.first + "_AvgTimeSeries",
                        name_to_histogram.second.m_Timing->SerializeAvgPerfSeries(
                                time_series,
                                most_ancient_time,
                                most_recent_time,
                                loop, current_index));
                }
            }
        }

        // The time series needs to be serialized only if the container is not
        // empty.
        if (!time_series.empty()) {
            ret.SetByKey("ID_get_time_series",
                         m_IdGetStat.Serialize(time_series,
                                               most_ancient_time,
                                               most_recent_time,
                                               loop, current_index));
            ret.SetByKey("ID_getblob_time_series",
                         m_IdGetblobStat.Serialize(time_series,
                                                   most_ancient_time,
                                                   most_recent_time,
                                                   loop, current_index));
            ret.SetByKey("ID_resolve_time_series",
                         m_IdResolveStat.Serialize(time_series,
                                                   most_ancient_time,
                                                   most_recent_time,
                                                   loop, current_index));
            ret.SetByKey("ID_acc_ver_hist_time_series",
                         m_IdAccVerHistStat.Serialize(time_series,
                                                      most_ancient_time,
                                                      most_recent_time,
                                                      loop, current_index));
            ret.SetByKey("ID_get_tse_chunk_time_series",
                         m_IdGetTSEChunkStat.Serialize(time_series,
                                                       most_ancient_time,
                                                       most_recent_time,
                                                       loop, current_index));
            ret.SetByKey("ID_get_na_time_series",
                         m_IdGetNAStat.Serialize(time_series,
                                                 most_ancient_time,
                                                 most_recent_time,
                                                 loop, current_index));
            ret.SetByKey("IPG_resolve_time_series",
                         m_IpgResolveStat.Serialize(time_series,
                                                    most_ancient_time,
                                                    most_recent_time,
                                                    loop, current_index));

            bool        momentous_cnt_loop;
            size_t      momentous_cnt_current_index;
            m_TCPConnectionsStat.GetLoopAndIndex(momentous_cnt_loop, momentous_cnt_current_index);
            ret.SetByKey("TCP_connections_time_series",
                         m_TCPConnectionsStat.Serialize(time_series,
                                                        most_ancient_time,
                                                        most_recent_time,
                                                        momentous_cnt_loop,
                                                        momentous_cnt_current_index));
            ret.SetByKey("active_requests_time_series",
                         m_ActiveRequestsStat.Serialize(time_series,
                                                        most_ancient_time,
                                                        most_recent_time,
                                                        momentous_cnt_loop,
                                                        momentous_cnt_current_index));
            ret.SetByKey("backlog_time_series",
                         m_BacklogStat.Serialize(time_series,
                                                 most_ancient_time,
                                                 most_recent_time,
                                                 momentous_cnt_loop,
                                                 momentous_cnt_current_index));

            // Here: need to calculate the max of requests separately for each
            // kind of response: ok, error, warning, not found
            // Note: the logic of the raw data index is the same as in the
            // serialization above
            size_t      raw_index;
            if (current_index == 0) {
                raw_index = kSeriesIntervals - 1;
                loop = false;   // to avoid going the second time over
            } else {
                raw_index = current_index - 1;
            }

            uint64_t    max_requests = 0;
            uint64_t    max_errors = 0;
            uint64_t    max_warnings = 0;
            uint64_t    max_not_found = 0;
            for (;;) {
                x_UpdateMaxReqsStat(raw_index, max_requests, max_errors,
                                    max_warnings, max_not_found);
                if (raw_index == 0)
                    break;
                --raw_index;
            }

            if (loop) {
                raw_index = kSeriesIntervals - 1;
                while (raw_index > current_index + 1) {
                    x_UpdateMaxReqsStat(raw_index, max_requests, max_errors,
                                        max_warnings, max_not_found);
                    --raw_index;
                }
            }

            // Add the max requests (converted to per sec) to the serialized
            // data
            ret.SetDouble("MaxPerSec_Requests", max_requests / 60.0);
            ret.SetDouble("MaxPerSec_Errors", max_errors / 60.0);
            ret.SetDouble("MaxPerSec_Warnings", max_warnings / 60.0);
            ret.SetDouble("MaxPerSec_NotFound", max_not_found / 60.0);


            // Serialize per processor handled requests
            for (auto & item : m_ProcGroupToIndex) {
                ret.SetByKey(item.first + "_ID_get_time_series",
                             m_IdGetDoneByProc[item.second]->Serialize(time_series,
                                                                       most_ancient_time,
                                                                       most_recent_time,
                                                                       loop, current_index));
                ret.SetByKey(item.first + "_ID_getblob_time_series",
                             m_IdGetblobDoneByProc[item.second]->Serialize(time_series,
                                                                           most_ancient_time,
                                                                           most_recent_time,
                                                                           loop, current_index));
                ret.SetByKey(item.first + "_ID_resolve_time_series",
                             m_IdResolveDoneByProc[item.second]->Serialize(time_series,
                                                                           most_ancient_time,
                                                                           most_recent_time,
                                                                           loop, current_index));
                ret.SetByKey(item.first + "_ID_get_tse_chunk_time_series",
                             m_IdGetTSEChunkDoneByProc[item.second]->Serialize(time_series,
                                                                               most_ancient_time,
                                                                               most_recent_time,
                                                                               loop, current_index));
                ret.SetByKey(item.first + "_ID_get_na_time_series",
                             m_IdGetNADoneByProc[item.second]->Serialize(time_series,
                                                                         most_ancient_time,
                                                                         most_recent_time,
                                                                         loop, current_index));
            }
            ret.SetByKey("error_time_series",
                         m_ErrorTimeSeries.Serialize(time_series,
                                                     most_ancient_time,
                                                     most_recent_time,
                                                     loop, current_index));
        }
    } else {
        lock_guard<mutex>       guard(m_Lock);

        for (const auto &  name : histogram_names) {
            string      histogram_name(name.data(), name.size());
            const auto  iter = m_NamesMap.find(histogram_name);
            if (iter != m_NamesMap.end()) {
                if (iter->second.m_Timing == nullptr) {
                    // Some items need to be skipped, e.g. NA resolve is
                    // collected only for CDD and SNP processors
                    continue;
                }
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


void
COperationTiming::x_UpdateMaxReqsStat(size_t  index,
                                      uint64_t &  max_requests,
                                      uint64_t &  max_errors,
                                      uint64_t &  max_warnings,
                                      uint64_t &  max_not_found) const
{
    uint64_t    requests = 0;
    uint64_t    errors = 0;
    uint64_t    warnings = 0;
    uint64_t    not_found = 0;

    m_IdGetStat.AppendData(index, requests, errors, warnings, not_found);
    m_IdGetblobStat.AppendData(index, requests, errors, warnings, not_found);
    m_IdResolveStat.AppendData(index, requests, errors, warnings, not_found);
    m_IdAccVerHistStat.AppendData(index, requests, errors, warnings, not_found);
    m_IdGetTSEChunkStat.AppendData(index, requests, errors, warnings, not_found);
    m_IdGetNAStat.AppendData(index, requests, errors, warnings, not_found);
    m_IpgResolveStat.AppendData(index, requests, errors, warnings, not_found);

    max_requests = max(max_requests, requests);
    max_errors = max(max_errors, errors);
    max_warnings = max(max_warnings, warnings);
    max_not_found = max(max_not_found, not_found);
}

