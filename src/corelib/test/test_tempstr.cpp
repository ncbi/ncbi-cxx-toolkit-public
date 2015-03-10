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


bool s_CompareSign(int n1, int n2)
{
    if (n1 < 0) {
        return n2 < 0;
    }
    if (n1 > 0) {
        return n2 > 0;
    }
    return n2 == 0;
}


BOOST_AUTO_TEST_CASE(TestTempString)
{
    const string str("hello, world");
    const char*  str_c = str.c_str();
    CTempString tmp(str);

    ERR_POST(Info << "str = " << str);
    ERR_POST(Info << "tmp = " << tmp);

    string::size_type str_pos;
    string::size_type tmp_pos;


    /// ----------------------------------------------------------------------
    ///
    /// length()
    ///
    /// ----------------------------------------------------------------------

    ERR_POST(Info << "str.length() = " << str.length());
    ERR_POST(Info << "tmp.length() = " << str.length());
    BOOST_CHECK(str.length() == tmp.length());


    /// ----------------------------------------------------------------------
    ///
    /// find_first_of(), find_first_not_of()
    ///
    /// ----------------------------------------------------------------------

    /// test of simple find_first_of()
    str_pos = str.find_first_of(",");
    tmp_pos = tmp.find_first_of(",");
    ERR_POST(Info << "str.find_first_of(\",\") = " << str_pos);
    ERR_POST(Info << "tmp.find_first_of(\",\") = " << tmp_pos);
    BOOST_CHECK(str_pos == tmp_pos);

    /// test of simple find_first_of(), starting from the previous position
    str_pos = str.find_first_of(" ", str_pos);
    tmp_pos = tmp.find_first_of(" ", tmp_pos);
    ERR_POST(Info << "str.find_first_of(\" \") = " << str_pos);
    ERR_POST(Info << "tmp.find_first_of(\" \") = " << tmp_pos);
    BOOST_CHECK(str_pos == tmp_pos);

    /// test of simple find_first_of(), starting from the previous position,
    /// looking for a non-existent character
    str_pos = str.find_first_of(":", str_pos);
    tmp_pos = tmp.find_first_of(":", tmp_pos);
    ERR_POST(Info << "str.find_first_of(\":\") = " << str_pos);
    ERR_POST(Info << "tmp.find_first_of(\":\") = " << tmp_pos);
    BOOST_CHECK(str_pos == tmp_pos);

    /// test of simple find_first_not_of()
    str_pos = str.find_first_not_of("hel");
    tmp_pos = tmp.find_first_not_of("hel");
    ERR_POST(Info << "str.find_first_not_of(\"hel\") = " << str_pos);
    ERR_POST(Info << "tmp.find_first_not_of(\"hel\") = " << tmp_pos);
    BOOST_CHECK(str_pos == tmp_pos);

    /// test of simple find_first_not_of(), starting from a previous position
    str_pos = str.find_first_not_of("o, ", str_pos);
    tmp_pos = tmp.find_first_not_of("o, ", tmp_pos);
    ERR_POST(Info << "str.find_first_not_of(\"o, \") = " << str_pos);
    ERR_POST(Info << "tmp.find_first_not_of(\"o, \") = " << tmp_pos);
    BOOST_CHECK(str_pos == tmp_pos);

    /// test of simple find_last_of(), looking for first character
    str_pos = str.find_last_of("h");
    tmp_pos = tmp.find_last_of("h");
    ERR_POST(Info << "str.find_last_of(\"h\") = " << str_pos);
    ERR_POST(Info << "tmp.find_last_of(\"h\") = " << tmp_pos);
    BOOST_CHECK(str_pos == tmp_pos);


    /// ----------------------------------------------------------------------
    ///
    /// find_last_of(), find_last_not_of()
    ///
    /// ----------------------------------------------------------------------

    /// test of simple find_last_of()
    str_pos = str.find_last_of("d");
    tmp_pos = tmp.find_last_of("d");
    ERR_POST(Info << "str.find_last_of(\"d\") = " << str_pos);
    ERR_POST(Info << "tmp.find_last_of(\"d\") = " << tmp_pos);
    BOOST_CHECK(str_pos == tmp_pos);

    /// test of simple find_last_of()
    str_pos = str.find_last_of(",");
    tmp_pos = tmp.find_last_of(",");
    ERR_POST(Info << "str.find_last_of(\",\") = " << str_pos);
    ERR_POST(Info << "tmp.find_last_of(\",\") = " << tmp_pos);
    BOOST_CHECK(str_pos == tmp_pos);

    /// test of simple find_last_of(), starting from the previous position
    str_pos = str.find_last_of("e", str_pos);
    tmp_pos = tmp.find_last_of("e", tmp_pos);
    ERR_POST(Info << "str.find_last_of(\"e\") = " << str_pos);
    ERR_POST(Info << "tmp.find_last_of(\"e\") = " << tmp_pos);
    BOOST_CHECK(str_pos == tmp_pos);

    /// test of simple find_last_of(), starting from the previous position,
    /// looking for a non-existent character
    str_pos = str.find_last_of(":", str_pos);
    tmp_pos = tmp.find_last_of(":", tmp_pos);
    ERR_POST(Info << "str.find_last_of(\":\") = " << str_pos);
    ERR_POST(Info << "tmp.find_last_of(\":\") = " << tmp_pos);
    BOOST_CHECK(str_pos == tmp_pos);

    /// test of simple find_last_of(), looking for first character
    str_pos = str.find_last_of("h", str_pos);
    tmp_pos = tmp.find_last_of("h", tmp_pos);
    ERR_POST(Info << "str.find_last_of(\"h\") = " << str_pos);
    ERR_POST(Info << "tmp.find_last_of(\"h\") = " << tmp_pos);
    BOOST_CHECK(str_pos == tmp_pos);

    /// test of simple find_last_not_of()
    str_pos = str.find_last_not_of("rld");
    tmp_pos = tmp.find_last_not_of("rld");
    ERR_POST(Info << "str.find_last_not_of(\"rld\") = " << str_pos);
    ERR_POST(Info << "tmp.find_last_not_of(\"rld\") = " << tmp_pos);
    BOOST_CHECK(str_pos == tmp_pos);

    /// test of simple find_last_not_of(), starting from a previous position
    str_pos = str.find_last_not_of("wo", str_pos);
    tmp_pos = tmp.find_last_not_of("wo", tmp_pos);
    ERR_POST(Info << "str.find_last_not_of(\"wo\") = " << str_pos);
    ERR_POST(Info << "tmp.find_last_not_of(\"wo\") = " << tmp_pos);
    BOOST_CHECK(str_pos == tmp_pos);

    /// test of simple find_first_not_of(), looking for first character
    str_pos = str.find_first_not_of("ello, world");
    tmp_pos = tmp.find_first_not_of("ello, world");
    ERR_POST(Info << "str.find_first_not_of(\"ello, world\") = " << str_pos);
    ERR_POST(Info << "tmp.find_first_not_of(\"ello, world\") = " << tmp_pos);
    BOOST_CHECK(str_pos == tmp_pos);

    /// test of simple find_first_not_of(), looking for non-existent character
    str_pos = str.find_first_not_of(str);
    tmp_pos = tmp.find_first_not_of(str);
    ERR_POST(Info << "str.find_first_not_of(str) = " << str_pos);
    ERR_POST(Info << "tmp.find_first_not_of(str) = " << tmp_pos);
    BOOST_CHECK(str_pos == tmp_pos);


    /// ----------------------------------------------------------------------
    ///
    /// find()
    ///
    /// ----------------------------------------------------------------------

    /// find() for single character
    ERR_POST(Info << "str.find('o') = " << str.find('o'));
    ERR_POST(Info << "tmp.find('o') = " << tmp.find('o'));
    BOOST_CHECK(str.find('o') == tmp.find('o'));
    
    /// find() for single character
    ERR_POST(Info << "str.find('z') = " << str.find('z'));
    ERR_POST(Info << "tmp.find('z') = " << tmp.find('z'));
    BOOST_CHECK(str.find('z') == tmp.find('z'));

    /// find() for single character
    ERR_POST(Info << "str.find('o', 6) = " << str.find('o', 6));
    ERR_POST(Info << "tmp.find('o', 6) = " << tmp.find('o', 6));
    BOOST_CHECK(str.find('o', 6) == tmp.find('o', 6));

    /// find() for single character
    ERR_POST(Info << "str.find('d', 25) = " << str.find('d', 25));
    ERR_POST(Info << "tmp.find('d', 25) = " << tmp.find('d', 25));
    BOOST_CHECK(str.find('d', 25) == tmp.find('d', 25));

    /// find() for empty string
    ERR_POST(Info << "str.find(\"\") = " << str.find(""));
    ERR_POST(Info << "tmp.find(\"\") = " << tmp.find(""));
    BOOST_CHECK(str.find("") == tmp.find(""));

    /// find() of simple word at the beginning
    ERR_POST(Info << "str.find(\"hello\") = " << str.find("hello"));
    ERR_POST(Info << "tmp.find(\"hello\") = " << tmp.find("hello"));
    BOOST_CHECK(str.find("hello") == tmp.find("hello"));

    /// find() for word not at the beginning
    ERR_POST(Info << "str.find(\"o, wo\") = " << str.find("o, wo"));
    ERR_POST(Info << "tmp.find(\"o, wo\") = " << tmp.find("o, wo"));
    BOOST_CHECK(str.find("o, wo") == tmp.find("o, wo"));

    /// find() for word, beginning at a position
    ERR_POST(Info << "str.find(\"world\", 5) = " << str.find("world", 5));
    ERR_POST(Info << "tmp.find(\"world\", 5) = " << tmp.find("world", 5));
    BOOST_CHECK(str.find("world", 5) == tmp.find("world", 5));

    /// find() for word, beginning at a position off the end
    ERR_POST(Info << "str.find(\"world\", 25) = " << str.find("world", 25));
    ERR_POST(Info << "tmp.find(\"world\", 25) = " << tmp.find("world", 25));
    BOOST_CHECK(str.find("world", 25) == tmp.find("world", 25));

    /// find() for non-existent text
    ERR_POST(Info << "str.find(\"wirld\", 5) = " << str.find("wirld", 5));
    ERR_POST(Info << "tmp.find(\"wirld\", 5) = " << tmp.find("wirld", 5));
    BOOST_CHECK(str.find("wirld") == tmp.find("wirld"));

    
    /// ----------------------------------------------------------------------
    ///
    /// rfind()
    ///
    /// ----------------------------------------------------------------------

    /// rfind() for single character
    ERR_POST(Info << "str.rfind('o') = " << str.rfind('o'));
    ERR_POST(Info << "tmp.rfind('o') = " << tmp.rfind('o'));
    BOOST_CHECK(str.rfind('o') == tmp.rfind('o'));

    /// rfind() for single character
    ERR_POST(Info << "str.rfind('z') = " << str.rfind('z'));
    ERR_POST(Info << "tmp.rfind('z') = " << tmp.rfind('z'));
    BOOST_CHECK(str.rfind('z') == tmp.rfind('z'));

    /// rfind() for single character
    ERR_POST(Info << "str.rfind('o', 6) = " << str.rfind('o', 6));
    ERR_POST(Info << "tmp.rfind('o', 6) = " << tmp.rfind('o', 6));
    BOOST_CHECK(str.rfind('o', 6) == tmp.rfind('o', 6));

    /// rfind() for single character
    ERR_POST(Info << "str.rfind('d', 25) = " << str.rfind('d', 25));
    ERR_POST(Info << "tmp.rfind('d', 25) = " << tmp.rfind('d', 25));
    BOOST_CHECK(str.rfind('d', 25) == tmp.rfind('d', 25));

    /// rfind() for empty string
    ERR_POST(Info << "str.rfind(\"\") = " << str.rfind(""));
    ERR_POST(Info << "tmp.rfind(\"\") = " << tmp.rfind(""));
    BOOST_CHECK(str.rfind("") == tmp.rfind(""));

    /// rfind() of simple word at the beginning
    ERR_POST(Info << "str.find(\"hello\") = " << str.find("hello"));
    ERR_POST(Info << "tmp.find(\"hello\") = " << tmp.find("hello"));
    BOOST_CHECK(str.find("hello") == tmp.find("hello"));

    /// rfind() for word in the middle
    ERR_POST(Info << "str.find(\", w\") = " << str.find(", w"));
    ERR_POST(Info << "tmp.find(\", w\") = " << tmp.find(", w"));
    BOOST_CHECK(str.find(", w") == tmp.find(", w"));

    /// rfind() of simple word at the end
    ERR_POST(Info << "str.find(\"world\") = " << str.find("world"));
    ERR_POST(Info << "tmp.find(\"world\") = " << tmp.find("world"));
    BOOST_CHECK(str.find("world") == tmp.find("world"));

    /// rfind() for word, beginning at a position
    ERR_POST(Info << "str.find(\"world\", 5) = " << str.find("world", 5));
    ERR_POST(Info << "tmp.find(\"world\", 5) = " << tmp.find("world", 5));
    BOOST_CHECK(str.find("world", 5) == tmp.find("world", 5));

    /// rfind() for word, beginning at a position off the end
    ERR_POST(Info << "str.find(\"world\", 25) = " << str.find("world", 25));
    ERR_POST(Info << "tmp.find(\"world\", 25) = " << tmp.find("world", 25));
    BOOST_CHECK(str.find("world", 25) == tmp.find("world", 25));

    /// rfind() for non-existent text
    ERR_POST(Info << "str.find(\"wirld\") = " << str.find("wirld"));
    ERR_POST(Info << "tmp.find(\"wirld\") = " << tmp.find("wirld"));
    BOOST_CHECK(str.find("wirld") == tmp.find("wirld"));


    /// ----------------------------------------------------------------------
    ///
    /// test comparators
    ///
    /// ----------------------------------------------------------------------

    /// Comparing with NULL and empty strings.
    /// Constructing std::string(NULL) is not possible, but allowed for CTempString.
    {{
        const char*  c_null  = NULL;
        const char*  c_empty = "\0";
        CTempString  t_empty;
        const string s_empty;

        // Empty CTempString
        BOOST_CHECK( s_CompareSign(t_empty.compare(c_null),  s_empty.compare(s_empty)) );
        BOOST_CHECK( s_CompareSign(t_empty.compare(c_empty), s_empty.compare(s_empty)) );
        BOOST_CHECK( s_CompareSign(t_empty.compare(s_empty), s_empty.compare(s_empty)) );

        // Non-empty CTempString
        BOOST_CHECK( s_CompareSign(tmp.compare(c_null),  str.compare(s_empty)) );
        BOOST_CHECK( s_CompareSign(tmp.compare(c_empty), str.compare(s_empty)) );
        BOOST_CHECK( s_CompareSign(tmp.compare(s_empty), str.compare(s_empty)) );
    }}

    /// Comparing to self / equal
    {{
        BOOST_CHECK( s_CompareSign(tmp.compare(tmp), str.compare(str)) );

        BOOST_CHECK(tmp == tmp);
        BOOST_CHECK(tmp == str);
        BOOST_CHECK(tmp == str_c);
        BOOST_CHECK(str == tmp);
        BOOST_CHECK(str_c == tmp);

        BOOST_CHECK( (tmp != tmp ) == false );
        BOOST_CHECK( (tmp > tmp )  == false );
        BOOST_CHECK( (tmp < tmp )  == false );
    }}

    {{
        const string str1 = "help";
        const char*  str1_c = str1.c_str();
        CTempString tmp1(str1);

        BOOST_CHECK( s_CompareSign(tmp.compare(str1),   str.compare(str1)) );
        BOOST_CHECK( s_CompareSign(tmp.compare(str1_c), str.compare(str1)) );
        BOOST_CHECK( s_CompareSign(tmp.compare(tmp1),   str.compare(str1)) );

        BOOST_CHECK( (tmp  < tmp1) == (str  < str1) );
        BOOST_CHECK( (tmp1 < tmp ) == (str1 < str ) );

        BOOST_CHECK( tmp    != str1 );
        BOOST_CHECK( tmp    != str1_c);
        BOOST_CHECK( str1   != tmp );
        BOOST_CHECK( str1_c != tmp );

        BOOST_CHECK( tmp    < str1 );
        BOOST_CHECK( tmp    < str1_c );
        BOOST_CHECK( str1   > tmp );
        BOOST_CHECK( str1_c > tmp );
    }}
    {{
        const string str1 = "hello";
        const char*  str1_c = str1.c_str();
        CTempString tmp1(str1);

        BOOST_CHECK( s_CompareSign(tmp.compare(str1),   str.compare(str1)) );
        BOOST_CHECK( s_CompareSign(tmp.compare(str1_c), str.compare(str1)) );
        BOOST_CHECK( s_CompareSign(tmp.compare(tmp1),   str.compare(str1)) );

        BOOST_CHECK( (tmp  < tmp1) == (str  < str1) );
        BOOST_CHECK( (tmp1 < tmp ) == (str1 < str ) );

        BOOST_CHECK( tmp    != str1 );
        BOOST_CHECK( tmp    != str1_c);
        BOOST_CHECK( str1   != tmp );
        BOOST_CHECK( str1_c != tmp );

        BOOST_CHECK( tmp    > str1 );
        BOOST_CHECK( tmp    > str1_c );
        BOOST_CHECK( str1   < tmp );
        BOOST_CHECK( str1_c < tmp );
    }}



    /// ----------------------------------------------------------------------
    ///
    /// test substring
    ///
    /// ----------------------------------------------------------------------
    {{
        string sub = str.substr(0, 5);
        CTempString temp_sub = tmp.substr(0, 5);
        ERR_POST(Info << "str.substr() = " << sub);
        ERR_POST(Info << "tmp.substr() = " << temp_sub);
        BOOST_CHECK(temp_sub == sub);

        sub = str.substr(5, 5);
        temp_sub = tmp.substr(5, 5);
        ERR_POST(Info << "str.substr() = " << sub);
        ERR_POST(Info << "tmp.substr() = " << temp_sub);
        BOOST_CHECK(temp_sub == sub);
    }}


    /// ----------------------------------------------------------------------
    ///
    /// erase()
    ///
    /// ----------------------------------------------------------------------
    {{
        string sub = str;
        sub.erase(5);
        CTempString temp_sub = tmp;
        temp_sub.erase(5);
        ERR_POST(Info << "str.erase(5) = " << sub);
        ERR_POST(Info << "tmp.erase(5) = " << temp_sub);
        BOOST_CHECK(temp_sub == sub);
    }}


    /// ----------------------------------------------------------------------
    ///
    /// NStr::TruncateSpaces
    ///
    /// ----------------------------------------------------------------------
    {{
        string sub = "  hello, world  ";
        CTempString temp_sub = "  hello, world  ";
        sub = NStr::TruncateSpaces(sub);
        temp_sub = NStr::TruncateSpaces_Unsafe(temp_sub);
        ERR_POST(Info << "NStr::TruncateSpaces(sub) = >>" << sub << "<<");
        ERR_POST(Info << "NStr::TruncateSpaces(temp_sub) = >>" << temp_sub << "<<");
        BOOST_CHECK(temp_sub == sub);
        BOOST_CHECK(temp_sub == tmp);
        BOOST_CHECK(sub == str);

        sub = "-";
        sub = NStr::TruncateSpaces(sub);
        ERR_POST(Info << "NStr::TruncateSpaces(\"-\")  = >>" << sub << "<<");
        BOOST_CHECK(sub == "-");

        sub = "- ";
        sub = NStr::TruncateSpaces(sub);
        ERR_POST(Info << "NStr::TruncateSpaces(\"- \") = >>" << sub << "<<");
        BOOST_CHECK(sub == "-");

        sub = " hello ";
        sub = NStr::TruncateSpaces(sub);
        ERR_POST(Info << "NStr::TruncateSpaces(\" hello \") = >>" << sub << "<<");
        BOOST_CHECK(sub == "hello");

        sub = " hello";
        sub = NStr::TruncateSpaces(sub);
        ERR_POST(Info << "NStr::TruncateSpaces(\" hello\")  = >>" << sub << "<<");
        BOOST_CHECK(sub == "hello");
    }}


    /// ----------------------------------------------------------------------
    ///
    /// Copy() / substr()
    ///
    /// ----------------------------------------------------------------------
    {{
        string s;
        string sub = str.substr(0, 5);
        tmp.Copy(s, 0, 5);
        ERR_POST(Info << "str.substr()    = " << sub);
        ERR_POST(Info << "tmp.Copy() = " << s);
        BOOST_CHECK(s == sub);

        sub = str.substr(5, 5);
        tmp.Copy(s, 5, 5);
        ERR_POST(Info << "str.substr()    = " << sub);
        ERR_POST(Info << "tmp.Copy() = " << s);
        BOOST_CHECK(s == sub);

        sub = str.substr(5, 25);
        tmp.Copy(s, 5, 25);
        ERR_POST(Info << "str.substr()    = " << sub);
        ERR_POST(Info << "tmp.Copy() = " << s);
        BOOST_CHECK(s == sub);
    }}
}


NCBITEST_AUTO_INIT()
{
    boost::unit_test::framework::master_test_suite().p_name->assign
        ("CTempString Unit Test");
    //SetDiagPostLevel(eDiag_Info);
}
