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
#include "stl_msvc_usage.hpp"

#include "msvc_project_context.hpp"
#include "msvc_tools_implement.hpp"
#include "proj_builder_app.hpp"
#include "msvc_site.hpp"
#include "msvc_prj_defines.hpp"
#include "ptb_err_codes.hpp"
#include "proj_tree_builder.hpp"

#include <algorithm>
#include <set>


BEGIN_NCBI_SCOPE

map<string, set<string> > CMsvcPrjProjectContext::s_EnabledPackages;
map<string, set<string> > CMsvcPrjProjectContext::s_DisabledPackages;

//-----------------------------------------------------------------------------
CMsvcPrjProjectContext::CMsvcPrjProjectContext(const CProjItem& project)
    : m_Project(project)
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
                            (project.m_SourcesBaseDir,
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

    string lib_dir = GetApp().GetBuildRoot();
    if (lib_dir.empty()) {
        lib_dir = GetApp().GetProjectTreeInfo().m_Compilers;
        lib_dir = CDirEntry::ConcatPath(lib_dir, 
                                        GetApp().GetRegSettings().m_CompilersSubdir);
    }
    string type_dir = CDirEntry::ConcatPath(lib_dir, 
                                            GetApp().GetBuildType().GetTypeStr());
// it is either root/buildtype/[lib|bin]/$ConfigurationName
// or just      root/$ConfigurationName
    if (CDirEntry(type_dir).Exists()) {
        m_StaticLibRoot  = CDirEntry::ConcatPath(type_dir, "lib");
        m_DynamicLibRoot = CDirEntry::ConcatPath(type_dir, "bin");
    } else {
        m_StaticLibRoot = m_DynamicLibRoot = lib_dir;
    }
// find sources
    string t, try_dir, inc_dir;
    for ( t = try_dir = m_StaticLibRoot; ; try_dir = t) {
        inc_dir = CDirEntry::ConcatPath(try_dir, 
            GetApp().GetConfig().Get("ProjectTree", "include"));
        if (CDirEntry(inc_dir).Exists()) {
            m_SrcRoot = CDirEntry(inc_dir).GetDir();
            break;
        }
        t = CDirEntry(try_dir).GetDir();
        if (t == try_dir) {
            break;
        }
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
        string incl_path = CDirEntry::NormalizePath(
            CDirEntry::ConcatPath(GetApp().GetProjectTreeInfo().m_Include, 
                                  rel_path));
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
    m_IncludeDirsAbs = project.m_Includes;
    m_InlineDirsAbs  = project.m_Inlines;

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

    m_IncludeDirsAbs.sort();
    m_IncludeDirsAbs.unique();
    m_InlineDirsAbs.sort();
    m_InlineDirsAbs.unique();

    // Get custom build files and adjust pathes 
    GetMsvcProjectMakefile().GetCustomBuildInfo(&m_CustomBuildInfo);

    // Collect include dirs, specified in project Makefiles
    m_ProjectIncludeDirs = project.m_IncludeDirs;

    // LIBS from Makefiles
    if (!project.m_Libs3Party.empty()) {
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
    }
    m_Requires.sort();
    m_Requires.unique();

    // Proprocessor definitions from makefiles:
    m_Defines = project.m_Defines;
    if (GetApp().GetBuildType().GetType() == CBuildType::eDll) {
        m_Defines.push_back(GetApp().GetConfig().Get(CMsvc7RegSettings::GetMsvcSection(), "DllBuildDefine"));
    }
    // Pre-Builds for LIB projects:
    {
        ITERATE(set<CProjKey>, p, project.m_UnconditionalDepends) {
            CProjKey proj_key = *p;
            {
                const CProjectItemsTree* curr_tree = GetApp().GetCurrentBuildTree();
                if (GetApp().GetIncompleteBuildTree()) {
                    // do not attempt to prebuild what is missing
                    if (GetApp().GetIncompleteBuildTree()->m_Projects.find(proj_key) ==
                        GetApp().GetIncompleteBuildTree()->m_Projects.end()) {
                        continue;
                    }
                } else if (curr_tree) {
                    if (curr_tree->m_Projects.find(proj_key) ==
                        curr_tree->m_Projects.end()) {
                        
                        bool depfound = false;
                        string dll(GetDllHost(*curr_tree, proj_key.Id()));
                        if (!dll.empty()) {
                            CProjKey id_alt(CProjKey::eDll,dll);
                            if (curr_tree->m_Projects.find(id_alt) !=
                                curr_tree->m_Projects.end()) {
                                proj_key = id_alt;
                                depfound = true;
                            }
                        }
                        if (!depfound) {
                            PTB_WARNING_EX(
                                CDirEntry::ConcatPath(m_SourcesBaseDir, m_ProjectName),
                                    ePTB_ProjectNotFound, "depends on missing project: " << proj_key.Id());
                        }
                    }
                }
                if (!SMakeProjectT::IsConfigurableDefine(proj_key.Id())) {
                    m_PreBuilds.push_back(proj_key);
                }
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

    if (!GetApp().m_IncDir.empty()) {
        string config_inc = CDirEntry::AddTrailingPathSeparator(GetApp().m_IncDir);
        config_inc = CDirEntry::CreateRelativePath(m_ProjectDir, config_inc);
        add_include_dirs_list.push_back( config_inc );
    }

    // project dir
    string tree_inc_abs(GetApp().GetProjectTreeInfo().m_Include);
    string tree_inc = CDirEntry::CreateRelativePath(m_ProjectDir, tree_inc_abs);
    tree_inc = CDirEntry::AddTrailingPathSeparator(tree_inc);
    add_include_dirs_list.push_back( tree_inc );
    
    // internal, if present
    string internal_inc = CDirEntry::ConcatPath(tree_inc,"internal");
    if (CDirEntry(CDirEntry::NormalizePath(CDirEntry::ConcatPath(m_ProjectDir,internal_inc))).IsDir()) {
        add_include_dirs_list.push_back( CDirEntry::AddTrailingPathSeparator(internal_inc) );
    }
    
    //take into account project include dirs
    ITERATE(list<string>, p, m_ProjectIncludeDirs) {
        const string& dir_abs = *p;
        if (dir_abs == tree_inc_abs) {
            continue;
        }
        dirs.clear();
        if (CSymResolver::IsDefine(dir_abs)) {
            GetApp().GetSite().GetLibInclude( dir_abs, cfg_info, &dirs);
        } else {
            dirs.push_back(dir_abs);
        }
        for (list<string>::const_iterator i = dirs.begin(); i != dirs.end(); ++i) {
            dir = *i;
            /*if (CDirEntry(dir).IsDir())*/ {
                add_include_dirs_list.push_back(
                    !GetApp().UseAbsolutePath(dir) && SameRootDirs(m_ProjectDir,dir) ?
                        CDirEntry::CreateRelativePath(m_ProjectDir, dir) :
                        dir);
            }
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
        GetApp().GetSite().GetLibInclude(*p, cfg_info, &dirs);
        for (list<string>::const_iterator i = dirs.begin(); i != dirs.end(); ++i) {
            dir = *i;
            if ( !dir.empty() ) {
                if (!GetApp().UseAbsolutePath(dir) && SameRootDirs(m_ProjectDir,dir)) {
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

    string ext_inc;
    const CProjectItemsTree* all_projects = GetApp().GetIncompleteBuildTree();
    if (all_projects) {
        string inc_dir = CDirEntry::ConcatPath(m_SrcRoot, 
            GetApp().GetConfig().Get("ProjectTree", "include"));
        if (CDirEntry(inc_dir).Exists()) {
            try {
                ext_inc = CDirEntry::CreateRelativePath(m_ProjectDir, inc_dir);
            } catch (CFileException&) {
                ext_inc = inc_dir;
            }
            ext_inc = CDirEntry::AddTrailingPathSeparator(ext_inc);
            if (NStr::CompareNocase(tree_inc, ext_inc) != 0) {
                add_include_dirs_list.push_back( ext_inc );
            }
        }
    }
    //Leave only unique dirs and join them to string
//    add_include_dirs_list.sort();
//    add_include_dirs_list.unique();
    return NStr::Join(add_include_dirs_list, ";");
}


string CMsvcPrjProjectContext::AdditionalLinkerOptions
                                            (const SConfigInfo& cfg_info) const
{
    list<string> additional_libs;
    const CMsvcSite& site = GetApp().GetSite();

    // Take into account requires, default and makefiles libs
    list<string> libs_list;
    CreateLibsList(&libs_list);
    ITERATE(list<string>, p, libs_list) {
        const string& requires = *p;
        if (site.Is3PartyLibWithChoice(requires)) {
            if (site.GetChoiceFor3PartyLib(requires, cfg_info) == CMsvcSite::eLib) {
                continue;
            }
        }
        SLibInfo lib_info;
        site.GetLibInfo(requires, cfg_info, &lib_info);
        if ( site.IsLibOk(lib_info) &&
            GetApp().GetSite().IsLibEnabledInConfig(requires, cfg_info)) {
            if ( !lib_info.m_Libs.empty() ) {
                copy(lib_info.m_Libs.begin(), lib_info.m_Libs.end(), 
                    back_inserter(additional_libs));
            }
            if ( !lib_info.m_StdLibs.empty() ) {
                copy(lib_info.m_StdLibs.begin(), lib_info.m_StdLibs.end(), 
                    back_inserter(additional_libs));
            }
        } else {
            if (!lib_info.IsEmpty() && !lib_info.m_Libs.empty()) {
                PTB_WARNING_EX(lib_info.m_LibPath, ePTB_FileNotFound,
                               requires << "|" << cfg_info.GetConfigFullName()
                               << " unavailable: missing additional libraries: "
                               << NStr::Join(lib_info.m_Libs,";"));

            }
        }
    }

    // NCBI C Toolkit libs
    ITERATE(list<string>, p, m_NcbiCLibs) {
        string ncbi_lib = *p + ".lib";
        additional_libs.push_back(ncbi_lib);        
    }

    const CProjectItemsTree* all_projects = GetApp().GetIncompleteBuildTree();
    if (all_projects) {
        string static_lib_dir  = CDirEntry::ConcatPath(m_StaticLibRoot, cfg_info.GetConfigFullName());
        string dynamic_lib_dir = CDirEntry::ConcatPath(m_DynamicLibRoot, cfg_info.GetConfigFullName());
        ITERATE(list<CProjKey>, n, m_Project.m_Depends) {
            const CProjKey& depend_id = *n;
            if (SMakeProjectT::IsConfigurableDefine(depend_id.Id())) {
                continue;
            }
            if (depend_id.Type() == CProjKey::eLib || depend_id.Type() == CProjKey::eDll) {
                CProjectItemsTree::TProjects::const_iterator i =
                    all_projects->m_Projects.find(depend_id);
                if (i == all_projects->m_Projects.end()) {
                    string lib_path = CDirEntry::ConcatPath(
                        depend_id.Type() == CProjKey::eLib ? static_lib_dir : dynamic_lib_dir,
                        depend_id.Id());
                    lib_path += ".lib";
                    CDirEntry lib(lib_path);
                    if (!lib.Exists()) {
                        if (!GetApp().m_BuildRoot.empty()) {
                            PTB_WARNING_EX(lib_path, ePTB_FileNotFound,
                                        "Library not found: " << lib_path);
                        } else {
                            PTB_ERROR_EX(lib_path, ePTB_FileNotFound,
                                        "Library not found: " << lib_path);
                        }
                    }
                    additional_libs.push_back(lib.GetName());
                }
            }
        }
    }

    if (CMsvc7RegSettings::GetMsvcPlatform() < CMsvc7RegSettings::eUnix) {
        additional_libs.sort();
        additional_libs.unique();
    }
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
    list<string> dir_list;
    const CMsvcSite& site = GetApp().GetSite();
// library folder
    const CProjectItemsTree* all_projects = GetApp().GetIncompleteBuildTree();
    if (all_projects) {
        string lib_dir;
        try {
            lib_dir = CDirEntry::CreateRelativePath(ProjectDir(), m_StaticLibRoot);
        } catch (CFileException&) {
            lib_dir = m_StaticLibRoot;
        }
        lib_dir = CDirEntry::ConcatPath(lib_dir, CMsvc7RegSettings::GetConfigNameKeyword());
        dir_list.push_back(CDirEntry::AddTrailingPathSeparator(lib_dir));
        if (GetApp().GetBuildType().GetType() == CBuildType::eDll) {
            try {
                lib_dir = CDirEntry::CreateRelativePath(ProjectDir(), m_DynamicLibRoot);
            } catch (CFileException&) {
                lib_dir = m_DynamicLibRoot;
            }
            lib_dir = CDirEntry::ConcatPath(lib_dir, CMsvc7RegSettings::GetConfigNameKeyword());
            dir_list.push_back(CDirEntry::AddTrailingPathSeparator(lib_dir));
        }
    }

    // Take into account requires, default and makefiles libs
    list<string> libs_list;
    CreateLibsList(&libs_list);
    ITERATE(list<string>, p, libs_list) {
        const string& requires = *p;
        if (site.Is3PartyLibWithChoice(requires)) {
            if (site.GetChoiceFor3PartyLib(requires, cfg_info) == CMsvcSite::eLib) {
                continue;
            }
        }
        SLibInfo lib_info;
        site.GetLibInfo(requires, cfg_info, &lib_info);
        if ( site.IsLibOk(lib_info) &&
             site.IsLibEnabledInConfig(requires, cfg_info) ) {
            if ( !lib_info.m_LibPath.empty() ) {
                dir_list.push_back(CDirEntry::AddTrailingPathSeparator(lib_info.m_LibPath));
            }
        } else {
            if (!lib_info.IsEmpty()) {
                PTB_WARNING_EX(lib_info.m_LibPath, ePTB_FileNotFound,
                               requires << "|" << cfg_info.GetConfigFullName()
                               << " unavailable: library folder ignored: "
                               << lib_info.m_LibPath);
            }
        }
    }
    dir_list.sort();
    dir_list.unique();
    return NStr::Join(dir_list, ";");
}


void CMsvcPrjProjectContext::CreateLibsList(list<string>* libs_list) const
{
    libs_list->clear();
    // We'll build libs list.
    *libs_list = m_Requires;
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
    //take into account default libs from site:
    libs_list->push_back(MSVC_DEFAULT_LIBS_TAG);
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
        if ( !GetApp().GetSite().IsProvided(requires) &&
             !GetApp().GetSite().IsProvided(requires, false) ) {
            if (unmet) {
                *unmet = requires;
            }
            return false;
        }
    }
    return true;
}


bool CMsvcPrjProjectContext::IsConfigEnabled(const SConfigInfo& config,
    string* unmet, string* unmet_req) const
{
    list<string> libs_3party;
    ITERATE(list<string>, p, m_ProjectLibs) {
        const string& lib = *p;
        list<string> components;
        GetApp().GetSite().GetComponents(lib, &components);
        copy(components.begin(), 
             components.end(), back_inserter(libs_3party));
    }
    list<string> libs_required;
    ITERATE(list<string>, p, m_Requires) {
        const string& lib = *p;
        list<string> components;
        GetApp().GetSite().GetComponents(lib, &components);
        if (components.empty()) {
            libs_required.push_back(lib);
        } else {
            copy(components.begin(), 
                components.end(), back_inserter(libs_required));
        }
    }

    // Add requires to test : If there is such library and configuration for 
    // this library is disabled then we'll disable this config
    copy(m_Requires.begin(), m_Requires.end(), back_inserter(libs_3party));
    libs_3party.sort();
    libs_3party.unique();

    // Test third-party libs and requires:
    const CMsvcSite& site = GetApp().GetSite();
    bool result = true;
    ITERATE(list<string>, p, libs_3party) {
        const string& requires = *p;
        SLibInfo lib_info;
        site.GetLibInfo(requires, config, &lib_info);
        
        if ( lib_info.IsEmpty() ) {
            bool st = 
                (config.m_rtType == SConfigInfo::rtSingleThreaded ||
                 config.m_rtType == SConfigInfo::rtSingleThreadedDebug);
            if ((requires == "MT" && st) || (requires == "-MT" && !st)) {
                if (unmet) {
                    if (!unmet->empty()) {
                        *unmet += ", ";
                    }
                    *unmet += requires;
                }
                result = false;
            }
            continue;
        }

        if ( !site.IsLibEnabledInConfig(requires, config) ) {
            if (unmet) {
                if (!unmet->empty()) {
                    *unmet += ", ";
                }
                *unmet += requires;
            }
            if (find( libs_required.begin(), libs_required.end(), requires )
                   != libs_required.end()) {
                result = false;
            }
            s_DisabledPackages[config.GetConfigFullName()].insert(requires);
        } else {
            s_EnabledPackages[config.GetConfigFullName()].insert(requires);
        }

        if ( !site.IsLibOk(lib_info,true) && !site.Is3PartyLibWithChoice(requires) ) {
            if (unmet_req) {
                if (!unmet_req->empty()) {
                    *unmet_req += ", ";
                }
                *unmet_req += requires;
            }
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

bool CMsvcPrjProjectContext::IsPchEnabled(const SConfigInfo& config) const
{
    string value = GetConfigData("UsePch","UsePch",config);
    if (value.empty()) {
        return false;
    }
    return NStr::StringToBool(value);
}

string CMsvcPrjProjectContext::GetPchHeader(
        const string& project_id,
        const string& source_file_full_path,
        const string& tree_src_dir, const SConfigInfo& config) const
{
    string value = m_MsvcProjectMakefile->GetConfigOpt("UsePch","DefaultPch",config);
    if (value.empty()) {
        if (!m_Project.m_Pch.empty()) {
            try {
                NStr::StringToBool(m_Project.m_Pch);
            }
            catch (...) {
                return m_Project.m_Pch;
            }
        }
        value = GetApp().GetMetaMakefile().GetUsePchThroughHeader(
            project_id, source_file_full_path, tree_src_dir);
    }
    return value;
}

string CMsvcPrjProjectContext::GetConfigData(
    const string& section, const string& entry, const SConfigInfo& config) const
{
    string value = m_MsvcProjectMakefile->GetConfigOpt(section,entry,config);
    if (value.empty()) {
        if (section == "UsePch" && entry == "UsePch" && !m_Project.m_Pch.empty()) {
            try {
                NStr::StringToBool(m_Project.m_Pch);
                return m_Project.m_Pch;
            }
            catch (...) {
            }
            return "true";
        }
        value = GetApp().GetMetaMakefile().GetConfigOpt(section,entry,config);
    }
    return value;
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
    case CProjKey::eDataSpec:
        m_Type = eDataSpec;
        break;
    default:
        m_Type = eOther;
        break;
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
        output_dir_abs = CDirEntry::ConcatPath(output_dir_abs, "lib");
    }

    output_dir_abs = 
        CDirEntry::ConcatPath(output_dir_abs, CMsvc7RegSettings::GetConfigNameKeyword());
    m_OutputDirectory = 
        CDirEntry::AddTrailingPathSeparator(
        CDirEntry::CreateRelativePath(prj_context.ProjectDir(), 
                                      output_dir_abs));

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
        CDirEntry::ConcatPath(output_dir_prefix, config.GetConfigFullName());
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
    } else if (project_context.ProjectType() == CProjKey::eDataSpec ||
               project_context.ProjectType() == CProjKey::eUtility) {
        m_PreBuildEvent.reset(new CPreBuildEventToolDummyImpl);
    } else {
        m_PreBuildEvent.reset(new CPreBuildEventTool(project_context.PreBuilds(),
                                                     project_context.GetMakeType()));
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

static bool s_IsUtility(const CMsvcPrjGeneralContext& general_context,
                    const CMsvcPrjProjectContext& project_context)
{
    return general_context.m_Type == CMsvcPrjGeneralContext::eDataSpec ||
           general_context.m_Type == CMsvcPrjGeneralContext::eOther;
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
                        general_context.ConfigurationName(),
                        project_context.GetMsvcProjectMakefile(),
                        general_context.GetMsvcMetaMakefile(),
                        general_context.m_Config );

    if ( s_IsLib(general_context, project_context) )
	    return new CConfigurationImpl<SLib>
                        (general_context.OutputDirectory(), 
                         general_context.ConfigurationName(),
                         project_context.GetMsvcProjectMakefile(),
                         general_context.GetMsvcMetaMakefile(),
                         general_context.m_Config );

    if ( s_IsUtility(general_context, project_context) )
	    return new CConfigurationImpl<SUtility>
                        (general_context.OutputDirectory(), 
                         general_context.ConfigurationName(),
                         project_context.GetMsvcProjectMakefile(),
                         general_context.GetMsvcMetaMakefile(),
                         general_context.m_Config );

    if ( s_IsDll(general_context, project_context) )
	    return new CConfigurationImpl<SDll>
                        (general_context.OutputDirectory(), 
                         general_context.ConfigurationName(),
                         project_context.GetMsvcProjectMakefile(),
                         general_context.GetMsvcMetaMakefile(),
                         general_context.m_Config );
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
        project_context.Defines(general_context.m_Config),
        project_context.ProjectId());
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
    if ( s_IsLib(general_context, project_context) ||
         s_IsUtility(general_context, project_context) )
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
