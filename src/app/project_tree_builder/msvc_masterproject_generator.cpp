
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

#include <app/project_tree_builder/msvc_masterproject_generator.hpp>


#include <app/project_tree_builder/msvc_prj_utils.hpp>
#include <app/project_tree_builder/proj_builder_app.hpp>
#include <app/project_tree_builder/msvc_prj_defines.hpp>


BEGIN_NCBI_SCOPE


CMsvcMasterProjectGenerator::CMsvcMasterProjectGenerator
    ( const CProjectItemsTree& tree,
      const list<SConfigInfo>& configs,
      const string&            project_dir)
    :m_Tree          (tree),
     m_Configs       (configs),
	 m_Name          ("_MasterProject"),
     m_ProjectDir    (project_dir),
     m_ProjectItemExt(".build")
{
    m_CustomBuildCommand  = "@echo on\n";
    m_CustomBuildCommand += "devenv "\
                            "/build $(ConfigurationName) "\
                            "/project $(InputName) "\
                            "\"$(SolutionPath)\"\n";
}


CMsvcMasterProjectGenerator::~CMsvcMasterProjectGenerator(void)
{
}


string 
CMsvcMasterProjectGenerator::ConfigName(const string& config) const
{
    return config +'|'+ MSVC_PROJECT_PLATFORM;
}


void 
CMsvcMasterProjectGenerator::SaveProject(const string& base_name)
{
    CVisualStudioProject xmlprj;
    
    {{
        //Attributes:
        xmlprj.SetAttlist().SetProjectType  (MSVC_PROJECT_PROJECT_TYPE);
        xmlprj.SetAttlist().SetVersion      (MSVC_PROJECT_VERSION);
        xmlprj.SetAttlist().SetName         (m_Name);
        xmlprj.SetAttlist().SetRootNamespace
            (MSVC_MASTERPROJECT_ROOT_NAMESPACE);
        xmlprj.SetAttlist().SetKeyword      (MSVC_MASTERPROJECT_KEYWORD);
    }}
    
    {{
        //Platforms
         CRef<CPlatform> platform(new CPlatform(""));
         platform->SetAttlist().SetName(MSVC_PROJECT_PLATFORM);
         xmlprj.SetPlatforms().SetPlatform().push_back(platform);
    }}

    ITERATE(list<SConfigInfo>, p , m_Configs) {
        // Iterate all configurations
        const string& config = (*p).m_Name;
        
        CRef<CConfiguration> conf(new CConfiguration());

#define SET_ATTRIBUTE( node, X, val ) node->SetAttlist().Set##X(val)        

        {{
            //Configuration
            SET_ATTRIBUTE(conf, Name,               ConfigName(config));

            SET_ATTRIBUTE(conf, 
                          OutputDirectory,
                          "$(SolutionDir)$(ConfigurationName)");
            
            SET_ATTRIBUTE(conf, 
                          IntermediateDirectory,  
                          "$(ConfigurationName)");
            
            SET_ATTRIBUTE(conf, ConfigurationType,  "10");
            
            SET_ATTRIBUTE(conf, CharacterSet,       "2");
            
            SET_ATTRIBUTE(conf, ManagedExtensions,  "TRUE");
        }}

        {{
            //VCCustomBuildTool
            CRef<CTool> tool(new CTool(""));
            SET_ATTRIBUTE(tool, Name, "VCCustomBuildTool" );
            conf->SetTool().push_back(tool);
        }}
        {{
            //VCMIDLTool
            CRef<CTool> tool(new CTool(""));
            SET_ATTRIBUTE(tool, Name, "VCMIDLTool" );
            conf->SetTool().push_back(tool);
        }}
        {{
            //VCPostBuildEventTool
            CRef<CTool> tool(new CTool(""));
            SET_ATTRIBUTE(tool, Name, "VCPostBuildEventTool" );
            conf->SetTool().push_back(tool);
        }}
        {{
            //VCPreBuildEventTool
            CRef<CTool> tool(new CTool(""));
            SET_ATTRIBUTE(tool, Name, "VCPreBuildEventTool" );
            conf->SetTool().push_back(tool);
        }}

        xmlprj.SetConfigurations().SetConfiguration().push_back(conf);
    }

    {{
        //References
        xmlprj.SetReferences("");
    }}

    {{
        //Filters
        list<string> root_ids;
        m_Tree.GetRoots(&root_ids);
        ITERATE(list<string>, p, root_ids) {
            const string& root_id = *p;
            ProcessProjectBranch(root_id, &xmlprj.SetFiles());
        }
    }}

    {{
        //Globals
        xmlprj.SetGlobals("");
    }}


    string project_path = CDirEntry::ConcatPath(m_ProjectDir, base_name);

    project_path += ".vcproj";

    SaveToXmlFile(project_path, xmlprj);
}


void 
CMsvcMasterProjectGenerator::ProcessProjectBranch(const string&  project_id,
                                                  CSerialObject* parent)
{
    if ( IsAlreadyProcessed(project_id) )
        return;

    CRef<CFilter> project_filter = FindOrCreateFilter(project_id, parent);
    if ( !project_filter )
        return;

    list<string> sibling_ids;
    m_Tree.GetSiblings(project_id, &sibling_ids);
    ITERATE(list<string>, p, sibling_ids) {
        //continue recursive lookup
        const string& sibling_id = *p;
        ProcessProjectBranch(sibling_id, &(*project_filter));
    }

    AddProjectToFilter(project_filter, project_id);
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


CRef<CFilter>
CMsvcMasterProjectGenerator::FindOrCreateFilter(const string&  project_id,
                                                CSerialObject* parent)
{
    CProjectItemsTree::TProjects::const_iterator p = 
        m_Tree.m_Projects.find(project_id);

    if (p != m_Tree.m_Projects.end()) {
        //Find filter for the project or create a new one
        const CProjItem& project = p->second;
        
        TFiltersCache::iterator n = 
            m_FiltersCache.find(project.m_SourcesBaseDir);
        if (n != m_FiltersCache.end())
            return n->second;
        
        CRef<CFilter> filter(new CFilter());
        filter->SetAttlist().SetName(GetFolder(project.m_SourcesBaseDir));
        filter->SetAttlist().SetFilter("");

        m_FiltersCache[project.m_SourcesBaseDir] = filter;
        s_RegisterCreatedFilter(filter, parent);
        return filter;
    } else {
        //will return uninitilized CRef
        LOG_POST("||||||||| No project with id : " + project_id);
        return CRef<CFilter>();
    }
}


bool 
CMsvcMasterProjectGenerator::IsAlreadyProcessed(const string& project_id)
{
    set<string>::const_iterator p = m_ProcessedIds.find(project_id);
    if (p == m_ProcessedIds.end()) {
        m_ProcessedIds.insert(project_id);
        return false;
    }
    return true;
}


void 
CMsvcMasterProjectGenerator::AddProjectToFilter(CRef<CFilter>& filter, 
                                                const string&  project_id)
{
    CProjectItemsTree::TProjects::const_iterator p = 
        m_Tree.m_Projects.find(project_id);

    if (p != m_Tree.m_Projects.end()) {
        // Add project to this filter (folder)
        const CProjItem& project = p->second;

        CRef<CFFile> file(new CFFile());
        file->SetAttlist().SetRelativePath(project.m_ID + m_ProjectItemExt);
        
        CreateProjectFileItem(project);

        ITERATE(list<SConfigInfo>, n , m_Configs) {
            // Iterate all configurations
            const string& config = (*n).m_Name;

            CRef<CFileConfiguration> file_config(new CFileConfiguration());
            file_config->SetAttlist().SetName(ConfigName(config));

            CRef<CTool> custom_build(new CTool(""));
            custom_build->SetAttlist().SetName("VCCustomBuildTool");
            custom_build->SetAttlist().SetDescription
                ("Building project : $(InputName)");
            custom_build->SetAttlist().SetCommandLine(m_CustomBuildCommand);
            custom_build->SetAttlist().SetOutputs("$(InputPath).aanofile.out");
            file_config->SetTool(*custom_build);

            file->SetFileConfiguration().push_back(file_config);
        }
        CRef< CFilter_Base::C_FF::C_E > ce( new CFilter_Base::C_FF::C_E() );
        ce->SetFile(*file);
        filter->SetFF().SetFF().push_back(ce);
    } else {
        LOG_POST("||||||||| No project with id : " + project_id);
    }
}


void 
CMsvcMasterProjectGenerator::CreateProjectFileItem(const CProjItem& project)
{
    string file_path = CDirEntry::ConcatPath(m_ProjectDir, project.m_ID);
    file_path += m_ProjectItemExt;

    CNcbiOfstream  ofs(file_path.c_str(), IOS_BASE::out | IOS_BASE::trunc);
    if ( !ofs )
        NCBI_THROW(CProjBulderAppException, eFileCreation, file_path);
}


END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.3  2004/01/26 19:27:28  gorelenk
 * += MSVC meta makefile support
 * += MSVC project makefile support
 *
 * Revision 1.2  2004/01/22 17:57:54  gorelenk
 * first version
 *
 * ===========================================================================
 */
