#include <app/project_tree_builder/stl_msvc_usage.hpp>

#include <app/project_tree_builder/msvc_project_context.hpp>
#include <app/project_tree_builder/msvc_tools_implement.hpp>
#include <app/project_tree_builder/proj_builder_app.hpp>

#include <set>


BEGIN_NCBI_SCOPE
//------------------------------------------------------------------------------

CMsvcPrjProjectContext::CMsvcPrjProjectContext(const CProjItem& project)
{
    //
    m_ProjectName = project.m_ID;

    // Collect all dirs of source files into m_SourcesDirsAbs:
    set<string> sources_dirs;
    sources_dirs.insert(project.m_SourcesBaseDir);
    ITERATE(list<string>, p, project.m_Sources) {
        
        string dir;
        CDirEntry::SplitPath(*p, &dir);
        sources_dirs.insert(dir);
    }
    copy(sources_dirs.begin(), 
         sources_dirs.end(), 
         back_inserter(m_SourcesDirsAbs));

    // /src/
    const string src_tag(string(1,CDirEntry::GetPathSeparator()) + 
                         "src" +
                         CDirEntry::GetPathSeparator());
 
    // /include/
    const string include_tag(string(1,CDirEntry::GetPathSeparator()) + 
                             "include" +
                             CDirEntry::GetPathSeparator());


    // /compilers/msvc7_prj/
    const string project_tag(string(1,CDirEntry::GetPathSeparator()) + 
                             "compilers" +
                             CDirEntry::GetPathSeparator() + 
                             "msvc7_prj" + 
                             CDirEntry::GetPathSeparator()); 


    // Creating project dir:
    m_ProjectDir = project.m_SourcesBaseDir;
    m_ProjectDir.replace(m_ProjectDir.find(src_tag), 
                         src_tag.length(), 
                         project_tag);

    //TODO 
    m_AdditionalLinkerOptions = "";
    //TODO 
    m_AdditionalLibrarianOptions = "";

    m_ProjType = project.m_ProjType;

     // Generate include dirs:
    //Root for all include dirs:
    string incl_dir = string(project.m_SourcesBaseDir, 0, 
                             project.m_SourcesBaseDir.find(src_tag));
    m_IncludeDirsRoot = CDirEntry::ConcatPath(incl_dir, "include");

#if 0
    ITERATE(list<string>, p, m_SourcesDirsAbs) {

        size_t tag_pos =  (*p).find(src_tag);
        if (tag_pos != NPOS) {

            string incl_dir = string(*p, 0, tag_pos);
            m_IncludeDirsRoot = CDirEntry::ConcatPath(incl_dir, "include");
            break;
        }
    }
#endif

    ITERATE(list<string>, p, m_SourcesDirsAbs) {
    
        string include_dir(*p);
        include_dir.replace(include_dir.find(src_tag), 
                            src_tag.length(), 
                            include_tag);
        m_IncludeDirsAbs.push_back(include_dir);
    }
    
  
    m_AdditionalIncludeDirectories = CDirEntry::CreateRelativePath(
                                               m_ProjectDir, m_IncludeDirsRoot);

#if 0
    // Relaxed dirs - we'll add path to include dir in addition to IncludeRoot.
    if ( GetApp().GetRelaxedIncludeDirs() ) {

        list<string> include_dirs;
        ITERATE(list<string>, p, m_IncludeDirsAbs) {

            string incl_dir = CDirEntry::CreateRelativePath(m_ProjectDir, *p);
            include_dirs.push_back(incl_dir);
        }
        if ( !include_dirs.empty() ) {
    
            m_AdditionalIncludeDirectories += ", ";
            m_AdditionalIncludeDirectories += NStr::Join(include_dirs, ", ");
        }
    }
#endif
}

//------------------------------------------------------------------------------
CMsvcPrjGeneralContext::CMsvcPrjGeneralContext(const string& config, 
                                      const CMsvcPrjProjectContext& prj_context)
{
    //m_Type
    switch ( prj_context.ProjectType() ) {
    case CProjItem::eLib:
        m_Type = eLib;
        break;
    case CProjItem::eApp:
        m_Type = eExe;
        break;
    default:
        //TODO - handle Dll(s)
   	    NCBI_THROW(CProjBulderAppException, eProjectType, 
                        NStr::IntToString(prj_context.ProjectType()));
    }
    
    //m_RunTime and m_BuildType
    if (config == "Debug") {

        m_RunTime   = eStatic;
        m_BuildType = eDebug;
    }
    else if (config == "DebugDLL") {

        m_RunTime   = eDynamicMT;
        m_BuildType = eDebug;
    }
    else if (config == "DebugMT") {

        m_RunTime   = eStaticMT;
        m_BuildType = eDebug;
    }
    else if (config == "Release") {

        m_RunTime   = eStatic;
        m_BuildType = eRelease;
    }
    else if (config == "ReleaseDLL") {

        m_RunTime   = eDynamicMT;
        m_BuildType = eRelease;
    }
    else if (config == "ReleaseMT") {

        m_RunTime   = eStaticMT;
        m_BuildType = eRelease;
    }
    else {

   	    NCBI_THROW(CProjBulderAppException, eBuildConfiguration, config);
    }

    //m_OutputDirectory;
    // /compilers/msvc7_prj/
    const string project_tag(string(1,CDirEntry::GetPathSeparator()) + 
                             "compilers" +
                             CDirEntry::GetPathSeparator() + 
                             "msvc7_prj" + 
                             CDirEntry::GetPathSeparator());
    
    string project_dir = prj_context.ProjectDir();
    string output_dir_prefix = string (project_dir, 0,
                                       project_dir.find(
                                           project_tag) + project_tag.length());
    if (m_Type == eLib)
        output_dir_prefix = CDirEntry::ConcatPath(output_dir_prefix, "lib");
    else if (m_Type == eExe)
        output_dir_prefix = CDirEntry::ConcatPath(output_dir_prefix, "bin");
    else {
        //TODO - handle Dll(s)
   	    NCBI_THROW(CProjBulderAppException, 
                   eProjectType, NStr::IntToString(m_Type));
    }
    string output_dir_abs = CDirEntry::ConcatPath(output_dir_prefix, config);
    m_OutputDirectory = CDirEntry::CreateRelativePath(project_dir, 
                                                      output_dir_abs);

    m_ConfigurationName = config;
}

//------------------------------------------------------------------------------

static IConfiguration * s_CreateConfiguration(
                                const CMsvcPrjGeneralContext& general_context,
                                const CMsvcPrjProjectContext& project_context);

static ICompilerTool * s_CreateCompilerTool(
                                const CMsvcPrjGeneralContext& general_context,
                                const CMsvcPrjProjectContext& project_context);

static ILinkerTool * s_CreateLinkerTool(
                                const CMsvcPrjGeneralContext& general_context,
                                const CMsvcPrjProjectContext& project_context);

static ILibrarianTool * s_CreateLibrarianTool(
                                const CMsvcPrjGeneralContext& general_context,
                                const CMsvcPrjProjectContext& project_context);

static IResourceCompilerTool * s_CreateResourceCompilerTool(
                                const CMsvcPrjGeneralContext& general_context,
                                const CMsvcPrjProjectContext& project_context);

//------------------------------------------------------------------------------

CMsvcTools::CMsvcTools(	const CMsvcPrjGeneralContext& general_context,
                        const CMsvcPrjProjectContext& project_context )
{
    //configuration
    m_Configuration = auto_ptr<IConfiguration>(s_CreateConfiguration(
                                             general_context, project_context));
    //compiler
    m_Compiler	= auto_ptr<ICompilerTool>(s_CreateCompilerTool(
                                             general_context, project_context));
    //Linker:
    m_Linker = auto_ptr<ILinkerTool>(s_CreateLinkerTool(
                                             general_context, project_context));
    //Librarian
    m_Librarian = auto_ptr<ILibrarianTool>(s_CreateLibrarianTool(
                                             general_context, project_context));
    //Dummies
    m_CustomBuid = auto_ptr<ICustomBuildTool>(new CCustomBuildToolDummyImpl());
    m_MIDL = auto_ptr<IMIDLTool>(new CMIDLToolDummyImpl());
    m_PostBuildEvent = auto_ptr<IPostBuildEventTool>(
                                            new CPostBuildEventToolDummyImpl());
    m_PreBuildEvent = auto_ptr<IPreBuildEventTool>(
                                            new CPreBuildEventToolDummyImpl());
    m_PreLinkEvent = auto_ptr<IPreLinkEventTool>(
                                            new CPreLinkEventToolDummyImpl());

    //Resource Compiler
    m_ResourceCompiler 
        = auto_ptr<IResourceCompilerTool>(s_CreateResourceCompilerTool(
                                            general_context, project_context));

    //Dummies
    m_WebServiceProxyGenerator = auto_ptr<IWebServiceProxyGeneratorTool>(
                                new CWebServiceProxyGeneratorToolDummyImpl());
    m_XMLDataGenerator = auto_ptr<IXMLDataGeneratorTool>(
                                new CXMLDataGeneratorToolDummyImpl());
    m_ManagedWrapperGenerator = auto_ptr<IManagedWrapperGeneratorTool>(
                                new CManagedWrapperGeneratorToolDummyImpl());
    m_AuxiliaryManagedWrapperGenerator=
                            auto_ptr<IAuxiliaryManagedWrapperGeneratorTool> (
                          new CAuxiliaryManagedWrapperGeneratorToolDummyImpl());
}


IConfiguration * CMsvcTools::Configuration(void) const
{
    return m_Configuration.get();
}


ICompilerTool * CMsvcTools::Compiler(void) const
{
    return m_Compiler.get();
}


ILinkerTool * CMsvcTools::Linker(void) const
{
    return m_Linker.get();
}


ILibrarianTool * CMsvcTools::Librarian(void) const
{
    return m_Librarian.get();
}


ICustomBuildTool * CMsvcTools::CustomBuid(void) const
{
    return m_CustomBuid.get();
}


IMIDLTool * CMsvcTools::MIDL(void) const
{
    return m_MIDL.get();
}


IPostBuildEventTool * CMsvcTools::PostBuildEvent(void) const
{
    return m_PostBuildEvent.get();
}


IPreBuildEventTool * CMsvcTools::PreBuildEvent(void) const
{
    return m_PreBuildEvent.get();
}


IPreLinkEventTool * CMsvcTools::PreLinkEvent(void) const
{
    return m_PreLinkEvent.get();
}


IResourceCompilerTool * CMsvcTools::ResourceCompiler(void) const
{
    return m_ResourceCompiler.get();
}


IWebServiceProxyGeneratorTool * CMsvcTools::WebServiceProxyGenerator(void) const
{
    return m_WebServiceProxyGenerator.get();
}


IXMLDataGeneratorTool * CMsvcTools::XMLDataGenerator(void) const
{
    return m_XMLDataGenerator.get();
}


IManagedWrapperGeneratorTool * CMsvcTools::ManagedWrapperGenerator(void) const
{
    return m_ManagedWrapperGenerator.get();
}


IAuxiliaryManagedWrapperGeneratorTool * 
                        CMsvcTools::AuxiliaryManagedWrapperGenerator(void) const
{
    return m_AuxiliaryManagedWrapperGenerator.get();
}


CMsvcTools::~CMsvcTools()
{
}
//------------------------------------------------------------------------------
static bool s_IsExe(const CMsvcPrjGeneralContext& general_context,
                    const CMsvcPrjProjectContext& project_context)
{
    return general_context.m_Type == CMsvcPrjGeneralContext::TTargetType::eExe;
}


static bool s_IsLib(const CMsvcPrjGeneralContext& general_context,
                    const CMsvcPrjProjectContext& project_context)
{
    return general_context.m_Type == CMsvcPrjGeneralContext::TTargetType::eLib;
}


static bool s_IsDll(const CMsvcPrjGeneralContext& general_context,
                    const CMsvcPrjProjectContext& project_context)
{
    return general_context.m_Type == CMsvcPrjGeneralContext::TTargetType::eDll;
}

//------------------------------------------------------------------------------

static bool s_IsStatic(const CMsvcPrjGeneralContext& general_context,
                       const CMsvcPrjProjectContext& project_context)
{
    return general_context.m_RunTime == 
                             CMsvcPrjGeneralContext::TRunTimeLibType::eStatic;
}


static bool s_IsStaticMT(const CMsvcPrjGeneralContext& general_context,
                       const CMsvcPrjProjectContext& project_context)
{
    return general_context.m_RunTime == 
                             CMsvcPrjGeneralContext::TRunTimeLibType::eStaticMT;
}


static bool s_IsDynamicMT(const CMsvcPrjGeneralContext& general_context,
                       const CMsvcPrjProjectContext& project_context)
{
    return general_context.m_RunTime == 
                             CMsvcPrjGeneralContext::TRunTimeLibType::eDynamicMT;
}

//------------------------------------------------------------------------------

static bool s_IsDebug(const CMsvcPrjGeneralContext& general_context,
                       const CMsvcPrjProjectContext& project_context)
{
    return general_context.m_BuildType == 
                                 CMsvcPrjGeneralContext::TBuildType::eDebug;
}


static bool s_IsRelease(const CMsvcPrjGeneralContext& general_context,
                       const CMsvcPrjProjectContext& project_context)
{
    return general_context.m_BuildType == 
                                 CMsvcPrjGeneralContext::TBuildType::eRelease;
}
//------------------------------------------------------------------------------


static IConfiguration * s_CreateConfiguration(
                                  const CMsvcPrjGeneralContext& general_context,
                                  const CMsvcPrjProjectContext& project_context)
{
    if ( s_IsExe(general_context, project_context) )
	    return new CConfigurationImpl<SApp>( general_context.OutputDirectory(), 
                                           general_context.ConfigurationName());

    if ( s_IsLib(general_context, project_context) )
	    return new CConfigurationImpl<SLib>( general_context.OutputDirectory(), 
                                           general_context.ConfigurationName());
    if ( s_IsDll(general_context, project_context) )

	    return new CConfigurationImpl<SDll>( general_context.OutputDirectory(), 
                                           general_context.ConfigurationName());
    return NULL;
}


//------------------------------------------------------------------------------------------
static ICompilerTool * s_CreateCompilerTool(
								 const CMsvcPrjGeneralContext& general_context,
								 const CMsvcPrjProjectContext& project_context)
{
    //-------- EXE ---------
    //
    if ( s_IsExe   (general_context, project_context)  &&
         s_IsStatic(general_context, project_context)  &&
         s_IsDebug (general_context, project_context) )
	     return new CCompilerToolImpl<SCrtSingleThreadedDebug, 
				                     SDebug, 
                                     SApp>
                               (project_context.AdditionalIncludeDirectories());


    if ( s_IsExe    (general_context, project_context)  &&
         s_IsStatic (general_context, project_context)  &&
         s_IsRelease(general_context, project_context) )
	     return new CCompilerToolImpl<SCrtSingleThreaded, 
				                     SRelease, 
                                     SApp>
                               (project_context.AdditionalIncludeDirectories());

    //
    if ( s_IsExe     (general_context, project_context)  &&
         s_IsStaticMT(general_context, project_context)  &&
         s_IsDebug   (general_context, project_context) )
	     return new CCompilerToolImpl<SCrtMultiThreadedDebug, 
				                     SDebug, 
                                     SApp>
                               (project_context.AdditionalIncludeDirectories());


    if ( s_IsExe     (general_context, project_context)  &&
         s_IsStaticMT(general_context, project_context)  &&
         s_IsRelease (general_context, project_context) )
	     return new CCompilerToolImpl<SCrtMultiThreaded, 
				                     SRelease, 
                                     SApp>
                               (project_context.AdditionalIncludeDirectories());

    //
    if ( s_IsExe      (general_context, project_context)  &&
         s_IsDynamicMT(general_context, project_context)  &&
         s_IsDebug    (general_context, project_context) )
	     return new CCompilerToolImpl<SCrtMultiThreadedDebugDLL, 
				                     SDebug, 
                                     SApp>
                               (project_context.AdditionalIncludeDirectories());


    if ( s_IsExe      (general_context, project_context)  &&
         s_IsDynamicMT(general_context, project_context)  &&
         s_IsRelease  (general_context, project_context) )
	     return new CCompilerToolImpl<SCrtMultiThreadedDLL, 
				                     SRelease, 
                                     SApp>
                               (project_context.AdditionalIncludeDirectories());

    //-------- LIB ---------
    //
    if ( s_IsLib   (general_context, project_context)  &&
         s_IsStatic(general_context, project_context)  &&
         s_IsDebug (general_context, project_context) )
	     return new CCompilerToolImpl<SCrtSingleThreadedDebug, 
				                     SDebug, 
                                     SLib>
                               (project_context.AdditionalIncludeDirectories());


    if ( s_IsLib    (general_context, project_context)  &&
         s_IsStatic (general_context, project_context)  &&
         s_IsRelease(general_context, project_context) )
	     return new CCompilerToolImpl<SCrtSingleThreaded, 
				                     SRelease, 
                                     SLib>
                               (project_context.AdditionalIncludeDirectories());

    //
    if ( s_IsLib     (general_context, project_context)  &&
         s_IsStaticMT(general_context, project_context)  &&
         s_IsDebug   (general_context, project_context) )
	     return new CCompilerToolImpl<SCrtMultiThreadedDebug, 
				                     SDebug, 
                                     SLib>
                               (project_context.AdditionalIncludeDirectories());


    if ( s_IsLib     (general_context, project_context)  &&
         s_IsStaticMT(general_context, project_context)  &&
         s_IsRelease (general_context, project_context) )
	     return new CCompilerToolImpl<SCrtMultiThreaded, 
				                     SRelease, 
                                     SLib>
                               (project_context.AdditionalIncludeDirectories());

    //
    if ( s_IsLib      (general_context, project_context)  &&
         s_IsDynamicMT(general_context, project_context)  &&
         s_IsDebug    (general_context, project_context) )
	     return new CCompilerToolImpl<SCrtMultiThreadedDebugDLL, 
				                     SDebug, 
                                     SLib>
                               (project_context.AdditionalIncludeDirectories());


    if ( s_IsLib      (general_context, project_context)  &&
         s_IsDynamicMT(general_context, project_context)  &&
         s_IsRelease  (general_context, project_context) )
	     return new CCompilerToolImpl<SCrtMultiThreadedDLL, 
				                     SRelease, 
                                     SLib>
                               (project_context.AdditionalIncludeDirectories());
    //-------- DLL ---------
    if ( s_IsDll   (general_context, project_context)  &&
         s_IsStatic(general_context, project_context)  &&
         s_IsDebug (general_context, project_context) )
	     return new CCompilerToolImpl<SCrtSingleThreadedDebug, 
				                     SDebug, 
                                     SDll>
                               (project_context.AdditionalIncludeDirectories());


    if ( s_IsDll    (general_context, project_context)  &&
         s_IsStatic (general_context, project_context)  &&
         s_IsRelease(general_context, project_context) )
	     return new CCompilerToolImpl<SCrtSingleThreaded, 
				                     SRelease, 
                                     SDll>
                               (project_context.AdditionalIncludeDirectories());

    //
    if ( s_IsDll     (general_context, project_context)  &&
         s_IsStaticMT(general_context, project_context)  &&
         s_IsDebug   (general_context, project_context) )
	     return new CCompilerToolImpl<SCrtMultiThreadedDebug, 
				                     SDebug, 
                                     SDll>
                               (project_context.AdditionalIncludeDirectories());


    if ( s_IsDll     (general_context, project_context)  &&
         s_IsStaticMT(general_context, project_context)  &&
         s_IsRelease (general_context, project_context) )
	     return new CCompilerToolImpl<SCrtMultiThreaded, 
				                     SRelease, 
                                     SDll>
                               (project_context.AdditionalIncludeDirectories());

    //
    if ( s_IsDll      (general_context, project_context)  &&
         s_IsDynamicMT(general_context, project_context)  &&
         s_IsDebug    (general_context, project_context) )
	     return new CCompilerToolImpl<SCrtMultiThreadedDebugDLL, 
				                     SDebug, 
                                     SDll>
                               (project_context.AdditionalIncludeDirectories());


    if ( s_IsDll      (general_context, project_context)  &&
         s_IsDynamicMT(general_context, project_context)  &&
         s_IsRelease  (general_context, project_context) )
	     return new CCompilerToolImpl<SCrtMultiThreadedDLL, 
				                     SRelease, 
                                     SDll>
                               (project_context.AdditionalIncludeDirectories());

    // unsupported tool
    return NULL;
}


static ILinkerTool * s_CreateLinkerTool(
                                  const CMsvcPrjGeneralContext& general_context,
                                  const CMsvcPrjProjectContext& project_context)
{
    //---- EXE ----
    if ( s_IsExe  (general_context, project_context)  &&
         s_IsDebug(general_context, project_context) )
        return new CLinkerToolImpl<SDebug, SApp>
                                    ( project_context.AdditionalLinkerOptions(),
                                      project_context.ProjectName() );

    if ( s_IsExe    (general_context, project_context)  &&
         s_IsRelease(general_context, project_context) )
        return new CLinkerToolImpl<SRelease, SApp>
                                    ( project_context.AdditionalLinkerOptions(),
                                      project_context.ProjectName() );

    //---- LIB ----
    if ( s_IsLib(general_context, project_context) )
        return new CLinkerToolDummyImpl();

    //---- DLL ----
    if ( s_IsDll  (general_context, project_context)  &&
         s_IsDebug(general_context, project_context) )
        return new CLinkerToolImpl<SDebug, SDll>
                                    ( project_context.AdditionalLinkerOptions(),
                                      project_context.ProjectName() );

    if ( s_IsDll    (general_context, project_context)  &&
         s_IsRelease(general_context, project_context) )
        return new CLinkerToolImpl<SRelease, SDll>
                                    ( project_context.AdditionalLinkerOptions(),
                                      project_context.ProjectName() );
    // unsupported tool
    return NULL;
}


static ILibrarianTool * s_CreateLibrarianTool(
                                  const CMsvcPrjGeneralContext& general_context,
                                  const CMsvcPrjProjectContext& project_context)
{
    if ( s_IsLib(general_context, project_context) )
	    return new CLibrarianToolImpl
                                (  project_context.AdditionalLibrarianOptions(),
								   project_context.ProjectName() );

    // unsupported tool
    return new CLibrarianToolDummyImpl();
}


static IResourceCompilerTool * s_CreateResourceCompilerTool(
                                  const CMsvcPrjGeneralContext& general_context,
                                  const CMsvcPrjProjectContext& project_context)
{

    if ( s_IsDll  (general_context, project_context)  &&
         s_IsDebug(general_context, project_context) )
        return new CResourceCompilerToolImpl<SDebug>();

    if ( s_IsDll    (general_context, project_context)  &&
         s_IsRelease(general_context, project_context) )
        return new CResourceCompilerToolImpl<SRelease>();

    // unsupported tool
    return new CResourceCompilerToolDummyImpl();
}


//------------------------------------------------------------------------------
END_NCBI_SCOPE


