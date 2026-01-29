#ifndef TIME_SERIES_STAT__HPP
#define TIME_SERIES_STAT__HPP

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

#include <connect/services/json_over_uttp.hpp>
#include <corelib/request_status.hpp>
USING_NCBI_SCOPE;


// Request time series for:
// - number of requests
// - number of errors
// - number of warnings
// - number of not found
// All the values are collected for 30 days with a granularity of a minute
static constexpr size_t    kSeriesIntervals = 60*24*30;


class CTimeSeriesBase
{
    public:
        CTimeSeriesBase();
        virtual ~CTimeSeriesBase() {}

        virtual void Rotate(void) = 0;
        virtual void Reset(void) = 0;

    public:
        // This is to be able to sum values from different requests.
        // Sinse the change of the slot for a minute is almost synchronous for
        // all the requests the current values can be received from any of them
        // and then iterate from outside.
        void GetLoopAndIndex(bool &  loop, size_t &  current_index) const
        {
            loop = m_Loop;
            current_index = m_CurrentIndex.load();
        }

    protected:
        enum EPSGS_SkipCheckResult {
            ePSGS_SkipBegin,
            ePSGS_SkipEnd,
            ePSGS_DontSkip
        };
        EPSGS_SkipCheckResult  CheckToSkip(int  most_ancient_time,
                                           int  most_recent_time,
                                           ssize_t  current_values_start_sec,
                                           ssize_t  current_values_end_sec) const;

    protected:
        // Includes the current minute
        atomic_uint_fast64_t    m_TotalMinutesCollected;

        // Tells if the current index made a loop
        bool        m_Loop;

        // Note: there is no any kind of lock to protect the current index.
        // This is an intention due to a significant performance penalty at
        // least under a production load. If a lock is used then the blob
        // retrieval could be up to 25% slower. Most probably it is related to
        // the fact that the other threads which finished requests are
        // increasing the counters under a lock so it leads to impossibility to
        // use these threads to process the blob chunks though they are already
        // retrieved.
        // All the precautions were made to prevent data corruption without the
        // lock. The only possible implication is to have a request registered
        // under a wrong minute (which can be like harmless timing) and slight
        // off for the total number of requests sent to the client (which is
        // almost harmless since the client is interested in a trend but not in
        // absolutely precise data).
        atomic_uint_fast64_t    m_CurrentIndex;
};


// The class collects momentous counters
class CMomentousCounterSeries : public CTimeSeriesBase
{
    public:
        CMomentousCounterSeries() :
            m_Accumulated(0), m_AccumulatedCount(0),
            m_TotalValues(0.0),
            m_MaxValue(0.0)
        {
            Reset();
        }

        void Add(uint64_t   value)
        {
            m_Accumulated += value;
            ++m_AccumulatedCount;
        }

        void Rotate(void);
        void Reset(void);
        CJsonNode  Serialize(const vector<pair<int, int>> &  time_series,
                             int  most_ancient_time,
                             int  most_recent_time,
                             bool  loop, size_t  current_index) const;

    protected:
        CJsonNode x_SerializeOneSeries(const vector<pair<int, int>> &  time_series,
                                       int  most_ancient_time,
                                       int  most_recent_time,
                                       bool  loop,
                                       size_t  current_index) const;

    protected:
        // Accumulated within a minute
        uint64_t    m_Accumulated;
        size_t      m_AccumulatedCount;

        // Average per minute
        uint64_t    m_Values[kSeriesIntervals];
        double      m_TotalValues;
        double      m_MaxValue;
};


// The class collects average performance per minute intervals
class CAvgPerformanceSeries : public CTimeSeriesBase
{
    public:
        CAvgPerformanceSeries() :
            m_Accumulated(0), m_AccumulatedCount(0),
            m_AllTimeAbsoluteMinMks(0),
            m_AllTimeAbsoluteMaxMks(0)
        {
            Reset();
        }

        void Add(uint64_t   value)
        {
            m_Accumulated += value;
            ++m_AccumulatedCount;

            if (value > m_AllTimeAbsoluteMaxMks) {
                m_AllTimeAbsoluteMaxMks = value;
            }
            if (value > 0) {
                if (value < m_AllTimeAbsoluteMinMks || m_AllTimeAbsoluteMinMks == 0) {
                    m_AllTimeAbsoluteMinMks = value;
                }
            }
        }

        void Rotate(void);
        void Reset(void);
        CJsonNode  Serialize(const vector<pair<int, int>> &  time_series,
                             int  most_ancient_time,
                             int  most_recent_time,
                             bool  loop, size_t  current_index) const;

    protected:
        CJsonNode x_SerializeOneSeries(const vector<pair<int, int>> &  time_series,
                                       int  most_ancient_time,
                                       int  most_recent_time,
                                       bool  loop,
                                       size_t  current_index) const;

    protected:
        // Accumulated within a minute
        uint64_t    m_Accumulated;
        size_t      m_AccumulatedCount;

        // Average per minute
        uint64_t    m_IntervalSum[kSeriesIntervals];
        size_t      m_IntervalCOunt[kSeriesIntervals];

        uint64_t    m_AllTimeAbsoluteMinMks;
        uint64_t    m_AllTimeAbsoluteMaxMks;
};


// Used to collect e.g. errors (5xx request stops) in time perspective.
class CMonotonicCounterSeries : public CTimeSeriesBase
{
    public:
        CMonotonicCounterSeries() :
            m_TotalValues(0), m_MaxValue(0)
        {
            Reset();
        }

        void Add(void);
        void Rotate(void);
        void Reset(void);
        CJsonNode  Serialize(const vector<pair<int, int>> &  time_series,
                             int  most_ancient_time,
                             int  most_recent_time,
                             bool  loop, size_t  current_index) const;

    protected:
        CJsonNode x_SerializeOneSeries(const uint64_t *  values,
                                       uint64_t  grand_total,
                                       const vector<pair<int, int>> &  time_series,
                                       int  most_ancient_time,
                                       int  most_recent_time,
                                       bool  loop,
                                       size_t  current_index) const;

    protected:
        uint64_t    m_Values[kSeriesIntervals];
        uint64_t    m_TotalValues;
        uint64_t    m_MaxValue;
};


// The class collects only information when a processor did something for a
// request.
class CProcessorRequestTimeSeries : public CTimeSeriesBase
{
    public:
        CProcessorRequestTimeSeries();
        virtual ~CProcessorRequestTimeSeries() = default;

        void Add(void);
        void Rotate(void);
        void Reset(void);
        CJsonNode  Serialize(const vector<pair<int, int>> &  time_series,
                             int  most_ancient_time, int  most_recent_time,
                             bool  loop, size_t  current_index) const;

    protected:
        CJsonNode x_SerializeOneSeries(const uint64_t *  values,
                                       uint64_t  grand_total,
                                       uint64_t  grand_max,
                                       const vector<pair<int, int>> &  time_series,
                                       int  most_ancient_time, int  most_recent_time,
                                       bool  loop,
                                       size_t  current_index) const;

    protected:
        uint64_t    m_Requests[kSeriesIntervals];
        uint64_t    m_TotalRequests;

        uint64_t    m_MaxValue;
};


// Extends the CProcessorRequestTimeSeries so that 4 items are collected:
// requests (as in the base class), errors, warnings and not found
class CRequestTimeSeries : public CProcessorRequestTimeSeries
{
    public:
        enum EPSGSCounter
        {
            eRequest,
            eError,
            eWarning,
            eNotFound
        };

    public:
        CRequestTimeSeries();
        void Add(EPSGSCounter  counter);
        void Rotate(void);
        void Reset(void);
        CJsonNode  Serialize(const vector<pair<int, int>> &  time_series,
                             int  most_ancient_time, int  most_recent_time,
                             bool  loop, size_t  current_index) const;
        static EPSGSCounter RequestStatusToCounter(CRequestStatus::ECode  status);

    public:
        void AppendData(size_t      index,
                        uint64_t &  requests,
                        uint64_t &  errors,
                        uint64_t &  warnings,
                        uint64_t &  not_found) const
        {
            requests += m_Requests[index];
            errors += m_Errors[index];
            warnings += m_Warnings[index];
            not_found += m_NotFound[index];
        }

    private:
        uint64_t    m_Errors[kSeriesIntervals];
        uint64_t    m_TotalErrors;
        uint64_t    m_MaxErrors;
        uint64_t    m_Warnings[kSeriesIntervals];
        uint64_t    m_TotalWarnings;
        uint64_t    m_MaxWarnings;
        uint64_t    m_NotFound[kSeriesIntervals];
        uint64_t    m_TotalNotFound;
        uint64_t    m_MaxNotFound;
};


#endif /* TIME_SERIES_STAT__HPP */

