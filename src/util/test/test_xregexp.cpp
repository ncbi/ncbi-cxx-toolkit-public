/*  $Id$
 * ===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *               National Center for Biotechnology Information
 *
 *   This software/database is a "United States Government Work" under the
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
 * Authors: Vladimir Ivanov, Alex Kotliarov
 *
 * File Description:
 *   Test program for xregexp library:
 *       - CMaskRegexp
 *       - ConvertDateTo_iso8601() methods.
 *
 * ConvertDateTo* tests copied and adapted from gpipe/common/unit_test/unit_test_dateutil.cpp
 */

#include <ncbi_pch.hpp>
#include <util/xregexp/mask_regexp.hpp>
#include <util/xregexp/convert_dates_iso8601.hpp>

#define BOOST_AUTO_TEST_MAIN
#include <corelib/test_boost.hpp>

#include <common/test_assert.h>  // This header must go last


USING_NCBI_SCOPE;


BOOST_AUTO_TEST_CASE(MaskRegexp)
{
    CMaskRegexp mask;
    BOOST_CHECK( mask.Match(""));
    BOOST_CHECK( mask.Match("text"));

    mask.Add("D..");
    mask.Add("....");
    mask.Add("[0-9][0-9]*");
    mask.AddExclusion("d.*m");

    BOOST_CHECK( mask.Match("DOG"));
    BOOST_CHECK(!mask.Match("dog"));
    BOOST_CHECK( mask.Match("dog", NStr::eNocase));
    BOOST_CHECK( mask.Match("Dam"));
    BOOST_CHECK(!mask.Match("dam"));
    BOOST_CHECK( mask.Match("abcd"));
    BOOST_CHECK(!mask.Match("abc"));
    BOOST_CHECK( mask.Match("123"));

    mask.Remove("[0-9][0-9]*");
    BOOST_CHECK(!mask.Match("123"));
}


BOOST_AUTO_TEST_CASE(ConvertDate_iso8601)
{
    pair<string, string> test_cases[] = {
        {"2010-05-13",          "2010-05-13"},
        {"2010-05-13T01:00:59", "2010-05-13"},
        {"2010-05-13 01:00:59", "2010-05-13"},
        {"2010-05-13 01:00",    "2010-05-13"},
        {"2010-05",             "2010-05"},
        {"2010",                "2010"}, 
        {"05/13/2010",          "2010-05-13"},
        {"13/05/2010",          "2010-05-13"},
        {"13-May-2010",         "2010-05-13"},
        {"03/03/2010",          "2010-03-03"},
        {"03/04/2010",          "2010"},
        {"May 13, 2010",        "2010-05-13"},
        {"may 13, 2010",        "2010-05-13"},
        {"february 13 2010",    "2010-02-13"},
        {"13-may-2010",         "2010-05-13"},
        {"23/12/2010 12:00 AM", "2010-12-23"},
        {"23/12/10 12:00AM",    "2010-12-23"},
        {"05/14/10 11:59pm",    "2010-05-14"}, 
        {"unknown",             "missing"},
        {"n.a.",                "missing"},
        {"NA",                  "missing"},
        {"not collected",       "missing"},
        {"have no idea when",   "missing"},
        {"2010-05-32",          ""},
        {"2010-13",             ""},
        {"2010-00-13",          ""},
        {"2010-14-13",          ""},
        {"32-May-2010",         ""}
    };

    for ( auto const& i: test_cases ) {
        BOOST_CHECK( i.second == ConvertDateTo_iso8601(i.first) );
    }
}


BOOST_AUTO_TEST_CASE(ConvertDate_iso8601_and_annotate)
{
    struct TestCase {
        char const* value;
        char const* expect_value;
        char const* expect_annot;
    }
     test_cases[] = {
        {"2010-05-13",            "2010-05-13",   "ISO-8601"},
        {"2010-05-13T01:00:59",   "2010-05-13",   "ISO-8601"},
        {"2010-05-13 01:00:59",   "2010-05-13",   "ISO-8601"},
        {"2010-05-13 01:00",      "2010-05-13",   "ISO-8601"},
        {"2010-05",               "2010-05",      "ISO-8601"},
        {"2010",                  "2010",         "ISO-8601"}, 
        {"05/13/2010",            "2010-05-13",   "CAST|ISO-8601"},
        {"13/05/2010",            "2010-05-13",   "CAST|ISO-8601"},
        {"13-May-2010",           "2010-05-13",   "CAST|ISO-8601"},
        {"03/03/2010",            "2010-03-03",   "CAST|ISO-8601"},
        {"03/04/2010",            "2010",         "CAST|YYYY"},
        {"May 13, 2010",          "2010-05-13",   "CAST|ISO-8601"},
        {"23/12/2010 12:00 AM",   "2010-12-23",   "CAST|ISO-8601"},
        {"unknown",               "missing",      "CAST|NA"},
        {"2010-05-32",            "",             "NODATE"},
        {"2010-13",               "",             "NODATE"},
        {"2010-00-13",            "",             "NODATE"},
        {"2010-14-13",            "",             "NODATE"},
        {"32-May-2010",           "",             "NODATE"},
        {"",                      "",             "NODATE"},
        {"2010/2011",             "2010/2011",    "RANGE|ISO-8601"},
        {"2010-04/2011-05",       "2010-04/2011-05", "RANGE|ISO-8601"},
        {"2010-01-01/2012-01-01", "2010-01-01/2012-01-01", "RANGE|ISO-8601"},
        {"before 2011",           "1900/2010",    "RANGE|CAST|ISO-8601"},
        {"pre-1986",              "1900/1985",    "RANGE|CAST|ISO-8601"},
        {"1980s",                 "1980/1989",    "RANGE|CAST|ISO-8601"},
        {"between 2011-01 and 2012-04", "2011-01/2012-04", "RANGE|CAST|ISO-8601"},
        {0, 0, 0}
    };

    for ( struct TestCase* entry = test_cases; entry->value != 0; ++entry ) {
        pair<string, string> result = ConvertDateTo_iso8601_and_annotate(entry->value);
        BOOST_CHECK( result.second == entry->expect_value );
        BOOST_CHECK( result.first == entry->expect_annot );
    }
}


BOOST_AUTO_TEST_CASE(ConvertDate_iso8601_partial)
{
    pair<string, string> test_cases[] = {

        {"2010/1/22",    "2010-01-22"},
        {"2010/01/22",   "2010-01-22"},
        {"20102/01/22",  ""},

        {"1/22/2010",    "2010-01-22"},
        {"01/22/2010",   "2010-01-22"},
        {"10/22/2010",   "2010-10-22"},
        {"1/22/10",      "2010-01-22"},
        {"01/22/10",     "2010-01-22"},
        {"10/22/10",     "2010-10-22"},

        {"1/2/2010",     "2010"},
        {"01/2/2010",    "2010"},
        {"10/2/2010",    "2010"},
        {"1/2/10",       "2010"},
        {"01/2/10",      "2010"},
        {"10/2/10",      "2010"},

        {"1/02/2010",    "2010"},
        {"01/02/2010",   "2010"},
        {"10/02/2010",   "2010"},
        {"1/02/10",      "2010"},
        {"01/02/10",     "2010"},
        {"10/02/10",     "2010"},

        {"22/1/2010",    "2010-01-22"},
        {"22/01/2010",   "2010-01-22"},
        {"22/10/2010",   "2010-10-22"},
        {"22/1/10",      "2010-01-22"},
        {"22/01/10",     "2010-01-22"},
        {"22/10/10",     "2010-10-22"},

        {"2/1/2010",     "2010"}, // ambiguous dates, not sure where is a day or month
        {"2/01/2010",    "2010"},
        {"2/10/2010",    "2010"},
        {"2/1/10",       "2010"},
        {"2/01/10",      "2010"},
        {"2/10/10",      "2010"},

        {"2 september, 2010",  "2010-09-02"},
        {"2 september 2010",   "2010-09-02"},
        {"2 september, 10",    "2010-09-02"},
        {"2 september 10",     "2010-09-02"},
        {"2 sep, 2010",        "2010-09-02"},
        {"2 sep 2010",         "2010-09-02"},
        {"2 sep, 10",          "2010-09-02"},
        {"2 sep 10",           "2010-09-02"},
        {"22 september, 2010", "2010-09-22"},
        {"22 september 2010",  "2010-09-22"},
        {"22 september, 10",   "2010-09-22"},
        {"22 september 10",    "2010-09-22"},
        {"22 sep, 2010",       "2010-09-22"},
        {"22 sep 2010",        "2010-09-22"},
        {"22 sep, 10",         "2010-09-22"},
        {"22 sep 10",          "2010-09-22"},

        {"2 april, 2010",   "2010-04-02"},
        {"2 april 2010",    "2010-04-02"},
        {"2 april, 10",     "2010-04-02"},
        {"2 april 10",      "2010-04-02"},
        {"2 apr, 2010",     "2010-04-02"},
        {"2 apr 2010",      "2010-04-02"},
        {"2 apr, 10",       "2010-04-02"},
        {"2 apr 10",        "2010-04-02"},
        {"22 april, 2010",  "2010-04-22"},
        {"22 april 2010",   "2010-04-22"},
        {"22 april, 10",    "2010-04-22"},
        {"22 april 10",     "2010-04-22"},
        {"22 apr, 2010",    "2010-04-22"},
        {"22 apr 2010",     "2010-04-22"},
        {"22 apr, 10",      "2010-04-22"},
        {"22 apr 10",       "2010-04-22"},

        {"2 march, 2010",   "2010-03-02"},
        {"2 march 2010",    "2010-03-02"},
        {"2 march, 10",     "2010-03-02"},
        {"2 march 10",      "2010-03-02"},
        {"2 mar, 2010",     "2010-03-02"},
        {"2 mar 2010",      "2010-03-02"},
        {"2 mar, 10",       "2010-03-02"},
        {"2 mar 10",        "2010-03-02"},
        {"22 march, 2010",  "2010-03-22"},
        {"22 march 2010",   "2010-03-22"},
        {"22 march, 10",    "2010-03-22"},
        {"22 march 10",     "2010-03-22"},
        {"22 mar, 2010",    "2010-03-22"},
        {"22 mar 2010",     "2010-03-22"},
        {"22 mar, 10",      "2010-03-22"},
        {"22 mar 10",       "2010-03-22"},

        {"2 june, 2010",    "2010-06-02"},
        {"2 june 2010",     "2010-06-02"},
        {"2 june, 10",      "2010-06-02"},
        {"2 june 10",       "2010-06-02"},
        {"2 jun, 2010",     "2010-06-02"},
        {"2 jun 2010",      "2010-06-02"},
        {"2 jun, 10",       "2010-06-02"},
        {"2 jun 10",        "2010-06-02"},
        {"22 june, 2010",   "2010-06-22"},
        {"22 june 2010",    "2010-06-22"},
        {"22 june, 10",     "2010-06-22"},
        {"22 june 10",      "2010-06-22"},
        {"22 jun, 2010",    "2010-06-22"},
        {"22 jun 2010",     "2010-06-22"},
        {"22 jun, 10",      "2010-06-22"},
        {"22 jun 10",       "2010-06-22"},

        {"2 may, 2010",     "2010-05-02"},
        {"2 may 2010",      "2010-05-02"},
        {"2 may, 10",       "2010-05-02"},
        {"2 may 10",        "2010-05-02"},
        {"2 may, 2010",     "2010-05-02"},
        {"2 may 2010",      "2010-05-02"},
        {"2 may, 10",       "2010-05-02"},
        {"2 may 10",        "2010-05-02"},
        {"22 may, 2010",    "2010-05-22"},
        {"22 may 2010",     "2010-05-22"},
        {"22 may, 10",      "2010-05-22"},
        {"22 may 10",       "2010-05-22"},
        {"22 may, 2010",    "2010-05-22"},
        {"22 may 2010",     "2010-05-22"},
        {"22 may, 10",      "2010-05-22"},
        {"22 may 10",       "2010-05-22"},

        {"may 2010",        "2010-05"},
        {"2010 may",        "2010-05"},
        {"05/2010",         "2010-05"}
    };

    for ( auto const& i: test_cases ) {
        BOOST_CHECK( i.second == ConvertDateTo_iso8601(i.first) );
    }
}

