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
#include <app/project_tree_builder/stl_msvc_usage.hpp>

#include <app/project_tree_builder/msvc_project_context.hpp>
#include <app/project_tree_builder/msvc_tools_implement.hpp>
#include <app/project_tree_builder/proj_builder_app.hpp>
#include <app/project_tree_builder/msvc_site.hpp>
#include <app/project_tree_builder/msvc_prj_defines.hpp>

#include <algorithm>
#include <set>


BEGIN_NCBI_SCOPE

map<string, set<string> > CMsvcPrjProjectContext::s_EnabledPackages;
map<string, set<string> > CMsvcPrjProjectContext::s_DisabledPackages;

//-----------------------------------------------------------------------------
CMsvcPrjProjectContext::CMsvcPrjProjectContext(const CProjItem& project)
{
    m_MakeType = project.m_MakeType;
    //MSVC project name created from project type and project ID
    m_ProjectName  = CreateProjectName(CProjKey(project.m_ProjType, 
                                                project.m_ID));
    m_ProjectId    = project.m_ID;
    m_ProjType     = project.m_ProjType;

    m_SourcesBaseDir = project.m_SourcesBaseDir;
    m_Requires       = project.m_Requires;
    
    // Get msvc project makefile
    m_MsvcProjectMakefile.reset
        (new CMsvcProjectMakefile
                    (CDirEntry::ConcatPath
                            (project.m_MsvcProjectMakefileDir, 
                             CreateMsvcProjectMakefileName(project))));

    // Done if this is ready MSVC project
    if ( project.m_ProjType == CProjKey::eMsvc)
        return;

    // Collect all dirs of source files into m_SourcesDirsAbs:
    set<string> sources_dirs;
    sources_dirs.insert(m_SourcesBaseDir);
    ITERATE(list<string>, p, project.m_Sources) {

        const string& src_rel = *p;
        string src_path = CDirEntry::ConcatPath(m_SourcesBaseDir, src_rel);
        src_path = CDirEntry::NormalizePath(src_path);

        string dir;
        CDirEntry::SplitPath(src_path, &dir);
        sources_dirs.insert(dir);
    }
    copy(sources_dirs.begin(), 
         sources_dirs.end(), 
         back_inserter(m_SourcesDirsAbs));


    // Creating project dir:
    if (project.m_ProjType == CProjKey::eDll) {
        //For dll - it is so
        m_ProjectDir = project.m_SourcesBaseDir;

    } else {
        m_ProjectDir = GetApp().GetProjectTreeInfo().m_Compilers;
        m_ProjectDir = 
            CDirEntry::ConcatPath(m_ProjectDir, 
                                  GetApp().GetRegSettings().m_CompilersSubdir);
        m_ProjectDir = 
            CDirEntry::ConcatPath(m_ProjectDir, 
                                  GetApp().GetBuildType().GetTypeStr());
        m_ProjectDir =
            CDirEntry::ConcatPath(m_ProjectDir,
                                  GetApp().GetRegSettings().m_ProjectsSubdir);
        m_ProjectDir = 
            CDirEntry::ConcatPath(m_ProjectDir, 
                                  CDirEntry::CreateRelativePath
                                      (GetApp().GetProjectTreeInfo().m_Src, 
                                      m_SourcesBaseDir));
        m_ProjectDir = CDirEntry::AddTrailingPathSeparator(m_ProjectDir);
    }

    // Generate include dirs:
    // Include dirs for appropriate src dirs
    set<string> include_dirs;
    ITERATE(list<string>, p, project.m_Sources) {
        //create full path for src file
        const string& src_rel = *p;
        string src_abs  = CDirEntry::ConcatPath(m_SourcesBaseDir, src_rel);
        src_abs = CDirEntry::NormalizePath(src_abs);
        //part of path (from <src> dir)
        string rel_path  = 
            CDirEntry::CreateRelativePath(GetApp().GetProjectTreeInfo().m_Src, 
                                          src_abs);
        //add this part to <include> dir
        string incl_path = 
            CDirEntry::ConcatPath(GetApp().GetProjectTreeInfo().m_Include, 
                                  rel_path);
        string incl_dir;
        CDirEntry::SplitPath(incl_path, &incl_dir);
        include_dirs.insert(incl_dir);

        //impl include sub-dir
        string impl_dir = 
            CDirEntry::ConcatPath(incl_dir, 
                                  GetApp().GetProjectTreeInfo().m_Impl);
        impl_dir = CDirEntry::AddTrailingPathSeparator(impl_dir);
        include_dirs.insert(impl_dir);
    }
    SConfigInfo cfg_info; // default is enough
    list<string> headers_in_include;
    list<string> inlines_in_include;
    set<string>::const_iterator i;
    list<string>::const_iterator h, hs;
    GetMsvcProjectMakefile().GetHeadersInInclude( cfg_info, &headers_in_include);
    GetMsvcProjectMakefile().GetInlinesInInclude( cfg_info, &inlines_in_include);
    for (i = include_dirs.begin(); i != include_dirs.end(); ++i) {
        for (h = headers_in_include.begin(); h != headers_in_include.end(); ++h) {
            m_IncludeDirsAbs.push_back(CDirEntry::ConcatPath(*i, *h));
        }
        for (h = inlines_in_include.begin(); h != inlines_in_include.end(); ++h) {
            m_InlineDirsAbs.push_back(CDirEntry::ConcatPath(*i, *h));
        }
    }
    list<string> headers_in_src;
    list<string> inlines_in_src;
    GetMsvcProjectMakefile().GetHeadersInSrc( cfg_info, &headers_in_src);
    GetMsvcProjectMakefile().GetInlinesInSrc( cfg_info, &inlines_in_src);
    for (hs = m_SourcesDirsAbs.begin(); hs != m_SourcesDirsAbs.end(); ++hs) {
        for (h = headers_in_src.begin(); h != headers_in_src.end(); ++h) {
            m_IncludeDirsAbs.push_back(CDirEntry::ConcatPath(*hs, *h));
        }
        for (h = inlines_in_src.begin(); h != inlines_in_src.end(); ++h) {
            m_InlineDirsAbs.push_back(CDirEntry::ConcatPath(*hs, *h));
        }
    }

    // Get custom build files and adjust pathes 
    GetMsvcProjectMakefile().GetCustomBuildInfo(&m_CustomBuildInfo);
    NON_CONST_ITERATE(list<SCustomBuildInfo>, p, m_CustomBuildInfo) {
       SCustomBuildInfo& build_info = *p;
       string file_path_abs = 
           CDirEntry::ConcatPath(m_SourcesBaseDir, build_info.m_SourceFile);
       build_info.m_SourceFile = 
           CDirEntry::CreateRelativePath(m_ProjectDir, file_path_abs);           
    }

    // Collect include dirs, specified in project Makefiles
    m_ProjectIncludeDirs = project.m_IncludeDirs;

    // LIBS from Makefiles
    // m_ProjectLibs = project.m_Libs3Party;
    list<string> installed_3party;
    GetApp().GetSite().GetThirdPartyLibsToInstall(&installed_3party);

    ITERATE(list<string>, p, project.m_Libs3Party) {
        const string& lib_id = *p;
        if ( GetApp().GetSite().IsLibWithChoice(lib_id) ) {
            if ( GetApp().GetSite().GetChoiceForLib(lib_id) == CMsvcSite::eLib )
                m_ProjectLibs.push_back(lib_id);
        } else {
            m_ProjectLibs.push_back(lib_id);
        }

        ITERATE(list<string>, i, installed_3party) {
            const string& component = *i;
            bool lib_ok = true;
            ITERATE(list<SConfigInfo>, j, GetApp().GetRegSettings().m_ConfigInfo) {
                const SConfigInfo& config = *j;
                SLibInfo lib_info;
                GetApp().GetSite().GetLibInfo(component, config, &lib_info);
                if (find( lib_info.m_Macro.begin(), lib_info.m_Macro.end(), lib_id) ==
                          lib_info.m_Macro.end()) {
                    lib_ok = false;
                    break;
                }
            }
            if (lib_ok) {
                m_Requires.push_back(component);
            }
        }
    }
    m_Requires.sort();
    m_Requires.unique();

    // Proprocessor definitions from makefiles:
    m_Defines = project.m_Defines;
    if (GetApp().GetBuildType().GetType() == CBuildType::eDll) {
        m_Defines.push_back(GetApp().GetDllsInfo().GetBuildDefine());
        if (project.m_ProjType == CProjKey::eDll) {
            CMsvcDllsInfo::SDllInfo dll_info;
            GetApp().GetDllsInfo().GetDllInfo(project.m_ID, &dll_info);
            m_Defines.push_back(dll_info.m_DllDefine);
        }
    }
    // Pre-Builds for LIB projects:
    if (m_ProjType == CProjKey::eLib) {
        ITERATE(list<CProjKey>, p, project.m_Depends) {
            const CProjKey& proj_key = *p;
            if (proj_key.Type() == CProjKey::eLib) {
                m_PreBuilds.push_back(CreateProjectName(proj_key));
            }
        }
    }

    // Libraries from NCBI C Toolkit
    m_NcbiCLibs = project.m_NcbiCLibs;
}


string CMsvcPrjProjectContext::AdditionalIncludeDirectories
                                            (const SConfigInfo& cfg_info) const
{
    list<string> add_include_dirs_list;
    list<string> dirs;
    string dir;

    // project dir
    add_include_dirs_list.push_back 
        (CDirEntry::CreateRelativePath(m_ProjectDir, 
                                      GetApp().GetProjectTreeInfo().m_Include));
    
    //take into account project include dirs
    ITERATE(list<string>, p, m_ProjectIncludeDirs) {
        const string& dir_abs = *p;
        dirs.clear();
        if (CSymResolver::IsDefine(dir_abs)) {
            GetApp().GetSite().GetLibInclude( dir_abs, cfg_info, &dirs);
        } else {
            dirs.push_back(dir_abs);
        }
        for (list<string>::const_iterator i = dirs.begin(); i != dirs.end(); ++i) {
            dir = *i;
            add_include_dirs_list.push_back(SameRootDirs(m_ProjectDir,dir) ?
                    CDirEntry::CreateRelativePath(m_ProjectDir, dir) :
                    dir);
        }
    }

    //MSVC Makefile additional include dirs
    list<string> makefile_add_incl_dirs;
    GetMsvcProjectMakefile().GetAdditionalIncludeDirs(cfg_info, 
                                                    &makefile_add_incl_dirs);

    ITERATE(list<string>, p, makefile_add_incl_dirs) {
        const string& dir = *p;
        string dir_abs = 
            CDirEntry::AddTrailingPathSeparator
                (CDirEntry::ConcatPath(m_SourcesBaseDir, dir));
        dir_abs = CDirEntry::NormalizePath(dir_abs);
        dir_abs = 
            CDirEntry::CreateRelativePath
                        (m_ProjectDir, dir_abs);
        add_include_dirs_list.push_back(dir_abs);
    }

    // Additional include dirs for 3-party libs
    list<string> libs_list;
    CreateLibsList(&libs_list);
    ITERATE(list<string>, p, libs_list) {
        if ( *p == string(MSVC_DEFAULT_LIBS_TAG)) {
            continue;
        }
        GetApp().GetSite().GetLibInclude(*p, cfg_info, &dirs);
        for (list<string>::const_iterator i = dirs.begin(); i != dirs.end(); ++i) {
            dir = *i;
            if ( !dir.empty() ) {
                if (SameRootDirs(m_ProjectDir,dir)) {
                    dir = CDirEntry::CreateRelativePath(m_ProjectDir, dir);
                }
                if (find(add_include_dirs_list.begin(),
                    add_include_dirs_list.end(), dir) !=add_include_dirs_list.end()) {
                    continue;
                }
                add_include_dirs_list.push_back(dir);
            }
        }
    }

    //Leave only unique dirs and join them to string
//    add_include_dirs_list.sort();
//    add_include_dirs_list.unique();
    return NStr::Join(add_include_dirs_list, ", ");
}


string CMsvcPrjProjectContext::AdditionalLinkerOptions
                                            (const SConfigInfo& cfg_info) const
{
    list<string> additional_libs;

    // Take into account requires, default and makefiles libs
    list<string> libs_list;
    CreateLibsList(&libs_list);
    ITERATE(list<string>, p, libs_list) {
        const string& requires = *p;
        if (GetApp().GetSite().Is3PartyLibWithChoice(requires)) {
            if (GetApp().GetSite().GetChoiceFor3PartyLib(requires, cfg_info) == CMsvcSite::eLib) {
                continue;
            }
        }
        SLibInfo lib_info;
        GetApp().GetSite().GetLibInfo(requires, cfg_info, &lib_info);
        if ( CMsvcSite::IsLibOk(lib_info, true) ) {
            if ( !lib_info.m_Libs.empty() ) {
                copy(lib_info.m_Libs.begin(), lib_info.m_Libs.end(), 
                    back_inserter(additional_libs));
            }
            if ( !lib_info.m_StdLibs.empty() ) {
                copy(lib_info.m_StdLibs.begin(), lib_info.m_StdLibs.end(), 
                    back_inserter(additional_libs));
            }
        } else {
            if (!lib_info.IsEmpty()) {
                LOG_POST(Warning << requires << "|" << cfg_info.m_Name
                              << " unavailable: additional libraries ignored: "
                              << NStr::Join(lib_info.m_Libs,","));

            }
        }
    }

    // NCBI C Toolkit libs
    ITERATE(list<string>, p, m_NcbiCLibs) {
        string ncbi_lib = *p + ".lib";
        additional_libs.push_back(ncbi_lib);        
    }
    additional_libs.sort();
    additional_libs.unique();

    return NStr::Join(additional_libs, " ");
}

#if 0
string CMsvcPrjProjectContext::AdditionalLibrarianOptions
                                            (const SConfigInfo& cfg_info) const
{
    return AdditionalLinkerOptions(cfg_info);
}
#endif

string CMsvcPrjProjectContext::AdditionalLibraryDirectories
                                            (const SConfigInfo& cfg_info) const
{

    // Take into account requires, default and makefiles libs
    list<string> libs_list;
    CreateLibsList(&libs_list);
    list<string> dir_list;
    ITERATE(list<string>, p, libs_list) {
        const string& requires = *p;
        if (GetApp().GetSite().Is3PartyLibWithChoice(requires)) {
            if (GetApp().GetSite().GetChoiceFor3PartyLib(requires, cfg_info) == CMsvcSite::eLib) {
                continue;
            }
        }
        SLibInfo lib_info;
        GetApp().GetSite().GetLibInfo(requires, cfg_info, &lib_info);
        if ( CMsvcSite::IsLibOk(lib_info, true) ) {
            if ( !lib_info.m_LibPath.empty() ) {
                dir_list.push_back(lib_info.m_LibPath);
            }
        } else {
            if (!lib_info.IsEmpty()) {
                LOG_POST(Warning << requires << "|" << cfg_info.m_Name
                              << " unavailable: library folder  ignored: "
                              << lib_info.m_LibPath);
            }
        }
    }
    dir_list.sort();
    dir_list.unique();
    return NStr::Join(dir_list, ", ");
}


void CMsvcPrjProjectContext::CreateLibsList(list<string>* libs_list) const
{
    libs_list->clear();
    // We'll build libs list.
    *libs_list = m_Requires;
    //take into account default libs from site:
    libs_list->push_back(MSVC_DEFAULT_LIBS_TAG);
    //and LIBS from Makefiles:
    ITERATE(list<string>, p, m_ProjectLibs) {
        const string& lib = *p;
        list<string> components;
        GetApp().GetSite().GetComponents(lib, &components);
        copy(components.begin(), 
             components.end(), back_inserter(*libs_list));

    }
    libs_list->sort();
    libs_list->unique();
}

const CMsvcCombinedProjectMakefile& 
CMsvcPrjProjectContext::GetMsvcProjectMakefile(void) const
{
    if ( m_MsvcCombinedProjectMakefile.get() )
        return *m_MsvcCombinedProjectMakefile;

    string rules_dir = GetApp().GetProjectTreeInfo().m_Compilers;
    rules_dir = 
            CDirEntry::ConcatPath(rules_dir, 
                                  GetApp().GetRegSettings().m_CompilersSubdir);


    // temporary fix with const_cast
    (const_cast<auto_ptr<CMsvcCombinedProjectMakefile>&>
        (m_MsvcCombinedProjectMakefile)).reset(new CMsvcCombinedProjectMakefile
                                                  (m_ProjType,
                                                   m_MsvcProjectMakefile.get(),
                                                   rules_dir,
                                                   m_Requires));

    return *m_MsvcCombinedProjectMakefile;
}


bool CMsvcPrjProjectContext::IsRequiresOk(const CProjItem& prj, string* unmet)
{
    ITERATE(list<string>, p, prj.m_Requires) {
        const string& requires = *p;
        if ( !GetApp().GetSite().IsProvided(requires) ) {
            if (unmet) {
                *unmet = requires;
            }
            return false;
        }
    }
    return true;
}


bool CMsvcPrjProjectContext::IsConfigEnabled(const SConfigInfo& config, string* unmet) const
{
    list<string> libs_3party;
    ITERATE(list<string>, p, m_ProjectLibs) {
        const string& lib = *p;
        list<string> components;
        GetApp().GetSite().GetComponents(lib, &components);
        copy(components.begin(), 
             components.end(), back_inserter(libs_3party));
    }

    // Add requires to test : If there is such library and configuration for 
    // this library is disabled then we'll disable this config
    copy(m_Requires.begin(), m_Requires.end(), back_inserter(libs_3party));
    libs_3party.sort();
    libs_3party.unique();

    // Test third-party libs and requires:
    bool result = true;
    ITERATE(list<string>, p, libs_3party) {
        const string& requires = *p;
        SLibInfo lib_info;
        GetApp().GetSite().GetLibInfo(requires, config, &lib_info);
        
        if ( lib_info.IsEmpty() ) 
            continue;

        if ( !GetApp().GetSite().IsLibEnabledInConfig(requires, config) ) {
            if (unmet) {
                if (!unmet->empty()) {
                    *unmet += ", ";
                }
                *unmet += requires;
            }
            result = false;
            s_DisabledPackages[config.m_Name].insert(requires);
        } else {
            s_EnabledPackages[config.m_Name].insert(requires);
        }
    }

    return result;
}


const list<string> CMsvcPrjProjectContext::Defines(const SConfigInfo& cfg_info) const
{
    list<string> defines(m_Defines);

    list<string> libs_list;
    CreateLibsList(&libs_list);
    ITERATE(list<string>, p, libs_list) {
        const string& lib_id = *p;
        if (GetApp().GetSite().Is3PartyLibWithChoice(lib_id)) {
            if (GetApp().GetSite().GetChoiceFor3PartyLib(lib_id, cfg_info) == CMsvcSite::eLib) {
                continue;
            }
        }
        SLibInfo lib_info;
        GetApp().GetSite().GetLibInfo(lib_id, cfg_info, &lib_info);
        if ( !lib_info.m_LibDefines.empty() ) {
            copy(lib_info.m_LibDefines.begin(),
                 lib_info.m_LibDefines.end(),
                 back_inserter(defines));
        }
    }
    defines.sort();
    defines.unique();
    return defines;
}



//-----------------------------------------------------------------------------
CMsvcPrjGeneralContext::CMsvcPrjGeneralContext
    (const SConfigInfo&            config, 
     const CMsvcPrjProjectContext& prj_context)
     :m_Config          (config),
      m_MsvcMetaMakefile(GetApp().GetMetaMakefile())
{
    //m_Type
    switch ( prj_context.ProjectType() ) {
    case CProjKey::eLib:
        m_Type = eLib;
        break;
    case CProjKey::eApp:
        m_Type = eExe;
        break;
    case CProjKey::eDll:
        m_Type = eDll;
        break;
    default:
        //TODO - handle Dll(s)
   	    NCBI_THROW(CProjBulderAppException, eProjectType, 
                        NStr::IntToString(prj_context.ProjectType()));
    }
    

    //m_OutputDirectory;
    // /compilers/msvc7_prj/
    string output_dir_abs = GetApp().GetProjectTreeInfo().m_Compilers;
    output_dir_abs = 
            CDirEntry::ConcatPath(output_dir_abs, 
                                  GetApp().GetRegSettings().m_CompilersSubdir);
    output_dir_abs = 
            CDirEntry::ConcatPath(output_dir_abs, 
                                  GetApp().GetBuildType().GetTypeStr());
    if (m_Type == eLib)
        output_dir_abs = CDirEntry::ConcatPath(output_dir_abs, "lib");
    else if (m_Type == eExe)
        output_dir_abs = CDirEntry::ConcatPath(output_dir_abs, "bin");
    else if (m_Type == eDll) // same dir as exe 
        output_dir_abs = CDirEntry::ConcatPath(output_dir_abs, "bin"); 
    else {
        //TODO - handle Dll(s)
   	    NCBI_THROW(CProjBulderAppException, 
                   eProjectType, NStr::IntToString(m_Type));
    }

    output_dir_abs = 
        CDirEntry::ConcatPath(output_dir_abs, /*config.m_Name*/"$(ConfigurationName)");
    m_OutputDirectory = 
        CDirEntry::CreateRelativePath(prj_context.ProjectDir(), 
                                      output_dir_abs);

#if 0

    const string project_tag(string(1,CDirEntry::GetPathSeparator()) + 
                             "compilers" +
                             CDirEntry::GetPathSeparator() + 
                             GetApp().GetRegSettings().m_CompilersSubdir +
                             CDirEntry::GetPathSeparator());
    
    string project_dir = prj_context.ProjectDir();
    string output_dir_prefix = 
        string (project_dir, 
                0, 
                project_dir.find(project_tag) + project_tag.length());
    
    output_dir_prefix = 
        CDirEntry::ConcatPath(output_dir_prefix, 
                              GetApp().GetBuildType().GetTypeStr());

    if (m_Type == eLib)
        output_dir_prefix = CDirEntry::ConcatPath(output_dir_prefix, "lib");
    else if (m_Type == eExe)
        output_dir_prefix = CDirEntry::ConcatPath(output_dir_prefix, "bin");
    else if (m_Type == eDll) // same dir as exe 
        output_dir_prefix = CDirEntry::ConcatPath(output_dir_prefix, "bin"); 
    else {
        //TODO - handle Dll(s)
   	    NCBI_THROW(CProjBulderAppException, 
                   eProjectType, NStr::IntToString(m_Type));
    }

    //output to ..static\DebugDLL or ..dll\DebugDLL
    string output_dir_abs = 
        CDirEntry::ConcatPath(output_dir_prefix, config.m_Name);
    m_OutputDirectory = 
        CDirEntry::CreateRelativePath(project_dir, output_dir_abs);
#endif
}

//------------------------------------------------------------------------------
static IConfiguration* s_CreateConfiguration
    (const CMsvcPrjGeneralContext& general_context,
     const CMsvcPrjProjectContext& project_context);

static ICompilerTool* s_CreateCompilerTool
    (const CMsvcPrjGeneralContext& general_context,
     const CMsvcPrjProjectContext& project_context);

static ILinkerTool* s_CreateLinkerTool
    (const CMsvcPrjGeneralContext& general_context,
     const CMsvcPrjProjectContext& project_context);

static ILibrarianTool* s_CreateLibrarianTool
    (const CMsvcPrjGeneralContext& general_context,
     const CMsvcPrjProjectContext& project_context);

static IResourceCompilerTool* s_CreateResourceCompilerTool
    (const CMsvcPrjGeneralContext& general_context,
     const CMsvcPrjProjectContext& project_context);

//-----------------------------------------------------------------------------
CMsvcTools::CMsvcTools(const CMsvcPrjGeneralContext& general_context,
                       const CMsvcPrjProjectContext& project_context)
{
    //configuration
    m_Configuration.reset
        (s_CreateConfiguration(general_context, project_context));
    //compiler
    m_Compiler.reset
        (s_CreateCompilerTool(general_context, project_context));
    //Linker:
    m_Linker.reset(s_CreateLinkerTool(general_context, project_context));
    //Librarian
    m_Librarian.reset(s_CreateLibrarianTool
                                     (general_context, project_context));
    //Dummies
    m_CustomBuid.reset    (new CCustomBuildToolDummyImpl());
    m_MIDL.reset          (new CMIDLToolDummyImpl());
    m_PostBuildEvent.reset(new CPostBuildEventToolDummyImpl());

    //Pre-build event - special case for LIB projects
    if (project_context.ProjectType() == CProjKey::eLib) {
        m_PreBuildEvent.reset(new CPreBuildEventToolLibImpl
                                                (project_context.PreBuilds(),
                                                 project_context.GetMakeType()));
    } else {
        m_PreBuildEvent.reset(new CPreBuildEventTool(project_context.GetMakeType()));
    }
    m_PreLinkEvent.reset(new CPreLinkEventToolDummyImpl());

    //Resource Compiler
    m_ResourceCompiler.reset(s_CreateResourceCompilerTool
                                     (general_context,project_context));

    //Dummies
    m_WebServiceProxyGenerator.reset
        (new CWebServiceProxyGeneratorToolDummyImpl());

    m_XMLDataGenerator.reset
        (new CXMLDataGeneratorToolDummyImpl());

    m_ManagedWrapperGenerator.reset
        (new CManagedWrapperGeneratorToolDummyImpl());

    m_AuxiliaryManagedWrapperGenerator.reset
        (new CAuxiliaryManagedWrapperGeneratorToolDummyImpl());
}


IConfiguration* CMsvcTools::Configuration(void) const
{
    return m_Configuration.get();
}


ICompilerTool* CMsvcTools::Compiler(void) const
{
    return m_Compiler.get();
}


ILinkerTool* CMsvcTools::Linker(void) const
{
    return m_Linker.get();
}


ILibrarianTool* CMsvcTools::Librarian(void) const
{
    return m_Librarian.get();
}


ICustomBuildTool* CMsvcTools::CustomBuid(void) const
{
    return m_CustomBuid.get();
}


IMIDLTool* CMsvcTools::MIDL(void) const
{
    return m_MIDL.get();
}


IPostBuildEventTool* CMsvcTools::PostBuildEvent(void) const
{
    return m_PostBuildEvent.get();
}


IPreBuildEventTool* CMsvcTools::PreBuildEvent(void) const
{
    return m_PreBuildEvent.get();
}


IPreLinkEventTool* CMsvcTools::PreLinkEvent(void) const
{
    return m_PreLinkEvent.get();
}


IResourceCompilerTool* CMsvcTools::ResourceCompiler(void) const
{
    return m_ResourceCompiler.get();
}


IWebServiceProxyGeneratorTool* CMsvcTools::WebServiceProxyGenerator(void) const
{
    return m_WebServiceProxyGenerator.get();
}


IXMLDataGeneratorTool* CMsvcTools::XMLDataGenerator(void) const
{
    return m_XMLDataGenerator.get();
}


IManagedWrapperGeneratorTool* CMsvcTools::ManagedWrapperGenerator(void) const
{
    return m_ManagedWrapperGenerator.get();
}


IAuxiliaryManagedWrapperGeneratorTool* 
                       CMsvcTools::AuxiliaryManagedWrapperGenerator(void) const
{
    return m_AuxiliaryManagedWrapperGenerator.get();
}


CMsvcTools::~CMsvcTools()
{
}


static bool s_IsExe(const CMsvcPrjGeneralContext& general_context,
                    const CMsvcPrjProjectContext& project_context)
{
    return general_context.m_Type == CMsvcPrjGeneralContext::eExe;
}


static bool s_IsLib(const CMsvcPrjGeneralContext& general_context,
                    const CMsvcPrjProjectContext& project_context)
{
    return general_context.m_Type == CMsvcPrjGeneralContext::eLib;
}


static bool s_IsDll(const CMsvcPrjGeneralContext& general_context,
                    const CMsvcPrjProjectContext& project_context)
{
    return general_context.m_Type == CMsvcPrjGeneralContext::eDll;
}


static bool s_IsDebug(const CMsvcPrjGeneralContext& general_context,
                      const CMsvcPrjProjectContext& project_context)
{
    return general_context.m_Config.m_Debug;
}


static bool s_IsRelease(const CMsvcPrjGeneralContext& general_context,
                        const CMsvcPrjProjectContext& project_context)
{
    return !(general_context.m_Config.m_Debug);
}


//-----------------------------------------------------------------------------
// Creators:
static IConfiguration* 
s_CreateConfiguration(const CMsvcPrjGeneralContext& general_context,
                      const CMsvcPrjProjectContext& project_context)
{
    if ( s_IsExe(general_context, project_context) )
	    return new CConfigurationImpl<SApp>
                       (general_context.OutputDirectory(), 
                        general_context.ConfigurationName());

    if ( s_IsLib(general_context, project_context) )
	    return new CConfigurationImpl<SLib>
                        (general_context.OutputDirectory(), 
                         general_context.ConfigurationName());

    if ( s_IsDll(general_context, project_context) )
	    return new CConfigurationImpl<SDll>
                        (general_context.OutputDirectory(), 
                         general_context.ConfigurationName());
    return NULL;
}


static ICompilerTool* 
s_CreateCompilerTool(const CMsvcPrjGeneralContext& general_context,
					 const CMsvcPrjProjectContext& project_context)
{
    return new CCompilerToolImpl
       (project_context.AdditionalIncludeDirectories(general_context.m_Config),
        project_context.GetMsvcProjectMakefile(),
        general_context.m_Config.m_RuntimeLibrary,
        general_context.GetMsvcMetaMakefile(),
        general_context.m_Config,
        general_context.m_Type,
        project_context.Defines(general_context.m_Config));
}


static ILinkerTool* 
s_CreateLinkerTool(const CMsvcPrjGeneralContext& general_context,
                   const CMsvcPrjProjectContext& project_context)
{
    //---- EXE ----
    if ( s_IsExe  (general_context, project_context) )
        return new CLinkerToolImpl<SApp>
                       (project_context.AdditionalLinkerOptions
                                            (general_context.m_Config),
                        project_context.AdditionalLibraryDirectories
                                            (general_context.m_Config),
                        project_context.ProjectId(),
                        project_context.GetMsvcProjectMakefile(),
                        general_context.GetMsvcMetaMakefile(),
                        general_context.m_Config);


    //---- LIB ----
    if ( s_IsLib(general_context, project_context) )
        return new CLinkerToolDummyImpl();

    //---- DLL ----
    if ( s_IsDll  (general_context, project_context) )
        return new CLinkerToolImpl<SDll>
                       (project_context.AdditionalLinkerOptions
                                            (general_context.m_Config),
                        project_context.AdditionalLibraryDirectories
                                            (general_context.m_Config),
                        project_context.ProjectId(),
                        project_context.GetMsvcProjectMakefile(),
                        general_context.GetMsvcMetaMakefile(),
                        general_context.m_Config);

    // unsupported tool
    return NULL;
}


static ILibrarianTool* 
s_CreateLibrarianTool(const CMsvcPrjGeneralContext& general_context,
                      const CMsvcPrjProjectContext& project_context)
{
    if ( s_IsLib  (general_context, project_context) )
	    return new CLibrarianToolImpl
                                (project_context.ProjectId(),
                                 project_context.GetMsvcProjectMakefile(),
                                 general_context.GetMsvcMetaMakefile(),
                                 general_context.m_Config);

    // dummy tool
    return new CLibrarianToolDummyImpl();
}


static IResourceCompilerTool* s_CreateResourceCompilerTool
                                (const CMsvcPrjGeneralContext& general_context,
                                 const CMsvcPrjProjectContext& project_context)
{

    if ( s_IsDll  (general_context, project_context)  &&
         s_IsDebug(general_context, project_context) )
        return new CResourceCompilerToolImpl<SDebug>
       (project_context.AdditionalIncludeDirectories(general_context.m_Config),
        project_context.GetMsvcProjectMakefile(),
        general_context.GetMsvcMetaMakefile(),
        general_context.m_Config);

    if ( s_IsDll    (general_context, project_context)  &&
         s_IsRelease(general_context, project_context) )
        return new CResourceCompilerToolImpl<SRelease>
       (project_context.AdditionalIncludeDirectories(general_context.m_Config),
        project_context.GetMsvcProjectMakefile(),
        general_context.GetMsvcMetaMakefile(),
        general_context.m_Config);

    if ( s_IsExe  (general_context, project_context)  &&
         s_IsDebug(general_context, project_context) )
        return new CResourceCompilerToolImpl<SDebug>
       (project_context.AdditionalIncludeDirectories(general_context.m_Config),
        project_context.GetMsvcProjectMakefile(),
        general_context.GetMsvcMetaMakefile(),
        general_context.m_Config);


    if ( s_IsExe    (general_context, project_context)  &&
         s_IsRelease(general_context, project_context) )
        return new CResourceCompilerToolImpl<SRelease>
       (project_context.AdditionalIncludeDirectories(general_context.m_Config),
        project_context.GetMsvcProjectMakefile(),
        general_context.GetMsvcMetaMakefile(),
        general_context.m_Config);


    // dummy tool
    return new CResourceCompilerToolDummyImpl();
}


END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.47  2005/02/14 18:52:29  gouriano
 * Generate a file with all features and packages listed
 *
 * Revision 1.46  2005/01/31 16:37:38  gouriano
 * Keep track of subproject types and propagate it down the project tree
 *
 * Revision 1.45  2005/01/10 15:40:09  gouriano
 * Make PTB pick up MSVC tune-up for DLLs
 *
 * Revision 1.44  2005/01/04 14:10:48  gouriano
 * Removed unused variable
 *
 * Revision 1.43  2004/12/30 17:47:51  gouriano
 * Also add StdLibs to the list of additional libraries
 *
 * Revision 1.42  2004/12/20 15:24:23  gouriano
 * Changed diagnostic output
 *
 * Revision 1.41  2004/11/29 17:03:39  gouriano
 * Add 3rd party library dependencies on per-configuration basis
 *
 * Revision 1.40  2004/11/23 20:12:12  gouriano
 * Tune libraries with the choice for each configuration independently
 *
 * Revision 1.39  2004/11/09 17:38:32  gouriano
 * Do not sort INCLUDE directories
 *
 * Revision 1.38  2004/11/03 19:37:58  gouriano
 * Correctly process LibChoices settings
 *
 * Revision 1.37  2004/10/12 13:27:36  gouriano
 * Added possibility to specify which headers to include into project
 *
 * Revision 1.36  2004/08/25 19:38:40  gouriano
 * Implemented optional dependency on a third party library
 *
 * Revision 1.35  2004/08/04 13:27:24  gouriano
 * Added processing of EXPENDABLE projects
 *
 * Revision 1.34  2004/07/13 15:58:40  gouriano
 * Add more parameterization
 *
 * Revision 1.33  2004/06/15 19:01:40  gorelenk
 * Fixed compilation errors on GCC 2.95 .
 *
 * Revision 1.32  2004/06/10 15:16:46  gorelenk
 * Changed macrodefines to be comply with GCC 3.4.0 .
 *
 * Revision 1.31  2004/06/07 19:16:07  gorelenk
 * + Taking into account 'impl' in creation of header dir list.
 *
 * Revision 1.30  2004/06/07 13:56:17  gorelenk
 * Changed CMsvcPrjProjectContext::GetMsvcProjectMakefile to accomodate
 * project creation rules.
 *
 * Revision 1.29  2004/05/21 21:41:41  gorelenk
 * Added PCH ncbi_pch.hpp
 *
 * Revision 1.28  2004/05/11 15:55:03  gorelenk
 * Changed CMsvcPrjProjectContext::IsConfigEnabled - added test REQUIRES.
 *
 * Revision 1.27  2004/05/10 19:52:04  gorelenk
 * Changed CMsvcPrjProjectContext class constructor .
 *
 * Revision 1.26  2004/04/19 15:44:50  gorelenk
 * Changed implementation of class CMsvcPrjProjectContext constructor:
 * added lib choice test while creating list of dependencies (m_ProjectLibs).
 *
 * Revision 1.25  2004/04/13 17:08:55  gorelenk
 * Changed implementation of class CMsvcPrjProjectContext constructor.
 *
 * Revision 1.24  2004/03/22 14:50:50  gorelenk
 * Removed implementation of
 * CMsvcPrjProjectContext::AdditionalLibrarianOptions .
 * Added implemetation of CMsvcPrjProjectContext::Defines .
 *
 * Revision 1.23  2004/03/16 21:46:17  gorelenk
 * Changed implementation of
 * CMsvcPrjProjectContext::AdditionalIncludeDirectories : implemented
 * uniqueness of include dirs from 3-party libraries.
 *
 * Revision 1.22  2004/03/16 16:37:33  gorelenk
 * Changed msvc7_prj subdirs structure: Separated "static" and "dll" branches.
 *
 * Revision 1.21  2004/03/10 21:28:42  gorelenk
 * Changed implementation of class CMsvcPrjProjectContext constructor.
 *
 * Revision 1.20  2004/03/10 16:44:21  gorelenk
 * Changed implementation of class CMsvcPrjProjectContext.
 *
 * Revision 1.19  2004/03/08 23:36:11  gorelenk
 * Changed implementation of class CMsvcPrjProjectContext constructor.
 *
 * Revision 1.18  2004/03/02 23:33:55  gorelenk
 * Changed implementation of class CMsvcPrjGeneralContext constructor.
 *
 * Revision 1.17  2004/02/26 15:15:38  gorelenk
 * Changed implementation of CMsvcPrjProjectContext::IsConfigEnabled.
 *
 * Revision 1.16  2004/02/24 20:54:26  gorelenk
 * Added implementation of member-function bool IsConfigEnabled
 * of class CMsvcPrjProjectContext.
 *
 * Revision 1.15  2004/02/24 18:13:26  gorelenk
 * Changed implementation of class CMsvcPrjProjectContext constructor.
 * Changed implementation of member-function AdditionalLinkerOptions of
 * class CMsvcPrjProjectContext.
 *
 * Revision 1.14  2004/02/23 20:58:41  gorelenk
 * Fixed double properties apperience in generated MSVC projects.
 *
 * Revision 1.13  2004/02/23 20:42:57  gorelenk
 * Added support of MSVC ResourceCompiler tool.
 *
 * Revision 1.12  2004/02/20 22:53:26  gorelenk
 * Added analysis of ASN projects depends.
 *
 * Revision 1.11  2004/02/06 23:14:59  gorelenk
 * Implemented support of ASN projects, semi-auto configure,
 * CPPFLAGS support. Second working version.
 *
 * Revision 1.10  2004/02/05 16:28:47  gorelenk
 * GetComponents was moved to class CMsvcSite.
 *
 * Revision 1.9  2004/02/05 00:02:08  gorelenk
 * Added support of user site and  Makefile defines.
 *
 * Revision 1.8  2004/02/03 17:17:38  gorelenk
 * Changed implementation of class CMsvcPrjProjectContext constructor.
 *
 * Revision 1.7  2004/01/29 15:46:44  gorelenk
 * Added support of default libs, defined in user site
 *
 * Revision 1.6  2004/01/28 17:55:49  gorelenk
 * += For msvc makefile support of :
 *                 Requires tag, ExcludeProject tag,
 *                 AddToProject section (SourceFiles and IncludeDirs),
 *                 CustomBuild section.
 * += For support of user local site.
 *
 * Revision 1.5  2004/01/26 19:27:29  gorelenk
 * += MSVC meta makefile support
 * += MSVC project makefile support
 *
 * Revision 1.4  2004/01/22 17:57:54  gorelenk
 * first version
 *
 * ===========================================================================
 */

