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

// Helper class for calculating delay (usually increasing) between steps
// (e.g. for retrying failed network requests).
// Delay for every new step is calculated as <last-step-delay> * <multiplier> + <increment>.
// The initiai delay is provided via parameters, as well as the maximum.
class NCBI_XUTIL_EXPORT CIncreasingTime
{
public:
    struct SParam
    {
        const char* m_ParamName;  // primary param name
        const char* m_ParamName2; // optional secondary param name
        double m_DefaultValue;    // default param value
    };
    struct SAllParams
    {
        SParam m_Initial;    // value >= 0
        SParam m_Maximal;    // value >= 0
        SParam m_Multiplier; // value > 1.0
        SParam m_Increment;
    };

    CIncreasingTime(const SAllParams& params);

    // Read values from the config or use the default ones.
    void Init(CConfig& conf, const string& driver_name, const SAllParams& params);
    // Read values from the registry section or use the default ones.
    void Init(const CNcbiRegistry& reg, const string& section, const SAllParams& params);

    double GetTime(int step) const;

protected:
    static double x_GetDoubleParam(CConfig& conf,
                                   const string& driver_name,
                                   const SParam& param);
    static double x_GetDoubleParam(const CNcbiRegistry& reg,
                                   const string& driver_name,
                                   const SParam& param);
    
private:
    void x_VerifyParams(void);

    double m_InitTime, m_MaxTime, m_Multiplier, m_Increment;
};


END_NCBI_SCOPE

#endif // INCR_TIME__HPP_INCLUDED
