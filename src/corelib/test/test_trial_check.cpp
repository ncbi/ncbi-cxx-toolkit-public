#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbifile.hpp>

USING_NCBI_SCOPE;

class CTestApplication : public CNcbiApplication
{
public:
    virtual int Run(void);

    bool GoodProgram(const string& check_program_name) const;
};


int CTestApplication::Run(void)
{
    bool ok = true;
    if ( !GoodProgram("test_trial") ) {
        ok = false;
    }
    if ( GoodProgram("test_trial_fail") ) {
        ok = false;
    }
    NcbiCout << (ok? "Passed.": "Failed!") << NcbiEndl;
    return ok? 0: 1;
}


bool CTestApplication::GoodProgram(const string& check_program_name) const
{
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
    NcbiCout << "Looking for program: " << path << NcbiEndl;
    if ( CDirEntry(path).Exists() ) {
        NcbiCout << "Program file found." << NcbiEndl;
        return true;
    }
    else {
        NcbiCout << "Cannot find the program." << NcbiEndl;
        return false;
    }
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN

int main(int argc, const char* argv[]) 
{
    return CTestApplication().AppMain(argc, argv);
}
