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
#include <app/project_tree_builder/proj_tree_builder.hpp>
#include <app/project_tree_builder/proj_builder_app.hpp>
#include <app/project_tree_builder/proj_src_resolver.hpp>
#include <app/project_tree_builder/msvc_prj_defines.hpp>

#include <app/project_tree_builder/proj_projects.hpp>

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
CProjItem::TProjType SMakeProjectT::GetProjType(const string& base_dir,
                                                const string& projname)
{
    string fname = "Makefile." + projname;
    
    if ( CDirEntry(CDirEntry::ConcatPath
               (base_dir, fname + ".lib")).Exists() )
        return CProjKey::eLib;
    else if (CDirEntry(CDirEntry::ConcatPath
               (base_dir, fname + ".app")).Exists() )
        return CProjKey::eApp;
    else if (CDirEntry(CDirEntry::ConcatPath
               (base_dir, fname + ".msvcproj")).Exists() )
        return CProjKey::eMsvc;

    LOG_POST(Error << "No .lib or .app projects for : " + projname +
                      " in directory: " + base_dir);
    return CProjKey::eNoProj;
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


bool SMakeProjectT::IsUserProjFile(const string& name)
{
    return NStr::StartsWith(name, "Makefile")  &&  
	       NStr::EndsWith(name, ".msvcproj");
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
                            ITERATE(list<string>, l, resolved_def) {
                                const string& define = *l;
                                if ( IsConfigurableDefine(define) ) {
                                    string resolved_def_str = 
                                        GetApp().GetSite().ResolveDefine
                                                     (StripConfigurableDefine
                                                                     (define));
                                    if ( !resolved_def_str.empty() ) {
                                        list<string> resolved_defs;
                                        NStr::Split(resolved_def_str, 
                                                    LIST_SEPARATOR, 
                                                    resolved_defs);
                                        copy(resolved_defs.begin(),
                                             resolved_defs.end(),
                                             back_inserter(new_vals));
                                    } else {
                                        new_vals.push_back(define);
                                    }

                                } else {
                                    new_vals.push_back(define);
                                }
                            }
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

        // process additional include dirs for LibChoices
        if(CSymResolver::IsDefine(flag)) {
            string sflag = CSymResolver::StripDefine(flag);
            list<string> libchoices_abs_includes ;
            GetApp().GetSite().GetLibChoiceIncludes(sflag, 
                                                    &libchoices_abs_includes);

            ITERATE(list<string>, n, libchoices_abs_includes) {
                const string& dir = *n;

                if ( !dir.empty() && CDirEntry(dir).IsDir() ) {
                    include_dirs->push_back(dir);    
                }
            }
        }
    }
    include_dirs->sort();
    include_dirs->unique();
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
        if ( IsConfigurableDefine(flag) ) {
            libs_list->push_back(StripConfigurableDefine(flag));    
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

    p = makein_contents.m_Contents.find("MSVC_PROJ");
    if (p != makein_contents.m_Contents.end()) {

        info->push_back(SMakeInInfo(SMakeInInfo::eMsvc, p->second)); 
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
    
    if (proj_type==CProjKey::eLib)
        return fname + ".lib";

    if (proj_type==CProjKey::eApp)
        return fname + ".app";

    if (proj_type==CProjKey::eMsvc)
        return fname + ".msvcproj";

    return "";
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


void SMakeProjectT::ConvertLibDepends(const list<string>& depends_libs, 
                                      list<CProjKey>*     depends_ids)
{
    depends_ids->clear();
    ITERATE(list<string>, p, depends_libs)
    {
        const string& id = *p;
        depends_ids->push_back(CProjKey(CProjKey::eLib, id));
    }
}


bool SMakeProjectT::IsConfigurableDefine(const string& define)
{
    return  NStr::StartsWith(define, "@")  &&
            NStr::EndsWith  (define, "@");

}


string SMakeProjectT::StripConfigurableDefine(const string& define)
{
    return IsConfigurableDefine(define) ? 
                define.substr(1, define.length() - 2): "";
}


//-----------------------------------------------------------------------------
void SAppProjectT::CreateNcbiCToolkitLibs(const CSimpleMakeFileContents& makefile,
                                          list<string>* libs_list)
{
    CSimpleMakeFileContents::TContents::const_iterator k = 
    makefile.m_Contents.find("NCBI_C_LIBS");
    if (k == makefile.m_Contents.end()) {
        return;
    }
    const list<string>& values = k->second;

    ITERATE(list<string>, p, values) {
        const string& val = *p;
        if ( NStr::StartsWith(val, "-l") ) {
            string lib_id = val.substr(2);
            libs_list->push_back(lib_id);
        } else {
            libs_list->push_back(val);
        }
    }

    libs_list->sort();
    libs_list->unique();
}

CProjKey SAppProjectT::DoCreate(const string& source_base_dir,
                                const string& proj_name,
                                const string& applib_mfilepath,
                                const TFiles& makeapp , 
                                CProjectItemsTree* tree)
{
    CProjectItemsTree::TFiles::const_iterator m = makeapp.find(applib_mfilepath);
    if (m == makeapp.end()) {

        LOG_POST(Info << "No Makefile.*.app for Makefile.in :"
                  + applib_mfilepath);
        return CProjKey();
    }
    
    const CSimpleMakeFileContents& makefile = m->second;

    CSimpleMakeFileContents::TContents::const_iterator k = 
        makefile.m_Contents.find("SRC");
    if (k == makefile.m_Contents.end()) {

        LOG_POST(Warning << "No SRC key in Makefile.*.app :"
                  + applib_mfilepath);
        return CProjKey();
    }

    //sources - relative  pathes from source_base_dir
    //We'll create relative pathes from them
    CProjSRCResolver src_resolver(applib_mfilepath, 
                                  source_base_dir, k->second);
    list<string> sources;
    src_resolver.ResolveTo(&sources);

    //depends
    list<string> depends;
    k = makefile.m_Contents.find("LIB");
    if (k != makefile.m_Contents.end())
        depends = k->second;
    //Adjust depends by information from msvc Makefile
    CMsvcProjectMakefile project_makefile
                       ((CDirEntry::ConcatPath
                          (source_base_dir, 
                           CreateMsvcProjectMakefileName(proj_name, 
                                                         CProjKey::eApp))));
    list<string> added_depends;
    project_makefile.GetAdditionalLIB(SConfigInfo(), &added_depends);

    list<string> excluded_depends;
    project_makefile.GetExcludedLIB(SConfigInfo(), &excluded_depends);

    list<string> adj_depends(depends);
    copy(added_depends.begin(), 
         added_depends.end(), back_inserter(adj_depends));
    adj_depends.sort();
    adj_depends.unique();

    PLibExclude pred(excluded_depends);
    EraseIf(adj_depends, pred);

    list<CProjKey> depends_ids;
    SMakeProjectT::ConvertLibDepends(adj_depends, &depends_ids);
    ///////////////////////////////////

    //requires
    list<string> requires;
    k = makefile.m_Contents.find("REQUIRES");
    if (k != makefile.m_Contents.end())
        requires = k->second;

    //project name
    k = makefile.m_Contents.find("APP");
    if (k == makefile.m_Contents.end()  ||  
                                           k->second.empty()) {

        LOG_POST(Error << "No APP key or empty in Makefile.*.app :"
                  + applib_mfilepath);
        return CProjKey();
    }
    string proj_id = k->second.front();

    //LIBS
    list<string> libs_3_party;
    k = makefile.m_Contents.find("LIBS");
    if (k != makefile.m_Contents.end()) {
        const list<string> libs_flags = k->second;
        SMakeProjectT::Create3PartyLibs(libs_flags, &libs_3_party);
    }
    
    //CPPFLAGS
    list<string> include_dirs;
    list<string> defines;
    k = makefile.m_Contents.find("CPPFLAGS");
    if (k != makefile.m_Contents.end()) {
        const list<string> cpp_flags = k->second;
        SMakeProjectT::CreateIncludeDirs(cpp_flags, 
                                         source_base_dir, &include_dirs);
        SMakeProjectT::CreateDefines(cpp_flags, &defines);
    }

    //NCBI_C_LIBS - Special case for NCBI C Toolkit
    k = makefile.m_Contents.find("NCBI_C_LIBS");
    list<string> ncbi_clibs;
    if (k != makefile.m_Contents.end()) {
        libs_3_party.push_back("NCBI_C_LIBS");
        CreateNcbiCToolkitLibs(makefile, &ncbi_clibs);
    }
    
    CProjItem project(CProjKey::eApp, 
                      proj_name, 
                      proj_id,
                      source_base_dir,
                      sources, 
                      depends_ids,
                      requires,
                      libs_3_party,
                      include_dirs,
                      defines);
    //
    project.m_NcbiCLibs = ncbi_clibs;

    //DATATOOL_SRC
    k = makefile.m_Contents.find("DATATOOL_SRC");
    if ( k != makefile.m_Contents.end() ) {
        //Add depends from datatoool for ASN projects
        project.m_Depends.push_back(CProjKey(CProjKey::eApp, GetApp().GetDatatoolId()));
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
    CProjKey proj_key(CProjKey::eApp, proj_id);
    tree->m_Projects[proj_key] = project;

    return proj_key;
}


//-----------------------------------------------------------------------------
CProjKey SLibProjectT::DoCreate(const string& source_base_dir,
                                const string& proj_name,
                                const string& applib_mfilepath,
                                const TFiles& makelib , 
                                CProjectItemsTree* tree)
{
    TFiles::const_iterator m = makelib.find(applib_mfilepath);
    if (m == makelib.end()) {

        LOG_POST(Info << "No Makefile.*.lib for Makefile.in :"
                  + applib_mfilepath);
        return CProjKey();
    }

    CSimpleMakeFileContents::TContents::const_iterator k = 
        m->second.m_Contents.find("SRC");
    if (k == m->second.m_Contents.end()) {

        LOG_POST(Warning << "No SRC key in Makefile.*.lib :"
                  + applib_mfilepath);
        return CProjKey();
    }

    // sources - relative pathes from source_base_dir
    // We'll create relative pathes from them)
    CProjSRCResolver src_resolver(applib_mfilepath, 
                                  source_base_dir, k->second);
    list<string> sources;
    src_resolver.ResolveTo(&sources);

    // depends - TODO
    list<CProjKey> depends_ids;
    k = m->second.m_Contents.find("ASN_DEP");
    if (k != m->second.m_Contents.end()) {
        const list<string> depends = k->second;
        SMakeProjectT::ConvertLibDepends(depends, &depends_ids);
    }

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
        return CProjKey();
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
    CProjKey proj_key(CProjKey::eLib, proj_id);
    tree->m_Projects[proj_key] = CProjItem(CProjKey::eLib,
                                           proj_name, 
                                           proj_id,
                                           source_base_dir,
                                           sources, 
                                           depends_ids,
                                           requires,
                                           libs_3_party,
                                           include_dirs,
                                           defines);
    return proj_key;
}


//-----------------------------------------------------------------------------
CProjKey SAsnProjectT::DoCreate(const string& source_base_dir,
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
    return CProjKey();
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
CProjKey SAsnProjectSingleT::DoCreate(const string& source_base_dir,
                                      const string& proj_name,
                                      const string& applib_mfilepath,
                                      const TFiles& makeapp, 
                                      const TFiles& makelib, 
                                      CProjectItemsTree* tree)
{
    CProjItem::TProjType proj_type = 
        SMakeProjectT::GetProjType(source_base_dir, proj_name);
    
    CProjKey proj_id = 
        proj_type == CProjKey::eLib? 
            SLibProjectT::DoCreate(source_base_dir, 
                               proj_name, applib_mfilepath, makelib, tree) : 
            SAppProjectT::DoCreate(source_base_dir, 
                               proj_name, applib_mfilepath, makeapp, tree);
    if ( proj_id.Id().empty() )
        return CProjKey();
    
    TProjects::iterator p = tree->m_Projects.find(proj_id);
    if (p == tree->m_Projects.end()) {
        LOG_POST(Error << "Can not find ASN project with id : " + proj_id.Id());
        return CProjKey();
    }
    CProjItem& project = p->second;

    //Add depends from datatoool for ASN projects
    project.m_Depends.push_back(CProjKey(CProjKey::eApp,
                                         GetApp().GetDatatoolId()));

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
CProjKey SAsnProjectMultipleT::DoCreate(const string& source_base_dir,
                                        const string& proj_name,
                                        const string& applib_mfilepath,
                                        const TFiles& makeapp, 
                                        const TFiles& makelib, 
                                        CProjectItemsTree* tree)
{
    CProjItem::TProjType proj_type = 
        SMakeProjectT::GetProjType(source_base_dir, proj_name);
    

    const TFiles& makefile = proj_type == CProjKey::eLib? makelib : makeapp;
    TFiles::const_iterator m = makefile.find(applib_mfilepath);
    if (m == makefile.end()) {

        LOG_POST(Info << "No Makefile.*.lib/app  for Makefile.in :"
                  + applib_mfilepath);
        return CProjKey();
    }
    const CSimpleMakeFileContents& fc = m->second;

    // ASN
    CSimpleMakeFileContents::TContents::const_iterator k = 
        fc.m_Contents.find("ASN");
    if (k == fc.m_Contents.end()) {

        LOG_POST(Error << "No ASN key in multiple ASN  project:"
                  + applib_mfilepath);
        return CProjKey();
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
        return CProjKey();
    }
    const list<string> src_list = k->second;
    list<string> sources;
    ITERATE(list<string>, p, src_list) {
        const string& src = *p;
        if ( !CSymResolver::IsDefine(src) )
            sources.push_back(src);
    }

    CProjKey proj_id = 
        proj_type == CProjKey::eLib? 
        SLibProjectT::DoCreate(source_base_dir, 
                               proj_name, applib_mfilepath, makelib, tree) :
        SAppProjectT::DoCreate(source_base_dir, 
                               proj_name, applib_mfilepath, makeapp, tree);
    if ( proj_id.Id().empty() )
        return CProjKey();
    
    TProjects::iterator p = tree->m_Projects.find(proj_id);
    if (p == tree->m_Projects.end()) {
        LOG_POST(Error << "Can not find ASN project with id : " +proj_id.Id());
        return CProjKey();
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
    project.m_Depends.push_back(CProjKey(CProjKey::eApp, 
                                         GetApp().GetDatatoolId()));

    return proj_id;
}

//-----------------------------------------------------------------------------
CProjKey SMsvcProjectT::DoCreate(const string&      source_base_dir,
                                 const string&      proj_name,
                                 const string&      applib_mfilepath,
                                 const TFiles&      makemsvc, 
                                 CProjectItemsTree* tree)
{
    TFiles::const_iterator m = makemsvc.find(applib_mfilepath);
    if (m == makemsvc.end()) {

        LOG_POST(Info << "No User makefile.*.* for Makefile.in :"
                  + applib_mfilepath);
        return CProjKey();
    }

    // VCPROJ - will map to src
    CSimpleMakeFileContents::TContents::const_iterator k = 
        m->second.m_Contents.find("VCPROJ");
    if (k == m->second.m_Contents.end()) {

        LOG_POST(Warning << "No VCPROJ key in User Makefile.*.* :"
                  + applib_mfilepath);
        return CProjKey();
    }
    list<string> sources = k->second;

    // depends - 
    list<CProjKey> depends_ids;
    k = m->second.m_Contents.find("LIB_DEP");
    if (k != m->second.m_Contents.end()) {
        const list<string> deps = k->second;
        ITERATE(list<string>, p, deps) {
            depends_ids.push_back(CProjKey(CProjKey::eLib, *p));
        }
    }
    k = m->second.m_Contents.find("APP_DEP");
    if (k != m->second.m_Contents.end()) {
        const list<string> deps = k->second;
        ITERATE(list<string>, p, deps) {
            depends_ids.push_back(CProjKey(CProjKey::eApp, *p));
        }
    }
    k = m->second.m_Contents.find("DLL_DEP");
    if (k != m->second.m_Contents.end()) {
        const list<string> deps = k->second;
        ITERATE(list<string>, p, deps) {
            depends_ids.push_back(CProjKey(CProjKey::eDll, *p));
        }
    }
    k = m->second.m_Contents.find("MSVC_DEP");
    if (k != m->second.m_Contents.end()) {
        const list<string> deps = k->second;
        ITERATE(list<string>, p, deps) {
            depends_ids.push_back(CProjKey(CProjKey::eMsvc, *p));
        }
    }

    //requires
    list<string> requires;
    k = m->second.m_Contents.find("REQUIRES");
    if (k != m->second.m_Contents.end())
        requires = k->second;

    //project id
    k = m->second.m_Contents.find("MSVC_PROJ");
    if (k == m->second.m_Contents.end()  ||  
                                           k->second.empty()) {

        LOG_POST(Error << "No LIB key or empty in Makefile.*.lib :"
                  + applib_mfilepath);
        return CProjKey();
    }
    string proj_id = k->second.front();

    list<string> libs_3_party;
    list<string> include_dirs;
    list<string> defines;

    CProjKey proj_key(CProjKey::eMsvc, proj_id);
    tree->m_Projects[proj_key] = CProjItem(CProjKey::eMsvc,
                                           proj_name, 
                                           proj_id,
                                           source_base_dir,
                                           sources, 
                                           depends_ids,
                                           requires,
                                           libs_3_party,
                                           include_dirs,
                                           defines);
    return proj_key;
}
//-----------------------------------------------------------------------------
void 
CProjectTreeBuilder::BuildOneProjectTree(const IProjectFilter* filter,
                                         const string&         root_src_path,
                                         CProjectItemsTree*    tree)
{
    SMakeFiles subtree_makefiles;

    ProcessDir(root_src_path, 
               true,
               filter,
               &subtree_makefiles);

    // Resolve macrodefines
    list<string> metadata_files;
    GetApp().GetMetaDataFiles(&metadata_files);
    CSymResolver resolver;
    resolver += GetApp().GetSite().GetMacros();
    ITERATE(list<string>, p, metadata_files) {
	    resolver += CSymResolver(CDirEntry::ConcatPath(root_src_path, *p));
	}
    ResolveDefs(resolver, subtree_makefiles);

    // Build projects tree
    CProjectItemsTree::CreateFrom(root_src_path,
                                  subtree_makefiles.m_In, 
                                  subtree_makefiles.m_Lib, 
                                  subtree_makefiles.m_App,
                                  subtree_makefiles.m_User, tree);
}


void 
CProjectTreeBuilder::BuildProjectTree(const IProjectFilter* filter,
                                      const string&         root_src_path,
                                      CProjectItemsTree*    tree)
{
    // Bulid subtree
    CProjectItemsTree target_tree;
    BuildOneProjectTree(filter, root_src_path, &target_tree);

    // Analyze subtree depends
    list<CProjKey> external_depends;
    target_tree.GetExternalDepends(&external_depends);

    // We have to add more projects to the target tree
    if ( !external_depends.empty()  &&  !filter->PassAll() ) {

        list<CProjKey> depends_to_resolve = external_depends;

        while ( !depends_to_resolve.empty() ) {
            bool modified = false;
            ITERATE(list<CProjKey>, p, depends_to_resolve) {
                // id of project we have to resolve
                const CProjKey& prj_id = *p;
                CProjectItemsTree::TProjects::const_iterator n = 
                               GetApp().GetWholeTree().m_Projects.find(prj_id);
            
                if (n != GetApp().GetWholeTree().m_Projects.end()) {
                    //insert this project to target_tree
                    target_tree.m_Projects[prj_id] = n->second;
                    modified = true;
                } else {
                    LOG_POST (Error << "No project with id :" + prj_id.Id());
                }
            }

            if (!modified) {
                //we done - no more projects was added to target_tree
                AddDatatoolSourcesDepends(&target_tree);
                *tree = target_tree;
                return;
            } else {
                //continue resolving dependences
                target_tree.GetExternalDepends(&depends_to_resolve);
            }
        }
    }

    AddDatatoolSourcesDepends(&target_tree);
    *tree = target_tree;
}


void CProjectTreeBuilder::ProcessDir(const string&         dir_name, 
                                     bool                  is_root,
                                     const IProjectFilter* filter,
                                     SMakeFiles*           makefiles)
{
#if 1
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

        if ( (*i)->IsFile()  &&  
             !is_root        &&  
             filter->CheckProject(CDirEntry(path).GetDir()) ) {

            if ( SMakeProjectT::IsMakeInFile(name) )
	            ProcessMakeInFile(path, makefiles);
            else if ( SMakeProjectT::IsMakeLibFile(name) )
	            ProcessMakeLibFile(path, makefiles);
            else if ( SMakeProjectT::IsMakeAppFile(name) )
	            ProcessMakeAppFile(path, makefiles);
            else if ( SMakeProjectT::IsUserProjFile(name) )
	            ProcessUserProjFile(path, makefiles);
        } 
        else if ( (*i)->IsDir() ) {

            ProcessDir(path, false, filter, makefiles);
        }
    }
#endif

#if 0

    // Node - Makefile.in should present
    string node_path = 
        CDirEntry::ConcatPath(dir_name, 
                              GetApp().GetProjectTreeInfo().m_TreeNode);
    if ( !CDirEntry(node_path).Exists() )
        return;
    
    bool process_projects = !is_root && filter->CheckProject(dir_name);
    
    // Process Makefile.*.lib
    if ( process_projects ) {
        CDir dir(dir_name);
        CDir::TEntries contents = dir.GetEntries("Makefile.*.lib");
        ITERATE(CDir::TEntries, p, contents) {
            const AutoPtr<CDirEntry>& dir_entry = *p;
            if ( SMakeProjectT::IsMakeLibFile(dir_entry->GetName()) )
	            ProcessMakeLibFile(dir_entry->GetPath(), makefiles);

        }
    }
    // Process Makefile.*.app
    if ( process_projects ) {
        CDir dir(dir_name);
        CDir::TEntries contents = dir.GetEntries("Makefile.*.app");
        ITERATE(CDir::TEntries, p, contents) {
            const AutoPtr<CDirEntry>& dir_entry = *p;
            if ( SMakeProjectT::IsMakeAppFile(dir_entry->GetName()) )
	            ProcessMakeAppFile(dir_entry->GetPath(), makefiles);

        }
    }
    // Process Makefile.*.msvcproj
    if ( process_projects ) {
        CDir dir(dir_name);
        CDir::TEntries contents = dir.GetEntries("Makefile.*.msvcproj");
        ITERATE(CDir::TEntries, p, contents) {
            const AutoPtr<CDirEntry>& dir_entry = *p;
            if ( SMakeProjectT::IsUserProjFile(dir_entry->GetName()) )
	            ProcessUserProjFile(dir_entry->GetPath(), makefiles);

        }
    }

    // Process Makefile.in
    list<string> subprojects;
    if ( process_projects ) {
        ProcessMakeInFile(node_path, makefiles);
        TFiles::const_iterator p = makefiles->m_In.find(node_path);
        const CSimpleMakeFileContents& makefile = p->second;
        // 
        CSimpleMakeFileContents::TContents::const_iterator k = 
            makefile.m_Contents.find("SUB_PROJ");
        if (k != makefile.m_Contents.end()) {
            const list<string>& values = k->second;
            copy(values.begin(), 
                 values.end(), 
                 back_inserter(subprojects));
        }
        k = makefile.m_Contents.find("EXPENDABLE_SUB_PROJ");
        if (k != makefile.m_Contents.end()) {
            const list<string>& values = k->second;
            copy(values.begin(), 
                 values.end(), 
                 back_inserter(subprojects));
        }
    }
    subprojects.sort();
    subprojects.unique();

    // Convert subprojects to subdirs
    list<string> subprojects_dirs;
    if ( is_root ) {
        // for root node we'll take all subdirs
        CDir dir(dir_name);
        CDir::TEntries contents = dir.GetEntries("*");
        ITERATE(CDir::TEntries, p, contents) {
            const AutoPtr<CDirEntry>& dir_entry = *p;
            string name  = dir_entry->GetName();
            if ( name == "."  ||  name == ".."  ||  
                 name == string(1,CDir::GetPathSeparator()) ) {
                continue;
            }
            if ( dir_entry->IsDir() ) {
                subprojects_dirs.push_back(dir_entry->GetPath());
            }
        }
    } else {
        // for non-root only subprojects
        ITERATE(list<string>, p, subprojects) {
            const string& subproject = *p;
            string subproject_dir = 
                CDirEntry::ConcatPath(dir_name, subproject);
            subprojects_dirs.push_back(subproject_dir);
        }
    }

    // Process subproj ( e.t. subdirs )
    ITERATE(list<string>, p, subprojects_dirs) {
        const string& subproject_dir = *p;
        ProcessDir(subproject_dir, false, filter, makefiles);
    }

#endif
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


void CProjectTreeBuilder::ProcessUserProjFile(const string& file_name, 
                                             SMakeFiles*   makefiles)
{
    LOG_POST(Info << "Processing MakeApp: " + file_name);

    CSimpleMakeFileContents fc(file_name);
    if ( !fc.m_Contents.empty() )
	    makefiles->m_User[file_name] = fc;
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
        keys.insert("NCBI_C_LIBS");
        SMakeProjectT::DoResolveDefs(resolver, makefiles.m_App, keys);
    }}

    {{
        //Lib
        set<string> keys;
        keys.insert("LIBS");
        SMakeProjectT::DoResolveDefs(resolver, makefiles.m_Lib, keys);
    }}
}


//analyze modules
void s_CollectDatatoolIds(const CProjectItemsTree& tree,
                          map<string, CProjKey>*   datatool_ids)
{
    ITERATE(CProjectItemsTree::TProjects, p, tree.m_Projects) {
        const CProjKey&  project_id = p->first;
        const CProjItem& project    = p->second;
        ITERATE(list<CDataToolGeneratedSrc>, n, project.m_DatatoolSources) {
            const CDataToolGeneratedSrc& src = *n;
            string src_abs_path = 
                CDirEntry::ConcatPath(src.m_SourceBaseDir, src.m_SourceFile);
            string src_rel_path = 
                CDirEntry::CreateRelativePath
                                 (GetApp().GetProjectTreeInfo().m_Src, 
                                  src_abs_path);
            (*datatool_ids)[src_rel_path] = project_id;
        }
    }
}


void CProjectTreeBuilder::AddDatatoolSourcesDepends(CProjectItemsTree* tree)
{
    //datatool src rel path / project ID

    // 1. Collect all projects with datatool-generated-sources
    map<string, CProjKey> whole_datatool_ids;
    s_CollectDatatoolIds(GetApp().GetWholeTree(), &whole_datatool_ids);



    // 2. Extent tree to accomodate more ASN projects if necessary
    bool tree_extented = false;
    map<string, CProjKey> datatool_ids;

    do {
        
        tree_extented = false;
        s_CollectDatatoolIds(*tree, &datatool_ids);

        NON_CONST_ITERATE(CProjectItemsTree::TProjects, p, tree->m_Projects) {
            const CProjKey&  project_id = p->first;
            CProjItem& project          = p->second;
            ITERATE(list<CDataToolGeneratedSrc>, n, project.m_DatatoolSources) {
                const CDataToolGeneratedSrc& src = *n;
                ITERATE(list<string>, i, src.m_ImportModules) {
                    const string& module = *i;
                    map<string, CProjKey>::const_iterator j = 
                        datatool_ids.find(module);
                    if (j == datatool_ids.end()) {
                        j = whole_datatool_ids.find(module);
                        if (j != whole_datatool_ids.end()) {
                            const CProjKey& depends_id = j->second;
                            tree->m_Projects[depends_id] = 
                                GetApp().GetWholeTree().m_Projects.find(depends_id)->second;
                            tree_extented = true;
                        }
                    }
                }
            }
        }
    } while( tree_extented );


    // 3. Finally - generate depends
    NON_CONST_ITERATE(CProjectItemsTree::TProjects, p, tree->m_Projects) {
        const CProjKey&  project_id = p->first;
        CProjItem& project          = p->second;
        ITERATE(list<CDataToolGeneratedSrc>, n, project.m_DatatoolSources) {
            const CDataToolGeneratedSrc& src = *n;
            ITERATE(list<string>, i, src.m_ImportModules) {
                const string& module = *i;
                map<string, CProjKey>::const_iterator j = 
                    datatool_ids.find(module);
                if (j != datatool_ids.end()) {
                    const CProjKey& depends_id = j->second;
                    if (depends_id != project_id) {
                        project.m_Depends.push_back(depends_id);
                        project.m_Depends.sort();
                        project.m_Depends.unique();
                    }
                }
            }
        }
    }

}


END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.14  2004/07/20 13:38:40  gouriano
 * Added conditional macro definition
 *
 * Revision 1.13  2004/06/16 16:29:11  gorelenk
 * Changed directories traversing proc.
 *
 * Revision 1.12  2004/06/16 14:26:05  gorelenk
 * Re-designed CProjectTreeBuilder::ProcessDir .
 *
 * Revision 1.11  2004/06/15 14:14:31  gorelenk
 * Changed CProjectTreeBuilder::ProcessDir - changed procedure of
 * subdirs iteration .
 *
 * Revision 1.10  2004/06/14 18:57:59  gorelenk
 * Changed CProjectTreeBuilder::ProcessDir
 * - added support of EXPENDABLE_SUB_PROJ .
 *
 * Revision 1.9  2004/06/14 14:18:21  gorelenk
 * Changed CProjectTreeBuilder::ProcessDir - added SUB_PROJ processing.
 *
 * Revision 1.8  2004/05/21 21:41:41  gorelenk
 * Added PCH ncbi_pch.hpp
 *
 * Revision 1.7  2004/05/13 14:55:35  gorelenk
 * Changed SMakeProjectT::CreateIncludeDirs .
 *
 * Revision 1.6  2004/05/10 19:50:42  gorelenk
 * Implemented SMsvcProjectT .
 *
 * Revision 1.5  2004/04/06 17:15:47  gorelenk
 * Implemented member-functions IsConfigurableDefine and
 * StripConfigurableDefine of struct SMakeProjectT.
 * Changed implementations of SMakeProjectT::DoResolveDefs
 * SMakeProjectT::Create3PartyLibs and SAppProjectT::CreateNcbiCToolkitLibs.
 *
 * Revision 1.4  2004/03/23 14:41:50  gorelenk
 * Changed implementations of CProjectTreeBuilder::AddDatatoolSourcesDepends
 * and CProjectTreeBuilder::BuildProjectTree.
 *
 * Revision 1.3  2004/03/16 23:53:14  gorelenk
 * Changed implementations of:
 * SAppProjectT::DoCreate and
 * SAppProjectT::CreateNcbiCToolkitLibs .
 *
 * Revision 1.2  2004/03/04 23:31:10  gorelenk
 * Added call to AddDatatoolSourcesDepends in implementation of
 * CProjectTreeBuilder::BuildProjectTree.
 *
 * Revision 1.1  2004/03/02 16:23:57  gorelenk
 * Initial revision.
 *
 * ===========================================================================
 */
