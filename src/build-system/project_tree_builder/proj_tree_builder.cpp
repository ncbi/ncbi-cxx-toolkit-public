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
#include "proj_tree_builder.hpp"
#include "proj_builder_app.hpp"
#include "proj_src_resolver.hpp"
#include "msvc_prj_defines.hpp"

#include "proj_projects.hpp"
#include <algorithm>

#include "ptb_err_codes.hpp"

BEGIN_NCBI_SCOPE

const char* s_check_separator = " ____ ";
static map<string,size_t> s_buildOrder_byname;
static size_t s_BuildOrder=0;

void s_WriteBuildOrder(const string& dir_name, const string& mkname)
{
    string name( CDirEntry::ConcatPath(dir_name,mkname));
    s_buildOrder_byname[name] = ++s_BuildOrder;
}

bool CProjectTreeBuilder::VerifyBuildOrder(
    const CProjItem& item, list<string> dependencies, const CProjectItemsTree&  tree)
{
    bool res = true;
    if (item.m_MkName.empty()) {
// error?
        return res;
    }
    ITERATE(list<string>, d, dependencies) {
        CProjectItemsTree::TProjects::const_iterator n =
            tree.m_Projects.find(CreateProjKey(*d));
            if (n != tree.m_Projects.end()) {
                const CProjItem& dep_item = n->second;
                if (dep_item.m_MkName.empty()) {
                    //error?
                } else if (s_buildOrder_byname[dep_item.m_MkName] >
                           s_buildOrder_byname[item.m_MkName]) {
                    PTB_WARNING_EX(item.m_MkName,
                        ePTB_InvalidMakefile,
                        "should be built after: " << dep_item.m_MkName);
                    res = false;
                }
            }

    }
    return res;
}


struct PLibExclude
{
    PLibExclude(const string& prj_name, const list<string>& excluded_lib_ids)
        : m_Prj(prj_name)
    {
        copy(excluded_lib_ids.begin(), excluded_lib_ids.end(), 
             inserter(m_ExcludedLib, m_ExcludedLib.end()) );
    }

    bool operator() (const string& lib_id) const
    {
        if (m_ExcludedLib.find(lib_id) != m_ExcludedLib.end()) {
            LOG_POST(Warning << "Project " << m_Prj << ": library excluded by request: " << lib_id);
            return true;
        }
        return false;
    }

private:
    string m_Prj;
    set<string> m_ExcludedLib;
};


//-----------------------------------------------------------------------------
CProjItem::TProjType SMakeProjectT::GetProjType(const string& base_dir,
                                                const string& projname,
                                                SMakeInInfo::TMakeinType type)
{
    string fname = "Makefile." + projname;

    string fname_base = CDirEntry::ConcatPath(base_dir, fname);
    string fname_app = CDirEntry::ConcatPath(base_dir, fname + ".app");
    string fname_lib = CDirEntry::ConcatPath(base_dir, fname + ".lib");
    string fname_dll = CDirEntry::ConcatPath(base_dir, fname + ".dll");
    string fname_msvc= CDirEntry::ConcatPath(base_dir, fname);
    string fname_msvc2(fname_msvc);
    if (CMsvc7RegSettings::GetMsvcPlatform() != CMsvc7RegSettings::eUnix) {
        fname_msvc += ".msvcproj";
    } else {
        fname_msvc2 += ".in";
    }

    switch (type) {
    case SMakeInInfo::eApp:
        if ( CDirEntry(fname_app).Exists()) {
            return CProjKey::eApp;
        }
        break;
    case SMakeInInfo::eLib:
        if ( CDirEntry(fname_lib).Exists()) {
            return CProjKey::eLib;
        }
        break;
    case SMakeInInfo::eDll:
        if ( CDirEntry(fname_dll).Exists()) {
            return CProjKey::eDll;
        }
        break;
    case SMakeInInfo::eMsvc:
        if ( CDirEntry(fname_msvc).Exists() || CDirEntry(fname_msvc2).Exists()) {
            return CProjKey::eMsvc;
        }
        break;

    default:
        break;
    }

    if ( CDirEntry(fname_lib).Exists() )
        return CProjKey::eLib;
    else if (CDirEntry(fname_dll).Exists() )
        return CProjKey::eDll;
    else if (CDirEntry(fname_app).Exists() )
        return CProjKey::eApp;
    else if (CDirEntry(fname_msvc).Exists() || CDirEntry(fname_msvc2).Exists() )
        return CProjKey::eMsvc;


    switch (type) {
    case SMakeInInfo::eApp:
        PTB_WARNING_EX(fname_app, ePTB_MissingMakefile,
                       "Makefile not found");
        break;

    case SMakeInInfo::eLib:
        PTB_WARNING_EX(fname_lib, ePTB_MissingMakefile,
                       "Makefile not found");
        break;

    case SMakeInInfo::eDll:
        PTB_WARNING_EX(fname_dll, ePTB_MissingMakefile,
                       "Makefile not found");
        break;

    case SMakeInInfo::eMsvc:
        PTB_WARNING_EX(fname_msvc, ePTB_MissingMakefile,
                       "Makefile not found");
        break;

    default:
        PTB_WARNING_EX(fname_base, ePTB_MissingMakefile,
                       "Makefile not found");
        break;
    }
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

bool SMakeProjectT::IsMakeDllFile(const string& name)
{
    return NStr::StartsWith(name, "Makefile")  &&  
	       NStr::EndsWith(name, ".dll");
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
    const CMsvcSite& site = GetApp().GetSite();
    set<string> defs_unresolved;
    map<string,string> defs_resolved;
    NON_CONST_ITERATE(CProjectTreeBuilder::TFiles, p, files) {

        CMsvcProjectMakefile msvc_prj(p->first + "." + GetApp().GetRegSettings().m_MakefilesExt);
        bool msvc_empty = msvc_prj.IsEmpty();

	    NON_CONST_ITERATE(CSimpleMakeFileContents::TContents, 
                          n, 
                          p->second.m_Contents) {
            
            const string& key    = n->first;
            list<string>& values = n->second;
            bool cppflags = key == "CPPFLAGS";

//		    if (keys.find(key) != keys.end())
		    {
                bool modified = false;
                list<string> new_vals;
                list<string>  redef_values;
                modified = msvc_prj.Redefine(values,redef_values);
                NON_CONST_ITERATE(list<string>, k, redef_values) {
//                NON_CONST_ITERATE(list<string>, k, values) {
                    //iterate all values and try to resolve 
                    const string& val = *k;
                    if (cppflags && site.IsCppflagDescribed(val)) {
                        if (msvc_empty) {
                            new_vals.push_back(val);
                        } else {
                            msvc_prj.Append(new_vals,val);
                        }
                    } else if( !CSymResolver::HasDefine(val) ) {
                        if (msvc_empty) {
                            new_vals.push_back(val);
                        } else {
                            msvc_prj.Append(new_vals,val);
                        }
                    } else {
                        list<string> resolved_def;
                        string val_define = FilterDefine(val);
	                    resolver.Resolve(val, &resolved_def, p->second);
	                    if ( resolved_def.empty() ) {
                            defs_unresolved.insert(val);
		                    new_vals.push_back(val_define); //not resolved - keep old val
                        } else {
                            defs_resolved[val] = NStr::Join( resolved_def, " ");
                            //was resolved
                            ITERATE(list<string>, l, resolved_def) {
                                const string& define = *l;
                                if ( IsConfigurableDefine(define) ) {
                                    string stripped = StripConfigurableDefine(define);
                                    string resolved_def_str = site.GetDefinesEntry(stripped);
                                    if ( !resolved_def_str.empty() ) {
                                        defs_resolved[define] = resolved_def_str;
                                        list<string> resolved_defs;
                                        NStr::Split(resolved_def_str, 
                                                    LIST_SEPARATOR, 
                                                    resolved_defs);
                                        if (msvc_empty) {
                                            copy(resolved_defs.begin(),
                                                resolved_defs.end(),
                                                back_inserter(new_vals));
                                        } else {
                                            msvc_prj.Append(new_vals,resolved_defs);
                                        }
                                    } else {
// configurable definitions could be described in terms of components
                                        list<string> components;
                                        site.GetComponents(stripped, &components);
                                        if (!components.empty()) {
                                            defs_resolved[define] = "Component= " + NStr::Join( components, ", ");
                                        } else {
                                            defs_unresolved.insert(define);
                                        }
                                        if (msvc_empty) {
                                            new_vals.push_back(define);
                                        } else {
                                            msvc_prj.Append(new_vals,define);
                                        }
                                    }

                                } else if (HasConfigurableDefine(define)) {
                                    string def(define);
                                    while (HasConfigurableDefine(def)) {
                                        string raw = ExtractConfigurableDefine(def);
                                        string stripped = StripConfigurableDefine(raw);
                                        string resolved_def_str = site.GetDefinesEntry(stripped);
                                        if (resolved_def_str == " ") {
                                            resolved_def_str.erase();
                                        }
                                        NStr::ReplaceInPlace(def, raw, resolved_def_str);
                                    }
                                    if (msvc_empty) {
                                        new_vals.push_back( def);
                                    } else {
                                        msvc_prj.Append(new_vals,def);
                                    }
                                } else {
                                    if (msvc_empty) {
                                        new_vals.push_back(define);
                                    } else {
                                        msvc_prj.Append(new_vals,define);
                                    }
                                }
                            }
		                    modified = true;
                        }
                    }
                }
                if (modified) {
                    msvc_prj.Redefine(new_vals,redef_values);
                    values = redef_values; // by ref!
                }
		    }
        }
    }

    if (!defs_resolved.empty()) {
        string s;
        for (map<string,string>::const_iterator r = defs_resolved.begin();
            r != defs_resolved.end(); ++r) {
            s += ' ';
            s += r->first;
            s += " = ";
            s += r->second;
            s += ";";
        }
        PTB_INFO("Resolved macro definitions: " << s);
    }
    if (!defs_unresolved.empty()) {
        string s;
        for (set<string>::const_iterator u = defs_unresolved.begin();
            u != defs_unresolved.end(); ++u) {
            s += ' ';
            s += *u;
        }
        PTB_WARNING_EX(kEmptyStr, ePTB_MacroUndefined,
                       "Unresolved macro definitions:" << s);
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
//        string token("-I$(includedir)");

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
        token_val = SMakeProjectT::GetOneIncludeDir(flag, "-I$(incdir)");
        if ( !token_val.empty() ) {
            string dir = CDirEntry::ConcatPath(GetApp().m_IncDir,token_val);
            dir = CDirEntry::NormalizePath(dir);
            dir = CDirEntry::AddTrailingPathSeparator(dir);

            include_dirs->push_back(dir);
        }

        // process -I$(srcdir)
        token_val = SMakeProjectT::GetOneIncludeDir(flag, "-I$(srcdir)");
        if ( !token_val.empty() || flag == "-I$(srcdir)" )  {
            string dir = 
                CDirEntry::ConcatPath(source_base_dir,
                                      token_val);
            dir = CDirEntry::NormalizePath(dir);
            dir = CDirEntry::AddTrailingPathSeparator(dir);

            include_dirs->push_back(dir);
        }

        // process -Ipath
        token_val = SMakeProjectT::GetOneIncludeDir(flag, "-I");
        if ( !token_val.empty() && token_val[0] != '$' && token_val[0] != '@' && token_val[0] != ':' )  {
            string dir = CDirEntry::NormalizePath(token_val);
            dir = CDirEntry::AddTrailingPathSeparator(dir);
            include_dirs->push_back(dir);
        }
        
        // process defines like NCBI_C_INCLUDE
        if(CSymResolver::IsDefine(flag)) {
            string dir_all;
            GetApp().GetSite().ResolveDefine(CSymResolver::StripDefine(flag), dir_all);
            if ( !dir_all.empty() ) {
                list<string> dir_list;
                NStr::Split(dir_all, LIST_SEPARATOR, dir_list);
                ITERATE(list<string>, dir_item, dir_list) {
                    const string& dir = *dir_item;
                    if ( CDirEntry(dir).IsDir() ) {
                        include_dirs->push_back(dir);    
                    } else if (CDirEntry::IsAbsolutePath(dir)) {
                        LOG_POST(Warning << "In " << source_base_dir << ": "
                            << flag << " = " << dir << ": "
                            << dir << " not found");
                        include_dirs->push_back(dir);    
                    } else {
                        string d = 
                            CDirEntry::ConcatPath(GetApp().GetProjectTreeInfo().m_Include, dir);
                        d = CDirEntry::NormalizePath(d);
                        d = CDirEntry::AddTrailingPathSeparator(d);
                        if ( CDirEntry(d).IsDir() ) {
                            include_dirs->push_back(d);    
                        }
/*
                        else {
                            LOG_POST(Warning << flag << " = " << dir << ": "
                                        << dir << " not found");
                        }
*/
                    }
                }
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
                if ( !dir.empty() ) {
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


void SMakeProjectT::Create3PartyLibs(
    const list<string>& libs_flags, const list<string>& expected_flags,
    list<string>*   libs_list,  const string* mkname)
{
    bool liborder_found = mkname != NULL && GetApp().m_LibraryOrder.find(*mkname) != GetApp().m_LibraryOrder.end();
    set<string> done;
    list<string> unkflags;
    list<CProjKey> libs3;
    ITERATE(list<string>, p, libs_flags) {
        string flag = *p;
        if (flag == "#") {
            break;
        } else if ( IsConfigurableDefine(flag) ) {
            libs_list->push_back(StripConfigurableDefine(flag));    
            done.insert(flag);
        } else if (NStr::StartsWith(flag, "-l")) {
            CSymResolver::StripSuffix(flag);
            if (liborder_found && find(
                    GetApp().m_LibraryOrder[*mkname].begin(),
                    GetApp().m_LibraryOrder[*mkname].end(), flag.substr(2)) !=
                        GetApp().m_LibraryOrder[*mkname].end()) {
                continue;
            }
            libs3.push_back( CProjKey(CProjKey::eLib, flag.substr(2)));
// user cannot be trusted
//            GetApp().m_3PartyLibs.insert(flag.substr(2));
            done.insert(flag.substr(2));
        } else if ( NStr::CompareCase(flag, "-framework") == 0 ) {
            if (p != libs_flags.end()) {
                GetApp().m_3PartyLibs.insert(*(++p));
                GetApp().m_Frameworks.insert(*p);
                done.insert(flag);
            }
        } else {
            unkflags.push_back(flag);
        }
    }
    bool added = false;
    if (GetApp().m_AddMissingDep) {
        ITERATE(list<string>, p, expected_flags) {
            const string& flag = *p;
            if (NStr::StartsWith(flag, "-l")) {
                if (done.find(flag.substr(2)) == done.end()) {
                    libs3.push_back( CProjKey(CProjKey::eLib, flag.substr(2)));
                    done.insert(flag.substr(2));
                    added = true;
                }
            } else if (IsConfigurableDefine(flag)) {
                if (done.find(flag) == done.end()) {
                    libs3.push_back( CProjKey(CProjKey::eLib, flag));
                    done.insert(flag);
                    added = true;
                }
            } else {
                if (find(unkflags.begin(), unkflags.end(), flag) == unkflags.end()) {
                    unkflags.push_back(flag);
                    added = true;
                }
            }
        }
    }
    if (mkname != NULL && !GetApp().IsScanningWholeTree() && !libs3.empty()) {
        list<string> liborder;
        if (added) {
            liborder.push_back("");
        }
        VerifyLibDepends(libs3, *mkname, liborder);
        if (!liborder.empty()) {
            GetApp().m_3PartyLibraryOrder[*mkname] = unkflags;
            ITERATE( list<string>, s, liborder) {
                if (!s->empty()) {
                    GetApp().m_3PartyLibraryOrder[*mkname].push_back(*s);
                    if (libs_list && find(libs_list->begin(), libs_list->end(), *s) == libs_list->end()) {
                        if ( IsConfigurableDefine(*s) ) {
                            libs_list->push_back(StripConfigurableDefine(*s));
                        } else {
                            libs_list->push_back(*s);
                        }
                    }
                }
            }
        }
    }
}


void SMakeProjectT::AnalyzeMakeIn
    (const CSimpleMakeFileContents& makein_contents,
     TMakeInInfoList*               info)
{
    info->clear();
    CSimpleMakeFileContents::TContents::const_iterator p;

    p = makein_contents.m_Contents.find("LIB_PROJ");
    if (p != makein_contents.m_Contents.end()) {

        info->push_back(SMakeInInfo(SMakeInInfo::eLib, p->second,
            makein_contents.GetMakeType())); 
    }
    p = makein_contents.m_Contents.find("EXPENDABLE_LIB_PROJ");
    if (p != makein_contents.m_Contents.end()) {

        info->push_back(SMakeInInfo(SMakeInInfo::eLib, p->second,
            max(makein_contents.GetMakeType(),eMakeType_Expendable))); 
    }
    p = makein_contents.m_Contents.find("POTENTIAL_LIB_PROJ");
    if (p != makein_contents.m_Contents.end()) {

        info->push_back(SMakeInInfo(SMakeInInfo::eLib, p->second,
            max(makein_contents.GetMakeType(),eMakeType_Potential))); 
    }

    p = makein_contents.m_Contents.find("DLL_PROJ");
    if (p != makein_contents.m_Contents.end()) {

        info->push_back(SMakeInInfo(SMakeInInfo::eDll, p->second,
            makein_contents.GetMakeType())); 
    }
    p = makein_contents.m_Contents.find("EXPENDABLE_DLL_PROJ");
    if (p != makein_contents.m_Contents.end()) {

        info->push_back(SMakeInInfo(SMakeInInfo::eDll, p->second,
            max(makein_contents.GetMakeType(),eMakeType_Expendable))); 
    }
    p = makein_contents.m_Contents.find("POTENTIAL_DLL_PROJ");
    if (p != makein_contents.m_Contents.end()) {

        info->push_back(SMakeInInfo(SMakeInInfo::eDll, p->second,
            max(makein_contents.GetMakeType(),eMakeType_Potential))); 
    }

    p = makein_contents.m_Contents.find("APP_PROJ");
    if (p != makein_contents.m_Contents.end()) {

        info->push_back(SMakeInInfo(SMakeInInfo::eApp, p->second,
            makein_contents.GetMakeType())); 
    }
    p = makein_contents.m_Contents.find("EXPENDABLE_APP_PROJ");
    if (p != makein_contents.m_Contents.end()) {

        info->push_back(SMakeInInfo(SMakeInInfo::eApp, p->second,
            max(makein_contents.GetMakeType(),eMakeType_Expendable))); 
    }
    p = makein_contents.m_Contents.find("POTENTIAL_APP_PROJ");
    if (p != makein_contents.m_Contents.end()) {

        info->push_back(SMakeInInfo(SMakeInInfo::eApp, p->second,
            max(makein_contents.GetMakeType(),eMakeType_Potential))); 
    }

    p = makein_contents.m_Contents.find("ASN_PROJ");
    if (p != makein_contents.m_Contents.end()) {

        info->push_back(SMakeInInfo(SMakeInInfo::eASN, p->second,
            makein_contents.GetMakeType())); 
    }
    p = makein_contents.m_Contents.find("DTD_PROJ");
    if (p != makein_contents.m_Contents.end()) {

        info->push_back(SMakeInInfo(SMakeInInfo::eDTD, p->second,
            makein_contents.GetMakeType())); 
    }
    p = makein_contents.m_Contents.find("XSD_PROJ");
    if (p != makein_contents.m_Contents.end()) {

        info->push_back(SMakeInInfo(SMakeInInfo::eXSD, p->second,
            makein_contents.GetMakeType())); 
    }
    p = makein_contents.m_Contents.find("WSDL_PROJ");
    if (p != makein_contents.m_Contents.end()) {

        info->push_back(SMakeInInfo(SMakeInInfo::eWSDL, p->second,
            makein_contents.GetMakeType())); 
    }

    if (CMsvc7RegSettings::GetMsvcPlatform() == CMsvc7RegSettings::eUnix) {
        p = makein_contents.m_Contents.find("UNIX_PROJ");
    } else {
        p = makein_contents.m_Contents.find("MSVC_PROJ");
    }
    if (p != makein_contents.m_Contents.end()) {

        info->push_back(SMakeInInfo(SMakeInInfo::eMsvc, p->second,
            makein_contents.GetMakeType())); 
    }

    if (CMsvc7RegSettings::GetMsvcPlatform() == CMsvc7RegSettings::eUnix) {
        p = makein_contents.m_Contents.find("EXPENDABLE_UNIX_PROJ");
        if (p != makein_contents.m_Contents.end()) {

            info->push_back(SMakeInInfo(SMakeInInfo::eMsvc, p->second,
                max(makein_contents.GetMakeType(),eMakeType_Expendable))); 
        }
    }
}


string SMakeProjectT::CreateMakeAppLibFileName
                (const string&            base_dir,
                 const string&            projname,
                 SMakeInInfo::TMakeinType type)
{
    CProjItem::TProjType proj_type = 
            SMakeProjectT::GetProjType(base_dir, projname, type);

    string fname = "Makefile." + projname;
    switch (proj_type) {
    case CProjKey::eLib:  fname += ".lib"; break;
    case CProjKey::eDll:  fname += ".dll"; break;
    case CProjKey::eApp:  fname += ".app"; break;
    case CProjKey::eMsvc:
        if (CMsvc7RegSettings::GetMsvcPlatform() == CMsvc7RegSettings::eUnix) {
            if (!CDirEntry( CDirEntry::ConcatPath(base_dir,fname)).Exists()) {
                fname += ".in";
            }
        } else {
            fname += ".msvcproj";
        }
        break;
    default: break;
    }
    return CDirEntry::ConcatPath(base_dir, fname);
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

static
void s_CollectAllLeaves(const map<string, set<string> >& source_dep,
                        const map<string, set<string> >& source_flags,
                        const string& branch,
                        set<string>& all_dep,
                        set<string>& all_flags)
{
    if (all_dep.find(branch) != all_dep.end()) {
        return;
    }
    all_dep.insert(branch);
    if (source_flags.find(branch) != source_flags.end()) {
        const set<string>& flags(source_flags.find(branch)->second);
        ITERATE(set<string>, f, flags) {
            all_flags.insert(*f);
        }
    }
    if (source_flags.find(branch) != source_flags.end()) {
        const set<string>& branches(source_dep.find(branch)->second);
        ITERATE(set<string>, b, branches) {
            s_CollectAllLeaves(source_dep, source_flags, *b, all_dep, all_flags);
        }
    }
}

void  SMakeProjectT::VerifyLibDepends(
     list<CProjKey>&  depends_ids_arg, const string& mkname, list<string>& liborder,
     const set<string>* libs_3party, list<string>* expected_3party)
{
    if (depends_ids_arg.empty()) {
        return;
    }
    if (GetApp().m_GraphDepPrecedes.empty()) {
        return;
    }
    CProjBulderApp& app(GetApp());
    list<string> warnings;
    list<string> original;
    list<string> duplicates;
    list<string> missing;
    map<string, string> missing_suffix;
    set<string> alldepends;
    set<string> allflags;
    list<CProjKey>  depends_ids( depends_ids_arg);

    for(list<CProjKey>::const_iterator p = depends_ids.begin();
        p != depends_ids.end(); ++p) {
        for(list<CProjKey>::const_iterator i = p;
            ++i != depends_ids.end();) {
            if (*i == *p) {
                duplicates.push_back(i->Id());
                break;
            }
        }
        original.push_back(p->Id());
        s_CollectAllLeaves( app.m_GraphDepPrecedes, app.m_GraphDepFlags, p->Id(), alldepends, allflags);
    }
    if (expected_3party != nullptr) {
        ITERATE( set<string>, s, allflags) {
            expected_3party->push_back(*s);
        }
    }
    ITERATE( set<string>, s, alldepends) {
        string id(*s);
        string s_suffix;
        CSymResolver::StripSuffix(id, &s_suffix);
        list<CProjKey>::const_iterator p = depends_ids.begin();
        for(; p != depends_ids.end(); ++p) {
            if (p->Id() == id) {
                break;
            }
        }
        if (p == depends_ids.end()) {
            for(p = depends_ids.begin(); p != depends_ids.end(); ++p) {
                if (app.m_GraphDepIncludes[p->Id()].find(id) != app.m_GraphDepIncludes[p->Id()].end()) {
                    break;
                }
            }
            if (p == depends_ids.end()) {
                if (libs_3party == nullptr || 
                    libs_3party->find(id) == libs_3party->end()) {
                    if (!SMakeProjectT::IsConfigurableDefine(id)) {
                        missing.push_back(id);
                        missing_suffix[id] = s_suffix;
                    }
                } else if (expected_3party != nullptr) {
                    if (SMakeProjectT::IsConfigurableDefine(id)) {
                        expected_3party->push_back( id);
                    } else {
                        expected_3party->push_back( "-l" + id);
                    }
                }
            }
        }
    }
    if (!missing.empty()) {
        warnings.push_back("missing dependencies: " + NStr::Join(missing,","));
        if (app.m_AddMissingDep && libs_3party != nullptr) {
            ITERATE( list<string>, m,  missing) {
                depends_ids.push_back(CProjKey(CProjKey::eLib, *m, missing_suffix[*m]));
            }
        }
    }
    if (!duplicates.empty()) {
        warnings.push_back("duplicate dependencies: " + NStr::Join(duplicates,","));
    }
#if 1
    set<string> projlibs;
    bool fix = (!liborder.empty() && liborder.begin()->empty()) ||
                 !duplicates.empty() || depends_ids_arg.size() != depends_ids.size();
    if (fix) {
        liborder.clear();
    }
    if (!app.m_GraphDepPrecedes.empty()) {
        set<string> libsofar;
        for(list<CProjKey>::const_iterator p = depends_ids.begin();
            p != depends_ids.end(); ++p) {
            list<string> wrong;
            bool obsolete = false;
            ITERATE(set<string>, s, libsofar) {
                if (app.m_GraphDepPrecedes.find(p->Id()) != app.m_GraphDepPrecedes.end()) {
                    if (app.m_GraphDepPrecedes[p->Id()].find(*s) != app.m_GraphDepPrecedes[p->Id()].end()) {
                        wrong.push_back(*s);
                    }
                }
                if (app.m_GraphDepIncludes[p->Id()].find(*s) != app.m_GraphDepIncludes[p->Id()].end()) {
                    fix=true;
                    obsolete = true;
                    projlibs.erase(*s);
                    projlibs.insert(p->Id());
                    warnings.push_back("obsolete library: " + *s + " already included into " + p->Id());
                }
                if (app.m_GraphDepIncludes[*s].find(p->Id()) != app.m_GraphDepIncludes[*s].end()) {
                    fix=true;
                    obsolete = true;
                    projlibs.erase(p->Id());
                    projlibs.insert(*s);
                    warnings.push_back("obsolete library: " + p->Id() + " already included into " + *s);
                }
            }
            if (!wrong.empty()) {
                fix=true;
                warnings.push_back("wrong library order: " + p->Id() + " should precede " + NStr::Join(wrong,","));
            }
            libsofar.insert(p->Id());
            if (!obsolete) {
                projlibs.insert(p->Id());
            }
        }
// all toolkit libs should be known
// while 3rd party may be unknown
        if (expected_3party != nullptr) {
            list<string> unknown;
            ITERATE (set<string>, p,  projlibs) {
                if (app.m_GraphDepPrecedes.find(*p) == app.m_GraphDepPrecedes.end()) {
                    unknown.push_back(*p);
                }
            }
            if (!unknown.empty()) {
                fix = false;
                warnings.push_back("unknown toolkit libraries: "  + NStr::Join(unknown,","));
            }
        }
    }
    if (fix) {
        if (app.m_GraphDepRank.empty()) {
            ITERATE (set<string>, p,  projlibs) {
                liborder.push_back(*p);
            }
        } else {
            vector< list<string> > recommend;
            for (set<string>::const_iterator p= projlibs.begin(); p != projlibs.end(); ++p) {
                size_t rank = app.m_GraphDepRank[*p];
                while (recommend.size() < rank+1) {
                    list<string> t;
                    recommend.push_back(t);
                }
                recommend[rank].push_back(*p);
            }
            list<string> advice;
            for (size_t a= recommend.size(); a!= 0; --a) {
                advice.insert(advice.end(), recommend[a-1].begin(), recommend[a-1].end());
            }
            warnings.push_back("present     library order: " + NStr::Join(original,","));
            warnings.push_back("recommended library order: " + NStr::Join(advice,","));

            list<string> advice_full;
            ITERATE( list<string>, a, advice) {
                for(list<CProjKey>::const_iterator p = depends_ids.begin();
                    p != depends_ids.end(); ++p) {
                    if (*a == p->Id()) {
                        advice_full.push_back( p->FullId());
                        break;
                    }
                }
            }
            liborder = advice_full;
        }
    }
    if (!warnings.empty() && expected_3party != nullptr) {
        if (libs_3party == nullptr) {
            warnings.push_front("====== Library order warnings (3rd party libs) ======");
        } else {
            warnings.push_front("====== Library order warnings (toolkit libs) ======");
        }
        PTB_WARNING_EX(mkname,ePTB_InvalidMakefile,
            NStr::Join(warnings,"\n"));
    }
#else
/*
    this compares dependency rank,
    BUT rank does not mean that one library really needs another one;
    maybe, they are just in different dependency branches.
    That is, while, in general, it is good to place higher rank libs first,
    in reality, it is not necessarily a problem.

    ALSO, this is very slow, most likely because of large number of warning generated
*/
    if (!app.m_GraphDepRank.empty()) {
        set<string> libsofar;
        for(list<CProjKey>::const_iterator p = depends_ids.begin();
            p != depends_ids.end(); ++p) {
            list<string> wrong;
            ITERATE(set<string>, s, libsofar) {
                if (app.m_GraphDepRank[*s] < app.m_GraphDepRank[p->Id()]) {
                    wrong.push_back(*s);
                }
                if (app.m_GraphDepIncludes[p->Id()].find(*s) != app.m_GraphDepIncludes[p->Id()].end()) {
                    PTB_WARNING_EX(mkname,ePTB_InvalidMakefile,
                        "obsolete library: " << *s << " already included into " << p->Id());
                }
                if (app.m_GraphDepIncludes[*s].find(p->Id()) != app.m_GraphDepIncludes[*s].end()) {
                    PTB_WARNING_EX(mkname,ePTB_InvalidMakefile,
                        "obsolete library: " << p->Id() << " already included into " << *s);
                }
            }
            if (!wrong.empty()) {
                PTB_WARNING_EX(mkname,ePTB_InvalidMakefile,
                    "wrong library order: " << p->Id() << " should precede " << NStr::Join(wrong,","));
            }
            libsofar.insert(p->Id());
        }
    }
#endif
    if (depends_ids_arg.size() != depends_ids.size()) {
        depends_ids_arg = depends_ids;
    }
}

void SMakeProjectT::ConvertLibDepends(const list<string>& depends,
                                      list<CProjKey>*     depends_ids,
                                      const string* mkname,
                                      list<string>* expected_3party)
{
    list<string> depends_libs;
    SMakeProjectT::ConvertLibDependsMacro(depends, depends_libs);

    const CMsvcSite& site = GetApp().GetSite();
    ITERATE(list<string>, p, depends_libs) {
        string id = *p;
        string suffix;
        CSymResolver::StripSuffix(id, &suffix);
        if(CSymResolver::IsDefine(id)) {
            string def;
            GetApp().GetSite().ResolveDefine(CSymResolver::StripDefine(id), def);
            list<string> resolved_def;
            NStr::Split(def, LIST_SEPARATOR, resolved_def);
            ITERATE(list<string>, r, resolved_def) {
                id = *r;
                if (!site.IsLibWithChoice(id) ||
                     site.GetChoiceForLib(id) == CMsvcSite::eLib) {
                    depends_ids->push_back(CProjKey(CProjKey::eLib, id, suffix));
                }
            }
        } else if (SMakeProjectT::IsConfigurableDefine(id)) {
        } else if (id.empty()) {
        } else {
            if (!site.IsLibWithChoice(id) ||
                 site.GetChoiceForLib(id) == CMsvcSite::eLib) {
                depends_ids->push_back(CProjKey(CProjKey::eLib, id, suffix));
            }
        }
    }

    if (mkname != NULL && !GetApp().IsScanningWholeTree()) {
        VerifyLibDepends(*depends_ids, *mkname, GetApp().m_LibraryOrder[ *mkname],
            &GetApp().m_3PartyLibs, expected_3party);
    }

    depends_ids->sort();
    depends_ids->unique();
}

void SMakeProjectT::ConvertLibDependsMacro(const list<string>& depends, 
                                           list<string>& depends_libs)
{
    const CMsvcSite& site = GetApp().GetSite();
    ITERATE(list<string>, p, depends) {
        const string& id = *p;
        if (id[0] == '#') {
            break;
        }
        string lib = site.ProcessMacros(id,false);
        if (!lib.empty()) {
            depends_libs.push_back(lib);
        } else {
            if (CSymResolver::IsDefine(id) &&
                site.GetMacros().GetValue(CSymResolver::StripDefine(id),lib)) {
                list<string> res;
                NStr::Split(lib, LIST_SEPARATOR, res);
                ITERATE( list<string>, r, res) {
                    if (NStr::StartsWith(*r, "-l")) {
                        depends_libs.push_back(r->substr(2));
                    } else {
                        depends_libs.push_back(*r);
                    }
                }
            } else {
                depends_libs.push_back(id);
            }
        }
    }
}


bool SMakeProjectT::IsConfigurableDefine(const string& define)
{
    if (NStr::StartsWith(define, "@")) {
        string::size_type end = define.find("@",1);
        return end != string::npos && ( ++end == define.size() || define[end] == '\0');
    }
    return false;

}


string SMakeProjectT::StripConfigurableDefine(const string& define)
{
    return IsConfigurableDefine(define) ? 
                define.substr(1, define.length() - 2): "";
}

bool   SMakeProjectT::HasConfigurableDefine(const string& define)
{
    return define.find("@") != string::npos;
}

string SMakeProjectT::ExtractConfigurableDefine (const string& define)
{
    string::size_type start, end;
    start = define.find("@");
    end = define.find("@",start+1);
    if (end == string::npos) {
        LOG_POST(Warning << "Possibly incorrect MACRO definition in: " + define);
        return define;
    }
    return define.substr(start,end-start+1);
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
                                CProjectItemsTree* tree,
                                EMakeFileType maketype)
{
    CProjectItemsTree::TFiles::const_iterator m = makeapp.find(applib_mfilepath);
    if (m == makeapp.end()) {
        /// FIXME: items may not be really missing here; they may just be
        /// excluded based on user preference
        /**
        PTB_WARNING_EX(applib_mfilepath, ePTB_MissingMakefile,
                       "Makefile not found");
                       **/
        return CProjKey();
    }
    
    const CSimpleMakeFileContents& makefile = m->second;
    string full_makefile_name = CDirEntry(applib_mfilepath).GetName();
    string full_makefile_path = applib_mfilepath;

    CSimpleMakeFileContents::TContents::const_iterator k;
    //project id
    k = makefile.m_Contents.find("APP");
    if (k == makefile.m_Contents.end()  ||  k->second.empty()) {
        if (GetApp().IsScanningWholeTree()) {
            PTB_WARNING_EX(full_makefile_path, ePTB_InvalidMakefile,
                        "APP is not specified: " << full_makefile_name);
        } else {
            PTB_ERROR_EX(full_makefile_path, ePTB_InvalidMakefile,
                        "APP is not specified: " << full_makefile_name);
        }
        return CProjKey();
    }
    string proj_id = k->second.front();
    {{
        CProjKey proj_key(CProjKey::eApp, proj_id);
        CProjectItemsTree::TProjects::const_iterator z = tree->m_Projects.find(proj_key);
        if (z != tree->m_Projects.end()) {
            if (z->second.m_MakeType < eMakeType_Excluded) {
                PTB_WARNING_EX(full_makefile_path, ePTB_ConfigurationError,
                            "Application " << proj_id << " already defined at "
                            << tree->m_Projects[proj_key].m_SourcesBaseDir);
                if (maketype == eMakeType_Excluded || GetApp().IsScanningWholeTree()) {
                    return CProjKey();
                } else {
                    GetApp().RegisterSuspiciousProject(proj_key);
                }
            } else {
                tree->m_Projects.erase(proj_key);
            }
        }
    }}

    k = makefile.m_Contents.find("SRC");
    if (k == makefile.m_Contents.end()) {
        if (GetApp().IsScanningWholeTree()) {
            PTB_WARNING_EX(full_makefile_path, ePTB_InvalidMakefile,
                        "SRC is not specified: " << full_makefile_name);
        } else {
            PTB_ERROR_EX(full_makefile_path, ePTB_InvalidMakefile,
                        "SRC is not specified: " << full_makefile_name);
        }
        return CProjKey();
    }

    //sources - relative  paths from source_base_dir
    //We'll create relative pathes from them
    CProjSRCResolver src_resolver(applib_mfilepath, 
                                  source_base_dir, k->second);
    list<string> sources;
    src_resolver.ResolveTo(&sources);

    if (CMsvc7RegSettings::GetMsvcPlatform() >= CMsvc7RegSettings::eUnix) {
        k = makefile.m_Contents.find("UNIX_SRC");
        if (k != makefile.m_Contents.end()) {
            CProjSRCResolver unix_src_resolver(applib_mfilepath, 
                                        source_base_dir, k->second);
            list<string> unix_sources;
            unix_src_resolver.ResolveTo(&unix_sources);
            copy(unix_sources.begin(), unix_sources.end(), back_inserter(sources));
        }
    }

    //depends
    list<string> depends;
    k = makefile.m_Contents.find("LIB");
    if (GetApp().GetBuildType().GetType() == CBuildType::eStatic) {
        CSimpleMakeFileContents::TContents::const_iterator tmp_k =
            makefile.m_Contents.find("STATIC_LIB");
        if (tmp_k != makefile.m_Contents.end()) {
            k = tmp_k;
        }
    }
    if (k != makefile.m_Contents.end()) {
//        depends = k->second;
        ITERATE(list<string>, i, k->second) {
#if 0
            depends.push_back(
                NStr::Replace(NStr::Replace(*i, "-dll", kEmptyStr),
                              "-static", kEmptyStr));
#else
            depends.push_back( *i);
#endif
        }
    }
    //Adjust depends by information from msvc Makefile
    CMsvcProjectMakefile project_makefile( CDirEntry::ConcatPath(
        source_base_dir, CreateMsvcProjectMakefileName(proj_name, CProjKey::eApp)));

    list<string> added_depends;
    project_makefile.GetAdditionalLIB(SConfigInfo(), &added_depends);

    list<string> excluded_depends;
    project_makefile.GetExcludedLIB(SConfigInfo(), &excluded_depends);

    list<string> adj_depends(depends);
    copy(added_depends.begin(), 
         added_depends.end(), back_inserter(adj_depends));

    PLibExclude pred(proj_name, excluded_depends);
    EraseIf(adj_depends, pred);

    list<string> expected_3party;
    list<CProjKey> depends_ids;
    SMakeProjectT::ConvertLibDepends(adj_depends, &depends_ids,
                                     &applib_mfilepath, &expected_3party);

    list<CProjKey> unconditional_depends_ids;
    k = m->second.m_Contents.find("USR_DEP");
    if (k != m->second.m_Contents.end()) {
        const list<string> depends = k->second;
        SMakeProjectT::ConvertLibDepends(depends, &unconditional_depends_ids);
        copy(unconditional_depends_ids.begin(),
             unconditional_depends_ids.end(), back_inserter(depends_ids));
    }
    k = m->second.m_Contents.find("MSVC_DEP");
    if (k != m->second.m_Contents.end()) {
        const list<string> deps = k->second;
        ITERATE(list<string>, p, deps) {
            depends_ids.push_back(CProjKey(CProjKey::eMsvc, *p));
        }
    }
    ///////////////////////////////////

    //requires
    list<string> requires;
    list<string> req_lst;
    if (makefile.CollectValues("REQUIRES", req_lst,
        CSimpleMakeFileContents::eSortUnique)) {
        project_makefile.Redefine(req_lst,requires);
    }
    
    //LIBS
    list<string> libs_3_party;
    k = makefile.m_Contents.find("LIBS");
    if (GetApp().GetBuildType().GetType() == CBuildType::eStatic) {
        CSimpleMakeFileContents::TContents::const_iterator tmp_k =
            makefile.m_Contents.find("STATIC_LIBS");
        if (tmp_k != makefile.m_Contents.end()) {
            k = tmp_k;
        }
    }
    if (k != makefile.m_Contents.end() || !expected_3party.empty()) {
        list<string> libs_flags;
        if (k != makefile.m_Contents.end()) {
            libs_flags = k->second;
        }
        SMakeProjectT::Create3PartyLibs(libs_flags, expected_3party, &libs_3_party, &applib_mfilepath);
    }
    
    //CPPFLAGS
    list<string> include_dirs;
    list<string> defines;
    k = makefile.m_Contents.find("CPPFLAGS");
    if (k != makefile.m_Contents.end()) {
        const list<string>& cpp_flags = k->second;
        SMakeProjectT::CreateIncludeDirs(cpp_flags, 
                                         source_base_dir, &include_dirs);
        SMakeProjectT::CreateDefines(cpp_flags, &defines);
    }
    bool style_objcpp = false;
    k = makefile.m_Contents.find("CXXFLAGS");
    if (k != makefile.m_Contents.end()) {
        const list<string>& cxx_flags = k->second;
        style_objcpp = find(cxx_flags.begin(), cxx_flags.end(), "objective-c++") != cxx_flags.end();
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
                      defines,
                      maketype,
        IdentifySlnGUID(source_base_dir, CProjKey(CProjKey::eApp, proj_id)));
    //
    project.m_NcbiCLibs = ncbi_clibs;
    project.m_StyleObjcpp = style_objcpp;
    project.m_MkName = applib_mfilepath;
    project.m_DataSource = CSimpleMakeFileContents(applib_mfilepath);;

    //DATATOOL_SRC
    list<CDataToolGeneratedSrc> datatool_sources;
    k = makefile.m_Contents.find("DATATOOL_SRC");
    if ( k != makefile.m_Contents.end() ) {
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
            else if ( CDirEntry(source_file_path + ".xsd").Exists() )
                source_file_path += ".xsd";

            CDataToolGeneratedSrc data_tool_src;
            CDataToolGeneratedSrc::LoadFrom(source_file_path, &data_tool_src);
            if ( !data_tool_src.IsEmpty() )
                datatool_sources.push_back(data_tool_src);
        }
    }
    if ( !datatool_sources.empty() ) {
        project.m_DatatoolSources = datatool_sources;
        if (GetApp().m_Dtdep && !GetApp().GetDatatoolId().empty()) {   
              project.m_Depends.push_back(CProjKey(CProjKey::eApp, GetApp().GetDatatoolId())); 
        }
    }

// assemble check info
    string check_info;
    string check_dir = CDirEntry::CreateRelativePath(
        GetApp().GetProjectTreeInfo().m_Src, source_base_dir);
    NStr::ReplaceInPlace(check_dir,"\\","/");
    if (NStr::EndsWith(check_dir,'/')) {
        check_dir.erase(check_dir.size()-1,1);
    }
    string check_testname(proj_name);
    string check_appname(proj_id);

    string check_copy;
    k = makefile.m_Contents.find("CHECK_COPY");
    if ( k != makefile.m_Contents.end() && !k->second.empty() ) {
        check_copy = NStr::Join(k->second, " ");
    }
    string check_timeout("200");
    k = makefile.m_Contents.find("CHECK_TIMEOUT");
    if ( k != makefile.m_Contents.end() && !k->second.empty() ) {
        check_timeout = NStr::Join(k->second, " ");
    }
    bool check_requires_ok = true;
    string check_requires;
    k = makefile.m_Contents.find("CHECK_REQUIRES");
    if ( k != makefile.m_Contents.end() && !k->second.empty() ) {
        ITERATE(list<string>, p, k->second) {
            if ( !GetApp().GetSite().IsProvided(*p) ) {
                check_requires_ok = false;
                break;
            }
        }
        check_requires = NStr::Join(k->second, " ");
    }
    if (check_requires_ok) {
        k = makefile.m_Contents.find("REQUIRES");
        if ( k != makefile.m_Contents.end() && !k->second.empty() ) {
            if (!check_requires.empty()) {
                check_requires += " ";
            }
            check_requires += NStr::Join(k->second, " ");
        }
    }
    
    string check_authors;
    list<string> lst_authors;
    if (makefile.CollectValues("WATCHERS", lst_authors,
        CSimpleMakeFileContents::eSortUnique)) {
        check_authors = NStr::Join(lst_authors, " ");
        project.m_Watchers = check_authors;
    } else {
        k = makefile.m_Contents.find("CHECK_AUTHORS");
        if ( k != makefile.m_Contents.end() && !k->second.empty() ) {
            check_authors = NStr::Join(k->second, " ");
        }
    }

    k = makefile.m_Contents.find("CHECK_CMD");
    if ( check_requires_ok && k != makefile.m_Contents.end() ) {
        const list<string> check_cmd_list = k->second;
        string test_name("/CHECK_NAME=");
        ITERATE(list<string>, i, check_cmd_list) {
            string check_cmd(*i), check_name;
            string::size_type  n = check_cmd.find(test_name);
            if (n != string::npos) {
                check_name = check_cmd.substr(n+test_name.size());
                check_cmd = check_cmd.substr(0,n);
            }
            NStr::TruncateSpacesInPlace(check_cmd);
            CNcbiOstrstream check;
            check << check_dir
                << s_check_separator << check_testname
                << s_check_separator << check_appname
                << s_check_separator << check_cmd
                << s_check_separator << check_name
                << s_check_separator << check_copy
                << s_check_separator << check_timeout
                << s_check_separator << check_requires
                << s_check_separator << check_authors;
            project.m_CheckInfo.push_back( CNcbiOstrstreamToString(check) );
        }
    }

    project.m_ProjTags.push_back("exe");
    if (find(requires.begin(), requires.end(), "internal") == requires.end() ) {
        project.m_ProjTags.push_back("public");
    } else {
        project.m_ProjTags.push_back("internal");
    }
    makefile.CollectValues("PROJ_TAG", project.m_ProjTags,
        CSimpleMakeFileContents::eMergePlusMinus);

    list<string> pch_lst;
    if (makefile.CollectValues("USE_PCH", pch_lst,
        CSimpleMakeFileContents::eFirstNonempty)) {
        project.m_Pch = pch_lst.front();
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
                                CProjectItemsTree* tree,
                                EMakeFileType maketype)
{
    TFiles::const_iterator m = makelib.find(applib_mfilepath);
    if (m == makelib.end()) {
        /// FIXME: items may not be really missing here; they may just be
        /// excluded based on user preference
        /**
        PTB_WARNING_EX(applib_mfilepath, ePTB_MissingMakefile,
                       "Makefile not found");
                       **/
        return CProjKey();
    }

//    const CSimpleMakeFileContents& makefile = m->second;
    string full_makefile_name = CDirEntry(applib_mfilepath).GetName();
    string full_makefile_path = applib_mfilepath;

    CSimpleMakeFileContents::TContents::const_iterator k;
    //project name
    k = m->second.m_Contents.find("LIB");
    if (GetApp().GetBuildType().GetType() == CBuildType::eStatic) {
        CSimpleMakeFileContents::TContents::const_iterator tmp_k =
            m->second.m_Contents.find("STATIC_LIB");
        if (tmp_k != m->second.m_Contents.end()) {
            k = tmp_k;
        }
    }
    if (k == m->second.m_Contents.end()  ||  
                                           k->second.empty()) {
        if (GetApp().IsScanningWholeTree()) {
            PTB_WARNING_EX(full_makefile_path, ePTB_InvalidMakefile,
                        "LIB is not specified: " << full_makefile_name);
        } else {
            PTB_ERROR_EX(full_makefile_path, ePTB_InvalidMakefile,
                        "LIB is not specified: " << full_makefile_name);
        }
        return CProjKey();
    }
    string proj_id = k->second.front();
    {{
        CProjKey proj_key(CProjKey::eLib, proj_id);
        CProjectItemsTree::TProjects::const_iterator z = tree->m_Projects.find(proj_key);
        if (z != tree->m_Projects.end()) {
            if (z->second.m_MakeType < eMakeType_Excluded) {
                PTB_WARNING_EX(full_makefile_path, ePTB_ConfigurationError,
                            "Library " << proj_id << " already defined at "
                            << tree->m_Projects[proj_key].m_SourcesBaseDir);
                if (maketype == eMakeType_Excluded || GetApp().IsScanningWholeTree()) {
                    return CProjKey();
                } else {
                    GetApp().RegisterSuspiciousProject(proj_key);
                }
            } else {
                tree->m_Projects.erase(proj_key);
            }
            
        }
    }}

    k = m->second.m_Contents.find("SRC");
    if (k == m->second.m_Contents.end()) {
        if (GetApp().IsScanningWholeTree()) {
            PTB_WARNING_EX(full_makefile_path, ePTB_InvalidMakefile,
                        "SRC is not specified: " << full_makefile_name);
        } else {
            PTB_ERROR_EX(full_makefile_path, ePTB_InvalidMakefile,
                        "SRC is not specified: " << full_makefile_name);
        }
        return CProjKey();
    }

    // sources - relative pathes from source_base_dir
    // We'll create relative pathes from them)
    CProjSRCResolver src_resolver(applib_mfilepath, 
                                  source_base_dir, k->second);
    list<string> sources;
    src_resolver.ResolveTo(&sources);

    if (CMsvc7RegSettings::GetMsvcPlatform() >= CMsvc7RegSettings::eUnix) {
        k = m->second.m_Contents.find("UNIX_SRC");
        if (k != m->second.m_Contents.end()) {
            CProjSRCResolver unix_src_resolver(applib_mfilepath, 
                                        source_base_dir, k->second);
            list<string> unix_sources;
            unix_src_resolver.ResolveTo(&unix_sources);
            copy(unix_sources.begin(), unix_sources.end(), back_inserter(sources));
        }
    }

    // depends
    list<CProjKey> depends_ids;
    list<CProjKey> unconditional_depends_ids;
    k = m->second.m_Contents.find("ASN_DEP");
    if (k != m->second.m_Contents.end()) {
        const list<string> depends = k->second;
        SMakeProjectT::ConvertLibDepends(depends, &unconditional_depends_ids);
        copy(unconditional_depends_ids.begin(),
             unconditional_depends_ids.end(), back_inserter(depends_ids));
    }
    k = m->second.m_Contents.find("USR_DEP");
    if (k != m->second.m_Contents.end()) {
        const list<string> depends = k->second;
        SMakeProjectT::ConvertLibDepends(depends, &unconditional_depends_ids);
        copy(unconditional_depends_ids.begin(),
             unconditional_depends_ids.end(), back_inserter(depends_ids));
    }
    k = m->second.m_Contents.find("MSVC_DEP");
    if (k != m->second.m_Contents.end()) {
        const list<string> deps = k->second;
        ITERATE(list<string>, p, deps) {
            depends_ids.push_back(CProjKey(CProjKey::eMsvc, *p));
        }
    }

    string dll_host;
    string lib_or_dll;
    k = m->second.m_Contents.find("LIB_OR_DLL");
    if (k != m->second.m_Contents.end()) {
        lib_or_dll = k->second.front();
    }
    if (NStr::CompareNocase(lib_or_dll,"dll") == 0 ||
        NStr::CompareNocase(lib_or_dll,"both") == 0) {
		dll_host = proj_id;
    }
    bool need_dll = (!dll_host.empty() &&
                      GetApp().GetBuildType().GetType() == CBuildType::eDll);

    //requires
    list<string> requires;
    list<string> req_lst;
    if (m->second.CollectValues("REQUIRES",req_lst,
        CSimpleMakeFileContents::eSortUnique)) {
        CMsvcProjectMakefile project_makefile( CDirEntry::ConcatPath(
            source_base_dir, CreateMsvcProjectMakefileName(proj_name, CProjKey::eLib)));
        project_makefile.Redefine(req_lst,requires);        
    }

    //CPPFLAGS
    list<string> include_dirs;
    list<string> defines;
    k = m->second.m_Contents.find("CPPFLAGS");
    if (k != m->second.m_Contents.end()) {
        const list<string>& cpp_flags = k->second;
        SMakeProjectT::CreateIncludeDirs(cpp_flags, 
                                         source_base_dir, &include_dirs);
        SMakeProjectT::CreateDefines(cpp_flags, &defines);

    }
    bool style_objcpp = false;
    k = m->second.m_Contents.find("CXXFLAGS");
    if (k != m->second.m_Contents.end()) {
        const list<string>& cxx_flags = k->second;
        style_objcpp = find(cxx_flags.begin(), cxx_flags.end(), "objective-c++") != cxx_flags.end();
    }
    
    bool isbundle = false;
    k = m->second.m_Contents.find("DLL_TYPE");
    if (k != m->second.m_Contents.end() && k->second.front() == "plugin") {
        isbundle = true;
    }
//    if (!lib_or_dll.empty() ||
//        CMsvc7RegSettings::GetMsvcPlatform() >= CMsvc7RegSettings::eUnix) {
//        if (GetApp().GetBuildType().GetType() == CBuildType::eDll) {
            list<string> dll_depends;
            k = m->second.m_Contents.find("DLL_LIB");
            if (GetApp().m_AllDllBuild) {
                CSimpleMakeFileContents::TContents::const_iterator tmp_k =
                    m->second.m_Contents.find("DLL_DLIB");
                if (tmp_k != m->second.m_Contents.end()) {
                    k = tmp_k;
                }
            }
            if (k != m->second.m_Contents.end()) {
                ITERATE(list<string>, i, k->second) {
#if 0
                    dll_depends.push_back(
                        NStr::Replace(NStr::Replace(*i, "-dll", kEmptyStr),
                              "-static", kEmptyStr));
#else
                    dll_depends.push_back(*i);
#endif
                }
            }
            list<string> expected_3party;
            list<CProjKey> dll_depends_ids;
            SMakeProjectT::ConvertLibDepends(dll_depends, &dll_depends_ids, 
                need_dll ? &applib_mfilepath : NULL,
                need_dll ? &expected_3party  : NULL);
            copy(dll_depends_ids.begin(), 
                    dll_depends_ids.end(), 
                    back_inserter(depends_ids));
//        }
//    }

    //LIBS
    list<string> libs_3_party;
    k = m->second.m_Contents.find("LIBS");
    if (GetApp().GetBuildType().GetType() == CBuildType::eStatic) {
        CSimpleMakeFileContents::TContents::const_iterator tmp_k =
            m->second.m_Contents.find("STATIC_LIBS");
        if (tmp_k != m->second.m_Contents.end()) {
            k = tmp_k;
        }
    }
    if (k != m->second.m_Contents.end() || !expected_3party.empty()) {
        list<string> libs_flags;
        if (k != m->second.m_Contents.end()) {
            libs_flags = k->second;
        }
        SMakeProjectT::Create3PartyLibs(libs_flags, expected_3party, &libs_3_party,
            need_dll ? &applib_mfilepath : NULL);
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
                                           defines,
                                           maketype,
        IdentifySlnGUID(source_base_dir, proj_key));
    (tree->m_Projects[proj_key]).m_StyleObjcpp = style_objcpp;
    (tree->m_Projects[proj_key]).m_MkName = applib_mfilepath;
    (tree->m_Projects[proj_key]).m_DataSource = CSimpleMakeFileContents(applib_mfilepath);

    k = m->second.m_Contents.find("HEADER_EXPORT");
    if (k != m->second.m_Contents.end()) {
        (tree->m_Projects[proj_key]).m_ExportHeaders = k->second;
    }
    k = m->second.m_Contents.find("PACKAGE_EXPORT");
    if (k != m->second.m_Contents.end()) {
        (tree->m_Projects[proj_key]).m_ExportHeadersDest = k->second.front();
    }
    list<string> lst_watchers;
    if (m->second.CollectValues("WATCHERS", lst_watchers,
        CSimpleMakeFileContents::eSortUnique)) {
        tree->m_Projects[proj_key].m_Watchers = NStr::Join(lst_watchers, " ");
    }

    tree->m_Projects[proj_key].m_ProjTags.push_back("lib");
    if (find(requires.begin(), requires.end(), "internal") == requires.end() ) {
        tree->m_Projects[proj_key].m_ProjTags.push_back("public");
    } else {
        tree->m_Projects[proj_key].m_ProjTags.push_back("internal");
    }
    m->second.CollectValues("PROJ_TAG", tree->m_Projects[proj_key].m_ProjTags,
        CSimpleMakeFileContents::eMergePlusMinus);

    list<string> pch_lst;
    if (m->second.CollectValues("USE_PCH", pch_lst,
        CSimpleMakeFileContents::eFirstNonempty)) {
        tree->m_Projects[proj_key].m_Pch = pch_lst.front();
    }

    if (!dll_host.empty() && GetApp().GetBuildType().GetType() == CBuildType::eDll) {
        tree->m_Projects[proj_key].m_DllHost = dll_host;
        CProjKey proj_dll(CProjKey::eDll, dll_host);
        if (tree->m_Projects.find(proj_dll) == tree->m_Projects.end()) {
        CProjItem item_dll = tree->m_Projects[proj_dll];
        item_dll.m_ProjType = CProjKey::eDll;
#if 0
        item_dll.m_Name = dll_host;
        item_dll.m_ID = dll_host;
#else
        item_dll.m_Name = proj_name;
        item_dll.m_ID = proj_id;
#endif
        item_dll.m_SourcesBaseDir = source_base_dir;
        item_dll.m_MakeType = maketype;
        item_dll.m_HostedLibs.push_back(proj_id);
        item_dll.m_GUID  = IdentifySlnGUID(source_base_dir, proj_dll);
        item_dll.m_IsBundle = isbundle;
        item_dll.m_External = true;
        item_dll.m_StyleObjcpp = style_objcpp;
        item_dll.m_MkName = applib_mfilepath;
        item_dll.m_ProjTags = tree->m_Projects[proj_key].m_ProjTags;
        item_dll.m_ProjTags.push_back("dll");
        tree->m_Projects[proj_dll] = item_dll;
        }
    }
    ITERATE(list<CProjKey>, u,  unconditional_depends_ids) {
        (tree->m_Projects[proj_key]).m_UnconditionalDepends.insert( *u);
    }
    return proj_key;
}

CProjKey SLibProjectT::DoCreateDataSpec(
            const string& source_base_dir,
            const string& proj_name,
            const string& proj_id,
            CProjectItemsTree* tree,
            EMakeFileType maketype)
{
    string spec_proj_name = proj_name;
    string spec_proj_id   = proj_id;

    list<string>   s_empty;
    list<CProjKey> d_empty;
    CProjKey::TProjType type = CProjKey::eDataSpec;
    CProjKey proj_key(type, spec_proj_id);
    tree->m_Projects[proj_key] = CProjItem(type,
                                           spec_proj_name, 
                                           spec_proj_id,
                                           source_base_dir,
                                           s_empty, 
                                           d_empty,
                                           s_empty,
                                           s_empty,
                                           s_empty,
                                           s_empty,
                                           maketype,
        IdentifySlnGUID(source_base_dir, proj_key));
    return proj_key;
}

CProjItem CreateUtilityProjectItem( const string& prj_dir, const string& name)
{
    string spec_proj_name = name;
    string spec_proj_id   = NStr::Replace(name, "-", "_");

    list<string>   s_empty;
    list<CProjKey> d_empty;
    CProjKey::TProjType type = CProjKey::eUtility;
    CProjKey proj_key(type, spec_proj_id);
    return CProjItem(type,
                spec_proj_name, 
                spec_proj_id,
                prj_dir,
                s_empty, 
                d_empty,
                s_empty,
                s_empty,
                s_empty,
                s_empty,
                eMakeType_Undefined,
                IdentifySlnGUID(prj_dir, proj_key));
}

//-----------------------------------------------------------------------------
CProjKey SDllProjectT::DoCreate(const string& source_base_dir,
                                const string& proj_name,
                                const string& applib_mfilepath,
                                const TFiles& makedll , 
                                CProjectItemsTree* tree,
                                EMakeFileType maketype)
{
    TFiles::const_iterator m = makedll.find(applib_mfilepath);
    if (m == makedll.end()) {

        LOG_POST(Info << "Dll Makefile not found: " << applib_mfilepath);
        return CProjKey();
    }
    CSimpleMakeFileContents::TContents::const_iterator k;

    //DLL
    k = m->second.m_Contents.find("DLL");
    if (k == m->second.m_Contents.end()  ||  
                                           k->second.empty()) {
        LOG_POST(Info << "No DLL specified in Makefile." << proj_name
                      << ".dll  at " << applib_mfilepath);
        return CProjKey();
    }
    string proj_id = k->second.front();
    {{
        CProjKey proj_key(CProjKey::eDll, proj_id);
        CProjectItemsTree::TProjects::const_iterator z = tree->m_Projects.find(proj_key);
        if (z != tree->m_Projects.end()) {
            if (z->second.m_MakeType < eMakeType_Excluded) {
                const CProjItem& item = tree->m_Projects[proj_key];
                if (item.m_HostedLibs.size() != 1 || item.m_HostedLibs.front() != proj_id) {
                    string full_makefile_path = applib_mfilepath;
                    PTB_WARNING_EX(full_makefile_path, ePTB_ConfigurationError,
                                "DLL " << proj_id << " already defined at "
                                << tree->m_Projects[proj_key].m_SourcesBaseDir);
                    if (maketype == eMakeType_Excluded || GetApp().IsScanningWholeTree()) {
                        return CProjKey();
                    } else {
                        GetApp().RegisterSuspiciousProject(proj_key);
                    }
                }
            } else {
                tree->m_Projects.erase(proj_key);
            }
        }
    }}

    //CPPFLAGS
    list<string> include_dirs;
    list<string> defines;
    k = m->second.m_Contents.find("CPPFLAGS");
    if (k != m->second.m_Contents.end()) {
        const list<string>& cpp_flags = k->second;
        SMakeProjectT::CreateIncludeDirs(cpp_flags, 
                                         source_base_dir, &include_dirs);
        SMakeProjectT::CreateDefines(cpp_flags, &defines);

    }
    bool style_objcpp = false;
    k = m->second.m_Contents.find("CXXFLAGS");
    if (k != m->second.m_Contents.end()) {
        const list<string>& cxx_flags = k->second;
        style_objcpp = find(cxx_flags.begin(), cxx_flags.end(), "objective-c++") != cxx_flags.end();
    }

    list<CProjKey> depends_ids;
    k = m->second.m_Contents.find("DEPENDENCIES");
    if (k != m->second.m_Contents.end()) {
        const list<string> depends = k->second;
        SMakeProjectT::ConvertLibDepends(depends, &depends_ids /*, &applib_mfilepath*/);
    }

    list<string> requires;
    requires.push_back("DLL");

    list<string> sources;
    list<string> libs_3_party;

    CProjKey proj_key(CProjKey::eDll, proj_id);
    tree->m_Projects[proj_key] = CProjItem(CProjKey::eDll,
                                           proj_name, 
                                           proj_id,
                                           source_base_dir,
                                           sources, 
                                           depends_ids,
                                           requires,
                                           libs_3_party,
                                           include_dirs,
                                           defines,
                                           maketype,
        IdentifySlnGUID(source_base_dir, proj_key));
    tree->m_Projects[proj_key].m_External = true;
    tree->m_Projects[proj_key].m_StyleObjcpp = style_objcpp;
    tree->m_Projects[proj_key].m_MkName = applib_mfilepath;
    tree->m_Projects[proj_key].m_DataSource = CSimpleMakeFileContents(applib_mfilepath);;
    
    k = m->second.m_Contents.find("HOSTED_LIBS");
    if (k != m->second.m_Contents.end()) {
        tree->m_Projects[proj_key].m_HostedLibs = k->second;
    }
    k = m->second.m_Contents.find("DLL_TYPE");
    if (k != m->second.m_Contents.end() && k->second.front() == "plugin") {
        tree->m_Projects[proj_key].m_IsBundle = true;
    }
    list<string> lst_watchers;
    if (m->second.CollectValues("WATCHERS", lst_watchers,
        CSimpleMakeFileContents::eSortUnique)) {
        tree->m_Projects[proj_key].m_Watchers = NStr::Join(lst_watchers, " ");
    }
    tree->m_Projects[proj_key].m_ProjTags.push_back("dll");
    m->second.CollectValues("PROJ_TAG", tree->m_Projects[proj_key].m_ProjTags,
        CSimpleMakeFileContents::eMergePlusMinus);
    return proj_key;
}

//-----------------------------------------------------------------------------
CProjKey SAsnProjectT::DoCreate(const string& source_base_dir,
                                const string& proj_name,
                                const string& applib_mfilepath,
                                const TFiles& makeapp, 
                                const TFiles& makelib, 
                                CProjectItemsTree* tree,
                                const SMakeProjectT::SMakeInInfo& makeinfo)
{
    TAsnType asn_type = GetAsnProjectType(applib_mfilepath, makeapp, makelib);
    if (asn_type == eMultiple) {
        return SAsnProjectMultipleT::DoCreate(source_base_dir,
                                              proj_name,
                                              applib_mfilepath,
                                              makeapp, 
                                              makelib, 
                                              tree, makeinfo);
    }
    if(asn_type == eSingle) {
        return SAsnProjectSingleT::DoCreate(source_base_dir,
                                              proj_name,
                                              applib_mfilepath,
                                              makeapp, 
                                              makelib, 
                                              tree, makeinfo);
    }
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
    return eNoAsn;
}


//-----------------------------------------------------------------------------
CProjKey SAsnProjectSingleT::DoCreate(const string& source_base_dir,
                                      const string& proj_name,
                                      const string& applib_mfilepath,
                                      const TFiles& makeapp, 
                                      const TFiles& makelib, 
                                      CProjectItemsTree* tree,
                                      const SMakeProjectT::SMakeInInfo& makeinfo)
{
    EMakeFileType maketype = makeinfo.m_MakeType;
    CProjItem::TProjType proj_type = 
        IsMakeLibFile( CDirEntry(applib_mfilepath).GetName()) ? CProjKey::eLib : CProjKey::eApp;
    
    CProjKey proj_id = 
        proj_type == CProjKey::eLib? 
            SLibProjectT::DoCreate(source_base_dir, 
                               proj_name, applib_mfilepath, makelib, tree, maketype) : 
            SAppProjectT::DoCreate(source_base_dir, 
                               proj_name, applib_mfilepath, makeapp, tree, maketype);
    if ( proj_id.Id().empty() )
        return CProjKey();
    
    TProjects::iterator p = tree->m_Projects.find(proj_id);
    if (p == tree->m_Projects.end()) {
        LOG_POST(Error << "ASN project not found: " + proj_id.Id());
        return CProjKey();
    }
    CProjItem& project = p->second;

    //Will process .asn or .dtd files
    string source_file_path = CDirEntry::ConcatPath(source_base_dir, proj_name);
    switch (makeinfo.m_Type) {
    case SMakeProjectT::SMakeInInfo::eASN:
        if ( CDirEntry(source_file_path + ".asn").Exists() )
            source_file_path += ".asn";
        break;
    case SMakeProjectT::SMakeInInfo::eDTD:
        if ( CDirEntry(source_file_path + ".dtd").Exists() )
            source_file_path += ".dtd";
        break;
    case SMakeProjectT::SMakeInInfo::eXSD:
        if ( CDirEntry(source_file_path + ".xsd").Exists() )
            source_file_path += ".xsd";
        break;
    case SMakeProjectT::SMakeInInfo::eWSDL:
        if ( CDirEntry(source_file_path + ".wsdl").Exists() )
            source_file_path += ".wsdl";
        break;
    default:
        break;
    }
    if ( !CDirEntry(source_file_path).Exists() ) {
        LOG_POST(Error << "Data specification for ASN project not found: " + proj_id.Id());
        return CProjKey();
    }

    CDataToolGeneratedSrc data_tool_src;
    CDataToolGeneratedSrc::LoadFrom(source_file_path, &data_tool_src);
    if ( !data_tool_src.IsEmpty() ) {
        project.m_DatatoolSources.push_back(data_tool_src);
        if (GetApp().m_Dtdep && !GetApp().GetDatatoolId().empty()) {   
              project.m_Depends.push_back(CProjKey(CProjKey::eApp, GetApp().GetDatatoolId())); 
        }
    }

    return proj_id;
}


//-----------------------------------------------------------------------------
CProjKey SAsnProjectMultipleT::DoCreate(const string& source_base_dir,
                                        const string& proj_name,
                                        const string& applib_mfilepath,
                                        const TFiles& makeapp, 
                                        const TFiles& makelib, 
                                        CProjectItemsTree* tree,
                                        const SMakeProjectT::SMakeInInfo& makeinfo)
{
    EMakeFileType maketype = makeinfo.m_MakeType;
    CProjItem::TProjType proj_type = 
        IsMakeLibFile( CDirEntry(applib_mfilepath).GetName()) ? CProjKey::eLib : CProjKey::eApp;
    

    const TFiles& makefile = proj_type == CProjKey::eLib? makelib : makeapp;
    TFiles::const_iterator m = makefile.find(applib_mfilepath);
    if (m == makefile.end()) {

        LOG_POST(Info << "AsnProject Makefile not found: " << applib_mfilepath);
        return CProjKey();
    }
    const CSimpleMakeFileContents& fc = m->second;

    // ASN
    CSimpleMakeFileContents::TContents::const_iterator k = 
        fc.m_Contents.find("ASN");
    if (k == fc.m_Contents.end()) {

        LOG_POST(Info << "No ASN specified in Makefile: project " << proj_name
                      << "  at " << applib_mfilepath);
        return CProjKey();
    }
    const list<string> asn_names = k->second;

    list<CDataToolGeneratedSrc> datatool_sources;
    ITERATE(list<string>, p, asn_names) {
        const string& asn = *p;
        
        // this dir
        string asn_path_abs = CDirEntry::NormalizePath(source_base_dir);
        asn_path_abs = CDirEntry::AddTrailingPathSeparator(asn_path_abs);
        asn_path_abs = CDirEntry::ConcatPath(asn_path_abs, asn);
        if ( CDirEntry(asn_path_abs + ".asn").Exists() )
            asn_path_abs += ".asn";
        else if ( CDirEntry(asn_path_abs + ".dtd").Exists() )
            asn_path_abs += ".dtd";
        else if ( CDirEntry(asn_path_abs + ".xsd").Exists() )
            asn_path_abs += ".xsd";
        else {
            // one level up
            string parent_dir_abs = ParentDir(source_base_dir);
            string asn_dir_abs = CDirEntry::ConcatPath(parent_dir_abs, asn);
            asn_dir_abs = CDirEntry::NormalizePath(asn_dir_abs);
            asn_dir_abs = CDirEntry::AddTrailingPathSeparator(asn_dir_abs);
        
            asn_path_abs = CDirEntry::ConcatPath(asn_dir_abs, asn);
            if ( CDirEntry(asn_path_abs + ".asn").Exists() )
                asn_path_abs += ".asn";
            else if ( CDirEntry(asn_path_abs + ".dtd").Exists() )
                asn_path_abs += ".dtd";
            else if ( CDirEntry(asn_path_abs + ".xsd").Exists() )
                asn_path_abs += ".xsd";
            else {
                PTB_ERROR_EX(asn_path_abs, ePTB_FileNotFound,
                            "ASN spec file not found");
            }
        }

        CDataToolGeneratedSrc data_tool_src;
        CDataToolGeneratedSrc::LoadFrom(asn_path_abs, &data_tool_src);
        if ( !data_tool_src.IsEmpty() )
            datatool_sources.push_back(data_tool_src);

    }

    // SRC
    k = fc.m_Contents.find("SRC");
    if (k == fc.m_Contents.end()) {

        LOG_POST(Info << "No SRC specified in Makefile: project " << proj_name
                      << "  at " << applib_mfilepath);
        return CProjKey();
    }
    list<string> src_list = k->second;
    if (CMsvc7RegSettings::GetMsvcPlatform() >= CMsvc7RegSettings::eUnix) {
        k = fc.m_Contents.find("UNIX_SRC");
        if (k != fc.m_Contents.end()) {
            copy(k->second.begin(), k->second.end(), back_inserter(src_list));
        }
    }
    list<string> sources;
    ITERATE(list<string>, p, src_list) {
        const string& src = *p;
        if ( !CSymResolver::IsDefine(src) )
            sources.push_back(src);
    }

    CProjKey proj_id = 
        proj_type == CProjKey::eLib? 
        SLibProjectT::DoCreate(source_base_dir, 
                               proj_name, applib_mfilepath, makelib, tree, maketype) :
        SAppProjectT::DoCreate(source_base_dir, 
                               proj_name, applib_mfilepath, makeapp, tree, maketype);
    if ( proj_id.Id().empty() )
        return CProjKey();
    
    TProjects::iterator pid = tree->m_Projects.find(proj_id);
    if (pid == tree->m_Projects.end()) {
        LOG_POST(Error << "ASN project not found: " << proj_id.Id()
                       << " at " << applib_mfilepath);
        return CProjKey();
    }
    CProjItem& project = pid->second;

    // Adjust created proj item
    //SRC - 
    project.m_Sources.clear();
    ITERATE(list<string>, p, src_list) {
        const string& src = *p;
        if ( !CSymResolver::IsDefine(src) )
            project.m_Sources.push_front(src);    
    }
    project.m_Sources.remove(proj_name);
//    project.m_Sources.push_back(proj_name + "__");
//    project.m_Sources.push_back(proj_name + "___");
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

        project.m_Sources.remove(asn);
        project.m_Sources.remove(asn + "__");
        project.m_Sources.remove(asn + "___");
        project.m_Sources.push_back(src + "__");
        project.m_Sources.push_back(src + "___");
    }

    if ( !datatool_sources.empty() ) {
        project.m_DatatoolSources = datatool_sources;
        if (GetApp().m_Dtdep && !GetApp().GetDatatoolId().empty()) {   
              project.m_Depends.push_back(CProjKey(CProjKey::eApp, GetApp().GetDatatoolId())); 
        }
    }

    return proj_id;
}

//-----------------------------------------------------------------------------
CProjKey SMsvcProjectT::DoCreate(const string&      source_base_dir,
                                 const string&      proj_name,
                                 const string&      applib_mfilepath,
                                 const TFiles&      makemsvc, 
                                 CProjectItemsTree* tree,
                                 EMakeFileType maketype)
{
    TFiles::const_iterator m = makemsvc.find(applib_mfilepath);
    if (m == makemsvc.end()) {

        LOG_POST(Info << "Native Makefile not found: " << applib_mfilepath);
        return CProjKey();
    }

    CSimpleMakeFileContents::TContents::const_iterator k;
    //project id
    string proj_id;
    if (CMsvc7RegSettings::GetMsvcPlatform() == CMsvc7RegSettings::eUnix) {
        proj_id = proj_name;
    } else {
        k = m->second.m_Contents.find("MSVC_PROJ");
        if (k == m->second.m_Contents.end()  ||  
                                               k->second.empty()) {

            LOG_POST(Info << "No MSVC_PROJ specified in Makefile: project " << proj_name
                          << "  at " << applib_mfilepath);
            return CProjKey();
        }
        proj_id = k->second.front();
    }
    {{
        CProjKey proj_key(CProjKey::eMsvc, proj_id);
        CProjectItemsTree::TProjects::const_iterator z = tree->m_Projects.find(proj_key);
        if (z != tree->m_Projects.end()) {
            if (z->second.m_MakeType < eMakeType_Excluded) {
                string full_makefile_path = applib_mfilepath;
                PTB_WARNING_EX(full_makefile_path, ePTB_ConfigurationError,
                            "Native project \'" << proj_id << "\' already defined at "
                            << tree->m_Projects[proj_key].m_SourcesBaseDir);
                if (maketype == eMakeType_Excluded || GetApp().IsScanningWholeTree()) {
                    return CProjKey();
                } else {
                    GetApp().RegisterSuspiciousProject(proj_key);
                }
            } else {
                tree->m_Projects.erase(proj_key);
            }
        }
    }}

    string vcproj_file;
    list<string> sources;
    if (CMsvc7RegSettings::GetMsvcPlatform() != CMsvc7RegSettings::eUnix) {
        // VCPROJ - will map to src
        string vcproj_key("VCPROJ");
        if (CMsvc7RegSettings::GetMsvcVersion() >= CMsvc7RegSettings::eMsvc1000) {
            vcproj_key = "VCXPROJ";
        }
        k = m->second.m_Contents.find(vcproj_key);
        if (k == m->second.m_Contents.end()) {

            LOG_POST(Info << "No " << vcproj_key <<" specified in Makefile: project " << proj_name
                          << "  at " << applib_mfilepath);
            return CProjKey();
        }
        ITERATE(list<string>, s, k->second) {
            string d = GetApp().ProcessLocationMacros( *s );
            vcproj_file = d;
            if (CDirEntry::IsAbsolutePath(d)) {
                d = CDirEntry::CreateRelativePath( source_base_dir, d);
            }
            sources.push_back( d );
            break;
        }
    }

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
    k = m->second.m_Contents.find("USR_DEP");
    if (k != m->second.m_Contents.end()) {
        const list<string> deps = k->second;
        list<CProjKey> depends_ids;
        SMakeProjectT::ConvertLibDepends(deps, &depends_ids);
        copy(depends_ids.begin(), depends_ids.end(), back_inserter(depends_ids));
    }

    //requires
    list<string> requires;
    list<string> req_lst;
    if (m->second.CollectValues("REQUIRES", req_lst,
        CSimpleMakeFileContents::eSortUnique)) {
        CMsvcProjectMakefile project_makefile( CDirEntry::ConcatPath(
            source_base_dir, CreateMsvcProjectMakefileName(proj_name, CProjKey::eMsvc)));
        project_makefile.Redefine(req_lst,requires);        
    }

    list<string> libs_3_party;
    list<string> include_dirs;
    list<string> defines;

    CMsvcProjectMakefile project_makefile
                       ((CDirEntry::ConcatPath( source_base_dir, 
                           CreateMsvcProjectMakefileName(proj_name, 
                                                         CProjKey::eMsvc))));
    CProjKey proj_key(CProjKey::eMsvc, proj_id);
    CProjItem project(CProjKey::eMsvc,
                       proj_name, 
                       proj_id,
                       source_base_dir,
                       sources, 
                       depends_ids,
                       requires,
                       libs_3_party,
                       include_dirs,
                       defines,
                       maketype,
        IdentifySlnGUID(vcproj_file, proj_key));

    m->second.CollectValues("PROJ_TAG", project.m_ProjTags,
        CSimpleMakeFileContents::eMergePlusMinus);

    list<string> watchers;
    if (m->second.CollectValues("WATCHERS", watchers,
        CSimpleMakeFileContents::eSortUnique)) {
        project.m_Watchers = NStr::Join(watchers, " ");
    }

    project.m_MkName = applib_mfilepath;
    tree->m_Projects[proj_key] = project;

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
               &subtree_makefiles, eMakeType_Undefined, NULL);

    // Resolve macrodefines
    list<string> metadata_files;
    GetApp().GetMetaDataFiles(&metadata_files);
    CSymResolver resolver;
    resolver.Append( GetApp().GetSite().GetMacros());
    ITERATE(list<string>, p, metadata_files) {
	    string fileloc(CDirEntry::ConcatPath(
	                root_src_path, CDirEntry::ConvertToOSPath(*p)));
	    if (!CDirEntry(fileloc).Exists() && !GetApp().m_ExtSrcRoot.empty()) {
	        fileloc = CDirEntry::ConcatPath( CDirEntry::ConcatPath(
	            GetApp().m_ExtSrcRoot,
	                GetApp().GetConfig().Get("ProjectTree", "src")),
	                    CDirEntry::ConvertToOSPath(*p));
	    }
        if (!CDirEntry(fileloc).Exists()) {
            fileloc = CDirEntry::ConcatPath(CDirEntry(GetApp().m_Solution).GetDir(),*p);
        }
        if (CDirEntry(fileloc).Exists()) {
            LOG_POST(Info << "Resolve macros using rules from " << fileloc);
	        resolver.Append( CSymResolver(fileloc), true);;
        }
	}
    ResolveDefs(resolver, subtree_makefiles);
//    GetApp().UpdateDepGraph( subtree_makefiles.m_Lib);

    // Build projects tree
    CProjectItemsTree::CreateFrom(root_src_path,
                                  subtree_makefiles.m_In, 
                                  subtree_makefiles.m_Lib, 
                                  subtree_makefiles.m_Dll, 
                                  subtree_makefiles.m_App,
                                  subtree_makefiles.m_User, tree);
}


void 
CProjectTreeBuilder::BuildProjectTree(const IProjectFilter* filter,
                                      const string&         root_src_path,
                                      CProjectItemsTree*    tree)
{
    // Build subtree
    CProjectItemsTree target_tree;

    BuildOneProjectTree(filter, root_src_path, &target_tree);

    if (GetApp().IsScanningWholeTree()) {
        *tree = target_tree;
        NON_CONST_ITERATE( CProjectItemsTree::TProjects, t, tree->m_Projects) {
            t->second.m_MakeType = eMakeType_Expendable;
            t->second.m_External = true;
        }
        return;
    }

    GetApp().ExcludeProjectsByTag(target_tree);
    if ( GetApp().m_InteractiveCfg &&
        !GetApp().Gui_ConfirmProjects(target_tree))
    {
        GetApp().SetFail();
        return;
    }
    GetApp().ExcludeUnrequestedProjects(target_tree);

    for (;;) {
        size_t orig_size = target_tree.m_Projects.size();
        // Analyze subtree dependencies
        list<CProjKey> external_depends;
        target_tree.GetExternalDepends(&external_depends);

        // We have to add more projects to the target tree
        if ( !external_depends.empty()) {
            list<CProjKey> depends_to_resolve = external_depends;
            while ( !depends_to_resolve.empty() ) {
                bool modified = false;
                ITERATE(list<CProjKey>, p, depends_to_resolve) {
                    // id of the project we have to resolve
                    const CProjKey& prj_id = *p;
                    CProjectItemsTree::TProjects::const_iterator n = 
                                   GetApp().GetWholeTree().m_Projects.find(prj_id);

                    if (n != GetApp().GetWholeTree().m_Projects.end()) {
                        //insert this project into the target_tree
                        target_tree.m_Projects[prj_id] = n->second;
                        modified = true;
                    } else {
                        /// FIXME: is this needed?
                        _TRACE("Project not found: " + prj_id.Id());
                    }
                }
    
                if (!modified) {
                    //done - no projects has been added to target_tree
                    AddDatatoolSourcesDepends(&target_tree);
                    *tree = target_tree;
                    return;
                } else {
                    //continue resolving dependencies
                    target_tree.GetExternalDepends(&depends_to_resolve);
                }
            }
        }

        AddDatatoolSourcesDepends(&target_tree);
        if (orig_size == target_tree.m_Projects.size()) {
            break;
        }
    }
    *tree = target_tree;
}


void CProjectTreeBuilder::ProcessDir(const string&         dir_name, 
                                     bool                  is_root,
                                     const IProjectFilter* filter,
                                     SMakeFiles*           makefiles,
                                     EMakeFileType         maketype,
                                     const CSimpleMakeFileContents* parent)
{
    // Node - Makefile.in should present
    // this is true if and only if there are also Makefile.*.lib or
    // Makefile.*.app project makefiles to process
    string node_path = 
        CDirEntry::ConcatPath(dir_name, 
                              GetApp().GetProjectTreeInfo().m_TreeNode);
    if ( !is_root  &&  !CDirEntry(node_path).Exists() ) {
        CDir::TGetEntriesFlags flags = CDir::fIgnoreRecursive;
        CDir::TEntries entries =
            CDir(dir_name).GetEntries("Makefile.*.lib", flags);
        if (entries.empty()) {
            entries = CDir(dir_name).GetEntries("Makefile.*.app", flags);
        }
        if ( !entries.empty() ) {
            PTB_WARNING_EX(node_path, ePTB_MissingMakefile,
                           "Makefile.in missing");
        }

        string bld_sys = CDirEntry::DeleteTrailingPathSeparator(
            CDirEntry(GetApp().GetConfigPath()).GetDir());
        if (NStr::CompareNocase(bld_sys,dir_name) == 0) {
            CDir dir(dir_name);
            CDir::TEntries contents;
            contents = dir.GetEntries(GetApp().GetProjectTreeInfo().m_CustomMetaData);
            ITERATE(CDir::TEntries, p, contents) {
                GetApp().AddCustomMetaData( (*p)->GetPath());
            }
            contents = dir.GetEntries(GetApp().GetProjectTreeInfo().m_CustomConfH);
            ITERATE(CDir::TEntries, p, contents) {
                GetApp().AddCustomConfH( (*p)->GetPath());
            }
        }
        return;
    }
    if (!is_root &&
        CMsvc7RegSettings::GetMsvcPlatform() == CMsvc7RegSettings::eUnix) {
        // on UNIX the build tree is already configured,
        // we check if this particular subtree is enabled (ie, exists) there
        string subtree =
            CDirEntry::CreateRelativePath(
                GetApp().GetProjectTreeInfo().m_Src, dir_name);
        subtree = CDirEntry::ConcatPath(CDirEntry(GetApp().m_Solution).GetDir(), subtree);
        if (!CDirEntry(subtree).Exists()) {
            PTB_INFO_EX(subtree, ePTB_NoError,
                        "skipped missing subtree");
            return;
        }
    }
    
    bool weak=false;
    bool process_projects = !is_root && filter->CheckProject(dir_name,&weak);
    if (!process_projects && !weak && !is_root) {
        return;
    }
    
    // Process Makefile.in
    const CSimpleMakeFileContents* mkin = NULL;
    map<string, EMakeFileType> subprojects;
    map<string, EMakeFileType> appprojects;
    map<string, EMakeFileType> libprojects;
    map<string, EMakeFileType> dllprojects;
    map<string, EMakeFileType> userprojects;
    vector<string> ordered_subprojects;
    string topbuilddir;
    bool get_order = GetApp().IsScanningWholeTree();
    if (is_root && get_order) {
        topbuilddir = GetApp().GetRegSettings().GetTopBuilddir();
    }

    if ( process_projects || weak || !topbuilddir.empty()) {
        string node(topbuilddir.empty() ? node_path : topbuilddir);
        ProcessMakeInFile(node, makefiles, maketype, parent);
        TFiles::const_iterator p = makefiles->m_In.find(node);
        if (p != makefiles->m_In.end()) {
        const CSimpleMakeFileContents& makefile = p->second;
        mkin = &makefile;
        CSimpleMakeFileContents::TContents::const_iterator k;
        int j;
        string subproj[] = {"SUB_PROJ","EXPENDABLE_SUB_PROJ","POTENTIAL_SUB_PROJ",""};
        EMakeFileType subtype[] = {eMakeType_Undefined,eMakeType_Expendable,eMakeType_Potential};
        if (filter->ExcludePotential()) {
            subtype[2] = eMakeType_Excluded;
        }
        for (j=0; !subproj[j].empty(); ++j) {
            k = makefile.m_Contents.find(subproj[j]);
            if (k != makefile.m_Contents.end()) {
                const list<string>& values = k->second;
                for (list<string>::const_iterator i=values.begin(); i!=values.end(); ++i) {
                    if (i->at(0) == '#') {
                        break;
                    }
                    subprojects[*i] = max(maketype, subtype[j]);
                    ordered_subprojects.push_back(*i);
                }
            }
        }
        if ( process_projects ) {
        string userproj[] = {"UNIX_PROJ","EXPENDABLE_UNIX_PROJ", ""};
        if (CMsvc7RegSettings::GetMsvcPlatform() != CMsvc7RegSettings::eUnix) {
            userproj[0] = "MSVC_PROJ";
            userproj[1] = "";
        }
        EMakeFileType usertype[] = {eMakeType_Undefined,eMakeType_Expendable};
        for (j=0; !userproj[j].empty(); ++j) {
            k = makefile.m_Contents.find(userproj[j]);
            if (k != makefile.m_Contents.end()) {
                const list<string>& values = k->second;
                for (list<string>::const_iterator i=values.begin(); i!=values.end(); ++i) {
                    if (i->at(0) == '#') {
                        break;
                    }
                    if (CMsvc7RegSettings::GetMsvcPlatform() == CMsvc7RegSettings::eUnix) {
                        userprojects["Makefile." + *i] = max(maketype, usertype[j]);
                        userprojects["Makefile." + *i + ".in"] = max(maketype, usertype[j]);
                        if (get_order) {
                            s_WriteBuildOrder(dir_name,"Makefile." + *i);
                            s_WriteBuildOrder(dir_name,"Makefile." + *i + ".in");
                        }
                    } else {
                        string mkname("Makefile." + *i + ".msvcproj");
                        userprojects[mkname] = max(maketype, usertype[j]);
                        if (get_order) {
                            s_WriteBuildOrder(dir_name,mkname);
                        }
                    }
                }
            }
        }
        string libproj[] = {"ASN_PROJ","DTD_PROJ","XSD_PROJ","WSDL_PROJ",
            "LIB_PROJ","EXPENDABLE_LIB_PROJ","POTENTIAL_LIB_PROJ",""};
        EMakeFileType libtype[] = {
            eMakeType_Undefined, eMakeType_Undefined, eMakeType_Undefined, eMakeType_Undefined,
            eMakeType_Undefined,eMakeType_Expendable,eMakeType_Potential};
        if (filter->ExcludePotential()) {
            libtype[6] = eMakeType_Excluded;
        }
        for (j=0; !libproj[j].empty(); ++j) {
            k = makefile.m_Contents.find(libproj[j]);
            if (k != makefile.m_Contents.end()) {
                const list<string>& values = k->second;
                for (list<string>::const_iterator i=values.begin(); i!=values.end(); ++i) {
                    if (i->at(0) == '#') {
                        break;
                    }
                    string mkname("Makefile." + *i + ".lib");
                    libprojects[mkname] = max(maketype, libtype[j]);
                    if (get_order) {
                        s_WriteBuildOrder(dir_name,mkname);
                    }
                }
            }
        }
        string dllproj[] = {"DLL_PROJ","EXPENDABLE_DLL_PROJ","POTENTIAL_DLL_PROJ",""};
        EMakeFileType dlltype[] = {eMakeType_Undefined,eMakeType_Expendable,eMakeType_Potential};
        if (filter->ExcludePotential()) {
            dlltype[2] = eMakeType_Excluded;
        }
        for (j=0; !dllproj[j].empty(); ++j) {
            k = makefile.m_Contents.find(dllproj[j]);
            if (k != makefile.m_Contents.end()) {
                const list<string>& values = k->second;
                for (list<string>::const_iterator i=values.begin(); i!=values.end(); ++i) {
                    if (i->at(0) == '#') {
                        break;
                    }
                    string mkname("Makefile." + *i + ".dll");
                    dllprojects[mkname] = max(maketype, dlltype[j]);
                    if (get_order) {
                        s_WriteBuildOrder(dir_name,mkname);
                    }
                }
            }
        }
        string appproj[] = {"APP_PROJ","EXPENDABLE_APP_PROJ","POTENTIAL_APP_PROJ",""};
        EMakeFileType apptype[] = {eMakeType_Undefined,eMakeType_Expendable,eMakeType_Potential};
        if (filter->ExcludePotential()) {
            apptype[2] = eMakeType_Excluded;
        }
        for (j=0; !appproj[j].empty(); ++j) {
            k = makefile.m_Contents.find(appproj[j]);
            if (k != makefile.m_Contents.end()) {
                const list<string>& values = k->second;
                for (list<string>::const_iterator i=values.begin(); i!=values.end(); ++i) {
                    if (i->at(0) == '#') {
                        break;
                    }
                    string mkname("Makefile." + *i + ".app");
                    appprojects[mkname] = max(maketype, apptype[j]);
                    if (get_order) {
                        s_WriteBuildOrder(dir_name,mkname);
                    }
                }
            }
        }
        }
        }
    }

    // Process Makefile.*.lib
    if ( process_projects && !libprojects.empty()) {
        CDir dir(dir_name);
        CDir::TEntries contents = dir.GetEntries("Makefile.*.lib");
        ITERATE(CDir::TEntries, p, contents) {
            const AutoPtr<CDirEntry>& dir_entry = *p;
            const string name = dir_entry->GetName();
            if (libprojects.find(name) != libprojects.end() &&
                SMakeProjectT::IsMakeLibFile(name) )
	            ProcessMakeLibFile(dir_entry->GetPath(), makefiles, libprojects[name], mkin);

        }
    }
    // Process Makefile.*.dll
    if ( process_projects && !dllprojects.empty()) {
        CDir dir(dir_name);
        CDir::TEntries contents = dir.GetEntries("Makefile.*.dll");
        ITERATE(CDir::TEntries, p, contents) {
            const AutoPtr<CDirEntry>& dir_entry = *p;
            const string name = dir_entry->GetName();
            if (dllprojects.find(name) != dllprojects.end() &&
                SMakeProjectT::IsMakeDllFile(name) )
	            ProcessMakeDllFile(dir_entry->GetPath(), makefiles, dllprojects[name], mkin);

        }
    }
    // Process Makefile.*.app
    if ( process_projects && !appprojects.empty() ) {
        CDir dir(dir_name);
        CDir::TEntries contents = dir.GetEntries("Makefile.*.app");
        ITERATE(CDir::TEntries, p, contents) {
            const AutoPtr<CDirEntry>& dir_entry = *p;
            const string name = dir_entry->GetName();
            if (appprojects.find(name) != appprojects.end() &&
                SMakeProjectT::IsMakeAppFile(name) )
	            ProcessMakeAppFile(dir_entry->GetPath(), makefiles, appprojects[name], mkin);

        }
    }
    // Process Makefile.*.msvcproj
    if ( process_projects && !userprojects.empty() ) {
        CDir dir(dir_name);
//        CDir::TEntries contents = dir.GetEntries("Makefile.*.msvcproj");
        CDir::TEntries contents = dir.GetEntries("Makefile.*");
        ITERATE(CDir::TEntries, p, contents) {
            const AutoPtr<CDirEntry>& dir_entry = *p;
            const string name = dir_entry->GetName();
            if (userprojects.find(name) != userprojects.end() /*&&
                SMakeProjectT::IsUserProjFile(name)*/ )
	            ProcessUserProjFile(dir_entry->GetPath(), makefiles, userprojects[name], mkin);

        }
    }

    if ( process_projects) {
        /*if (!GetApp().IsScanningWholeTree())*/ {
            CDir dir(dir_name);
            CDir::TEntries contents = dir.GetEntries(GetApp().GetProjectTreeInfo().m_CustomMetaData);
            ITERATE(CDir::TEntries, p, contents) {
                GetApp().AddCustomMetaData( (*p)->GetPath());
            }
            contents = dir.GetEntries(GetApp().GetProjectTreeInfo().m_CustomConfH);
            ITERATE(CDir::TEntries, p, contents) {
                GetApp().AddCustomConfH( (*p)->GetPath());
            }
        }
    }

    // Convert subprojects to subdirs
    map<string, EMakeFileType> subprojects_dirs;
    vector<string> ordered_subprojects_dirs;

    // begin with explicitely requested subprojects
    ITERATE( vector<string>, p, ordered_subprojects) {
        string name(*p);
        CDirEntry dir_entry( CDirEntry::ConcatPath(dir_name, name));
        if ( dir_entry.IsDir() ) {
            if (subprojects.find(name) != subprojects.end()) {
                subprojects_dirs[dir_entry.GetPath()] = subprojects[name];
            } else {
                subprojects_dirs[dir_entry.GetPath()] =
                    is_root ? eMakeType_Undefined : eMakeType_Excluded;
            }
            if (find(ordered_subprojects_dirs.begin(), ordered_subprojects_dirs.end(), name) == 
                     ordered_subprojects_dirs.end()) {
                ordered_subprojects_dirs.push_back(name);
            } else {
                PTB_WARNING_EX(node_path, ePTB_InvalidMakefile,
                            "Duplicate entry: " << name);
            }
        }
    }

//    if ( is_root || (!process_projects && weak) ) {
        CDir dir(dir_name);
        CDir::TEntries contents = dir.GetEntries("*");
        ITERATE(CDir::TEntries, p, contents) {
            const AutoPtr<CDirEntry>& dir_entry = *p;
            if ( !dir_entry->IsDir() ) {
                continue;
            }
            string name  = dir_entry->GetName();
//            if ( name == "."  ||  name == ".." ||  name == "CVS" ||  name == ".svn" ||
            if ( name[0] == '.'  ||  name == "CVS" ||
                 name == string(1,CDir::GetPathSeparator()) ) {
                continue;
            }
            if (find(ordered_subprojects_dirs.begin(), ordered_subprojects_dirs.end(), name) != 
                     ordered_subprojects_dirs.end()) {
                // already processed
                continue;
            }
            if (subprojects.find(name) != subprojects.end()) {
                subprojects_dirs[dir_entry->GetPath()] = subprojects[name];
            } else {
                subprojects_dirs[dir_entry->GetPath()] =
                    is_root ? eMakeType_Undefined : eMakeType_Excluded;
            }
            if (find(ordered_subprojects_dirs.begin(), ordered_subprojects_dirs.end(), name) == 
                        ordered_subprojects_dirs.end()) {
                ordered_subprojects_dirs.push_back(name);
            }
        }
        {
            map<string, EMakeFileType>::const_iterator s;
            for (s = subprojects.begin(); s != subprojects.end(); ++s) {
                if (s->first.find('/') != string::npos) {
                    CDir dir_entry(CDirEntry::NormalizePath(
                        CDirEntry::ConcatPath(dir_name, s->first)));
                    if (dir_entry.IsDir()) {
                        subprojects_dirs[dir_entry.GetPath()] = subprojects[s->first];
                    }
                }
            }
        }
/*
    } else {
        // for non-root only subprojects
        map<string, EMakeFileType>::const_iterator p;
        for (p = subprojects.begin(); p != subprojects.end(); ++p) {
            const string& subproject = p->first;
            string subproject_dir = 
                CDirEntry::ConcatPath(dir_name, subproject);
            subprojects_dirs[subproject_dir] = p->second;
        }
    }
*/

    // Process subproj ( e.t. subdirs )
    ITERATE( vector<string>, ps, ordered_subprojects_dirs) {
        const string& subproject_dir = CDirEntry::ConcatPath(dir_name, *ps);
        ProcessDir(subproject_dir, false, filter, makefiles, subprojects_dirs[subproject_dir], mkin);
    }

}


void CProjectTreeBuilder::ProcessMakeInFile(const string& file_name, 
                                            SMakeFiles*   makefiles,
                                            EMakeFileType type,
                                            const CSimpleMakeFileContents* parent)
{
    CSimpleMakeFileContents fc(file_name, type);
    fc.SetParent(parent);
    if ( !fc.m_Contents.empty() ) {
	    makefiles->m_In[file_name] = fc;
        PTB_TRACE_EX(file_name, 0, MakeFileTypeAsString(type));
	} else {
        PTB_WARNING(file_name, "ignored; empty");
	}
}


void CProjectTreeBuilder::ProcessMakeLibFile(const string& file_name, 
                                             SMakeFiles*   makefiles,
                                             EMakeFileType type,
                                             const CSimpleMakeFileContents* parent)
{
    CSimpleMakeFileContents fc(file_name, type);
    fc.SetParent(parent);
    if ( !fc.m_Contents.empty()  ) {
        makefiles->m_Lib[file_name] = fc;
        PTB_TRACE_EX(file_name, 0, MakeFileTypeAsString(type));
	} else {
        PTB_WARNING(file_name, "ignored; empty");
	}
}

void CProjectTreeBuilder::ProcessMakeDllFile(const string& file_name, 
                                             SMakeFiles*   makefiles,
                                             EMakeFileType type,
                                             const CSimpleMakeFileContents* parent)
{
    string s = "MakeDll : " + file_name + "   ";

    CSimpleMakeFileContents fc(file_name, type);
    fc.SetParent(parent);
    if ( !fc.m_Contents.empty()  ) {
        makefiles->m_Dll[file_name] = fc;
	} else {
        LOG_POST(Info << s << "rejected (is empty)");
	}
}


void CProjectTreeBuilder::ProcessMakeAppFile(const string& file_name, 
                                             SMakeFiles*   makefiles,
                                             EMakeFileType type,
                                             const CSimpleMakeFileContents* parent)
{
    CSimpleMakeFileContents fc(file_name, type);
    fc.SetParent(parent);
    if ( !fc.m_Contents.empty() ) {
        makefiles->m_App[file_name] = fc;
        PTB_TRACE_EX(file_name, 0, MakeFileTypeAsString(type));
	} else {
        PTB_WARNING(file_name, "ignored; empty");
	}
}


void CProjectTreeBuilder::ProcessUserProjFile(const string& file_name, 
                                             SMakeFiles*   makefiles,
                                             EMakeFileType type,
                                             const CSimpleMakeFileContents* parent)
{
    bool allow_empty = CMsvc7RegSettings::GetMsvcPlatform() == CMsvc7RegSettings::eUnix;
    CSimpleMakeFileContents fc(file_name, type);
    fc.SetParent(parent);
    if ( allow_empty || !fc.m_Contents.empty() ) {
	    makefiles->m_User[file_name] = fc;
        PTB_TRACE_EX(file_name, 0, MakeFileTypeAsString(type));
	} else {
        PTB_WARNING(file_name, "ignored; empty");
	}
}

//recursive resolving
void CProjectTreeBuilder::ResolveDefs(CSymResolver& resolver, 
                                      SMakeFiles&   makefiles)
{
    {{
        _TRACE("*** Resolving macrodefinitions in App projects ***");
        //App
        set<string> keys;
        keys.insert("LIB");
        keys.insert("LIBS");
        if (GetApp().GetBuildType().GetType() == CBuildType::eStatic) {
            keys.insert("STATIC_LIB");
            keys.insert("STATIC_LIBS");
        }
        keys.insert("NCBI_C_LIBS");
        SMakeProjectT::DoResolveDefs(resolver, makefiles.m_App, keys);
    }}

    {{
        _TRACE("*** Resolving macrodefinitions in Lib projects ***");
        //Lib
        set<string> keys;
        keys.insert("LIB");
        keys.insert("LIBS");
        if (GetApp().GetBuildType().GetType() == CBuildType::eStatic) {
            keys.insert("STATIC_LIB");
            keys.insert("STATIC_LIBS");
        }
        keys.insert("SRC");
        keys.insert("DLL_LIB");
        if (GetApp().GetBuildType().GetType() == CBuildType::eDll) {
            keys.insert("DLL_DLIB");
        }
        SMakeProjectT::DoResolveDefs(resolver, makefiles.m_Lib, keys);
    }}

    {{
        _TRACE("*** Resolving macrodefinitions in Msvc projects ***");
        set<string> keys;
        keys.insert("DLL_DEP");
        SMakeProjectT::DoResolveDefs(resolver, makefiles.m_User, keys);
    }}
}


//analyze modules
void s_CollectDatatoolIds(const CProjectItemsTree& tree,
                          map<string, CProjKey>*   datatool_ids)
{
    ITERATE(CProjectItemsTree::TProjects, p, tree.m_Projects) {
        const CProjKey&  project_id = p->first;
        if (project_id.Type() == CProjKey::eDataSpec) {
            continue;
        }
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
    bool whole_collected = false;
    if (GetApp().IsScanningWholeTree()) {
        whole_collected = true;
        s_CollectDatatoolIds(GetApp().GetWholeTree(), &whole_datatool_ids);
    }

    // 2. Extent tree to accomodate more ASN projects if necessary
    bool tree_extented = false;
    map<string, CProjKey> datatool_ids;

    do {
        
        tree_extented = false;
        s_CollectDatatoolIds(*tree, &datatool_ids);

        CProjectItemsTree::TProjects added;

        NON_CONST_ITERATE(CProjectItemsTree::TProjects, p, tree->m_Projects) {
//            const CProjKey&  project_id = p->first;
            CProjItem& project          = p->second;
            ITERATE(list<CDataToolGeneratedSrc>, n, project.m_DatatoolSources) {
                const CDataToolGeneratedSrc& src = *n;
                ITERATE(list<string>, i, src.m_ImportModules) {
                    const string& module = *i;
                    map<string, CProjKey>::const_iterator j = 
                        datatool_ids.find(module);
                    if (j == datatool_ids.end()) {
                        if (!whole_collected) {
                            whole_collected = true;
                            s_CollectDatatoolIds(GetApp().GetWholeTree(), &whole_datatool_ids);
                        }
                        j = whole_datatool_ids.find(module);
                        if (j != whole_datatool_ids.end()) {
                            const CProjKey& depends_id = j->second;
#if 1
datatool_ids[module] = depends_id;
added[depends_id] = GetApp().GetWholeTree().m_Projects.find(depends_id)->second;
#else
                            tree->m_Projects[depends_id] = 
                                GetApp().GetWholeTree().m_Projects.find(depends_id)->second;
                            tree_extented = true;
#endif
                        }
                    }
                }
            }
        }
#if 1
        tree_extented = !added.empty();
        ITERATE(CProjectItemsTree::TProjects, p, added) {
            tree->m_Projects[p->first] = p->second;
        }
#endif
    } while( tree_extented );


    CProjKey proj_key(CProjKey::eDataSpec, GetApp().GetDataspecProjId());
    CProjectItemsTree::TProjects::iterator z = tree->m_Projects.find(proj_key);

    // 3. Finally - generate depends
    NON_CONST_ITERATE(CProjectItemsTree::TProjects, p, tree->m_Projects) {
        const CProjKey&  project_id = p->first;
        if (project_id.Type() == CProjKey::eDataSpec) {
            continue;
        }
        CProjItem& project          = p->second;
        ITERATE(list<CDataToolGeneratedSrc>, n, project.m_DatatoolSources) {
            const CDataToolGeneratedSrc& src = *n;
            if (z != tree->m_Projects.end()) {
                z->second.m_DatatoolSources.push_back(src);
            }
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
