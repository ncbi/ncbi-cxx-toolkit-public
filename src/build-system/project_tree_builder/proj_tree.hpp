#ifndef PROJECT_TREE_BUILDER__PROJ_TREE__HPP
#define PROJECT_TREE_BUILDER__PROJ_TREE__HPP

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


#include "proj_item.hpp"
#include <set>
#include "file_contents.hpp"


#include <corelib/ncbienv.hpp>

BEGIN_NCBI_SCOPE


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

    /// Full file path / File contents.
    typedef map<string, CSimpleMakeFileContents> TFiles;

    /// Project ID / ProjectItem.
    typedef map<CProjKey, CProjItem> TProjects;
    TProjects m_Projects;

    /// Collect all depends for all project items.
    void GetInternalDepends(list<CProjKey>* depends) const;

    /// Get depends that are not inside this project tree.
    void GetExternalDepends(list<CProjKey>* externalDepends) const;
    
    void VerifyExternalDepends(void);
    void VerifyDataspecProj(void);

    // for navigation through the tree use class CProjectTreeFolders below.

    friend class CProjectTreeBuilder;

private:
    //helper for CProjectTreeBuilder
    static void CreateFrom(	const string&      root_src,
                            const TFiles&      makein, 
                            const TFiles&      makelib, 
                            const TFiles&      makedll, 
                            const TFiles&      makeapp, 
                            const TFiles&      makemsvc, 
                            CProjectItemsTree* tree);


    void Clear(void);
    void SetFrom(const CProjectItemsTree& projects);
};




/////////////////////////////////////////////////////////////////////////////
///
/// CCyclicDepends --
///
/// Analyzer of cyclic dependencies in project tree.
///
/// Looks for dependencies cycles and report them.

class CCyclicDepends
{
public:
    typedef CProjectItemsTree::TProjects TProjects;

    typedef list<CProjKey>               TDependsChain;
    typedef list<TDependsChain>          TDependsChains;
    typedef set <TDependsChain>          TDependsCycles;
    
    static void FindCycles(const TProjects& tree,
                           TDependsCycles*  cycles);

    static void FindCyclesNew(const TProjects& tree,
                              TDependsCycles*  cycles);

private:
    static bool AnalyzeProjItemNew( const TProjects& tree,
                                    const CProjKey&  proj_id,
                                    set< CProjKey>& projkeys,
                                    TDependsChain& chain);
    
    static bool IsInAnyCycle(const CProjKey&       proj_id,
                             const TDependsCycles& cycles);

    static void AnalyzeProjItem(const CProjKey&  proj_id,
                                const TProjects& tree,
                                TDependsCycles*  cycles);

    static bool ExtendChains(const CProjKey&  proj_id, 
                             const TProjects& tree,
                             TDependsChains*  chains,
                             TDependsChain*   cycle_found);

    static bool IsCyclic(const CProjKey&       proj_id, 
                         const TDependsChains& chains,
                         TDependsChain*        cycle_found);
};


/////////////////////////////////////////////////////////////////////////////
///
/// SProjectTreeFolder --
///
/// Abstraction of a folder in project tree.
///
/// One project tree folder.

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


/////////////////////////////////////////////////////////////////////////////
///
/// CProjectTreeFolders --
///
/// Abstraction of project tree structure.
///
/// Creates project tree structure as a tree of SProjectTreeFolder(s).

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


/////////////////////////////////////////////////////////////////////////////
class CMakeNode
{
public:
    CMakeNode(void);
    ~CMakeNode(void);
    CMakeNode(const CMakeNode& other);
    CMakeNode& operator=(const CMakeNode& other);

    void AddHeader(const string& name);
    void AddDefinition(const string& key, const string& value);
    void AddInclude(const string& name);
    void AddProject( const string& prj);
    void AddSubdir( const string& dir);

    void Write(const string& dirname) const;

private:
    vector<string> m_NodeHeaders;
    vector<string> m_NodeDefinitions;
    vector<string> m_NodeIncludes;
    set<string> m_NodeProjects;
    set<string> m_NodeSubdirs;
};

/////////////////////////////////////////////////////////////////////////////
class CMakeProperty
{
public:
    CMakeProperty(const string& name);
    ~CMakeProperty(void);
    CMakeProperty(const CMakeProperty& other);
    CMakeProperty operator=(const CMakeProperty& other);

    CMakeProperty& AddValue(const string& value);
    void Write(CNcbiOstream& out) const; 
private:
    string m_Propname;
    vector<string> m_Propvalue;
};

/////////////////////////////////////////////////////////////////////////////
class CMakeProject
{
public:
    CMakeProject(void);
    ~CMakeProject(void);
    CMakeProject(const CMakeProject& other);
    CMakeProject& operator=(const CMakeProject& other);

    void SetProjKey(const CProjKey& prj_key);
    void AddDefinition(const string& key, const string& value);
    void AddSourceFile(const string& folder, const string& name);
    void AddIncludeDirectory(const string& name);
    void AddLibrary(const string& name);
    void AddDependency(const string& name);
    void AddProperty(const CMakeProperty& prop);

    void Write(const string& dirname) const;

private:
    CProjKey m_Prj_key;
    vector<string> m_Definitions;
//    set<string> m_Sources;
    map<string, set<string> > m_Sources;
    set<string> m_IncludeDir;
    vector<string> m_Libraries;
    vector<string> m_Dependencies;
    vector<CMakeProperty> m_Properties;
};

/////////////////////////////////////////////////////////////////////////////
class CMakeGenerator
{
public:
    static void GenerateCMakeTree(CProjectItemsTree& projects_tree);
};

/////////////////////////////////////////////////////////////////////////////
class CMakefilePatch
{
public:
    static void PatchTreeMakefiles(const CProjectItemsTree& prj_tree);
};

END_NCBI_SCOPE

#endif //PROJECT_TREE_BUILDER__PROJ_TREE__HPP
