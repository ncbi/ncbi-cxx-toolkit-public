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
#include "msvc_site.hpp"
#include "proj_builder_app.hpp"
#include "proj_tree_builder.hpp"
#include "msvc_prj_defines.hpp"
#include "ptb_err_codes.hpp"

#include <algorithm>

#include <corelib/ncbistr.hpp>

BEGIN_NCBI_SCOPE

CMsvcSite::TDirectoryExistenceMap CMsvcSite::sm_DirExists;


//-----------------------------------------------------------------------------
CMsvcSite::CMsvcSite(const string& reg_path)
{
    m_RegPath = reg_path;
    CNcbiIfstream istr(reg_path.c_str(), IOS_BASE::in | IOS_BASE::binary);
    m_Registry.Read(istr);

    string str;

    if (CMsvc7RegSettings::GetMsvcPlatform() != CMsvc7RegSettings::eUnix) {
        // MSWin
        // Provided requests
        str = x_GetConfigureEntry("ProvidedRequests");
        list<string> provided;
        NStr::Split(str, LIST_SEPARATOR, provided);
        ITERATE (list<string>, it, provided) {
            m_ProvidedThing.insert(*it);
        }
        if (GetApp().GetBuildType().GetType() == CBuildType::eDll) {
            m_ProvidedThing.insert("DLL");
        }

        GetStandardFeatures(provided);
        ITERATE (list<string>, it, provided) {
            m_ProvidedThing.insert(*it);
        }

        // Not provided requests
        str = x_GetConfigureEntry("NotProvidedRequests");
        list<string> not_provided;
        NStr::Split(str, LIST_SEPARATOR, not_provided);
        ITERATE (list<string>, it, not_provided) {
            m_NotProvidedThing.insert(*it);
        }

        string unix_cfg = m_Registry.Get("ProjectTree","MetaData");
        if (!unix_cfg.empty()) {
            string fileloc;
	        fileloc = CDirEntry::ConcatPath( CDirEntry::ConcatPath(
	            GetApp().m_Root,
	                GetApp().GetConfig().Get("ProjectTree", "src")),
	                    CDirEntry::ConvertToOSPath(unix_cfg));
            if (!CFile(fileloc).Exists() && !GetApp().m_ExtSrcRoot.empty()) {
	            fileloc = CDirEntry::ConcatPath( CDirEntry::ConcatPath(
	                GetApp().m_ExtSrcRoot,
	                    GetApp().GetConfig().Get("ProjectTree", "src")),
	                        CDirEntry::ConvertToOSPath(unix_cfg));
            }
            if (CFile(fileloc).Exists()) {
                m_UnixMakeDef.SetValueSeparator(LIST_SEPARATOR_LIBS);
                CSimpleMakeFileContents::LoadFrom(fileloc,&m_UnixMakeDef);
            }
        }
    } else {
        // unix
        string unix_cfg = m_Registry.Get(CMsvc7RegSettings::GetMsvcSection(),"MetaData");
        if (!unix_cfg.empty()) {
            string fileloc(CDirEntry::ConcatPath( CDirEntry(GetApp().m_Solution).GetDir(),unix_cfg));
            if (!CFile(fileloc).Exists()) {
                if (!GetApp().m_BuildRoot.empty()) {
                    fileloc = CDirEntry::ConcatPath(GetApp().m_BuildRoot,unix_cfg);
                }
            }
            if (CFile(fileloc).Exists()) {
                m_UnixMakeDef.SetValueSeparator(LIST_SEPARATOR_LIBS);
                CSimpleMakeFileContents::LoadFrom(fileloc,&m_UnixMakeDef);
            }
        }
    
        CDir status_dir(GetApp().m_StatusDir);
        CDir::TEntries files = status_dir.GetEntries("*.enabled");
        ITERATE(CDir::TEntries, f, files) {
            string name = (*f)->GetBase();
            if (name[0] == '-') {
                name = name.substr(1);
                m_NotProvidedThing.insert(name);
            } else {
                m_ProvidedThing.insert(name);
            }
        }
    }
}

void CMsvcSite::InitializeLibChoices(void)
{
    string str;

    str = x_GetConfigureEntry("ComponentChoices");
    list<string> comp_choices;
    NStr::Split(str, LIST_SEPARATOR, comp_choices);

    if (CMsvc7RegSettings::GetMsvcPlatform() != CMsvc7RegSettings::eUnix) {
        // special cases
        ITERATE(list<string>, p, comp_choices) {
            const string& choice_str = *p;
            string lib_id;
            string lib_3party_id;
            if ( NStr::SplitInTwo(choice_str, "/", lib_id, lib_3party_id) ) {
                if (IsProvided(lib_3party_id))
                {
                    m_NotProvidedThing.insert(lib_id);
                }
            } else {
               PTB_ERROR_EX(m_RegPath, ePTB_ConfigurationError,
                            "ComponentChoices: " << choice_str);
            }
        }
    } else {
        // unix
        // special cases
        ITERATE(list<string>, p, comp_choices) {
            const string& choice_str = *p;
            string lib_id;
            string lib_3party_id;
            if ( NStr::SplitInTwo(choice_str, "/", lib_id, lib_3party_id) ) {
                if (IsProvided(lib_id) && IsProvided(lib_3party_id))
                {
                    m_NotProvidedThing.insert(lib_3party_id);
                }
            } else {
               PTB_ERROR_EX(m_RegPath, ePTB_ConfigurationError,
                            "ComponentChoices: " << choice_str);
            }
        }
    }

    // Lib choices
    str = x_GetConfigureEntry("LibChoices");
    list<string> lib_choices_list;
    NStr::Split(str, LIST_SEPARATOR, lib_choices_list);
    ITERATE(list<string>, p, lib_choices_list) {
        const string& choice_str = *p;
        string lib_id;
        string lib_3party_id;
        if ( NStr::SplitInTwo(choice_str, "/", lib_id, lib_3party_id) ) {
            m_LibChoices.push_back(SLibChoice(*this, lib_id, lib_3party_id));
        } else {
           PTB_ERROR_EX(m_RegPath, ePTB_ConfigurationError,
                        "Invalid LibChoices definition: " << choice_str);
        }
    }
}

bool CMsvcSite::IsProvided(const string& thing, bool deep) const
{
    if (thing.empty()) {
        return true;
    }
    if (thing[0] == '-') {
        return !IsProvided( thing.c_str() + 1, deep);
    }
    if ( m_NotProvidedThing.find(thing) != m_NotProvidedThing.end() ) {
        return false;
    }
    if ( m_ProvidedThing.find(thing) != m_ProvidedThing.end() ) {
        return true;
    }
    if (!deep) {
        string section("__EnabledUserRequests");
        if (GetApp().m_CustomConfiguration.DoesValueContain(
                section, thing, false)) {
            return true;
        }
        bool env = g_GetConfigFlag(section.c_str(), thing.c_str(), NULL, false);
        if (env) {
            string value;
            GetApp().m_CustomConfiguration.GetValue(section, value);
            if (!value.empty()) {
                value += " ";
            }
            value += thing;            
            GetApp().m_CustomConfiguration.AddDefinition(section, value);
            return true;
        }
        return false;
    }

    bool res = 
        CMsvc7RegSettings::GetMsvcPlatform() != CMsvc7RegSettings::eUnix ?
            IsDescribed(thing) : false;
    if ( res) {
        list<string> components;
        GetComponents(thing, &components);
        if (components.empty()) {
            components.push_back(thing);
        }
        // in at least one configuration all components must be ok
        ITERATE(list<SConfigInfo>, config , GetApp().GetRegSettings().m_ConfigInfo) {
            res = true;
            ITERATE(list<string>, p, components) {
                const string& component = *p;
                SLibInfo lib_info;
                GetLibInfo(component, *config, &lib_info);
                res = IsLibOk(lib_info);
                if ( !res ) {
                    break;
                }
            }
            if (res) {
                break;
            }
        }
    }
    return res;
}

bool CMsvcSite::IsBanned(const string& thing) const
{
    return !thing.empty() &&
        m_NotProvidedThing.find(thing) != m_NotProvidedThing.end();
}

bool CMsvcSite::IsDescribed(const string& section) const
{
    return m_Registry.HasEntry(section) ||
           m_Registry.HasEntry(section + "." +
                CMsvc7RegSettings::GetMsvcRegSection());
}


void CMsvcSite::GetComponents(const string& entry, 
                              list<string>* components) const
{
    components->clear();
    NStr::Split(m_Registry.Get(entry, "Component"), " ,\t", *components);
}

string CMsvcSite::ProcessMacros(string raw_data, bool preserve_unresolved) const
{
    string data(raw_data), raw_macro, macro, definition;
    string::size_type start, end, done = 0;
    while ((start = data.find("$(", done)) != string::npos) {
        end = data.find(")", start);
        if (end == string::npos) {
            PTB_WARNING_EX("", ePTB_ConfigurationError,
                           "Malformatted macro definition: " + raw_data);
            return data;
        }
        raw_macro = data.substr(start,end-start+1);
        if (CSymResolver::IsDefine(raw_macro)) {
            macro = CSymResolver::StripDefine(raw_macro);
            if (macro == "incdir") {
                definition = GetApp().m_IncDir;
            } else if (macro == "rootdir") {
                definition = GetApp().GetProjectTreeInfo().m_Root;
            } else {
                if (!GetApp().m_CustomConfiguration.GetValue(macro,definition)) {
                    definition = x_GetConfigureEntry(macro);
                }
            }
            if (definition.empty() && preserve_unresolved) {
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
    string libinfokey(lib + config.GetConfigFullName());
    map<string,SLibInfo>::const_iterator li;
    li = m_AllLibInfo.find(libinfokey);
    if (li != m_AllLibInfo.end()) {
        *libinfo = li->second;
        return;
    }

    string section(lib);
    if (CMsvc7RegSettings::GetMsvcPlatform() >= CMsvc7RegSettings::eUnix) {
        section += '.';
        section += CMsvc7RegSettings::GetMsvcRegSection();
        if (!IsDescribed(section)) {
            section = lib;
        }
    }
    libinfo->Clear();
    libinfo->valid = IsDescribed(section);
    if (!libinfo->valid) {
        libinfo->valid = IsProvided(lib, false);
    } else {
        string include_str    = ToOSPath(
            ProcessMacros(GetOpt(m_Registry, section, "INCLUDE", config),false));
        NStr::Split(include_str, LIST_SEPARATOR, libinfo->m_IncludeDir);

        string defines_str    = GetOpt(m_Registry, section, "DEFINES", config);
        NStr::Split(defines_str, LIST_SEPARATOR, libinfo->m_LibDefines);

        libinfo->m_LibPath    = ToOSPath(
            ProcessMacros(GetOpt(m_Registry, section, "LIBPATH", config),false));

        string libs_str = GetOpt(m_Registry, section, "LIB", config);
        NStr::Split(libs_str, LIST_SEPARATOR, libinfo->m_Libs);

        libs_str = GetOpt(m_Registry, section, "STDLIB", config);
        NStr::Split(libs_str, LIST_SEPARATOR, libinfo->m_StdLibs);

        string macro_str = GetOpt(m_Registry, section, "MACRO", config);
        NStr::Split(macro_str, LIST_SEPARATOR, libinfo->m_Macro);

        string files_str    = ProcessMacros(GetOpt(m_Registry, section, "FILES", config),false);
        list<string> tmp;
        NStr::Split(files_str, "|", tmp);
        ITERATE( list<string>, f, tmp) {
            libinfo->m_Files.push_back( ToOSPath(*f));
        }
    }

    libinfo->m_libinfokey = libinfokey;
    libinfo->m_good = IsLibOk(*libinfo);
//    m_AllLibInfo.insert( make_pair<string,SLibInfo>(libinfokey,*libinfo));
    m_AllLibInfo.insert( map<string,SLibInfo>::value_type(libinfokey,*libinfo));
//    m_AllLibInfo[ libinfokey ] = *libinfo;
}


bool CMsvcSite::IsLibEnabledInConfig(const string&      lib, 
                                     const SConfigInfo& config) const
{
    string section(lib);
    if (CMsvc7RegSettings::GetMsvcPlatform() >= CMsvc7RegSettings::eUnix) {
        section += '.';
        section += CMsvc7RegSettings::GetMsvcRegSection();
    }
    if (!m_Registry.HasEntry(section)) {
        return true;
    }
    string enabled_configs_str = m_Registry.Get(section, "CONFS");
    if (enabled_configs_str.empty()) {
        return true;
    }
    list<string> enabled_configs;
    NStr::Split(enabled_configs_str, 
                LIST_SEPARATOR, enabled_configs);

    return find(enabled_configs.begin(), 
                enabled_configs.end(), 
                config.m_Name) != enabled_configs.end();
}


bool CMsvcSite::ResolveDefine(const string& define, string& resolved) const
{
    if (CMsvc7RegSettings::GetMsvcPlatform() == CMsvc7RegSettings::eUnix) {
        if (m_UnixMakeDef.GetValue(define,resolved)) {
            return true;
        }
    }
//    resolved = m_Registry.Get("Defines", define);
    resolved = x_GetDefinesEntry(define);
    if (resolved.empty()) {
        if (m_Registry.HasEntry("Defines", define)) {
            return true;
        }
        if (CMsvc7RegSettings::GetMsvcPlatform() != CMsvc7RegSettings::eUnix) {
            if (m_UnixMakeDef.GetValue(define,resolved)) {
                list<string> lst, res;
                NStr::Split(resolved, LIST_SEPARATOR_LIBS, lst);
                ITERATE(list<string>, l, lst) {
                    if (SMakeProjectT::IsConfigurableDefine(*l)) {
                        resolved = x_GetDefinesEntry(SMakeProjectT::StripConfigurableDefine(*l));
                        if (!resolved.empty()) {
                            res.push_back(resolved);
                            continue;
                        }
                    }
                    res.push_back(*l);
                }
                resolved = NStr::Join(res, " ");
                return true;
            }
        }
        return false;
    }
    resolved = ProcessMacros(resolved);
    return true;
}


string CMsvcSite::GetConfigureDefinesPath(void) const
{
    return x_GetConfigureEntry("DefinesPath");
}


void CMsvcSite::GetConfigureDefines(list<string>* defines) const
{
    defines->clear();
    NStr::Split(x_GetConfigureEntry("Defines"), LIST_SEPARATOR,
                *defines);
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

bool CMsvcSite::Is3PartyLib(const string& lib_id) const
{
    ITERATE(list<SLibChoice>, p, m_LibChoices) {
        const SLibChoice& choice = *p;
        if (lib_id == choice.m_LibId) {
            return choice.m_Choice == e3PartyLib;
        }
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
    if (CMsvc7RegSettings::GetMsvcPlatform() != CMsvc7RegSettings::eUnix) {
        m_Choice = e3PartyLib;
        // special case: lzo is always 3rd party lib
        if (lib == "lzo") {
            return;
        }
        ITERATE(list<SConfigInfo>, p, GetApp().GetRegSettings().m_ConfigInfo) {
            const SConfigInfo& config = *p;
            SLibInfo lib_info;
            site.GetLibInfo(m_3PartyLib, config, &lib_info);

            if ( !site.IsLibOk(lib_info) ) {

                m_Choice = eLib;
                break;
            }
        }
    } else {
        m_Choice = site.IsProvided(lib_3party) ? e3PartyLib : eLib;
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
            if (GetApp().GetBuildType().GetType() == CBuildType::eDll) {
                return choice.m_Choice;
            } else {
                SLibInfo lib_info;
                GetLibInfo(lib3party_id, cfg_info, &lib_info);
                return IsLibOk(lib_info,true) ? e3PartyLib : eLib;
            }
        }
    }
    return eUnknown;
}


void CMsvcSite::GetLibChoiceIncludes(
    const string& cpp_flags_define, list<string>* abs_includes) const
{
    abs_includes->clear();

    string include_str = m_Registry.Get("LibChoicesIncludes", 
                                               cpp_flags_define);
    if (!include_str.empty()) {
        abs_includes->push_back("$(" + cpp_flags_define + ")");
    }
}

void CMsvcSite::GetLibChoiceIncludes(
    const string& cpp_flags_define, const SConfigInfo& cfg_info,
    list<string>* abs_includes) const
{
    abs_includes->clear();
    string include_str = m_Registry.Get("LibChoicesIncludes", 
                                               cpp_flags_define);
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
            bool b3;
            if (GetApp().GetBuildType().GetType() == CBuildType::eDll) {
                b3 = choice.m_Choice == e3PartyLib;
                if (lib_id == "lzo") {
                    b3 = IsLibOk(lib_info, true);
                }
            } else {
                b3 = IsLibOk(lib_info, true);
            }
            if (b3) {
                copy(lib_info.m_IncludeDir.begin(), 
                    lib_info.m_IncludeDir.end(), back_inserter(*abs_includes));
            } else {
                const string& rel_include_path = *p;
                if (*p != ".") {
                    string abs_include_path = 
                        GetApp().GetProjectTreeInfo().m_Include;
                    abs_include_path = 
                        CDirEntry::ConcatPath(abs_include_path, rel_include_path);
                    abs_include_path = CDirEntry::NormalizePath(abs_include_path);
                    abs_includes->push_back(abs_include_path);
                }
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
            LOG_POST(Warning << lib_id << "|" << cfg_info.GetConfigFullName()
                          << " unavailable: library include ignored: "
                          << NStr::Join(lib_info.m_IncludeDir,";"));
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
    return m_Registry.Get("DefaultResource", "app");
}


void CMsvcSite::GetThirdPartyLibsToInstall(list<string>* libs) const
{
    libs->clear();

    string libs_str = x_GetConfigureEntry("ThirdPartyLibsToInstall");
    NStr::Split(libs_str, LIST_SEPARATOR, *libs);
}


string CMsvcSite::GetThirdPartyLibsBinPathSuffix(void) const
{
    return x_GetConfigureEntry("ThirdPartyLibsBinPathSuffix");
}

string CMsvcSite::GetThirdPartyLibsBinSubDir(void) const
{
    return ToOSPath(x_GetConfigureEntry("ThirdPartyLibsBinSubDir"));
}

void   CMsvcSite::SetThirdPartyLibBin(const string& lib, const string& bin)
{
    m_ThirdPartyLibBin[lib] = bin;
}

string CMsvcSite::GetThirdPartyLibBin(const string& lib) const
{
    if (m_ThirdPartyLibBin.find(lib) != m_ThirdPartyLibBin.end()) {
        return m_ThirdPartyLibBin.find(lib)->second;
    }
    return kEmptyStr;
}

void CMsvcSite::GetStandardFeatures(list<string>& features) const
{
    features.clear();
    NStr::Split(x_GetConfigureEntry("StandardFeatures"),
                LIST_SEPARATOR, features);
}

//-----------------------------------------------------------------------------
bool CMsvcSite::x_DirExists(const string& dir_name)
{
    TDirectoryExistenceMap::iterator it = sm_DirExists.find(dir_name);
    if (it == sm_DirExists.end()) {
        bool exists = CDirEntry(dir_name).Exists();
        it = sm_DirExists.insert
            (TDirectoryExistenceMap::value_type(dir_name, exists)).first;
    }
    return it->second;
}

string CMsvcSite::GetConfigureEntry(const string& entry) const
{
    return ProcessMacros(x_GetConfigureEntry(entry));
}
string CMsvcSite::GetDefinesEntry(const string& entry) const
{
    return ProcessMacros(x_GetDefinesEntry(entry));
}

string CMsvcSite::x_GetConfigureEntry(const string& entry) const
{
    string str;
    str = m_Registry.Get( CMsvc7RegSettings::GetMsvcSection(), entry);
    if (str.empty()) {
        str = m_Registry.Get( CMsvc7RegSettings::GetMsvcRegSection(), entry);
        if (str.empty()) {
            str = m_Registry.Get("Configure", entry);
        }
    }
    return str;
}

string CMsvcSite::x_GetDefinesEntry(const string& entry) const
{
    string str;
    str = m_Registry.Get( CMsvc7RegSettings::GetMsvcSection(), entry);
    if (str.empty()) {
        str = m_Registry.Get( CMsvc7RegSettings::GetMsvcRegSection(), entry);
        if (str.empty()) {
            str = m_Registry.Get("Defines", entry);
        }
    }
    return str;
}

string CMsvcSite::GetPlatformInfo(const string& sysname,
    const string& type, const string& orig) const
{
    string section("PlatformSynonyms_");
    section += sysname;
    string str = m_Registry.Get( section, type);
    string result(orig), syn, res;
    if (!str.empty() && NStr::SplitInTwo(str, ":", syn, res)) {
        list< string > entries;
        NStr::Split(syn, LIST_SEPARATOR, entries);
        if (find(entries.begin(), entries.end(), orig) != entries.end()) {
            result = res;
        }
    }
    return result;
}

bool CMsvcSite::IsCppflagDescribed(const string& raw_value) const
{
    if (NStr::StartsWith(raw_value, "-I")) {
        return true;
    }
    if (!CSymResolver::IsDefine(raw_value)) {
        return false;
    }
    string stripped = CSymResolver::StripDefine(FilterDefine(raw_value));
    string tmp = m_Registry.Get("LibChoicesIncludes", stripped);
    if (!tmp.empty()) {
        return true;
    }
    tmp = x_GetDefinesEntry(stripped);
    if (!tmp.empty()) {
        return true;
    }
    return false;
}


bool CMsvcSite::IsLibOk(const SLibInfo& lib_info, bool silent) const
{
    map<string,SLibInfo>::const_iterator li = m_AllLibInfo.find(lib_info.m_libinfokey);
    if (li != m_AllLibInfo.end()) {
        return lib_info.m_good;
    }
    silent = false;

    if ( !lib_info.valid /*|| lib_info.IsEmpty()*/ )
        return false;
#ifndef PSEUDO_XCODE
    if ( !lib_info.m_IncludeDir.empty() ) {
        ITERATE(list<string>, i, lib_info.m_IncludeDir) {
            if (!x_DirExists(*i) ) {
                if (!silent) {
                    PTB_WARNING_EX(*i, ePTB_PathNotFound,
                                   "INCLUDE path not found");
                }
                return false;
            }
        }
    }
#endif
    if ( !lib_info.m_LibPath.empty() &&
         !x_DirExists(lib_info.m_LibPath) ) {
        if (!silent) {
            PTB_WARNING_EX(lib_info.m_LibPath, ePTB_PathNotFound,
                           "LIB path not found");
        }
        return false;
    }
    if ( !lib_info.m_LibPath.empty()) {
        if (CMsvc7RegSettings::GetMsvcPlatform() >= CMsvc7RegSettings::eUnix) {
            ITERATE(list<string>, p, lib_info.m_Libs) {
                string lib = *p;
                if (NStr::StartsWith(lib, "-l")) {
                    NStr::ReplaceInPlace(lib, "-l", "lib");
                    string lib_path_abs = CDirEntry::ConcatPath(lib_info.m_LibPath, lib);
                    if ( !lib_path_abs.empty() &&
                        !x_DirExists(lib_path_abs+".a") &&
                        !x_DirExists(lib_path_abs+".dylib") ) {
                        if (!silent) {
                            PTB_WARNING_EX(lib_path_abs, ePTB_PathNotFound,
                                        "LIB path not found");
                        }
                        return false;
                    }
                    
                }
            }
        } else {
            ITERATE(list<string>, p, lib_info.m_Libs) {
                const string& lib = *p;
                string lib_path_abs = CDirEntry::ConcatPath(lib_info.m_LibPath, lib);
                if ( !lib_path_abs.empty() &&
                    !x_DirExists(lib_path_abs) ) {
                    if (!silent) {
                        PTB_WARNING_EX(lib_path_abs, ePTB_PathNotFound,
                                    "LIB path not found");
                    }
                    return false;
                }
            }
        }
    }
    if ( !lib_info.m_Files.empty()) {
        bool group_exists = false;
        ITERATE(list<string>, g, lib_info.m_Files) {
            list<string> tmp;
            NStr::Split(*g, LIST_SEPARATOR, tmp);
            bool file_exists = true;
            ITERATE( list<string>, p, tmp) {
                string file = *p;
                if (!CDirEntry::IsAbsolutePath(file)) {
                    file = CDirEntry::ConcatPath(GetApp().GetProjectTreeInfo().m_Root, file);
                }
                if ( !x_DirExists(file) ) {
                    file_exists = false;
                    if (!GetApp().GetExtSrcRoot().empty()) {
                        file = *p;
                        if (!CDirEntry::IsAbsolutePath(file)) {
                            file = CDirEntry::ConcatPath(GetApp().GetExtSrcRoot(), file);
                        }
                        file_exists = x_DirExists(file);
                    }
                }
                if (!file_exists) {
                    if (!silent) {
                        PTB_WARNING_EX(file, ePTB_FileNotFound,
                                       "file not found");
                    }
                    break;
                }
            }
            group_exists = group_exists || file_exists;
        }
        if (!group_exists) {
            return false;
        }
    }

    return true;
}

void CMsvcSite::ProcessMacros(const list<SConfigInfo>& configs)
{
    list<string> macros;
    NStr::Split(x_GetConfigureEntry("Macros"), LIST_SEPARATOR, macros);

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
            if (CMsvc7RegSettings::GetMsvcPlatform() != CMsvc7RegSettings::eUnix) {
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
                                << component << "|" << config.GetConfigFullName());
                        }
//                      res = false;
//                      break;
                    }
                }
            } else {
                res = IsProvided(component);
                if (!res) {
                    break;
                }
            }
        }
        if (res) {
            m_Macros.AddDefinition(macro, m_Registry.Get(macro, "Value"));
        } else {
            m_Macros.AddDefinition(macro, m_Registry.Get(macro, "DefValue"));
        }
    }
}

string CMsvcSite::ToOSPath(const string& path)
{
    string xpath(path);
    char separator = CDirEntry::GetPathSeparator();
#ifdef PSEUDO_XCODE
    separator = '/';
#endif
    for (size_t i = 0; i < xpath.length(); i++) {
        char c = xpath[i];
        if ( (c == '\\' || c == '/') && c != separator) {
            xpath[i] = separator;
        }
    }
    return xpath;
}


END_NCBI_SCOPE
