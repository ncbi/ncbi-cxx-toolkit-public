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
 * Authors:  Clifford Clausen
 *
 * File Description:
 *   Test program for CRegexp:
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>

#include <util/regexp.hpp>
#include <util/arg_regexp.hpp>
#include <util/mask_regexp.hpp>

#include <test/test_assert.h>  /* This header must go last */


USING_NCBI_SCOPE;


class CRegexApplication : public CNcbiApplication
{
private:
    virtual void Init(void);
    virtual int  Run(void);
};



void CRegexApplication::Init(void)
{
    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "Test program for regexps");

    // Describe the expected command-line arguments
    arg_desc->AddExtra
        (1, 10,
         "These arguments must match a name-like regular expression",
         CArgDescriptions::eString);
    arg_desc->SetConstraint
        (kEmptyStr, new CArgAllow_Regexp("^[A-Z][a-z][a-z]*$"));

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}


int CRegexApplication::Run(void)
{
    // Simple way to use regular expressions
    CRegexp pattern("D\\w*g");
    LOG_POST(pattern.GetMatch("The Dodgers play baseball."));
    
    // Perl compatible regular expression pattern to match
    string pat("(q.*k).*f?x");
    pattern.Set(pat);
    
    // String to find matching pattern in
    const string text
    ("The quick brown fox jumped over the lazy dogs.\n"             \
        "Now is the time for all good men to come to the aid of "      \
        "their country.\nTwas the night before Christmas and all "     \
        "through the house, not a\n creature was stirring, not "       \
        "even a mouse.\n");

    // Display pattern and sub pattern matches
    LOG_POST(pattern.GetMatch(text));
    for (int k = 1;  k < pattern.NumFound();  k++) {
        LOG_POST(pattern.GetSub(text, 1));
    }

    LOG_POST(string(33, '-'));

    // Set new pattern and ignore case

    pattern.Set("t\\w*e", CRegexp::fCompile_ignore_case);
    // Find all matches to pattern
    size_t start = 0;
    while (start != string::npos) {
        string match = pattern.GetMatch(text, start);
        if (pattern.NumFound() > 0) {
            LOG_POST(match);
            start = text.find(match, start) + 1;            
        } else {
            break;
        }
    }
    LOG_POST(string(33, '-'));

    // Same as above but using GetResults() instead of return string

    start = 0;
    for (;;) {
        pattern.GetMatch(text, start, 0, CRegexp::fMatch_default, true);
        if (pattern.NumFound() > 0) {
            const int* rslt = pattern.GetResults(0);
            LOG_POST(text.substr(rslt[0], rslt[1] - rslt[0]));
            start = rslt[1];
        } else {
            break;
        }
    }
    LOG_POST(string(33, '-'));

    // Match() test
    {{
        pattern.Set("d?g");
        assert(!pattern.IsMatch(""));
        assert( pattern.NumFound() <= 0);
        assert( pattern.IsMatch("dog"));
        assert( pattern.NumFound() == 1);
        assert(!pattern.IsMatch("DOG"));
        assert( pattern.IsMatch("dog dog"));
        assert( pattern.NumFound() == 1);
        assert(!pattern.IsMatch("doc"));
        assert( pattern.NumFound() <= 0);
    }}

    // Escape special metacharacters test
    {{
        assert(CRegexp::Escape("a+b") == "a\\+b");
        assert(CRegexp::Escape("^.*[0-9]\\{3\\}") ==
                            "\\^\\.\\*\\[0\\-9\\]\\\\\\{3\\\\\\}");
        assert(CRegexp::Escape("a_b") == "a_b");
        assert(CRegexp::Escape("") == "");

        pattern.Set("d.*g");
        assert( pattern.IsMatch("dog"));
        pattern.Set(CRegexp::Escape("d.*g"));
        assert(!pattern.IsMatch("dog"));
        assert( pattern.IsMatch("d.*g"));

        pattern.Set(CRegexp::Escape("[0-9]{3}"));
        assert(!pattern.IsMatch("123"));
        assert( pattern.IsMatch("[0-9]{3}"));

        pattern.Set(CRegexp::Escape(".?*+$^[](){}/\\|-"));
        assert(pattern.IsMatch(".?*+$^[](){}/\\|-"));

        pattern.Set(CRegexp::WildcardToRegexp("c*t c?t"));
        assert(pattern.IsMatch("...ct cat..."));
        assert(pattern.IsMatch("...cat city..."));
        assert(pattern.IsMatch("...colt c.t..."));
        assert(!pattern.IsMatch("...cat ct..."));
    }}

    // CMaskRegexp
    {{
        CMaskRegexp mask;
        assert( mask.Match(""));
        assert( mask.Match("text"));

        mask.Add("D..");
        mask.Add("....");
        mask.Add("[0-9][0-9]*");
        mask.AddExclusion("d.*m");

        assert( mask.Match("DOG"));
        assert(!mask.Match("dog"));
        assert( mask.Match("dog", NStr::eNocase));
        assert( mask.Match("Dam"));
        assert(!mask.Match("dam"));
        assert( mask.Match("abcd"));
        assert(!mask.Match("abc"));
        assert( mask.Match("123"));

        mask.Remove("[0-9][0-9]*");
        assert(!mask.Match("123"));
    }}

    return 0;
}



/////////////////////////////////////////////////////////////////////////////
//  MAIN


int main(int argc, const char* argv[])
{
    // Execute main application function
    return CRegexApplication().AppMain(argc, argv, 0, eDS_Default, 0);
}


/*
 * ===========================================================================
 * $Log$
 * Revision 1.11  2006/11/21 21:00:25  ivanov
 * Added test for CMaskRegexp
 *
 * Revision 1.10  2006/05/15 16:03:31  ivanov
 * Added tests for CRegexp::WildcardToRegexp()
 *
 * Revision 1.9  2006/03/27 19:48:05  ivanov
 * Added tests for Escape(), IsMatch()
 *
 * Revision 1.8  2006/01/09 12:37:36  ivanov
 * Use LOG_POST instead of cout
 *
 * Revision 1.7  2005/10/24 15:28:39  vakatov
 * Added test for regexp-based cmdline argument validation (CArgAllow_Regexp).
 * Simplified some other tests.
 *
 * Revision 1.6  2004/11/22 17:20:50  ivanov
 * Use fCompile_* and fMatch_* flags.
 *
 * Revision 1.5  2004/11/22 16:47:57  ivanov
 * Use CRegexp::eMatch_default instead of 0 as default pattern match flags.
 *
 * Revision 1.4  2004/05/17 21:09:26  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 1.3  2003/06/26 15:37:41  lavr
 * Remove unused variable "len", line 97
 *
 * Revision 1.2  2003/06/20 18:31:51  clausen
 * Switched to work with new CRegexp interface
 *
 * Revision 1.1  2003/06/03 14:51:53  clausen
 * test_regexp.cpp
 *
 * ===========================================================================
 */
