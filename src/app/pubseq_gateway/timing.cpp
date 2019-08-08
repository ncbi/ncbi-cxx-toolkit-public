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


const unsigned long   kMaxBlobSize = 1024L*1024L*1024L*8L;   // 8 GB


CJsonNode CPSGTimingBase::Serialize(void) const
{
    static string   kBins("Bins");
    static string   kStart("Start");
    static string   kEnd("End");
    static string   kCount("Count");
    static string   kLowerAnomaly("LowerAnomaly");
    static string   kUpperAnomaly("UpperAnomaly");
    static string   kTotalCount("TotalCount");

    CJsonNode       ret(CJsonNode::NewObjectNode());
    CJsonNode       bins(CJsonNode::NewArrayNode());

    size_t          bin_count =  m_PSGTiming->GetNumberOfBins();
    size_t          last_bin_index = bin_count - 1;
    auto            starts = m_PSGTiming->GetBinStarts();
    auto            counters = m_PSGTiming->GetBinCounters();
    for (size_t  k = 0; k < bin_count; ++k) {
        CJsonNode   bin(CJsonNode::NewObjectNode());
        bin.SetInteger(kStart, starts[k]);
        if (k >= last_bin_index)
            bin.SetInteger(kEnd, m_PSGTiming->GetMax());
        else
            bin.SetInteger(kEnd, starts[k+1] - 1);
        bin.SetInteger(kCount, counters[k]);

        bins.Append(bin);
    }
    ret.SetByKey(kBins, bins);

    // GetCount() does not include anomalies!
    auto    lower_anomalies = m_PSGTiming->GetLowerAnomalyCount();
    auto    upper_anomalies = m_PSGTiming->GetUpperAnomalyCount();
    ret.SetInteger(kLowerAnomaly, lower_anomalies);
    ret.SetInteger(kUpperAnomaly, upper_anomalies);
    ret.SetInteger(kTotalCount, m_PSGTiming->GetCount() +
                                lower_anomalies + upper_anomalies);
    return ret;
}


CLmdbCacheTiming::CLmdbCacheTiming(unsigned long  min_stat_value,
                                   unsigned long  max_stat_value,
                                   unsigned long  n_bins,
                                   TPSGTiming::EScaleType  stat_type,
                                   bool &  reset_to_default)
{
    reset_to_default = false;

    try {
        m_PSGTiming.reset(new TPSGTiming(min_stat_value, max_stat_value,
                                         n_bins, stat_type));
    } catch (...) {
        reset_to_default = true;
        m_PSGTiming.reset(new TPSGTiming(kMinStatValue,
                                         kMaxStatValue,
                                         kNStatBins,
                                         TPSGTiming::eLog2));
    }
}


CLmdbResolutionTiming::CLmdbResolutionTiming(unsigned long  min_stat_value,
                                             unsigned long  max_stat_value,
                                             unsigned long  n_bins,
                                             TPSGTiming::EScaleType  stat_type,
                                             bool &  reset_to_default)
{
    reset_to_default = false;

    try {
        m_PSGTiming.reset(new TPSGTiming(min_stat_value, max_stat_value,
                                         n_bins, stat_type));
    } catch (...) {
        reset_to_default = true;
        m_PSGTiming.reset(new TPSGTiming(kMinStatValue,
                                         kMaxStatValue,
                                         kNStatBins,
                                         TPSGTiming::eLog2));
    }
}


CCassTiming::CCassTiming(unsigned long  min_stat_value,
                         unsigned long  max_stat_value,
                         unsigned long  n_bins,
                         TPSGTiming::EScaleType  stat_type,
                         bool &  reset_to_default)
{
    reset_to_default = false;

    try {
        m_PSGTiming.reset(new TPSGTiming(min_stat_value, max_stat_value,
                                         n_bins, stat_type));
    } catch (...) {
        reset_to_default = true;
        m_PSGTiming.reset(new TPSGTiming(kMinStatValue,
                                         kMaxStatValue,
                                         kNStatBins,
                                         TPSGTiming::eLog2));
    }
}


CCassResolutionTiming::CCassResolutionTiming(unsigned long  min_stat_value,
                                             unsigned long  max_stat_value,
                                             unsigned long  n_bins,
                                             TPSGTiming::EScaleType  stat_type,
                                             bool &  reset_to_default)
{
    reset_to_default = false;

    try {
        m_PSGTiming.reset(new TPSGTiming(min_stat_value, max_stat_value,
                                         n_bins, stat_type));
    } catch (...) {
        reset_to_default = true;
        m_PSGTiming.reset(new TPSGTiming(kMinStatValue,
                                         kMaxStatValue,
                                         kNStatBins,
                                         TPSGTiming::eLog2));
    }
}


CBlobRetrieveTiming::CBlobRetrieveTiming(size_t  min_blob_size,
                                         size_t  max_blob_size,
                                         unsigned long  min_stat_value,
                                         unsigned long  max_stat_value,
                                         unsigned long  n_bins,
                                         TPSGTiming::EScaleType  stat_type,
                                         bool &  reset_to_default) :
    m_MinBlobSize(min_blob_size), m_MaxBlobSize(max_blob_size)
{
    reset_to_default = false;

    try {
        m_PSGTiming.reset(new TPSGTiming(min_stat_value, max_stat_value,
                                         n_bins, stat_type));
    } catch (...) {
        reset_to_default = true;
        m_PSGTiming.reset(new TPSGTiming(kMinStatValue,
                                         kMaxStatValue,
                                         kNStatBins,
                                         TPSGTiming::eLog2));
    }
}


CJsonNode CBlobRetrieveTiming::Serialize(void) const
{
    static string   kMinBlobSize("MinBlobSize");
    static string   kMaxBlobSize("MaxBlobSize");

    CJsonNode       timing = CPSGTimingBase::Serialize();
    timing.SetInteger(kMinBlobSize, m_MinBlobSize);
    timing.SetInteger(kMaxBlobSize, m_MaxBlobSize);
    return timing;
}


CHugeBlobRetrieveTiming::CHugeBlobRetrieveTiming(
        unsigned long  min_stat_value,
        unsigned long  max_stat_value,
        unsigned long  n_bins,
        TPSGTiming::EScaleType  stat_type,
        bool &  reset_to_default)
{
    reset_to_default = false;

    try {
        m_PSGTiming.reset(new TPSGTiming(min_stat_value, max_stat_value,
                                         n_bins, stat_type));
    } catch (...) {
        reset_to_default = true;
        m_PSGTiming.reset(new TPSGTiming(kMinStatValue,
                                         kMaxStatValue,
                                         kNStatBins,
                                         TPSGTiming::eLog2));
    }
}


CNotFoundBlobRetrieveTiming::CNotFoundBlobRetrieveTiming(
        unsigned long  min_stat_value,
        unsigned long  max_stat_value,
        unsigned long  n_bins,
        TPSGTiming::EScaleType  stat_type,
        bool &  reset_to_default)
{
    reset_to_default = false;

    try {
        m_PSGTiming.reset(new TPSGTiming(min_stat_value, max_stat_value,
                                         n_bins, stat_type));
    } catch (...) {
        reset_to_default = true;
        m_PSGTiming.reset(new TPSGTiming(kMinStatValue,
                                         kMaxStatValue,
                                         kNStatBins,
                                         TPSGTiming::eLog2));
    }
}


CNARetrieveTiming::CNARetrieveTiming(unsigned long  min_stat_value,
                                     unsigned long  max_stat_value,
                                     unsigned long  n_bins,
                                     TPSGTiming::EScaleType  stat_type,
                                     bool &  reset_to_default)
{
    reset_to_default = false;

    try {
        m_PSGTiming.reset(new TPSGTiming(min_stat_value, max_stat_value,
                                         n_bins, stat_type));
    } catch (...) {
        reset_to_default = true;
        m_PSGTiming.reset(new TPSGTiming(kMinStatValue,
                                         kMaxStatValue,
                                         kNStatBins,
                                         TPSGTiming::eLog2));
    }
}


CSplitHistoryRetrieveTiming::CSplitHistoryRetrieveTiming(unsigned long  min_stat_value,
                                                         unsigned long  max_stat_value,
                                                         unsigned long  n_bins,
                                                         TPSGTiming::EScaleType  stat_type,
                                                         bool &  reset_to_default)
{
    reset_to_default = false;

    try {
        m_PSGTiming.reset(new TPSGTiming(min_stat_value, max_stat_value,
                                         n_bins, stat_type));
    } catch (...) {
        reset_to_default = true;
        m_PSGTiming.reset(new TPSGTiming(kMinStatValue,
                                         kMaxStatValue,
                                         kNStatBins,
                                         TPSGTiming::eLog2));
    }
}



COperationTiming::COperationTiming(unsigned long  min_stat_value,
                                   unsigned long  max_stat_value,
                                   unsigned long  n_bins,
                                   const string &  stat_type,
                                   unsigned long  small_blob_size) :
    m_StartTime(chrono::system_clock::now())
{
    auto        scale_type = TPSGTiming::eLog2;
    if (NStr::CompareNocase(stat_type, "linear") == 0)
        scale_type = TPSGTiming::eLinear;

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

    reset_to_default |= x_SetupBlobSizeBins(min_stat_value, max_stat_value,
                                            n_bins, scale_type, small_blob_size);


    if (reset_to_default)
        ERR_POST("Invalid statistics parameters detected. Default parameters "
                 "were used");
}


bool COperationTiming::x_SetupBlobSizeBins(unsigned long  min_stat_value,
                                           unsigned long  max_stat_value,
                                           unsigned long  n_bins,
                                           TPSGTiming::EScaleType  stat_type,
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
                                chrono::high_resolution_clock::time_point &  op_begin_ts,
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
    }
}


void COperationTiming::Reset(void)
{
    m_StartTime = chrono::system_clock::now();

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

    for (auto &  item : m_BlobRetrieveTiming)
        item->Reset();
}


CJsonNode COperationTiming::Serialize(void) const
{
    static string   kStartTime("StartTime");
    static string   kLookupLmdbSi2csiFound("LookupLmdbSi2csiFound");
    static string   kLookupLmdbSi2csiNotFound("LookupLmdbSi2csiNotFound");
    static string   kLookupLmdbBioseqInfoFound("LookupLmdbBioseqInfoFound");
    static string   kLookupLmdbBioseqInfoNotFound("LookupLmdbBioseqInfoNotFound");
    static string   kLookupLmdbBlobPropFound("LookupLmdbBlobPropFound");
    static string   kLookupLmdbBlobPropNotFound("LookupLmdbBlobPropNotFound");
    static string   kLookupCassSi2csiFound("LookupCassSi2csiFound");
    static string   kLookupCassSi2csiNotFound("LookupCassSi2csiNotFound");
    static string   kLookupCassBioseqInfoFound("LookupCassBioseqInfoFound");
    static string   kLookupCassBioseqInfoNotFound("LookupCassBioseqInfoNotFound");
    static string   kLookupCassBlobPropFound("LookupCassBlobPropFound");
    static string   kLookupCassBlobPropNotFound("LookupCassBlobPropNotFound");
    static string   kResolutionLmdbFound("ResolutionLmdbFound");
    static string   kResolutionLmdbNotFound("ResolutionLmdbNotFound");
    static string   kResolutionCassFound("ResolutionCassFound");
    static string   kResolutionCassNotFound("ResolutionCassNotFound");
    static string   kNARetrieveFound("NARetrieveFound");
    static string   kNARetrieveNotFound("NARetrieveNotFound");
    static string   kSplitHistoryRetrieveFound("SplitHistoryRetrieveFound");
    static string   kSplitHistoryRetrieveNotFound("SplitHistoryRetrieveNotFound");
    static string   kHugeBlobRetrieval("HugeBlobRetrieval");
    static string   kBlobRetrievalNotFound("BlobRetrievalNotFound");
    static string   kBlobRetrieval("BlobRetrieval");

    CJsonNode       ret(CJsonNode::NewObjectNode());
    ret.SetString(kStartTime, FormatPreciseTime(m_StartTime));

    ret.SetByKey(kLookupLmdbSi2csiFound, m_LookupLmdbSi2csiTiming[0]->Serialize());
    ret.SetByKey(kLookupLmdbSi2csiNotFound, m_LookupLmdbSi2csiTiming[1]->Serialize());
    ret.SetByKey(kLookupLmdbBioseqInfoFound, m_LookupLmdbBioseqInfoTiming[0]->Serialize());
    ret.SetByKey(kLookupLmdbBioseqInfoNotFound, m_LookupLmdbBioseqInfoTiming[1]->Serialize());
    ret.SetByKey(kLookupLmdbBlobPropFound, m_LookupLmdbBlobPropTiming[0]->Serialize());
    ret.SetByKey(kLookupLmdbBlobPropNotFound, m_LookupLmdbBlobPropTiming[1]->Serialize());
    ret.SetByKey(kLookupCassSi2csiFound, m_LookupCassSi2csiTiming[0]->Serialize());
    ret.SetByKey(kLookupCassSi2csiNotFound, m_LookupCassSi2csiTiming[1]->Serialize());
    ret.SetByKey(kLookupCassBioseqInfoFound, m_LookupCassBioseqInfoTiming[0]->Serialize());
    ret.SetByKey(kLookupCassBioseqInfoNotFound, m_LookupCassBioseqInfoTiming[1]->Serialize());
    ret.SetByKey(kLookupCassBlobPropFound, m_LookupCassBlobPropTiming[0]->Serialize());
    ret.SetByKey(kLookupCassBlobPropNotFound, m_LookupCassBlobPropTiming[1]->Serialize());
    ret.SetByKey(kResolutionLmdbFound, m_ResolutionLmdbTiming[0]->Serialize());
    ret.SetByKey(kResolutionLmdbNotFound, m_ResolutionLmdbTiming[1]->Serialize());
    ret.SetByKey(kResolutionCassFound, m_ResolutionCassTiming[0]->Serialize());
    ret.SetByKey(kResolutionCassNotFound, m_ResolutionCassTiming[1]->Serialize());
    ret.SetByKey(kNARetrieveFound, m_NARetrieveTiming[0]->Serialize());
    ret.SetByKey(kNARetrieveNotFound, m_NARetrieveTiming[1]->Serialize());
    ret.SetByKey(kSplitHistoryRetrieveFound, m_SplitHistoryRetrieveTiming[0]->Serialize());
    ret.SetByKey(kSplitHistoryRetrieveNotFound, m_SplitHistoryRetrieveTiming[1]->Serialize());
    ret.SetByKey(kHugeBlobRetrieval, m_HugeBlobRetrievalTiming->Serialize());
    ret.SetByKey(kBlobRetrievalNotFound, m_NotFoundBlobRetrievalTiming->Serialize());

    CJsonNode       retrieval(CJsonNode::NewArrayNode());
    for (const auto &  item : m_BlobRetrieveTiming)
        retrieval.Append(item->Serialize());
    ret.SetByKey(kBlobRetrieval, retrieval);

    return ret;
}

