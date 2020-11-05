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
* Author:  Justin Foley, NCBI.
*
* File Description:
*   Alignment reader unit test.
*
* ===========================================================================
*/


#include <ncbi_pch.hpp>

#include <corelib/ncbi_system.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbifile.hpp>

#include <objtools/readers/reader_error_codes.hpp>
#include <objtools/readers/aln_reader.hpp>
#include <objtools/readers/fasta.hpp>

#include <cstdio>

// This header must be included before any Boost.Test header. 
#include <corelib/test_boost.hpp>


USING_NCBI_SCOPE;
USING_SCOPE(objects);

//  ============================================================================
class CErrorLogger:
//  ============================================================================
    public CMessageListenerBase
{
public:
    CErrorLogger(
            const string& filename) {
        mStream.open(filename.c_str());
    };

    ~CErrorLogger() {
        mStream.close();
    };
    
    bool
    PutError(
        const ILineError& err ) 
    {
        StoreError(err);

        auto code = err.GetCode();
        string codeStr = ENUM_METHOD_NAME(EReaderCode)()->FindName(err.GetCode(), false);
        string subCodeStr;
        switch(code) {
        default:
            break;
        case EReaderCode::eReader_Alignment:
            subCodeStr = ENUM_METHOD_NAME(EAlnSubcode)()->FindName(err.GetSubCode(), false);
            break;
        case EReaderCode::eReader_Mods:
            subCodeStr = ENUM_METHOD_NAME(EModSubcode)()->FindName(err.GetSubCode(), false);
            break;
        }

        if (!err.SeqId().empty()) {  
            mStream << "SeqID      : " << err.SeqId() << endl;
        }   

        if (err.Line() == 0) { 
            mStream << "Line No    : " 
                    << "None (Encountered during pre or post processing)" << endl;
        }
        else {
            mStream << "Line No    : " << err.Line() << endl;
        }
        mStream << "Severity   : " << err.SeverityStr() << endl;

        if (err.GetCode()) {
            mStream << "Problem    : " << codeStr << "." << subCodeStr << endl;
        }

        vector<string> messageLines;
        NStr::Split(err.Message(), "\n", messageLines);
        if (!messageLines.empty()) {
            mStream << "Message    : " << messageLines.front() << endl;
            for (auto i=1; i < messageLines.size(); ++i) {
                mStream << "             " << messageLines[i] << endl;
            }
        }

        auto qualName = err.QualifierName();
        if (!qualName.empty()) {
            mStream << "Mod Name   : " << qualName << endl;
        }

        auto qualValue = err.QualifierValue();
        if (!qualValue.empty()) {
            mStream << "Mod Value  : " << qualValue << endl;
        }

        mStream << endl;

        if (Count() < 500000  &&  err.Severity() != eDiag_Fatal) {
            return true;
        }
        NCBI_THROW2(CObjReaderParseException, eFormat, err.Message(), 0);
        return false;
    };

protected:
    ofstream mStream;
};    

// Customization data
const string extInput("aln");
const string extOutput("asn");
const string extErrors("errors");
const string extKeep("new");
const string dirTestFiles("alnreader_test_cases");


struct STestInfo {
    CFile mInFile;
    CFile mOutFile;
    CFile mErrorFile;
};

using TTestName = string;
using TTestNameToInfoMap = map<TTestName, STestInfo>;

class  CTestNameToInfoMapLoader {

public:
    CTestNameToInfoMapLoader(
        TTestNameToInfoMap* pTestNameToInfoMap,
        const string& extInput,
        const string& extOutput,
        const string& extErrors)
    : m_pTestNameToInfoMap(pTestNameToInfoMap),
      mExtInput(extInput),
      mExtOutput(extOutput),
      mExtErrors(extErrors)
    {}

    void operator()(const CDirEntry& dirEntry) {
        if (!dirEntry.IsFile()) {
            return;
        }

        CFile file(dirEntry);
        string name = file.GetName();
        if (NStr::EndsWith(name, ".txt")  ||  
                NStr::EndsWith(name, ".new")  ||
                NStr::EndsWith(name, ".sh")  ||  
                NStr::StartsWith(name, ".")) {
            return;
        }
        // extract info from the file name
        const string sFileName = file.GetName();
        vector<CTempString> vecFileNamePieces;
        NStr::Split(sFileName, ".", vecFileNamePieces);
        BOOST_REQUIRE(vecFileNamePieces.size() == 2);

        CTempString tsTestName = vecFileNamePieces[0];
        BOOST_REQUIRE(!tsTestName.empty());
        CTempString tsFileType = vecFileNamePieces[1];
        BOOST_REQUIRE(!tsFileType.empty());
    
        STestInfo & test_info_to_load = 
            (*m_pTestNameToInfoMap)[vecFileNamePieces[0]];

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
    TTestNameToInfoMap * m_pTestNameToInfoMap;
    string mExtInput;
    string mExtOutput;
    string mExtErrors;
};

CAlnReader*
sGetReader(
    const string& testName,
    ifstream& ifstr)
{
    auto pReader = new CAlnReader(ifstr);
    if (!NStr::EndsWith(testName, "prot")) {
        pReader->SetAlphabet(CAlnReader::eAlpha_Nucleotide);
    }
    if (NStr::EndsWith(testName, "-cni")) {
        pReader->SetUseNexusInfo(false);
    }
    return pReader;
}

CAlnReader::TFastaFlags
sGetFastaFlags(
    const string& testName)
{
    return (NStr::FindNoCase(testName, "no-mods") == NPOS) ? CFastaReader::fAddMods : 0;
}

void sUpdateCase(CDir& test_cases_dir, const string& test_name) 
{
    string input = CDir::ConcatPath( test_cases_dir.GetPath(), test_name + "." + extInput);
    string output = CDir::ConcatPath( test_cases_dir.GetPath(), test_name + "." + extOutput);
    string errors = CDir::ConcatPath( test_cases_dir.GetPath(), test_name + "." + extErrors);

    if (!CFile(input).Exists()) {
        BOOST_FAIL("input file " << input << " does not exist.");
    }
    cerr << "Creating new test case from " << input << " ..." << endl;

    CErrorLogger logger(errors);
    CNcbiIfstream ifstr(input.c_str());
    unique_ptr<CAlnReader> pReader(sGetReader(test_name, ifstr));

    CAlnReader::TReadFlags flags = 0;
    if (NStr::EndsWith(test_name, "-lcl")) {
        flags |= CAlnReader::fGenerateLocalIDs;
    }

    CRef<CSeq_entry> pEntry;
    try {
        pReader->Read(flags, &logger);
        pEntry = pReader->GetSeqEntry(sGetFastaFlags(test_name), &logger);
    } 
    catch (std::exception&) {
    }
    ifstr.close();
    cerr << " Produced new error listing " << output << "." << endl;

    CNcbiOfstream ofstr(output.c_str());
    if (pEntry) {
        ofstr << MSerial_AsnText << *pEntry;
    }
    ofstr.close();
    cerr << "    Produced new ASN1 file " << output << "." << endl;
    cerr << " ... Done." << endl;
}


void sUpdateAll(CDir& test_cases_dir) {
    
    const vector<string> kEmptyStringVec;
    TTestNameToInfoMap testNameToInfoMap;
    CTestNameToInfoMapLoader testInfoLoader(
        &testNameToInfoMap, extInput, extOutput, extErrors);
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


void sRunTest(const string &sTestName, const STestInfo& testInfo, bool keep)
{
    cerr << "Testing " << testInfo.mInFile.GetName() << " against " << 
        testInfo.mOutFile.GetName() << " and " << 
        testInfo.mErrorFile.GetName() << endl;


    CNcbiIfstream ifstr(testInfo.mInFile.GetPath().c_str());
    string newOutput = CDirEntry::GetTmpName();
    string newErrors = CDirEntry::GetTmpName();
    CErrorLogger logger(newErrors.c_str());

    unique_ptr<CAlnReader> pReader(sGetReader(sTestName, ifstr));

    CAlnReader::TReadFlags flags = 0;
    if (NStr::EndsWith(sTestName, "-lcl")) {
        flags |= CAlnReader::fGenerateLocalIDs;
    }

    CRef<CSeq_entry> pEntry;
    try {
        pReader->Read(flags, &logger);
        pEntry = pReader->GetSeqEntry(sGetFastaFlags(sTestName), &logger);
    } 
    catch (CObjReaderParseException&) {
        cerr << "";
    }
    ifstr.close();

    CNcbiOfstream ofstr(newOutput.c_str());
    if (pEntry) {
        ofstr << MSerial_AsnText << *pEntry;
    }
    ofstr.close();

    bool successOutput = 
        testInfo.mOutFile.CompareTextContents(newOutput, CFile::eIgnoreWs);
    bool successErrors =
        testInfo.mErrorFile.CompareTextContents(newErrors, CFile::eIgnoreWs);
    if (!successOutput) {
        CDirEntry deNewOutput= CDirEntry(newOutput);
        if (keep) {
            deNewOutput.Copy(testInfo.mOutFile.GetPath() + "." + extKeep);
        }
    }   
    if (!successErrors) {
        CDirEntry deNewErrors= CDirEntry(newErrors);
        if (keep) {
            deNewErrors.Copy(testInfo.mErrorFile.GetPath() + "." + extKeep);
        }
    }   
    if (!successOutput  ||  !successErrors) {
        BOOST_ERROR("Error: " << sTestName << " failed due to post processing diffs.");
    }
    CDirEntry(newOutput).Remove();
    CDirEntry(newErrors).Remove();
}

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
    arg_descrs->AddDefaultKey("single-case",
        "SINGLE_CASE", 
        "Run specified case only",
        CArgDescriptions::eString,
        "");
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
                                   
    string single_case = args["single-case"].AsString();
    if (!single_case.empty()) {
        STestInfo testInfo;
        testInfo.mInFile = CDir::ConcatPath( 
            test_cases_dir.GetPath(), single_case + "." + extInput);
        testInfo.mOutFile = CDir::ConcatPath(
            test_cases_dir.GetPath(), single_case + "." + extOutput);
        testInfo.mErrorFile = CDir::ConcatPath( 
            test_cases_dir.GetPath(), single_case + "." + extErrors);
        BOOST_CHECK_NO_THROW(
            sRunTest(single_case, testInfo, args["keep-diffs"]));
        return;
    }

    const vector<string> kEmptyStringVec;
    TTestNameToInfoMap testNameToInfoMap;
    CTestNameToInfoMapLoader testInfoLoader(&testNameToInfoMap, extInput, extOutput, extErrors);
    FindFilesInDir(test_cases_dir,
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
//        BOOST_REQUIRE_MESSAGE( testInfo.mErrorFile.Exists(),
//            extErrors + " file does not exist: " << testInfo.mErrorFile.GetPath() );
    }

    ITERATE(TTestNameToInfoMap, name_to_info_it, testNameToInfoMap) {
        const string & sName = name_to_info_it->first;
        const STestInfo & testInfo = name_to_info_it->second;
        cout << "Running test: " << sName << endl;

        BOOST_CHECK_NO_THROW(sRunTest(sName, testInfo, args["keep-diffs"]));
    }   
}
