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


CRequestTimeSeries::CRequestTimeSeries()
{
    memset(m_Requests, 0, sizeof(m_Requests));
    m_TotalRequests = 0;
    memset(m_Errors, 0, sizeof(m_Errors));
    m_TotalErrors = 0;
    memset(m_Warnings, 0, sizeof(m_Warnings));
    m_TotalWarnings = 0;
    memset(m_NotFound, 0, sizeof(m_NotFound));
    m_TotalNotFound = 0;
}


void CRequestTimeSeries::Add(EPSGSCounter  counter)
{

}


void CRequestTimeSeries::Rotate(void)
{

}


void CRequestTimeSeries::Reset(void)
{

}


CJsonNode  CRequestTimeSeries::Serialize(void)
{

}

