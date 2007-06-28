#include <ncbi_pch.hpp>

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbifile.hpp>
#include <corelib/rwstream.hpp>
#include <util/line_reader.hpp>


BEGIN_NCBI_SCOPE


class CTestApp : public CNcbiApplication
{
private:

    virtual void Init(void);
    virtual int  Run(void);
    virtual void Exit(void);

    void RunFile(const string& filename);

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
    
    SetupArgDescriptions(arg_desc.release());
}


int CTestApp::Run(void)
{
    const CArgs& args (GetArgs());

    try_memory = args["mem"];
    use_stream = args["stream"];
    fast_scan = args["fast"];

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

void CTestApp::RunFile(const string& filename)
{
    NcbiCout << "Processing file: "<< filename << NcbiEndl;
    auto_ptr<IReader> reader;
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
                    new CStreamLineReader(*new CNcbiIfstream(filename.c_str(), ios::binary),
                                          eTakeOwnership);
            }
        }
        else {
            line_reader = ILineReader::New(filename);
        }
    }
    
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
