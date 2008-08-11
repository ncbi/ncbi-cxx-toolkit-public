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

BEGIN_NCBI_SCOPE
#if NCBI_COMPILER_MSVC

//-----------------------------------------------------------------------------
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
    const string& full_path, const CVisualStudioProject& prj)
{
    m_UtilityProjects.push_back(
        TUtilityProject(full_path, prj.GetAttlist().GetProjectGUID()));
    m_PathToName[full_path] = prj.GetAttlist().GetName();
}


void 
CMsvcSolutionGenerator::AddConfigureProject(const string& full_path,
                                            const CVisualStudioProject& prj)
{
    m_ConfigureProjects.push_back(
        TUtilityProject(full_path, prj.GetAttlist().GetProjectGUID()));
    m_PathToName[full_path] = prj.GetAttlist().GetName();
}


void 
CMsvcSolutionGenerator::AddBuildAllProject(const string& full_path,
                                           const CVisualStudioProject& prj)
{
    m_BuildAllProject = TUtilityProject(full_path, prj.GetAttlist().GetProjectGUID());
    m_PathToName[full_path] = prj.GetAttlist().GetName();
}

void 
CMsvcSolutionGenerator::AddAsnAllProject(const string& full_path,
                                         const CVisualStudioProject& prj)
{
    m_AsnAllProject = TUtilityProject(full_path, prj.GetAttlist().GetProjectGUID());
    m_PathToName[full_path] = prj.GetAttlist().GetName();
}

void
CMsvcSolutionGenerator::AddLibsAllProject(const string& full_path,
                                          const CVisualStudioProject& prj)
{
    m_LibsAllProject = TUtilityProject(full_path, prj.GetAttlist().GetProjectGUID());
    m_PathToName[full_path] = prj.GetAttlist().GetName();
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

    // Start sln file
    ofs << MSVC_SOLUTION_HEADER_LINE
        << GetApp().GetRegSettings().GetSolutionFileFormatVersion() << endl;

    // Utility projects
    ITERATE(list<TUtilityProject>, p, m_UtilityProjects) {
        const TUtilityProject& utl_prj = *p;
        WriteUtilityProject(utl_prj, ofs);
    }
    // Configure projects
    ITERATE(list<TUtilityProject>, p, m_ConfigureProjects) {
        const TUtilityProject& utl_prj = *p;
        WriteConfigureProject(utl_prj, ofs);
    }
    // BuildAll project
    if ( !m_BuildAllProject.first.empty() &&
        !m_BuildAllProject.second.empty() ) {
        WriteBuildAllProject(m_BuildAllProject, ofs);
    }
    // AsnAll project
    if ( !m_AsnAllProject.first.empty() &&
        !m_AsnAllProject.second.empty() ) {
        WriteAsnAllProject(m_AsnAllProject, ofs);
    }
    // LibsAll
    if ( !m_LibsAllProject.first.empty() &&
        !m_LibsAllProject.second.empty() ) {
        WriteLibsAllProject(m_LibsAllProject, ofs);
    }

    // Projects from the projects tree
    ITERATE(TProjects, p, m_Projects) {
        
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

    list<string> proj_guid;
    // Utility projects
    ITERATE(list<TUtilityProject>, p, m_UtilityProjects) {
        const TUtilityProject& utl_prj = *p;
        proj_guid.push_back(utl_prj.second);
    }
    ITERATE(list<TUtilityProject>, p, m_ConfigureProjects) {
        const TUtilityProject& utl_prj = *p;
        proj_guid.push_back(utl_prj.second);
    }
    // BuildAll project
    if ( !m_BuildAllProject.first.empty() &&
         !m_BuildAllProject.second.empty() ) {
        proj_guid.push_back(m_BuildAllProject.second);
    }
    if ( !m_AsnAllProject.first.empty() &&
         !m_AsnAllProject.second.empty() ) {
        proj_guid.push_back(m_AsnAllProject.second);
    }
    // Projects from tree
    ITERATE(TProjects, p, m_Projects) {
        proj_guid.push_back(p->second.m_GUID);
    }
//    proj_guid.sort();
//    proj_guid.unique();
    WriteProjectConfigurations( ofs, proj_guid);
    ofs << '\t' << "EndGlobalSection" << endl;

    //End of global section
    ofs << "EndGlobal" << endl;
}


//------------------------------------------------------------------------------
CMsvcSolutionGenerator::CPrjContext::CPrjContext(void)
{
    Clear();
}


CMsvcSolutionGenerator::CPrjContext::CPrjContext(const CPrjContext& context)
{
    SetFrom(context);
}


CMsvcSolutionGenerator::CPrjContext::CPrjContext(const CProjItem& project)
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
        m_ProjectPath += MSVC_PROJECT_FILE_EXT;
    }
}


CMsvcSolutionGenerator::CPrjContext& 
CMsvcSolutionGenerator::CPrjContext::operator= (const CPrjContext& context)
{
    if (this != &context) {
        Clear();
        SetFrom(context);
    }
    return *this;
}


CMsvcSolutionGenerator::CPrjContext::~CPrjContext(void)
{
    Clear();
}


void 
CMsvcSolutionGenerator::CPrjContext::Clear(void)
{
    //TODO
}


void 
CMsvcSolutionGenerator::CPrjContext::SetFrom
    (const CPrjContext& project_context)
{
    m_Project     = project_context.m_Project;

    m_GUID        = project_context.m_GUID;
    m_ProjectName = project_context.m_ProjectName;
    m_ProjectPath = project_context.m_ProjectPath;
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
                PTB_WARNING_EX(project.m_ProjectName, ePTB_MissingDependency,
                               "Project " << project.m_ProjectName
                               << " depends on missing project " << id.Id());
                continue;
            }
        }
        const CPrjContext& prj_i = n->second;
        proj_guid.push_back(prj_i.m_GUID);
    }
    if (!proj_guid.empty()) {
        ofs << '\t' << "ProjectSection(ProjectDependencies) = postProject" << endl;
        proj_guid.sort();
        ITERATE(list<string>, p, proj_guid) {
            ofs << '\t' << '\t' << *p << " = " << *p << endl;
        }
        ofs << '\t' << "EndProjectSection" << endl;
    }
    ofs << "EndProject" << endl;
}

void CMsvcSolutionGenerator::BeginUtilityProject(
    const TUtilityProject& project, CNcbiOfstream& ofs)
{
    string name = m_PathToName[project.first];
    ofs << "Project(\"" 
        << MSVC_SOLUTION_ROOT_GUID
        << "\") = \"" 
        << name //CDirEntry(project.first).GetBase() //basename
        << "\", \"";

    ofs << CDirEntry::CreateRelativePath(m_SolutionDir, project.first)
        << "\", \"";

    ofs << project.second //m_GUID 
        << "\"" 
        << endl;
}

void CMsvcSolutionGenerator::EndUtilityProject(
    const TUtilityProject& project, CNcbiOfstream& ofs)
{
    ofs << "EndProject" << endl;
}

void 
CMsvcSolutionGenerator::WriteUtilityProject(const TUtilityProject& project,
                                            CNcbiOfstream&         ofs)
{
    BeginUtilityProject(project,ofs);
    EndUtilityProject(project,ofs);
}

void 
CMsvcSolutionGenerator::WriteConfigureProject(const TUtilityProject& project,
                                            CNcbiOfstream&         ofs)
{
    BeginUtilityProject(project,ofs);
/*
    if (CMsvc7RegSettings::GetMsvcVersion() > CMsvc7RegSettings::eMsvc710) {
        string ptb("project_tree_builder");
        CProjKey id_ptb(CProjKey::eApp,ptb);
        TProjects::const_iterator n = m_Projects.find(id_ptb);
        if (n == m_Projects.end()) {
            LOG_POST(Warning << "Project " + 
                    project.first + " depends on missing project " + id_ptb.Id());
        } else {
            const CPrjContext& prj_i = n->second;

            ofs << '\t' << "ProjectSection(ProjectDependencies) = postProject" << endl;
            ofs << '\t' << '\t' 
                << prj_i.m_GUID 
                << " = " 
                << prj_i.m_GUID << endl;
            ofs << '\t' << "EndProjectSection" << endl;
        }
    }
*/
    EndUtilityProject(project,ofs);
}


void 
CMsvcSolutionGenerator::WriteBuildAllProject(const TUtilityProject& project, 
                                             CNcbiOfstream&         ofs)
{
    BeginUtilityProject(project,ofs);
    list<string> proj_guid;
    
    ITERATE(TProjects, p, m_Projects) {
//        const CProjKey&    id    = p->first;
        const CPrjContext& prj_i = p->second;
        if (prj_i.m_Project.m_MakeType == eMakeType_Excluded) {
            _TRACE("For reference only: " << prj_i.m_ProjectName);
            continue;
        }
        if (prj_i.m_Project.m_MakeType == eMakeType_ExcludedByReq) {
            PTB_WARNING_EX(prj_i.m_ProjectName, ePTB_ProjectExcluded,
                           "Excluded due to unmet requirements");
            continue;
        }
        proj_guid.push_back(prj_i.m_GUID);
    }

    if (!proj_guid.empty()) {
        ofs << '\t' << "ProjectSection(ProjectDependencies) = postProject" << endl;
        proj_guid.sort();
        ITERATE(list<string>, p, proj_guid) {
            ofs << '\t' << '\t' << *p << " = " << *p << endl;
        }
        ofs << '\t' << "EndProjectSection" << endl;
    }
    EndUtilityProject(project,ofs);
}

void 
CMsvcSolutionGenerator::WriteAsnAllProject(const TUtilityProject& project, 
                                             CNcbiOfstream&         ofs)
{
    BeginUtilityProject(project,ofs);
    list<string> proj_guid;
    
    ITERATE(TProjects, p, m_Projects) {
        const CPrjContext& prj_i = p->second;
        if (!prj_i.m_Project.m_DatatoolSources.empty()) {
            proj_guid.push_back(prj_i.m_GUID);
        }
    }

    if (!proj_guid.empty()) {
        ofs << '\t' << "ProjectSection(ProjectDependencies) = postProject" << endl;
        proj_guid.sort();
        ITERATE(list<string>, p, proj_guid) {
            ofs << '\t' << '\t' << *p << " = " << *p << endl;
        }
        ofs << '\t' << "EndProjectSection" << endl;
    }
    EndUtilityProject(project,ofs);
}

void 
CMsvcSolutionGenerator::WriteLibsAllProject(const TUtilityProject& project, 
                                            CNcbiOfstream&         ofs)
{
    BeginUtilityProject(project,ofs);
    list<string> proj_guid;
    
    ITERATE(TProjects, p, m_Projects) {
        const CPrjContext& prj_i = p->second;
        if (prj_i.m_Project.m_ProjType == CProjKey::eLib ||
            prj_i.m_Project.m_ProjType == CProjKey::eDll) {
            proj_guid.push_back(prj_i.m_GUID);
        }
    }

    if (!proj_guid.empty()) {
        ofs << '\t' << "ProjectSection(ProjectDependencies) = postProject" << endl;
        proj_guid.sort();
        ITERATE(list<string>, p, proj_guid) {
            ofs << '\t' << '\t' << *p << " = " << *p << endl;
        }
        ofs << '\t' << "EndProjectSection" << endl;
    }
    EndUtilityProject(project,ofs);
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


void 
CMsvcSolutionGenerator::WriteUtilityProjectConfiguration(const TUtilityProject& project,
                                                        CNcbiOfstream& ofs)
{
    ITERATE(list<SConfigInfo>, p, m_Configs) {

        const string& config = (*p).GetConfigFullName();
        string cfg1 = config;
        if (CMsvc7RegSettings::GetMsvcVersion() > CMsvc7RegSettings::eMsvc710) {
            cfg1 = ConfigName(config);
        }

        ofs << '\t' 
            << '\t' 
            << project.second // project.m_GUID 
            << '.' 
            << cfg1
            << ".ActiveCfg = " 
            << ConfigName(config)
            << endl;

        ofs << '\t' 
            << '\t' 
            << project.second // project.m_GUID 
            << '.' 
            << cfg1
            << ".Build.0 = " 
            << ConfigName(config)
            << endl;
    }
}
#endif //NCBI_COMPILER_MSVC

END_NCBI_SCOPE
