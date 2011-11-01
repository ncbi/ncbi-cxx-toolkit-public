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
#include "stl_msvc_usage.hpp"
#include "msvc_sln_generator.hpp"
#include "msvc_prj_utils.hpp"
#include <corelib/ncbifile.hpp>
#include <corelib/ncbistr.hpp>
#include "proj_builder_app.hpp"
#include "msvc_prj_defines.hpp"
#include "msvc_makefile.hpp"
#include "ptb_err_codes.hpp"
#include "proj_tree_builder.hpp"

BEGIN_NCBI_SCOPE
#if NCBI_COMPILER_MSVC

//------------------------------------------------------------------------------
//  CPrjContext

CPrjContext::CPrjContext(void)
{
    Clear();
}


CPrjContext::CPrjContext(const CPrjContext& context)
{
    SetFrom(context);
}


CPrjContext::CPrjContext(const CProjItem& project)
    :m_Project(project)
{
    m_GUID = project.m_GUID;

    CMsvcPrjProjectContext project_context(project);
    if (project.m_ProjType == CProjKey::eMsvc) {
        m_ProjectName = project_context.ProjectName();

        string project_rel_path = project.m_Sources.front();
        m_ProjectPath = CDirEntry::ConcatPath(project.m_SourcesBaseDir, 
                                             project_rel_path);
        m_ProjectPath = CDirEntry::NormalizePath(m_ProjectPath);

    } else {
        m_ProjectName = project_context.ProjectName();
        m_ProjectPath = CDirEntry::ConcatPath(project_context.ProjectDir(),
                                              project_context.ProjectName());
        m_ProjectPath += CMsvc7RegSettings::GetVcprojExt();
    }
}


CPrjContext& CPrjContext::operator= (const CPrjContext& context)
{
    if (this != &context) {
        Clear();
        SetFrom(context);
    }
    return *this;
}


CPrjContext::~CPrjContext(void)
{
    Clear();
}


void CPrjContext::Clear(void)
{
    //TODO
}

void CPrjContext::SetFrom(const CPrjContext& project_context)
{
    m_Project     = project_context.m_Project;

    m_GUID        = project_context.m_GUID;
    m_ProjectName = project_context.m_ProjectName;
    m_ProjectPath = project_context.m_ProjectPath;
}

//-----------------------------------------------------------------------------
// CUtilityProject

CUtilityProject::CUtilityProject(
    const string& full_path, const string& guid, const string& name)
    : m_HasFilter(false), m_SaveEmpty(true),
      m_Full_path(full_path), m_Guid(guid), m_Name(name)
{
}

CUtilityProject::~CUtilityProject(void)
{
}

bool CUtilityProject::IsIncluded(const CPrjContext& /*prj*/) const
{
    return false;
}

//-----------------------------------------------------------------------------
class CUtilityBuildAllProject : public CUtilityProject
{
public:
    CUtilityBuildAllProject(
        const string& full_path, const string& guid, const string& name)
        : CUtilityProject(full_path, guid, name)
    {
        m_HasFilter = true;
    }
    virtual bool IsIncluded(const CPrjContext& prj) const
    {
        if (prj.m_Project.m_MakeType == eMakeType_Excluded) {
            _TRACE("For reference only: " << prj.m_ProjectName);
            return false;
        }
        if (prj.m_Project.m_MakeType == eMakeType_ExcludedByReq) {
            PTB_WARNING_EX(prj.m_ProjectName, ePTB_ProjectExcluded,
                           "Excluded due to unmet requirements");
            return false;
        }
        return true;
    }
};

//-----------------------------------------------------------------------------
class CUtilityAsnAllProject : public CUtilityProject
{
public:
    CUtilityAsnAllProject(
        const string& full_path, const string& guid, const string& name)
        : CUtilityProject(full_path, guid, name)
    {
        m_HasFilter = true;
    }
    virtual bool IsIncluded(const CPrjContext& prj) const
    {
        return !prj.m_Project.m_DatatoolSources.empty();
    }
};

//-----------------------------------------------------------------------------
class CUtilityLibsAllProject : public CUtilityProject
{
public:
    CUtilityLibsAllProject(
        const string& full_path, const string& guid, const string& name)
        : CUtilityProject(full_path, guid, name)
    {
        m_HasFilter = true;
        m_SaveEmpty = false;
    }
    virtual bool IsIncluded(const CPrjContext& prj) const
    {
        return prj.m_Project.m_ProjType == CProjKey::eLib ||
               prj.m_Project.m_ProjType == CProjKey::eDll;
    }
};

//---------------------------------------------------------------------------
class CUtilityTagProject : public CUtilityProject
{
public:
    CUtilityTagProject(
        const string& full_path, const string& guid, const string& name, const string& tags)
        : CUtilityProject(full_path, guid, name), m_Tags(tags)
    {
        m_HasFilter = true;
        m_SaveEmpty = false;
    }
    virtual bool IsIncluded(const CPrjContext& prj) const
    {
        return GetApp().IsAllowedProjectTag(prj.m_Project, &m_Tags);
    }
private:
    string m_Tags;
};

//-----------------------------------------------------------------------------
// CMsvcSolutionGenerator

CMsvcSolutionGenerator::CMsvcSolutionGenerator
                                            (const list<SConfigInfo>& configs)
    :m_Configs(configs)
{
}


CMsvcSolutionGenerator::~CMsvcSolutionGenerator(void)
{
}


void 
CMsvcSolutionGenerator::AddProject(const CProjItem& project)
{
    m_Projects[CProjKey(project.m_ProjType, 
                        project.m_ID)] = CPrjContext(project);
}

void CMsvcSolutionGenerator::AddUtilityProject(
    const string& full_path, const string& guid, const string& name)
{
    if (guid.empty()) {
        return;
    }
    m_Utils.push_back(
        CRef<CUtilityProject>(new CUtilityProject(full_path, guid, name)));
}


void 
CMsvcSolutionGenerator::AddConfigureProject(
    const string& full_path, const string& guid, const string& name)
{
    if (guid.empty()) {
        return;
    }
    m_Utils.push_back(
        CRef<CUtilityProject>(new CUtilityProject(full_path, guid, name)));
}


void 
CMsvcSolutionGenerator::AddBuildAllProject(
    const string& full_path, const string& guid, const string& name)
{
    if (guid.empty()) {
        return;
    }
    m_Utils.push_back(
        CRef<CUtilityProject>(new CUtilityBuildAllProject(full_path, guid, name)));
}

void 
CMsvcSolutionGenerator::AddAsnAllProject(
    const string& full_path, const string& guid, const string& name)
{
    if (guid.empty()) {
        return;
    }
    m_Utils.push_back(
        CRef<CUtilityProject>(new CUtilityAsnAllProject(full_path, guid, name)));
}

void
CMsvcSolutionGenerator::AddLibsAllProject(
    const string& full_path, const string& guid, const string& name)
{
    if (guid.empty()) {
        return;
    }
    m_Utils.push_back(
        CRef<CUtilityProject>(new CUtilityLibsAllProject(full_path, guid, name)));
}

void CMsvcSolutionGenerator::AddTagProject(
    const string& full_path, const string& guid, const string& name,
    const string& tags)
{
    if (guid.empty()) {
        return;
    }
    m_Utils.push_back(
        CRef<CUtilityProject>(new CUtilityTagProject(full_path, guid, name, tags)));
}

void CMsvcSolutionGenerator::VerifyProjectDependencies(void)
{
    for (bool changed=true; changed;) {
        changed = false;
        NON_CONST_ITERATE(TProjects, p, m_Projects) {
            CPrjContext& project = p->second;
            if (project.m_Project.m_MakeType == eMakeType_ExcludedByReq) {
                continue;
            }
            ITERATE(list<CProjKey>, p, project.m_Project.m_Depends) {
                const CProjKey& id = *p;
                if ( id.Type() == CProjKey::eLib &&
                     GetApp().GetSite().IsLibWithChoice(id.Id()) &&
                     GetApp().GetSite().GetChoiceForLib(id.Id()) == CMsvcSite::e3PartyLib ) {
                        continue;
                }
                TProjects::const_iterator n = m_Projects.find(id);
                if (n == m_Projects.end()) {
                    CProjKey id_alt(CProjKey::eMsvc,id.Id());
                    n = m_Projects.find(id_alt);
                }
                if (n != m_Projects.end()) {
                    const CPrjContext& prj_i = n->second;
                    if (prj_i.m_Project.m_MakeType == eMakeType_ExcludedByReq) {
                        project.m_Project.m_MakeType = eMakeType_ExcludedByReq;
                        PTB_WARNING_EX(project.m_ProjectPath,
                            ePTB_ConfigurationError,
                            "Project excluded because of dependent project: " <<
                            prj_i.m_ProjectName);
                        changed = true;
                        break;
                    }
                }
            }
        }
    }
}

void 
CMsvcSolutionGenerator::SaveSolution(const string& file_path)
{
    VerifyProjectDependencies();

    CDirEntry::SplitPath(file_path, &m_SolutionDir);

    // Create dir for output sln file
    CDir(m_SolutionDir).CreatePath();

    CNcbiOfstream  ofs(file_path.c_str(), IOS_BASE::out | IOS_BASE::trunc);
    if ( !ofs )
        NCBI_THROW(CProjBulderAppException, eFileCreation, file_path);

    GetApp().RegisterGeneratedFile( file_path );
    // Start sln file
    ofs << MSVC_SOLUTION_HEADER_LINE
        << GetApp().GetRegSettings().GetSolutionFileFormatVersion() << endl;

    list<string> proj_guid;
    // Utility projects
    ITERATE(vector< CRef<CUtilityProject> >, p, m_Utils) {
        const CUtilityProject& utl_prj = **p;
        if (WriteUtilityProject(utl_prj, ofs)) {
            proj_guid.push_back(utl_prj.Guid());
        }
    }

    // Projects from the projects tree
    ITERATE(TProjects, p, m_Projects) {
        proj_guid.push_back(p->second.m_GUID);
        if (p->second.m_Project.m_MakeType == eMakeType_ExcludedByReq) {
            continue;
        }
        WriteProjectAndSection(ofs, p->second);
    }

    // Start "Global" section
    ofs << "Global" << endl;
	
    // Write all configurations
    if (CMsvc7RegSettings::GetMsvcVersion() == CMsvc7RegSettings::eMsvc710) {
        ofs << '\t' << "GlobalSection(SolutionConfiguration) = preSolution" << endl;
    } else {
        ofs << '\t' << "GlobalSection(SolutionConfigurationPlatforms) = preSolution" << endl;
    }
    ITERATE(list<SConfigInfo>, p, m_Configs) {
        string config = (*p).GetConfigFullName();
        if (CMsvc7RegSettings::GetMsvcVersion() > CMsvc7RegSettings::eMsvc710) {
            config = ConfigName(config);
        }
        ofs << '\t' << '\t' << config << " = " << config << endl;
    }
    ofs << '\t' << "EndGlobalSection" << endl;
    
    if (CMsvc7RegSettings::GetMsvcVersion() > CMsvc7RegSettings::eMsvc710) {
        ofs << '\t' << "GlobalSection(ProjectConfigurationPlatforms) = postSolution" << endl;
    } else {
        ofs << '\t' << "GlobalSection(ProjectConfiguration) = postSolution" << endl;
    }

//    proj_guid.sort();
//    proj_guid.unique();
    WriteProjectConfigurations( ofs, proj_guid);
    ofs << '\t' << "EndGlobalSection" << endl;

    //End of global section
    ofs << "EndGlobal" << endl;
}

void CMsvcSolutionGenerator::CollectLibToLibDependencies(
        set<string>& dep, set<string>& visited,
        const CPrjContext& lib, const CPrjContext& lib_dep)
{
    if (GetApp().m_AllDllBuild) {
        dep.insert(lib_dep.m_GUID);
        return;
    }
    if (visited.find(lib_dep.m_GUID) != visited.end() ||
        lib_dep.m_GUID == lib.m_GUID) {
        return;
    }
    visited.insert(lib_dep.m_GUID);
    if (!lib_dep.m_Project.m_DatatoolSources.empty() ||
        !lib_dep.m_Project.m_ExportHeaders.empty() ||
        lib.m_Project.m_UnconditionalDepends.find(
            CProjKey(lib_dep.m_Project.m_ProjType, lib_dep.m_Project.m_ID)) !=
            lib.m_Project.m_UnconditionalDepends.end()) {
        dep.insert(lib_dep.m_GUID);
    }
    ITERATE(list<CProjKey>, p, lib_dep.m_Project.m_Depends) {
        if (p->Type() == CProjKey::eLib) {
            TProjects::const_iterator n = m_Projects.find(*p);
            if (n != m_Projects.end()) {
                CollectLibToLibDependencies(dep, visited, lib, n->second);
            }
        }
    }
}

void 
CMsvcSolutionGenerator::WriteProjectAndSection(CNcbiOfstream&     ofs, 
                                               const CPrjContext& project)
{
    ofs << "Project(\"" 
        << MSVC_SOLUTION_ROOT_GUID 
        << "\") = \"" 
        << project.m_ProjectName 
        << "\", \"";

    ofs << CDirEntry::CreateRelativePath(m_SolutionDir, project.m_ProjectPath)
        << "\", \"";

    ofs << project.m_GUID 
        << "\"" 
        << endl;

    list<string> proj_guid;
    set<string> lib_guid, visited;

    ITERATE(list<CProjKey>, p, project.m_Project.m_Depends) {

        const CProjKey& id = *p;

        if (CMsvc7RegSettings::GetMsvcVersion() == CMsvc7RegSettings::eMsvc710) {
            // Do not generate lib-to-lib depends.
            if (project.m_Project.m_ProjType == CProjKey::eLib  &&
                id.Type() == CProjKey::eLib) {
                continue;
            }
        }
        if ( GetApp().GetSite().IsLibWithChoice(id.Id()) ) {
            if ( GetApp().GetSite().GetChoiceForLib(id.Id()) == CMsvcSite::e3PartyLib ) {
                continue;
            }
        }
        TProjects::const_iterator n = m_Projects.find(id);
        if (n == m_Projects.end()) {
// also check user projects
            CProjKey id_alt(CProjKey::eMsvc,id.Id());
            n = m_Projects.find(id_alt);
            if (n == m_Projects.end()) {
                if (!SMakeProjectT::IsConfigurableDefine(id.Id())) {
                    PTB_WARNING_EX(project.m_ProjectName, ePTB_MissingDependency,
                                "Project " << project.m_ProjectName
                                << " depends on missing project " << id.Id());
                }
                continue;
            }
        }
        const CPrjContext& prj_i = n->second;
        if (project.m_Project.m_ProjType == CProjKey::eLib  &&
            id.Type() == CProjKey::eLib) {
            CollectLibToLibDependencies(lib_guid, visited, project, prj_i);
            continue;
        }
        if (project.m_GUID == prj_i.m_GUID) {
            continue;
        }
        proj_guid.push_back(prj_i.m_GUID);
    }
    copy(lib_guid.begin(), lib_guid.end(), back_inserter(proj_guid));
    if (!proj_guid.empty()) {
        proj_guid.sort();
        proj_guid.unique();
        ofs << '\t' << "ProjectSection(ProjectDependencies) = postProject" << endl;
        ITERATE(list<string>, p, proj_guid) {
            ofs << '\t' << '\t' << *p << " = " << *p << endl;
        }
        ofs << '\t' << "EndProjectSection" << endl;
    }
    ofs << "EndProject" << endl;
}

void CMsvcSolutionGenerator::BeginUtilityProject(
    const CUtilityProject& project, CNcbiOfstream& ofs)
{
    string name = project.Name();
    ofs << "Project(\"" 
        << MSVC_SOLUTION_ROOT_GUID
        << "\") = \"" 
        << name
        << "\", \"";

    ofs << CDirEntry::CreateRelativePath(m_SolutionDir, project.Path())
        << "\", \"";

    ofs << project.Guid()
        << "\"" 
        << endl;
}
void CMsvcSolutionGenerator::EndUtilityProject(
    const CUtilityProject& /*project*/, CNcbiOfstream& ofs)
{
    ofs << "EndProject" << endl;
}
bool CMsvcSolutionGenerator::WriteUtilityProject(
    const CUtilityProject& project, CNcbiOfstream& ofs)
{
    list<string> proj_guid;
    if (project.HasFilter()) {
        ITERATE(TProjects, p, m_Projects) {
            const CPrjContext& prj_i = p->second;
            if (project.IsIncluded(prj_i)) {
                proj_guid.push_back(prj_i.m_GUID);
            }
        }
    }
    if (!proj_guid.empty() || project.SaveEmpty()) {
        BeginUtilityProject(project,ofs);
        if (!proj_guid.empty()) {
            ofs << '\t' << "ProjectSection(ProjectDependencies) = postProject" << endl;
            proj_guid.sort();
            ITERATE(list<string>, p, proj_guid) {
                ofs << '\t' << '\t' << *p << " = " << *p << endl;
            }
            ofs << '\t' << "EndProjectSection" << endl;
        }
        EndUtilityProject(project,ofs);
        return true;
    }
    return false;
}

void 
CMsvcSolutionGenerator::WriteProjectConfigurations(CNcbiOfstream&     ofs, 
                                                   const CPrjContext& project)
{
    ITERATE(list<SConfigInfo>, p, m_Configs) {

        const SConfigInfo& cfg_info = *p;

        CMsvcPrjProjectContext context(project.m_Project);
        
//        bool config_enabled = context.IsConfigEnabled(cfg_info, 0);

        const string& config = cfg_info.GetConfigFullName();
        string cfg1 = config;
        if (CMsvc7RegSettings::GetMsvcVersion() > CMsvc7RegSettings::eMsvc710) {
            cfg1 = ConfigName(config);
        }

        ofs << '\t' 
            << '\t' 
            << project.m_GUID 
            << '.' 
            << cfg1
            << ".ActiveCfg = " 
            << ConfigName(config)
            << endl;

        ofs << '\t' 
            << '\t' 
            << project.m_GUID 
            << '.' 
            << cfg1
            << ".Build.0 = " 
            << ConfigName(config)
            << endl;
    }
}
void CMsvcSolutionGenerator::WriteProjectConfigurations(
    CNcbiOfstream&  ofs, const list<string>& projects)
{
    ITERATE(list<string>, p, projects) {
        ITERATE(list<SConfigInfo>, c, m_Configs) {
            const SConfigInfo& cfg_info = *c;
            const string& config = cfg_info.GetConfigFullName();
            string cfg1 = config;
            if (CMsvc7RegSettings::GetMsvcVersion() > CMsvc7RegSettings::eMsvc710) {
                cfg1 = ConfigName(config);
            }
            ofs << '\t' << '\t' << *p << '.' << cfg1
                << ".ActiveCfg = " << ConfigName(config) << endl;
            ofs << '\t' << '\t' << *p << '.' << cfg1
                << ".Build.0 = " << ConfigName(config) << endl;
        }
    }
}
#endif //NCBI_COMPILER_MSVC

END_NCBI_SCOPE
