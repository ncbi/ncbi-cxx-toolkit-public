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

#include <app/project_tree_builder/proj_item.hpp>
#include <app/project_tree_builder/proj_builder_app.hpp>
#include <set>

BEGIN_NCBI_SCOPE


CProjItem::CProjItem(void)
{
    Clear();
}


CProjItem::CProjItem(const CProjItem& item)
{
    SetFrom(item);
}


CProjItem& CProjItem::operator= (const CProjItem& item)
{
    if (this != &item) {
        Clear();
        SetFrom(item);
    }
    return *this;
}


CProjItem::CProjItem(TProjType type,
                     const string& name,
                     const string& id,
                     const string& sources_base,
                     const list<string>& sources, 
                     const list<string>& depends)
   :m_Name    (name), 
    m_ID      (id),
    m_ProjType(type),
    m_SourcesBaseDir(sources_base),
    m_Sources (sources), 
    m_Depends (depends)
{
}



CProjItem::~CProjItem(void)
{
    Clear();
}


void CProjItem::Clear(void)
{
    m_ProjType = eNoProj;
}


void CProjItem::SetFrom(const CProjItem& item)
{
    m_Name           = item.m_Name;
    m_ID		     = item.m_ID;
    m_ProjType       = item.m_ProjType;
    m_SourcesBaseDir = item.m_SourcesBaseDir;
    m_Sources        = item.m_Sources;
    m_Depends        = item.m_Depends;
}


////////////////////////////////////////////////////////////////////////////////


CProjectItemsTree::CProjectItemsTree(void)
{
    Clear();
}


CProjectItemsTree::CProjectItemsTree(const string& root_src)
{
    Clear();
    m_RootSrc = root_src;
}


CProjectItemsTree::CProjectItemsTree(const CProjectItemsTree& projects)
{
    SetFrom(projects);
}


CProjectItemsTree& 
CProjectItemsTree::operator= (const CProjectItemsTree& projects)
{
    if (this != &projects) {
        Clear();
        SetFrom(projects);
    }
    return *this;
}


CProjectItemsTree::~CProjectItemsTree(void)
{
    Clear();
}


void CProjectItemsTree::Clear(void)
{
    m_RootSrc.erase();
    m_Projects.clear();
}


void CProjectItemsTree::SetFrom(const CProjectItemsTree& projects)
{
    m_RootSrc  = projects.m_RootSrc;
    m_Projects = projects.m_Projects;
}


void CProjectItemsTree::CreateFrom(const string& root_src,
                                   const TFiles& makein, 
                                   const TFiles& makelib, 
                                   const TFiles& makeapp , 
                                   CProjectItemsTree* tree)
{
    tree->m_Projects.clear();
    tree->m_RootSrc = root_src;

    ITERATE(TFiles, p, makein) {

        string sources_dir;
        CDirEntry::SplitPath(p->first, &sources_dir);

        TMakeInInfoList list_info;
        AnalyzeMakeIn(p->second, &list_info);
        ITERATE(TMakeInInfoList, i, list_info) {

            const SMakeInInfo& info = *i;

            //Iterate all project_name(s) from makefile.in 
            ITERATE(list<string>, n, info.m_ProjNames) {

                //project id
                const string proj_name = *n;
        
                string applib_mfilepath = CDirEntry::ConcatPath(sources_dir,
                               CreateMakeAppLibFileName(info.m_ProjType, proj_name));
            
                if (info.m_ProjType == CProjItem::eApp) {

                    TFiles::const_iterator m = makeapp.find(applib_mfilepath);
                    if (m == makeapp.end()) {

                        LOG_POST("**** No Makefile.*.app for Makefile.in :"
                                  + p->first);
                        continue;
                    }

                    CSimpleMakeFileContents::TContents::const_iterator k = 
                        m->second.m_Contents.find("SRC");
                    if (k == m->second.m_Contents.end()) {

                        LOG_POST("**** No SRC key in Makefile.*.app :"
                                  + applib_mfilepath);
                        continue;
                    }

                    //sources - full pathes
                    //We'll create relative pathes from them
                    list<string> sources;
                    CreateFullPathes(sources_dir, k->second, &sources);

                    //depends
                    list<string> depends;
                    k = m->second.m_Contents.find("LIB");
                    if (k != m->second.m_Contents.end())
                        depends = k->second;

                    //project name
                    k = m->second.m_Contents.find("APP");
                    if (k == m->second.m_Contents.end()  ||  
                                                            k->second.empty()) {

                        LOG_POST("**** No APP key or empty in Makefile.*.app :"
                                  + applib_mfilepath);
                        continue;
                    }
                    string proj_id = k->second.front();

                    string source_base_dir;
                    CDirEntry::SplitPath(p->first, &source_base_dir);
                    tree->m_Projects[proj_id] =  CProjItem( CProjItem::eApp, 
                                                               proj_name, 
                                                               proj_id,
                                                               source_base_dir,
                                                               sources, 
                                                               depends);

                }
                else if (info.m_ProjType == CProjItem::eLib) {

                    TFiles::const_iterator m = makelib.find(applib_mfilepath);
                    if (m == makelib.end()) {

                        LOG_POST("**** No Makefile.*.lib for Makefile.in :"
                                  + p->first);
                        continue;
                    }

                    CSimpleMakeFileContents::TContents::const_iterator k = 
                        m->second.m_Contents.find("SRC");
                    if (k == m->second.m_Contents.end()) {

                        LOG_POST("**** No SRC key in Makefile.*.lib :"
                                  + applib_mfilepath);
                        continue;
                    }

                    // sources - full pathes 
                    // We'll create relative pathes from them)
                    list<string> sources;
                    CreateFullPathes(sources_dir, k->second, &sources);

                    // depends - TODO
                    list<string> depends;

                    //project name
                    k = m->second.m_Contents.find("LIB");
                    if (k == m->second.m_Contents.end()  ||  
                                                            k->second.empty()) {

                        LOG_POST("**** No LIB key or empty in Makefile.*.lib :"
                                  + applib_mfilepath);
                        continue;
                    }
                    string proj_id = k->second.front();

                    string source_base_dir;
                    CDirEntry::SplitPath(p->first, &source_base_dir);
                    tree->m_Projects[proj_id] =  CProjItem( CProjItem::eLib,
                                                               proj_name, 
                                                               proj_id,
                                                               source_base_dir,
                                                               sources, 
                                                               depends);
                }
            }
        }
    }
}


void CProjectItemsTree::AnalyzeMakeIn
    (const CSimpleMakeFileContents& makein_contents,
     TMakeInInfoList*               info)
{
    info->clear();

    CSimpleMakeFileContents::TContents::const_iterator p = 
        makein_contents.m_Contents.find("LIB_PROJ");

    if (p != makein_contents.m_Contents.end()) {

        info->push_back(SMakeInInfo(CProjItem::eLib, p->second)); 
    }

    p = makein_contents.m_Contents.find("APP_PROJ");
    if (p != makein_contents.m_Contents.end()) {

        info->push_back(SMakeInInfo(CProjItem::eApp, p->second)); 
    }

    //TODO - DLL_PROJ
}


string CProjectItemsTree::CreateMakeAppLibFileName
    (CProjItem::TProjType projtype,
     const string&        projname)
{
    string fname = "Makefile." + projname;

    switch (projtype) {
    case CProjItem::eApp:
        fname += ".app";
        break;
    case CProjItem::eLib:
        fname += ".lib";
        break;
    }
    return fname;
}


void CProjectItemsTree::CreateFullPathes(const string&      dir, 
                                         const list<string> files,
                                         list<string>*      full_pathes)
{
    ITERATE(list<string>, p, files) {
        string full_path = CDirEntry::ConcatPath(dir, *p);
        full_pathes->push_back(full_path);
    }
}


void CProjectItemsTree::GetInternalDepends(list<string>* depends) const
{
    depends->clear();

    set<string> depends_set;

    ITERATE(TProjects, p, m_Projects) {
        const CProjItem& proj_item = p->second;
        ITERATE(list<string>, n, proj_item.m_Depends) {
            depends_set.insert(*n);
        }
    }

    copy(depends_set.begin(), depends_set.end(), back_inserter(*depends));
}


void 
CProjectItemsTree::GetExternalDepends(list<string>* external_depends) const
{
    external_depends->clear();

    list<string> depends;
    GetInternalDepends(&depends);
    ITERATE(list<string>, p, depends) {
        const string& depend_id = *p;
        if (m_Projects.find(depend_id) == m_Projects.end())
            external_depends->push_back(depend_id);
    }
}


void CProjectItemsTree::GetRoots(list<string>* ids) const
{
    ids->clear();

    set<string> dirs;
    ITERATE(TProjects, p, m_Projects) {
        //collect all project dirs:
        const CProjItem& project = p->second;
        dirs.insert(project.m_SourcesBaseDir);
    }

    ITERATE(TProjects, p, m_Projects) {
        //if project parent dir is not in dirs - it's a root
        const CProjItem& project = p->second;
        if (dirs.find(GetParentDir(project.m_SourcesBaseDir)) == dirs.end())
            ids->push_back(project.m_ID);
    }
}


void CProjectItemsTree::GetSiblings(const string& parent_id,
                                    list<string>* ids) const
{
    ids->clear();

    TProjects::const_iterator n = m_Projects.find(parent_id);
    if (n == m_Projects.end()) 
        return;

    const CProjItem& parent_project = n->second;

    ITERATE(TProjects, p, m_Projects) {
        //looking for projects having parent dir as parent_project
        const CProjItem& project_i = p->second;
        if (GetParentDir(project_i.m_SourcesBaseDir) == 
                                parent_project.m_SourcesBaseDir)
            ids->push_back(project_i.m_ID);
    }
}


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


void 
CProjectTreeBuilder::BuildOneProjectTree(const string&       start_node_path,
                                         const string&       root_src_path,
                                         CProjectItemsTree*  tree  )
{
    SMakeFiles subtree_makefiles;

    ProcessDir(start_node_path, 
               start_node_path == root_src_path,
               &subtree_makefiles);

    // Resolve macrodefines
    list<string> metadata_files;
    GetApp().GetMetaDataFiles(&metadata_files);
    ITERATE(list<string>, p, metadata_files) {
	    CSymResolver resolver(CDirEntry::ConcatPath(root_src_path, *p));
	    ResolveDefs(resolver, subtree_makefiles);
    }

    // Build projects tree
    CProjectItemsTree::CreateFrom(root_src_path,
                                  subtree_makefiles.m_In, 
                                  subtree_makefiles.m_Lib, 
                                  subtree_makefiles.m_App, tree);
}


void 
CProjectTreeBuilder::BuildProjectTree(const string&       start_node_path,
                                      const string&       root_src_path,
                                      CProjectItemsTree*  tree  )
{
    // Bulid subtree
    CProjectItemsTree target_tree;
    BuildOneProjectTree(start_node_path, root_src_path, &target_tree);

    // Analyze subtree depends
    list<string> external_depends;
    target_tree.GetExternalDepends(&external_depends);

    // If we have to add more projects to the target tree...
    if ( !external_depends.empty() ) {
        // Get whole project tree
        CProjectItemsTree whole_tree;
        BuildOneProjectTree(root_src_path, root_src_path, &whole_tree);

        list<string> depends_to_resolve = external_depends;

        while ( !depends_to_resolve.empty() ) {
            bool modified = false;
            ITERATE(list<string>, p, depends_to_resolve) {
                // id of project we have to resolve
                const string& prj_id = *p;
                CProjectItemsTree::TProjects::const_iterator n = 
                                             whole_tree.m_Projects.find(prj_id);
            
                if (n != whole_tree.m_Projects.end()) {
                    //insert this project to target_tree
                    target_tree.m_Projects[prj_id] = n->second;
                    modified = true;
                } else {
                    LOG_POST ("========= No project with id :" + prj_id);
                }
            }

            if (!modified) {
                //we done - no more projects was added to target_tree
                *tree = target_tree;
                return;
            } else {
                //continue resolving dependences
                target_tree.GetExternalDepends(&depends_to_resolve);
            }
        }
    }

    *tree = target_tree;
}


void CProjectTreeBuilder::ProcessDir(const string& dir_name, 
                                     bool          is_root, 
                                     SMakeFiles*   makefiles)
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
	            ProcessMakeInFile(path, makefiles);
            else if ( s_IsMakeLibFile(name) )
	            ProcessMakeLibFile(path, makefiles);
            else if ( s_IsMakeAppFile(name) )
	            ProcessMakeAppFile(path, makefiles);
        } 
        else if ( (*i)->IsDir() ) {
            ProcessDir(path, false, makefiles);
        }
    }
}


void CProjectTreeBuilder::ProcessMakeInFile(const string& file_name, 
                                            SMakeFiles*   makefiles)
{
    LOG_POST("Processing MakeIn: " + file_name);

    CSimpleMakeFileContents fc(file_name);
    if ( !fc.m_Contents.empty() )
	    makefiles->m_In[file_name] = fc;
}


void CProjectTreeBuilder::ProcessMakeLibFile(const string& file_name, 
                                             SMakeFiles*   makefiles)
{
    LOG_POST("Processing MakeLib: " + file_name);

    CSimpleMakeFileContents fc(file_name);
    if ( !fc.m_Contents.empty() )
	    makefiles->m_Lib[file_name] = fc;
}


void CProjectTreeBuilder::ProcessMakeAppFile(const string& file_name, 
                                             SMakeFiles*   makefiles)
{
    LOG_POST("Processing MakeApp: " + file_name);

    CSimpleMakeFileContents fc(file_name);
    if ( !fc.m_Contents.empty() )
	    makefiles->m_App[file_name] = fc;
}


//recursive resolving
void CProjectTreeBuilder::ResolveDefs(CSymResolver& resolver, 
                                      SMakeFiles&   makefiles)
{
    NON_CONST_ITERATE(TFiles, p, makefiles.m_App) {
	    NON_CONST_ITERATE(CSimpleMakeFileContents::TContents, 
                          n, 
                          p->second.m_Contents) {
            
            const string& key    = n->first;
            list<string>& values = n->second;
		    if (key == "LIB") {
                list<string> new_vals;
                bool modified = false;
                NON_CONST_ITERATE(list<string>, k, values) {
                    //iterate all values and try to resolve 
                    const string& val = *k;
	                list<string> resolved_def;
	                resolver.Resolve(val, &resolved_def);
	                if ( resolved_def.empty() )
		                new_vals.push_back(val); //not resolved - keep old val
                    else {
                        //was resolved
		                copy(resolved_def.begin(), 
			                 resolved_def.end(), 
			                 back_inserter(new_vals));
		                modified = true;
                    }
                }
                if (modified)
                    values = new_vals; // by ref!
		    }
        }
    }
}


END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.4  2004/01/22 17:57:55  gorelenk
 * first version
 *
 * ===========================================================================
 */
