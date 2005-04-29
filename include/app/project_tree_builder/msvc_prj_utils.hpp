#ifndef PROJECT_TREE_BUILDER__MSVC_PRJ_UTILS__HPP
#define PROJECT_TREE_BUILDER__MSVC_PRJ_UTILS__HPP

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
#include <corelib/ncbienv.hpp>
#include <app/project_tree_builder/msvc71_project__.hpp>
#include <app/project_tree_builder/proj_item.hpp>
#include <set>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

/// Creates CVisualStudioProject class instance from file.
///
/// @param file_path
///   Path to file load from.
/// @return
///   Created on heap CVisualStudioProject instance or NULL
///   if failed.
CVisualStudioProject * LoadFromXmlFile(const string& file_path);


/// Save CVisualStudioProject class instance to file.
///
/// @param file_path
///   Path to file project will be saved to.
/// @param project
///   Project to save.
void SaveToXmlFile  (const string&               file_path, 
                     const CVisualStudioProject& project);

/// Save CVisualStudioProject class instance to file only if no such file 
//  or contents of this file will be different from already present file.
///
/// @param file_path
///   Path to file project will be saved to.
/// @param project
///   Project to save.
void SaveIfNewer    (const string&               file_path, 
                     const CVisualStudioProject& project);

/// Consider promotion candidate to present 
bool PromoteIfDifferent(const string& present_path, 
                        const string& candidate_path);


/// Generate pseudo-GUID.
string GenerateSlnGUID(void);

/// Get extension for source file without extension.
///
/// @param file_path
///   Source file full path withour extension.
/// @return
///   Extension of source file (".cpp" or ".c") 
///   if such file exist. Empty string string if there is no
///   such file.
string SourceFileExt(const string& file_path);


/////////////////////////////////////////////////////////////////////////////
///
/// SConfigInfo --
///
/// Abstraction of configuration informaion.
///
/// Configuration name, debug/release flag, runtime library 
/// 

struct SConfigInfo
{
    SConfigInfo(void);
    SConfigInfo(const string& name, 
                bool          debug, 
                const string& runtime_library);
    void DefineRtType();

    string m_Name;
    bool   m_Debug;
    string m_RuntimeLibrary;
    enum {
        rtMultiThreaded = 0,
        rtMultiThreadedDebug = 1,
        rtMultiThreadedDLL = 2,
        rtMultiThreadedDebugDLL = 3,
        rtSingleThreaded = 4,
        rtSingleThreadedDebug = 5,
        rtUnknown = 6
    } m_rtType;
};

// Helper to load configs from ini files
void LoadConfigInfoByNames(const CNcbiRegistry& registry, 
                           const list<string>&  config_names, 
                           list<SConfigInfo>*   configs);


/////////////////////////////////////////////////////////////////////////////
///
/// SCustomBuildInfo --
///
/// Abstraction of custom build source file.
///
/// Information for custom buil source file 
/// (not *.c, *.cpp, *.midl, *.rc, etc.)
/// MSVC does not know how to buil this file and
/// we provide information how to do it.
/// 

struct SCustomBuildInfo
{
    string m_SourceFile; // absolut path!
    string m_CommandLine;
    string m_Description;
    string m_Outputs;
    string m_AdditionalDependencies;

    bool IsEmpty(void) const
    {
        return m_SourceFile.empty() || m_CommandLine.empty();
    }
    void Clear(void)
    {
        m_SourceFile.erase();
        m_CommandLine.erase();
        m_Description.erase();
        m_Outputs.erase();
        m_AdditionalDependencies.erase();
    }
};


/////////////////////////////////////////////////////////////////////////////
///
/// CMsvc7RegSettings --
///
/// Abstraction of [msvc7] section in app registry.
///
/// Settings for generation of msvc 7.10 projects
/// 

class CMsvc7RegSettings
{
public:
    CMsvc7RegSettings(void);

    string            m_Version;
    list<SConfigInfo> m_ConfigInfo;
    string            m_CompilersSubdir;
    string            m_ProjectsSubdir;
    string            m_MakefilesExt;
    string            m_MetaMakefile;
    string            m_DllInfo;

private:
    CMsvc7RegSettings(const CMsvc7RegSettings&);
    CMsvc7RegSettings& operator= (const CMsvc7RegSettings&);
};


/// Is abs_dir a parent of abs_parent_dir.
bool IsSubdir(const string& abs_parent_dir, const string& abs_dir);


/// Erase if predicate is true
template <class C, class P> 
void EraseIf(C& cont, const P& pred)
{
    for (typename C::iterator p = cont.begin(); p != cont.end(); )
    {
        if ( pred(*p) ) {
            typename C::iterator p_next = p;
	    ++p_next;
            cont.erase(p);
	    p = p_next;
        }
        else
            ++p;
    }
}


/// Get option fron registry from  
///     [<section>.debug.<ConfigName>] section for debug configuratios
///  or [<section>.release.<ConfigName>] for release configurations
///
/// if no such option then try      
///     [<section>.debug]
/// or  [<section>.release]
///
/// if no such option then finally try
///     [<section>]
///
string GetOpt(const CNcbiRegistry& registry, 
              const string&        section, 
              const string&        opt, 
              const SConfigInfo&   config);

/// return <config>|Win32 as needed by MSVC compiler
string ConfigName(const string& config);



//-----------------------------------------------------------------------------

// Base interface class for all insertors
class IFilesToProjectInserter
{
public:
    virtual ~IFilesToProjectInserter(void)
    {
    }

    virtual void AddSourceFile (const string& rel_file_path) = 0;
    virtual void AddHeaderFile (const string& rel_file_path) = 0;
    virtual void AddInlineFile (const string& rel_file_path) = 0;

    virtual void Finalize      (void)                        = 0;
};


// Insert .cpp and .c files to filter and set PCH usage if necessary
class CSrcToFilterInserterWithPch
{
public:
    CSrcToFilterInserterWithPch(const string&            project_id,
                                const list<SConfigInfo>& configs,
                                const string&            project_dir);

    ~CSrcToFilterInserterWithPch(void);

    void operator() (CRef<CFilter>& filter, 
                     const string&  rel_source_file);

private:
    string            m_ProjectId;
    list<SConfigInfo> m_Configs;
    string            m_ProjectDir;

    typedef set<string> TPchHeaders;
    TPchHeaders m_PchHeaders;

    enum EUsePch {
        eNotUse = 0,
        eCreate = 1,
        eUse    = 3
    };
    typedef pair<EUsePch, string> TPch;

    TPch DefinePchUsage(const string&     project_dir,
                        const string&     rel_source_file);

    void InsertFile    (CRef<CFilter>&    filter, 
                        const string&     rel_source_file,
                        const string&     enable_cfg = kEmptyStr);

    // Prohibited to:
    CSrcToFilterInserterWithPch(void);
    CSrcToFilterInserterWithPch(const CSrcToFilterInserterWithPch&);
    CSrcToFilterInserterWithPch& operator=(const CSrcToFilterInserterWithPch&);
};


class CBasicProjectsFilesInserter : public IFilesToProjectInserter
{
public:
    CBasicProjectsFilesInserter(CVisualStudioProject*    vcproj,
                                const string&            project_id,
                                const list<SConfigInfo>& configs,
                                const string&            project_dir);

    virtual ~CBasicProjectsFilesInserter(void);

    // IFilesToProjectInserter implementation
    virtual void AddSourceFile (const string& rel_file_path);
    virtual void AddHeaderFile (const string& rel_file_path);
    virtual void AddInlineFile (const string& rel_file_path);

    virtual void Finalize      (void);

    struct SFiltersItem
    {
        SFiltersItem(void);
        SFiltersItem(const string& project_dir);

        CRef<CFilter> m_SourceFiles;
        CRef<CFilter> m_HeaderFiles;
        CRef<CFilter> m_HeaderFilesPrivate;
        CRef<CFilter> m_HeaderFilesImpl;
        CRef<CFilter> m_InlineFiles;
        
        string        m_ProjectDir;

        void Initilize(void);

        void AddSourceFile (CSrcToFilterInserterWithPch& inserter_w_pch,
                            const string&                rel_file_path);

        void AddHeaderFile (const string& rel_file_path);

        void AddInlineFile (const string& rel_file_path);

    };

private:
    CVisualStudioProject*       m_Vcproj;
    
    CSrcToFilterInserterWithPch m_SrcInserter;
    SFiltersItem                m_Filters;
    

    // Prohibited to:
    CBasicProjectsFilesInserter(void);
    CBasicProjectsFilesInserter(const CBasicProjectsFilesInserter&);
    CBasicProjectsFilesInserter& operator=(const CBasicProjectsFilesInserter&);
};

class CDllProjectFilesInserter : public IFilesToProjectInserter
{
public:
    CDllProjectFilesInserter(CVisualStudioProject*    vcproj,
                             const CProjKey           dll_project_key,
                             const list<SConfigInfo>& configs,
                             const string&            project_dir);

    virtual ~CDllProjectFilesInserter(void);

    // IFilesToProjectInserter implementation
    virtual void AddSourceFile (const string& rel_file_path);
    virtual void AddHeaderFile (const string& rel_file_path);
    virtual void AddInlineFile (const string& rel_file_path);

    virtual void Finalize      (void);

private:
    CVisualStudioProject*       m_Vcproj;
    CProjKey                    m_DllProjectKey;
    CSrcToFilterInserterWithPch m_SrcInserter;
    string                      m_ProjectDir;

    typedef CBasicProjectsFilesInserter::SFiltersItem TFiltersItem;
    TFiltersItem  m_PrivateFilters;
    CRef<CFilter> m_HostedLibrariesRootFilter;

    typedef map<CProjKey, TFiltersItem> THostedLibs;
    THostedLibs m_HostedLibs;

    // Prohibited to:
    CDllProjectFilesInserter(void);
    CDllProjectFilesInserter(const CDllProjectFilesInserter&);
    CDllProjectFilesInserter& operator=(const CDllProjectFilesInserter&);
};


/// Common function shared by 
/// CMsvcMasterProjectGenerator and CMsvcProjectGenerator
void AddCustomBuildFileToFilter(CRef<CFilter>&          filter, 
                                const list<SConfigInfo> configs,
                                const string&           project_dir,
                                const SCustomBuildInfo& build_info);

/// Checks if 2 dirs has the same root
bool SameRootDirs(const string& dir1, const string& dir2);


/// Fill-In MSVC 7.10 Utility project
void CreateUtilityProject(const string&            name, 
                          const list<SConfigInfo>& configs, 
                          CVisualStudioProject*    project);

/// Project naming schema
string CreateProjectName(const CProjKey& project_id);


/// Utility class for distinguish between static and dll builds
class CBuildType
{
public:
    CBuildType(bool dll_flag);

    enum EBuildType {
        eStatic,
        eDll
    };

    EBuildType GetType   (void) const;
    string     GetTypeStr(void) const;

private:
    EBuildType m_Type;
    
    //prohibited to:
    CBuildType(void);
    CBuildType(const CBuildType&);
    CBuildType& operator= (const CBuildType&);
};


/// Distribution if source files by lib projects
/// Uses in dll project to separate source files to groups by libs
class CDllSrcFilesDistr
{
public:
    CDllSrcFilesDistr(void);


    // Register .cpp .c files during DLL creation
    void RegisterSource  (const string&   src_file_path, 
                          const CProjKey& dll_project_id,
                          const CProjKey& lib_project_id);
    // Register .hpp .h files during DLL creation
    void RegisterHeader  (const string&   hrd_file_path, 
                          const CProjKey& dll_project_id,
                          const CProjKey& lib_project_id);
    // Register .inl    files during DLL creation
    void RegisterInline  (const string&   inl_file_path, 
                          const CProjKey& dll_project_id,
                          const CProjKey& lib_project_id);

    
    // Retrive original lib_id for .cpp .c file
    CProjKey GetSourceLib(const string&   src_file_path, 
                          const CProjKey& dll_project_id) const;
    // Retrive original lib_id for .cpp .c file
    CProjKey GetHeaderLib(const string&   hdr_file_path, 
                          const CProjKey& dll_project_id) const;
    // Retrive original lib_id for .inl file
    CProjKey GetInlineLib(const string&   inl_file_path, 
                          const CProjKey& dll_project_id) const;
private:

    typedef pair<string,    CProjKey> TDllSrcKey;
    typedef map<TDllSrcKey, CProjKey> TDistrMap;
    TDistrMap m_SourcesMap;
    TDistrMap m_HeadersMap;
    TDistrMap m_InlinesMap;

    //prohibited to
    CDllSrcFilesDistr(const CDllSrcFilesDistr&);
    CDllSrcFilesDistr& operator= (const CDllSrcFilesDistr&);
};


END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.27  2005/04/29 14:10:26  gouriano
 * Added definition of runtime library type
 *
 * Revision 1.26  2004/12/06 18:12:40  gouriano
 * Improved diagnostics
 *
 * Revision 1.25  2004/10/12 16:19:53  ivanov
 * + CSrcToFilterInserterWithPch::InsertFile()
 *
 * Revision 1.24  2004/06/14 19:18:43  gorelenk
 * Changed definition of enum in CBuildType .
 *
 * Revision 1.23  2004/06/10 15:12:55  gorelenk
 * Added newline at the file end to avoid GCC warning.
 *
 * Revision 1.22  2004/06/08 16:28:27  gorelenk
 * Added members m_HeaderFilesPrivate and m_HeaderFilesImpl
 * to struct SFiltersItem.
 *
 * Revision 1.21  2004/06/07 13:51:38  gorelenk
 * Added m_DllInfo to class CMsvc7RegSettings.
 *
 * Revision 1.20  2004/05/26 17:56:59  gorelenk
 * Refactored source files inserter:
 * old inserter moved to class CSrcToFilterInserterWithPch,
 * declared interface IFilesToProjectInserter and 2 implementations:
 * CBasicProjectsFilesInserter for app/lib projects and
 * CDllProjectFilesInserter for dlls.
 *
 * Revision 1.19  2004/05/17 16:13:44  gorelenk
 * Added declaration of class CDllSrcFilesDistr .
 *
 * Revision 1.18  2004/05/10 14:25:47  gorelenk
 * + CSourceFileToProjectInserter .
 *
 * Revision 1.17  2004/04/13 17:06:02  gorelenk
 * Added member m_CompilersSubdir to class CMsvc7RegSettings.
 *
 * Revision 1.16  2004/03/18 17:41:03  gorelenk
 * Aligned classes member-functions parameters inside declarations.
 *
 * Revision 1.15  2004/03/10 16:42:12  gorelenk
 * Changed declaration of class CMsvc7RegSettings.
 *
 * Revision 1.14  2004/03/02 23:28:17  gorelenk
 * Added declaration of class CBuildType.
 *
 * Revision 1.13  2004/02/20 22:54:45  gorelenk
 * Added analysis of ASN projects depends.
 *
 * Revision 1.12  2004/02/12 23:13:49  gorelenk
 * Declared function for MSVC utility project creation.
 *
 * Revision 1.11  2004/02/12 16:22:40  gorelenk
 * Changed generation of command line for custom build info.
 *
 * Revision 1.10  2004/02/10 18:08:16  gorelenk
 * Added declaration of functions SaveIfNewer and PromoteIfDifferent
 * - support for file overwriting only if it was changed.
 *
 * Revision 1.9  2004/02/06 23:15:40  gorelenk
 * Implemented support of ASN projects, semi-auto configure,
 * CPPFLAGS support. Second working version.
 *
 * Revision 1.8  2004/02/05 16:26:43  gorelenk
 * Function GetComponents moved to class CMsvcSite member.
 *
 * Revision 1.7  2004/02/04 23:17:58  gorelenk
 * Added declarations of functions GetComponents and SameRootDirs.
 *
 * Revision 1.6  2004/02/03 17:09:46  gorelenk
 * Changed declaration of class CMsvc7RegSettings.
 * Added declaration of function GetComponents.
 *
 * Revision 1.5  2004/01/28 17:55:05  gorelenk
 * += For msvc makefile support of :
 *                 Requires tag, ExcludeProject tag,
 *                 AddToProject section (SourceFiles and IncludeDirs),
 *                 CustomBuild section.
 * += For support of user local site.
 *
 * Revision 1.4  2004/01/26 19:25:41  gorelenk
 * += MSVC meta makefile support
 * += MSVC project makefile support
 *
 * Revision 1.3  2004/01/22 17:57:08  gorelenk
 * first version
 *
 * ===========================================================================
 */

#endif //PROJECT_TREE_BUILDER__MSVC_PRJ_UTILS__HPP
