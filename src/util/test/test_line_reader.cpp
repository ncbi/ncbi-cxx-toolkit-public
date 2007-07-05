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
        for ( int i = 0; i < size; ++i ) {
            line[i] = r.GetRand(' ', 126);
        }
        lines.push_back(line);
    }
    string filename = CFile::GetTmpName(CFile::eTmpFileCreate);
    try {
        {{
            CNcbiOfstream out(filename.c_str(), ios::binary);
            ITERATE ( vector<string>, i, lines ) {
                positions.push_back(out.tellp());
                out << *i;
                if ( r.GetRand()&1 ) out << '\r';
                out << '\n';
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
                _ASSERT(rdr->GetPosition() == positions[l+1]);
                if ( !(s == lines[l]) ) {
                    ERR_POST("ILineReader type "<<type<<" at "<<l<<
                             " \""<<s<<"\" != \""<<lines[l]<<"\"");
                    ++errors;
                    break;
                }
                _ASSERT(s == lines[l]);
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
    return CTestApp().AppMain(argc, argv, 0, eDS_Default, 0);
}
