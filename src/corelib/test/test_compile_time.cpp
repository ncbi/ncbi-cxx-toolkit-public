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
 * Authors:  Sergiy Gotvyanskyy
 *
 * File Description:
 *   Test for ct::const_map
 *
 */

#include <ncbi_pch.hpp>

#include <corelib/compile_time.hpp>
#include <corelib/test_boost.hpp>

#include <common/test_assert.h>  /* This header must go last */



MAKE_TWOWAY_CONST_MAP(test_two_way1, false, const char*, const char*,
{
    {"SO:0000001", "region"},
    {"SO:0000002", "sequece_secondary_structure"},
    {"SO:0000005", "satellite_DNA"},
    {"SO:0000013", "scRNA"},
    {"SO:0000035", "riboswitch"},
    {"SO:0000036", "matrix_attachment_site"},
    {"SO:0000037", "locus_control_region"},
    {"SO:0000104", "polypeptide"},
    {"SO:0000110", "sequence_feature"},
    {"SO:0000139", "ribosome_entry_site"}
});


MAKE_TWOWAY_CONST_MAP(test_two_way2, false, const char*, int,
{
    {"SO:0000001", 1},
    {"SO:0000002", 2},
    {"SO:0000005", 5},
    {"SO:0000013", 13},
    {"SO:0000035", 35}
});

BOOST_AUTO_TEST_CASE(TestConstMap)
{
    auto t1 = test_two_way1.find("SO:0000001");
    BOOST_CHECK((t1 != test_two_way1.end()) && (ncbi::NStr::CompareCase(t1->second, "region") == 0));

    auto t2 = test_two_way1.find("SO:0000003");
    BOOST_CHECK(t2 == test_two_way1.end());

    auto t3 = test_two_way1_flipped.find("RegioN");
    BOOST_CHECK((t3 != test_two_way1_flipped.end()) && (ncbi::NStr::CompareNocase(t3->second, "so:0000001") == 0));

    assert(test_two_way1.size() == 10);
    BOOST_CHECK(test_two_way1_flipped.size() == 10);

    auto t4 = test_two_way2.find("SO:0000002");
    BOOST_CHECK((t4 != test_two_way2.end()) && (t4->second == 2));

    auto t5 = test_two_way2.find("SO:0000003");
    BOOST_CHECK((t5 == test_two_way2.end()));

    auto t6 = test_two_way2_flipped.find(5);
    BOOST_CHECK((t6 != test_two_way2_flipped.end()) && (ncbi::NStr::CompareNocase(t6->second, "so:0000005") == 0));

};

NCBITEST_AUTO_INIT()
{
    boost::unit_test::framework::master_test_suite().p_name->assign(
        "compile time const_map Unit Test");
}

//#include <ct/sse_crc32c.cpp>


