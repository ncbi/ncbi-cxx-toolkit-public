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

