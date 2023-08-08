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
 * Author:  Igor Filippov
 *
 * File Description:
 *   Unit tests for CFixFeatureId.
 *
 * ===========================================================================
 */

#include <ncbi_pch.hpp>

// This header must be included before all Boost.Test headers if there are any
#include <corelib/test_boost.hpp>
#include <corelib/ncbifile.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <serial/iterator.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objtools/cleanup/fix_feature_id.hpp>

#include <common/test_assert.h>  /* This header must go last */


USING_NCBI_SCOPE;
USING_SCOPE(objects);

static string testfilesDir{"test_cases/FixFeatureIds"};

static void sx_RunTest_Unique_Feature_Ids(const string& testDir, const string& file_name)
{
    string asn_name = CDir::ConcatPath(testDir, file_name+".asn");
    string ref_name = CDir::ConcatPath(testDir, file_name+".ref");
    string out_name = CDir::ConcatPath(testDir, file_name+".out");
    CSeq_entry entry, ref_entry;
    {
        CNcbiIfstream in(asn_name.c_str());
        in >> MSerial_AsnText >> entry;
    }

    CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));;
    CSeq_entry_Handle tse = scope->AddTopLevelSeqEntry(entry);
    entry.Parentize();

    map<CSeq_feat_Handle, CRef<CSeq_feat> > changed_feats;
    CFixFeatureId::s_ApplyToSeqInSet(tse, changed_feats);
    for (auto &fh_feat : changed_feats)
    {
        auto orig_feat = fh_feat.first;
        auto new_feat = fh_feat.second;
        CSeq_feat_EditHandle feh(orig_feat);
        feh.Replace(*new_feat);
    }

    {
        CNcbiIfstream in(ref_name.c_str());
        in >> MSerial_AsnText >> ref_entry;
    }
    if ( !entry.Equals(ref_entry) ) {
        // update delayed parsing buffers for correct ASN.1 text indentation.
        for ( CStdTypeConstIterator<string> it(ConstBegin(entry)); it; ++it ) {
        }
        {
            CNcbiOfstream out(out_name.c_str());
            out << MSerial_AsnText << entry;
        }
#ifdef NCBI_OS_UNIX
        // this call is just for displaying file difference in log
        string cmd = "diff \""+out_name+"\" \""+ref_name+"\"";
        BOOST_CHECK_EQUAL(system(cmd.c_str()), 0);
#endif
        BOOST_CHECK(entry.Equals(ref_entry));
    }
}

static void sx_RunTest_Reassign_Feature_Ids(const string& testDir, const string& file_name)
{
    string asn_name = CDir::ConcatPath(testDir, file_name+".asn");
    string ref_name = CDir::ConcatPath(testDir, file_name+".ref");
    string out_name = CDir::ConcatPath(testDir, file_name+".out");
    CSeq_entry entry, ref_entry;
    {
        CNcbiIfstream in(asn_name.c_str());
        in >> MSerial_AsnText >> entry;
    }

    CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));
    CSeq_entry_Handle tse = scope->AddTopLevelSeqEntry(entry);
    entry.Parentize();

    map<CSeq_feat_Handle, CRef<CSeq_feat> > changed_feats;
    CFixFeatureId::s_ReassignFeatureIds(tse, changed_feats);
    for (auto &fh_feat : changed_feats)
    {
        auto orig_feat = fh_feat.first;
        auto new_feat = fh_feat.second;
        CSeq_feat_EditHandle feh(orig_feat);
        feh.Replace(*new_feat);
    }

    {
        CNcbiIfstream in(ref_name.c_str());
        in >> MSerial_AsnText >> ref_entry;
    }
    if ( !entry.Equals(ref_entry) ) {
        // update delayed parsing buffers for correct ASN.1 text indentation.
        for ( CStdTypeConstIterator<string> it(ConstBegin(entry)); it; ++it ) {
        }
        {
            CNcbiOfstream out(out_name.c_str());
            out << MSerial_AsnText << entry;
        }
#ifdef NCBI_OS_UNIX
        // this call is just for displaying file difference in log
        string cmd = "diff \""+out_name+"\" \""+ref_name+"\"";
        BOOST_CHECK_EQUAL(system(cmd.c_str()), 0);
#endif
        BOOST_CHECK(entry.Equals(ref_entry));
    }
}


NCBITEST_AUTO_INIT()
{
}


NCBITEST_INIT_CMDLINE(arg_descrs)
{
    arg_descrs->AddDefaultKey("test-dir", "TEST_FILE_DIRECTORY",
        "Set the root directory under which all test files can be found.",
        CArgDescriptions::eDirectory,
        testfilesDir);
}


NCBITEST_AUTO_FINI()
{
}


BOOST_AUTO_TEST_CASE(TestExample1)
{
    const CArgs& args = CNcbiApplication::Instance()->GetArgs();
    const string testDir = args["test-dir"].AsString();
    sx_RunTest_Unique_Feature_Ids(testDir, "GB-2561");
}


BOOST_AUTO_TEST_CASE(TestExample2)
{
    const CArgs& args = CNcbiApplication::Instance()->GetArgs();
    const string testDir = args["test-dir"].AsString();
    sx_RunTest_Reassign_Feature_Ids(testDir, "comparison_1.raw_test");
}




