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
#include <app/project_tree_builder/msvc_dlls_info.hpp>
#include <app/project_tree_builder/proj_builder_app.hpp>
#include <app/project_tree_builder/msvc_prj_defines.hpp>
#include <app/project_tree_builder/proj_projects.hpp>
#include <app/project_tree_builder/proj_tree_builder.hpp>
#include <app/project_tree_builder/msvc_project_context.hpp>
#include <app/project_tree_builder/msvc_prj_files_collector.hpp>
#include <app/project_tree_builder/msvc_dlls_info_utils.hpp>

#include <corelib/ncbistre.hpp>

#include <algorithm>

BEGIN_NCBI_SCOPE


CMsvcDllsInfo::CMsvcDllsInfo(const string& file_path)
{
    CNcbiIfstream ifs(file_path.c_str(), IOS_BASE::in | IOS_BASE::binary);
    if (ifs) {
        //read registry
        m_Registry.Read(ifs);
    } else {
        LOG_POST(Error << file_path << " not found");
    }
}


CMsvcDllsInfo::~CMsvcDllsInfo(void)
{
}


void CMsvcDllsInfo::GetDllsList(list<string>* dlls_ids) const
{
    ncbi::GetDllsList(m_Registry, dlls_ids);
}


void CMsvcDllsInfo::GetBuildConfigs(list<SConfigInfo>* config) const
{
    config->clear();

    string configs_str = 
        m_Registry.GetString("DllBuild", "Configurations", "");
    list<string> config_names;
    NStr::Split(configs_str, LIST_SEPARATOR, config_names);
    LoadConfigInfoByNames(GetApp().GetConfig(), config_names, config);
}


string CMsvcDllsInfo::GetBuildDefine(void) const
{
    return m_Registry.GetString("DllBuild", "BuildDefine", "");
}


bool CMsvcDllsInfo::SDllInfo::IsEmpty(void) const
{
    return  m_Hosting.empty() &&
            m_Depends.empty() &&
            m_DllDefine.empty();
}
 
       
void CMsvcDllsInfo::SDllInfo::Clear(void)
{
    m_Hosting.clear();
    m_Depends.clear();
    m_DllDefine.erase();
}


void CMsvcDllsInfo::GetDllInfo(const string& dll_id, SDllInfo* dll_info) const
{
    dll_info->Clear();

    GetHostedLibs(m_Registry, dll_id, &(dll_info->m_Hosting) );

    string depends_str = m_Registry.GetString(dll_id, "Dependencies", "");
    NStr::Split(depends_str, LIST_SEPARATOR, dll_info->m_Depends);

    dll_info->m_DllDefine = m_Registry.GetString(dll_id, "DllDefine", "");
}


bool CMsvcDllsInfo::IsDllHosted(const string& lib_id) const
{
    return !GetDllHost(lib_id).empty();
}


string CMsvcDllsInfo::GetDllHost(const string& lib_id) const
{
    list<string> dll_list;
    GetDllsList(&dll_list);

    ITERATE(list<string>, p, dll_list) {
        const string& dll_id = *p;
        SDllInfo dll_info;
        GetDllInfo(dll_id, &dll_info);
        if (find(dll_info.m_Hosting.begin(),
                 dll_info.m_Hosting.end(),
                 lib_id) != dll_info.m_Hosting.end()) {
            return dll_id;
        }
    }
    map<string,string>::const_iterator i = m_DllHostedLibs.find(lib_id);
    if ( i != m_DllHostedLibs.end()) {
        return i->second;
    }
    return "";
}

void CMsvcDllsInfo::AddDllHostedLib(const string& lib_id, const string& host)
{
    m_DllHostedLibs[lib_id] = host;
}


//-----------------------------------------------------------------------------
void FilterOutDllHostedProjects(const CProjectItemsTree& tree_src, 
                                CProjectItemsTree*       tree_dst)
{
    tree_dst->m_RootSrc = tree_src.m_RootSrc;

    tree_dst->m_Projects.clear();
    ITERATE(CProjectItemsTree::TProjects, p, tree_src.m_Projects) {

        const CProjKey&  proj_id = p->first;
        const CProjItem& project = p->second;

        bool dll_hosted = (proj_id.Type() == CProjKey::eLib)  &&
                           GetApp().GetDllsInfo().IsDllHosted(proj_id.Id());
        if ( !dll_hosted ) {
            tree_dst->m_Projects[proj_id] = project;
        }
    }    
}

static bool s_IsInTree(CProjKey::TProjType      proj_type,
                       const string&            proj_id,
                       const CProjectItemsTree& tree)
{
    return tree.m_Projects.find
                  (CProjKey(proj_type, 
                            proj_id)) != 
                                    tree.m_Projects.end();
}


static bool s_IsDllProject(const string& project_id)
{
    CMsvcDllsInfo::SDllInfo dll_info;
    GetApp().GetDllsInfo().GetDllInfo(project_id, &dll_info);
    return !dll_info.IsEmpty();
}


static void s_InitalizeDllProj(const string&                  dll_id, 
                               const CMsvcDllsInfo::SDllInfo& dll_info,
                               CProjItem*                     dll,
                               CProjectItemsTree*             tree_dst)
{
    dll->m_Name           = dll_id;
    dll->m_ID             = dll_id;
    dll->m_ProjType       = CProjKey::eDll;

    ITERATE(list<string>, p, dll_info.m_Depends) {

        const string& depend_id = *p;

        // Is this a dll?
        if ( s_IsDllProject(depend_id) ) {
            dll->m_Depends.push_back(CProjKey(CProjKey::eDll, 
                                               depend_id));    
        } else  {
            if ( s_IsInTree(CProjKey::eApp, 
                            depend_id, 
                            GetApp().GetWholeTree()) ) {

                CProjKey depend_key(CProjKey::eApp, depend_id);
                dll->m_Depends.push_back(depend_key);
                tree_dst->m_Projects[depend_key] = 
                 (GetApp().GetWholeTree().m_Projects.find(depend_key))->second;
            }
            else if ( s_IsInTree(CProjKey::eLib, 
                                 depend_id, 
                                 GetApp().GetWholeTree()) ) {

                CProjKey depend_key(CProjKey::eLib, depend_id);
                dll->m_Depends.push_back(depend_key); 
                tree_dst->m_Projects[depend_key] = 
                 (GetApp().GetWholeTree().m_Projects.find(depend_key))->second;

            } else  {
                LOG_POST(Warning << dll_id << ": project not found: " + depend_id);
            }
        }
    }

    string dll_project_dir = GetApp().GetProjectTreeInfo().m_Compilers;
    dll_project_dir = 
        CDirEntry::ConcatPath(dll_project_dir, 
                              GetApp().GetRegSettings().m_CompilersSubdir);
    dll_project_dir = 
        CDirEntry::ConcatPath(dll_project_dir, 
                              GetApp().GetBuildType().GetTypeStr());
    dll_project_dir =
        CDirEntry::ConcatPath(dll_project_dir,
                              GetApp().GetRegSettings().m_ProjectsSubdir);
    dll_project_dir = 
        CDirEntry::ConcatPath(dll_project_dir, dll_id);

    dll_project_dir = CDirEntry::AddTrailingPathSeparator(dll_project_dir);

    dll->m_SourcesBaseDir = dll_project_dir;
    dll->m_MsvcProjectMakefileDir.erase();

    dll->m_Sources.clear();
    dll->m_Sources.push_back("..\\..\\dll_main");
}


static void s_AddProjItemToDll(const CProjItem& lib, CProjItem* dll)
{
    // If this library is available as a third-party,
    // then we'll require it
    if (GetApp().GetSite().GetChoiceForLib(lib.m_ID) 
                                                   == CMsvcSite::e3PartyLib ) {
        CMsvcSite::SLibChoice choice = 
            GetApp().GetSite().GetLibChoiceForLib(lib.m_ID);
        dll->m_Requires.push_back(choice.m_3PartyLib);
        dll->m_Requires.sort();
        dll->m_Requires.unique();
        return;
    }

    CMsvcPrjProjectContext lib_context(lib);
    // Define empty configuration list -- to skip configurable file
    // generation on this step. They will be generated later.
    const list<SConfigInfo> no_configs;
    CMsvcPrjFilesCollector collector(lib_context, no_configs, lib);

    // Sources - all pathes are relative to one dll->m_SourcesBaseDir
    ITERATE(list<string>, p, collector.SourceFiles()) {
        const string& rel_path = *p;
        string abs_path = 
            CDirEntry::ConcatPath(lib_context.ProjectDir(), rel_path);
        abs_path = CDirEntry::NormalizePath(abs_path);

        // Register DLL source files as belongs to lib
        // With .ext 
        GetApp().GetDllFilesDistr().RegisterSource
            (abs_path,
             CProjKey(CProjKey::eDll, dll->m_ID),
             CProjKey(CProjKey::eLib, lib.m_ID) );

        string dir;
        string base;
        CDirEntry::SplitPath(abs_path, &dir, &base);
        string abs_source_path = dir + base;

        string new_rel_path = 
            CDirEntry::CreateRelativePath(dll->m_SourcesBaseDir, 
                                          abs_source_path);
        dll->m_Sources.push_back(new_rel_path);
    }
    dll->m_Sources.sort();
    dll->m_Sources.unique();

    // Header files - also register them
    ITERATE(list<string>, p, collector.HeaderFiles()) {
        const string& rel_path = *p;
        string abs_path = 
            CDirEntry::ConcatPath(lib_context.ProjectDir(), rel_path);
        abs_path = CDirEntry::NormalizePath(abs_path);
        GetApp().GetDllFilesDistr().RegisterHeader
            (abs_path,
             CProjKey(CProjKey::eDll, dll->m_ID),
             CProjKey(CProjKey::eLib, lib.m_ID) );
    }
    // Inline files - also register them
    ITERATE(list<string>, p, collector.InlineFiles()) {
        const string& rel_path = *p;
        string abs_path = 
            CDirEntry::ConcatPath(lib_context.ProjectDir(), rel_path);
        abs_path = CDirEntry::NormalizePath(abs_path);
        GetApp().GetDllFilesDistr().RegisterInline
            (abs_path,
             CProjKey(CProjKey::eDll, dll->m_ID),
             CProjKey(CProjKey::eLib, lib.m_ID) );
    }

    // Depends
    ITERATE(list<CProjKey>, p, lib.m_Depends) {

        const CProjKey& depend_id = *p;

        bool dll_hosted = (depend_id.Type() == CProjKey::eLib)  &&
                           GetApp().GetDllsInfo().IsDllHosted(depend_id.Id());
        if ( !dll_hosted ) {
            dll->m_Depends.push_back(depend_id);
        } else {
            dll->m_Depends.push_back
                (CProjKey(CProjKey::eDll, 
                 GetApp().GetDllsInfo().GetDllHost(depend_id.Id())));
        }
    }
    dll->m_Depends.sort();
    dll->m_Depends.unique();


    // m_Requires
    copy(lib.m_Requires.begin(), 
         lib.m_Requires.end(), back_inserter(dll->m_Requires));
    dll->m_Requires.sort();
    dll->m_Requires.unique();

    // Libs 3-Party
    copy(lib.m_Libs3Party.begin(), 
         lib.m_Libs3Party.end(), back_inserter(dll->m_Libs3Party));
    dll->m_Libs3Party.sort();
    dll->m_Libs3Party.unique();

    // m_IncludeDirs
    copy(lib.m_IncludeDirs.begin(), 
         lib.m_IncludeDirs.end(), back_inserter(dll->m_IncludeDirs));
    dll->m_IncludeDirs.sort();
    dll->m_IncludeDirs.unique();

    // m_DatatoolSources
    copy(lib.m_DatatoolSources.begin(), 
         lib.m_DatatoolSources.end(), back_inserter(dll->m_DatatoolSources));
    dll->m_DatatoolSources.sort();
    dll->m_DatatoolSources.unique();

    // m_Defines
    copy(lib.m_Defines.begin(), 
         lib.m_Defines.end(), back_inserter(dll->m_Defines));
    dll->m_Defines.sort();
    dll->m_Defines.unique();

    {{
        string makefile_name = 
            SMakeProjectT::CreateMakeAppLibFileName(lib.m_SourcesBaseDir,
                                                    lib.m_Name);
        CSimpleMakeFileContents makefile(
            CDirEntry::ConcatPath(lib.m_SourcesBaseDir,makefile_name),
            eMakeType_Undefined);
        CSimpleMakeFileContents::TContents::const_iterator p = 
            makefile.m_Contents.find("NCBI_C_LIBS");

        list<string> ncbi_clibs;
        if (p != makefile.m_Contents.end()) {
            SAppProjectT::CreateNcbiCToolkitLibs(makefile, &ncbi_clibs);

            dll->m_Libs3Party.push_back("NCBI_C_LIBS");
            dll->m_Libs3Party.sort();
            dll->m_Libs3Party.unique();

            copy(ncbi_clibs.begin(),
                 ncbi_clibs.end(),
                 back_inserter(dll->m_NcbiCLibs));
            dll->m_NcbiCLibs.sort();
            dll->m_NcbiCLibs.unique();

        }
    }}

    // m_NcbiCLibs
    copy(lib.m_NcbiCLibs.begin(), 
         lib.m_NcbiCLibs.end(), back_inserter(dll->m_NcbiCLibs));
    dll->m_NcbiCLibs.sort();
    dll->m_NcbiCLibs.unique();

    if (dll->m_MsvcProjectMakefileDir.empty()) {
        dll->m_MsvcProjectMakefileDir = lib.m_MsvcProjectMakefileDir;
    } else {
        string dll_project_dir = GetApp().GetProjectTreeInfo().m_Compilers;
        dll_project_dir = 
            CDirEntry::ConcatPath(dll_project_dir, 
                                GetApp().GetRegSettings().m_CompilersSubdir);
        dll_project_dir = 
            CDirEntry::ConcatPath(dll_project_dir, 
                                GetApp().GetBuildType().GetTypeStr());

        dll->m_MsvcProjectMakefileDir = CDirEntry::AddTrailingPathSeparator(dll_project_dir);
    }
/*
    auto_ptr<CMsvcProjectMakefile> msvc_project_makefile = 
        auto_ptr<CMsvcProjectMakefile>
            (new CMsvcProjectMakefile
                    (CDirEntry::ConcatPath
                            (lib.m_SourcesBaseDir, 
                             CreateMsvcProjectMakefileName(lib))));
*/
}


void CreateDllBuildTree(const CProjectItemsTree& tree_src, 
                        CProjectItemsTree*       tree_dst)
{
    tree_dst->m_RootSrc = tree_src.m_RootSrc;

    FilterOutDllHostedProjects(tree_src, tree_dst);

    NON_CONST_ITERATE(CProjectItemsTree::TProjects, p, tree_dst->m_Projects) {

        list<CProjKey> new_depends;
        CProjItem& project = p->second;
        ITERATE(list<CProjKey>, n, project.m_Depends) {
            const CProjKey& depend_id = *n;

            bool dll_hosted = 
                (depend_id.Type() == CProjKey::eLib)  &&
                 GetApp().GetDllsInfo().IsDllHosted(depend_id.Id());
            if ( !dll_hosted ) {
                new_depends.push_back(depend_id);
            } else {
                new_depends.push_back
                    (CProjKey(CProjKey::eDll, 
                     GetApp().GetDllsInfo().GetDllHost(depend_id.Id())));
            }
        }
        new_depends.sort();
        new_depends.unique();
        project.m_Depends = new_depends;
    }

    list<string> dll_ids;
    CreateDllsList(tree_src, &dll_ids);

    list<string> dll_depends_ids;
    CollectDllsDepends(dll_ids, &dll_depends_ids);
    copy(dll_depends_ids.begin(), 
        dll_depends_ids.end(), back_inserter(dll_ids));
    dll_ids.sort();
    dll_ids.unique();

    ITERATE(list<string>, p, dll_ids) {

        const string& dll_id = *p;
        CMsvcDllsInfo::SDllInfo dll_info;
        GetApp().GetDllsInfo().GetDllInfo(dll_id, &dll_info);

        CProjItem dll;
        s_InitalizeDllProj(dll_id, dll_info, &dll, tree_dst);

        bool is_empty = true;
        CProjectItemsTree::TProjects::const_iterator k;
        string str_log;
        ITERATE(list<string>, n, dll_info.m_Hosting) {
            const string& lib_id = *n;
            k = GetApp().GetWholeTree().m_Projects.find(CProjKey(CProjKey::eLib,
                                                                 lib_id));
            if (k == GetApp().GetWholeTree().m_Projects.end()) {
                str_log += " " + lib_id;
                continue;
                //LOG_POST(Error << "DLL " + dll_id + " generation skipped");
                //complete = false; // do not create incomplete DLLs
                //break;
            }
            const CProjItem& lib = k->second;
            s_AddProjItemToDll(lib, &dll);
            is_empty = false;
        }
        k = tree_src.m_Projects.find(CProjKey(CProjKey::eLib, dll_id));
        if (k != tree_src.m_Projects.end()) {
            const CProjItem& lib = k->second;
            s_AddProjItemToDll(lib, &dll);
            is_empty = false;
        }
        if ( !is_empty ) {
            tree_dst->m_Projects[CProjKey(CProjKey::eDll, dll_id)] = dll;
            if (str_log.empty()) {
                LOG_POST(Info << "Ok: " << dll_id);
            } else {
                LOG_POST(Info << "Reduced: " << dll_id
                              << ":     not found: " << str_log);
            }
        } else {
            LOG_POST(Info << "Skipped empty: " << dll_id);
        }
    }
}


void CreateDllsList(const CProjectItemsTree& tree_src,
                    list<string>*            dll_ids)
{
    dll_ids->clear();

    set<string> dll_set;

    ITERATE(CProjectItemsTree::TProjects, p, tree_src.m_Projects) {

        const CProjKey&  proj_id = p->first;
//        const CProjItem& project = p->second;

        bool dll_hosted = (proj_id.Type() == CProjKey::eLib)  &&
                           GetApp().GetDllsInfo().IsDllHosted(proj_id.Id());
        if ( dll_hosted ) {
            dll_set.insert(GetApp().GetDllsInfo().GetDllHost(proj_id.Id()));
        }
    }    
    copy(dll_set.begin(), dll_set.end(), back_inserter(*dll_ids));
}


void CollectDllsDepends(const list<string>& dll_ids,
                        list<string>*       dll_depends_ids)
{
    size_t depends_cnt = dll_depends_ids->size();

    ITERATE(list<string>, p, dll_ids) {

        const string& dll_id = *p;
        CMsvcDllsInfo::SDllInfo dll_info;
        GetApp().GetDllsInfo().GetDllInfo(dll_id, &dll_info);

        ITERATE(list<string>, n, dll_info.m_Depends) {

            const string& depend_id = *n;
            if ( s_IsDllProject(depend_id)  &&
                 find(dll_ids.begin(), 
                      dll_ids.end(), depend_id) == dll_ids.end() ) {
                dll_depends_ids->push_back(depend_id);
            }
        }
    }
    
    dll_depends_ids->sort();
    dll_depends_ids->unique();
    if ( !(dll_depends_ids->size() > depends_cnt) )
        return;
    
    list<string> total_dll_ids(dll_ids);
    copy(dll_depends_ids->begin(), 
         dll_depends_ids->end(), back_inserter(total_dll_ids));
    total_dll_ids.sort();
    total_dll_ids.unique();

    CollectDllsDepends(total_dll_ids, dll_depends_ids);
}


END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.29  2005/01/31 16:37:38  gouriano
 * Keep track of subproject types and propagate it down the project tree
 *
 * Revision 1.28  2005/01/10 15:40:09  gouriano
 * Make PTB pick up MSVC tune-up for DLLs
 *
 * Revision 1.27  2004/12/20 21:07:33  gouriano
 * Eliminate compiler warnings
 *
 * Revision 1.26  2004/12/20 15:22:09  gouriano
 * Changed diagnostic output
 *
 * Revision 1.25  2004/12/06 18:12:20  gouriano
 * Improved diagnostics
 *
 * Revision 1.24  2004/12/01 15:28:38  gouriano
 * Skip empty DLL projects
 *
 * Revision 1.23  2004/10/12 16:18:26  ivanov
 * Added configurable file support
 *
 * Revision 1.22  2004/10/04 15:31:57  gouriano
 * Take into account LIB_OR_DLL Makefile parameter
 *
 * Revision 1.21  2004/09/13 13:49:08  gouriano
 * Make it to rely more on UNIX makefiles
 *
 * Revision 1.20  2004/06/23 14:15:59  gorelenk
 * CreateDllBuildTree - DLL generation will be skipped of not all libraries
 * are available in local build tree - for the sake of 'import_project.wsf' .
 *
 * Revision 1.19  2004/06/07 13:46:21  gorelenk
 * Class CMsvcDllsInfo separated from application config.
 *
 * Revision 1.18  2004/06/04 14:48:53  gorelenk
 * Changed dll_main location.
 *
 * Revision 1.17  2004/06/01 16:05:40  gorelenk
 * Changed implementation of s_AddProjItemToDll : added conditional branch for
 * processing lib_choice when library to add is available as a third-party .
 *
 * Revision 1.16  2004/05/26 17:58:03  gorelenk
 * Changed implementation of s_AddProjItemToDll - added registration of
 * inline source files.
 *
 * Revision 1.15  2004/05/21 21:41:41  gorelenk
 * Added PCH ncbi_pch.hpp
 *
 * Revision 1.14  2004/05/19 14:24:32  gorelenk
 * Changed s_AddProjItemToDll - added code for registration of DLLs source
 * files.
 *
 * Revision 1.13  2004/04/20 14:14:24  gorelenk
 * Changed implementation of CMsvcDllsInfo::GetDllsList and
 * CMsvcDllsInfo::GetDllInfo .
 *
 * Revision 1.12  2004/04/13 17:07:52  gorelenk
 * Changed implementation of s_InitalizeDllProj .
 *
 * Revision 1.11  2004/03/23 14:39:41  gorelenk
 * Changed implementation of functions s_InitalizeDllProj and
 * CreateDllBuildTree to use whole build tree by GetApp().GetWholeTree().
 *
 * Revision 1.10  2004/03/16 23:51:43  gorelenk
 * Changed implementation of s_AddProjItemToDll .
 *
 * Revision 1.9  2004/03/16 17:43:33  gorelenk
 * Addition of dllmain moved from s_AddProjItemToDll to s_InitalizeDllProj .
 *
 * Revision 1.8  2004/03/16 16:37:33  gorelenk
 * Changed msvc7_prj subdirs structure: Separated "static" and "dll" branches.
 *
 * Revision 1.7  2004/03/15 21:21:47  gorelenk
 * Added definition of function CollectDllsDepends. Changed implementation of
 * function CreateDllBuildTree.
 *
 * Revision 1.6  2004/03/12 20:20:00  gorelenk
 * Changed implementation of s_InitalizeDllProj - changed processing of
 * dll dependencies.
 *
 * Revision 1.5  2004/03/10 22:54:58  gorelenk
 * Changed implementation of s_InitalizeDllProj - added dependencies from
 * other dll.
 *
 * Revision 1.4  2004/03/10 16:36:39  gorelenk
 * Implemented functions FilterOutDllHostedProjects,
 * CreateDllBuildTree and CreateDllsList.
 *
 * Revision 1.3  2004/03/08 23:34:06  gorelenk
 * Implemented member-functions GelDllInfo, IsDllHosted, GetDllHost and
 * GetLibPrefixes of class CMsvcDllsInfo.
 *
 * Revision 1.2  2004/03/03 22:16:45  gorelenk
 * Added implementation of class CMsvcDllsInfo.
 *
 * Revision 1.1  2004/03/03 17:07:14  gorelenk
 * Initial revision.
 *
 * ===========================================================================
 */
