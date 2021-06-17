#ifndef UTIL___MASK_REGEXP__HPP
#define UTIL___MASK_REGEXP__HPP

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
 * Author:  Vladimir Ivanov
 *
 */

/// @file mask_regexp.hpp
/// 
/// CMaskRegexp -- regexp based class to match string against set of masks.

#include <corelib/ncbistd.hpp>
#include <corelib/ncbi_mask.hpp>
#include <util/xregexp/regexp.hpp>


/** @addtogroup Utility Regexp
 *
 * @{
 */

BEGIN_NCBI_SCOPE


//////////////////////////////////////////////////////////////////////////////
///
/// CMaskRegexp --
///
/// Class to match string against set of masks using regular expressions.
///
/// The empty mask object always correspond to "all is included" case.
/// Throw exceptions on error.
///
class NCBI_XREGEXP_EXPORT CMaskRegexp : public CMask
{
public:
    /// Match string
    ///
    /// @param str
    ///   String to match.
    /// @param use_case
    ///   Whether to do a case sensitive compare (eCase -- default), or a
    ///   case-insensitive compare (eNocase).
    /// @return 
    ///   Return TRUE if string 'str' matches to one of inclusion masks
    ///   and not matches none of exclusion masks, or match masks are
    ///   not specified. Otherwise return FALSE.
    /// @sa
    ///   NStr::MatchesMask, CMask
    bool Match(CTempString str, NStr::ECase use_case = NStr::eCase) const;
};

/* @} */


END_NCBI_SCOPE

#endif /* UTIL___MASK_REGEXP__HPP */
