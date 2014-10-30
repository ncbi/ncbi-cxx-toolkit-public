#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbifile.hpp>

USING_NCBI_SCOPE;

class CTestApplication : public CNcbiApplication
{
public:
    virtual int Run(void);
};


int CTestApplication::Run(void)
{
    const string check_program_name = "test_trial";
    string path = GetProgramExecutablePath();
    if ( path.empty() ) {
#if defined(NCBI_OS_MSWIN)
        path = CDirEntry::MakePath("", check_program_name, ".exe");
#else
        path = check_program_name;
#endif
    }
    else {
        string dir, base, ext;
        CDirEntry::SplitPath(path, &dir, &base, &ext);
        path = CDirEntry::MakePath(dir, check_program_name, ext);
    }
    NcbiCout << "Checking existence of program: " << path << NcbiEndl;
    if ( CDirEntry(path).Exists() ) {
        NcbiCout << "Program file found." << NcbiEndl;
        NcbiCout << "Passed" << NcbiEndl;
        return 0;
    }
    else {
        NcbiCout << "Cannot find the program!" << NcbiEndl;
        NcbiCout << "Failed" << NcbiEndl;
        return 1;
    }
}

/////////////////////////////////////////////////////////////////////////////
//  MAIN

int main(int argc, const char* argv[]) 
{
    return CTestApplication().AppMain(argc, argv);
}
