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
        //and remember dir from where it was loaded
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

    int max_match = 0;
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
        if ( !val.empty() )
            m_PchInfo->m_PchUsageMap[key] = val;
    }

    string do_not_use_pch_str = 
        m_MakeFile.GetString("UsePch", "DoNotUsePch", "");
    NStr::Split(do_not_use_pch_str, LIST_SEPARATOR, m_PchInfo->m_DontUsePchList);

    return *m_PchInfo;
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
            m_MakeFile.GetString(source_file, "CommandLine", "");
        build_info.m_Description = 
            m_MakeFile.GetString(source_file, "Description", "");
        build_info.m_Outputs = 
            m_MakeFile.GetString(source_file, "Outputs", "");
        build_info.m_AdditionalDependencies = 
            m_MakeFile.GetString(source_file, "AdditionalDependencies", "");

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
string GetCompilerOpt(const CMsvcMetaMakefile&    meta_file, 
                      const CMsvcProjectMakefile& project_file,
                      const string&               opt,
                      const SConfigInfo&          config)
{
   string val = project_file.GetCompilerOpt(opt, config);
   if ( !val.empty() )
       return val;
   
   return meta_file.GetCompilerOpt(opt, config);
}


string GetLinkerOpt(const CMsvcMetaMakefile&    meta_file, 
                    const CMsvcProjectMakefile& project_file,
                    const string&               opt,
                    const SConfigInfo&          config)
{
   string val = project_file.GetLinkerOpt(opt, config);
   if ( !val.empty() )
       return val;
   
   return meta_file.GetLinkerOpt(opt, config);
}


string GetLibrarianOpt(const CMsvcMetaMakefile&    meta_file, 
                       const CMsvcProjectMakefile& project_file,
                       const string&               opt,
                       const SConfigInfo&          config)
{
   string val = project_file.GetLibrarianOpt(opt, config);
   if ( !val.empty() )
       return val;
   
   return meta_file.GetLibrarianOpt(opt, config);
}

string GetResourceCompilerOpt(const CMsvcMetaMakefile&    meta_file, 
                              const CMsvcProjectMakefile& project_file,
                              const string&               opt,
                              const SConfigInfo&          config)
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
