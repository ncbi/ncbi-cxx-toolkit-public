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
 *   Declaration of CRangeList (list of integer ranges).
 *
 */

/// @file uttp.hpp
/// This file contains declaration of CRangeList.

#ifndef UTIL___RANGELIST__HPP
#define UTIL___RANGELIST__HPP

#include <corelib/ncbistd.hpp>

BEGIN_NCBI_SCOPE

/// @deprecated
class NCBI_DEPRECATED NCBI_XUTIL_EXPORT CRangeList
{
public:
    typedef std::pair<int, int> TIntegerRange;
    typedef std::vector<TIntegerRange> TRangeVector;

    NCBI_DEPRECATED
    const TRangeVector& Parse(const char* init_string,
            const char* config_param_name)
    {
        ParseImpl(init_string, config_param_name, &m_RangeVector);
        return m_RangeVector;
    }

    const TRangeVector& GetRangeList() const {return m_RangeVector;}

    NCBI_DEPRECATED
    static void Parse(const char* init_string,
            const char* config_param_name,
            TRangeVector* range_vector)
    {
        return ParseImpl(init_string, config_param_name, range_vector);
    }

private:
    TRangeVector m_RangeVector;

    friend class CDiscreteDistribution;
    static void ParseImpl(const char* init_string,
            const char* config_param_name,
            TRangeVector* range_vector);
};

END_NCBI_SCOPE

#endif /* UTIL___RANGELIST__HPP */
