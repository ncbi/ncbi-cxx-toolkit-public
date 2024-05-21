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
 * File Description:
 *   PSG server request time series statistics
 *
 */
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include "time_series_stat.hpp"


static string           kActualTimeRangeStart("TimeRangeStart");
static string           kActualTimeRangeEnd("TimeRangeEnd");


CTimeSeriesBase::CTimeSeriesBase() :
    m_TotalMinutesCollected(1),
    m_Loop(false),
    m_CurrentIndex(0)
{}


CTimeSeriesBase::EPSGS_SkipCheckResult
CTimeSeriesBase::CheckToSkip(int  most_ancient_time,
                             int  most_recent_time,
                             ssize_t  current_values_start_sec,
                             ssize_t  current_values_end_sec) const
{
    if (most_recent_time >= 0) {
        // most_recent_time is defined
        if (most_recent_time > current_values_end_sec) {
            return EPSGS_SkipCheckResult::ePSGS_SkipBegin;
        }
    }

    if (most_ancient_time >= 0) {
        // most_ancient_time is defined
        if (most_ancient_time < current_values_start_sec) {
            return EPSGS_SkipCheckResult::ePSGS_SkipEnd;
        }
    }

    return EPSGS_SkipCheckResult::ePSGS_DontSkip;
}


CMomentousCounterSeries::CMomentousCounterSeries() :
    m_Accumulated(0), m_AccumulatedCount(0),
    m_TotalValues(0.0),
    m_MaxValue(0.0)
{
    Reset();
}


void CMomentousCounterSeries::Add(uint64_t   value)
{
    // Adding happens every 5 seconds and goes to the accumulated values
    m_Accumulated += value;
    ++m_AccumulatedCount;
}


void CMomentousCounterSeries::Rotate(void)
{
    // Rotate should:
    // - calculate avarage
    // - store it in the current index cell
    // - rotate the current index

    size_t      current_index = m_CurrentIndex.load();
    m_Values[current_index] = double(m_Accumulated) / double(m_AccumulatedCount);
    m_TotalValues += m_Values[current_index];
    if (m_Values[current_index] > m_MaxValue) {
        m_MaxValue = m_Values[current_index];
    }

    m_Accumulated = 0;
    m_AccumulatedCount = 0;

    size_t      new_current_index = m_CurrentIndex.load();
    if (new_current_index == kSeriesIntervals - 1) {
        new_current_index = 0;
    } else {
        ++new_current_index;
    }

    m_Values[new_current_index] = 0;

    m_CurrentIndex.store(new_current_index);
    ++m_TotalMinutesCollected;
    if (new_current_index == 0) {
        m_Loop = true;
    }
}


void CMomentousCounterSeries::Reset(void)
{
    for (size_t  k = 0; k < kSeriesIntervals; ++k)
        m_Values[k] = 0.0;
    m_TotalValues = 0.0;
    m_MaxValue = 0.0;

    m_Accumulated = 0;
    m_AccumulatedCount = 0;

    m_CurrentIndex.store(0);
    m_TotalMinutesCollected.store(1);
    m_Loop = false;
}


CJsonNode
CMomentousCounterSeries::Serialize(const vector<pair<int, int>> &  time_series,
                                   int  most_ancient_time,
                                   int  most_recent_time,
                                   bool  loop, size_t  current_index) const
{
    CJsonNode   ret(CJsonNode::NewObjectNode());

    ret.SetByKey("AverageValues",
                 x_SerializeOneSeries(time_series,
                                      most_ancient_time, most_recent_time,
                                      loop, current_index));
    return ret;
}


CJsonNode
CMomentousCounterSeries::x_SerializeOneSeries(const vector<pair<int, int>> &  time_series,
                                              int  most_ancient_time,
                                              int  most_recent_time,
                                              bool  loop,
                                              size_t  current_index) const
{
    CJsonNode   ret(CJsonNode::NewObjectNode());

    if (current_index == 0 && loop == false) {
        // There is no data collected yet
        return ret;
    }

    CJsonNode   output_series(CJsonNode::NewArrayNode());

    // Index in the array where the data are collected
    size_t      raw_index;
    if (current_index == 0) {
        raw_index = kSeriesIntervals - 1;
        loop = false;   // to avoid going the second time over
    } else {
        raw_index = current_index - 1;
    }

    size_t      current_accumulated_mins = 0;
    double      current_accumulated_vals = 0;
    size_t      output_data_index = 0;
    double      total_processed_vals = 0.0;
    double      max_observed_val = 0.0;
    size_t      observed_minutes = 0;
    double      total_observed_vals = 0.0;

    // The current index in the 'time_series', i.e. a pair of
    // <mins to accumulate>:<last sequential data index>
    // It is guaranteed they are both > 0.
    size_t      range_index = 0;
    size_t      current_mins_to_accumulate = time_series[range_index].first;
    size_t      current_last_seq_index = time_series[range_index].second;

    ssize_t     actual_range_start_sec = -1;
    ssize_t     actual_range_end_sec = -1;
    ssize_t     past_minute = -1;

    for ( ;; ) {
        total_processed_vals += m_Values[raw_index];

        ++past_minute;
        ssize_t     current_values_start_sec = past_minute * 60;
        ssize_t     current_values_end_sec = current_values_start_sec + 59;

        auto        skip_check = CheckToSkip(most_ancient_time, most_recent_time,
                                             current_values_start_sec,
                                             current_values_end_sec);
        if (skip_check != EPSGS_SkipCheckResult::ePSGS_DontSkip) {
            // Regardless how it was skipped (at the beginning or at the end)
            // it needs to continue due to a necessity to calculate
            // total_processed_vals
            if (raw_index == 0)
                break;
            --raw_index;
            continue;
        }

        // Update the actual covered time frame
        if (actual_range_start_sec < 0) {
            actual_range_start_sec = current_values_start_sec;
        }
        actual_range_end_sec = current_values_end_sec;


        double  val = m_Values[raw_index];
        if (val > max_observed_val) {
            max_observed_val = val;
        }
        ++observed_minutes;
        total_observed_vals += val;

        ++current_accumulated_mins;
        current_accumulated_vals += val;

        if (current_accumulated_mins >= current_mins_to_accumulate) {
            output_series.AppendDouble(double(current_accumulated_vals) /
                                       double(current_accumulated_mins));
            current_accumulated_mins = 0;
            current_accumulated_vals = 0.0;
        }

        ++output_data_index;
        if (output_data_index > current_last_seq_index) {
            ++range_index;
            current_mins_to_accumulate = time_series[range_index].first;
            current_last_seq_index = time_series[range_index].second;
        }

        if (raw_index == 0)
            break;
        --raw_index;
    }

    if (loop) {
        raw_index = kSeriesIntervals - 1;
        while (raw_index > current_index + 1) {
            total_processed_vals += m_Values[raw_index];

            ++past_minute;
            ssize_t     current_values_start_sec = past_minute * 60;
            ssize_t     current_values_end_sec = current_values_start_sec + 59;
            auto        skip_check = CheckToSkip(most_ancient_time, most_recent_time,
                                                 current_values_start_sec,
                                                 current_values_end_sec);
            if (skip_check != EPSGS_SkipCheckResult::ePSGS_DontSkip) {
                // Regardless how it was skipped (at the beginning or at the end)
                // it needs to continue due to a necessity to calculate
                // total_processed_vals
                --raw_index;
                continue;
            }

            // Update the actual covered time frame
            if (actual_range_start_sec < 0) {
                actual_range_start_sec = current_values_start_sec;
            }
            actual_range_end_sec = current_values_end_sec;

            double    val = m_Values[raw_index];
            if (val > max_observed_val) {
                max_observed_val = val;
            }
            ++observed_minutes;
            total_observed_vals += val;

            --raw_index;

            ++current_accumulated_mins;
            current_accumulated_vals += val;

            if (current_accumulated_mins >= current_mins_to_accumulate) {
                output_series.AppendDouble(double(current_accumulated_vals) /
                                           double(current_accumulated_mins));
                current_accumulated_mins = 0;
                current_accumulated_vals = 0;
            }

            ++output_data_index;
            if (output_data_index > current_last_seq_index) {
                ++range_index;
                current_mins_to_accumulate = time_series[range_index].first;
                current_last_seq_index = time_series[range_index].second;
            }
        }
    }

    if (actual_range_start_sec == -1 && actual_range_end_sec == -1) {
        // No data points were picked
        return ret;
    }

    ret.SetInteger(kActualTimeRangeStart, actual_range_start_sec);
    ret.SetInteger(kActualTimeRangeEnd, actual_range_end_sec);

    if (current_accumulated_mins > 0) {
        output_series.AppendDouble(double(current_accumulated_vals) /
                                   double(current_accumulated_mins));
    }

    if (loop && most_ancient_time == -1) {
        // The current minute is not participating in the calculation
        int64_t     rest_mins = m_TotalMinutesCollected.load() - kSeriesIntervals - 1;
        double      rest_vals = m_TotalValues - total_processed_vals - m_Values[current_index];

        if (rest_mins > 0) {
            ret.SetDouble("RestAvgPerSec", rest_vals / (rest_mins * 60.0));
        }
    }

    ret.SetDouble("MaxAllTimePerSec", m_MaxValue / 60.0);
    ret.SetDouble("AvgAllTimePerSec", m_TotalValues /
                                      ((m_TotalMinutesCollected.load() - 1) * 60.0));

    ret.SetDouble("MaxObservedPerSec", max_observed_val / 60.0);
    ret.SetDouble("AvgObservedPerSec", total_observed_vals / (observed_minutes * 60.0));

    ret.SetByKey("time_series", output_series);
    return ret;
}


CMonotonicCounterSeries::CMonotonicCounterSeries()
{
    Reset();
}

void CMonotonicCounterSeries::Add(void)
{
    size_t      current_index = m_CurrentIndex.load();
    ++m_Values[current_index];
    ++m_TotalValues;

    if (m_Values[current_index] > m_MaxValue) {
        m_MaxValue = m_Values[current_index];
    }
}


void CMonotonicCounterSeries::Rotate(void)
{
    size_t      new_current_index = m_CurrentIndex.load();
    if (new_current_index == kSeriesIntervals - 1) {
        new_current_index = 0;
    } else {
        ++new_current_index;
    }

    m_Values[new_current_index] = 0;

    m_CurrentIndex.store(new_current_index);
    ++m_TotalMinutesCollected;
    if (new_current_index == 0) {
        m_Loop = true;
    }
}


void CMonotonicCounterSeries::Reset(void)
{
    memset(m_Values, 0, sizeof(m_Values));
    m_TotalValues = 0;
    m_MaxValue = 0;

    m_CurrentIndex.store(0);
    m_TotalMinutesCollected.store(1);
    m_Loop = false;
}


CJsonNode
CMonotonicCounterSeries::Serialize(const vector<pair<int, int>> &  time_series,
                                   int  most_ancient_time,
                                   int  most_recent_time,
                                   bool  loop, size_t  current_index) const
{
    CJsonNode   ret(CJsonNode::NewObjectNode());

    ret.SetByKey("Values",
                 x_SerializeOneSeries(m_Values, m_TotalValues,
                                      time_series,
                                      most_ancient_time, most_recent_time,
                                      loop, current_index));
    return ret;
}


CJsonNode
CMonotonicCounterSeries::x_SerializeOneSeries(const uint64_t *  values,
                                              uint64_t  grand_total,
                                              const vector<pair<int, int>> &  time_series,
                                              int  most_ancient_time,
                                              int  most_recent_time,
                                              bool  loop,
                                              size_t  current_index) const
{
    CJsonNode   ret(CJsonNode::NewObjectNode());

    if (current_index == 0 && loop == false) {
        // There is no data collected yet
        return ret;
    }

    CJsonNode   output_series(CJsonNode::NewArrayNode());

    // Index in the array where the data are collected
    size_t      raw_index;
    if (current_index == 0) {
        raw_index = kSeriesIntervals - 1;
        loop = false;   // to avoid going the second time over
    } else {
        raw_index = current_index - 1;
    }

    size_t      current_accumulated_mins = 0;
    uint64_t    current_accumulated_vals = 0;
    size_t      output_data_index = 0;
    uint64_t    total_processed_vals = 0;
    uint64_t    max_observed_val = 0;
    size_t      observed_minutes = 0;
    uint64_t    total_observed_vals = 0;

    // The current index in the 'time_series', i.e. a pair of
    // <mins to accumulate>:<last sequential data index>
    // It is guaranteed they are both > 0.
    size_t      range_index = 0;
    size_t      current_mins_to_accumulate = time_series[range_index].first;
    size_t      current_last_seq_index = time_series[range_index].second;

    ssize_t     actual_range_start_sec = -1;
    ssize_t     actual_range_end_sec = -1;
    ssize_t     past_minute = -1;

    for ( ;; ) {
        total_processed_vals += values[raw_index];

        ++past_minute;
        ssize_t     current_values_start_sec = past_minute * 60;
        ssize_t     current_values_end_sec = current_values_start_sec + 59;

        auto        skip_check = CheckToSkip(most_ancient_time, most_recent_time,
                                             current_values_start_sec,
                                             current_values_end_sec);
        if (skip_check != EPSGS_SkipCheckResult::ePSGS_DontSkip) {
            // Regardless how it was skipped (at the beginning or at the end)
            // it needs to continue due to a necessity to calculate
            // total_processed_vals
            if (raw_index == 0)
                break;
            --raw_index;
            continue;
        }

        // Update the actual covered time frame
        if (actual_range_start_sec < 0) {
            actual_range_start_sec = current_values_start_sec;
        }
        actual_range_end_sec = current_values_end_sec;


        uint64_t    vals = values[raw_index];
        if (vals > max_observed_val) {
            max_observed_val = vals;
        }
        ++observed_minutes;
        total_observed_vals += vals;

        ++current_accumulated_mins;
        current_accumulated_vals += vals;

        if (current_accumulated_mins >= current_mins_to_accumulate) {
            output_series.AppendInteger(current_accumulated_vals);
            current_accumulated_mins = 0;
            current_accumulated_vals = 0;
        }

        ++output_data_index;
        if (output_data_index > current_last_seq_index) {
            ++range_index;
            current_mins_to_accumulate = time_series[range_index].first;
            current_last_seq_index = time_series[range_index].second;
        }

        if (raw_index == 0)
            break;
        --raw_index;
    }

    if (loop) {
        raw_index = kSeriesIntervals - 1;
        while (raw_index > current_index + 1) {
            total_processed_vals += values[raw_index];

            ++past_minute;
            ssize_t     current_values_start_sec = past_minute * 60;
            ssize_t     current_values_end_sec = current_values_start_sec + 59;
            auto        skip_check = CheckToSkip(most_ancient_time, most_recent_time,
                                                 current_values_start_sec,
                                                 current_values_end_sec);
            if (skip_check != EPSGS_SkipCheckResult::ePSGS_DontSkip) {
                // Regardless how it was skipped (at the beginning or at the end)
                // it needs to continue due to a necessity to calculate
                // total_processed_vals
                --raw_index;
                continue;
            }

            // Update the actual covered time frame
            if (actual_range_start_sec < 0) {
                actual_range_start_sec = current_values_start_sec;
            }
            actual_range_end_sec = current_values_end_sec;

            uint64_t    vals = values[raw_index];
            if (vals > max_observed_val) {
                max_observed_val = vals;
            }
            ++observed_minutes;
            total_observed_vals += vals;

            --raw_index;

            ++current_accumulated_mins;
            current_accumulated_vals += vals;

            if (current_accumulated_mins >= current_mins_to_accumulate) {
                output_series.AppendInteger(current_accumulated_vals);
                current_accumulated_mins = 0;
                current_accumulated_vals = 0;
            }

            ++output_data_index;
            if (output_data_index > current_last_seq_index) {
                ++range_index;
                current_mins_to_accumulate = time_series[range_index].first;
                current_last_seq_index = time_series[range_index].second;
            }
        }
    }

    if (actual_range_start_sec == -1 && actual_range_end_sec == -1) {
        // No data points were picked
        return ret;
    }

    ret.SetInteger(kActualTimeRangeStart, actual_range_start_sec);
    ret.SetInteger(kActualTimeRangeEnd, actual_range_end_sec);

    if (current_accumulated_mins > 0) {
        output_series.AppendInteger(current_accumulated_vals);
    }

    if (loop && most_ancient_time == -1) {
        // The current minute and the last minute in case of a loop are not
        // sent to avoid unreliable data
        uint64_t    rest_mins = m_TotalMinutesCollected.load() - kSeriesIntervals - 1;
        uint64_t    rest_vals = grand_total - total_processed_vals - values[current_index];

        if (rest_mins > 0) {
            ret.SetDouble("RestAvgPerSec", rest_vals / (rest_mins * 60.0));
        }
    }

    ret.SetDouble("MaxAllTimePerSec", m_MaxValue / 60.0);
    ret.SetDouble("AvgAllTimePerSec", grand_total /
                                      ((m_TotalMinutesCollected.load() - 1) * 60.0));

    ret.SetDouble("MaxObservedPerSec", max_observed_val / 60.0);
    ret.SetDouble("AvgObservedPerSec", total_observed_vals / (observed_minutes * 60.0));

    ret.SetInteger("ValObserved", total_observed_vals);
    ret.SetInteger("ValAllTime", grand_total - values[current_index]);

    ret.SetByKey("time_series", output_series);
    return ret;
}


CProcessorRequestTimeSeries::CProcessorRequestTimeSeries()
{
    Reset();
}


void CProcessorRequestTimeSeries::Add(void)
{
    size_t      current_index = m_CurrentIndex.load();
    ++m_Requests[current_index];
    ++m_TotalRequests;

    if (m_Requests[current_index] > m_MaxValue) {
        m_MaxValue = m_Requests[current_index];
    }
}


void CProcessorRequestTimeSeries::Rotate(void)
{
    size_t      new_current_index = m_CurrentIndex.load();
    if (new_current_index == kSeriesIntervals - 1) {
        new_current_index = 0;
    } else {
        ++new_current_index;
    }

    m_Requests[new_current_index] = 0;

    m_CurrentIndex.store(new_current_index);
    ++m_TotalMinutesCollected;
    if (new_current_index == 0) {
        m_Loop = true;
    }
}


void CProcessorRequestTimeSeries::Reset(void)
{
    memset(m_Requests, 0, sizeof(m_Requests));
    m_TotalRequests = 0;
    m_MaxValue = 0;

    m_CurrentIndex.store(0);
    m_TotalMinutesCollected.store(1);
    m_Loop = false;
}


CJsonNode
CProcessorRequestTimeSeries::Serialize(const vector<pair<int, int>> &  time_series,
                                       int  most_ancient_time, int  most_recent_time,
                                       bool  loop, size_t  current_index) const
{
    CJsonNode   ret(CJsonNode::NewObjectNode());

    ret.SetByKey("Requests",
                 x_SerializeOneSeries(m_Requests, m_TotalRequests, m_MaxValue,
                                      time_series, most_ancient_time,
                                      most_recent_time, loop, current_index));
    return ret;
}


CJsonNode
CProcessorRequestTimeSeries::x_SerializeOneSeries(const uint64_t *  values,
                                                  uint64_t  grand_total,
                                                  uint64_t  grand_max,
                                                  const vector<pair<int, int>> &  time_series,
                                                  int  most_ancient_time,
                                                  int  most_recent_time,
                                                  bool  loop,
                                                  size_t  current_index) const
{
    CJsonNode   ret(CJsonNode::NewObjectNode());

    if (current_index == 0 && loop == false) {
        // There is no data collected yet
        return ret;
    }

    CJsonNode   output_series(CJsonNode::NewArrayNode());

    // Needed to calculate max and average reqs/sec
    uint64_t    max_observed_val = 0;
    uint64_t    total_observed_vals = 0;
    uint64_t    observed_minutes = 0;

    // Index in the array where the data are collected
    size_t      raw_index;
    if (current_index == 0) {
        raw_index = kSeriesIntervals - 1;
        loop = false;   // to avoid going the second time over
    } else {
        raw_index = current_index - 1;
    }

    size_t      current_accumulated_mins = 0;
    uint64_t    current_accumulated_reqs = 0;
    size_t      output_data_index = 0;
    uint64_t    total_processed_vals = 0;

    // The current index in the 'time_series', i.e. a pair of
    // <mins to accumulate>:<last sequential data index>
    // It is guaranteed they are both > 0.
    size_t      range_index = 0;
    size_t      current_mins_to_accumulate = time_series[range_index].first;
    size_t      current_last_seq_index = time_series[range_index].second;


    ssize_t     actual_range_start_sec = -1;
    ssize_t     actual_range_end_sec = -1;
    ssize_t     past_minute = -1;

    for ( ;; ) {
        total_processed_vals += values[raw_index];

        ++past_minute;
        ssize_t     current_values_start_sec = past_minute * 60;
        ssize_t     current_values_end_sec = current_values_start_sec + 59;

        auto        skip_check = CheckToSkip(most_ancient_time, most_recent_time,
                                             current_values_start_sec,
                                             current_values_end_sec);
        if (skip_check != EPSGS_SkipCheckResult::ePSGS_DontSkip) {
            // Regardless how it was skipped (at the beginning or at the end)
            // it needs to continue due to a necessity to calculate
            // total_processed_vals
            if (raw_index == 0)
                break;
            --raw_index;
            continue;
        }

        // Update the actual covered time frame
        if (actual_range_start_sec < 0) {
            actual_range_start_sec = current_values_start_sec;
        }
        actual_range_end_sec = current_values_end_sec;


        uint64_t    reqs = values[raw_index];
        if (reqs > max_observed_val) {
            max_observed_val = reqs;
        }
        ++observed_minutes;
        total_observed_vals += reqs;

        ++current_accumulated_mins;
        current_accumulated_reqs += reqs;

        if (current_accumulated_mins >= current_mins_to_accumulate) {
            output_series.AppendDouble(double(current_accumulated_reqs) /
                                       (double(current_accumulated_mins) * 60.0));
            current_accumulated_mins = 0;
            current_accumulated_reqs = 0;
        }

        ++output_data_index;
        if (output_data_index > current_last_seq_index) {
            ++range_index;
            current_mins_to_accumulate = time_series[range_index].first;
            current_last_seq_index = time_series[range_index].second;
        }

        if (raw_index == 0)
            break;
        --raw_index;
    }

    if (loop) {
        raw_index = kSeriesIntervals - 1;
        while (raw_index > current_index + 1) {
            total_processed_vals += values[raw_index];

            ++past_minute;
            ssize_t     current_values_start_sec = past_minute * 60;
            ssize_t     current_values_end_sec = current_values_start_sec + 59;
            auto        skip_check = CheckToSkip(most_ancient_time, most_recent_time,
                                                 current_values_start_sec,
                                                 current_values_end_sec);
            if (skip_check != EPSGS_SkipCheckResult::ePSGS_DontSkip) {
                // Regardless how it was skipped (at the beginning or at the end)
                // it needs to continue due to a necessity to calculate
                // total_processed_vals
                --raw_index;
                continue;
            }

            // Update the actual covered time frame
            if (actual_range_start_sec < 0) {
                actual_range_start_sec = current_values_start_sec;
            }
            actual_range_end_sec = current_values_end_sec;

            uint64_t    reqs = values[raw_index];
            --raw_index;

            if (reqs > max_observed_val) {
               max_observed_val = reqs;
            }
            ++observed_minutes;
            total_observed_vals += reqs;

            ++current_accumulated_mins;
            current_accumulated_reqs += reqs;

            if (current_accumulated_mins >= current_mins_to_accumulate) {
                output_series.AppendDouble(double(current_accumulated_reqs) /
                                           (double(current_accumulated_mins) * 60.0));
                current_accumulated_mins = 0;
                current_accumulated_reqs = 0;
            }

            ++output_data_index;
            if (output_data_index > current_last_seq_index) {
                ++range_index;
                current_mins_to_accumulate = time_series[range_index].first;
                current_last_seq_index = time_series[range_index].second;
            }
        }
    }

    if (actual_range_start_sec == -1 && actual_range_end_sec == -1) {
        // No data points were picked
        return ret;
    }

    ret.SetInteger(kActualTimeRangeStart, actual_range_start_sec);
    ret.SetInteger(kActualTimeRangeEnd, actual_range_end_sec);

    if (current_accumulated_mins > 0) {
        output_series.AppendDouble(double(current_accumulated_reqs) /
                                   (double(current_accumulated_mins) * 60.0));
    }

    if (loop && most_ancient_time == -1) {
        // The current minute is not participating in the calculation
        uint64_t    rest_mins = m_TotalMinutesCollected.load() - kSeriesIntervals - 1;
        uint64_t    rest_reqs = grand_total - total_processed_vals - values[current_index];

        if (rest_mins > 0) {
            ret.SetDouble("RestAvgPerSec", rest_reqs / (rest_mins * 60.0));
        }
    }

    ret.SetDouble("MaxAllTimePerSec", grand_max / 60.0);
    ret.SetDouble("AvgAllTimePerSec", grand_total /
                                      ((m_TotalMinutesCollected.load() - 1) * 60.0));

    ret.SetDouble("MaxObservedPerSec", max_observed_val / 60.0);
    ret.SetDouble("AvgObservedPerSec", total_observed_vals / (observed_minutes * 60.0));

    ret.SetInteger("ValObserved", total_observed_vals);
    ret.SetInteger("ValAllTime", grand_total - values[current_index]);

    ret.SetByKey("time_series", output_series);
    return ret;
}




// Converts a request status to the counter in the time series
// The logic matches the logic in GRID dashboard
CRequestTimeSeries::EPSGSCounter
CRequestTimeSeries::RequestStatusToCounter(CRequestStatus::ECode  status)
{
    if (status == CRequestStatus::e404_NotFound)
        return eNotFound;

    if (status >= CRequestStatus::e500_InternalServerError)
        return eError;

    if (status >= CRequestStatus::e400_BadRequest)
        return eWarning;

    return eRequest;
}


CRequestTimeSeries::CRequestTimeSeries()
{
    Reset();
}


void CRequestTimeSeries::Add(EPSGSCounter  counter)
{
    size_t      current_index = m_CurrentIndex.load();
    switch (counter) {
        case eRequest:
            ++m_Requests[current_index];
            ++m_TotalRequests;
            if (m_Requests[current_index] > m_MaxValue) {
                m_MaxValue = m_Requests[current_index];
            }
            break;
        case eError:
            ++m_Errors[current_index];
            ++m_TotalErrors;
            if (m_Errors[current_index] > m_MaxErrors) {
                m_MaxErrors = m_Errors[current_index];
            }
            break;
        case eWarning:
            ++m_Warnings[current_index];
            ++m_TotalWarnings;
            if (m_Warnings[current_index] > m_MaxWarnings) {
                m_MaxWarnings = m_Warnings[current_index];
            }
            break;
        case eNotFound:
            ++m_NotFound[current_index];
            ++m_TotalNotFound;
            if (m_NotFound[current_index] > m_MaxNotFound) {
                m_MaxNotFound = m_NotFound[current_index];
            }
            break;
        default:
            break;
    }
}


void CRequestTimeSeries::Rotate(void)
{
    size_t      new_current_index = m_CurrentIndex.load();
    if (new_current_index == kSeriesIntervals - 1) {
        new_current_index = 0;
    } else {
        ++new_current_index;
    }

    m_Requests[new_current_index] = 0;
    m_Errors[new_current_index] = 0;
    m_Warnings[new_current_index] = 0;
    m_NotFound[new_current_index] = 0;

    m_CurrentIndex.store(new_current_index);
    ++m_TotalMinutesCollected;
    if (new_current_index == 0) {
        m_Loop = true;
    }
}


void CRequestTimeSeries::Reset(void)
{
    memset(m_Requests, 0, sizeof(m_Requests));
    m_TotalRequests = 0;
    m_MaxValue = 0;
    memset(m_Errors, 0, sizeof(m_Errors));
    m_TotalErrors = 0;
    m_MaxErrors = 0;
    memset(m_Warnings, 0, sizeof(m_Warnings));
    m_TotalWarnings = 0;
    m_MaxWarnings = 0;
    memset(m_NotFound, 0, sizeof(m_NotFound));
    m_TotalNotFound = 0;
    m_MaxNotFound = 0;

    m_CurrentIndex.store(0);
    m_TotalMinutesCollected.store(1);
    m_Loop = false;
}


CJsonNode
CRequestTimeSeries::Serialize(const vector<pair<int, int>> &  time_series,
                              int  most_ancient_time, int  most_recent_time,
                              bool  loop, size_t  current_index) const
{
    CJsonNode   ret(CJsonNode::NewObjectNode());

    ret.SetByKey("Requests",
                 x_SerializeOneSeries(m_Requests, m_TotalRequests, m_MaxValue,
                                      time_series,
                                      most_ancient_time, most_recent_time,
                                      loop, current_index));
    ret.SetByKey("Errors",
                 x_SerializeOneSeries(m_Errors, m_TotalErrors, m_MaxErrors,
                                      time_series,
                                      most_ancient_time, most_recent_time,
                                      loop, current_index));
    ret.SetByKey("Warnings",
                 x_SerializeOneSeries(m_Warnings, m_TotalWarnings, m_MaxWarnings,
                                      time_series,
                                      most_ancient_time, most_recent_time,
                                      loop, current_index));
    ret.SetByKey("NotFound",
                 x_SerializeOneSeries(m_NotFound, m_TotalNotFound, m_MaxNotFound,
                                      time_series,
                                      most_ancient_time, most_recent_time,
                                      loop, current_index));
    return ret;
}

