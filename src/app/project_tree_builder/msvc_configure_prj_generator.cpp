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
#include <corelib/ncbistre.hpp>
#include <app/project_tree_builder/msvc_configure_prj_generator.hpp>
#include <app/project_tree_builder/msvc_prj_defines.hpp>
#include <app/project_tree_builder/proj_builder_app.hpp>

#include <corelib/ncbienv.hpp>
BEGIN_NCBI_SCOPE

CMsvcConfigureProjectGenerator::CMsvcConfigureProjectGenerator
                                  (const string&            output_dir,
                                   const list<SConfigInfo>& configs,
                                   const string&            project_dir,
                                   const string&            tree_root,
                                   const string&            subtree_to_build,
                                   const string&            solution_to_build)
:m_Name          ("_CONFIGURE_"),
 m_OutputDir     (output_dir),
 m_Configs       (configs),
 m_ProjectDir    (project_dir),
 m_TreeRoot      (tree_root),
 m_SubtreeToBuild(subtree_to_build),
 m_SolutionToBuild(solution_to_build),
 m_SrcFileName   ("configure"),
 m_ProjectItemExt("._"),
 m_FilesSubdir   ("UtilityProjectsFiles")
{
    m_CustomBuildCommand = "@echo on\n";
    m_CustomBuildCommand += "set _THE_PATH=" + m_OutputDir + "$(ConfigurationName)\n";
    m_CustomBuildCommand += "copy /Y $(InputPath) $(InputDir)$(InputName).bat\n";
    m_CustomBuildCommand += "call $(InputDir)$(InputName).bat\n";
    m_CustomBuildCommand += "del /Q $(InputDir)$(InputName).bat\n";
}


CMsvcConfigureProjectGenerator::~CMsvcConfigureProjectGenerator(void)
{
}


void CMsvcConfigureProjectGenerator::SaveProject(const string& base_name)
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
        CRef<CFilter> filter(new CFilter());
        filter->SetAttlist().SetName("Configure");
        filter->SetAttlist().SetFilter("");

        //Include collected source files
        CreateProjectFileItem();
        CRef< CFFile > file(new CFFile());
        file->SetAttlist().SetRelativePath(m_SrcFileName + m_ProjectItemExt);
        SCustomBuildInfo build_info;
        string source_file_path_abs = 
            CDirEntry::ConcatPath(m_ProjectDir, 
                                  m_SrcFileName + m_ProjectItemExt);
        source_file_path_abs = CDirEntry::NormalizePath(source_file_path_abs);
        build_info.m_SourceFile  = source_file_path_abs;
        build_info.m_Description = "Configure solution : $(SolutionName)";
        build_info.m_CommandLine = m_CustomBuildCommand;
        build_info.m_Outputs     = "$(InputPath).aanofile.out";
        
        AddCustomBuildFileToFilter(filter, 
                                   m_Configs, 
                                   m_ProjectDir, 
                                   build_info);
        xmlprj.SetFiles().SetFilter().push_back(filter);

    }}

    {{
        //Globals
        xmlprj.SetGlobals("");
    }}


    string project_path = CDirEntry::ConcatPath(m_ProjectDir, base_name);

    project_path += ".vcproj";

    SaveIfNewer(project_path, xmlprj);
}


void CMsvcConfigureProjectGenerator::CreateProjectFileItem(void) const
{
    string file_path = CDirEntry::ConcatPath(m_ProjectDir, m_SrcFileName);
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

    ofs << "%_THE_PATH%\\project_tree_builder.exe"
        << " -logfile out.log"
        << " -conffile %_THE_PATH%\\..\\..\\project_tree_builder.ini "
        << m_TreeRoot << " " << m_SubtreeToBuild << " " << m_SolutionToBuild ;

}


END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2004/02/12 16:24:59  gorelenk
 * Initial revision.
 *
 * ===========================================================================
 */
