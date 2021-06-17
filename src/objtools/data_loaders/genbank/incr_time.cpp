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
#include <objtools/data_loaders/genbank/impl/incr_time.hpp>
#include <corelib/ncbi_config.hpp>
#include <cmath>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

/////////////////////////////////////////////////////////////////////////////
// CIncreasingTime
/////////////////////////////////////////////////////////////////////////////


void CIncreasingTime::Init(CConfig& conf,
                           const string& driver_name,
                           const SAllParams& params)
{
    m_InitTime = x_GetDoubleParam(conf, driver_name, params.m_Initial);
    m_MaxTime = x_GetDoubleParam(conf, driver_name, params.m_Maximal);
    m_Multiplier = x_GetDoubleParam(conf, driver_name, params.m_Multiplier);
    m_Increment = x_GetDoubleParam(conf, driver_name, params.m_Increment);
}


double CIncreasingTime::x_GetDoubleParam(CConfig& conf,
                                         const string& driver_name,
                                         const SParam& param)
{
    string value = conf.GetString(driver_name,
                                  param.m_ParamName,
                                  CConfig::eErr_NoThrow,
                                  "");
    if ( value.empty() && param.m_ParamName2 ) {
        value = conf.GetString(driver_name,
                               param.m_ParamName2,
                               CConfig::eErr_NoThrow,
                                  "");
    }
    if ( value.empty() ) {
        return param.m_DefaultValue;
    }
    return NStr::StringToDouble(value, NStr::fDecimalPosixOrLocal);
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
