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

#include <app/project_tree_builder/msvc_prj_utils.hpp>
#include <app/project_tree_builder/proj_builder_app.hpp>
#include <app/project_tree_builder/msvc_prj_defines.hpp>
#include <serial/objostrxml.hpp>
#include <serial/objistr.hpp>
#include <serial/serial.hpp>



BEGIN_NCBI_SCOPE


CVisualStudioProject * LoadFromXmlFile(const string& file_path)
{
    auto_ptr<CObjectIStream> in(CObjectIStream::Open(eSerial_Xml, 
                                                    file_path, 
                                                    eSerial_StdWhenAny));
    if ( in->fail() )
	    NCBI_THROW(CProjBulderAppException, eFileOpen, file_path);
    
    auto_ptr<CVisualStudioProject> prj(new CVisualStudioProject());
    in->Read(prj.get(), prj->GetThisTypeInfo());
    return prj.release();
}


void SaveToXmlFile  (const string&               file_path, 
                     const CVisualStudioProject& project)
{
    // Create dir if no such dir...
    string dir;
    CDirEntry::SplitPath(file_path, &dir);
    CDir project_dir(dir);
    if ( !project_dir.Exists() ) {
        CDir(dir).CreatePath();
    }

    CNcbiOfstream  ofs(file_path.c_str(), 
                       IOS_BASE::out | IOS_BASE::trunc );
    if ( !ofs )
	    NCBI_THROW(CProjBulderAppException, eFileCreation, file_path);

    CObjectOStreamXml xs(ofs, false);
    xs.SetReferenceDTD(false);
    xs.SetEncoding(CObjectOStreamXml::eEncoding_Windows_1252);

    xs << project;
}


void PromoteIfDifferent(const string& present_path, 
                        const string& candidate_path)
{
    // Open both files
    CNcbiIfstream ifs_present(present_path.c_str(), 
                              IOS_BASE::in | IOS_BASE::binary);
    if ( !ifs_present ) {
        NCBI_THROW(CProjBulderAppException, eFileOpen, present_path);
    }

    ifs_present.seekg(0, ios::end);
    size_t file_length_present = ifs_present.tellg();
    ifs_present.seekg(0, ios::beg);

    CNcbiIfstream ifs_new (candidate_path.c_str(), 
                              IOS_BASE::in | IOS_BASE::binary);
    if ( !ifs_new ) {
        NCBI_THROW(CProjBulderAppException, eFileOpen, candidate_path);
    }

    ifs_new.seekg(0, ios::end);
    size_t file_length_new = ifs_new.tellg();
    ifs_new.seekg(0, ios::beg);

    if (file_length_present != file_length_new) {
        ifs_present.close();
        ifs_new.close();
        CDirEntry(present_path).Remove();
        CDirEntry(candidate_path).Rename(present_path);
        return;
    }

    // Load both to memory
    typedef AutoPtr<char, ArrayDeleter<char> > TAutoArray;
    TAutoArray buf_present = TAutoArray(new char [file_length_present]);
    TAutoArray buf_new     = TAutoArray(new char [file_length_new]);

    ifs_present.read(buf_present.get(), file_length_present);
    ifs_new.read    (buf_new.get(),     file_length_new);

    ifs_present.close();
    ifs_new.close();

    // If candidate file is not the same as present file it'll be a new file
    if (memcmp(buf_present.get(), buf_new.get(), file_length_present) != 0) {
        CDirEntry(present_path).Remove();
        CDirEntry(candidate_path).Rename(present_path);
        return;
    } else {
        CDirEntry(candidate_path).Remove();
    }
}


void SaveIfNewer    (const string&               file_path, 
                     const CVisualStudioProject& project)
{
    // If no such file then simple write it
    if ( !CDirEntry(file_path).Exists() ) {
        SaveToXmlFile(file_path, project);
        return;
    }


    // Save new file to tmp path.
    string candidate_file_path = file_path + ".candidate";
    SaveToXmlFile(candidate_file_path, project);
    PromoteIfDifferent(file_path, candidate_file_path);
}

//-----------------------------------------------------------------------------
class CGuidGenerator
{
public:
    ~CGuidGenerator(void);
    friend string GenerateSlnGUID(void);

private:
    CGuidGenerator(void);

    const string root_guid; // root GUID for MSVC solutions
    const string guid_base;
    string Generate12Chars(void);
    unsigned int m_Seed;

    string DoGenerateSlnGUID(void);
    set<string> m_Trace;
};



string GenerateSlnGUID(void)
{
    static CGuidGenerator guid_gen;
    return guid_gen.DoGenerateSlnGUID();
}


CGuidGenerator::CGuidGenerator(void)
    :root_guid("8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942"),
     guid_base("8BC9CEB8-8B4A-11D0-8D11-"),
     m_Seed(0)
{
}


CGuidGenerator::~CGuidGenerator(void)
{
}


string CGuidGenerator::Generate12Chars(void)
{
    CNcbiOstrstream ost;

    ost << hex << uppercase << noshowbase << setw(12) << setfill('A') 
        << m_Seed++ << ends << flush;

    return ost.str();
}


string CGuidGenerator::DoGenerateSlnGUID(void)
{
    for ( ;; ) {
        //GUID prototype
        string proto = guid_base + Generate12Chars();
        if (proto != root_guid  &&  m_Trace.find(proto) == m_Trace.end()) {
            m_Trace.insert(proto);
            return "{" + proto + "}";
        }
    }
}

 
string SourceFileExt(const string& file_path)
{
    string ext;
    CDirEntry::SplitPath(file_path, NULL, NULL, &ext);
    
    bool explicit_c   = NStr::CompareNocase(ext, ".c"  )== 0;
    if (explicit_c  &&  CFile(file_path).Exists()) {
        return ".c";
    }
    bool explicit_cpp = NStr::CompareNocase(ext, ".cpp")== 0;
    if (explicit_cpp  &&  CFile(file_path).Exists()) {
        return ".cpp";
    }

    string file_path_cpp = file_path + ".cpp";
    if ( CFile(file_path_cpp).Exists() ) 
        return ".cpp";

    string file_path_c = file_path + ".c";
    if ( CFile(file_path_c).Exists() ) 
        return ".c";

    return "";
}


//-----------------------------------------------------------------------------
SConfigInfo::SConfigInfo(void)
    :m_Debug(false)
{
}

SConfigInfo::SConfigInfo(const string& name, 
                         bool debug, 
                         const string& runtime_library)
    :m_Name          (name),
     m_Debug         (debug),
     m_RuntimeLibrary(runtime_library)
{
}


void LoadConfigInfoByNames(const CNcbiRegistry& registry, 
                           const list<string>&  config_names, 
                           list<SConfigInfo>*   configs)
{
    ITERATE(list<string>, p, config_names) {

        const string& config_name = *p;
        SConfigInfo config;
        config.m_Name  = config_name;
        config.m_Debug = registry.GetString(config_name, 
                                            "debug",
                                            "FALSE") != "FALSE";
        config.m_RuntimeLibrary = registry.GetString(config_name, 
                                                     "runtimeLibraryOption",
                                                     "0");
        configs->push_back(config);
    }
}


//-----------------------------------------------------------------------------
CMsvc7RegSettings::CMsvc7RegSettings(void)
{
    //TODO
}


bool IsSubdir(const string& abs_parent_dir, const string& abs_dir)
{
    return abs_dir.find(abs_parent_dir) == 0;
}


string GetOpt(const CNcbiRegistry& registry, 
              const string& section, 
              const string& opt, 
              const string& config)
{
    string section_spec = section + '.' + config;
    string val_spec = registry.GetString(section_spec, opt, "");
    if ( !val_spec.empty() )
        return val_spec;

    return registry.GetString(section, opt, "");
}


string GetOpt(const CNcbiRegistry& registry, 
              const string&        section, 
              const string&        opt, 
              const SConfigInfo&   config)
{
    string section_spec(section);
    section_spec += config.m_Debug? ".debug": ".release";
    string section_dr(section_spec); //section.debug or section.release
    section_spec += '.';
    section_spec += config.m_Name;

    string val_spec = registry.GetString(section_spec, opt, "");
    if ( !val_spec.empty() )
        return val_spec;

    val_spec = registry.GetString(section_dr, opt, "");
    if ( !val_spec.empty() )
        return val_spec;

    return registry.GetString(section, opt, "");
}



string ConfigName(const string& config)
{
    return config +'|'+ MSVC_PROJECT_PLATFORM;
}

//-----------------------------------------------------------------------------
CSourceFileToProjectInserter::CSourceFileToProjectInserter
                                        (const string&            project_id,
                                         const list<SConfigInfo>& configs,
                                         const string&            project_dir)
    :m_ProjectId  (project_id),
     m_Configs    (configs),
     m_ProjectDir (project_dir)
{
}


CSourceFileToProjectInserter::~CSourceFileToProjectInserter(void)
{
}

void 
CSourceFileToProjectInserter::operator()(CRef<CFilter>&  filter, 
                                         const string&   rel_source_file)
{
#if 0
    CRef< CFFile > file(new CFFile());
    file->SetAttlist().SetRelativePath(rel_source_file);

    CRef< CFilter_Base::C_FF::C_E > ce(new CFilter_Base::C_FF::C_E());
    ce->SetFile(*file);
    filter->SetFF().SetFF().push_back(ce);
#endif

#if 1
    CRef< CFFile > file(new CFFile());
    file->SetAttlist().SetRelativePath(rel_source_file);
    //
    TPch pch_usage = DefinePchUsage(m_ProjectDir, rel_source_file);
    //
    ITERATE(list<SConfigInfo>, n , m_Configs) {
        // Iterate all configurations
        const string& config = (*n).m_Name;

        CRef<CFileConfiguration> file_config(new CFileConfiguration());
        file_config->SetAttlist().SetName(ConfigName(config));

        CRef<CTool> compilerl_tool(new CTool(""));
        compilerl_tool->SetAttlist().SetName("VCCLCompilerTool");

        if (pch_usage.first == eCreate) {
            compilerl_tool->SetAttlist().SetPreprocessorDefinitions
                             (GetApp().GetMetaMakefile().GetPchUsageDefine());
            compilerl_tool->SetAttlist().SetUsePrecompiledHeader("1");
            compilerl_tool->SetAttlist().SetPrecompiledHeaderThrough
                                                            (pch_usage.second);
        } else if (pch_usage.first == eUse) {
            compilerl_tool->SetAttlist().SetPreprocessorDefinitions
                              (GetApp().GetMetaMakefile().GetPchUsageDefine());
            compilerl_tool->SetAttlist().SetUsePrecompiledHeader("3");
            compilerl_tool->SetAttlist().SetPrecompiledHeaderThrough
                                                            (pch_usage.second);
        }
        else {
            compilerl_tool->SetAttlist().SetUsePrecompiledHeader("0");
            compilerl_tool->SetAttlist().SetPrecompiledHeaderThrough("");
        }
        file_config->SetTool(*compilerl_tool);

        file->SetFileConfiguration().push_back(file_config);
    }
    //
    CRef< CFilter_Base::C_FF::C_E > ce(new CFilter_Base::C_FF::C_E());
    ce->SetFile(*file);
    filter->SetFF().SetFF().push_back(ce);

#endif
}

CSourceFileToProjectInserter::TPch 
CSourceFileToProjectInserter::DefinePchUsage(const string& project_dir,
                                             const string& rel_source_file)
{
    // Check global permission
    if ( !GetApp().GetMetaMakefile().IsPchEnabled() )
        return TPch(eNotUse, "");

    string abs_source_file = 
        CDirEntry::ConcatPath(project_dir, rel_source_file);
    abs_source_file = CDirEntry::NormalizePath(abs_source_file);

    // .c files - not use PCH
    string ext;
    CDirEntry::SplitPath(abs_source_file, NULL, NULL, &ext);
    if ( NStr::CompareNocase(ext, ".c") == 0)
        return TPch(eNotUse, "");

    // PCH usage is defined in msvc master makefile
    string pch_file = 
        GetApp().GetMetaMakefile().GetUsePchThroughHeader
                                       (m_ProjectId,
                                        abs_source_file, 
                                        GetApp().GetProjectTreeInfo().m_Src);
    // No PCH - not use
    if ( pch_file.empty() )
        return TPch(eNotUse, "");

    if (m_PchHeaders.find(pch_file) == m_PchHeaders.end()) {
        // New PCH - this source file will create it
        m_PchHeaders.insert(pch_file);
        return TPch(eCreate, pch_file);
    } else {
        // Use PCH (previously created)
        return TPch(eUse, pch_file);
    }
}

void AddCustomBuildFileToFilter(CRef<CFilter>&          filter, 
                                const list<SConfigInfo> configs,
                                const string&           project_dir,
                                const SCustomBuildInfo& build_info)
{
    CRef<CFFile> file(new CFFile());
    file->SetAttlist().SetRelativePath
        (CDirEntry::CreateRelativePath(project_dir, 
                                       build_info.m_SourceFile));

    ITERATE(list<SConfigInfo>, n , configs) {
        // Iterate all configurations
        const string& config = (*n).m_Name;

        CRef<CFileConfiguration> file_config(new CFileConfiguration());
        file_config->SetAttlist().SetName(ConfigName(config));

        CRef<CTool> custom_build(new CTool(""));
        custom_build->SetAttlist().SetName("VCCustomBuildTool");
        custom_build->SetAttlist().SetDescription(build_info.m_Description);
        custom_build->SetAttlist().SetCommandLine(build_info.m_CommandLine);
        custom_build->SetAttlist().SetOutputs(build_info.m_Outputs);
        custom_build->SetAttlist().SetAdditionalDependencies
                                      (build_info.m_AdditionalDependencies);
        file_config->SetTool(*custom_build);

        file->SetFileConfiguration().push_back(file_config);
    }
    CRef< CFilter_Base::C_FF::C_E > ce(new CFilter_Base::C_FF::C_E());
    ce->SetFile(*file);
    filter->SetFF().SetFF().push_back(ce);
}


bool SameRootDirs(const string& dir1, const string& dir2)
{
    if ( dir1.empty() )
        return false;
    if ( dir2.empty() )
        return false;

    return dir1[0] == dir2[0];
}


void CreateUtilityProject(const string&            name, 
                          const list<SConfigInfo>& configs, 
                          CVisualStudioProject*    project)
{
    
    {{
        //Attributes:
        project->SetAttlist().SetProjectType  (MSVC_PROJECT_PROJECT_TYPE);
        project->SetAttlist().SetVersion      (MSVC_PROJECT_VERSION);
        project->SetAttlist().SetName         (name);
        project->SetAttlist().SetRootNamespace
            (MSVC_MASTERPROJECT_ROOT_NAMESPACE);
        project->SetAttlist().SetKeyword      (MSVC_MASTERPROJECT_KEYWORD);
    }}
    
    {{
        //Platforms
         CRef<CPlatform> platform(new CPlatform(""));
         platform->SetAttlist().SetName(MSVC_PROJECT_PLATFORM);
         project->SetPlatforms().SetPlatform().push_back(platform);
    }}

    ITERATE(list<SConfigInfo>, p , configs) {
        // Iterate all configurations
        const string& config = (*p).m_Name;
        
        CRef<CConfiguration> conf(new CConfiguration());

#define SET_ATTRIBUTE( node, X, val ) node->SetAttlist().Set##X(val)        

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
            
            SET_ATTRIBUTE(conf, ManagedExtensions,  "TRUE");
        }}

        {{
            //VCCustomBuildTool
            CRef<CTool> tool(new CTool(""));
            SET_ATTRIBUTE(tool, Name, "VCCustomBuildTool" );
            conf->SetTool().push_back(tool);
        }}
        {{
            //VCMIDLTool
            CRef<CTool> tool(new CTool(""));
            SET_ATTRIBUTE(tool, Name, "VCMIDLTool" );
            conf->SetTool().push_back(tool);
        }}
        {{
            //VCPostBuildEventTool
            CRef<CTool> tool(new CTool(""));
            SET_ATTRIBUTE(tool, Name, "VCPostBuildEventTool" );
            conf->SetTool().push_back(tool);
        }}
        {{
            //VCPreBuildEventTool
            CRef<CTool> tool(new CTool(""));
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


string CreateProjectName(const CProjKey& project_id)
{
    switch (project_id.Type()) {
    case CProjKey::eApp:
        return project_id.Id() + ".exe";
    case CProjKey::eLib:
        return project_id.Id() + ".lib";
    case CProjKey::eDll:
        return project_id.Id() + ".dll";
    case CProjKey::eMsvc:
        return project_id.Id() + ".vcproj";
    default:
        NCBI_THROW(CProjBulderAppException, eProjectType, project_id.Id());
        return "";
    }
}


//-----------------------------------------------------------------------------
CBuildType::CBuildType(bool dll_flag)
    :m_Type(dll_flag? eDll: eStatic)
{
    
}


CBuildType::TBuildType CBuildType::GetType(void) const
{
    return m_Type;
}
    

string CBuildType::GetTypeStr(void) const
{
    switch (m_Type) {
    case eStatic:
        return "static";
    case eDll:
        return "dll";
    }

    NCBI_THROW(CProjBulderAppException, 
               eProjectType, 
               NStr::IntToString(m_Type));
    return "";
}




END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.22  2004/05/17 14:40:30  gorelenk
 * Changed implementation of CSourceFileToProjectInserter::operator():
 * get rid of hard-coded define for PCH usage.
 *
 * Revision 1.21  2004/05/13 16:15:24  gorelenk
 * Changed CSourceFileToProjectInserter::operator() .
 *
 * Revision 1.20  2004/05/10 19:53:43  gorelenk
 * Changed CreateProjectName .
 *
 * Revision 1.19  2004/05/10 14:29:03  gorelenk
 * Implemented CSourceFileToProjectInserter .
 *
 * Revision 1.18  2004/04/22 18:15:38  gorelenk
 * Changed implementation of SourceFileExt .
 *
 * Revision 1.17  2004/03/10 16:42:44  gorelenk
 * Changed implementation of class CMsvc7RegSettings.
 *
 * Revision 1.16  2004/03/02 23:32:54  gorelenk
 * Added implemetation of class CBuildType member-functions.
 *
 * Revision 1.15  2004/02/20 22:53:26  gorelenk
 * Added analysis of ASN projects depends.
 *
 * Revision 1.14  2004/02/13 20:39:51  gorelenk
 * Minor cosmetic changes.
 *
 * Revision 1.13  2004/02/12 23:15:29  gorelenk
 * Implemented utility projects creation and configure re-build of the app.
 *
 * Revision 1.12  2004/02/12 16:27:57  gorelenk
 * Changed generation of command line for datatool.
 *
 * Revision 1.11  2004/02/10 18:09:12  gorelenk
 * Added definitions of functions SaveIfNewer and PromoteIfDifferent
 * - support for file overwriting only if it was changed.
 *
 * Revision 1.10  2004/02/06 23:14:59  gorelenk
 * Implemented support of ASN projects, semi-auto configure,
 * CPPFLAGS support. Second working version.
 *
 * Revision 1.9  2004/02/05 16:29:49  gorelenk
 * GetComponents was moved to class CMsvcSite.
 *
 * Revision 1.8  2004/02/04 23:35:17  gorelenk
 * Added definition of functions GetComponents and SameRootDirs.
 *
 * Revision 1.7  2004/02/03 17:19:04  gorelenk
 * Changed implementation of class CMsvc7RegSettings.
 * Added implementation of function GetComponents.
 *
 * Revision 1.6  2004/01/30 20:46:55  gorelenk
 * Added support of ASN projects.
 *
 * Revision 1.5  2004/01/28 17:55:49  gorelenk
 * += For msvc makefile support of :
 *                 Requires tag, ExcludeProject tag,
 *                 AddToProject section (SourceFiles and IncludeDirs),
 *                 CustomBuild section.
 * += For support of user local site.
 *
 * Revision 1.4  2004/01/26 19:27:28  gorelenk
 * += MSVC meta makefile support
 * += MSVC project makefile support
 *
 * Revision 1.3  2004/01/22 17:57:54  gorelenk
 * first version
 *
 * ===========================================================================
 */
