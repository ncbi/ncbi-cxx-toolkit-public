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
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/general/Object_id.hpp>
#include <objtools/readers/fasta.hpp>
#include <objtools/readers/message_listener.hpp>

#include <cstdio>

// This header must be included before all Boost.Test headers if there are any
#include <corelib/test_boost.hpp>


USING_NCBI_SCOPE;
USING_SCOPE(objects);

//  ============================================================================
//  Customization data:
const string extConfig("cfg");
const string extInput("fsa");
const string extOutput("asn");
const string extErrors("errors");
const string extKeep("new");
const string dirTestFiles("fasta_reader_test_cases");
// !!!
// !!! Must also customize reader type in sRunTest !!!
// !!!
//  ============================================================================

static CFastaReader::TFlags s_StringToFastaFlag(const string& flag_string)
{
    map<string, CFastaReader::EFlags> sFlagsMap =
    {
        { "fAssumeNuc",    CFastaReader::fAssumeNuc},
        { "fAssumeProt",   CFastaReader::fAssumeProt},
        { "fForceType",    CFastaReader::fForceType},
        { "fNoParseID",    CFastaReader::fNoParseID},
        { "fParseGaps",    CFastaReader::fParseGaps},
        { "fOneSeq",       CFastaReader::fOneSeq},
        { "fNoSeqData",    CFastaReader::fNoSeqData},
        { "fRequireID",    CFastaReader::fRequireID},
        { "fDLOptional",   CFastaReader::fDLOptional},
        { "fParseRawID",   CFastaReader::fParseRawID},
        { "fSkipCheck",    CFastaReader::fSkipCheck},
        { "fNoSplit",      CFastaReader::fNoSplit},
        { "fValidate",     CFastaReader::fValidate},
        { "fUniqueIDs",    CFastaReader::fUniqueIDs},
        { "fStrictGuess",  CFastaReader::fStrictGuess},
        { "fLaxGuess",     CFastaReader::fLaxGuess},
        { "fAddMods",      CFastaReader::fAddMods},
        { "fLetterGaps",   CFastaReader::fLetterGaps},
        { "fNoUserObjs",   CFastaReader::fNoUserObjs},
        { "fLeaveAsText",  CFastaReader::fLeaveAsText},
        { "fQuickIDCheck", CFastaReader::fQuickIDCheck},
        { "fUseIupacaa",   CFastaReader::fUseIupacaa},
        { "fHyphensIgnoreAndWarn", CFastaReader::fHyphensIgnoreAndWarn},
        { "fDisableNoResidues",    CFastaReader::fDisableNoResidues},
        { "fDisableParseRange",    CFastaReader::fDisableParseRange},
        { "fIgnoreMods",           CFastaReader::fIgnoreMods}
    };

    auto it = sFlagsMap.find(flag_string);
    if (it != end(sFlagsMap)) {
        return static_cast<CFastaReader::TFlags>(it->second);
    }

    string message = "Unrecognized FASTA flag : " + it->second;
    NCBI_THROW(CException, eUnknown, message);

    return 0;
}

struct SConfigInfo
{
    list<CFastaReader::TFlags> flags;
    vector<string> excluded_mods;
    enum class EEntryType {
        eBioseq,
        eBioset
    };
    EEntryType entry_type = EEntryType::eBioseq;
};


static void s_ReadConfig(CNcbiIfstream& ifstr, SConfigInfo& config)
{
    for (string line; getline(ifstr, line);) {
        NStr::TruncateSpaces(line);
        if (line.empty()) {
            continue;
        }
        list<string> tokens;
        NStr::Split(line, " \t", tokens, NStr::fSplit_Tokenize);

        if (tokens.front() == "FLAGS") {
            transform(next(begin(tokens)), end(tokens),
                    back_inserter(config.flags), s_StringToFastaFlag);
            continue;
        }

        if (tokens.front() == "EXCLUDED_MODS") {
            copy(next(begin(tokens)), end(tokens),
                    back_inserter(config.excluded_mods));
            continue;
        }

        if (tokens.front() == "ENTRY_TYPE") {
            if (tokens.back() == "Bioset") {
                config.entry_type = SConfigInfo::EEntryType::eBioset;
            }
        }
    }
}

struct STestInfo {
    CFile mConfigFile;
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
        const string& extConfig,
        const string& extInput,
        const string& extOutput,
        const string& extErrors)
        : m_TestNameToInfoMap(testNameToInfoMap),
          mExtConfig(extConfig),
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
        if (tsFileType == mExtConfig) {
            BOOST_REQUIRE( test_info_to_load.mConfigFile.GetPath().empty() );
            test_info_to_load.mConfigFile = file;
        }
        else
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
    string mExtConfig;
    string mExtInput;
    string mExtOutput;
    string mExtErrors;
};

static bool
sReadInputAndGenerateAsn(
        const string& fasta_file,
        const string& config_file,
        CRef<CSeq_entry>& pSeqEntry,
        CMessageListenerBase*  pMessageListener)
{
    CNcbiIfstream ifstr(fasta_file.c_str());
    CRef<ILineReader> pLineReader = ILineReader::New(ifstr);

    CFastaReader::TFlags fFlags = 0;
    SConfigInfo config_info;
    CNcbiIfstream cfgstr(config_file.c_str());
    s_ReadConfig(
        cfgstr,
        config_info);
    if (!config_info.flags.empty()) {
        for (auto flag : config_info.flags) {
            fFlags |= flag;
        }
    }

    try {
        CFastaReader fasta_reader(*pLineReader, fFlags);
        if (!config_info.excluded_mods.empty()) {
            fasta_reader.SetExcludedMods(config_info.excluded_mods);
        }
        if (config_info.entry_type == SConfigInfo::EEntryType::eBioseq) {
            pSeqEntry = fasta_reader.ReadOneSeq(pMessageListener);
        }
        else {
            pSeqEntry = fasta_reader.ReadSet(100, pMessageListener);
        }
    }
    catch (...) {
        ifstr.close();
        cfgstr.close();
        throw;
    }
    return true;
}

void sUpdateCase(CDir& test_cases_dir, const string& test_name)
{
    string cfg    = CDir::ConcatPath( test_cases_dir.GetPath(), test_name + "." + extConfig);
    string input  = CDir::ConcatPath( test_cases_dir.GetPath(), test_name + "." + extInput);
    string output = CDir::ConcatPath( test_cases_dir.GetPath(), test_name + "." + extOutput);
    string errors = CDir::ConcatPath( test_cases_dir.GetPath(), test_name + "." + extErrors);
    if (!CFile(input).Exists()) {
         BOOST_FAIL("input file " << input << " does not exist.");
    }
    if (!CFile(cfg).Exists()) {
         BOOST_FAIL("config file " << cfg << " does not exist.");
    }
    cerr << "Creating new test case from " << input << " ..." << endl;

   // CNcbiIfstream ifstr(input.c_str());
    unique_ptr<CMessageListenerBase> pMessageListener(new CMessageListenerLenient());
    CRef<CSeq_entry> pSeqEntry;

    if (!sReadInputAndGenerateAsn(input, cfg,
                pSeqEntry, pMessageListener.get())) {

        BOOST_FAIL("Error: " << input << " failed during conversion.");
        return;
    }

    CNcbiOfstream ofstr(output.c_str());
    ofstr << MSerial_AsnText << pSeqEntry;
    ofstr.close();
    cerr << "    Produced new ASN1 file " << output << "." << endl;

    CNcbiOfstream errstr(errors.c_str());
    if (pMessageListener->Count() > 0) {
        pMessageListener->Dump(errstr);
    }
    errstr.close();
    cerr << "    Produced new error listing " << errors << "." << endl;
    cerr << " ... Done." << endl;
}


void sUpdateAll(CDir& test_cases_dir) {

    const vector<string> kEmptyStringVec;
    TTestNameToInfoMap testNameToInfoMap;
    CTestNameToInfoMapLoader testInfoLoader(
        testNameToInfoMap, extConfig, extInput, extOutput, extErrors);
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
    cerr << "Testing " <<
        testInfo.mConfigFile.GetName() << " and " <<
        testInfo.mInFile.GetName() << " against " <<
        testInfo.mOutFile.GetName() << " and " <<
        testInfo.mErrorFile.GetName() << endl;

    const string& logName = CDirEntry::GetTmpName();
    CNcbiOfstream errstr(logName.c_str());
    const string& resultName = CDirEntry::GetTmpName();
    CNcbiOfstream ofstr(resultName.c_str());


    unique_ptr<CMessageListenerBase> pMessageListener(new CMessageListenerLenient());
    CRef<CSeq_entry> pSeqEntry;

    if (!sReadInputAndGenerateAsn(
            testInfo.mInFile.GetPath(),
            testInfo.mConfigFile.GetPath(),
            pSeqEntry,
            pMessageListener.get())) {
        BOOST_ERROR("Error: " << sTestName << " failed during conversion.");
        return;
    }

    ofstr << MSerial_AsnText << pSeqEntry;
    if (pMessageListener->Count() > 0) {
        pMessageListener->Dump(errstr);
    }
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
        testNameToInfoMap, extConfig, extInput, extOutput, extErrors);
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
        BOOST_REQUIRE_MESSAGE( testInfo.mConfigFile.Exists(),
            extConfig + " file does not exist: " << testInfo.mConfigFile.GetPath() );
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
