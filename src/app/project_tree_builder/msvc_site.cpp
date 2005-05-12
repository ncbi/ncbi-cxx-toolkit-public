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
#include <app/project_tree_builder/msvc_site.hpp>
#include <app/project_tree_builder/proj_builder_app.hpp>
#include <app/project_tree_builder/msvc_prj_defines.hpp>

#include <algorithm>

#include <corelib/ncbistr.hpp>

BEGIN_NCBI_SCOPE


//-----------------------------------------------------------------------------
CMsvcSite::CMsvcSite(const CNcbiRegistry& registry)
    :m_Registry(registry)
{
    // Not provided requests
    string not_provided_requests_str = 
        m_Registry.GetString("Configure", "NotProvidedRequests", "");
    
    list<string> not_provided_requests_list;
    NStr::Split(not_provided_requests_str, LIST_SEPARATOR, 
                not_provided_requests_list);

    copy(not_provided_requests_list.begin(),
         not_provided_requests_list.end(),
         inserter(m_NotProvidedThing, m_NotProvidedThing.end()));

    // Lib choices
    string lib_choices_str = 
        m_Registry.GetString("Configure", "LibChoices", "");

    list<string> lib_choices_list;
    NStr::Split(lib_choices_str, LIST_SEPARATOR, 
                lib_choices_list);
    ITERATE(list<string>, p, lib_choices_list) {
        const string& choice_str = *p;
        string lib_id;
        string lib_3party_id;
        if ( NStr::SplitInTwo(choice_str, "/", lib_id, lib_3party_id) ) {
            m_LibChoices.push_back(SLibChoice(*this, lib_id, lib_3party_id));
        } else {
           LOG_POST(Error << "Incorrect LibChoices definition: " << choice_str);
        }
    }

}


bool CMsvcSite::IsProvided(const string& thing) const
{
    return m_NotProvidedThing.find(thing) == m_NotProvidedThing.end();
}


bool CMsvcSite::IsDescribed(const string& section) const
{
    list<string> sections;
    m_Registry.EnumerateSections(&sections);
    return find(sections.begin(), sections.end(), section) != sections.end();
}


void CMsvcSite::GetComponents(const string& entry, 
                              list<string>* components) const
{
    components->clear();
    string comp_str = m_Registry.GetString(entry, "Component", "");
    NStr::Split(comp_str, " ,\t", *components);
}

string CMsvcSite::ProcessMacros(string raw_data) const
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
            definition = m_Registry.GetString("Configure", macro, "");
            if (definition.empty()) {
                // preserve unresolved macros
                done = end;
            } else {
                data = NStr::Replace(data, raw_macro, definition);
            }
        }
    }
    return data;
}

void CMsvcSite::GetLibInfo(const string& lib, 
                           const SConfigInfo& config, SLibInfo* libinfo) const
{
    libinfo->Clear();

    string include_str    = ProcessMacros(GetOpt(m_Registry, lib, "INCLUDE", config));
    NStr::Split(include_str, LIST_SEPARATOR, libinfo->m_IncludeDir);
    
    string defines_str    = GetOpt(m_Registry, lib, "DEFINES", config);
    NStr::Split(defines_str, LIST_SEPARATOR, libinfo->m_LibDefines);

    libinfo->m_LibPath    = ProcessMacros(GetOpt(m_Registry, lib, "LIBPATH", config));

    string libs_str = GetOpt(m_Registry, lib, "LIB", config);
    NStr::Split(libs_str, LIST_SEPARATOR, libinfo->m_Libs);

    libs_str = GetOpt(m_Registry, lib, "STDLIB", config);
    NStr::Split(libs_str, LIST_SEPARATOR, libinfo->m_StdLibs);

    string macro_str = GetOpt(m_Registry, lib, "MACRO", config);
    NStr::Split(macro_str, LIST_SEPARATOR, libinfo->m_Macro);
}


bool CMsvcSite::IsLibEnabledInConfig(const string&      lib, 
                                     const SConfigInfo& config) const
{
    string enabled_configs_str = m_Registry.GetString(lib, "CONFS", "");
    list<string> enabled_configs;
    NStr::Split(enabled_configs_str, 
                LIST_SEPARATOR, enabled_configs);

    return find(enabled_configs.begin(), 
                enabled_configs.end(), 
                config.m_Name) != enabled_configs.end();
}


string CMsvcSite::ResolveDefine(const string& define) const
{
    return ProcessMacros(m_Registry.GetString("Defines", define, ""));
}


string CMsvcSite::GetConfigureDefinesPath(void) const
{
    return m_Registry.GetString("Configure", "DefinesPath", "");
}


void CMsvcSite::GetConfigureDefines(list<string>* defines) const
{
    defines->clear();
    string defines_str = m_Registry.GetString("Configure", "Defines", "");
    NStr::Split(defines_str, LIST_SEPARATOR, *defines);
}


bool CMsvcSite::IsLibWithChoice(const string& lib_id) const
{
    ITERATE(list<SLibChoice>, p, m_LibChoices) {
        const SLibChoice& choice = *p;
        if (lib_id == choice.m_LibId)
            return true;
    }
    return false;
}


bool CMsvcSite::Is3PartyLibWithChoice(const string& lib3party_id) const
{
    ITERATE(list<SLibChoice>, p, m_LibChoices) {
        const SLibChoice& choice = *p;
        if (lib3party_id == choice.m_3PartyLib)
            return true;
    }
    return false;
}


CMsvcSite::SLibChoice::SLibChoice(void)
 :m_Choice(eUnknown)
{
}


CMsvcSite::SLibChoice::SLibChoice(const CMsvcSite& site,
                                  const string&    lib,
                                  const string&    lib_3party)
 :m_LibId    (lib),
  m_3PartyLib(lib_3party)
{
    m_Choice = e3PartyLib;

    ITERATE(list<SConfigInfo>, p, GetApp().GetRegSettings().m_ConfigInfo) {

        const SConfigInfo& config = *p;
        SLibInfo lib_info;
        site.GetLibInfo(m_3PartyLib, config, &lib_info);

        if ( !CMsvcSite::IsLibOk(lib_info) ) {

            m_Choice = eLib;
            break;
        }
    }
}


CMsvcSite::ELibChoice CMsvcSite::GetChoiceForLib(const string& lib_id) const
{
    ITERATE(list<SLibChoice>, p, m_LibChoices) {

        const SLibChoice& choice = *p;
        if (choice.m_LibId == lib_id) 
            return choice.m_Choice;
    }
    return eUnknown;
}

CMsvcSite::ELibChoice CMsvcSite::GetChoiceFor3PartyLib(
    const string& lib3party_id, const SConfigInfo& cfg_info) const
{
    ITERATE(list<SLibChoice>, p, m_LibChoices) {
        const SLibChoice& choice = *p;
        if (choice.m_3PartyLib == lib3party_id) {
            SLibInfo lib_info;
            GetLibInfo(lib3party_id, cfg_info, &lib_info);
            return IsLibOk(lib_info,true) ? e3PartyLib : eLib;
        }
    }
    return eUnknown;
}


void CMsvcSite::GetLibChoiceIncludes(
    const string& cpp_flags_define, list<string>* abs_includes) const
{
    abs_includes->clear();

    string include_str = m_Registry.GetString("LibChoicesIncludes", 
                                              cpp_flags_define, "");
    if (!include_str.empty()) {
        abs_includes->push_back("$(" + cpp_flags_define + ")");
    }
}

void CMsvcSite::GetLibChoiceIncludes(
    const string& cpp_flags_define, const SConfigInfo& cfg_info,
    list<string>* abs_includes) const
{
    abs_includes->clear();
    string include_str = m_Registry.GetString("LibChoicesIncludes", 
                                              cpp_flags_define, "");
    //split on parts
    list<string> parts;
    NStr::Split(include_str, LIST_SEPARATOR, parts);

    string lib_id;
    ITERATE(list<string>, p, parts) {
        if ( lib_id.empty() )
            lib_id = *p;
        else  {
            
            SLibChoice choice = GetLibChoiceForLib(lib_id);
            SLibInfo lib_info;
            GetLibInfo(choice.m_3PartyLib, cfg_info, &lib_info);
            if ( IsLibOk(lib_info, true) ) {
//                abs_includes->push_back(lib_info.m_IncludeDir);
                copy(lib_info.m_IncludeDir.begin(), 
                    lib_info.m_IncludeDir.end(), back_inserter(*abs_includes));
            } else {
                const string& rel_include_path = *p;
                string abs_include_path = 
                    GetApp().GetProjectTreeInfo().m_Include;
                abs_include_path = 
                    CDirEntry::ConcatPath(abs_include_path, rel_include_path);
                abs_include_path = CDirEntry::NormalizePath(abs_include_path);
                abs_includes->push_back(abs_include_path);
            }
            lib_id.erase();
        }
    }
}

void CMsvcSite::GetLibInclude(const string& lib_id,
    const SConfigInfo& cfg_info, list<string>* includes) const
{
    includes->clear();
    if (CSymResolver::IsDefine(lib_id)) {
        GetLibChoiceIncludes( CSymResolver::StripDefine(lib_id), cfg_info, includes);
        return;
    }
    SLibInfo lib_info;
    GetLibInfo(lib_id, cfg_info, &lib_info);
    if ( IsLibOk(lib_info, true) ) {
//        includes->push_back(lib_info.m_IncludeDir);
        copy(lib_info.m_IncludeDir.begin(),
             lib_info.m_IncludeDir.end(), back_inserter(*includes));
        return;
    } else {
        if (!lib_info.IsEmpty()) {
            LOG_POST(Warning << lib_id << "|" << cfg_info.m_Name
                          << " unavailable: library include ignored: "
                          << NStr::Join(lib_info.m_IncludeDir,","));
        }
    }
}

CMsvcSite::SLibChoice CMsvcSite::GetLibChoiceForLib(const string& lib_id) const
{
    ITERATE(list<SLibChoice>, p, m_LibChoices) {

        const SLibChoice& choice = *p;
        if (choice.m_LibId == lib_id) 
            return choice;
    }
    return SLibChoice();

}

CMsvcSite::SLibChoice CMsvcSite::GetLibChoiceFor3PartyLib(const string& lib3party_id) const
{
    ITERATE(list<SLibChoice>, p, m_LibChoices) {
        const SLibChoice& choice = *p;
        if (choice.m_3PartyLib == lib3party_id)
            return choice;
    }
    return SLibChoice();
}


string CMsvcSite::GetAppDefaultResource(void) const
{
    return m_Registry.GetString("DefaultResource", "app", "");
}


void CMsvcSite::GetThirdPartyLibsToInstall(list<string>* libs) const
{
    libs->clear();

    string libs_str = m_Registry.GetString("Configure", 
                                           "ThirdPartyLibsToInstall", "");
    NStr::Split(libs_str, LIST_SEPARATOR, *libs);
}


string CMsvcSite::GetThirdPartyLibsBinPathSuffix(void) const
{
    return m_Registry.GetString("Configure", 
                                "ThirdPartyLibsBinPathSuffix", "");
}

string CMsvcSite::GetThirdPartyLibsBinSubDir(void) const
{
    return m_Registry.GetString("Configure", 
                                "ThirdPartyLibsBinSubDir", "");
}

void CMsvcSite::GetStandardFeatures(list<string>& features) const
{
    features.clear();
    string features_str = m_Registry.GetString("Configure", 
                                           "StandardFeatures", "");
    NStr::Split(features_str, LIST_SEPARATOR, features);
}

//-----------------------------------------------------------------------------
bool CMsvcSite::IsLibOk(const SLibInfo& lib_info, bool silent)
{
    if ( lib_info.IsEmpty() )
        return false;
    if ( !lib_info.m_IncludeDir.empty() ) {
        ITERATE(list<string>, i, lib_info.m_IncludeDir) {
            if (!CDirEntry(*i).Exists() ) {
                if (!silent) {
                    LOG_POST(Warning << "No LIB INCLUDE: " + *i);
                }
                return false;
            }
        }
    }
    if ( !lib_info.m_LibPath.empty() &&
         !CDirEntry(lib_info.m_LibPath).Exists() ) {
        if (!silent) {
            LOG_POST(Warning << "No LIBPATH: " + lib_info.m_LibPath);
        }
        return false;
    }
    if ( !lib_info.m_LibPath.empty()) {
        ITERATE(list<string>, p, lib_info.m_Libs) {
            const string& lib = *p;
            string lib_path_abs = CDirEntry::ConcatPath(lib_info.m_LibPath, lib);
            if ( !lib_path_abs.empty() &&
                !CDirEntry(lib_path_abs).Exists() ) {
                if (!silent) {
                    LOG_POST(Warning << "No LIB: " + lib_path_abs);
                }
                return false;
            }
        }
    }

    return true;
}

void CMsvcSite::ProcessMacros(const list<SConfigInfo>& configs)
{
    string macros_str = m_Registry.GetString("Configure", "Macros", "");
    list<string> macros;
    NStr::Split(macros_str, LIST_SEPARATOR, macros);

    ITERATE(list<string>, m, macros) {
        const string& macro = *m;
        if (!IsDescribed(macro)) {
            // add empty value
            LOG_POST(Error << "Macro " << macro << " is not described");
        }
        list<string> components;
        GetComponents(macro, &components);
        bool res = false;
        ITERATE(list<string>, p, components) {
            const string& component = *p;
            ITERATE(list<SConfigInfo>, n, configs) {
                const SConfigInfo& config = *n;
                SLibInfo lib_info;
                GetLibInfo(component, config, &lib_info);
                if ( IsLibOk(lib_info) ) {
                    res = true;
                } else {
                    if (!lib_info.IsEmpty()) {
                        LOG_POST(Warning << "Macro " << macro
                            << " cannot be resolved for "
                            << component << "|" << config.m_Name);
                    }
//                    res = false;
//                    break;
                }
            }
        }
        if (res) {
            string value =  m_Registry.GetString(macro, "Value", "");
            m_Macros.AddDefinition(macro,value);
        }
    }
}


END_NCBI_SCOPE
/*
 * ===========================================================================
 * $Log$
 * Revision 1.28  2005/05/12 18:06:34  gouriano
 * Preserve unresolved macros
 *
 * Revision 1.27  2005/02/15 19:04:29  gouriano
 * Added list of standard features
 *
 * Revision 1.26  2004/12/30 17:48:49  gouriano
 * INCLUDE changed from string to list of strings
 *
 * Revision 1.25  2004/12/20 15:24:56  gouriano
 * Changed diagnostic output
 *
 * Revision 1.24  2004/12/06 18:12:20  gouriano
 * Improved diagnostics
 *
 * Revision 1.23  2004/11/30 17:21:05  gouriano
 * Resolve Macros on per-configuration basis
 *
 * Revision 1.22  2004/11/29 17:03:39  gouriano
 * Add 3rd party library dependencies on per-configuration basis
 *
 * Revision 1.21  2004/11/23 20:12:12  gouriano
 * Tune libraries with the choice for each configuration independently
 *
 * Revision 1.20  2004/11/17 19:55:51  gouriano
 * Expand macros in Defines section
 *
 * Revision 1.19  2004/11/02 18:09:19  gouriano
 * Allow macros in the definition of 3rd party library paths
 *
 * Revision 1.18  2004/08/25 19:38:40  gouriano
 * Implemented optional dependency on a third party library
 *
 * Revision 1.17  2004/07/20 13:38:40  gouriano
 * Added conditional macro definition
 *
 * Revision 1.16  2004/06/01 16:03:54  gorelenk
 * Implemented CMsvcSite::GetLibChoiceForLib and SLibChoice::SLibChoice .
 *
 * Revision 1.15  2004/05/24 14:41:59  gorelenk
 * Implemented member-functions supporting auto-install of third-party libs:
 * CMsvcSite::GetThirdPartyLibsToInstall,
 * CMsvcSite::GetThirdPartyLibsBinPathSuffix,
 * CMsvcSite::GetThirdPartyLibsBinSubDir .
 *
 * Revision 1.14  2004/05/21 21:41:41  gorelenk
 * Added PCH ncbi_pch.hpp
 *
 * Revision 1.13  2004/05/17 14:41:10  gorelenk
 * Implemented CMsvcSite::GetAppDefaultResource .
 *
 * Revision 1.12  2004/05/13 14:55:00  gorelenk
 * Implemented CMsvcSite::GetLibChoiceIncludes .
 *
 * Revision 1.11  2004/04/20 22:09:00  gorelenk
 * Changed implementation of struct SLibChoice constructor.
 *
 * Revision 1.10  2004/04/19 15:39:43  gorelenk
 * Implemeted choice related members of class CMsvcSite:
 * struct SLibChoice constructor,functions IsLibWithChoice,
 * Is3PartyLibWithChoice, GetChoiceForLib and GetChoiceFor3PartyLib .
 *
 * Revision 1.9  2004/03/22 14:48:38  gorelenk
 * Changed implementation of CMsvcSite::GetLibInfo .
 *
 * Revision 1.8  2004/02/24 20:53:16  gorelenk
 * Added implementation of member function IsLibEnabledInConfig
 * of class  CMsvcSite.
 *
 * Revision 1.7  2004/02/06 23:14:59  gorelenk
 * Implemented support of ASN projects, semi-auto configure,
 * CPPFLAGS support. Second working version.
 *
 * Revision 1.6  2004/02/05 16:31:19  gorelenk
 * Added definition of function GetComponents.
 *
 * Revision 1.5  2004/02/05 15:29:06  gorelenk
 * + Configuration information provision.
 *
 * Revision 1.4  2004/02/05 00:02:09  gorelenk
 * Added support of user site and  Makefile defines.
 *
 * Revision 1.3  2004/02/03 17:16:43  gorelenk
 * Removed implementation of method IsExplicitExclude from class CMsvcSite.
 *
 * Revision 1.2  2004/01/28 17:55:49  gorelenk
 * += For msvc makefile support of :
 *                 Requires tag, ExcludeProject tag,
 *                 AddToProject section (SourceFiles and IncludeDirs),
 *                 CustomBuild section.
 * += For support of user local site.
 *
 * Revision 1.1  2004/01/26 19:27:29  gorelenk
 * += MSVC meta makefile support
 * += MSVC project makefile support
 *
 * ===========================================================================
 */
