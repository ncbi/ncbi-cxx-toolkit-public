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
* Author:  Colleen Bollin
*
* File Description:
*   Simple unit test for CString_constraint.
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <objects/macro/String_constraint.hpp>
#include <objects/macro/String_location.hpp>
#include <objects/macro/Word_substitution.hpp>
#include <objects/macro/Word_substitution_set.hpp>

#include <corelib/ncbiapp.hpp>
#include <corelib/test_boost.hpp>

#include <util/util_misc.hpp>

#include <common/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;
USING_SCOPE(objects);

NCBITEST_AUTO_INIT()
{
}

BOOST_AUTO_TEST_CASE(Test_WordSubstitution)
{
    CWord_substitution word;

    word.SetWord("fruit");
    word.SetSynonyms().push_back("apple");
    word.SetSynonyms().push_back("orange");
    word.SetSynonyms().push_back("pear");
    word.SetSynonyms().push_back("grapefruit");
    word.SetSynonyms().push_back("fruit, canned");

    vector<size_t> match_lens = word.GetMatchLens("fruit, canned", "fruit", 0);
    BOOST_CHECK_EQUAL(match_lens.size(), 1);
    BOOST_CHECK_EQUAL(match_lens[0], 13);
}


BOOST_AUTO_TEST_CASE(Test_SimpleConstraints)
{
    CString_constraint s;

    s.SetMatch_text("cat");
    s.SetMatch_location(eString_location_contains);

    BOOST_CHECK_EQUAL(s.Match("cat"), true);
    BOOST_CHECK_EQUAL(s.Match("catalog"), true);
    BOOST_CHECK_EQUAL(s.Match("the catalog"), true);
    BOOST_CHECK_EQUAL(s.Match("ducat"), true);
    BOOST_CHECK_EQUAL(s.Match("dog"), false);
    BOOST_CHECK_EQUAL(s.Match("dog, cat, cow"), true);

    s.SetMatch_location(eString_location_equals);
    BOOST_CHECK_EQUAL(s.Match("cat"), true);
    BOOST_CHECK_EQUAL(s.Match("catalog"), false);
    BOOST_CHECK_EQUAL(s.Match("the catalog"), false);
    BOOST_CHECK_EQUAL(s.Match("ducat"), false);
    BOOST_CHECK_EQUAL(s.Match("dog"), false);
    BOOST_CHECK_EQUAL(s.Match("dog, cat, cow"), false);

    s.SetMatch_location(eString_location_starts);
    BOOST_CHECK_EQUAL(s.Match("cat"), true);
    BOOST_CHECK_EQUAL(s.Match("catalog"), true);
    BOOST_CHECK_EQUAL(s.Match("the catalog"), false);
    BOOST_CHECK_EQUAL(s.Match("ducat"), false);
    BOOST_CHECK_EQUAL(s.Match("dog"), false);
    BOOST_CHECK_EQUAL(s.Match("dog, cat, cow"), false);

    s.SetMatch_location(eString_location_ends);
    BOOST_CHECK_EQUAL(s.Match("cat"), true);
    BOOST_CHECK_EQUAL(s.Match("catalog"), false);
    BOOST_CHECK_EQUAL(s.Match("the catalog"), false);
    BOOST_CHECK_EQUAL(s.Match("ducat"), true);
    BOOST_CHECK_EQUAL(s.Match("dog"), false);
    BOOST_CHECK_EQUAL(s.Match("dog, cat, cow"), false);

    s.SetMatch_location(eString_location_inlist);
    BOOST_CHECK_EQUAL(s.Match("cat"), true);
    BOOST_CHECK_EQUAL(s.Match("catalog"), false);
    BOOST_CHECK_EQUAL(s.Match("the catalog"), false);
    BOOST_CHECK_EQUAL(s.Match("ducat"), false);
    BOOST_CHECK_EQUAL(s.Match("dog"), false);
    BOOST_CHECK_EQUAL(s.Match("dog,cat,cow"), false); // because list is in constraint

    s.SetMatch_text("dog, cat, cow");
    BOOST_CHECK_EQUAL(s.Match("cat"), true);
    BOOST_CHECK_EQUAL(s.Match("catalog"), false);
    BOOST_CHECK_EQUAL(s.Match("the catalog"), false);
    BOOST_CHECK_EQUAL(s.Match("ducat"), false);
    BOOST_CHECK_EQUAL(s.Match("dog"), true);

    s.SetMatch_location(eString_location_contains);
    s.SetIgnore_punct(true);
    BOOST_CHECK_EQUAL(s.Match("dog cat cow"), true);
    BOOST_CHECK_EQUAL(s.Match("dog  cat cow"), false);
    BOOST_CHECK_EQUAL(s.Match("dogcatcow"), false);
    BOOST_CHECK_EQUAL(s.Match("dog.cat.cow"), false);
    BOOST_CHECK_EQUAL(s.Match("dog,cat,cow"), false);

    s.SetIgnore_space(true);
    BOOST_CHECK_EQUAL(s.Match("dog cat cow"), true);
    BOOST_CHECK_EQUAL(s.Match("dog  cat cow"), true);
    BOOST_CHECK_EQUAL(s.Match("dogcatcow"), true);
    BOOST_CHECK_EQUAL(s.Match("dog.cat.cow"), true);
    BOOST_CHECK_EQUAL(s.Match("dog,cat,cow"), true);

    s.ResetIgnore_punct();
    BOOST_CHECK_EQUAL(s.Match("dog cat cow"), false);
    BOOST_CHECK_EQUAL(s.Match("dog  cat cow"), false);
    BOOST_CHECK_EQUAL(s.Match("dogcatcow"), false);
    BOOST_CHECK_EQUAL(s.Match("dog.cat.cow"), false);
    BOOST_CHECK_EQUAL(s.Match("dog,cat,cow"), true);

    s.Reset();
    s.SetMatch_text("cat");
    s.SetWhole_word(true);
    s.SetMatch_location(eString_location_contains);
    BOOST_CHECK_EQUAL(s.Match("cat"), true);
    BOOST_CHECK_EQUAL(s.Match("catalog"), false);
    BOOST_CHECK_EQUAL(s.Match("the catalog"), false);
    BOOST_CHECK_EQUAL(s.Match("ducat"), false);
    BOOST_CHECK_EQUAL(s.Match("dog"), false);
    BOOST_CHECK_EQUAL(s.Match("dog,cat,cow"), true); 



    string before;
    s.Reset();
    s.SetMatch_text("cat");
    s.SetMatch_location(eString_location_contains);

    before = "cat";
    BOOST_CHECK_EQUAL(s.ReplaceStringConstraintPortionInString(before, "dog"), true);
    BOOST_CHECK_EQUAL(before, "dog");

    before = "catalog";
    BOOST_CHECK_EQUAL(s.ReplaceStringConstraintPortionInString(before, "dog"), true);
    BOOST_CHECK_EQUAL(before, "dogalog");

    before = "the catalog";
    BOOST_CHECK_EQUAL(s.ReplaceStringConstraintPortionInString(before, "dog"), true);
    BOOST_CHECK_EQUAL(before, "the dogalog");

    before = "ducat";
    BOOST_CHECK_EQUAL(s.ReplaceStringConstraintPortionInString(before, "dog"), true);
    BOOST_CHECK_EQUAL(before, "dudog");

    before = "dog, cat, cow";
    BOOST_CHECK_EQUAL(s.ReplaceStringConstraintPortionInString(before, "dog"), true);
    BOOST_CHECK_EQUAL(before, "dog, dog, cow");

    before = "feline";
    BOOST_CHECK_EQUAL(s.ReplaceStringConstraintPortionInString(before, "dog"), false);
    BOOST_CHECK_EQUAL(before, "feline");

}


BOOST_AUTO_TEST_CASE(Test_StringConstraintWithSynonyms)
{
    string text = "The quick brown fox jumped over the lazy dog.";

    CString_constraint s;
    s.SetMatch_location(eString_location_contains);
    s.SetMatch_text("dog leaped");
    CRef<CWord_substitution> subst1(new CWord_substitution("leap", "jump"));
    s.SetIgnore_words().Set().push_back(subst1);
    CRef<CWord_substitution> subst2(new CWord_substitution("dog", "fox"));
    s.SetIgnore_words().Set().push_back(subst2);

    BOOST_CHECK_EQUAL(s.Match(text), true);

    s.Reset();
    s.SetMatch_location(eString_location_equals);
    s.SetMatch_text("A fast beige wolf leaped across a sleepy beagle.");
    CRef<CWord_substitution> article(new CWord_substitution("a", "the"));
    s.SetIgnore_words().Set().push_back(article);
    CRef<CWord_substitution> speedy(new CWord_substitution("fast", "quick"));
    s.SetIgnore_words().Set().push_back(speedy);
    CRef<CWord_substitution> color(new CWord_substitution("beige", "brown"));
    s.SetIgnore_words().Set().push_back(color);
    CRef<CWord_substitution> wild(new CWord_substitution("wolf", "fox"));
    s.SetIgnore_words().Set().push_back(wild);
    CRef<CWord_substitution> hop(new CWord_substitution("leap", "jump"));
    s.SetIgnore_words().Set().push_back(hop);
    CRef<CWord_substitution> direction(new CWord_substitution("across", "over"));
    s.SetIgnore_words().Set().push_back(direction);
    CRef<CWord_substitution> tired(new CWord_substitution("sleepy", "lazy"));
    s.SetIgnore_words().Set().push_back(tired);
    CRef<CWord_substitution> tame(new CWord_substitution("beagle", "dog"));
    s.SetIgnore_words().Set().push_back(tame);

    BOOST_CHECK_EQUAL(s.Match(text), true);

    // won't work if leap is whole word
    hop->SetWhole_word(true);
    BOOST_CHECK_EQUAL(s.Match(text), false);

    // won't work if articles are case sensitive
    hop->SetWhole_word(false);
    article->SetCase_sensitive(true);
    BOOST_CHECK_EQUAL(s.Match(text), false);

}

BOOST_AUTO_TEST_CASE(Test_synonyms)
{
    // string_constraint with ignore-words
    CString_constraint s;
    s.SetMatch_text("Homo sapiens");
    s.SetMatch_location(eString_location_equals);
    s.SetIgnore_space(true);
    s.SetIgnore_punct(true);

    CRef <CWord_substitution> word_sub(new CWord_substitution);
    word_sub->SetWord("Homo sapiens");
    list <string> syns;
    syns.push_back("human");
    syns.push_back("Homo sapien");
    syns.push_back("Homosapiens");
    syns.push_back("Homo-sapiens");
    syns.push_back("Homo spiens");
    syns.push_back("Homo Sapience");
    syns.push_back("homosapein");
    syns.push_back("homosapiens");
    syns.push_back("homosapien");
    syns.push_back("homo_sapien");
    syns.push_back("homo_sapiens");
    syns.push_back("Homosipian");
    word_sub->SetSynonyms() = syns;
    s.SetIgnore_words().Set().push_back(word_sub);

    CRef <CWord_substitution> word_sub2(new CWord_substitution);
    word_sub2->SetWord("sapiens");
    syns.clear();
    syns.push_back("sapien");
    syns.push_back("sapeins");
    syns.push_back("sapein");
    syns.push_back("sapins");
    syns.push_back("sapens");
    syns.push_back("sapin");
    syns.push_back("sapen");
    syns.push_back("sapians");
    syns.push_back("sapian");
    syns.push_back("sapies");
    syns.push_back("sapie");
    word_sub2->SetSynonyms() = syns;
    s.SetIgnore_words().Set().push_back(word_sub2);
    string test = "human";
    BOOST_CHECK_EQUAL(s.Match(test), true);
    test = "humano";
    BOOST_CHECK_EQUAL(s.Match(test), false);
    test = "Homo sapien";
    BOOST_CHECK_EQUAL(s.Match(test), true);
    test = "Human sapien";
    BOOST_CHECK_EQUAL(s.Match(test), false);
    test = "sapien";
    BOOST_CHECK_EQUAL(s.Match(test), false);
}


BOOST_AUTO_TEST_CASE(Test_SQD_2048)
{
    CString_constraint s;
    s.SetMatch_text("cytochrome b gene");
    s.SetMatch_location(eString_location_equals);
    s.SetCase_sensitive(false);
    s.SetIgnore_space(true);
    s.SetIgnore_punct(true);

    CRef<CWord_substitution> subst1(new CWord_substitution());
    subst1->SetWord("cytochrome b gene");
    subst1->SetSynonyms().push_back("cytochrome b cytb");
    subst1->SetSynonyms().push_back("cytochrome b cyt b");
    subst1->SetSynonyms().push_back("cytochrome b (cytb)");
    subst1->SetSynonyms().push_back("cytochrome b (cyt b)");
    subst1->SetCase_sensitive(false);
    subst1->SetWhole_word(false);

    s.SetIgnore_words().Set().push_back(subst1);

    CRef<CWord_substitution> subst2(new CWord_substitution());
    subst2->SetWord("gene");
    subst2->SetSynonyms().push_back("sequence");
    subst2->SetSynonyms().push_back("partial");
    subst2->SetSynonyms().push_back("complete");
    subst2->SetSynonyms().push_back("region");
    subst2->SetSynonyms().push_back("partial sequence");
    subst2->SetSynonyms().push_back("complete sequence");
    subst2->SetCase_sensitive(false);
    subst2->SetWhole_word(false);

    s.SetIgnore_words().Set().push_back(subst2);
    s.SetWhole_word(false);
    s.SetNot_present(false);
    s.SetIs_all_caps(false);
    s.SetIs_all_lower(false);
    s.SetIs_all_punct(false);
    s.SetIgnore_weasel(false);

    BOOST_CHECK_EQUAL(s.Match("cytochrome b gene"), true);
}
