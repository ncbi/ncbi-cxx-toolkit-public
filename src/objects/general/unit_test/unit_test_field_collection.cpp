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
 * Author:  Wratko Hlavina, NCBI
 *
 * File Description:
 *   Unit test for CUser_object and CUser_field, as containers of fields.
 *
 * ===========================================================================
 */

#include <ncbi_pch.hpp>

#include <objects/general/User_object.hpp>
#include <objects/general/User_field.hpp>

#include <corelib/ncbiapp.hpp>
#include <corelib/test_boost.hpp>

#include <boost/test/parameterized_test.hpp>
#include <util/util_exception.hpp>

#include <common/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;
USING_SCOPE(objects);

// Load test data once, for all tests.
static CUser_object data;

NCBITEST_INIT_CMDLINE(arg_desc)
{
    arg_desc->AddKey("data-in", "InputData",
                     "Input User-object for testing, in ASN.1 Text form.",
                     CArgDescriptions::eInputFile);
}

NCBITEST_AUTO_INIT()
{
    // force use of built-in accession guide
    CNcbiApplication::Instance()->GetConfig().Set("NCBI", "Data", kEmptyStr,
                                                  IRegistry::fPersistent);

    const CArgs& args = CNcbiApplication::Instance()->GetArgs();
    CNcbiIstream& istr = args["data-in"].AsInputFile();
    istr >> MSerial_AsnText >> data;
}

BOOST_AUTO_TEST_CASE(s_TestModelEvidence)
{
    const string test = "TestModelEvidence";
    const string test_no_case = "testmodelevidencE";
    BOOST_CHECK(data.HasField(test));
    BOOST_CHECK( ! data.HasField(test_no_case));
    BOOST_CHECK( data.HasField(test_no_case, ".", NStr::eNocase) );
    BOOST_CHECK(data.GetFieldRef(test));
    BOOST_CHECK( ! data.GetFieldRef(test_no_case, ".", NStr::eCase));
    BOOST_CHECK(data.GetFieldRef(test_no_case, ".", NStr::eNocase));
    const CUser_object& uo(data.GetField(test).GetData().GetObject());

    BOOST_CHECK(uo.HasField("Method"));
    BOOST_CHECK(uo.GetFieldRef("Method"));

    BOOST_CHECK_EQUAL(uo.GetField("Method")
            .GetData().GetStr(), "BestRefSeq");
    BOOST_CHECK_EQUAL(uo.GetField("method", ".", NStr::eNocase)
            .GetData().GetStr(), "BestRefSeq");

}

/// Test that spurious fields aren't found.
///
/// @see CXX-1971 "CUser_object:::SetField*() will fail
///                w/User-fields with non-string labels".
///
BOOST_AUTO_TEST_CASE(s_TestGeneOntology)
{
    // Use GeneOntology user object as an example.
    const string test = "TestGeneOntology";
    BOOST_CHECK(data.HasField(test));
    BOOST_CHECK(data.GetFieldRef(test));
    const CUser_object& uo(data.GetField(test).GetData().GetObject());

    BOOST_CHECK(! uo.HasField("ModelEvidence"));
    BOOST_CHECK(! uo.GetFieldRef("ModelEvidence"));
}

BOOST_AUTO_TEST_CASE(s_TestSeqFeat1)
{
    const string test = "TestSeqFeat1";
    BOOST_CHECK(data.HasField(test));
    BOOST_CHECK(data.GetFieldRef(test));
    const CUser_object& uo(data.GetField(test).GetData().GetObject());

    BOOST_CHECK(uo.HasField("ModelEvidence"));

    // NOTE: While it would be convenient for nested paths to
    //       function with common user objects such as ModelEvidence,
    //       they are not, in fact, required to work according to
    //       the API requirements of User-field objects. Nesting of
    //       User-objects is the culprit.
    //
    //       The behaviour here is actually unspecified right now.
    //
    // Requirements??? BOOST_CHECK(uo.HasField("ModelEvidence.Method"));
    // Requirements??? BOOST_CHECK_EQUAL(uo.GetField("ModelEvidence.Method")
    //                         .GetData().GetStr(), "BestRefSeq");
}

/// Test that nesting of fields works correctly.
///
/// @note Only User-fields nest. User-objects do not appear to participate
///       in field nesting. This may seem surprising, and should be
///       clarified.
///
BOOST_AUTO_TEST_CASE(s_TestNesting)
{
    const string test = "TestNesting";
    BOOST_CHECK(data.HasField(test));
    BOOST_CHECK(data.GetFieldRef(test));

    // Nested fields should not be returned unexpectedly. Recursive search
    // should not apply to anchored paths.
    BOOST_CHECK(! data.HasField("FieldsField"));
    BOOST_CHECK(! data.HasField("Nested"));

    // Test various types of fileds: string and int.

    BOOST_CHECK(data.HasField("TestNesting.StrField"));
    BOOST_CHECK(data.GetFieldRef("TestNesting.StrField"));
    BOOST_CHECK_EQUAL(data.GetField("TestNesting.StrField")
            .GetData().GetStr(), "Str");

    BOOST_CHECK(data.HasField("TestNesting.IntField"));
    BOOST_CHECK(data.GetFieldRef("TestNesting.IntField"));
    BOOST_CHECK_EQUAL(data.GetField("TestNesting.IntField")
            .GetData().GetInt(), 100);

    // Test nested paths, multiple levels deep. Only User-fields
    // appear in these nesting tests. User-objects don't appear
    // to participate in field nesting.
    BOOST_CHECK(data.HasField("TestNesting.FieldsField"));
    BOOST_CHECK(data.GetFieldRef("TestNesting.FieldsField"));
    BOOST_CHECK(data.HasField("TestNesting.FieldsField.Nested"));
    BOOST_CHECK(data.GetFieldRef("TestNesting.FieldsField.Nested"));
    BOOST_CHECK_EQUAL(data.GetField("TestNesting.FieldsField.Nested")
            .GetData().GetStr(), "NestedStr");
    BOOST_CHECK_EQUAL(
        data.GetField("TESTNESTING.fieldsfield.NESTED", ".", NStr::eNocase)
            .GetData().GetStr(),
        "NestedStr");

    /// Test GetFieldsMap
    CConstRef<CUser_field> pTestField = data.GetFieldRef(test);
    CUser_field::TMapFieldNameToRef mapFieldNameToRef;
    pTestField->GetFieldsMap(mapFieldNameToRef, 
        CUser_field::fFieldMapFlags_ExcludeThis);
    
    BOOST_CHECK( ! mapFieldNameToRef.empty() );

    // make sure the mapping is correct
    ITERATE( CUser_field::TMapFieldNameToRef, map_iter, mapFieldNameToRef ) {
        CNcbiOstrstream name_strm;
        // name_strm << test << ".";
        map_iter->first.Join(name_strm);
        const string sName = CNcbiOstrstreamToString(name_strm);
        cerr << "Testing name: " << sName << endl;
        BOOST_CHECK_EQUAL(
            pTestField->GetFieldRef(sName), map_iter->second );
    }
}
