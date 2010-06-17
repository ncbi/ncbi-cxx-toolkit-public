#include <ncbi_pch.hpp>
#include "msvc_prj_generator.hpp"
#include "proj_builder_app.hpp"
#include "msvc_project_context.hpp"


#include "msvc_prj_utils.hpp"
#include "msvc_prj_defines.hpp"
#include "ptb_err_codes.hpp"

#define USE_MSVC2010  0
#if USE_MSVC2010
#if NCBI_COMPILER_MSVC
#   include <build-system/project_tree_builder/msbuild/msbuild_dataobj__.hpp>
#endif //NCBI_COMPILER_MSVC
#endif //USE_MSVC2010

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
    if ( project_context.IsPchEnabled(cfg_tmp) ) {
        string noname = CDirEntry::ConcatPath(prj.m_SourcesBaseDir,"aanofile");
        m_pch_default = project_context.GetPchHeader(
            prj.m_ID, noname, GetApp().GetProjectTreeInfo().m_Src, cfg_tmp);
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

    ITERATE(list<SConfigInfo>, p , m_project_configs) {

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
            if (!m_pkg_export_command.empty()) {
                tool->SetAttlist().SetCommandLine( m_pkg_export_command);
            }
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
            "Using datatool to create a C++ object from ASN/DTD/Schema $(InputPath)";

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
            "Using datatool to create a C++ object from ASN/DTD $(InputPath)";

        //Outputs
        build_info->m_Outputs = "$(InputDir)$(InputName).files;$(InputDir)$(InputName)__.cpp;$(InputDir)$(InputName)___.cpp";

        //Additional Dependencies
        build_info->m_AdditionalDependencies = "$(InputDir)$(InputName).def;" + tool_exe_location;
    }
}

#if USE_MSVC2010

template<typename Container>
void __SET_PROPGROUP_ELEMENT(
    Container& container, const string& name, const string& value,
    const string& condition = kEmptyStr)
{
    CRef<msbuild::CPropertyGroup::C_E> e(new msbuild::CPropertyGroup::C_E);
    e->SetAnyContent().SetName(name);
    e->SetAnyContent().SetValue(value);
    if (!condition.empty()) {
       e->SetAnyContent().AddAttribute("Condition", kEmptyStr, condition);
    }
    container->SetPropertyGroup().SetPropertyGroup().push_back(e);
}

template<typename Container>
void __SET_CLCOMPILE_ELEMENT(
    Container& container, const string& name, const string& value)
{
    CRef<msbuild::CClCompile::C_E> e(new msbuild::CClCompile::C_E);
    e->SetAnyContent().SetName(name);
    e->SetAnyContent().SetValue(value);
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
    e->SetAnyContent().SetValue(value);
    container->SetLib().Set().push_back(e);
}


#endif  // USE_MSVC2010


void CMsvcProjectGenerator::GenerateMsbuild(
    CMsvcPrjFilesCollector& collector,
    CMsvcPrjProjectContext& project_context, CProjItem& prj)
{
#if USE_MSVC2010
    msbuild::CProject project;
    project.SetAttlist().SetDefaultTargets("Build");
    project.SetAttlist().SetToolsVersion("4.0");

// ProjectLevelTagExceptTargetOrImportType
    {
        // project GUID
        CRef<msbuild::CProject::C_ProjectLevelTagExceptTargetOrImportType::C_E> t(new msbuild::CProject::C_ProjectLevelTagExceptTargetOrImportType::C_E);
        t->SetPropertyGroup().SetAttlist().SetLabel("Globals");
        project.SetProjectLevelTagExceptTargetOrImportType().SetProjectLevelTagExceptTargetOrImportType().push_back(t);
        __SET_PROPGROUP_ELEMENT( t, "ProjectGuid", prj.m_GUID );
        __SET_PROPGROUP_ELEMENT( t, "Keyword", MSVC_PROJECT_KEYWORD_WIN32 );
    }
    {
        // project configurations
        CRef<msbuild::CProject::C_ProjectLevelTagExceptTargetOrImportType::C_E> t(new msbuild::CProject::C_ProjectLevelTagExceptTargetOrImportType::C_E);
        t->SetItemGroup().SetAttlist().SetLabel("ProjectConfigurations");
        ITERATE(list<SConfigInfo>, c , m_project_configs) {
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
        ITERATE(list<SConfigInfo>, c , m_project_configs) {

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
            __SET_PROPGROUP_ELEMENT(t, "CharacterSet", msvc_tool.Configuration()->CharacterSet());
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
        ITERATE(list<SConfigInfo>, c , m_project_configs) {
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
    
    {
        // File version
        CRef<msbuild::CProject::C_ProjectLevelTagType::C_E> t(new msbuild::CProject::C_ProjectLevelTagType::C_E);
        project.SetProjectLevelTagType().SetProjectLevelTagType().push_back(t);
        __SET_PROPGROUP_ELEMENT( t, "_ProjectFileVersion", GetApp().GetRegSettings().GetProjectFileFormatVersion());

        // OutDir/IntDir/TargetName
        ITERATE(list<SConfigInfo>, c , m_project_configs) {

            const SConfigInfo& cfg_info = *c;
            CMsvcPrjGeneralContext general_context(cfg_info, project_context);
            CMsvcTools msvc_tool(general_context, project_context);
            string cfg_condition("'$(Configuration)|$(Platform)'=='");
            cfg_condition += c->GetConfigFullName() + "|" + CMsvc7RegSettings::GetMsvcPlatformName() + "'";

            __SET_PROPGROUP_ELEMENT(t, "OutDir", msvc_tool.Configuration()->OutputDirectory(), cfg_condition);
            __SET_PROPGROUP_ELEMENT(t, "IntDir", msvc_tool.Configuration()->IntermediateDirectory(), cfg_condition);
            __SET_PROPGROUP_ELEMENT(t, "TargetName", project_context.ProjectId(), cfg_condition);
        }
    }

    // compilation settings
    ITERATE(list<SConfigInfo>, c , m_project_configs) {

        const SConfigInfo& cfg_info = *c;
        CMsvcPrjGeneralContext general_context(cfg_info, project_context);
        CMsvcTools msvc_tool(general_context, project_context);
        string cfg_condition("'$(Configuration)|$(Platform)'=='");
        cfg_condition += c->GetConfigFullName() + "|" + CMsvc7RegSettings::GetMsvcPlatformName() + "'";

        {
            CRef<msbuild::CProject::C_ProjectLevelTagType::C_E> t(new msbuild::CProject::C_ProjectLevelTagType::C_E);
            t->SetItemDefinitionGroup().SetAttlist().SetCondition(cfg_condition);
            project.SetProjectLevelTagType().SetProjectLevelTagType().push_back(t);
            // compiler
            {
               CRef<msbuild::CItemDefinitionGroup::C_E> p(new msbuild::CItemDefinitionGroup::C_E);
               t->SetItemDefinitionGroup().SetItemDefinitionGroup().push_back(p);
               __SET_CLCOMPILE(p, AdditionalOptions);
               __SET_CLCOMPILE(p, Optimization);
               __SET_CLCOMPILE(p, InlineFunctionExpansion);
               __SET_CLCOMPILE(p, FavorSizeOrSpeed);
               __SET_CLCOMPILE(p, OmitFramePointers);
               __SET_CLCOMPILE(p, AdditionalIncludeDirectories);
               __SET_CLCOMPILE(p, PreprocessorDefinitions);
               __SET_CLCOMPILE(p, IgnoreStandardIncludePath);
               __SET_CLCOMPILE(p, StringPooling);
               __SET_CLCOMPILE(p, MinimalRebuild);
               __SET_CLCOMPILE(p, BasicRuntimeChecks);
               __SET_CLCOMPILE(p, RuntimeLibrary);
               __SET_CLCOMPILE(p, ProgramDataBaseFileName);
               __SET_CLCOMPILE(p, StructMemberAlignment);
               __SET_CLCOMPILE(p, RuntimeTypeInfo);
//todo: PrecompiledHeader
// 
               __SET_CLCOMPILE(p, BrowseInformation);
               __SET_CLCOMPILE(p, WarningLevel);
               __SET_CLCOMPILE(p, DebugInformationFormat);
               __SET_CLCOMPILE(p, CallingConvention);
               __SET_CLCOMPILE(p, CompileAs);
               __SET_CLCOMPILE(p, DisableSpecificWarnings);
               __SET_CLCOMPILE(p, UndefinePreprocessorDefinitions);
            }
            // librarian
            {
               CRef<msbuild::CItemDefinitionGroup::C_E> p(new msbuild::CItemDefinitionGroup::C_E);
               t->SetItemDefinitionGroup().SetItemDefinitionGroup().push_back(p);
               __SET_LIB_ELEMENT(p, "AdditionalOptions", msvc_tool.Librarian()->AdditionalOptions());
               __SET_LIB_ELEMENT(p, "OutputFile", msvc_tool.Librarian()->OutputFile());
               __SET_LIB_ELEMENT(p, "AdditionalLibraryDirectories", msvc_tool.Librarian()->AdditionalLibraryDirectories());
               __SET_LIB_ELEMENT(p, "IgnoreAllDefaultLibraries", msvc_tool.Librarian()->IgnoreAllDefaultLibraries());
               __SET_LIB_ELEMENT(p, "IgnoreSpecificDefaultLibraries", msvc_tool.Librarian()->IgnoreDefaultLibraryNames());
            }
        }
    }
    
    // sources
    {
        CRef<msbuild::CProject::C_ProjectLevelTagType::C_E> t(new msbuild::CProject::C_ProjectLevelTagType::C_E);
        project.SetProjectLevelTagType().SetProjectLevelTagType().push_back(t);
        

        ITERATE(list<string>, f, collector.SourceFiles()) {
            const string& rel_source_file = *f;
            CRef<msbuild::CItemGroup::C_E> p(new msbuild::CItemGroup::C_E);
            t->SetItemGroup().SetItemGroup().push_back(p);
            p->SetClCompile().SetAttlist().SetInclude(rel_source_file);
/*
            {
                CRef<msbuild::CClCompile::C_E> e(new msbuild::CClCompile::C_E);
                e->SetAnyContent().SetName("PreprocessorDefinitions");
                e->SetAnyContent().SetValue("NCBI_USE_PCH");
                p->SetClCompile().SetClCompile().push_back(e);
            }
*/
        }
    }
    


    // alsmost done
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
#endif
}

#endif //NCBI_COMPILER_MSVC


END_NCBI_SCOPE
