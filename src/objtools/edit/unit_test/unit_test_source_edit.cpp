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
#include "unit_test_source_edit.hpp"

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

#include <objtools/edit/source_edit.hpp>

#include <common/test_assert.h>  /* This header must go last */


USING_NCBI_SCOPE;
USING_SCOPE(objects);

extern const char* sc_TestEntry_FixOrgnames;

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


BOOST_AUTO_TEST_CASE(Test_RemoveOldName)
{
    CBioSource src;
    src.SetOrg().SetTaxname("Norovirus");

    unit_test_util::SetOrgMod(src, COrgMod::eSubtype_old_name, "Norwalk-like viruses");
    unit_test_util::SetOrgMod(src, COrgMod::eSubtype_acronym, "acronym");
    unit_test_util::SetOrgMod(src, COrgMod::eSubtype_strain, "strain");
    edit::RemoveOldName(src);
    FOR_EACH_ORGMOD_ON_BIOSOURCE ( orgmod, src ) {
        if ((*orgmod)->IsSetSubtype())
            BOOST_CHECK_EQUAL((*orgmod)->GetSubtype() != COrgMod::eSubtype_old_name, true);
    }
}

BOOST_AUTO_TEST_CASE(Test_RemoveTaxId)
{
    CBioSource src;
    src.SetOrg().SetTaxname("Norovirus");

    unit_test_util::SetDbxref (src, "newDB", "3456");
    unit_test_util::SetDbxref (src, "taxon", "4569");
    edit::RemoveTaxId(src);
    FOR_EACH_DBXREF_ON_ORGREF ( db, src.GetOrg() ) {
        if ((*db)->IsSetDb())
            BOOST_CHECK_EQUAL(NStr::Equal((*db)->GetDb(), "taxon", NStr::eNocase), false);
    }
}



BOOST_AUTO_TEST_CASE(Test_CleanupForTaxnameChange)
{
    CBioSource src;
    src.SetOrg().SetTaxname("Norovirus");

    // set the tax id
    unit_test_util::SetDbxref (src, "newDB", "3456");
    unit_test_util::SetDbxref (src, "taxon", "4569");
    // set the old name
    unit_test_util::SetOrgMod(src, COrgMod::eSubtype_acronym, "acronym");
    unit_test_util::SetOrgMod(src, COrgMod::eSubtype_old_name, "Norwalk-like viruses");
    unit_test_util::SetOrgMod(src, COrgMod::eSubtype_strain, "strain");
    // set common name
    src.SetOrg().SetCommon("common name for Norovirus");
    // set synonym
    list<string> synonyms;
    synonyms.push_back("syn1");
    synonyms.push_back("syn2");
    src.SetOrg().SetSyn() = synonyms;


    edit::CleanupForTaxnameChange(src);
    FOR_EACH_DBXREF_ON_ORGREF ( db, src.GetOrg() ) {
        if ((*db)->IsSetDb())
            BOOST_CHECK_EQUAL(NStr::Equal((*db)->GetDb(), "taxon", NStr::eNocase), false);
    }

    FOR_EACH_ORGMOD_ON_BIOSOURCE ( orgmod, src ) {
        if ((*orgmod)->IsSetSubtype())
            BOOST_CHECK_EQUAL((*orgmod)->GetSubtype() != COrgMod::eSubtype_old_name, true);
    }

    BOOST_CHECK_EQUAL(src.GetOrg().IsSetSyn(), false);
}


bool s_HasOrgMod(const COrg_ref& org, COrgMod::ESubtype subtype, const string& val)
{
    if (!org.IsSetOrgname() || !org.GetOrgname().IsSetMod()) {
        return false;
    }
    ITERATE(COrgName::TMod, it, org.GetOrgname().GetMod()) {
        if ((*it)->GetSubtype() == subtype && NStr::Equal((*it)->GetSubname(), val)) {
            return true;
        }
    }
    return false;
}


BOOST_AUTO_TEST_CASE(Test_SQD_2100)
{
    CRef<CBioSource> src1(new CBioSource());
    CRef<CBioSource> src2(new CBioSource());

    CRef<CBioSource> common = edit::MakeCommonBioSource(*src1, *src2);
    // fail because no org-refs set
    BOOST_CHECK(!common);

    // TaxIds are same, drop non-common attributes
    src1->SetOrg().SetTaxname("Pneumocystis carinii");
    CRef<CDbtag> tag(new CDbtag());
    tag->SetDb("taxon");
    tag->SetTag().SetId(142052);
    src1->SetOrg().SetDb().push_back(tag);
    CRef<COrgMod> mod(new COrgMod());
    mod->SetSubtype(COrgMod::eSubtype_forma_specialis);
    mod->SetSubname("rattus-tertii");
    src1->SetOrg().SetOrgname().SetMod().push_back(mod);

    src2->Assign(*src1);

    src1->SetGenome(CBioSource::eGenome_apicoplast);
    src2->SetGenome(CBioSource::eGenome_chloroplast);

    common = edit::MakeCommonBioSource(*src1, *src2);
    BOOST_CHECK(common);
    BOOST_CHECK_EQUAL(common->GetOrg().GetTaxname(), "Pneumocystis carinii");
    BOOST_CHECK_EQUAL(common->GetOrg().GetTaxId(), TAX_ID_CONST(4754));
    BOOST_CHECK_EQUAL(s_HasOrgMod(common->GetOrg(), COrgMod::eSubtype_forma_specialis, "rattus-tertii"), true);
    BOOST_CHECK(!common->IsSetGenome());

    // examples from CXX-9372

    //tax IDs are the same, but need to look up before merging to get taxnames to match
    src1->ResetOrg();
    src1->SetOrg().SetTaxname("Eremothecium gossypii ATCC 10895");
    src1->SetOrg().SetTaxId(TAX_ID_CONST(284811));
    src1->SetOrg().SetOrgname().SetMod().push_back(CRef<COrgMod>(new COrgMod(COrgMod::eSubtype_strain, "ATCC 10895")));
    src1->SetOrg().SetOrgname().SetMod().push_back(CRef<COrgMod>(new COrgMod(COrgMod::eSubtype_culture_collection, "ATCC:10895")));

    src2->ResetOrg();
    src2->SetOrg().SetTaxname("Ashbya gossypii ATCC 10895");
    src2->SetOrg().SetTaxId(TAX_ID_CONST(284811));
    src2->SetOrg().SetOrgname().SetMod().push_back(CRef<COrgMod>(new COrgMod(COrgMod::eSubtype_strain, "ATCC 10895")));
    src2->SetOrg().SetOrgname().SetMod().push_back(CRef<COrgMod>(new COrgMod(COrgMod::eSubtype_culture_collection, "ATCC:10895")));
    common = edit::MakeCommonBioSource(*src1, *src2);
    BOOST_CHECK(common);
    BOOST_CHECK_EQUAL(common->GetOrg().GetTaxname(), "Eremothecium gossypii ATCC 10895");
    BOOST_CHECK_EQUAL(common->GetOrg().GetTaxId(), TAX_ID_CONST(284811));
    BOOST_CHECK_EQUAL(s_HasOrgMod(common->GetOrg(), COrgMod::eSubtype_strain, "ATCC 10895"), true);
    BOOST_CHECK_EQUAL(s_HasOrgMod(common->GetOrg(), COrgMod::eSubtype_culture_collection, "ATCC:10895"), true);

    //2 sub-sub strains with common strain above them
    src1->ResetOrg();
    src1->SetOrg().SetTaxId(TAX_ID_CONST(588858));
    src1->SetOrg().SetOrgname().SetMod().push_back(CRef<COrgMod>(new COrgMod(COrgMod::eSubtype_strain, "14028S")));
    src1->SetOrg().SetOrgname().SetMod().push_back(CRef<COrgMod>(new COrgMod(COrgMod::eSubtype_serovar, "Typhimurium")));
    src2->ResetOrg();
    src2->SetOrg().SetTaxId(TAX_ID_CONST(1159899));
    src2->SetOrg().SetOrgname().SetMod().push_back(CRef<COrgMod>(new COrgMod(COrgMod::eSubtype_strain, "LT2-4")));
    src2->SetOrg().SetOrgname().SetMod().push_back(CRef<COrgMod>(new COrgMod(COrgMod::eSubtype_serovar, "Typhimurium")));
    common = edit::MakeCommonBioSource(*src1, *src2);
    BOOST_CHECK(common);
    BOOST_CHECK_EQUAL(common->GetOrg().GetTaxname(), "Salmonella enterica subsp. enterica serovar Typhimurium str. LT2");
    BOOST_CHECK_EQUAL(common->GetOrg().GetTaxId(), TAX_ID_CONST(99287));
    BOOST_CHECK_EQUAL(s_HasOrgMod(common->GetOrg(), COrgMod::eSubtype_serovar, "Typhimurium"), true);
    BOOST_CHECK_EQUAL(s_HasOrgMod(common->GetOrg(), COrgMod::eSubtype_strain, "LT2"), true);

    // 2 strains that their common parent is several levels up, but still under same species
    src1->ResetOrg();
    src1->SetOrg().SetTaxId(TAX_ID_CONST(588858));
    src2->ResetOrg();
    src2->SetOrg().SetTaxId(TAX_ID_CONST(1410937));
    common = edit::MakeCommonBioSource(*src1, *src2);
    BOOST_CHECK(common);
    BOOST_CHECK_EQUAL(common->GetOrg().GetTaxname(), "Salmonella enterica subsp. enterica serovar Typhimurium");
    BOOST_CHECK_EQUAL(common->GetOrg().GetTaxId(), TAX_ID_CONST(90371));
    BOOST_CHECK_EQUAL(s_HasOrgMod(common->GetOrg(), COrgMod::eSubtype_serovar, "Typhimurium"), true);

    // one taxid is parent of the other
    src1->ResetOrg();
    src1->SetOrg().SetTaxId(TAX_ID_CONST(588858));
    src2->ResetOrg();
    src2->SetOrg().SetTaxId(TAX_ID_CONST(59201));
    common = edit::MakeCommonBioSource(*src1, *src2);
    BOOST_CHECK(common);
    BOOST_CHECK_EQUAL(common->GetOrg().GetTaxname(), "Salmonella enterica subsp. enterica");
    BOOST_CHECK_EQUAL(common->GetOrg().GetTaxId(), TAX_ID_CONST(59201));
    BOOST_CHECK_EQUAL(s_HasOrgMod(common->GetOrg(), COrgMod::eSubtype_serovar, "Typhimurium"), false);

    // strains of different species
    // result should be failure, because 2 different species cannot be merged
    src1->ResetOrg();
    src1->SetOrg().SetTaxId(TAX_ID_CONST(1416749));
    src2->ResetOrg();
    src2->SetOrg().SetTaxId(TAX_ID_CONST(644337));
    common = edit::MakeCommonBioSource(*src1, *src2);
    BOOST_CHECK(!common);

    //
    // Tax IDs not species level same

    src2->SetOrg().SetDb().front()->SetTag().SetId(1069680);
    src2->SetOrg().SetTaxname("Pneumocystis murina B123");
    src2->SetOrg().SetOrgname().ResetMod();
    CRef<COrgMod> strain(new COrgMod());
    strain->SetSubtype(COrgMod::eSubtype_strain);
    strain->SetSubname("B123");
    src2->SetOrg().SetOrgname().SetMod().push_back(strain);
    CRef<COrgMod> subsp(new COrgMod());
    subsp->SetSubtype(COrgMod::eSubtype_sub_species);
    subsp->SetSubname("B123");
    src2->SetOrg().SetOrgname().SetMod().push_back(subsp);
    common = edit::MakeCommonBioSource(*src1, *src2);
    BOOST_CHECK(!common);

}

