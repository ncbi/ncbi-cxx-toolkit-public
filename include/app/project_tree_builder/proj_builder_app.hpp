#ifndef PROJ_BUILDER_APP_HEADER
#define PROJ_BUILDER_APP_HEADER

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbifile.hpp>
#include <app/project_tree_builder/file_contents.hpp>
#include <app/project_tree_builder/resolver.hpp>
#include <app/project_tree_builder/proj_utils.hpp>


BEGIN_NCBI_SCOPE
//------------------------------------------------------------------------------

class CProjBulderApp : public CNcbiApplication
{
public:
    

    /// ShortCut for enums
    int EnumOpt(const string& enum_name, const string& enum_val) const;

    /// Singleton
    friend CProjBulderApp& GetApp();


private:
    CProjBulderApp(void);

    virtual void Init(void);
    virtual int  Run (void);
    virtual void Exit(void);

    /// Parse program arguments.
    void ParseArguments(void);



    /// Root dir of building tree.
    string m_Root;

    /// src child dir of Root.
    string m_RootSrc;

    /// Subtree to buil (default is m_RootSrc).
    string m_SubTree;

    /// Solution to build.
    string m_Solution;


    //TFiles m_FilesMakeIn;
    //TFiles m_FilesMakeLib;
    //TFiles m_FilesMakeApp;

    typedef map<string, CSimpleMakeFileContents> TFiles;
    void DumpFiles(const TFiles& files, const string& filename) const;

public:
    void GetMetaDataFiles(list<string> * pFiles) const;

    void GetBuildConfigs(list<string> * pConfigs);
};


// access to App singleton
CProjBulderApp& GetApp();


//------------------------------------------------------------------------------

class CProjBulderAppException : public CException
{
public:
    enum EErrCode {
        eFileCreation,
        eEnumValue,
        eFileOpen,
        eProjectType,
        eBuildConfiguration
    };
    virtual const char* GetErrCodeString(void) const {
        switch ( GetErrCode() ) {
        case eFileCreation:
            return "Can not create file";
        case eEnumValue:
            return "Bad or missing enum value";
        case eFileOpen:
            return "Can not open file";
        case eProjectType:
            return "Unknown project type";
        case eBuildConfiguration:
            return "Unknown build configuration";
        default:
            return CException::GetErrCodeString();
        }
    }
    NCBI_EXCEPTION_DEFAULT(CProjBulderAppException, CException);
};

//------------------------------------------------------------------------------
END_NCBI_SCOPE

#endif
