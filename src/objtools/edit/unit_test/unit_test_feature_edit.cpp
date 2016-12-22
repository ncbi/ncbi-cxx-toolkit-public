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
* Author:  Justin Foley, NCBI
*
* File Description:
*   Unit tests for feature editing - based on the writer unit tests
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/mapped_feat.hpp>
#include <objmgr/seq_feat_handle.hpp>

//#include "unit_test_feature_edit.hpp"
#include <corelib/ncbi_system.hpp>
#include <corelib/ncbifile.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objtools/edit/feature_edit.hpp>
#include <objects/blastxml2/Range.hpp>

// This header must be included before all Boost.Test headers if there are any
#include <corelib/test_boost.hpp>

#include <common/test_assert.h> // This header must go last


USING_NCBI_SCOPE;
USING_SCOPE(objects);

CRef<CSeq_feat> s_ReadFeat(const string& filename) 
{
    ifstream in_file(filename.c_str());
    BOOST_REQUIRE(in_file.good());
    CRef<CSeq_feat> pFeat(new CSeq_feat());
    in_file >> MSerial_AsnText >> *pFeat;

    return pFeat;
}

bool s_AreFeatsEqual(
    const CSeq_feat& feat1,
    const CSeq_feat& feat2)
{
    return feat1.Equals(feat2);
}

struct STestInfo {
    CFile m_InFile;
    CFile m_BaselineFile;
    CRange<TSeqPos> m_Range;
};

using TTestName = string;
using TTestNameToInfoMap = map<TTestName, STestInfo>;


CRef<CScope> s_pScope;

class CTestNameToInfoMapLoader {
public:
    CTestNameToInfoMapLoader(
        TTestNameToInfoMap* pTestNameToInfoMap,
        const string& extInput,
        const string& extBaseline) : 
        m_pTestNameToInfoMap(pTestNameToInfoMap),
        m_ExtInput(extInput),
        m_ExtBaseline(extBaseline) {}
        

    void operator()(const CDirEntry& dirEntry) {
        
        if (!dirEntry.IsFile()) {
            return;
        }

        CFile file(dirEntry);

        const string fileName = file.GetName();
        if (NStr::StartsWith(fileName, ".")) {
            return;
        }

        vector<string> vecFileNamePieces;
        NStr::Split(fileName, ".", vecFileNamePieces);
        BOOST_REQUIRE(vecFileNamePieces.size() == 3);

        const string testRange = vecFileNamePieces[1];
        BOOST_REQUIRE(!testRange.empty());

        const string testName = vecFileNamePieces[0] + "_" + testRange ;
        BOOST_REQUIRE(!testName.empty());

        vector<string> vecRange;
        NStr::Split(testRange, "_", vecRange);
        BOOST_REQUIRE(vecRange.size() == 2);

        const string fileType = vecFileNamePieces[2];
        BOOST_REQUIRE(!fileType.empty());

        STestInfo& test_info_to_load = 
            (*m_pTestNameToInfoMap)[testName];

        if (test_info_to_load.m_Range.Empty()) {
            TSeqPos start = NStr::StringToInt(vecRange[0]);
            TSeqPos stop = NStr::StringToInt(vecRange[1]);
            test_info_to_load.m_Range = CRange<TSeqPos>(start, stop);
        }

        if (fileType == m_ExtInput) {
            BOOST_REQUIRE( test_info_to_load.m_InFile.GetPath().empty() );
            test_info_to_load.m_InFile = file;
        } 
        else 
        if (fileType == m_ExtBaseline) {
            BOOST_REQUIRE( test_info_to_load.m_BaselineFile.GetPath().empty() );
            test_info_to_load.m_BaselineFile = file;
        }
        else {
            BOOST_FAIL("Unknown file type " << fileName << ".");
        }

    }
private:
    // raw pointer because we do not own this
    TTestNameToInfoMap* m_pTestNameToInfoMap;
    string m_ExtInput;
    string m_ExtBaseline;
};


void sRunFeatureTrimTest(const string& testName, const STestInfo& testInfo)
{

    cout << "Running feature-trim test\n";

    if (!testName.empty()) {
        cerr << "Running " << testName << "\n";
    }

    // Looks like I will need to 
    CRef<CSeq_feat> infeat = s_ReadFeat(testInfo.m_InFile.GetPath());
    CRef<CSeq_feat> baseline = s_ReadFeat(testInfo.m_BaselineFile.GetPath());

    CRef<CSeq_annot> temp_annot = Ref(new CSeq_annot());
    temp_annot->SetData().SetFtable().push_back(infeat);
    s_pScope->AddSeq_annot(*temp_annot);

    CSeq_feat_Handle sfh = s_pScope->GetSeq_featHandle(*infeat);

    if (!sfh) {
        cout << "Failed to get sfh" << endl;
    }

    CMappedFeat mapped_feat(sfh);
    mapped_feat = edit::CFeatTrim::Apply(mapped_feat, testInfo.m_Range);

    const CSeq_feat& trimmed_feat = mapped_feat.GetMappedFeature();

    const bool success = trimmed_feat.Equals(*baseline);
    if (!success) {
        BOOST_ERROR("Error: " << testName << " failed.");
    }

}

const string dirTestFiles("feature_edit_test_cases");

NCBITEST_AUTO_INIT()
{
    s_pScope = Ref(new CScope(*CObjectManager::GetInstance()));
}


NCBITEST_INIT_CMDLINE(arg_descrs)
{
    arg_descrs->AddDefaultKey("test-dir", "TEST_FILE_DIRECTORY",
        "Set the root directory under which all test files can be found.",
        CArgDescriptions::eDirectory,
        dirTestFiles);
}


NCBITEST_AUTO_FINI()
{
}


BOOST_AUTO_TEST_CASE(RunTests)
{
    const string extInput("input");
    const string extBaseline("baseline");

    const CArgs& args = CNcbiApplication::Instance()->GetArgs();

    CDir test_cases_dir(args["test-dir"].AsDirectory());
    BOOST_REQUIRE_MESSAGE(test_cases_dir.IsDir(),
        "Cannot find dir: " << test_cases_dir.GetPath());


    const vector<string> kEmptyStringVec;
    TTestNameToInfoMap testNameToInfoMap;
    CTestNameToInfoMapLoader testInfoLoader(
        &testNameToInfoMap, extInput, extBaseline);

    FindFilesInDir(
        test_cases_dir,
        kEmptyStringVec,
        kEmptyStringVec,
        testInfoLoader,
        fFF_Default | fFF_Recursive);

    ITERATE(TTestNameToInfoMap, name_to_info_it, testNameToInfoMap) {
        const string& testName = name_to_info_it->first;
        const STestInfo& testInfo = name_to_info_it->second;
        cout << "Verifying: " << testName << endl;
        BOOST_REQUIRE_MESSAGE(testInfo.m_InFile.Exists(),
            extInput + " file does not exist: " << testInfo.m_InFile.GetPath());
        BOOST_REQUIRE_MESSAGE(testInfo.m_BaselineFile.Exists(),
            extBaseline + " file does not exits: " << testInfo.m_BaselineFile.GetPath());
    }
    
    ITERATE(TTestNameToInfoMap, name_to_info_it, testNameToInfoMap) {
        const string& testName = name_to_info_it->first;
        const STestInfo& testInfo = name_to_info_it->second;

        BOOST_CHECK_NO_THROW(sRunFeatureTrimTest(testName, testInfo));
    }
}




