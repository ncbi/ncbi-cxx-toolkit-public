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

#include <cstdio>
#include <objtools/readers/mod_reader.hpp>
#include <corelib/test_boost.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);

const string extInput("input");
const string extOutput("output");
const string extErrors("errors");
const string extKeep("new");
const string dirTestFiles("mod_handler_test_cases");


struct STestInfo {
    CFile mInFile;
    CFile mOutFile;
    CFile mErrorFile;
};

using TTestName = string;
using TTestNameToInfoMap = map<TTestName, STestInfo>;

class CTestNameToInfoMapLoader
{
public:
    CTestNameToInfoMapLoader(
        TTestNameToInfoMap& testNameToInfoMap,
        const string& extInput,
        const string& extOutput,
        const string& extErrors)
    : m_TestNameToInfoMap(testNameToInfoMap),
      mExtInput(extInput),
      mExtOutput(extOutput),
      mExtErrors(extErrors) {}


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


static bool sFindBrackets(const CTempString& line, size_t& start, size_t& stop, size_t& eq_pos)
{ // Copied from CSourceModParser
    size_t i = start;
    bool found = false;

    eq_pos = CTempString::npos;
    const char* s = line.data() + start;

    int nested_brackets = -1;
    while (i < line.size())
    {
        switch (*s)
        {
        case '[':
            nested_brackets++;
            if (nested_brackets == 0)
            {
                start = i;
            }
            break;
        case '=':
            if (nested_brackets >= 0 && eq_pos == CTempString::npos) {
                eq_pos = i;
            }
            break;
        case ']':
            if (nested_brackets == 0)
            {
                stop = i;
                return true;
               // if (eq_pos == CTempString::npos)
               //     eq_pos = i;
               // return true;
            }
            else
            if (nested_brackets < 0) {
                return false;
            }
            else
            {
                nested_brackets--;
            }
        }
        i++; s++;
    }
    return false;
};


static bool sGetMods(const CTempString& title, multimap<string, string>& mods)
{
    size_t pos = 0;
    while(pos < title.size()) {
        size_t lb_pos, end_pos, eq_pos;
        lb_pos = pos;
        if (sFindBrackets(title, lb_pos, end_pos, eq_pos))
        {
            if (eq_pos < end_pos) {
                CTempString key = NStr::TruncateSpaces_Unsafe(title.substr(lb_pos+1, eq_pos - lb_pos - 1));
                CTempString value = NStr::TruncateSpaces_Unsafe(title.substr(eq_pos + 1, end_pos - eq_pos - 1));
                mods.emplace(key, value);
            }
            pos = end_pos + 1;
        }
        else
        {
            break;
        }
    }
    return !mods.empty();
}


struct SModInfo {
    string name;
    string value;
    CModHandler::EHandleExisting handle_existing;
};


static CModHandler::EHandleExisting sGetHandleExisting(const string& handle_existing)
{
    if (handle_existing == "replace") {
        return CModHandler::eReplace;
    }

    if (handle_existing == "preserve") {
        return CModHandler::ePreserve;
    }

    if (handle_existing == "append-replace") {
        return CModHandler::eAppendReplace;
    }

    if (handle_existing == "append-preserve") {
        return CModHandler::eAppendPreserve;
    }

    // default
    return CModHandler::eReplace;
}


static void sGetModInfo(const string& line, SModInfo& mod_info)
{
    if (NStr::IsBlank(line)) {
        return;
    }

    vector<string> info_vec;
    NStr::Split(line, " \t", info_vec, NStr::fSplit_Tokenize);

    if (info_vec.size() < 2) {
        return;
    }


    mod_info.name = info_vec[0];

    if (info_vec.size() == 3) {
        mod_info.value = info_vec[1];
        mod_info.handle_existing = sGetHandleExisting(info_vec[2]);
        return;
    }

    if (info_vec.size() == 2) {
        mod_info.handle_existing = sGetHandleExisting(info_vec[1]);
        return;
    }
}

void sRunTest(const string &sTestName, const STestInfo & testInfo, bool keep)
{
    cerr << "Testing " << testInfo.mInFile.GetName() << " against " <<
        testInfo.mOutFile.GetName() << " and " <<
        testInfo.mErrorFile.GetName() << endl;

    string logName = CDirEntry::GetTmpName();

    CNcbiIfstream ifstr(testInfo.mInFile.GetPath().c_str());
    const string& resultName = CDirEntry::GetTmpName();
    CNcbiOfstream ofstr(resultName.c_str());
    CModHandler mod_handler;
    try {
        multimap<string, string> mods;
        for (string line; getline(ifstr, line);) {
            SModInfo mod_info;
            sGetModInfo(line, mod_info);
            mod_handler.AddMod(mod_info.name,
                               mod_info.value,
                               mod_info.handle_existing);
        }


        for (auto mod_entry : mod_handler.GetNormalizedMods()) {
            for (const auto& value_attrib : mod_entry.second) {
                ofstr << mod_entry.first << "  " << value_attrib.GetValue() << endl;
            }
        }
        //edit::CModApply mod_apply(mods);
       // bool replace_preexisting_vals = true;
       // mod_apply.Apply(bioseq);
    }
    catch (...) {
        BOOST_ERROR("Error: " << sTestName << " failed during conversion.");
        ifstr.close();
        return;
    }

    ifstr.close();
    ofstr.close();


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
     //   sUpdateAll(test_cases_dir);
        return;
    }

    string update_case = args["update-case"].AsString();
    if (!update_case.empty()) {
     //   sUpdateCase(test_cases_dir, update_case);
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
