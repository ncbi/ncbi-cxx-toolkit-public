#ifndef MSVC_SLN_GENERATOR_HEADER
#define MSVC_SLN_GENERATOR_HEADER

#include <app/project_tree_builder/msvc_project_context.hpp>

#include <corelib/ncbienv.hpp>
USING_NCBI_SCOPE;

//------------------------------------------------------------------------------

class CMsvcSolutionGenerator
{
public:
    CMsvcSolutionGenerator(const list<string>& configs);
    ~CMsvcSolutionGenerator();
    
    void AddProject(const CProjItem& project);

    void SaveSolution(const string& file_path);
    
private:
    list<string> m_Configs;

    string m_SolutionDir;

    // File tags:
    const string m_HeaderLine;//"Microsoft Visual Studio Solution File, Format Version 8.00"
    const string m_RootGUID; //"{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}"

    class CPrjContext
    {
    public:

        CPrjContext(void);
        CPrjContext(const CPrjContext& context);
        CPrjContext(const CProjItem& project);
        CPrjContext& operator = (const CPrjContext& context);
        ~CPrjContext(void);

        CProjItem m_Project;

        string    m_GUID;
        string    m_ProjectName;
        string    m_ProjectPath;
    
    private:
        void Clear(void);
        void SetFrom(const CPrjContext& project_context);
    };

    typedef map<string, CPrjContext> TProjects;
    TProjects m_Projects;

    //Writers:
    void WriteProjectAndSection(CNcbiOfstream& ofs, 
                            const CMsvcSolutionGenerator::CPrjContext& project);
    void WriteProjectConfigurations(CNcbiOfstream& ofs, 
                            const CMsvcSolutionGenerator::CPrjContext& project);

    // Prohibited to:
    CMsvcSolutionGenerator(void);
    CMsvcSolutionGenerator(const CMsvcSolutionGenerator&);
    CMsvcSolutionGenerator& operator = (const CMsvcSolutionGenerator&);
};

//------------------------------------------------------------------------------

#endif // MSVC_SLN_GENERATOR_HEADER