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
#include <objects/macro/Suspect_rule.hpp>
#include <objects/macro/Replace_rule.hpp>
#include <objects/macro/Replace_func.hpp>
#include <objects/macro/Simple_replace.hpp>
#include <objects/macro/Search_func.hpp>

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
    
    //NcbiCout << MSerial_AsnText << s;

    BOOST_CHECK_EQUAL(s.Match("cytochrome b gene"), true);
    BOOST_CHECK_EQUAL(s.Match("cytochrome b partial"), true);
    BOOST_CHECK_EQUAL(s.Match("cytb"), false);
}


BOOST_AUTO_TEST_CASE(Test_SQD_2093)
{
    CSuspect_rule rule;

    rule.SetFind().SetString_constraint().SetMatch_text("localisation");
    rule.SetFind().SetString_constraint().SetMatch_location(eString_location_contains);
    rule.SetReplace().SetReplace_func().SetSimple_replace().SetReplace("localization");
    rule.SetReplace().SetReplace_func().SetSimple_replace().SetWhole_string(false);
    rule.SetReplace().SetReplace_func().SetSimple_replace().SetWeasel_to_putative(false);
    rule.SetReplace().SetMove_to_note(false);

    string original = "Localisation of periplasmic protein complexes";
    BOOST_CHECK_EQUAL(rule.GetFind().Match(original), true);
    BOOST_CHECK_EQUAL(rule.ApplyToString(original), true);
    BOOST_CHECK_EQUAL(original, "localization of periplasmic protein complexes");

}


BOOST_AUTO_TEST_CASE(Test_CytochromeOxidase)
{
    CString_constraint s;
    s.SetMatch_text("cytochrome oxidase subunit I gene");
    s.SetMatch_location(eString_location_equals);
    s.SetCase_sensitive(false);
    s.SetIgnore_space(true);
    s.SetIgnore_punct(true);

    CRef<CWord_substitution> subst1(new CWord_substitution());
    subst1->SetWord("cytochrome oxidase subunit I gene");
    subst1->SetSynonyms().push_back("cytochrome oxidase I gene");
    subst1->SetSynonyms().push_back("cytochrome oxidase I");
    subst1->SetSynonyms().push_back("cytochrome subunit I");
    subst1->SetCase_sensitive(false);
    subst1->SetWhole_word(false);

    s.SetIgnore_words().Set().push_back(subst1);

    CRef<CWord_substitution> subst2(new CWord_substitution());
    subst2->SetWord("gene");
    subst2->SetCase_sensitive(false);
    subst2->SetWhole_word(false);
    s.SetIgnore_words().Set().push_back(subst2);
    
    CRef<CWord_substitution> subst3(new CWord_substitution());
    subst3->SetWord("gene");
    /* Instead of having subst2, we can add the line below to subst3, the effect is the same
     * subst3->SetSynonyms().push_back(kEmptyStr);
     */
    subst3->SetSynonyms().push_back("sequence");
    subst3->SetSynonyms().push_back("partial");
    subst3->SetSynonyms().push_back("complete");
    subst3->SetSynonyms().push_back("region");
    subst3->SetSynonyms().push_back("partial sequence");
    subst3->SetSynonyms().push_back("complete sequence");
    subst3->SetCase_sensitive(false);
    subst3->SetWhole_word(false);
    s.SetIgnore_words().Set().push_back(subst3);
    
    CRef<CWord_substitution> subst4(new CWord_substitution());
    subst4->SetWord("oxidase");
    subst4->SetSynonyms().push_back("oxydase");
    subst4->SetCase_sensitive(false);
    subst4->SetWhole_word(false);
    s.SetIgnore_words().Set().push_back(subst4);
    
    s.SetWhole_word(false);
    s.SetNot_present(false);
    s.SetIs_all_caps(false);
    s.SetIs_all_lower(false);
    s.SetIs_all_punct(false);
    s.SetIgnore_weasel(false);
    
    BOOST_CHECK_EQUAL(s.Match("cytochrome oxidase subunit I"), true);
    BOOST_CHECK_EQUAL(s.Match("cytochrome oxydase subunit I"), true);
    BOOST_CHECK_EQUAL(s.Match("cytochrome oxydase subunit I gene"), true);
}

BOOST_AUTO_TEST_CASE(Test_AntigenGene)
{
    CString_constraint s;
    s.SetMatch_text("MHC CLASS II ANTIGEN gene");
    s.SetMatch_location(eString_location_equals);
    s.SetCase_sensitive(false);
    s.SetIgnore_space(true);
    s.SetIgnore_punct(true);

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
    
    BOOST_CHECK_EQUAL(s.Match("MHC CLASS II ANTIGEN gene"), true);
    BOOST_CHECK_EQUAL(s.Match("MHC class II antigen gene"), true);
}

BOOST_AUTO_TEST_CASE(Test_Upper_LowerCases)
{
    CString_constraint s;
    s.SetIs_all_caps(true);

    BOOST_CHECK_EQUAL(s.Match("MHC CLASS ii ANTIGEN gene"), false);
    BOOST_CHECK_EQUAL(s.Match("ANTIGEN"), true);
    BOOST_CHECK_EQUAL(s.Match("ANTIGEN GENE"), true);
    BOOST_CHECK_EQUAL(s.Match("CLASS: ANTIGEN"), true);

    s.SetIs_all_caps(false);
    s.SetIs_all_lower(true);

    BOOST_CHECK_EQUAL(s.Match("MHC CLASS ii ANTIGEN gene"), false);
    BOOST_CHECK_EQUAL(s.Match("antigen"), true);
    BOOST_CHECK_EQUAL(s.Match("antigen gene"), true);
    BOOST_CHECK_EQUAL(s.Match("class: antigen!"), true);
}


BOOST_AUTO_TEST_CASE(Test_NADH_dehydrogenase)
{
    CString_constraint s;
    s.SetMatch_text("NADH dehydrogenase subunit 1 gene");
    s.SetMatch_location(eString_location_equals);
    s.SetCase_sensitive(false);
    s.SetIgnore_space(true);
    s.SetIgnore_punct(true);

    CRef<CWord_substitution> subst1(new CWord_substitution());
    subst1->SetWord("NADH dehydrogenase subunit 1 gene");
    subst1->SetSynonyms().push_back("NADH dehydrogenase subunit 1");
    subst1->SetSynonyms().push_back("NADH dehydrogenase 1 gene");
    subst1->SetSynonyms().push_back("NADH dehydrogenase 1");
    subst1->SetSynonyms().push_back("NADH dehydrogenase subunit 1 protein");
    subst1->SetSynonyms().push_back("NADH dehydrogenase 1 protein");
    subst1->SetCase_sensitive(false);
    subst1->SetWhole_word(false);
    s.SetIgnore_words().Set().push_back(subst1);

    CRef<CWord_substitution> subst2(new CWord_substitution());
    subst2->SetWord("1");
    subst2->SetSynonyms().push_back("one");
    subst2->SetCase_sensitive(false);
    subst2->SetWhole_word(false);
    s.SetIgnore_words().Set().push_back(subst2);

    CRef<CWord_substitution> subst3(new CWord_substitution());
    subst3->SetWord("gene");
    subst3->SetSynonyms().push_back("sequence");
    subst3->SetSynonyms().push_back("partial");
    subst3->SetSynonyms().push_back("complete");
    subst3->SetSynonyms().push_back("region");
    subst3->SetSynonyms().push_back("partial sequence");
    subst3->SetSynonyms().push_back("complete sequence");
    subst3->SetCase_sensitive(false);
    subst3->SetWhole_word(false);
    s.SetIgnore_words().Set().push_back(subst3);

    s.SetWhole_word(false);
    s.SetNot_present(false);
    s.SetIs_all_caps(false);
    s.SetIs_all_lower(false);
    s.SetIs_all_punct(false);
    s.SetIgnore_weasel(false);

    BOOST_CHECK_EQUAL(s.Match("NADH dehydrogenase subunit one sequence"), true);
    BOOST_CHECK_EQUAL(s.Match("NADH dehydrogenase subunit 1 gene"), true);
    BOOST_CHECK_EQUAL(s.Match("NADH dehydrogenase subunit one"), false);
    BOOST_CHECK_EQUAL(s.Match("NADH dehydrogenase subunit 2 gene"), false);
    BOOST_CHECK_EQUAL(s.Match("NADH dehydrogenase subunit sequence"), false);
}

BOOST_AUTO_TEST_CASE(Test_Beta_actinGene)
{
    CString_constraint s;
    s.SetMatch_text("beta-actin gene");
    s.SetMatch_location(eString_location_equals);
    s.SetCase_sensitive(false);
    s.SetIgnore_space(true);
    s.SetIgnore_punct(true);

    CRef<CWord_substitution> subst1(new CWord_substitution());
    subst1->SetWord("beta-actin gene");
    subst1->SetSynonyms().push_back("beta-actin");
    subst1->SetSynonyms().push_back("beta actin");
    subst1->SetSynonyms().push_back("beta actin gene");
    subst1->SetSynonyms().push_back("beta_actin");
    subst1->SetSynonyms().push_back("beta_actin gene");
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

    BOOST_CHECK_EQUAL(s.Match("beta actin"), true);
    BOOST_CHECK_EQUAL(s.Match("beta-actin gene"), true);
    BOOST_CHECK_EQUAL(s.Match("beta_actin sequence"), true);
}

BOOST_AUTO_TEST_CASE(Test_FirstCaps)
{
    CString_constraint s;
    s.SetIs_first_cap(true);

    BOOST_CHECK_EQUAL(s.Match(""), false);
    BOOST_CHECK_EQUAL(s.Match("beta actin"), false);
    BOOST_CHECK_EQUAL(s.Match("beta Actin"), false);
    BOOST_CHECK_EQUAL(s.Match("bEta actin"), false);
    BOOST_CHECK_EQUAL(s.Match("BEta actin"), true);
    BOOST_CHECK_EQUAL(s.Match("Beta-actin Gene"), true);
    BOOST_CHECK_EQUAL(s.Match("?Beta_Actin Gene"), true);
    BOOST_CHECK_EQUAL(s.Match("  Beta actin"), true);
    BOOST_CHECK_EQUAL(s.Match("4"), false);
    BOOST_CHECK_EQUAL(s.Match("-12Beta"), false);

    s.SetIs_first_cap(false);
    s.SetIs_first_each_cap(true);

    BOOST_CHECK_EQUAL(s.Match(""), false);
    BOOST_CHECK_EQUAL(s.Match("beta actin"), false);
    BOOST_CHECK_EQUAL(s.Match("Beta Actin"), true);
    BOOST_CHECK_EQUAL(s.Match("bEta Actin"), false);
    BOOST_CHECK_EQUAL(s.Match(" BEta.Actin"), true);
    BOOST_CHECK_EQUAL(s.Match("Beta-actin Gene"), true); //!!
    BOOST_CHECK_EQUAL(s.Match("Beta-Actin Gene"), true);
    BOOST_CHECK_EQUAL(s.Match("Beta_actin Gene"), false);
    BOOST_CHECK_EQUAL(s.Match("-Beta-actin Gene"), true);
    BOOST_CHECK_EQUAL(s.Match("?Beta_Actin Gene"), true);
    BOOST_CHECK_EQUAL(s.Match(" BETA ACTIN"), true);
    BOOST_CHECK_EQUAL(s.Match("12 Ribosomal RNA"), true);
    BOOST_CHECK_EQUAL(s.Match("12R Ribosomal RNA"), false); //!!
    BOOST_CHECK_EQUAL(s.Match("12r Ribosomal RNA"), false); //!!
}

BOOST_AUTO_TEST_CASE(Test_Matching_OptionalString)
{
    CString_constraint s;
    s.SetMatch_text("16S ribosomal RNA gene");
    s.SetMatch_location(eString_location_equals);
    s.SetCase_sensitive(false);
    s.SetIgnore_space(true);
    s.SetIgnore_punct(true);

    CRef<CWord_substitution> subst1(new CWord_substitution());
    subst1->SetWord("");
    subst1->SetSynonyms().push_back("partial sequence");
    subst1->SetSynonyms().push_back("complete sequence");
    subst1->SetSynonyms().push_back("partial");
    subst1->SetSynonyms().push_back("complete");
    subst1->SetSynonyms().push_back("gene");
    subst1->SetSynonyms().push_back("region");

    subst1->SetCase_sensitive(false);
    subst1->SetWhole_word(false);
    s.SetIgnore_words().Set().push_back(subst1);

    CRef<CWord_substitution> subst2(new CWord_substitution());
    subst2->SetWord("16S");
    subst2->SetSynonyms().push_back("5.8S");
    subst2->SetSynonyms().push_back("12S");
    subst2->SetSynonyms().push_back("18S");
    subst2->SetSynonyms().push_back("23S");
    subst2->SetSynonyms().push_back("28S");

    subst2->SetCase_sensitive(false);
    subst2->SetWhole_word(false);
    s.SetIgnore_words().Set().push_back(subst2);

    CRef<CWord_substitution> subst3(new CWord_substitution());
    subst3->SetWord("gene");
    subst3->SetCase_sensitive(false);
    subst3->SetWhole_word(false);
    s.SetIgnore_words().Set().push_back(subst3);

    s.SetWhole_word(false);
    s.SetNot_present(false);
    s.SetIs_all_caps(false);
    s.SetIs_all_lower(false);
    s.SetIs_all_punct(false);
    s.SetIgnore_weasel(false);

    BOOST_CHECK_EQUAL(s.Match("18S ribosomal RNA gene"), true);
    BOOST_CHECK_EQUAL(s.Match("18S ribosomal RNA gene, partial sequence"), true);
}

BOOST_AUTO_TEST_CASE(Test_Matching_COI)
{
    CString_constraint s;
    s.SetMatch_text("cytochrome oxidase subunit I (COI)");
    s.SetMatch_location(eString_location_equals);
    s.SetCase_sensitive(false);
    s.SetIgnore_space(true);
    s.SetIgnore_punct(true);

    CRef<CWord_substitution> subst1(new CWord_substitution());
    subst1->SetWord("cytochrome oxidase subunit I (COI)");
    subst1->SetSynonyms().push_back("cytochrome oxidase subunit I");
    
    subst1->SetCase_sensitive(false);
    subst1->SetWhole_word(false);
    s.SetIgnore_words().Set().push_back(subst1);

    //BOOST_CHECK_EQUAL(s.Match("cytochrome oxidase subunit I (COI)"), true);// fails
}