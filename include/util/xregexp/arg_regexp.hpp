#ifndef UTIL___ARG_REGEXP__HPP
#define UTIL___ARG_REGEXP__HPP

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
 * Author:  Denis Vakatov
 *
 */

/// @file arg_regexp.hpp
/// 
/// CArgAllow_Regexp -- regexp based constraint for argument value

#include <corelib/ncbiargs.hpp>
#include <util/xregexp/regexp.hpp>


/** @addtogroup Args
 *
 * @{
 */

BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
///
/// CArgAllow_Regexp --
///
/// Define constraint to match arg value against Perl regular expression.
///
/// Examples:
/// - To allow only capitalized words (one per argument):
///   SetConstraint("MyArg", new CArgAllow_Regexp("^[A-Z][a-z][a-z]*$"))
///

class NCBI_XREGEXP_EXPORT CArgAllow_Regexp : public CArgAllow
{
public:
    /// @param pattern
    ///   Perl regexp pattern that the argument value must match
    /// @sa CRegexp
    CArgAllow_Regexp(const string& pattern);

    virtual ~CArgAllow_Regexp(void);

    /// @param value
    ///   Argument value to match against the Perl regular expression
    /// @sa CArgAllow_Regexp()
    virtual bool Verify(const string& value) const;

    /// Get usage information.
    virtual string GetUsage(void) const;

    /// Print constraints in XML format
    virtual void PrintUsageXml(CNcbiOstream& out) const;

    virtual CArgAllow* Clone(void) const;

private:
    const string  m_Pattern;  ///< Regexp pattern to match against
    CRegexp       m_Regexp;   ///< Pre-compiled regexp
};


END_NCBI_SCOPE

/* @} */

#endif  /* UTIL___ARG_REGEXP__HPP */
