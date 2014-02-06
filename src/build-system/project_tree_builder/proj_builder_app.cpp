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
#include "proj_builder_app.hpp"
#include "proj_item.hpp"
#include "proj_tree_builder.hpp"
#include "msvc_prj_utils.hpp"
#include "msvc_prj_generator.hpp"
#include "mac_prj_generator.hpp"
#include "msvc_sln_generator.hpp"
#include "msvc_masterproject_generator.hpp"
#include "proj_utils.hpp"
#include "msvc_configure.hpp"
#include "msvc_prj_defines.hpp"
#include "msvc_configure_prj_generator.hpp"
#include "proj_projects.hpp"
#include "configurable_file.hpp"
#include "ptb_err_codes.hpp"
#include <corelib/ncbitime.hpp>
#include <corelib/expr.hpp>

#include <common/test_assert.h>  /* This header must go last */


// special task
#define DO_PATCHTREEMAKEFILES  0

BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
///
/// Windows-specific command-line logger
/// This is used to format error output in such a way that the Windows error
/// logger can pick this up

class CWindowsCmdErrorHandler : public CDiagHandler
{
public:
    
    CWindowsCmdErrorHandler()
    {
        m_OrigHandler.reset(GetDiagHandler(true));
        CNcbiApplication* app = CNcbiApplication::Instance();
        if (app) {
            m_AppName = app->GetProgramDisplayName();
        } else {
            m_AppName = "unknown_app";
        }
    }

    ~CWindowsCmdErrorHandler()
    {
    }

    /// post a message
    void Post(const SDiagMessage& msg)
    {
        if (m_OrigHandler.get()) {
            m_OrigHandler->Post(msg);
        }

        CTempString str(msg.m_Buffer, msg.m_BufferLen);
        if (!msg.m_Buffer) {
            str.assign(kEmptyStr.c_str(),0);
        }

        /// screen for error message level data only
        /// MSVC doesn't handle the other parts
        switch (msg.m_Severity) {
        case eDiag_Error:
        case eDiag_Critical:
        case eDiag_Fatal:
        case eDiag_Warning:
            break;

        case eDiag_Info:
        case eDiag_Trace:
            if (msg.m_ErrCode == ePTB_NoError) {
                /// simple pass-through to stderr
                if (strlen(msg.m_File) != 0) {
                    cerr << msg.m_File << ": ";
                }
                cerr << str << endl;
                return;
            }
            break;
        }

        /// REQUIRED: origin
        if (strlen(msg.m_File) == 0) {
            cerr << m_AppName;
        } else {
            cerr << msg.m_File;
        }
        if (msg.m_Line) {
            cerr << "(" << msg.m_Line << ")";
        }
        cerr << ": ";

        /// OPTIONAL: subcategory
        //cerr << m_AppName << " ";

        /// REQUIRED: category
        /// the MSVC system understands only 'error' and 'warning'
        switch (msg.m_Severity) {
        case eDiag_Error:
        case eDiag_Critical:
        case eDiag_Fatal:
            cerr << "error ";
            break;

        case eDiag_Warning:
            cerr << "warning ";
            break;

        case eDiag_Info:
        case eDiag_Trace:
            /// FIXME: find out how to get this in the messages tab
            cerr << "info ";
            break;
        }

        /// REQUIRED: error code
        cerr << msg.m_ErrCode << ": ";

        /// OPTIONAL: text
        cerr << str << endl;
    }

private:
    auto_ptr<CDiagHandler> m_OrigHandler;

    /// the original diagnostics handler
    string        m_AppName;

};

/////////////////////////////////////////////////////////////////////////////


// When defined, this environment variable
// instructs PTB to exclude CONFIGURE, INDEX, and HIERARCHICAL VIEW
// projects
const char* s_ptb_skipconfig = "__PTB__SKIP__CONFIG__";

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

    typedef map<string, AutoPtr<CPtbRegistry> > TMakefiles;
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
                    AutoPtr<CPtbRegistry>
                         (new CPtbRegistry(CNcbiIfstream(path.c_str(), 
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
                    (p->second)->GetString("Common", "ExcludeProject");
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
                        << " by Makefile." << project.m_Name << ".*."
                        << GetApp().GetRegSettings().m_MakefilesExt);
            return true;
        }
        return false;
    }
};

#endif

struct PIsExcludedByTag
{
    typedef CProjectItemsTree::TProjects::value_type TValueType;
    bool operator() (const TValueType& item) const
    {
        const CProjItem& project = item.second;
        string unmet;
        if ( project.m_ProjType != CProjKey::eDataSpec && 
            !GetApp().IsAllowedProjectTag(project) ) {
            string unmet( NStr::Join(project.m_ProjTags,","));
            PTB_WARNING_EX(project.GetPath(), ePTB_ProjectExcluded,
                           "Excluded due to proj_tag; this project tags: " << unmet);
            return true;
        }
        return false;
    }
};

struct PIsExcludedByUser
{
    typedef CProjectItemsTree::TProjects::value_type TValueType;
    bool operator() (const TValueType& item) const
    {
        const CProjItem& project = item.second;
        if (project.m_ProjType != CProjKey::eDll &&
            project.m_ProjType != CProjKey::eDataSpec &&
            !GetApp().m_CustomConfiguration.DoesValueContain(
            "__AllowedProjects",
            CreateProjectName(CProjKey(project.m_ProjType, project.m_ID)), true)) {
            PTB_WARNING_EX(project.GetPath(), ePTB_ProjectExcluded,
                           "Excluded by user request");
            return true;
        }
        return false;

    }
};

struct PIsExcludedByRequires
{
    typedef CProjectItemsTree::TProjects::value_type TValueType;
    bool operator() (const TValueType& item) const
    {
        string unmet;
        const CProjItem& project = item.second;
        if ( !CMsvcPrjProjectContext::IsRequiresOk(project, &unmet) ) {
            PTB_WARNING_EX(project.GetPath(), ePTB_ProjectExcluded,
                           "Excluded due to unmet requirement: "
                           << unmet);
            return true;
        }
        return false;

    }
};

struct PIsExcludedByDisuse
{
    typedef CProjectItemsTree::TProjects::value_type TValueType;
    bool operator() (const TValueType& item) const
    {
        const CProjItem& project = item.second;
        if (project.m_External) {
            PTB_WARNING_EX(project.GetPath(), ePTB_ProjectExcluded,
                           "Excluded unused external");
            return true;
        }
        return false;
    }
};


//-----------------------------------------------------------------------------
CProjBulderApp::CProjBulderApp(void)
{
    SetVersion( CVersionInfo(3,11,0) );
    m_ScanningWholeTree = false;
    m_Dll = false;
    m_AddMissingLibs = false;
    m_ScanWholeTree  = true;
    m_TweakVTuneR = false;
    m_TweakVTuneD = false;
    m_AddUnicode = false;
    m_CurrentBuildTree = 0;
    m_IncompleteBuildTree = 0;
    m_ProjTagCmnd = false;
    m_ConfirmCfg = false;
    m_AllDllBuild = false;
    m_InteractiveCfg = false;
    m_Dtdep = false;
    m_AddMissingDep = false;
    m_CMakeMode = false;
    m_Ide = 0;
    m_ExitCode = 0;
}


void CProjBulderApp::Init(void)
{
    string logfile = GetLogFile();
    if (CDirEntry(logfile).Exists()) {
        RegisterGeneratedFile(CDirEntry::NormalizePath(logfile));
    }
    if (CMsvc7RegSettings::GetMsvcPlatform() < CMsvc7RegSettings::eUnix) {
        if (logfile != "STDERR") {
            SetDiagHandler(new CWindowsCmdErrorHandler);
        }
    }
    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    string context;
    if (CMsvc7RegSettings::GetMsvcPlatform() < CMsvc7RegSettings::eUnix) {
        context = "MSVC ";
/*
        context += CMsvc7RegSettings::GetProjectFileFormatVersion() +
            " (" + CMsvc7RegSettings::GetMsvcPlatformName() + ")";
*/
    } else if (CMsvc7RegSettings::GetMsvcPlatform() > CMsvc7RegSettings::eUnix) {
        context = "XCODE ";
/*
        context += CMsvc7RegSettings::GetMsvcVersionName() +
            " (" + CMsvc7RegSettings::GetMsvcPlatformName() + ")";
*/
    } else {
        context = CMsvc7RegSettings::GetMsvcPlatformName();
    }
    context += " projects tree builder application";
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),context);

    // Programm arguments:

    arg_desc->AddPositional("root",
                            "Root directory of the build tree. "\
                                "This directory ends with \"c++\".",
                            CArgDescriptions::eString);

    arg_desc->AddPositional("subtree",
                            "Subtree, or a file with a list of subtrees to build."\
                            " Examples: src/corelib/ scripts/projects/ncbi_cpp.lst",
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
                             "Expression. Include only projects that match."\
                             " Example: \"core && (test || !demo)\"",
                             CArgDescriptions::eString);

#if defined(NCBI_XCODE_BUILD) || defined(PSEUDO_XCODE)
    arg_desc->AddOptionalKey("ide", "xcode_version",
                             "Target version of Xcode, for example: 30",
                             CArgDescriptions::eInteger);
    arg_desc->AddOptionalKey("arch", "architecture",
                             "Target architecture, for example: ppc, i386",
                             CArgDescriptions::eString);
#elif defined(NCBI_COMPILER_MSVC)
    arg_desc->AddOptionalKey("ide", "msvc_version",
                             "Target version of MS Visual Studio, for example: 900, 1000, 1100",
                             CArgDescriptions::eInteger);
    arg_desc->AddOptionalKey("arch", "platform",
                             "Target platform, for example: Win32, x64",
                             CArgDescriptions::eString);
#endif

    arg_desc->AddFlag      ("cfg", 
                            "Show GUI to confirm configuration parameters (MS Windows only).");
    arg_desc->AddFlag      ("i", 
                            "Run interactively. Can only be used by PTB GUI shell!");
    arg_desc->AddFlag      ("dtdep", 
                            "Add dependency on datatool where needed.");
    arg_desc->AddFlag      ("noadddep", 
                            "Do not add missing dependencies.");

    arg_desc->AddOptionalKey("args", "args_file",
                             "Read arguments from a file",
                             CArgDescriptions::eString);

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}


static 
void s_ReportDependenciesStatus(const CCyclicDepends::TDependsCycles& cycles,
    CProjectItemsTree::TProjects& tree)
{
    bool reported = false;
    ITERATE(CCyclicDepends::TDependsCycles, p, cycles) {
        const CCyclicDepends::TDependsChain& cycle = *p;
        bool real_cycle = false;
        string host0, host;
        ITERATE(CCyclicDepends::TDependsChain, m, cycle) {
            host = tree[*m].m_DllHost;
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
        LOG_POST(Warning << str_chain);
        reported = true;
        CCyclicDepends::TDependsChain::const_iterator i = cycle.end();
        const CProjKey& last = *(--i);
        const CProjKey& prev = *(--i);
        if (last.Type() == CProjKey::eLib && prev.Type() == CProjKey::eLib) {
            CProjectItemsTree::TProjects::const_iterator t = tree.find(prev);
            if (t != tree.end()) {
                CProjItem item = t->second;
                item.m_Depends.remove(last);
                tree[prev] = item;
                LOG_POST(Warning << "Removing LIB dependency: "
                               << prev.Id() << " - " << last.Id());
            }
        }
    }
    if (!reported) {
        PTB_INFO("No dependency cycles found.");
    }
}

int CProjBulderApp::Run(void)
{
    // Set error posting and tracing on maximum.
//    SetDiagTrace(eDT_Enable);
    SetDiagPostAllFlags(eDPF_All & ~eDPF_DateTime);
    SetDiagPostLevel(eDiag_Info);
    LOG_POST(Info << "Started at " + CTime(CTime::eCurrent).AsString());
    LOG_POST(Info << "Project tree builder version " + GetVersion().Print());

    CStopWatch sw;
    sw.Start();

    // Get and check arguments
    ParseArguments();
    LOG_POST(Info << "Project tags filter: " + m_ProjTags);
    if (m_InteractiveCfg && !Gui_ConfirmConfiguration())
    {
        LOG_POST(Info << "Cancelled by request.");
        return 1;
    }
    else if (m_ConfirmCfg && !ConfirmConfiguration())
    {
        LOG_POST(Info << "Cancelled by request.");
        return 1;
    }
    VerifyArguments();
    m_CustomConfiguration.Save(m_CustomConfFile);

    // Configure 
    CMsvcConfigure configure;
    configure.Configure(const_cast<CMsvcSite&>(GetSite()), 
                        GetRegSettings().m_ConfigInfo, 
                        m_IncDir);
    m_AllDllBuild = GetSite().IsProvided("DLL_BUILD");

#if DO_PATCHTREEMAKEFILES
    // this is just a task; it is to be run very rarely
    {
        bool b = m_ScanWholeTree;
        m_ScanWholeTree = true;
        CMakefilePatch::PatchTreeMakefiles(GetWholeTree());
        m_ScanWholeTree = b;
        return 0;
    }
#endif
    // Build projects tree
#ifndef _DEBUG
// I need this to collect library relations data and metadata
    {
        bool b = m_ScanWholeTree;
        m_ScanWholeTree = true;
        GetWholeTree();
        m_ScanWholeTree = b;
    }
#endif

    CProjectItemsTree projects_tree(GetProjectTreeInfo().m_Src);
    CProjectTreeBuilder::BuildProjectTree(GetProjectTreeInfo().m_IProjectFilter.get(), 
                                          GetProjectTreeInfo().m_Src, 
                                          &projects_tree);
    if (m_ExitCode != 0) {
        LOG_POST(Info << "Cancelled by request.");
        return m_ExitCode;
    }
    configure.CreateConfH(const_cast<CMsvcSite&>(GetSite()), 
                        GetRegSettings().m_ConfigInfo, 
                        m_IncDir);
    
    // MSVC specific part:
    PTB_INFO("Checking project requirements...");
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

    CProjectItemsTree dll_projects_tree;
    bool dll = (GetBuildType().GetType() == CBuildType::eDll);
    if (dll) {
        PTB_INFO("Assembling DLLs...");
//        AnalyzeDllData(projects_tree);
        CreateDllBuildTree(projects_tree, &dll_projects_tree);
    }
    CProjectItemsTree& prj_tree = dll ? dll_projects_tree : projects_tree;
    prj_tree.VerifyExternalDepends();
    {{
        // Erase obsolete external projects
        EraseIf(prj_tree.m_Projects, PIsExcludedByDisuse());
    }}
    prj_tree.VerifyDataspecProj();

    PTB_INFO("Checking project inter-dependencies...");
    CCyclicDepends::TDependsCycles cycles;
    CCyclicDepends::FindCyclesNew(prj_tree.m_Projects, &cycles);
    s_ReportDependenciesStatus(cycles,projects_tree.m_Projects);

    if (!m_SuspiciousProj.empty()) {
        ITERATE( set<CProjKey>, key, m_SuspiciousProj) {
            if (prj_tree.m_Projects.find(*key) != prj_tree.m_Projects.end()) {
                PTB_ERROR(prj_tree.m_Projects.find(*key)->second.GetPath(),
                    "More than one target with this name");
                m_ExitCode = 1;
            }
        }
    }
    if (m_ExitCode != 0) {
        string subtree = CDirEntry::ConcatPath(m_ProjectTreeInfo->m_Root, m_Subtree);
        if (CDirEntry(subtree).IsFile()) {
            return m_ExitCode;
        }
        m_ExitCode = 0;
    }
    PTB_INFO("Creating projects...");
    if (IsCMakeMode()) {
//        CMakeGenerator::GenerateCMakeTree(prj_tree);
    } else {
        if (CMsvc7RegSettings::GetMsvcPlatform() < CMsvc7RegSettings::eUnix) {
            GenerateMsvcProjects(prj_tree);
        }
        else if (CMsvc7RegSettings::GetMsvcPlatform() == CMsvc7RegSettings::eUnix) {
            GenerateUnixProjects(prj_tree);
        }
        else {
            GenerateMacProjects(prj_tree);
        }
        ReportGeneratedFiles();
        ReportProjectWatchers();
    }
    //
    PTB_INFO("Done.  Elapsed time = " << sw.Elapsed() << " seconds");
    return m_ExitCode;
}

void CProjBulderApp::GenerateMsvcProjects(CProjectItemsTree& projects_tree)
{
#if NCBI_COMPILER_MSVC
    PTB_INFO("Generating MSBuild projects...");

    bool dll = (GetBuildType().GetType() == CBuildType::eDll);
    list<SConfigInfo> dll_configs;
    const list<SConfigInfo>* configurations = 0;
    bool skip_config = !GetEnvironment().Get(s_ptb_skipconfig).empty();
    string str_config;

    if (dll) {
        _TRACE("DLL build");
        GetBuildConfigs(&dll_configs);
        configurations = &dll_configs;
    } else {
        _TRACE("Static build");
        configurations = &GetRegSettings().m_ConfigInfo;
    }
    {{
        ITERATE(list<SConfigInfo>, p , *configurations) {
            str_config += p->GetConfigFullName() + " ";
        }
        PTB_INFO("Building configurations: " << str_config);
    }}

    m_CurrentBuildTree = &projects_tree;
    if ( m_AddMissingLibs ) {
        m_IncompleteBuildTree = &projects_tree;
    }
    // Projects
    CMsvcProjectGenerator prj_gen(*configurations);
    NON_CONST_ITERATE(CProjectItemsTree::TProjects, p, projects_tree.m_Projects) {
        prj_gen.Generate(p->second);
    }

    //Utility projects dir
    string utility_projects_dir = GetApp().GetUtilityProjectsDir();

    // MasterProject
    CMsvcMasterProjectGenerator master_prj_gen(projects_tree,
                                               *configurations,
                                               utility_projects_dir);
    if (!skip_config) {
        master_prj_gen.SaveProject();
    }

    // ConfigureProject
    string output_dir = GetProjectTreeInfo().m_Compilers;
    output_dir = CDirEntry::ConcatPath(output_dir, 
                                        GetRegSettings().m_CompilersSubdir);
    output_dir = CDirEntry::ConcatPath(output_dir, 
        (m_BuildPtb && dll) ? "static" : GetBuildType().GetTypeStr());
    output_dir = CDirEntry::ConcatPath(output_dir, "bin");
    output_dir = CDirEntry::AddTrailingPathSeparator(output_dir);
    CMsvcConfigureProjectGenerator configure_generator(
                                            output_dir,
                                            *configurations,
                                            dll,
                                            utility_projects_dir,
                                            GetProjectTreeInfo().m_Root,
                                            m_Subtree,
                                            m_Solution,
                                            m_BuildPtb);
    if (!skip_config) {
        configure_generator.SaveProject(false, &prj_gen);
        configure_generator.SaveProject(true, &prj_gen);
    }

    // INDEX dummy project
    string index_prj_path = CDirEntry::ConcatPath(utility_projects_dir, "_INDEX_");
    index_prj_path += CMsvc7RegSettings::GetVcprojExt();
    string index_prj_guid, index_prj_name;
    if (CMsvc7RegSettings::GetMsvcVersion() < CMsvc7RegSettings::eMsvc1000) {
        CVisualStudioProject xmlprj;
        CreateUtilityProject(" INDEX, see here: ", *configurations, &xmlprj);
        if (!skip_config) {
            SaveIfNewer(index_prj_path, xmlprj);
            index_prj_guid = xmlprj.GetAttlist().GetProjectGUID();
            index_prj_name = xmlprj.GetAttlist().GetName();
        }
    }

    string utils[] = {"_DATASPEC_ALL_", "-DATASPEC-ALL-",
                      "_LIBS_ALL_", "-LIBS-ALL-",
                      "_BUILD_ALL_","-BUILD-ALL-"};
    vector<string> utils_id;
    int i = 0, num_util = 3;
    
    for (i = 0; i < num_util; ++i) {
        string prj_file(utils[i*2]);
        string prj_name(utils[i*2+1]);
        if (CMsvc7RegSettings::GetMsvcVersion() >= CMsvc7RegSettings::eMsvc1100) {
            NStr::ReplaceInPlace(prj_name,"-", "_");
        }
        string prj_path = CDirEntry::ConcatPath(utility_projects_dir, prj_file);
        prj_path += CMsvc7RegSettings::GetVcprojExt();
        utils_id.push_back(prj_path);
        if (CMsvc7RegSettings::GetMsvcVersion() < CMsvc7RegSettings::eMsvc1000) {
            CVisualStudioProject xmlprj;
            CreateUtilityProject(prj_name, *configurations, &xmlprj);
            SaveIfNewer(prj_path, xmlprj);
            utils_id.push_back(xmlprj.GetAttlist().GetProjectGUID());
            utils_id.push_back(xmlprj.GetAttlist().GetName());
        } else {
            string prj_dir =  GetApp().GetUtilityProjectsSrcDir();
            CProjItem prj_item( CreateUtilityProjectItem(prj_dir, prj_name));
            prj_gen.Generate(prj_item);
            utils_id.push_back(prj_item.m_GUID);
            utils_id.push_back(prj_item.m_Name);
        }
    }
    if (m_ProjTags == "*") {
        for (map<string,string>::const_iterator composite = m_CompositeProjectTags.begin();
            composite != m_CompositeProjectTags.end(); ++composite) {
            string composite_name = "_TAG_" + composite->first;
            string composite_filter = composite->second;


            string prj_path = CDirEntry::ConcatPath(utility_projects_dir, composite_name);
            prj_path += CMsvc7RegSettings::GetVcprojExt();
            utils_id.push_back(prj_path);
            if (CMsvc7RegSettings::GetMsvcVersion() < CMsvc7RegSettings::eMsvc1000) {
                CVisualStudioProject xmlprj;
                CreateUtilityProject(composite_name, *configurations, &xmlprj);
                SaveIfNewer(prj_path, xmlprj);
                utils_id.push_back(xmlprj.GetAttlist().GetProjectGUID());
                utils_id.push_back(xmlprj.GetAttlist().GetName());
            } else {
                string prj_dir =  GetApp().GetUtilityProjectsSrcDir();
                CProjItem prj_item( CreateUtilityProjectItem(prj_dir, composite_name));
                prj_gen.Generate(prj_item);
                utils_id.push_back(prj_item.m_GUID);
                utils_id.push_back(prj_item.m_Name);
            }
            utils_id.push_back(composite_filter);
            ++num_util;
        }
    }

    // Solution
    CMsvcSolutionGenerator sln_gen(*configurations);
    ITERATE(CProjectItemsTree::TProjects, p, projects_tree.m_Projects) {
        sln_gen.AddProject(p->second);
    }
    if (!skip_config) {

        if (CMsvc7RegSettings::GetMsvcVersion() < CMsvc7RegSettings::eMsvc1000) {
            sln_gen.AddUtilityProject( master_prj_gen.GetPath(),
                master_prj_gen.GetVisualStudioProject().GetAttlist().GetProjectGUID(),
                master_prj_gen.GetVisualStudioProject().GetAttlist().GetName());

            sln_gen.AddUtilityProject( index_prj_path, index_prj_guid, index_prj_name);
        }

        string cfg_path, cfg_guid, cfg_name;
        configure_generator.GetVisualStudioProject(cfg_path, cfg_guid, cfg_name, false);
        sln_gen.AddConfigureProject( cfg_path, cfg_guid, cfg_name);

        configure_generator.GetVisualStudioProject(cfg_path, cfg_guid, cfg_name, true);
        sln_gen.AddConfigureProject( cfg_path, cfg_guid, cfg_name);
    }

    int u = 0;
    for (i = 0; i < num_util; ++i) {
        switch (i) {
        case 0:
            sln_gen.AddAsnAllProject(   utils_id[u],  utils_id[u+1],  utils_id[u+2]);
            u += 3;
            break;
        case 1:
            sln_gen.AddLibsAllProject(  utils_id[u],  utils_id[u+1],  utils_id[u+2]);
            u += 3;
            break;
        case 2:
            sln_gen.AddBuildAllProject( utils_id[u],  utils_id[u+1],  utils_id[u+2]);
            u += 3;
            break;
        default:
            sln_gen.AddTagProject( utils_id[u],  utils_id[u+1],  utils_id[u+2],  utils_id[u+3]);
            u += 4;
            break;
        }
    }

    sln_gen.SaveSolution(m_Solution);

    CreateCheckList(configurations, projects_tree);
    list<string> enabled, disabled;
    CreateFeaturesAndPackagesFiles(configurations, enabled, disabled);
    GenerateSummary(*configurations, enabled, disabled);
#endif //NCBI_COMPILER_MSVC
}

void CProjBulderApp::GenerateMacProjects(CProjectItemsTree& projects_tree)
{
#if defined(NCBI_XCODE_BUILD) || defined(PSEUDO_XCODE)
    PTB_INFO("Generating XCode projects...");

    bool dll = (GetBuildType().GetType() == CBuildType::eDll);
    list<SConfigInfo> dll_configs;
    const list<SConfigInfo>* configurations = 0;
//    bool skip_config = !GetEnvironment().Get(s_ptb_skipconfig).empty();
    string str_config;

    if (dll) {
        _TRACE("DLL build");
        GetBuildConfigs(&dll_configs);
        configurations = &dll_configs;
    } else {
        _TRACE("Static build");
        configurations = &GetRegSettings().m_ConfigInfo;
    }
    {{
        ITERATE(list<SConfigInfo>, p , *configurations) {
            str_config += p->GetConfigFullName() + " ";
        }
        PTB_INFO("Building configurations: " << str_config);
    }}

    m_CurrentBuildTree = &projects_tree;
    if ( m_AddMissingLibs ) {
        m_IncompleteBuildTree = &projects_tree;
    }
    // Projects
    CMacProjectGenerator prj_gen(*configurations, projects_tree);
    prj_gen.Generate(m_Solution);

    CreateCheckList(configurations, projects_tree);
    list<string> enabled, disabled;
    CreateFeaturesAndPackagesFiles(configurations, enabled, disabled);
    GenerateSummary(*configurations, enabled, disabled);
#endif
}
void CProjBulderApp::CollectLibToLibDependencies(
    CProjectItemsTree& projects_tree,
    set<string>& dep, set<string>& visited,
    CProjectItemsTree::TProjects::const_iterator& lib,
    CProjectItemsTree::TProjects::const_iterator& lib_dep)
{
    string lib_name(CreateProjectName(lib->first));
    string lib_dep_name(CreateProjectName(lib_dep->first));
    if (m_AllDllBuild) {
        dep.insert(lib_dep_name);
//        return;
    }
    if (visited.find(lib_dep_name) != visited.end() ||
        lib_dep_name == lib_name) {
        return;
    }
    visited.insert(lib_dep_name);
    if (!lib_dep->second.m_DatatoolSources.empty() ||
        !lib_dep->second.m_ExportHeaders.empty() ||
        lib->second.m_UnconditionalDepends.find(lib_dep->first) !=
            lib->second.m_UnconditionalDepends.end()) {
        dep.insert(lib_dep_name);
    }
    ITERATE(list<CProjKey>, p, lib_dep->second.m_Depends) {
        if (p->Type() == CProjKey::eLib) {
            CProjectItemsTree::TProjects::const_iterator n =
                projects_tree.m_Projects.find(*p);
            if (n != projects_tree.m_Projects.end()) {
                CollectLibToLibDependencies(projects_tree, dep, visited, lib, n);
            }
        }
    }
}

void CProjBulderApp::GenerateUnixProjects(CProjectItemsTree& projects_tree)
{
    map< string, list< string > > path_to_target;
    string solution_dir(CDirEntry(m_Solution).GetDir());
    CNcbiOfstream ofs(m_Solution.c_str(), IOS_BASE::out | IOS_BASE::trunc);
    if (!ofs.is_open()) {
        NCBI_THROW(CProjBulderAppException, eFileOpen, m_Solution);
        return;
    }
    GetApp().RegisterGeneratedFile( m_Solution );
    ofs << "# This file was generated by PROJECT_TREE_BUILDER v"
        <<  GetVersion().Print() << endl;
    ofs << "# on " << CTime(CTime::eCurrent).AsString() << endl << endl;

// see CXX-950
#if 0
    ofs << "# This is tricky part; it might work incorrectly on some platforms" << endl;
//    ofs << "MARK=$(shell date +%Y%m%d%H%M%S)" << endl;
    ofs << "MARK=$(if $(MARK2),,$(eval MARK2=$(shell date +%Y%m%d%H%M%S)))$(MARK2)" << endl;
    ofs << "MARK:sh =date +%Y%m%d%H%M%S" << endl;
    ofs << endl;
    ofs << "prefix=.ncbi.signal." << endl;
    ofs << "s=$(prefix)$(MARK)" << endl;
    ofs << "sign=rm -f $(prefix)*.$@; touch $(s).$@" << endl;
    ofs << endl;
#endif

    ofs << "MINPUT=" << CDirEntry(m_Solution).GetName() << endl << endl;
    ofs << "# Use empty MTARGET to build a project;" << endl;
    ofs << "# MTARGET=clean - to clean, or MTARGET=purge - to purge" << endl;
    ofs << "MTARGET =" << endl << endl;

    if (m_ExtSrcRoot.empty()) {
       ofs << "top_srcdir=" << m_Root << endl;
    } else {
        ofs << "top_srcdir=" << m_ExtSrcRoot << endl;
    }
    ofs << "# Non-redundant flags (will be overridden for GNU Make to avoid" << endl;
    ofs << "# --jobserver-fds=* proliferation)" << endl;
    ofs << "MFLAGS_NR = $(MFLAGS)" << endl;
    ofs << "SKIP_PRELIMINARIES= sources= configurables=configurables.null" << endl;

// all dirs -----------------------------------------------------------------
    list<string> all_dirs;
    ITERATE(CProjectItemsTree::TProjects, p, projects_tree.m_Projects) {
        if (p->first.Type() == CProjKey::eDataSpec) {
            continue;
        }
        all_dirs.push_back(
            CDirEntry::DeleteTrailingPathSeparator( CDirEntry::CreateRelativePath(
                GetProjectTreeInfo().m_Src,p->second.m_SourcesBaseDir)));
    }
    all_dirs.sort();
    all_dirs.unique();
    ofs << "all_dirs =";
    ITERATE(list<string>, p, all_dirs) {
        ofs << " \\" <<endl << "    " << *p;
    }
    ofs << endl << endl;

    ofs << "include $(top_srcdir)/src/build-system/Makefile.is_gmake" << endl;
    ofs << "include $(top_srcdir)/src/build-system/Makefile.meta.$(is_gmake)" << endl;
    ofs << endl;

    string dotreal(".real");
    string dotfiles(".files");

// all projects -------------------------------------------------------------
    ofs << "all_projects =";
    ITERATE(CProjectItemsTree::TProjects, p, projects_tree.m_Projects) {
        if (p->second.m_MakeType == eMakeType_Excluded ||
            p->second.m_MakeType == eMakeType_ExcludedByReq) {
            LOG_POST(Info << "For reference only: " << CreateProjectName(p->first));
            continue;
        }
        if (p->first.Type() == CProjKey::eDataSpec) {
            continue;
        }
        ofs << " \\" <<endl << "    " << CreateProjectName(p->first);
    }
    ofs << endl << endl;

    ofs << "ptb_all :" << endl
        << "\t$(MAKE) $(MFLAGS_NR) -f $(MINPUT) ptb_all" << dotreal
        << " MTARGET=$(MTARGET)";
    ofs << endl << endl;
    ofs << "ptb_all" << dotreal << " :" << " $(all_projects:%=%" << dotreal << ")";
    ofs << endl << endl;

// all libs -----------------------------------------------------------------
    ofs << "all_libraries =";
    ITERATE(CProjectItemsTree::TProjects, p, projects_tree.m_Projects) {
        if (p->second.m_MakeType == eMakeType_Excluded ||
            p->second.m_MakeType == eMakeType_ExcludedByReq) {
            continue;
        }
        if (p->first.Type() == CProjKey::eLib ||
            p->first.Type() == CProjKey::eDll) {
            ofs << " \\" <<endl << "    " << CreateProjectName(p->first);
        }
    }
    ofs << endl << endl;

    ofs << "all_libs :" << endl
        << "\t$(MAKE) $(MFLAGS_NR) -f $(MINPUT) all_libs" << dotreal
        << " MTARGET=$(MTARGET)";
    ofs << endl << endl;
    ofs << "all_libs" << dotreal << " :" << " $(all_libraries:%=%" << dotreal << ")";
    ofs << endl << endl;

// all sources --------------------------------------------------------------
    ofs << "all_dataspec =";
    ITERATE(CProjectItemsTree::TProjects, p, projects_tree.m_Projects) {
        if (p->second.m_MakeType == eMakeType_Excluded ||
            p->second.m_MakeType == eMakeType_ExcludedByReq) {
            continue;
        }
        if (p->first.Type() == CProjKey::eDataSpec) {
            continue;
        }
        if (p->second.m_DatatoolSources.empty()) {
            continue;
        }
        ofs << " \\" <<endl << "    " << CreateProjectName(p->first) << dotfiles;
    }
    ofs << endl << endl;

    ofs << "all_files :" << endl
        << "\t$(MAKE) $(MFLAGS_NR) -f $(MINPUT) all_files" << dotreal;
    ofs << endl << endl;
    ofs << "all_files" << dotreal << " :" << " $(all_dataspec:%=%" << dotreal << ")";
    ofs << endl << endl;

// all apps -----------------------------------------------------------------
    ofs << "all_apps =";
    ITERATE(CProjectItemsTree::TProjects, p, projects_tree.m_Projects) {
        if (p->second.m_MakeType == eMakeType_Excluded ||
            p->second.m_MakeType == eMakeType_ExcludedByReq) {
            continue;
        }
        if (p->first.Type() == CProjKey::eApp) {
            ofs << " \\" <<endl << "    " << CreateProjectName(p->first);
        }
    }
    ofs << endl << endl;

// all Unix -------------------------------------------------------------
    ofs << "all_unix =";
    ITERATE(CProjectItemsTree::TProjects, p, projects_tree.m_Projects) {
        if (p->second.m_MakeType == eMakeType_Excluded ||
            p->second.m_MakeType == eMakeType_ExcludedByReq) {
            continue;
        }
        if (p->first.Type() == CProjKey::eMsvc) {
            ofs << " \\" <<endl << "    " << CreateProjectName(p->first);
        }
    }
    ofs << endl << endl;

// all excluded -------------------------------------------------------------
    ofs << "all_excluded =";
    ITERATE(CProjectItemsTree::TProjects, p, projects_tree.m_Projects) {
        if (p->first.Type() == CProjKey::eDataSpec) {
            continue;
        }
        if (p->second.m_MakeType == eMakeType_Excluded ||
            p->second.m_MakeType == eMakeType_ExcludedByReq) {
            ofs << " \\" <<endl << "    " << CreateProjectName(p->first);
        }
    }
    ofs << endl << endl;

// CompositeProjectTags -----------------------------------------------------
// (add always)
    vector<string> existing_composite_names;
    ITERATE(set<string>, r, m_RegisteredProjectTags) {
        m_CompositeProjectTags[*r] = *r;
    }
    /*if (m_ProjTags == "*")*/ {
        for (map<string,string>::const_iterator composite = m_CompositeProjectTags.begin();
            composite != m_CompositeProjectTags.end(); ++composite) {
            string composite_name = "TAG_" + composite->first;
            string composite_filter = composite->second;
            vector<string> matching;

            ITERATE(CProjectItemsTree::TProjects, p, projects_tree.m_Projects) {
                if (p->second.m_MakeType == eMakeType_Excluded ||
                    p->second.m_MakeType == eMakeType_ExcludedByReq) {
                    continue;
                }
                if (p->first.Type() == CProjKey::eDataSpec) {
                    continue;
                }
                if (IsAllowedProjectTag(p->second, &composite_filter)) {
                    matching.push_back( CreateProjectName(p->first));
                }
            }
            if (!matching.empty()) {
                existing_composite_names.push_back(composite_name);
                ofs << composite_name << "_projects =";
                ITERATE(vector<string>, c, matching) {
                    ofs << " \\" <<endl << "    " << *c;
                }
                ofs << endl << endl;

                ofs << composite_name << " :" << endl
                    << "\t$(MAKE) $(MFLAGS_NR) -f $(MINPUT) " << composite_name << dotreal
                    << " MTARGET=$(MTARGET)";
                ofs << endl << endl;
                ofs << composite_name << dotreal << " :" << " $("
                    << composite_name << "_projects" << ":%=%" << dotreal << ")";
                ofs << endl << endl;
                
            }
        }
    }


// help and list targets ----------------------------------------------------
    ofs << "help :"
        << endl << "\t@echo Build all projects"
        << endl << "\t@echo \"    make -f $(MINPUT) -j 12 ptb_all\""
        << endl << "\t@echo Build a project, for example, xncbi.lib"
        << endl << "\t@echo \"    make -f $(MINPUT) -j 12 xncbi.lib\""
        << endl << "\t@echo Clean project intermediate files"
        << endl << "\t@echo \"    make -f $(MINPUT) -j 12 xncbi.lib MTARGET=clean\""
        << endl << "\t@echo Clean project intermediate and output files"
        << endl << "\t@echo \"    make -f $(MINPUT) -j 12 xncbi.lib MTARGET=purge\""
        << endl << "\t@echo Target lists: "
        << endl << "\t@echo \"    list-all         - list all targets\""
        << endl << "\t@echo \"    list-apps        - list all applications\""
        << endl << "\t@echo \"    list-libs        - list all libraries\""
        << endl << "\t@echo \"    list-unix        - list all native Unix projects\""
        << endl << "\t@echo \"    list-excluded    - list 'excluded' targets\""
        << endl << "\t@echo \"    list-tags        - list composite targets\""
        << endl << "\t@echo \"    list-tag-TagName - list all targets in a composite target TagName\"";
    ofs << endl << endl;

    ofs << "list-all :"
        << endl << "\t@echo"
        << endl << "\t@echo"
        << endl << "\t@echo --------------------------------------"
        << endl << "\t@echo APPLICATIONS"
        << endl << "\t@echo"
        << endl << "\t@for i in $(all_apps); do echo $$i; done"
        << endl << "\t@echo"
        << endl << "\t@echo"
        << endl << "\t@echo --------------------------------------"
        << endl << "\t@echo LIBRARIES"
        << endl << "\t@echo"
        << endl << "\t@for i in $(all_libraries); do echo $$i; done";

    if (!existing_composite_names.empty()) {
       ofs 
            << endl << "\t@echo"
            << endl << "\t@echo"
            << endl << "\t@echo --------------------------------------"
            << endl << "\t@echo COMPOSITE TARGETS"
            << endl << "\t@echo";
        ITERATE(vector<string>, c, existing_composite_names) {
            ofs  << endl << "\t@echo " << *c;
        }
    }
    ofs
        << endl << "\t@echo"
        << endl << "\t@echo"
        << endl << "\t@echo --------------------------------------"
        << endl << "\t@echo DIRECTORIES"
        << endl << "\t@echo"
        << endl << "\t@for i in $(all_dirs); do echo $$i/; done";
    ofs << endl << endl;
    ofs << "list-apps :"
        << endl << "\t@for i in $(all_apps); do echo $$i; done";
    ofs << endl << endl;
    ofs << "list-libs :"
        << endl << "\t@for i in $(all_libraries); do echo $$i; done";
    ofs << endl << endl;
    ofs << "list-unix :"
        << endl << "\t@for i in $(all_unix); do echo $$i; done";
    ofs << endl << endl;
    ofs << "list-excluded :"
        << endl << "\t@for i in $(all_excluded); do echo $$i; done";
    ofs << endl << endl;

    ofs << "list-tags :";
    if (!existing_composite_names.empty()) {
        ITERATE(vector<string>, c, existing_composite_names) {
            ofs  << endl << "\t@echo " << *c;
        }
    }
    ofs << endl << endl;
    ITERATE(vector<string>, c, existing_composite_names) {
        ofs << "list-tag-" << *c << " :"
            << endl << "\t@for i in $(" << *c << "_projects); do echo $$i; done";
        ofs << endl << endl;
    }

// --------------------------------------------------------------------------
    string datatool_key;
    string datatool( GetDatatoolId() );
    set<string> dataspec_dirs;
    if (!datatool.empty()) {
        CProjKey t(CProjKey::eApp, datatool);
        if (projects_tree.m_Projects.find(t) != projects_tree.m_Projects.end()) {
            datatool_key = CreateProjectName(t);
        }
    }
    
    ITERATE(CProjectItemsTree::TProjects, p, projects_tree.m_Projects) {

        if (p->first.Type() == CProjKey::eDataSpec) {
            continue;
        }

        bool isLibrary = p->first.Type() == CProjKey::eLib;
        bool hasDataspec = !p->second.m_DatatoolSources.empty();
        string target, target_app, target_lib, target_user;
        list<string> dependencies;
        CProjectItemsTree::TProjects::const_iterator n;

        target = CreateProjectName(p->first);
        target_app = target_lib = target_user = "\"\"";
        if (p->first.Type() == CProjKey::eApp) {
            target_app = p->second.m_Name;
        } else if (p->first.Type() == CProjKey::eMsvc) {
            target_user = p->second.m_Name;
        } else if (p->first.Type() == CProjKey::eLib) {
            target_lib = p->second.m_Name;
        } else {
            target_lib = p->second.m_Name;
        }
        string rel_path = CDirEntry::CreateRelativePath(
            GetProjectTreeInfo().m_Src,p->second.m_SourcesBaseDir);

// check for missing dependencies -------------------------------------------
        string error;
        if (p->second.m_MakeType != eMakeType_Expendable && m_BuildRoot.empty()) {
            ITERATE(set<CProjKey>, u, p->second.m_UnconditionalDepends) {
                CProjKey proj_key = *u;
                if (projects_tree.m_Projects.find(proj_key) ==
                    projects_tree.m_Projects.end()) {
                    bool depfound = false;
                    string dll(GetDllHost(projects_tree, proj_key.Id()));
                    if (!dll.empty()) {
                        CProjKey id_alt(CProjKey::eDll,dll);
                        depfound = (projects_tree.m_Projects.find(id_alt) !=
                            projects_tree.m_Projects.end());
                    }
                    if (!depfound &&
                        !SMakeProjectT::IsConfigurableDefine(proj_key.Id())) {
                        error = "@echo ERROR: this project depends on missing " +
                            CreateProjectName(proj_key);
                    }
                }
            }
        }

// collect dependencies -----------------------------------------------------
        set<string> lib_guid, visited;
        ITERATE(list<CProjKey>, i, p->second.m_Depends) {

            const CProjKey& id = *i;
            // exclude 3rd party libs
            if ( GetSite().IsLibWithChoice(id.Id()) ) {
                if ( GetSite().GetChoiceForLib(id.Id()) == CMsvcSite::e3PartyLib ) {
                    continue;
                }
            }
            // exclude missing projects
            n = projects_tree.m_Projects.find(id);
            if (n == projects_tree.m_Projects.end()) {
/*
                CProjKey id_alt(CProjKey::eDll,GetDllsInfo().GetDllHost(id.Id()));
                n = projects_tree.m_Projects.find(id_alt);
                if (n == projects_tree.m_Projects.end())
*/
                {
                    if (!SMakeProjectT::IsConfigurableDefine(id.Id())) {
                        LOG_POST(Warning << "Project " + p->first.Id() + 
                                 " depends on missing project " + id.Id());
                    }
                    continue;
                }
            }
            if (isLibrary && id.Type() == CProjKey::eLib) {
                CollectLibToLibDependencies(projects_tree, lib_guid, visited, p, n);
                continue;
            }
            dependencies.push_back(CreateProjectName(n->first));
        }
        copy(lib_guid.begin(), lib_guid.end(), back_inserter(dependencies));
        dependencies.sort();
        dependencies.unique();
        CProjectTreeBuilder::VerifyBuildOrder( p->second, dependencies, projects_tree);

        if (isLibrary && !m_AllDllBuild) {
            list<string> new_dependencies;
//            new_dependencies.push_back( target + dotfiles);
            ITERATE(list<string>, d, dependencies) {
                if (*d == datatool_key) {
                    continue;
                }
                n = projects_tree.m_Projects.find(CreateProjKey(*d));
                if (n != projects_tree.m_Projects.end() &&
                    !n->second.m_DatatoolSources.empty()) {
                    new_dependencies.push_back(*d + dotfiles);
                    continue;
                }
                new_dependencies.push_back(*d);
            }
            dependencies = new_dependencies;
        }

// collect paths ------------------------------------------------------------
        if (p->first.Type() != CProjKey::eDataSpec) {

            path_to_target[rel_path].push_back(target);
            if (p->second.m_MakeType != eMakeType_Excluded &&
                p->second.m_MakeType != eMakeType_ExcludedByReq) {
                string stop_path(CDirEntry::AddTrailingPathSeparator("."));
                string parent_path, prev_parent(rel_path);
                for (;;) {
                    parent_path = ParentDir(prev_parent);
// see CXX-950
#if 0
                    path_to_target[parent_path].push_back(prev_parent + ".real");
#else
                    path_to_target[parent_path].push_back(prev_parent);
#endif
                    if (parent_path == stop_path) {
                        break;
                    }
                    prev_parent = parent_path;
                }
            }
        }
                                                            
#if NCBI_COMPILER_MSVC
        rel_path = NStr::Replace(rel_path,"\\","/");
#endif //NCBI_COMPILER_MSVC

// see CXX-950
#if 0
            ofs << target << " : " << rel_path << "$(s)." << target << ".real";
        ofs << endl << endl;
        ofs << rel_path << "$(s)." << target << ".real" << " :" << endl
            << "\t$(MAKE) $(MFLAGS_NR) -f $(MINPUT) " << target << ".real"
            << " MTARGET=$(MTARGET) MARK=$(MARK)";
        ofs << endl << endl;
        ofs << target << ".real" << " :";
        ITERATE(list<string>, d, dependencies) {
            ofs << " " << *d;
        }
        ofs << endl << "\t";
        if (!error.empty()) {
            ofs << error << endl << "\t@exit 1" << endl << "\t";
        }
        ofs << "+";
        if (p->second.m_MakeType == eMakeType_Expendable) {
            ofs << "-";
            }
        ofs << "cd " << rel_path << "; $(MAKE) $(MFLAGS_NR)"
            << " APP_PROJ=" << target_app
        << " LIB_PROJ=" << target_lib
        << " $(MTARGET)" << endl
        << "\t@" << "cd " << rel_path << "; $(sign)";
        ofs << endl << endl;
#else
        ofs << target << " :" << endl
            << "\t$(MAKE) $(MFLAGS) -f $(MINPUT) " << target << dotreal
            << " MTARGET=$(MTARGET)";
        ofs << endl << endl;
        ofs << target << dotreal << " :";

        if (hasDataspec) {
            dataspec_dirs.insert(rel_path);
        }
        ITERATE(list<string>, d, dependencies) {
            if (*d == datatool_key) {
                dataspec_dirs.insert(rel_path);
            } else {
                ofs << " " << *d << dotreal;
            }
        }
        ofs << " " << rel_path << dotfiles << dotreal;


        ofs << endl << "\t";
        if (!error.empty()) {
            ofs << error << endl << "\t@exit 1" << endl << "\t";
        }
        ofs << "+";
        if (p->second.m_MakeType >= eMakeType_Expendable) {
            ofs << "-";
        }
        ofs << "cd " << rel_path << " && ";
        if (p->second.m_MakeType == eMakeType_Expendable) {
            ofs << " NCBI_BUT_EXPENDABLE=' (but expendable)'";
        }
        ofs << " $(MAKE) $(MFLAGS)"
            << " APP_PROJ=" << target_app
            << " LIB_PROJ=" << target_lib
            << " UNIX_PROJ=" << target_user
            << " $(MTARGET) $(SKIP_PRELIMINARIES)" << endl << endl;
 #endif
        if (hasDataspec) {
            ofs << target << dotfiles << " :" << endl
                << "\t$(MAKE) $(MFLAGS) -f $(MINPUT) $(SKIP_PRELIMINARIES) "
                << target << dotfiles << dotreal;
            ofs << endl << endl;
            ofs << target << dotfiles << dotreal << " :";
            ofs << " " << rel_path << dotfiles << dotreal;
            ofs << endl << endl;
        }

// recommended library order
        if (p->first.Type() == CProjKey::eApp || p->first.Type() == CProjKey::eDll) {
            const list<string>& liborder  = m_LibraryOrder[       p->second.m_MkName];
            const list<string>& lib3order = m_3PartyLibraryOrder[ p->second.m_MkName];
            string filename(
                CDirEntry::ConcatPath(solution_dir,
                    CDirEntry::CreateRelativePath( GetProjectTreeInfo().m_Src, p->second.GetPath() + ".libdep")));
            if (!liborder.empty() ||  !lib3order.empty()) {
                CNcbiOfstream ofs(filename.c_str(), IOS_BASE::out | IOS_BASE::trunc);
                if (ofs.is_open()) {
                    if (!liborder.empty()) {
                        ofs << "GENERATED_LIB_ORDER = -l" << NStr::Join(liborder," -l") << endl;
                    }
                    if (!lib3order.empty()) {
                        ofs << "GENERATED_LIB3PARTY_ORDER =";
                        ITERATE( list<string>, l3, lib3order) {
                            ofs << " ";
                            if (m_Frameworks.find(*l3) != m_Frameworks.end()) {
                                ofs << "-framework ";
                            } else if (m_3PartyLibs.find(*l3) != m_3PartyLibs.end()) {
                                ofs << "-l";
                            } else if (l3->at(0) != '-' && 
                                       l3->find_first_of("$/.") == string::npos) {
                                ofs << "-l";
                            }
                            ofs << *l3;
                        }
                        ofs << endl;
                    }
                }
            }
            else {
                CFile(filename).Remove();
            }
        }
    }

// folder targets -----------------------------------------------------------
    map< string, list< string > >::const_iterator pt;
    for ( pt = path_to_target.begin(); pt != path_to_target.end(); ++pt) {
        string target(pt->first);
        ofs << ".PHONY : " << target << endl << endl;
        ofs << target << " :" << endl
            << "\t$(MAKE) $(MFLAGS_NR) -f $(MINPUT) " << target << dotreal
            << " MTARGET=$(MTARGET)";
        ofs << endl << endl;
        ofs << target << dotreal << " :";
        if (!pt->second.empty()) {
            list< string > tt(pt->second);
            tt.sort();
            tt.unique();
// see CXX-950
#if 0
            ofs << " " << NStr::Join( tt, " ");
#else
            ofs << " " << NStr::Join( tt, dotreal + " ") << dotreal;
#endif
            ofs << endl << endl;
        }

        ofs << target << dotfiles << " :" << endl
            << "\t$(MAKE) $(MFLAGS_NR) -f $(MINPUT) " << target << dotfiles << dotreal
            << " MTARGET=$(MTARGET)";
        ofs << endl << endl;
        ofs << target << dotfiles << dotreal << " :";
        if (m_Dtdep && !datatool_key.empty() &&
            dataspec_dirs.find(target) != dataspec_dirs.end()) {
            ofs << " " << datatool_key << dotreal;
        }
        ofs << endl << "\t";
        ofs << "-";
        ofs << "cd " << target << " && $(MAKE) $(MFLAGS) sources";
        ofs << endl << endl;
    }
}


void CProjBulderApp::CreateFeaturesAndPackagesFiles(
    const list<SConfigInfo>* configs,
    list<string>& list_enabled, list<string>& list_disabled)
{
    PTB_INFO("Generating Features_And_Packages files...");
    // Create makefile path
    string base_path = GetProjectTreeInfo().m_Compilers;
    base_path = CDirEntry::ConcatPath(base_path, 
        GetRegSettings().m_CompilersSubdir);

    base_path = CDirEntry::ConcatPath(base_path, GetBuildType().GetTypeStr());
    ITERATE(list<SConfigInfo>, c , *configs) {
        string file_path = CDirEntry::ConcatPath(base_path, c->GetConfigFullName());
        CDir(file_path).CreatePath();
        string enabled = CDirEntry::ConcatPath(file_path, 
            "features_and_packages.txt");
        string disabled = CDirEntry::ConcatPath(file_path, 
            "features_and_packages_disabled.txt");
        file_path = CDirEntry::ConcatPath(file_path, 
                                          "features_and_packages.txt");
        CNcbiOfstream ofs(enabled.c_str(), IOS_BASE::out | IOS_BASE::trunc );
        if ( !ofs )
            NCBI_THROW(CProjBulderAppException, eFileCreation, enabled);
        GetApp().RegisterGeneratedFile( enabled );

        CNcbiOfstream ofsd(disabled.c_str(), IOS_BASE::out | IOS_BASE::trunc );
        if ( !ofsd )
            NCBI_THROW(CProjBulderAppException, eFileCreation, disabled);
        GetApp().RegisterGeneratedFile( disabled );

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
        const set<string>& epackages =
            CMsvcPrjProjectContext::GetEnabledPackages(c->GetConfigFullName());
        ITERATE(set<string>, e, epackages) {
            ofs << *e << endl;
            list_enabled.push_back(*e);
        }

        list<string> std_features;
        GetSite().GetStandardFeatures(std_features);
        ITERATE(list<string>, s, std_features) {
            ofs << *s << endl;
            list_enabled.push_back(*s);
        }

        const set<string>& dpackages =
            CMsvcPrjProjectContext::GetDisabledPackages(c->GetConfigFullName());
        ITERATE(set<string>, d, dpackages) {
            ofsd << *d << endl;
            list_disabled.push_back(*d);
        }
    }
    list_enabled.sort();
    list_enabled.unique();
    list_disabled.sort();
    list_disabled.unique();
}

void CProjBulderApp::GenerateSummary(const list<SConfigInfo> configs, 
    const list<string>& enabled, const list<string>& disabled)
{
    if (!m_ConfSrc.empty() && !m_ConfDest.empty()) {
        string orig_ext = CDirEntry( CDirEntry(m_ConfSrc).GetBase() ).GetExt();
        ITERATE(list<SConfigInfo>, p , configs) {
            const SConfigInfo& cfg_info = *p;
            string file_dst_path;
            file_dst_path = m_ConfDest + "." +
                            ConfigurableFileSuffix(cfg_info.GetConfigFullName())+
                            orig_ext;
            CreateConfigurableFile(m_ConfSrc, file_dst_path,
                                   cfg_info.GetConfigFullName());
        }
    }

    string str_config;
    // summary
    SetDiagPostAllFlags(eDPF_Log);
    PTB_INFO("===========================================================");
    PTB_INFO("SOLUTION: " << m_Solution);
    PTB_INFO("PROJECTS: " << CDirEntry::ConcatPath(m_ProjectTreeInfo->m_Root, m_Subtree));
    PTB_INFO("CONFIGURATIONS: " << str_config);
    PTB_INFO("FEATURES AND PACKAGES: ");
    string str_pkg = "     enabled: ";
    ITERATE( list<string>, p, enabled) {
        if (str_pkg.length() > 70) {
            PTB_INFO(str_pkg);
            str_pkg = "              ";
        }
        str_pkg += " ";
        str_pkg += *p;
    }
    if (!str_pkg.empty()) {
        PTB_INFO(str_pkg);
    }
    str_pkg = "    disabled: ";
    ITERATE( list<string>, p, disabled) {
        if (str_pkg.length() > 70) {
            PTB_INFO(str_pkg);
            str_pkg = "              ";
        }
        str_pkg += " ";
        str_pkg += *p;
    }
    if (!str_pkg.empty()) {
        PTB_INFO(str_pkg);
    }
    string str_path = GetProjectTreeInfo().m_Compilers;
    str_path = CDirEntry::ConcatPath(str_path, 
        GetRegSettings().m_CompilersSubdir);
    str_path = CDirEntry::ConcatPath(str_path, GetBuildType().GetTypeStr());

    PTB_INFO(" ");
    PTB_INFO("    If a package is present in both lists,");
    PTB_INFO("    it is disabled in SOME configurations only");
    PTB_INFO("    For details see 'features_and_packages' files in");
    PTB_INFO("    " << str_path << "/%ConfigurationName%");
    PTB_INFO("===========================================================");
}

void CProjBulderApp::CreateCheckList(const list<SConfigInfo>* configs,
    CProjectItemsTree& projects_tree)
{
    PTB_INFO("Generating check.sh.list files...");
    string output_dir(m_Solution);
    if (CMsvc7RegSettings::GetMsvcPlatform() < CMsvc7RegSettings::eUnix) {
        string::size_type n = output_dir.find_last_of('.');
        if (n != string::npos) {
            output_dir = output_dir.substr(0,n);
        }
    }
    output_dir += ".check";
    ITERATE(list<SConfigInfo>, c , *configs) {
        string cfg(c->GetConfigFullName());
        string file_path = CDirEntry::ConcatPath(output_dir, cfg);
        CDir dir(file_path);
        if (!dir.Exists()) {
            dir.CreatePath();
        }
        file_path = CDirEntry::ConcatPath(file_path, "check.sh.list");
        CNcbiOfstream ofs(file_path.c_str(), IOS_BASE::out | IOS_BASE::trunc );
        if ( !ofs )
            NCBI_THROW(CProjBulderAppException, eFileCreation, file_path);
        GetApp().RegisterGeneratedFile( file_path );
        list<string> all_cmd;
        ITERATE(CProjectItemsTree::TProjects, p, projects_tree.m_Projects) {
            const CProjItem& project = p->second;
            if (project.m_MakeType == eMakeType_Excluded ||
                project.m_MakeType == eMakeType_ExcludedByReq) {
                continue;
            }
            if (project.m_CheckConfigs.find(cfg) != project.m_CheckConfigs.end()) {
                ITERATE( list<string>, cmd, project.m_CheckInfo) {
                    all_cmd.push_back(*cmd);
                }
            } else if (!project.m_CheckInfo.empty()) {
                PTB_INFO("Project: " << p->first.Id() << ": CHECK_CMD disabled in " << cfg);
            }
        }
        all_cmd.sort();
        all_cmd.unique();
        ITERATE(list<string>, cmd, all_cmd) {
            ofs << *cmd << endl;
        }
    }
}

void CProjBulderApp::ReportGeneratedFiles(void)
{
    m_GeneratedFiles.sort();
    string file_path( m_Solution + "_generated_files.txt");
    string sep;
    sep += CDirEntry::GetPathSeparator();
    string root(m_Root);
    if (!CDirEntry::IsAbsolutePath(root)) {
        root = CDirEntry::ConcatPath(CDir::GetCwd(), root);
    }
    CNcbiOfstream ofs(file_path.c_str(), IOS_BASE::out | IOS_BASE::trunc );
    if (ofs.is_open()) {
        ITERATE( list<string>, f, m_GeneratedFiles) {
            string path(*f);
            if (!CDirEntry::IsAbsolutePath(path)) {
                path = CDirEntry::ConcatPath(CDir::GetCwd(), path);
            }
            path = CDirEntry::CreateRelativePath(root, path);
            ofs << NStr::ReplaceInPlace( path, sep, "/") << endl;
        }
    }
}

void CProjBulderApp::ReportProjectWatchers(void)
{
    m_ProjWatchers.sort();
    string file_path( m_Solution + "_watchers.txt");
    CNcbiOfstream ofs(file_path.c_str(), IOS_BASE::out | IOS_BASE::trunc );
    if (ofs.is_open()) {
        ITERATE( list<string>, f, m_ProjWatchers) {
            ofs << *f << endl;
        }
    }
}

void CProjBulderApp::Exit(void)
{
}


void CProjBulderApp::ParseArguments(void)
{
    const CArgs& args = GetArgs();
    string root;
    bool extroot = false;
    bool argfile = false;
    string argsfile;

    if ( args["args"] ) {
        argsfile = args["args"].AsString();
        if (CDirEntry(argsfile).Exists()) {
            argfile = true;
            m_CustomConfiguration.LoadFrom(argsfile,&m_CustomConfiguration);
        } else {
            NCBI_THROW(CProjBulderAppException, eFileOpen, 
                                    argsfile + " not found");
        }
    }

    root = args["root"].AsString();
    if (root == "\"\"") {
        root = "";
    }
    if (argfile && root.empty()) {
        m_CustomConfiguration.GetPathValue("__arg_root", root);
    }
    m_Root = CDirEntry::IsAbsolutePath(root) ? 
        root : CDirEntry::ConcatPath( CDir::GetCwd(), root);
    m_Root = CDirEntry::AddTrailingPathSeparator(m_Root);
    m_Root = CDirEntry::NormalizePath(m_Root);
    m_Root = CDirEntry::AddTrailingPathSeparator(m_Root);

    m_Subtree        = args["subtree"].AsString();
    if (m_Subtree == "\"\"") {
        m_Subtree = "";
    }
    if (CDirEntry::IsAbsolutePath(m_Subtree)) {
        m_Subtree = CDirEntry::NormalizePath(
                        CDirEntry::CreateRelativePath(m_Root, m_Subtree));
    }
    m_Solution       = CDirEntry::NormalizePath(args["solution"].AsString());
    m_CMakeMode = NStr::CompareNocase(CDirEntry(m_Solution).GetName(), "cmake?") == 0;
    if (m_Solution == "\"\"") {
        m_Solution = "";
    }
    if (!m_Solution.empty() && !CDirEntry::IsAbsolutePath(m_Solution)) {
        m_Solution = CDirEntry::ConcatPath( CDir::GetCwd(), m_Solution);
    }

    if (argfile) {
        string v;
        if (GetConfigPath().empty() && 
            m_CustomConfiguration.GetPathValue("__arg_conffile", v)) {
            if (!CDirEntry::IsAbsolutePath(v)) {
                v = CDirEntry::ConcatPath(m_Root,v);
            }
            LoadConfig(GetConfig(),&v);
        }
        string subtree;
        m_CustomConfiguration.GetPathValue("__arg_subtree", subtree);
        if (m_Subtree.empty()) {
            m_Subtree = subtree;
        } else if (m_Subtree != subtree) {
            m_CustomConfiguration.RemoveDefinition("__AllowedProjects");
        }
        if (m_Solution.empty()) {
            m_CustomConfiguration.GetPathValue("__arg_solution", m_Solution);
            if (!CDirEntry::IsAbsolutePath(m_Solution)) {
                m_Solution = CDirEntry::ConcatPath(m_Root,m_Solution);
            }
        }

        if (m_CustomConfiguration.GetValue("__arg_dll", v)) {
            m_Dll = NStr::StringToBool(v);
        }
        if (m_CustomConfiguration.GetValue("__arg_nobuildptb", v)) {
            m_BuildPtb = !NStr::StringToBool(v);
        }

        if (m_CustomConfiguration.GetValue("__arg_ext", v)) {
            m_AddMissingLibs = NStr::StringToBool(v);
        }
        if (m_CustomConfiguration.GetValue("__arg_nws", v)) {
            m_ScanWholeTree = !NStr::StringToBool(v);
        }
        extroot = m_CustomConfiguration.GetPathValue("__arg_extroot", m_BuildRoot);
        m_CustomConfiguration.GetValue("__arg_projtag", m_ProjTags);

#if defined(NCBI_COMPILER_MSVC) || defined(NCBI_XCODE_BUILD) || defined(PSEUDO_XCODE)
        if (m_CustomConfiguration.GetValue("__arg_ide", v)) {
            m_Ide = NStr::StringToInt(v);
        }
        m_CustomConfiguration.GetValue("__arg_arch", m_Arch);
#endif
    } else {
        m_Dll            =   (bool)args["dll"];
        m_BuildPtb       = !((bool)args["nobuildptb"]);
        m_AddMissingLibs =   (bool)args["ext"];
        m_ScanWholeTree  = !((bool)args["nws"]);
        extroot     = (bool)args["extroot"];
        if (extroot) {
            m_BuildRoot      = args["extroot"].AsString();
        }
        if ( const CArgValue& t = args["projtag"] ) {
            m_ProjTags = t.AsString();
            m_ProjTagCmnd = true;
        } else {
            m_ProjTags = CProjectsLstFileFilter::GetAllowedTagsInfo(
                CDirEntry::ConcatPath(m_Root, m_Subtree));
            m_ProjTagCmnd = false;
        }
#if defined(NCBI_COMPILER_MSVC) || defined(NCBI_XCODE_BUILD) || defined(PSEUDO_XCODE)
        const CArgValue& ide = args["ide"];
        if ((bool)ide) {
            m_Ide = ide.AsInteger();
        }
        const CArgValue& arch = args["arch"];
        if ((bool)arch) {
            m_Arch = arch.AsString();
        }
#endif
    }

    CMsvc7RegSettings::IdentifyPlatform();

    string entry[] = {"","",""};
    if (CMsvc7RegSettings::GetMsvcPlatform() < CMsvc7RegSettings::eUnix) {
        entry[0] = "ThirdPartyBasePath";
        entry[1] = "ThirdParty_C_ncbi";
    }
    else if (CMsvc7RegSettings::GetMsvcPlatform() == CMsvc7RegSettings::eXCode) {
        entry[0] = "XCode_ThirdPartyBasePath";
        entry[1] = "XCode_ThirdParty_C_ncbi";
    }
    if (argfile) {
        // this replaces path separators in entry[j] with a native one
        string v;
        for (int j=0; !entry[j].empty(); ++j) {
            if (m_CustomConfiguration.GetPathValue(entry[j], v)) {
                m_CustomConfiguration.AddDefinition(entry[j], v);
            }
        }
    }

    m_ConfirmCfg = false;
#if defined(NCBI_COMPILER_MSVC)
    m_ConfirmCfg =   (bool)args["cfg"];
#endif
    m_InteractiveCfg = (bool)args["i"];
    m_Dtdep = (bool)args["dtdep"];
    m_AddMissingDep = !((bool)args["noadddep"]);

    // Solution
    PTB_INFO("Solution: " << m_Solution);
    m_StatusDir = 
        CDirEntry::NormalizePath( CDirEntry::ConcatPath( CDirEntry::ConcatPath( 
            CDirEntry(m_Solution).GetDir(),".."),"status"));
//    m_BuildPtb = m_BuildPtb &&
//        CMsvc7RegSettings::GetMsvcVersion() == CMsvc7RegSettings::eMsvc710;

    if ( extroot ) {
        if (CDirEntry(m_BuildRoot).Exists()) {
// verify status dir
            if (!CDirEntry(m_StatusDir).Exists() && !m_BuildRoot.empty()) {
                m_StatusDir = CDirEntry::NormalizePath(
                    CDirEntry::ConcatPath( CDirEntry::ConcatPath( 
                        m_BuildRoot,".."),"status"));
            }

            string t, try_dir;
            string src = GetConfig().Get("ProjectTree", "src");
            for ( t = try_dir = m_BuildRoot; ; try_dir = t) {
                if (CDirEntry(
                    CDirEntry::ConcatPath(try_dir, src)).Exists()) {
                    m_ExtSrcRoot = try_dir;
                    break;
                }
                t = CDirEntry(try_dir).GetDir();
                if (t == try_dir) {
                    break;
                }
            }
        }
    }
    if (m_ProjTags.empty() || m_ProjTags == "\"\"" || m_ProjTags == "#") {
        m_ProjTags = "*";
    }

    string tmp(GetConfig().Get("ProjectTree", "CustomConfiguration"));
    if (!tmp.empty()) {
        m_CustomConfFile = CDirEntry::ConcatPath( CDirEntry(m_Solution).GetDir(), tmp);
    }
    CDir sln_dir(CDirEntry(m_Solution).GetDir());
    if ( !sln_dir.Exists() ) {
        sln_dir.CreatePath();
    }
    if (!argfile) {
        if (CFile(m_CustomConfFile).Exists()) {
            m_CustomConfiguration.LoadFrom(m_CustomConfFile,&m_CustomConfiguration);

            string subtree;
            m_CustomConfiguration.GetPathValue("__arg_subtree", subtree);
            if (m_Subtree.empty()) {
                m_Subtree = subtree;
            } else if (m_Subtree != subtree) {
                m_CustomConfiguration.RemoveDefinition("__AllowedProjects");
            }
        } else {
            if (CMsvc7RegSettings::GetMsvcPlatform() != CMsvc7RegSettings::eUnix) {
                for (int j=0; !entry[j].empty(); ++j) {
                    m_CustomConfiguration.AddDefinition(entry[j], GetSite().GetConfigureEntry(entry[j]));
                }
            }
            if (CMsvc7RegSettings::GetMsvcPlatform() < CMsvc7RegSettings::eUnix) {
                string vt = GetSite().IsProvided("TweakVTune", false) ? "yes" : "no";
                m_CustomConfiguration.AddDefinition("__TweakVTuneR", vt);
                m_CustomConfiguration.AddDefinition("__TweakVTuneD", vt);
            }
        }

        string v;
        v = GetConfigPath();
        if (CDirEntry::IsAbsolutePath(v)) {
            try {
                v = CDirEntry::CreateRelativePath(m_Root, v);
            } catch (CFileException&) {
            }
        }
        m_CustomConfiguration.AddDefinition("__arg_conffile", v);

        m_CustomConfiguration.AddDefinition("__arg_root", root);
        m_CustomConfiguration.AddDefinition("__arg_subtree", m_Subtree);
        v = m_Solution;
        if (CDirEntry::IsAbsolutePath(v)) {
            try {
                v = CDirEntry::CreateRelativePath(m_Root, v);
            } catch (CFileException&) {
            }
        }
        m_CustomConfiguration.AddDefinition("__arg_solution", v);

        m_CustomConfiguration.AddDefinition("__arg_dll", m_Dll ? "yes" : "no");
        m_CustomConfiguration.AddDefinition("__arg_nobuildptb", m_BuildPtb ? "no" : "yes");
        m_CustomConfiguration.AddDefinition("__arg_ext", m_AddMissingLibs ? "yes" : "no");
        m_CustomConfiguration.AddDefinition("__arg_nws", m_ScanWholeTree ? "no" : "yes");
        m_CustomConfiguration.AddDefinition("__arg_extroot", m_BuildRoot);
        m_CustomConfiguration.AddDefinition("__arg_projtag", m_ProjTags);

#if defined(NCBI_COMPILER_MSVC) || defined(NCBI_XCODE_BUILD) || defined(PSEUDO_XCODE)
        m_CustomConfiguration.AddDefinition("__arg_ide", NStr::IntToString(m_Ide));
        m_CustomConfiguration.AddDefinition("__arg_arch", m_Arch);
#endif
        // this replaces path separators in entry[j] with a native one
        for (int j=0; !entry[j].empty(); ++j) {
            if (m_CustomConfiguration.GetPathValue(entry[j], v)) {
                m_CustomConfiguration.AddDefinition(entry[j], v);
            }
        }
    }
    if (CMsvc7RegSettings::GetMsvcPlatform() < CMsvc7RegSettings::eUnix) {
        string v;
        if (m_CustomConfiguration.GetValue("__TweakVTuneR", v)) {
            m_TweakVTuneR = NStr::StringToBool(v);
        }
        if (m_CustomConfiguration.GetValue("__TweakVTuneD", v)) {
            m_TweakVTuneD = NStr::StringToBool(v);
        }
        m_AddUnicode = GetSite().IsProvided("Ncbi_Unicode", false) ||
                       GetSite().IsProvided("Ncbi-Unicode", false);
        if (m_AddUnicode && !(
            GetSite().IsProvided("Ncbi_Unicode", false) &&
            GetSite().IsProvided("Ncbi-Unicode", false))) {
            //workaround to handle both
            string add;
            if (GetSite().IsProvided("Ncbi_Unicode", false)) {
                add = "Ncbi-Unicode";
            } else {
                add = "Ncbi_Unicode";
            }
            string section("__EnabledUserRequests");
            string value;
            m_CustomConfiguration.GetValue(section, value);
            if (!value.empty()) {
                value += " ";
            }
            value += add;            
            GetApp().m_CustomConfiguration.AddDefinition(section, value);
        }
    }
    tmp = GetConfig().Get("Configure", "UserRequests");
    if (!tmp.empty()) {
        m_CustomConfiguration.AddDefinition("__UserRequests", tmp);
    } else {
        m_CustomConfiguration.RemoveDefinition("__UserRequests");
    }
    if ( m_MsvcRegSettings.get() ) {
        GetBuildConfigs(&m_MsvcRegSettings->m_ConfigInfo);
    }
    m_AbsDirs.clear();
    for (int j=0; !entry[j].empty(); ++j) {
        string v;
        if (m_CustomConfiguration.GetPathValue(entry[j], v)) {
            m_AbsDirs.push_back(v);
        }
    }
}

void CProjBulderApp::VerifyArguments(void)
{
    m_Root = CDirEntry::AddTrailingPathSeparator(m_Root);
    if (CMsvc7RegSettings::GetMsvcPlatform() < CMsvc7RegSettings::eUnix) {
        NStr::ToLower(m_Root);
    }

    m_IncDir = GetProjectTreeInfo().m_Compilers;
    if (CMsvc7RegSettings::GetMsvcPlatform() == CMsvc7RegSettings::eUnix) {
        m_IncDir = CDirEntry(m_Solution).GetDir();
        m_IncDir = CDirEntry::ConcatPath(m_IncDir,"..");
    } else {
        m_IncDir = CDirEntry::ConcatPath(m_IncDir,GetRegSettings().m_CompilersSubdir);
        m_IncDir = CDirEntry::ConcatPath(m_IncDir, GetBuildType().GetTypeStr());
    }
    m_IncDir = CDirEntry::ConcatPath(m_IncDir, "inc");
    m_IncDir = CDirEntry::ConcatPath(m_IncDir, CMsvc7RegSettings::GetConfigNameKeyword());
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

bool CProjBulderApp::UseAbsolutePath(const string& path) const
{
    ITERATE(list<string>, p, m_AbsDirs) {
        if (NStr::strncasecmp(path.c_str(), p->c_str(), p->length()) == 0) {
            return true;
        }
    }
    return false;
}

void CProjBulderApp::AddCustomMetaData(const string& file)
{
    string s( CDirEntry::CreateRelativePath(GetProjectTreeInfo().m_Src, file));
    if ( find(m_CustomMetaData.begin(), m_CustomMetaData.end(), s) ==
              m_CustomMetaData.end()) {
        m_CustomMetaData.push_back( s );
    }
}

void CProjBulderApp::GetMetaDataFiles(list<string>* files) const
{
    *files = m_CustomMetaData;
    if (CMsvc7RegSettings::GetMsvcPlatform() != CMsvc7RegSettings::eUnix) {
        NStr::Split(GetConfig().Get("ProjectTree", "MetaData"), LIST_SEPARATOR,
                    *files);
    } else {
        string name(GetApp().GetSite().GetConfigureEntry("MetaData"));
        if (!name.empty()) {
            files->push_back( name);
        }
    }
}

void CProjBulderApp::AddCustomConfH(const string& file)
{
    m_CustomConfH.push_back(file);
}

void CProjBulderApp::GetCustomConfH(list<string>* files) const
{
    *files = m_CustomConfH;
}


void CProjBulderApp::GetBuildConfigs(list<SConfigInfo>* configs)
{
    configs->clear();
    string name = m_Dll ? "DllConfigurations" : "Configurations";
    const string& config_str
      = GetConfig().Get(CMsvc7RegSettings::GetMsvcSection(), name);
    list<string> configs_list;
    NStr::Split(config_str, LIST_SEPARATOR, configs_list);
    LoadConfigInfoByNames(GetConfig(), configs_list, configs);
}


const CMsvc7RegSettings& CProjBulderApp::GetRegSettings(void)
{
    if ( !m_MsvcRegSettings.get() ) {
        m_MsvcRegSettings.reset(new CMsvc7RegSettings());

        string section(CMsvc7RegSettings::GetMsvcRegSection());

        m_MsvcRegSettings->m_MakefilesExt = 
            GetConfig().GetString(section, "MakefilesExt", "msvc");
    
        m_MsvcRegSettings->m_ProjectsSubdir  = 
            GetConfig().GetString(section, "Projects", "build");

        m_MsvcRegSettings->m_MetaMakefile = CDirEntry::ConvertToOSPath(
            GetConfig().Get(section, "MetaMakefile"));

        m_MsvcRegSettings->m_DllInfo = 
            GetConfig().Get(section, "DllInfo");
    
        m_MsvcRegSettings->m_Version = 
            GetConfig().Get(CMsvc7RegSettings::GetMsvcSection(), "Version");

        m_MsvcRegSettings->m_CompilersSubdir  = 
            GetConfig().Get(CMsvc7RegSettings::GetMsvcSection(), "msvc_prj");

        GetBuildConfigs(&m_MsvcRegSettings->m_ConfigInfo);
    }
    return *m_MsvcRegSettings;
}


const CMsvcSite& CProjBulderApp::GetSite(void)
{
    if ( !m_MsvcSite.get() ) {
        m_MsvcSite.reset(new CMsvcSite(GetConfigPath()));
    }
    
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
    PTB_INFO("Project tree root: " << m_Root);

    // all possible project tags
    string tagsfile = CDirEntry::ConvertToOSPath(
        GetConfig().Get("ProjectTree", "ProjectTags"));
    if (!tagsfile.empty()) {
        string fileloc(CDirEntry::ConcatPath(m_ProjectTreeInfo->m_Root, tagsfile));
        if (!CDirEntry(fileloc).Exists() && !m_ExtSrcRoot.empty()) {
            fileloc = CDirEntry::ConcatPath(m_ExtSrcRoot,tagsfile);
        }
        LoadProjectTags(fileloc);
    }

    //dependencies
    list<string> depsfile;
    if (!FindDepGraph(m_ProjectTreeInfo->m_Root, depsfile) && !m_ExtSrcRoot.empty()) {
        FindDepGraph(m_ExtSrcRoot, depsfile);
    }
    if (!depsfile.empty()) {
        ITERATE(list<string>, d, depsfile) {
            PTB_INFO("Library dependencies graph: " << *d);
            LoadDepGraph(*d);
        }
    }
    
    /// <include> branch of tree
    string include = GetConfig().Get("ProjectTree", "include");
    m_ProjectTreeInfo->m_Include = 
            CDirEntry::ConcatPath(m_ProjectTreeInfo->m_Root, 
                                  include);
    m_ProjectTreeInfo->m_Include = 
        CDirEntry::AddTrailingPathSeparator(m_ProjectTreeInfo->m_Include);
    

    /// <src> branch of tree
    string src = GetConfig().Get("ProjectTree", "src");
    m_ProjectTreeInfo->m_Src = 
            CDirEntry::ConcatPath(m_ProjectTreeInfo->m_Root, 
                                  src);
    m_ProjectTreeInfo->m_Src =
        CDirEntry::AddTrailingPathSeparator(m_ProjectTreeInfo->m_Src);

    // Subtree to build - projects filter
    string subtree = CDirEntry::ConcatPath(m_ProjectTreeInfo->m_Root, m_Subtree);
    LOG_POST(Info << "Project list or subtree: " << subtree);
    if (!CDirEntry(subtree).Exists()) {
        LOG_POST(Info << "WARNING: " << subtree << " does not exist");
    }
    m_ProjectTreeInfo->m_IProjectFilter.reset(
        new CProjectsLstFileFilter(m_ProjectTreeInfo->m_Src, subtree));

    /// <compilers> branch of tree
    string compilers = GetConfig().Get("ProjectTree", "compilers");
    m_ProjectTreeInfo->m_Compilers = 
            CDirEntry::ConcatPath(m_ProjectTreeInfo->m_Root, 
                                  compilers);
    m_ProjectTreeInfo->m_Compilers = 
        CDirEntry::AddTrailingPathSeparator
                   (m_ProjectTreeInfo->m_Compilers);

    /// ImplicitExcludedBranches - all subdirs will be excluded by default
    string implicit_exclude_str 
        = GetConfig().Get("ProjectTree", "ImplicitExclude");
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
    string projects = CDirEntry::ConvertToOSPath(
        GetConfig().Get("ProjectTree", "projects"));
    m_ProjectTreeInfo->m_Projects = 
            CDirEntry::ConcatPath(m_ProjectTreeInfo->m_Root, 
                                  projects);
    m_ProjectTreeInfo->m_Projects = 
        CDirEntry::AddTrailingPathSeparator
                   (m_ProjectTreeInfo->m_Compilers);

    /// impl part if include project node
    m_ProjectTreeInfo->m_Impl = 
        GetConfig().Get("ProjectTree", "impl");

    /// Makefile in tree node
    m_ProjectTreeInfo->m_TreeNode = 
        GetConfig().Get("ProjectTree", "TreeNode");

    m_ProjectTreeInfo->m_CustomMetaData =
        GetConfig().Get("ProjectTree", "CustomMetaData");
    m_ProjectTreeInfo->m_CustomConfH =
        GetConfig().Get("ProjectTree", "CustomConfH");

    return *m_ProjectTreeInfo;
}


const CBuildType& CProjBulderApp::GetBuildType(void)
{
    if ( !m_BuildType.get() ) {
        m_BuildType.reset(new CBuildType(m_Dll));
    }    
    return *m_BuildType;
}

const CProjectItemsTree& CProjBulderApp::GetWholeTree(void)
{
    if ( !m_WholeTree.get() ) {
        m_WholeTree.reset(new CProjectItemsTree);
        if (m_ScanWholeTree) {
            m_ScanningWholeTree = true;
            CProjectsLstFileFilter pass_all_filter(m_ProjectTreeInfo->m_Src, m_ProjectTreeInfo->m_Src);
//            pass_all_filter.SetExcludePotential(false);
            CProjectTreeBuilder::BuildProjectTree(&pass_all_filter, 
                                                GetProjectTreeInfo().m_Src, 
                                                m_WholeTree.get());
            m_ScanningWholeTree = false;
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

string CProjBulderApp::GetDataspecProjId(void) const
{
    return "_generate_all_objects";
}

string CProjBulderApp::GetDatatoolId(void) const
{
    return GetConfig().GetString("Datatool", "datatool",
        CMsvc7RegSettings::GetMsvcPlatform() >= CMsvc7RegSettings::eUnix ? "datatool" : "");
}


string CProjBulderApp::GetDatatoolPathForApp(void) const
{
    if (CMsvc7RegSettings::GetMsvcPlatform() == CMsvc7RegSettings::eXCode) {
        return GetConfig().GetString("Datatool", "Location.xcode", "datatool");
    }
    return GetConfig().GetString("Datatool", "Location.App", "datatool.exe");
}


string CProjBulderApp::GetDatatoolPathForLib(void) const
{
    if (CMsvc7RegSettings::GetMsvcPlatform() == CMsvc7RegSettings::eXCode) {
        return GetConfig().GetString("Datatool", "Location.xcode", "datatool");
    }
    return GetConfig().GetString("Datatool", "Location.Lib", "datatool.exe");
}


string CProjBulderApp::GetDatatoolCommandLine(void) const
{
    if (CMsvc7RegSettings::GetMsvcPlatform() == CMsvc7RegSettings::eXCode) {
        return GetConfig().GetString("Datatool", "CommandLine.xcode", "");
    }
    return GetConfig().Get("Datatool", "CommandLine");
}

string CProjBulderApp::GetProjectTreeRoot(void) const
{
    string path = CDirEntry::ConcatPath(
        m_ProjectTreeInfo->m_Compilers,
        m_MsvcRegSettings->m_CompilersSubdir);
    return CDirEntry::AddTrailingPathSeparator(path);
}

bool CProjBulderApp::IsAllowedProjectTag(
    const CProjItem& project, const string* filter /*= NULL*/) const
{
    // verify that all project tags are registered
    list<string>::const_iterator i;
    for (i = project.m_ProjTags.begin(); i != project.m_ProjTags.end(); ++i) {
        if (m_RegisteredProjectTags.find(*i) == m_RegisteredProjectTags.end()) {
            NCBI_THROW(CProjBulderAppException, eUnknownProjectTag,
                project.GetPath() + ": Unregistered project tag: " + *i);
            return false;
        }
    }

    if (filter == NULL) {
        filter = &m_ProjTags;
    }
    // no filter - everything is allowed
    if (filter->empty() || *filter == "*") {
        return true;
    }

    CExprParser parser;
    ITERATE( set<string>, p, m_RegisteredProjectTags) {
        parser.AddSymbol(p->c_str(),
            find( project.m_ProjTags.begin(), project.m_ProjTags.end(), *p) != project.m_ProjTags.end());
    }
    parser.Parse(filter->c_str());
    return parser.GetResult().GetBool();
}

void CProjBulderApp::LoadProjectTags(const string& filename)
{
    CNcbiIfstream ifs(filename.c_str(), IOS_BASE::in | IOS_BASE::binary);
    if ( ifs.is_open() ) {
        string line;
        while ( NcbiGetlineEOL(ifs, line) ) {
            if (line.empty() || line[0] == '#') {
                continue;
            }
            list<string> values;
            if (line.find('=') != string::npos) {
                NStr::Split(line, "=", values);
                if (values.size() > 1) {
                    string first = NStr::TruncateSpaces(values.front());
                    string second = NStr::TruncateSpaces(values.back());
                    m_CompositeProjectTags[first] = second;
                    
                }
                continue;
            }
            NStr::Split(line, LIST_SEPARATOR, values);
            ITERATE(list<string>,v,values) {
                m_RegisteredProjectTags.insert(*v);
            }
        }
    }
    m_RegisteredProjectTags.insert("exe");
    m_RegisteredProjectTags.insert("lib");
    m_RegisteredProjectTags.insert("dll");
    m_RegisteredProjectTags.insert("public");
    m_RegisteredProjectTags.insert("internal");
}

bool  CProjBulderApp::FindDepGraph(const string& root, list<string>& found) const
{
    found.clear();
    list<string> locations;
    string locstr(GetConfig().Get("ProjectTree", "DepGraph"));
    NStr::Split(locstr, LIST_SEPARATOR, locations);
    for (list<string>::const_iterator l = locations.begin(); l != locations.end(); ++l) {
        CDirEntry fileloc(CDirEntry::ConcatPath(root,CDirEntry::ConvertToOSPath(*l)));
        if (fileloc.Exists() && fileloc.IsFile()) {
            found.push_back(fileloc.GetPath());
        }
    }
    return !found.empty();
}

void   CProjBulderApp::LoadDepGraph(const string& filename)
{
    const CMsvcSite& site = GetSite();
    CNcbiIfstream ifs(filename.c_str(), IOS_BASE::in);
    if ( ifs.is_open() ) {
        string line;
        while ( NcbiGetlineEOL(ifs, line) ) {
            if (line.empty() || line[0] == '#') {
                continue;
            }
            list<string> values;
            NStr::Split(line, " ", values);
            if (values.size() > 2) {
                list<string>::const_iterator l= values.begin();
                string first  = *l++;
                string second = *l++;
                string third  = *l++;

                list<string> first_list;
                if (CSymResolver::IsDefine(first)) {
                    string resolved;
                    site.ResolveDefine(CSymResolver::StripDefine(first), resolved);
                    first = resolved;
                }
                NStr::Split(first, LIST_SEPARATOR_LIBS, first_list);

                list<string> third_list;
#if DO_PATCHTREEMAKEFILES
                third_list.push_back(third);
#else
                if (CSymResolver::IsDefine(third)) {
                    string resolved;
                    if (CMsvc7RegSettings::GetMsvcPlatform() != CMsvc7RegSettings::eUnix) {
                        string stripped(CSymResolver::StripDefine(third));
                        if (stripped != "NCBI_C_ncbi") {
                            site.ResolveDefine(stripped, resolved);
                        }
                        if (resolved.empty()) {
                            resolved = "@" + stripped + "@";
                        }
                    } else {
                        site.ResolveDefine(CSymResolver::StripDefine(third), resolved);
                    }
                    third = resolved;
                }
                NStr::Split(third, LIST_SEPARATOR_LIBS, third_list);
#endif
                ITERATE(list<string>, f, first_list) {
                    string f_name = NStr::StartsWith(*f, "-l") ? f->substr(2) : *f;
                    if (f_name.at(0) == '-') {
                        continue;
                    }
                    CSymResolver::StripSuffix(f_name);
                    ITERATE(list<string>, t, third_list) {
                        string t_name(*t);
                        CSymResolver::StripSuffix(t_name);
                        bool is_lib = false;
                        bool is_framework = false;
                        if (NStr::StartsWith(*t, "-l")) {
                            is_lib = true;
                            t_name = t->substr(2);
                        } else if (NStr::CompareNocase(*t,"-framework") == 0) {
                            if (t != third_list.end()) {
                                is_framework = true;
                                t_name = *(++t);
                            } else {
                                continue;
                            }
                        } else {
                            is_lib = t->at(0) != '-';
                        }
                        if (*f == t_name) {
                            continue;
                        }
                        if (!is_lib && !is_framework) {
                            if (second == "needs3party") {
                                m_GraphDepFlags[*f].insert(t_name);
                            }
                            continue;
                        }
                        if (second == "includes") {
                            m_GraphDepIncludes[*f].insert(t_name);
                        } else if (second == "includes3party") {
                            m_GraphDepIncludes[*f].insert(t_name);
                            m_3PartyLibs.insert(t_name);
                            if (is_framework) {
                                m_Frameworks.insert(t_name);
                            }
                        } else if (second == "needs") {
                            m_GraphDepPrecedes[*f].insert(t_name);
                        } else if (second == "needs3party") {
                            m_GraphDepPrecedes[*f].insert(t_name);
                            m_3PartyLibs.insert(t_name);
                            if (is_framework) {
                                m_Frameworks.insert(t_name);
                            }
                        }
                    }
                }
            }
        }
    }

#if !DO_PATCHTREEMAKEFILES
    if (CMsvc7RegSettings::GetMsvcPlatform() != CMsvc7RegSettings::eUnix) {
        return;
    }
    vector< set<string> > graph;
    for (map<string, set<string> >::const_iterator d= m_GraphDepPrecedes.begin();
            d!= m_GraphDepPrecedes.end(); ++d) {
        InsertDep(graph, d->first);
    }
    for (size_t s= 0; s<graph.size(); ++s) {
        for (set<string>::const_iterator l = graph[s].begin(); l != graph[s].end(); ++l) {
            m_GraphDepRank[*l] = s;
        }
    }
#endif
}

void  CProjBulderApp::InsertDep(vector< set<string> >& graph, const string& dep)
{
    const set<string>& dependents = m_GraphDepPrecedes[dep];
    size_t graphset=0;
    for (set<string>::const_iterator d = dependents.begin(); d != dependents.end(); ++d) {
        for (bool found=false; !found; ) {
            for (size_t s= 0; !found && s<graph.size(); ++s) {
                if (graph[s].find(*d) != graph[s].end()) {
                    graphset = max(graphset,s);
                    found = true;
                }
            }
            if (!found) {
                InsertDep(graph, *d);
            }
        }
    }
    if (!dependents.empty()) {
        ++graphset;
    }
    if (graphset < graph.size()) {
        graph[graphset].insert(dep);
    } else {
        set<string> t;
        t.insert(dep);
        graph.push_back(t);
    }
}

string CProjBulderApp::ProcessLocationMacros(string raw_data)
{
    string data(raw_data), raw_macro, macro, definition;
    string::size_type start, end, done = 0;
    while ((start = data.find("$(", done)) != string::npos) {
        end = data.find(")", start);
        if (end == string::npos) {
            LOG_POST(Warning << "Possibly incorrect MACRO definition in: " + raw_data);
            return data;
        }
        raw_macro = data.substr(start,end-start+1);
        if (CSymResolver::IsDefine(raw_macro)) {
            macro = CSymResolver::StripDefine(raw_macro);
            definition.erase();
            definition = GetConfig().GetString(CMsvc7RegSettings::GetMsvcSection(), macro, "");
            if (!definition.empty()) {
                definition = CDirEntry::ConcatPath(
                    m_ProjectTreeInfo->m_Compilers, definition);
                data = NStr::Replace(data, raw_macro, definition);
            } else {
                done = end;
            }
        }
    }
    return data;
}

string CProjBulderApp::GetConfigureMacro(string data)
{
    string definition = GetConfig().GetString(CMsvc7RegSettings::GetMsvcSection(), data, "");
    if (!definition.empty()) {
        definition = CDirEntry::ConcatPath( m_ProjectTreeInfo->m_Compilers, definition);
    }
    return definition;
}

void CProjBulderApp::RegisterSuspiciousProject(const CProjKey& proj)
{
    m_SuspiciousProj.insert(proj);
}

void CProjBulderApp::RegisterGeneratedFile( const string& file)
{
    m_GeneratedFiles.push_back(file);
}

void CProjBulderApp::RegisterProjectWatcher(
    const string& project, const string& dir,  const string& watcher)
{
    if (watcher.empty()) {
        return;
    }
    string sep;
    sep += CDirEntry::GetPathSeparator();
    string root(GetProjectTreeInfo().m_Src);
    if (!CDirEntry::IsAbsolutePath(root)) {
        root = CDirEntry::ConcatPath(CDir::GetCwd(), root);
    }
    string path(dir);
    if (!CDirEntry::IsAbsolutePath(path)) {
        path = CDirEntry::ConcatPath(CDir::GetCwd(), path);
    }
    path = CDirEntry::DeleteTrailingPathSeparator(
        CDirEntry::CreateRelativePath(root, path));
    NStr::ReplaceInPlace( path, sep, "/");
    m_ProjWatchers.push_back( project + ", " + path + ", " + watcher );
}

void CProjBulderApp::ExcludeProjectsByTag(CProjectItemsTree& tree) const
{
    EraseIf(tree.m_Projects, PIsExcludedByTag());
    if (!m_ProjTags.empty() && m_ProjTags != "*") {
        NON_CONST_ITERATE(CProjectItemsTree::TProjects, p, tree.m_Projects) {
            if (p->second.m_ProjType == CProjKey::eDll) {
                p->second.m_External = false;
            }
        }
    }
}

void CProjBulderApp::ExcludeUnrequestedProjects(CProjectItemsTree& tree) const
{
    EraseIf(tree.m_Projects, PIsExcludedByUser());
}

string CProjBulderApp::GetUtilityProjectsDir(void) const
{
    string utility_projects_dir = CDirEntry(m_Solution).GetDir();
    utility_projects_dir = 
        CDirEntry::ConcatPath(utility_projects_dir, "UtilityProjects");
    utility_projects_dir = 
        CDirEntry::AddTrailingPathSeparator(utility_projects_dir);
    return utility_projects_dir;
}

string CProjBulderApp::GetUtilityProjectsSrcDir(void)
{
    string prj = GetProjectTreeInfo().m_Compilers;
    prj = CDirEntry::ConcatPath(prj, GetRegSettings().m_CompilersSubdir);
    prj = CDirEntry::ConcatPath(prj, GetBuildType().GetTypeStr());
    prj = CDirEntry::ConcatPath(prj, GetRegSettings().m_ProjectsSubdir);

    string sln = CDirEntry(m_Solution).GetDir();
    prj = CDirEntry::CreateRelativePath( prj, sln);
    prj = CDirEntry::ConcatPath(GetProjectTreeInfo().m_Src, prj);
    prj = CDirEntry::ConcatPath(prj, "UtilityProjects");
    prj = CDirEntry::AddTrailingPathSeparator(prj);
    return prj;
}

void CProjBulderApp::SetConfFileData(const string& src, const string& dest)
{
    m_ConfSrc = src;
    m_ConfDest= dest;
}

CProjBulderApp& GetApp(void)
{
    static CProjBulderApp theApp;
    return theApp;
}

END_NCBI_SCOPE


USING_NCBI_SCOPE;

int NcbiSys_main(int argc, TXChar* argv[])
{
    // Execute main application function
    CDiagContext::SetLogTruncate(true);
    GetDiagContext().SetLogRate_Limit(CDiagContext::eLogRate_Err, (unsigned int)-1);
    return GetApp().AppMain(argc, argv, 0, eDS_Default);
}
