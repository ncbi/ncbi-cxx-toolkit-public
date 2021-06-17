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
* Author:  Frank Ludwig, NCBI.
*
* File Description:
*   Feature importer unit test.
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <corelib/ncbi_system.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbifile.hpp>
#include <util/line_reader.hpp>

#include <objtools/import/import_message_handler.hpp>
#include <objtools/import/feat_importer.hpp>

// This header must be included before all Boost.Test headers if there are any
#include <corelib/test_boost.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);  

const string DIRDATA = "unit_test/data";

//  ============================================================================
bool
sGetTestParameters(
    const string& testEntry,
    string& testName,
    string& inputFile,
    string& inputFormat,
    vector<string>& commandArgs)
//  ============================================================================
{
    string testDescr;
    NStr::SplitInTwo(testEntry, ":", testName, testDescr);
    NStr::TruncateSpacesInPlace(testName);
    NStr::TruncateSpacesInPlace(testDescr);

    vector<string> testParts;
    NStr::Split(testDescr, " \t", testParts);
    if (testParts.empty()) {
        return false;
    }
    inputFile = testParts[0];
    auto lastDot = inputFile.find_last_of('.');
    inputFormat = inputFile.substr(lastDot + 1);
    commandArgs.assign(testParts.begin() + 1, testParts.end());
    return true;
}

//  ============================================================================
bool
sSkipTest(
    const CArgs& args,
    const string& testName,
    const string& testFormat)
//  ============================================================================
{
    auto singleCase = args["single-case"].AsString();
    if (!singleCase.empty()) {
        return (singleCase != testName);
    }
    auto singleFormat = args["single-format"].AsString();
    if (!singleFormat.empty()) {
        return (singleFormat != testFormat);
    }
    return false;
}

    
//  ============================================================================
void sRunTest(
    const string& testName,
    const string& format,
    const vector<string>& commandArgs,
    const string& inputName,
    const string& goldenNameAnnot,
    const string& goldenNameErrors,
    const string& outputNameAnnot,
    const string& outputNameErrors,
    const string& diffName,
    bool updateAll)
//  ============================================================================
{
    cerr << "Running test " << testName << " ...\n";
    // produce output:
    CImportMessageHandler msgHandler;
    unique_ptr<CFeatImporter> pImporter(
        CFeatImporter::Get(format, 0, msgHandler));
    CSeq_annot annot;

    CNcbiOfstream oStrErrors(outputNameErrors.c_str());
    CNcbiOfstream oStrAnnot(outputNameAnnot.c_str());
    CNcbiIfstream iStr(inputName.c_str(), ios::binary);
    try {
        while (true) {
            pImporter->ReadSeqAnnot(iStr, annot);
            const auto& annotData = annot.GetData();
            if (!annotData.IsFtable()  ||  annotData.GetFtable().empty()) {
                break;
            }
            oStrAnnot << MSerial_Format_AsnText() << annot;
            if (msgHandler.GetWorstErrorLevel() <= CImportError::CRITICAL) {
                break;
            }
        }
    }
    catch (const CImportError&) {
    }
    catch(std::exception& err) {
        BOOST_ERROR("Error: Test \"" << testName << "\" failed during import:");
        BOOST_ERROR("  " << err.what());
        return;
    }
    iStr.close();
    oStrAnnot.close();
    msgHandler.Dump(oStrErrors);
    oStrErrors.close();

    // compare with golden data:
    bool successAnnot = CFile(goldenNameAnnot).CompareTextContents(
        outputNameAnnot, CFile::eIgnoreWs);
    if (!successAnnot) {
        BOOST_ERROR(
            "Error: Test \"" << testName << "\" failed due to annotation diffs.");
    }
    bool successErrors = CFile(goldenNameErrors).CompareTextContents(
        outputNameErrors, CFile::eIgnoreWs);
    if (!successErrors) {
        BOOST_ERROR(
            "Error: Test \"" << testName << "\" failed due to error diffs.");
    }

    // clean up, if applicable, update golden data:
    if (successAnnot) {
        CFile(outputNameAnnot).Remove();
    }
    else {
        if (updateAll) {
            CFile(outputNameAnnot).Copy(goldenNameAnnot, CFile::fCF_Overwrite);
            CFile(outputNameAnnot).Remove();
        }
    }
    if (successErrors) {
        CFile(outputNameErrors).Remove();
    }
    else {
        if (updateAll) {
            CFile(outputNameErrors).Copy(goldenNameErrors, CFile::fCF_Overwrite);
            CFile(outputNameErrors).Remove();
        }
    }
};

//  ============================================================================
NCBITEST_AUTO_INIT()
//  ============================================================================
{
}


//  ============================================================================
NCBITEST_INIT_CMDLINE(arg_descrs)
//  ============================================================================
{
    arg_descrs->AddDefaultKey(
        "data-dir", 
        "TEST_DATA_DIRECTORY",
        "Set the root directory under which all test files can be found.",
        CArgDescriptions::eString,
        DIRDATA);
    arg_descrs->AddDefaultKey(
        "single-case",
        "TEST_SINGLE_CASE",
        "Limit to only the given case.",
        CArgDescriptions::eString,
        "");
    arg_descrs->AddDefaultKey(
        "single-format",
        "TEST_SINGLE_FORMAT",
        "Limit to only the given format.",
        CArgDescriptions::eString,
        "");
    arg_descrs->AddFlag(
        "update-all",
        "Update all test cases to current reader code.",
        true );
    arg_descrs->AddFlag("keep-diffs",
        "Keep output files that are different from the expected.",
        true );
}

//  ============================================================================
NCBITEST_AUTO_FINI()
//  ============================================================================
{
}

//  ============================================================================
BOOST_AUTO_TEST_CASE(RunTests)
//  ============================================================================
{
    const CArgs& args = CNcbiApplication::Instance()->GetArgs();

    string dataDir( args["data-dir"].AsString() );
    bool updateAll = args["update-all"].AsBoolean();
    bool keepDiffs = args["keep-diffs"].AsBoolean();

    const string fileTestCases(dataDir + "/" + "test-cases.txt");
    const string dirInput(dataDir + "/input/");
    const string dirGolden(dataDir + "/golden/");
    const string dirOutput(dataDir + "/output/");
    const string dirDiffs(dataDir + "/diffs/");

    BOOST_REQUIRE_MESSAGE(
        CFile(fileTestCases).Exists(), 
        "Cannot find test data file: " << fileTestCases);

    // clean out stale results from prior tests:
    auto oldOutputFiles = CDir(dirOutput).GetEntries("", CDir::fIgnoreRecursive);
    for (auto oldFile: oldOutputFiles) {
        oldFile->Remove();
    }

    CNcbiIfstream ifStream(fileTestCases.c_str());
    CStreamLineReader testCaseStream(ifStream);
    while (!testCaseStream.AtEOF()) {
        auto testDescription = *(++testCaseStream);
        NStr::TruncateSpacesInPlace(testDescription);
        if (testDescription.empty()) {
            continue;
        }
        if (NStr::StartsWith(testDescription, '#')) {
            continue;
        }
        string testName, inputFile, inputFormat;
        vector<string> commandArgs;
        BOOST_REQUIRE_MESSAGE(
            sGetTestParameters(testDescription, testName, inputFile, inputFormat, commandArgs),
            "Invalid test description: \"" << testDescription << "\"");
        if (sSkipTest(args, testName, inputFormat)) {
            continue;
        }
        auto inFile = dirInput + inputFile;
        auto goldenFile = dirGolden + testName + ".asn1";
        auto errorsFile = dirGolden + testName + ".errors";
        auto outputFileAnnot = dirOutput + testName + ".asn1";
        auto outputFileErrors = dirOutput + testName + ".errors";
        string diffsFile = "";
        if (keepDiffs) {
            diffsFile = dirDiffs + testName + ".diff";
        }
        BOOST_CHECK_NO_THROW(
            sRunTest(
                testName, inputFormat, commandArgs,
                inFile, goldenFile, errorsFile, 
                outputFileAnnot, outputFileErrors, diffsFile, 
                updateAll));
    }
}
