#ifndef PROJ_ITEM_HEADER
#define PROJ_ITEM_HEADER

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

#include <app/project_tree_builder/file_contents.hpp>
#include <app/project_tree_builder/proj_utils.hpp>
#include <app/project_tree_builder/resolver.hpp>
#include <app/project_tree_builder/proj_datatool_generated_src.hpp>
#include <app/project_tree_builder/proj_projects.hpp>

#include <set>

#include <corelib/ncbienv.hpp>

BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
///
/// CProjKey --
///
/// Project key  abstraction.
///
/// Project key (type + project_id).

class CProjKey
{
public:
    typedef enum {
        eNoProj,
        eLib,
        eApp,
        eLast   //TODO - add eDll
    } TProjType;

    CProjKey(void);
    CProjKey(TProjType type, const string& project_id);
    CProjKey(const CProjKey& key);
    CProjKey& operator= (const CProjKey& key);
    ~CProjKey(void);

    bool operator<  (const CProjKey& key) const;
    bool operator== (const CProjKey& key) const;
    bool operator!= (const CProjKey& key) const;

    TProjType     Type(void) const;
    const string& Id  (void) const;

private:
    TProjType m_Type;
    string    m_Id;

};

/////////////////////////////////////////////////////////////////////////////
///
/// CProjItem --
///
/// Project abstraction.
///
/// Representation of one project from the build tree.

class CProjItem
{
public:
    typedef CProjKey::TProjType TProjType;


    CProjItem(void);
    CProjItem(const CProjItem& item);
    CProjItem& operator= (const CProjItem& item);

    CProjItem(TProjType type,
              const string& name,
              const string& id,
              const string& sources_base,
              const list<string>&   sources, 
              const list<CProjKey>& depends,
              const list<string>&   requires,
              const list<string>&   libs_3_party,
              const list<string>&   include_dirs,
              const list<string>&   defines);
    
    ~CProjItem(void);

    /// Name of atomic project.
    string       m_Name;

    /// ID of atomic project.
    string       m_ID;

    /// Type of the project.
    TProjType    m_ProjType;

    /// Base directory of source files (....c++/src/a/ )
    string       m_SourcesBaseDir;

    /// List of source files without extension ( *.cpp or *.c ) -
    /// with relative pathes from m_SourcesBaseDir.
    list<string> m_Sources;
    
    /// What projects this project is depend upon (IDs).
    list<CProjKey> m_Depends;

    /// What this project requires to have (in user site).
    list<string> m_Requires;

    /// Resolved contents of LIBS flag (Third-party libs)
    list<string> m_Libs3Party;

    /// Resolved contents of CPPFLAG ( -I$(include)<m_IncludeDir> -I$(srcdir)/..)
    /// Absolute pathes
    list<string>  m_IncludeDirs;

    /// Source files *.asn , *.dtd to be processed by datatool app
    list<CDataToolGeneratedSrc> m_DatatoolSources;

    /// Defines like USE_MS_DBLIB
    list<string>  m_Defines;

    /// Libraries from NCBI C Toolkit to link with
    list<string>  m_NcbiCLibs;


private:
    void Clear(void);
    void SetFrom(const CProjItem& item);
};


/////////////////////////////////////////////////////////////////////////////
///
/// CProjectItemsTree --
///
/// Build tree abstraction.
///
/// Container for project items as well as utilits for tree analysis 
/// and navigation.

class CProjectItemsTree
{
public:
    CProjectItemsTree(void);
    CProjectItemsTree(const string& root_src);
    CProjectItemsTree(const CProjectItemsTree& projects);
    CProjectItemsTree& operator= (const CProjectItemsTree& projects);
    ~CProjectItemsTree(void);

    /// Root directory of Project Tree.
    string m_RootSrc;

    /// Project ID / ProjectItem.
    typedef map<CProjKey, CProjItem> TProjects;
    TProjects m_Projects;

    /// Full file path / File contents.
    typedef map<string, CSimpleMakeFileContents> TFiles;

    /// Collect all depends for all project items.
    void GetInternalDepends(list<CProjKey>* depends) const;

    /// Get depends that are not inside this project tree.
    void GetExternalDepends(list<CProjKey>* externalDepends) const;

#if 0
    void GetDepends(const CProjKey& proj_id,
                    list<CProjKey>* depends) const;

    void CreateBuildOrder(const CProjKey& proj_id, 
                          list<CProjKey>* projects) const;
#endif

    // for navigation through the tree use class CProjectTreeFolders below.

    friend class CProjectTreeBuilder;

private:
    //helper for CProjectTreeBuilder
    static void CreateFrom(	const string& root_src,
                            const TFiles& makein, 
                            const TFiles& makelib, 
                            const TFiles& makeapp , CProjectItemsTree* tree);


    void Clear(void);
    void SetFrom(const CProjectItemsTree& projects);
};

// Traits classes - creation helpers for CProjectTreeBuilder

struct SMakeProjectT
{
    typedef CProjectItemsTree::TFiles TFiles;

    static CProjItem::TProjType GetProjType(const string& base_dir,
                                            const string& projname);

    struct SMakeInInfo
    {
        typedef enum {
            eApp,
            eLib,
            eAsn 
        } TMakeinType;

        SMakeInInfo(TMakeinType          type,
                    const list<string>&  names)
            :m_Type     (type),
             m_ProjNames(names)
        {
        }
        
        TMakeinType   m_Type;
        list<string>  m_ProjNames;
    };

    typedef list<SMakeInInfo> TMakeInInfoList;
    static void AnalyzeMakeIn(const CSimpleMakeFileContents& makein_contents,
                              TMakeInInfoList*               info);

    
    static string CreateMakeAppLibFileName(const string& base_dir,
                                           const string& projname);

    static void CreateFullPathes(const string&      dir, 
                                 const list<string> files,
                                 list<string>*      full_pathes);

    static string GetOneIncludeDir(const string& flag, const string& token);
    
    
    static void CreateIncludeDirs(const list<string>& cpp_flags,
                                  const string&       source_base_dir,
                                  list<string>*       include_dirs);

    static void CreateDefines(const list<string>& cpp_flags,
                              list<string>*       defines);

    
    static void Create3PartyLibs(const list<string>& libs_flags, 
                                 list<string>*       libs_list);

    static void DoResolveDefs(CSymResolver& resolver, 
                              CProjectItemsTree::TFiles& files,
                              const set<string>& keys);

    static bool IsMakeInFile(const string& name);

    static bool IsMakeLibFile(const string& name);


    static bool IsMakeAppFile(const string& name);

    static void ConvertLibDepends(const list<string>& depends_libs, 
                                  list<CProjKey>*     depends_ids);
};


struct SAppProjectT : public SMakeProjectT
{
    static void CreateNcbiCToolkitLibs(const string& applib_mfilepath,
                                       const TFiles& makeapp,
                                       list<string>* libs_list);

    static CProjKey DoCreate(const string& source_base_dir,
                             const string& proj_name,
                             const string& applib_mfilepath,
                             const TFiles& makeapp, 
                             CProjectItemsTree* tree);
};

struct SLibProjectT : public SMakeProjectT
{
    static CProjKey DoCreate(const string& source_base_dir,
                             const string& proj_name,
                             const string& applib_mfilepath,
                             const TFiles& makeapp, 
                             CProjectItemsTree* tree);
};

struct SAsnProjectT : public SMakeProjectT
{
    typedef CProjectItemsTree::TProjects TProjects;

    static CProjKey DoCreate(const string& source_base_dir,
                             const string& proj_name,
                             const string& applib_mfilepath,
                             const TFiles& makeapp, 
                             const TFiles& makelib, 
                             CProjectItemsTree* tree);
    
    typedef enum {
            eNoAsn,
            eSingle,
            eMultiple,
        } TAsnType;

    static TAsnType GetAsnProjectType(const string& applib_mfilepath,
                                       const TFiles& makeapp,
                                       const TFiles& makelib);
};


struct SAsnProjectSingleT : public SAsnProjectT
{
    static CProjKey DoCreate(const string& source_base_dir,
                             const string& proj_name,
                             const string& applib_mfilepath,
                             const TFiles& makeapp, 
                             const TFiles& makelib, 
                             CProjectItemsTree* tree);
};

struct SAsnProjectMultipleT : public SAsnProjectT
{
    static CProjKey DoCreate(const string& source_base_dir,
                             const string& proj_name,
                             const string& applib_mfilepath,
                             const TFiles& makeapp, 
                             const TFiles& makelib, 
                             CProjectItemsTree* tree);
};

/////////////////////////////////////////////////////////////////////////////
///
/// CProjectTreeBuilder --
///
/// Builder class for project tree.
///
/// Builds tree, resolvs macrodefines and adds dependents projects.

class CProjectTreeBuilder
{
public:
    typedef map<string, CSimpleMakeFileContents> TFiles;

    //              IN      LIB     APP
//    typedef STriple<TFiles, TFiles, TFiles> TMakeFiles;
    
    struct SMakeFiles
    {
        TFiles m_In;
        TFiles m_Lib;
        TFiles m_App;
    };


    /// Build project tree and include all projects this tree depends upon
    static void BuildProjectTree(const IProjectFilter* filter,
                                 const string&         root_src_path,
                                 CProjectItemsTree*    tree  );
private:
    /// Build one project tree and do not resolve (include) depends
    static void BuildOneProjectTree(const IProjectFilter* filter,
                                    const string&         root_src_path,
                                    CProjectItemsTree*    tree  );
    
    static void ProcessDir (const string&         dir_name, 
                            bool                  is_root,
                            const IProjectFilter* filter,
                            SMakeFiles*           makefiles);

    static void ProcessMakeInFile (const string& file_name, 
                                   SMakeFiles*   makefiles);

    static void ProcessMakeLibFile(const string& file_name, 
                                   SMakeFiles*   makefiles);

    static void ProcessMakeAppFile(const string& file_name, 
                                   SMakeFiles*   makefiles);

    static void ResolveDefs(CSymResolver& resolver, SMakeFiles& makefiles);

    
    static void AddDatatoolSourcesDepends(CProjectItemsTree* tree);

};

struct  SProjectTreeFolder
{
    SProjectTreeFolder()
        :m_Parent(NULL)
    {
    }


    SProjectTreeFolder(const string& name, SProjectTreeFolder* parent)
        :m_Name  (name),
         m_Parent(parent)
    {
    }

    string    m_Name;
    
    typedef map<string, SProjectTreeFolder* > TSiblings;
    TSiblings m_Siblings;

    typedef set<CProjKey> TProjects;
    TProjects m_Projects;

    SProjectTreeFolder* m_Parent;
    bool IsRoot(void) const
    {
        return m_Parent == NULL;
    }
};


class CProjectTreeFolders
{
public:
    CProjectTreeFolders(const CProjectItemsTree& tree);
    
    SProjectTreeFolder m_RootParent;

    typedef list<string> TPath;
    SProjectTreeFolder* FindFolder(const TPath& path);
    SProjectTreeFolder* FindOrCreateFolder(const TPath& path);
    
    static void CreatePath(const string& root_src_dir, 
                           const string& project_base_dir,
                           TPath*        path);

private:
    SProjectTreeFolder* CreateFolder(SProjectTreeFolder* parent, 
                                     const string&       folder_name);

    list<SProjectTreeFolder> m_Folders;

    CProjectTreeFolders(void);
    CProjectTreeFolders(const CProjectTreeFolders&);
    CProjectTreeFolders& operator= (const CProjectTreeFolders&);
};

END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.12  2004/02/26 21:24:03  gorelenk
 * Declaration member-functions of class CProjectTreeBuilder:
 * BuildProjectTree, BuildOneProjectTree and ProcessDir changed to use
 * IProjectFilter* filter instead of const string& subtree_to_build.
 *
 * Revision 1.11  2004/02/24 17:15:25  gorelenk
 * Added member m_NcbiCLibs to class CProjItem.
 * Added declaration of member function CreateNcbiCToolkitLibs
 * to struct SAppProjectT.
 *
 * Revision 1.10  2004/02/20 22:55:12  gorelenk
 * Added analysis of ASN projects depends.
 *
 * Revision 1.9  2004/02/06 23:15:40  gorelenk
 * Implemented support of ASN projects, semi-auto configure,
 * CPPFLAGS support. Second working version.
 *
 * Revision 1.8  2004/02/04 23:15:27  gorelenk
 * To class CProjItem added members m_Libs3Party and m_IncludeDirs.
 * Redesigned constructors of class CProjItem.
 *
 * Revision 1.7  2004/02/03 17:05:08  gorelenk
 * Added members to class CProjItem
 *
 * Revision 1.6  2004/01/30 20:42:21  gorelenk
 * Added support of ASN projects
 *
 * Revision 1.5  2004/01/26 19:25:42  gorelenk
 * += MSVC meta makefile support
 * += MSVC project makefile support
 *
 * Revision 1.4  2004/01/22 17:57:09  gorelenk
 * first version
 *
 * ===========================================================================
 */

#endif // PROJ_ITEM_HEADER
