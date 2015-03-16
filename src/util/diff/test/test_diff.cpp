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
 * Authors:  Vladimir Ivanov
 *
 * File Description:  Diff API test program.
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbifile.hpp>
#include <util/diff/diff.hpp>
#include <util/dictionary_util.hpp>

#include <common/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;

// Allow internal tests.
// Make all methods/members public in the CDiff/CDiffList class first.
#define ALLOW_INTERNAL_TESTS 0


class CTestApplication : public CNcbiApplication
{
private:
    virtual void Init(void);
    virtual int  Run(void);
};


void CTestApplication::Init(void)
{
    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "Test program for CDiff");
    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}

// Local defines to simplicity a reading the code
#define DIFF_DELETE CDiffOperation::eDelete
#define DIFF_EQUAL  CDiffOperation::eEqual
#define DIFF_INSERT CDiffOperation::eInsert


// Print difference list in human readable format
void s_PrintDiff(const CDiffList& diff)
{
    ITERATE(CDiffList::TList, it, diff.GetList()) {
        string op;
        size_t n1 = 0;
        size_t n2 = 0;

        if (it->IsDelete()) {
            op = "-";
            n1 = it->GetLine().first;
        } else if (it->IsInsert()) {
            op = "+";
            n2 = it->GetLine().second;
        } else {
            op = "=";
            n1 = it->GetLine().first;
            n2 = it->GetLine().second;
        }
        cout << op << " ("
             << n1 << "," << n2 << ")"
             << ": " << "'" << it->GetString() << "'" << endl;
    }
}

// Internal tests.
// Make all methods/members public in CDiff/CDiffList class first.
#if ALLOW_INTERNAL_TESTS
// CDiff::x_DiffHalfMatch
void s_Internal_DiffHalfMatch(void)
{
    struct HalfMatchTest {
        HalfMatchTest(CTempString s1, CTempString s2, bool res,
                        CTempString hm_str = kEmptyStr,
                        int timeout_sec = 1000)
        {
            CDiff d;
            // Set timeout
            CTimeout timeout(CTimeout::eInfinite);
            if (timeout_sec >= 0) {
                timeout.Set(timeout_sec, 0);
            }
            d.SetTimeout(timeout);
            auto_ptr<CDeadline> real_deadline(new CDeadline(timeout));
            d.m_Deadline = real_deadline.get();
            // Find half match
            CDiff::TDiffHalfMatchList hm(5);
            bool r = d.x_DiffHalfMatch(s1, s2, hm);
            assert(r == res);
            if ( r ) {
                string s = (string)hm[0] + "," + (string)hm[1] + "," + (string)hm[2] + "," + (string)hm[3] + "," + (string)hm[4];
                assert(s == hm_str);
            }
        }
    };

    // No match
    HalfMatchTest ("1234567890", "abcdef", false);
    HalfMatchTest ("12345", "23", false);
    // Single match
    HalfMatchTest ("12345", "23", false);
    HalfMatchTest ("1234567890", "a345678z", true, "12,90,a,z,345678");
    HalfMatchTest ("abc56789z", "1234567890", true, "abc,z,1234,0,56789");
    HalfMatchTest ("a23456xyz", "1234567890", true, "a,xyz,1,7890,23456");
    // Multiple matches
    HalfMatchTest ("121231234123451234123121", "a1234123451234z", true, "12123,123121,a,z,1234123451234");
    HalfMatchTest ("x-=-=-=-=-=-=-=-=-=-=-=-=", "xx-=-=-=-=-=-=-=", true, ",-=-=-=-=-=,x,,x-=-=-=-=-=-=-=");
    HalfMatchTest ("-=-=-=-=-=-=-=-=-=-=-=-=y", "-=-=-=-=-=-=-=yy", true, "-=-=-=-=-=,,,y,-=-=-=-=-=-=-=y" );
    // Non-optimal halfmatch
    // Optimal diff would be -q+x=H-i+e=lloHe+Hu=llo-Hew+y not -qHillo+x=HelloHe-w+Hulloy
    HalfMatchTest ("qHilloHelloHew", "xHelloHeHulloy", true, "qHillo,w,x,Hulloy,HelloHe");
    // Optimal no halfmatch
    HalfMatchTest ("qHilloHelloHew", "xHelloHeHulloy", false, "", -1 /*infinty*/);
}
#endif // ALLOW_INTERNAL_TESTS

// CDiffList::CleanupAndMerge()
void s_CleanupAndMerge_MergeEquities(void)
{
    {{
        string s1 = "Good dog";
        string s2 = "Bad dog";
        CDiffList diffs;
        diffs.Append(DIFF_DELETE, CTempString(s1, 0, 2));
        diffs.Append(DIFF_DELETE, CTempString(s1, 2, 2));
        diffs.Append(DIFF_DELETE, CTempString(s1, 4, 1));
        diffs.Append(DIFF_INSERT, CTempString(s2, 0, 1));
        diffs.Append(DIFF_INSERT, CTempString(s2, 1, 2));
        diffs.Append(DIFF_INSERT, CTempString(s2, 3, 1));
        diffs.Append(DIFF_EQUAL,  CTempString(s1, 5, 1));
        diffs.Append(DIFF_EQUAL,  CTempString(s1, 6, 2));
        diffs.CleanupAndMerge();
        assert(diffs.GetList().size() == 3);
        CDiffList::TList::const_iterator it = diffs.GetList().begin();
        assert(*it == CDiffOperation(DIFF_DELETE, "Goo"));   it++;
        assert(*it == CDiffOperation(DIFF_INSERT, "Ba"));    it++;
        assert(*it == CDiffOperation(DIFF_EQUAL,  "d dog")); it++; 
        assert(diffs.GetEditDistance() == 3);
        assert(diffs.GetLongestCommonSubstring() == "d dog");
        assert(CDictionaryUtil::GetEditDistance(s1, s2) == 3);
    }}
    {{
        string s1 = "This";
        string s2 = "That";
        CDiffList diffs;
        diffs.Append(DIFF_EQUAL,  CTempString(s1, 0, 1));
        diffs.Append(DIFF_EQUAL,  CTempString(s1, 1, 1));
        diffs.Append(DIFF_DELETE, CTempString(s1, 2, 1));
        diffs.Append(DIFF_DELETE, CTempString(s1, 3, 1));
        diffs.Append(DIFF_INSERT, CTempString(s2, 2, 1));
        diffs.Append(DIFF_INSERT, CTempString(s2, 3, 1));
        diffs.CleanupAndMerge();
        assert(diffs.GetList().size() == 3);
        CDiffList::TList::const_iterator it = diffs.GetList().begin();
        assert(*it == CDiffOperation(DIFF_EQUAL,  "Th")); it++;
        assert(*it == CDiffOperation(DIFF_DELETE, "is")); it++;
        assert(*it == CDiffOperation(DIFF_INSERT, "at")); it++;
        assert(diffs.GetEditDistance() == 2);
        assert(diffs.GetLongestCommonSubstring() == "Th");
        assert(CDictionaryUtil::GetEditDistance(s1, s2) == 2);
    }}
    {{
        string s1 = "This dog";
        string s2 = "That dog";
        CDiffList diffs;
        diffs.Append(DIFF_EQUAL,  CTempString(s1, 0, 1));
        diffs.Append(DIFF_EQUAL,  CTempString(s1, 1, 1));
        diffs.Append(DIFF_DELETE, CTempString(s1, 2, 1));
        diffs.Append(DIFF_DELETE, CTempString(s1, 3, 1));
        diffs.Append(DIFF_INSERT, CTempString(s2, 2, 1));
        diffs.Append(DIFF_INSERT, CTempString(s2, 3, 1));
        diffs.Append(DIFF_EQUAL,  CTempString(s1, 4, 2));
        diffs.Append(DIFF_EQUAL,  CTempString(s1, 6, 2));
        diffs.CleanupAndMerge();
        assert(diffs.GetList().size() == 4);
        CDiffList::TList::const_iterator it = diffs.GetList().begin();
        assert(*it == CDiffOperation(DIFF_EQUAL,  "Th"));   it++;
        assert(*it == CDiffOperation(DIFF_DELETE, "is"));   it++;
        assert(*it == CDiffOperation(DIFF_INSERT, "at"));   it++;
        assert(*it == CDiffOperation(DIFF_EQUAL,  " dog")); it++; 
        assert(diffs.GetEditDistance() == 2);
        assert(diffs.GetLongestCommonSubstring() == " dog");
        assert(CDictionaryUtil::GetEditDistance(s1, s2) == 2);
    }}
}

// CDiffList::CleanupAndMerge()
void s_CleanupAndMerge_Cleanup(void)
{
   {{
        // A<BA>C --> <AB>AC
        string s1 = "AC";
        string s2 = "ABAC";
        CDiffList diffs;
        diffs.Append(DIFF_EQUAL,  CTempString(s1, 0, 1));
        diffs.Append(DIFF_INSERT, CTempString(s2, 1, 2));
        diffs.Append(DIFF_EQUAL,  CTempString(s1, 1, 1));
        diffs.CleanupAndMerge();
        assert(diffs.GetList().size() == 2);
        assert(diffs.GetEditDistance() == 2);
        assert(diffs.GetLongestCommonSubstring() == "AC");
        CDiffList::TList::const_iterator it = diffs.GetList().begin();
        assert(*it == CDiffOperation(DIFF_INSERT, "AB")); it++;
        assert(*it == CDiffOperation(DIFF_EQUAL,  "AC")); it++;
        assert(CDictionaryUtil::GetEditDistance(s1, s2) == 2);
    }}
    {{
        // A<BA>C --> <AB>AC
        string s1 = "ABAC";
        string s2 = "AC";
        CDiffList diffs;
        diffs.Append(DIFF_EQUAL,  CTempString(s1, 0, 1));
        diffs.Append(DIFF_DELETE, CTempString(s1, 1, 2));
        diffs.Append(DIFF_EQUAL,  CTempString(s1, 3, 1));
        diffs.CleanupAndMerge();
        assert(diffs.GetList().size() == 2);
        assert(diffs.GetEditDistance() == 2);
        assert(diffs.GetLongestCommonSubstring() == "AC");
        CDiffList::TList::const_iterator it = diffs.GetList().begin();
        assert(*it == CDiffOperation(DIFF_DELETE, "AB")); it++;
        assert(*it == CDiffOperation(DIFF_EQUAL,  "AC")); it++;
        assert(CDictionaryUtil::GetEditDistance(s1, s2) == 2);
    }}
    {{
        // A<CB>C --> AC<BC>
        string s1 = "AC";
        string s2 = "ACBC";
        CDiffList diffs;
        diffs.Append(DIFF_EQUAL,  CTempString(s1, 0, 1));
        diffs.Append(DIFF_INSERT, CTempString(s2, 1, 2));
        diffs.Append(DIFF_EQUAL,  CTempString(s1, 1, 1));
        diffs.CleanupAndMerge();
        assert(diffs.GetList().size() == 2);
        assert(diffs.GetEditDistance() == 2);
        assert(diffs.GetLongestCommonSubstring() == "AC");
        CDiffList::TList::const_iterator it = diffs.GetList().begin();
        assert(*it == CDiffOperation(DIFF_EQUAL,  "AC")); it++;
        assert(*it == CDiffOperation(DIFF_INSERT, "BC")); it++;
        assert(CDictionaryUtil::GetEditDistance(s1, s2) == 2);
    }}
    {{
        // A<CB>C --> AC<BC>
        string s1 = "ACBC";
        string s2 = "AC";
        CDiffList diffs;
        diffs.Append(DIFF_EQUAL,  CTempString(s1, 0, 1));
        diffs.Append(DIFF_DELETE, CTempString(s1, 1, 2));
        diffs.Append(DIFF_EQUAL,  CTempString(s1, 3, 1));
        diffs.CleanupAndMerge();
        assert(diffs.GetList().size() == 2);
        assert(diffs.GetEditDistance() == 2);
        assert(diffs.GetLongestCommonSubstring() == "AC");
        CDiffList::TList::const_iterator it = diffs.GetList().begin();
        assert(*it == CDiffOperation(DIFF_EQUAL,  "AC")); it++;
        assert(*it == CDiffOperation(DIFF_DELETE, "BC")); it++;
        assert(CDictionaryUtil::GetEditDistance(s1, s2) == 2);
    }}
}

// CDiff
void s_Diff(void)
{
    {{
        string s1 = "Good dog Amigo";
        string s2 = "Bad dog Buzz";
        CDiff d;
        CDiffList& diffs = d.Diff(s1, s2, CDiff::fCalculateOffsets);
        assert(diffs.GetList().size() == 5);
        CDiffList::TList::const_iterator it = diffs.GetList().begin();
        assert(*it == CDiffOperation(DIFF_DELETE, "Goo"   ));
        assert(it->GetOffset().first  == 0);
        assert(it->GetOffset().second == NPOS);
        it++;
        assert(*it == CDiffOperation(DIFF_INSERT, "Ba"    ));
        assert(it->GetOffset().first  == NPOS);
        assert(it->GetOffset().second == 0);
        it++;
        assert(*it == CDiffOperation(DIFF_EQUAL,  "d dog "));
        assert(it->GetOffset().first  == 3);
        assert(it->GetOffset().second == 2);
        it++;
        assert(*it == CDiffOperation(DIFF_DELETE, "Amigo" ));
        assert(it->GetOffset().first  == 9);
        assert(it->GetOffset().second == NPOS);
        it++;
        assert(*it == CDiffOperation(DIFF_INSERT, "Buzz"  ));
        assert(it->GetOffset().first  == NPOS);
        assert(it->GetOffset().second == 8);
        it++;
        assert(diffs.GetEditDistance() == 8);
        assert(diffs.GetLongestCommonSubstring() == "d dog ");
        assert(CDictionaryUtil::GetEditDistance(s1, s2) == 8);
    }}
    {{
        string s1 = "mouse";
        string s2 = "sofas";
        CDiff d;
        CDiffList& diffs = d.Diff(s1, s2, CDiff::fNoCleanup);
        CDiffList::TList::const_iterator it = diffs.GetList().begin();
        assert(diffs.GetList().size() == 7);
        assert(*it == CDiffOperation(DIFF_DELETE, "m" )); it++;
        assert(*it == CDiffOperation(DIFF_INSERT, "s" )); it++;
        assert(*it == CDiffOperation(DIFF_EQUAL,  "o" )); it++;
        assert(*it == CDiffOperation(DIFF_DELETE, "u" )); it++;
        assert(*it == CDiffOperation(DIFF_INSERT, "fa")); it++;
        assert(*it == CDiffOperation(DIFF_EQUAL,  "s" )); it++;
        assert(*it == CDiffOperation(DIFF_DELETE, "e" )); it++;
        assert(diffs.GetEditDistance() == 4);
        assert(diffs.GetLongestCommonSubstring() == "o");
        assert(CDictionaryUtil::GetEditDistance(s1, s2) == 4);
    }}
}


// CDiffText
void s_DiffText(void)
{
    string s1 = "123\n123\n123\n";
    string s2 = "abcd\n123\r\nef\n";
    {{
        CDiffText d;
        CDiffList& diffs = d.Diff(s1, s2);
        CDiffList::TList::const_iterator it = diffs.GetList().begin();
        assert(diffs.GetList().size() == 7);
        assert(*it == CDiffOperation(DIFF_DELETE, "123\n"  )); it++;
        assert(*it == CDiffOperation(DIFF_DELETE, "123\n"  )); it++;
        assert(*it == CDiffOperation(DIFF_DELETE, "123\n"  )); it++;
        assert(*it == CDiffOperation(DIFF_INSERT, "abcd\n" )); it++;
        assert(*it == CDiffOperation(DIFF_INSERT, "123\r\n")); it++;
        assert(*it == CDiffOperation(DIFF_INSERT, "ef\n"   )); it++;
        assert(*it == CDiffOperation(DIFF_EQUAL,  ""       )); it++;
    }}
    {{
        CDiffText d;
        CDiffList& diffs = d.Diff(s1, s2, CDiffText::fRemoveEOL);
        CDiffList::TList::const_iterator it = diffs.GetList().begin();
        assert(diffs.GetList().size() == 7);
        assert(*it == CDiffOperation(DIFF_DELETE, "123" )); it++;
        assert(*it == CDiffOperation(DIFF_DELETE, "123" )); it++;
        assert(*it == CDiffOperation(DIFF_DELETE, "123" )); it++;
        assert(*it == CDiffOperation(DIFF_INSERT, "abcd")); it++;
        assert(*it == CDiffOperation(DIFF_INSERT, "123" )); it++;
        assert(*it == CDiffOperation(DIFF_INSERT, "ef"  )); it++;
        assert(*it == CDiffOperation(DIFF_EQUAL,  ""    )); it++;
    }}
    {{
        CDiffText d;
        CDiffList& diffs = d.Diff(s1, s2, CDiffText::fIgnoreEOL);
        CDiffList::TList::const_iterator it = diffs.GetList().begin();
        assert(diffs.GetList().size() == 6);
        assert(*it == CDiffOperation(DIFF_INSERT, "abcd\n")); it++;
        assert(*it == CDiffOperation(DIFF_EQUAL,  "123\n" )); it++;
        assert(*it == CDiffOperation(DIFF_DELETE, "123\n" )); it++;
        assert(*it == CDiffOperation(DIFF_DELETE, "123\n" )); it++;
        assert(*it == CDiffOperation(DIFF_INSERT, "ef\n"  )); it++;
        assert(*it == CDiffOperation(DIFF_EQUAL,  ""      )); it++;
    }}
    {{
        CDiffText d;
        CDiffList& diffs = d.Diff(s1, s2, CDiffText::fIgnoreEOL|CDiffText::fRemoveEOL);
        CDiffList::TList::const_iterator it = diffs.GetList().begin();
        assert(diffs.GetList().size() == 6);
        assert(*it == CDiffOperation(DIFF_INSERT, "abcd")); it++;
        assert(*it == CDiffOperation(DIFF_EQUAL,  "123" )); it++;
        assert(*it == CDiffOperation(DIFF_DELETE, "123" )); it++;
        assert(*it == CDiffOperation(DIFF_DELETE, "123" )); it++;
        assert(*it == CDiffOperation(DIFF_INSERT, "ef"  )); it++;
        assert(*it == CDiffOperation(DIFF_EQUAL,  ""    )); it++;
    }}
    {{
        string s1 = "123\nAAA\nBBB\n123\n";
        string s2 = "CCC\n123\nDDD\n";

        CDiffText d;
        CDiffList& diffs = d.Diff(s1, s2, CDiffText::fRemoveEOL);
        CDiffList::TList::const_iterator it = diffs.GetList().begin();
        assert(diffs.GetList().size() == 7);

        assert(*it == CDiffOperation(DIFF_INSERT, "CCC")); 
        assert(it->GetLine().first  == NPOS);
        assert(it->GetLine().second == 1);
        it++;
        assert(*it == CDiffOperation(DIFF_EQUAL,  "123"));
        assert(it->GetLine().first  == 1);
        assert(it->GetLine().second == 2);
        it++;
        assert(*it == CDiffOperation(DIFF_DELETE, "AAA"));
        assert(it->GetLine().first  == 2);
        assert(it->GetLine().second == NPOS);
        it++;
        assert(*it == CDiffOperation(DIFF_DELETE, "BBB"));
        assert(it->GetLine().first  == 3);
        assert(it->GetLine().second == NPOS);
        it++;
        assert(*it == CDiffOperation(DIFF_DELETE, "123"));
        assert(it->GetLine().first  == 4);
        assert(it->GetLine().second == NPOS);
        it++;
        assert(*it == CDiffOperation(DIFF_INSERT, "DDD")); 
        assert(it->GetLine().first  == NPOS);
        assert(it->GetLine().second == 3);
        it++;
        assert(*it == CDiffOperation(DIFF_EQUAL,  ""   ));
        assert(it->GetLine().first  == 5);
        assert(it->GetLine().second == 4);
        it++;
    }}
}


// Run a single test for unified format
bool s_DiffText_Unified_File(const string& test_name, unsigned num_common_lines)
{
    const string kTestDirName = "testdata";
    const size_t kBufSize = 16*1024;
    string num_str = NStr::NumericToString(num_common_lines);

    string fsrc_name  = CFile::ConcatPath(kTestDirName, test_name + "_src.txt");
    string fdst_name  = CFile::ConcatPath(kTestDirName, test_name + "_dst.txt");
    string fdiff_name = CFile::ConcatPath(kTestDirName, test_name + "_diff_" + num_str + ".txt");
    CNcbiIfstream fsrc(fsrc_name.c_str(),   IOS_BASE::in | IOS_BASE::binary);
    CNcbiIfstream fdst(fdst_name.c_str(),   IOS_BASE::in | IOS_BASE::binary);
    CNcbiIfstream fdiff(fdiff_name.c_str(), IOS_BASE::in | IOS_BASE::binary);
    assert( fsrc && fdst && fdiff );

    AutoArray<char> src_buf_arr(kBufSize);
    AutoArray<char> dst_buf_arr(kBufSize);
    char* src_buf = src_buf_arr.get();
    char* dst_buf = dst_buf_arr.get();

    fsrc.read(src_buf, kBufSize);
    string src(src_buf, fsrc.gcount());
    fdst.read(dst_buf, kBufSize);
    string dst(dst_buf, fdst.gcount());
    assert(!src.empty() && !dst.empty());

    CDiffText diff;
    CNcbiOstrstream ostr;
    diff.PrintUnifiedDiff(ostr, src, dst, num_common_lines);
    string s = CNcbiOstrstreamToString(ostr);
    assert(!s.empty());
    return NcbiStreamCompareText(fdiff, s, eCompareText_IgnoreEol);
}


// CDiffText::PrintUnifiedDiff
void s_DiffText_Unified(void)
{
    assert ( s_DiffText_Unified_File("test_1", 0) );
    assert ( s_DiffText_Unified_File("test_1", 1) );
    assert ( s_DiffText_Unified_File("test_1", 3) );
    assert ( s_DiffText_Unified_File("test_1", 9) );
    return;
}


int CTestApplication::Run(void)
{
#if ALLOW_INTERNAL_TESTS
    s_Internal_DiffHalfMatch();
#endif
    s_CleanupAndMerge_MergeEquities();
    s_CleanupAndMerge_Cleanup();
    s_Diff();
    s_DiffText();
    s_DiffText_Unified();
    return 0;
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN

int main(int argc, const char* argv[])
{
    // Execute main application function
    return CTestApplication().AppMain(argc, argv);
}
