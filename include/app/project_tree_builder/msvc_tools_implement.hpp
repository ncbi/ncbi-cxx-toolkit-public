#ifndef TOOLS_IMPLEMENT_HEADER
#define TOOLS_IMPLEMENT_HEADER

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

#include <app/project_tree_builder/msvc_project_context.hpp>
#include <app/project_tree_builder/msvc_traits.hpp>

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
    CConfigurationImpl& operator= (const CConfigurationImpl&);
};

/////////////////////////////////////////////////////////////////////////////
///
/// CCompilerToolImpl --
///
/// Implementation of ICompilerTool interface.
///
/// Accepts trait classes as a template parameters.

template < class DebugReleaseTrait > 
class CCompilerToolImpl : public ICompilerTool
{
public:
    CCompilerToolImpl(const string&               additional_include_dirs,
                      const CMsvcProjectMakefile& project_makefile,
                      const string&               runtimeLibraryOption,
                      const CMsvcMetaMakefile&    meta_makefile)
	    :m_AdditionalIncludeDirectories(additional_include_dirs),
         m_MsvcProjectMakefile         (project_makefile),
         m_RuntimeLibraryOption        (runtimeLibraryOption),
         m_MsvcMetaMakefile            (meta_makefile)
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
                              DebugReleaseTrait::debug() ); \
    }

    SUPPORT_COMPILER_OPTION(Optimization)

    virtual string AdditionalIncludeDirectories(void) const
    {
	    return m_AdditionalIncludeDirectories;
    }

    SUPPORT_COMPILER_OPTION(PreprocessorDefinitions)
    SUPPORT_COMPILER_OPTION(MinimalRebuild)
    SUPPORT_COMPILER_OPTION(BasicRuntimeChecks)

    virtual string RuntimeLibrary(void) const
    {
	    return m_RuntimeLibraryOption;
    }

    SUPPORT_COMPILER_OPTION(RuntimeTypeInfo)
    SUPPORT_COMPILER_OPTION(UsePrecompiledHeader)
    SUPPORT_COMPILER_OPTION(WarningLevel)
    SUPPORT_COMPILER_OPTION(Detect64BitPortabilityProblems)
    SUPPORT_COMPILER_OPTION(DebugInformationFormat)
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

private:
    string                      m_AdditionalIncludeDirectories;
    const CMsvcProjectMakefile& m_MsvcProjectMakefile;
    string                      m_RuntimeLibraryOption;
    const CMsvcMetaMakefile&    m_MsvcMetaMakefile;

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

template <  class DebugReleaseTrait,
            class ConfTrait > 
class CLinkerToolImpl : public ILinkerTool
{
public:
    CLinkerToolImpl(const string& additional_options,
                    const string& project_name,
                    const CMsvcProjectMakefile& project_makefile,
                    const CMsvcMetaMakefile&    meta_makefile)
	    :m_AdditionalOptions    (additional_options),
		 m_ProjectName          (project_name),
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
	    return m_AdditionalOptions;
    }
    virtual string OutputFile(void) const
    {
	    return string("$(OutDir)/") + m_ProjectName + ConfTrait::TargetExtension();
    }

#define SUPPORT_LINKER_OPTION(opt) \
    virtual string opt(void) const \
    { \
        return GetLinkerOpt(m_MsvcMetaMakefile, \
                            m_MsvcProjectMakefile, \
                            #opt, \
                            DebugReleaseTrait::debug() ); \
    }
    
    SUPPORT_LINKER_OPTION(LinkIncremental)
    SUPPORT_LINKER_OPTION(GenerateDebugInformation)

    virtual string ProgramDatabaseFile(void) const
    {
	    return string("$(OutDir)/") + m_ProjectName + ".pdb";
    }

    SUPPORT_LINKER_OPTION(SubSystem)
    
    virtual string ImportLibrary(void) const
    {
	    return string("$(OutDir)/") + m_ProjectName + ".lib";
    }

    SUPPORT_LINKER_OPTION(TargetMachine)
    SUPPORT_LINKER_OPTION(OptimizeReferences)
    SUPPORT_LINKER_OPTION(EnableCOMDATFolding)
    SUPPORT_LINKER_OPTION(IgnoreAllDefaultLibraries)
    SUPPORT_LINKER_OPTION(IgnoreDefaultLibraryNames)
    SUPPORT_LINKER_OPTION(AdditionalLibraryDirectories)

private:
    string m_AdditionalOptions;
    string m_ProjectName;

    const CMsvcProjectMakefile& m_MsvcProjectMakefile;
    const CMsvcMetaMakefile&    m_MsvcMetaMakefile;

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
    SUPPORT_DUMMY_OPTION(AdditionalOptions)
    SUPPORT_DUMMY_OPTION(OutputFile)
    SUPPORT_DUMMY_OPTION(LinkIncremental)
    SUPPORT_DUMMY_OPTION(GenerateDebugInformation)
    SUPPORT_DUMMY_OPTION(ProgramDatabaseFile)
    SUPPORT_DUMMY_OPTION(SubSystem)
    SUPPORT_DUMMY_OPTION(ImportLibrary)
    SUPPORT_DUMMY_OPTION(TargetMachine)
    SUPPORT_DUMMY_OPTION(OptimizeReferences)
    SUPPORT_DUMMY_OPTION(EnableCOMDATFolding)

    virtual string IgnoreAllDefaultLibraries(void) const
    {
        return "FALSE";
    }
    virtual string IgnoreDefaultLibraryNames(void) const
    {
        return "FALSE";
    }

    SUPPORT_DUMMY_OPTION(AdditionalLibraryDirectories)

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

template <  class DebugReleaseTrait > 
class CLibrarianToolImpl : public ILibrarianTool
{
public:
    CLibrarianToolImpl( const string& additional_options,
                        const string& project_name,
                        const CMsvcProjectMakefile& project_makefile,
                        const CMsvcMetaMakefile&    meta_makefile)
	    :m_AdditionalOptions    (additional_options),
	     m_ProjectName          (project_name),
         m_MsvcProjectMakefile  (project_makefile),
         m_MsvcMetaMakefile     (meta_makefile)
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

#define SUPPORT_LIBRARIAN_OPTION(opt) \
    virtual string opt(void) const \
    { \
        return GetLinkerOpt(m_MsvcMetaMakefile, \
                            m_MsvcProjectMakefile, \
                            #opt, \
                            DebugReleaseTrait::debug() ); \
    }
    
    SUPPORT_LIBRARIAN_OPTION(IgnoreAllDefaultLibraries)
    SUPPORT_LIBRARIAN_OPTION(IgnoreDefaultLibraryNames)
    SUPPORT_LIBRARIAN_OPTION(AdditionalLibraryDirectories)

private:
    string m_AdditionalOptions;
    string m_ProjectName;
   
    const CMsvcProjectMakefile& m_MsvcProjectMakefile;
    const CMsvcMetaMakefile&    m_MsvcMetaMakefile;

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
    CLibrarianToolDummyImpl()
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

private:
	CLibrarianToolDummyImpl(const CLibrarianToolDummyImpl&);
	CLibrarianToolDummyImpl& operator= (const CLibrarianToolDummyImpl&);
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

    SUPPORT_DUMMY_OPTION(PreprocessorDefinitions)
    SUPPORT_DUMMY_OPTION(Culture)
    SUPPORT_DUMMY_OPTION(AdditionalIncludeDirectories)

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

/*
 * ===========================================================================
 * $Log$
 * Revision 1.4  2004/01/26 19:25:42  gorelenk
 * += MSVC meta makefile support
 * += MSVC project makefile support
 *
 * Revision 1.3  2004/01/22 17:57:09  gorelenk
 * first version
 *
 * ===========================================================================
 */

#endif // TOOLS_IMPLEMENT_HEADER
