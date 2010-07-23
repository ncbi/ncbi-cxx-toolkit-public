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
* Author:  Greg Boratyn
*
* File Description:
*   Unit tests for CLinks class
*
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <algo/cobalt/links.hpp>

// This macro should be defined before inclusion of test_boost.hpp in all
// "*.cpp" files inside executable except one. It is like function main() for
// non-Boost.Test executables is defined only in one *.cpp file - other files
// should not include it. If NCBI_BOOST_NO_AUTO_TEST_MAIN will not be defined
// then test_boost.hpp will define such "main()" function for tests.
//
// Usually if your unit tests contain only one *.cpp file you should not
// care about this macro at all.
//
#undef NCBI_BOOST_NO_AUTO_TEST_MAIN


// This header must be included before all Boost.Test headers if there are any
#include <corelib/test_boost.hpp>

#ifndef SKIP_DOXYGEN_PROCESSING

USING_NCBI_SCOPE;
USING_SCOPE(cobalt);

BOOST_AUTO_TEST_SUITE(links)

BOOST_AUTO_TEST_CASE(TestAddLink)
{
    const int kNumElements = 10;

    CLinks links(kNumElements);

    // adding link with node that has index larger than number of elements
    // causes expection
    BOOST_REQUIRE_THROW(links.AddLink(0, kNumElements + 1, 0.0),
                        CLinksException);
}

BOOST_AUTO_TEST_CASE(TestSort)
{
    const int kNumElements = 10;
    const double kSmallerWeight = 0.0;
    const double kLargerWeight = 0.3;

    CLinks links(kNumElements);

    BOOST_REQUIRE(kSmallerWeight < kLargerWeight);
        
    links.AddLink(0, 1, kSmallerWeight);
    links.AddLink(1, 2, kLargerWeight);
    links.AddLink(1, 3, kLargerWeight + 0.1);

    links.Sort();
    CLinks::SLink_CI first_it = links.begin();
    CLinks::SLink_CI second_it = first_it;
    ++second_it;

    // links must be sorted in ascending order accoring to weights
    BOOST_REQUIRE(first_it->weight < second_it->weight);    
}

BOOST_AUTO_TEST_CASE(TestIsSorted)
{
    const int kNumElements = 10;

    CLinks links(kNumElements);

    // initially links must be unsorted
    BOOST_REQUIRE(!links.IsSorted());
    links.AddLink(0, 1, 0.0);    
    BOOST_REQUIRE(!links.IsSorted());

    // afetr sorting links must be reported as sorted
    links.Sort();
    BOOST_REQUIRE(links.IsSorted());

    // after sorting and adding another link, links must be unsorted
    links.AddLink(0, 2, 0.0);
    BOOST_REQUIRE(!links.IsSorted());
}

BOOST_AUTO_TEST_CASE(TestIsLink)
{
    const int kNumElements = 10;

    CLinks links(kNumElements);
    links.AddLink(1, 0, 0.0);

    // links must be sorted before check can be made
    BOOST_CHECK_THROW(links.IsLink(0, 1), CLinksException);

    links.Sort();

    // existent link
    BOOST_REQUIRE(links.IsLink(0, 1));
    // links are undirected
    BOOST_REQUIRE(links.IsLink(1, 0));
    
    // non existent link
    BOOST_REQUIRE(!links.IsLink(1, 2));
    BOOST_REQUIRE(!links.IsLink(2, 1));
}

BOOST_AUTO_TEST_SUITE_END()

#endif /* SKIP_DOXYGEN_PROCESSING */
