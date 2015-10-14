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
* Author:  Andrea Asztalos
*
* File Description:
*   Unit tests for parsing text options
*
*/

#include <ncbi_pch.hpp>

// This header must be included before all Boost.Test headers if there are any
#include <corelib/test_boost.hpp>
#include <objtools/edit/parse_text_options.hpp>

#include <common/test_assert.h>  /* This header must go last */


USING_NCBI_SCOPE;
USING_SCOPE(objects);
USING_SCOPE(edit);

NCBITEST_AUTO_INIT()
{
}

NCBITEST_AUTO_FINI()
{
}

BOOST_AUTO_TEST_CASE(Test_SelectedText)
{
    string text("Rain, RAIN, go away! All the 6 children want to play!");
    CRef<CParseTextOptions> options(new CParseTextOptions);
    BOOST_CHECK_EQUAL(options->GetSelectedText(text), text);

    options->Reset();
    options->SetStartText("RAIN, ");
    options->SetCaseInsensitive(false);
    options->SetStopText("!");
    options->SetIncludeStop(true);
    BOOST_CHECK_EQUAL(options->GetSelectedText(text), string("go away!"));

    options->Reset();
    options->SetStartDigits();
    options->SetStopText(" ");
    options->SetIncludeStart(true);
    BOOST_CHECK_EQUAL(options->GetSelectedText(text), "6");

    options->Reset();
    options->SetStartText("IN, ");
    options->SetCaseInsensitive(true);
    options->SetStopDigits();
    BOOST_CHECK_EQUAL(options->GetSelectedText(text), "RAIN, go away! All the");

    text.assign("internal transcribed, spacer 1 (ITS1) rna");
    options->Reset();
    options->SetStartDigits();
    options->SetStopLetters();
    BOOST_CHECK_EQUAL(options->GetSelectedText(text), string("("));

    options->Reset();
    options->SetStartText(")");
    options->SetStopText("(");
    BOOST_CHECK(options->GetSelectedText(text).empty());
}

BOOST_AUTO_TEST_CASE(Test_RemoveSelectedText)
{
    string text("Rain, RAIN, go away! All the 6 children want to play!");
    CRef<CParseTextOptions> options(new CParseTextOptions);
    options->SetStartText("! ");
    options->SetStopDigits();
    options->SetIncludeStop(true);
    BOOST_CHECK_EQUAL(options->GetSelectedText(text), string("All the 6"));
    
    string copy(text);
    options->RemoveSelectedText(copy);
    BOOST_CHECK_EQUAL(copy, string("Rain, RAIN, go away!  children want to play!"));

    options->Reset();
    options->SetStartText("!");
    options->SetStopText("(");
    BOOST_CHECK(options->GetSelectedText(text).empty());

    options->Reset();
    options->SetStartDigits();
    options->SetStopText(" to");
    BOOST_CHECK_EQUAL(options->GetSelectedText(text), string("children want"));

    copy.assign(text);
    options->SetShouldRmvBeforePattern(true);
    options->SetShouldRmvAfterPattern(true);
    options->RemoveSelectedText(copy);
    BOOST_CHECK_EQUAL(copy, string("Rain, RAIN, go away! All the  play!"));
}

BOOST_AUTO_TEST_CASE(Test_FindInText)
{
    string text("Hypothetical protein gene");
    CRef<CParseTextMarker> marker(new CParseTextMarker);
    marker->SetDigits();
    size_t pos(0), len(0), start_search(0);
    bool case_insensitive(false), whole_word(false);
    BOOST_CHECK(!marker->FindInText(text, pos, len, start_search, case_insensitive, whole_word));
    BOOST_CHECK(len == 0);

    text.assign("small subunit and 18S ribosomal rna");
    pos = 0; len = 0; start_search = 6;
    BOOST_CHECK(marker->FindInText(text, pos, len, start_search, case_insensitive, whole_word));
    BOOST_CHECK(pos == 18);
    BOOST_CHECK(len == 2);

    marker->Reset();
    marker->SetLetters();
    pos = 0; len = 0; start_search = 18;
    BOOST_CHECK(marker->FindInText(text, pos, len, start_search, case_insensitive, whole_word));
    BOOST_CHECK(pos == 20);
    BOOST_CHECK(len == 1);

    // searching for whole words
    text.assign("the cat has fleas, the catalog is on the table");
    marker->Reset();
    whole_word = true;
    pos = 0; len = 0; start_search = 0;
    case_insensitive = false;
    marker->SetText("cat");
    BOOST_CHECK(marker->FindInText(text, pos, len, start_search, case_insensitive, whole_word));
    BOOST_CHECK(pos = 3);
    BOOST_CHECK(len == 3);

    pos = 0; len = 0; start_search = 6;
    BOOST_CHECK(!marker->FindInText(text, pos, len, start_search, case_insensitive, whole_word));
    BOOST_CHECK(len == 0);
    
}
