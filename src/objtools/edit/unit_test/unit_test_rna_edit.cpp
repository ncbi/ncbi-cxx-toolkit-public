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
* Author:  Igor Filippov, based on vcf reader unit test by Frank Ludwig, NCBI.
*
* File Description:
*   Find ITS parser unit test.
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <corelib/ncbi_system.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbifile.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>

#include <objtools/edit/rna_edit.hpp>

#include <cstdio>

// This header must be included before all Boost.Test headers if there are any
#include <corelib/test_boost.hpp>

#include <common/test_assert.h>  /* This header must go last */


USING_NCBI_SCOPE;
USING_SCOPE(objects);

//  ============================================================================
//  Customization data:
const string extOrigInput("input.asn");
const string extInput("find_its_output.tab");
const string extOutput("output.asn");
const string dirTestFiles("rna_edit_test_cases");
const string extKeep("new");
//  ============================================================================

struct STestInfo {
    CFile mOrigInFile;
    CFile mInFile;
    CFile mOutFile;
};
typedef string TTestName;
typedef map<TTestName, STestInfo> TTestNameToInfoMap;

class CTestNameToInfoMapLoader {
public:
    CTestNameToInfoMapLoader(
        TTestNameToInfoMap * pTestNameToInfoMap)
        : m_pTestNameToInfoMap(pTestNameToInfoMap)
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
        vector<string> vecFileNamePieces;
        NStr::Tokenize( sFileName, ".", vecFileNamePieces );
        BOOST_REQUIRE(vecFileNamePieces.size() == 3);

        string tsTestName = vecFileNamePieces[0];
        BOOST_REQUIRE(!tsTestName.empty());
        string tsFileType = vecFileNamePieces[1] + "." + vecFileNamePieces[2];
        BOOST_REQUIRE(!tsFileType.empty());
            
        STestInfo & test_info_to_load =
            (*m_pTestNameToInfoMap)[vecFileNamePieces[0]];

        // figure out what type of file we have and set appropriately
        if (tsFileType == extInput) {
            BOOST_REQUIRE( test_info_to_load.mInFile.GetPath().empty() );
            test_info_to_load.mInFile = file;
        } 
        else if (tsFileType == extOrigInput) {
            BOOST_REQUIRE( test_info_to_load.mOrigInFile.GetPath().empty() );
            test_info_to_load.mOrigInFile = file;
        } 
        else if (tsFileType == extOutput) {
            BOOST_REQUIRE( test_info_to_load.mOutFile.GetPath().empty() );
            test_info_to_load.mOutFile = file;
        } 
        else {
            BOOST_FAIL("Unknown file type " << sFileName << ".");
        }
    }

private:
    // raw pointer because we do NOT own this
    TTestNameToInfoMap * m_pTestNameToInfoMap;
};

void sUpdateCase(CDir& test_cases_dir, const string& test_name)
{   
    string input = CDir::ConcatPath( test_cases_dir.GetPath(), test_name + "." + extInput);
    string orig_input = CDir::ConcatPath( test_cases_dir.GetPath(), test_name + "." + extOrigInput);
    string output = CDir::ConcatPath( test_cases_dir.GetPath(), test_name + "." + extOutput);
    if (!CFile(input).Exists()) {
         BOOST_FAIL("input file " << input << " does not exist.");
    }
    cerr << "Creating new test case from " << input << " ..." << endl;

    CNcbiIfstream ifstr(orig_input.c_str());
    CRef <CSeq_entry> entry(new CSeq_entry());
    ifstr >> MSerial_AsnText >> *entry;
    ifstr.close();
    CRef <CScope> scope(new CScope(*CObjectManager::GetInstance()));
    CSeq_entry_Handle tse = scope->AddTopLevelSeqEntry(*entry);

    CNcbiOfstream ofstr(output.c_str());
    try 
    {
        edit::CFindITSParser parser(input.c_str(), tse);
        do 
        {           
            CRef <CSeq_feat> new_mrna = parser.ParseLine();
            if (new_mrna)
                ofstr << MSerial_AsnText << *new_mrna;
        } while ( !parser.AtEOF() );
    }
    catch (...) 
    {
        BOOST_FAIL("Error: " << input << " failed during conversion.");
    }
  
    ofstr.close();
    cerr << "    Produced new ASN1 file " << output << "." << endl;

    cerr << " ... Done." << endl;
}

void sUpdateAll(CDir& test_cases_dir) {

    const vector<string> kEmptyStringVec;
    TTestNameToInfoMap testNameToInfoMap;
    CTestNameToInfoMapLoader testInfoLoader(&testNameToInfoMap);
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
        testInfo.mOutFile.GetName() << endl;

    CNcbiIfstream ifstr(testInfo.mOrigInFile.GetPath().c_str());
    CRef <CSeq_entry> entry(new CSeq_entry());
    ifstr >>  MSerial_AsnText >> *entry;
    ifstr.close();
    CRef <CScope> scope(new CScope(*CObjectManager::GetInstance()));
    CSeq_entry_Handle tse = scope->AddTopLevelSeqEntry(*entry);


    string resultName = CDirEntry::GetTmpName();
    CNcbiOfstream ofstr(resultName.c_str());    
    try 
    {
        edit::CFindITSParser parser(testInfo.mInFile.GetPath().c_str(), tse);
        do 
        {           
            CRef <CSeq_feat> new_mrna = parser.ParseLine();
            if (new_mrna)
                ofstr << MSerial_AsnText << *new_mrna;
        } while ( !parser.AtEOF() );
    }
    catch (...) 
    {
        BOOST_FAIL("Error: " << sTestName << " failed during conversion.");
        return;
    }
    ofstr.close();

    bool success = testInfo.mOutFile.CompareTextContents(resultName, CFile::eIgnoreWs);
    if (!success) {
        CDirEntry deResult = CDirEntry(resultName);
        if (keep) {
            deResult.Copy(testInfo.mOutFile.GetPath() + "." + extKeep);
        }
        deResult.Remove();
        BOOST_ERROR("Error: " << sTestName << " failed due to post processing diffs.");
    }
    CDirEntry(resultName).Remove();
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
    CTestNameToInfoMapLoader testInfoLoader(&testNameToInfoMap);
    FindFilesInDir(test_cases_dir, kEmptyStringVec,kEmptyStringVec,testInfoLoader, fFF_Default | fFF_Recursive );

    ITERATE(TTestNameToInfoMap, name_to_info_it, testNameToInfoMap) {
        const string & sName = name_to_info_it->first;
        const STestInfo & testInfo = name_to_info_it->second;
        cout << "Verifying: " << sName << endl;
        BOOST_REQUIRE_MESSAGE( testInfo.mOrigInFile.Exists(), extOrigInput + " file does not exist: " << testInfo.mOrigInFile.GetPath() );
        BOOST_REQUIRE_MESSAGE( testInfo.mInFile.Exists(),     extInput + " file does not exist: " << testInfo.mInFile.GetPath() );
        BOOST_REQUIRE_MESSAGE( testInfo.mOutFile.Exists(),    extOutput + " file does not exist: " << testInfo.mOutFile.GetPath() );
    }
    ITERATE(TTestNameToInfoMap, name_to_info_it, testNameToInfoMap) {
        const string & sName = name_to_info_it->first;
        const STestInfo & testInfo = name_to_info_it->second;
        
        cout << "Running test: " << sName << endl;

        BOOST_CHECK_NO_THROW(sRunTest(sName, testInfo, args["keep-diffs"]));
    }
}
