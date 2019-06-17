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

#include <util/compile_time.hpp>
#include <corelib/test_boost.hpp>

#include <common/test_assert.h>  /* This header must go last */


NCBITEST_AUTO_INIT()
{
    boost::unit_test::framework::master_test_suite().p_name->assign(
        "compile time const_map Unit Test");
}

BOOST_AUTO_TEST_CASE(TestConstCharset1)
{
    constexpr
        ct::const_xlate_table<int>::init_pair_t init_charset[] = {
            {500, {'A', 'B', 'C'}},
            {700, {'x', 'y', 'z'}},
            {400, {"OPRS"}}
    };

    constexpr
        ct::const_xlate_table<int> test_xlate(100, init_charset);

    BOOST_CHECK(test_xlate['A'] == 500);
    BOOST_CHECK(test_xlate['B'] == 500);
    BOOST_CHECK(test_xlate['C'] == 500);
    BOOST_CHECK(test_xlate['D'] == 100);
    BOOST_CHECK(test_xlate['E'] == 100);
    BOOST_CHECK(test_xlate[0] == 100);
    BOOST_CHECK(test_xlate['X'] == 100);
    BOOST_CHECK(test_xlate['x'] == 700);
    BOOST_CHECK(test_xlate['P'] == 400);
}

using xlate_call = std::string(*)(const char*);

static std::string call1(const char* s)
{
    return s;
}
static std::string call2(const char* s)
{
    return std::string(s) + ":" + s;
}

BOOST_AUTO_TEST_CASE(TestConstCharset2)
{
    //xlate_call c1 = [](const char*s)->std::string { return s; };
    //xlate_call c2 = [](const char*s)->std::string { return std::string(s) + ":" + s; };

    constexpr ct::const_xlate_table<xlate_call>::init_pair_t init_charset[] = {
            {call1, {'A', 'B', 'c'} },
            {call2, {'x', 'y', 'z'} }
    };
    constexpr ct::const_xlate_table<xlate_call> test_xlate(nullptr, init_charset);

    BOOST_CHECK(test_xlate['P'] == nullptr);
    BOOST_CHECK(test_xlate['A']("alpha") == "alpha");
    BOOST_CHECK(test_xlate['x']("beta") == "beta:beta");
}

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

using bitset = ct::const_bitset<64 * 3, int>;
using bitset_pair = ct::const_pair<int, bitset>;

BOOST_AUTO_TEST_CASE(TestConstBitset)
{
    // this test ensures various approaches of instantiating const bitsets
    constexpr bitset t1(1, 10, 20, 30);
    constexpr bitset t2{ 1, 10, 20, 30 };

    constexpr bitset t3[] = { { 5,10}, {15, 10} };
    constexpr bitset_pair t4{ 4, t1 };
    constexpr bitset_pair t5(t4);
    constexpr bitset_pair t6[] = { {4, t1}, {5, t2} };
    constexpr bitset_pair t7[] = { {4, bitset(1, 2)}, {5, t3[0]} };

    constexpr bitset t8 = bitset::set_range(5, 12);

    BOOST_CHECK(t1.test(10));
    BOOST_CHECK(t2.test(20));
    BOOST_CHECK(!t3[1].test(11));
    BOOST_CHECK(!t4.second.test(11));
    BOOST_CHECK(t5.second.test(10));
    BOOST_CHECK(t6[1].second.test(1));
    BOOST_CHECK(t7[0].second.test(2));
    BOOST_CHECK(t8.test(5));
    BOOST_CHECK(t8.test(12));
    BOOST_CHECK(!t8.test(4));
    BOOST_CHECK(!t8.test(13));

    // array of pairs, all via aggregate initialization
    constexpr 
    bitset_pair t9[] = {
        {4,                //first
            {5, 6, 7, 8}}, //second
        {5,                //first
            {4, 5}}        //second
    };

    BOOST_CHECK(t9[1].second.test(5));
}

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

BOOST_AUTO_TEST_CASE(TestCRC32)
{
    constexpr auto hash_good_cs = ct::SaltedCRC32<true>::ct("Good");
    constexpr auto hash_good_ncs = ct::SaltedCRC32<false>::ct("Good");
    static_assert(hash_good_cs != hash_good_ncs, "not good");

    static_assert(948072359 == hash_good_cs, "not good");
    static_assert(
        ct::SaltedCRC32<false>::ct("Good") == ct::SaltedCRC32<true>::ct("good"),
        "not good");

    BOOST_CHECK(hash_good_cs  == ct::SaltedCRC32<true>::general("Good", 4));
    BOOST_CHECK(hash_good_ncs == ct::SaltedCRC32<false>::general("Good", 4));
}
