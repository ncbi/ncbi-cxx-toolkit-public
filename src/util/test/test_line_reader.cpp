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
 * Author:  Aaron Ucko, Anatoliy Kuznetsov, Eugene Vasilchenko
 *
 * File Description:
 *   Lightweight interface for getting lines of data with minimal
 *   memory copying.
 *
 * ===========================================================================
 */

#include <ncbi_pch.hpp>

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbifile.hpp>
#include <corelib/rwstream.hpp>
#include <corelib/tempstr.hpp>
#include <util/random_gen.hpp>
#include <util/line_reader.hpp>                                                    
#include <corelib/test_boost.hpp>

#include <common/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;


static bool s_TryMemory;
static bool s_UseStream;
static bool s_FastScan;

NCBITEST_INIT_TREE()
{}


NCBITEST_INIT_CMDLINE(arg_desc)
{
    arg_desc->SetUsageContext(CNcbiApplication::Instance()->
                                            GetArguments().GetProgramBasename(),
                              "CArgDescriptions demo program");

    arg_desc->AddOptionalKey("in", "in", "Input FASTA filename",
                             CArgDescriptions::eInputFile);
    arg_desc->AddOptionalKey("ins", "ins", "Input FASTA filenames",
                             CArgDescriptions::eString);
    arg_desc->AddFlag("mem", "Use memory stream if possible");
    arg_desc->AddFlag("stream", "Use istream");
    arg_desc->AddFlag("fast", "Scan as fast as possible");
    arg_desc->AddFlag("selftest", "Run self checks");
}


NCBITEST_AUTO_FINI()
{
    SetDiagStream(0);
}


static CRef<ILineReader> s_CreateLineReader(const string& filename)
{
    CRef<ILineReader> line_reader;
    if (s_TryMemory) {
        try {
            line_reader.Reset(new CMemoryLineReader(new CMemoryFile(filename),
                eTakeOwnership));
        }
        catch (...) { // fall back to streams
        }
    }
    if (!line_reader) {
        if (s_UseStream) {
            if (filename == "-") {
                line_reader =
                    new CStreamLineReader(NcbiCin);
            }
            else {
                line_reader =
                    new CStreamLineReader(*new CNcbiIfstream(filename.c_str(),
                    ios::binary),
                    eTakeOwnership);
            }
        }
        else {
            line_reader = ILineReader::New(filename);
        }
    }
    return line_reader;
}


NCBITEST_AUTO_INIT()
{
    boost::unit_test::
        framework::master_test_suite().p_name->assign("ILineReader Unit Test");
    const CArgs& args (CNcbiApplication::Instance()->GetArgs());

    s_TryMemory = args["mem"];
    s_UseStream = args["stream"];
    s_FastScan = args["fast"];

    if ( !args["selftest"] ) {
        NCBITEST_DISABLE( RunSelfTest );
    }

    if ( !args["in"] && !args["ins"] ) {
        NCBITEST_DISABLE(RunFile);
    }
}


static void s_RunFile(const string& filename)
{
    NcbiCout << "Processing file: "<< filename << NcbiEndl;
    CRef<ILineReader> line_reader = s_CreateLineReader(filename);
    
    if ( s_FastScan ) {
        size_t lines = 0, chars = 0;
        while ( !line_reader->AtEOF() ) {
            ++lines;
            chars += (*++*line_reader).size();
        }
        NcbiCout << "Lines: " << lines << NcbiEndl;
        NcbiCout << "Chars: " << chars << NcbiEndl;
    }
    else {
        size_t lines = 0, chars = 0, sum = 0;
        ++*line_reader;
        line_reader->UngetLine();
        while ( !line_reader->AtEOF() ) {
            CTempString s = *++*line_reader;
            ++lines;
            chars += s.size();
            ITERATE ( CTempString, i, s ) {
                sum += *i;
            }
        }
        NcbiCout << "Lines: " << lines << NcbiEndl;
        NcbiCout << "Chars: " << chars << NcbiEndl;
        NcbiCout << "  Sum: " << sum   << NcbiEndl;
    }
}


BOOST_AUTO_TEST_CASE( RunFile )
{
    const CArgs& args = CNcbiApplication::Instance()->GetArgs();
    if (args["in"]) {
        const string& filename = args["in"].AsString();
        s_RunFile(filename);
    }

    if (args["ins"]) {
        list<string> ff;
        NStr::Split(args["ins"].AsString(), " ,", ff);
        ITERATE(list<string>, it, ff) {
            s_RunFile(*it);
        }
    }
}

/** Fill test file with test data and return its name */
static string s_CreateTestFile(vector<string>& lines, 
                               vector<CT_POS_TYPE>& positions)
{
    CRandom r;
    int lines_num = 100;
    /* If the supplied vector is empty, fill it randomly */
    if (lines.empty()) {
        for (int i = 0; i < lines_num; ++i) {
            int size;
            if (r.GetRand(0, lines_num / 2) == 0) {
                size = r.GetRand(0, 100000);
            }
            else {
                size = r.GetRand(0, 10);
            }
            string line(size, ' ');
            for (int j = 0; j < size; ++j) {
                line[j] = (char)r.GetRand(' ', 126);
            }
            lines.push_back(line);
        }
    }
    string filename = CFile::GetTmpName(CFile::eTmpFileCreate);
    try {
        {{
            CNcbiOfstream out(filename.c_str(), ios::binary);
            bool bLastShowR = true;
            bool bLastShowN = true;
            ITERATE ( vector<string>, i, lines ) {
                positions.push_back(out.tellp());
                out << *i;
                // we test LF, CRLF, and (just in case) CR
                CRandom::TValue rand_value = r.GetRand();
                bool bShowR = (rand_value & 1);
                bool bShowN = (rand_value & 2);
                if( i->empty() && bLastShowR && ! bLastShowN && ! bShowR ) {
                    // this line is blank, so if last line is just
                    // an '\r', this can't be just an '\n' or
                    // we'll throw off our line counts.
                    bShowR = true;
                }
                if( ! bShowR && ! bShowN ) {
                    // have to have at least one endline char
                    bShowN = true;
                }
                if ( bShowR ) out << '\r';
                if( bShowN ) out << '\n';
                bLastShowR = bShowR;
                bLastShowN = bShowN;
            }
            positions.push_back(out.tellp());
        }}
    }
    catch (...) {
        CFile(filename).Remove();
        throw;
    }
    return filename;
}

/** Get one of ILineReader implementations:
 *  1. CMemoryLineReader
 *  2. CStreamLineReader
 *  3. CBufferedLineReader */
static CRef<ILineReader> s_GetLineReader(string filename, int type)
{
    CRef<ILineReader> rdr;
    switch ( type ) {
    case 0: 
        LOG_POST(Error << "CMemoryLineReader");
        rdr = new CMemoryLineReader(new CMemoryFile(filename),
                                    eTakeOwnership);
        break;
    case 1:
        LOG_POST(Error << "CStreamLineReader");
        rdr = new CStreamLineReader(
                        *new CNcbiIfstream(filename.c_str(), ios::binary),
                        CStreamLineReader::eEOL_mixed,
                        eTakeOwnership);
        break;
    case 2:
        LOG_POST(Error << "CBufferedLineReader");
        rdr = CBufferedLineReader::New(filename);
        break;
    }
    return rdr;
}

BOOST_AUTO_TEST_CASE( RunSelfTest )
{
    CRandom r;
    int errors = 0;
    vector<CT_POS_TYPE> positions;
    vector<string> lines;
    string filename = s_CreateTestFile(lines,
                                       positions);
    for ( int type = 0; type < 3; ++type ) {
        CRef<ILineReader> rdr;
        rdr = s_GetLineReader(filename, type);
        /* Test itself. For each reader the following behavior is tested:
            1. Right after constructor, PeekChar returns first symbol of first string
            2. Right after Unget
            3. If line is empty*/
        int l = 0;
        while ( !rdr->AtEOF() ) {
            if ( r.GetRand()&1 ) {
                ++*rdr;
                rdr->UngetLine();
            }
            CTempString s = *++*rdr;
            if ( !(s == lines[l]) ||
                    rdr->GetPosition() != positions[l+1] ) {
                ERR_POST("ILineReader type "<<type<<" at "<<l<<
                            " \""<<s<<"\" vs \""<<lines[l]<<"\"");
                ERR_POST("Next line position: "<<
                         NcbiStreamposToInt8(rdr->GetPosition())<<
                         " vs "<< NcbiStreamposToInt8(positions[l]));
                ++errors;
                break;
            }
            _ASSERT(s == lines[l]);
            _ASSERT(rdr->GetPosition() == positions[l+1]);
            ++l;
        }
    }
    CFile(filename).Remove();
    // return errors;
}


BOOST_AUTO_TEST_CASE( PeekCharAfterUnget__ReturnFirstSymbolCurrentLine )
{
    vector<string> lines;
    lines.push_back("First line");
    lines.push_back("Second line");
    lines.push_back("Third line");
    vector<CT_POS_TYPE> positions;
    string filename = s_CreateTestFile(lines,
                                       positions);

    for ( int type = 0; type < 3; ++type ) {
        CRef<ILineReader> rdr;
        rdr = s_GetLineReader(filename, type);

        /* 1. ++ 
           2. Unget 
           3. PeekChar
           4. Char should equal 'F' (first symbol in the first string) */
        ++(*rdr);
        rdr->UngetLine();
        char c = rdr->PeekChar();
        NCBITEST_CHECK_EQUAL(c, 'F');
        /* 1. ++, ++ 
           2. Unget 
           3. PeekChar
           4. Char should equal 'S' (first symbol in the second string) */
        ++(*rdr);
        ++(*rdr);
        rdr->UngetLine();
        c = rdr->PeekChar();
        NCBITEST_CHECK_EQUAL(c, 'S');        
        /* 1. ++, ++
           2. Unget
           3. PeekChar
           4. Char should equal 'T' (first symbol in the third string) */
        ++(*rdr);
        ++(*rdr);
        rdr->UngetLine();
        c = rdr->PeekChar();
        NCBITEST_CHECK_EQUAL(c, 'T');
    }
    
    CFile(filename).Remove();
}


BOOST_AUTO_TEST_CASE(PeekCharAfterPlusPlus__FirstCharNextString)
{
    vector<string> lines;
    lines.push_back("First line");
    lines.push_back("Second line");
    lines.push_back("Third line");
    vector<CT_POS_TYPE> positions;
    string filename = s_CreateTestFile(lines,
                                       positions);

    for ( int type = 0; type < 3; ++type ) {
        CRef<ILineReader> rdr;
        rdr = s_GetLineReader(filename, type);

        /* 1. ++ 
           2. PeekChar
           3. Char should equal 'S' (first symbol in the second string) */
        ++(*rdr);
        char c = rdr->PeekChar();
        NCBITEST_CHECK_EQUAL(c, 'S');    
        /* 1. ++
           2. PeekChar
           3. Char should equal 'T' (first symbol in the third string) */
        ++(*rdr);
        c = rdr->PeekChar();
        NCBITEST_CHECK_EQUAL(c, 'T');
    }
    
    CFile(filename).Remove();
}


BOOST_AUTO_TEST_CASE(PeekCharAfterTwicePlusPlus__FirstCharNextNextString)
{
    vector<string> lines;
    lines.push_back("First line");
    lines.push_back("Second line");
    lines.push_back("Third line");
    vector<CT_POS_TYPE> positions;
    string filename = s_CreateTestFile(lines,
                                       positions);

    for ( int type = 0; type < 3; ++type ) {
        CRef<ILineReader> rdr;
        rdr = s_GetLineReader(filename, type);

        /* 1. ++ 
           2. PeekChar
           3. Char should equal 'T' (first symbol in the third string) */
        ++(*rdr);
        ++(*rdr);
        char c = rdr->PeekChar();
        NCBITEST_CHECK_EQUAL(c, 'T');    
    }
    
    CFile(filename).Remove();
}

BOOST_AUTO_TEST_CASE(PeekCharAtEOF__Undefined)
{
    /* undefined behavior */
}

BOOST_AUTO_TEST_CASE(PeekCharAfterPlusPlusAtEOF__Undefined)
{
    /* undefined behavior */
}


BOOST_AUTO_TEST_CASE(PeekCharAfterConstructor__FirstCharFirstString)
{
    vector<string> lines;
    lines.push_back("First line");
    lines.push_back("Second line");
    lines.push_back("Third line");
    vector<CT_POS_TYPE> positions;
    string filename = s_CreateTestFile(lines,
                                       positions);

    for ( int type = 0; type < 3; ++type ) {
        CRef<ILineReader> rdr;
        rdr = s_GetLineReader(filename, type);

        /* 1. Constructor
           2. PeekChar
           3. Char should equal 'F' (first symbol in the first string) */
        char c = rdr->PeekChar();
        NCBITEST_CHECK_EQUAL(c, 'F');    
    }    
    CFile(filename).Remove();
}


BOOST_AUTO_TEST_CASE(PeekCharEmptyString__ReturnsZero)
{
    vector<string> lines;
    lines.push_back("");
    lines.push_back("");
    lines.push_back("");
    vector<CT_POS_TYPE> positions;
    string filename = s_CreateTestFile(lines,
                                       positions);

    for ( int type = 0; type < 3; ++type ) {
        CRef<ILineReader> rdr;
        rdr = s_GetLineReader(filename, type);
        /* 1. PeekChar
           2. Char should equal 0 (first symbol in the first string) */
        char c = rdr->PeekChar();
        NCBITEST_CHECK_EQUAL(c, 0);
        /* 1. ++
           2. PeekChar
           3. Char should equal 'S' (first symbol in the second string) */
        ++(*rdr);
        c = rdr->PeekChar();
        NCBITEST_CHECK_EQUAL(c, 0);
        /* 1. ++
           2. PeekChar
           3. Char should equal 'T' (first symbol in the third string) */
        ++(*rdr);
        c = rdr->PeekChar();
        NCBITEST_CHECK_EQUAL(c, 0);
    }
    CFile(filename).Remove();
}


BOOST_AUTO_TEST_CASE(GetCurrentLineAfterConstructor__NULL)
{
    vector<string> lines;
    lines.push_back("First line");
    lines.push_back("Second line");
    lines.push_back("Third line");
    vector<CT_POS_TYPE> positions;
    string filename = s_CreateTestFile(lines,
                                       positions);

    for ( int type = 0; type < 3; ++type ) {
        CRef<ILineReader> rdr;
        rdr = s_GetLineReader(filename, type);

        /* 1. Constructor
           2. Operator*
           3. String should be NULL */
        CTempString str = **rdr;
        NCBITEST_CHECK_MESSAGE(str.empty(), "Returned CTempString is not empty");
    }    
    CFile(filename).Remove();
}


BOOST_AUTO_TEST_CASE(GetCurrentLineAfterPlusPlus__FirstLine)
{
    vector<string> lines;
    lines.push_back("First line");
    lines.push_back("Second line");
    lines.push_back("Third line");
    vector<CT_POS_TYPE> positions;
    string filename = s_CreateTestFile(lines,
                                       positions);

    for ( int type = 0; type < 3; ++type ) {
        CRef<ILineReader> rdr;
        rdr = s_GetLineReader(filename, type);

        /* 1. Constructor
           2. Operator++
           3. Operator*
           4. String should be first string */
        ++*rdr;
        CTempString str = **rdr;
        NCBITEST_CHECK_EQUAL(lines[0], str);

        /* 1. Constructor
           2. Operator++
           3. Operator*
           4. String should be first string */
        ++*rdr;
        str = **rdr;
        NCBITEST_CHECK_EQUAL(lines[1], str);

        /* 1. Constructor
           2. Operator++
           3. Operator*
           4. String should be first string */
        ++*rdr;
        str = **rdr;
        NCBITEST_CHECK_EQUAL(lines[2], str);
    }    
    CFile(filename).Remove();
}


BOOST_AUTO_TEST_CASE(GetCurrentLineAfterTwicePlusPlus__SecondLine)
{
    vector<string> lines;
    lines.push_back("First line");
    lines.push_back("Second line");
    lines.push_back("Third line");
    vector<CT_POS_TYPE> positions;
    string filename = s_CreateTestFile(lines,
                                       positions);

    for ( int type = 0; type < 3; ++type ) {
        CRef<ILineReader> rdr;
        rdr = s_GetLineReader(filename, type);

        /* 1. Constructor
           2. Operator++, operator++
           3. Operator*
           4. String should be second string */
        ++*rdr;
        ++*rdr;
        CTempString str = **rdr;
        NCBITEST_CHECK_EQUAL(lines[1], str);
    }    
    CFile(filename).Remove();
}


BOOST_AUTO_TEST_CASE(GetCurrentLineAtEOF__NULL)
{
    vector<string> lines;
    lines.push_back("First line");
    lines.push_back("Second line");
    lines.push_back("Third line");
    vector<CT_POS_TYPE> positions;
    string filename = s_CreateTestFile(lines,
                                       positions);

    for ( int type = 0; type < 3; ++type ) {
        CRef<ILineReader> rdr;
        rdr = s_GetLineReader(filename, type);

        /* 1. Constructor
           2. Operator++, operator++, operator++, operator++
           3. Operator*
           4. String should be second string */
        ++*rdr; //on the first line
        ++*rdr; //on the second line
        ++*rdr; //on the third line
        ++*rdr; //EOF
        CTempString str = **rdr;
        NCBITEST_CHECK_MESSAGE(str.empty(), "Returned CTempString is not empty");
    }
    CFile(filename).Remove();
}


BOOST_AUTO_TEST_CASE(GetCurrentLineAfterPlusPlusAtEOF__NULL)
{
    vector<string> lines;
    lines.push_back("First line");
    lines.push_back("Second line");
    lines.push_back("Third line");
    vector<CT_POS_TYPE> positions;
    string filename = s_CreateTestFile(lines,
                                       positions);

    for ( int type = 0; type < 3; ++type ) {
        CRef<ILineReader> rdr;
        rdr = s_GetLineReader(filename, type);

        /* 1. Constructor
           2. Operator++, operator++, operator++, operator++, operator++
           3. Operator*
           4. String should be second string */
        ++*rdr; //on the first line
        ++*rdr; //on the second line
        ++*rdr; //on the third line
        ++*rdr; //EOF
        ++*rdr; //EOF
        CTempString str = **rdr;
        NCBITEST_CHECK_MESSAGE(str.empty(), 
                               "Returned CTempString is not empty");
    }    
    CFile(filename).Remove();
}


BOOST_AUTO_TEST_CASE(GetLineNumberAfterConstructor__0)
{
    vector<string> lines;
    lines.push_back("First line");
    lines.push_back("Second line");
    lines.push_back("Third line");
    vector<CT_POS_TYPE> positions;
    string filename = s_CreateTestFile(lines,
                                       positions);

    for ( int type = 0; type < 3; ++type ) {
        CRef<ILineReader> rdr;
        rdr = s_GetLineReader(filename, type);

        /* 1. Constructor
           2. GetLineNumber returns 0 */
        int line_number = rdr->GetLineNumber();
        NCBITEST_CHECK_EQUAL(line_number, 0);
    }    
    CFile(filename).Remove();
}


BOOST_AUTO_TEST_CASE(GetLineNumberAfterPlusPlus__1)
{
    vector<string> lines;
    lines.push_back("First line");
    lines.push_back("Second line");
    lines.push_back("Third line");
    vector<CT_POS_TYPE> positions;
    string filename = s_CreateTestFile(lines,
                                       positions);

    for ( int type = 0; type < 3; ++type ) {
        CRef<ILineReader> rdr;
        rdr = s_GetLineReader(filename, type);

        /* 1. Constructor
           2. Operator++
           3. GetLineNumber returns 1 */
        ++*rdr;
        int line_number = rdr->GetLineNumber();
        NCBITEST_CHECK_EQUAL(line_number, 1);
        /* 1. Constructor
           2. Operator++
           3. GetLineNumber returns 1 */
        ++*rdr;
        line_number = rdr->GetLineNumber();
        NCBITEST_CHECK_EQUAL(line_number, 2);
        /* 1. Constructor
           2. Operator++
           3. GetLineNumber returns 1 */
        ++*rdr;
        line_number = rdr->GetLineNumber();
        NCBITEST_CHECK_EQUAL(line_number, 3);
    }    
    CFile(filename).Remove();
}


BOOST_AUTO_TEST_CASE(GetLineNumberAfterTwicePlusPlus__2)
{
    vector<string> lines;
    lines.push_back("First line");
    lines.push_back("Second line");
    lines.push_back("Third line");
    vector<CT_POS_TYPE> positions;
    string filename = s_CreateTestFile(lines,
                                       positions);

    for ( int type = 0; type < 3; ++type ) {
        CRef<ILineReader> rdr;
        rdr = s_GetLineReader(filename, type);

        /* 1. Constructor
           2. Operator++, operator++
           3. GetLineNumber returns 2 */
        ++*rdr;
        ++*rdr;
        int line_number = rdr->GetLineNumber();
        NCBITEST_CHECK_EQUAL(line_number, 2);
    }    
    CFile(filename).Remove();
}


BOOST_AUTO_TEST_CASE(GetLineNumberAtEOF__LastNumber)
{
    vector<string> lines;
    lines.push_back("First line");
    lines.push_back("Second line");
    lines.push_back("Third line");
    vector<CT_POS_TYPE> positions;
    string filename = s_CreateTestFile(lines,
                                       positions);

    for ( int type = 0; type < 3; ++type ) {
        CRef<ILineReader> rdr;
        rdr = s_GetLineReader(filename, type);

        /* 1. Constructor
           2. Operator++, operator++, operator++, operator++
           3. GetLineNumber returns 2 */
        ++*rdr; //on the first line
        ++*rdr; //on the second line
        ++*rdr; //on the third line
        ++*rdr; //EOF
        int line_number = rdr->GetLineNumber();
        NCBITEST_CHECK_EQUAL(line_number, 3);
    }    
    CFile(filename).Remove();
}


BOOST_AUTO_TEST_CASE(GetLineNumberAfterPlusPlusAtEOF__LastNumber)
{
    vector<string> lines;
    lines.push_back("First line");
    lines.push_back("Second line");
    lines.push_back("Third line");
    vector<CT_POS_TYPE> positions;
    string filename = s_CreateTestFile(lines,
                                       positions);

    for ( int type = 0; type < 3; ++type ) {
        CRef<ILineReader> rdr;
        rdr = s_GetLineReader(filename, type);

        /* 1. Constructor
           2. Operator++, operator++, operator++, operator++, operator++
           3. GetLineNumber returns 2 */
        ++*rdr; //on the first line
        ++*rdr; //on the second line
        ++*rdr; //on the third line
        ++*rdr; //EOF
        ++*rdr; //EOF
        int line_number = rdr->GetLineNumber();
        NCBITEST_CHECK_EQUAL(line_number, 3);
    }    
    CFile(filename).Remove();
}
