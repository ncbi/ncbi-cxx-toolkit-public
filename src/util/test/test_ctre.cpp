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
#include <util/regexp/ctre/ctre.hpp>
#include <util/compile_time.hpp>

#include <corelib/test_boost.hpp>
#include <cassert>

#include <common/test_assert.h>  /* This header must go last */

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
//#if __cplusplus > 201703L
static bool check_suspicious_id(std::string_view id)
{
    auto result = ctre::search<"chromosome|plasmid|mito|chloroplast|apicoplast|plastid|^chr|^lg|\\bnw_|\\bnz_|\\bnm_|\\bnc_|\\bac_|cp\\d\\d\\d\\d\\d\\d|^x$|^y$|^z$|^w$|^mt$|^pltd$|^chl$">
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
//#endif


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
