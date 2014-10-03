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

/// Parse and store a list of integer ranges.
class NCBI_XUTIL_EXPORT CRangeList
{
public:
    typedef std::pair<int, int> TIntegerRange;
    typedef std::vector<TIntegerRange> TRangeVector;

    /// Parse a string of integer ranges and initialize this object.
    ///
    /// The string must be in the following format:
    ///     N1, R1, N2, ..., Rn, Nm
    ///
    /// Where:
    ///     N1 ... Nm - stand-alone numbers, which are saved as a
    ///                 TIntegerRange with equal ends: [Nm, Nm].
    ///     R1 ... Rn - integer ranges defined as intervals
    ///                 in the form of Nfrom - Nto.
    /// Example:
    ///     4, 6 - 9, 16 - 40, 64
    ///
    /// @param init_string
    ///     Actual initialization string, which must conform
    ///     to the format described above.
    ///
    /// @param config_param_name
    ///     Configuration parameter name. It is used only for
    ///     error reporting to specify the source of the error.
    ///
    /// @return A reference to the initialized range list.
    ///
    const TRangeVector& Parse(const char* init_string,
            const char* config_param_name)
    {
        Parse(init_string, config_param_name, &m_RangeVector);
        return m_RangeVector;
    }

    /// Return the previously initialized list.
    const TRangeVector& GetRangeList() const {return m_RangeVector;}

    /// A static variant of Parse(init_string, config_param_name).
    /// This method can be used to parse the input string into an
    /// external TRangeVector object.
    ///
    /// @param init_string
    ///     Initialization string.
    ///
    /// @param config_param_name
    ///     Configuration parameter name. It is used only for
    ///     error reporting to specify the source of the error.
    ///
    /// @param range_vector
    ///     A pointer to the TRangeVector instance for storing
    ///     the parsed integer ranges.
    ///
    /// @see Parse(init_string, config_param_name)
    ///
    static void Parse(const char* init_string,
            const char* config_param_name,
            TRangeVector* range_vector);

private:
    TRangeVector m_RangeVector;
};

END_NCBI_SCOPE

#endif /* UTIL___RANGELIST__HPP */
