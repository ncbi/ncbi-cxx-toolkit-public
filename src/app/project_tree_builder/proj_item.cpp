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


static CProjItem::TProjType s_GetAsnProjectType(const string& base_dir,
                                                const string& projname);


//-----------------------------------------------------------------------------
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
                     const list<string>& depends,
                     const list<string>& requires,
                     const list<CDataToolGeneratedSrc> datatool_src)
   :m_Name    (name), 
    m_ID      (id),
    m_ProjType(type),
    m_SourcesBaseDir (sources_base),
    m_Sources (sources), 
    m_Depends (depends),
    m_Requires(requires),
    m_DatatoolSources(datatool_src)
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
    m_Requires       = item.m_Requires;
    m_DatatoolSources= item.m_DatatoolSources;
}


//-----------------------------------------------------------------------------
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

        string source_base_dir;
        CDirEntry::SplitPath(p->first, &source_base_dir);

        TMakeInInfoList list_info;
        AnalyzeMakeIn(p->second, &list_info);
        ITERATE(TMakeInInfoList, i, list_info) {

            const SMakeInInfo& info = *i;

            //Iterate all project_name(s) from makefile.in 
            ITERATE(list<string>, n, info.m_ProjNames) {

                //project id
                const string proj_name = *n;
        
                string applib_mfilepath = 
                    CDirEntry::ConcatPath(source_base_dir,
                         CreateMakeAppLibFileName(source_base_dir, 
                                                  info.m_Type, proj_name));
                if ( applib_mfilepath.empty() )
                    continue;
            
                if (info.m_Type == SMakeInInfo::eApp) {

                    DoCreateAppProject(source_base_dir, 
                                       proj_name, 
                                       applib_mfilepath, makeapp, tree);
                }
                else if (info.m_Type == SMakeInInfo::eLib) {

                    DoCreateLibProject(source_base_dir, 
                                       proj_name, 
                                       applib_mfilepath, makelib, tree);
                }
                else if (info.m_Type == SMakeInInfo::eAsn) {

                    DoCreateAsnProject(source_base_dir, 
                                       proj_name, 
                                       applib_mfilepath, 
                                       makeapp, makelib, tree);
                }
            }
        }
    }

    {{
        // REQUIRES tags in Makefile.in(s)
        ITERATE(TFiles, p, makein) {

            string makein_dir;
            CDirEntry::SplitPath(p->first, &makein_dir);
            const CSimpleMakeFileContents& makein_contents = p->second;
            NON_CONST_ITERATE(TProjects, n, tree->m_Projects) {

                CProjItem& project = n->second;
                if ( IsSubdir(makein_dir, project.m_SourcesBaseDir) ) {

                    CSimpleMakeFileContents::TContents::const_iterator k = 
                        makein_contents.m_Contents.find("REQUIRES");
                    if ( k != makein_contents.m_Contents.end() ) {

                        const list<string> requires = k->second;
                        copy(requires.begin(), 
                             requires.end(), 
                             back_inserter(project.m_Requires));
                        
                        project.m_Requires.sort();
                        project.m_Requires.unique();
                    }
                }
            }
        }
    }}
}


string CProjectItemsTree::DoCreateAppProject(const string& source_base_dir,
                                             const string& proj_name,
                                             const string& applib_mfilepath,
                                             const TFiles& makeapp , 
                                             CProjectItemsTree* tree)
{
    TFiles::const_iterator m = makeapp.find(applib_mfilepath);
    if (m == makeapp.end()) {

        LOG_POST("**** No Makefile.*.app for Makefile.in :"
                  + applib_mfilepath);
        return "";
    }

    CSimpleMakeFileContents::TContents::const_iterator k = 
        m->second.m_Contents.find("SRC");
    if (k == m->second.m_Contents.end()) {

        LOG_POST("**** No SRC key in Makefile.*.app :"
                  + applib_mfilepath);
        return "";
    }

    //sources - full pathes
    //We'll create relative pathes from them
    list<string> sources;
    CreateFullPathes(source_base_dir, k->second, &sources);

    //depends
    list<string> depends;
    k = m->second.m_Contents.find("LIB");
    if (k != m->second.m_Contents.end())
        depends = k->second;

    //requires
    list<string> requires;
    k = m->second.m_Contents.find("REQUIRES");
    if (k != m->second.m_Contents.end())
        requires = k->second;

    //project name
    k = m->second.m_Contents.find("APP");
    if (k == m->second.m_Contents.end()  ||  
                                           k->second.empty()) {

        LOG_POST("**** No APP key or empty in Makefile.*.app :"
                  + applib_mfilepath);
        return "";
    }
    string proj_id = k->second.front();

    list<CDataToolGeneratedSrc> datatool_src;
    tree->m_Projects[proj_id] =  CProjItem(CProjItem::eApp, 
                                           proj_name, 
                                           proj_id,
                                           source_base_dir,
                                           sources, 
                                           depends,
                                           requires,
                                           datatool_src);
    return proj_id;
}


string CProjectItemsTree::DoCreateLibProject(const string& source_base_dir,
                                             const string& proj_name,
                                             const string& applib_mfilepath,
                                             const TFiles& makelib , 
                                             CProjectItemsTree* tree)
{
    TFiles::const_iterator m = makelib.find(applib_mfilepath);
    if (m == makelib.end()) {

        LOG_POST("**** No Makefile.*.lib for Makefile.in :"
                  + applib_mfilepath);
        return "";
    }

    CSimpleMakeFileContents::TContents::const_iterator k = 
        m->second.m_Contents.find("SRC");
    if (k == m->second.m_Contents.end()) {

        LOG_POST("**** No SRC key in Makefile.*.lib :"
                  + applib_mfilepath);
        return "";
    }

    // sources - full pathes 
    // We'll create relative pathes from them)
    list<string> sources;
    CreateFullPathes(source_base_dir, k->second, &sources);

    // depends - TODO
    list<string> depends;

    //requires
    list<string> requires;
    k = m->second.m_Contents.find("REQUIRES");
    if (k != m->second.m_Contents.end())
        requires = k->second;

    //project name
    k = m->second.m_Contents.find("LIB");
    if (k == m->second.m_Contents.end()  ||  
                                           k->second.empty()) {

        LOG_POST("**** No LIB key or empty in Makefile.*.lib :"
                  + applib_mfilepath);
        return "";
    }
    string proj_id = k->second.front();

    list<CDataToolGeneratedSrc> datatool_src;
    tree->m_Projects[proj_id] =  CProjItem(CProjItem::eLib,
                                           proj_name, 
                                           proj_id,
                                           source_base_dir,
                                           sources, 
                                           depends,
                                           requires,
                                           datatool_src);
    return proj_id;
}


string CProjectItemsTree::DoCreateAsnProject(const string& source_base_dir,
                                             const string& proj_name,
                                             const string& applib_mfilepath,
                                             const TFiles& makeapp, 
                                             const TFiles& makelib, 
                                             CProjectItemsTree* tree)
{
    CProjItem::TProjType proj_type = 
        s_GetAsnProjectType(source_base_dir, proj_name);
    
    string proj_id = 
        proj_type == CProjItem::eLib? 
            DoCreateLibProject(source_base_dir, 
                               proj_name, applib_mfilepath, makelib, tree) : 
            DoCreateAppProject(source_base_dir, 
                               proj_name, applib_mfilepath, makeapp, tree);
    if ( proj_id.empty() )
        return "";
    
    CProjectItemsTree::TProjects::iterator p = tree->m_Projects.find(proj_id);
    if (p == tree->m_Projects.end()) {
        LOG_POST("^^^^^^^^^ Can not find ASN project with id : " + proj_id);
        return "";
    }
    CProjItem& project = p->second;

    //Add depends from datatoool for ASN projects
    project.m_Depends.push_back(GetApp().GetDatatoolId());

    //Will process .asn or .dtd files
    string source_file_path = CDirEntry::ConcatPath(source_base_dir, proj_name);
    if ( CDirEntry(source_file_path + ".asn").Exists() )
        source_file_path += ".asn";
    else if ( CDirEntry(source_file_path + ".dtd").Exists() )
        source_file_path += ".dtd";

    CDataToolGeneratedSrc data_tool_src;
    CDataToolGeneratedSrc::LoadFrom(source_file_path, &data_tool_src);
    if ( !data_tool_src.IsEmpty() )
        project.m_DatatoolSources.push_back(data_tool_src);

    return proj_id;
}


void CProjectItemsTree::AnalyzeMakeIn
    (const CSimpleMakeFileContents& makein_contents,
     TMakeInInfoList*               info)
{
    info->clear();

    CSimpleMakeFileContents::TContents::const_iterator p = 
        makein_contents.m_Contents.find("LIB_PROJ");

    if (p != makein_contents.m_Contents.end()) {

        info->push_back(SMakeInInfo(SMakeInInfo::eLib, p->second)); 
    }

    p = makein_contents.m_Contents.find("APP_PROJ");
    if (p != makein_contents.m_Contents.end()) {

        info->push_back(SMakeInInfo(SMakeInInfo::eApp, p->second)); 
    }

    p = makein_contents.m_Contents.find("ASN_PROJ");
    if (p != makein_contents.m_Contents.end()) {

        info->push_back(SMakeInInfo(SMakeInInfo::eAsn, p->second)); 
    }

    //TODO - DLL_PROJ
}


static CProjItem::TProjType s_GetAsnProjectType(const string& base_dir,
                                                const string& projname)
{
    string fname = "Makefile." + projname;
    
    if ( CDirEntry(CDirEntry::ConcatPath
               (base_dir, fname + ".lib")).Exists() )
        return CProjItem::eLib;
    else if (CDirEntry(CDirEntry::ConcatPath
               (base_dir, fname + ".app")).Exists() )
        return CProjItem::eApp;

    LOG_POST("Inconsistent ASN project: " + projname);
    return CProjItem::eNoProj;
}


string CProjectItemsTree::CreateMakeAppLibFileName
    (const string&            base_dir,
     SMakeInInfo::TMakeinType makeintype,
     const string&            projname)
{
    string fname = "Makefile." + projname;

    switch (makeintype) {
    case SMakeInInfo::eApp:
        fname += ".app";
        break;
    case SMakeInInfo::eLib:
        fname += ".lib";
        break;
    case SMakeInInfo::eAsn: {
        CProjItem::TProjType proj_type = 
            s_GetAsnProjectType(base_dir, projname);
        
        if (proj_type == CProjItem::eNoProj)
            return "";
        
        fname += proj_type==CProjItem::eLib? ".lib": ".app";
        break;
    }
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


//-----------------------------------------------------------------------------
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


//-----------------------------------------------------------------------------
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


//-----------------------------------------------------------------------------
CProjectTreeFolders::CProjectTreeFolders(const CProjectItemsTree& tree)
:m_RootParent("/", NULL)
{
    ITERATE(CProjectItemsTree::TProjects, p, tree.m_Projects) {

        const CProjItem& project = p->second;
        
        TPath path;
        CreatePath(GetApp().GetProjectTreeInfo().m_RootSrc, 
                   project.m_SourcesBaseDir, 
                   &path);
        SProjectTreeFolder* folder = FindOrCreateFolder(path);
        folder->m_Name = path.back();
        folder->m_Projects.insert(project.m_ID);
    }
}


SProjectTreeFolder* 
CProjectTreeFolders::CreateFolder(SProjectTreeFolder* parent,
                                  const string&       folder_name)
{
    m_Folders.push_back(SProjectTreeFolder(folder_name, parent));
    SProjectTreeFolder* inserted = &(m_Folders.back());

    parent->m_Siblings.insert
        (SProjectTreeFolder::TSiblings::value_type(folder_name, inserted));
    
    return inserted;

}

SProjectTreeFolder* 
CProjectTreeFolders::FindFolder(const TPath& path)
{
    SProjectTreeFolder& folder_i = m_RootParent;
    ITERATE(TPath, p, path) {
        const string& node = *p;
        SProjectTreeFolder::TSiblings::iterator n = 
            folder_i.m_Siblings.find(node);
        if (n == folder_i.m_Siblings.end())
            return NULL;
        folder_i = *(n->second);
    }
    return &folder_i;
}


SProjectTreeFolder* 
CProjectTreeFolders::FindOrCreateFolder(const TPath& path)
{
    SProjectTreeFolder* folder_i = &m_RootParent;
    ITERATE(TPath, p, path) {
        const string& node = *p;
        SProjectTreeFolder::TSiblings::iterator n = folder_i->m_Siblings.find(node);
        if (n == folder_i->m_Siblings.end()) {
            folder_i = CreateFolder(folder_i, node);
        } else {        
            folder_i = n->second;
        }
    }
    return folder_i;
}


void CProjectTreeFolders::CreatePath(const string& root_src_dir, 
                                     const string& project_base_dir,
                                     TPath*        path)
{
    path->clear();
    
    string rel_dir = 
        CDirEntry::CreateRelativePath(root_src_dir, project_base_dir);
    string sep(1, CDirEntry::GetPathSeparator());
    NStr::Split(rel_dir, sep, *path);
}


END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.7  2004/01/30 20:44:22  gorelenk
 * Initial revision.
 *
 * Revision 1.6  2004/01/28 17:55:50  gorelenk
 * += For msvc makefile support of :
 *                 Requires tag, ExcludeProject tag,
 *                 AddToProject section (SourceFiles and IncludeDirs),
 *                 CustomBuild section.
 * += For support of user local site.
 *
 * Revision 1.5  2004/01/26 19:27:30  gorelenk
 * += MSVC meta makefile support
 * += MSVC project makefile support
 *
 * Revision 1.4  2004/01/22 17:57:55  gorelenk
 * first version
 *
 * ===========================================================================
 */
