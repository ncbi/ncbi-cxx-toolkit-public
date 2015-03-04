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
#include "msvc_makefile.hpp"
#include "proj_builder_app.hpp"
#include "msvc_prj_defines.hpp"

#include <algorithm>

#include <corelib/ncbistr.hpp>

BEGIN_NCBI_SCOPE

//-----------------------------------------------------------------------------
CMsvcMetaMakefile::CMsvcMetaMakefile(const string& file_path)
{
#if defined(NCBI_COMPILER_MSVC) || defined(NCBI_XCODE_BUILD) || defined(PSEUDO_XCODE)
    if (CFile(file_path).Exists()) {
        CNcbiIfstream ifs(file_path.c_str(), IOS_BASE::in | IOS_BASE::binary);
        //read registry
        m_MakeFile.Read(ifs);
        //and remember dir from where it has been loaded
        CDirEntry::SplitPath(file_path, &m_MakeFileBaseDir);
//        LOG_POST(Info << "Using rules from " << file_path);
    }
#endif //NCBI_COMPILER_MSVC
}


bool CMsvcMetaMakefile::IsEmpty(void) const
{
    return m_MakeFile.Empty();
}

string CMsvcMetaMakefile::TranslateOpt(
    const string& value, const string& section, const string& opt)
{
    if (value.empty() ||
        (CMsvc7RegSettings::GetMsvcVersion() < CMsvc7RegSettings::eMsvc1000 ||
        CMsvc7RegSettings::GetMsvcVersion() >= CMsvc7RegSettings::eXCode30) ) {
        return value;
    }
    string name(section+"_"+opt+"_"+value);
    return GetApp().GetMetaMakefile().m_MakeFile.GetString(
        "Translate", name, value);
}

string CMsvcMetaMakefile::TranslateCommand(const string& value)
{
    if (value.empty() ||
        (CMsvc7RegSettings::GetMsvcVersion() < CMsvc7RegSettings::eMsvc1000 ||
        CMsvc7RegSettings::GetMsvcVersion() >= CMsvc7RegSettings::eXCode30) ) {
        return value;
    }
    const CMsvcMetaMakefile& meta = GetApp().GetMetaMakefile();

    string data(value), raw_macro, macro, definition;
    string::size_type start, end, done = 0;
    for (;;) {
        if ((start = data.find("$(", done)) == string::npos) {
            break;
        }
        end = data.find(")", start);
        if (end == string::npos) {
            break;
        }
        raw_macro = data.substr(start,end-start+1);
        if (CSymResolver::IsDefine(raw_macro)) {
            macro = CSymResolver::StripDefine(raw_macro);
            definition = meta.m_MakeFile.Get("Translate", string("Macro_") + macro);
            if (definition.empty()) {
                done = end;
            } else {
                data = NStr::Replace(data, raw_macro, definition);
                done = 0;
            } 
        }
    }
    data = NStr::Replace(data, "@echo", "%40echo");
    return data;
}

string CMsvcMetaMakefile::GetConfigurationOpt(const string& opt, 
                                         const SConfigInfo& config) const
{
    string sec("Configuration");
    return TranslateOpt( GetOpt(m_MakeFile, sec, opt, config), sec, opt);
}

string CMsvcMetaMakefile::GetCompilerOpt(const string& opt, 
                                         const SConfigInfo& config) const
{
    string sec("Compiler");
    return TranslateOpt( GetOpt(m_MakeFile, sec, opt, config), sec, opt);
}


string CMsvcMetaMakefile::GetLinkerOpt(const string& opt, 
                                       const SConfigInfo& config) const
{
    string sec("Linker");
    return TranslateOpt( GetOpt(m_MakeFile, sec, opt, config), sec, opt);
}


string CMsvcMetaMakefile::GetLibrarianOpt(const string& opt, 
                                          const SConfigInfo& config) const
{
    string sec("Librarian");
    return TranslateOpt( GetOpt(m_MakeFile, sec, opt, config), sec, opt);
}


string CMsvcMetaMakefile::GetResourceCompilerOpt
                          (const string& opt, const SConfigInfo& config) const
{
    string sec("ResourceCompiler");
    return TranslateOpt( GetOpt(m_MakeFile, sec, opt, config), sec, opt);
}

string CMsvcMetaMakefile::GetConfigOpt(
    const string& section, const string& opt, const SConfigInfo& config) const
{
    return GetOpt(m_MakeFile, section, opt, config);
}


bool CMsvcMetaMakefile::IsPchEnabled(void) const
{
    return GetPchInfo().m_UsePch;
}


string CMsvcMetaMakefile::GetUsePchThroughHeader 
                          (const string& project_id,
                           const string& source_file_full_path,
                           const string& tree_src_dir) const
{
    const SPchInfo& pch_info = GetPchInfo();

    if (find(pch_info.m_DontUsePchList.begin(),
             pch_info.m_DontUsePchList.end(),
             project_id) != pch_info.m_DontUsePchList.end()) {
        return kEmptyStr;
    }

    string source_file_dir;
    CDirEntry::SplitPath(source_file_full_path, &source_file_dir);
    source_file_dir = CDirEntry::AddTrailingPathSeparator(source_file_dir);

    size_t max_match = 0;
    string pch_file;
    bool found = false;
    ITERATE(SPchInfo::TSubdirPchfile, p, pch_info.m_PchUsageMap) {
        const string& branch_subdir = p->first;
        string abs_branch_subdir = 
            CDirEntry::ConcatPath(tree_src_dir, branch_subdir);
        abs_branch_subdir = 
            CDirEntry::AddTrailingPathSeparator(abs_branch_subdir);
        if ( IsSubdir(abs_branch_subdir, source_file_dir) ) {
            if ( branch_subdir.length() > max_match ) {
                max_match = branch_subdir.length();
                pch_file  = p->second;
                found = true;
            }
        }
    }
    if (found) {
        return pch_file;
    }
    return m_PchInfo->m_DefaultPch;
}


const CMsvcMetaMakefile::SPchInfo& CMsvcMetaMakefile::GetPchInfo(void) const
{
    if ( m_PchInfo.get() )
        return *m_PchInfo;

    (const_cast<CMsvcMetaMakefile&>(*this)).m_PchInfo.reset(new SPchInfo);

    string use_pch_str          = m_MakeFile.GetString("UsePch", "UsePch", "TRUE");
    m_PchInfo->m_UsePch = NStr::StringToBool(use_pch_str);
    m_PchInfo->m_PchUsageDefine = m_MakeFile.GetString("UsePch", "PchUsageDefine");
    m_PchInfo->m_DefaultPch     = m_MakeFile.GetString("UsePch", "DefaultPch");
    string do_not_use_pch_str   = m_MakeFile.GetString("UsePch", "DoNotUsePch");
    NStr::Split(do_not_use_pch_str, LIST_SEPARATOR, m_PchInfo->m_DontUsePchList);
    string irrelevant[] = {"UsePch","PchUsageDefine","DefaultPch","DoNotUsePch",""};

    list<string> projects_with_pch_dirs;
    m_MakeFile.EnumerateEntries("UsePch", &projects_with_pch_dirs);
    ITERATE(list<string>, p, projects_with_pch_dirs) {
        const string& key = *p;
        bool ok = true;
        for (int i=0; ok && !irrelevant[i].empty(); ++i) {
            ok = key != irrelevant[i];
        }
        if (!ok)
            continue;

        string val = m_MakeFile.GetString("UsePch", key, "-");
        if ( val == "-" ) {
            val = "";
        }
        string tmp = CDirEntry::ConvertToOSPath(key);
        m_PchInfo->m_PchUsageMap[tmp] = val;
    }
    return *m_PchInfo;
}

string CMsvcMetaMakefile::GetPchUsageDefine(void) const
{
    return GetPchInfo().m_PchUsageDefine;
}
//-----------------------------------------------------------------------------
string CreateMsvcProjectMakefileName(const string&        project_name,
                                     CProjItem::TProjType type)
{
    string name("Makefile.");
    
    name += project_name + '.';
    
    switch (type) {
    case CProjKey::eApp:
        name += "app.";
        break;
    case CProjKey::eLib:
        name += "lib.";
        break;
    case CProjKey::eDll:
        name += "dll.";
        break;
    case CProjKey::eMsvc:
        if (CMsvc7RegSettings::GetMsvcPlatform() == CMsvc7RegSettings::eMsvcWin32 ||
            CMsvc7RegSettings::GetMsvcPlatform() == CMsvc7RegSettings::eMsvcX64) {
            name += "msvcproj.";
        }
        break;
    case CProjKey::eDataSpec:
        name += "dataspec.";
        break;
    case CProjKey::eUtility:
        name += "utility.";
        break;
    default:
        NCBI_THROW(CProjBulderAppException, 
                   eProjectType, 
                   NStr::IntToString(type));
        break;
    }
    name += GetApp().GetRegSettings().m_MakefilesExt;
    return name;
}


string CreateMsvcProjectMakefileName(const CProjItem& project)
{
    return CreateMsvcProjectMakefileName(project.m_Name, 
                                         project.m_ProjType);
}


//-----------------------------------------------------------------------------
CMsvcProjectMakefile::CMsvcProjectMakefile(const string& file_path, bool compound)
    :CMsvcMetaMakefile(file_path)
{
    CDirEntry::SplitPath(file_path, &m_ProjectBaseDir);
    m_FilePath = file_path;
    m_Compound = compound;
}


string CMsvcProjectMakefile::GetGUID(void) const
{
    return m_MakeFile.GetString("Common", "ProjectGUID");
}

bool CMsvcProjectMakefile::Redefine(const string& value, list<string>& redef) const
{
    redef.clear();
    if (IsEmpty()) {
        return false;
    }
    string::size_type start, end;
    if ((start = value.find("$(")) != string::npos && 
        (end   = value.find(")"))  != string::npos  && (end > start)) {
        string raw_define = value.substr(start+2,end-start-2);
        string new_val = m_MakeFile.GetString("Redefine", raw_define);
        if (!new_val.empty()) {
            redef.push_back("$(" + new_val + ")");
            _TRACE(m_FilePath << " redefines:  " << raw_define << " = " << new_val);
            return true;
        }
    } else if (NStr::StartsWith(value, "@") && NStr::EndsWith(value, "@")) {
        string raw_define = value.substr(1,value.length()-2);
        string new_val = m_MakeFile.GetString("Redefine", raw_define);
        if (!new_val.empty()) {
            redef.push_back("@" + new_val + "@");
            _TRACE(m_FilePath << " redefines:  " << raw_define << " = " << new_val);
            return true;
        }
    } else {
        string new_val = m_MakeFile.GetString("Redefine", value);
        if (!new_val.empty()) {
            redef.clear();
            NStr::Split(new_val, LIST_SEPARATOR, redef);
            _TRACE(m_FilePath << " redefines:  " << value << " = " << new_val);
            return true;
        }
    }
    return false;
}

bool CMsvcProjectMakefile::Redefine(const list<string>& value, list<string>& redef) const
{
    bool res=false;
    redef.clear();
    if (IsEmpty()) {
        redef.insert(redef.end(),value.begin(), value.end());
    } else {
        list<string> newval;
        ITERATE(list<string>, k, value) {
            if (Redefine(*k,newval)) {
                redef.insert(redef.end(),newval.begin(), newval.end());
                res=true;
            } else {
                redef.push_back(*k);
            }
        }
    }
    return res;
}

void CMsvcProjectMakefile::Append( list<string>& values, const string& def) const
{
    if (IsEmpty()) {
        values.push_back(def);
    } else {
        list<string> redef;
        if (Redefine(def,redef)) {
            values.insert(values.end(), redef.begin(), redef.end());
        } else {
            values.push_back(def);
        }
    }
}

void CMsvcProjectMakefile::Append( list<string>& values, const list<string>& def) const
{
    if (IsEmpty()) {
        values.insert(values.end(), def.begin(), def.end());
    } else {
        ITERATE(list<string>, k, def) {
            Append(values,*k);
        }
    }
}

bool CMsvcProjectMakefile::IsExcludeProject(bool default_val) const
{
    string val = m_MakeFile.GetString("Common", "ExcludeProject");

    if ( val.empty() )
        return default_val;

    return val != "FALSE";
}


void CMsvcProjectMakefile::GetAdditionalSourceFiles(const SConfigInfo& config,
                                                    list<string>* files) const
{
    string files_string = 
        GetOpt(m_MakeFile, "AddToProject", "SourceFiles", config);
    
    NStr::Split(files_string, LIST_SEPARATOR, *files);
}


void CMsvcProjectMakefile::GetAdditionalLIB(const SConfigInfo& config, 
                                            list<string>*      lib_ids) const
{
    string lib_string = 
        GetOpt(m_MakeFile, "AddToProject", "LIB", config);
    
    NStr::Split(lib_string, LIST_SEPARATOR, *lib_ids);
}


void CMsvcProjectMakefile::GetExcludedSourceFiles(const SConfigInfo& config,  
                                                  list<string>* files) const
{
    string files_string = 
        GetOpt(m_MakeFile, 
               "ExcludedFromProject", "SourceFiles", config);
    
    NStr::Split(files_string, LIST_SEPARATOR, *files);
}


void CMsvcProjectMakefile::GetExcludedLIB(const SConfigInfo& config, 
                                          list<string>*      lib_ids) const
{
    string lib_string = 
        GetOpt(m_MakeFile, 
               "ExcludedFromProject", "LIB", config);
    
    NStr::Split(lib_string, LIST_SEPARATOR, *lib_ids);
}


void CMsvcProjectMakefile::GetAdditionalIncludeDirs(const SConfigInfo& config,  
                                                    list<string>* dirs) const
{
    string dirs_string = 
        GetOpt(m_MakeFile, "AddToProject", "IncludeDirs", config);
    
    NStr::Split(dirs_string, LIST_SEPARATOR, *dirs);
}

void CMsvcProjectMakefile::GetHeadersInInclude(const SConfigInfo& config, 
                                               list<string>*  files) const
{
    x_GetHeaders(config, "HeadersInInclude", files);
}

void CMsvcProjectMakefile::GetHeadersInSrc(const SConfigInfo& config, 
                                           list<string>*  files) const
{
    x_GetHeaders(config, "HeadersInSrc", files);
}

void CMsvcProjectMakefile::x_GetHeaders(
    const SConfigInfo& config, const string& entry, list<string>* files) const
{
    string dirs_string =  GetOpt(m_MakeFile, "AddToProject", entry, config);
    string separator;
    separator += CDirEntry::GetPathSeparator();
    dirs_string = NStr::Replace(dirs_string,"/",separator);
    dirs_string = NStr::Replace(dirs_string,"\\",separator);
    
    files->clear();
    NStr::Split(dirs_string, LIST_SEPARATOR, *files);
    if (files->empty() && !m_Compound) {
        files->push_back("*.h");
        files->push_back("*.hpp");
    }
}

void CMsvcProjectMakefile::GetInlinesInInclude(const SConfigInfo& , 
                                               list<string>*  files) const
{
    files->clear();
    files->push_back("*.inl");
}

void CMsvcProjectMakefile::GetInlinesInSrc(const SConfigInfo& , 
                                           list<string>*  files) const
{
    files->clear();
    files->push_back("*.inl");
}

void 
CMsvcProjectMakefile::GetCustomBuildInfo(list<SCustomBuildInfo>* info) const
{
    info->clear();

    string source_files_str = 
        m_MakeFile.GetString("CustomBuild", "SourceFiles");
    if (source_files_str.empty()) {
        return;
    }
    
    list<string> source_files;
    NStr::Split(source_files_str, LIST_SEPARATOR, source_files);

    ITERATE(list<string>, p, source_files){
        const string& source_file = *p;
        
        SCustomBuildInfo build_info;
        string source_file_path_abs = 
            CDirEntry::ConcatPath(m_MakeFileBaseDir, source_file);
        build_info.m_SourceFile = 
            CDirEntry::NormalizePath(source_file_path_abs);
        build_info.m_CommandLine =
            GetApp().GetSite().ProcessMacros(
                m_MakeFile.GetString(source_file, "CommandLine"));
        build_info.m_Description = 
            m_MakeFile.GetString(source_file, "Description");
        build_info.m_Outputs = 
            m_MakeFile.GetString(source_file, "Outputs");
        build_info.m_AdditionalDependencies = 
            GetApp().GetSite().ProcessMacros(
                m_MakeFile.GetString(source_file, "AdditionalDependencies"));

        if ( !build_info.IsEmpty() )
            info->push_back(build_info);
    }
}

void
CMsvcProjectMakefile::GetCustomScriptInfo(SCustomScriptInfo& info) const
{
    string sec("CustomScript");
    info.m_Input  = m_MakeFile.GetString(sec, "Input");
    info.m_Output = m_MakeFile.GetString(sec, "Output");
    info.m_Shell  = m_MakeFile.GetString(sec, "Shell");
    info.m_Script = m_MakeFile.GetString(sec, "Script");
}


void CMsvcProjectMakefile::GetResourceFiles(const SConfigInfo& config, 
                                            list<string>*      files) const
{
    string files_string = 
        GetOpt(m_MakeFile, "AddToProject", "ResourceFiles", config);
    
    NStr::Split(files_string, LIST_SEPARATOR, *files);
}

void CMsvcProjectMakefile::GetExtraFiles(map<string, list<string> >*  files_map) const
{
    string prefix("ExtraFileGroup.");
    list<string> sections;
    m_MakeFile.EnumerateSections(&sections);
    ITERATE(list<string>, s, sections) {
        if (NStr::StartsWith(*s,prefix)) {
            string section(*s);
            string group_name = NStr::Replace(s->substr(prefix.size()),"_"," ");
            string files_string = m_MakeFile.Get(section, "Files");
            list<string> raw_files, files;
            NStr::Split(files_string, LIST_SEPARATOR, raw_files);
            string fname;
            bool started = false;
            ITERATE(list<string>, f, raw_files) {
                string part(*f);
                if (part[0] == '\"' && !started) {
                    fname = part.substr(1);
                    started = true;
                    continue;
                }
                else if (part[part.size()-1] == '\"') {
                    fname += ' ';
                    fname += part.substr(0,part.size()-1);
                }
                else if (started) {
                    fname += ' ';
                    fname += part;
                    continue;
                }
                else {
                    fname = part;
                }
                files.push_back(fname);
                fname.clear();
                started = false;
            }
            if (!group_name.empty() && !files.empty()) {
                (*files_map)[group_name] = files;
            }
        }
    }
}

//-----------------------------------------------------------------------------
CMsvcProjectRuleMakefile::CMsvcProjectRuleMakefile(const string& file_path, bool compound)
    :CMsvcProjectMakefile(file_path, compound)
{
}


int CMsvcProjectRuleMakefile::GetRulePriority(const SConfigInfo& config) const
{
    string priority_string = 
        GetOpt(m_MakeFile, "Rule", "Priority", config);
    
    if ( priority_string.empty() )
        return 0;

    return NStr::StringToInt(priority_string);
}


//-----------------------------------------------------------------------------
static string s_CreateRuleMakefileFilename(CProjItem::TProjType project_type,
                                           const string& requires)
{
    string name = "Makefile." + requires;
    switch (project_type) {
    case CProjKey::eApp:
        name += ".app";
        break;
    case CProjKey::eLib:
        name += ".lib";
        break;
    case CProjKey::eDll:
        name += ".dll";
        break;
    default:
        break;
    }
    return name + "." + GetApp().GetRegSettings().m_MakefilesExt;
}

CMsvcCombinedProjectMakefile::CMsvcCombinedProjectMakefile
                              (CProjItem::TProjType        project_type,
                               const CMsvcProjectMakefile* project_makefile,
                               const string&               rules_basedir,
                               const list<string>          requires_list)
    :m_ProjectMakefile(project_makefile)
{
    ITERATE(list<string>, p, requires_list) {
        const string& requires = *p;
        string rule_path = rules_basedir;
        rule_path = 
            CDirEntry::ConcatPath(rule_path, 
                                  s_CreateRuleMakefileFilename(project_type, 
                                                               requires));
        
        TRule rule(new CMsvcProjectRuleMakefile(rule_path, project_type== CProjKey::eDll));
        if ( !rule->IsEmpty() )
            m_Rules.push_back(rule);
    }
}


CMsvcCombinedProjectMakefile::~CMsvcCombinedProjectMakefile(void)
{
}

#define IMPLEMENT_COMBINED_MAKEFILE_OPT(X)  \
string CMsvcCombinedProjectMakefile::X(const string&       opt,               \
                                         const SConfigInfo&  config) const    \
{                                                                             \
    string prj_val = m_ProjectMakefile->X(opt, config);                       \
    if ( !prj_val.empty() )                                                   \
        return prj_val;                                                       \
    string val;                                                               \
    int priority = 0;                                                         \
    ITERATE(TRules, p, m_Rules) {                                             \
        const TRule& rule = *p;                                               \
        string rule_val = rule->X(opt, config);                               \
        if ( !rule_val.empty() && priority < rule->GetRulePriority(config)) { \
            val      = rule_val;                                              \
            priority = rule->GetRulePriority(config);                         \
        }                                                                     \
    }                                                                         \
    return val;                                                               \
}                                                                          


IMPLEMENT_COMBINED_MAKEFILE_OPT(GetConfigurationOpt)
IMPLEMENT_COMBINED_MAKEFILE_OPT(GetCompilerOpt)
IMPLEMENT_COMBINED_MAKEFILE_OPT(GetLinkerOpt)
IMPLEMENT_COMBINED_MAKEFILE_OPT(GetLibrarianOpt)
IMPLEMENT_COMBINED_MAKEFILE_OPT(GetResourceCompilerOpt)

bool CMsvcCombinedProjectMakefile::IsExcludeProject(bool default_val) const
{
    return m_ProjectMakefile->IsExcludeProject(default_val);
}


static void s_ConvertRelativePaths(const string&       rule_base_dir,
                                   const list<string>& rules_paths_list,
                                   const string&       project_base_dir,
                                   list<string>*       project_paths_list)
{
    project_paths_list->clear();
    ITERATE(list<string>, p, rules_paths_list) {
        const string& rules_path = *p;
        string rules_abs_path = 
            CDirEntry::ConcatPath(rule_base_dir, rules_path);
        string project_path = 
            CDirEntry::CreateRelativePath(project_base_dir, rules_abs_path);
        project_paths_list->push_back(project_path);
    }
}


#define IMPLEMENT_COMBINED_MAKEFILE_VALUES(X)  \
void CMsvcCombinedProjectMakefile::X(const SConfigInfo& config,               \
                                       list<string>*      values_list) const  \
{                                                                             \
    list<string> prj_val;                                                     \
    m_ProjectMakefile->X(config, &prj_val);                                   \
    if ( !prj_val.empty() ) {                                                 \
        *values_list = prj_val;                                               \
        return;                                                               \
    }                                                                         \
    list<string> val;                                                         \
    int priority = 0;                                                         \
    ITERATE(TRules, p, m_Rules) {                                             \
        const TRule& rule = *p;                                               \
        list<string> rule_val;                                                \
        rule->X(config, &rule_val);                                           \
        if ( !rule_val.empty() && priority < rule->GetRulePriority(config)) { \
            val      = rule_val;                                              \
            priority = rule->GetRulePriority(config);                         \
        }                                                                     \
    }                                                                         \
    *values_list = val;                                                       \
}


#define IMPLEMENT_COMBINED_MAKEFILE_FILESLIST(X)  \
void CMsvcCombinedProjectMakefile::X(const SConfigInfo& config,               \
                                       list<string>*      values_list) const  \
{                                                                             \
    list<string> prj_val;                                                     \
    m_ProjectMakefile->X(config, &prj_val);                                   \
    if ( !prj_val.empty() ) {                                                 \
        *values_list = prj_val;                                               \
        return;                                                               \
    }                                                                         \
    list<string> val;                                                         \
    int priority = 0;                                                         \
    string rule_base_dir;                                                     \
    ITERATE(TRules, p, m_Rules) {                                             \
        const TRule& rule = *p;                                               \
        list<string> rule_val;                                                \
        rule->X(config, &rule_val);                                           \
        if ( !rule_val.empty() && priority < rule->GetRulePriority(config)) { \
            val      = rule_val;                                              \
            priority = rule->GetRulePriority(config);                         \
            rule_base_dir = rule->m_ProjectBaseDir;                           \
        }                                                                     \
    }                                                                         \
    s_ConvertRelativePaths(rule_base_dir,                                     \
                           val,                                               \
                           m_ProjectMakefile->m_ProjectBaseDir,               \
                           values_list);                                      \
}


IMPLEMENT_COMBINED_MAKEFILE_FILESLIST(GetAdditionalSourceFiles)                                                                          
IMPLEMENT_COMBINED_MAKEFILE_VALUES   (GetAdditionalLIB)
IMPLEMENT_COMBINED_MAKEFILE_FILESLIST(GetExcludedSourceFiles)
IMPLEMENT_COMBINED_MAKEFILE_VALUES   (GetExcludedLIB)
IMPLEMENT_COMBINED_MAKEFILE_FILESLIST(GetAdditionalIncludeDirs)
IMPLEMENT_COMBINED_MAKEFILE_FILESLIST(GetHeadersInInclude)
IMPLEMENT_COMBINED_MAKEFILE_FILESLIST(GetHeadersInSrc)
IMPLEMENT_COMBINED_MAKEFILE_FILESLIST(GetInlinesInInclude)
IMPLEMENT_COMBINED_MAKEFILE_FILESLIST(GetInlinesInSrc)
IMPLEMENT_COMBINED_MAKEFILE_FILESLIST(GetResourceFiles)

void CMsvcCombinedProjectMakefile::GetExtraFiles(map<string, list<string> >*  files) const
{
   m_ProjectMakefile->GetExtraFiles(files);                                   \
 }

void CMsvcCombinedProjectMakefile::GetCustomBuildInfo
                                           (list<SCustomBuildInfo>* info) const
{
    m_ProjectMakefile->GetCustomBuildInfo(info);
}

void CMsvcCombinedProjectMakefile::GetCustomScriptInfo
                                           (SCustomScriptInfo& info) const
{
    m_ProjectMakefile->GetCustomScriptInfo(info);
}


//-----------------------------------------------------------------------------
string GetConfigurationOpt(const IMsvcMetaMakefile&    meta_file, 
                      const IMsvcMetaMakefile& project_file,
                      const string&               opt,
                      const SConfigInfo&          config)
{
    string val = project_file.GetConfigurationOpt(opt, config);
    if ( val.empty() ) {
        val = meta_file.GetConfigurationOpt(opt, config);
    }
    if (val == "-") {
        return kEmptyStr;
    }
    return val;
}

string GetCompilerOpt(const IMsvcMetaMakefile&    meta_file, 
                      const IMsvcMetaMakefile& project_file,
                      const string&               opt,
                      const SConfigInfo&          config)
{
    string val = project_file.GetCompilerOpt(opt, config);
    if ( val.empty() && CMsvc7RegSettings::GetMsvcVersion() < CMsvc7RegSettings::eMsvc1000 ) {
        val = meta_file.GetCompilerOpt(opt, config);
    }
    if (val == "-") {
        return kEmptyStr;
    }
    return val;
}


string GetLinkerOpt(const IMsvcMetaMakefile& meta_file, 
                    const IMsvcMetaMakefile& project_file,
                    const string&            opt,
                    const SConfigInfo&       config)
{
    string val = project_file.GetLinkerOpt(opt, config);
    if ( val.empty() && CMsvc7RegSettings::GetMsvcVersion() < CMsvc7RegSettings::eMsvc1000 ) {
        val = meta_file.GetLinkerOpt(opt, config);
    }
    if (val == "-") {
        return kEmptyStr;
    }
    return val;
}


string GetLibrarianOpt(const IMsvcMetaMakefile& meta_file, 
                       const IMsvcMetaMakefile& project_file,
                       const string&            opt,
                       const SConfigInfo&       config)
{
    string val = project_file.GetLibrarianOpt(opt, config);
    if ( val.empty() && CMsvc7RegSettings::GetMsvcVersion() < CMsvc7RegSettings::eMsvc1000 ) {
        val = meta_file.GetLibrarianOpt(opt, config);
    }
    if (val == "-") {
        return kEmptyStr;
    }
    return val;
}

string GetResourceCompilerOpt(const IMsvcMetaMakefile& meta_file, 
                              const IMsvcMetaMakefile& project_file,
                              const string&            opt,
                              const SConfigInfo&       config)
{
    string val = project_file.GetResourceCompilerOpt(opt, config);
    if ( val.empty() && CMsvc7RegSettings::GetMsvcVersion() < CMsvc7RegSettings::eMsvc1000 ) {
        val = meta_file.GetResourceCompilerOpt(opt, config);
    }
    if (val == "-") {
        return kEmptyStr;
    }
    return val;
}

END_NCBI_SCOPE
