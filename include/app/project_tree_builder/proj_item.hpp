#ifndef PROJ_ITEM_HEADER
#define PROJ_ITEM_HEADER

#include <app/project_tree_builder/file_contents.hpp>
#include <app/project_tree_builder/proj_utils.hpp>
#include <app/project_tree_builder/resolver.hpp>

#include <corelib/ncbienv.hpp>

BEGIN_NCBI_SCOPE
//------------------------------------------------------------------------------

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
    CProjItem& operator = (const CProjItem& item);
    
    CProjItem(TProjType type,
              const string& name,
              const string& id,
              const string& sources_base,
              const list<string>& sources, 
              const list<string>& depends);

    ~CProjItem(void);

    /// Name of atomic project
    string       m_Name;

    /// ID of atomic project
    string       m_ID;


    /// Type of the project.
    TProjType    m_ProjType;

    /// Base directory of source files (....c++/src/a/;
    string       m_SourcesBaseDir;

    /// List of source files without extension ( *.cpp ) - with full pathes
    list<string> m_Sources;
    /// what projects this project is depend upon (IDs)
    list<string> m_Depends;

private:
    void Clear(void);
    void SetFrom(const CProjItem& item);
};


class CProjectItemsTree
{
public:
    CProjectItemsTree(void);
    CProjectItemsTree(const string& root_src);
    CProjectItemsTree(const CProjectItemsTree& projects);
    CProjectItemsTree& operator = (const CProjectItemsTree& projects);
    ~CProjectItemsTree(void);

    /// Root directory of Project Tree.
    string m_RootSrc;

    /// Project ID / ProjectItem.
    typedef map<string, CProjItem> TProjects;
    TProjects m_Projects;

    /// Full file path / File contents.
    typedef map<string, CSimpleMakeFileContents> TFiles;

    static void CreateFrom(	const string& root_src,
                            const TFiles& makein, 
                            const TFiles& makelib, 
                            const TFiles& makeapp , CProjectItemsTree * pTree);

    /// Collect all depends for all project items.
    void GetInternalDepends(list<string> * pDepends) const;

    /// Get depends that are not inside this project tree.
    void GetExternalDepends(list<string> * pExternalDepends) const;

    /// Navigation through the tree:
    /// Root items.
    void GetRoots(list<string> * pIds) const;

    /// First children.
    void GetSiblings(const string& parent_id, 
                     list<string> * pIds) const;

private:
    void Clear(void);
    void SetFrom(const CProjectItemsTree& projects);

    /// ProjType / ProjName(s)
    typedef pair<CProjItem::TProjType, list<string> > TMakeInInfo;
    static void AnalyzeMakeIn(const CSimpleMakeFileContents& makein_contents, 
                              list<TMakeInInfo> * pInfo);

    
    static string CreateMakeAppLibFileName(CProjItem::TProjType projtype, 
                                   const string& projname);

    static void CreateFullPathes(const string& dir, 
                                 const list<string> files,
                                 list<string> * pFullPathes);

};


//------------------------------------------------------------------------------
class CProjectTreeBuilder
{
public:
    typedef map<string, CSimpleMakeFileContents> TFiles;

    //              IN      LIB     APP
    typedef STriple<TFiles, TFiles, TFiles> TMakeFiles;

    static void BuildOneProjectTree( const string& start_node_path,
                                     const string& root_src_path,
                                     CProjectItemsTree *  pTree  );

    static void BuildProjectTree( const string& start_node_path,
                                  const string& root_src_path,
                                  CProjectItemsTree *  pTree  );
private:
    
    static void ProcessDir (const string& dir_name, 
                            bool is_root, 
                            TMakeFiles * pMakeFiles);

    static void ProcessMakeInFile (const string& file_name, 
                                   TMakeFiles * pMakeFiles);

    static void ProcessMakeLibFile(const string& file_name, 
                                   TMakeFiles * pMakeFiles);

    static void ProcessMakeAppFile(const string& file_name, 
                                   TMakeFiles * pMakeFiles);

    static void ResolveDefs(CSymResolver& resolver, TMakeFiles& makefiles);

};
//------------------------------------------------------------------------------


//------------------------------------------------------------------------------
END_NCBI_SCOPE


#endif // PROJ_ITEM_HEADER
