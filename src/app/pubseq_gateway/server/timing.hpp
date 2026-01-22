#ifndef PUBSEQ_GATEWAY_TIMING__HPP
#define PUBSEQ_GATEWAY_TIMING__HPP

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
 *   PSG server timing
 *
 */

#include "pubseq_gateway_types.hpp"
#include "psgs_request.hpp"
#include "time_series_stat.hpp"
#include "ipsgs_processor.hpp"

#include <vector>
#include <mutex>
using namespace std;

#include <util/data_histogram.hpp>
#include <connect/services/json_over_uttp.hpp>
USING_NCBI_SCOPE;


const unsigned long     kMinStatValue = 0;
const unsigned long     kMaxStatValue = 16 * 1024 * 1024;
const unsigned long     kNStatBins = 24;
const string            kStatScaleType = "log";
const unsigned long     kTickSpan = 10;


class IPSGS_Processor;


enum EPSGOperationStatus {
    eOpStatusFound,
    eOpStatusNotFound
};

enum EPSGOperation {
    // Low level: more or less monolit operations
    eLookupLmdbSi2csi = 0,          // (2)
    eLookupLmdbBioseqInfo,          // (2)
    eLookupLmdbBlobProp,            // (2)
    eLookupCassSi2csi,              // (2)
    eLookupCassBioseqInfo,          // (2)
    eLookupCassBlobProp,            // (2)

    eMyNCBIRetrieve,                // (2)
    eMyNCBIRetrieveError,           // (1)

    eResolutionLmdb,                // (2) From request start
    eResolutionCass,                // (2) From cassandra start

    eResolutionFoundInCassandra,    // (5) From cassandra start

    eSplitHistoryRetrieve,
    ePublicCommentRetrieve,
    eAccVerHistRetrieve,
    eIPGResolveRetrieve,

    eVDBOpen,
    eBacklog,
    eSNP_PTISLookup,
    eWGS_VDBLookup,

    // The timings below are collected per processor

    eResolutionError,               // (1) From request start
    eResolutionNotFound,            // (1) From request start
    eResolutionFound,               // (1) From request start

    eBlobRetrieve,
    eNARetrieve,

    eTseChunkRetrieve,
    eNAResolve,

    eOperationLast  // Not for using in the processors' code.
                    // It is for allocating an array with an applog identifiers
};


// Timing is registered in microseconds, i.e. in integers
// Technically speaking the counters will be updated from many threads and
// something could be missed. However it seems reasonable not to use atomic
// counters because:
// - even if something is lost it is not a big deal
// - the losses are going to be miserable
// - atomic counters may introduce some delays
typedef CHistogram<uint64_t, uint64_t, uint64_t>            TOnePSGTiming;
typedef CHistogramTimeSeries<uint64_t, uint64_t, uint64_t>  TPSGTiming;


// Returns a serialized dictionary
CJsonNode SerializeHistogram(const TOnePSGTiming &  histogram,
                             const string &  name,
                             const string &  description,
                             uint64_t  max_value);


// The base class for all the collected statistics.
// Individual type of operations will derive from it so that they will be able
// to tune the nins and intervals as they need.
class CPSGTimingBase
{
    public:
        CPSGTimingBase() : m_MaxValue(0) {}
        virtual ~CPSGTimingBase() {}

    public:
        void Add(uint64_t   mks)
        {
            if (m_PSGTiming)
                m_PSGTiming->Add(mks);
            m_AvgTimeSeries.Add(mks);
            if (mks > m_MaxValue)
                m_MaxValue = mks;
        }

        void Reset(void)
        {
            if (m_PSGTiming)
                m_PSGTiming->Reset();
            m_AvgTimeSeries.Reset();
        }

        void Rotate(void)
        {
            if (m_PSGTiming)
                m_PSGTiming->Rotate();
        }

        uint64_t GetMaxValue(void) const
        {
            return m_MaxValue;
        }

        // The Average time series rotate is done once per minute while the
        // usual rotate is configurable. So there is a separate method to do
        // that properly.
        void RotateAvgTimeSeries(void)
        {
            m_AvgTimeSeries.Rotate();
        }

        TPSGTiming::TTicks  GetNumberOfCoveredTicks(void) const
        {
            if (m_PSGTiming) {
                // The histogram series counts ticks starting from 0 so even
                // before the first tick some data may be collected. So +1
                // is added here.
                return m_PSGTiming->GetCurrentTick() + 1;
            }
            return 0;
        }

        // Generic serialization to json
        virtual CJsonNode SerializeCombined(int  most_ancient_time,
                                            int  most_recent_time,
                                            unsigned long  tick_span,
                                            const string &  name,
                                            const string &  description) const;
        virtual CJsonNode SerializeSeries(int  most_ancient_time,
                                          int  most_recent_time,
                                          unsigned long  tick_span,
                                          const string &  name,
                                          const string &  description) const;
        CJsonNode SerializeAvgPerfSeries(const vector<pair<int, int>> &  time_series,
                                         int  most_ancient_time,
                                         int  most_recent_time,
                                         bool  loop,
                                         size_t  current_index)
        {
            return m_AvgTimeSeries.Serialize(time_series,
                                             most_ancient_time,
                                             most_recent_time,
                                             loop, current_index);
        }

    protected:
        unique_ptr<TPSGTiming>      m_PSGTiming;

        // Lets to collect average mks required by an operation within 1 min
        // intervals for a total of 1 month
        CAvgPerformanceSeries       m_AvgTimeSeries;

        uint64_t                    m_MaxValue;
};

// At the moment almost all the timing classes are the same
// So a macro is used to define them
#define TIMING_CLASS(class_name)                                \
    class class_name : public CPSGTimingBase                    \
    {                                                           \
        public:                                                 \
            class_name(unsigned long  min_stat_value,           \
                       unsigned long  max_stat_value,           \
                       unsigned long  n_bins,                   \
                       TOnePSGTiming::EScaleType  stat_type,    \
                       bool &  reset_to_default);               \
    }


// LMDB cache operations are supposed to be fast.
// There are three tables covered by LMDB cache.
TIMING_CLASS(CLmdbCacheTiming);

// LMDB resolution may involve many tries with the cached tables.
TIMING_CLASS(CLmdbResolutionTiming);

// Cassandra operations are supposed to be slower than LMDB.
// There are three cassandra tables
TIMING_CLASS(CCassTiming);

// Retrieve data from MyNCBI
TIMING_CLASS(CMyNCBITiming);

// Cassandra resolution may need a few tries with a two tables
TIMING_CLASS(CCassResolutionTiming);

// Out of range blob size; should not really happened
TIMING_CLASS(CHugeBlobRetrieveTiming);

// Not found blob
TIMING_CLASS(CNotFoundBlobRetrieveTiming);

// Named annotation retrieval
TIMING_CLASS(CNARetrieveTiming);

// Split history retrieval
TIMING_CLASS(CSplitHistoryRetrieveTiming);

// Public comment retrieval
TIMING_CLASS(CPublicCommentRetrieveTiming);

// Accession version history retrieval
TIMING_CLASS(CAccVerHistoryRetrieveTiming);

// IPG resolve resolution
TIMING_CLASS(CIPGResolveRetrieveTiming);

// TSE chunk retrieve
TIMING_CLASS(CTSEChunkRetrieveTiming);

// NA resolve for non-cassandra processors
TIMING_CLASS(CNAResolveTiming);

// VDB opening timing
TIMING_CLASS(CVDBOpenTiming);

// SNP PTIS lookup timing
TIMING_CLASS(CSNPPTISLookupTiming);

// WGS VDB lookup timing
TIMING_CLASS(CWGSVDBLookupTiming);

// Resolution
TIMING_CLASS(CResolutionTiming);

// Time staying in the backlog
TIMING_CLASS(CBacklogTiming);

// Time staying in the backlog
TIMING_CLASS(CProcessorPerformanceTiming);

// Time spent in the too long operations
TIMING_CLASS(COpTooLongTiming);


// Blob retrieval depends on a blob size
class CBlobRetrieveTiming : public CPSGTimingBase
{
    public:
        CBlobRetrieveTiming(unsigned long  min_blob_size,
                            unsigned long  max_blob_size,
                            unsigned long  min_stat_value,
                            unsigned long  max_stat_value,
                            unsigned long  n_bins,
                            TOnePSGTiming::EScaleType  stat_type,
                            bool &  reset_to_default);
        ~CBlobRetrieveTiming() {}

    public:
        virtual CJsonNode SerializeCombined(int  most_ancient_time,
                                            int  most_recent_time,
                                            unsigned long  tick_span,
                                            const string &  name,
                                            const string &  description) const;
        virtual CJsonNode SerializeSeries(int  most_ancient_time,
                                          int  most_recent_time,
                                          unsigned long  tick_span,
                                          const string &  name,
                                          const string &  description) const;

        unsigned long GetMinBlobSize(void) const
        { return m_MinBlobSize; }

        unsigned long GetMaxBlobSize(void) const
        { return m_MaxBlobSize; }

    private:
        unsigned long      m_MinBlobSize;
        unsigned long      m_MaxBlobSize;
};



class COperationTiming
{
    public:
        COperationTiming(unsigned long  min_stat_value,
                         unsigned long  max_stat_value,
                         unsigned long  n_bins,
                         const string &  stat_type,
                         unsigned long  small_blob_size,
                         const string &  only_for_processor,
                         const map<string, size_t> &  proc_group_to_index);
        ~COperationTiming() {}

    public:
        // Blob size is taken into consideration only if
        // operation == eBlobRetrieve
        uint64_t Register(IPSGS_Processor *  processor,
                          EPSGOperation  operation,
                          EPSGOperationStatus  status,
                          const psg_time_point_t &  op_begin_ts,
                          size_t  blob_size=0);
        void RegisterForTimeSeries(CPSGS_Request::EPSGS_Type  request_type,
                                   CRequestStatus::ECode  status);
        void RegisterProcessorDone(CPSGS_Request::EPSGS_Type  request_type,
                                   IPSGS_Processor *  processor);
        void RegisterProcessorPerformance(IPSGS_Processor *  processor,
                                          IPSGS_Processor::EPSGS_Status  proc_finish_status);

    public:
        void Rotate(void);
        void RotateRequestStat(void);
        void RotateAvgPerfTimeSeries(void);
        void CollectMomentousStat(size_t  tcp_conn_count,
                                  size_t  active_request_count,
                                  size_t  backlog_count);
        void Reset(void);
        CJsonNode Serialize(int  most_ancient_time,
                            int  most_recent_time,
                            const vector<CTempString> &  histogram_names,
                            const vector<pair<int, int>> &   time_series,
                            unsigned long  tick_span) const;

    private:
        bool x_SetupBlobSizeBins(unsigned long  min_stat_value,
                                 unsigned long  max_stat_value,
                                 unsigned long  n_bins,
                                 TOnePSGTiming::EScaleType  stat_type,
                                 unsigned long  small_blob_size);
        ssize_t x_GetBlobRetrievalBinIndex(unsigned long  blob_size);
        void x_UpdateMaxReqsStat(size_t  index,
                                 uint64_t &  max_requests,
                                 uint64_t &  max_errors,
                                 uint64_t &  max_warnings,
                                 uint64_t &  max_not_found) const;
        size_t x_GetLogThreshold(const string &  proc_group_name);

    private:
        string                                              m_OnlyForProcessor;
        string                                              m_TooLongIDs[eOperationLast];

        // Note: 2 items, found and not found
        vector<unique_ptr<CLmdbCacheTiming>>                m_LookupLmdbSi2csiTiming;
        vector<unique_ptr<CLmdbCacheTiming>>                m_LookupLmdbBioseqInfoTiming;
        vector<unique_ptr<CLmdbCacheTiming>>                m_LookupLmdbBlobPropTiming;
        vector<unique_ptr<CCassTiming>>                     m_LookupCassSi2csiTiming;
        vector<unique_ptr<CCassTiming>>                     m_LookupCassBioseqInfoTiming;
        vector<unique_ptr<CCassTiming>>                     m_LookupCassBlobPropTiming;
        vector<unique_ptr<CMyNCBITiming>>                   m_RetrieveMyNCBITiming;
        unique_ptr<CMyNCBITiming>                           m_RetrieveMyNCBIErrorTiming;

        vector<unique_ptr<CLmdbResolutionTiming>>           m_ResolutionLmdbTiming;
        vector<unique_ptr<CCassResolutionTiming>>           m_ResolutionCassTiming;

        vector<vector<unique_ptr<CNARetrieveTiming>>>       m_NARetrieveTiming;
        vector<vector<unique_ptr<CTSEChunkRetrieveTiming>>> m_TSEChunkRetrieveTiming;

        // It makes sense only for SNP and CDD processors
        // The space will be reserved for all the processors however:
        // - only SNP and CDD processor actually make calls for that
        // - at the time of serializing only SNP and CDD will be sent
        vector<vector<unique_ptr<CNAResolveTiming>>>        m_NAResolveTiming;

        vector<unique_ptr<CSplitHistoryRetrieveTiming>>     m_SplitHistoryRetrieveTiming;
        vector<unique_ptr<CPublicCommentRetrieveTiming>>    m_PublicCommentRetrieveTiming;
        vector<unique_ptr<CAccVerHistoryRetrieveTiming>>    m_AccVerHistoryRetrieveTiming;
        vector<unique_ptr<CIPGResolveRetrieveTiming>>       m_IPGResolveRetrieveTiming;
        vector<unique_ptr<CVDBOpenTiming>>                  m_VDBOpenTiming;
        vector<unique_ptr<CSNPPTISLookupTiming>>            m_SNPPTISLookupTiming;
        vector<unique_ptr<CWGSVDBLookupTiming>>             m_WGSVDBLookupTiming;

        // The index is calculated basing on the blob size
        vector<vector<unique_ptr<CBlobRetrieveTiming>>>     m_BlobRetrieveTiming;
        vector<unsigned long>                               m_Ends;
        vector<unique_ptr<CHugeBlobRetrieveTiming>>         m_HugeBlobRetrievalTiming;
        vector<unique_ptr<CNotFoundBlobRetrieveTiming>>     m_NotFoundBlobRetrievalTiming;
        vector<vector<uint64_t>>                            m_BlobByteCounters;
        vector<uint64_t>                                    m_HugeBlobByteCounter;

        // Resolution timing
        vector<unique_ptr<CResolutionTiming>>               m_ResolutionErrorTiming;
        vector<unique_ptr<CResolutionTiming>>               m_ResolutionNotFoundTiming;
        vector<unique_ptr<CResolutionTiming>>               m_ResolutionFoundTiming;

        unique_ptr<CBacklogTiming>                          m_BacklogTiming;

        // 1, 2, 3, 4, 5+ trips to cassandra
        vector<unique_ptr<CResolutionTiming>>               m_ResolutionFoundCassandraTiming;

        struct SInfo {
            CPSGTimingBase *    m_Timing;
            string              m_Name;
            string              m_Description;

            // Specific for blob retrieval. They also have a cummulative
            // counter for the blob bytes sent to the user.
            uint64_t *          m_Counter;
            string              m_CounterId;
            string              m_CounterName;
            string              m_CounterDescription;

            SInfo() :
                m_Timing(nullptr),
                m_Counter(nullptr)
            {}

            SInfo(CPSGTimingBase *  timing,
                  const string &  name, const string &  description) :
                m_Timing(timing), m_Name(name), m_Description(description),
                m_Counter(nullptr)
            {}

            SInfo(CPSGTimingBase *  timing,
                  const string &  name, const string &  description,
                  uint64_t *  counter,
                  const string &  counter_id,
                  const string &  counter_name,
                  const string &  counter_description) :
                m_Timing(timing), m_Name(name), m_Description(description),
                m_Counter(counter),
                m_CounterId(counter_id),
                m_CounterName(counter_name),
                m_CounterDescription(counter_description)
            {}

            SInfo(const SInfo &) = default;
            SInfo & operator=(const SInfo &) = default;
            SInfo(SInfo &&) = default;
            SInfo & operator=(SInfo &&) = default;
        };
        map<string, SInfo>          m_NamesMap;

        map<string, size_t>         m_ProcGroupToIndex;
        map<size_t, size_t>         m_ProcIndexToLogTimingThreshold;

        mutable mutex               m_Lock; // reset-rotate-serialize lock

        CRequestTimeSeries          m_IdGetStat;
        CRequestTimeSeries          m_IdGetblobStat;
        CRequestTimeSeries          m_IdResolveStat;
        CRequestTimeSeries          m_IdAccVerHistStat;
        CRequestTimeSeries          m_IdGetTSEChunkStat;
        CRequestTimeSeries          m_IdGetNAStat;
        CRequestTimeSeries          m_IpgResolveStat;

        CMomentousCounterSeries     m_TCPConnectionsStat;
        CMomentousCounterSeries     m_ActiveRequestsStat;
        CMomentousCounterSeries     m_BacklogStat;

        // The items below collect only the fact (per processor) that the
        // processor did something for the request i.e. finished with status
        // 'Done'
        vector<unique_ptr<CProcessorRequestTimeSeries>>     m_IdGetDoneByProc;
        vector<unique_ptr<CProcessorRequestTimeSeries>>     m_IdGetblobDoneByProc;
        vector<unique_ptr<CProcessorRequestTimeSeries>>     m_IdResolveDoneByProc;
        vector<unique_ptr<CProcessorRequestTimeSeries>>     m_IdGetTSEChunkDoneByProc;
        vector<unique_ptr<CProcessorRequestTimeSeries>>     m_IdGetNADoneByProc;

        CMonotonicCounterSeries                             m_ErrorTimeSeries;
        CMonotonicCounterSeries                             m_WarningTimeSeries;
        vector<unique_ptr<CMonotonicCounterSeries>>         m_OpTooLongByProc;
        vector<unique_ptr<COpTooLongTiming>>                m_OpTooLongTimingByProc;

        // The first index is a request kind
        // The second index is a processor kind
        vector<vector<unique_ptr<CProcessorPerformanceTiming>>>     m_DoneProcPerformance;
        vector<vector<unique_ptr<CProcessorPerformanceTiming>>>     m_NotFoundProcPerformance;
        vector<vector<unique_ptr<CProcessorPerformanceTiming>>>     m_TimeoutProcPerformance;
        vector<vector<unique_ptr<CProcessorPerformanceTiming>>>     m_ErrorProcPerformance;
};

#endif /* PUBSEQ_GATEWAY_TIMING__HPP */

