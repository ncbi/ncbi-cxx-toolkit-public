#include <app/project_tree_builder/stl_msvc_usage.hpp>
#include <app/project_tree_builder/msvc_sln_generator.hpp>
#include <app/project_tree_builder/msvc_prj_utils.hpp>
#include <corelib/ncbifile.hpp>
#include <corelib/ncbistr.hpp>
#include <app/project_tree_builder/proj_builder_app.hpp>

//------------------------------------------------------------------------------

CMsvcSolutionGenerator::CMsvcSolutionGenerator(const list<string>& configs)
    :m_Configs(configs),
     m_HeaderLine("Microsoft Visual Studio Solution File, Format Version 8.00"),
     m_RootGUID("{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}")
{
}


CMsvcSolutionGenerator::~CMsvcSolutionGenerator()
{
}


void CMsvcSolutionGenerator::AddProject(const CProjItem& project)
{
    m_Projects[project.m_ID] = CPrjContext(project);
}


void CMsvcSolutionGenerator::SaveSolution(const string& file_path)
{
    CDirEntry::SplitPath(file_path, &m_SolutionDir);

    // Create dir for output sln file
    CDir(m_SolutionDir).CreatePath();

    CNcbiOfstream  ofs(file_path.c_str(), ios::out | ios::trunc);
    if(!ofs)
        NCBI_THROW(CProjBulderAppException, eFileCreation, file_path);

    //Start sln file
    ofs << m_HeaderLine << endl;

    ITERATE(TProjects, p, m_Projects) {
        
        WriteProjectAndSection(ofs, p->second);
    }

    //Start "Global" section
    ofs << "Global" << endl;
	
    //Write all configurations
    ofs << '\t' << "GlobalSection(SolutionConfiguration) = preSolution" << endl;
    ITERATE(list<string>, p, m_Configs)
    {
        ofs << '\t' << '\t' << *p << " = " << *p << endl;
    }
    ofs << '\t' << "EndGlobalSection" << endl;
    
    ofs << '\t' << "GlobalSection(ProjectConfiguration) = postSolution" << endl;
    ITERATE(TProjects, p, m_Projects) {
        
        WriteProjectConfigurations(ofs, p->second);
    }
    ofs << '\t' << "EndGlobalSection" << endl;

    //meanless stuff
    ofs << '\t' << "GlobalSection(ExtensibilityGlobals) = postSolution" << endl;
	ofs << '\t' << "EndGlobalSection" << endl;
	ofs << '\t' << "GlobalSection(ExtensibilityAddIns) = postSolution" << endl;
	ofs << '\t' << "EndGlobalSection" << endl;
   
    //End of global section
    ofs << "EndGlobal" << endl;
}


//------------------------------------------------------------------------------
CMsvcSolutionGenerator::CPrjContext::CPrjContext(void)
{
    Clear();
}


CMsvcSolutionGenerator::CPrjContext::CPrjContext(const CPrjContext& context)
{
    SetFrom(context);
}


CMsvcSolutionGenerator::CPrjContext::CPrjContext(const CProjItem& project)
    :m_Project(project)
{
    m_GUID = GenerateSlnGUID();

    CMsvcPrjProjectContext project_context(project);
    m_ProjectName = project_context.ProjectName();
    m_ProjectPath = CDirEntry::ConcatPath(project_context.ProjectDir(),
                                          project_context.ProjectName());
    m_ProjectPath += ".vcproj";
}


CMsvcSolutionGenerator::CPrjContext& 
    CMsvcSolutionGenerator::CPrjContext::operator = (const CPrjContext& context)
{
    if(this != &context)
    {
        Clear();
        SetFrom(context);
    }
    return *this;
}


CMsvcSolutionGenerator::CPrjContext::~CPrjContext(void)
{
    Clear();
}


void CMsvcSolutionGenerator::CPrjContext::Clear(void)
{
    //TODO
}


void CMsvcSolutionGenerator::CPrjContext::SetFrom(
                                             const CPrjContext& project_context)
{
    m_Project     = project_context.m_Project;

    m_GUID        = project_context.m_GUID;
    m_ProjectName = project_context.m_ProjectName;
    m_ProjectPath = project_context.m_ProjectPath;
}

//------------------------------------------------------------------------------
//Project("{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}") = "util_all", "util\util_all.vcproj", "{EE5BB51D-9D33-4CCB-8596-2530204A39D9}"
//	ProjectSection(ProjectDependencies) = postProject
//		{A2BD051C-15FC-4757-AF79-B6959C0289F7} = {A2BD051C-15FC-4757-AF79-B6959C0289F7}
//		{46056CE1-F74D-4592-A10E-D7B2B5FB64D7} = {46056CE1-F74D-4592-A10E-D7B2B5FB64D7}
//		{433B7D1E-AA60-4B2B-A2DA-E37EE180C87A} = {433B7D1E-AA60-4B2B-A2DA-E37EE180C87A}
//		{49B981D7-7B6A-46FA-9246-7BEC65BC296A} = {49B981D7-7B6A-46FA-9246-7BEC65BC296A}
//		{37B853D3-3A69-49A8-AF98-A41307C22138} = {37B853D3-3A69-49A8-AF98-A41307C22138}
//		{E2BA4286-8E01-4024-BC22-E1121640304A} = {E2BA4286-8E01-4024-BC22-E1121640304A}
//		{22E61D2C-859F-4E86-9C76-E7C723F0D24F} = {22E61D2C-859F-4E86-9C76-E7C723F0D24F}
//		{3BD708FD-A8CF-43B1-B8E8-3CFFC02105E4} = {3BD708FD-A8CF-43B1-B8E8-3CFFC02105E4}
//		{386DD7FC-D3B8-4903-8F02-4F7D206483FC} = {386DD7FC-D3B8-4903-8F02-4F7D206483FC}
//		{7231DE6A-900A-4D34-95B3-5D2EE17CCD1E} = {7231DE6A-900A-4D34-95B3-5D2EE17CCD1E}
//		{DDCC87D4-0146-4BC9-93CC-BB9E82CBFF6F} = {DDCC87D4-0146-4BC9-93CC-BB9E82CBFF6F}
//		{DC184280-5530-493D-824B-161D0CC13B0C} = {DC184280-5530-493D-824B-161D0CC13B0C}
//		{805839BC-C31D-4369-9155-F2716E054DFC} = {805839BC-C31D-4369-9155-F2716E054DFC}
//	EndProjectSection
//EndProject

void CMsvcSolutionGenerator::WriteProjectAndSection(CNcbiOfstream& ofs, 
                             const CMsvcSolutionGenerator::CPrjContext& project)
{
    ofs << "Project(\"" 
        << m_RootGUID 
        << "\") = \"" 
        << project.m_ProjectName 
        << "\", \"";

    ofs << CDirEntry::CreateRelativePath(m_SolutionDir, project.m_ProjectPath) 
        << "\", \"";

    ofs << project.m_GUID 
        << "\"" 
        << endl;

    ofs << '\t' << "ProjectSection(ProjectDependencies) = postProject" << endl;

    ITERATE(list<string>, p, project.m_Project.m_Depends) {

        const string& id = *p;

        TProjects::const_iterator n = m_Projects.find(id);
        if(n != m_Projects.end()) {

            const CPrjContext& prj_i = n->second;

            ofs << '\t' << '\t' << prj_i.m_GUID << " = " << prj_i.m_GUID << endl;
        }
        else {

            LOG_POST("&&&&&&& Project: " + 
                      project.m_ProjectName + " is dependend of " + id + 
                      ". But no such project");
        }
    }
    ofs << '\t' << "EndProjectSection" << endl;
    ofs << "EndProject" << endl;
}


//------------------------------------------------------------------------------

//		{08090CA6-5107-40DA-A6F3-91A5134774D7}.Debug.ActiveCfg = Debug|Win32
//		{08090CA6-5107-40DA-A6F3-91A5134774D7}.Debug.Build.0 = Debug|Win32
//		{08090CA6-5107-40DA-A6F3-91A5134774D7}.DebugDLL.ActiveCfg = DebugDLL|Win32
//		{08090CA6-5107-40DA-A6F3-91A5134774D7}.DebugDLL.Build.0 = DebugDLL|Win32
//		{08090CA6-5107-40DA-A6F3-91A5134774D7}.DebugMT.ActiveCfg = DebugMT|Win32
//		{08090CA6-5107-40DA-A6F3-91A5134774D7}.DebugMT.Build.0 = DebugMT|Win32
//		{08090CA6-5107-40DA-A6F3-91A5134774D7}.Release.ActiveCfg = Release|Win32
//		{08090CA6-5107-40DA-A6F3-91A5134774D7}.Release.Build.0 = Release|Win32
//		{08090CA6-5107-40DA-A6F3-91A5134774D7}.ReleaseDLL.ActiveCfg = ReleaseDLL|Win32
//		{08090CA6-5107-40DA-A6F3-91A5134774D7}.ReleaseDLL.Build.0 = ReleaseDLL|Win32
//		{08090CA6-5107-40DA-A6F3-91A5134774D7}.ReleaseMT.ActiveCfg = ReleaseMT|Win32
//		{08090CA6-5107-40DA-A6F3-91A5134774D7}.ReleaseMT.Build.0 = ReleaseMT|Win32

void CMsvcSolutionGenerator::WriteProjectConfigurations(CNcbiOfstream& ofs, 
                             const CMsvcSolutionGenerator::CPrjContext& project)
{
    ITERATE(list<string>, p, m_Configs) {

        const string& config = *p;
        ofs << '\t' 
            << '\t' 
            << project.m_GUID 
            << '.' 
            << config 
            << ".ActiveCfg = " 
            << config 
            << "|Win32" 
            << endl;

        ofs << '\t' 
            << '\t' 
            << project.m_GUID 
            << '.' 
            << config 
            << ".Build.0 = " 
            << config 
            << "|Win32" 
            << endl;
    }
}
//------------------------------------------------------------------------------
