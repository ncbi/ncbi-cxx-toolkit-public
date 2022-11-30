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
const size_t    kSeriesIntervals = 60*24*30;
class CRequestTimeSeries
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
        CJsonNode  Serialize(void) const;
        static EPSGSCounter RequestStatusToCounter(CRequestStatus::ECode  status);

    private:
        CJsonNode x_SerializeOneSeries(const uint64_t *  values) const;

    private:
        uint64_t    m_Requests[kSeriesIntervals];
        uint64_t    m_TotalRequests;
        uint64_t    m_Errors[kSeriesIntervals];
        uint64_t    m_TotalErrors;
        uint64_t    m_Warnings[kSeriesIntervals];
        uint64_t    m_TotalWarnings;
        uint64_t    m_NotFound[kSeriesIntervals];
        uint64_t    m_TotalNotFound;

        size_t      m_CurrentIndex;
};


#endif /* TIME_SERIES_STAT__HPP */

