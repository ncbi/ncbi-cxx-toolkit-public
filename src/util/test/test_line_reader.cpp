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

    void RunFile(const string& filename, bool try_memory);
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
    
    SetupArgDescriptions(arg_desc.release());
}


int CTestApp::Run(void)
{
    const CArgs& args (GetArgs());

    bool try_memory = args["mem"];
    if ( args["in"] ) {
        RunFile(args["in"].AsString(), try_memory);
    }
    if ( args["ins"] ) {
        list<string> ff;
        NStr::Split(args["ins"].AsString(), " ,", ff);
        ITERATE ( list<string>, it, ff ) {
            RunFile(*it, try_memory);
        }
    }
    return 0;
}

void CTestApp::RunFile(const string& filename, bool try_memory)
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
        line_reader = ILineReader::New(filename);
    }
    
    int lines = 0, chars = 0, sum = 0;
    while ( !line_reader->AtEOF() ) {
        CTempString s = *++*line_reader;
        ++lines;
        chars += s.size();
        for ( size_t i = 0, l = s.size(); i < l; ++i ) {
            sum += s[i];
        }
    }
    NcbiCout << "Lines: " << lines << NcbiEndl;
    NcbiCout << "Chars: " << chars << NcbiEndl;
    NcbiCout << "  Sum: " << sum << NcbiEndl;
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
