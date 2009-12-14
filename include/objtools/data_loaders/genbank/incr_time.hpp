#ifndef INCR_TIME__HPP_INCLUDED
#define INCR_TIME__HPP_INCLUDED

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

#include <corelib/ncbistd.hpp>

BEGIN_NCBI_SCOPE

class CConfig;

BEGIN_SCOPE(objects)

class NCBI_XREADER_EXPORT CIncreasingTime
{
public:
    CIncreasingTime(double init_time,
                    double max_time,
                    double multiplier,
                    double increment)
        : m_InitTime(init_time),
          m_MaxTime(max_time),
          m_Multiplier(multiplier),
          m_Increment(increment)
        {
        }

    void Init(CConfig& conf,
              const string& driver_name,
              const char* init_param,
              const char* max_param,
              const char* mul_param,
              const char* incr_param);

    double GetTime(int step) const;

protected:
    static double x_GetDoubleParam(CConfig& conf,
                                   const string& driver_name,
                                   const char* param_name,
                                   double default_value);
    
private:
    double m_InitTime, m_MaxTime, m_Multiplier, m_Increment;
};


END_SCOPE(objects)
END_NCBI_SCOPE

#endif // INCR_TIME__HPP_INCLUDED
