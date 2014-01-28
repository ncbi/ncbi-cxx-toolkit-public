#ifndef PROJECT_TREE_BUILDER__TOOLS_IMPLEMENT__HPP
#define PROJECT_TREE_BUILDER__TOOLS_IMPLEMENT__HPP

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


#include <string>

#include "msvc_project_context.hpp"
#include "msvc_traits.hpp"
#include "msvc_prj_utils.hpp"

#include <corelib/ncbienv.hpp>

BEGIN_NCBI_SCOPE

/////////////////////////////////////////////////////////////////////////////
///
/// CConfigurationImpl --
///
/// Implementation of IConfiguration interface.
///
/// Accepts trait class as a template parameter.

template <class ConfTrait> 
class CConfigurationImpl : public IConfiguration
{
public:
    CConfigurationImpl(	const string& output_directory, 
                        const string& configuration_name,
                        const IMsvcMetaMakefile& project_makefile,
                        const IMsvcMetaMakefile& meta_makefile,
                        const SConfigInfo&       config )
                        :m_OutputDirectory  (output_directory),
                         m_ConfigurationName(configuration_name),
                         m_MsvcProjectMakefile         (project_makefile),
                         m_MsvcMetaMakefile            (meta_makefile),
                         m_Config                      (config)
    {
    }

#define SUPPORT_CONFIGURATION_OPTION(opt) \
    virtual string opt(void) const \
    { \
        return GetConfigurationOpt(m_MsvcMetaMakefile, \
                              m_MsvcProjectMakefile, \
                              #opt, \
                              m_Config ); \
    }

    virtual string Name(void) const
    {
	    return ConfigName(m_ConfigurationName);
    }
    virtual string OutputDirectory(void) const
    {
	    return m_OutputDirectory;
    }
    virtual string IntermediateDirectory(void) const
    {
        if (CMsvc7RegSettings::GetMsvcPlatform() < CMsvc7RegSettings::eUnix) {
	        return CDirEntry::AddTrailingPathSeparator(
                CDirEntry::ConcatPath(
                    CMsvc7RegSettings::GetConfigNameKeyword(),"$(TargetName)"));
        }
	    return CDirEntry::AddTrailingPathSeparator(
                CMsvc7RegSettings::GetConfigNameKeyword());
    }
    virtual string ConfigurationType(void) const
    {
	    return CMsvcMetaMakefile::TranslateOpt(
	        ConfTrait::ConfigurationType(),"Configuration","ConfigurationType");
    }
#if 1
    virtual string CharacterSet(void) const
    {
        if (m_Config.m_Unicode) {
            return CMsvcMetaMakefile::TranslateOpt("1","Configuration","CharacterSet");
        }
        string val = GetConfigurationOpt(
            m_MsvcMetaMakefile, m_MsvcProjectMakefile,
            "CharacterSet",m_Config );
        if (val.empty()) {
            val = CMsvcMetaMakefile::TranslateOpt("2","Configuration","CharacterSet");
        }
        return val;
    }
#else
    SUPPORT_CONFIGURATION_OPTION(CharacterSet)
#endif
    SUPPORT_CONFIGURATION_OPTION(PlatformToolset)
    virtual string BuildLogFile(void) const
    {
	    return "$(IntDir)BuildLog_$(TargetName).htm";
    }


private:
    string m_OutputDirectory;
    string m_ConfigurationName;
    const IMsvcMetaMakefile& m_MsvcProjectMakefile;
    const IMsvcMetaMakefile& m_MsvcMetaMakefile;
    SConfigInfo              m_Config;

    CConfigurationImpl(void);
    CConfigurationImpl(const CConfigurationImpl&);
    CConfigurationImpl& operator= (const CConfigurationImpl&);
};


static string s_GetDefaultPreprocessorDefinitions
                            (const SConfigInfo&                   config, 
                             CMsvcPrjGeneralContext::TTargetType  target_type)
{
    string defines = config.m_Debug ? "_DEBUG;" : "NDEBUG;" ;
    switch (target_type) {
    case CMsvcPrjGeneralContext::eLib:
        defines +=  "WIN32;_LIB;";
        break;
    case CMsvcPrjGeneralContext::eExe:
        defines += "WIN32;_CONSOLE;";
        break;
    case CMsvcPrjGeneralContext::eDll:
        defines += "WIN32;_WINDOWS;_USRDLL;";
        break;
    default:
        break;
    }
    return defines;
}


/////////////////////////////////////////////////////////////////////////////
///
/// CCompilerToolImpl --
///
/// Implementation of ICompilerTool interface.
///
/// Uses msvc makefiles information
class CCompilerToolImpl : public ICompilerTool
{
public:
    typedef CMsvcPrjGeneralContext::TTargetType TTargetType;

    CCompilerToolImpl(const string&            additional_include_dirs,
                      const IMsvcMetaMakefile& project_makefile,
                      const string&            runtimeLibraryOption,
                      const IMsvcMetaMakefile& meta_makefile,
                      const SConfigInfo&       config,
                      TTargetType              target_type,
                      const list<string>&      defines,
                      const string&            project_id)
	    :m_AdditionalIncludeDirectories(additional_include_dirs),
         m_MsvcProjectMakefile         (project_makefile),
         m_RuntimeLibraryOption        (runtimeLibraryOption),
         m_MsvcMetaMakefile            (meta_makefile),
         m_Config                      (config),
         m_Defines                     (defines),
         m_TargetType                  (target_type),
		 m_ProjectId                   (project_id)
    {
    }

    virtual string Name(void) const
    {
	    return "VCCLCompilerTool";
    }

#define SUPPORT_COMPILER_OPTION(opt) \
    virtual string opt(void) const \
    { \
        return GetCompilerOpt(m_MsvcMetaMakefile, \
                              m_MsvcProjectMakefile, \
                              #opt, \
                              m_Config ); \
    }

    SUPPORT_COMPILER_OPTION(Optimization)

    virtual string AdditionalIncludeDirectories(void) const
    {
	    return m_AdditionalIncludeDirectories;
    }

    //SUPPORT_COMPILER_OPTION(PreprocessorDefinitions)
    virtual string PreprocessorDefinitions(void) const
    {
        string defines = 
            s_GetDefaultPreprocessorDefinitions(m_Config, m_TargetType);

        ITERATE(list<string>, p, m_Defines) {
            const string& define = *p;
            defines += define;
            defines += ';';
        }

#if 0
        defines += GetCompilerOpt(m_MsvcMetaMakefile,
                                  m_MsvcProjectMakefile,
                                  "PreprocessorDefinitions",
                                  m_Config );
#else
        string opt("PreprocessorDefinitions");
        string val;
        val = m_MsvcProjectMakefile.GetCompilerOpt(opt, m_Config);
        if (!val.empty()) {
            defines += val;
            defines += ';';
        }
        val = m_MsvcMetaMakefile.GetCompilerOpt(opt, m_Config);
        if (!val.empty()) {
            defines += val;
        }
#endif
        return defines;
    }

    SUPPORT_COMPILER_OPTION(MinimalRebuild)
    virtual string BasicRuntimeChecks(void) const
    {
#if 0
        if (m_Config.m_VTuneAddon) {
            return CMsvcMetaMakefile::TranslateOpt("0", "Compiler", "BasicRuntimeChecks");
        }
#endif
        return GetCompilerOpt(m_MsvcMetaMakefile,
                              m_MsvcProjectMakefile,
                              "BasicRuntimeChecks",
                              m_Config );
    }
    virtual string RuntimeLibrary(void) const
    {
	    return m_RuntimeLibraryOption;
    }

    SUPPORT_COMPILER_OPTION(RuntimeTypeInfo)
    SUPPORT_COMPILER_OPTION(UsePrecompiledHeader)
    SUPPORT_COMPILER_OPTION(WarningLevel)
    SUPPORT_COMPILER_OPTION(Detect64BitPortabilityProblems)
    virtual string DebugInformationFormat(void) const
    {
        if (m_Config.m_VTuneAddon) {
            return CMsvcMetaMakefile::TranslateOpt("3", "Compiler", "DebugInformationFormat");
        }
        return GetCompilerOpt(m_MsvcMetaMakefile,
                              m_MsvcProjectMakefile,
                              "DebugInformationFormat",
                              m_Config );
    }
    SUPPORT_COMPILER_OPTION(CompileAs)
    SUPPORT_COMPILER_OPTION(InlineFunctionExpansion)
    SUPPORT_COMPILER_OPTION(OmitFramePointers)
    SUPPORT_COMPILER_OPTION(StringPooling)
    SUPPORT_COMPILER_OPTION(EnableFunctionLevelLinking)


    //Latest additions
    SUPPORT_COMPILER_OPTION(OptimizeForProcessor)
    SUPPORT_COMPILER_OPTION(StructMemberAlignment)
    SUPPORT_COMPILER_OPTION(CallingConvention)
    SUPPORT_COMPILER_OPTION(IgnoreStandardIncludePath)
    SUPPORT_COMPILER_OPTION(ExceptionHandling)
    SUPPORT_COMPILER_OPTION(BufferSecurityCheck)
    SUPPORT_COMPILER_OPTION(DisableSpecificWarnings)
    SUPPORT_COMPILER_OPTION(UndefinePreprocessorDefinitions)
    SUPPORT_COMPILER_OPTION(AdditionalOptions)
    SUPPORT_COMPILER_OPTION(GlobalOptimizations)
    SUPPORT_COMPILER_OPTION(FavorSizeOrSpeed)
    SUPPORT_COMPILER_OPTION(BrowseInformation)

    virtual string ProgramDataBaseFileName(void) const
    {
        string pdb_file = 
            GetCompilerOpt(m_MsvcMetaMakefile,
                         m_MsvcProjectMakefile,
                         "ProgramDataBaseFileName", 
                         m_Config );
        if( !pdb_file.empty() )
            return pdb_file;

	    return string("$(IntDir)") + m_ProjectId + ".pdb";
//	    return string("$(OutDir)$(ProjectName).pdb");
    }

private:
    string                   m_AdditionalIncludeDirectories;
    const IMsvcMetaMakefile& m_MsvcProjectMakefile;
    string                   m_RuntimeLibraryOption;
    const IMsvcMetaMakefile& m_MsvcMetaMakefile;
    SConfigInfo              m_Config;
    list<string>             m_Defines;

    TTargetType              m_TargetType;
    string      m_ProjectId;

    // No value-type semantics
    CCompilerToolImpl(void);
    CCompilerToolImpl(const CCompilerToolImpl&);
    CCompilerToolImpl& operator= (const CCompilerToolImpl&);
};


/////////////////////////////////////////////////////////////////////////////
///
/// CLinkerToolImpl --
///
/// Implementation of ILinkerTool interface.
///
/// Accepts trait classes as a template parameters.

template <class ConfTrait > 
class CLinkerToolImpl : public ILinkerTool
{
public:
    CLinkerToolImpl(const string&            additional_options,
                    const string&            additional_library_directories,
                    const string&            project_id,
                    const IMsvcMetaMakefile& project_makefile,
                    const IMsvcMetaMakefile& meta_makefile,
                    const SConfigInfo&       config)
	    :m_AdditionalOptions    (additional_options),
         m_AdditionalLibraryDirectories(additional_library_directories),
		 m_ProjectId            (project_id),
         m_Config               (config),
         m_MsvcProjectMakefile  (project_makefile),
         m_MsvcMetaMakefile     (meta_makefile)
    {
    }
    virtual string Name(void) const
    {
	    return "VCLinkerTool";
    }
    virtual string AdditionalOptions(void) const
    {
#if 0
	    return m_AdditionalOptions + " " +
               GetLinkerOpt(m_MsvcMetaMakefile,
                            m_MsvcProjectMakefile,
                            "AdditionalOptions", 
                            m_Config );
#else
        string options(m_AdditionalOptions);
        string add = m_MsvcMetaMakefile.GetLinkerOpt("AdditionalOptions", m_Config);
        if (!add.empty()) {
            options += " " + add;
        }
        add = m_MsvcProjectMakefile.GetLinkerOpt("AdditionalOptions", m_Config);
        if (!add.empty()) {
            options += " " + add;
        }
        return options;
#endif
    }
    virtual string OutputFile(void) const
    {
        string output_file = 
            GetLinkerOpt(m_MsvcMetaMakefile,
                         m_MsvcProjectMakefile,
                         "OutputFile", 
                         m_Config );
        if( !output_file.empty() )
            return output_file;

//	    return string("$(OutDir)") + m_ProjectId + ConfTrait::TargetExtension();
        if (CMsvc7RegSettings::GetMsvcVersion() >= CMsvc7RegSettings::eMsvc1000) {
    	    return string("$(OutDir)$(TargetName)$(TargetExt)");
        }
	    return string("$(OutDir)$(ProjectName)");
    }

#define SUPPORT_LINKER_OPTION(opt) \
    virtual string opt(void) const \
    { \
        return GetLinkerOpt(m_MsvcMetaMakefile, \
                            m_MsvcProjectMakefile, \
                            #opt, \
                            m_Config ); \
    }
    
    SUPPORT_LINKER_OPTION(LinkIncremental)
    SUPPORT_LINKER_OPTION(LargeAddressAware)
    virtual string GenerateDebugInformation(void) const
    {
        if (m_Config.m_VTuneAddon) {
            return "true";
        }
        return GetLinkerOpt(m_MsvcMetaMakefile,
                            m_MsvcProjectMakefile,
                            "GenerateDebugInformation",
                            m_Config );
    }

    virtual string ProgramDatabaseFile(void) const
    {
        string pdb_file = 
            GetLinkerOpt(m_MsvcMetaMakefile,
                         m_MsvcProjectMakefile,
                         "ProgramDatabaseFile", 
                         m_Config );
        if( !pdb_file.empty() )
            return pdb_file;

	    return string("$(OutDir)") + m_ProjectId + ".pdb";
//	    return string("$(OutDir)$(ProjectName).pdb");
    }

    SUPPORT_LINKER_OPTION(SubSystem)
    
    virtual string ImportLibrary(void) const
    {
	    return string("$(OutDir)") + m_ProjectId + ".lib";
//	    return string("$(OutDir)$(ProjectName).lib");
    }

    SUPPORT_LINKER_OPTION(TargetMachine)
    SUPPORT_LINKER_OPTION(ImageHasSafeExceptionHandlers)
    SUPPORT_LINKER_OPTION(OptimizeReferences)
    SUPPORT_LINKER_OPTION(EnableCOMDATFolding)
    SUPPORT_LINKER_OPTION(IgnoreAllDefaultLibraries)
    SUPPORT_LINKER_OPTION(IgnoreDefaultLibraryNames)
    SUPPORT_LINKER_OPTION(AdditionalDependencies)

    virtual string AdditionalLibraryDirectories(void) const
    {
        string add = 
            GetLinkerOpt(m_MsvcMetaMakefile,
                         m_MsvcProjectMakefile,
                         "AdditionalLibraryDirectories", 
                         m_Config );
        if (!add.empty() && !m_AdditionalLibraryDirectories.empty()) {
            add += ", ";
        }
	    return add + m_AdditionalLibraryDirectories;
    }

    virtual string FixedBaseAddress(void) const
    {
#if 0
        if (m_Config.m_VTuneAddon) {
            return CMsvcMetaMakefile::TranslateOpt("1", "Linker", "FixedBaseAddress");
        }
#endif
        return GetLinkerOpt(m_MsvcMetaMakefile,
                            m_MsvcProjectMakefile,
                            "FixedBaseAddress",
                            m_Config );
    }
    SUPPORT_LINKER_OPTION(GenerateManifest)
    SUPPORT_LINKER_OPTION(EmbedManifest)

private:
    string      m_AdditionalOptions;
    string      m_AdditionalLibraryDirectories;
    string      m_ProjectId;
    SConfigInfo m_Config;

    const IMsvcMetaMakefile& m_MsvcProjectMakefile;
    const IMsvcMetaMakefile&            m_MsvcMetaMakefile;

    CLinkerToolImpl(void);
    CLinkerToolImpl(const CLinkerToolImpl&);
    CLinkerToolImpl& operator= (const CLinkerToolImpl&);
};


#define SUPPORT_DUMMY_OPTION(opt) \
    virtual string opt(void) const \
    { \
        return ""; \
    }


/////////////////////////////////////////////////////////////////////////////
///
/// CLinkerToolDummyImpl --
///
/// Implementation of ILinkerTool interface.
///
/// Dummy (name-only) implementation.
class CLinkerToolDummyImpl : public ILinkerTool // for LIB targets:
{
public:
    CLinkerToolDummyImpl()
    {
    }
    virtual string Name(void) const
    {
	    return "VCLinkerTool";
    }
    SUPPORT_DUMMY_OPTION(AdditionalDependencies)
    SUPPORT_DUMMY_OPTION(AdditionalOptions)
    SUPPORT_DUMMY_OPTION(OutputFile)
    SUPPORT_DUMMY_OPTION(LinkIncremental)
    SUPPORT_DUMMY_OPTION(LargeAddressAware)
    SUPPORT_DUMMY_OPTION(GenerateDebugInformation)
    SUPPORT_DUMMY_OPTION(ProgramDatabaseFile)
    SUPPORT_DUMMY_OPTION(SubSystem)
    SUPPORT_DUMMY_OPTION(ImportLibrary)
    SUPPORT_DUMMY_OPTION(TargetMachine)
    SUPPORT_DUMMY_OPTION(ImageHasSafeExceptionHandlers)
    SUPPORT_DUMMY_OPTION(OptimizeReferences)
    SUPPORT_DUMMY_OPTION(EnableCOMDATFolding)

    virtual string IgnoreAllDefaultLibraries(void) const
    {
        return "false";
    }
    virtual string IgnoreDefaultLibraryNames(void) const
    {
        return "";
    }

    SUPPORT_DUMMY_OPTION(AdditionalLibraryDirectories)
    SUPPORT_DUMMY_OPTION(FixedBaseAddress)
    SUPPORT_DUMMY_OPTION(GenerateManifest)
    SUPPORT_DUMMY_OPTION(EmbedManifest)

private:
    CLinkerToolDummyImpl(const CLinkerToolDummyImpl&);
    CLinkerToolDummyImpl& operator= (const CLinkerToolDummyImpl&);
};

/////////////////////////////////////////////////////////////////////////////
///
/// CLibrarianToolImpl --
///
/// Implementation of ILibrarianTool interface.
///
/// Implementation for LIB targets.
class CLibrarianToolImpl : public ILibrarianTool
{
public:
    CLibrarianToolImpl( const string&            project_id,
                        const IMsvcMetaMakefile& project_makefile,
                        const IMsvcMetaMakefile& meta_makefile,
                        const SConfigInfo&       config)
        :m_ProjectId            (project_id),
         m_Config               (config),
         m_MsvcProjectMakefile  (project_makefile),
         m_MsvcMetaMakefile     (meta_makefile)
    {
    }
    virtual string Name(void) const
    {
	    return "VCLibrarianTool";
    }

    virtual string OutputFile(void) const
    {
//	    return string("$(OutDir)") + m_ProjectId + ".lib";
        if (CMsvc7RegSettings::GetMsvcVersion() >= CMsvc7RegSettings::eMsvc1000) {
    	    return string("$(OutDir)$(TargetName)$(TargetExt)");
        }
	    return string("$(OutDir)$(ProjectName)");
    }

#define SUPPORT_LIBRARIAN_OPTION(opt) \
    virtual string opt(void) const \
    { \
        return GetLibrarianOpt(m_MsvcMetaMakefile, \
                            m_MsvcProjectMakefile, \
                            #opt, \
                            m_Config ); \
    }
    SUPPORT_LIBRARIAN_OPTION(AdditionalOptions)
    SUPPORT_LIBRARIAN_OPTION(AdditionalLibraryDirectories)
    SUPPORT_LIBRARIAN_OPTION(IgnoreAllDefaultLibraries)
    SUPPORT_LIBRARIAN_OPTION(IgnoreDefaultLibraryNames)
    SUPPORT_LIBRARIAN_OPTION(TargetMachine)

private:
    string      m_ProjectId;
    SConfigInfo m_Config;
   
    const IMsvcMetaMakefile& m_MsvcProjectMakefile;
    const IMsvcMetaMakefile& m_MsvcMetaMakefile;

    CLibrarianToolImpl(void);
    CLibrarianToolImpl(const CLibrarianToolImpl&);
    CLibrarianToolImpl& operator= (const CLibrarianToolImpl&);
};


/////////////////////////////////////////////////////////////////////////////
///
/// CLibrarianToolDummyImpl --
///
/// Implementation of ILibrarianTool interface.
///
/// Dummy (name-only) implementation for APP and DLL targets.

class CLibrarianToolDummyImpl : public ILibrarianTool // for APP and DLL
{
public:
    CLibrarianToolDummyImpl(void)
    {
    }

    virtual string Name(void) const
    {
	    return "VCLibrarianTool";
    }

    SUPPORT_DUMMY_OPTION(AdditionalOptions)
    SUPPORT_DUMMY_OPTION(OutputFile)
    SUPPORT_DUMMY_OPTION(IgnoreAllDefaultLibraries)
    SUPPORT_DUMMY_OPTION(IgnoreDefaultLibraryNames)
    SUPPORT_DUMMY_OPTION(AdditionalLibraryDirectories)
    SUPPORT_DUMMY_OPTION(TargetMachine)

private:
	CLibrarianToolDummyImpl(const CLibrarianToolDummyImpl&);
	CLibrarianToolDummyImpl& operator= (const CLibrarianToolDummyImpl&);
};


class CPreBuildEventToolDummyImpl : public IPreBuildEventTool // for APP and DLL
{
public:
    CPreBuildEventToolDummyImpl(void)
    {
    }

    virtual string Name(void) const
    {
	    return "VCPreBuildEventTool";
    }

    SUPPORT_DUMMY_OPTION(CommandLine)

private:
	CPreBuildEventToolDummyImpl(const CPreBuildEventToolDummyImpl&);
	CPreBuildEventToolDummyImpl& operator= (const CPreBuildEventToolDummyImpl&);
};

class CPreBuildEventTool : public IPreBuildEventTool
{
public:
    CPreBuildEventTool(const list<CProjKey>& lib_depends, EMakeFileType maketype)
        : m_LibDepends(lib_depends), m_MakeType(maketype)
    {
    }
    virtual string Name(void) const
    {
	    return "VCPreBuildEventTool";
    }
    virtual string CommandLine(void) const
    {
        string command_line;
        if (m_MakeType != eMakeType_Undefined) {
            string echo = MakeFileTypeAsString(m_MakeType);
            if (!echo.empty()) {
                command_line += "@echo " + echo + " project\n";
            }
        }
        if ( !m_LibDepends.empty() ) {
            const CProjectItemsTree* tree = GetApp().GetCurrentBuildTree();
            ITERATE(list<CProjKey>, p, m_LibDepends) {
                if (tree->m_Projects.find(*p) == tree->m_Projects.end()) {
                    command_line += "@echo ERROR: This project depends on missing " + CreateProjectName(*p) + "\n";
                    command_line += "exit 1\n";
                    break;
                }
            }
        }
        return command_line;
    }
protected:
    list<CProjKey> m_LibDepends;

private:    
    EMakeFileType m_MakeType;

	CPreBuildEventTool(const CPreBuildEventTool&);
	CPreBuildEventTool& operator= (const CPreBuildEventTool&);
};

class CPreBuildEventToolLibImpl : public CPreBuildEventTool // for LIB
{
public:
    CPreBuildEventToolLibImpl(const list<CProjKey>& lib_depends, EMakeFileType maketype)
        : CPreBuildEventTool(lib_depends, maketype)
    {
    }

    virtual string CommandLine(void) const
    {
        string command_line = CPreBuildEventTool::CommandLine();
        if (CMsvc7RegSettings::GetMsvcVersion() > CMsvc7RegSettings::eMsvc710) {
            return command_line;
        }
        string cmd;
        if ( !m_LibDepends.empty() ) {
            cmd = "\"";
            cmd += GetApp().GetProjectTreeRoot();
            cmd += "asn_prebuild.bat\"";
            cmd += " \"$(OutDir)\" \"$(ConfigurationName)\" \"$(SolutionPath)\"";
        }
        ITERATE(list<CProjKey>, p, m_LibDepends)
        {
            const string& lib = CreateProjectName(*p);
            cmd += " ";
            cmd += lib;
        }
        if (!cmd.empty()) {
            command_line += "@echo " + cmd + "\n" + cmd;
        }
        return command_line;
    }

private:
	CPreBuildEventToolLibImpl(const CPreBuildEventToolLibImpl&);
	CPreBuildEventToolLibImpl& operator= (const CPreBuildEventToolLibImpl&);
};


/// Dummy (name-only) tool implementations.

#define DEFINE_NAME_ONLY_DUMMY_TOOL(C,I,N)\
class C : public I\
{\
public:\
    C()\
    {\
    }\
    virtual string Name(void) const\
    {\
	    return N;\
    }\
private:\
    C(const C&);\
    C& operator= (const C&);\
}

DEFINE_NAME_ONLY_DUMMY_TOOL(CCustomBuildToolDummyImpl,
                            ICustomBuildTool, 
                            "VCCustomBuildTool"); 

DEFINE_NAME_ONLY_DUMMY_TOOL(CMIDLToolDummyImpl, 
                            IMIDLTool, 
                            "VCMIDLTool"); 

DEFINE_NAME_ONLY_DUMMY_TOOL(CPostBuildEventToolDummyImpl,
                            IPostBuildEventTool, 
                            "VCPostBuildEventTool"); 
#if 0
DEFINE_NAME_ONLY_DUMMY_TOOL(CPreBuildEventToolDummyImpl,
                            IPreBuildEventTool, 
                            "VCPreBuildEventTool"); 
#endif

DEFINE_NAME_ONLY_DUMMY_TOOL(CPreLinkEventToolDummyImpl,
                            IPreLinkEventTool, 
                            "VCPreLinkEventTool");


/////////////////////////////////////////////////////////////////////////////
///
/// CResourceCompilerToolImpl --
///
/// Implementation of IResourceCompilerTool interface.
///
/// Accepts traits as a template parameter.

template <class DebugReleaseTrait>
class CResourceCompilerToolImpl : public IResourceCompilerTool
{
public:
    CResourceCompilerToolImpl(const string&            additional_include_dirs,
                              const IMsvcMetaMakefile& project_makefile,
                              const IMsvcMetaMakefile& meta_makefile,
                              const SConfigInfo&       config)
      :m_AdditionalIncludeDirectories(additional_include_dirs),
       m_MsvcProjectMakefile(project_makefile),
       m_MsvcMetaMakefile   (meta_makefile),
       m_Config             (config)
    {
    }
    virtual string Name(void) const
    {
	    return "VCResourceCompilerTool";
    }

    virtual string AdditionalIncludeDirectories(void) const
    {
	    return m_AdditionalIncludeDirectories;
    }

#define SUPPORT_RESOURCE_COMPILER_OPTION(opt) \
    virtual string opt(void) const \
    { \
        return GetResourceCompilerOpt(m_MsvcMetaMakefile, \
                                      m_MsvcProjectMakefile, \
                                      #opt, \
                                      m_Config ); \
    }
    
    SUPPORT_RESOURCE_COMPILER_OPTION(AdditionalOptions)
    SUPPORT_RESOURCE_COMPILER_OPTION(Culture)


    virtual string PreprocessorDefinitions(void) const
    {
	    return DebugReleaseTrait::PreprocessorDefinitions();
    }

private:
    string                   m_AdditionalIncludeDirectories;
    const IMsvcMetaMakefile& m_MsvcProjectMakefile;
    const IMsvcMetaMakefile& m_MsvcMetaMakefile;
    const SConfigInfo&       m_Config;

    CResourceCompilerToolImpl(const CResourceCompilerToolImpl&);
    CResourceCompilerToolImpl& operator= (const CResourceCompilerToolImpl&);

};


/////////////////////////////////////////////////////////////////////////////
///
/// CResourceCompilerToolImpl --
///
/// Implementation of IResourceCompilerTool interface.
///
/// Dummy (name-only) implementation.

class CResourceCompilerToolDummyImpl : public IResourceCompilerTool //no resources
{
public:
    CResourceCompilerToolDummyImpl()
    {
    }
    virtual string Name(void) const
    {
        return "VCResourceCompilerTool";
    }

    SUPPORT_DUMMY_OPTION(AdditionalIncludeDirectories)
    SUPPORT_DUMMY_OPTION(AdditionalOptions)
    SUPPORT_DUMMY_OPTION(Culture)
    SUPPORT_DUMMY_OPTION(PreprocessorDefinitions)

private:
    CResourceCompilerToolDummyImpl
        (const CResourceCompilerToolDummyImpl&);
    CResourceCompilerToolDummyImpl& operator= 
        (const CResourceCompilerToolDummyImpl&);
};


/// Dummy (name-only) tool implementations.

DEFINE_NAME_ONLY_DUMMY_TOOL(CWebServiceProxyGeneratorToolDummyImpl,
                            IWebServiceProxyGeneratorTool, 
                            "VCWebServiceProxyGeneratorTool");

DEFINE_NAME_ONLY_DUMMY_TOOL(CXMLDataGeneratorToolDummyImpl,
                            IXMLDataGeneratorTool, 
                            "VCXMLDataGeneratorTool");

DEFINE_NAME_ONLY_DUMMY_TOOL(CManagedWrapperGeneratorToolDummyImpl,
                            IManagedWrapperGeneratorTool, 
                            "VCManagedWrapperGeneratorTool");

DEFINE_NAME_ONLY_DUMMY_TOOL(CAuxiliaryManagedWrapperGeneratorToolDummyImpl,
                            IAuxiliaryManagedWrapperGeneratorTool, 
                            "VCAuxiliaryManagedWrapperGeneratorTool");



END_NCBI_SCOPE

#endif //PROJECT_TREE_BUILDER__TOOLS_IMPLEMENT__HPP
