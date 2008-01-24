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

/** @file uniform_search_unit_test.cpp
 * Unit tests for the uniform search API
 */

#include <ncbi_pch.hpp>
#include <algo/blast/api/uniform_search.hpp>

// Keep Boost's inclusion of <limits> from breaking under old WorkShop versions.
#if defined(numeric_limits)  &&  defined(NCBI_NUMERIC_LIMITS)
#  undef numeric_limits
#endif

#include <boost/test/auto_unit_test.hpp>

#ifndef BOOST_AUTO_TEST_CASE
#  define BOOST_AUTO_TEST_CASE BOOST_AUTO_UNIT_TEST
#endif

#ifndef SKIP_DOXYGEN_PROCESSING

USING_NCBI_SCOPE;
USING_SCOPE(blast);
USING_SCOPE(objects);

BOOST_AUTO_TEST_CASE(SearchDatabase_RestrictionGiList)
{
    CSearchDatabase::TGiList gis(1, 5);
    CSearchDatabase db("junk", CSearchDatabase::eBlastDbIsProtein);
    db.SetGiListLimitation(gis);
    BOOST_REQUIRE_THROW(db.SetNegativeGiListLimitation(gis), CBlastException);
}

BOOST_AUTO_TEST_CASE(SearchDatabase_Restriction)
{
    CSearchDatabase::TGiList gis(1, 5);
    CSearchDatabase db("junk", CSearchDatabase::eBlastDbIsProtein);
    db.SetNegativeGiListLimitation(gis);
    BOOST_REQUIRE_THROW(db.SetGiListLimitation(gis), CBlastException);
}

#endif /* SKIP_DOXYGEN_PROCESSING */
