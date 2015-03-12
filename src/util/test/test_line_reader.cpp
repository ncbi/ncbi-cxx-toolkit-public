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
#include <util/random_gen.hpp>
#include <util/line_reader.hpp>

#include <common/test_assert.h>  /* This header must go last */

BEGIN_NCBI_SCOPE


class CTestApp : public CNcbiApplication
{
private:

    virtual void Init(void);
    virtual int  Run(void);
    virtual void Exit(void);

    int RunSelfTest();
    void RunFile(const string& filename);
    CRef<ILineReader> CreateLineReader(const string& filename);

    bool try_memory;
    bool use_stream;
    bool fast_scan;
};


void CTestApp::Init(void)
{
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "CArgDescriptions demo program");

    arg_desc->AddOptionalKey("in", "in", "Input FASTA filename",
                     CArgDescriptions::eInputFile);
    arg_desc->AddOptionalKey("ins", "ins", "Input FASTA filenames",
                     CArgDescriptions::eString);
    arg_desc->AddFlag("mem", "Use memory stream if possible");
    arg_desc->AddFlag("stream", "Use istream");
    arg_desc->AddFlag("fast", "Scan as fast as possible");
    arg_desc->AddFlag("selftest", "Run self checks");
    
    SetupArgDescriptions(arg_desc.release());
}


int CTestApp::Run(void)
{
    const CArgs& args (GetArgs());

    try_memory = args["mem"];
    use_stream = args["stream"];
    fast_scan = args["fast"];

    if ( args["selftest"] ) {
        return RunSelfTest();
    }

    if ( args["in"] ) {
        RunFile(args["in"].AsString());
    }
    if ( args["ins"] ) {
        list<string> ff;
        NStr::Split(args["ins"].AsString(), " ,", ff);
        ITERATE ( list<string>, it, ff ) {
            RunFile(*it);
        }
    }
    return 0;
}


CRef<ILineReader> CTestApp::CreateLineReader(const string& filename)
{
    CRef<ILineReader> line_reader;
    if ( try_memory ) {
        try {
            line_reader.Reset(new CMemoryLineReader(new CMemoryFile(filename),
                                                    eTakeOwnership));
        }
        catch (...) { // fall back to streams
        }
    }
    if ( !line_reader ) {
        if ( use_stream ) {
            if ( filename == "-" ) {
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


void CTestApp::RunFile(const string& filename)
{
    NcbiCout << "Processing file: "<< filename << NcbiEndl;
    CRef<ILineReader> line_reader = CreateLineReader(filename);
    
    if ( fast_scan ) {
        int lines = 0, chars = 0;
        while ( !line_reader->AtEOF() ) {
            ++lines;
            chars += (*++*line_reader).size();
        }
        NcbiCout << "Lines: " << lines << NcbiEndl;
        NcbiCout << "Chars: " << chars << NcbiEndl;
    }
    else {
        int lines = 0, chars = 0, sum = 0;
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
        NcbiCout << "  Sum: " << sum << NcbiEndl;
    }
}


int CTestApp::RunSelfTest()
{
    CRandom r;
    int errors = 0;
    vector<string> lines;
    vector<CT_POS_TYPE> positions;
    for ( int i = 0; i < 100000; ++i ) {
        int size;
        if ( r.GetRand(0, 100) == 0 ) {
            size = r.GetRand(0, 100000);
        }
        else {
            size = r.GetRand(0, 10);
        }
        string line(size, ' ');
        for ( int j = 0; j < size; ++j ) {
            line[j] = r.GetRand(' ', 126);
        }
        lines.push_back(line);
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
        for ( int type = 0; type < 3; ++type ) {
            CRef<ILineReader> rdr;
            switch ( type ) {
            case 0: 
                rdr = new CMemoryLineReader(new CMemoryFile(filename),
                                            eTakeOwnership);
                break;
            case 1:
                rdr =new CStreamLineReader(*new CNcbiIfstream(filename.c_str(),
                                                              ios::binary),
                                                              CStreamLineReader::eEOL_mixed,
                                           eTakeOwnership);
                break;
            case 2:
                rdr = CBufferedLineReader::New(filename);
                break;
            }
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
                             " vs "<<
                             NcbiStreamposToInt8(positions[l]));
                    ++errors;
                    break;
                }
                _ASSERT(s == lines[l]);
                _ASSERT(rdr->GetPosition() == positions[l+1]);
                ++l;
            }
        }
    }
    catch ( ... ) {
        CFile(filename).Remove();
        throw;
    }
    CFile(filename).Remove();
    return errors;
}


void CTestApp::Exit(void)
{
    SetDiagStream(0);
}


END_NCBI_SCOPE


USING_NCBI_SCOPE;

int main(int argc, const char* argv[])
{
    return CTestApp().AppMain(argc, argv);
}
