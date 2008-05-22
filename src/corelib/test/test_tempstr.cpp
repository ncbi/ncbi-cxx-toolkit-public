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
 *   Sample for the command-line arguments' processing ("ncbiargs.[ch]pp"):
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbistr.hpp>

#include <boost/version.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/test/unit_test_log.hpp>
using boost::unit_test_framework::test_suite;

USING_NCBI_SCOPE;



void TestTempString()
{
    string str("hello, world");
    CTempString temp_str(str);
    LOG_POST(Info << "str      = " << str);
    LOG_POST(Info << "temp_str = " << temp_str);

    ///
    /// test length()
    ///
    LOG_POST(Info << "str.length()      = " << str.length());
    LOG_POST(Info << "temp_str.length() = " << str.length());
    BOOST_CHECK(str.length() == temp_str.length());

    ///
    /// test find()
    ///

    /// find() for single character
    LOG_POST(Info << "str.find('o')      = " << str.find('o'));
    LOG_POST(Info << "temp_str.find('o') = " << temp_str.find('o'));
    BOOST_CHECK(str.find('o') == temp_str.find('o'));
    BOOST_CHECK(str.find('o') == temp_str.find('o'));

    /// find() for single character
    LOG_POST(Info << "str.find('z')      = " << str.find('z'));
    LOG_POST(Info << "temp_str.find('z') = " << temp_str.find('z'));
    BOOST_CHECK(str.find('z') == temp_str.find('z'));

    /// find() for single character
    LOG_POST(Info << "str.find('o', 6)      = " << str.find('o', 6));
    LOG_POST(Info << "temp_str.find('o', 6) = " << temp_str.find('o', 6));
    BOOST_CHECK(str.find('o', 6) == temp_str.find('o', 6));

    /// find() for single character
    LOG_POST(Info << "str.find('d', 25)      = " << str.find('d', 25));
    LOG_POST(Info << "temp_str.find('d', 25) = " << temp_str.find('d', 25));
    BOOST_CHECK(str.find('d', 25) == temp_str.find('d', 25));

    /// find() for empty string
    LOG_POST(Info << "str.find(\"\")      = " << str.find(""));
    LOG_POST(Info << "temp_str.find(\"\") = " << temp_str.find(""));
    BOOST_CHECK(str.find("") == temp_str.find(""));

    /// find() of simple word at the beginning
    LOG_POST(Info << "str.find(\"hello\")      = " << str.find("hello"));
    LOG_POST(Info << "temp_str.find(\"hello\") = " << temp_str.find("hello"));
    BOOST_CHECK(str.find("hello") == temp_str.find("hello"));

    /// find() for word not at the beginning
    LOG_POST(Info << "str.find(\"o, wo\")      = " << str.find("o, wo"));
    LOG_POST(Info << "temp_str.find(\"o, wo\") = " << temp_str.find("o, wo"));
    BOOST_CHECK(str.find("o, wo") == temp_str.find("o, wo"));

    /// find() for word, beginning at a position
    LOG_POST(Info << "str.find(\"world\", 5)      = " << str.find("world", 5));
    LOG_POST(Info << "temp_str.find(\"world\", 5) = " << temp_str.find("world", 5));
    BOOST_CHECK(str.find("world", 5) == temp_str.find("world", 5));

    /// find() for word, beginning at a position off the end
    LOG_POST(Info << "str.find(\"world\", 25)      = " << str.find("world", 25));
    LOG_POST(Info << "temp_str.find(\"world\", 25) = " << temp_str.find("world", 25));
    BOOST_CHECK(str.find("world", 25) == temp_str.find("world", 25));

    /// find() for non-existent text
    LOG_POST(Info << "str.find(\"wirld\", 5)      = " << str.find("wirld", 5));
    LOG_POST(Info << "temp_str.find(\"wirld\", 5) = " << temp_str.find("wirld", 5));
    BOOST_CHECK(str.find("wirld") == temp_str.find("wirld"));

    ///
    /// test find_first_of(), find_first_not_of()
    ///

    /// test of simple find_first_of()
    string::size_type str_pos1      = str.find_first_of(",");
    string::size_type temp_str_pos1 = temp_str.find_first_of(",");
    LOG_POST(Info << "str.find_first_of(\",\")      = " << str_pos1);
    LOG_POST(Info << "temp_str.find_first_of(\",\") = " << temp_str_pos1);
    BOOST_CHECK(str_pos1 == temp_str_pos1);

    /// test of simple find_first_of(), starting from the previous position
    str_pos1      = str.find_first_of(" ", str_pos1);
    temp_str_pos1 = temp_str.find_first_of(" ", temp_str_pos1);
    LOG_POST(Info << "str.find_first_of(\" \")      = " << str_pos1);
    LOG_POST(Info << "temp_str.find_first_of(\" \") = " << temp_str_pos1);
    BOOST_CHECK(str_pos1 == temp_str_pos1);

    /// test of simple find_first_of(), starting from the previous position,
    /// looking for a non-existent character
    str_pos1      = str.find_first_of(":", str_pos1);
    temp_str_pos1 = temp_str.find_first_of(":", temp_str_pos1);
    LOG_POST(Info << "str.find_first_of(\":\")      = " << str_pos1);
    LOG_POST(Info << "temp_str.find_first_of(\":\") = " << temp_str_pos1);
    BOOST_CHECK(str_pos1 == temp_str_pos1);

    /// test of simple find_first_not_of()
    str_pos1      = str.find_first_not_of("hel");
    temp_str_pos1 = temp_str.find_first_not_of("hel");
    LOG_POST(Info << "str.find_first_not_of(\"hel\")      = " << str_pos1);
    LOG_POST(Info << "temp_str.find_first_not_of(\"hel\") = " << temp_str_pos1);
    BOOST_CHECK(str_pos1 == temp_str_pos1);

    /// test of simple find_first_not_of(), starting from a previous position
    str_pos1      = str.find_first_not_of("o, ", str_pos1);
    temp_str_pos1 = temp_str.find_first_not_of("o, ", temp_str_pos1);
    LOG_POST(Info << "str.find_first_not_of(\"o, \")      = " << str_pos1);
    LOG_POST(Info << "temp_str.find_first_not_of(\"o, \") = " << temp_str_pos1);
    BOOST_CHECK(str_pos1 == temp_str_pos1);

    ///
    /// test comparators
    ///


    /// test operator==
    LOG_POST(Info << "  " << temp_str << " == " << temp_str << ": "
             << (temp_str == temp_str ? "true" : "false"));
    BOOST_CHECK(temp_str == temp_str);
    LOG_POST(Info << "  " << temp_str << " == " << str << ": "
             << (temp_str == str ? "true" : "false"));
    BOOST_CHECK(temp_str == str);
    BOOST_CHECK(str == temp_str);

    string test1("help");
    CTempString temp_test1(test1);

    LOG_POST(Info << "CTempString() comparisons:");
    LOG_POST(Info << "  " << temp_str << " < " << temp_str << ": "
             << (temp_str < temp_str ? "true" : "false"));
    LOG_POST(Info << "  " << temp_str << " < " << temp_test1 << ": "
             << (temp_str < temp_test1 ? "true" : "false"));
    LOG_POST(Info << "  " << temp_test1 << " < " << temp_str << ": "
             << (temp_test1 < temp_str ? "true" : "false"));

    LOG_POST(Info << "std::string() comparisons:");
    LOG_POST(Info << "  " << str << " < " << str << ": "
             << (str < str ? "true" : "false"));
    LOG_POST(Info << "  " << str << " < " << test1 << ": "
             << (str < test1 ? "true" : "false"));
    LOG_POST(Info << "  " << test1 << " < " << str << ": "
             << (test1 < str ? "true" : "false"));

    BOOST_CHECK( (temp_str < temp_str) == (str < str) );
    BOOST_CHECK( (temp_str < temp_test1) == (str < test1) );
    BOOST_CHECK( (temp_test1 < temp_str) == (test1 < str) );
    BOOST_CHECK(temp_str < test1);

    string test2("hello");
    CTempString temp_test2(test2);

    LOG_POST(Info << "CTempString() comparisons:");
    LOG_POST(Info << "  " << temp_str << " < " << temp_str << ": "
             << (temp_str < temp_str ? "true" : "false"));
    LOG_POST(Info << "  " << temp_str << " < " << temp_test2 << ": "
             << (temp_str < temp_test2 ? "true" : "false"));
    LOG_POST(Info << "  " << temp_test2 << " < " << temp_str << ": "
             << (temp_test2 < temp_str ? "true" : "false"));

    LOG_POST(Info << "std::string() comparisons:");
    LOG_POST(Info << "  " << str << " < " << str << ": "
             << (str < str ? "true" : "false"));
    LOG_POST(Info << "  " << str << " < " << test2 << ": "
             << (str < test2 ? "true" : "false"));
    LOG_POST(Info << "  " << test2 << " < " << str << ": "
             << (test2 < str ? "true" : "false"));

    BOOST_CHECK( (temp_str < temp_str) == (str < str) );
    BOOST_CHECK( (temp_str < temp_test2) == (str < test2) );
    BOOST_CHECK( (temp_test2 < temp_str) == (test2 < str) );

    /// test substring
    string sub = str.substr(0, 5);
    CTempString temp_sub = temp_str.substr(0, 5);
    LOG_POST(Info << "str.substr()      = " << sub);
    LOG_POST(Info << "temp_str.substr() = " << temp_sub);
    BOOST_CHECK(temp_sub == sub);

    sub = str.substr(5, 5);
    temp_sub = temp_str.substr(5, 5);
    LOG_POST(Info << "str.substr()      = " << sub);
    LOG_POST(Info << "temp_str.substr() = " << temp_sub);
    BOOST_CHECK(temp_sub == sub);

    /// test erase()
    sub = str;
    sub.erase(5);
    temp_sub = temp_str;
    temp_sub.erase(5);
    LOG_POST(Info << "str.erase(5)      = " << sub);
    LOG_POST(Info << "temp_str.erase(5) = " << temp_sub);
    BOOST_CHECK(temp_sub == sub);

    /// test NStr::TruncateSpaces
    sub = "  hello, world  ";
    temp_sub = "  hello, world  ";
    sub = NStr::TruncateSpaces(sub);
    temp_sub = NStr::TruncateSpaces(temp_sub);
    LOG_POST(Info << "NStr::TruncateSpaces(sub)      = >>" << sub << "<<");
    LOG_POST(Info << "NStr::TruncateSpaces(temp_sub) = >>" << temp_sub << "<<");
    BOOST_CHECK(temp_sub == sub);
    BOOST_CHECK(temp_sub == temp_str);
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


    /// test CopySubstr
    string s;
    sub = str.substr(0, 5);
    temp_str.Copy(s, 0, 5);
    LOG_POST(Info << "str.substr()    = " << sub);
    LOG_POST(Info << "temp_str.Copy() = " << s);
    BOOST_CHECK(s == sub);

    sub = str.substr(5, 5);
    temp_str.Copy(s, 5, 5);
    LOG_POST(Info << "str.substr()    = " << sub);
    LOG_POST(Info << "temp_str.Copy() = " << s);
    BOOST_CHECK(s == sub);

    sub = str.substr(5, 25);
    temp_str.Copy(s, 5, 25);
    LOG_POST(Info << "str.substr()    = " << sub);
    LOG_POST(Info << "temp_str.Copy() = " << s);
    BOOST_CHECK(s == sub);
}


test_suite* init_unit_test_suite(int /*argc*/, char * /*argv*/[])
{
    test_suite* suite = BOOST_TEST_SUITE("CTempString Unit Test");
    suite->add(BOOST_TEST_CASE(TestTempString));

    return suite;
}
