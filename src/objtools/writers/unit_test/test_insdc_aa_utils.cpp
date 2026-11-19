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
* Author: Justin Foley
*
* File Description:
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <objects/seqfeat/Trna_ext.hpp>
#include <util/compile_time.hpp>

#include "../insdc_aa_utils.hpp"
#include <corelib/test_boost.hpp>
#include <array>


USING_NCBI_SCOPE;
USING_SCOPE(objects);

BOOST_AUTO_TEST_CASE(CHECK_INVALID_OR_MISSING_AA)
{
    BOOST_CHECK_EQUAL(g_GetAminoAcid(-1), "other");
    BOOST_CHECK_EQUAL(g_GetAminoAcid(28), "other");

    auto pTrnaExt = Ref(new CTrna_ext());
    BOOST_CHECK(!g_GetAminoAcid(*pTrnaExt));
    
    pTrnaExt->SetAa();
    BOOST_CHECK(!g_GetAminoAcid(*pTrnaExt));
}

BOOST_AUTO_TEST_CASE(CHECK_IUPACaa)
{
    auto pTrnaExt = Ref(new CTrna_ext());
    auto& aa = pTrnaExt->SetAa();

    MAKE_CONST_MAP(IupacMap, char, ct::tagStrCase,
    {
        {'A', "alanine"},
        {'B', "aspartic_acid or asparagine"},
        {'C', "cysteine"},
        {'D', "aspartic_acid"},
        {'E', "glutamic_acid"},
        {'F', "phenylalanine"},
        {'G', "glycine"},
        {'H', "histidine"},
        {'I', "isoleucine"},
        {'J', "leucine or isoleucine"},
        {'K', "lysine"},
        {'L', "leucine"},
        {'M', "methionine"},
        {'N', "asparagine"},
        {'O', "pyrrolysine"},
        {'P', "proline"},
        {'Q', "glutamine"},
        {'R', "arginine"},
        {'S', "serine"},
        {'T', "threonine"},
        {'V', "valine"},
        {'W', "tryptophan"},
        {'X', "other"},
        {'Y', "tyrosine"},
        {'Z', "glutamic_acid or glutamine"}
    });

    for (auto [first, second] : IupacMap) {
        aa.SetIupacaa(first);
        BOOST_CHECK_EQUAL(*g_GetAminoAcid(*pTrnaExt), second);
    }
}



BOOST_AUTO_TEST_CASE(CHECK_NCBIeaa)
{
    auto pTrnaExt = Ref(new CTrna_ext());
    auto& aa = pTrnaExt->SetAa();

    MAKE_CONST_MAP(NcbieaaMap, char, ct::tagStrCase,
    {
        {'A', "alanine"},
        {'B', "aspartic_acid or asparagine"},
        {'C', "cysteine"},
        {'D', "aspartic_acid"},
        {'E', "glutamic_acid"},
        {'F', "phenylalanine"},
        {'G', "glycine"},
        {'H', "histidine"},
        {'I', "isoleucine"},
        {'J', "leucine or isoleucine"},
        {'K', "lysine"},
        {'L', "leucine"},
        {'M', "methionine"},
        {'N', "asparagine"},
        {'O', "pyrrolysine"},
        {'P', "proline"},
        {'Q', "glutamine"},
        {'R', "arginine"},
        {'S', "serine"},
        {'T', "threonine"},
        {'U', "selenocysteine"},
        {'V', "valine"},
        {'W', "tryptophan"},
        {'X', "other"},
        {'Y', "tyrosine"},
        {'Z', "glutamic_acid or glutamine"},
        {'*', "stop_codon"},
        {'-', "other"}
    });

    for (auto [first, second] : NcbieaaMap) {
        aa.SetNcbieaa(first);
        BOOST_CHECK_EQUAL(*g_GetAminoAcid(*pTrnaExt), second);
    }
}


BOOST_AUTO_TEST_CASE(CHECK_NCBIstdaa)
{
    auto pTrnaExt = Ref(new CTrna_ext());
    auto& aa = pTrnaExt->SetAa();
    static constexpr array<string_view, 28> ncbistdaa = {
        "other",
        "alanine",
        "aspartic_acid or asparagine",
        "cysteine",
        "aspartic_acid",
        "glutamic_acid",
        "phenylalanine",
        "glycine",
        "histidine",
        "isoleucine",
        "lysine",
        "leucine",
        "methionine",
        "asparagine",
        "proline",
        "glutamine",
        "arginine",
        "serine",
        "threonine",
        "valine",
        "tryptophan",
        "other",
        "tyrosine",
        "glutamic_acid or glutamine",
        "selenocysteine",
        "stop_codon",
        "pyrrolysine",
        "leucine or isoleucine"
    };

    int index {0};
    for (auto name : ncbistdaa) {
        aa.SetNcbistdaa(index++);
        BOOST_CHECK_EQUAL(*g_GetAminoAcid(*pTrnaExt), name);
    }
}


