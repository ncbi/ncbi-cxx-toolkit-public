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
 * Authors: Christiam Camacho
 *
 */

/** @file blast_unit_test.cpp
 * Unit tests for the genetic code singleton
 */

#include <ncbi_pch.hpp>
#include <algo/blast/api/sseqloc.hpp>
#include <algo/blast/core/gencode_singleton.h>
#include <algo/blast/api/blast_aux.hpp>

// Keep Boost's inclusion of <limits> from breaking under old WorkShop versions.
#if defined(numeric_limits)  &&  defined(NCBI_NUMERIC_LIMITS)
#  undef numeric_limits
#endif

#define BOOST_AUTO_TEST_MAIN    // this should only be defined here!
#include <boost/test/auto_unit_test.hpp>

#ifndef BOOST_AUTO_TEST_CASE
#  define BOOST_AUTO_TEST_CASE BOOST_AUTO_UNIT_TEST
#endif

#ifndef SKIP_DOXYGEN_PROCESSING

USING_NCBI_SCOPE;
USING_SCOPE(blast);
USING_SCOPE(objects);

BOOST_AUTO_TEST_CASE(GenCodeSingleton_Find)
{
    CAutomaticGenCodeSingleton instance;

    Uint4 gc_id = 1;
    TAutoUint1ArrayPtr gc = FindGeneticCode(gc_id);
    Int2 rv = GenCodeSingletonAdd((Uint4)gc_id, gc.get());
    BOOST_CHECK_EQUAL(rv, 0);

    Uint1* gc_str =  GenCodeSingletonFind(gc_id);
    BOOST_CHECK(gc_str != NULL);

    gc_id = 5;
    gc_str =  GenCodeSingletonFind(gc_id);
    BOOST_CHECK(gc_str == NULL);

    gc = FindGeneticCode(gc_id);
    rv = GenCodeSingletonAdd((Uint4)gc_id, gc.get());
    BOOST_CHECK_EQUAL(rv, 0);
    gc_str =  GenCodeSingletonFind(gc_id);
    BOOST_CHECK(gc_str != NULL);
}

BOOST_AUTO_TEST_CASE(GenCodeSingleton_NonExistentGeneticCode)
{
    CAutomaticGenCodeSingleton instance;

    Uint4 gc_id = 500;
    TAutoUint1ArrayPtr gc = FindGeneticCode(gc_id);
    BOOST_CHECK(gc.get() == NULL);
    Int2 rv = GenCodeSingletonAdd((Uint4)gc_id, gc.get());
    BOOST_CHECK(rv == BLASTERR_INVALIDPARAM);
}
#endif /* SKIP_DOXYGEN_PROCESSING */
