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
* Author: Azat Badretdin
*
* File Description:
*   Sample unit tests file for testing SQLITE3 based taxonomy functionality
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <corelib/ncbi_system.hpp>
#include <corelib/ncbiapp.hpp>

#include <objects/taxon1/local_taxon.hpp>

#include <serial/serial.hpp>
#include <serial/objostr.hpp>
#include <serial/objistr.hpp>

// This header must be included before all Boost.Test headers if there are any
#include <corelib/test_boost.hpp>
#include <algorithm>

USING_NCBI_SCOPE;
USING_SCOPE(objects);
USING_SCOPE(std);
unique_ptr<CLocalTaxon> tax1;

ESerialDataFormat format = eSerial_AsnBinary;

NCBITEST_AUTO_INIT()
{
    // Your application initialization code here (optional)
}


NCBITEST_INIT_CMDLINE(arg_desc)
{
    CLocalTaxon::AddArguments(*arg_desc);
}



NCBITEST_AUTO_FINI()
{
    // Your application finalization code here (optional)
    // tax1.Fini();
}

BOOST_AUTO_TEST_SUITE(local_taxon)

NCBITEST_INIT_TREE()
{
    // Here we can set some dependencies between tests (if one disabled or
    // failed other shouldn't execute) and hard-coded disablings. Note though
    // that more preferable way to make always disabled test is add to
    // ini-file in UNITTESTS_DISABLE section this line:
    // TestName = true

    NCBITEST_DEPENDS_ON(test_tax1_synonyms, test_local_taxon_init);

    //    NCBITEST_DISABLE(AlwaysDisabled);
}

BOOST_AUTO_TEST_CASE(test_local_taxon_init) {
    tax1.reset(new CLocalTaxon(CNcbiApplication::Instance()->GetArgs()));
}

BOOST_AUTO_TEST_CASE(test_tax1_synonyms) {
    auto synonyms = tax1->GetSynonyms(562);
    list<string> expected_synonyms =             {
                     "Bacillus coli",
                     "Bacterium coli",
                     "Bacterium coli commune",
                     "Enterococcus coli"
    };

    BOOST_REQUIRE_MESSAGE( synonyms.size() == 4, "Number of synonyms returned is not 4" );
    for(auto expected_synonym: expected_synonyms) {
        BOOST_REQUIRE_MESSAGE( std::find(synonyms.begin(), synonyms.end(), expected_synonym) !=  synonyms.end(), expected_synonym + " not found in the output" );
    }
    for(auto synonym: synonyms) {
        BOOST_REQUIRE_MESSAGE( std::find(expected_synonyms.begin(), expected_synonyms.end(), synonym) !=  expected_synonyms.end(), synonym + " is an extra synonym found in the output" );
    }
}

BOOST_AUTO_TEST_SUITE_END();
