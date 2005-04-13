#include <ncbi_pch.hpp>
#include <app/project_tree_builder/msvc_prj_generator.hpp>
#include <app/project_tree_builder/proj_builder_app.hpp>
#include <app/project_tree_builder/msvc_project_context.hpp>


#include <app/project_tree_builder/msvc_prj_utils.hpp>
#include <app/project_tree_builder/msvc_prj_defines.hpp>
#include <app/project_tree_builder/msvc_prj_files_collector.hpp>

#include <algorithm>

BEGIN_NCBI_SCOPE


static 
void s_CreateDatatoolCustomBuildInfo(const CProjItem&              prj,
                                     const CMsvcPrjProjectContext& context,
                                     const CDataToolGeneratedSrc&  src,                                   
                                     SCustomBuildInfo*             build_info);


//-----------------------------------------------------------------------------

CMsvcProjectGenerator::CMsvcProjectGenerator(const list<SConfigInfo>& configs)
    :m_Configs(configs)
{
}


CMsvcProjectGenerator::~CMsvcProjectGenerator(void)
{
}


void CMsvcProjectGenerator::Generate(const CProjItem& prj)
{
    // Already have it
    if ( prj.m_ProjType == CProjKey::eMsvc) {
        return;
    }

    CMsvcPrjProjectContext project_context(prj);
    CVisualStudioProject xmlprj;

    // Checking configuration availability:
    string str_log;
    list<SConfigInfo> project_configs;
    ITERATE(list<SConfigInfo>, p , m_Configs) {
        const SConfigInfo& cfg_info = *p;
        string unmet;
        // Check config availability
        if ( !project_context.IsConfigEnabled(cfg_info, &unmet) ) {
            str_log += " " + cfg_info.m_Name + "(because of " + unmet + ")";
        } else {
            project_configs.push_back(cfg_info);
        }
    }
    if (!str_log.empty()) {
        LOG_POST(Warning << prj.m_ID << ": disabled configurations: " << str_log);
    }
    if (project_configs.empty()) {
        LOG_POST(Warning << prj.m_ID << ": skipped (all configurations are disabled)");
        return;
    }
    
    // Attributes:
    {{
        xmlprj.SetAttlist().SetProjectType (MSVC_PROJECT_PROJECT_TYPE);
        xmlprj.SetAttlist().SetVersion     (MSVC_PROJECT_VERSION);
        xmlprj.SetAttlist().SetName        (project_context.ProjectName());
        xmlprj.SetAttlist().SetKeyword     (MSVC_PROJECT_KEYWORD_WIN32);
    }}

    // Platforms
    {{
        CRef<CPlatform> platform(new CPlatform(""));
        platform->SetAttlist().SetName(MSVC_PROJECT_PLATFORM);
        xmlprj.SetPlatforms().SetPlatform().push_back(platform);
    }}

    ITERATE(list<SConfigInfo>, p , project_configs) {

        const SConfigInfo& cfg_info = *p;
 
        // Contexts:
        
        CMsvcPrjGeneralContext general_context(cfg_info, project_context);

        // MSVC Tools
        CMsvcTools msvc_tool(general_context, project_context);

        CRef<CConfiguration> conf(new CConfiguration());

#define BIND_TOOLS(tool, msvctool, X) \
                  tool->SetAttlist().Set##X(msvctool->X())

        // Configuration
        {{
            BIND_TOOLS(conf, msvc_tool.Configuration(), Name);
            BIND_TOOLS(conf, msvc_tool.Configuration(), OutputDirectory);
            BIND_TOOLS(conf, msvc_tool.Configuration(), IntermediateDirectory);
            BIND_TOOLS(conf, msvc_tool.Configuration(), ConfigurationType);
            BIND_TOOLS(conf, msvc_tool.Configuration(), CharacterSet);
        }}
       
        // Compiler
        {{
            CRef<CTool> tool(new CTool("")); 

            BIND_TOOLS(tool, msvc_tool.Compiler(), Name);
            BIND_TOOLS(tool, msvc_tool.Compiler(), Optimization);
            //AdditionalIncludeDirectories - more dirs are coming from makefile
            BIND_TOOLS(tool, 
                       msvc_tool.Compiler(), AdditionalIncludeDirectories);
            BIND_TOOLS(tool, msvc_tool.Compiler(), PreprocessorDefinitions);
            BIND_TOOLS(tool, msvc_tool.Compiler(), MinimalRebuild);
            BIND_TOOLS(tool, msvc_tool.Compiler(), BasicRuntimeChecks);
            BIND_TOOLS(tool, msvc_tool.Compiler(), RuntimeLibrary);
            BIND_TOOLS(tool, msvc_tool.Compiler(), RuntimeTypeInfo);
            BIND_TOOLS(tool, msvc_tool.Compiler(), UsePrecompiledHeader);
            BIND_TOOLS(tool, msvc_tool.Compiler(), WarningLevel);
            BIND_TOOLS(tool,
                       msvc_tool.Compiler(), Detect64BitPortabilityProblems);
            BIND_TOOLS(tool, msvc_tool.Compiler(), DebugInformationFormat);
            BIND_TOOLS(tool, msvc_tool.Compiler(), CompileAs);
            BIND_TOOLS(tool, msvc_tool.Compiler(), InlineFunctionExpansion);
            BIND_TOOLS(tool, msvc_tool.Compiler(), OmitFramePointers);
            BIND_TOOLS(tool, msvc_tool.Compiler(), StringPooling);
            BIND_TOOLS(tool, msvc_tool.Compiler(), EnableFunctionLevelLinking);
            BIND_TOOLS(tool, msvc_tool.Compiler(), OptimizeForProcessor);
            BIND_TOOLS(tool, msvc_tool.Compiler(), StructMemberAlignment);
            BIND_TOOLS(tool, msvc_tool.Compiler(), CallingConvention);
            BIND_TOOLS(tool, msvc_tool.Compiler(), IgnoreStandardIncludePath);
            BIND_TOOLS(tool, msvc_tool.Compiler(), ExceptionHandling);
            BIND_TOOLS(tool, msvc_tool.Compiler(), BufferSecurityCheck);
            BIND_TOOLS(tool, msvc_tool.Compiler(), DisableSpecificWarnings);
            BIND_TOOLS(tool, 
                       msvc_tool.Compiler(), UndefinePreprocessorDefinitions);
            BIND_TOOLS(tool, msvc_tool.Compiler(), AdditionalOptions);
            BIND_TOOLS(tool, msvc_tool.Compiler(), GlobalOptimizations);
            BIND_TOOLS(tool, msvc_tool.Compiler(), FavorSizeOrSpeed);
            BIND_TOOLS(tool, msvc_tool.Compiler(), BrowseInformation);

            conf->SetTool().push_back(tool);
        }}

        // Linker
        {{
            CRef<CTool> tool(new CTool(""));

            BIND_TOOLS(tool, msvc_tool.Linker(), Name);
            BIND_TOOLS(tool, msvc_tool.Linker(), AdditionalOptions);
            BIND_TOOLS(tool, msvc_tool.Linker(), OutputFile);
            BIND_TOOLS(tool, msvc_tool.Linker(), LinkIncremental);
            BIND_TOOLS(tool, msvc_tool.Linker(), GenerateDebugInformation);
            BIND_TOOLS(tool, msvc_tool.Linker(), ProgramDatabaseFile);
            BIND_TOOLS(tool, msvc_tool.Linker(), SubSystem);
            BIND_TOOLS(tool, msvc_tool.Linker(), ImportLibrary);
            BIND_TOOLS(tool, msvc_tool.Linker(), TargetMachine);
            BIND_TOOLS(tool, msvc_tool.Linker(), OptimizeReferences);
            BIND_TOOLS(tool, msvc_tool.Linker(), EnableCOMDATFolding);
            BIND_TOOLS(tool, msvc_tool.Linker(), IgnoreAllDefaultLibraries);
            BIND_TOOLS(tool, msvc_tool.Linker(), IgnoreDefaultLibraryNames);
            BIND_TOOLS(tool, msvc_tool.Linker(), AdditionalLibraryDirectories);

            conf->SetTool().push_back(tool);
        }}

        // Librarian
        {{
            CRef<CTool> tool(new CTool(""));

            BIND_TOOLS(tool, msvc_tool.Librarian(), Name);
            BIND_TOOLS(tool, msvc_tool.Librarian(), AdditionalOptions);
            BIND_TOOLS(tool, msvc_tool.Librarian(), OutputFile);
            BIND_TOOLS(tool, msvc_tool.Librarian(), IgnoreAllDefaultLibraries);
            BIND_TOOLS(tool, msvc_tool.Librarian(), IgnoreDefaultLibraryNames);
            BIND_TOOLS(tool, 
                       msvc_tool.Librarian(), AdditionalLibraryDirectories);

            conf->SetTool().push_back(tool);
        }}

        // CustomBuildTool
        {{
            CRef<CTool> tool(new CTool(""));
            BIND_TOOLS(tool, msvc_tool.CustomBuid(), Name);
            conf->SetTool().push_back(tool);
        }}

        // MIDL
        {{
            CRef<CTool> tool(new CTool(""));
            BIND_TOOLS(tool, msvc_tool.MIDL(), Name);
            conf->SetTool().push_back(tool);
        }}

        // PostBuildEvent
        {{
            CRef<CTool> tool(new CTool(""));
            BIND_TOOLS(tool, msvc_tool.PostBuildEvent(), Name);
            conf->SetTool().push_back(tool);
        }}

        // PreBuildEvent
        {{
            CRef<CTool> tool(new CTool(""));
            BIND_TOOLS(tool, msvc_tool.PreBuildEvent(), Name);
            BIND_TOOLS(tool, msvc_tool.PreBuildEvent(), CommandLine);
            conf->SetTool().push_back(tool);
        }}

        // PreLinkEvent
        {{
            CRef<CTool> tool(new CTool(""));
            BIND_TOOLS(tool, msvc_tool.PreLinkEvent(), Name);
            conf->SetTool().push_back(tool);
        }}

        // ResourceCompiler
        {{
            CRef<CTool> tool(new CTool(""));
            BIND_TOOLS(tool, msvc_tool.ResourceCompiler(), Name);
            BIND_TOOLS(tool, 
                       msvc_tool.ResourceCompiler(), 
                       AdditionalIncludeDirectories);
            BIND_TOOLS(tool, 
                       msvc_tool.ResourceCompiler(), 
                       AdditionalOptions);
            BIND_TOOLS(tool, msvc_tool.ResourceCompiler(), Culture);
            BIND_TOOLS(tool, 
                       msvc_tool.ResourceCompiler(), 
                       PreprocessorDefinitions);
            conf->SetTool().push_back(tool);
        }}

        // WebServiceProxyGenerator
        {{
            CRef<CTool> tool(new CTool(""));
            BIND_TOOLS(tool, msvc_tool.WebServiceProxyGenerator(), Name);
            conf->SetTool().push_back(tool);
        }}

        // XMLDataGenerator
        {{
            CRef<CTool> tool(new CTool(""));
            BIND_TOOLS(tool, msvc_tool.XMLDataGenerator(), Name);
            conf->SetTool().push_back(tool);
        }}

        // ManagedWrapperGenerator
        {{
            CRef<CTool> tool(new CTool(""));
            BIND_TOOLS(tool, msvc_tool.ManagedWrapperGenerator(), Name);
            conf->SetTool().push_back(tool);
        }}

        // AuxiliaryManagedWrapperGenerator
        {{
            CRef<CTool> tool(new CTool(""));
            BIND_TOOLS(tool, 
                       msvc_tool.AuxiliaryManagedWrapperGenerator(),
                       Name);
            conf->SetTool().push_back(tool);
        }}

        xmlprj.SetConfigurations().SetConfiguration().push_back(conf);
    }
    // References
    {{
        xmlprj.SetReferences("");
    }}

    // Collect all source, header, inline, resource files
    CMsvcPrjFilesCollector collector(project_context, project_configs, prj);
    
    // Insert sources, headers, inlines:
    auto_ptr<IFilesToProjectInserter> inserter;

    if (prj.m_ProjType == CProjKey::eDll) {
        inserter.reset(new CDllProjectFilesInserter
                                    (&xmlprj,
                                     CProjKey(prj.m_ProjType, prj.m_ID), 
                                     project_configs, 
                                     project_context.ProjectDir()));

    } else {
        inserter.reset(new CBasicProjectsFilesInserter 
                                     (&xmlprj,
                                      prj.m_ID,
                                      project_configs, 
                                      project_context.ProjectDir()));
    }

    ITERATE(list<string>, p, collector.SourceFiles()) {
        //Include collected source files
        const string& rel_source_file = *p;
        inserter->AddSourceFile(rel_source_file);
    }

    ITERATE(list<string>, p, collector.HeaderFiles()) {
        //Include collected header files
        const string& rel_source_file = *p;
        inserter->AddHeaderFile(rel_source_file);
    }
    ITERATE(list<string>, p, collector.InlineFiles()) {
        //Include collected inline files
        const string& rel_source_file = *p;
        inserter->AddInlineFile(rel_source_file);
    }
    inserter->Finalize();

    {{
        //Resource Files - header files - empty
        CRef<CFilter> filter(new CFilter());
        filter->SetAttlist().SetName("Resource Files");
        filter->SetAttlist().SetFilter
            ("rc;ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe;resx");

        ITERATE(list<string>, p, collector.ResourceFiles()) {
            //Include collected header files
            CRef<CFFile> file(new CFFile());
            file->SetAttlist().SetRelativePath(*p);

            CRef< CFilter_Base::C_FF::C_E > ce(new CFilter_Base::C_FF::C_E());
            ce->SetFile(*file);
            filter->SetFF().SetFF().push_back(ce);
        }

        xmlprj.SetFiles().SetFilter().push_back(filter);
    }}
    {{
        //Custom Build files
        const list<SCustomBuildInfo>& info_list = 
            project_context.GetCustomBuildInfo();

        if ( !info_list.empty() ) {
            CRef<CFilter> filter(new CFilter());
            filter->SetAttlist().SetName("Custom Build Files");
            filter->SetAttlist().SetFilter("");
            
            ITERATE(list<SCustomBuildInfo>, p, info_list) { 
                const SCustomBuildInfo& build_info = *p;
                AddCustomBuildFileToFilter(filter, 
                                           project_configs, 
                                           project_context.ProjectDir(),
                                           build_info);
            }

            xmlprj.SetFiles().SetFilter().push_back(filter);
        }
    }}
    {{
        //Datatool files
        if ( !prj.m_DatatoolSources.empty() ) {
            
            CRef<CFilter> filter(new CFilter());
            filter->SetAttlist().SetName("Datatool Files");
            filter->SetAttlist().SetFilter("");

            ITERATE(list<CDataToolGeneratedSrc>, p, prj.m_DatatoolSources) {

                const CDataToolGeneratedSrc& src = *p;
                SCustomBuildInfo build_info;
                s_CreateDatatoolCustomBuildInfo(prj, 
                                              project_context, 
                                              src, 
                                              &build_info);
                AddCustomBuildFileToFilter(filter, 
                                           project_configs, 
                                           project_context.ProjectDir(), 
                                           build_info);
            }

            xmlprj.SetFiles().SetFilter().push_back(filter);
        }
    }}

    {{
        //Globals
        xmlprj.SetGlobals("");
    }}


    string project_path = CDirEntry::ConcatPath(project_context.ProjectDir(), 
                                                project_context.ProjectName());
    project_path += MSVC_PROJECT_FILE_EXT;

    SaveIfNewer(project_path, xmlprj);
}

static 
void s_CreateDatatoolCustomBuildInfo(const CProjItem&              prj,
                                     const CMsvcPrjProjectContext& context,
                                     const CDataToolGeneratedSrc&  src,                                   
                                     SCustomBuildInfo*             build_info)
{
    build_info->Clear();

    //SourceFile
    build_info->m_SourceFile = 
        CDirEntry::ConcatPath(src.m_SourceBaseDir, src.m_SourceFile);

    //CommandLine
    //exe location - path is supposed to be relative encoded
    string tool_exe_location("\"");
    if (prj.m_ProjType == CProjKey::eApp)
        tool_exe_location += GetApp().GetDatatoolPathForApp();
    else if (prj.m_ProjType == CProjKey::eLib)
        tool_exe_location += GetApp().GetDatatoolPathForLib();
    else if (prj.m_ProjType == CProjKey::eDll)
        tool_exe_location += GetApp().GetDatatoolPathForApp();
    else
        return;
    tool_exe_location += "\"";
    //command line
    string tool_cmd_prfx = GetApp().GetDatatoolCommandLine();
    tool_cmd_prfx += " -or ";
    tool_cmd_prfx += 
        CDirEntry::CreateRelativePath(GetApp().GetProjectTreeInfo().m_Src,
                                      src.m_SourceBaseDir);
    tool_cmd_prfx += " -oR ";
    tool_cmd_prfx += CDirEntry::CreateRelativePath(context.ProjectDir(),
                                         GetApp().GetProjectTreeInfo().m_Root);

    string tool_cmd(tool_cmd_prfx);
    if ( !src.m_ImportModules.empty() ) {
        tool_cmd += " -M \"";
        tool_cmd += NStr::Join(src.m_ImportModules, " ");
        tool_cmd += '"';
    }
    build_info->m_CommandLine = 
        "@echo on\n" + tool_exe_location + " " + tool_cmd;

    //Description
    build_info->m_Description = 
        "Using datatool to create a C++ object from ASN/DTD $(InputPath)";

    //Outputs
#if 0
    // if I do this then ASN src will always be regenerated
    string combined;
    list<string> cmd_args;
    NStr::Split(tool_cmd_prfx, LIST_SEPARATOR, cmd_args);
    list<string>::const_iterator i = find(cmd_args.begin(), cmd_args.end(), "-oc");
    if (i != cmd_args.end()) {
        ++i;
        combined = "$(InputDir)" + *i + "__.cpp;" + "$(InputDir)" + *i + "___.cpp;";
    }
#endif
    build_info->m_Outputs = "$(InputDir)$(InputName).files;";

    //Additional Dependencies
    build_info->m_AdditionalDependencies = "$(InputDir)$(InputName).def;" + tool_exe_location;
}


END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.41  2005/04/13 15:57:33  gouriano
 * Handle paths with spaces
 *
 * Revision 1.40  2004/12/20 15:23:44  gouriano
 * Changed diagnostic output
 *
 * Revision 1.39  2004/12/06 18:12:20  gouriano
 * Improved diagnostics
 *
 * Revision 1.38  2004/10/13 13:36:38  gouriano
 * add dependency on datatool to ASN projects
 *
 * Revision 1.37  2004/10/12 16:18:26  ivanov
 * Added configurable file support
 *
 * Revision 1.36  2004/08/20 13:35:46  gouriano
 * Added warning when all project configurations are disabled
 *
 * Revision 1.35  2004/06/10 15:16:46  gorelenk
 * Changed macrodefines to be comply with GCC 3.4.0 .
 *
 * Revision 1.34  2004/05/26 18:01:46  gorelenk
 * Changed CMsvcProjectGenerator::Generate :
 * Changed insertion fo source files, headers, inlines.
 * Changed s_CreateDatatoolCustomBuildInfo :
 * used $(InputDir)$(InputName).files as a datatool sources custom build step
 * output.
 *
 * Revision 1.33  2004/05/21 21:41:41  gorelenk
 * Added PCH ncbi_pch.hpp
 *
 * Revision 1.32  2004/05/10 19:55:08  gorelenk
 * Changed CMsvcProjectGenerator::Generate .
 *
 * Revision 1.31  2004/05/10 14:28:01  gorelenk
 * Changed implementation CMsvcProjectGenerator::Generate .
 *
 * Revision 1.30  2004/04/30 13:57:01  dicuccio
 * Fixed project generation to correct dependency rules for datatool-generated
 * code.
 *
 * Revision 1.29  2004/03/18 22:44:26  gorelenk
 * Changed implementation of CMsvcProjectGenerator::Generate - implemented
 * custom builds only in available configurations.
 *
 * Revision 1.28  2004/03/16 21:26:26  gorelenk
 * Added generation dependencies from $(InputDir)$(InputName).def
 * to s_CreateDatatoolCustomBuildInfo.
 *
 * Revision 1.27  2004/03/15 22:22:01  gorelenk
 * Changed implementation of s_CreateDatatoolCustomBuildInfo.
 *
 * Revision 1.26  2004/03/10 16:39:56  gorelenk
 * Changed LOG_POST inside CMsvcProjectGenerator::Generate.
 *
 * Revision 1.25  2004/03/08 23:35:04  gorelenk
 * Changed implementation of CMsvcProjectGenerator::Generate.
 *
 * Revision 1.24  2004/03/05 20:31:54  gorelenk
 * Files collecting functionality was moved to separate class :
 * CMsvcPrjFilesCollector.
 *
 * Revision 1.23  2004/03/05 18:07:22  gorelenk
 * Changed implementation of s_CreateDatatoolCustomBuildInfo .
 *
 * Revision 1.22  2004/02/25 20:19:36  gorelenk
 * Changed implementation of function s_IsInsideDatatoolSourceDir.
 *
 * Revision 1.21  2004/02/25 19:42:35  gorelenk
 * Changed implementation of function s_IsProducedByDatatool.
 *
 * Revision 1.20  2004/02/24 21:03:06  gorelenk
 * Added checking of config availability to implementation of
 * member-function Generate of class CMsvcProjectGenerator.
 *
 * Revision 1.19  2004/02/23 20:42:57  gorelenk
 * Added support of MSVC ResourceCompiler tool.
 *
 * Revision 1.18  2004/02/20 22:53:26  gorelenk
 * Added analysis of ASN projects depends.
 *
 * Revision 1.17  2004/02/13 23:11:10  gorelenk
 * Changed implementation of function s_CreateDatatoolCustomBuildInfo - added
 * list of output files to generated SCustomBuildInfo.
 *
 * Revision 1.16  2004/02/13 20:31:20  gorelenk
 * Changed source relative path for datatool input files.
 *
 * Revision 1.15  2004/02/12 16:27:57  gorelenk
 * Changed generation of command line for datatool.
 *
 * Revision 1.14  2004/02/10 18:09:12  gorelenk
 * Added definitions of functions SaveIfNewer and PromoteIfDifferent
 * - support for file overwriting only if it was changed.
 *
 * Revision 1.13  2004/02/06 23:14:59  gorelenk
 * Implemented support of ASN projects, semi-auto configure,
 * CPPFLAGS support. Second working version.
 *
 * Revision 1.12  2004/02/05 00:02:08  gorelenk
 * Added support of user site and  Makefile defines.
 *
 * Revision 1.11  2004/02/03 17:20:47  gorelenk
 * Changed implementation of method Generate of class CMsvcProjectGenerator.
 *
 * Revision 1.10  2004/01/30 20:46:55  gorelenk
 * Added support of ASN projects.
 *
 * Revision 1.9  2004/01/29 15:48:58  gorelenk
 * Support of projects fitering transfered to CProjBulderApp class
 *
 * Revision 1.8  2004/01/28 17:55:48  gorelenk
 * += For msvc makefile support of :
 *                 Requires tag, ExcludeProject tag,
 *                 AddToProject section (SourceFiles and IncludeDirs),
 *                 CustomBuild section.
 * += For support of user local site.
 *
 * Revision 1.7  2004/01/26 19:27:28  gorelenk
 * += MSVC meta makefile support
 * += MSVC project makefile support
 *
 * Revision 1.6  2004/01/22 17:57:54  gorelenk
 * first version
 *
 * ===========================================================================
 */
