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
#include "msvc_prj_defines.hpp"

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

    if (!GetApp().IsCMakeMode()) {
        string dataspec = GetApp().GetDataspecProjId();
        string utility_projects_dir = GetApp().GetUtilityProjectsSrcDir();
        SLibProjectT::DoCreateDataSpec(
            utility_projects_dir, dataspec, dataspec, tree, eMakeType_Undefined);
    }

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
            if (d != m_Projects.end() /*&& d->second.m_External*/) {
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

/////////////////////////////////////////////////////////////////////////////

CMakeNode::CMakeNode(void)
{
}
CMakeNode::~CMakeNode()
{
}
CMakeNode::CMakeNode(const CMakeNode& other)
{
    m_NodeProjects = other.m_NodeProjects;
    m_NodeSubdirs = other.m_NodeSubdirs;
    m_NodeDefinitions = other.m_NodeDefinitions;
    m_NodeIncludes = other.m_NodeIncludes;
    m_NodeHeaders = other.m_NodeHeaders;
}
CMakeNode& CMakeNode::operator=(const CMakeNode& other)
{
    if (&other != this) {
        m_NodeProjects = other.m_NodeProjects;
        m_NodeSubdirs = other.m_NodeSubdirs;
        m_NodeDefinitions = other.m_NodeDefinitions;
        m_NodeIncludes = other.m_NodeIncludes;
        m_NodeHeaders = other.m_NodeHeaders;
    }
    return *this;
}

void CMakeNode::AddHeader(const string& name)
{
    m_NodeHeaders.push_back(name);
}

void CMakeNode::AddDefinition(const string& key, const string& value)
{
    m_NodeDefinitions.push_back(key + " " + value);
}

void CMakeNode::AddInclude(const string& name)
{
    m_NodeIncludes.push_back(name);
}

void CMakeNode::AddProject( const string& prj)
{
    m_NodeProjects.insert(prj);
}
void CMakeNode::AddSubdir( const string& dir)
{
    m_NodeSubdirs.insert(dir);
}

void CMakeNode::Write(const string& dirname) const
{
    CDir(dirname).CreatePath();
    string outname(CDirEntry::ConcatPath(dirname, "CMakeLists.txt"));
    CNcbiOfstream out(outname.c_str());
    if (!out.is_open()) {
        return;
    }

    ITERATE(vector<string>, s, m_NodeHeaders) {
        out << *s << endl;
    }
    for (vector<string>::const_iterator s = m_NodeDefinitions.begin();
        s != m_NodeDefinitions.end(); ++s) {
        out << "set(" << *s << ")" << endl;
    }
    ITERATE(vector<string>, s, m_NodeIncludes) {
        out << "include( " << *s << ")" << endl;
    }
    ITERATE(set<string>, s, m_NodeProjects) {
        out << "include( CMakeLists." << *s << ".txt)" << endl;
    }
    ITERATE(set<string>, s, m_NodeSubdirs) {
        out << "add_subdirectory(" << *s << ")" << endl;
    }
}

/////////////////////////////////////////////////////////////////////////////
CMakeProperty::CMakeProperty(const string& name)
    : m_Propname(name) {
}
CMakeProperty::~CMakeProperty(void) {
}
CMakeProperty::CMakeProperty(const CMakeProperty& other) {
    m_Propname = other.m_Propname;
    m_Propvalue = other.m_Propvalue;
}
CMakeProperty CMakeProperty::operator=(const CMakeProperty& other) {
    m_Propname = other.m_Propname;
    m_Propvalue = other.m_Propvalue;
    return *this;
}
CMakeProperty& CMakeProperty::AddValue(const string& value) {
    m_Propvalue.push_back(value);
    return *this;
}
void CMakeProperty::Write(CNcbiOstream& out) const {
    out << m_Propname  << "(" << endl;
    ITERATE( vector<string>, s, m_Propvalue) {
        out << "    " << *s << endl;
    }
    out << ")" << endl;
}

/////////////////////////////////////////////////////////////////////////////

CMakeProject::CMakeProject()
{
}

CMakeProject::~CMakeProject(void)
{
}
CMakeProject::CMakeProject(const CMakeProject& other)
{
    m_Prj_key = other.m_Prj_key;
    m_Definitions = other.m_Definitions;
    m_Sources = other.m_Sources;
    m_IncludeDir = other.m_IncludeDir;
    m_Libraries = other.m_Libraries;
    m_Dependencies = other.m_Dependencies;
    m_Properties = other.m_Properties;
}
CMakeProject& CMakeProject::operator=(const CMakeProject& other)
{
    if (&other != this) {
        m_Prj_key = other.m_Prj_key;
        m_Definitions = other.m_Definitions;
        m_Sources = other.m_Sources;
        m_IncludeDir = other.m_IncludeDir;
        m_Libraries = other.m_Libraries;
        m_Dependencies = other.m_Dependencies;
        m_Properties = other.m_Properties;
    }
    return *this;
}

void CMakeProject::SetProjKey(const CProjKey& prj_key)
{
    m_Prj_key = prj_key;
}
void CMakeProject::AddDefinition(const string& key, const string& value)
{
    m_Definitions.push_back( key + " " + value);
}
void CMakeProject::AddSourceFile(const string& folder, const string& name)
{
#if 0
    if (folder.empty()) {
        m_Sources.insert(name);
    } else {
        m_Sources.insert(CDirEntry::ConcatPath(folder, name));
    }
#else
    m_Sources[folder].insert(name);
#endif
}
void CMakeProject::AddIncludeDirectory(const string& name)
{
    m_IncludeDir.push_back(name);
}
void CMakeProject::AddLibrary(const string& name)
{
    m_Libraries.push_back(name);
}
void CMakeProject::AddDependency(const string& name)
{
    m_Dependencies.push_back(name);
}
void CMakeProject::AddProperty(const CMakeProperty& prop)
{
    m_Properties.push_back(prop);
}


void CMakeProject::Write(const string& dirname) const
{
    string prj_name(CreateProjectName(m_Prj_key));
    string prj_id(m_Prj_key.Id());
    if (m_Prj_key.Type() == CProjKey::eApp) {
        prj_id += ".exe";
    }
    CDir(dirname).CreatePath();
    string outname(CDirEntry::ConcatPath(dirname, "CMakeLists." + prj_name + ".txt"));
    CNcbiOfstream out(outname.c_str());
    if (!out.is_open()) {
        return;
    }

    for (vector<string>::const_iterator s = m_Definitions.begin();
        s != m_Definitions.end(); ++s) {
        out << "set(" << *s << ")" << endl;
    }
    out << endl;

    if (m_Prj_key.Type() == CProjKey::eLib) {
        if (prj_id == "general") {
            prj_id = "general-lib";
        }
        out << "add_library( "  << prj_id << " STATIC" << endl;
    } else if (m_Prj_key.Type() == CProjKey::eDll) {
        out << "add_library( " << prj_id << " SHARED" << endl;
    } else if (m_Prj_key.Type() == CProjKey::eApp) {
        out << "add_executable( " << prj_id << endl;
    } else {
        cerr << "unsupported project type: " << prj_name << endl;
        return;
    }
#if 0
    ITERATE (set<string>, s,  m_Sources) {
//        out << "    " << CDirEntry::CreateRelativePath(dirname, *s) << endl;
        out << "    " << *s << endl;
    }
#else
    for ( map<string, set<string> >::const_iterator s = m_Sources.begin();
            s != m_Sources.end(); ++s) {
        ITERATE (set<string>, n,  s->second) {
//            out << "    " << *n << endl;
            out << "    " << CDirEntry::ConcatPath(s->first, *n) << endl;
        }
    }
#endif
    out << ")" << endl;
#if 0
    for ( map<string, set<string> >::const_iterator s = m_Sources.begin();
            s != m_Sources.end(); ++s) {
        if (!s->second.empty()) {
            out << "set_source_files_properties(" << endl;
            ITERATE (set<string>, n,  s->second) {
                out << "    " << *n << endl;
            }
            out << "    PROPERTIES LOCATION " << s->first << ")" << endl;
        }
    }
#endif

#if 0
    if (prj_name != prj_id) {
        out << "set_target_properties(" << prj_name << endl <<
            " PROPERTIES OUTPUT_NAME " << prj_id << ")" << endl;
    }
#endif

    if (!m_IncludeDir.empty()) {
        out << "include_directories( " << endl;
        ITERATE (vector<string>, s,  m_IncludeDir) {
            out << "    " << *s << endl;
        }
        out << ")" << endl;
    }

    if (!m_Dependencies.empty()) {
        out << "add_dependencies( " << prj_id << endl;
        ITERATE (vector<string>, s,  m_Dependencies) {
            if (*s == "general") {
                out << "    " << "general-lib" << endl;
            } else {
                out << "    " << *s << endl;
            }
        }
        out << ")" << endl;
    }

    ITERATE( vector<CMakeProperty>, s, m_Properties) {
        s->Write(out);
    }

    out << "target_link_libraries( " << prj_id << endl;
    ITERATE (vector<string>, s,  m_Libraries) {
        if (*s == "general") {
            out << "    " << "general-lib" << endl;
        } else {
            out << "    " << *s << endl;
        }
    }
    out << ")" << endl;
}

/////////////////////////////////////////////////////////////////////////////

void CMakeGenerator::GenerateCMakeTree(CProjectItemsTree& projects_tree)
{
    CProjBulderApp& theApp = GetApp();
    string build_dir(CDirEntry(theApp.m_Solution).GetDir());
    string sln_dir(CDirEntry::ConcatPath(build_dir, "_cmake"));
    string src_root( CDirEntry::ConvertToOSPath(projects_tree.m_RootSrc));
    string separator;
    separator += CDirEntry::GetPathSeparator();
    map<string, CMakeNode> mkIn;
    map<string, CMakeProject> mkPrj;

// root node definitions
    {
        string unix_cfg = theApp.GetConfig().Get( CMsvc7RegSettings::GetMsvcSection(),"MetaData");
        if (!unix_cfg.empty()) {
            unix_cfg = CDirEntry::ConcatPath(build_dir,unix_cfg);
            if (CFile(unix_cfg).Exists()) {
                CSimpleMakeFileContents unixDef;
                CSimpleMakeFileContents::LoadFrom(unix_cfg,&unixDef);
                string bldType;
                if (unixDef.GetValue("DEBUG_SFX", bldType)) {
                    mkIn[""].AddDefinition("CMAKE_BUILD_TYPE", bldType);
                }
            }
        }
        mkIn[""].AddHeader("cmake_minimum_required(VERSION 2.8)");
        mkIn[""].AddDefinition("NCBI_TREE_ROOT",
            CDirEntry::DeleteTrailingPathSeparator(
                CDirEntry::ConcatPath("${CMAKE_CURRENT_SOURCE_DIR}",
                    CDirEntry::CreateRelativePath(sln_dir, theApp.m_Root))));
        mkIn[""].AddDefinition("NCBI_BUILD_ROOT",
            CDirEntry::DeleteTrailingPathSeparator(
                CDirEntry::ConcatPath("${CMAKE_CURRENT_SOURCE_DIR}",
                    CDirEntry::CreateRelativePath(sln_dir, build_dir))));
        mkIn[""].AddDefinition( "NCBI_CMAKE_ROOT", "${CMAKE_CURRENT_SOURCE_DIR}");
        mkIn[""].AddDefinition("CMAKE_MODULE_PATH",
                "${CMAKE_MODULE_PATH} ${NCBI_TREE_ROOT}/src/build-system/cmake");

        mkIn[""].AddInclude("CMakeLists.defaults");
    }


    ITERATE(CProjectItemsTree::TProjects, p, projects_tree.m_Projects) {
        const CProjItem& prj = p->second;
        if (prj.m_MakeType >= eMakeType_Expendable) {
// expendables can and do fail to compile
            continue;
        }
        string relpath =
            CDirEntry::DeleteTrailingPathSeparator(
                CDirEntry::CreateRelativePath(src_root, 
                    CDirEntry::ConvertToOSPath(prj.m_SourcesBaseDir)));
        list<string> pathnodes;
        NStr::Split(relpath, separator, pathnodes);

// subdirectories/ build tree
        string curpath;
        ITERATE( list<string>, n, pathnodes) {
            mkIn[curpath].AddSubdir(*n);
            if (!curpath.empty()) {
                curpath += separator;
            }
            curpath += *n;
        }
        
        string prj_name(CreateProjectName( p->first));
        string prj_path(CDirEntry::ConcatPath(relpath,prj_name));

        mkIn[ relpath].AddProject( prj_name);
        mkPrj[prj_path].SetProjKey( p->first);

// sources 
        mkPrj[prj_path].AddDefinition("NCBI_PROJECT_SRC_DIR", "${NCBI_TREE_ROOT}/src/" + relpath);
        mkPrj[prj_path].AddDefinition("NCBI_PROJECT_BUILD_DIR", "${NCBI_BUILD_ROOT}/" + relpath);
        mkPrj[prj_path].AddDefinition("srcdir", "${NCBI_PROJECT_SRC_DIR}");
        
        list<string> prj_sources;
        if (CMsvc7RegSettings::GetMsvcPlatform() == CMsvc7RegSettings::eUnix) {
            prj.m_DataSource.GetValue("UNIX_SRC", prj_sources);
        }
        prj_sources.insert( prj_sources.end(), prj.m_Sources.begin(), prj.m_Sources.end());
        bool smth_by_datatool = false;
        string location_default("${NCBI_PROJECT_SRC_DIR}");
        ITERATE( list<string>, s, prj_sources) {
            string relname = CMsvcSite::ToOSPath(*s);
            string name = CDirEntry::ConcatPath(prj.m_SourcesBaseDir, relname);
            if (s->at(0) != '@' && s->at(0) != '$') {
                bool isfound = false;
                string location(location_default);
                string ext = SourceFileExt(name);
                if (NStr::EndsWith(ext, ".in")) {
                    name = CDirEntry::ConcatPath(CDirEntry::ConcatPath(build_dir, relpath), *s);
                    relname = CDirEntry(name).GetName();
                    location = "${NCBI_PROJECT_BUILD_DIR}";
                    isfound = true;
                } else if (!prj.m_DatatoolSources.empty()) {
                    const CDataToolGeneratedSrc *pFound = IsProducedByDatatool(name, prj);
                    if (pFound) {
                        smth_by_datatool = true;
                        location = CDirEntry::ConcatPath(location,
                            CDirEntry::CreateRelativePath(prj.m_SourcesBaseDir,
                                                          pFound->m_SourceBaseDir));
                        isfound = true;
                        mkPrj[prj_path].AddProperty(
                            CMakeProperty("set_source_files_properties")
                                .AddValue(CDirEntry::ConcatPath(location, relname + ".cpp") +
                                    " PROPERTIES GENERATED 1"));
                    }
                }
                isfound = isfound || CFile(name + ext).Exists();
                if (!isfound) {
                    mkPrj[prj_path].AddProperty(
                        CMakeProperty("set_source_files_properties")
                            .AddValue(CDirEntry::ConcatPath(location, relname + ".cpp") +
                                " PROPERTIES GENERATED 1"));
                }
                mkPrj[prj_path].AddSourceFile(location, relname);
#if 0
            } else if (CSymResolver::IsDefine(*s)) {
//                mkPrj[prj_path].AddSourceFile( location_default, "${" + CSymResolver::StripDefine(FilterDefine(*s)) + "}");
cerr << "Unhandled source: " << *s << " in " << prj.m_Name << endl;
            } else if (SMakeProjectT::IsConfigurableDefine(*s)) {
//                mkPrj[prj_path].AddSourceFile( location_default, "${" + SMakeProjectT::StripConfigurableDefine(*s) + "}");
cerr << "Unhandled source: " << *s << " in " << prj.m_Name << endl;
            } else {
cerr << "Unhandled source: " << *s << " in " << prj.m_Name << endl;
#endif
            }
        }
// datatool custom build
        if (smth_by_datatool) {
            ITERATE(list<CDataToolGeneratedSrc>, d, prj.m_DatatoolSources) {
                CMakeProperty cust_build("add_custom_command");
                string dataspec_module(CDirEntry(d->m_SourceFile).GetBase());
                string location =
                    CDirEntry::ConcatPath( CDirEntry::ConcatPath(
                        "${NCBI_PROJECT_SRC_DIR}",
                            CDirEntry::CreateRelativePath(prj.m_SourcesBaseDir,
                                                          d->m_SourceBaseDir)),
                            dataspec_module);
                string output;
                output  = "OUTPUT";
                output += " " + location + ".files";
                output += " " + location + "__.cpp";
                output += " " + location + "___.cpp";
                cust_build.AddValue(output);
                string command;
                command  = "COMMAND";
                command += " ${NCBI_DATATOOL}";
                command += " -oR ${NCBI_TREE_ROOT}";
                command += " -opm  ${NCBI_TREE_ROOT}/src";
                command += " -m " + location + CDirEntry(d->m_SourceFile).GetExt();
                command += " -M \\\"" + NStr::Join(d->m_ImportModules, " ") + "\\\"";
                command += " -oA ";
                command += " -oc " + dataspec_module;
                command += " -or " + relpath;
                command += " -odi -od " + location + ".def";
                command += " -oex \\\"\\\" -ocvs -pch ncbi_pch.hpp";
                cust_build.AddValue(command);
                string comment("COMMENT");
                comment += " \"Using datatool to generate C++ classes from " + d->m_SourceFile + "\"";
                cust_build.AddValue(comment);
                string depends("DEPENDS");
                depends += " " + location + CDirEntry(d->m_SourceFile).GetExt();
                if (CDirEntry(CDirEntry::ConcatPath(d->m_SourceBaseDir, dataspec_module + ".def")).Exists()) {
                    depends += " " + location + ".def";
                }
                cust_build.AddValue(depends);
                mkPrj[prj_path].AddProperty(cust_build);
            }
        }
// dependencies
        vector<string> to_process;
        list<string> tmp_list;
        string value;
        for (int deptype=0; deptype<2; ++deptype) {
// deptype = 0  -- libraries
// deptype = 1  -- other dependencies
            list<string> prj_libs;
            to_process.clear();
            if (deptype == 0) {
                if (p->first.Type() == CProjKey::eLib ||
                    p->first.Type() == CProjKey::eDll) {
#if 0
// this works
                    to_process.push_back("DLL_LIB");
                    to_process.push_back("LIBS");
#else
// this could be better.. or not..
//                    to_process.push_back("USES_LIBRARIES");
#endif
                } else if (p->first.Type() == CProjKey::eApp) {
                    to_process.clear();
                    to_process.push_back("LIB");
                    to_process.push_back("LIBS");
                }
            } else if (deptype == 1) {
                to_process.push_back("ASN_DEP");
//                to_process.push_back("USR_DEP");
            }
            ITERATE(vector<string>, top, to_process) {
                if (prj.m_DataSource.GetValue(*top, tmp_list)) {
                    ITERATE( list<string>, s, tmp_list) {
                        if (CSymResolver::IsDefine(*s)) {
                            value = CSymResolver::StripDefine(FilterDefine(*s));
                            prj_libs.push_back( "${" + value + "}");
                        } else if (CSymResolver::HasDefine(*s)) {
                            string key = FilterDefine(*s);
                            if (CSymResolver::IsDefine(key)) {
                                key = CSymResolver::StripDefine(key);
                                if (prj.m_DataSource.GetValue(key, value)) {
                                    if (CSymResolver::HasDefine(value)) {
                                        NStr::ReplaceInPlace(value, "$(", "${");
                                        NStr::ReplaceInPlace(value, ")", "}");
                                    }
                                    if (NStr::FindCase( value,"general") != NPOS) {
                                        list<string> vv_lst;
                                        NStr::Split(value, LIST_SEPARATOR_LIBS, vv_lst);
                                        NON_CONST_ITERATE( list<string>, vv, vv_lst) {
                                            if (*vv == "general") {
                                                *vv = "general-lib";
                                            }
                                        }
                                        value = NStr::Join(vv_lst, " ");
                                    }
                                    mkPrj[prj_path].AddDefinition(key, value);
                                }
                                prj_libs.push_back( "${" + key + "}");
                            } else {
                                prj_libs.push_back( CSymResolver::TrimDefine(*s));
                            }
                        } else if (SMakeProjectT::IsConfigurableDefine(*s)) {
                            value = SMakeProjectT::StripConfigurableDefine(*s);
                            prj_libs.push_back( "${" + value + "}");
                        } else if (SMakeProjectT::HasConfigurableDefine(*s)) {
cerr << "Found ConfigurableDefine: " << *s << " in " << prj.m_Name << endl;
                        } else if (NStr::StartsWith(*s, "-l")) {
                            prj_libs.push_back( s->substr(2));
                        } else {
                            prj_libs.push_back( *s);
                        }
                    }
                }
                else if (deptype == 0 && *top == "LIBS") {
                    prj_libs.push_back("${ORIG_LIBS}");
                }
            }
            ITERATE( list<string>, d, prj_libs) {
                if (deptype == 0) {
                    mkPrj[prj_path].AddLibrary( *d);
                } else if (deptype == 1) {
                    mkPrj[prj_path].AddDependency( *d);
                }
            }
        }

//include dirs
        to_process.clear();
        to_process.push_back("CPPFLAGS");
        ITERATE(vector<string>, top, to_process) {
            prj.m_DataSource.GetValue(*top, tmp_list);
            vector<string> defines;
            if (tmp_list.empty()) {
                defines.push_back("${ORIG_CPPFLAGS}");
            }
            ITERATE( list<string>, s, tmp_list) {
                if (CSymResolver::IsDefine(*s)) {
                    value = CSymResolver::StripDefine(FilterDefine(*s));
                    if (NStr::EndsWith(value, "_INCLUDE")) {
                        mkPrj[prj_path].AddIncludeDirectory("${" + value + "}");
// also look for additional definitions
                        const CMsvcSite& site = GetApp().GetSite();
                        string resolved;
                        site.ResolveDefine(value, resolved);
                        list<string> resolved_list;
                        NStr::Split(resolved, " ", resolved_list);
                        ITERATE( list<string>, r, resolved_list) {
                            if (NStr::StartsWith(*r, "-D")) {
                                defines.push_back(*r);
                            }
                        }

                    }
                    else {
                        defines.push_back("${" + value + "}");
                    }
                } else if (CSymResolver::HasDefine(*s)) {
//cerr << "Found smth has define: " << *s << " in " << prj.m_Name << endl;
                    if (NStr::StartsWith(*s, "-I")) {
                        string val = SMakeProjectT::GetOneIncludeDir(*s, "-I");
#if 0
                        if (NStr::StartsWith(val,"$(srcdir)")) {
                            mkPrj[prj_path].AddIncludeDirectory(NStr::Replace(val, "$(srcdir)", "${NCBI_PROJECT_SRC_DIR}"));
                        } else if (NStr::StartsWith(val,"$(top_srcdir)")) {
                            mkPrj[prj_path].AddIncludeDirectory(NStr::Replace(val, "$(top_srcdir)", "${NCBI_TREE_ROOT}"));
                        }
#else
                        NStr::ReplaceInPlace(val,"$(", "${");
                        NStr::ReplaceInPlace(val,")", "}");
                        mkPrj[prj_path].AddIncludeDirectory(val);
#endif
                    }
                } else if (NStr::StartsWith(*s, "-D")) {
                    defines.push_back(*s);
                }
            }
            if (!defines.empty()) {
                CMakeProperty def( "add_definitions");
                ITERATE( vector<string>, d, defines) {
                    def.AddValue( *d);
                }
                mkPrj[prj_path].AddProperty( def);
            }
        }

// tests
        string prj_id0(p->first.Id());
        string prj_id(p->first.Id());
        if (p->first.Type() == CProjKey::eApp) {
            prj_id += ".exe";
        }
        if (prj.m_DataSource.GetValue("CHECK_COPY", tmp_list)) {
            list<string> ttmp;
            ITERATE( list<string>, s, tmp_list) {
                if (CDirEntry(CDirEntry::ConcatPath(prj.m_SourcesBaseDir, *s)).Exists()) {
                    ttmp.push_back(*s);
                }
            }
            tmp_list = ttmp;
            if (!tmp_list.empty()) {
                CMakeProperty cust_build("add_custom_command");
                cust_build.AddValue("TARGET " +  prj_id + " POST_BUILD");
                ITERATE( list<string>, s, tmp_list) {
                    cust_build.AddValue( "COMMAND  test -e ${NCBI_PROJECT_SRC_DIR}/"+ *s + " && cp -rf ${NCBI_PROJECT_SRC_DIR}/" + *s + " ${NCBI_PROJECT_BUILD_DIR}/");
                }
                mkPrj[prj_path].AddProperty(cust_build);
            }
        }
        if (prj.m_DataSource.GetValue("CHECK_CMD", tmp_list)) {
            {
                CMakeProperty cust_build("add_custom_command");
                cust_build.AddValue("TARGET " +  prj_id + " POST_BUILD");
                cust_build.AddValue( "COMMAND  cp -rf ${NCBI_BUILD_BIN}/" + prj_id + " ${NCBI_PROJECT_BUILD_DIR}/" + prj_id0);
                mkPrj[prj_path].AddProperty(cust_build);
            }
            set<string> checks;
            if (tmp_list.empty()) {
                tmp_list.push_back(prj_id);
            }
            ITERATE( list<string>, s, tmp_list) {
                value = *s;
                string check_name("/CHECK_NAME=");
                string::size_type chkn = s->find(check_name);
                if (chkn != string::npos) {
                    value = s->substr(0, chkn);
                    check_name = s->substr( chkn + check_name.length());
                    NStr::ReplaceInPlace(check_name, " ", "_");
                } else {
                    if (value.empty()) {
                        value = prj_id0;
                    }
                    check_name = prj_id;
                }
                while (checks.find(check_name) != checks.end()) {
                    check_name += NStr::NumericToString( checks.size());
                }
                checks.insert(check_name);
                mkPrj[prj_path].AddProperty(
                    CMakeProperty("add_test")
                        .AddValue("NAME " + check_name)
                        .AddValue("WORKING_DIRECTORY ${NCBI_PROJECT_BUILD_DIR}")
                        .AddValue("COMMAND " + value));
            }
        }
    }

// done
    for (map<string,CMakeNode >::const_iterator n = mkIn.begin(); n != mkIn.end(); ++n) {
        n->second.Write( CDirEntry::ConcatPath(sln_dir, n->first));
    }

    for (map<string,CMakeProject >::const_iterator n = mkPrj.begin(); n != mkPrj.end(); ++n) {
        n->second.Write( CDirEntry(CDirEntry::ConcatPath(sln_dir, n->first)).GetDir());
    }
}

/////////////////////////////////////////////////////////////////////////////
void CMakefilePatch::PatchTreeMakefiles(const CProjectItemsTree& projects_tree)
{
    const string keyword("USES_LIBRARIES = ");
    size_t count = 0;
    CProjBulderApp& theApp = GetApp();
    ITERATE(CProjectItemsTree::TProjects, p, projects_tree.m_Projects) {
        const CProjKey& prj_key = p->first;
// definitely I should not modify APP projects
// I am not sure about DLL ones
        if (prj_key.Type() != CProjKey::eLib) {
            continue;
        }
        const CProjItem& prj = p->second;
        map<string, set<string> >::const_iterator deps = theApp.m_GraphDepPrecedes.find(prj_key.Id());
        if (deps != theApp.m_GraphDepPrecedes.end()) {
            if (!deps->second.empty()) {

cout << "Modify " << prj.m_MkName << endl;
// remove previous definition
                {
                    bool modified = false;
                    string tmpname = CDirEntry::GetTmpName();
                    CNcbiIfstream ifs(prj.m_MkName.c_str(), IOS_BASE::in | IOS_BASE::binary );
                    if (ifs.is_open()) {
                        CNcbiOfstream ofs(tmpname.c_str(), IOS_BASE::out | IOS_BASE::trunc );
                        if (ofs.is_open()) {
                            string strline;
                            bool found = false;
                            while ( NcbiGetlineEOL(ifs, strline) ) {
                                if (found) {
                                    found = NStr::EndsWith(strline, "\\");
                                    continue;
                                }
                                if (NStr::Find(strline, keyword) != NPOS) {
                                    modified = true;
                                    found = NStr::EndsWith(strline, "\\");
                                    continue;
                                }
                                ofs << strline << endl;
                            }
                        }
                        ifs.close();
                        if (modified) {
                            if (!CDirEntry(tmpname).Copy(prj.m_MkName , CDirEntry::fCF_Overwrite)) {
cout << "Error while modiying " << prj.m_MkName << endl << CNcbiError::GetLast() << endl;
                            }
                        }
                        CDirEntry(tmpname).Remove();
                    }
                }
// add new one
                {
                    CNcbiOfstream ofs(prj.m_MkName.c_str(), IOS_BASE::out | IOS_BASE::app );
                    if (ofs.is_open()) {
                        ofs << keyword;
                        string libdep;
                        size_t len=80;
                        ITERATE( set<string>, s, deps->second) {
#if 0
                            CProjKey t(CProjKey::eLib, *s);
                            if (projects_tree.m_Projects.find(t) == projects_tree.m_Projects.end()) {
cout << "WARNING: library not found: " << *s << endl;
                            }
#endif
                            libdep += " " + *s;
		                    if (len > 60) {
        	                    ofs << " \\" << endl << "   ";
		                        len = 0;
		                    }
                            ofs << " " << *s;
                            len += s->size() + 1;
                        }
                        ofs << endl;
cout << "USES_LIBRARIES =" << libdep << endl;
                    }
                }
            }
            ++count;
        }
    }
cout << "Count files to modify: " << count << endl;
}


END_NCBI_SCOPE
