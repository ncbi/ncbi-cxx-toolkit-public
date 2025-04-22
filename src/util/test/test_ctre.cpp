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
 *    compile time regex compatibility test
 *       it is only smoke test that ctre is compilable on a certain platform
 *
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistl.hpp>
#include <util/regexp/ctre/replace.hpp>
#include <util/regexp/ctre/ctre.hpp>

#include <corelib/test_boost.hpp>
#include <cassert>

#include <common/test_assert.h>  /* This header must go last */

using namespace ncbi;

static constexpr auto pattern_P94 = ctll::fixed_string{ ".*sp\\. P94\\([0-9]{4}\\)" };

static bool match_P94(std::string_view sv) noexcept
{
    return ctre::match<pattern_P94>(sv);
}

BOOST_AUTO_TEST_CASE(test_ctre_1)
{
    BOOST_CHECK(!match_P94("AAA"));
    BOOST_CHECK(match_P94("Pseudoalteromonas sp. P94(2023)"));
    BOOST_CHECK(!match_P94("Pseudoalteromonas sp. P94(22023)"));
}


static constexpr auto suspicious_id_re = ctll::fixed_string{ "chromosome|plasmid|mito|chloroplast|apicoplast|plastid|^chr|^lg|\\bnw_|\\bnz_|\\bnm_|\\bnc_|\\bac_|cp\\d\\d\\d\\d\\d\\d|^x$|^y$|^z$|^w$|^mt$|^pltd$|^chl$" };

BOOST_AUTO_TEST_CASE(test_ctre_2)
{
    BOOST_CHECK(!ctre::search<suspicious_id_re>("AAA"));
    BOOST_CHECK(ctre::search<suspicious_id_re>("begin chromosome end"));
    BOOST_CHECK(ctre::search<suspicious_id_re>("chr other"));
    BOOST_CHECK(ctre::search<suspicious_id_re>("lg other"));
}


// This test requires C++ 20 semantics, which is expected to be default
#if __cpp_nontype_template_args > 201411L
namespace ncbi
{
    // string literal to use as "The word"_fs or L"Hello"_fs

    template<compile_time_bits::fixed_string s>
    constexpr auto operator ""_fs()
    {
        return s;
    }
}

static bool check_suspicious_id(std::string_view id)
{
    auto result = ctre::search<suspicious_id_re>
        (id);

    return result;
}

BOOST_AUTO_TEST_CASE(test_ctre_3)
{
    BOOST_CHECK(!check_suspicious_id("AAA"));
    BOOST_CHECK(check_suspicious_id("begin chromosome end"));
    BOOST_CHECK(check_suspicious_id("chr other"));
    BOOST_CHECK(check_suspicious_id("lg other"));
}

namespace detail
{

    typedef bool (*ctre_func_type)(std::string_view);
    template<ctll::fixed_string re_string, typename... Modifiers>
    struct ctre_fixed_function
    {
        static constexpr bool search_func(std::string_view s)
        {
            auto res = ctre::search<re_string, Modifiers...>(s);
            return res;
        }
        static constexpr bool match_func(std::string_view s)
        {
            auto res = ctre::match<re_string, Modifiers...>(s);
            return res;
        }
    };


}

template<ctll::fixed_string re_string>
constexpr detail::ctre_func_type operator""_re()
{
    return &detail::ctre_fixed_function<re_string>::search_func;
}

BOOST_AUTO_TEST_CASE(test_ctre_4)
{
    //constexpr auto re_func = "AAA"_re;

    using map_type1 = ct::const_map<ct::tagStrNocase, std::pair<int, detail::ctre_func_type>>;

    constexpr auto map1 = map_type1::construct({
        {"S1", {3, "AAA"_re} },
        {"S2", {4, "BBB"_re} },
        {"S3", {5, "CCC"_re} },
        {"S4", {6, "DDD"_re} },
    });

    auto res = "AAA"_re("AAA");
    BOOST_CHECK(res);

    auto it = map1.find("S1");
    res = it->second.second("AAA");
    BOOST_CHECK(res);

    it = map1.find("S2");
    res = it->second.second("AAA");
    BOOST_CHECK(!res);

}


template<typename _CharType, size_t N>
bool TestRegexCompilation(const _CharType (&str)[N], size_t error_pos, std::string_view expected)
{
    auto compiled = ct_regex_details::compiled_regex(str);

    if (compiled.error_pos() != error_pos)
        return false;

    if (compiled.real_size() != expected.size())
        return false;

    for (size_t i=0; i<compiled.real_size(); ++i) {
        auto left = compiled[i];
        auto right = expected[i];
        if (right == '.') {
            auto all_coded = ct::inline_bitset<
                ct_regex_details::e_action::group,
                ct_regex_details::e_action::group_name,
                ct_regex_details::e_action::follows,
                ct_regex_details::e_action::precedes,
                ct_regex_details::e_action::input>;
            if  (all_coded.test(left.cmd))
                continue;
            else
                return false;
        }

        if (left.cmd != ct_regex_details::e_action::character)
            return false;

        if (left.detail != uint32_t(right))
            return false;

    }

    return true;
}

template<typename _Res, ct_fixed_string_input_param str1, ct_fixed_string_input_param str2>
constexpr bool TestRegexInvocable()
{
    bool uuuu = std::is_invocable_r_v<bool, decltype(ct::search_replace<str1, str2>), _Res&>;
    return uuuu;
}

BOOST_AUTO_TEST_CASE(test_ctre_replacer_parser)
{
    BOOST_CHECK(TestRegexCompilation("ABC", 0, "ABC"));
    BOOST_CHECK(TestRegexCompilation("$ABC", 1, ""));
    BOOST_CHECK(TestRegexCompilation("${ABC", 5, ""));
    BOOST_CHECK(TestRegexCompilation("${1ABC", 6, ""));
    BOOST_CHECK(TestRegexCompilation("${1}ABC$", 8, ""));
    BOOST_CHECK(TestRegexCompilation("${1}ABC$1234", 8, ""));
    BOOST_CHECK(TestRegexCompilation(L"${1}ABC$1234", 0, ".ABC."));
    BOOST_CHECK(TestRegexCompilation("$1", 0, "."));
    BOOST_CHECK(TestRegexCompilation("${}", 2, ""));
    BOOST_CHECK(TestRegexCompilation("$<>", 2, ""));
    BOOST_CHECK(TestRegexCompilation("${}a", 2, ""));
    BOOST_CHECK(TestRegexCompilation("$<>a", 2, ""));
    BOOST_CHECK(TestRegexCompilation("${1}", 0, "."));
    BOOST_CHECK(TestRegexCompilation("$<A>", 0, ".A"));
    BOOST_CHECK(TestRegexCompilation("$<AB>C", 0, ".ABC"));
    BOOST_CHECK(TestRegexCompilation("$<1>", 0, ".1"));
    BOOST_CHECK(TestRegexCompilation("", 0, ""));

    BOOST_CHECK(TestRegexCompilation("${1234}", 2, ""));
    BOOST_CHECK(TestRegexCompilation(L"${12345}", 0, ".12345"));
    BOOST_CHECK(TestRegexCompilation("${year}-${month}-xx", 0, ".year-.month-xx"));

}

BOOST_AUTO_TEST_CASE(test_ctre_replacer_basic_cxx17)
{
    static constexpr ct::fixed_string re1 {"123"};
    static constexpr ct::fixed_string re2 {L"$1ABC"};
    static constexpr ct::fixed_string re3 {L"$1ABC$2$$"};

    constexpr auto o1 = ct_regex_details::build_replace_runtime<re1>();
    constexpr auto o2 = ct_regex_details::build_replace_runtime<re2>();
    constexpr auto o3 = ct_regex_details::build_replace_runtime<re3>();

    BOOST_CHECK(o1.is_single_action);
    BOOST_CHECK(!o2.is_single_action);
    BOOST_CHECK(!o3.is_single_action);
}

// These tests only work in C++-20 mode
BOOST_AUTO_TEST_CASE(test_ctre_replacer_basic)
{
    auto o1 = ct_regex_details::build_replace_runtime<"123">::build();
    BOOST_CHECK(std::tuple_size_v<decltype(o1)> == 1);
    auto o2 = ct_regex_details::build_replace_runtime<"">::build();
    BOOST_CHECK(std::tuple_size_v<decltype(o2)> == 1);

    constexpr auto rt1 = ct_regex_details::build_replace_runtime<"123">();
    constexpr auto rt2 = ct_regex_details::build_replace_runtime<L"$1ABC">();
    constexpr auto rt3 = ct_regex_details::build_replace_runtime<L"$1ABC$2$$">();
    constexpr auto rt4 = ct_regex_details::build_replace_runtime<"">();

    BOOST_CHECK(rt1.is_single_action);
    BOOST_CHECK(!rt2.is_single_action);
    BOOST_CHECK(!rt3.is_single_action);
    BOOST_CHECK(rt4.is_single_action);

    std::string input("First222 Second 222 Third333");
    std::wstring winput(L"First Second Third");
    //ct::pcre_replace<"ABC">{}.replace()

    ct::search_replace<"333", "">(input);
    BOOST_CHECK_EQUAL(input, "First222 Second 222 Third");
    ct::search_replace<"Second", "xyz">(input);
    BOOST_CHECK_EQUAL(input, "First222 xyz 222 Third");

    ct::search_replace<"\\b(222)\\b", "$1${1}aaa">(input);
    BOOST_CHECK_EQUAL(input, "First222 xyz 222222aaa Third");

    ct::search_replace<L"Second", L"ABC">(winput);
    BOOST_CHECK(winput == L"First ABC Third");

    ct::search_replace<L"\\b(ABC)\\b", L"$1.${1}">(winput);
    BOOST_CHECK(winput == L"First ABC.ABC Third");

    bool v;
    v = TestRegexInvocable<std::string, "A", "B">();
    BOOST_CHECK(v);
    v = TestRegexInvocable<std::wstring, "A", "B">();
    BOOST_CHECK(!v);
    v = TestRegexInvocable<std::string, L"A", L"B">();
    BOOST_CHECK(!v);
    v = TestRegexInvocable<std::wstring, L"A", L"B">();
    BOOST_CHECK(v);

    v = TestRegexInvocable<std::string, "A", L"B">();
    BOOST_CHECK(!v);
    v = TestRegexInvocable<std::string, L"A", "B">();
    BOOST_CHECK(v);
    v = TestRegexInvocable<std::wstring, "A", L"B">();
    BOOST_CHECK(v);
    v = TestRegexInvocable<std::wstring, L"A", "B">();
    BOOST_CHECK(!v);

    v = TestRegexInvocable<std::string, "A", "$0">();
    BOOST_CHECK(v);
    v = TestRegexInvocable<std::string, "A", "$1">();
    //BOOST_CHECK(!v);
    v = TestRegexInvocable<std::string, "(A)", "$1">();
    BOOST_CHECK(v);
    v = TestRegexInvocable<std::string, "(A)", "$2">();
    //BOOST_CHECK(!v);
    v = TestRegexInvocable<std::string, "(A)(B)", "${1}${2}">();
    BOOST_CHECK(v);

    std::cout << "Parsed\n";
    //std::cout << "Parsed: " << sizeof(p1) << ":" << p1.reduced.m_only_chars << "\n";
    //std::cout << "Parsed: " << sizeof(p2) << ":" << p2.reduced.m_only_chars << "\n";
}

BOOST_AUTO_TEST_CASE(test_ctre_replacer_ex)
{
    BOOST_CHECK(TestRegexCompilation("$`", 0, "."));
    BOOST_CHECK(TestRegexCompilation("$'", 0, "."));
    BOOST_CHECK(TestRegexCompilation("$_", 0, "."));

    constexpr auto o1 = ct_regex_details::build_replace_runtime<"$`">();
    constexpr auto o2 = ct_regex_details::build_replace_runtime<"$'">();
    constexpr auto o3 = ct_regex_details::build_replace_runtime<"$_">();

    BOOST_CHECK(o1.is_single_action);
    BOOST_CHECK(o2.is_single_action);
    BOOST_CHECK(o3.is_single_action);


    std::string input("ABCdefXYZ");

    ct::search_replace<"def", "$'">(input);
    BOOST_CHECK_EQUAL(input, "ABCXYZXYZ");
    input = "ABCdefXYZ";
    ct::search_replace<"def", "$`">(input);
    BOOST_CHECK_EQUAL(input, "ABCABCXYZ");
    input = "ABCdefXYZ";
    ct::search_replace<"def", "$_">(input);
    BOOST_CHECK_EQUAL(input, "ABCABCdefXYZXYZ");

}

BOOST_AUTO_TEST_CASE(test_ctre_replacer_named)
{

    std::string input = "2024/01/05";
    auto result = ctre::match<"(?<year>\\d{4})/(?<month>\\d{1,2})/(?<day>\\d{1,2})">(input);

    auto g = result.get<"year">().to_view();
    BOOST_CHECK_EQUAL(g, "2024");

    //auto compiled = ct_regex_details::compiled_regex(1, "${year}-${month}-1");
    //constexpr
    //auto res = ct_regex_details::build_replace_runtime_actions<"${year}-${month}-1">();
    //auto res = ct_regex_details::build_replace_runtime_actions<"aaa${year}${year}          ">();

    ct::search_replace<"(?<year>\\d{4})/(?<month>\\d{1,2})/(?<day>\\d{1,2})", "${year}-${month}-xx">(input);
    BOOST_CHECK_EQUAL(input, "2024-01-xx");


}

BOOST_AUTO_TEST_CASE(test_ctre_mutable_string)
{
    const char ccc4[4] = "aaa";
    char cc4[4] = "aaa";

    std::string s_AAA{"aaa"};
    const std::string& cref_AAA = s_AAA;
    std::string& ref_AAA = s_AAA;

    ct_regex_details::mutable_string s_mut1{s_AAA};

    std::string_view view1 = s_mut1.to_view();
    const std::string& cref1 = s_mut1;
    std::string& ref1 = s_mut1;
    s_mut1 = "bbb";
    view1 = s_mut1.to_view();
    const std::string& cref2 = s_mut1;
    std::string& ref2 = s_mut1;
    s_mut1 = "ccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc";
    view1 = s_mut1.to_view();
    //size_t s1 = s_mut1->size();


    ct_regex_details::mutable_string s_mut2{cref_AAA};
    const std::string& cref3 = s_mut2;
    //std::string& ref3 = s_mut2;
    std::string_view view3 = s_mut2.to_view();
    //size_t s2 = s_mut2->size();


    ct_regex_details::mutable_string s_mut3(s_AAA.data());
    //const std::string& cref4 = s_mut3;
    std::string_view view4 = s_mut3;
    //size_t s3 = s_mut3->size();

    ct_regex_details::mutable_string s_mut4{(const char*)s_AAA.data()};
    std::string_view view5 = s_mut4.to_view();
    //const std::string& cref5 = s_mut4;
    //size_t s4 = s_mut4->size();

    //ct_regex_details::mutable_string<const char*> s_mut5{};

    ct_regex_details::mutable_string m1 {s_AAA};
    ct_regex_details::mutable_string m2 {s_AAA.data()};
    //ct_regex_details::mutable_string m3 {std::move(s_AAA)};
    ct_regex_details::mutable_string m4 {ccc4};
    ct_regex_details::mutable_string m5 {cc4};
    //ct_regex_details::mutable_string m6 {std::string("AAA")};

}

BOOST_AUTO_TEST_CASE(test_ctre_replace_all)
{
    std::string s_input("one    two three  four");
    std::string_view v_input("one    two three  four");
    char* sl_input = s_input.data();
    char s4[4] = "aaa";
    const char cs4[4] = "aaa";

    auto result1 = s_input | ct::search_replace_all<"xxxx", "">;
    auto result2 = v_input | ct::search_replace_all<"( +)", " ">;
    auto result3 = "v_input" | ct::search_replace_all<" ", "">;
    auto result4 = (sl_input) | ct::search_replace_all<" ", "">;

    auto result5 = s4 | ct::search_replace_all<" ", "">;
    auto result6 = cs4 | ct::search_replace_all<" ", "">;

    BOOST_CHECK(result1 == s_input);
    //BOOST_CHECK(result1->size() == 22);
    //BOOST_CHECK(result3->size() == 7);

    BOOST_CHECK(result2 == "one two three four");
    BOOST_CHECK(result3 == "v_input");

    bool cmp1 = ("v_input" | ct::search_replace_all<" ", "">) == "v_input";
    BOOST_CHECK(cmp1);

    bool cmp2 = (v_input | ct::search_replace_all<"( +)", "'$1'">) == "one'    'two' 'three'  'four";
    BOOST_CHECK(cmp2);
}

BOOST_AUTO_TEST_CASE(test_ctre_replace_all_w)
{
    std::wstring s_input(L"one    two three  four");
    std::wstring_view v_input(L"one    two three  four");

    auto result1 = s_input | ct::search_replace_all<"xxxx", L"">;
    auto result2 = v_input | ct::search_replace_all<"( +)", L" ">;
    auto result3 = L"v_input" | ct::search_replace_all<" ", L"">;

    BOOST_CHECK(result1 == s_input);
    BOOST_CHECK(result2 == L"one two three four");
    BOOST_CHECK(result3 == L"v_input");

    bool cmp1 = (L"v_input" | ct::search_replace_all<" ", L"">) == L"v_input";
    BOOST_CHECK(cmp1);

    bool cmp2 = (v_input | ct::search_replace_all<"( +)", L"'$1'">) == L"one'    'two' 'three'  'four";
    BOOST_CHECK(cmp2);

}

BOOST_AUTO_TEST_CASE(test_ctre_replace_pipe)
{
    std::string s_input("one    two three  four");
    std::wstring w_input(L"one    two three  four");

    auto res1 = s_input | ct::search_replace_all<" +", " ">;
    auto res2 = "aa   AA" | ct::search_replace_all<" +", " ">;
    auto res3 = w_input | ct::search_replace_all<L" +", L" ">;
    auto res4 = L"bb    BB" | ct::search_replace_all<L" +", L" ">;

    BOOST_CHECK(res1 == "one two three four");
    BOOST_CHECK(res2 == "aa AA");
    BOOST_CHECK(res3 == L"one two three four");
    BOOST_CHECK(res4 == L"bb BB");

    auto res5 = s_input | ct::search_replace_all<" +", " "> | ct::search_replace_one<"o", "O">;
    auto res6 = res5 | ct::search_replace_one<"e", "E">;
    BOOST_CHECK(res5 == "One two three four");
    BOOST_CHECK(res6 == "OnE two three four");



}

BOOST_AUTO_TEST_CASE(test_ctre_literals)
{
    constexpr auto aa = "AA"_fs;
    constexpr auto laa = L"AA"_fs;

    BOOST_CHECK(aa.to_view() == "AA"sv);
    BOOST_CHECK(laa.to_view() == L"AA"sv);

    std::cout << "aa: " << aa.to_view() << "\n";
}

BOOST_AUTO_TEST_CASE(test_fixed_strings)
{
    ct::fixed_string x1("AA");
    ct::fixed_string x2("BBB");
    ct::fixed_string x3("CCC");

    auto s1 = x1 + x2;
    auto s2 = x1 + x2 + x3;

    BOOST_CHECK( s1.to_view() == "AABBB"sv);
    BOOST_CHECK( s2.to_view() == "AABBBCCC"sv);
}


#endif
