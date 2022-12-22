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


CRequestTimeSeries::CRequestTimeSeries() :
    m_CurrentIndex(0)
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
            break;
        case eError:
            ++m_Errors[current_index];
            ++m_TotalErrors;
            break;
        case eWarning:
            ++m_Warnings[current_index];
            ++m_TotalWarnings;
            break;
        case eNotFound:
            ++m_NotFound[current_index];
            ++m_TotalNotFound;
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
}


void CRequestTimeSeries::Reset(void)
{
    memset(m_Requests, 0, sizeof(m_Requests));
    m_TotalRequests = 0;
    memset(m_Errors, 0, sizeof(m_Errors));
    m_TotalErrors = 0;
    memset(m_Warnings, 0, sizeof(m_Warnings));
    m_TotalWarnings = 0;
    memset(m_NotFound, 0, sizeof(m_NotFound));
    m_TotalNotFound = 0;

    m_CurrentIndex.store(0);
}


CJsonNode  CRequestTimeSeries::Serialize(void) const
{
    size_t      current_index = m_CurrentIndex.load();
    CJsonNode   ret(CJsonNode::NewObjectNode());

    ret.SetInteger("BinCoverageSec", 60);
    ret.SetInteger("TotalRequests", m_TotalRequests);
    ret.SetByKey("RequestsTimeSeries", x_SerializeOneSeries(m_Requests, current_index));
    ret.SetInteger("TotalErrors", m_TotalErrors);
    ret.SetByKey("ErrorsTimeSeries", x_SerializeOneSeries(m_Errors, current_index));
    ret.SetInteger("TotalWarnings", m_TotalErrors);
    ret.SetByKey("WarningsTimeSeries", x_SerializeOneSeries(m_Warnings, current_index));
    ret.SetInteger("TotalNotFound", m_TotalNotFound);
    ret.SetByKey("NotFoundTimeSeries", x_SerializeOneSeries(m_NotFound, current_index));

    return ret;
}


CJsonNode  CRequestTimeSeries::x_SerializeOneSeries(const uint64_t *  values,
                                                    size_t  current_index) const
{
    size_t      index = current_index;
    CJsonNode   ret(CJsonNode::NewArrayNode());

    for ( ;; ) {
        ret.AppendInteger(values[index]);

        if (index == 0)
            break;
        --index;
    }

    index = kSeriesIntervals - 1;
    while (index > current_index) {
        ret.AppendInteger(values[index]);
        --index;
    }

    return ret;
}

