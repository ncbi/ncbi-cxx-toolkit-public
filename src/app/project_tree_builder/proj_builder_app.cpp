/* $Id$
 * ===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *               National Center for Biotechnology Information
 *
 *  This software/database is a "United States Government Work" under the
 *  terms of the United States Copyright Act.  It was written as part of
 *  the author's official duties as a United States Government employee and
 *  thus cannot be copyrighted.  This software/database is freely available
 *  to the public for use. The National Library of Medicine and the U.S.
 *  Government have not placed any restriction on its use or reproduction.
 *
 *  Although all reasonable efforts have been taken to ensure the accuracy
 *  and reliability of the software and data, the NLM and the U.S.
 *  Government do not and cannot warrant the performance or results that
 *  may be obtained by using this software or data. The NLM and the U.S.
 *  Government disclaim all warranties, express or implied, including
 *  warranties of performance, merchantability or fitness for any particular
 *  purpose.
 *
 *  Please cite the author in any work or product based on this material.
 *
 * ===========================================================================
 *
 * Author:  Viatcheslav Gorelenkov
 *
 */

#include <ncbi_pch.hpp>
#include <app/project_tree_builder/proj_builder_app.hpp>
#include <app/project_tree_builder/proj_item.hpp>
#include <app/project_tree_builder/proj_tree_builder.hpp>
#include <app/project_tree_builder/msvc_prj_utils.hpp>
#include <app/project_tree_builder/msvc_prj_generator.hpp>
#include <app/project_tree_builder/msvc_sln_generator.hpp>
#include <app/project_tree_builder/msvc_masterproject_generator.hpp>
#include <app/project_tree_builder/proj_utils.hpp>
#include <app/project_tree_builder/msvc_configure.hpp>
#include <app/project_tree_builder/msvc_prj_defines.hpp>
#include <app/project_tree_builder/msvc_configure_prj_generator.hpp>
#include <app/project_tree_builder/proj_projects.hpp>
#include <corelib/ncbitime.hpp>

BEGIN_NCBI_SCOPE

#ifdef COMBINED_EXCLUDE
struct PIsExcludedByProjectMakefile
{
    typedef CProjectItemsTree::TProjects::value_type TValueType;
    bool operator() (const TValueType& item) const
    {
        const CProjItem& project = item.second;
        CMsvcPrjProjectContext prj_context(project);
        const list<string> implicit_exclude_dirs = 
            GetApp().GetProjectTreeInfo().m_ImplicitExcludedAbsDirs;
        ITERATE(list<string>, p, implicit_exclude_dirs) {
            const string& dir = *p;
            if ( IsSubdir(dir, project.m_SourcesBaseDir) ) {
                // implicitly excluded from build
                return prj_context.GetMsvcProjectMakefile().IsExcludeProject
                                                                        (true);
            }
        }
        // implicitly included to build
        return prj_context.GetMsvcProjectMakefile().IsExcludeProject(false);
    }
};


struct PIsExcludedMakefileIn
{
    typedef CProjectItemsTree::TProjects::value_type TValueType;

    PIsExcludedMakefileIn(const string& root_src_dir)
        :m_RootSrcDir(CDirEntry::NormalizePath(root_src_dir))
    {
        ProcessDir(root_src_dir);
    }

    bool operator() (const TValueType& item) const
    {
        const CProjItem& project = item.second;

        const list<string> implicit_exclude_dirs = 
            GetApp().GetProjectTreeInfo().m_ImplicitExcludedAbsDirs;
        ITERATE(list<string>, p, implicit_exclude_dirs) {
            const string& dir = *p;
            if ( IsSubdir(dir, project.m_SourcesBaseDir) ) {
                // implicitly excluded from build
                return !IsExcplicitlyIncluded(project.m_SourcesBaseDir);
            }
        }
        return false;
    }

private:
    string m_RootSrcDir;

    typedef map<string, AutoPtr<CNcbiRegistry> > TMakefiles;
    TMakefiles m_Makefiles;

    void ProcessDir(const string& dir_name)
    {
        CDir dir(dir_name);
        CDir::TEntries contents = dir.GetEntries("*");
        ITERATE(CDir::TEntries, i, contents) {
            string name  = (*i)->GetName();
            if ( name == "."  ||  name == ".."  ||  
                 name == string(1,CDir::GetPathSeparator()) ) {
                continue;
            }
            string path = (*i)->GetPath();

            if ( (*i)->IsFile()        &&
                name          == "Makefile.in.msvc" ) {
                m_Makefiles[path] = 
                    AutoPtr<CNcbiRegistry>
                         (new CNcbiRegistry(CNcbiIfstream(path.c_str(), 
                                            IOS_BASE::in | IOS_BASE::binary)));
            } 
            else if ( (*i)->IsDir() ) {

                ProcessDir(path);
            }
        }
    }

    bool IsExcplicitlyIncluded(const string& project_base_dir) const
    {
        string dir = project_base_dir;
        for(;;) {

            if (dir == m_RootSrcDir) 
                return false;
            string path = CDirEntry::ConcatPath(dir, "Makefile.in.msvc");
            TMakefiles::const_iterator p = 
                m_Makefiles.find(path);
            if ( p != m_Makefiles.end() ) {
                string val = 
                    (p->second)->GetString("Common", "ExcludeProject", "");
                if (val == "FALSE")
                    return true;
            }

            dir = CDirEntry::ConcatPath(dir, "..");
            dir = CDirEntry::NormalizePath(dir);
        }

        return false;
    }
};


template <class T1, class T2, class V> class CCombine
{
public:
    CCombine(const T1& t1, const T2& t2)
        :m_T1(t1), m_T2(t2)
    {
    }
    bool operator() (const V& v) const
    {
        return m_T1(v)  &&  m_T2(v);
    }
private:
    const T1 m_T1;
    const T2 m_T2;
};
#else
// not def COMBINED_EXCLUDE
struct PIsExcludedByProjectMakefile
{
    typedef CProjectItemsTree::TProjects::value_type TValueType;
    bool operator() (const TValueType& item) const
    {
        const CProjItem& project = item.second;
        CMsvcPrjProjectContext prj_context(project);
        const list<string> implicit_exclude_dirs = 
            GetApp().GetProjectTreeInfo().m_ImplicitExcludedAbsDirs;
        ITERATE(list<string>, p, implicit_exclude_dirs) {
            const string& dir = *p;
            if ( IsSubdir(dir, project.m_SourcesBaseDir) ) {
                // implicitly excluded from build
                if (prj_context.GetMsvcProjectMakefile().IsExcludeProject(true)) {
                    LOG_POST(Warning << "Excluded:  project " << project.m_Name
                                << " by ProjectTree/ImplicitExclude");
                    return true;
                }
                return false;
            }
        }
        // implicitly included into build
        if (prj_context.GetMsvcProjectMakefile().IsExcludeProject(false)) {
            LOG_POST(Warning << "Excluded:  project " << project.m_Name
                        << " by Makefile." << project.m_Name << ".*.msvc");
            return true;
        }
        return false;
    }
};

#endif

struct PIsExcludedByRequires
{
    typedef CProjectItemsTree::TProjects::value_type TValueType;
    bool operator() (const TValueType& item) const
    {
        const CProjItem& project = item.second;
        string unmet;
        if ( CMsvcPrjProjectContext::IsRequiresOk(project, &unmet) ) {
            return false;
        }
        LOG_POST(Warning << "Excluded:  " << project.m_Name
                      << "    project requires    " << unmet);
        return true;
    }
};


//-----------------------------------------------------------------------------
CProjBulderApp::CProjBulderApp(void)
{
    m_ScanningWholeTree = false;
    m_Dll = false;
    m_AddMissingLibs = false;
    m_ScanWholeTree  = true;
    m_CurrentBuildTree = 0;
    m_ConfirmCfg = false;
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
                            "Root directory of the build tree. "\
                                "This directory ends with \"c++\".",
                            CArgDescriptions::eString);

    arg_desc->AddPositional("subtree",
                            "Subtree to build. Example: src/corelib/ .",
                            CArgDescriptions::eString);

    arg_desc->AddPositional("solution", 
                            "MSVC Solution to build.",
						    CArgDescriptions::eString);

    arg_desc->AddFlag      ("dll", 
                            "Dll(s) will be built instead of static libraries.",
						    true);

    arg_desc->AddFlag      ("nobuildptb", 
                            "Exclude \"build PTB\" step from CONFIGURE project.");

    arg_desc->AddFlag      ("ext", 
                            "Use external libraries instead of missing in-tree ones.");
    arg_desc->AddFlag      ("nws", 
                            "Do not scan the whole source tree for missing projects.");
    arg_desc->AddOptionalKey("extroot", "external_build_root",
                             "Subtree in which to look for external libraries.",
                             CArgDescriptions::eString);

    arg_desc->AddOptionalKey("projtag", "project_tag",
                             "Include projects that have this tags only.",
                             CArgDescriptions::eString);
    arg_desc->AddFlag      ("cfg", 
                            "Show GUI to confirm configuration parameters (MS Windows only).");
    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}


static 
void s_ReportDependenciesStatus(const CCyclicDepends::TDependsCycles& cycles)
{
    const CMsvcDllsInfo& dlls = GetApp().GetDllsInfo();
    bool reported = false;
    ITERATE(CCyclicDepends::TDependsCycles, p, cycles) {
        const CCyclicDepends::TDependsChain& cycle = *p;
        bool real_cycle = false;
        string host0, host;
        ITERATE(CCyclicDepends::TDependsChain, m, cycle) {
            host = dlls.GetDllHost(m->Id());
            if (m == cycle.begin()) {
                host0 = host;
            } else {
                real_cycle = (host0 != host) || (host0.empty() && host.empty());
                if (real_cycle) {
                    break;
                }
            }
        }
        if (!real_cycle) {
            continue;
        }
        string str_chain("Dependency cycle found: ");
        ITERATE(CCyclicDepends::TDependsChain, n, cycle) {
            const CProjKey& proj_id = *n;
            if (n != cycle.begin()) {
                str_chain += " - ";
            }
            str_chain += proj_id.Id();
        }
        LOG_POST(Error << str_chain);
        reported = true;
    }
    if (!reported) {
        LOG_POST(Info << "No dependency cycles found.");
    }
}

int CProjBulderApp::Run(void)
{
	// Set error posting and tracing on maximum.
	SetDiagTrace(eDT_Enable);
	SetDiagPostFlag(eDPF_Default);
	SetDiagPostLevel(eDiag_Info);

    // Start 
    LOG_POST(Info << "Started at " + CTime(CTime::eCurrent).AsString());

    // Get and check arguments
    ParseArguments();
    if (m_ConfirmCfg && !ConfirmConfiguration())
    {
        LOG_POST(Info << "Cancelled by request.");
        return 1;
    }
    VerifyArguments();

    // Configure 
    CMsvcConfigure configure;
    configure.Configure(const_cast<CMsvcSite&>(GetSite()), 
                        GetRegSettings().m_ConfigInfo, 
                        GetProjectTreeInfo().m_Root);

    // Build projects tree
    LOG_POST(Info << "*** Analyzing subtree makefiles ***");
    CProjectItemsTree projects_tree(GetProjectTreeInfo().m_Src);
    CProjectTreeBuilder::BuildProjectTree(GetProjectTreeInfo().m_IProjectFilter.get(), 
                                          GetProjectTreeInfo().m_Src, 
                                          &projects_tree);
    
    // Analyze tree for dependencies cycles
    LOG_POST(Info << "*** Checking projects inter-dependencies ***");
    CCyclicDepends::TDependsCycles cycles;
    CCyclicDepends::FindCycles(projects_tree.m_Projects, &cycles);
    s_ReportDependenciesStatus(cycles);


    // MSVC specific part:
    
    LOG_POST(Info << "*** Checking requirements ***");
    // Exclude some projects from build:
#ifdef COMBINED_EXCLUDE
    {{
        // Implicit/Exclicit exclude by msvc Makefiles.in.msvc
        // and project .msvc makefiles.
        PIsExcludedMakefileIn          p_make_in(GetProjectTreeInfo().m_Src);
        PIsExcludedByProjectMakefile   p_project_makefile;
        CCombine<PIsExcludedMakefileIn, 
                 PIsExcludedByProjectMakefile,  
                 CProjectItemsTree::TProjects::value_type> 
                                  logical_combine(p_make_in, p_project_makefile);
        EraseIf(projects_tree.m_Projects, logical_combine);
    }}
#else
    {{
        // Implicit/Exclicit exclude by msvc Makefiles.in.msvc
        PIsExcludedByProjectMakefile   p_project_makefile;
        EraseIf(projects_tree.m_Projects, p_project_makefile);
    }}

#endif
    {{
        // Project requires are not provided

        EraseIf(projects_tree.m_Projects, PIsExcludedByRequires());

    }}

    const list<SConfigInfo>* configurations = 0;
    LOG_POST(Info << "*** Generating MSVC projects ***");
    if (GetBuildType().GetType() == CBuildType::eStatic) {
        
        // Static build
        LOG_POST(Info << "Static build");
        string str_log("Configurations: ");
        configurations = &GetRegSettings().m_ConfigInfo;
        ITERATE(list<SConfigInfo>, p , GetRegSettings().m_ConfigInfo) {
            str_log += p->m_Name + " ";
        }
        LOG_POST(Info << str_log);
        
        // Projects
        if ( m_AddMissingLibs ) {
            m_CurrentBuildTree = &projects_tree;
        }
        CMsvcProjectGenerator prj_gen(GetRegSettings().m_ConfigInfo);
        ITERATE(CProjectItemsTree::TProjects, p, projects_tree.m_Projects) {
            prj_gen.Generate(p->second);
        }

        //Utility projects dir
        string utility_projects_dir = CDirEntry(m_Solution).GetDir();
        utility_projects_dir = 
            CDirEntry::ConcatPath(utility_projects_dir, "UtilityProjects");
        utility_projects_dir = 
            CDirEntry::AddTrailingPathSeparator(utility_projects_dir);
        // MasterProject
        CMsvcMasterProjectGenerator master_prj_gen(projects_tree,
                                                   GetRegSettings().m_ConfigInfo,
                                                   utility_projects_dir);
        master_prj_gen.SaveProject();

        // ConfigureProject
        string output_dir = GetProjectTreeInfo().m_Compilers;
        output_dir = CDirEntry::ConcatPath(output_dir, 
                                           GetRegSettings().m_CompilersSubdir);
        output_dir = CDirEntry::ConcatPath(output_dir, 
                                           GetBuildType().GetTypeStr());
        output_dir = CDirEntry::ConcatPath(output_dir, "bin");
        output_dir = CDirEntry::AddTrailingPathSeparator(output_dir);
        CMsvcConfigureProjectGenerator configure_generator
                                              (output_dir,
                                               GetRegSettings().m_ConfigInfo,
                                               false,
                                               utility_projects_dir,
                                               GetProjectTreeInfo().m_Root,
                                               m_Subtree,
                                               m_Solution,
                                               m_BuildPtb);
        configure_generator.SaveProject(false);
        configure_generator.SaveProject(true);

        // INDEX dummy project
        CVisualStudioProject index_xmlprj;
        CreateUtilityProject(" INDEX, see here: ", 
                             GetRegSettings().m_ConfigInfo, 
                             &index_xmlprj);
        string index_prj_path = 
            CDirEntry::ConcatPath(utility_projects_dir, "_INDEX_");
        index_prj_path += MSVC_PROJECT_FILE_EXT;
        SaveIfNewer(index_prj_path, index_xmlprj);
        //

        // BuildAll utility project
        CVisualStudioProject build_all_xmlprj;
        CreateUtilityProject("-BUILD-ALL-", 
                             GetRegSettings().m_ConfigInfo, 
                             &build_all_xmlprj);
        string build_all_prj_path = 
            CDirEntry::ConcatPath(utility_projects_dir, "_BUILD_ALL_");
        build_all_prj_path += MSVC_PROJECT_FILE_EXT;
        SaveIfNewer(build_all_prj_path, build_all_xmlprj);
        //

        // Solution
        CMsvcSolutionGenerator sln_gen(GetRegSettings().m_ConfigInfo);
        ITERATE(CProjectItemsTree::TProjects, p, projects_tree.m_Projects) {
            sln_gen.AddProject(p->second);
        }
        sln_gen.AddUtilityProject (master_prj_gen.GetPath());
        sln_gen.AddUtilityProject (configure_generator.GetPath(false));
        sln_gen.AddUtilityProject (configure_generator.GetPath(true));
        sln_gen.AddUtilityProject (index_prj_path);
        sln_gen.AddBuildAllProject(build_all_prj_path);
        sln_gen.SaveSolution(m_Solution);
    }

    list<SConfigInfo> dll_configs;
    if (GetBuildType().GetType() == CBuildType::eDll) {

        //Dll build
        LOG_POST(Info << "DLL build");

        GetDllsInfo().GetBuildConfigs(&dll_configs);
        configurations = &dll_configs;

        string str_log("Configurations: ");
        ITERATE(list<SConfigInfo>, p , dll_configs) {
            str_log += p->m_Name + " ";
        }
        LOG_POST(Info << str_log);

        LOG_POST(Info << "Assembling DLLs:");
        CProjectItemsTree dll_projects_tree;
        CreateDllBuildTree(projects_tree, &dll_projects_tree);

        // Projects
        if ( m_AddMissingLibs ) {
            m_CurrentBuildTree = &dll_projects_tree;
        }
        CMsvcProjectGenerator prj_gen(dll_configs);
        ITERATE(CProjectItemsTree::TProjects, p, dll_projects_tree.m_Projects) {
            prj_gen.Generate(p->second);
        }

        //Utility projects dir
        string utility_projects_dir = CDirEntry(m_Solution).GetDir();
        utility_projects_dir = 
            CDirEntry::ConcatPath(utility_projects_dir, "UtilityProjects");
        utility_projects_dir = 
            CDirEntry::AddTrailingPathSeparator(utility_projects_dir);

        // MasterProject
        CMsvcMasterProjectGenerator master_prj_gen(dll_projects_tree,
                                                   dll_configs,
                                                   utility_projects_dir);
        master_prj_gen.SaveProject();

        // ConfigureProject
        string output_dir = GetProjectTreeInfo().m_Compilers;
        output_dir = CDirEntry::ConcatPath(output_dir, 
                                           GetRegSettings().m_CompilersSubdir);
        output_dir = CDirEntry::ConcatPath(output_dir, 
            m_BuildPtb ? "static" : GetBuildType().GetTypeStr());
        output_dir = CDirEntry::ConcatPath(output_dir, "bin");
        output_dir = CDirEntry::AddTrailingPathSeparator(output_dir);
        CMsvcConfigureProjectGenerator configure_generator
                              (output_dir,
                               dll_configs,
                               true,
                               utility_projects_dir,
                               GetProjectTreeInfo().m_Root,
                               m_Subtree,
                               m_Solution,
                               m_BuildPtb);
        configure_generator.SaveProject(false);
        configure_generator.SaveProject(true);

        // INDEX dummy project
        CVisualStudioProject index_xmlprj;
        CreateUtilityProject(" INDEX, see here: ", 
                             dll_configs, 
                             &index_xmlprj);
        string index_prj_path = 
            CDirEntry::ConcatPath(utility_projects_dir, "_INDEX_");
        index_prj_path += MSVC_PROJECT_FILE_EXT;
        SaveIfNewer(index_prj_path, index_xmlprj);
        //

        // BuildAll utility project
        CVisualStudioProject build_all_xmlprj;
        CreateUtilityProject("-BUILD-ALL-", 
                             dll_configs, 
                             &build_all_xmlprj);
        string build_all_prj_path = 
            CDirEntry::ConcatPath(utility_projects_dir, "_BUILD_ALL_");
        build_all_prj_path += MSVC_PROJECT_FILE_EXT;
        SaveIfNewer(build_all_prj_path, build_all_xmlprj);
        //

        // Solution
        CMsvcSolutionGenerator sln_gen(dll_configs);
        ITERATE(CProjectItemsTree::TProjects, p, dll_projects_tree.m_Projects) {
            sln_gen.AddProject(p->second);
        }
        sln_gen.AddUtilityProject (master_prj_gen.GetPath());
        sln_gen.AddUtilityProject (configure_generator.GetPath(false));
        sln_gen.AddUtilityProject (configure_generator.GetPath(true));
        sln_gen.AddUtilityProject (index_prj_path);
        sln_gen.AddBuildAllProject(build_all_prj_path);
        sln_gen.SaveSolution(m_Solution);
    }

    CreateFeaturesAndPackagesFiles(configurations);

    //
    LOG_POST(Info << "Finished at "+ CTime(CTime::eCurrent).AsString());
    return 0;
}


void CProjBulderApp::CreateFeaturesAndPackagesFiles(
    const list<SConfigInfo>* configs)
{
    // Create makefile path
    string base_path = GetProjectTreeInfo().m_Compilers;
    base_path = CDirEntry::ConcatPath(base_path, 
        GetRegSettings().m_CompilersSubdir);

    base_path = CDirEntry::ConcatPath(base_path, GetBuildType().GetTypeStr());
    ITERATE(list<SConfigInfo>, c , *configs) {
        string file_path = CDirEntry::ConcatPath(base_path, c->m_Name);
        string enabled = CDirEntry::ConcatPath(file_path, 
            "features_and_packages.txt");
        string disabled = CDirEntry::ConcatPath(file_path, 
            "features_and_packages_disabled.txt");
        file_path = CDirEntry::ConcatPath(file_path, 
                                          "features_and_packages.txt");
        CNcbiOfstream ofs(enabled.c_str(), IOS_BASE::out | IOS_BASE::trunc );
        if ( !ofs )
            NCBI_THROW(CProjBulderAppException, eFileCreation, enabled);

        CNcbiOfstream ofsd(disabled.c_str(), IOS_BASE::out | IOS_BASE::trunc );
        if ( !ofsd )
            NCBI_THROW(CProjBulderAppException, eFileCreation, disabled);
        if (c->m_rtType == SConfigInfo::rtMultiThreaded) {
            ofs << "MT" << endl;
        } else if (c->m_rtType == SConfigInfo::rtMultiThreadedDebug) {
            ofs << "MT" << endl << "Debug" << endl;
        } else if (c->m_rtType == SConfigInfo::rtMultiThreadedDLL) {
            ofs << "MT" << endl;
        } else if (c->m_rtType == SConfigInfo::rtMultiThreadedDebugDLL) {
            ofs << "MT" << endl << "Debug" << endl;
        } else if (c->m_rtType == SConfigInfo::rtSingleThreaded) {
        } else if (c->m_rtType == SConfigInfo::rtSingleThreadedDebug) {
            ofs << "Debug" << endl;
        }
        if (GetBuildType().GetType() == CBuildType::eDll) {
            ofs << "DLL" << endl;
        }
        ofs << "mswin" << endl;
        const set<string>& epackages =
            CMsvcPrjProjectContext::GetEnabledPackages(c->m_Name);
        ITERATE(set<string>, e, epackages) {
            ofs << *e << endl;
        }

        list<string> std_features;
        GetSite().GetStandardFeatures(std_features);
        ITERATE(list<string>, s, std_features) {
            ofs << *s << endl;
        }

        const set<string>& dpackages =
            CMsvcPrjProjectContext::GetDisabledPackages(c->m_Name);
        ITERATE(set<string>, d, dpackages) {
            ofsd << *d << endl;
        }
    }
}


void CProjBulderApp::Exit(void)
{
	//TODO
}


void CProjBulderApp::ParseArguments(void)
{
    CArgs args = GetArgs();

    m_Subtree = args["subtree"].AsString();

    m_Root = args["root"].AsString();
    m_Root = CDirEntry::AddTrailingPathSeparator(m_Root);
    m_Root = CDirEntry::NormalizePath(m_Root);
    m_Root = CDirEntry::AddTrailingPathSeparator(m_Root);

    m_Dll     = (bool)args["dll"];
    /// Root dir of building tree,
    /// src child dir of Root,
    /// Subtree to buil (default is m_RootSrc)
    /// are provided by SProjectTreeInfo (see GetProjectTreeInfo(void) below)

    // Solution
    m_Solution = CDirEntry::NormalizePath(args["solution"].AsString());
    LOG_POST(Info << "Solution: " << m_Solution);
    m_BuildPtb = !((bool)args["nobuildptb"]);

    m_AddMissingLibs =   (bool)args["ext"];
    m_ScanWholeTree  = !((bool)args["nws"]);
    if ( const CArgValue& t = args["extroot"] ) {
        m_BuildRoot = t.AsString();
    }
    if ( const CArgValue& t = args["projtag"] ) {
        m_ProjTags = t.AsString();
    }
    if (m_ProjTags.empty()) {
        m_ProjTags = "*";
    }
    m_ConfirmCfg =   (bool)args["cfg"];
}

void CProjBulderApp::VerifyArguments(void)
{
    m_Root = CDirEntry::AddTrailingPathSeparator(m_Root);

    list<string> values;
    NStr::Split(m_ProjTags, LIST_SEPARATOR, values);
    ITERATE(list<string>, n, values) {
        string value(*n);
        if (!value.empty()) {
            if (value.find('!') != NPOS) {
                value = NStr::Replace(value, "!", kEmptyStr);
                if (!value.empty()) {
                    m_DisallowedTags.insert(value);
                }
            } else {
                if (value.find('*') == NPOS) {
                    m_AllowedTags.insert(value);
                }
            }
        }
    }
    if (m_AllowedTags.empty()) {
        m_AllowedTags.insert("*");
    }
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
    CNcbiOfstream  ofs(filename.c_str(), IOS_BASE::out | IOS_BASE::trunc);
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


void CProjBulderApp::GetMetaDataFiles(list<string>* files) const
{
    files->clear();
    string files_str = GetConfig().GetString("ProjectTree", "MetaData", "");
    NStr::Split(files_str, LIST_SEPARATOR, *files);
}


void CProjBulderApp::GetBuildConfigs(list<SConfigInfo>* configs) const
{
    configs->clear();
    string config_str = GetConfig().GetString("msvc7", "Configurations", "");
    list<string> configs_list;
    NStr::Split(config_str, LIST_SEPARATOR, configs_list);
    LoadConfigInfoByNames(GetConfig(), configs_list, configs);
}


const CMsvc7RegSettings& CProjBulderApp::GetRegSettings(void)
{
    if ( !m_MsvcRegSettings.get() ) {
        m_MsvcRegSettings.reset(new CMsvc7RegSettings());
    
        m_MsvcRegSettings->m_Version = 
            GetConfig().GetString("msvc7", "Version", "7.10");

        GetBuildConfigs(&m_MsvcRegSettings->m_ConfigInfo);

        m_MsvcRegSettings->m_CompilersSubdir  = 
            GetConfig().GetString("msvc7", "compilers", "msvc710_prj");
    
        m_MsvcRegSettings->m_ProjectsSubdir  = 
            GetConfig().GetString("msvc7", "Projects", "build");

        m_MsvcRegSettings->m_MakefilesExt = 
            GetConfig().GetString("msvc7", "MakefilesExt", "msvc");

        m_MsvcRegSettings->m_MetaMakefile = 
            GetConfig().GetString("msvc7", "MetaMakefile", "");

        m_MsvcRegSettings->m_DllInfo = 
            GetConfig().GetString("msvc7", "DllInfo", "");
    }
    return *m_MsvcRegSettings;
}


const CMsvcSite& CProjBulderApp::GetSite(void)
{
    if ( !m_MsvcSite.get() ) 
        m_MsvcSite.reset(new CMsvcSite(GetConfig()));
    
    return *m_MsvcSite;
}


const CMsvcMetaMakefile& CProjBulderApp::GetMetaMakefile(void)
{
    if ( !m_MsvcMetaMakefile.get() ) {
        //Metamakefile must be in RootSrc directory
        m_MsvcMetaMakefile.reset(new CMsvcMetaMakefile
                    (CDirEntry::ConcatPath(GetProjectTreeInfo().m_Src,
                                           GetRegSettings().m_MetaMakefile)));
        
        //Metamakefile must present and must not be empty
        if ( m_MsvcMetaMakefile->IsEmpty() )
            NCBI_THROW(CProjBulderAppException, 
                       eMetaMakefile, GetRegSettings().m_MetaMakefile);
    }

    return *m_MsvcMetaMakefile;
}


const SProjectTreeInfo& CProjBulderApp::GetProjectTreeInfo(void)
{
    if ( m_ProjectTreeInfo.get() )
        return *m_ProjectTreeInfo;
        
    m_ProjectTreeInfo.reset(new SProjectTreeInfo);
    
    // Root, etc.
    m_ProjectTreeInfo->m_Root = m_Root;
    LOG_POST(Info << "Project tree root: " << m_Root);

    // all possible project tags
    string  tagsfile = GetConfig().GetString("ProjectTree", "ProjectTags", "");
    if (!tagsfile.empty()) {
        LoadProjectTags(
            CDirEntry::ConcatPath(m_ProjectTreeInfo->m_Root, tagsfile));
    }
    
    /// <include> branch of tree
    string include = GetConfig().GetString("ProjectTree", "include", "");
    m_ProjectTreeInfo->m_Include = 
            CDirEntry::ConcatPath(m_ProjectTreeInfo->m_Root, 
                                  include);
    m_ProjectTreeInfo->m_Include = 
        CDirEntry::AddTrailingPathSeparator(m_ProjectTreeInfo->m_Include);
    

    /// <src> branch of tree
    string src = GetConfig().GetString("ProjectTree", "src", "");
    m_ProjectTreeInfo->m_Src = 
            CDirEntry::ConcatPath(m_ProjectTreeInfo->m_Root, 
                                  src);
    m_ProjectTreeInfo->m_Src =
        CDirEntry::AddTrailingPathSeparator(m_ProjectTreeInfo->m_Src);

    // Subtree to build - projects filter
    string subtree = CDirEntry::ConcatPath(m_ProjectTreeInfo->m_Root, m_Subtree);
    string ext;
    CDirEntry::SplitPath(subtree, NULL, NULL, &ext);
    if (NStr::CompareNocase(ext, ".lst") == 0) {
        LOG_POST(Info << "Project list: " << subtree);
        //If this is *.lst file
        m_ProjectTreeInfo->m_IProjectFilter.reset
            (new CProjectsLstFileFilter(m_ProjectTreeInfo->m_Src,
                                        subtree));
    } else {
        LOG_POST(Info << "Project subtree: " << subtree);
        //Simple subtree
        subtree = CDirEntry::AddTrailingPathSeparator(subtree);
        m_ProjectTreeInfo->m_IProjectFilter.reset
            (new CProjectOneNodeFilter(m_ProjectTreeInfo->m_Src,
                                        subtree));
    }

    /// <compilers> branch of tree
    string compilers = 
        GetConfig().GetString("ProjectTree", "compilers", "");
    m_ProjectTreeInfo->m_Compilers = 
            CDirEntry::ConcatPath(m_ProjectTreeInfo->m_Root, 
                                  compilers);
    m_ProjectTreeInfo->m_Compilers = 
        CDirEntry::AddTrailingPathSeparator
                   (m_ProjectTreeInfo->m_Compilers);

    /// ImplicitExcludedBranches - all subdirs will be excluded by default
    string implicit_exclude_str 
        = GetConfig().GetString("ProjectTree", "ImplicitExclude", "");
    list<string> implicit_exclude_list;
    NStr::Split(implicit_exclude_str, 
                LIST_SEPARATOR, 
                implicit_exclude_list);
    ITERATE(list<string>, p, implicit_exclude_list) {
        const string& subdir = *p;
        string dir = CDirEntry::ConcatPath(m_ProjectTreeInfo->m_Src, 
                                           subdir);
        dir = CDirEntry::AddTrailingPathSeparator(dir);
        m_ProjectTreeInfo->m_ImplicitExcludedAbsDirs.push_back(dir);
    }

    /// <projects> branch of tree (scripts\projects)
    string projects = 
        GetConfig().GetString("ProjectTree", "projects", "");
    m_ProjectTreeInfo->m_Projects = 
            CDirEntry::ConcatPath(m_ProjectTreeInfo->m_Root, 
                                  projects);
    m_ProjectTreeInfo->m_Projects = 
        CDirEntry::AddTrailingPathSeparator
                   (m_ProjectTreeInfo->m_Compilers);

    /// impl part if include project node
    m_ProjectTreeInfo->m_Impl = 
        GetConfig().GetString("ProjectTree", "impl", "");

    /// Makefile in tree node
    m_ProjectTreeInfo->m_TreeNode = 
        GetConfig().GetString("ProjectTree", "TreeNode", "");

    return *m_ProjectTreeInfo;
}


const CBuildType& CProjBulderApp::GetBuildType(void)
{
    if ( !m_BuildType.get() ) {
        m_BuildType.reset(new CBuildType(m_Dll));
    }    
    return *m_BuildType;
}


CMsvcDllsInfo& CProjBulderApp::GetDllsInfo(void)
{
    if ( !m_DllsInfo.get() ) {
        string site_ini_dir = GetProjectTreeInfo().m_Compilers;
        site_ini_dir = 
                CDirEntry::ConcatPath(site_ini_dir, 
                                      GetRegSettings().m_CompilersSubdir);
        site_ini_dir = 
            CDirEntry::ConcatPath(site_ini_dir, 
                                  GetBuildType().GetTypeStr());
        string dll_info_file_name = GetRegSettings().m_DllInfo;
        dll_info_file_name =
            CDirEntry::ConcatPath(site_ini_dir, dll_info_file_name);
        m_DllsInfo.reset(new CMsvcDllsInfo(dll_info_file_name));
    }    
    return *m_DllsInfo;
}


const CProjectItemsTree& CProjBulderApp::GetWholeTree(void)
{
    if ( !m_WholeTree.get() ) {
        m_WholeTree.reset(new CProjectItemsTree);
        if (m_ScanWholeTree) {
            LOG_POST(Info << "*** Analyzing the whole tree makefiles ***");
            m_ScanningWholeTree = true;
            CProjectDummyFilter pass_all_filter;
            CProjectTreeBuilder::BuildProjectTree(&pass_all_filter, 
                                                GetProjectTreeInfo().m_Src, 
                                                m_WholeTree.get());
            m_ScanningWholeTree = false;
        } else {
            LOG_POST(Info << "*** Skipping scanning the whole tree makefiles ***");
        }
    }    
    return *m_WholeTree;
}


CDllSrcFilesDistr& CProjBulderApp::GetDllFilesDistr(void)
{
    if (m_DllSrcFilesDistr.get())
        return *m_DllSrcFilesDistr;

    m_DllSrcFilesDistr.reset ( new CDllSrcFilesDistr() );
    return *m_DllSrcFilesDistr;
}


string CProjBulderApp::GetDatatoolId(void) const
{
    return GetConfig().GetString("Datatool", "datatool", "datatool");
}


string CProjBulderApp::GetDatatoolPathForApp(void) const
{
    return GetConfig().GetString("Datatool", "Location.App", "datatool.exe");
}


string CProjBulderApp::GetDatatoolPathForLib(void) const
{
    return GetConfig().GetString("Datatool", "Location.Lib", "datatool.exe");
}


string CProjBulderApp::GetDatatoolCommandLine(void) const
{
    return GetConfig().GetString("Datatool", "CommandLine", "");
}

string CProjBulderApp::GetProjectTreeRoot(void) const
{
    string path = CDirEntry::ConcatPath(
        m_ProjectTreeInfo->m_Compilers,
        m_MsvcRegSettings->m_CompilersSubdir);
    return CDirEntry::AddTrailingPathSeparator(path);
}

bool CProjBulderApp::IsAllowedProjectTag(const CSimpleMakeFileContents& mk,
                                         string& unmet) const
{
    if (m_ScanningWholeTree) {
        return true;
    }
    CSimpleMakeFileContents::TContents::const_iterator k;
    k = mk.m_Contents.find("PROJ_TAG");
    if (k == mk.m_Contents.end()) {
        // makefile has no tag -- verify that *any tag* is allowed
        return m_AllowedTags.find("*") != m_AllowedTags.end();
    } else {
        const list<string>& values = k->second;
        list<string>::const_iterator i;
        // verify that all project tags are registered
        for (i = values.begin(); i != values.end(); ++i) {
            if (m_ProjectTags.find(*i) == m_ProjectTags.end()) {
                NCBI_THROW(CProjBulderAppException, eUnknownProjectTag, *i);
                return false;
            }
        }
        // for each tag see if it is not prohibited explicitly
        if (!m_DisallowedTags.empty()) {
            for (i = values.begin(); i != values.end(); ++i) {
                if (m_DisallowedTags.find(*i) != m_DisallowedTags.end()) {
                    unmet = *i;
                    return false;
                }
            }
        }
        if (m_AllowedTags.find("*") != m_AllowedTags.end()) {
            return true;
        }
        for (i = values.begin(); i != values.end(); ++i) {
            if (m_AllowedTags.find(*i) != m_AllowedTags.end()) {
                return true;
            }
        }
    }
    unmet = NStr::Join(k->second,",");
    return false;
}

void CProjBulderApp::LoadProjectTags(const string& filename)
{
    CNcbiIfstream ifs(filename.c_str(), IOS_BASE::in | IOS_BASE::binary);
    if ( ifs.is_open() ) {
        string line;
        while ( NcbiGetlineEOL(ifs, line) ) {
            NStr::TruncateSpacesInPlace(line);
            if (!line.empty()) {
                m_ProjectTags.insert(line);
            }
        }
    }
}

CProjBulderApp& GetApp(void)
{
    static CProjBulderApp theApp;
    return theApp;
}

END_NCBI_SCOPE


USING_NCBI_SCOPE;

int main(int argc, const char* argv[])
{
    // Execute main application function
    return GetApp().AppMain(argc, argv, 0, eDS_Default);
}


/*
 * ===========================================================================
 * $Log$
 * Revision 1.59  2005/05/13 13:14:43  gouriano
 * Do not filter by tags when scanning the whole tree
 *
 * Revision 1.58  2005/05/10 17:31:34  gouriano
 * Added verification of project tags
 *
 * Revision 1.57  2005/05/09 17:04:09  gouriano
 * Added filtering by project tag and GUI
 *
 * Revision 1.56  2005/04/29 14:12:02  gouriano
 * Use runtime library type instead of string
 *
 * Revision 1.55  2005/04/19 14:50:53  gouriano
 * Use PTB from the current build tree when PTB build is not requested
 *
 * Revision 1.54  2005/04/07 16:58:16  gouriano
 * Make it possible to find and reference missing libraries
 * without creating project dependencies
 *
 * Revision 1.53  2005/03/23 19:33:20  gouriano
 * Make it possible to exclude PTB build when configuring
 *
 * Revision 1.52  2005/02/16 19:28:28  gouriano
 * Corrected logging DLL feature
 *
 * Revision 1.51  2005/02/15 19:04:29  gouriano
 * Added list of standard features
 *
 * Revision 1.50  2005/02/14 18:52:29  gouriano
 * Generate a file with all features and packages listed
 *
 * Revision 1.49  2004/12/20 15:26:03  gouriano
 * Changed diagnostic output
 *
 * Revision 1.48  2004/12/06 18:12:20  gouriano
 * Improved diagnostics
 *
 * Revision 1.47  2004/11/09 17:39:03  gouriano
 * Changed generation rules for ncbiconf_msvc_site.h
 *
 * Revision 1.46  2004/10/04 15:31:57  gouriano
 * Take into account LIB_OR_DLL Makefile parameter
 *
 * Revision 1.45  2004/07/20 13:38:40  gouriano
 * Added conditional macro definition
 *
 * Revision 1.44  2004/07/16 16:33:08  gouriano
 * change pre-build rule for projects with ASN dependencies
 *
 * Revision 1.43  2004/06/15 19:01:40  gorelenk
 * Fixed compilation errors on GCC 2.95 .
 *
 * Revision 1.42  2004/06/14 14:16:58  gorelenk
 * Changed implementation of CProjBulderApp::GetProjectTreeInfo - added
 * m_TreeNode .
 *
 * Revision 1.41  2004/06/07 19:14:54  gorelenk
 * Changed CProjBulderApp::GetProjectTreeInfo .
 *
 * Revision 1.40  2004/06/07 13:57:43  gorelenk
 * Changed implementation of CProjBulderApp::GetRegSettings and
 * CProjBulderApp::GetDllsInfo.
 *
 * Revision 1.39  2004/05/21 21:41:41  gorelenk
 * Added PCH ncbi_pch.hpp
 *
 * Revision 1.38  2004/05/19 14:23:40  gorelenk
 * Implemented CProjBulderApp::GetDllFilesDistr .
 *
 * Revision 1.37  2004/05/17 16:21:38  gorelenk
 * Implemeted CProjBulderApp::GetDllFilesDistr .
 *
 * Revision 1.36  2004/04/13 17:09:39  gorelenk
 * Changed implementation of CProjBulderApp::Run .
 *
 * Revision 1.35  2004/04/08 18:45:56  gorelenk
 * Conditionaly enabled exclude projects by msvc makefiles .
 *
 * Revision 1.34  2004/03/23 14:35:51  gorelenk
 * Added implementation of CProjBulderApp::GetWholeTree.
 *
 * Revision 1.33  2004/03/10 21:29:27  gorelenk
 * Changed implementation of CProjBulderApp::Run.
 *
 * Revision 1.32  2004/03/10 16:50:13  gorelenk
 * Separated static build and dll build processing inside CProjBulderApp::Run.
 *
 * Revision 1.31  2004/03/08 23:37:01  gorelenk
 * Implemented CProjBulderApp::GetDllsInfo.
 *
 * Revision 1.30  2004/03/05 18:08:26  gorelenk
 * Excluded filtering of projects by .msvc makefiles.
 *
 * Revision 1.29  2004/03/03 22:20:02  gorelenk
 * Added predicate PIsExcludedMakefileIn, class template CCombine,  redesigned
 * projects exclusion inside CProjBulderApp::Run - to support local
 * Makefile.in.msvc .
 *
 * Revision 1.28  2004/03/03 00:06:25  gorelenk
 * Changed implementation of CProjBulderApp::Run.
 *
 * Revision 1.27  2004/03/02 23:35:41  gorelenk
 * Changed implementation of CProjBulderApp::Init (added flag "dll").
 * Added implementation of CProjBulderApp::GetBuildType.
 *
 * Revision 1.26  2004/03/02 16:25:41  gorelenk
 * Added include for proj_tree_builder.hpp.
 *
 * Revision 1.25  2004/03/01 18:01:19  gorelenk
 * Added processing times reporting to log. Added project tree checking
 * for projects inter-dependencies.
 *
 * Revision 1.24  2004/02/26 21:31:51  gorelenk
 * Re-designed CProjBulderApp::GetProjectTreeInfo - use creation of
 * IProjectFilter-derived classes instead of string.
 * Changed signature of call BuildProjectTree inside CProjBulderApp::Run.
 *
 * Revision 1.23  2004/02/25 19:44:04  gorelenk
 * Added creation of BuildAll utility project to CProjBulderApp::Run.
 *
 * Revision 1.22  2004/02/20 22:53:58  gorelenk
 * Added analysis of ASN projects depends.
 *
 * Revision 1.21  2004/02/18 23:37:06  gorelenk
 * Changed definition of member-function GetProjectTreeInfo.
 *
 * Revision 1.20  2004/02/13 20:39:52  gorelenk
 * Minor cosmetic changes.
 *
 * Revision 1.19  2004/02/13 17:52:42  gorelenk
 * Added command line parametrs path normalization.
 *
 * Revision 1.18  2004/02/12 23:15:29  gorelenk
 * Implemented utility projects creation and configure re-build of the app.
 *
 * Revision 1.17  2004/02/12 18:46:20  ivanov
 * Removed extra argument from AppMain() call to autoload .ini file
 *
 * Revision 1.16  2004/02/12 17:41:52  gorelenk
 * Implemented creation of MasterProject and ConfigureProject in different
 * folder.
 *
 * Revision 1.15  2004/02/12 16:27:10  gorelenk
 * Added _CONFIGURE_ project generation.
 *
 * Revision 1.14  2004/02/11 15:40:44  gorelenk
 * Implemented support for multiple implicit excludes from source tree.
 *
 * Revision 1.13  2004/02/10 18:18:43  gorelenk
 * Changed LOG_POST massages.
 *
 * Revision 1.12  2004/02/06 23:14:59  gorelenk
 * Implemented support of ASN projects, semi-auto configure,
 * CPPFLAGS support. Second working version.
 *
 * Revision 1.11  2004/02/04 23:59:52  gorelenk
 * Changed log messages generation.
 *
 * Revision 1.10  2004/02/03 17:14:24  gorelenk
 * Changed implementation of class CProjBulderApp member functions.
 *
 * Revision 1.9  2004/01/30 20:44:22  gorelenk
 * Initial revision.
 *
 * Revision 1.8  2004/01/29 15:45:13  gorelenk
 * Added support of project tree filtering
 *
 * Revision 1.7  2004/01/28 17:55:50  gorelenk
 * += For msvc makefile support of :
 *                 Requires tag, ExcludeProject tag,
 *                 AddToProject section (SourceFiles and IncludeDirs),
 *                 CustomBuild section.
 * += For support of user local site.
 *
 * Revision 1.6  2004/01/26 19:27:30  gorelenk
 * += MSVC meta makefile support
 * += MSVC project makefile support
 *
 * Revision 1.5  2004/01/22 17:57:55  gorelenk
 * first version
 *
 * ===========================================================================
 */


