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
 * Authors:  Vladimir Ivanov
 *
 * File Description:  NCBI tests specific functions
 *
 */

#include <ncbi_pch.hpp>

#include <stdlib.h>
#include <corelib/ncbi_test.hpp>
#include <corelib/ncbi_process.hpp>
#include "ncbisys.hpp"

 
#define NCBI_USE_ERRCODE_X   Corelib_System


BEGIN_NCBI_SCOPE


unsigned int CNcbiTest::GetRandomSeed(void)
{
    unsigned int seed;
    const char* seed_str = ::getenv("NCBI_TEST_RANDOM_SEED");
    if (!seed_str || !NStr::StringToNumeric(seed_str, &seed, NStr::fConvErr_NoThrow)) {
        seed = static_cast<unsigned int>(CCurrentProcess::GetPid()) ^
               static_cast<unsigned int>(time(nullptr));
    }
    return seed;
}

unsigned int CNcbiTest::SetRandomSeed(const string& prefix)
{
    unsigned int seed = GetRandomSeed();
    srand(seed);
    string pfx = prefix.empty() ? "R" : (prefix + " r");
    LOG_POST(pfx << "andomization seed value: " << seed);
    return seed;
}


END_NCBI_SCOPE
