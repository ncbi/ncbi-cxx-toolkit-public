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

#include <app/project_tree_builder/stl_msvc_usage.hpp>

#include <app/project_tree_builder/msvc_project_context.hpp>
#include <app/project_tree_builder/msvc_tools_implement.hpp>
#include <app/project_tree_builder/proj_builder_app.hpp>
#include <app/project_tree_builder/msvc_site.hpp>
#include <app/project_tree_builder/msvc_prj_defines.hpp>

#include <set>


BEGIN_NCBI_SCOPE


//-----------------------------------------------------------------------------
CMsvcPrjProjectContext::CMsvcPrjProjectContext(const CProjItem& project)
{
    //MSVC project name it's project ID from project tree
    m_ProjectName    = project.m_ID;

    m_SourcesBaseDir = project.m_SourcesBaseDir;
    m_Requires       = project.m_Requires;

    // Get msvc project makefile
    m_MsvcProjectMakefile = 
        auto_ptr<CMsvcProjectMakefile>
            (new CMsvcProjectMakefile
                    (CDirEntry::ConcatPath
                            (m_SourcesBaseDir, 
                             CreateMsvcProjectMakefileName(project))));

    // Collect all dirs of source files into m_SourcesDirsAbs:
    set<string> sources_dirs;
    sources_dirs.insert(m_SourcesBaseDir);
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
                             GetApp().GetRegSettings().m_CompilersSubdir +
                             CDirEntry::GetPathSeparator()); 


    // Creating project dir:
    m_ProjectDir = m_SourcesBaseDir;
    m_ProjectDir.replace(m_ProjectDir.find(src_tag), 
                         src_tag.length(), 
                         project_tag);

    m_ProjType = project.m_ProjType;

     // Generate include dirs:
    //Root for all include dirs:
    string incl_dir = string(m_SourcesBaseDir, 
                             0, 
                             m_SourcesBaseDir.find(src_tag));
    m_IncludeDirsRoot = CDirEntry::ConcatPath(incl_dir, "include");


    ITERATE(list<string>, p, m_SourcesDirsAbs) {
        //Make include dirs from the source dirs
        string include_dir(*p);
        include_dir.replace(include_dir.find(src_tag), 
                            src_tag.length(), 
                            include_tag);
        m_IncludeDirsAbs.push_back(include_dir);
    }

    // Get custom build files and adjust pathes 
    m_MsvcProjectMakefile->GetCustomBuildInfo(&m_CustomBuildInfo);
    NON_CONST_ITERATE(list<SCustomBuildInfo>, p, m_CustomBuildInfo) {
       SCustomBuildInfo& build_info = *p;
       string file_path_abs = 
           CDirEntry::ConcatPath(m_SourcesBaseDir, build_info.m_SourceFile);
       build_info.m_SourceFile = 
           CDirEntry::CreateRelativePath(m_ProjectDir, file_path_abs);           
    }
}


string CMsvcPrjProjectContext::AdditionalIncludeDirectories
                                            (const SConfigInfo& cfg_info) const
{
    string add_include_dirs = 
        CDirEntry::CreateRelativePath(m_ProjectDir, m_IncludeDirsRoot);

    list<string> makefile_add_incl_dirs;
    m_MsvcProjectMakefile->GetAdditionalIncludeDirs(cfg_info, 
                                                    &makefile_add_incl_dirs);

    ITERATE(list<string>, p, makefile_add_incl_dirs) {
        const string& dir = *p;
        string dir_abs = 
            CDirEntry::AddTrailingPathSeparator
                (CDirEntry::ConcatPath(m_SourcesBaseDir, dir));
        dir_abs = CDirEntry::NormalizePath(dir_abs);
        add_include_dirs += ", ";
        add_include_dirs += 
            CDirEntry::CreateRelativePath
                        (m_ProjectDir, dir_abs);
    }
    //take into account default libs from site:
    list<string> libs_list(m_Requires);
    libs_list.push_back(MSVC_DEFAULT_LIBS_TAG);
    ITERATE(list<string>, p, libs_list) {
        const string& requires = *p;
        SLibInfo lib_info;
        GetApp().GetSite().GetLibInfo(requires, cfg_info, &lib_info);
        if ( !lib_info.m_IncludeDir.empty() ) {
            add_include_dirs += ", ";
            add_include_dirs += lib_info.m_IncludeDir;
        }
    }

    return add_include_dirs;
}


string CMsvcPrjProjectContext::AdditionalLinkerOptions
                                            (const SConfigInfo& cfg_info) const
{
    string libs_str("");

    // Take into account default libs from site:
    list<string> libs_list(m_Requires);
    libs_list.push_back(MSVC_DEFAULT_LIBS_TAG);
    ITERATE(list<string>, p, libs_list) {
        const string& requires = *p;
        SLibInfo lib_info;
        GetApp().GetSite().GetLibInfo(requires, cfg_info, &lib_info);
        if ( !lib_info.m_Libs.empty() ) {
            libs_str += " ";
            libs_str += NStr::Join(lib_info.m_Libs, " ");
        }
    }

    return libs_str;
}


string CMsvcPrjProjectContext::AdditionalLibrarianOptions
                                            (const SConfigInfo& cfg_info) const
{
    return AdditionalLinkerOptions(cfg_info);
}


string CMsvcPrjProjectContext::AdditionalLibraryDirectories
                                            (const SConfigInfo& cfg_info) const
{
    string dirs_str("");

    // Take into account default libs from site:
    list<string> libs_list(m_Requires);
    libs_list.push_back(MSVC_DEFAULT_LIBS_TAG);
    ITERATE(list<string>, p, libs_list) {
        const string& requires = *p;
        SLibInfo lib_info;
        GetApp().GetSite().GetLibInfo(requires, cfg_info, &lib_info);
        if ( !lib_info.IsEmpty() ) {
            dirs_str += lib_info.m_LibPath;
            dirs_str += ", ";
        }
    }

    return dirs_str;
}



const CMsvcProjectMakefile& 
CMsvcPrjProjectContext::GetMsvcProjectMakefile(void) const
{
    return *m_MsvcProjectMakefile;
}


bool CMsvcPrjProjectContext::IsRequiresOk(const CProjItem& prj)
{
    ITERATE(list<string>, p, prj.m_Requires) {
        const string& requires = *p;
        if ( !GetApp().GetSite().IsProvided(requires) )
            return false;
    }
    return true;
}


//-----------------------------------------------------------------------------
CMsvcPrjGeneralContext::CMsvcPrjGeneralContext
    (const SConfigInfo&            config, 
     const CMsvcPrjProjectContext& prj_context)
     :m_Config          (config),
      m_MsvcMetaMakefile(GetApp().GetMetaMakefile())
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
    

    //m_OutputDirectory;
    // /compilers/msvc7_prj/
    const string project_tag(string(1,CDirEntry::GetPathSeparator()) + 
                             "compilers" +
                             CDirEntry::GetPathSeparator() + 
                             GetApp().GetRegSettings().m_CompilersSubdir +
                             CDirEntry::GetPathSeparator());
    
    string project_dir = prj_context.ProjectDir();
    string output_dir_prefix = 
        string (project_dir, 
                0, 
                project_dir.find(project_tag) + project_tag.length());

    if (m_Type == eLib)
        output_dir_prefix = CDirEntry::ConcatPath(output_dir_prefix, "lib");
    else if (m_Type == eExe)
        output_dir_prefix = CDirEntry::ConcatPath(output_dir_prefix, "bin");
    else {
        //TODO - handle Dll(s)
   	    NCBI_THROW(CProjBulderAppException, 
                   eProjectType, NStr::IntToString(m_Type));
    }

    string output_dir_abs = CDirEntry::ConcatPath(output_dir_prefix, config.m_Name);
    m_OutputDirectory = CDirEntry::CreateRelativePath(project_dir, 
                                                      output_dir_abs);
}

//------------------------------------------------------------------------------
static IConfiguration* s_CreateConfiguration
    (const CMsvcPrjGeneralContext& general_context,
     const CMsvcPrjProjectContext& project_context);

static ICompilerTool* s_CreateCompilerTool
    (const CMsvcPrjGeneralContext& general_context,
     const CMsvcPrjProjectContext& project_context);

static ILinkerTool* s_CreateLinkerTool
    (const CMsvcPrjGeneralContext& general_context,
     const CMsvcPrjProjectContext& project_context);

static ILibrarianTool* s_CreateLibrarianTool
    (const CMsvcPrjGeneralContext& general_context,
     const CMsvcPrjProjectContext& project_context);

static IResourceCompilerTool* s_CreateResourceCompilerTool
    (const CMsvcPrjGeneralContext& general_context,
     const CMsvcPrjProjectContext& project_context);

//-----------------------------------------------------------------------------
CMsvcTools::CMsvcTools(const CMsvcPrjGeneralContext& general_context,
                       const CMsvcPrjProjectContext& project_context)
{
    //configuration
    m_Configuration = 
        auto_ptr<IConfiguration>(s_CreateConfiguration(general_context, 
                                                       project_context));
    //compiler
    m_Compiler = 
        auto_ptr<ICompilerTool>(s_CreateCompilerTool(general_context, 
                                                     project_context));
    //Linker:
    m_Linker = 
        auto_ptr<ILinkerTool>(s_CreateLinkerTool(general_context, 
                                                 project_context));
    //Librarian
    m_Librarian = 
        auto_ptr<ILibrarianTool>(s_CreateLibrarianTool(general_context, 
                                                       project_context));
    //Dummies
    m_CustomBuid = auto_ptr<ICustomBuildTool>(new CCustomBuildToolDummyImpl());
    m_MIDL       = auto_ptr<IMIDLTool>(new CMIDLToolDummyImpl());
    m_PostBuildEvent =
        auto_ptr<IPostBuildEventTool>(new CPostBuildEventToolDummyImpl());
    m_PreBuildEvent = 
        auto_ptr<IPreBuildEventTool>(new CPreBuildEventToolDummyImpl());
    m_PreLinkEvent = 
        auto_ptr<IPreLinkEventTool>(new CPreLinkEventToolDummyImpl());

    //Resource Compiler
    m_ResourceCompiler =
        auto_ptr<IResourceCompilerTool>
                        (s_CreateResourceCompilerTool(general_context, 
                                                      project_context));

    //Dummies
    m_WebServiceProxyGenerator =
        auto_ptr<IWebServiceProxyGeneratorTool>
                        (new CWebServiceProxyGeneratorToolDummyImpl());

    m_XMLDataGenerator = 
        auto_ptr<IXMLDataGeneratorTool>
                        (new CXMLDataGeneratorToolDummyImpl());

    m_ManagedWrapperGenerator =
        auto_ptr<IManagedWrapperGeneratorTool>
                        (new CManagedWrapperGeneratorToolDummyImpl());

    m_AuxiliaryManagedWrapperGenerator=
        auto_ptr<IAuxiliaryManagedWrapperGeneratorTool> 
                        (new CAuxiliaryManagedWrapperGeneratorToolDummyImpl());
}


IConfiguration* CMsvcTools::Configuration(void) const
{
    return m_Configuration.get();
}


ICompilerTool* CMsvcTools::Compiler(void) const
{
    return m_Compiler.get();
}


ILinkerTool* CMsvcTools::Linker(void) const
{
    return m_Linker.get();
}


ILibrarianTool* CMsvcTools::Librarian(void) const
{
    return m_Librarian.get();
}


ICustomBuildTool* CMsvcTools::CustomBuid(void) const
{
    return m_CustomBuid.get();
}


IMIDLTool* CMsvcTools::MIDL(void) const
{
    return m_MIDL.get();
}


IPostBuildEventTool* CMsvcTools::PostBuildEvent(void) const
{
    return m_PostBuildEvent.get();
}


IPreBuildEventTool* CMsvcTools::PreBuildEvent(void) const
{
    return m_PreBuildEvent.get();
}


IPreLinkEventTool* CMsvcTools::PreLinkEvent(void) const
{
    return m_PreLinkEvent.get();
}


IResourceCompilerTool* CMsvcTools::ResourceCompiler(void) const
{
    return m_ResourceCompiler.get();
}


IWebServiceProxyGeneratorTool* CMsvcTools::WebServiceProxyGenerator(void) const
{
    return m_WebServiceProxyGenerator.get();
}


IXMLDataGeneratorTool* CMsvcTools::XMLDataGenerator(void) const
{
    return m_XMLDataGenerator.get();
}


IManagedWrapperGeneratorTool* CMsvcTools::ManagedWrapperGenerator(void) const
{
    return m_ManagedWrapperGenerator.get();
}


IAuxiliaryManagedWrapperGeneratorTool* 
                       CMsvcTools::AuxiliaryManagedWrapperGenerator(void) const
{
    return m_AuxiliaryManagedWrapperGenerator.get();
}


CMsvcTools::~CMsvcTools()
{
}


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


static bool s_IsDebug(const CMsvcPrjGeneralContext& general_context,
                      const CMsvcPrjProjectContext& project_context)
{
    return general_context.m_Config.m_Debug;
}


static bool s_IsRelease(const CMsvcPrjGeneralContext& general_context,
                        const CMsvcPrjProjectContext& project_context)
{
    return !(general_context.m_Config.m_Debug);
}


//-----------------------------------------------------------------------------
// Creators:
static IConfiguration* 
s_CreateConfiguration(const CMsvcPrjGeneralContext& general_context,
                      const CMsvcPrjProjectContext& project_context)
{
    if ( s_IsExe(general_context, project_context) )
	    return new CConfigurationImpl<SApp>
                       (general_context.OutputDirectory(), 
                        general_context.ConfigurationName());

    if ( s_IsLib(general_context, project_context) )
	    return new CConfigurationImpl<SLib>
                        (general_context.OutputDirectory(), 
                         general_context.ConfigurationName());

    if ( s_IsDll(general_context, project_context) )
	    return new CConfigurationImpl<SDll>
                        (general_context.OutputDirectory(), 
                         general_context.ConfigurationName());
    return NULL;
}


static ICompilerTool* 
s_CreateCompilerTool(const CMsvcPrjGeneralContext& general_context,
					 const CMsvcPrjProjectContext& project_context)
{
    return new CCompilerToolImpl
       (project_context.AdditionalIncludeDirectories(general_context.m_Config),
        project_context.GetMsvcProjectMakefile(),
        general_context.m_Config.m_RuntimeLibrary,
        general_context.GetMsvcMetaMakefile(),
        general_context.m_Config,
        general_context.m_Type);
}


static ILinkerTool* 
s_CreateLinkerTool(const CMsvcPrjGeneralContext& general_context,
                   const CMsvcPrjProjectContext& project_context)
{
    //---- EXE ----
    if ( s_IsExe  (general_context, project_context) )
        return new CLinkerToolImpl<SApp>
                       (project_context.AdditionalLinkerOptions
                                            (general_context.m_Config),
                        project_context.AdditionalLibraryDirectories
                                            (general_context.m_Config),
                        project_context.ProjectName(),
                        project_context.GetMsvcProjectMakefile(),
                        general_context.GetMsvcMetaMakefile(),
                        general_context.m_Config);


    //---- LIB ----
    if ( s_IsLib(general_context, project_context) )
        return new CLinkerToolDummyImpl();

    //---- DLL ----
    if ( s_IsDll  (general_context, project_context) )
        return new CLinkerToolImpl<SDll>
                       (project_context.AdditionalLinkerOptions
                                            (general_context.m_Config),
                        project_context.AdditionalLibraryDirectories
                                            (general_context.m_Config),
                        project_context.ProjectName(),
                        project_context.GetMsvcProjectMakefile(),
                        general_context.GetMsvcMetaMakefile(),
                        general_context.m_Config);

    // unsupported tool
    return NULL;
}


static ILibrarianTool* 
s_CreateLibrarianTool(const CMsvcPrjGeneralContext& general_context,
                      const CMsvcPrjProjectContext& project_context)
{
    if ( s_IsLib  (general_context, project_context) )
	    return new CLibrarianToolImpl
                                (project_context.AdditionalLibrarianOptions
                                                    (general_context.m_Config),
                                 project_context.AdditionalLibraryDirectories
                                                    (general_context.m_Config),
								 project_context.ProjectName(),
                                 project_context.GetMsvcProjectMakefile(),
                                 general_context.GetMsvcMetaMakefile(),
                                 general_context.m_Config);

    // dummy tool
    return new CLibrarianToolDummyImpl();
}


static IResourceCompilerTool* s_CreateResourceCompilerTool
                                (const CMsvcPrjGeneralContext& general_context,
                                 const CMsvcPrjProjectContext& project_context)
{

    if ( s_IsDll  (general_context, project_context)  &&
         s_IsDebug(general_context, project_context) )
        return new CResourceCompilerToolImpl<SDebug>();

    if ( s_IsDll    (general_context, project_context)  &&
         s_IsRelease(general_context, project_context) )
        return new CResourceCompilerToolImpl<SRelease>();

    // dummy tool
    return new CResourceCompilerToolDummyImpl();
}


END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.7  2004/01/29 15:46:44  gorelenk
 * Added support of default libs, defined in user site
 *
 * Revision 1.6  2004/01/28 17:55:49  gorelenk
 * += For msvc makefile support of :
 *                 Requires tag, ExcludeProject tag,
 *                 AddToProject section (SourceFiles and IncludeDirs),
 *                 CustomBuild section.
 * += For support of user local site.
 *
 * Revision 1.5  2004/01/26 19:27:29  gorelenk
 * += MSVC meta makefile support
 * += MSVC project makefile support
 *
 * Revision 1.4  2004/01/22 17:57:54  gorelenk
 * first version
 *
 * ===========================================================================
 */

