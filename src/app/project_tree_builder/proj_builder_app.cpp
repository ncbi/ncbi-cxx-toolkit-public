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

#include <app/project_tree_builder/proj_builder_app.hpp>
#include <app/project_tree_builder/proj_item.hpp>
#include <app/project_tree_builder/msvc_prj_utils.hpp>
#include <app/project_tree_builder/msvc_prj_generator.hpp>
#include <app/project_tree_builder/msvc_sln_generator.hpp>
#include <app/project_tree_builder/msvc_masterproject_generator.hpp>
#include <app/project_tree_builder/proj_utils.hpp>


BEGIN_NCBI_SCOPE


CProjBulderApp::CProjBulderApp(void)
{
}


void CProjBulderApp::Init(void)
{
    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "MSVC 7.10 projects builder application");

    // Programm arguments:

    arg_desc->AddPositional("root",
                            "Root directory of the build tree. "\
                                "This directory ends with \"c++\".",
                            CArgDescriptions::eString);

    arg_desc->AddPositional("subtree",
                            "Subtree to build. Example: src/corelib/ .",
                            CArgDescriptions::eString);

    arg_desc->AddPositional("solution", 
                            "MSVC Solution to buld.",
						    CArgDescriptions::eString);


    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}


int CProjBulderApp::Run(void)
{
    LOG_POST("Started.");
    // Get and check arguments
    ParseArguments();

	// Set error posting and tracing on maximum.
	SetDiagTrace(eDT_Enable);
	SetDiagPostFlag(eDPF_Default);
	SetDiagPostLevel(eDiag_Info);


    // Build projects tree
    CProjectItemsTree projects_tree(m_RootSrc);
    CProjectTreeBuilder::BuildProjectTree(m_SubTree, 
                                          m_RootSrc, 
                                          &projects_tree);

    // MSVC specific part:
    
    // Projects
    CMsvcProjectGenerator prj_gen(GetRegSettings().m_ConfigInfo);

    ITERATE(CProjectItemsTree::TProjects, p, projects_tree.m_Projects) {
        prj_gen.Generate(p->second);
    }

    // MasterProject
    CMsvcMasterProjectGenerator master_prj_gen(projects_tree,
                                               GetRegSettings().m_ConfigInfo,
                                               CDirEntry(m_Solution).GetDir());
    master_prj_gen.SaveProject("_MasterProject");

    // Solution
    CMsvcSolutionGenerator sln_gen(GetRegSettings().m_ConfigInfo);
    ITERATE(CProjectItemsTree::TProjects, p, projects_tree.m_Projects) {
        sln_gen.AddProject(p->second);
    }
    sln_gen.AddMasterProject("_MasterProject");
    sln_gen.SaveSolution(m_Solution);

    //
    LOG_POST("Finished.");
    return 0;
}

void CProjBulderApp::Exit(void)
{
	//TODO
}


void CProjBulderApp::ParseArguments(void)
{
    CArgs args = GetArgs();

    // Root
    m_Root     = CDirEntry::AddTrailingPathSeparator(args["root"].AsString());
    m_RootSrc  = CDirEntry::AddTrailingPathSeparator
                    (CDirEntry::ConcatPath(m_Root, "src"));

    // Subtree to build
    string subtree = args["subtree"].AsString();
    m_SubTree  = CDirEntry::ConcatPath(m_Root, subtree);

    // Solution
    m_Solution = args["solution"].AsString();
}


int CProjBulderApp::EnumOpt(const string& enum_name, 
                            const string& enum_val) const
{
    int opt = GetConfig().GetInt(enum_name, enum_val, -1);
    if (opt == -1) {
	    NCBI_THROW(CProjBulderAppException, eEnumValue, 
                                enum_name + "::" + enum_val);
    }
    return opt;
}


void CProjBulderApp::DumpFiles(const TFiles& files, 
							   const string& filename) const
{
    CNcbiOfstream  ofs(filename.c_str(), IOS_BASE::out | IOS_BASE::trunc);
    if ( !ofs ) {
	    NCBI_THROW(CProjBulderAppException, eFileCreation, filename);
    }

    ITERATE(TFiles, p, files) {
	    ofs << "+++++++++++++++++++++++++\n";
	    ofs << p->first << endl;
	    p->second.Dump(ofs);
	    ofs << "-------------------------\n";
    }
}


void CProjBulderApp::GetMetaDataFiles(list<string>* files) const
{
    files->clear();
    string files_str = GetConfig().GetString("Common", "MetaData", "");
    NStr::Split(files_str, " \t,", *files);
}


void CProjBulderApp::GetBuildConfigs(list<SConfigInfo>* configs) const
{
    configs->clear();
    string config_str = GetConfig().GetString("msvc7", "Configurations", "");
    list<string> configs_list;
    NStr::Split(config_str, " \t,", configs_list);
    ITERATE(list<string>, p, configs_list) {

        const string& config_name = *p;
        SConfigInfo config;
        config.m_Name  = config_name;
        config.m_Debug = GetConfig().GetString(config_name, 
                                               "Debug",
                                               "FALSE") != "FALSE";
        config.m_RuntimeLibrary = GetConfig().GetString(config_name, 
                                                        "runtimeLibraryOption",
                                                        "0");
        configs->push_back(config);
    }
}


const CMsvc7RegSettings& CProjBulderApp::GetRegSettings(void)
{
    if ( !m_MsvcRegSettings.get() ) {
        m_MsvcRegSettings = auto_ptr<CMsvc7RegSettings>(new CMsvc7RegSettings());
    
        m_MsvcRegSettings->m_Version = 
            GetConfig().GetString("msvc7", "Version", "7.10");

        GetBuildConfigs(&m_MsvcRegSettings->m_ConfigInfo);

        m_MsvcRegSettings->m_ProjectEngineName = 
            GetConfig().GetString("msvc7", "Name", "VisualStudioProject");
    
        m_MsvcRegSettings->m_Encoding = 
            GetConfig().GetString("msvc7", "encoding","Windows-1252");

        m_MsvcRegSettings->m_CompilersSubdir  = 
            GetConfig().GetString("msvc7", "compilers", "msvc7_prj");
    
        m_MsvcRegSettings->m_MakefilesExt = 
            GetConfig().GetString("msvc7", "MakefilesExt", "msvc");
    }
    return *m_MsvcRegSettings;
}


const CMsvcSite& CProjBulderApp::GetSite(void)
{
    if ( !m_MsvcSite.get() ) 
        m_MsvcSite = auto_ptr<CMsvcSite>(new CMsvcSite(GetConfig()));
    
    return *m_MsvcSite;
}


const CMsvcMetaMakefile& CProjBulderApp::GetMetaMakefile(void)
{
    if ( !m_MsvcMetaMakefile.get() ) {
        //Get metamakefile file name from the registry
        string meta_makefile_fname = 
            GetConfig().GetString("Common", 
                                  "MetaMakefile", "Makefile.mk.in.msvc");
        
        //Metamakefile must be in RootSrc directory
        m_MsvcMetaMakefile = 
            auto_ptr<CMsvcMetaMakefile>
                (new CMsvcMetaMakefile
                        (CDirEntry::ConcatPath(m_RootSrc,
                                               meta_makefile_fname)));
        
        //Metamakefile must present and must not be empty
        if ( m_MsvcMetaMakefile->Empty() )
            NCBI_THROW(CProjBulderAppException, 
                       eMetaMakefile, meta_makefile_fname);
    }

    return *m_MsvcMetaMakefile;
}


CProjBulderApp& GetApp(void)
{
    static CProjBulderApp theApp;
    return theApp;
}

END_NCBI_SCOPE


USING_NCBI_SCOPE;

int main(int argc, const char* argv[])
{
    // Execute main application function
    return GetApp().AppMain(argc, argv, 0, eDS_Default, 0);
}


/*
 * ===========================================================================
 * $Log$
 * Revision 1.6  2004/01/26 19:27:30  gorelenk
 * += MSVC meta makefile support
 * += MSVC project makefile support
 *
 * Revision 1.5  2004/01/22 17:57:55  gorelenk
 * first version
 *
 * ===========================================================================
 */


