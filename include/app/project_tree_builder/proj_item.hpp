#ifndef PROJ_ITEM_HEADER
#define PROJ_ITEM_HEADER

#include <app/project_tree_builder/file_contents.hpp>
#include <app/project_tree_builder/proj_builder_app.hpp>

#include <corelib/ncbienv.hpp>
USING_NCBI_SCOPE;

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
    CProjectItemsTree(const CProjectItemsTree& projects);
    CProjectItemsTree& operator = (const CProjectItemsTree& projects);
    ~CProjectItemsTree(void);

    /// Project ID / ProjectItem
    typedef map<string, CProjItem> TProjects;
    TProjects m_Projects;

    typedef CProjBulderApp::TFiles TFiles;
    static void CreateFrom(	const TFiles& makein, 
                            const TFiles& makelib, 
                            const TFiles& makeapp , CProjectItemsTree * pTree);
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

#endif // PROJ_ITEM_HEADER
