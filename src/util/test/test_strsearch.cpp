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
 * Authors: Anatoliy Kuznetsov, Aaron Ucko, Colleen Bollin
 *
 * File Description: Test application for string search
 *
 */


#include <ncbi_pch.hpp>
#include <corelib/test_boost.hpp>

#include <util/strsearch.hpp>

USING_NCBI_SCOPE;


BOOST_AUTO_TEST_CASE(Test_BoyerMooreMatcher)
{
    const char* str = "123 567 BB";
    size_t len = strlen(str);
    {{    
        CBoyerMooreMatcher matcher("BB");
        size_t pos = matcher.Search(str, 0, len);
        BOOST_CHECK_EQUAL(pos, 8U);
    }}
    {{    
        CBoyerMooreMatcher matcher("BB", NStr::eNocase, 
                                   CBoyerMooreMatcher::eWholeWordMatch);
        size_t pos = matcher.Search(str, 0, len);
        BOOST_CHECK_EQUAL(pos, 8U);
    }}
    {{
        CBoyerMooreMatcher matcher("123", NStr::eNocase, 
                                   CBoyerMooreMatcher::eWholeWordMatch);
        size_t pos = matcher.Search(str, 0, len);
        BOOST_CHECK_EQUAL(pos, 0U);
    }}
    {{
        CBoyerMooreMatcher matcher("1234", NStr::eNocase, 
                                   CBoyerMooreMatcher::eWholeWordMatch);
        size_t pos = matcher.Search(str, 0, len);
        BOOST_CHECK_EQUAL(static_cast<int>(pos), -1);
    }}
    {{
        CBoyerMooreMatcher matcher("bb", NStr::eCase, 
                                   CBoyerMooreMatcher::eWholeWordMatch);
        size_t pos = matcher.Search(str, 0, len);
        BOOST_CHECK_EQUAL(static_cast<int>(pos), -1);
    }}
    {{    
        CBoyerMooreMatcher matcher("67", NStr::eNocase, 
                                   CBoyerMooreMatcher::eWholeWordMatch);
        size_t pos = matcher.Search(str, 0, len);
        BOOST_CHECK_EQUAL(static_cast<int>(pos), -1);
    }}
    {{
        CBoyerMooreMatcher matcher("67", NStr::eNocase, 
                                   CBoyerMooreMatcher::eSubstrMatch);
        size_t pos = matcher.Search(str, 0, len);
        BOOST_CHECK_EQUAL(pos, 5U);
    }}
    {{
        CBoyerMooreMatcher matcher("67", NStr::eNocase, 
                                   CBoyerMooreMatcher::eSuffixMatch);
        size_t pos = matcher.Search(str, 0, len);
        BOOST_CHECK_EQUAL(pos, 5U);
    }}
    {{
        CBoyerMooreMatcher matcher("56", NStr::eNocase, 
                                   CBoyerMooreMatcher::ePrefixMatch);
        size_t pos = matcher.Search(str, 0, len);
        BOOST_CHECK_EQUAL(pos, 4U);
    }}
    {{
        CBoyerMooreMatcher matcher("123", NStr::eNocase, 
                                   CBoyerMooreMatcher::ePrefixMatch);
        size_t pos = matcher.Search(str, 0, len);
        BOOST_CHECK_EQUAL(pos, 0U);
    }}
    {{
        CBoyerMooreMatcher matcher("drosophila", NStr::eNocase, 
                                   CBoyerMooreMatcher::ePrefixMatch);
        matcher.InitCommonDelimiters();
        const char* str1 = 
            "eukaryotic initiation factor 4E-I [Drosophila melanogaster]";

        size_t len1 = strlen(str1);
        size_t pos  = matcher.Search(str1, 0, len1);
        BOOST_CHECK(pos != static_cast<size_t>(-1));
    }}
}


BOOST_AUTO_TEST_CASE(Test_BoyerMooreMatcher_8bit)
{
    const char* str = "123 567 \342\342";
    size_t len = strlen(str);
    {{    
        CBoyerMooreMatcher matcher("\342\342", NStr::eCase);
        size_t pos = matcher.Search(str, 0, len);
        BOOST_CHECK_EQUAL(pos, 8U);
    }}
    {{    
        CBoyerMooreMatcher matcher("\342\342", NStr::eNocase);
        size_t pos = matcher.Search(str, 0, len);
        BOOST_CHECK_EQUAL(pos, 8U);
    }}
    {{    
        CBoyerMooreMatcher matcher("\342\342", NStr::eCase, 
                                   CBoyerMooreMatcher::eWholeWordMatch);
        size_t pos = matcher.Search(str, 0, len);
        BOOST_CHECK_EQUAL(pos, 8U);
    }}
    {{    
        CBoyerMooreMatcher matcher("\342\342", NStr::eNocase, 
                                   CBoyerMooreMatcher::eWholeWordMatch);
        size_t pos = matcher.Search(str, 0, len);
        BOOST_CHECK_EQUAL(pos, 8U);
    }}
    {{
        CBoyerMooreMatcher matcher("123", NStr::eNocase, 
                                   CBoyerMooreMatcher::eWholeWordMatch);
        size_t pos = matcher.Search(str, 0, len);
        BOOST_CHECK_EQUAL(pos, 0U);
    }}
    {{
        CBoyerMooreMatcher matcher("1234", NStr::eNocase, 
                                   CBoyerMooreMatcher::eWholeWordMatch);
        size_t pos = matcher.Search(str, 0, len);
        BOOST_CHECK_EQUAL(static_cast<int>(pos), -1);
    }}
    {{
        CBoyerMooreMatcher matcher("bb", NStr::eCase, 
                                   CBoyerMooreMatcher::eWholeWordMatch);
        size_t pos = matcher.Search(str, 0, len);
        BOOST_CHECK_EQUAL(static_cast<int>(pos), -1);
    }}
    {{    
        CBoyerMooreMatcher matcher("67", NStr::eNocase, 
                                   CBoyerMooreMatcher::eWholeWordMatch);
        size_t pos = matcher.Search(str, 0, len);
        BOOST_CHECK_EQUAL(static_cast<int>(pos), -1);
    }}
    {{
        CBoyerMooreMatcher matcher("67", NStr::eNocase, 
                                   CBoyerMooreMatcher::eSubstrMatch);
        size_t pos = matcher.Search(str, 0, len);
        BOOST_CHECK_EQUAL(pos, 5U);
    }}
    {{
        CBoyerMooreMatcher matcher("67", NStr::eNocase, 
                                   CBoyerMooreMatcher::eSuffixMatch);
        size_t pos = matcher.Search(str, 0, len);
        BOOST_CHECK_EQUAL(pos, 5U);
    }}
    {{
        CBoyerMooreMatcher matcher("56", NStr::eNocase, 
                                   CBoyerMooreMatcher::ePrefixMatch);
        size_t pos = matcher.Search(str, 0, len);
        BOOST_CHECK_EQUAL(pos, 4U);
    }}
    {{
        CBoyerMooreMatcher matcher("123", NStr::eNocase, 
                                   CBoyerMooreMatcher::ePrefixMatch);
        size_t pos = matcher.Search(str, 0, len);
        BOOST_CHECK_EQUAL(pos, 0U);
    }}
}


BOOST_AUTO_TEST_CASE(Test_CTextFsa)
{
    static const char* const s_SourceQualPrefixes[] = {
        "acronym:",
        "anamorph:",
        "variety:"
    };

    CTextFsa tag_list;

    size_t size = sizeof(s_SourceQualPrefixes) / sizeof(const char*);

    for (size_t i = 0; i < size; ++i ) {
        tag_list.AddWord(s_SourceQualPrefixes[i]);
    }

    tag_list.Prime();

    string str = s_SourceQualPrefixes[0];

    int state = tag_list.GetInitialState();
    bool found = false;

    ITERATE(string, str_itr, str) {
        const char ch = *str_itr;
        state = tag_list.GetNextState(state, ch);
        if (tag_list.IsMatchFound(state)) {
            // match was found
            found = true;
            break;
        }
    }
    BOOST_CHECK(found);
}


NCBITEST_AUTO_INIT()
{
    boost::unit_test::framework::master_test_suite().p_name->assign
        ("String search test");
}
