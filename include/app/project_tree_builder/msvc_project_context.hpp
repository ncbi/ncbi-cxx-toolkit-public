#ifndef PROJECT_CONTEXT_HEADER
#define PROJECT_CONTEXT_HEADER

#include<list>
#include<string>
#include<map>

#include <app/project_tree_builder/proj_item.hpp>

#include <corelib/ncbienv.hpp>
USING_NCBI_SCOPE;


//------------------------------------------------------------------------------

class CMsvcPrjProjectContext
{
public:
    //no value type	semantics
    CMsvcPrjProjectContext(const CProjItem& project);

    //Compiler::General
    string AdditionalIncludeDirectories(void) const
    {
        return m_AdditionalIncludeDirectories;
    }

    //Linker::General
    string ProjectName(void) const
    {
        return m_ProjectName;
    }

    string AdditionalLinkerOptions(void) const
    {
        return m_AdditionalLinkerOptions;
    }


    string AdditionalLibrarianOptions(void) const
    {
        return m_AdditionalLibrarianOptions;
    }

    string ProjectDir(void) const
    {
        return m_ProjectDir;
    }

    CProjItem::TProjType ProjectType(void) const
    {
        return m_ProjType;
    }

    const list<string>& SourcesDirsAbs(void) const
    {
        return m_SourcesDirsAbs;
    }
    const string& IncludeDirsRoot(void) const
    {
        return m_IncludeDirsRoot;
    }
    const list<string>& IncludeDirsAbs(void) const
    {
        return m_IncludeDirsAbs;
    }

private:
    CMsvcPrjProjectContext(void);
    CMsvcPrjProjectContext(const CMsvcPrjProjectContext&);
    CMsvcPrjProjectContext&	operator = (const CMsvcPrjProjectContext&);

    string m_ProjectName;
    string m_AdditionalIncludeDirectories;
    string m_AdditionalLinkerOptions;
    string m_AdditionalLibrarianOptions;

    string m_ProjectDir;

    CProjItem::TProjType m_ProjType;

    list<string> m_SourcesDirsAbs;

    string       m_IncludeDirsRoot;
    list<string> m_IncludeDirsAbs;
};


//------------------------------------------------------------------------------


class CMsvcPrjGeneralContext
{
public:
    //no value type semantics
    CMsvcPrjGeneralContext( const string& config, 
                            const CMsvcPrjProjectContext& prj_context );

    typedef enum { 
        eExe, 
        eLib, 
        eDll, 
        eOther } TTargetType;
    TTargetType m_Type;

    typedef enum { 
        eStatic, 
        eStaticMT, 
        eDynamicMT } TRunTimeLibType;
    TRunTimeLibType m_RunTime;

    typedef enum { 
        eDebug, 
        eRelease } TBuildType;
    TBuildType m_BuildType;

    //Configuration::General
    string OutputDirectory(void) const
    {
        return m_OutputDirectory;
    }


    string ConfigurationName(void) const
    {
        return m_ConfigurationName;
    }


//	string GetOutputDirSuffix(void) const; // lib or bin

private:
    CMsvcPrjGeneralContext(void);
    CMsvcPrjGeneralContext(const CMsvcPrjGeneralContext&);
    CMsvcPrjGeneralContext& operator = (const CMsvcPrjGeneralContext&);

    string m_OutputDirectory;
    string m_ConfigurationName;
};

//------------------------------------------------------------------------------

struct ITool
{
    virtual string Name(void) const = 0;

    virtual ~ITool(void)
    {
    }
};
struct IConfiguration : public ITool
{
    //string Name;// ="Debug|Win32" - Configuration+'|'+"Win32"
    virtual string OutputDirectory(void)		const = 0;// ="../../Binaries/Debug"
    virtual string IntermediateDirectory(void)	const = 0;//="Debug"
    virtual string ConfigurationType(void)		const = 0;//="2"
    virtual string CharacterSet(void)			const = 0;//="2">
};
struct ICompilerTool : public ITool
{
    //debug - EXE DLL LIB
    //string Name;// ="VCCLCompilerTool"
    virtual string Optimization(void)					const = 0;// ="0"
    virtual string AdditionalIncludeDirectories(void)	const = 0;// ="../aaa,"

    // Preprocessor definition:
    // LIB (Debug)		WIN32;_DEBUG;_LIB
    // LIB (Release)	WIN32;NDEBUG;_LIB
    // APP (Debug)		WIN32;_DEBUG;_CONSOLE
    // APP (Release)	WIN32;NDEBUG;_CONSOLE
    // DLL (Debug)		WIN32;_DEBUG;_WINDOWS;_USRDLL;IR2COREIMAGELIB_EXPORTS 
    //                                                (project_name(uppercase)+"_EXPORTS")

    // DLL (Relese)		WIN32;NDEBUG;_WINDOWS;_USRDLL;IR2COREIMAGELIB_EXPORTS
    //                                                (project_name(uppercase)+"_EXPORTS")

    virtual string PreprocessorDefinitions(void)		const = 0;

    virtual string MinimalRebuild(void)					const = 0;// ="TRUE"
    virtual string BasicRuntimeChecks(void)				const = 0;// ="3"
    virtual string RuntimeLibrary(void)					const = 0;// ="3"
    virtual string RuntimeTypeInfo(void)                const = 0;// ="TRUE"
    virtual string UsePrecompiledHeader(void)			const = 0;// ="0"pchNone
    virtual string WarningLevel(void)					const = 0;// ="3"
    virtual string Detect64BitPortabilityProblems(void) const = 0;// ="FALSE"
    virtual string DebugInformationFormat(void)			const = 0;// ="4"
    virtual string CompileAs(void)                      const = 0;// ="0"Default

    //release - EXE DLL LIB
    virtual string InlineFunctionExpansion(void)		const = 0;// ="1"
    virtual string OmitFramePointers(void)				const = 0;// ="TRUE"
    virtual string StringPooling(void)					const = 0;// ="TRUE"
    virtual string EnableFunctionLevelLinking(void)		const = 0;// ="TRUE"
};
struct ILinkerTool : public ITool
{
    //debug - EXE DLL
    //string Name;// ="VCLinkerTool"
    virtual string AdditionalOptions(void)			const = 0;// ="Ws2_32.lib ../../binaries/Debug/PSOAP_INET.lib"
    virtual string OutputFile(void)					const = 0;// ="$(OutDir)/PSOAP_SRV.dll"
    virtual string LinkIncremental(void)			const = 0;// ="2"
    virtual string GenerateDebugInformation(void)	const = 0;// ="TRUE"
    virtual string ProgramDatabaseFile(void)		const = 0;// ="$(OutDir)/PSOAP_SRV.pdb"
    virtual string SubSystem(void)					const = 0;// ="2" // VG our case - "1" (console)
    virtual string ImportLibrary(void)				const = 0;// ="$(OutDir)/PSOAP_SRV.lib"
    virtual string TargetMachine(void)				const = 0;// ="1"/> // VG : 1-machineX86

    //release - EXE DLL +=
    virtual string OptimizeReferences(void)			const = 0;// ="2"
    virtual string EnableCOMDATFolding(void)		const = 0;// ="2"
};
struct ILibrarianTool : public ITool
{
    //debug and release - LIB
    //string Name;// ="VCLibrarianTool"
    virtual string AdditionalOptions(void)			const = 0;// ="/OUT:&quot;../Binaries/Debug/TIFF_LIB.lib&quot; /NOLOGO"
    virtual string OutputFile(void)					const = 0;// ="$(OutDir)/TIFF_LIB.lib"
    virtual string IgnoreAllDefaultLibraries(void)	const = 0;// ="TRUE"
    virtual string IgnoreDefaultLibraryNames(void)	const = 0;// =""/>
};

//dummies:
struct ICustomBuildTool : public ITool
{
    //string Name="VCCustomBuildTool"
};
struct IMIDLTool : public ITool
{
    //string Name="VCMIDLTool"
};
struct IPostBuildEventTool : public ITool
{
    //string Name="VCPostBuildEventTool"
};
struct IPreBuildEventTool : public ITool
{
    //string Name="VCPreBuildEventTool"
};
struct IPreLinkEventTool : public ITool
{
    //string Name="VCPreLinkEventTool"
};


//not alway a dummy
struct IResourceCompilerTool : public ITool
{
    //string Name="VCResourceCompilerTool"
    //(LIB, APP, DLL (Debug)  : =_DEBUG
    //(LIB, APP, DLL (Release): =NDEBUG
    virtual string PreprocessorDefinitions(void)		const = 0;//="_DEBUG" // or "NDEBUG"
    virtual string Culture(void)						const = 0;//="1033"
    virtual string AdditionalIncludeDirectories(void)	const = 0;//="$(IntDir)"/>
};


//more dummies:
struct IWebServiceProxyGeneratorTool : public ITool
{
    //string Name="VCWebServiceProxyGeneratorTool"
};
struct IXMLDataGeneratorTool : public ITool
{
    //string Name="VCXMLDataGeneratorTool"
};
struct IManagedWrapperGeneratorTool : public ITool
{
    //string Name="VCManagedWrapperGeneratorTool"
};
struct IAuxiliaryManagedWrapperGeneratorTool : public ITool
{
    //string Name="VCAuxiliaryManagedWrapperGeneratorTool"
};


//------------------------------------------------------------------------------


class CMsvcTools
{
public:
    CMsvcTools(	const CMsvcPrjGeneralContext& general_context,
                const CMsvcPrjProjectContext& project_context );

    ~CMsvcTools();

    IConfiguration      * Configuration	(void) const;
    ICompilerTool       * Compiler      (void) const;
    ILinkerTool         * Linker        (void) const;
    ILibrarianTool      * Librarian     (void) const;
    ICustomBuildTool	* CustomBuid    (void) const;
    IMIDLTool			* MIDL		    (void) const;
    IPostBuildEventTool * PostBuildEvent(void) const;
    IPreBuildEventTool	* PreBuildEvent (void) const;
    IPreLinkEventTool	* PreLinkEvent  (void) const;
    
    IResourceCompilerTool *
                          ResourceCompiler          (void) const;

    IWebServiceProxyGeneratorTool *
                          WebServiceProxyGenerator	(void) const;

    IXMLDataGeneratorTool *
                          XMLDataGenerator          (void) const;

    IManagedWrapperGeneratorTool *
                          ManagedWrapperGenerator	(void) const;

    IAuxiliaryManagedWrapperGeneratorTool *
                          AuxiliaryManagedWrapperGenerator(void) const;

private:
    auto_ptr<IConfiguration>        m_Configuration;
    auto_ptr<ICompilerTool>         m_Compiler;
    auto_ptr<ILinkerTool>           m_Linker;
    auto_ptr<ILibrarianTool>        m_Librarian;
    
    auto_ptr<ICustomBuildTool>      m_CustomBuid;
    auto_ptr<IMIDLTool>             m_MIDL;
    auto_ptr<IPostBuildEventTool>	m_PostBuildEvent;
    auto_ptr<IPreBuildEventTool>	m_PreBuildEvent;
    auto_ptr<IPreLinkEventTool>		m_PreLinkEvent;
    
    auto_ptr<IResourceCompilerTool>	m_ResourceCompiler;

    auto_ptr<IWebServiceProxyGeneratorTool>	
                                    m_WebServiceProxyGenerator;
    
    auto_ptr<IXMLDataGeneratorTool>	m_XMLDataGenerator;
    
    auto_ptr<IManagedWrapperGeneratorTool>	
                                    m_ManagedWrapperGenerator;
    
    auto_ptr<IAuxiliaryManagedWrapperGeneratorTool> 
                                    m_AuxiliaryManagedWrapperGenerator;

    //this is not a value-type class
    CMsvcTools();
    CMsvcTools(const CMsvcTools&);
    CMsvcTools& operator = (const CMsvcTools&);
};




#endif // PROJECT_CONTEXT_HEADER