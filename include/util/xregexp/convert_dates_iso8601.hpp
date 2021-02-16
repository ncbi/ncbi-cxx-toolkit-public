#ifndef UTIL___CONVERT_DATES_ISO8601__HPP
#define UTIL___CONVERT_DATES_ISO8601__HPP

/* $Id$
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
 * Author:  Alex Kotliarov
 *
 */

/// @file convert_dates_iso8601.hpp
/// Convert dates from an arbitrary format to corresponding ISO 8601.
///
/// Copied and adapted from gpipe/common/read_date_iso8601.[ch]pp

#include <corelib/ncbistl.hpp>
#include <string>
#include <utility>

/** @addtogroup Regexp
 *
 * @{
 */

BEGIN_NCBI_SCOPE


/// Convert dates from an arbitrary format to corresponding ISO 8601.
///
/// @param value
///   String in some arbitrary date format.
/// @return
///   Corresponding string in ISO 8601 format. Contain the same field as an original string,
///   but rearranged to conform ISO 8601.
///   Return empty string on error, if we cannot locate valid date, or it is ambiguous.



NCBI_XREGEXP_EXPORT 
string ConvertDateTo_iso8601(string const& value);


/// Convert dates from an arbitrary format to corresponding ISO 8601, with annotation.
///
/// @param value
///   String in some arbitrary data/time format.
/// @return
///   Return pair of strings where
///      on success:
///        - first string is a tag, marking original format (MM/DD/YYYY, etc);
///        - second string is a date in ISO 8601 format.
///      on failure (we cannot locate valid date, or date is ambiguous)
///        - first string is a tag: ["ambiguous", "miss"];
///        - second string is empty.
/// @sa 
///   ConvertDateTo_iso8601

NCBI_XREGEXP_EXPORT 
pair<string, string> ConvertDateTo_iso8601_and_annotate(string const& value);


END_NCBI_SCOPE

#endif  /* UTIL___CONVERT_DATES_ISO8601__HPP */
