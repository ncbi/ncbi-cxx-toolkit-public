#include <app/project_tree_builder/proj_builder_app.hpp>
#include <app/project_tree_builder/proj_item.hpp>
#include <app/project_tree_builder/msvc_prj_utils.hpp>
#include <app/project_tree_builder/msvc_prj_generator.hpp>
#include <app/project_tree_builder/msvc_sln_generator.hpp>
#include <app/project_tree_builder/msvc_masterproject_generator.hpp>
#include <app/project_tree_builder/proj_utils.hpp>


BEGIN_NCBI_SCOPE
//------------------------------------------------------------------------------


CProjBulderApp::CProjBulderApp(void)
{
}


void CProjBulderApp::Init(void)
{
    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "MSVC 7.10 projects builder application");

    // Programm arguments:

    arg_desc->AddPositional("root",
          "Root directory of the build tree. This directory ends with \"c++\".",
          CArgDescriptions::eString);

    arg_desc->AddPositional("subtree",
                     "Subtree to build. Example: src/corelib/ .",
                     CArgDescriptions::eString);

    arg_desc->AddPositional("solution", "MSVC Solution to buld.",
						    CArgDescriptions::eString);


    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}


int CProjBulderApp::Run(void)
{
    LOG_POST("Started.");
    // Get and check arguments
    ParseArguments();

	// Set error posting and tracing on maximum.
	SetDiagTrace(eDT_Enable);
	SetDiagPostFlag(eDPF_Default);
	SetDiagPostLevel(eDiag_Info);


    // Build projects tree
    CProjectItemsTree projects_tree(m_RootSrc);
    CProjectTreeBuilder::BuildProjectTree(m_SubTree, m_RootSrc, &projects_tree);

    // Get configurations
    list<string> configs;
    GetBuildConfigs(&configs);
    
    // Projects
    CMsvcProjectGenerator prj_gen(configs);

    ITERATE(CProjectItemsTree::TProjects, p, projects_tree.m_Projects) {
        prj_gen.Generate(p->second);
    }

    // MasterProject
    CMsvcMasterProjectGenerator master_prj_gen(projects_tree,
                                               configs,
                                               CDirEntry(m_Solution).GetDir());
    master_prj_gen.SaveProject("_MasterProject");

    // Solution
    CMsvcSolutionGenerator sln_gen(configs);
    ITERATE(CProjectItemsTree::TProjects, p, projects_tree.m_Projects) {
        sln_gen.AddProject(p->second);
    }
    sln_gen.AddMasterProject("_MasterProject");
    sln_gen.SaveSolution(m_Solution);

    //
    LOG_POST("Finished.");
    return 0;
}

void CProjBulderApp::Exit(void)
{
	//TODO
}


void CProjBulderApp::ParseArguments(void)
{
    CArgs args = GetArgs();

    // Root
    m_Root     = CDirEntry::AddTrailingPathSeparator(args["root"].AsString());
    m_RootSrc  = CDirEntry::AddTrailingPathSeparator( 
                                    CDirEntry::ConcatPath(m_Root, "src"));

    // Subtree to build
    string subtree = args["subtree"].AsString();
    m_SubTree  = CDirEntry::ConcatPath(m_Root, subtree);

    // Solution
    m_Solution = args["solution"].AsString();
}


//------------------------------------------------------------------------------

int CProjBulderApp::EnumOpt(const string& enum_name, 
                            const string& enum_val) const
{
    int opt = GetConfig().GetInt(enum_name, enum_val, -1);
    if (opt == -1) {
	    NCBI_THROW(CProjBulderAppException, eEnumValue, 
                                enum_name + "::" + enum_val);
    }
    return opt;
}


void CProjBulderApp::DumpFiles(const TFiles& files, 
							   const string& filename) const
{
    CNcbiOfstream  ofs(filename.c_str(), ios::out | ios::trunc);
    if ( !ofs ) {
	    NCBI_THROW(CProjBulderAppException, eFileCreation, filename);
    }

    ITERATE(TFiles, p, files) {
	    ofs << "+++++++++++++++++++++++++\n";
	    ofs << p->first << endl;
	    p->second.Dump(ofs);
	    ofs << "-------------------------\n";
    }
}


void CProjBulderApp::GetMetaDataFiles(list<string> * pFiles) const
{
    pFiles->clear();
    string files_str = GetConfig().GetString("Common", "MetaData", "");
    NStr::Split(files_str, " \t,", *pFiles);
}


void CProjBulderApp::GetBuildConfigs(list<string> * pConfigs) const
{
    pConfigs->clear();
    string config_str = GetConfig().GetString("msvc7", "Configurations", "");
    NStr::Split(config_str, " \t,", *pConfigs);
}

#if 0
void CProjBulderApp::GetAdditionalPossibleIncludeDirs(list<string> * 
                                                            pIncludeDirs) const
{
    pIncludeDirs->clear();
    string include_str = GetConfig().GetString("Common", 
                                               "AdditionalIncludeDirs", "");
    NStr::Split(include_str, " \t,", *pIncludeDirs);
}


bool CProjBulderApp::GetRelaxedIncludeDirs(void) const
{
    return GetConfig().GetBool("Common", "RelaxedIncludeDirs", false);
}
#endif


CProjBulderApp& GetApp()
{
    static CProjBulderApp theApp;
    return theApp;
}

//------------------------------------------------------------------------------
END_NCBI_SCOPE

USING_NCBI_SCOPE;

int main(int argc, const char* argv[])
{
    // Execute main application function
    return GetApp().AppMain(argc, argv, 0, eDS_Default, 0);
}
//------------------------------------------------------------------------------


