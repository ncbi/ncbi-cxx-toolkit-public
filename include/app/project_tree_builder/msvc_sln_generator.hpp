#ifndef MSVC_SLN_GENERATOR_HEADER
#define MSVC_SLN_GENERATOR_HEADER

#include <app/project_tree_builder/msvc_project_context.hpp>

#include <corelib/ncbienv.hpp>
BEGIN_NCBI_SCOPE
//------------------------------------------------------------------------------

class CMsvcSolutionGenerator
{
public:
    CMsvcSolutionGenerator(const list<string>& configs);
    ~CMsvcSolutionGenerator();
    
    void AddProject(const CProjItem& project);
    
    void AddMasterProject(const string& base_name);

    void SaveSolution(const string& file_path);
    
private:
    list<string> m_Configs;

    string m_SolutionDir;

    /// Basename / GUID
    pair<string, string> m_MasterProject;
    bool IsSetMasterProject() const;

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
    
    void WriteMasterProject(CNcbiOfstream& ofs);

    void WriteProjectConfigurations(CNcbiOfstream& ofs, 
                            const CMsvcSolutionGenerator::CPrjContext& project);

    void WriteMasterProjectConfiguration(CNcbiOfstream& ofs);

    // Prohibited to:
    CMsvcSolutionGenerator(void);
    CMsvcSolutionGenerator(const CMsvcSolutionGenerator&);
    CMsvcSolutionGenerator& operator = (const CMsvcSolutionGenerator&);
};

//------------------------------------------------------------------------------
END_NCBI_SCOPE

#endif // MSVC_SLN_GENERATOR_HEADER