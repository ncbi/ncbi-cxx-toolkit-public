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

#include <app/project_tree_builder/stl_msvc_usage.hpp>
#include <app/project_tree_builder/msvc_sln_generator.hpp>
#include <app/project_tree_builder/msvc_prj_utils.hpp>
#include <corelib/ncbifile.hpp>
#include <corelib/ncbistr.hpp>
#include <app/project_tree_builder/proj_builder_app.hpp>
#include <app/project_tree_builder/msvc_prj_defines.hpp>

BEGIN_NCBI_SCOPE


CMsvcSolutionGenerator::CMsvcSolutionGenerator(const list<string>& configs)
    :m_Configs(configs)
{
}


CMsvcSolutionGenerator::~CMsvcSolutionGenerator()
{
}


void 
CMsvcSolutionGenerator::AddProject(const CProjItem& project)
{
    m_Projects[project.m_ID] = CPrjContext(project);
}


void 
CMsvcSolutionGenerator::AddMasterProject(const string& base_name)
{
    m_MasterProject.first  = base_name;
    m_MasterProject.second = GenerateSlnGUID();
}


bool 
CMsvcSolutionGenerator::IsSetMasterProject() const
{
    return !m_MasterProject.first.empty() && !m_MasterProject.second.empty();
}


void 
CMsvcSolutionGenerator::SaveSolution(const string& file_path)
{
    CDirEntry::SplitPath(file_path, &m_SolutionDir);

    // Create dir for output sln file
    CDir(m_SolutionDir).CreatePath();

    CNcbiOfstream  ofs(file_path.c_str(), ios::out | ios::trunc);
    if ( !ofs )
        NCBI_THROW(CProjBulderAppException, eFileCreation, file_path);

    //Start sln file
    ofs << MSVC_SOLUTION_HEADER_LINE << endl;

    ITERATE(TProjects, p, m_Projects) {
        
        WriteProjectAndSection(ofs, p->second);
    }
    if ( IsSetMasterProject() )
        WriteMasterProject(ofs);

    //Start "Global" section
    ofs << "Global" << endl;
	
    //Write all configurations
    ofs << '\t' << "GlobalSection(SolutionConfiguration) = preSolution" << endl;
    ITERATE(list<string>, p, m_Configs)
    {
        ofs << '\t' << '\t' << *p << " = " << *p << endl;
    }
    ofs << '\t' << "EndGlobalSection" << endl;
    
    ofs << '\t' 
        << "GlobalSection(ProjectConfiguration) = postSolution" 
        << endl;

    ITERATE(TProjects, p, m_Projects) {
        
        WriteProjectConfigurations(ofs, p->second);
    }
    if ( IsSetMasterProject() )
        WriteMasterProjectConfiguration(ofs);

    ofs << '\t' << "EndGlobalSection" << endl;

    //meanless stuff
    ofs << '\t' 
        << "GlobalSection(ExtensibilityGlobals) = postSolution" << endl;
	ofs << '\t' << "EndGlobalSection" << endl;
	ofs << '\t' << "GlobalSection(ExtensibilityAddIns) = postSolution" << endl;
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
    m_GUID = GenerateSlnGUID();

    CMsvcPrjProjectContext project_context(project);
    m_ProjectName = project_context.ProjectName();
    m_ProjectPath = CDirEntry::ConcatPath(project_context.ProjectDir(),
                                          project_context.ProjectName());
    m_ProjectPath += MSVC_PROJECT_FILE_EXT;
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


void CMsvcSolutionGenerator::CPrjContext::Clear(void)
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


void CMsvcSolutionGenerator::WriteProjectAndSection(CNcbiOfstream&     ofs, 
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

    ofs << '\t' << "ProjectSection(ProjectDependencies) = postProject" << endl;

    ITERATE(list<string>, p, project.m_Project.m_Depends) {

        const string& id = *p;

        TProjects::const_iterator n = m_Projects.find(id);
        if (n != m_Projects.end()) {

            const CPrjContext& prj_i = n->second;

            ofs << '\t' << '\t' 
                << prj_i.m_GUID 
                << " = " 
                << prj_i.m_GUID << endl;
        } else {

            LOG_POST("&&&&&&& Project: " + 
                      project.m_ProjectName + " is dependend of " + id + 
                      ". But no such project");
        }
    }
    ofs << '\t' << "EndProjectSection" << endl;
    ofs << "EndProject" << endl;
}


void CMsvcSolutionGenerator::WriteMasterProject(CNcbiOfstream& ofs)
{
    ofs << "Project(\"" 
        << MSVC_SOLUTION_ROOT_GUID
        << "\") = \"" 
        << m_MasterProject.first //basename
        << "\", \"";

    ofs << m_MasterProject.first + MSVC_PROJECT_FILE_EXT 
        << "\", \"";

    ofs << m_MasterProject.second //m_GUID 
        << "\"" 
        << endl;

    ofs << '\t' << "ProjectSection(ProjectDependencies) = postProject" << endl;

    ofs << '\t' << "EndProjectSection" << endl;
    ofs << "EndProject" << endl;
}



void 
CMsvcSolutionGenerator::WriteProjectConfigurations(CNcbiOfstream&     ofs, 
                                                   const CPrjContext& project)
{
    ITERATE(list<string>, p, m_Configs) {

        const string& config = *p;
        ofs << '\t' 
            << '\t' 
            << project.m_GUID 
            << '.' 
            << config 
            << ".ActiveCfg = " 
            << config 
            << "|Win32" 
            << endl;

        ofs << '\t' 
            << '\t' 
            << project.m_GUID 
            << '.' 
            << config 
            << ".Build.0 = " 
            << config 
            << "|Win32" 
            << endl;
    }
}


void 
CMsvcSolutionGenerator::WriteMasterProjectConfiguration(CNcbiOfstream& ofs)
{
    ITERATE(list<string>, p, m_Configs) {

        const string& config = *p;
        ofs << '\t' 
            << '\t' 
            << m_MasterProject.second // project.m_GUID 
            << '.' 
            << config 
            << ".ActiveCfg = " 
            << config 
            << "|Win32" 
            << endl;

        ofs << '\t' 
            << '\t' 
            << m_MasterProject.second // project.m_GUID 
            << '.' 
            << config 
            << ".Build.0 = " 
            << config 
            << "|Win32" 
            << endl;
    }
}


END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.4  2004/01/22 17:57:54  gorelenk
 * first version
 *
 * ===========================================================================
 */
