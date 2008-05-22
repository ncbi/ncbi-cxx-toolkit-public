#ifndef CORELIB___TEST_BOOST__HPP
#define CORELIB___TEST_BOOST__HPP

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
 * Author:  Pavel Ivanov
 *
 */

/// @file test_boost.hpp
///   Utility stuff for more convenient using of Boost.Test library.
///
/// This header must be included before any Boost.Test header
/// (if you have any).


#include <corelib/ncbistd.hpp>

/// Redefinition of the name of global function used in Boost.Test for
/// initialization. It is called now from NCBI wrapper function called before
/// user-defined.
#define init_unit_test_suite  NcbiInitUnitTestSuite


// Keep Boost's inclusion of <limits> from breaking under old WorkShop versions.
#if defined(numeric_limits)  &&  defined(NCBI_NUMERIC_LIMITS)
#  undef numeric_limits
#endif

#include <boost/test/auto_unit_test.hpp>

#include <string>


/** @addtogroup Tests
 *
 * @{
 */


BEGIN_NCBI_SCOPE


/// Type of function with unit tests initialization code
typedef void (*TNcbiBoostInitFunc)(void);

/// Macro defining initialization function which will be called before
/// tests execution. The usage of this macro is similar to
/// BOOST_AUTO_TEST_CASE, i.e.:
/// BOOST_AUTO_INITIALIZATION(FunctionName)
/// {
///     // initialization function body
/// }
#define BOOST_AUTO_INITIALIZATION(name)                                     \
static void name(void);                                                     \
static ::NCBI_NS_NCBI::SNcbiBoostIniter BOOST_JOIN(name, _initer) (&name);  \
static void name(void)

/// Registrar of all initialization functions
void RegisterNcbiBoostInit(TNcbiBoostInitFunc func);

/// Class for implementing automatic registration of init-functions
struct SNcbiBoostIniter
{
    SNcbiBoostIniter(TNcbiBoostInitFunc func)
    {
        RegisterNcbiBoostInit(func);
    }
};

/// Define some test as disabled in current configuration
/// This function should be called for every existing test which is not added
/// to Boost's test suite for execution. Such tests will be included in
/// detailed report given by Boost after execution.
///
/// @param test_name
///   Name of the disabled test
/// @param reason
///   Reason of test disabling. It will be printed in the report.
extern void NcbiBoostTestDisable(CTempString  test_name,
                                 CTempString  reason = "");


END_NCBI_SCOPE


/* @} */

#endif  /* CORELIB___TEST_BOOST__HPP */
