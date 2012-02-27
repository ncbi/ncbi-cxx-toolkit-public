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

#include <ncbi_pch.hpp>
#include "proj_tree.hpp"
#include "proj_tree_builder.hpp"
#include "proj_utils.hpp"
#include "proj_builder_app.hpp"

#include <algorithm>

BEGIN_NCBI_SCOPE


//-----------------------------------------------------------------------------
CProjectItemsTree::CProjectItemsTree(void)
{
    Clear();
}


CProjectItemsTree::CProjectItemsTree(const string& root_src)
{
    Clear();
    m_RootSrc = root_src;
}


CProjectItemsTree::CProjectItemsTree(const CProjectItemsTree& projects)
{
    SetFrom(projects);
}


CProjectItemsTree& 
CProjectItemsTree::operator= (const CProjectItemsTree& projects)
{
    if (this != &projects) {
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
    m_RootSrc.erase();
    m_Projects.clear();
}


void CProjectItemsTree::SetFrom(const CProjectItemsTree& projects)
{
    m_RootSrc  = projects.m_RootSrc;
    m_Projects = projects.m_Projects;
}


void CProjectItemsTree::CreateFrom(const string& root_src,
                                   const TFiles& makein, 
                                   const TFiles& makelib, 
                                   const TFiles& makedll, 
                                   const TFiles& makeapp, 
                                   const TFiles& makemsvc, 
                                   CProjectItemsTree* tree)
{
    tree->m_Projects.clear();
    tree->m_RootSrc = root_src;

    string dataspec = GetApp().GetDataspecProjId();
    string utility_projects_dir = GetApp().GetUtilityProjectsSrcDir();
    SLibProjectT::DoCreateDataSpec(
        utility_projects_dir, dataspec, dataspec, tree, eMakeType_Undefined);

    ITERATE(TFiles, p, makein) {

        const string& fc_path = p->first;
        const CSimpleMakeFileContents& fc_makein = p->second;

        string source_base_dir;
        CDirEntry::SplitPath(fc_path, &source_base_dir);

        SMakeProjectT::TMakeInInfoList list_info;
        SMakeProjectT::AnalyzeMakeIn(fc_makein, &list_info);
        ITERATE(SMakeProjectT::TMakeInInfoList, i, list_info) {

            const SMakeProjectT::SMakeInInfo& info = *i;

            //Iterate all project_name(s) from makefile.in 
            ITERATE(list<string>, n, info.m_ProjNames) {

                //project id will be defined latter
                const string& proj_name = *n;
                if (proj_name[0] == '#') {
                    break;
                }
                if (proj_name[0] == '@'  &&
                    proj_name[proj_name.size() - 1] == '@') {
                    /// skip these - these are dead macros
                    continue;
                }
        
                string applib_mfilepath = 
                    SMakeProjectT::CreateMakeAppLibFileName(source_base_dir, 
                                                            proj_name, info.m_Type);
                if ( applib_mfilepath.empty() )
                    continue;
            
                if (info.m_Type == SMakeProjectT::SMakeInInfo::eApp) {

                    SAsnProjectT::TAsnType asn_type = 
                        SAsnProjectT::GetAsnProjectType(applib_mfilepath,
                                                        makeapp,
                                                        makelib);

                    if (asn_type == SAsnProjectT::eMultiple) {
                        SAsnProjectT::DoCreate(source_base_dir, 
                                               proj_name, 
                                               applib_mfilepath, 
                                               makeapp, makelib, tree, info);
                    } else {
                        SAppProjectT::DoCreate(source_base_dir, 
                                               proj_name, 
                                               applib_mfilepath, 
                                               makeapp, tree, info.m_MakeType);
                    }
                }
                else if (info.m_Type == SMakeProjectT::SMakeInInfo::eLib) {

                    SAsnProjectT::TAsnType asn_type = 
                        SAsnProjectT::GetAsnProjectType(applib_mfilepath,
                                                        makeapp,
                                                        makelib);

                    if (asn_type == SAsnProjectT::eMultiple) {
                        SAsnProjectT::DoCreate(source_base_dir, 
                                               proj_name, 
                                               applib_mfilepath, 
                                               makeapp, makelib, tree, info);
                    } else {
                        SLibProjectT::DoCreate(source_base_dir, 
                                               proj_name, 
                                               applib_mfilepath, 
                                               makelib, tree, info.m_MakeType);
                    }
                }
                else if (info.m_Type == SMakeProjectT::SMakeInInfo::eDll) {
                    SDllProjectT::DoCreate(source_base_dir, 
                                           proj_name, 
                                           applib_mfilepath, 
                                           makedll, tree, info.m_MakeType);
                }
                else if (info.m_Type == SMakeProjectT::SMakeInInfo::eASN ||
                         info.m_Type == SMakeProjectT::SMakeInInfo::eDTD ||
                         info.m_Type == SMakeProjectT::SMakeInInfo::eXSD ||
                         info.m_Type == SMakeProjectT::SMakeInInfo::eWSDL) {

                    SAsnProjectT::DoCreate(source_base_dir, 
                                           proj_name, 
                                           applib_mfilepath, 
                                           makeapp, makelib, tree, info);
                }
                else if (info.m_Type == SMakeProjectT::SMakeInInfo::eMsvc) {

                    SMsvcProjectT::DoCreate(source_base_dir,
                                            proj_name,
                                            applib_mfilepath,
                                            makemsvc, tree, info.m_MakeType);
                }
            }
        }
    }
    AnalyzeDllData(*tree);
}


void CProjectItemsTree::GetInternalDepends(list<CProjKey>* depends) const
{
    depends->clear();

    set<CProjKey> depends_set;

    ITERATE(TProjects, p, m_Projects) {
        const CProjItem& proj_item = p->second;
// DLL dependencies will be handled later
        if (proj_item.m_ProjType != CProjKey::eDll) {
            ITERATE(list<CProjKey>, n, proj_item.m_Depends) {
                depends_set.insert(*n);
            }
            if (proj_item.m_ProjType == CProjKey::eLib &&
                GetApp().GetBuildType().GetType() == CBuildType::eDll &&
                !proj_item.m_DllHost.empty()) {
                    depends_set.insert(CProjKey(
                        CProjKey::eDll, proj_item.m_DllHost));
            }
        } else {
            if (GetApp().GetBuildType().GetType() == CBuildType::eDll) {
                ITERATE(list<string>, n, proj_item.m_HostedLibs) {
                    depends_set.insert(CProjKey(CProjKey::eLib, *n));
                }
            }
        }
    }

    copy(depends_set.begin(), depends_set.end(), back_inserter(*depends));
}


void 
CProjectItemsTree::GetExternalDepends(list<CProjKey>* external_depends) const
{
    external_depends->clear();

    list<CProjKey> depends;
    GetInternalDepends(&depends);
    ITERATE(list<CProjKey>, p, depends) {
        const CProjKey& depend_id = *p;
        if (m_Projects.find(depend_id) == m_Projects.end())
            external_depends->push_back(depend_id);
    }
}

void CProjectItemsTree::VerifyExternalDepends(void)
{
    ITERATE(TProjects, p, m_Projects) {
        const CProjItem& proj_item = p->second;
        if (proj_item.m_External) {
            continue;
        }
        ITERATE(list<CProjKey>, n, proj_item.m_Depends) {
            if (*n == p->first) {
                continue;
            }
            TProjects::iterator d = m_Projects.find(*n);
            if (d != m_Projects.end() && d->second.m_External) {
                d->second.m_MakeType = min(d->second.m_MakeType, p->second.m_MakeType);
            }
        }
    }

    set<CProjKey> depends_set;
    ITERATE(TProjects, p, m_Projects) {
        const CProjItem& proj_item = p->second;
        if (proj_item.m_External) {
            continue;
        }
        ITERATE(list<CProjKey>, n, proj_item.m_Depends) {
            if (*n == p->first) {
                continue;
            }
            depends_set.insert(*n);
        }
    }
    bool modified = false;
    ITERATE(set<CProjKey>, d, depends_set) {
        TProjects::iterator p = m_Projects.find(*d);
        if (p != m_Projects.end() && p->second.m_External) {
            p->second.m_External = false;
            modified = true;
        }
    }
    if (modified) {
        VerifyExternalDepends();
    }
}

void CProjectItemsTree::VerifyDataspecProj(void)
{
    CProjKey proj_key(CProjKey::eDataSpec, GetApp().GetDataspecProjId());
    CProjectItemsTree::TProjects::iterator z = m_Projects.find(proj_key);
    if (z == m_Projects.end()) {
        return;
    }
    CProjItem& dataspec_prj(z->second);
    list<CDataToolGeneratedSrc>::iterator s;
    for (s = dataspec_prj.m_DatatoolSources.begin();
        s != dataspec_prj.m_DatatoolSources.end(); ) {
        bool found = false;
        TProjects::const_iterator p;
        for(p = m_Projects.begin(); !found && p != m_Projects.end(); ++p) {
            if (p->first.Type() != CProjKey::eDataSpec) {
                const CProjItem& project = p->second;
                list<CDataToolGeneratedSrc>::const_iterator n;
                for (n = project.m_DatatoolSources.begin();
                    !found && n != project.m_DatatoolSources.end(); ++n) {
                    found = (*n == *s);
                }
            }
        }
        list<CDataToolGeneratedSrc>::iterator prev = s;
        ++s;
        if (!found) {
            dataspec_prj.m_DatatoolSources.erase(prev);
        }
    }
/*
    maybe, it is better to keep even an empty one
    for consistency..
    if (dataspec_prj.m_DatatoolSources.empty()) {
        m_Projects.erase(z);
    }
*/
}


//-----------------------------------------------------------------------------
void CCyclicDepends::FindCyclesNew(const TProjects& tree,
                        TDependsCycles*  cycles)
{
    cycles->clear();

    set< CProjKey> projkeys;
    TDependsChain chain;

    // For all projects in tree.
    ITERATE(TProjects, p, tree) {
        const CProjKey& proj_id = p->first;
        if (p->second.m_ProjType == CProjKey::eDll) {
            continue;
        }
        if (AnalyzeProjItemNew(tree, proj_id, projkeys, chain)) {
            cycles->insert(chain);
            chain.clear();
        }
        _ASSERT(chain.size() == 0);
        _ASSERT(projkeys.size() == 0);
    }
}

bool CCyclicDepends::AnalyzeProjItemNew(
    const TProjects& tree,
    const CProjKey&  proj_id,
    set< CProjKey>& projkeys,
    TDependsChain& chain)
{
    if (projkeys.find(proj_id) != projkeys.end()) {
        chain.push_back(proj_id);
        return true;
    }
    TProjects::const_iterator p = tree.find(proj_id);
    if (p == tree.end()) {
        if (!SMakeProjectT::IsConfigurableDefine(proj_id.Id())) {
            if (!GetApp().GetSite().IsLibWithChoice(proj_id.Id()) ||
                 GetApp().GetSite().GetChoiceForLib(proj_id.Id()) == CMsvcSite::eLib ) {
                string str_chain("Dependency chain: ");
                ITERATE(TDependsChain, n, chain) {
                    str_chain += n->Id();
                    str_chain += " - ";
                }
                str_chain += proj_id.Id();
                LOG_POST( Warning << str_chain << ": Undefined project: " << proj_id.Id() );
            }
        }
        return false;
    }
    const CProjItem& project = p->second;
    TDependsChain::const_iterator i = project.m_Depends.begin();
    if (i == project.m_Depends.end()) {
        return false;
    }
    chain.push_back(proj_id);
    projkeys.insert(proj_id);
    bool found=false;
    for ( ; !found && i != project.m_Depends.end(); ++i) {
        if (*i == proj_id) {
            continue;
        }
        found = AnalyzeProjItemNew(tree, *i, projkeys,chain);
    }
    if (!found) {
        chain.pop_back();
    }
    projkeys.erase(proj_id);
    return found;
}


void CCyclicDepends::FindCycles(const TProjects& tree,
                                TDependsCycles*  cycles)
{
    cycles->clear();

    ITERATE(TProjects, p, tree) {
        // Look throgh all projects in tree.
        const CProjKey& project_id = p->first;
        // If this proj_id was already reported in some cycle, 
        // it's no need to test it again.
        if ( !IsInAnyCycle(project_id, *cycles) ) {
            // Analyze for cycles
            AnalyzeProjItem(project_id, tree, cycles);
        }
    }
}


bool CCyclicDepends::IsInAnyCycle(const CProjKey&       proj_id,
                                  const TDependsCycles& cycles)
{
    ITERATE(TDependsCycles, p, cycles) {
        const TDependsChain& cycle = *p;
        if (find(cycle.begin(), cycle.end(), proj_id) != cycle.end())
            return true;
    }
    return false;
}


void CCyclicDepends::AnalyzeProjItem(const CProjKey&  proj_id,
                                     const TProjects& tree,
                                     TDependsCycles*  cycles)
{
    TProjects::const_iterator p = tree.find(proj_id);
    if (p == tree.end()) {
        LOG_POST( Error << "Undefined project: " << proj_id.Id() );
        return;
    }
    
    const CProjItem& project = p->second;
    // No depends - no cycles
    if ( project.m_Depends.empty() )
        return;

    TDependsChains chains;
    ITERATE(list<CProjKey>, n, project.m_Depends) {

        // Prepare initial state of depends chains
        // one depend project in each chain
        const CProjKey& depend_id = *n;
        if ( !IsInAnyCycle(depend_id, *cycles) ) {
            TDependsChain one_chain;
            one_chain.push_back(depend_id);
            chains.push_back(one_chain);
        }
    }
    // Try to extend this chains
    TDependsChain cycle_found;
    bool cycles_found = ExtendChains(proj_id, tree, &chains, &cycle_found);
    if ( cycles_found ) {
        // Report chains as a cycles
        cycles->insert(cycle_found);
    }
}


bool CCyclicDepends::ExtendChains(const CProjKey&  proj_id, 
                                  const TProjects& tree,
                                  TDependsChains*  chains,
                                  TDependsChain*   cycle_found)
{
    for (TDependsChains::iterator p = chains->begin(); 
          p != chains->end();  ) {
        // Iterate through all chains
        TDependsChain& one_chain = *p;
        // we'll consider last element.
        const CProjKey& depend_id = one_chain.back();
        TProjects::const_iterator n = tree.find(depend_id);
        if (n == tree.end()) {
            //LOG_POST( Error << "Unknown project: " << depend_id.Id() );
            //return false;
            p = chains->erase(p);
            continue;
        }
        const CProjItem& depend_project = n->second;
        if ( depend_project.m_Depends.empty() ) {
            // If nobody depends from this project - remove this chain
            p = chains->erase(p);
        } else {
            // We'll create new chains 
            // by adding depend_project dependencies to old_chain
            TDependsChain old_chain = one_chain;
            p = chains->erase(p);

            ITERATE(list<CProjKey>, k, depend_project.m_Depends) {
                const CProjKey& new_depend_id = *k;
                // add each new depends to the end of the old_chain.
                TDependsChain new_chain = old_chain;
                new_chain.push_back(new_depend_id);
                p = chains->insert(p, new_chain);
                ++p;
            }
        }
    }
    // No chains - no cycles
    if ( chains->empty() )
        return false;
    // got cycles in chains - we done
    if ( IsCyclic(proj_id, *chains, cycle_found) )
        return true;
    // otherwise - continue search.
    return ExtendChains(proj_id, tree, chains, cycle_found);
}


bool CCyclicDepends::IsCyclic(const CProjKey&       proj_id, 
                              const TDependsChains& chains,
                              TDependsChain*        cycle_found)
{
    // First iteration - we'll try to find project to
    // consider inside depends chains. If we found - we have a cycle.
    ITERATE(TDependsChains, p, chains) {
        const TDependsChain& one_chain = *p;
        if (find(one_chain.begin(), 
                 one_chain.end(), 
                 proj_id) != one_chain.end()) {
            *cycle_found = one_chain;
            return true;
        }
    }

    // We look into all chais
    ITERATE(TDependsChains, p, chains) {
        TDependsChain one_chain = *p;
        TDependsChain original_chain = *p;
        // remember original size of chain
        size_t orig_size = one_chain.size();
        // remove all non-unique elements
        one_chain.sort();
        one_chain.unique();
        // if size of the chain is altered - we have a cycle.
        if (one_chain.size() != orig_size) {
            *cycle_found = original_chain;
            return true;
        }
    }
    
    // Got nothing - no cycles
    return false;
}


//-----------------------------------------------------------------------------
CProjectTreeFolders::CProjectTreeFolders(const CProjectItemsTree& tree)
:m_RootParent("/", NULL)
{
    ITERATE(CProjectItemsTree::TProjects, p, tree.m_Projects) {

        const CProjKey&  project_id = p->first;
        const CProjItem& project    = p->second;
        
        TPath path;
        CreatePath(GetApp().GetProjectTreeInfo().m_Src, 
                   project.m_SourcesBaseDir, 
                   &path);
        SProjectTreeFolder* folder = FindOrCreateFolder(path);
        folder->m_Name = path.back();
        folder->m_Projects.insert(project_id);
    }
}


SProjectTreeFolder* 
CProjectTreeFolders::CreateFolder(SProjectTreeFolder* parent,
                                  const string&       folder_name)
{
    m_Folders.push_back(SProjectTreeFolder(folder_name, parent));
    SProjectTreeFolder* inserted = &(m_Folders.back());

    parent->m_Siblings.insert
        (SProjectTreeFolder::TSiblings::value_type(folder_name, inserted));
    
    return inserted;

}

SProjectTreeFolder* 
CProjectTreeFolders::FindFolder(const TPath& path)
{
    SProjectTreeFolder& folder_i = m_RootParent;
    ITERATE(TPath, p, path) {
        const string& node = *p;
        SProjectTreeFolder::TSiblings::iterator n = 
            folder_i.m_Siblings.find(node);
        if (n == folder_i.m_Siblings.end())
            return NULL;
        folder_i = *(n->second);
    }
    return &folder_i;
}


SProjectTreeFolder* 
CProjectTreeFolders::FindOrCreateFolder(const TPath& path)
{
    SProjectTreeFolder* folder_i = &m_RootParent;
    ITERATE(TPath, p, path) {
        const string& node = *p;
        SProjectTreeFolder::TSiblings::iterator n = folder_i->m_Siblings.find(node);
        if (n == folder_i->m_Siblings.end()) {
            folder_i = CreateFolder(folder_i, node);
        } else {        
            folder_i = n->second;
        }
    }
    return folder_i;
}


void CProjectTreeFolders::CreatePath(const string& root_src_dir, 
                                     const string& project_base_dir,
                                     TPath*        path)
{
    path->clear();
    
    string rel_dir = 
        CDirEntry::CreateRelativePath(root_src_dir, project_base_dir);
    string sep(1, CDirEntry::GetPathSeparator());
    NStr::Split(rel_dir, sep, *path);
}


END_NCBI_SCOPE
