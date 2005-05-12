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
#include <app/project_tree_builder/msvc_makefile.hpp>
#include <app/project_tree_builder/proj_builder_app.hpp>
#include <app/project_tree_builder/msvc_prj_defines.hpp>

#include <algorithm>

#include <corelib/ncbistr.hpp>

BEGIN_NCBI_SCOPE

//-----------------------------------------------------------------------------
CMsvcMetaMakefile::CMsvcMetaMakefile(const string& file_path)
{
    CNcbiIfstream ifs(file_path.c_str(), IOS_BASE::in | IOS_BASE::binary);
    if (ifs) {
        //read registry
        m_MakeFile.Read(ifs);
        //and remember dir from where it has been loaded
        CDirEntry::SplitPath(file_path, &m_MakeFileBaseDir);
    }
}


bool CMsvcMetaMakefile::IsEmpty(void) const
{
    return m_MakeFile.Empty();
}


string CMsvcMetaMakefile::GetCompilerOpt(const string& opt, 
                                         const SConfigInfo& config) const
{
    return GetOpt(m_MakeFile, "Compiler", opt, config);
}


string CMsvcMetaMakefile::GetLinkerOpt(const string& opt, 
                                       const SConfigInfo& config) const
{
    return GetOpt(m_MakeFile, "Linker", opt, config);
}


string CMsvcMetaMakefile::GetLibrarianOpt(const string& opt, 
                                          const SConfigInfo& config) const
{
    return GetOpt(m_MakeFile, "Librarian", opt, config);
}


string CMsvcMetaMakefile::GetResourceCompilerOpt
                          (const string& opt, const SConfigInfo& config) const
{
    return GetOpt(m_MakeFile, "ResourceCompiler", opt, config);
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

    string source_file_dir;
    CDirEntry::SplitPath(source_file_full_path, &source_file_dir);
    source_file_dir = CDirEntry::AddTrailingPathSeparator(source_file_dir);

    size_t max_match = 0;
    string pch_file;
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
            }
        }
    }
    if ( pch_file.empty() )
        return "";
    
    if (find(pch_info.m_DontUsePchList.begin(),
             pch_info.m_DontUsePchList.end(),
             project_id) == pch_info.m_DontUsePchList.end())
        return pch_file;

    return "";
}


const CMsvcMetaMakefile::SPchInfo& CMsvcMetaMakefile::GetPchInfo(void) const
{
    if ( m_PchInfo.get() )
        return *m_PchInfo;

    (const_cast<CMsvcMetaMakefile&>(*this)).m_PchInfo.reset(new SPchInfo);

    string use_pch_str = m_MakeFile.GetString("UsePch", "UsePch", "");
    m_PchInfo->m_UsePch = (NStr::CompareNocase(use_pch_str, "TRUE") == 0);

    list<string> projects_with_pch_dirs;
    m_MakeFile.EnumerateEntries("UsePch", &projects_with_pch_dirs);
    ITERATE(list<string>, p, projects_with_pch_dirs) {
        const string& key = *p;
        if (key == "DoNotUsePch")
            continue;

        string val = m_MakeFile.GetString("UsePch", key, "");
        if ( !val.empty() ) {
            string tmp = CDirEntry::ConvertToOSPath(key);
            m_PchInfo->m_PchUsageMap[tmp] = val;
        }
    }

    string do_not_use_pch_str = 
        m_MakeFile.GetString("UsePch", "DoNotUsePch", "");
    NStr::Split(do_not_use_pch_str, LIST_SEPARATOR, m_PchInfo->m_DontUsePchList);

    m_PchInfo->m_PchUsageDefine = 
        m_MakeFile.GetString("UsePch", "PchUsageDefine", "");

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
        name += "msvcproj.";
        break;
    default:
        NCBI_THROW(CProjBulderAppException, 
                   eProjectType, 
                   NStr::IntToString(type));
        break;
    }

    name += "msvc";
    return name;
}


string CreateMsvcProjectMakefileName(const CProjItem& project)
{
    return CreateMsvcProjectMakefileName(project.m_Name, 
                                         project.m_ProjType);
}


//-----------------------------------------------------------------------------
CMsvcProjectMakefile::CMsvcProjectMakefile(const string& file_path)
    :CMsvcMetaMakefile(file_path)
{
    CDirEntry::SplitPath(file_path, &m_ProjectBaseDir);
}


bool CMsvcProjectMakefile::IsExcludeProject(bool default_val) const
{
    string val = m_MakeFile.GetString("Common", "ExcludeProject", "");

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
    if (files->empty()) {
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
        m_MakeFile.GetString("CustomBuild", "SourceFiles", "");
    
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
                m_MakeFile.GetString(source_file, "CommandLine", ""));
        build_info.m_Description = 
            m_MakeFile.GetString(source_file, "Description", "");
        build_info.m_Outputs = 
            m_MakeFile.GetString(source_file, "Outputs", "");
        build_info.m_AdditionalDependencies = 
            GetApp().GetSite().ProcessMacros(
                m_MakeFile.GetString(source_file, "AdditionalDependencies", ""));

        if ( !build_info.IsEmpty() )
            info->push_back(build_info);
    }
}


void CMsvcProjectMakefile::GetResourceFiles(const SConfigInfo& config, 
                                            list<string>*      files) const
{
    string files_string = 
        GetOpt(m_MakeFile, "AddToProject", "ResourceFiles", config);
    
    NStr::Split(files_string, LIST_SEPARATOR, *files);
}


//-----------------------------------------------------------------------------
CMsvcProjectRuleMakefile::CMsvcProjectRuleMakefile(const string& file_path)
    :CMsvcProjectMakefile(file_path)
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
    return name + ".msvc";
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
        
        TRule rule(new CMsvcProjectRuleMakefile(rule_path));
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


void CMsvcCombinedProjectMakefile::GetCustomBuildInfo
                                           (list<SCustomBuildInfo>* info) const
{
    m_ProjectMakefile->GetCustomBuildInfo(info);
}


//-----------------------------------------------------------------------------
string GetCompilerOpt(const IMsvcMetaMakefile&    meta_file, 
                      const IMsvcMetaMakefile& project_file,
                      const string&               opt,
                      const SConfigInfo&          config)
{
   string val = project_file.GetCompilerOpt(opt, config);
   if ( !val.empty() )
       return val;
   
   return meta_file.GetCompilerOpt(opt, config);
}


string GetLinkerOpt(const IMsvcMetaMakefile& meta_file, 
                    const IMsvcMetaMakefile& project_file,
                    const string&            opt,
                    const SConfigInfo&       config)
{
   string val = project_file.GetLinkerOpt(opt, config);
   if ( !val.empty() )
       return val;
   
   return meta_file.GetLinkerOpt(opt, config);
}


string GetLibrarianOpt(const IMsvcMetaMakefile& meta_file, 
                       const IMsvcMetaMakefile& project_file,
                       const string&            opt,
                       const SConfigInfo&       config)
{
   string val = project_file.GetLibrarianOpt(opt, config);
   if ( !val.empty() )
       return val;
   
   return meta_file.GetLibrarianOpt(opt, config);
}

string GetResourceCompilerOpt(const IMsvcMetaMakefile& meta_file, 
                              const IMsvcMetaMakefile& project_file,
                              const string&            opt,
                              const SConfigInfo&       config)
{
   string val = project_file.GetResourceCompilerOpt(opt, config);
   if ( !val.empty() )
       return val;
   
   return meta_file.GetResourceCompilerOpt(opt, config);
}

END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.19  2005/05/12 18:05:56  gouriano
 * Process macros in custom build info
 *
 * Revision 1.18  2004/12/20 15:30:24  gouriano
 * Typo fixed
 *
 * Revision 1.17  2004/10/12 13:27:35  gouriano
 * Added possibility to specify which headers to include into project
 *
 * Revision 1.16  2004/07/20 13:38:40  gouriano
 * Added conditional macro definition
 *
 * Revision 1.15  2004/06/10 15:16:46  gorelenk
 * Changed macrodefines to be comply with GCC 3.4.0 .
 *
 * Revision 1.14  2004/06/07 13:50:29  gorelenk
 * Added implementation of classes
 * CMsvcProjectRuleMakefile and CMsvcCombinedProjectMakefile.
 *
 * Revision 1.13  2004/05/21 21:41:41  gorelenk
 * Added PCH ncbi_pch.hpp
 *
 * Revision 1.12  2004/05/17 14:36:20  gorelenk
 * Implemented CMsvcMetaMakefile::GetPchUsageDefine .
 *
 * Revision 1.11  2004/05/10 19:52:52  gorelenk
 * Changed CreateMsvcProjectMakefileName.
 *
 * Revision 1.10  2004/05/10 14:27:04  gorelenk
 * Implemented IsPchEnabled and GetUsePchThroughHeader.
 *
 * Revision 1.9  2004/03/10 16:38:00  gorelenk
 * Added dll processing to function CreateMsvcProjectMakefileName.
 *
 * Revision 1.8  2004/02/23 20:42:57  gorelenk
 * Added support of MSVC ResourceCompiler tool.
 *
 * Revision 1.7  2004/02/23 18:50:31  gorelenk
 * Added implementation of GetResourceFiles member-function
 * of class CMsvcProjectMakefile.
 *
 * Revision 1.6  2004/02/20 22:53:25  gorelenk
 * Added analysis of ASN projects depends.
 *
 * Revision 1.5  2004/02/12 16:27:56  gorelenk
 * Changed generation of command line for datatool.
 *
 * Revision 1.4  2004/02/10 18:02:46  gorelenk
 * + GetAdditionalLIB.
 * + GetExcludedLIB - customization of depends.
 *
 * Revision 1.3  2004/02/06 23:14:59  gorelenk
 * Implemented support of ASN projects, semi-auto configure,
 * CPPFLAGS support. Second working version.
 *
 * Revision 1.2  2004/01/28 17:55:48  gorelenk
 * += For msvc makefile support of :
 *                 Requires tag, ExcludeProject tag,
 *                 AddToProject section (SourceFiles and IncludeDirs),
 *                 CustomBuild section.
 * += For support of user local site.
 *
 * Revision 1.1  2004/01/26 19:27:28  gorelenk
 * += MSVC meta makefile support
 * += MSVC project makefile support
 *
 * ===========================================================================
 */
