#include <ncbi_pch.hpp>
#include "msvc_prj_generator.hpp"
#include "proj_builder_app.hpp"
#include "msvc_project_context.hpp"


#include "msvc_prj_utils.hpp"
#include "msvc_prj_defines.hpp"
#include "msvc_prj_files_collector.hpp"
#include "ptb_err_codes.hpp"

#include <algorithm>

BEGIN_NCBI_SCOPE

#if NCBI_COMPILER_MSVC

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


void CMsvcProjectGenerator::Generate(CProjItem& prj)
{
    // Already have it
    if ( prj.m_ProjType == CProjKey::eMsvc) {
        return;
    }
    if (prj.m_GUID.empty()) {
        prj.m_GUID = GenerateSlnGUID();
    }

    CMsvcPrjProjectContext project_context(prj);
    CVisualStudioProject xmlprj;

    // Checking configuration availability:
    string str_log, req_log;
    list<SConfigInfo> project_configs;
    int failed=0;
    ITERATE(list<SConfigInfo>, p , m_Configs) {
        const SConfigInfo& cfg_info = *p;
        string unmet, unmet_req;
        // Check config availability
        if ( !project_context.IsConfigEnabled(cfg_info, &unmet, &unmet_req) ) {
            str_log += " " + cfg_info.GetConfigFullName() + "(because of " + unmet + ")";
        } else {
            if (!unmet_req.empty()) {
                ++failed;
                req_log += " " + cfg_info.GetConfigFullName() + "(because of " + unmet_req + ")";
            }
            project_configs.push_back(cfg_info);
        }
    }

    string path = CDirEntry::ConcatPath(prj.m_SourcesBaseDir, "Makefile.");
    path += prj.m_Name;
    switch (prj.m_ProjType) {
    case CProjKey::eApp:
        path += ".app";
        break;
    case CProjKey::eLib:
        path += ".lib";
        break;
    case CProjKey::eDll:
        path += ".dll";
        break;
    default:
        break;
    }
    if (!str_log.empty()) {
        PTB_WARNING_EX(path, ePTB_ConfigurationError,
                       "Disabled configurations: " << str_log);
    }
    if (!req_log.empty()) {
        PTB_WARNING_EX(path, ePTB_ConfigurationError,
                       "Invalid configurations: " << req_log);
    }
    if (project_configs.empty()) {
        PTB_WARNING_EX(path, ePTB_ConfigurationError,
                       "Disabled all configurations for project " << prj.m_Name);
        return;
    }
    if (failed == m_Configs.size()) {
        prj.m_MakeType = eMakeType_ExcludedByReq;
        PTB_WARNING_EX(path, ePTB_ConfigurationError,
                       "All build configurations are invalid, project excluded: " << prj.m_Name);
    }
    
    // Attributes:
    {{
        xmlprj.SetAttlist().SetProjectType (MSVC_PROJECT_PROJECT_TYPE);
        xmlprj.SetAttlist().SetVersion     (GetApp().GetRegSettings().GetProjectFileFormatVersion());
        xmlprj.SetAttlist().SetName        (project_context.ProjectName());
        xmlprj.SetAttlist().SetProjectGUID (prj.m_GUID);
        xmlprj.SetAttlist().SetKeyword     (MSVC_PROJECT_KEYWORD_WIN32);
    }}

    // Platforms
    {{
        CRef<CPlatform> platform(new CPlatform());
        platform->SetAttlist().SetName(CMsvc7RegSettings::GetMsvcPlatformName());
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
            CRef<CTool> tool(new CTool()); 

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
            BIND_TOOLS(tool, msvc_tool.Compiler(), ProgramDataBaseFileName);

            conf->SetTool().push_back(tool);
        }}

        // Linker
        {{
            CRef<CTool> tool(new CTool());

            BIND_TOOLS(tool, msvc_tool.Linker(), Name);
            BIND_TOOLS(tool, msvc_tool.Linker(), AdditionalDependencies);
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
            BIND_TOOLS(tool, msvc_tool.Linker(), LargeAddressAware);
            BIND_TOOLS(tool, msvc_tool.Linker(), FixedBaseAddress);

            conf->SetTool().push_back(tool);
        }}

        // Librarian
        {{
            CRef<CTool> tool(new CTool());

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
            CRef<CTool> tool(new CTool());
            BIND_TOOLS(tool, msvc_tool.CustomBuid(), Name);
            conf->SetTool().push_back(tool);
        }}

        // MIDL
        {{
            CRef<CTool> tool(new CTool());
            BIND_TOOLS(tool, msvc_tool.MIDL(), Name);
            conf->SetTool().push_back(tool);
        }}

        // PostBuildEvent
        {{
            CRef<CTool> tool(new CTool());
            BIND_TOOLS(tool, msvc_tool.PostBuildEvent(), Name);
            conf->SetTool().push_back(tool);
        }}

        // PreBuildEvent
        {{
            CRef<CTool> tool(new CTool());
            BIND_TOOLS(tool, msvc_tool.PreBuildEvent(), Name);
            BIND_TOOLS(tool, msvc_tool.PreBuildEvent(), CommandLine);
            conf->SetTool().push_back(tool);
        }}

        // PreLinkEvent
        {{
            CRef<CTool> tool(new CTool());
            BIND_TOOLS(tool, msvc_tool.PreLinkEvent(), Name);
            conf->SetTool().push_back(tool);
        }}

        // ResourceCompiler
        {{
            CRef<CTool> tool(new CTool());
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
            CRef<CTool> tool(new CTool());
            BIND_TOOLS(tool, msvc_tool.WebServiceProxyGenerator(), Name);
            conf->SetTool().push_back(tool);
        }}

        // XMLDataGenerator
        {{
            CRef<CTool> tool(new CTool());
            BIND_TOOLS(tool, msvc_tool.XMLDataGenerator(), Name);
            conf->SetTool().push_back(tool);
        }}

        // ManagedWrapperGenerator
        {{
            CRef<CTool> tool(new CTool());
            BIND_TOOLS(tool, msvc_tool.ManagedWrapperGenerator(), Name);
            conf->SetTool().push_back(tool);
        }}

        // AuxiliaryManagedWrapperGenerator
        {{
            CRef<CTool> tool(new CTool());
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
    if (!GetApp().m_BuildRoot.empty()) {
        string src_dir = CDirEntry::ConcatPath(context.GetSrcRoot(), 
            GetApp().GetConfig().Get("ProjectTree", "src"));
        if (CDirEntry(src_dir).Exists()) {
            tool_cmd += " -opm \"";
            tool_cmd += src_dir;
            tool_cmd += '"';
        }
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

#endif //NCBI_COMPILER_MSVC


END_NCBI_SCOPE
