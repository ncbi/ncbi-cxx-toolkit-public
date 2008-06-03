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
#include "msvc_configure_prj_generator.hpp"
#include "msvc_prj_defines.hpp"
#include "proj_builder_app.hpp"

#include <corelib/ncbienv.hpp>
BEGIN_NCBI_SCOPE

#if NCBI_COMPILER_MSVC

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
 m_NameGui       ("-CONFIGURE-DIALOG-"),
 m_OutputDir     (output_dir),
 m_Configs       (configs),
 m_DllBuild      (dll_build),
 m_ProjectDir    (project_dir),
 m_TreeRoot      (tree_root),
 m_SubtreeToBuild(subtree_to_build),
 m_SolutionToBuild(solution_to_build),
 m_ProjectItemExt("._"),
 m_SrcFileName   ("configure"),
 m_SrcFileNameGui("configure_dialog"),
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
        if (CMsvc7RegSettings::GetMsvcVersion() == CMsvc7RegSettings::eMsvc710) {
            ptb_path_par += "Release";
        } else {
            ptb_path_par += "ReleaseDLL";
        }
    } else {
        ptb_path_par += "$(ConfigurationName)";
    }

    string tree_root_par = "$(ProjectDir)" + CDirEntry::DeleteTrailingPathSeparator(
                            CDirEntry::CreateRelativePath(m_ProjectDir,tree_root));
#if 1
    string sln_path_par = "$(ProjectDir)" + 
        CDirEntry::CreateRelativePath(m_ProjectDir, GetApp().m_Solution);
#else
    string sln_path_par  = "$(SolutionPath)";
#endif

    m_CustomBuildCommand += "set PTB_PATH="  + ptb_path_par  + "\n";
    m_CustomBuildCommand +=
        "if exist \"%PREBUILT_PTB_EXE%\" (set PTB_EXE=%PREBUILT_PTB_EXE%)";
    m_CustomBuildCommand +=
        " else (set PTB_EXE=%PTB_PATH%\\project_tree_builder.exe)\n";
    m_CustomBuildCommand += "set TREE_ROOT=" + tree_root_par + "\n";
    m_CustomBuildCommand += "set SLN_PATH="  + sln_path_par  + "\n";
    m_CustomBuildCommand += "set PTB_PLATFORM=$(PlatformName)\n";
    if (m_BuildPtb) {
        m_CustomBuildOutput   = ptb_path_par  + "\\project_tree_builder.exe";
    }

    // Build command for project_tree_builder.sln
    if (m_BuildPtb) {
        m_CustomBuildCommand += 
            "if not exist \"%PTB_EXE%\" (";
        if (CMsvc7RegSettings::GetMsvcVersion() == CMsvc7RegSettings::eMsvc710) {
            m_CustomBuildCommand += 
                "devenv /rebuild Release /project project_tree_builder.exe ";
//           "devenv /build $(ConfigurationName) /project project_tree_builder.exe ";
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
                CDirEntry::ConcatPath(project_tree_builder_sln_dir, "build-system");
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
            
            m_CustomBuildCommand += project_tree_builder_sln_path;
        } else {
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
                CDirEntry::AddTrailingPathSeparator(project_tree_builder_sln_dir);
            
            string project_tree_builder_sln_path = 
                CDirEntry::ConcatPath(project_tree_builder_sln_dir, 
                                    "ncbi_cpp.sln");
            project_tree_builder_sln_path = 
                CDirEntry::CreateRelativePath(project_dir, 
                                            project_tree_builder_sln_path);
            
            m_CustomBuildCommand += "msbuild \"";
            m_CustomBuildCommand += project_tree_builder_sln_path;
            m_CustomBuildCommand +=
                "\" /t:\"project_tree_builder_exe:Rebuild\"";
            m_CustomBuildCommand +=
                " /p:Configuration=ReleaseDLL;Platform=$(PlatformName)";
            if (CMsvc7RegSettings::GetMsvcVersion() >= CMsvc7RegSettings::eMsvc900) {
                m_CustomBuildCommand += " /maxcpucount";
            }
        }
        m_CustomBuildCommand += ") else (echo USING PREBUILT PTB at %PTB_EXE%)\n";
        m_CustomBuildCommand += "if errorlevel 1 exit 1\n";
    }

    string lock_ptb_config = "lock_ptb_config.bat";
    string lock_ptb_config_path = "$(ProjectDir)" +
        CDirEntry::AddTrailingPathSeparator(
            CDirEntry::CreateRelativePath(project_dir, 
                CDirEntry::ConcatPath(GetApp().GetProjectTreeInfo().m_Compilers, 
                                      GetApp().GetRegSettings().m_CompilersSubdir)));
    m_CustomBuildCommand += "call \"" + lock_ptb_config_path +
        lock_ptb_config + "\" ON \"" + lock_ptb_config_path + "\"\n";
    m_CustomBuildCommand += "if errorlevel 1 exit 1\n";
    // Make *.bat file from source file of this custom build.
    // This file ( see CreateProjectFileItem below )
    // will use defines PTB_PATH, TREE_ROOT and SLN_PATH mentioned above
    m_CustomBuildCommand += "cd $(InputDir)\n";
    m_CustomBuildCommand += "copy /Y $(InputFileName) $(InputName).bat\n";
    m_CustomBuildCommand += "call $(InputName).bat\n";
    m_CustomBuildCommand += "call \"" + lock_ptb_config_path +
        lock_ptb_config + "\" OFF \"" + lock_ptb_config_path + "\"\n";
    m_CustomBuildCommand += "del /Q $(InputName).bat\n";

    CreateUtilityProject(m_Name, m_Configs, &m_Xmlprj);
    CreateUtilityProject(m_NameGui, m_Configs, &m_XmlprjGui);
}


CMsvcConfigureProjectGenerator::~CMsvcConfigureProjectGenerator(void)
{
}


void CMsvcConfigureProjectGenerator::SaveProject(bool with_gui)
{
    string name(with_gui ? m_NameGui : m_Name);
    string srcfile(with_gui ? m_SrcFileNameGui : m_SrcFileName);
    CVisualStudioProject& xmlprj = with_gui ? m_XmlprjGui : m_Xmlprj;

    {{
        CRef<CFilter> filter(new CFilter());
        filter->SetAttlist().SetName("Configure");
        filter->SetAttlist().SetFilter("");

        //Include collected source files
        CreateProjectFileItem(with_gui);
        CRef< CFFile > file(new CFFile());
        file->SetAttlist().SetRelativePath(srcfile + m_ProjectItemExt);
        SCustomBuildInfo build_info;
        string source_file_path_abs = 
            CDirEntry::ConcatPath(m_ProjectDir, srcfile + m_ProjectItemExt);
        source_file_path_abs = CDirEntry::NormalizePath(source_file_path_abs);
        build_info.m_SourceFile  = source_file_path_abs;
        build_info.m_Description = "Configure solution : $(SolutionName)";
        build_info.m_CommandLine = m_CustomBuildCommand;
        string outputs("$(InputPath).aanofile.out;");
        outputs += m_CustomBuildOutput;
        build_info.m_Outputs     = outputs;//"$(InputPath).aanofile.out";
        
        AddCustomBuildFileToFilter(filter, 
                                   m_Configs, 
                                   m_ProjectDir, 
                                   build_info);
        xmlprj.SetFiles().SetFilter().push_back(filter);

    }}



    SaveIfNewer(GetPath(with_gui), xmlprj);
}

string CMsvcConfigureProjectGenerator::GetPath(bool with_gui) const
{
    string project_path = CDirEntry::ConcatPath(m_ProjectDir, "_CONFIGURE_");
    if (with_gui) {
        project_path += "DIALOG_";
    }
    project_path += MSVC_PROJECT_FILE_EXT;
    return project_path;
}

const CVisualStudioProject&
CMsvcConfigureProjectGenerator::GetVisualStudioProject(bool with_gui) const
{
    if (with_gui) {
        return m_XmlprjGui;
    }
    return m_Xmlprj;
}

void CMsvcConfigureProjectGenerator::CreateProjectFileItem(bool with_gui) const
{
    string file_path = CDirEntry::ConcatPath(m_ProjectDir,
        with_gui ? m_SrcFileNameGui : m_SrcFileName);
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

    ofs << "@echo off" << endl
        << "echo ******************************************************************************" << endl
        << "echo Running -CONFIGURE- please wait" << endl
        << "echo ******************************************************************************" << endl
        << "@echo on" << endl;
    ofs << "\"%PTB_EXE%\"";

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
    if (with_gui /*|| GetApp().m_ConfirmCfg*/) {
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
        << "exit /b 0" << endl
        << endl
        << ":report_error" << endl
        << "echo ******************************************************************************" << endl
        << "echo -CONFIGURE- has failed" << endl
        << "echo Configuration log was saved at \"file://%SLN_PATH%_configuration_log.txt\"" << endl
        << "echo ******************************************************************************" << endl;
    if (!with_gui) {
        ofs << "if .%DIAG_SILENT_ABORT%==. ";
    }
    ofs << "start \"\" \"%SLN_PATH%_configuration_log.txt\"" << endl
        << "exit /b 1" << endl;
}
#endif //NCBI_COMPILER_MSVC

END_NCBI_SCOPE
