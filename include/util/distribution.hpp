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
 * Authors:
 *   Dmitry Kazimirov
 *
 * File Description:
 *   Declaration of the CDiscreteDistribution class.
 *
 */

/// @file uttp.hpp
/// This file contains declaration of the CDiscreteDistribution class.

#ifndef UTIL___DISTRIBUTION__HPP
#define UTIL___DISTRIBUTION__HPP

#include "rangelist.hpp"

BEGIN_NCBI_SCOPE

class CRandom;

/// @warning do not use this internal class, it will be removed.
class NCBI_XUTIL_EXPORT CDiscreteDistributionImpl
{
public:
    void InitFromParameter(
        const char* parameter_name,
        const char* parameter_value,
        CRandom* random_gen)
    {
        m_RandomGen = random_gen;
        CRangeListImpl::Parse(parameter_value, parameter_name, &m_RangeVector);
    }

    unsigned GetNextValue() const;

private:
    CRandom* m_RandomGen;
    CRangeListImpl::TRangeVector m_RangeVector;
};

/// @deprecated
struct NCBI_DEPRECATED NCBI_XUTIL_EXPORT CDiscreteDistribution :
    CDiscreteDistributionImpl {};

END_NCBI_SCOPE

#endif /* UTIL___DISTRIBUTION__HPP */
