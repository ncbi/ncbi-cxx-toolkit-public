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
#include <objects/general/Dbtag.hpp>
#include <objtools/unit_test_util/unit_test_util.hpp>
#include <objtools/edit/source_edit.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

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
    CRef<CBioSource> src(new CBioSource());
    src->SetOrg().SetTaxname("Norovirus");

    unit_test_util::SetOrgMod(*src, COrgMod::eSubtype_old_name, "Norwalk-like viruses");
    unit_test_util::SetOrgMod(*src, COrgMod::eSubtype_acronym, "acronym");
    unit_test_util::SetOrgMod(*src, COrgMod::eSubtype_strain, "strain");
    edit::RemoveOldName(*src);
    FOR_EACH_ORGMOD_ON_BIOSOURCE ( orgmod, *src ) {
        if ((*orgmod)->IsSetSubtype()) 
            BOOST_CHECK_EQUAL((*orgmod)->GetSubtype() != COrgMod::eSubtype_old_name, true);
    }
 
}

BOOST_AUTO_TEST_CASE(Test_RemoveTaxId)
{
    CRef<CBioSource> src(new CBioSource());
    src->SetOrg().SetTaxname("Norovirus");

    unit_test_util::SetDbxref (*src, "newDB", "3456");
    unit_test_util::SetDbxref (*src, "taxon", "4569");
    edit::RemoveTaxId(*src);
    FOR_EACH_DBXREF_ON_ORGREF ( db, src->GetOrg() ) {
        if ((*db)->IsSetDb())
            BOOST_CHECK_EQUAL(NStr::Equal((*db)->GetDb(), "taxon", NStr::eNocase), false);
    }
}



BOOST_AUTO_TEST_CASE(Test_CleanupForTaxnameChange)
{
    CRef<CBioSource> src(new CBioSource());
    src->SetOrg().SetTaxname("Norovirus");

    // set the tax id
    unit_test_util::SetDbxref (*src, "newDB", "3456");
    unit_test_util::SetDbxref (*src, "taxon", "4569");
    // set the old name
    unit_test_util::SetOrgMod(*src, COrgMod::eSubtype_acronym, "acronym");
    unit_test_util::SetOrgMod(*src, COrgMod::eSubtype_old_name, "Norwalk-like viruses");
    unit_test_util::SetOrgMod(*src, COrgMod::eSubtype_strain, "strain");
    // set common name
    src->SetOrg().SetCommon("common name for Norovirus");
    // set synonym
    list<string> synonyms;
    synonyms.push_back("syn1");
    synonyms.push_back("syn2");
    src->SetOrg().SetSyn() = synonyms;

    //cout << MSerial_AsnText << *src << endl;

    edit::CleanupForTaxnameChange(*src);
    FOR_EACH_DBXREF_ON_ORGREF ( db, src->GetOrg() ) {
        if ((*db)->IsSetDb())
            BOOST_CHECK_EQUAL(NStr::Equal((*db)->GetDb(), "taxon", NStr::eNocase), false);
    }

    FOR_EACH_ORGMOD_ON_BIOSOURCE ( orgmod, *src ) {
        if ((*orgmod)->IsSetSubtype()) 
            BOOST_CHECK_EQUAL((*orgmod)->GetSubtype() != COrgMod::eSubtype_old_name, true);
    }

    BOOST_CHECK_EQUAL(src->GetOrg().IsSetSyn(), false);

    //cout << MSerial_AsnText << *src << endl;
}
    

END_SCOPE(objects)
END_NCBI_SCOPE

