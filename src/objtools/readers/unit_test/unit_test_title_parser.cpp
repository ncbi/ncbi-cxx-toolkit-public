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
* Author:  Justin Foley
*
* File Description:
*   Reader unit test.
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <corelib/ncbi_system.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbifile.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/general/Object_id.hpp>
#include <objtools/readers/mod_reader.hpp>
#include <objtools/logging/listener.hpp>

#include <cstdio>

// This header must be included before all Boost.Test headers if there are any
#include <corelib/test_boost.hpp>


USING_NCBI_SCOPE;
USING_SCOPE(objects);

//  ============================================================================
//  Customization data:
const string extInput("defline");
const string extOutput("mods");
const string extErrors("errors");
const string extKeep("new");
const string dirTestFiles("title_parser_test_cases");
// !!!
// !!! Must also customize reader type in sRunTest !!!
// !!!
//  ============================================================================

struct STestInfo {
    CFile mInFile;
    CFile mOutFile;
    CFile mErrorFile;
};
typedef string TTestName;
typedef map<TTestName, STestInfo> TTestNameToInfoMap;

class CTestNameToInfoMapLoader {
public:
    CTestNameToInfoMapLoader(
        TTestNameToInfoMap& testNameToInfoMap,
        const string& extInput,
        const string& extOutput,
        const string& extErrors)
        : m_TestNameToInfoMap(testNameToInfoMap),
          mExtInput(extInput),
          mExtOutput(extOutput),
          mExtErrors(extErrors)
    { }

    void operator()( const CDirEntry & dirEntry ) {
        if( ! dirEntry.IsFile() ) {
            return;
        }

        CFile file(dirEntry);
        string name = file.GetName();
        if (NStr::EndsWith(name, ".txt")  ||  NStr::StartsWith(name, ".")) {
            return;
        }

        // extract info from the file name
        const string sFileName = file.GetName();
        vector<CTempString> vecFileNamePieces;
        NStr::Split( sFileName, ".", vecFileNamePieces );
        BOOST_REQUIRE(vecFileNamePieces.size() == 2);

        CTempString tsTestName = vecFileNamePieces[0];
        BOOST_REQUIRE(!tsTestName.empty());
        CTempString tsFileType = vecFileNamePieces[1];
        BOOST_REQUIRE(!tsFileType.empty());

        STestInfo & test_info_to_load = m_TestNameToInfoMap[tsTestName];

        // figure out what type of file we have and set appropriately
        if (tsFileType == mExtInput) {
            BOOST_REQUIRE( test_info_to_load.mInFile.GetPath().empty() );
            test_info_to_load.mInFile = file;
        }
        else if (tsFileType == mExtOutput) {
            BOOST_REQUIRE( test_info_to_load.mOutFile.GetPath().empty() );
            test_info_to_load.mOutFile = file;
        }
        else if (tsFileType == mExtErrors) {
            BOOST_REQUIRE( test_info_to_load.mErrorFile.GetPath().empty() );
            test_info_to_load.mErrorFile = file;
        }

        else {
            BOOST_FAIL("Unknown file type " << sFileName << ".");
        }
    }

private:
    // raw pointer because we do NOT own this
    TTestNameToInfoMap& m_TestNameToInfoMap;
    string mExtInput;
    string mExtOutput;
    string mExtErrors;
};




void sUpdateCase(CDir& test_cases_dir, const string& test_name)
{
    string input  = CDir::ConcatPath( test_cases_dir.GetPath(), test_name + "." + extInput);
    string output = CDir::ConcatPath( test_cases_dir.GetPath(), test_name + "." + extOutput);
    string errors = CDir::ConcatPath( test_cases_dir.GetPath(), test_name + "." + extErrors);
    if (!CFile(input).Exists()) {
         BOOST_FAIL("input file " << input << " does not exist.");
    }
    cerr << "Creating new test case from " << input << " ..." << endl;

    CNcbiIfstream ifstr(input.c_str());
    CNcbiOfstream ofstr(output.c_str());
    try {
        CModHandler::TModList mods;
        for (string line; getline(ifstr, line);) {
            string remainder;
            CTitleParser::Apply(line, mods, remainder);

            for (const auto& mod : mods) {
                ofstr << mod.GetName() << " " << mod.GetValue() << endl;
            }
            if (!remainder.empty()) {
                ofstr << remainder << endl;
            }
            mods.clear();
        }
    }
    catch (...) {
        ifstr.close();
        BOOST_FAIL("Error: " << input << " failed during conversion.");
    }
    ifstr.close();

    ofstr.close();
    cerr << "    Produced new Modifier file " << output << "." << endl;

  //  CNcbiOfstream errstr(errors.c_str());
  //  errstr.close();
  //  cerr << "    Produced new error listing " << errors << "." << endl;
  //  cerr << " ... Done." << endl;
}

void sUpdateAll(CDir& test_cases_dir) {

    const vector<string> kEmptyStringVec;
    TTestNameToInfoMap testNameToInfoMap;
    CTestNameToInfoMapLoader testInfoLoader(
        testNameToInfoMap, extInput, extOutput, extErrors);
    FindFilesInDir(
        test_cases_dir,
        kEmptyStringVec,
        kEmptyStringVec,
        testInfoLoader,
        fFF_Default | fFF_Recursive );

    ITERATE(TTestNameToInfoMap, name_to_info_it, testNameToInfoMap) {
        const string & sName = name_to_info_it->first;
        sUpdateCase(test_cases_dir, sName);
    }
}


void sRunTest(const string &sTestName, const STestInfo & testInfo, bool keep)
{
    cerr << "Testing " << testInfo.mInFile.GetName() << " against " <<
        testInfo.mOutFile.GetName() << " and " <<
        testInfo.mErrorFile.GetName() << endl;

    CNcbiIfstream ifstr(testInfo.mInFile.GetPath().c_str());
    const string& logName = CDirEntry::GetTmpName();
    CNcbiOfstream errstr(logName.c_str());
    const string& resultName = CDirEntry::GetTmpName();
    CNcbiOfstream ofstr(resultName.c_str());
    try {
        CModHandler::TModList mods;
        for (string line; getline(ifstr, line);) {
            string remainder;
            CTitleParser::Apply(line, mods, remainder);

            for (const auto& mod : mods) {
                ofstr << mod.GetName() << " " << mod.GetValue() << endl;
            }
            if (!remainder.empty()) {
                ofstr << remainder << endl;
            }
            mods.clear();
        }
    }
    catch (...) {
        BOOST_ERROR("Error: " << sTestName << " failed during conversion.");
        ifstr.close();
        return;
    }

    ifstr.close();
    ofstr.close();
    errstr.close();


    bool success = testInfo.mOutFile.CompareTextContents(resultName, CFile::eIgnoreWs);
    if (!success) {
        CDirEntry deResult = CDirEntry(resultName);
        if (keep) {
            deResult.Copy(testInfo.mOutFile.GetPath() + "." + extKeep);
        }
        deResult.Remove();
        CDirEntry(logName).Remove();
        BOOST_ERROR("Error: " << sTestName << " failed due to post processing diffs.");
    }
    CDirEntry(resultName).Remove();


    success = testInfo.mErrorFile.CompareTextContents(logName, CFile::eIgnoreWs);
    if (!success) {
        CDirEntry deErrors = CDirEntry(logName);
        if (keep) {
            deErrors.Copy(testInfo.mErrorFile.GetPath() + "." + extKeep);
        }
        deErrors.Remove();
        BOOST_ERROR("Error: " << sTestName << " failed due to error handling diffs.");
    }
};

NCBITEST_AUTO_INIT()
{
}


NCBITEST_INIT_CMDLINE(arg_descrs)
{
    arg_descrs->AddDefaultKey("test-dir", "TEST_FILE_DIRECTORY",
        "Set the root directory under which all test files can be found.",
        CArgDescriptions::eDirectory,
        dirTestFiles );
    arg_descrs->AddDefaultKey("update-case", "UPDATE_CASE",
        "Produce .asn and .error files from given name for new or updated test case.",
        CArgDescriptions::eString,
        "" );
    arg_descrs->AddFlag("update-all",
        "Update all test cases to current reader code (dangerous).",
        true );
    arg_descrs->AddFlag("keep-diffs",
        "Keep output files that are different from the expected.",
        true );
}

NCBITEST_AUTO_FINI()
{
}

BOOST_AUTO_TEST_CASE(RunTests)
{
    const CArgs& args = CNcbiApplication::Instance()->GetArgs();

    CDir test_cases_dir( args["test-dir"].AsDirectory() );
    BOOST_REQUIRE_MESSAGE( test_cases_dir.IsDir(),
        "Cannot find dir: " << test_cases_dir.GetPath() );
    bool update_all = args["update-all"].AsBoolean();
    if (update_all) {
        sUpdateAll(test_cases_dir);
        return;
    }

    string update_case = args["update-case"].AsString();
    if (!update_case.empty()) {
        sUpdateCase(test_cases_dir, update_case);
        return;
    }
    const vector<string> kEmptyStringVec;
    TTestNameToInfoMap testNameToInfoMap;
    CTestNameToInfoMapLoader testInfoLoader(
        testNameToInfoMap, extInput, extOutput, extErrors);
    FindFilesInDir(
        test_cases_dir,
        kEmptyStringVec,
        kEmptyStringVec,
        testInfoLoader,
        fFF_Default | fFF_Recursive );

    ITERATE(TTestNameToInfoMap, name_to_info_it, testNameToInfoMap) {
        const string & sName = name_to_info_it->first;
        const STestInfo & testInfo = name_to_info_it->second;
        cout << "Verifying: " << sName << endl;
        BOOST_REQUIRE_MESSAGE( testInfo.mInFile.Exists(),
            extInput + " file does not exist: " << testInfo.mInFile.GetPath() );
        BOOST_REQUIRE_MESSAGE( testInfo.mOutFile.Exists(),
            extOutput + " file does not exist: " << testInfo.mOutFile.GetPath() );
        BOOST_REQUIRE_MESSAGE( testInfo.mErrorFile.Exists(),
            extErrors + " file does not exist: " << testInfo.mErrorFile.GetPath() );
    }
    ITERATE(TTestNameToInfoMap, name_to_info_it, testNameToInfoMap) {
        const string & sName = name_to_info_it->first;
        const STestInfo & testInfo = name_to_info_it->second;

        cout << "Running test: " << sName << endl;

        BOOST_CHECK_NO_THROW(sRunTest(sName, testInfo, args["keep-diffs"]));
    }
}
