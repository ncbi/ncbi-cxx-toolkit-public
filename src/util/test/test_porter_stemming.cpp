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
* Author:  Mike DiCuccio
*
* File Description:
*   Unit test for Porter stemming algorithm
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <corelib/ncbiapp.hpp>
#include <util/dictionary_util.hpp>

// This header must be included before all Boost.Test headers if there are any
#include <corelib/test_boost.hpp>


USING_NCBI_SCOPE;

NCBITEST_INIT_CMDLINE(arg_desc)
{
    // Here we make descriptions of command line parameters that we are
    // going to use.

    arg_desc->AddKey("data-in", "InputData",
                     "Concatenated Seq-aligns used to generate gene models",
                     CArgDescriptions::eInputFile);

    arg_desc->AddFlag("perf-test",
                      "Test performance of stemming on the input data");
}



BOOST_AUTO_TEST_CASE(TestPorterStemming)
{
    const CArgs& args = CNcbiApplication::Instance()->GetArgs();
    CNcbiIstream& istr = args["data-in"].AsInputFile();

    if (args["perf-test"]) {
        ///
        /// we will read the entire corpus, and then stem the whole beast
        ///
        list<string> tokens;
        string line;
        while (NcbiGetlineEOL(istr, line)) {
            NStr::Split(line, ".[]{};:()!@#$%^&* \t", tokens);
        }

        cerr << "Found " << tokens.size() << " items to tokenize..." << endl;

        size_t iterations = 100;

        {{
             CStopWatch sw;
             sw.Start();

             string stem;
             stem.reserve(256);

             size_t count = 0;
             for (size_t i = 0;  i < iterations;  ++i) {
                 ITERATE (list<string>, it, tokens) {
                     CDictionaryUtil::Stem(*it, &stem);
                     ++count;
                 }
             }

             sw.Stop();
             double e = sw.Elapsed();
             cerr << "  ...done: " << count << " items in " << e
                  << " seconds = " << double(count) / e << " items/sec" << endl;
         }}

        {{
             CStopWatch sw;
             sw.Start();

             size_t count = 0;
             for (size_t i = 0;  i < iterations;  ++i) {
                 ITERATE (list<string>, it, tokens) {
                     string stem;
                     CDictionaryUtil::Stem(*it, &stem);
                     ++count;
                 }
             }

             sw.Stop();
             double e = sw.Elapsed();
             cerr << "  ...done: " << count << " items in " << e
                  << " seconds = " << (double)count / e << " items/sec" << endl;
         }}

    } else {
        ///
        /// stamdard unit test
        ///
        string line;
        while (NcbiGetlineEOL(istr, line)) {
            list<string> toks;
            NStr::Split(line, "\t", toks);
            if (toks.size() != 2) {
                NCBI_THROW(CException, eUnknown,
                           "invalid input line: " + line);
            }

            string stem;
            CDictionaryUtil::Stem(toks.front(), &stem);
            BOOST_CHECK_EQUAL(toks.back(), stem);
        }
    }
}




