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
#include <app/project_tree_builder/proj_src_resolver.hpp>
#include <set>

BEGIN_NCBI_SCOPE


struct PLibExclude
{
    PLibExclude(const list<string>& excluded_lib_ids)
    {
        copy(excluded_lib_ids.begin(), excluded_lib_ids.end(), 
             inserter(m_ExcludedLib, m_ExcludedLib.end()) );
    }

    bool operator() (const string& lib_id) const
    {
        return m_ExcludedLib.find(lib_id) != m_ExcludedLib.end();
    }

private:
    set<string> m_ExcludedLib;
};

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
                     const list<string>& libs_3_party,
                     const list<string>& include_dirs,
                     const list<string>& defines)
   :m_Name    (name), 
    m_ID      (id),
    m_ProjType(type),
    m_SourcesBaseDir (sources_base),
    m_Sources (sources), 
    m_Depends (depends),
    m_Requires(requires),
    m_Libs3Party (libs_3_party),
    m_IncludeDirs(include_dirs),
    m_Defines (defines)
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
    m_Libs3Party     = item.m_Libs3Party;
    m_IncludeDirs    = item.m_IncludeDirs;
    m_DatatoolSources= item.m_DatatoolSources;
    m_Defines        = item.m_Defines;
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

        const string& fc_path = p->first;
        const CSimpleMakeFileContents& fc_makein = p->second;

        string source_base_dir;
        CDirEntry::SplitPath(fc_path, &source_base_dir);

        SMakeProjectT::TMakeInInfoList list_info;
        SMakeProjectT::AnalyzeMakeIn(fc_makein, &list_info);
        ITERATE(SMakeProjectT::TMakeInInfoList, i, list_info) {

            const SMakeProjectT::SMakeInInfo& info = *i;

            //Iterate all project_name(s) from makefile.in 
            ITERATE(list<string>, n, info.m_ProjNames) {

                //project id will be defined latter
                const string proj_name = *n;
        
                string applib_mfilepath = 
                    CDirEntry::ConcatPath(source_base_dir,
                    SMakeProjectT::CreateMakeAppLibFileName(source_base_dir, 
                                                            proj_name));
                if ( applib_mfilepath.empty() )
                    continue;
            
                if (info.m_Type == SMakeProjectT::SMakeInInfo::eApp) {

                    SAsnProjectT::TAsnType asn_type = 
                        SAsnProjectT::GetAsnProjectType(applib_mfilepath,
                                                        makeapp,
                                                        makelib);

                    if (asn_type == SAsnProjectT::eMultiple) {
                        SAsnProjectT::DoCreate(source_base_dir, 
                                               proj_name, 
                                               applib_mfilepath, 
                                               makeapp, makelib, tree);
                    } else {
                        SAppProjectT::DoCreate(source_base_dir, 
                                               proj_name, 
                                               applib_mfilepath, 
                                               makeapp, tree);
                    }
                }
                else if (info.m_Type == SMakeProjectT::SMakeInInfo::eLib) {

                    SAsnProjectT::TAsnType asn_type = 
                        SAsnProjectT::GetAsnProjectType(applib_mfilepath,
                                                        makeapp,
                                                        makelib);

                    if (asn_type == SAsnProjectT::eMultiple) {
                        SAsnProjectT::DoCreate(source_base_dir, 
                                               proj_name, 
                                               applib_mfilepath, 
                                               makeapp, makelib, tree);
                    } else {
                        SLibProjectT::DoCreate(source_base_dir, 
                                               proj_name, 
                                               applib_mfilepath, 
                                               makelib, tree);
                    }
                }
                else if (info.m_Type == SMakeProjectT::SMakeInInfo::eAsn) {

                    SAsnProjectT::DoCreate(source_base_dir, 
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

//-----------------------------------------------------------------------------
CProjItem::TProjType SMakeProjectT::GetProjType(const string& base_dir,
                                                const string& projname)
{
    string fname = "Makefile." + projname;
    
    if ( CDirEntry(CDirEntry::ConcatPath
               (base_dir, fname + ".lib")).Exists() )
        return CProjItem::eLib;
    else if (CDirEntry(CDirEntry::ConcatPath
               (base_dir, fname + ".app")).Exists() )
        return CProjItem::eApp;

    LOG_POST(Error << "No .lib or .app projects for : " + projname +
                      " in directory: " + base_dir);
    return CProjItem::eNoProj;
}


bool SMakeProjectT::IsMakeInFile(const string& name)
{
    return name == "Makefile.in";
}


bool SMakeProjectT::IsMakeLibFile(const string& name)
{
    return NStr::StartsWith(name, "Makefile")  &&  
	       NStr::EndsWith(name, ".lib");
}


bool SMakeProjectT::IsMakeAppFile(const string& name)
{
    return NStr::StartsWith(name, "Makefile")  &&  
	       NStr::EndsWith(name, ".app");
}


void SMakeProjectT::DoResolveDefs(CSymResolver& resolver, 
                                  TFiles& files,
                                  const set<string>& keys)
{
    NON_CONST_ITERATE(CProjectTreeBuilder::TFiles, p, files) {
	    NON_CONST_ITERATE(CSimpleMakeFileContents::TContents, 
                          n, 
                          p->second.m_Contents) {
            
            const string& key    = n->first;
            list<string>& values = n->second;
		    if (keys.find(key) != keys.end()) {
                list<string> new_vals;
                bool modified = false;
                NON_CONST_ITERATE(list<string>, k, values) {
                    //iterate all values and try to resolve 
                    const string& val = *k;
                    if( !CSymResolver::IsDefine(val) ) {
                        new_vals.push_back(val);
                    } else {
                        list<string> resolved_def;
                        string val_define = FilterDefine(val);
	                    resolver.Resolve(val_define, &resolved_def);
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
                }
                if (modified)
                    values = new_vals; // by ref!
		    }
        }
    }
}


string SMakeProjectT::GetOneIncludeDir(const string& flag, const string& token)
{
    size_t token_pos = flag.find(token);
    if (token_pos != NPOS && 
        token_pos + token.length() < flag.length()) {
        return flag.substr(token_pos + token.length()); 
    }
    return "";
}


void SMakeProjectT::CreateIncludeDirs(const list<string>& cpp_flags,
                                      const string&       source_base_dir,
                                      list<string>*       include_dirs)
{
    include_dirs->clear();
    ITERATE(list<string>, p, cpp_flags) {
        const string& flag = *p;
        const string token("-I$(includedir)");

        // process -I$(includedir)
        string token_val;
        token_val = SMakeProjectT::GetOneIncludeDir(flag, "-I$(includedir)");
        if ( !token_val.empty() ) {
            string dir = 
                CDirEntry::ConcatPath(GetApp().GetProjectTreeInfo().m_Include,
                                      token_val);
            dir = CDirEntry::NormalizePath(dir);
            dir = CDirEntry::AddTrailingPathSeparator(dir);

            include_dirs->push_back(dir);
        }

        // process -I$(srcdir)
        token_val = SMakeProjectT::GetOneIncludeDir(flag, "-I$(srcdir)");
        if ( !token_val.empty() )  {
            string dir = 
                CDirEntry::ConcatPath(source_base_dir,
                                      token_val);
            dir = CDirEntry::NormalizePath(dir);
            dir = CDirEntry::AddTrailingPathSeparator(dir);

            include_dirs->push_back(dir);
        }
        
        // process defines like NCBI_C_INCLUDE
        if(CSymResolver::IsDefine(flag)) {
            string dir = GetApp().GetSite().ResolveDefine
                                             (CSymResolver::StripDefine(flag));
            if ( !dir.empty() && CDirEntry(dir).IsDir() ) {
                include_dirs->push_back(dir);    
            }
        }
    }
}


void SMakeProjectT::CreateDefines(const list<string>& cpp_flags,
                                  list<string>*       defines)
{
    defines->clear();

    ITERATE(list<string>, p, cpp_flags) {
        const string& flag = *p;
        if ( NStr::StartsWith(flag, "-D") ) {
            defines->push_back(flag.substr(2));
        }
    }
}


void SMakeProjectT::Create3PartyLibs(const list<string>& libs_flags, 
                                     list<string>*       libs_list)
{
    libs_list->clear();
    ITERATE(list<string>, p, libs_flags) {
        const string& flag = *p;
        if ( NStr::StartsWith(flag, "@") &&
             NStr::EndsWith(flag, "@") ) {
            libs_list->push_back(string(flag, 1, flag.length() - 2));    
        }
    }
}


void SMakeProjectT::AnalyzeMakeIn
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


string SMakeProjectT::CreateMakeAppLibFileName
                (const string&            base_dir,
                 const string&            projname)
{
    CProjItem::TProjType proj_type = 
            SMakeProjectT::GetProjType(base_dir, projname);

    string fname = "Makefile." + projname;
    fname += proj_type==CProjItem::eLib? ".lib": ".app";
    return fname;
}


void SMakeProjectT::CreateFullPathes(const string&      dir, 
                                     const list<string> files,
                                     list<string>*      full_pathes)
{
    ITERATE(list<string>, p, files) {
        string full_path = CDirEntry::ConcatPath(dir, *p);
        full_pathes->push_back(full_path);
    }
}


//-----------------------------------------------------------------------------
string SAppProjectT::DoCreate(const string& source_base_dir,
                              const string& proj_name,
                              const string& applib_mfilepath,
                              const TFiles& makeapp , 
                              CProjectItemsTree* tree)
{
    CProjectItemsTree::TFiles::const_iterator m = makeapp.find(applib_mfilepath);
    if (m == makeapp.end()) {

        LOG_POST(Info << "No Makefile.*.app for Makefile.in :"
                  + applib_mfilepath);
        return "";
    }

    CSimpleMakeFileContents::TContents::const_iterator k = 
        m->second.m_Contents.find("SRC");
    if (k == m->second.m_Contents.end()) {

        LOG_POST(Warning << "No SRC key in Makefile.*.app :"
                  + applib_mfilepath);
        return "";
    }

    //sources - relative  pathes from source_base_dir
    //We'll create relative pathes from them
    CProjSRCResolver src_resolver(applib_mfilepath, 
                                  source_base_dir, k->second);
    list<string> sources;
    src_resolver.ResolveTo(&sources);

    //depends
    list<string> depends;
    k = m->second.m_Contents.find("LIB");
    if (k != m->second.m_Contents.end())
        depends = k->second;
    //Adjust depends by information from msvc Makefile
    CMsvcProjectMakefile project_makefile
                       ((CDirEntry::ConcatPath
                          (source_base_dir, 
                           CreateMsvcProjectMakefileName(proj_name, 
                                                         CProjItem::eApp))));
    list<string> added_depends;
    project_makefile.GetAdditionalLIB(SConfigInfo(), &added_depends);

    list<string> excluded_depends;
    project_makefile.GetExcludedLIB(SConfigInfo(), &excluded_depends);

    list<string> adj_depends(depends);
    copy(added_depends.begin(), 
         added_depends.end(), back_inserter(adj_depends));
    adj_depends.unique();

    PLibExclude pred(excluded_depends);
    EraseIf(adj_depends, pred);
    ///////////////////////////////////

    //requires
    list<string> requires;
    k = m->second.m_Contents.find("REQUIRES");
    if (k != m->second.m_Contents.end())
        requires = k->second;

    //project name
    k = m->second.m_Contents.find("APP");
    if (k == m->second.m_Contents.end()  ||  
                                           k->second.empty()) {

        LOG_POST(Error << "No APP key or empty in Makefile.*.app :"
                  + applib_mfilepath);
        return "";
    }
    string proj_id = k->second.front();

    //LIBS
    list<string> libs_3_party;
    k = m->second.m_Contents.find("LIBS");
    if (k != m->second.m_Contents.end()) {
        const list<string> libs_flags = k->second;
        SMakeProjectT::Create3PartyLibs(libs_flags, &libs_3_party);
    }
    //CPPFLAGS
    list<string> include_dirs;
    list<string> defines;
    k = m->second.m_Contents.find("CPPFLAGS");
    if (k != m->second.m_Contents.end()) {
        const list<string> cpp_flags = k->second;
        SMakeProjectT::CreateIncludeDirs(cpp_flags, 
                                         source_base_dir, &include_dirs);
        SMakeProjectT::CreateDefines(cpp_flags, &defines);
    }

    CProjItem project(CProjItem::eApp, 
                      proj_name, 
                      proj_id,
                      source_base_dir,
                      sources, 
                      adj_depends,
                      requires,
                      libs_3_party,
                      include_dirs,
                      defines);

    //DATATOOL_SRC
    k = m->second.m_Contents.find("DATATOOL_SRC");
    if ( k != m->second.m_Contents.end() ) {
        //Add depends from datatoool for ASN projects
        project.m_Depends.push_back(GetApp().GetDatatoolId());
        const list<string> datatool_src_list = k->second;
        ITERATE(list<string>, i, datatool_src_list) {

            const string& src = *i;
            //Will process .asn or .dtd files
            string source_file_path = 
                CDirEntry::ConcatPath(source_base_dir, src);
            source_file_path = CDirEntry::NormalizePath(source_file_path);
            if ( CDirEntry(source_file_path + ".asn").Exists() )
                source_file_path += ".asn";
            else if ( CDirEntry(source_file_path + ".dtd").Exists() )
                source_file_path += ".dtd";

            CDataToolGeneratedSrc data_tool_src;
            CDataToolGeneratedSrc::LoadFrom(source_file_path, &data_tool_src);
            if ( !data_tool_src.IsEmpty() )
                project.m_DatatoolSources.push_back(data_tool_src);
        }
    }

    tree->m_Projects[proj_id] = project;

    return proj_id;
}


//-----------------------------------------------------------------------------
string SLibProjectT::DoCreate(const string& source_base_dir,
                              const string& proj_name,
                              const string& applib_mfilepath,
                              const TFiles& makelib , 
                              CProjectItemsTree* tree)
{
    TFiles::const_iterator m = makelib.find(applib_mfilepath);
    if (m == makelib.end()) {

        LOG_POST(Info << "No Makefile.*.lib for Makefile.in :"
                  + applib_mfilepath);
        return "";
    }

    CSimpleMakeFileContents::TContents::const_iterator k = 
        m->second.m_Contents.find("SRC");
    if (k == m->second.m_Contents.end()) {

        LOG_POST(Warning << "No SRC key in Makefile.*.lib :"
                  + applib_mfilepath);
        return "";
    }

    // sources - relative pathes from source_base_dir
    // We'll create relative pathes from them)
    CProjSRCResolver src_resolver(applib_mfilepath, 
                                  source_base_dir, k->second);
    list<string> sources;
    src_resolver.ResolveTo(&sources);

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

        LOG_POST(Error << "No LIB key or empty in Makefile.*.lib :"
                  + applib_mfilepath);
        return "";
    }
    string proj_id = k->second.front();

    //LIBS
    list<string> libs_3_party;
    k = m->second.m_Contents.find("LIBS");
    if (k != m->second.m_Contents.end()) {
        const list<string> libs_flags = k->second;
        SMakeProjectT::Create3PartyLibs(libs_flags, &libs_3_party);
    }
    //CPPFLAGS
    list<string> include_dirs;
    list<string> defines;
    k = m->second.m_Contents.find("CPPFLAGS");
    if (k != m->second.m_Contents.end()) {
        const list<string> cpp_flags = k->second;
        SMakeProjectT::CreateIncludeDirs(cpp_flags, 
                                         source_base_dir, &include_dirs);
        SMakeProjectT::CreateDefines(cpp_flags, &defines);

    }

    tree->m_Projects[proj_id] =  CProjItem(CProjItem::eLib,
                                           proj_name, 
                                           proj_id,
                                           source_base_dir,
                                           sources, 
                                           depends,
                                           requires,
                                           libs_3_party,
                                           include_dirs,
                                           defines);
    return proj_id;
}


//-----------------------------------------------------------------------------
string SAsnProjectT::DoCreate(const string& source_base_dir,
                              const string& proj_name,
                              const string& applib_mfilepath,
                              const TFiles& makeapp, 
                              const TFiles& makelib, 
                              CProjectItemsTree* tree)
{
    TAsnType asn_type = GetAsnProjectType(applib_mfilepath, makeapp, makelib);
    if (asn_type == eMultiple) {
        return SAsnProjectMultipleT::DoCreate(source_base_dir,
                                              proj_name,
                                              applib_mfilepath,
                                              makeapp, 
                                              makelib, 
                                              tree);
    }
    if(asn_type == eSingle) {
        return SAsnProjectSingleT::DoCreate(source_base_dir,
                                              proj_name,
                                              applib_mfilepath,
                                              makeapp, 
                                              makelib, 
                                              tree);
    }

    LOG_POST(Error << "Unsupported ASN project" + NStr::IntToString(asn_type));
    return "";
}


SAsnProjectT::TAsnType SAsnProjectT::GetAsnProjectType(const string& applib_mfilepath,
                                                       const TFiles& makeapp,
                                                       const TFiles& makelib)
{
    TFiles::const_iterator p = makeapp.find(applib_mfilepath);
    if ( p != makeapp.end() ) {
        const CSimpleMakeFileContents& fc = p->second;
        if (fc.m_Contents.find("ASN") != fc.m_Contents.end() )
            return eMultiple;
        else
            return eSingle;
    }

    p = makelib.find(applib_mfilepath);
    if ( p != makelib.end() ) {
        const CSimpleMakeFileContents& fc = p->second;
        if (fc.m_Contents.find("ASN") != fc.m_Contents.end() )
            return eMultiple;
        else
            return eSingle;
    }

    LOG_POST(Error << "Can not define ASN project: " + applib_mfilepath);
    return eNoAsn;
}


//-----------------------------------------------------------------------------
string SAsnProjectSingleT::DoCreate(const string& source_base_dir,
                                    const string& proj_name,
                                    const string& applib_mfilepath,
                                    const TFiles& makeapp, 
                                    const TFiles& makelib, 
                                    CProjectItemsTree* tree)
{
    CProjItem::TProjType proj_type = 
        SMakeProjectT::GetProjType(source_base_dir, proj_name);
    
    string proj_id = 
        proj_type == CProjItem::eLib? 
            SLibProjectT::DoCreate(source_base_dir, 
                               proj_name, applib_mfilepath, makelib, tree) : 
            SAppProjectT::DoCreate(source_base_dir, 
                               proj_name, applib_mfilepath, makeapp, tree);
    if ( proj_id.empty() )
        return "";
    
    TProjects::iterator p = tree->m_Projects.find(proj_id);
    if (p == tree->m_Projects.end()) {
        LOG_POST(Error << "Can not find ASN project with id : " + proj_id);
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


//-----------------------------------------------------------------------------
string SAsnProjectMultipleT::DoCreate(const string& source_base_dir,
                                      const string& proj_name,
                                      const string& applib_mfilepath,
                                      const TFiles& makeapp, 
                                      const TFiles& makelib, 
                                      CProjectItemsTree* tree)
{
    CProjItem::TProjType proj_type = 
        SMakeProjectT::GetProjType(source_base_dir, proj_name);
    

    const TFiles& makefile = proj_type == CProjItem::eLib? makelib : makeapp;
    TFiles::const_iterator m = makefile.find(applib_mfilepath);
    if (m == makefile.end()) {

        LOG_POST(Info << "No Makefile.*.lib/app  for Makefile.in :"
                  + applib_mfilepath);
        return "";
    }
    const CSimpleMakeFileContents& fc = m->second;

    // ASN
    CSimpleMakeFileContents::TContents::const_iterator k = 
        fc.m_Contents.find("ASN");
    if (k == fc.m_Contents.end()) {

        LOG_POST(Error << "No ASN key in multiple ASN  project:"
                  + applib_mfilepath);
        return "";
    }
    const list<string> asn_names = k->second;

    string parent_dir_abs = ParentDir(source_base_dir);
    list<CDataToolGeneratedSrc> datatool_sources;
    ITERATE(list<string>, p, asn_names) {
        const string& asn = *p;
        // one level up
        string asn_dir_abs = CDirEntry::ConcatPath(parent_dir_abs, asn);
        asn_dir_abs = CDirEntry::NormalizePath(asn_dir_abs);
        asn_dir_abs = CDirEntry::AddTrailingPathSeparator(asn_dir_abs);
    
        string asn_path_abs = CDirEntry::ConcatPath(asn_dir_abs, asn);
        if ( CDirEntry(asn_path_abs + ".asn").Exists() )
            asn_path_abs += ".asn";
        else if ( CDirEntry(asn_dir_abs + ".dtd").Exists() )
            asn_path_abs += ".dtd";

        CDataToolGeneratedSrc data_tool_src;
        CDataToolGeneratedSrc::LoadFrom(asn_path_abs, &data_tool_src);
        if ( !data_tool_src.IsEmpty() )
            datatool_sources.push_back(data_tool_src);

    }

    // SRC
    k = fc.m_Contents.find("SRC");
    if (k == fc.m_Contents.end()) {

        LOG_POST(Error << "No SRC key in multiple ASN  project:"
                  + applib_mfilepath);
        return "";
    }
    const list<string> src_list = k->second;
    list<string> sources;
    ITERATE(list<string>, p, src_list) {
        const string& src = *p;
        if ( !CSymResolver::IsDefine(src) )
            sources.push_back(src);
    }

    string proj_id = 
        proj_type == CProjItem::eLib? 
        SLibProjectT::DoCreate(source_base_dir, 
                               proj_name, applib_mfilepath, makelib, tree) :
        SAppProjectT::DoCreate(source_base_dir, 
                               proj_name, applib_mfilepath, makeapp, tree);
    if ( proj_id.empty() )
        return "";
    
    TProjects::iterator p = tree->m_Projects.find(proj_id);
    if (p == tree->m_Projects.end()) {
        LOG_POST(Error << "Can not find ASN project with id : " + proj_id);
        return "";
    }
    CProjItem& project = p->second;

    // Adjust created proj item
    //SRC - 
    project.m_Sources.clear();
    ITERATE(list<string>, p, src_list) {
        const string& src = *p;
        if ( !CSymResolver::IsDefine(src) )
            project.m_Sources.push_front(src);    
    }
    project.m_Sources.push_back(proj_name + "__");
    project.m_Sources.push_back(proj_name + "___");
    ITERATE(list<string>, p, asn_names) {
        const string& asn = *p;
        if (asn == proj_name)
            continue;
        string src(1, CDirEntry::GetPathSeparator());
        src += "..";
        src += CDirEntry::GetPathSeparator();
        src += asn;
        src += CDirEntry::GetPathSeparator();
        src += asn;

        project.m_Sources.push_back(src + "__");
        project.m_Sources.push_back(src + "___");
    }

    project.m_DatatoolSources = datatool_sources;

    //Add depends from datatoool for ASN projects
    project.m_Depends.push_back(GetApp().GetDatatoolId());

    return proj_id;
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
    CSymResolver resolver;
    ITERATE(list<string>, p, metadata_files) {
	    resolver += CSymResolver(CDirEntry::ConcatPath(root_src_path, *p));
	}
    ResolveDefs(resolver, subtree_makefiles);

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

    // We have to add more projects to the target tree
    // We'll consider whole tree
    // If there are some external depends
    // If we are creating whole tree it's unneccessary - we already got it
    if ( !external_depends.empty()  &&  start_node_path != root_src_path) {
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
                    LOG_POST (Error << "No project with id :" + prj_id);
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
            if ( SMakeProjectT::IsMakeInFile(name) )
	            ProcessMakeInFile(path, makefiles);
            else if ( SMakeProjectT::IsMakeLibFile(name) )
	            ProcessMakeLibFile(path, makefiles);
            else if ( SMakeProjectT::IsMakeAppFile(name) )
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
    LOG_POST(Info << "Processing MakeIn: " + file_name);

    CSimpleMakeFileContents fc(file_name);
    if ( !fc.m_Contents.empty() )
	    makefiles->m_In[file_name] = fc;
}


void CProjectTreeBuilder::ProcessMakeLibFile(const string& file_name, 
                                             SMakeFiles*   makefiles)
{
    LOG_POST(Info << "Processing MakeLib: " + file_name);

    CSimpleMakeFileContents fc(file_name);
    if ( !fc.m_Contents.empty() )
	    makefiles->m_Lib[file_name] = fc;
}


void CProjectTreeBuilder::ProcessMakeAppFile(const string& file_name, 
                                             SMakeFiles*   makefiles)
{
    LOG_POST(Info << "Processing MakeApp: " + file_name);

    CSimpleMakeFileContents fc(file_name);
    if ( !fc.m_Contents.empty() )
	    makefiles->m_App[file_name] = fc;
}


//recursive resolving
void CProjectTreeBuilder::ResolveDefs(CSymResolver& resolver, 
                                      SMakeFiles&   makefiles)
{
    {{
        //App
        set<string> keys;
        keys.insert("LIB");
        keys.insert("LIBS");
        SMakeProjectT::DoResolveDefs(resolver, makefiles.m_App, keys);
    }}

    {{
        //Lib
        set<string> keys;
        keys.insert("LIBS");
        SMakeProjectT::DoResolveDefs(resolver, makefiles.m_Lib, keys);
    }}
}


//-----------------------------------------------------------------------------
CProjectTreeFolders::CProjectTreeFolders(const CProjectItemsTree& tree)
:m_RootParent("/", NULL)
{
    ITERATE(CProjectItemsTree::TProjects, p, tree.m_Projects) {

        const CProjItem& project = p->second;
        
        TPath path;
        CreatePath(GetApp().GetProjectTreeInfo().m_Src, 
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
 * Revision 1.14  2004/02/13 16:02:24  gorelenk
 * Modified procedure of depends resolve.
 *
 * Revision 1.13  2004/02/11 16:46:29  gorelenk
 * Cnanged LOG_POST message.
 *
 * Revision 1.12  2004/02/10 21:20:44  gorelenk
 * Added support of DATATOOL_SRC tag.
 *
 * Revision 1.11  2004/02/10 18:17:05  gorelenk
 * Added support of depends fine-tuning.
 *
 * Revision 1.10  2004/02/06 23:15:00  gorelenk
 * Implemented support of ASN projects, semi-auto configure,
 * CPPFLAGS support. Second working version.
 *
 * Revision 1.9  2004/02/04 23:58:29  gorelenk
 * Added support of include dirs and 3-rd party libs.
 *
 * Revision 1.8  2004/02/03 17:12:55  gorelenk
 * Changed implementation of classes CProjItem and CProjectItemsTree.
 *
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
