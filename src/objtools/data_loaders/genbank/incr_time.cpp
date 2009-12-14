/*  $Id$
* ===========================================================================
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
* ===========================================================================
*
*  Author:  Eugene Vasilchenko
*
*  File Description: Support for configurable increasing timeout
*
*/

#include <ncbi_pch.hpp>
#include <objtools/data_loaders/genbank/incr_time.hpp>
#include <corelib/ncbi_config.hpp>
#include <cmath>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

/////////////////////////////////////////////////////////////////////////////
// CIncreasingTime
/////////////////////////////////////////////////////////////////////////////


void CIncreasingTime::Init(CConfig& conf,
                           const string& driver_name,
                           const char* init_param,
                           const char* max_param,
                           const char* mul_param,
                           const char* incr_param)
{
    m_InitTime =
        x_GetDoubleParam(conf, driver_name, init_param, m_InitTime);
    m_MaxTime =
        x_GetDoubleParam(conf, driver_name, max_param, m_MaxTime);
    m_Multiplier =
        x_GetDoubleParam(conf, driver_name, mul_param, m_Multiplier);
    m_Increment =
        x_GetDoubleParam(conf, driver_name, incr_param, m_Increment);
}


double CIncreasingTime::x_GetDoubleParam(CConfig& conf,
                                         const string& driver_name,
                                         const char* param_name,
                                         double default_value)
{
    string value = conf.GetString(driver_name,
                                  param_name,
                                  CConfig::eErr_NoThrow,
                                  "");
    if ( value.empty() ) {
        return default_value;
    }
    return NStr::StringToDouble(value);
}


double CIncreasingTime::GetTime(int step) const
{
    double time = m_InitTime;
    if ( step > 0 ) {
        if ( m_Multiplier <= 0 ) {
            time += step * m_Increment;
        }
        else {
            double p = pow(m_Multiplier, step);
            time = time * p + m_Increment * (p - 1) / (m_Multiplier - 1);
        }
    }
    return min(time, m_MaxTime);
}


END_SCOPE(objects)
END_NCBI_SCOPE
