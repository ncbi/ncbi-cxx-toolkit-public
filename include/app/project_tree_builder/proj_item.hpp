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

#include <set>

#include <corelib/ncbienv.hpp>

BEGIN_NCBI_SCOPE


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

    typedef enum {
        eNoProj,
        eLib,
        eApp,
        eLast   //TODO - add eDll
    } TProjType;

    CProjItem(void);
    CProjItem(const CProjItem& item);
    CProjItem& operator= (const CProjItem& item);
    
    CProjItem(TProjType type,
              const string& name,
              const string& id,
              const string& sources_base,
              const list<string>& sources, 
              const list<string>& depends,
              const list<string>& requires,
              const list<CDataToolGeneratedSrc> datatool_src);

    ~CProjItem(void);

    /// Name of atomic project.
    string       m_Name;

    /// ID of atomic project.
    string       m_ID;

    /// Type of the project.
    TProjType    m_ProjType;

    /// Base directory of source files (....c++/src/a/ )
    string       m_SourcesBaseDir;

    /// List of source files without extension ( *.cpp ) - with full pathes.
    list<string> m_Sources;
    
    /// What projects this project is depend upon (IDs).
    list<string> m_Depends;

    /// What this project requires to have.
    list<string> m_Requires;

    /// Source files *.asn , *.dtd to be processed by datatool app
    list<CDataToolGeneratedSrc> m_DatatoolSources;

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
    typedef map<string, CProjItem> TProjects;
    TProjects m_Projects;

    /// Full file path / File contents.
    typedef map<string, CSimpleMakeFileContents> TFiles;

    /// Collect all depends for all project items.
    void GetInternalDepends(list<string>* depends) const;

    /// Get depends that are not inside this project tree.
    void GetExternalDepends(list<string>* externalDepends) const;

    /// Navigation through the tree:
    /// Root items.
    void GetRoots(list<string>* ids) const;

    /// First children.
    void GetSiblings(const string& parent_id, 
                     list<string>* ids) const;

    friend class CProjectTreeBuilder;

private:
    //helper for CProjectTreeBuilder
    static void CreateFrom(	const string& root_src,
                            const TFiles& makein, 
                            const TFiles& makelib, 
                            const TFiles& makeapp , CProjectItemsTree* tree);


    void Clear(void);
    void SetFrom(const CProjectItemsTree& projects);

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

    
    static string CreateMakeAppLibFileName(const string&            base_dir,
                                           SMakeInInfo::TMakeinType makeintype,
                                           const string&            projname);

    static void CreateFullPathes(const string&      dir, 
                                 const list<string> files,
                                 list<string>*      full_pathes);

    /// Helpers for project creation
    static string DoCreateAppProject(const string& source_base_dir,
                                     const string& proj_name,
                                     const string& applib_mfilepath,
                                     const TFiles& makeapp, 
                                     CProjectItemsTree* tree);

    static string DoCreateLibProject(const string& source_base_dir,
                                     const string& proj_name,
                                     const string& applib_mfilepath,
                                     const TFiles& makelib, 
                                     CProjectItemsTree* tree);

    static string DoCreateAsnProject(const string& source_base_dir,
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
    static void BuildProjectTree(const string&      start_node_path,
                                 const string&      root_src_path,
                                 CProjectItemsTree* tree  );
private:
    /// Build one project tree and do not resolve (include) depends
    static void BuildOneProjectTree(const string&      start_node_path,
                                    const string&      root_src_path,
                                    CProjectItemsTree* tree  );
    
    static void ProcessDir (const string& dir_name, 
                            bool          is_root, 
                            SMakeFiles*   make_files);

    static void ProcessMakeInFile (const string& file_name, 
                                   SMakeFiles*   make_files);

    static void ProcessMakeLibFile(const string& file_name, 
                                   SMakeFiles*   make_files);

    static void ProcessMakeAppFile(const string& file_name, 
                                   SMakeFiles*   make_files);

    static void ResolveDefs(CSymResolver& resolver, SMakeFiles& makefiles);

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

    typedef set<string> TProjects;
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
