#ifndef MSVC_MAKEFILE_HEADER
#define MSVC_MAKEFILE_HEADER
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
#include <corelib/ncbireg.hpp>
#include <app/project_tree_builder/proj_item.hpp>
#include <app/project_tree_builder/msvc_prj_utils.hpp>

#include <corelib/ncbienv.hpp>
BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
///
/// CMsvcMetaMakefile --
///
/// Abstraction of global settings for building of C++ projects.
///
/// Provides information about default compiler, linker and librarian 
/// settinngs.

class CMsvcMetaMakefile
{
public:
    CMsvcMetaMakefile(const string& file_path);
    
    bool IsEmpty(void) const;


    string GetCompilerOpt         (const string&       opt, 
                                   const SConfigInfo&  config) const;

    string GetLinkerOpt           (const string&       opt, 
                                   const SConfigInfo&  config) const;

    string GetLibrarianOpt        (const string&       opt, 
                                   const SConfigInfo&  config) const;
    
    string GetResourceCompilerOpt (const string&       opt, 
                                   const SConfigInfo&  config) const;


    bool   IsPchEnabled           (void) const;
    string GetUsePchThroughHeader (const string& project_id,
                                   const string& source_file_full_path,
                                   const string& tree_src_dir) const;
    string GetPchUsageDefine      (void) const;

protected:
    CNcbiRegistry m_MakeFile;
    string        m_MakeFileBaseDir;

    struct SPchInfo
    {
        bool m_UsePch;

        typedef map<string, string> TSubdirPchfile;
        TSubdirPchfile m_PchUsageMap;

        typedef list<string> TDontUsePch;
        TDontUsePch    m_DontUsePchList;

        string m_PchUsageDefine;
    };
    const SPchInfo& GetPchInfo(void) const;

private:
    auto_ptr<SPchInfo> m_PchInfo;

    CMsvcMetaMakefile(const CMsvcMetaMakefile&);
    CMsvcMetaMakefile& operator= (const CMsvcMetaMakefile&);
};


/////////////////////////////////////////////////////////////////////////////
///
/// CMsvcProjectMakefile --
///
/// Abstraction of project MSVC specific settings
///
/// Provides information about project include/exclude to build, 
/// additional/excluded files and project specific compiler, linker 
/// and librarian settinngs.

class CMsvcProjectMakefile : public CMsvcMetaMakefile
{
public:
    CMsvcProjectMakefile(const string& file_path);

    bool IsExcludeProject(bool default_val) const;

    void GetAdditionalSourceFiles(const SConfigInfo& config, 
                                  list<string>*      files) const;

    void GetAdditionalLIB        (const SConfigInfo& config, 
                                  list<string>*      lib_ids) const;

    void GetExcludedSourceFiles  (const SConfigInfo& config, 
                                  list<string>*      files) const;

    void GetExcludedLIB          (const SConfigInfo& config, 
                                  list<string>*      lib_ids) const;
    
    void GetAdditionalIncludeDirs(const SConfigInfo& config, 
                                  list<string>*      files) const;

    void GetCustomBuildInfo(list<SCustomBuildInfo>* info) const;

    void GetResourceFiles        (const SConfigInfo& config, 
                                  list<string>*      files) const;

private:
    
    //Prohibited to
    CMsvcProjectMakefile(void);
    CMsvcProjectMakefile(const CMsvcProjectMakefile&);
    CMsvcProjectMakefile& operator= (const CMsvcProjectMakefile&);
};

/// Create project makefile name
string CreateMsvcProjectMakefileName(const string&        project_name,
                                     CProjItem::TProjType type);
string CreateMsvcProjectMakefileName(const CProjItem& project);



/// Get option with taking into account 2 makefiles : matafile and project_file

/// Compiler
string GetCompilerOpt (const CMsvcMetaMakefile&    meta_file, 
                       const CMsvcProjectMakefile& project_file,
                       const string&               opt,
                       const SConfigInfo&          config);

/// Linker
string GetLinkerOpt   (const CMsvcMetaMakefile&    meta_file, 
                       const CMsvcProjectMakefile& project_file,
                       const string&               opt,
                       const SConfigInfo&          config);

/// Librarian
string GetLibrarianOpt(const CMsvcMetaMakefile&    meta_file, 
                       const CMsvcProjectMakefile& project_file,
                       const string&               opt,
                       const SConfigInfo&          config);

/// ResourceCompiler
string GetResourceCompilerOpt(const CMsvcMetaMakefile&    meta_file, 
                              const CMsvcProjectMakefile& project_file,
                              const string&               opt,
                              const SConfigInfo&          config);

END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.8  2004/05/17 14:33:53  gorelenk
 * Added declaration of GetPchUsageDefine to class CMsvcMetaMakefile.
 *
 * Revision 1.7  2004/05/10 14:24:44  gorelenk
 * + IsPchEnabled and GetUsePchThroughHeader.
 *
 * Revision 1.6  2004/02/23 20:43:42  gorelenk
 * Added support of MSVC ResourceCompiler tool.
 *
 * Revision 1.5  2004/02/23 18:49:24  gorelenk
 * Added declaration of GetResourceFiles member-function
 * to class CMsvcProjectMakefile.
 *
 * Revision 1.4  2004/02/12 16:22:39  gorelenk
 * Changed generation of command line for custom build info.
 *
 * Revision 1.3  2004/02/10 18:02:01  gorelenk
 * + GetAdditionalLIB.
 * + GetExcludedLIB - customization of depends.
 *
 * Revision 1.2  2004/01/28 17:55:04  gorelenk
 * += For msvc makefile support of :
 *                 Requires tag, ExcludeProject tag,
 *                 AddToProject section (SourceFiles and IncludeDirs),
 *                 CustomBuild section.
 * += For support of user local site.
 *
 * Revision 1.1  2004/01/26 19:25:40  gorelenk
 * += MSVC meta makefile support
 * += MSVC project makefile support
 *
 * ===========================================================================
 */

#endif // MSVC_MAKEFILE_HEADER