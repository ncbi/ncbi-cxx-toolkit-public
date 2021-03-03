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
* Author:  Andrea Asztalos, based on code from Colleen Bollin
*
* File Description:
*   Unit tests for the source editing functions
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbi_system.hpp>
#include <corelib/ncbiapp.hpp>
//#include "unit_test_source_edit.hpp"

// This macro should be defined before inclusion of test_boost.hpp in all
// "*.cpp" files inside executable except one. It is like function main() for
// non-Boost.Test executables is defined only in one *.cpp file - other files
// should not include it. If NCBI_BOOST_NO_AUTO_TEST_MAIN will not be defined
// then test_boost.hpp will define such "main()" function for tests.
//
// Usually if your unit tests contain only one *.cpp file you should not
// care about this macro at all.
//
//#define NCBI_BOOST_NO_AUTO_TEST_MAIN


// This header must be included before all Boost.Test headers if there are any
#include <corelib/test_boost.hpp>
#include <objmgr/object_manager.hpp>
#include <objects/seqfeat/seqfeat_macros.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/seqfeat/OrgMod.hpp>
#include <objects/general/Dbtag.hpp>
#include <objtools/unit_test_util/unit_test_util.hpp>

#include <objtools/cleanup/capitalization_string.hpp>

#include <common/test_assert.h>  /* This header must go last */


USING_NCBI_SCOPE;
USING_SCOPE(objects);

extern const string sc_TestEntry_FixOrgnames;

NCBITEST_INIT_TREE()
{
    if ( !CNcbiApplication::Instance()->GetConfig().HasEntry("NCBI", "Data") ) {
    }
}

static bool s_debugMode = false;

NCBITEST_INIT_CMDLINE(arg_desc)
{
    // Here we make descriptions of command line parameters that we are
    // going to use.

    arg_desc->AddFlag( "debug_mode",
        "Debugging mode writes errors seen for each test" );
}

NCBITEST_AUTO_INIT()
{
    // initialization function body

    const CArgs& args = CNcbiApplication::Instance()->GetArgs();
    if (args["debug_mode"]) {
        s_debugMode = true;
    }
}


BOOST_AUTO_TEST_CASE(Test_StateAbbreviation)
{
    string state = kEmptyStr;
    GetStateAbbreviation(state);
    BOOST_CHECK_EQUAL(NStr::IsBlank(state), true);

    state.assign("south  carolina ");
    GetStateAbbreviation(state);
    BOOST_CHECK_EQUAL(state, string("SC"));

    state.assign(" ind ");
    GetStateAbbreviation(state);
    BOOST_CHECK_EQUAL(state, string("IN"));

    state.assign("Saxony");
    GetStateAbbreviation(state);
    BOOST_CHECK_EQUAL(state, string("SAXONY"));
}

BOOST_AUTO_TEST_CASE(Test_USAAndStateAbbreviations1)
{
    CAffil affil;
    CAffil::TStd& std = affil.SetStd();
    std.SetAffil("DuPont Nutrition & Health");
    std.SetCity("Madison");
    std.SetSub("Wisc");
    std.SetCountry("United States");

    bool modified = FixUSAAbbreviationInAffil(affil);
    BOOST_CHECK(modified);
    BOOST_CHECK_EQUAL(affil.GetStd().GetCountry(), "USA");

    modified = false;
    modified = FixStateAbbreviationsInAffil(affil);
    BOOST_CHECK(modified);
    BOOST_CHECK_EQUAL(affil.GetStd().GetSub(), "WI");
}
 

BOOST_AUTO_TEST_CASE(Test_USAAndStateAbbreviations2)
{
    CAffil affil;
    CAffil::TStd& std = affil.SetStd();
    std.SetAffil("DuPont Nutrition & Health");
    std.SetCity("Madison");
    std.SetCountry("United States");
    CCit_sub sub;
    sub.SetAuthors().SetAffil(affil);

    bool modified = FixStateAbbreviationsInCitSub(sub);
    BOOST_CHECK(modified);
    BOOST_CHECK_EQUAL(affil.GetStd().GetCountry(), "USA");

    std.SetSub("Wisc");
    modified = false;
    modified = FixStateAbbreviationsInCitSub(sub);
    BOOST_CHECK(modified);
    BOOST_CHECK_EQUAL(affil.GetStd().GetSub(), "WI");

    std.SetSub("Wisc");
    std.SetCountry("Albania");
    modified = false;
    modified = FixStateAbbreviationsInCitSub(sub);
    BOOST_CHECK(!modified);
    // it will only change the state if the country is 'USA'
}


BOOST_AUTO_TEST_CASE(Test_FixupMouseStrain)
{
    CRef<CBioSource> bsrc(new CBioSource);
    bsrc->SetOrg().SetTaxname("Mus musculus domesticus");
    unit_test_util::SetOrgMod (*bsrc, COrgMod::eSubtype_strain, "strain: icr");
    unit_test_util::SetOrgMod (*bsrc, COrgMod::eSubtype_strain, "new strain:8icr");
    unit_test_util::SetOrgMod (*bsrc, COrgMod::eSubtype_strain, "bad strain");
    unit_test_util::SetOrgMod (*bsrc, COrgMod::eSubtype_strain, "C3h");

    FOR_EACH_ORGMOD_ON_BIOSOURCE (orgmod, *bsrc) {
        if ((*orgmod)->IsSetSubname()) {
            bool changed(false);
            string orig_strain = (*orgmod)->GetSubname();
            changed = FixupMouseStrain(orig_strain);
            if (NStr::StartsWith((*orgmod)->GetSubname(), "strain")) {
                BOOST_CHECK_EQUAL(changed, true);
                BOOST_CHECK_EQUAL(orig_strain, string("strain: ICR"));
            } else if (NStr::StartsWith((*orgmod)->GetSubname(), "new")) {
                // does not change as it is not a whole word
                BOOST_CHECK_EQUAL(changed, false);
                BOOST_CHECK_EQUAL(orig_strain, string("new strain:8icr"));
            } else if (NStr::StartsWith((*orgmod)->GetSubname(), "bad")) {
                BOOST_CHECK_EQUAL(changed, false);
                BOOST_CHECK_EQUAL(orig_strain, string("bad strain"));
            } else if (NStr::StartsWith((*orgmod)->GetSubname(), "C3h")) {
                BOOST_CHECK_EQUAL(changed, true);
                BOOST_CHECK_EQUAL(orig_strain, string("C3H"));
            }
        }
    }

    // the function corrects the strain value independetly from the taxname
    bsrc->SetOrg().SetTaxname("Homo sapiens");
    bsrc->SetOrg().SetOrgname().ResetMod();
    unit_test_util::SetOrgMod (*bsrc, COrgMod::eSubtype_strain, "strain: icr");
    FOR_EACH_ORGMOD_ON_BIOSOURCE (orgmod, *bsrc) {
        if ((*orgmod)->IsSetSubname()) {
            string orig_strain = (*orgmod)->GetSubname();
            bool changed = FixupMouseStrain(orig_strain);
            BOOST_CHECK_EQUAL(changed, true);
            BOOST_CHECK_EQUAL(orig_strain, string("strain: ICR"));
        }
    }
}

BOOST_AUTO_TEST_CASE(Test_CapitalizationFunctions)
{
    string str_test;
    str_test.assign("bad: strain1,strain No.2 strain5,strain10");
    InsertMissingSpacesAfterCommas(str_test);
    InsertMissingSpacesAfterNo(str_test);
    FixCapitalizationInElement(str_test);
    BOOST_CHECK_EQUAL(str_test, string("Bad: Strain1, Strain No. 2 Strain5, Strain10"));

    str_test.assign("region: Dubai In uae");
    FindReplaceString_CountryFixes(str_test);
    FixShortWordsInElement(str_test);
    BOOST_CHECK_EQUAL(str_test, string("Region: Dubai in UAE"));
    
    str_test.assign("country: noplace");
    FindReplaceString_CountryFixes(str_test);
    BOOST_CHECK_EQUAL(str_test, string("country: noplace"));

    str_test.assign("Robert O'hair");
    CapitalizeAfterApostrophe(str_test);
    BOOST_CHECK_EQUAL(str_test, string("Robert O'Hair"));

    str_test.assign("Robert williams");
    CapitalizeAfterApostrophe(str_test);
    BOOST_CHECK_EQUAL(str_test, string("Robert williams"));

    str_test.assign("address: Po box");
    FixAffiliationShortWordsInElement(str_test);
    BOOST_CHECK_EQUAL(str_test, string("Address: PO Box"));

    str_test.assign("Guinea-bissau");
    FixCountryCapitalization(str_test);
    BOOST_CHECK_EQUAL(str_test, string("Guinea-Bissau"));

    str_test.assign("palestine");
    FixCountryCapitalization(str_test);
    BOOST_CHECK_EQUAL(str_test, string("palestine"));

    BOOST_CHECK(GetValidCountryCode(0) == "Afghanistan");
    BOOST_CHECK(GetValidCountryCode(10000).empty());
}


BOOST_AUTO_TEST_CASE(Test_FixOrgnames)
{
    CSeq_entry entry;
    CNcbiIstrstream istr(sc_TestEntry_FixOrgnames);
    istr >> MSerial_AsnText >> entry;

    CRef<CObjectManager> object_manager = CObjectManager::GetInstance();
    CRef<CScope> scope(new CScope(*object_manager));
    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(entry);

    // no particularies in the taxname
    string title("Unpublished Providencia alcalifaciens");
    FixOrgNames(seh, title);
    BOOST_CHECK_EQUAL(title, "Unpublished Providencia alcalifaciens");

    title.assign("Unpublished PROVIDENCIA ALCALIFACIENS");
    FixOrgNames(seh, title);
    BOOST_CHECK_EQUAL(title, "Unpublished Providencia alcalifaciens");

    title.assign("Unpublished [PROVIDENCIA ALCALIFACIENS]");
    FixOrgNames(seh, title);
    BOOST_CHECK_EQUAL(title, "Unpublished [Providencia alcalifaciens]");

    // with brackets
    title.assign("Unpublished [AEGOLIUS ACADICUS]");
    FixOrgNames(seh, title);
    BOOST_CHECK_EQUAL(title, "Unpublished [Aegolius acadicus]");

    title.assign("Unpublished AEGOLIUS ACADICUS");
    FixOrgNames(seh, title);
    BOOST_CHECK_EQUAL(title, "Unpublished Aegolius acadicus");

    title.assign("Unpublished (DIPNOI)");
    FixOrgNames(seh, title);
    BOOST_CHECK_EQUAL(title, "Unpublished (Dipnoi)");
   
    title.assign("Unpublished DIPNOI");
    FixOrgNames(seh, title);
    BOOST_CHECK_EQUAL(title, "Unpublished Dipnoi");

    title.assign("Unpublished [CANDIDA] sp.");
    FixOrgNames(seh, title);
    BOOST_CHECK_EQUAL(title, "Unpublished [Candida] sp.");

   // title.assign("Unpublished CANDIDA sp.");
   // FixOrgNames(seh, title);
   // BOOST_CHECK_EQUAL(title, "Unpublished Candida sp.");

    title.assign("Unpublished CANdida cf. auringIENSIS UWO(PS)99-304.7");
    FixOrgNames(seh, title);
    BOOST_CHECK_EQUAL(title, "Unpublished Candida cf. auringiensis UWO(PS)99-304.7");
}

const string sc_TestEntry_FixOrgnames = "\
Seq-entry ::= set {\
    class phy-set,\
    seq-set {\
        seq {\
            id {\
              local str \"seq_1\" },\
            descr{ \
              source { \
                  genome genomic , \
                    org { \
                      taxname \"Eptatretus burgeri\" , \
                      orgname { \
                         mod { \
                            { \
                              subtype isolate , \
                      subname \"BoBM478\" } } } } } }, \
             inst {\
                repr raw,\
                mol rna,\
                length 15 } } , \
        seq { \
          id {\
            local str \"seq_2\" },\
          descr{ \
            source { \
              genome genomic , \
                org { \
                  taxname \"Providencia alcalifaciens\" , \
                  db { \
                        { \
                         db \"taxon\" , \
                         tag id 3652 } }  } }, \
            molinfo { \
              biomol genomic } }, \
         inst {\
            repr raw,\
            mol dna,\
            length 20,\
            topology circular,\
            seq-data iupacna \"AAAATTTTGGGGCCCCAAAA\" } } , \
        seq { \
          id {\
            local str \"seq_3\" },\
          descr{ \
            source { \
              genome genomic , \
                org { \
                  taxname \"Felis catus]\" , \
                  db { \
                        { \
                         db \"taxon\" , \
                         tag id 9685 } } } }, \
            molinfo { \
              biomol genomic } }, \
         inst {\
            repr raw,\
            mol dna,\
            length 20,\
            topology circular,\
            seq-data iupacna \"AAAATTTTGGGGCCCCAAAA\" } } , \
        seq { \
          id {\
            local str \"seq_4\" },\
          descr{ \
            source { \
              genome genomic , \
                org { \
                  taxname \"[Aegolius acadicus]\" , \
                  db { \
                        { \
                         db \"taxon\" , \
                         tag id 56265 } } } }, \
            molinfo { \
              biomol genomic } }, \
         inst {\
            repr raw,\
            mol dna,\
            length 20,\
            topology circular,\
            seq-data iupacna \"AAAATTTTGGGGCCCCAAAA\" } } , \
         seq { \
          id {\
            local str \"seq_5\" },\
          descr{ \
            source { \
              genome genomic , \
                org { \
                  taxname \"[Dipnoi\" , \
                  db { \
                        { \
                         db \"taxon\" , \
                         tag id 7878 } } } }, \
            molinfo { \
              biomol genomic } }, \
         inst {\
            repr raw , \
            mol dna , \
            length 20 , \
            topology circular , \
            seq-data iupacna \"AAAATTTTGGGGCCCCAAAA\" } } , \
        seq { \
          id {\
            local str \"seq_6\" },\
          descr{ \
            source { \
              genome genomic , \
                org { \
                  taxname \"Candida cf. auringiensis UWO(PS)99-304.7\" , \
                  db { \
                        { \
                         db \"taxon\" , \
                         tag id 204243 } } } }, \
            molinfo { \
              biomol genomic } }, \
         inst {\
            repr raw , \
            mol dna , \
            length 20 , \
            topology circular , \
            seq-data iupacna \"AAAATTTTGGGGCCCCAAAA\" } } , \
        seq { \
          id {\
            local str \"seq_7\" },\
          descr{ \
            source { \
              genome genomic , \
                org { \
                  taxname \"[Candida] sp.\" , \
                  db { \
                        { \
                         db \"taxon\" , \
                         tag id 3652 } } } }, \
            molinfo { \
              biomol genomic } }, \
         inst {\
            repr raw , \
            mol dna , \
            length 20 , \
            topology circular , \
            seq-data iupacna \"AAAATTTTGGGGCCCCAAAA\" } } , \
        seq { \
          id {\
            local str \"seq_8\" },\
          descr{ \
            source { \
                org { \
                  taxname \"Gibbula divaricata: Trochidae: Vetigastropoda: Mollusca\" } }, \
            molinfo { \
              biomol genomic } }, \
         inst {\
            repr raw,\
            mol dna,\
            length 20,\
            topology circular,\
            seq-data iupacna \"AAAATTTTGGGGCCCCAAAA\" } } } } \
}";       

