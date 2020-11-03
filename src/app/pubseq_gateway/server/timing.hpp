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

#include <vector>
#include <mutex>
using namespace std;

#include <util/data_histogram.hpp>
#include <connect/services/json_over_uttp.hpp>
USING_NCBI_SCOPE;


const unsigned long     kMinStatValue = 1;
const unsigned long     kMaxStatValue = 16 * 1024 * 1024;
const unsigned long     kNStatBins = 24;
const string            kStatScaleType = "log";
const unsigned long     kTickSpan = 10;


enum EPSGOperationStatus {
    eOpStatusFound,
    eOpStatusNotFound
};

enum EPSGOperation {
    // Low level: more or less monolit operations
    eLookupLmdbSi2csi,              // (2)
    eLookupLmdbBioseqInfo,          // (2)
    eLookupLmdbBlobProp,            // (2)
    eLookupCassSi2csi,              // (2)
    eLookupCassBioseqInfo,          // (2)
    eLookupCassBlobProp,            // (2)

    eResolutionLmdb,                // (2) From request start
    eResolutionCass,                // (2) From cassandra start

    eResolutionError,               // (1) From request start
    eResolutionNotFound,            // (1) From request start
    eResolutionFound,               // (1) From request start
    eResolutionFoundInCassandra,    // (5) From cassandra start

    eBlobRetrieve,
    eNARetrieve,

    eSplitHistoryRetrieve,
    ePublicCommentRetrieve
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
                             const string &  description);


// The base class for all the collected statistics.
// Individual type of operations will derive from it so that they will be able
// to tune the nins and intervals as they need.
class CPSGTimingBase
{
    public:
        CPSGTimingBase() {}
        virtual ~CPSGTimingBase() {}

    public:
        void Add(uint64_t   mks)
        {
            if (m_PSGTiming)
                m_PSGTiming->Add(mks);
        }

        void Reset(void)
        {
            if (m_PSGTiming)
                m_PSGTiming->Reset();
        }

        void Rotate(void)
        {
            if (m_PSGTiming)
                m_PSGTiming->Rotate();
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

    protected:
        unique_ptr<TPSGTiming>      m_PSGTiming;
};


// LMDB cache operations are supposed to be fast.
// There are three tables covered by LMDB cache.
class CLmdbCacheTiming : public CPSGTimingBase
{
    public:
        CLmdbCacheTiming(unsigned long  min_stat_value,
                         unsigned long  max_stat_value,
                         unsigned long  n_bins,
                         TOnePSGTiming::EScaleType  stat_type,
                         bool &  reset_to_default);
};


// LMDB resolution may involve many tries with the cached tables.
class CLmdbResolutionTiming : public CPSGTimingBase
{
    public:
        CLmdbResolutionTiming(unsigned long  min_stat_value,
                              unsigned long  max_stat_value,
                              unsigned long  n_bins,
                              TOnePSGTiming::EScaleType  stat_type,
                              bool &  reset_to_default);
};



// Cassandra operations are supposed to be slower than LMDB.
// There are three cassandra tables
class CCassTiming : public CPSGTimingBase
{
    public:
        CCassTiming(unsigned long  min_stat_value,
                    unsigned long  max_stat_value,
                    unsigned long  n_bins,
                    TOnePSGTiming::EScaleType  stat_type,
                    bool &  reset_to_default);
};


// Cassandra resolution may need a few tries with a two tables
class CCassResolutionTiming : public CPSGTimingBase
{
    public:
        CCassResolutionTiming(unsigned long  min_stat_value,
                              unsigned long  max_stat_value,
                              unsigned long  n_bins,
                              TOnePSGTiming::EScaleType  stat_type,
                              bool &  reset_to_default);
};


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


// Out of range blob size; should not really happened
class CHugeBlobRetrieveTiming : public CPSGTimingBase
{
    public:
        CHugeBlobRetrieveTiming(unsigned long  min_stat_value,
                                unsigned long  max_stat_value,
                                unsigned long  n_bins,
                                TOnePSGTiming::EScaleType  stat_type,
                                bool &  reset_to_default);
};


// Not found blob
class CNotFoundBlobRetrieveTiming : public CPSGTimingBase
{
    public:
        CNotFoundBlobRetrieveTiming(unsigned long  min_stat_value,
                                    unsigned long  max_stat_value,
                                    unsigned long  n_bins,
                                    TOnePSGTiming::EScaleType  stat_type,
                                    bool &  reset_to_default);
};


// Named annotation retrieval
class CNARetrieveTiming : public CPSGTimingBase
{
    public:
        CNARetrieveTiming(unsigned long  min_stat_value,
                          unsigned long  max_stat_value,
                          unsigned long  n_bins,
                          TOnePSGTiming::EScaleType  stat_type,
                          bool &  reset_to_default);
};


// Split history retrieval
class CSplitHistoryRetrieveTiming : public CPSGTimingBase
{
    public:
        CSplitHistoryRetrieveTiming(unsigned long  min_stat_value,
                                    unsigned long  max_stat_value,
                                    unsigned long  n_bins,
                                    TOnePSGTiming::EScaleType  stat_type,
                                    bool &  reset_to_default);
};


// Public comment retrieval
class CPublicCommentRetrieveTiming : public CPSGTimingBase
{
    public:
        CPublicCommentRetrieveTiming(unsigned long  min_stat_value,
                                     unsigned long  max_stat_value,
                                     unsigned long  n_bins,
                                     TOnePSGTiming::EScaleType  stat_type,
                                     bool &  reset_to_default);
};


// Resolution
class CResolutionTiming : public CPSGTimingBase
{
    public:
        CResolutionTiming(unsigned long  min_stat_value,
                          unsigned long  max_stat_value,
                          unsigned long  n_bins,
                          TOnePSGTiming::EScaleType  stat_type,
                          bool &  reset_to_default);
};


class COperationTiming
{
    public:
        COperationTiming(unsigned long  min_stat_value,
                         unsigned long  max_stat_value,
                         unsigned long  n_bins,
                         const string &  stat_type,
                         unsigned long  small_blob_size);
        ~COperationTiming() {}

    public:
        // Blob size is taken into consideration only if
        // operation == eBlobRetrieve
        void Register(EPSGOperation  operation,
                      EPSGOperationStatus  status,
                      const TPSGS_HighResolutionTimePoint &  op_begin_ts,
                      size_t  blob_size=0);

    public:
        void Rotate(void);
        void Reset(void);
        CJsonNode Serialize(int  most_ancient_time,
                            int  most_recent_time,
                            const vector<CTempString> &  histogram_names,
                            unsigned long  tick_span) const;

    private:
        bool x_SetupBlobSizeBins(unsigned long  min_stat_value,
                                 unsigned long  max_stat_value,
                                 unsigned long  n_bins,
                                 TOnePSGTiming::EScaleType  stat_type,
                                 unsigned long  small_blob_size);
        ssize_t x_GetBlobRetrievalBinIndex(unsigned long  blob_size);

    private:
        // Note: 2 items, found and not found
        vector<unique_ptr<CLmdbCacheTiming>>                m_LookupLmdbSi2csiTiming;
        vector<unique_ptr<CLmdbCacheTiming>>                m_LookupLmdbBioseqInfoTiming;
        vector<unique_ptr<CLmdbCacheTiming>>                m_LookupLmdbBlobPropTiming;
        vector<unique_ptr<CCassTiming>>                     m_LookupCassSi2csiTiming;
        vector<unique_ptr<CCassTiming>>                     m_LookupCassBioseqInfoTiming;
        vector<unique_ptr<CCassTiming>>                     m_LookupCassBlobPropTiming;

        vector<unique_ptr<CLmdbResolutionTiming>>           m_ResolutionLmdbTiming;
        vector<unique_ptr<CCassResolutionTiming>>           m_ResolutionCassTiming;

        vector<unique_ptr<CNARetrieveTiming>>               m_NARetrieveTiming;
        vector<unique_ptr<CSplitHistoryRetrieveTiming>>     m_SplitHistoryRetrieveTiming;
        vector<unique_ptr<CPublicCommentRetrieveTiming>>    m_PublicCommentRetrieveTiming;

        // The index is calculated basing on the blob size
        vector<unique_ptr<CBlobRetrieveTiming>>             m_BlobRetrieveTiming;
        vector<unsigned long>                               m_Ends;
        unique_ptr<CHugeBlobRetrieveTiming>                 m_HugeBlobRetrievalTiming;
        unique_ptr<CNotFoundBlobRetrieveTiming>             m_NotFoundBlobRetrievalTiming;

        // Resolution timing
        unique_ptr<CResolutionTiming>                       m_ResolutionErrorTiming;
        unique_ptr<CResolutionTiming>                       m_ResolutionNotFoundTiming;
        unique_ptr<CResolutionTiming>                       m_ResolutionFoundTiming;

        // 1, 2, 3, 4, 5+ trips to cassandra
        vector<unique_ptr<CResolutionTiming>>               m_ResolutionFoundCassandraTiming;

        struct SInfo {
            CPSGTimingBase *    m_Timing;
            string              m_Name;
            string              m_Description;

            SInfo() :
                m_Timing(nullptr)
            {}

            SInfo(CPSGTimingBase *  timing,
                  const string &  name, const string &  description) :
                m_Timing(timing), m_Name(name), m_Description(description)
            {}

            SInfo(const SInfo &) = default;
            SInfo & operator=(const SInfo &) = default;
            SInfo(SInfo &&) = default;
            SInfo & operator=(SInfo &&) = default;
        };
        map<string, SInfo>                                  m_NamesMap;

        mutable mutex                                       m_Lock; // reset-rotate-serialize lock
};

#endif /* PUBSEQ_GATEWAY_TIMING__HPP */

