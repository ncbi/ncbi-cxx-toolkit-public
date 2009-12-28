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
 * Authors:  Denis Vakatov, Vladimir Ivanov
 *
 * File Description:
 *   TEST for CTempString class.
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/test_boost.hpp>
#include <common/test_assert.h>  /* This header must go last */


USING_NCBI_SCOPE;



BOOST_AUTO_TEST_CASE(TestTempString)
{
    string str("hello, world");
    CTempString tmp(str);
    LOG_POST(Info << "str = " << str);
    LOG_POST(Info << "tmp = " << tmp);

    string::size_type str_pos;
    string::size_type tmp_pos;


    ///
    /// length()
    ///

    LOG_POST(Info << "str.length() = " << str.length());
    LOG_POST(Info << "tmp.length() = " << str.length());
    BOOST_CHECK(str.length() == tmp.length());


    ///
    /// find_first_of(), find_first_not_of()
    ///

    /// test of simple find_first_of()
    str_pos = str.find_first_of(",");
    tmp_pos = tmp.find_first_of(",");
    LOG_POST(Info << "str.find_first_of(\",\") = " << str_pos);
    LOG_POST(Info << "tmp.find_first_of(\",\") = " << tmp_pos);
    BOOST_CHECK(str_pos == tmp_pos);

    /// test of simple find_first_of(), starting from the previous position
    str_pos = str.find_first_of(" ", str_pos);
    tmp_pos = tmp.find_first_of(" ", tmp_pos);
    LOG_POST(Info << "str.find_first_of(\" \") = " << str_pos);
    LOG_POST(Info << "tmp.find_first_of(\" \") = " << tmp_pos);
    BOOST_CHECK(str_pos == tmp_pos);

    /// test of simple find_first_of(), starting from the previous position,
    /// looking for a non-existent character
    str_pos = str.find_first_of(":", str_pos);
    tmp_pos = tmp.find_first_of(":", tmp_pos);
    LOG_POST(Info << "str.find_first_of(\":\") = " << str_pos);
    LOG_POST(Info << "tmp.find_first_of(\":\") = " << tmp_pos);
    BOOST_CHECK(str_pos == tmp_pos);

    /// test of simple find_first_not_of()
    str_pos = str.find_first_not_of("hel");
    tmp_pos = tmp.find_first_not_of("hel");
    LOG_POST(Info << "str.find_first_not_of(\"hel\") = " << str_pos);
    LOG_POST(Info << "tmp.find_first_not_of(\"hel\") = " << tmp_pos);
    BOOST_CHECK(str_pos == tmp_pos);

    /// test of simple find_first_not_of(), starting from a previous position
    str_pos = str.find_first_not_of("o, ", str_pos);
    tmp_pos = tmp.find_first_not_of("o, ", tmp_pos);
    LOG_POST(Info << "str.find_first_not_of(\"o, \") = " << str_pos);
    LOG_POST(Info << "tmp.find_first_not_of(\"o, \") = " << tmp_pos);
    BOOST_CHECK(str_pos == tmp_pos);

    /// test of simple find_last_of(), looking for first character
    str_pos = str.find_last_of("h");
    tmp_pos = tmp.find_last_of("h");
    LOG_POST(Info << "str.find_last_of(\"h\") = " << str_pos);
    LOG_POST(Info << "tmp.find_last_of(\"h\") = " << tmp_pos);
    BOOST_CHECK(str_pos == tmp_pos);


    ///
    /// find_last_of(), find_last_not_of()
    ///

    /// test of simple find_last_of()
    str_pos = str.find_last_of("d");
    tmp_pos = tmp.find_last_of("d");
    LOG_POST(Info << "str.find_last_of(\"d\") = " << str_pos);
    LOG_POST(Info << "tmp.find_last_of(\"d\") = " << tmp_pos);
    BOOST_CHECK(str_pos == tmp_pos);

    /// test of simple find_last_of()
    str_pos = str.find_last_of(",");
    tmp_pos = tmp.find_last_of(",");
    LOG_POST(Info << "str.find_last_of(\",\") = " << str_pos);
    LOG_POST(Info << "tmp.find_last_of(\",\") = " << tmp_pos);
    BOOST_CHECK(str_pos == tmp_pos);

    /// test of simple find_last_of(), starting from the previous position
    str_pos = str.find_last_of("e", str_pos);
    tmp_pos = tmp.find_last_of("e", tmp_pos);
    LOG_POST(Info << "str.find_last_of(\"e\") = " << str_pos);
    LOG_POST(Info << "tmp.find_last_of(\"e\") = " << tmp_pos);
    BOOST_CHECK(str_pos == tmp_pos);

    /// test of simple find_last_of(), starting from the previous position,
    /// looking for a non-existent character
    str_pos = str.find_last_of(":", str_pos);
    tmp_pos = tmp.find_last_of(":", tmp_pos);
    LOG_POST(Info << "str.find_last_of(\":\") = " << str_pos);
    LOG_POST(Info << "tmp.find_last_of(\":\") = " << tmp_pos);
    BOOST_CHECK(str_pos == tmp_pos);

    /// test of simple find_last_of(), looking for first character
    str_pos = str.find_last_of("h", str_pos);
    tmp_pos = tmp.find_last_of("h", tmp_pos);
    LOG_POST(Info << "str.find_last_of(\"h\") = " << str_pos);
    LOG_POST(Info << "tmp.find_last_of(\"h\") = " << tmp_pos);
    BOOST_CHECK(str_pos == tmp_pos);

    /// test of simple find_last_not_of()
    str_pos = str.find_last_not_of("rld");
    tmp_pos = tmp.find_last_not_of("rld");
    LOG_POST(Info << "str.find_last_not_of(\"rld\") = " << str_pos);
    LOG_POST(Info << "tmp.find_last_not_of(\"rld\") = " << tmp_pos);
    BOOST_CHECK(str_pos == tmp_pos);

    /// test of simple find_last_not_of(), starting from a previous position
    str_pos = str.find_last_not_of("wo", str_pos);
    tmp_pos = tmp.find_last_not_of("wo", tmp_pos);
    LOG_POST(Info << "str.find_last_not_of(\"wo\") = " << str_pos);
    LOG_POST(Info << "tmp.find_last_not_of(\"wo\") = " << tmp_pos);
    BOOST_CHECK(str_pos == tmp_pos);

    /// test of simple find_first_not_of(), looking for first character
    str_pos = str.find_first_not_of("ello, world");
    tmp_pos = tmp.find_first_not_of("ello, world");
    LOG_POST(Info << "str.find_first_not_of(\"ello, world\") = " << str_pos);
    LOG_POST(Info << "tmp.find_first_not_of(\"ello, world\") = " << tmp_pos);
    BOOST_CHECK(str_pos == tmp_pos);

    /// test of simple find_first_not_of(), looking for non-existent character
    str_pos = str.find_first_not_of(str);
    tmp_pos = tmp.find_first_not_of(str);
    LOG_POST(Info << "str.find_first_not_of(str) = " << str_pos);
    LOG_POST(Info << "tmp.find_first_not_of(str) = " << tmp_pos);
    BOOST_CHECK(str_pos == tmp_pos);


    ///
    /// find()
    ///

    /// find() for single character
    LOG_POST(Info << "str.find('o') = " << str.find('o'));
    LOG_POST(Info << "tmp.find('o') = " << tmp.find('o'));
    BOOST_CHECK(str.find('o') == tmp.find('o'));
    
    /// find() for single character
    LOG_POST(Info << "str.find('z') = " << str.find('z'));
    LOG_POST(Info << "tmp.find('z') = " << tmp.find('z'));
    BOOST_CHECK(str.find('z') == tmp.find('z'));

    /// find() for single character
    LOG_POST(Info << "str.find('o', 6) = " << str.find('o', 6));
    LOG_POST(Info << "tmp.find('o', 6) = " << tmp.find('o', 6));
    BOOST_CHECK(str.find('o', 6) == tmp.find('o', 6));

    /// find() for single character
    LOG_POST(Info << "str.find('d', 25) = " << str.find('d', 25));
    LOG_POST(Info << "tmp.find('d', 25) = " << tmp.find('d', 25));
    BOOST_CHECK(str.find('d', 25) == tmp.find('d', 25));

    /// find() for empty string
    LOG_POST(Info << "str.find(\"\") = " << str.find(""));
    LOG_POST(Info << "tmp.find(\"\") = " << tmp.find(""));
    BOOST_CHECK(str.find("") == tmp.find(""));

    /// find() of simple word at the beginning
    LOG_POST(Info << "str.find(\"hello\") = " << str.find("hello"));
    LOG_POST(Info << "tmp.find(\"hello\") = " << tmp.find("hello"));
    BOOST_CHECK(str.find("hello") == tmp.find("hello"));

    /// find() for word not at the beginning
    LOG_POST(Info << "str.find(\"o, wo\") = " << str.find("o, wo"));
    LOG_POST(Info << "tmp.find(\"o, wo\") = " << tmp.find("o, wo"));
    BOOST_CHECK(str.find("o, wo") == tmp.find("o, wo"));

    /// find() for word, beginning at a position
    LOG_POST(Info << "str.find(\"world\", 5) = " << str.find("world", 5));
    LOG_POST(Info << "tmp.find(\"world\", 5) = " << tmp.find("world", 5));
    BOOST_CHECK(str.find("world", 5) == tmp.find("world", 5));

    /// find() for word, beginning at a position off the end
    LOG_POST(Info << "str.find(\"world\", 25) = " << str.find("world", 25));
    LOG_POST(Info << "tmp.find(\"world\", 25) = " << tmp.find("world", 25));
    BOOST_CHECK(str.find("world", 25) == tmp.find("world", 25));

    /// find() for non-existent text
    LOG_POST(Info << "str.find(\"wirld\", 5) = " << str.find("wirld", 5));
    LOG_POST(Info << "tmp.find(\"wirld\", 5) = " << tmp.find("wirld", 5));
    BOOST_CHECK(str.find("wirld") == tmp.find("wirld"));

    
    ///
    /// rfind()
    ///

    /// rfind() for single character
    LOG_POST(Info << "str.rfind('o') = " << str.rfind('o'));
    LOG_POST(Info << "tmp.rfind('o') = " << tmp.rfind('o'));
    BOOST_CHECK(str.rfind('o') == tmp.rfind('o'));

    /// rfind() for single character
    LOG_POST(Info << "str.rfind('z') = " << str.rfind('z'));
    LOG_POST(Info << "tmp.rfind('z') = " << tmp.rfind('z'));
    BOOST_CHECK(str.rfind('z') == tmp.rfind('z'));

    /// rfind() for single character
    LOG_POST(Info << "str.rfind('o', 6) = " << str.rfind('o', 6));
    LOG_POST(Info << "tmp.rfind('o', 6) = " << tmp.rfind('o', 6));
    BOOST_CHECK(str.rfind('o', 6) == tmp.rfind('o', 6));

    /// rfind() for single character
    LOG_POST(Info << "str.rfind('d', 25) = " << str.rfind('d', 25));
    LOG_POST(Info << "tmp.rfind('d', 25) = " << tmp.rfind('d', 25));
    BOOST_CHECK(str.rfind('d', 25) == tmp.rfind('d', 25));

    /// rfind() for empty string
    LOG_POST(Info << "str.rfind(\"\") = " << str.rfind(""));
    LOG_POST(Info << "tmp.rfind(\"\") = " << tmp.rfind(""));
    BOOST_CHECK(str.rfind("") == tmp.rfind(""));

    /// rfind() of simple word at the beginning
    LOG_POST(Info << "str.find(\"hello\") = " << str.find("hello"));
    LOG_POST(Info << "tmp.find(\"hello\") = " << tmp.find("hello"));
    BOOST_CHECK(str.find("hello") == tmp.find("hello"));

    /// rfind() for word in the middle
    LOG_POST(Info << "str.find(\", w\") = " << str.find(", w"));
    LOG_POST(Info << "tmp.find(\", w\") = " << tmp.find(", w"));
    BOOST_CHECK(str.find(", w") == tmp.find(", w"));

    /// rfind() of simple word at the end
    LOG_POST(Info << "str.find(\"world\") = " << str.find("world"));
    LOG_POST(Info << "tmp.find(\"world\") = " << tmp.find("world"));
    BOOST_CHECK(str.find("world") == tmp.find("world"));

    /// rfind() for word, beginning at a position
    LOG_POST(Info << "str.find(\"world\", 5) = " << str.find("world", 5));
    LOG_POST(Info << "tmp.find(\"world\", 5) = " << tmp.find("world", 5));
    BOOST_CHECK(str.find("world", 5) == tmp.find("world", 5));

    /// rfind() for word, beginning at a position off the end
    LOG_POST(Info << "str.find(\"world\", 25) = " << str.find("world", 25));
    LOG_POST(Info << "tmp.find(\"world\", 25) = " << tmp.find("world", 25));
    BOOST_CHECK(str.find("world", 25) == tmp.find("world", 25));

    /// rfind() for non-existent text
    LOG_POST(Info << "str.find(\"wirld\") = " << str.find("wirld"));
    LOG_POST(Info << "tmp.find(\"wirld\") = " << tmp.find("wirld"));
    BOOST_CHECK(str.find("wirld") == tmp.find("wirld"));


    ///
    /// test comparators
    ///
    {{
        /// test operator==
        LOG_POST(Info << "  " << tmp << " == " << tmp << ": "
                      << (tmp == tmp ? "true" : "false"));
        BOOST_CHECK(tmp == tmp);
        LOG_POST(Info << "  " << tmp << " == " << str << ": "
                      << (tmp == str ? "true" : "false"));
        BOOST_CHECK(tmp == str);
        BOOST_CHECK(str == tmp);
    }}
    {{
        string str1("help");
        CTempString tmp1(str1);
        LOG_POST(Info << "CTempString() comparisons:");
        LOG_POST(Info << "  " << tmp << " < " << tmp << ": "
                      << (tmp <  tmp ? "true" : "false"));
        LOG_POST(Info << "  " << tmp << " < " << tmp1 << ": "
                      << (tmp <  tmp1 ? "true" : "false"));
        LOG_POST(Info << "  " << tmp1 << " < " << tmp << ": "
                      << (tmp1 < tmp ? "true" : "false"));

        LOG_POST(Info << "std::string() comparisons:");
        LOG_POST(Info << "  " << str << " < " << str << ": "
                      << (str <  str ? "true" : "false"));
        LOG_POST(Info << "  " << str << " < " << str1 << ": "
                      << (str <  str1 ? "true" : "false"));
        LOG_POST(Info << "  " << str1 << " < " << str << ": "
                      << (str1 < str ? "true" : "false"));

        BOOST_CHECK( (tmp  < tmp ) == (str  < str ) );
        BOOST_CHECK( (tmp  < tmp1) == (str  < str1) );
        BOOST_CHECK( (tmp1 < tmp ) == (str1 < str ) );
        BOOST_CHECK(  tmp  < str1);
    }}
    {{
        string str2("hello");
        CTempString tmp2(str2);

        LOG_POST(Info << "CTempString() comparisons:");
        LOG_POST(Info << "  " << tmp << " < " << tmp << ": "
                      << (tmp <  tmp ? "true" : "false"));
        LOG_POST(Info << "  " << tmp << " < " << tmp2 << ": "
                      << (tmp <  tmp2 ? "true" : "false"));
        LOG_POST(Info << "  " << tmp2 << " < " << tmp << ": "
                      << (tmp2 < tmp ? "true" : "false"));

        LOG_POST(Info << "std::string() comparisons:");
        LOG_POST(Info << "  " << str << " < " << str << ": "
                      << (str <  str ? "true" : "false"));
        LOG_POST(Info << "  " << str << " < " << str2 << ": "
                      << (str <  str2 ? "true" : "false"));
        LOG_POST(Info << "  " << str2 << " < " << str << ": "
                      << (str2 < str ? "true" : "false"));

        BOOST_CHECK( (tmp  < tmp ) == (str  < str ) );
        BOOST_CHECK( (tmp  < tmp2) == (str  < str2) );
        BOOST_CHECK( (tmp2 < tmp ) == (str2 < str ) );
    }}


    ///
    /// test substring
    ///
    {{
        string sub = str.substr(0, 5);
        CTempString temp_sub = tmp.substr(0, 5);
        LOG_POST(Info << "str.substr() = " << sub);
        LOG_POST(Info << "tmp.substr() = " << temp_sub);
        BOOST_CHECK(temp_sub == sub);

        sub = str.substr(5, 5);
        temp_sub = tmp.substr(5, 5);
        LOG_POST(Info << "str.substr() = " << sub);
        LOG_POST(Info << "tmp.substr() = " << temp_sub);
        BOOST_CHECK(temp_sub == sub);
    }}


    ///
    /// erase()
    ///
    {{
        string sub = str;
        sub.erase(5);
        CTempString temp_sub = tmp;
        temp_sub.erase(5);
        LOG_POST(Info << "str.erase(5) = " << sub);
        LOG_POST(Info << "tmp.erase(5) = " << temp_sub);
        BOOST_CHECK(temp_sub == sub);
    }}


    ///
    /// NStr::TruncateSpaces
    ///
    {{
        string sub = "  hello, world  ";
        CTempString temp_sub = "  hello, world  ";
        sub = NStr::TruncateSpaces(sub);
        temp_sub = NStr::TruncateSpaces(temp_sub);
        LOG_POST(Info << "NStr::TruncateSpaces(sub) = >>" << sub << "<<");
        LOG_POST(Info << "NStr::TruncateSpaces(temp_sub) = >>" << temp_sub << "<<");
        BOOST_CHECK(temp_sub == sub);
        BOOST_CHECK(temp_sub == tmp);
        BOOST_CHECK(sub == str);

        sub = "-";
        sub = NStr::TruncateSpaces(sub);
        LOG_POST(Info << "NStr::TruncateSpaces(\"-\")  = >>" << sub << "<<");
        BOOST_CHECK(sub == "-");

        sub = "- ";
        sub = NStr::TruncateSpaces(sub);
        LOG_POST(Info << "NStr::TruncateSpaces(\"- \") = >>" << sub << "<<");
        BOOST_CHECK(sub == "-");

        sub = " hello ";
        sub = NStr::TruncateSpaces(sub);
        LOG_POST(Info << "NStr::TruncateSpaces(\" hello \") = >>" << sub << "<<");
        BOOST_CHECK(sub == "hello");

        sub = " hello";
        sub = NStr::TruncateSpaces(sub);
        LOG_POST(Info << "NStr::TruncateSpaces(\" hello\")  = >>" << sub << "<<");
        BOOST_CHECK(sub == "hello");
    }}


    ///
    /// Copy() / substr()
    ///
    {{
        string s;
        string sub = str.substr(0, 5);
        tmp.Copy(s, 0, 5);
        LOG_POST(Info << "str.substr()    = " << sub);
        LOG_POST(Info << "tmp.Copy() = " << s);
        BOOST_CHECK(s == sub);

        sub = str.substr(5, 5);
        tmp.Copy(s, 5, 5);
        LOG_POST(Info << "str.substr()    = " << sub);
        LOG_POST(Info << "tmp.Copy() = " << s);
        BOOST_CHECK(s == sub);

        sub = str.substr(5, 25);
        tmp.Copy(s, 5, 25);
        LOG_POST(Info << "str.substr()    = " << sub);
        LOG_POST(Info << "tmp.Copy() = " << s);
        BOOST_CHECK(s == sub);
    }}
}


NCBITEST_AUTO_INIT()
{
    boost::unit_test::framework::master_test_suite().p_name->assign
        ("CTempString Unit Test");
    //SetDiagPostLevel(eDiag_Info);
}
