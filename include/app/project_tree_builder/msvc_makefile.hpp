#ifndef PROJECT_TREE_BULDER__MSVC_MAKEFILE__HPP
#define PROJECT_TREE_BULDER__MSVC_MAKEFILE__HPP
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
#include <app/project_tree_builder/ptb_registry.hpp>
#include <app/project_tree_builder/proj_item.hpp>
#include <app/project_tree_builder/msvc_prj_utils.hpp>
#include <corelib/ncbiobj.hpp>

#include <corelib/ncbienv.hpp>
BEGIN_NCBI_SCOPE

/// 
/// Interface of master msvc makefile
class IMsvcMetaMakefile
{
public:
    virtual string GetCompilerOpt (const string&       opt, 
                                   const SConfigInfo&  config)   const = 0;

    virtual string GetLinkerOpt   (const string&       opt, 
                                   const SConfigInfo&  config)   const = 0;

    virtual string GetLibrarianOpt(const string&       opt, 
                                   const SConfigInfo&  config)   const = 0;
    
    virtual string GetResourceCompilerOpt 
                                  (const string&       opt, 
                                   const SConfigInfo&  config)   const = 0;

};

///
/// Interface of msvc project makefile
class IMsvcProjectMakefile
{
public:
    virtual bool IsExcludeProject (bool default_val)             const = 0;

    virtual void GetAdditionalSourceFiles
                                  (const SConfigInfo& config, 
                                   list<string>*      files)     const = 0;

    virtual void GetAdditionalLIB (const SConfigInfo& config, 
                                   list<string>*      lib_ids)   const = 0;

    virtual void GetExcludedSourceFiles  
                                  (const SConfigInfo& config, 
                                   list<string>*      files)     const = 0;

    virtual void GetExcludedLIB   (const SConfigInfo& config, 
                                   list<string>*      lib_ids)   const = 0;
    
    virtual void GetAdditionalIncludeDirs
                                  (const SConfigInfo& config, 
                                   list<string>*      files)     const = 0;

    virtual void GetHeadersInInclude
                                  (const SConfigInfo& config, 
                                   list<string>*      files)     const = 0;

    virtual void GetHeadersInSrc
                                  (const SConfigInfo& config, 
                                   list<string>*      files)     const = 0;

    virtual void GetInlinesInInclude
                                  (const SConfigInfo& config, 
                                   list<string>*      files)     const = 0;

    virtual void GetInlinesInSrc
                                  (const SConfigInfo& config, 
                                   list<string>*      files)     const = 0;

    virtual void GetCustomBuildInfo
                                  (list<SCustomBuildInfo>* info) const = 0;

    virtual void GetResourceFiles (const SConfigInfo& config, 
                                  list<string>*      files)      const = 0;

};


/////////////////////////////////////////////////////////////////////////////
///
/// CMsvcMetaMakefile --
///
/// Abstraction of global settings for building of C++ projects.
///
/// Provides information about default compiler, linker and librarian 
/// settinngs.



class CMsvcMetaMakefile : public CObject,
                          public IMsvcMetaMakefile
{
public:
    CMsvcMetaMakefile(const string& file_path);
    
   
    bool IsEmpty                  (void) const;


    // IMsvcMetaMakefile
    virtual string GetCompilerOpt (const string&       opt, 
                                   const SConfigInfo&  config) const;

    virtual string GetLinkerOpt   (const string&       opt, 
                                   const SConfigInfo&  config) const;

    virtual string GetLibrarianOpt(const string&       opt, 
                                   const SConfigInfo&  config) const;
    
    virtual string GetResourceCompilerOpt 
                                  (const string&       opt, 
                                   const SConfigInfo&  config) const;


    bool   IsPchEnabled           (void) const;
    string GetUsePchThroughHeader (const string& project_id,
                                   const string& source_file_full_path,
                                   const string& tree_src_dir) const;
    string GetPchUsageDefine      (void) const;


protected:
    CPtbRegistry m_MakeFile;
    string       m_MakeFileBaseDir;
    string       m_FilePath;

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

class CMsvcProjectMakefile : public CMsvcMetaMakefile,
                             public IMsvcProjectMakefile
{
public:
    CMsvcProjectMakefile(const string& file_path);

    string GetGUID(void) const;
    bool Redefine(const string& value, list<string>& redef) const;
    bool Redefine(const list<string>& value, list<string>& redef) const;
    void Append( list<string>& values, const string& def) const;
    void Append( list<string>& values, const list<string>& def) const;

    // IMsvcProjectMakefile
    virtual bool IsExcludeProject        (bool default_val) const;

    virtual void GetAdditionalSourceFiles(const SConfigInfo& config, 
                                          list<string>*      files) const;

    virtual void GetAdditionalLIB        (const SConfigInfo& config, 
                                          list<string>*      lib_ids) const;

    virtual void GetExcludedSourceFiles  (const SConfigInfo& config, 
                                          list<string>*      files) const;

    virtual void GetExcludedLIB          (const SConfigInfo& config, 
                                          list<string>*      lib_ids) const;
    
    virtual void GetAdditionalIncludeDirs(const SConfigInfo& config, 
                                          list<string>*      files) const;

    virtual void GetHeadersInInclude     (const SConfigInfo& config, 
                                          list<string>*      files) const;

    virtual void GetHeadersInSrc         (const SConfigInfo& config, 
                                          list<string>*      files) const;

    virtual void GetInlinesInInclude     (const SConfigInfo& config, 
                                          list<string>*      files) const;

    virtual void GetInlinesInSrc         (const SConfigInfo& config, 
                                          list<string>*      files) const;

    virtual void GetCustomBuildInfo      (list<SCustomBuildInfo>* info) const;

    virtual void GetResourceFiles        (const SConfigInfo& config, 
                                          list<string>*      files) const;


    string m_ProjectBaseDir;

private:
    void x_GetHeaders(const SConfigInfo& config, const string& entry,
                      list<string>* files) const;
    
    //Prohibited to
    CMsvcProjectMakefile(void);
    CMsvcProjectMakefile(const CMsvcProjectMakefile&);
    CMsvcProjectMakefile& operator= (const CMsvcProjectMakefile&);
};


///
/// Abstraction of rule for generation of project settings 
/// based on component usage

class CMsvcProjectRuleMakefile : public CMsvcProjectMakefile
{
public:
    CMsvcProjectRuleMakefile(const string& file_path);

    int GetRulePriority(const SConfigInfo& config) const; 

private:
    //Prohibited to
    CMsvcProjectRuleMakefile(void);
    CMsvcProjectRuleMakefile(const CMsvcProjectRuleMakefile&);
    CMsvcProjectRuleMakefile& operator= (const CMsvcProjectRuleMakefile&);
};


///
/// Combining of rules and project makefile
class CMsvcCombinedProjectMakefile : public IMsvcMetaMakefile,
                                     public IMsvcProjectMakefile
{
public:
    CMsvcCombinedProjectMakefile(CProjItem::TProjType        project_type,
                                 const CMsvcProjectMakefile* project_makefile,
                                 const string&               rules_basedir,
                                 const list<string>          requires_list);

    virtual ~CMsvcCombinedProjectMakefile(void);
    
    // IMsvcMetaMakefile
    virtual string GetCompilerOpt (const string&       opt, 
                                   const SConfigInfo&  config) const;

    virtual string GetLinkerOpt   (const string&       opt, 
                                   const SConfigInfo&  config) const;

    virtual string GetLibrarianOpt(const string&       opt, 
                                   const SConfigInfo&  config) const;
    
    virtual string GetResourceCompilerOpt 
                                  (const string&       opt, 
                                   const SConfigInfo&  config) const;

    // IMsvcProjectMakefile
    virtual bool IsExcludeProject        (bool default_val) const;

    virtual void GetAdditionalSourceFiles(const SConfigInfo& config, 
                                          list<string>*      files) const;

    virtual void GetAdditionalLIB        (const SConfigInfo& config, 
                                          list<string>*      lib_ids) const;

    virtual void GetExcludedSourceFiles  (const SConfigInfo& config, 
                                          list<string>*      files) const;

    virtual void GetExcludedLIB          (const SConfigInfo& config, 
                                          list<string>*      lib_ids) const;
    
    virtual void GetAdditionalIncludeDirs(const SConfigInfo& config, 
                                          list<string>*      files) const;

    virtual void GetHeadersInInclude     (const SConfigInfo& config, 
                                          list<string>*      files) const;

    virtual void GetHeadersInSrc         (const SConfigInfo& config, 
                                          list<string>*      files) const;

    virtual void GetInlinesInInclude     (const SConfigInfo& config, 
                                          list<string>*      files) const;

    virtual void GetInlinesInSrc         (const SConfigInfo& config, 
                                          list<string>*      files) const;

    virtual void GetCustomBuildInfo      (list<SCustomBuildInfo>* info) const;

    virtual void GetResourceFiles        (const SConfigInfo& config, 
                                          list<string>*      files) const;

private:
    typedef const CMsvcProjectMakefile*    TProjectMakefile;
    TProjectMakefile                       m_ProjectMakefile;

    typedef CRef<CMsvcProjectRuleMakefile> TRule;
    typedef list<TRule>                    TRules;
    TRules                                 m_Rules;


    //Prohibited to
    CMsvcCombinedProjectMakefile(void);
    CMsvcCombinedProjectMakefile(const CMsvcCombinedProjectMakefile&);
    CMsvcCombinedProjectMakefile& 
        operator= (const CMsvcCombinedProjectMakefile&);
};



/// Create project makefile name
string CreateMsvcProjectMakefileName(const string&        project_name,
                                     CProjItem::TProjType type);
string CreateMsvcProjectMakefileName(const CProjItem& project);



/// Get option with taking into account 2 makefiles : matafile and project_file

/// Compiler
string GetCompilerOpt        (const IMsvcMetaMakefile& meta_file,
                              const IMsvcMetaMakefile& project_file,
                              const string&            opt,
                              const SConfigInfo&       config);

/// Linker
string GetLinkerOpt          (const IMsvcMetaMakefile& meta_file, 
                              const IMsvcMetaMakefile& project_file,
                              const string&            opt,
                              const SConfigInfo&       config);

/// Librarian
string GetLibrarianOpt       (const IMsvcMetaMakefile& meta_file, 
                              const IMsvcMetaMakefile& project_file,
                              const string&            opt,
                              const SConfigInfo&       config);

/// ResourceCompiler
string GetResourceCompilerOpt(const IMsvcMetaMakefile& meta_file, 
                              const IMsvcMetaMakefile& project_file,
                              const string&            opt,
                              const SConfigInfo&       config);

END_NCBI_SCOPE

#endif //PROJECT_TREE_BULDER__MSVC_MAKEFILE__HPP
