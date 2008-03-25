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
#include "msvc_masterproject_generator.hpp"


#include "msvc_prj_utils.hpp"
#include "proj_builder_app.hpp"
#include "msvc_prj_defines.hpp"
#include "ptb_err_codes.hpp"


BEGIN_NCBI_SCOPE

#if NCBI_COMPILER_MSVC

static void
s_RegisterCreatedFilter(CRef<CFilter>& filter, CSerialObject* parent);


//-----------------------------------------------------------------------------
CMsvcMasterProjectGenerator::CMsvcMasterProjectGenerator
    ( const CProjectItemsTree& tree,
      const list<SConfigInfo>& configs,
      const string&            project_dir)
    :m_Tree          (tree),
     m_Configs       (configs),
	 m_Name          ("-HIERARCHICAL-VIEW-"), //_MasterProject
     m_ProjectDir    (project_dir),
     m_ProjectItemExt("._"),
     m_FilesSubdir   ("UtilityProjectsFiles")
{
    m_CustomBuildCommand  = "@echo on\n";
    if (CMsvc7RegSettings::GetMsvcVersion() == CMsvc7RegSettings::eMsvc710) {
        m_CustomBuildCommand += "devenv "\
                                "/build $(ConfigurationName) "\
                                "/project $(InputName) "\
                                "\"$(SolutionPath)\"\n";
    } else {
        m_CustomBuildCommand += "msbuild \"$(SolutionPath)\" /t:\"$(InputName)\" /p:Configuration=$(ConfigurationName)";
        if (CMsvc7RegSettings::GetMsvcVersion() >= CMsvc7RegSettings::eMsvc900) {
            m_CustomBuildCommand += " /maxcpucount";
        }
    }
    CreateUtilityProject(m_Name, m_Configs, &m_Xmlprj);
}


CMsvcMasterProjectGenerator::~CMsvcMasterProjectGenerator(void)
{
}


void 
CMsvcMasterProjectGenerator::SaveProject()
{
    {{
        CProjectTreeFolders folders(m_Tree);
        ProcessTreeFolder(folders.m_RootParent, &m_Xmlprj.SetFiles());
    }}
    SaveIfNewer(GetPath(), m_Xmlprj);
}


string CMsvcMasterProjectGenerator::GetPath() const
{
    string project_path = 
        CDirEntry::ConcatPath(m_ProjectDir, "_HIERARCHICAL_VIEW_");
    project_path += MSVC_PROJECT_FILE_EXT;
    return project_path;
}


void CMsvcMasterProjectGenerator::ProcessTreeFolder
                                        (const SProjectTreeFolder&  folder,
                                         CSerialObject*             parent)
{
    if ( folder.IsRoot() ) {

        ITERATE(SProjectTreeFolder::TSiblings, p, folder.m_Siblings) {
            
            ProcessTreeFolder(*(p->second), parent);
        }
    } else {

        CRef<CFilter> filter(new CFilter());
        filter->SetAttlist().SetName(folder.m_Name);
        filter->SetAttlist().SetFilter("");
        s_RegisterCreatedFilter(filter, parent);

        ITERATE(SProjectTreeFolder::TProjects, p, folder.m_Projects) {

            const CProjKey& project_id = *p;
            AddProjectToFilter(filter, project_id);
        }
        ITERATE(SProjectTreeFolder::TSiblings, p, folder.m_Siblings) {
            
            ProcessTreeFolder(*(p->second), filter);
        }
    }
}


static void
s_RegisterCreatedFilter(CRef<CFilter>& filter, CSerialObject* parent)
{
    {{
        // Files section?
        CFiles* files_parent = dynamic_cast< CFiles* >(parent);
        if (files_parent != NULL) {
            // Parent is <Files> section of MSVC project
            files_parent->SetFilter().push_back(filter);
            return;
        }
    }}
    {{
        // Another folder?
        CFilter* filter_parent = dynamic_cast< CFilter* >(parent);
        if (filter_parent != NULL) {
            // Parent is another Filter (folder)
            CRef< CFilter_Base::C_FF::C_E > ce(new CFilter_Base::C_FF::C_E());
            ce->SetFilter(*filter);
            filter_parent->SetFF().SetFF().push_back(ce);
            return;
        }
    }}
}

void 
CMsvcMasterProjectGenerator::AddProjectToFilter(CRef<CFilter>&   filter, 
                                                const CProjKey&  project_id)
{
    CProjectItemsTree::TProjects::const_iterator p = 
        m_Tree.m_Projects.find(project_id);

    if (p != m_Tree.m_Projects.end()) {
        // Add project to this filter (folder)
//        const CProjItem& project = p->second;
        CreateProjectFileItem(project_id);

        SCustomBuildInfo build_info;
        string project_name = CreateProjectName(project_id);
        if (CMsvc7RegSettings::GetMsvcVersion() > CMsvc7RegSettings::eMsvc710) {
            project_name = NStr::Replace( project_name, ".", "_");
        }
        string source_file_path_abs = 
            CDirEntry::ConcatPath(m_ProjectDir, 
                                  project_name + m_ProjectItemExt);
        source_file_path_abs = CDirEntry::NormalizePath(source_file_path_abs);

        build_info.m_SourceFile  = source_file_path_abs;
        build_info.m_Description = "Building project : $(InputName)";
        build_info.m_CommandLine = m_CustomBuildCommand;
        build_info.m_Outputs     = "$(InputPath).aanofile.out";
        
        AddCustomBuildFileToFilter(filter, 
                                   m_Configs, 
                                   m_ProjectDir, 
                                   build_info);

    } else {
        PTB_WARNING_EX(project_id.Id(), ePTB_ProjectNotFound,
                       "Project not found: " << project_id.Id());
    }
}


void 
CMsvcMasterProjectGenerator::CreateProjectFileItem(const CProjKey& project_id)
{
    string project_name = CreateProjectName(project_id);
    if (CMsvc7RegSettings::GetMsvcVersion() > CMsvc7RegSettings::eMsvc710) {
        project_name = NStr::Replace( project_name, ".", "_");
    }
    string file_path = 
        CDirEntry::ConcatPath(m_ProjectDir, project_name);

    file_path += m_ProjectItemExt;

    // Create dir if no such dir...
    string dir;
    CDirEntry::SplitPath(file_path, &dir);
    CDir project_dir(dir);
    if ( !project_dir.Exists() ) {
        CDir(dir).CreatePath();
    }

    CNcbiOfstream  ofs(file_path.c_str(), IOS_BASE::out | IOS_BASE::trunc);
    if ( !ofs )
        NCBI_THROW(CProjBulderAppException, eFileCreation, file_path);
}

#endif //NCBI_COMPILER_MSVC

END_NCBI_SCOPE
