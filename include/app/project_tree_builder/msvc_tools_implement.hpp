#ifndef TOOLS_IMPLEMENT_HEADER
#define TOOLS_IMPLEMENT_HEADER

#include <string>

#include <app/project_tree_builder/msvc_project_context.hpp>
#include <app/project_tree_builder/msvc_traits.hpp>

#include <corelib/ncbienv.hpp>

BEGIN_NCBI_SCOPE
//------------------------------------------------------------------------------


template <class ConfTrait> 
class CConfigurationImpl : public IConfiguration
{
public:
    CConfigurationImpl(	const string& output_directory, 
                        const string& configuration_name )
                        :m_OutputDirectory  (output_directory),
                         m_ConfigurationName(configuration_name)
    {
    }

    virtual string Name(void) const
    {
	    return m_ConfigurationName + '|' + "Win32";
    }
    virtual string OutputDirectory(void) const
    {
	    return m_OutputDirectory;
    }
    virtual string IntermediateDirectory(void) const
    {
	    return m_ConfigurationName;
    }
    virtual string ConfigurationType(void) const
    {
	    return ConfTrait::ConfigurationType();
    }
    virtual string CharacterSet(void) const
    {
	    return "2";
    }


private:
    string m_OutputDirectory;
    string m_ConfigurationName;

    CConfigurationImpl(void);
    CConfigurationImpl(const CConfigurationImpl&);
    CConfigurationImpl& operator = (const CConfigurationImpl&);
};

template <  class CRTTrait, 
            class DebugReleaseTrait,
            class ConfTrait > 
class CCompilerToolImpl : public ICompilerTool
{
public:
    CCompilerToolImpl( const string& additional_include_directories )
	    :m_AdditionalIncludeDirectories(additional_include_directories)
    {
    }

    virtual string Name(void) const
    {
	    return "VCCLCompilerTool";
    }
    virtual string Optimization(void) const
    {
	    return DebugReleaseTrait::Optimization();
    }
    virtual string AdditionalIncludeDirectories(void) const
    {
	    return m_AdditionalIncludeDirectories;
    }
    virtual string PreprocessorDefinitions(void) const
    {
	    return DebugReleaseTrait::PreprocessorDefinitions() + 
		       ConfTrait::PreprocessorDefinitions();
    }
    virtual string MinimalRebuild(void) const
    {
	    return "FALSE";
    }
    virtual string BasicRuntimeChecks(void) const
    {
	    return DebugReleaseTrait::BasicRuntimeChecks();
    }
    virtual string RuntimeLibrary(void) const
    {
	    return CRTTrait::RuntimeLibrary();
    }
    virtual string RuntimeTypeInfo(void) const
    {
        return "TRUE";
    }
    virtual string UsePrecompiledHeader(void) const
    {
	    return "0";
    }
    virtual string WarningLevel(void) const
    {
	    return "3";
    }
    virtual string Detect64BitPortabilityProblems(void) const
    {
	    return "FALSE";
    }
    virtual string DebugInformationFormat(void) const
    {
	    return DebugReleaseTrait::DebugInformationFormat();
    }
    virtual string CompileAs(void) const
    {
        return "0";
    }

    virtual string InlineFunctionExpansion(void) const
    {
	    return DebugReleaseTrait::InlineFunctionExpansion();
    }
    virtual string OmitFramePointers(void) const
    {
	    return DebugReleaseTrait::OmitFramePointers();
    }
    virtual string StringPooling(void) const
    {
	    return DebugReleaseTrait::StringPooling();
    }
    virtual string EnableFunctionLevelLinking(void) const
    {
	    return DebugReleaseTrait::EnableFunctionLevelLinking();
    }

private:
    string m_AdditionalIncludeDirectories;

    CCompilerToolImpl(void);
    CCompilerToolImpl(const CCompilerToolImpl&);
    CCompilerToolImpl& operator = (const CCompilerToolImpl&);
};

template <  class DebugReleaseTrait,
            class ConfTrait > 
class CLinkerToolImpl : public ILinkerTool
{
public:
    CLinkerToolImpl(const string& additional_options,
                    const string& project_name)
	    :m_AdditionalOptions(additional_options),
		    m_ProjectName(project_name)
    {
    }
    virtual string Name(void) const
    {
	    return "VCLinkerTool";
    }
    virtual string AdditionalOptions(void) const
    {
	    return m_AdditionalOptions;
    }
    virtual string OutputFile(void) const
    {
	    return string("$(OutDir)/") + m_ProjectName + ConfTrait::TargetExtension();
    }
    virtual string LinkIncremental(void) const
    {
	    return "2";
    }
    virtual string GenerateDebugInformation(void) const
    {
	    return DebugReleaseTrait::GenerateDebugInformation();
    }
    virtual string ProgramDatabaseFile(void) const
    {
	    return string("$(OutDir)/") + m_ProjectName + ".pdb";
    }
    virtual string SubSystem(void) const
    {
	    return ConfTrait::SubSystem();
    }
    virtual string ImportLibrary(void) const
    {
	    return string("$(OutDir)/") + m_ProjectName + ".lib";
    }
    virtual string TargetMachine(void) const
    {
	    return "1";
    }
    virtual string OptimizeReferences(void) const
    {
	    return DebugReleaseTrait::OptimizeReferences();
    }
    virtual string EnableCOMDATFolding(void) const
    {
	    return DebugReleaseTrait::EnableCOMDATFolding();
    }

private:
    string m_AdditionalOptions;
    string m_ProjectName;

    CLinkerToolImpl(void);
    CLinkerToolImpl(const CLinkerToolImpl&);
    CLinkerToolImpl& operator = (const CLinkerToolImpl&);
};

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
    virtual string AdditionalOptions(void) const
    {
	    return "";
    }
    virtual string OutputFile(void) const
    {
	    return "";
    }
    virtual string LinkIncremental(void) const
    {
	    return "";
    }
    virtual string GenerateDebugInformation(void) const
    {
	    return "";
    }
    virtual string ProgramDatabaseFile(void) const
    {
	    return "";
    }
    virtual string SubSystem(void) const
    {
	    return "";
    }
    virtual string ImportLibrary(void) const
    {
	    return "";
    }
    virtual string TargetMachine(void) const
    {
	    return "";
    }
    virtual string OptimizeReferences(void) const
    {
	    return "";
    }
    virtual string EnableCOMDATFolding(void) const
    {
	    return "";
    }

private:
    CLinkerToolDummyImpl(const CLinkerToolDummyImpl&);
    CLinkerToolDummyImpl& operator = (const CLinkerToolDummyImpl&);
};

//for LIB targets
class CLibrarianToolImpl : public ILibrarianTool
{
public:
    CLibrarianToolImpl( const string& additional_options,
                        const string& project_name)
	    :m_AdditionalOptions(additional_options),
	     m_ProjectName(project_name)
    {
    }
    virtual string Name(void) const
    {
	    return "VCLibrarianTool";
    }

    virtual string AdditionalOptions(void) const
    {
	    return m_AdditionalOptions;
    }
    virtual string OutputFile(void) const
    {
	    return string("$(OutDir)/") + m_ProjectName + ".lib";
    }
    virtual string IgnoreAllDefaultLibraries(void) const
    {
	    return "TRUE";
    }
    virtual string IgnoreDefaultLibraryNames(void) const
    {
	    return "";
    }
private:
    string m_AdditionalOptions;
    string m_ProjectName;

    CLibrarianToolImpl(void);
    CLibrarianToolImpl(const CLibrarianToolImpl&);
    CLibrarianToolImpl& operator = (const CLibrarianToolImpl&);
};

class CLibrarianToolDummyImpl : public ILibrarianTool // for APP and DLL
{
public:
    CLibrarianToolDummyImpl()
    {
    }

    virtual string Name(void) const
    {
	    return "VCLibrarianTool";
    }

    virtual string AdditionalOptions(void) const
    {
	    return "";
    }
    virtual string OutputFile(void) const
    {
	    return "";
    }
    virtual string IgnoreAllDefaultLibraries(void) const
    {
	    return "";
    }
    virtual string IgnoreDefaultLibraryNames(void) const
    {
	    return "";
    }
private:
	CLibrarianToolDummyImpl(const CLibrarianToolDummyImpl&);
	CLibrarianToolDummyImpl& operator = (const CLibrarianToolDummyImpl&);
};

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
    C& operator = (const C&);\
};

DEFINE_NAME_ONLY_DUMMY_TOOL(CCustomBuildToolDummyImpl,
                            ICustomBuildTool, 
                            "VCCustomBuildTool"); 

DEFINE_NAME_ONLY_DUMMY_TOOL(CMIDLToolDummyImpl, 
                            IMIDLTool, 
                            "VCMIDLTool"); 

DEFINE_NAME_ONLY_DUMMY_TOOL(CPostBuildEventToolDummyImpl,
                            IPostBuildEventTool, 
                            "VCPostBuildEventTool"); 

DEFINE_NAME_ONLY_DUMMY_TOOL(CPreBuildEventToolDummyImpl,
                            IPreBuildEventTool, 
                            "VCPreBuildEventTool"); 

DEFINE_NAME_ONLY_DUMMY_TOOL(CPreLinkEventToolDummyImpl,
                            IPreLinkEventTool, 
                            "VCPreLinkEventTool");

template <class DebugReleaseTrait>
class CResourceCompilerToolImpl : public IResourceCompilerTool
{
public:
    CResourceCompilerToolImpl()
    {
    }
    virtual string Name(void) const
    {
	    return "VCResourceCompilerTool";
    }
    virtual string PreprocessorDefinitions(void) const
    {
	    return DebugReleaseTrait::PreprocessorDefinitions();
    }
    virtual string Culture(void) const
    {
	    return "1033";
    }
    virtual string AdditionalIncludeDirectories(void) const
    {
	    return "$(IntDir)";
    }

private:
    CResourceCompilerToolImpl(const CResourceCompilerToolImpl&);
    CResourceCompilerToolImpl& operator = (const CResourceCompilerToolImpl&);

};
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
    virtual string PreprocessorDefinitions(void) const
    {
	    return "";
    }
    virtual string Culture(void) const
    {
        return "";
    }
    virtual string AdditionalIncludeDirectories(void) const
    {
        return "";
    }

private:
    CResourceCompilerToolDummyImpl(
                                const CResourceCompilerToolDummyImpl&);
    CResourceCompilerToolDummyImpl& operator = (
                                const CResourceCompilerToolDummyImpl&);
};

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


//------------------------------------------------------------------------------
END_NCBI_SCOPE

#endif // TOOLS_IMPLEMENT_HEADER
