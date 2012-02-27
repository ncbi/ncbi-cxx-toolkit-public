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
#include "msvc_dlls_info.hpp"
#include "proj_builder_app.hpp"
#include "msvc_prj_defines.hpp"
#include "proj_projects.hpp"
#include "proj_tree_builder.hpp"
#include "msvc_project_context.hpp"
#include "msvc_prj_files_collector.hpp"
#include "msvc_dlls_info_utils.hpp"
#include "ptb_err_codes.hpp"

#include <corelib/ncbistre.hpp>

#include <algorithm>

BEGIN_NCBI_SCOPE


//-----------------------------------------------------------------------------
void FilterOutDllHostedProjects(const CProjectItemsTree& tree_src, 
                                CProjectItemsTree*       tree_dst)
{
    tree_dst->m_RootSrc = tree_src.m_RootSrc;

    tree_dst->m_Projects.clear();
    ITERATE(CProjectItemsTree::TProjects, p, tree_src.m_Projects) {

        const CProjKey&  proj_id = p->first;
        const CProjItem& project = p->second;

        if (proj_id.Type() == CProjKey::eDll) {
            continue;
        }
        bool dll_hosted =
            (proj_id.Type() == CProjKey::eLib) &&
            !project.m_DllHost.empty();
        if ( !dll_hosted) {
            tree_dst->m_Projects[proj_id] = project;
        }
    }    
}

static bool s_IsInTree(CProjKey::TProjType      proj_type,
                       const string&            proj_id,
                       const CProjectItemsTree* tree)
{
    return tree->m_Projects.find
                  (CProjKey(proj_type, 
                            proj_id)) != 
                                    tree->m_Projects.end();
}


static void s_InitalizeDllProj(const string&                  dll_id, 
                               CProjItem*                     dll,
                               const CProjectItemsTree&       tree_src,
                               CProjectItemsTree*             tree_dst)
{
    list<CProjKey> new_depends;
    ITERATE(list<CProjKey>, p, dll->m_Depends) {

        const string& depend_id = p->Id();
        const CProjectItemsTree* tree;

        // Is this a dll?
        if ( s_IsInTree(CProjKey::eDll, depend_id, tree = &tree_src) ||
             s_IsInTree(CProjKey::eDll, depend_id, tree = &GetApp().GetWholeTree())) {
            new_depends.push_back(CProjKey(CProjKey::eDll, depend_id));    
        } else  {
            if ( s_IsInTree(CProjKey::eApp, depend_id, tree = &tree_src) ||
                 s_IsInTree(CProjKey::eApp, depend_id, tree = &GetApp().GetWholeTree()) ) {

                CProjKey depend_key(CProjKey::eApp, depend_id);
                new_depends.push_back(depend_key);
                tree_dst->m_Projects[depend_key] = 
                    (tree->m_Projects.find(depend_key))->second;
            }
            else if ( s_IsInTree(CProjKey::eLib, depend_id, tree = &tree_src) ||
                      s_IsInTree(CProjKey::eLib, depend_id, tree = &GetApp().GetWholeTree()) ) {

                string dll_host = tree->m_Projects.find(CProjKey(CProjKey::eLib, depend_id))->second.m_DllHost;
                if (!dll_host.empty()) {
                    new_depends.push_back(CProjKey(CProjKey::eDll, dll_host));    
                } else {
                    CProjKey depend_key(CProjKey::eLib, depend_id);
                    new_depends.push_back(depend_key); 
                    tree_dst->m_Projects[depend_key] = 
                        (tree->m_Projects.find(depend_key))->second;
                }

            } else  {
                if (GetApp().m_AddMissingLibs) {
                    CProjKey depend_key(CProjKey::eLib, depend_id);
                    new_depends.push_back(depend_key); 
                }
                PTB_WARNING_EX(dll_id, ePTB_MissingDependency,
                            "Missing dependency: " << depend_id);
            }
        }
    }
    dll->m_Depends = new_depends;

    if (CMsvc7RegSettings::GetMsvcPlatform() < CMsvc7RegSettings::eUnix) {
        string dll_main = GetApp().GetProjectTreeInfo().m_Compilers;
        dll_main = CDirEntry::ConcatPath(dll_main, GetApp().GetRegSettings().m_CompilersSubdir);
        dll_main = CDirEntry::ConcatPath(dll_main, GetApp().GetBuildType().GetTypeStr());
        dll_main = CDirEntry::ConcatPath(dll_main, "dll_main");
        dll->m_Sources.push_back(CDirEntry::CreateRelativePath(dll->m_SourcesBaseDir, dll_main));
    }
}


static void s_AddProjItemToDll(const CProjectItemsTree& tree_src,
    const CProjItem& lib, CProjItem* dll)
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
    if (!lib.m_External) {
        dll->m_External = false;
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

    copy(lib_context.IncludeDirsAbs().begin(), 
         lib_context.IncludeDirsAbs().end(), back_inserter(dll->m_Includes));
    copy(lib_context.InlineDirsAbs().begin(), 
         lib_context.InlineDirsAbs().end(), back_inserter(dll->m_Inlines));

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

        CProjectItemsTree::TProjects::const_iterator i = tree_src.m_Projects.find(depend_id);
        if (i != tree_src.m_Projects.end()) {
            if (i->second.m_DllHost.empty()) {
                dll->m_Depends.push_back(depend_id);
            } else {
                dll->m_Depends.push_back(CProjKey(CProjKey::eDll, i->second.m_DllHost));
            }
        } else {
            string host = GetDllHost(tree_src,depend_id.Id());
            if (!host.empty()) {
                dll->m_Depends.push_back(CProjKey(CProjKey::eDll, host));
            }
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

    // watchers
    if (!lib.m_Watchers.empty()) {
        if (!dll->m_Watchers.empty()) {
            dll->m_Watchers += " ";
        }
        dll->m_Watchers += lib.m_Watchers;
    }
    {{
        string makefile_name = 
            SMakeProjectT::CreateMakeAppLibFileName(lib.m_SourcesBaseDir,
                                                    lib.m_Name);
        CSimpleMakeFileContents makefile(makefile_name,eMakeType_Undefined);
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

    dll->m_MakeType = max(lib.m_MakeType, dll->m_MakeType);
}

void AnalyzeDllData(CProjectItemsTree& tree)
{
    set<string> dll_to_erase;
    NON_CONST_ITERATE(CProjectItemsTree::TProjects, p, tree.m_Projects) {
        const CProjKey& key = p->first;
        CProjItem& project = p->second;
        if (key.Type() == CProjKey::eDll) {
            ITERATE( list<string>, h,  project.m_HostedLibs) {
                CProjectItemsTree::TProjects::iterator i;
                i = tree.m_Projects.find(CProjKey(CProjKey::eLib, *h));
                if (i != tree.m_Projects.end()) {
                    if (*h != key.Id()) {
                        i->second.m_DllHost = key.Id();
                        i = tree.m_Projects.find(CProjKey(CProjKey::eDll, *h));
                        if (i != tree.m_Projects.end()) {
                            dll_to_erase.insert(*h);
                        }
                    } else if (i->second.m_DllHost.empty()) {
                        i->second.m_DllHost = key.Id();
                    }
                }
            }
        }
    }
    ITERATE(set<string>, d, dll_to_erase) {
        CProjectItemsTree::TProjects::iterator i;
        i = tree.m_Projects.find(CProjKey(CProjKey::eDll, *d));
        if (i != tree.m_Projects.end()) {
            tree.m_Projects.erase(i);
        }
    }
}

string GetDllHost(const CProjectItemsTree& tree, const string& lib)
{
    ITERATE(CProjectItemsTree::TProjects, p, tree.m_Projects) {
        const CProjKey& key = p->first;
        const CProjItem& project = p->second;
        if (key.Type() == CProjKey::eDll) {
            ITERATE( list<string>, h,  project.m_HostedLibs) {
                if (*h == lib) {
                    return key.Id();
                }
            }
        }
    }
    return kEmptyStr;
}

void CreateDllBuildTree(const CProjectItemsTree& tree_src, 
                        CProjectItemsTree*       tree_dst)
{
    tree_dst->m_RootSrc = tree_src.m_RootSrc;

    FilterOutDllHostedProjects(tree_src, tree_dst);

    list<string> dll_ids;
    CreateDllsList(tree_src, &dll_ids);

    list<string> dll_depends_ids;
    CollectDllsDepends(tree_src, dll_ids, &dll_depends_ids);
    copy(dll_depends_ids.begin(), 
        dll_depends_ids.end(), back_inserter(dll_ids));
    dll_ids.sort();
    dll_ids.unique();

    ITERATE(list<string>, p, dll_ids) {

        const string& dll_id = *p;
        CProjectItemsTree::TProjects::const_iterator d;
        d = tree_src.m_Projects.find(CProjKey(CProjKey::eDll, dll_id));
        if (d == tree_src.m_Projects.end()) {
            d = GetApp().GetWholeTree().m_Projects.find(CProjKey(CProjKey::eDll, dll_id));
            if (d == GetApp().GetWholeTree().m_Projects.end()) {
                LOG_POST(Error << "DLL project not found: " << dll_id);
                continue;
            }
        }
        CProjItem dll( d->second);
        s_InitalizeDllProj(dll_id, &dll, tree_src, tree_dst);

        CProjectItemsTree::TProjects::const_iterator k;
        bool is_empty = true;
        string str_log;
        ITERATE(list<string>, n, dll.m_HostedLibs) {
            const string& lib_id = *n;
            k = tree_src.m_Projects.find(CProjKey(CProjKey::eLib,lib_id));
            if (k == tree_src.m_Projects.end()) {
                k = GetApp().GetWholeTree().m_Projects.find(CProjKey(CProjKey::eLib, lib_id));
                if (k != GetApp().GetWholeTree().m_Projects.end()) {
                    const CProjItem& lib = k->second;
                    s_AddProjItemToDll(tree_src, lib, &dll);
                    is_empty = false;
                } else if (GetApp().GetSite().GetChoiceForLib(lib_id) 
                                                   == CMsvcSite::e3PartyLib ) {
                    CMsvcSite::SLibChoice choice = 
                        GetApp().GetSite().GetLibChoiceForLib(lib_id);
                    dll.m_Requires.push_back(choice.m_3PartyLib);
                    dll.m_Requires.sort();
                    dll.m_Requires.unique();
                } else {
                    str_log += " " + lib_id;
                }
                continue;
            }
            const CProjItem& lib = k->second;
            s_AddProjItemToDll(tree_src, lib, &dll);
            is_empty = false;
        }
        if ( !is_empty ) {
            tree_dst->m_Projects[CProjKey(CProjKey::eDll, dll_id)] = dll;
            if ( !str_log.empty() ) {
                string path = CDirEntry::ConcatPath(dll.m_SourcesBaseDir, dll_id);
                path += CMsvc7RegSettings::GetVcprojExt();
                PTB_WARNING_EX(path, ePTB_ConfigurationError,
                               "Missing libraries not found: " << str_log);
            }
        } else {
            string path = CDirEntry::ConcatPath(dll.m_SourcesBaseDir, dll_id);
            path += CMsvc7RegSettings::GetVcprojExt();
            PTB_WARNING_EX(path, ePTB_ProjectExcluded,
                           "Skipped empty project: " << dll_id);
        }
    }
    NON_CONST_ITERATE(CProjectItemsTree::TProjects, p, tree_dst->m_Projects) {

        list<CProjKey> new_depends;
        CProjItem& project = p->second;
        ITERATE(list<CProjKey>, n, project.m_Depends) {
            const CProjKey& depend_id = *n;

            bool found = false;
            for (int pass=0; !found && pass<2; ++pass) {
                const CProjectItemsTree& tree = pass ? tree_src : *tree_dst;
                CProjectItemsTree::TProjects::const_iterator i = tree.m_Projects.find(depend_id);
                if (i != tree.m_Projects.end()) {
                    if (i->second.m_DllHost.empty()) {
                        new_depends.push_back(depend_id);
                    } else {
                        new_depends.push_back(CProjKey(CProjKey::eDll, i->second.m_DllHost));
                    }
                    found = true;
                    if (pass == 1 && GetApp().m_AddMissingLibs &&
                        i->second.m_MakeType >= eMakeType_Excluded) {
                        copy(i->second.m_Depends.begin(), i->second.m_Depends.end(),
                            back_inserter(new_depends));
                    }
                } else /* if (!GetApp().m_ScanWholeTree)*/ {
                    ITERATE(CProjectItemsTree::TProjects, d, tree.m_Projects) {
                        const list<string>& lst = d->second.m_HostedLibs;
                        if ( find (lst.begin(), lst.end(), depend_id.Id()) != lst.end()) {
                            new_depends.push_back(d->first);
                            found = true;
                            break;
                        }
                    }
                }
            }
            if (!found) {
                string path = CDirEntry::ConcatPath(project.m_SourcesBaseDir, project.m_ID);
                if (!SMakeProjectT::IsConfigurableDefine(depend_id.Id())) {
                    if (GetApp().m_AddMissingLibs) {
                        new_depends.push_back(depend_id);
                    } else {
                        PTB_WARNING_EX(path, ePTB_ProjectNotFound,
                                    "Depends on missing project: " << depend_id.Id());
                    }
                }

            }
        }
        new_depends.sort();
        new_depends.unique();
        project.m_Depends = new_depends;
    }
}


void CreateDllsList(const CProjectItemsTree& tree_src,
                    list<string>*            dll_ids)
{
    dll_ids->clear();

    set<string> dll_set;

    ITERATE(CProjectItemsTree::TProjects, p, tree_src.m_Projects) {
        if ( !p->second.m_DllHost.empty() ) {
            dll_set.insert(p->second.m_DllHost);
        }
    }    
    copy(dll_set.begin(), dll_set.end(), back_inserter(*dll_ids));
}


void CollectDllsDepends(const CProjectItemsTree& tree_src, 
                        const list<string>& dll_ids,
                        list<string>*       dll_depends_ids)
{
    size_t depends_cnt = dll_depends_ids->size();

    ITERATE(list<string>, p, dll_ids) {

        const string& dll_id = *p;
        CProjectItemsTree::TProjects::const_iterator i;
        i = tree_src.m_Projects.find( CProjKey(CProjKey::eDll,dll_id));
        if (i != tree_src.m_Projects.end()) {
            ITERATE(list<CProjKey>, n, i->second.m_Depends) {
                if ( tree_src.m_Projects.find( CProjKey(CProjKey::eDll,n->Id())) !=
                     tree_src.m_Projects.end() &&
                     find(dll_ids.begin(), dll_ids.end(), n->Id()) == dll_ids.end()) {
                    dll_depends_ids->push_back(n->Id());
                }
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

    CollectDllsDepends(tree_src, total_dll_ids, dll_depends_ids);
}


END_NCBI_SCOPE
