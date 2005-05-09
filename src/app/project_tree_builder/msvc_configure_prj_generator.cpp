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
#include <corelib/ncbistre.hpp>
#include <app/project_tree_builder/msvc_configure_prj_generator.hpp>
#include <app/project_tree_builder/msvc_prj_defines.hpp>
#include <app/project_tree_builder/proj_builder_app.hpp>

#include <corelib/ncbienv.hpp>
BEGIN_NCBI_SCOPE

CMsvcConfigureProjectGenerator::CMsvcConfigureProjectGenerator
                                  (const string&            output_dir,
                                   const list<SConfigInfo>& configs,
                                   bool                     dll_build,
                                   const string&            project_dir,
                                   const string&            tree_root,
                                   const string&            subtree_to_build,
                                   const string&            solution_to_build,
                                   bool  build_ptb)
:m_Name          ("-CONFIGURE-"),
 m_OutputDir     (output_dir),
 m_Configs       (configs),
 m_DllBuild      (dll_build),
 m_ProjectDir    (project_dir),
 m_TreeRoot      (tree_root),
 m_SubtreeToBuild(subtree_to_build),
 m_SolutionToBuild(solution_to_build),
 m_ProjectItemExt("._"),
 m_SrcFileName   ("configure"),
 m_FilesSubdir   ("UtilityProjectsFiles"),
 m_BuildPtb(build_ptb)
{
    m_CustomBuildCommand = "@echo on\n";

    // Macrodefines PTB_PATH  path to project_tree_builder app
    //              TREE_ROOT path to project tree root
    //              SLN_PATH  path to solution to buil
    string ptb_path_par = "$(ProjectDir)" + 
                           CDirEntry::AddTrailingPathSeparator
                                 (CDirEntry::CreateRelativePath(m_ProjectDir, 
                                                                m_OutputDir));
    if (m_BuildPtb) {
        ptb_path_par += "Release";
    } else {
        ptb_path_par += "$(ConfigurationName)";
    }

    string tree_root_par = "$(ProjectDir)" + CDirEntry::DeleteTrailingPathSeparator(
                            CDirEntry::CreateRelativePath(m_ProjectDir,tree_root));
    string sln_path_par  = "$(SolutionPath)";

    m_CustomBuildCommand += "set PTB_PATH="  + ptb_path_par  + "\n";
    m_CustomBuildCommand += "set TREE_ROOT=" + tree_root_par + "\n";
    m_CustomBuildCommand += "set SLN_PATH="  + sln_path_par  + "\n";
    //

    // Build command for project_tree_builder.sln
    if (m_BuildPtb) {
        m_CustomBuildCommand += 
            "devenv /build Release /project project_tree_builder.exe ";
//         "devenv /build $(ConfigurationName) /project project_tree_builder.exe ";

        string project_tree_builder_sln_dir = 
            GetApp().GetProjectTreeInfo().m_Compilers;
        project_tree_builder_sln_dir = 
            CDirEntry::ConcatPath(project_tree_builder_sln_dir,
                                GetApp().GetRegSettings().m_CompilersSubdir);
        project_tree_builder_sln_dir = 
            CDirEntry::ConcatPath(project_tree_builder_sln_dir, "static");
        project_tree_builder_sln_dir = 
            CDirEntry::ConcatPath(project_tree_builder_sln_dir, 
                                GetApp().GetRegSettings().m_ProjectsSubdir);
        project_tree_builder_sln_dir = 
            CDirEntry::ConcatPath(project_tree_builder_sln_dir, "app");
        project_tree_builder_sln_dir = 
            CDirEntry::ConcatPath(project_tree_builder_sln_dir, 
                                "project_tree_builder");
        project_tree_builder_sln_dir = 
            CDirEntry::AddTrailingPathSeparator(project_tree_builder_sln_dir);
        
        string project_tree_builder_sln_path = 
            CDirEntry::ConcatPath(project_tree_builder_sln_dir, 
                                "project_tree_builder.sln");
        project_tree_builder_sln_path = 
            CDirEntry::CreateRelativePath(project_dir, 
                                        project_tree_builder_sln_path);
        
        m_CustomBuildCommand += project_tree_builder_sln_path + "\n";
    }

    // Make *.bat file from source file of this custom build.
    // This file ( see CreateProjectFileItem below )
    // will use defines PTB_PATH, TREE_ROOT and SLN_PATH mentioned above
    m_CustomBuildCommand += "if errorlevel 1 exit 1\n";
    m_CustomBuildCommand += "cd $(InputDir)\n";
    m_CustomBuildCommand += "copy /Y $(InputFileName) $(InputName).bat\n";
    m_CustomBuildCommand += "call $(InputName).bat\n";
    m_CustomBuildCommand += "del /Q $(InputName).bat\n";
}


CMsvcConfigureProjectGenerator::~CMsvcConfigureProjectGenerator(void)
{
}


void CMsvcConfigureProjectGenerator::SaveProject()
{
    CVisualStudioProject xmlprj;
    CreateUtilityProject(m_Name, m_Configs, &xmlprj);


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



    SaveIfNewer(GetPath(), xmlprj);
}


string CMsvcConfigureProjectGenerator::GetPath() const
{
    string project_path = CDirEntry::ConcatPath(m_ProjectDir, "_CONFIGURE_");
    project_path += MSVC_PROJECT_FILE_EXT;
    return project_path;
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
    
    // Prototype of command line for launch project_tree_builder (See above)
    CNcbiOfstream  ofs(file_path.c_str(), IOS_BASE::out | IOS_BASE::trunc);
    if ( !ofs )
        NCBI_THROW(CProjBulderAppException, eFileCreation, file_path);

    ofs << "\"%PTB_PATH%\\project_tree_builder.exe\"";

    if ( m_DllBuild )
        ofs << " -dll";
    if (!m_BuildPtb) {
        ofs << " -nobuildptb";
    }
    if (GetApp().m_AddMissingLibs) {
        ofs << " -ext";
    }
    if (!GetApp().m_ScanWholeTree) {
        ofs << " -nws";
    }
    if (!GetApp().m_BuildRoot.empty()) {
        ofs << " -extroot \"" << GetApp().m_BuildRoot << "\"";
    }
    if (GetApp().m_ConfirmCfg) {
        ofs << " -cfg";
    }
    if (GetApp().m_ProjTags != "*") {
        ofs << " -projtag \"" << GetApp().m_ProjTags << "\"";
    }

    ofs << " -logfile \"%SLN_PATH%_configuration_log.txt\""
        << " -conffile \"%PTB_PATH%\\..\\..\\..\\project_tree_builder.ini\" "
        << "\"%TREE_ROOT%\"" << " " << m_SubtreeToBuild << " " << "\"%SLN_PATH%\"" << endl;
    ofs << "@echo off" << endl
        << "if errorlevel 1 goto report_error" << endl
        << endl
        << "echo ******************************************************************************" << endl
        << "echo -CONFIGURE- has succeeded" << endl
        << "echo Configuration log was saved at \"file://%SLN_PATH%_configuration_log.txt\"" << endl
        << "echo ******************************************************************************" << endl
        << "exit 0" << endl
        << endl
        << ":report_error" << endl
        << "echo ******************************************************************************" << endl
        << "echo -CONFIGURE- has failed" << endl
        << "echo Configuration log was saved at \"file://%SLN_PATH%_configuration_log.txt\"" << endl
        << "echo ******************************************************************************" << endl
        << "start \"\" \"%SLN_PATH%_configuration_log.txt\"" << endl
        << "exit 1" << endl;
}


END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
 * Revision 1.23  2005/05/09 17:04:09  gouriano
 * Added filtering by project tag and GUI
 *
 * Revision 1.22  2005/04/14 14:35:58  gouriano
 * Handle paths with spaces
 *
 * Revision 1.21  2005/04/13 15:57:33  gouriano
 * Handle paths with spaces
 *
 * Revision 1.20  2005/04/08 19:08:53  gouriano
 * Use configuration-dependent PTB when it is not to be built
 *
 * Revision 1.19  2005/04/07 16:58:16  gouriano
 * Make it possible to find and reference missing libraries
 * without creating project dependencies
 *
 * Revision 1.18  2005/03/23 19:33:20  gouriano
 * Make it possible to exclude PTB build when configuring
 *
 * Revision 1.17  2005/03/18 16:16:29  gouriano
 * Run CONFIGURE in Release mode always
 *
 * Revision 1.16  2004/12/20 21:07:33  gouriano
 * Eliminate compiler warnings
 *
 * Revision 1.15  2004/12/20 15:31:46  gouriano
 * changed generated batch file to open error log asynchronously
 *
 * Revision 1.14  2004/12/06 18:12:20  gouriano
 * Improved diagnostics
 *
 * Revision 1.13  2004/11/18 16:35:21  gouriano
 * Changed configure._ to report configuration result
 *
 * Revision 1.12  2004/05/21 21:41:41  gorelenk
 * Added PCH ncbi_pch.hpp
 *
 * Revision 1.11  2004/04/13 17:07:16  gorelenk
 * Changed implementation of class CMsvcConfigureProjectGenerator constructor.
 *
 * Revision 1.10  2004/04/01 17:31:06  gorelenk
 * Changed "rebuild" to "build"
 * in class CMsvcConfigureProjectGenerator constructor .
 *
 * Revision 1.9  2004/03/16 16:37:33  gorelenk
 * Changed msvc7_prj subdirs structure: Separated "static" and "dll" branches.
 *
 * Revision 1.8  2004/03/10 21:27:26  gorelenk
 * Changed CMsvcConfigureProjectGenerator constructor and
 * CreateProjectFileItem member-function implementation.
 *
 * Revision 1.7  2004/03/02 23:31:45  gorelenk
 * Changed implementation of
 * CMsvcConfigureProjectGenerator::CreateProjectFileItem .
 *
 * Revision 1.6  2004/02/20 22:53:25  gorelenk
 * Added analysis of ASN projects depends.
 *
 * Revision 1.5  2004/02/13 20:39:51  gorelenk
 * Minor cosmetic changes.
 *
 * Revision 1.4  2004/02/13 17:09:16  gorelenk
 * Changed custom build command generation.
 *
 * Revision 1.3  2004/02/12 23:15:28  gorelenk
 * Implemented utility projects creation and configure re-build of the app.
 *
 * Revision 1.2  2004/02/12 17:43:46  gorelenk
 * Changed command line for project re-configure.
 *
 * Revision 1.1  2004/02/12 16:24:59  gorelenk
 * Initial revision.
 *
 * ===========================================================================
 */
