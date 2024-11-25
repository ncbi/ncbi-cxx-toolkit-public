#ifndef NCBI_TEST__HPP
#define NCBI_TEST__HPP

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
 * File Description: NCBI tests specific functions
 *
 */

 /// @file ncbi_test.hpp
 ///
 /// NCBI tests specific functions.
 ///

#include <corelib/ncbicfg.h>
#include <corelib/ncbistr.hpp>

/** @addtogroup Test
 *
 * @{
 */

BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
///
/// CNcbiTest --
///
/// Various functions to use in the NCBI tests.

class NCBI_XNCBI_EXPORT CNcbiTest
{
public:
    /// Return a seed to use with srand() function to randomize
    /// new sequence of pseudo-random integers returned by rand().
    /// This can be a newly generated value or passed by the user.
    /// @note
    ///   For tests it is better to use pseudo-random generators with 
    ///   seed and rand(), instead of std::random_device.
    ///   In common case the later one doesn't allow to reproduce
    ///   generated random numbers, that is not akways good for a test.
    /// @note
    ///   To allow for the test reproducibility (in case of failure) 
    ///   the test seed can be configurable by the user, setting
    ///   $NCBI_TEST_RANDOM_SEED environment variable. 
    ///   GetSeed() automatically read it and returns it value.
    /// @note
    /// 
    /// @sa SetRandomSeed
    /// 
    static unsigned int GetRandomSeed(void);

    /// Same as GetRandomSeed(), but automatically set acquired seed
    /// using srand() and prints it via LOG_POST().
    /// @param prefix
    ///   Optional prefix for a printed message to distinguish seeds
    ///   used for different parts of the test. If the test is complex,
    ///   and executes slow, using separate localized seed may be a good
    ///   idea for easier problems tracking.
    ///   If SetRandomSeed() is used only once per test, the prefix can be omited.
    /// @sa GetRandomSeed
    ///
    static unsigned int SetRandomSeed(const string& prefix = kEmptyStr);
};


/* @} */

END_NCBI_SCOPE

#endif  /* NCBI_TEST__HPP */
