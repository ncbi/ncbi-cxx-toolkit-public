#include <app/project_tree_builder/proj_builder_app.hpp>
#include <app/project_tree_builder/proj_item.hpp>
#include <app/project_tree_builder/msvc_prj_utils.hpp>
#include <app/project_tree_builder/msvc_prj_generator.hpp>
#include <app/project_tree_builder/msvc_sln_generator.hpp>

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

    //int o1 = EnumOpt("CompileAsOptions", "CompileAsCPlusPlus");

    // Get all makefiles (Do not collect makefile from root directory):
    ProcessDir(m_SubTree, m_SubTree == m_RootSrc);
#if 0
    DumpFiles(m_FilesMakeIn,	"files_makein_dump.log");
    DumpFiles(m_FilesMakeLib,	"files_makelib_dump.log");
    DumpFiles(m_FilesMakeApp,	"files_makeapp_dump.log");
#endif

#if 0
    CSymResolver resolver(CDirEntry::ConcatPath(m_RootSrc, "Makefile.mk.in"));

    //
    list<string> names;
    resolver.Resolve("$(SEQ_LIBS)", &names);

    list<string> names1;
    resolver.Resolve("$(OBJMGR_LIBS)", &names1);
    ResolveDefs(resolver);
#endif

    // Resolve macrodefines
    list<string> metadata_files;
    GetMetaDataFiles(&metadata_files);
    ITERATE(list<string>, p, metadata_files) {
	    CSymResolver resolver(CDirEntry::ConcatPath(m_RootSrc, *p));
	    ResolveDefs(resolver);
    }

#if 0
    DumpFiles(m_FilesMakeApp,	"files_makeapp_resolved_dump.log");
#endif

    // Build projects tree
    CProjectItemsTree projects_tree;
    CProjectItemsTree::CreateFrom(m_FilesMakeIn, 
                                  m_FilesMakeLib, 
                                  m_FilesMakeApp, &projects_tree);
#if 0
    //test for project model
    {{
        auto_ptr<CVisualStudioProject> prj(LoadFromXmlFile("E:\\xblast.vcproj.txt.xml"));
        SaveToXmlFile("E:\\xblast.vcproj.txt.xml.out", *prj.get() );
    }}
#endif

#if 0
    //test for GUID generator
    {{
        string guid   = GenerateSlnGUID();
        string guid1  = GenerateSlnGUID();

        string guid2  = GenerateSlnGUID();
        string guid3  = GenerateSlnGUID();
        string guid4  = GenerateSlnGUID();
        string guid5  = GenerateSlnGUID();
        string guid6  = GenerateSlnGUID();
        string guid7  = GenerateSlnGUID();
        string guid8  = GenerateSlnGUID();
        string guid9  = GenerateSlnGUID();
        string guid10 = GenerateSlnGUID();
        string guid11 = GenerateSlnGUID();
        string guid12 = GenerateSlnGUID();
    }}
#endif

    list<string> configs;
    GetBuildConfigs(&configs);
    CMsvcProjectGenerator prj_gen(configs);

    ITERATE(CProjectItemsTree::TProjects, p, projects_tree.m_Projects) {

        prj_gen.Generate(p->second);
    }

#if 1
    CMsvcSolutionGenerator sln_gen(configs);
    ITERATE(CProjectItemsTree::TProjects, p, projects_tree.m_Projects) {

        sln_gen.AddProject(p->second);
    }
    sln_gen.SaveSolution(m_Solution);
#endif
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
static bool s_IsMakeInFile(const string& name)
{
    return name == "Makefile.in";
}
static bool s_IsMakeLibFile(const string& name)
{
    return NStr::StartsWith(name, "Makefile")  &&  
	       NStr::EndsWith(name, ".lib");
}
static bool s_IsMakeAppFile(const string& name)
{
    return NStr::StartsWith(name, "Makefile")  &&  
	       NStr::EndsWith(name, ".app");
}
//------------------------------------------------------------------------------


void CProjBulderApp::ProcessDir(const string& dir_name, bool is_root)
{
    // Do not collect makefile from root directory
    CDir dir(dir_name);
    CDir::TEntries contents = dir.GetEntries("*");
    ITERATE(CDir::TEntries, i, contents) {
        string name  = (*i)->GetName();
        if ( name == "."  ||  name == ".."  ||  
             name == string(1,CDir::GetPathSeparator()) ) {
            continue;
        }
        string path = (*i)->GetPath();

        if ( (*i)->IsFile()  &&  !is_root) {
            if ( s_IsMakeInFile(name) )
	            ProcessMakeInFile(path);
            else if ( s_IsMakeLibFile(name) )
	            ProcessMakeLibFile(path);
            else if ( s_IsMakeAppFile(name) )
	            ProcessMakeAppFile(path);
        } 
        else if ( (*i)->IsDir() ) {
            ProcessDir(path, false);
        }
    }
}


void CProjBulderApp::ProcessMakeInFile(const string& file_name)
{
    LOG_POST("Processing MakeIn: " + file_name);

    CSimpleMakeFileContents fc(file_name);
    if ( !fc.m_Contents.empty() )
	    m_FilesMakeIn[file_name] = fc;
}


void CProjBulderApp::ProcessMakeLibFile(const string& file_name)
{
    LOG_POST("Processing MakeLib: " + file_name);

    CSimpleMakeFileContents fc(file_name);
    if ( !fc.m_Contents.empty() )
	    m_FilesMakeLib[file_name] = fc;
}


void CProjBulderApp::ProcessMakeAppFile(const string& file_name)
{
    LOG_POST("Processing MakeApp: " + file_name);

    CSimpleMakeFileContents fc(file_name);
    if ( !fc.m_Contents.empty() )
	    m_FilesMakeApp[file_name] = fc;
}


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
    if (!ofs) {
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


void CProjBulderApp::GetBuildConfigs(list<string> * pConfigs)
{
    pConfigs->clear();
    string config_str = GetConfig().GetString("Common", "Configurations", "");
    NStr::Split(config_str, " \t,", *pConfigs);
}


void CProjBulderApp::ResolveDefs(CSymResolver& resolver)
{
    NON_CONST_ITERATE(TFiles, p, m_FilesMakeApp) {
	    NON_CONST_ITERATE(CSimpleMakeFileContents::TContents, 
                          n, 
                          p->second.m_Contents) {
		    if (n->first == "LIB") {
                list<string> new_vals;
                bool modified = false;
                NON_CONST_ITERATE(list<string>, k, n->second) {

	                list<string> resolved_def;
	                resolver.Resolve(*k, &resolved_def);
	                if(resolved_def.empty())
		                new_vals.push_back(*k);
                    else {

		                copy(resolved_def.begin(), 
			                 resolved_def.end(), 
			                 back_inserter(new_vals));
		                modified = true;
                    }
                }
                if(modified)
                    n->second = new_vals;
		    }
        }
    }
}


CProjBulderApp& GetApp()
{
    static CProjBulderApp theApp;
    return theApp;
}


int main(int argc, const char* argv[])
{
    // Execute main application function
    return GetApp().AppMain(argc, argv, 0, eDS_Default, 0);
}
