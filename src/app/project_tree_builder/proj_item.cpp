#include <app/project_tree_builder/proj_item.hpp>


CProjItem::CProjItem(void)
{
    Clear();
}


CProjItem::CProjItem(const CProjItem& item)
{
    SetFrom(item);
}


CProjItem& CProjItem::operator = (const CProjItem& item)
{
    if(this != &item) {

        Clear();
        SetFrom(item);
    }
    return *this;
}


CProjItem::CProjItem(TProjType type,
                     const string& name,
                     const string& id,
                     const string& sources_base,
                     const list<string>& sources, 
                     const list<string>& depends)
   :m_Name    (name), 
    m_ID      (id),
    m_ProjType(type),
    m_SourcesBaseDir(sources_base),
    m_Sources (sources), 
    m_Depends (depends)
{
}



CProjItem::~CProjItem(void)
{
    Clear();
}


void CProjItem::Clear(void)
{
    m_ProjType = eNoProj;
}


void CProjItem::SetFrom(const CProjItem& item)
{
    m_Name           = item.m_Name;
    m_ID		     = item.m_ID;
    m_ProjType       = item.m_ProjType;
    m_SourcesBaseDir = item.m_SourcesBaseDir;
    m_Sources        = item.m_Sources;
    m_Depends        = item.m_Depends;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

CProjectItemsTree::CProjectItemsTree(void)
{
    Clear();
}


CProjectItemsTree::CProjectItemsTree(const CProjectItemsTree& projects)
{
    SetFrom(projects);
}


CProjectItemsTree& CProjectItemsTree::operator = (
                                      const CProjectItemsTree& projects)
{
    if(this != &projects) {

        Clear();
        SetFrom(projects);
    }
    return *this;
}


CProjectItemsTree::~CProjectItemsTree(void)
{
    Clear();
}


void CProjectItemsTree::Clear(void)
{
    m_Projects.clear();
}


void CProjectItemsTree::SetFrom(const CProjectItemsTree& projects)
{
    m_Projects = projects.m_Projects;
}


void CProjectItemsTree::CreateFrom(const TFiles& makein, 
                                   const TFiles& makelib, 
                                   const TFiles& makeapp , 
                                   CProjectItemsTree * pTree)
{
    pTree->m_Projects.clear();

    ITERATE(TFiles, p, makein) {

        string sources_dir;
        CDirEntry::SplitPath(p->first, &sources_dir);

        list<TMakeInInfo> list_info;
        AnalyzeMakeIn(p->second, &list_info);
        ITERATE(list<TMakeInInfo>, i, list_info) {

            const TMakeInInfo& info = *i;

            //Iterate all project_name(s) from makefile.in 
            ITERATE(list<string>, n, info.second) {

                //project id
                const string proj_name = *n;
        
                string applib_mfilepath = CDirEntry::ConcatPath(sources_dir,
                               CreateMakeAppLibFileName(info.first, proj_name));
            
                if (info.first == CProjItem.eApp) {

                    TFiles::const_iterator m = makeapp.find(applib_mfilepath);
                    if (m == makeapp.end()) {

                        LOG_POST("**** No Makefile.*.app for Makefile.in :"
                                  + p->first);
                        continue;
                    }

                    CSimpleMakeFileContents::TContents::const_iterator k = 
                        m->second.m_Contents.find("SRC");
                    if (k == m->second.m_Contents.end()) {

                        LOG_POST("**** No SRC key in Makefile.*.app :"
                                  + applib_mfilepath);
                        continue;
                    }

                    //sources - full pathes(We'll create relative pathes from them)
                    list<string> sources;
                    CreateFullPathes(sources_dir, k->second, &sources);

                    //depends
                    list<string> depends;
                    k = m->second.m_Contents.find("LIB");
                    if (k != m->second.m_Contents.end())
                        depends = k->second;

                    //project name
                    k = m->second.m_Contents.find("APP");
                    if (k == m->second.m_Contents.end()  ||  k->second.empty()) {

                        LOG_POST("**** No APP key or empty in Makefile.*.app :"
                                  + applib_mfilepath);
                        continue;
                    }
                    string proj_id = k->second.front();

                    string source_base_dir;
                    CDirEntry::SplitPath(p->first, &source_base_dir);
                    pTree->m_Projects[proj_id] =  CProjItem( CProjItem::eApp, 
                                                               proj_name, 
                                                               proj_id,
                                                               source_base_dir,
                                                               sources, 
                                                               depends);

                }
                else if (info.first == CProjItem.eLib) {

                    TFiles::const_iterator m = makelib.find(applib_mfilepath);
                    if (m == makelib.end()) {

                        LOG_POST("**** No Makefile.*.lib for Makefile.in :"
                                  + p->first);
                        continue;
                    }

                    CSimpleMakeFileContents::TContents::const_iterator k = 
                        m->second.m_Contents.find("SRC");
                    if(k == m->second.m_Contents.end()) {

                        LOG_POST("**** No SRC key in Makefile.*.lib :"
                                  + applib_mfilepath);
                        continue;
                    }

                    //sources - full pathes (We'll create relative pathes from them)
                    list<string> sources;
                    CreateFullPathes(sources_dir, k->second, &sources);

                    //depends - TODO
                    list<string> depends;

                    //project name
                    k = m->second.m_Contents.find("LIB");
                    if (k == m->second.m_Contents.end()  ||  k->second.empty()) {

                        LOG_POST("**** No LIB key or empty in Makefile.*.lib :"
                                  + applib_mfilepath);
                        continue;
                    }
                    string proj_id = k->second.front();

                    string source_base_dir;
                    CDirEntry::SplitPath(p->first, &source_base_dir);
                    pTree->m_Projects[proj_id] =  CProjItem( CProjItem::eLib,
                                                               proj_name, 
                                                               proj_id,
                                                               source_base_dir,
                                                               sources, 
                                                               depends);
                }
            }
        }
    }
}


void CProjectItemsTree::AnalyzeMakeIn(
                              const CSimpleMakeFileContents& makein_contents,
                              list<TMakeInInfo> * pInfo)
{
    pInfo->clear();

    CSimpleMakeFileContents::TContents::const_iterator p = 
        makein_contents.m_Contents.find("LIB_PROJ");

    if (p != makein_contents.m_Contents.end()) {

        pInfo->push_back(TMakeInInfo(CProjItem::eLib, p->second)); 
    }

    p = makein_contents.m_Contents.find("APP_PROJ");
    if (p != makein_contents.m_Contents.end()) {

        pInfo->push_back(TMakeInInfo(CProjItem::eApp, p->second)); 
    }

    //TODO - DLL_PROJ
}


string CProjectItemsTree::CreateMakeAppLibFileName(CProjItem::TProjType projtype, 
                                   const string& projname)
{
    string fname = "Makefile." + projname;

    switch (projtype) {
    case CProjItem::eApp:
        fname += ".app";
        break;
    case CProjItem::eLib:
        fname += ".lib";
        break;
    }
    return fname;
}


void CProjectItemsTree::CreateFullPathes(const string& dir, 
                                        const list<string> files,
                                        list<string> * pFullPathes)
{
    ITERATE(list<string>, p, files) {

        string full_path = CDirEntry::ConcatPath(dir, *p);
        pFullPathes->push_back(full_path);
    }
}

