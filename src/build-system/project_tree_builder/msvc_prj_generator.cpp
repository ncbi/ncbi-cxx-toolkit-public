#include <ncbi_pch.hpp>
#include "msvc_prj_generator.hpp"
#include "proj_builder_app.hpp"
#include "msvc_project_context.hpp"


#include "msvc_prj_utils.hpp"
#include "msvc_prj_defines.hpp"
#include "ptb_err_codes.hpp"

#if NCBI_COMPILER_MSVC
#   include <build-system/project_tree_builder/msbuild/msbuild_dataobj__.hpp>
#endif //NCBI_COMPILER_MSVC

#include <algorithm>

BEGIN_NCBI_SCOPE

#if NCBI_COMPILER_MSVC

#define __USE_DISABLED_CFGS__ 1

static 
void s_CreateDatatoolCustomBuildInfo(const CProjItem&              prj,
                                     const CMsvcPrjProjectContext& context,
                                     const CDataToolGeneratedSrc&  src,                                   
                                     SCustomBuildInfo*             build_info);

void CreateUtilityProject(const string&            name, 
                          const list<SConfigInfo>& configs, 
                          CVisualStudioProject*    project)
{
    {{
        //Attributes:
        project->SetAttlist().SetProjectType  (MSVC_PROJECT_PROJECT_TYPE);
        project->SetAttlist().SetVersion      (GetApp().GetRegSettings().GetProjectFileFormatVersion());
        project->SetAttlist().SetName         (name);
        project->SetAttlist().SetRootNamespace
            (MSVC_MASTERPROJECT_ROOT_NAMESPACE);
        project->SetAttlist().SetProjectGUID  (GenerateSlnGUID());
        project->SetAttlist().SetKeyword      (MSVC_MASTERPROJECT_KEYWORD);
    }}
    
    {{
        //Platforms
         CRef<CPlatform> platform(new CPlatform());
         platform->SetAttlist().SetName(CMsvc7RegSettings::GetMsvcPlatformName());
         project->SetPlatforms().SetPlatform().push_back(platform);
    }}

    ITERATE(list<SConfigInfo>, p , configs) {
        // Iterate all configurations
        const string& config = (*p).GetConfigFullName();
        
        CRef<CConfiguration> conf(new CConfiguration());

#  define SET_ATTRIBUTE( node, X, val ) node->SetAttlist().Set##X(val)        

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
            SET_ATTRIBUTE(conf, ManagedExtensions,  "true");
        }}

        {{
            //VCCustomBuildTool
            CRef<CTool> tool(new CTool());
            SET_ATTRIBUTE(tool, Name, "VCCustomBuildTool" );
            conf->SetTool().push_back(tool);
        }}
        {{
            //VCMIDLTool
            CRef<CTool> tool(new CTool());
            SET_ATTRIBUTE(tool, Name, "VCMIDLTool" );
            conf->SetTool().push_back(tool);
        }}
        {{
            //VCPostBuildEventTool
            CRef<CTool> tool(new CTool());
            SET_ATTRIBUTE(tool, Name, "VCPostBuildEventTool" );
            conf->SetTool().push_back(tool);
        }}
        {{
            //VCPreBuildEventTool
            CRef<CTool> tool(new CTool());
            SET_ATTRIBUTE(tool, Name, "VCPreBuildEventTool" );
            conf->SetTool().push_back(tool);
        }}

        project->SetConfigurations().SetConfiguration().push_back(conf);
    }

    {{
        //References
        project->SetReferences("");
    }}

    {{
        //Globals
        project->SetGlobals("");
    }}
}

//-----------------------------------------------------------------------------

CMsvcProjectGenerator::CMsvcProjectGenerator(const list<SConfigInfo>& configs)
    :m_Configs(configs)
{
}


CMsvcProjectGenerator::~CMsvcProjectGenerator(void)
{
}


bool CMsvcProjectGenerator::CheckProjectConfigs(
    CMsvcPrjProjectContext& project_context, CProjItem& prj)
{
    m_project_configs.clear();
    string str_log, req_log;
    int failed=0;
    ITERATE(list<SConfigInfo>, p , m_Configs) {
        const SConfigInfo& cfg_info = *p;
        string unmet, unmet_req;
        // Check config availability
        if ( !project_context.IsConfigEnabled(cfg_info, &unmet, &unmet_req) ) {
            str_log += " " + cfg_info.GetConfigFullName() + "(because of " + unmet + ")";
        } else {
            prj.m_CheckConfigs.insert(cfg_info.GetConfigFullName());
            if (!unmet_req.empty()) {
                ++failed;
                req_log += " " + cfg_info.GetConfigFullName() + "(because of " + unmet_req + ")";
            }
            m_project_configs.push_back(cfg_info);
        }
    }

    string path = prj.GetPath();
    if (!str_log.empty()) {
        PTB_WARNING_EX(path, ePTB_ConfigurationError,
                       "Disabled configurations: " << str_log);
    }
    if (!req_log.empty()) {
        PTB_WARNING_EX(path, ePTB_ConfigurationError,
                       "Invalid configurations: " << req_log);
    }
    if (m_project_configs.empty()) {
        PTB_WARNING_EX(path, ePTB_ConfigurationError,
                       "Disabled all configurations for project " << prj.m_Name);
    }
    if (failed == m_Configs.size()) {
        prj.m_MakeType = eMakeType_ExcludedByReq;
        PTB_WARNING_EX(path, ePTB_ConfigurationError,
                       "All build configurations are invalid, project excluded: " << prj.m_Name);
    }
    return !m_project_configs.empty() && failed != m_Configs.size();
}

void CMsvcProjectGenerator::AnalyzePackageExport(
    CMsvcPrjProjectContext& project_context, CProjItem& prj)
{
    m_pkg_export_command.clear();
    m_pkg_export_output.clear();
    m_pkg_export_input.clear();
    if (!prj.m_ExportHeaders.empty()) {
        // destination
        string config_inc = GetApp().m_IncDir;
        config_inc = CDirEntry::ConcatPath(config_inc,
            CDirEntry::ConvertToOSPath( prj.m_ExportHeadersDest));
//        config_inc = CDirEntry::CreateRelativePath(prj.m_SourcesBaseDir, config_inc);
        config_inc = CDirEntry::AddTrailingPathSeparator( config_inc );
        config_inc = CDirEntry::CreateRelativePath(project_context.ProjectDir(), config_inc);

        // source
        string src_dir = 
            CDirEntry::CreateRelativePath(project_context.ProjectDir(), prj.m_SourcesBaseDir);
        src_dir = CDirEntry::AddTrailingPathSeparator( src_dir );
        
        string command, output, input, file, file_in, file_out;
        command = "@if not exist \"" + config_inc + "\" mkdir \"" + config_inc + "\"\n";
        ITERATE (list<string>, f, prj.m_ExportHeaders) {
            file = CDirEntry::ConvertToOSPath(*f);
            file_in = "\"" + src_dir + file + "\"";
            file_out = "\"" + config_inc + file + "\"";
            command += "xcopy /Y /D /F " + file_in + " \"" + config_inc + "\"\n";
            output += file_out + ";";
            input  += file_in  + ";";
        }
        m_pkg_export_command = command;
        m_pkg_export_output  = output;
        m_pkg_export_input   = input;
    }
}

void CMsvcProjectGenerator::GetDefaultPch(
    CMsvcPrjProjectContext& project_context, CProjItem& prj)
{
    SConfigInfo cfg_tmp;
    m_pch_default.clear();
    m_pch_define.clear();
    if ( project_context.IsPchEnabled(cfg_tmp) ) {
        string noname = CDirEntry::ConcatPath(prj.m_SourcesBaseDir,"aanofile");
        m_pch_default = project_context.GetPchHeader(
            prj.m_ID, noname, GetApp().GetProjectTreeInfo().m_Src, cfg_tmp);
        m_pch_define = GetApp().GetMetaMakefile().GetPchUsageDefine();
        m_pch_define += ";%(PreprocessorDefinitions)";
    }
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
    if (!CheckProjectConfigs(project_context, prj)) {
        return;
    }
    AnalyzePackageExport(project_context, prj);
    GetDefaultPch(project_context, prj);
    // Collect all source, header, inline, resource files
    CMsvcPrjFilesCollector collector(project_context, m_project_configs, prj);

    if (CMsvc7RegSettings::GetMsvcVersion() >= CMsvc7RegSettings::eMsvc1000) {
        GenerateMsbuild(collector, project_context, prj);
        GetApp().RegisterProjectWatcher(
            project_context.ProjectName(), prj.m_SourcesBaseDir, prj.m_Watchers);
        return;
    }

    CVisualStudioProject xmlprj;
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

#if __USE_DISABLED_CFGS__
    const list<SConfigInfo>& all_cfgs = GetApp().GetRegSettings().m_ConfigInfo;
#else
    const list<SConfigInfo>& all_cfgs = m_project_configs;
#endif
    ITERATE(list<SConfigInfo>, p , all_cfgs) {

        const SConfigInfo& cfg_info = *p;
        bool disabled_cfg = (m_project_configs.size() != all_cfgs.size() &&
            find(m_project_configs.begin(), m_project_configs.end(),cfg_info) ==
                 m_project_configs.end());
 
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
            BIND_TOOLS(conf, msvc_tool.Configuration(), BuildLogFile);
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
#if 0
            BIND_TOOLS(tool, msvc_tool.Compiler(), UsePrecompiledHeader);
#else
// set default
            if (!m_pch_default.empty()) {
                if (CMsvc7RegSettings::GetMsvcVersion() >= CMsvc7RegSettings::eMsvc800) {
                    tool->SetAttlist().SetUsePrecompiledHeader("2");
                } else {
                    tool->SetAttlist().SetUsePrecompiledHeader("3");
                }
                tool->SetAttlist().SetPrecompiledHeaderThrough(m_pch_default);
            }
#endif
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
#if __USE_DISABLED_CFGS__
            if (disabled_cfg) {
                tool->SetAttlist().SetGenerateManifest("false");
            } else {
                BIND_TOOLS(tool, msvc_tool.Linker(), GenerateManifest);
            }
#else
            BIND_TOOLS(tool, msvc_tool.Linker(), GenerateManifest);
#endif
            tool->SetAttlist().SetManifestFile("$(TargetPath).manifest");

            conf->SetTool().push_back(tool);
        }}
        
        // ManifestTool
        {{
            CRef<CTool> tool(new CTool());
            tool->SetAttlist().SetName("VCManifestTool");
            tool->SetAttlist().SetEmbedManifest(msvc_tool.Linker()->EmbedManifest());
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
#if 0
// this seems relevant place, but it does not work on MSVC 2005
// so we put it into PostBuildEvent
            if (!m_pkg_export_command.empty()) {
                tool->SetAttlist().SetCommandLine(m_pkg_export_command);
                tool->SetAttlist().SetOutputs(m_pkg_export_output);
//                tool->SetAttlist().SetAdditionalDependencies(m_pkg_export_input);
            }
#endif
#if __USE_DISABLED_CFGS__
            if (disabled_cfg) {
                tool->SetAttlist().SetCommandLine("@echo DISABLED configuration\n");
                tool->SetAttlist().SetOutputs("aanofile");
            }
#endif
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
#if 0
// This is workaround:
// if EXE is newer than its manifest, MT.EXE keeps "re-generating" the manifest.
// In fact, MT.EXE does nothing; but what is worse, it does not update the time stamp of the manifest
            if (!disabled_cfg && (prj.m_ProjType == CProjKey::eApp || prj.m_ProjType == CProjKey::eDll)) {
                if (NStr::CompareNocase(msvc_tool.Linker()->GenerateManifest(),"true") == 0 &&
                    NStr::CompareNocase(msvc_tool.Linker()->EmbedManifest(),"false") == 0) {
                    m_pkg_export_command += "copy /b /y \"$(IntDir)\\$(TargetFileName).intermediate.manifest\" \"$(TargetPath).manifest\"\n";
                }
            }
#endif
            if (!m_pkg_export_command.empty()) {
                tool->SetAttlist().SetCommandLine( m_pkg_export_command);
            }
            conf->SetTool().push_back(tool);
        }}

        // PreBuildEvent
        {{
            CRef<CTool> tool(new CTool());
            BIND_TOOLS(tool, msvc_tool.PreBuildEvent(), Name);

#if 0
            if (disabled_cfg) {
                string cmd_line("@echo DISABLED configuration\n");
                tool->SetAttlist().SetCommandLine(cmd_line);
            } else {
                BIND_TOOLS(tool, msvc_tool.PreBuildEvent(), CommandLine);
            }
#else
            BIND_TOOLS(tool, msvc_tool.PreBuildEvent(), CommandLine);
#endif

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
    
    // Insert sources, headers, inlines:
    auto_ptr<IFilesToProjectInserter> inserter;

    if (prj.m_ProjType == CProjKey::eDll) {
        inserter.reset(new CDllProjectFilesInserter
                                    (&xmlprj,
                                     CProjKey(prj.m_ProjType, prj.m_ID), 
                                     m_project_configs, 
                                     project_context.ProjectDir()));

    } else {
        inserter.reset(new CBasicProjectsFilesInserter 
                                     (&xmlprj,
                                      prj.m_ID,
                                      m_project_configs, 
                                      project_context.ProjectDir()));
    }

    ITERATE(list<string>, p, collector.SourceFiles()) {
        //Include collected source files
        const string& rel_source_file = *p;
        inserter->AddSourceFile(rel_source_file, m_pch_default);
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
        if (!collector.ResourceFiles().empty()) {
            //Resource Files - header files - empty
            CRef<CFilter> filter(new CFilter());
            filter->SetAttlist().SetName("Resource Files");
            filter->SetAttlist().SetFilter
                ("rc;ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe;resx");

            ITERATE(list<string>, p, collector.ResourceFiles()) {
                //Include collected header files
                CRef<CFFile> file(new CFFile());
                file->SetAttlist().SetRelativePath(*p);
// exclude from disabled configurations
#if __USE_DISABLED_CFGS__
                if (m_project_configs.size() != all_cfgs.size()) {
                    ITERATE(list<SConfigInfo>, p , all_cfgs) {
                        if (find(m_project_configs.begin(), m_project_configs.end(),*p) ==
                                m_project_configs.end()) {
                            const string& config = (*p).GetConfigFullName();
                            CRef<CFileConfiguration> file_config(new CFileConfiguration());
                            file_config->SetAttlist().SetName(ConfigName(config));
                            file_config->SetAttlist().SetExcludedFromBuild("true");

                            CRef<CTool> rescl_tool(new CTool());
                            rescl_tool->SetAttlist().SetName("VCResourceCompilerTool");

                            file_config->SetTool(*rescl_tool);
                            file->SetFileConfiguration().push_back(file_config);
                        }
                    }
                }
#endif
                CRef< CFilter_Base::C_FF::C_E > ce(new CFilter_Base::C_FF::C_E());
                ce->SetFile(*file);
                filter->SetFF().SetFF().push_back(ce);
            }

            xmlprj.SetFiles().SetFilter().push_back(filter);
        }
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
                                           m_project_configs, 
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
                                           m_project_configs, 
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

    GetApp().RegisterProjectWatcher(
        project_context.ProjectName(), prj.m_SourcesBaseDir, prj.m_Watchers);

    string project_path = CDirEntry::ConcatPath(project_context.ProjectDir(), 
                                                project_context.ProjectName());
    project_path += CMsvc7RegSettings::GetVcprojExt();

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

    CMsvc7RegSettings::EMsvcVersion eVer = CMsvc7RegSettings::GetMsvcVersion();
    if (eVer > CMsvc7RegSettings::eMsvc710 &&
        eVer < CMsvc7RegSettings::eXCode30) {
        //CommandLine
        string build_root = CDirEntry::ConcatPath(
                GetApp().GetProjectTreeInfo().m_Compilers,
                GetApp().GetRegSettings().m_CompilersSubdir);

        string output_dir = CDirEntry::ConcatPath(
                build_root,
                GetApp().GetBuildType().GetTypeStr());
    //    output_dir = CDirEntry::ConcatPath(output_dir, "bin\\$(ConfigurationName)");
        output_dir = CDirEntry::ConcatPath(output_dir, "..\\static\\bin\\ReleaseDLL");

        string dt_path = "$(ProjectDir)" + CDirEntry::DeleteTrailingPathSeparator(
            CDirEntry::CreateRelativePath(context.ProjectDir(), output_dir));

        string tree_root = "$(ProjectDir)" + CDirEntry::DeleteTrailingPathSeparator(
            CDirEntry::CreateRelativePath(context.ProjectDir(),
                GetApp().GetProjectTreeInfo().m_Root));

        build_root = "$(ProjectDir)" + CDirEntry::DeleteTrailingPathSeparator(
                CDirEntry::CreateRelativePath(context.ProjectDir(), build_root));

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

        build_info->m_CommandLine  =  "set DATATOOL_PATH=" + dt_path + "\n";
        build_info->m_CommandLine +=  "set TREE_ROOT=" + tree_root + "\n";
        build_info->m_CommandLine +=  "set PTB_PLATFORM=$(PlatformName)\n";
        build_info->m_CommandLine +=  "set BUILD_TREE_ROOT=" + build_root + "\n";
        build_info->m_CommandLine +=  "call \"%BUILD_TREE_ROOT%\\datatool.bat\" " + tool_cmd + "\n";
        build_info->m_CommandLine +=  "if errorlevel 1 exit 1";
        string tool_exe_location("\"");
        tool_exe_location += dt_path + "datatool.exe" + "\"";

        //Description
        build_info->m_Description = 
            "Using datatool to generate C++ classes from ASN/DTD/Schema $(InputPath)";

        //Outputs
        build_info->m_Outputs = "$(InputDir)$(InputName).files;$(InputDir)$(InputName)__.cpp;$(InputDir)$(InputName)___.cpp";

        //Additional Dependencies
        build_info->m_AdditionalDependencies = "$(InputDir)$(InputName).def;";
    } else {
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
            "@echo on\n" + tool_exe_location + " " + tool_cmd + "\n@echo off";
        //Description
        build_info->m_Description = 
            "Using datatool to generate C++ classes from ASN/DTD $(InputPath)";

        //Outputs
        build_info->m_Outputs = "$(InputDir)$(InputName).files;$(InputDir)$(InputName)__.cpp;$(InputDir)$(InputName)___.cpp";

        //Additional Dependencies
        build_info->m_AdditionalDependencies = "$(InputDir)$(InputName).def;" + tool_exe_location;
    }
}


template<typename Container>
void __SET_PROPGROUP_ELEMENT(
    Container& container, const string& name, const string& value,
    const string& condition = kEmptyStr)
{
    CRef<msbuild::CPropertyGroup::C_E> e(new msbuild::CPropertyGroup::C_E);
    e->SetAnyContent().SetName(name);
    e->SetAnyContent().SetValue(CUtf8::AsUTF8(value,eEncoding_UTF8));
    if (!condition.empty()) {
       e->SetAnyContent().AddAttribute("Condition", kEmptyStr, CUtf8::AsUTF8(condition,eEncoding_UTF8));
    }
    container->SetPropertyGroup().SetPropertyGroup().push_back(e);
}

template<typename Container>
void __SET_CLCOMPILE_ELEMENT(
    Container& container, const string& name, const string& value,
    const string& condition = kEmptyStr)
{
    CRef<msbuild::CClCompile::C_E> e(new msbuild::CClCompile::C_E);
    e->SetAnyContent().SetName(name);
    e->SetAnyContent().SetValue(CUtf8::AsUTF8(value,eEncoding_UTF8));
    if (!condition.empty()) {
       e->SetAnyContent().AddAttribute("Condition", kEmptyStr, CUtf8::AsUTF8(condition,eEncoding_UTF8));
    }
    container->SetClCompile().SetClCompile().push_back(e);
}
#define __SET_CLCOMPILE(container, name) \
    __SET_CLCOMPILE_ELEMENT(container, #name,  msvc_tool.Compiler()->name())


template<typename Container>
void __SET_LIB_ELEMENT(
    Container& container, const string& name, const string& value)
{
    CRef<msbuild::CLib::C_E> e(new msbuild::CLib::C_E);
    e->SetAnyContent().SetName(name);
    e->SetAnyContent().SetValue(CUtf8::AsUTF8(value,eEncoding_UTF8));
    container->SetLib().Set().push_back(e);
}

template<typename Container>
void __SET_LINK_ELEMENT(
    Container& container, const string& name, const string& value)
{
    CRef<msbuild::CLink::C_E> e(new msbuild::CLink::C_E);
    e->SetAnyContent().SetName(name);
    e->SetAnyContent().SetValue(CUtf8::AsUTF8(value,eEncoding_UTF8));
    container->SetLink().SetLink().push_back(e);
}
#define __SET_LINK(container, name) \
    __SET_LINK_ELEMENT(container, #name,  msvc_tool.Linker()->name())

template<typename Container>
void __SET_RC_ELEMENT(
    Container& container, const string& name, const string& value,
    const string& condition = kEmptyStr)
{
    CRef<msbuild::CResourceCompile::C_E> e(new msbuild::CResourceCompile::C_E);
    e->SetAnyContent().SetName(name);
    e->SetAnyContent().SetValue(CUtf8::AsUTF8(value,eEncoding_UTF8));
    if (!condition.empty()) {
       e->SetAnyContent().AddAttribute("Condition", kEmptyStr, CUtf8::AsUTF8(condition,eEncoding_UTF8));
    }
    container->SetResourceCompile().SetResourceCompile().push_back(e);
}

template<typename Container>
void __SET_NONE_ELEMENT(
    Container& container, const string& name, const string& value,
    const string& condition = kEmptyStr)
{
    CRef<msbuild::CNone::C_E> e(new msbuild::CNone::C_E);
    e->SetAnyContent().SetName(name);
    e->SetAnyContent().SetValue(CUtf8::AsUTF8(value,eEncoding_UTF8));
    if (!condition.empty()) {
       e->SetAnyContent().AddAttribute("Condition", kEmptyStr, CUtf8::AsUTF8(condition,eEncoding_UTF8));
    }
    container->SetNone().SetNone().push_back(e);
}

template<typename Container>
void __SET_CUSTOMBUILD_ELEMENT(
    Container& container, const string& name, const string& value,
    const string& condition = kEmptyStr)
{
    CRef<msbuild::CCustomBuild::C_E> e(new msbuild::CCustomBuild::C_E);
    e->SetAnyContent().SetName(name);
    e->SetAnyContent().SetValue(CUtf8::AsUTF8(value,eEncoding_UTF8));
    if (!condition.empty()) {
       e->SetAnyContent().AddAttribute("Condition", kEmptyStr, CUtf8::AsUTF8(condition,eEncoding_UTF8));
    }
    container->SetCustomBuild().SetCustomBuild().push_back(e);
}




void CMsvcProjectGenerator::GenerateMsbuild(
    CMsvcPrjFilesCollector& collector,
    CMsvcPrjProjectContext& project_context, CProjItem& prj)
{
    msbuild::CProject project;
    project.SetAttlist().SetDefaultTargets("Build");
    project.SetAttlist().SetToolsVersion("4.0");
#if __USE_DISABLED_CFGS__
    const list<SConfigInfo>& all_cfgs = GetApp().GetRegSettings().m_ConfigInfo;
#else
    const list<SConfigInfo>& all_cfgs = m_project_configs;
#endif

// ProjectLevelTagExceptTargetOrImportType
    {
        // project GUID
        CRef<msbuild::CProject::C_ProjectLevelTagExceptTargetOrImportType::C_E> t(new msbuild::CProject::C_ProjectLevelTagExceptTargetOrImportType::C_E);
        t->SetPropertyGroup().SetAttlist().SetLabel("Globals");
        project.SetProjectLevelTagExceptTargetOrImportType().SetProjectLevelTagExceptTargetOrImportType().push_back(t);
        __SET_PROPGROUP_ELEMENT( t, "ProjectGuid", prj.m_GUID);
        __SET_PROPGROUP_ELEMENT( t, "Keyword",     MSVC_PROJECT_KEYWORD_WIN32);
        if (prj.m_ProjType == CProjKey::eUtility) {
            __SET_PROPGROUP_ELEMENT( t, "ProjectName", prj.m_Name);
        } else {
            __SET_PROPGROUP_ELEMENT( t, "ProjectName", project_context.ProjectName());
        }
    }
    {
        // project configurations
        CRef<msbuild::CProject::C_ProjectLevelTagExceptTargetOrImportType::C_E> t(new msbuild::CProject::C_ProjectLevelTagExceptTargetOrImportType::C_E);
        t->SetItemGroup().SetAttlist().SetLabel("ProjectConfigurations");
        ITERATE(list<SConfigInfo>, c , all_cfgs) {
            string cfg_name(c->GetConfigFullName());
            string cfg_platform(CMsvc7RegSettings::GetMsvcPlatformName());
            {
                CRef<msbuild::CItemGroup::C_E> p(new msbuild::CItemGroup::C_E);
                p->SetProjectConfiguration().SetAttlist().SetInclude(cfg_name + "|" + cfg_platform);
                p->SetProjectConfiguration().SetConfiguration(cfg_name);
                p->SetProjectConfiguration().SetPlatform(cfg_platform);
                t->SetItemGroup().SetItemGroup().push_back(p);
            }
        }
        project.SetProjectLevelTagExceptTargetOrImportType().SetProjectLevelTagExceptTargetOrImportType().push_back(t);
    }

// TargetOrImportType
    project.SetTargetOrImportType().SetImport().SetAttlist().SetProject("$(VCTargetsPath)\\Microsoft.Cpp.Default.props");
    
// ProjectLevelTagType
    // Configurations
    {
        ITERATE(list<SConfigInfo>, c , all_cfgs) {

            const SConfigInfo& cfg_info = *c;
            CMsvcPrjGeneralContext general_context(cfg_info, project_context);
            CMsvcTools msvc_tool(general_context, project_context);
            string cfg_condition("'$(Configuration)|$(Platform)'=='");
            cfg_condition += c->GetConfigFullName() + "|" + CMsvc7RegSettings::GetMsvcPlatformName() + "'";

            CRef<msbuild::CProject::C_ProjectLevelTagType::C_E> t(new msbuild::CProject::C_ProjectLevelTagType::C_E);
            t->SetPropertyGroup().SetAttlist().SetCondition(cfg_condition);
            t->SetPropertyGroup().SetAttlist().SetLabel("Configuration");
            project.SetProjectLevelTagType().SetProjectLevelTagType().push_back(t);

            __SET_PROPGROUP_ELEMENT(t, "ConfigurationType", msvc_tool.Configuration()->ConfigurationType());
            __SET_PROPGROUP_ELEMENT(t, "CharacterSet",      msvc_tool.Configuration()->CharacterSet());
            if (CMsvc7RegSettings::GetMsvcVersion() >= CMsvc7RegSettings::eMsvc1100) {
                __SET_PROPGROUP_ELEMENT(t, "PlatformToolset",      msvc_tool.Configuration()->PlatformToolset());
            }
        }
    }

    // -----
    {
        CRef<msbuild::CProject::C_ProjectLevelTagType::C_E> t(new msbuild::CProject::C_ProjectLevelTagType::C_E);
        t->SetImport().SetAttlist().SetProject("$(VCTargetsPath)\\Microsoft.Cpp.props");
        project.SetProjectLevelTagType().SetProjectLevelTagType().push_back(t);
    }
    {
        CRef<msbuild::CProject::C_ProjectLevelTagType::C_E> t(new msbuild::CProject::C_ProjectLevelTagType::C_E);
        t->SetImportGroup().SetAttlist().SetLabel("ExtensionSettings");
        project.SetProjectLevelTagType().SetProjectLevelTagType().push_back(t);
    }
    // PropertySheets
    {
        ITERATE(list<SConfigInfo>, c , all_cfgs) {
            string cfg_condition("'$(Configuration)|$(Platform)'=='");
            cfg_condition += c->GetConfigFullName() + "|" + CMsvc7RegSettings::GetMsvcPlatformName() + "'";

            CRef<msbuild::CProject::C_ProjectLevelTagType::C_E> t(new msbuild::CProject::C_ProjectLevelTagType::C_E);
            t->SetImportGroup().SetAttlist().SetCondition(cfg_condition);
            t->SetImportGroup().SetAttlist().SetLabel("PropertySheets");
            {
                CRef<msbuild::CImportGroup::C_E> p(new msbuild::CImportGroup::C_E);
                p->SetImport().SetAttlist().SetProject("$(UserRootDir)\\Microsoft.Cpp.$(Platform).user.props");
                p->SetImport().SetAttlist().SetCondition("exists('$(UserRootDir)\\Microsoft.Cpp.$(Platform).user.props')");
                p->SetImport().SetAttlist().SetLabel("LocalAppDataPlatform");
                t->SetImportGroup().SetImportGroup().push_back(p);
            }
            project.SetProjectLevelTagType().SetProjectLevelTagType().push_back(t);
        }
    }
    
    // UserMacros
    {
        CRef<msbuild::CProject::C_ProjectLevelTagType::C_E> t(new msbuild::CProject::C_ProjectLevelTagType::C_E);
        t->SetPropertyGroup().SetAttlist().SetLabel("UserMacros");
        project.SetProjectLevelTagType().SetProjectLevelTagType().push_back(t);
    }
    
    bool customtargetname =  false;
    {
        // File version
        CRef<msbuild::CProject::C_ProjectLevelTagType::C_E> t(new msbuild::CProject::C_ProjectLevelTagType::C_E);
        project.SetProjectLevelTagType().SetProjectLevelTagType().push_back(t);
        __SET_PROPGROUP_ELEMENT( t, "_ProjectFileVersion", GetApp().GetRegSettings().GetProjectFileFormatVersion());

        // OutDir/IntDir/TargetName
        ITERATE(list<SConfigInfo>, c , all_cfgs) {

            const SConfigInfo& cfg_info = *c;
            CMsvcPrjGeneralContext general_context(cfg_info, project_context);
            CMsvcTools msvc_tool(general_context, project_context);
            string cfg_condition("'$(Configuration)|$(Platform)'=='");
            cfg_condition += c->GetConfigFullName() + "|" + CMsvc7RegSettings::GetMsvcPlatformName() + "'";

            string outputfile(msvc_tool.Linker()->OutputFile());
            string targetdir, targetname, targetext;
            if (!outputfile.empty()) {
                CDirEntry out(outputfile);
                targetname = out.GetBase();
                customtargetname =  !targetname.empty() && (targetname.find('$') == string::npos);
                if (customtargetname) {
                    targetext  = out.GetExt();
                    targetdir = NStr::Replace(out.GetDir(), "/", "\\");
                    NStr::ReplaceInPlace(targetdir,"$(OutDir)\\", msvc_tool.Configuration()->OutputDirectory());
                }
            }

            __SET_PROPGROUP_ELEMENT(t, "OutDir",          customtargetname ? targetdir : msvc_tool.Configuration()->OutputDirectory(), cfg_condition);
            __SET_PROPGROUP_ELEMENT(t, "IntDir",          msvc_tool.Configuration()->IntermediateDirectory(), cfg_condition);
            __SET_PROPGROUP_ELEMENT(t, "TargetName",      customtargetname ? targetname : project_context.ProjectId(), cfg_condition);
            __SET_PROPGROUP_ELEMENT(t, "LinkIncremental", msvc_tool.Linker()->LinkIncremental(), cfg_condition);
            string prop = msvc_tool.Linker()->GenerateManifest();
#if __USE_DISABLED_CFGS__
            if (all_cfgs.size() != m_project_configs.size() &&
                find(m_project_configs.begin(), m_project_configs.end(), *c) == m_project_configs.end()) {
                prop = "false";
            }
#endif
            __SET_PROPGROUP_ELEMENT(t, "GenerateManifest", prop, cfg_condition);
            __SET_PROPGROUP_ELEMENT(t, "EmbedManifest", msvc_tool.Linker()->EmbedManifest(), cfg_condition);
            if (customtargetname) {
                __SET_PROPGROUP_ELEMENT(t, "TargetExt", targetext, cfg_condition);
            }
        }
    }

    // compilation settings
    ITERATE(list<SConfigInfo>, c , all_cfgs) {

        const SConfigInfo& cfg_info = *c;
        CMsvcPrjGeneralContext general_context(cfg_info, project_context);
        CMsvcTools msvc_tool(general_context, project_context);
        string cfg_condition("'$(Configuration)|$(Platform)'=='");
        cfg_condition += c->GetConfigFullName() + "|" + CMsvc7RegSettings::GetMsvcPlatformName() + "'";

        {
            CRef<msbuild::CProject::C_ProjectLevelTagType::C_E> t(new msbuild::CProject::C_ProjectLevelTagType::C_E);
            t->SetItemDefinitionGroup().SetAttlist().SetCondition(cfg_condition);
            project.SetProjectLevelTagType().SetProjectLevelTagType().push_back(t);
            // PreBuild event
            {
                string cmd(msvc_tool.PreBuildEvent()->CommandLine());
#if __USE_DISABLED_CFGS__
                if (all_cfgs.size() != m_project_configs.size() &&
                    find(m_project_configs.begin(), m_project_configs.end(), *c) == m_project_configs.end()) {
                    cmd = "@echo DISABLED configuration\n";
                }
#endif
                if (!cmd.empty()) {
                    CRef<msbuild::CItemDefinitionGroup::C_E> p(new msbuild::CItemDefinitionGroup::C_E);
                    t->SetItemDefinitionGroup().SetItemDefinitionGroup().push_back(p);
                    CRef<msbuild::CPreBuildEvent::C_E> e(new msbuild::CPreBuildEvent::C_E);
                    p->SetPreBuildEvent().SetPreBuildEvent().push_back(e);
                    e->SetCommand(CMsvcMetaMakefile::TranslateCommand(cmd));
                }
            }
            // Midl
            if (CMsvc7RegSettings::GetMsvcPlatform() == CMsvc7RegSettings::eMsvcX64) {
                CRef<msbuild::CItemDefinitionGroup::C_E> p(new msbuild::CItemDefinitionGroup::C_E);
                t->SetItemDefinitionGroup().SetItemDefinitionGroup().push_back(p);
                CRef<msbuild::CMidl::C_E> e(new msbuild::CMidl::C_E);
                p->SetMidl().SetMidl().push_back(e);
                e->SetTargetEnvironment("X64");
            }
            // compiler
            {
                CRef<msbuild::CItemDefinitionGroup::C_E> p(new msbuild::CItemDefinitionGroup::C_E);
                t->SetItemDefinitionGroup().SetItemDefinitionGroup().push_back(p);

                __SET_CLCOMPILE(p, AdditionalIncludeDirectories);
                __SET_CLCOMPILE(p, AdditionalOptions);
                __SET_CLCOMPILE(p, BasicRuntimeChecks);
                __SET_CLCOMPILE(p, BrowseInformation);
                __SET_CLCOMPILE(p, BufferSecurityCheck);
                __SET_CLCOMPILE(p, CallingConvention);
                __SET_CLCOMPILE(p, CompileAs);
                __SET_CLCOMPILE(p, DebugInformationFormat);
                __SET_CLCOMPILE(p, DisableSpecificWarnings);
                __SET_CLCOMPILE(p, EnableFunctionLevelLinking);
                __SET_CLCOMPILE(p, FavorSizeOrSpeed);
                __SET_CLCOMPILE(p, IgnoreStandardIncludePath);
                __SET_CLCOMPILE(p, InlineFunctionExpansion);
                __SET_CLCOMPILE(p, MinimalRebuild);
                __SET_CLCOMPILE(p, OmitFramePointers);
                __SET_CLCOMPILE(p, Optimization);
                if (m_pch_default.empty()) {
                    __SET_CLCOMPILE_ELEMENT(p, "PrecompiledHeader", "NotUsing");
                } else {
                    __SET_CLCOMPILE_ELEMENT(p, "PrecompiledHeader", "Use");
                }
                __SET_CLCOMPILE_ELEMENT(p, "PrecompiledHeaderFile", m_pch_default);
                __SET_CLCOMPILE(p, PreprocessorDefinitions);
                __SET_CLCOMPILE(p, ProgramDataBaseFileName);
                __SET_CLCOMPILE(p, RuntimeLibrary);
                __SET_CLCOMPILE(p, RuntimeTypeInfo);
                __SET_CLCOMPILE(p, StringPooling);
                __SET_CLCOMPILE(p, StructMemberAlignment);
                __SET_CLCOMPILE(p, UndefinePreprocessorDefinitions);
                __SET_CLCOMPILE(p, WarningLevel);
            }
            // linker
            {
                CRef<msbuild::CItemDefinitionGroup::C_E> p(new msbuild::CItemDefinitionGroup::C_E);
                t->SetItemDefinitionGroup().SetItemDefinitionGroup().push_back(p);

                __SET_LINK(p, AdditionalDependencies);
                __SET_LINK(p, AdditionalLibraryDirectories);
                __SET_LINK(p, AdditionalOptions);
                __SET_LINK(p, EnableCOMDATFolding);
                __SET_LINK(p, FixedBaseAddress);
                __SET_LINK(p, GenerateDebugInformation);
                __SET_LINK(p, ImportLibrary);
                __SET_LINK(p, IgnoreAllDefaultLibraries);
                __SET_LINK(p, IgnoreDefaultLibraryNames);
                __SET_LINK(p, LargeAddressAware);
                __SET_LINK(p, OptimizeReferences);
#if 0
                if (!customtargetname) {
                    __SET_LINK(p, OutputFile);
                    __SET_LINK(p, ProgramDatabaseFile);
                }
                else {
                    __SET_LINK_ELEMENT(p, "OutputFile",          "$(OutDir)$(TargetName)$(TargetExt)");
                    __SET_LINK_ELEMENT(p, "ProgramDatabaseFile", "$(OutDir)$(TargetName).pdb");
                }
#endif
                __SET_LINK(p, SubSystem);
                __SET_LINK(p, TargetMachine);
                if (!msvc_tool.Linker()->ImageHasSafeExceptionHandlers().empty()) {
                    __SET_LINK(p, ImageHasSafeExceptionHandlers);
                }
            }
            // librarian
            {
                CRef<msbuild::CItemDefinitionGroup::C_E> p(new msbuild::CItemDefinitionGroup::C_E);
                t->SetItemDefinitionGroup().SetItemDefinitionGroup().push_back(p);
                __SET_LIB_ELEMENT(p, "AdditionalLibraryDirectories",   msvc_tool.Librarian()->AdditionalLibraryDirectories());
                __SET_LIB_ELEMENT(p, "AdditionalOptions",              msvc_tool.Librarian()->AdditionalOptions());
                __SET_LIB_ELEMENT(p, "IgnoreAllDefaultLibraries",      msvc_tool.Librarian()->IgnoreAllDefaultLibraries());
                __SET_LIB_ELEMENT(p, "IgnoreSpecificDefaultLibraries", msvc_tool.Librarian()->IgnoreDefaultLibraryNames());
                __SET_LIB_ELEMENT(p, "OutputFile",                     msvc_tool.Librarian()->OutputFile());
                __SET_LIB_ELEMENT(p, "TargetMachine",                  msvc_tool.Librarian()->TargetMachine());
            }
            // resource compiler
            {
                CRef<msbuild::CItemDefinitionGroup::C_E> p(new msbuild::CItemDefinitionGroup::C_E);
                t->SetItemDefinitionGroup().SetItemDefinitionGroup().push_back(p);
                __SET_RC_ELEMENT(p, "AdditionalIncludeDirectories", msvc_tool.ResourceCompiler()->AdditionalIncludeDirectories());
                __SET_RC_ELEMENT(p, "AdditionalOptions",            msvc_tool.ResourceCompiler()->AdditionalOptions());
                __SET_RC_ELEMENT(p, "PreprocessorDefinitions",      msvc_tool.ResourceCompiler()->PreprocessorDefinitions());
            }
            // PostBuild event
            {
                string cmd(m_pkg_export_command);
                if (!cmd.empty()) {
                    CRef<msbuild::CItemDefinitionGroup::C_E> p(new msbuild::CItemDefinitionGroup::C_E);
                    t->SetItemDefinitionGroup().SetItemDefinitionGroup().push_back(p);
                    CRef<msbuild::CPostBuildEvent::C_E> e(new msbuild::CPostBuildEvent::C_E);
                    p->SetPostBuildEvent().SetPostBuildEvent().push_back(e);
                    e->SetCommand(CMsvcMetaMakefile::TranslateCommand(cmd));
                }
            }
        }
    }
    
    // sources
    if (!collector.SourceFiles().empty()) {
        CRef<msbuild::CProject::C_ProjectLevelTagType::C_E> t(new msbuild::CProject::C_ProjectLevelTagType::C_E);
        project.SetProjectLevelTagType().SetProjectLevelTagType().push_back(t);

        bool first = true;
        ITERATE(list<string>, f, collector.SourceFiles()) {
            const string& rel_source_file = *f;
            bool pch_use = !m_pch_default.empty() && NStr::CompareNocase(CDirEntry(rel_source_file).GetExt(),".c") != 0;
            if ( NStr::Find(rel_source_file, ".@config@") == NPOS ) {
                CRef<msbuild::CItemGroup::C_E> p(new msbuild::CItemGroup::C_E);
                t->SetItemGroup().SetItemGroup().push_back(p);
                p->SetClCompile().SetAttlist().SetInclude(rel_source_file);
                ITERATE(list<SConfigInfo>, c , all_cfgs) {
                    string cfg_condition("'$(Configuration)|$(Platform)'=='");
                    cfg_condition += c->GetConfigFullName() + "|" + CMsvc7RegSettings::GetMsvcPlatformName() + "'";
#if __USE_DISABLED_CFGS__
                    if (all_cfgs.size() != m_project_configs.size() &&
                        find(m_project_configs.begin(), m_project_configs.end(), *c) == m_project_configs.end()) {
                        __SET_CLCOMPILE_ELEMENT(p, "ExcludedFromBuild", "true", cfg_condition);
                    }
#endif
                    if (pch_use) {
                        if (!m_pch_define.empty()) {
                            __SET_CLCOMPILE_ELEMENT(p, "PreprocessorDefinitions", m_pch_define, cfg_condition);
                        }
                        if (first) {
                            __SET_CLCOMPILE_ELEMENT(p, "PrecompiledHeader", "Create", cfg_condition);
                        }
                    } else {
                        __SET_CLCOMPILE_ELEMENT(p, "PrecompiledHeader", "NotUsing", cfg_condition);
                    }
                }
                if (pch_use) {
                    first = false;
                }
            } else {
                ITERATE(list<SConfigInfo>, c, all_cfgs) {
                    const string& cfg_name = c->GetConfigFullName();
                    string cfg_file = NStr::Replace(rel_source_file, ".@config@", "." + cfg_name);

                    CRef<msbuild::CItemGroup::C_E> p(new msbuild::CItemGroup::C_E);
                    t->SetItemGroup().SetItemGroup().push_back(p);
                    p->SetClCompile().SetAttlist().SetInclude(cfg_file);

                    ITERATE(list<SConfigInfo>, c2 , all_cfgs) {
                        const string& cfg2_name = c2->GetConfigFullName();
                        string cfg_condition("'$(Configuration)|$(Platform)'=='");
                        cfg_condition += cfg2_name + "|" + CMsvc7RegSettings::GetMsvcPlatformName() + "'";
#if __USE_DISABLED_CFGS__
                        if (all_cfgs.size() != m_project_configs.size() &&
                            find(m_project_configs.begin(), m_project_configs.end(), *c) == m_project_configs.end()) {
                            __SET_CLCOMPILE_ELEMENT(p, "ExcludedFromBuild", "true", cfg_condition);
                        } else
#endif
                        if (cfg2_name != cfg_name) {
                            __SET_CLCOMPILE_ELEMENT(p, "ExcludedFromBuild", "true", cfg_condition);
                        }
                        __SET_CLCOMPILE_ELEMENT(p, "PrecompiledHeader", "NotUsing", cfg_condition);
                    }
                }
            }
        }
    }
    // headers
    if (!collector.HeaderFiles().empty()) {
        CRef<msbuild::CProject::C_ProjectLevelTagType::C_E> t(new msbuild::CProject::C_ProjectLevelTagType::C_E);
        project.SetProjectLevelTagType().SetProjectLevelTagType().push_back(t);
        ITERATE(list<string>, f, collector.HeaderFiles()) {
            const string& rel_source_file = *f;
            CRef<msbuild::CItemGroup::C_E> p(new msbuild::CItemGroup::C_E);
            t->SetItemGroup().SetItemGroup().push_back(p);
            p->SetClInclude().SetAttlist().SetInclude(rel_source_file);
        }
    }
    // inline
    if (!collector.InlineFiles().empty()) {
        CRef<msbuild::CProject::C_ProjectLevelTagType::C_E> t(new msbuild::CProject::C_ProjectLevelTagType::C_E);
        project.SetProjectLevelTagType().SetProjectLevelTagType().push_back(t);
        ITERATE(list<string>, f, collector.InlineFiles()) {
            const string& rel_source_file = *f;
            CRef<msbuild::CItemGroup::C_E> p(new msbuild::CItemGroup::C_E);
            t->SetItemGroup().SetItemGroup().push_back(p);
            p->SetNone().SetAttlist().SetInclude(rel_source_file);
        }
    }
    // resource
    if (!collector.ResourceFiles().empty()) {
        CRef<msbuild::CProject::C_ProjectLevelTagType::C_E> t(new msbuild::CProject::C_ProjectLevelTagType::C_E);
        project.SetProjectLevelTagType().SetProjectLevelTagType().push_back(t);
        ITERATE(list<string>, f, collector.ResourceFiles()) {
            const string& rel_source_file = *f;
            CRef<msbuild::CItemGroup::C_E> p(new msbuild::CItemGroup::C_E);
            t->SetItemGroup().SetItemGroup().push_back(p);
            p->SetResourceCompile().SetAttlist().SetInclude(rel_source_file);
// exclude from disabled configurations
#if __USE_DISABLED_CFGS__
            if (m_project_configs.size() != all_cfgs.size()) {
                ITERATE(list<SConfigInfo>, c , all_cfgs) {
                    string cfg_condition("'$(Configuration)|$(Platform)'=='");
                    cfg_condition += c->GetConfigFullName() + "|" + CMsvc7RegSettings::GetMsvcPlatformName() + "'";
                    if (find(m_project_configs.begin(), m_project_configs.end(), *c) == m_project_configs.end()) {
                        __SET_RC_ELEMENT(p, "ExcludedFromBuild", "true", cfg_condition);
                    }
                }
            }
#endif
        }
    }
    // custom build and datatool files
    list<SCustomBuildInfo> info_list = prj.m_CustomBuild;
    copy(project_context.GetCustomBuildInfo().begin(), 
         project_context.GetCustomBuildInfo().end(), back_inserter(info_list));
    if ( !prj.m_DatatoolSources.empty() ) {
        ITERATE(list<CDataToolGeneratedSrc>, f, prj.m_DatatoolSources) {
            const CDataToolGeneratedSrc& src = *f;
            SCustomBuildInfo build_info;
            s_CreateDatatoolCustomBuildInfo(prj, project_context, src, &build_info);
            info_list.push_back(build_info);
        }
    }
    if ( !info_list.empty() ) {
        CRef<msbuild::CProject::C_ProjectLevelTagType::C_E> t(new msbuild::CProject::C_ProjectLevelTagType::C_E);
        project.SetProjectLevelTagType().SetProjectLevelTagType().push_back(t);
        set<string> processed;
        ITERATE(list<SCustomBuildInfo>, f, info_list) { 
            const SCustomBuildInfo& build_info = *f;
            string rel_source_file =
                CDirEntry::CreateRelativePath(project_context.ProjectDir(), build_info.m_SourceFile);
            if (processed.find(rel_source_file) != processed.end()) {
                continue;
            }
            processed.insert(rel_source_file);
            CRef<msbuild::CItemGroup::C_E> p(new msbuild::CItemGroup::C_E);
            t->SetItemGroup().SetItemGroup().push_back(p);
            p->SetCustomBuild().SetAttlist().SetInclude(rel_source_file);
            __SET_CUSTOMBUILD_ELEMENT(p,"FileType", "Document");
            ITERATE(list<SConfigInfo>, c , all_cfgs) {
                string cfg_condition("'$(Configuration)|$(Platform)'=='");
                cfg_condition += c->GetConfigFullName() + "|" + CMsvc7RegSettings::GetMsvcPlatformName() + "'";
                __SET_CUSTOMBUILD_ELEMENT(p,"Message",
                    CMsvcMetaMakefile::TranslateCommand(build_info.m_Description), cfg_condition);
                __SET_CUSTOMBUILD_ELEMENT(p,"Command",
                    CMsvcMetaMakefile::TranslateCommand(build_info.m_CommandLine), cfg_condition);
                __SET_CUSTOMBUILD_ELEMENT(p,"AdditionalInputs",
                    CMsvcMetaMakefile::TranslateCommand(build_info.m_AdditionalDependencies), cfg_condition);
                __SET_CUSTOMBUILD_ELEMENT(p,"Outputs",
                    CMsvcMetaMakefile::TranslateCommand(build_info.m_Outputs), cfg_condition);
            }
        }
    }
    // extra files
    if (!collector.GetExtraFiles().empty()) {
        const map<string, list<string> >& extra(collector.GetExtraFiles());
        for (map<string, list<string> >::const_iterator g = extra.begin(); g != extra.end(); ++g) {
            const list<string>& lst(g->second);
            CRef<msbuild::CProject::C_ProjectLevelTagType::C_E> t(new msbuild::CProject::C_ProjectLevelTagType::C_E);
            project.SetProjectLevelTagType().SetProjectLevelTagType().push_back(t);
            ITERATE( list<string>, f, lst) {
                const string& rel_source_file = *f;
                CRef<msbuild::CItemGroup::C_E> p(new msbuild::CItemGroup::C_E);
                t->SetItemGroup().SetItemGroup().push_back(p);
                p->SetNone().SetAttlist().SetInclude(rel_source_file);

                ITERATE(list<SConfigInfo>, c , all_cfgs) {
                    string cfg_condition("'$(Configuration)|$(Platform)'=='");
                    cfg_condition += c->GetConfigFullName() + "|" + CMsvc7RegSettings::GetMsvcPlatformName() + "'";
                    {
                        __SET_NONE_ELEMENT(p, "ExcludedFromBuild", "true", cfg_condition);

                    }
                }
            }
        }
    }


    // references
    if (prj.m_ProjType == CProjKey::eApp || prj.m_ProjType == CProjKey::eDll) {

        map<string,string> guid_to_path;
        ITERATE(list<CProjKey>, k, prj.m_Depends) {
            const CProjKey& id = *k;
            if ( GetApp().GetSite().IsLibWithChoice(id.Id()) ) {
                if ( GetApp().GetSite().GetChoiceForLib(id.Id()) == CMsvcSite::e3PartyLib ) {
                    continue;
                }
            }
            CProjectItemsTree::TProjects::const_iterator n = GetApp().GetCurrentBuildTree()->m_Projects.find(id);
            if (n != GetApp().GetCurrentBuildTree()->m_Projects.end() && 
                (n->first.Type() == CProjKey::eLib ||
                 n->first.Type() == CProjKey::eDll ||
                 n->first.Type() == CProjKey::eMsvc)) {
                if (prj.m_GUID != n->second.m_GUID) {
                    guid_to_path[n->second.m_GUID] = CDirEntry::ConcatPath(
                        CDirEntry::CreateRelativePath(prj.m_SourcesBaseDir, n->second.m_SourcesBaseDir),
                        CreateProjectName(n->first)) + CMsvc7RegSettings::GetVcprojExt();
                }
            }
        }

        if (!guid_to_path.empty()) {
            CRef<msbuild::CProject::C_ProjectLevelTagType::C_E> t(new msbuild::CProject::C_ProjectLevelTagType::C_E);
            project.SetProjectLevelTagType().SetProjectLevelTagType().push_back(t);
            for (map<string,string>::const_iterator gp = guid_to_path.begin(); gp != guid_to_path.end(); ++gp) {
                CRef<msbuild::CItemGroup::C_E> p(new msbuild::CItemGroup::C_E);
                t->SetItemGroup().SetItemGroup().push_back(p);
                p->SetProjectReference().SetAttlist().SetInclude(gp->second);
                {
                    CRef<msbuild::CProjectReference::C_E> e(new msbuild::CProjectReference::C_E);
                    e->SetNPRSTOPE().SetProject(gp->first);
                    p->SetProjectReference().SetProjectReference().push_back(e);
                }
            }
        }
    }

    // almost done
    {
        CRef<msbuild::CProject::C_ProjectLevelTagType::C_E> t(new msbuild::CProject::C_ProjectLevelTagType::C_E);
        t->SetImport().SetAttlist().SetProject("$(VCTargetsPath)\\Microsoft.Cpp.targets");
        project.SetProjectLevelTagType().SetProjectLevelTagType().push_back(t);
    }
    {
        CRef<msbuild::CProject::C_ProjectLevelTagType::C_E> t(new msbuild::CProject::C_ProjectLevelTagType::C_E);
        t->SetImportGroup().SetAttlist().SetLabel("ExtensionTargets");
        project.SetProjectLevelTagType().SetProjectLevelTagType().push_back(t);
    }

// save
    string project_path = CDirEntry::ConcatPath(project_context.ProjectDir(), 
                                                project_context.ProjectName());
    project_path += CMsvc7RegSettings::GetVcprojExt();

    SaveIfNewer(project_path, project);

    GenerateMsbuildFilters(collector, project_context, prj);
    GenerateMsbuildUser(collector, project_context, prj);
}

class CMsbuildFileFilter
{
public:
    CMsbuildFileFilter(
        msbuild::CProject& filters,
        CRef<msbuild::CProject::C_ProjectLevelTagExceptTargetOrImportType::C_E>& filter_list,
        const string& file_extensions,
        const string& tag_name,
        const string& def_filter_name,
        const string& project_dir,
        CMsvcPrjFilesCollector& collector,
        CProjItem& prj);
    ~CMsbuildFileFilter(void);
    void AddFile(const string& name);

    static void BeginNewProject(void)
    {
        s_project_initialized = false;
    }

private:
    msbuild::CProject& m_filters;
    CRef<msbuild::CProject::C_ProjectLevelTagExceptTargetOrImportType::C_E>& m_filter_list;
    string m_file_extensions;
    string m_tag_name;
    string m_project_dir;
    CMsvcPrjFilesCollector& m_collector;
    CProjItem& m_prj;

    map<string, CRef<msbuild::CItemGroup::C_E> > m_filter_id_map;
    map<string, CRef<msbuild::CProject::C_ProjectLevelTagType::C_E> > m_filter_files_map;
    map<string, string> m_filter_name_map;

    CDllSrcFilesDistr& m_dll_src;
    CProjKey m_proj_key;
    set<string> m_processed;
    static bool s_project_initialized;
};

bool CMsbuildFileFilter::s_project_initialized = false;

CMsbuildFileFilter::CMsbuildFileFilter(
        msbuild::CProject& filters,
        CRef<msbuild::CProject::C_ProjectLevelTagExceptTargetOrImportType::C_E>& filter_list,
        const string& file_extensions,
        const string& tag_name,
        const string& def_filter_name,
        const string& project_dir,
        CMsvcPrjFilesCollector& collector,
        CProjItem& prj) :

    m_filters(filters),
    m_filter_list(filter_list),
    m_file_extensions(file_extensions),
    m_project_dir(project_dir),
    m_tag_name(tag_name),
    m_collector(collector),
    m_prj(prj),
    m_dll_src( GetApp().GetDllFilesDistr()),
    m_proj_key(prj.m_ProjType, prj.m_ID)
{
    string filter_lib("Hosted Libraries");
    list<string> libs = prj.m_HostedLibs;
    libs.push_back("");
    libs.sort();
    libs.unique();
    
    
    ITERATE( list<string>, hosted_lib, libs) {
        m_filter_id_map[*hosted_lib] = new msbuild::CItemGroup::C_E;
        m_filter_files_map[*hosted_lib] = new msbuild::CProject::C_ProjectLevelTagType::C_E;
        if (hosted_lib->empty()) {
            m_filter_name_map[*hosted_lib] = def_filter_name;
        } else {
            m_filter_name_map[*hosted_lib] = filter_lib + "\\" + *hosted_lib + "\\" + def_filter_name;
        }
    }

    if (!prj.m_HostedLibs.empty() && !s_project_initialized) {
        ITERATE( list<string>, hosted_lib, libs) {
            string filter_name(filter_lib);
            if (!hosted_lib->empty()) {
                filter_name += "\\" + *hosted_lib;
            }
            CRef<msbuild::CItemGroup::C_E> filter_id( new msbuild::CItemGroup::C_E);
            filter_id->SetFilter().SetAttlist().SetInclude(filter_name);
            filter_id->SetFilter().SetUniqueIdentifier(GenerateSlnGUID());
            filter_id->SetFilter().SetExtensions("");
            m_filter_list->SetItemGroup().SetItemGroup().push_back(filter_id);
        }
        s_project_initialized = true;
    }
}

CMsbuildFileFilter::~CMsbuildFileFilter(void)
{
    map<string, CRef<msbuild::CProject::C_ProjectLevelTagType::C_E> >::const_iterator i;
    for (i = m_filter_files_map.begin(); i != m_filter_files_map.end(); ++i) {
        CRef<msbuild::CProject::C_ProjectLevelTagType::C_E> filter_files = i->second;
        if (filter_files->Which() != msbuild::CProject::C_ProjectLevelTagType::C_E::e_not_set) {
            CRef<msbuild::CItemGroup::C_E> filter_id = m_filter_id_map[i->first];
            string filter_name = m_filter_name_map[i->first];
            filter_id->SetFilter().SetAttlist().SetInclude(filter_name);
            filter_id->SetFilter().SetUniqueIdentifier(GenerateSlnGUID());
            filter_id->SetFilter().SetExtensions(m_file_extensions);
            m_filter_list->SetItemGroup().SetItemGroup().push_back(filter_id);
            m_filters.SetProjectLevelTagType().SetProjectLevelTagType().push_back(filter_files);
        }
    }
}

void CMsbuildFileFilter::AddFile(const string& file_name)
{
    if (m_processed.find(file_name) != m_processed.end()) {
        return;
    }
    m_processed.insert(file_name);
    string abs_name(file_name);
    if (!CDirEntry::IsAbsolutePath(file_name)) {
        abs_name = CDirEntry::ConcatPath( m_project_dir, file_name);
    }
    abs_name = CDirEntry::NormalizePath( abs_name);
    CProjKey hosted_key = m_dll_src.GetFileLib( abs_name, m_proj_key);
    CRef<msbuild::CProject::C_ProjectLevelTagType::C_E>& filter_files = m_filter_files_map[hosted_key.Id()];
    const string& filter_name = m_filter_name_map[hosted_key.Id()];

    CRef<msbuild::CItemGroup::C_E> file_id(new msbuild::CItemGroup::C_E);
    filter_files->SetItemGroup().SetItemGroup().push_back(file_id);
    file_id->SetAnyContent().SetName(m_tag_name);
    file_id->SetAnyContent().AddAttribute("Include",kEmptyStr,CUtf8::AsUTF8(file_name,eEncoding_UTF8));
    file_id->SetAnyContent().SetValue(CUtf8::AsUTF8("<Filter>" + filter_name + "</Filter>",eEncoding_UTF8));
}

void CMsvcProjectGenerator::GenerateMsbuildFilters(
    CMsvcPrjFilesCollector& collector,
    CMsvcPrjProjectContext& project_context, CProjItem& prj)
{
    msbuild::CProject filters;
    filters.SetAttlist().SetToolsVersion("4.0");
    string project_dir(project_context.ProjectDir());
#if __USE_DISABLED_CFGS__
    const list<SConfigInfo>& all_cfgs = GetApp().GetRegSettings().m_ConfigInfo;
#else
    const list<SConfigInfo>& all_cfgs = m_project_configs;
#endif

    CRef<msbuild::CProject::C_ProjectLevelTagExceptTargetOrImportType::C_E> filter_list(new msbuild::CProject::C_ProjectLevelTagExceptTargetOrImportType::C_E);
    CMsbuildFileFilter::BeginNewProject();

    // sources
    if (!collector.SourceFiles().empty()) {
        string tag_name("ClCompile");
        string filter_name("Source Files");
        CMsbuildFileFilter filter( filters, filter_list, "cpp;c;cxx",
            tag_name, filter_name, project_dir, collector, prj);
        ITERATE(list<string>, f, collector.SourceFiles()) {
            const string& rel_source_file = *f;
            if ( NStr::Find(rel_source_file, ".@config@") == NPOS ) {
                filter.AddFile(rel_source_file);
            } else {
                ITERATE(list<SConfigInfo>, c, all_cfgs) {
                    const string& cfg_name = c->GetConfigFullName();
                    string cfg_file = NStr::Replace(rel_source_file, ".@config@", "." + cfg_name);
                    filter.AddFile(cfg_file);
                }
            }
        }
    }
    if (!collector.HeaderFiles().empty()) {
        string tag_name("ClInclude");
        string filter_name("Header Files");
        CMsbuildFileFilter filter( filters, filter_list, "hpp;h;hxx",
            tag_name, filter_name, project_dir, collector, prj);
        ITERATE(list<string>, f, collector.HeaderFiles()) {
            filter.AddFile(*f);
        }
    }
    if (!collector.InlineFiles().empty()) {
        string tag_name("None");
        string filter_name("Inline Files");
        CMsbuildFileFilter filter( filters, filter_list, "inl",
            tag_name, filter_name, project_dir, collector, prj);
        ITERATE(list<string>, f, collector.InlineFiles()) {
            filter.AddFile(*f);
        }
    }
    if (!collector.ResourceFiles().empty()) {
        string tag_name("ResourceCompile");
        string filter_name("Resource Files");
        CMsbuildFileFilter filter( filters, filter_list, "rc",
            tag_name, filter_name, project_dir, collector, prj);
        ITERATE(list<string>, f, collector.ResourceFiles()) {
            filter.AddFile(*f);
        }
    }
    list<SCustomBuildInfo> info_list = prj.m_CustomBuild;
    copy(project_context.GetCustomBuildInfo().begin(), 
         project_context.GetCustomBuildInfo().end(), back_inserter(info_list));
    if ( !prj.m_DatatoolSources.empty() ) {
        ITERATE(list<CDataToolGeneratedSrc>, f, prj.m_DatatoolSources) {
            const CDataToolGeneratedSrc& src = *f;
            SCustomBuildInfo build_info;
            s_CreateDatatoolCustomBuildInfo(prj, project_context, src, &build_info);
            info_list.push_back(build_info);
        }
    }
    if ( !info_list.empty() ) {
        string tag_name("CustomBuild");
        string filter_name("Custom Build Files");
        CMsbuildFileFilter filter( filters, filter_list, "asn;dtd;xsd",
            tag_name, filter_name, project_dir, collector, prj);
        ITERATE(list<SCustomBuildInfo>, f, info_list) { 
            const SCustomBuildInfo& build_info = *f;
            string rel_source_file =
                CDirEntry::CreateRelativePath(project_context.ProjectDir(), build_info.m_SourceFile);
            filter.AddFile(rel_source_file);
        }
    }
    if (!collector.GetExtraFiles().empty()) {
        const map<string, list<string> >& extra(collector.GetExtraFiles());
        for (map<string, list<string> >::const_iterator g = extra.begin(); g != extra.end(); ++g) {
            const list<string>& lst(g->second);
            string tag_name("None");
            string filter_name(g->first);
            CMsbuildFileFilter filter( filters, filter_list, "",
                tag_name, filter_name, project_dir, collector, prj);
            ITERATE(list<string>, f, lst) {
                filter.AddFile(*f);
            }
        }
    }

    bool save_filters = false;
    try {
        save_filters = !filter_list->GetItemGroup().GetItemGroup().empty();
    } catch (CInvalidChoiceSelection&) {
        save_filters = false;
    }
    if (save_filters) {
        string project_path = CDirEntry::ConcatPath(project_context.ProjectDir(), 
                                                    project_context.ProjectName());
        project_path += CMsvc7RegSettings::GetVcprojExt();
        project_path += ".filters";

        filters.SetProjectLevelTagExceptTargetOrImportType().SetProjectLevelTagExceptTargetOrImportType().push_back(filter_list);
        SaveIfNewer(project_path, filters, "<UniqueIdentifier>");
    }
}

void CMsvcProjectGenerator::GenerateMsbuildUser(
                        CMsvcPrjFilesCollector& collector,
                        CMsvcPrjProjectContext& project_context,
                        CProjItem& prj)
{
    if (prj.m_ProjType != CProjKey::eApp) {
        return;
    }
    string project_path = CDirEntry::ConcatPath(project_context.ProjectDir(), 
                                                project_context.ProjectName());
    project_path += CMsvc7RegSettings::GetVcprojExt();
    project_path += ".user";
    if (CDirEntry(project_path).Exists()) {
        return;
    }
    const CMsvcSite& site = GetApp().GetSite();
    list<string> bin_dirs;

    ITERATE( list<string>, r, prj.m_Requires) {
        string bin = site.GetThirdPartyLibBin(*r);
        if (!bin.empty()) {
            bin_dirs.push_back( CDirEntry::ConcatPath(bin, "$(Configuration)"));
        }
    }
    ITERATE( list<string>, p, prj.m_Libs3Party) {
        list<string> cmp;
        site.GetComponents(*p, &cmp);
        ITERATE( list<string>, r, cmp) {
            string bin = site.GetThirdPartyLibBin(*r);
            if (!bin.empty()) {
                bin_dirs.push_back( CDirEntry::ConcatPath(bin, "$(Configuration)"));
            }
        }
    }
    if (bin_dirs.empty()) {
        return;
    }
    bin_dirs.sort();
    bin_dirs.unique();
    bin_dirs.push_back("%PATH%");
    string allbins = string("PATH=")+NStr::Join(bin_dirs, ";");

#if __USE_DISABLED_CFGS__
    const list<SConfigInfo>& all_cfgs = GetApp().GetRegSettings().m_ConfigInfo;
#else
    const list<SConfigInfo>& all_cfgs = m_project_configs;
#endif
    msbuild::CProject project;
    project.SetAttlist().SetToolsVersion("4.0");

    ITERATE(list<SConfigInfo>, c , all_cfgs) {
        const SConfigInfo& cfg_info = *c;
        string cfg_condition("'$(Configuration)|$(Platform)'=='");
        cfg_condition += c->GetConfigFullName() + "|" + CMsvc7RegSettings::GetMsvcPlatformName() + "'";
        CRef<msbuild::CProject::C_ProjectLevelTagType::C_E> t(new msbuild::CProject::C_ProjectLevelTagType::C_E);
        t->SetPropertyGroup().SetAttlist().SetCondition(cfg_condition);
        __SET_PROPGROUP_ELEMENT( t, "LocalDebuggerEnvironment", allbins);
        __SET_PROPGROUP_ELEMENT( t, "DebuggerFlavor", "WindowsLocalDebugger");
        project.SetProjectLevelTagType().SetProjectLevelTagType().push_back(t);
    }

    SaveIfNewer(project_path, project);
}


#endif //NCBI_COMPILER_MSVC


END_NCBI_SCOPE
