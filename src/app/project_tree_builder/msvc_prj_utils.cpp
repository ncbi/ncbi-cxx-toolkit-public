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

#include <ncbi_pch.hpp>
#include <app/project_tree_builder/msvc_prj_utils.hpp>
#include <app/project_tree_builder/proj_builder_app.hpp>
#include <app/project_tree_builder/msvc_prj_defines.hpp>
#include <app/project_tree_builder/ptb_err_codes.hpp>

#ifdef NCBI_COMPILER_MSVC
#  include <serial/objostrxml.hpp>
#  include <serial/objistr.hpp>
#  include <serial/serial.hpp>
#endif



BEGIN_NCBI_SCOPE

#if NCBI_COMPILER_MSVC

CVisualStudioProject* LoadFromXmlFile(const string& file_path)
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


void SaveToXmlFile(const string&               file_path, 
                   const CVisualStudioProject& project)
{
    // Create dir if no such dir...
    string dir;
    CDirEntry::SplitPath(file_path, &dir);
    CDir project_dir(dir);
    if ( !project_dir.Exists() ) {
        CDir(dir).CreatePath();
    }

    CNcbiOfstream ofs(file_path.c_str(), 
                      IOS_BASE::out | IOS_BASE::trunc);
    if ( !ofs )
	    NCBI_THROW(CProjBulderAppException, eFileCreation, file_path);

    CObjectOStreamXml xs(ofs, false);
    xs.SetReferenceDTD(false);
    xs.SetEncoding(eEncoding_Windows_1252);

    xs << project;
}

void SaveIfNewer(const string&               file_path, 
                 const CVisualStudioProject& project)
{
    // If no such file then simple write it
    if ( !CDirEntry(file_path).Exists() ) {
        SaveToXmlFile(file_path, project);
        PTB_WARNING_EX(file_path, ePTB_FileModified,
                       "Project created");
        return;
    }

    // Save new file to tmp path.
    string candidate_file_path = file_path + ".candidate";
    SaveToXmlFile(candidate_file_path, project);
    if (PromoteIfDifferent(file_path, candidate_file_path)) {
        PTB_WARNING_EX(file_path, ePTB_FileModified,
                       "Project updated");
    } else {
        PTB_TRACE("Left intact: " << project.GetAttlist().GetName());
    }
}
#endif //NCBI_COMPILER_MSVC

bool PromoteIfDifferent(const string& present_path, 
                        const string& candidate_path)
{
    // Open both files
    CNcbiIfstream ifs_present(present_path.c_str(), 
                              IOS_BASE::in | IOS_BASE::binary);
    if ( !ifs_present ) {
        CDirEntry(present_path).Remove();
        CDirEntry(candidate_path).Rename(present_path);
        return true;
    }

    ifs_present.seekg(0, ios::end);
    size_t file_length_present = ifs_present.tellg() - streampos(0);

    ifs_present.seekg(0, ios::beg);

    CNcbiIfstream ifs_new (candidate_path.c_str(), 
                              IOS_BASE::in | IOS_BASE::binary);
    if ( !ifs_new ) {
        NCBI_THROW(CProjBulderAppException, eFileOpen, candidate_path);
    }

    ifs_new.seekg(0, ios::end);
    size_t file_length_new = ifs_new.tellg() - streampos(0);
    ifs_new.seekg(0, ios::beg);

    if (file_length_present != file_length_new) {
        ifs_present.close();
        ifs_new.close();
        CDirEntry(present_path).Remove();
        CDirEntry(candidate_path).Rename(present_path);
        return true;
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
        return true;
    } else {
        CDirEntry(candidate_path).Remove();
    }
    return false;
}

//-----------------------------------------------------------------------------

class CGuidGenerator
{
public:
    CGuidGenerator(void);
    ~CGuidGenerator(void);

    string DoGenerateSlnGUID();
    bool Insert(const string& guid, const string& path);
    const string& GetGuidUser(const string& guid) const;

private:
    string Generate12Chars(void);

    const string root_guid; // root GUID for MSVC solutions
    const string guid_base;
    unsigned int m_Seed;
    map<string,string> m_Trace;
};

CGuidGenerator guid_gen;

string GenerateSlnGUID(void)
{
    return guid_gen.DoGenerateSlnGUID();
}

string IdentifySlnGUID(const string& source_dir, const CProjKey& proj)
{
    string vcproj;
    if (proj.Type() == CProjKey::eMsvc) {
        vcproj = source_dir;
    } else {
        if (proj.Type() == CProjKey::eDll) {
            vcproj = source_dir;
        } else {
            vcproj = GetApp().GetProjectTreeInfo().m_Compilers;
            vcproj = CDirEntry::ConcatPath(vcproj, 
                GetApp().GetRegSettings().m_CompilersSubdir);
            vcproj = CDirEntry::ConcatPath(vcproj, 
                GetApp().GetBuildType().GetTypeStr());
            vcproj = CDirEntry::ConcatPath(vcproj,
                GetApp().GetRegSettings().m_ProjectsSubdir);
            vcproj = CDirEntry::ConcatPath(vcproj, 
                CDirEntry::CreateRelativePath(
                    GetApp().GetProjectTreeInfo().m_Src, source_dir));
        }
        vcproj = CDirEntry::AddTrailingPathSeparator(vcproj);
        vcproj += CreateProjectName(proj);
        vcproj += MSVC_PROJECT_FILE_EXT;
    }
    string guid;
    if ( CDirEntry(vcproj).Exists() ) {
        char   buf[1024];
        CNcbiIfstream is(vcproj.c_str(), IOS_BASE::in);
        if (is.is_open()) {
            while ( !is.eof() ) {
                is.getline(buf, sizeof(buf));
                buf[sizeof(buf)-1] = char(0);
                string data(buf);
                string::size_type start, end;
                start = data.find("ProjectGUID");
                if (start != string::npos) {
                    start = data.find('{',start);
                    if (start != string::npos) {
                        end = data.find('}',++start);
                        if (end != string::npos) {
                            guid = data.substr(start,end-start);
                        }
                    }
                    break;
                }
            }
        }
    }
    if (!guid.empty() && !guid_gen.Insert(guid,vcproj)) {
        PTB_WARNING_EX(vcproj, ePTB_ConfigurationError,
                     "MSVC Project GUID already in use by "
                     << guid_gen.GetGuidUser(guid));
        if (proj.Type() != CProjKey::eMsvc) {
            guid.clear();
        }
    }
    if (!guid.empty()) {
        return  "{" + guid + "}";
    }
    return guid;
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
    ost.unsetf(ios::showbase);
    ost.setf  (ios::uppercase);
    ost << hex  << setw(12) << setfill('A') << m_Seed++ << ends << flush;
    return ost.str();
}

bool CGuidGenerator::Insert(const string& guid, const string& path)
{
    if (!guid.empty() && guid != root_guid) {
        if (m_Trace.find(guid) == m_Trace.end()) {
            m_Trace[guid] = path;
            return true;
        }
        if (!path.empty()) {
            return m_Trace[guid] == path;
        }
    }
    return false;
}

const string& CGuidGenerator::GetGuidUser(const string& guid) const
{
    map<string,string>::const_iterator i = m_Trace.find(guid);
    if (i != m_Trace.end()) {
       return i->second;
    }
    return kEmptyStr;
}

string CGuidGenerator::DoGenerateSlnGUID(void)
{
    for ( ;; ) {
        //GUID prototype
        string proto = guid_base + Generate12Chars();
        if (Insert(proto,kEmptyStr)) {
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
    string file = file_path + ".cpp";
    if ( CFile(file).Exists() ) {
        return ".cpp";
    }
    file += ".in";
    if ( CFile(file).Exists() ) {
        return ".cpp.in";
    }
    file = file_path + ".c";
    if ( CFile(file).Exists() ) {
        return ".c";
    }
    file += ".in";
    if ( CFile(file).Exists() ) {
        return ".c.in";
    }
    return "";
}


//-----------------------------------------------------------------------------
SConfigInfo::SConfigInfo(void)
    :m_Debug(false), m_VTuneAddon(false), m_rtType(rtUnknown)
{
}

SConfigInfo::SConfigInfo(const string& name, 
                         bool debug, 
                         const string& runtime_library)
    :m_Name          (name),
     m_RuntimeLibrary(runtime_library),
     m_Debug         (debug),
     m_VTuneAddon(false),
     m_rtType(rtUnknown)
{
    DefineRtType();
}

void SConfigInfo::DefineRtType()
{
    if (m_RuntimeLibrary == "0") {
        m_rtType = rtMultiThreaded;
    } else if (m_RuntimeLibrary == "1") {
        m_rtType = rtMultiThreadedDebug;
    } else if (m_RuntimeLibrary == "2") {
        m_rtType = rtMultiThreadedDLL;
    } else if (m_RuntimeLibrary == "3") {
        m_rtType = rtMultiThreadedDebugDLL;
    } else if (m_RuntimeLibrary == "4") {
        m_rtType = rtSingleThreaded;
    } else if (m_RuntimeLibrary == "5") {
        m_rtType = rtSingleThreadedDebug;
    }
}

string SConfigInfo::GetConfigFullName(void) const
{
    if (m_VTuneAddon) {
        return string("VTune_") + m_Name;
    } else {
        return m_Name;
    }
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
        config.DefineRtType();
        configs->push_back(config);
        if (( config.m_Debug && GetApp().m_TweakVTuneD) ||
            (!config.m_Debug && GetApp().m_TweakVTuneR))
        {
            config.m_VTuneAddon = true;
            configs->push_back(config);
        }
    }
}


//-----------------------------------------------------------------------------
#if NCBI_COMPILER_MSVC

#if _MSC_VER >= 1400
CMsvc7RegSettings::EMsvcVersion CMsvc7RegSettings::sm_MsvcVersion =
    CMsvc7RegSettings::eMsvc800;
string CMsvc7RegSettings::sm_MsvcVersionName = "800";
#else
CMsvc7RegSettings::EMsvcVersion CMsvc7RegSettings::sm_MsvcVersion =
    CMsvc7RegSettings::eMsvc710;
string CMsvc7RegSettings::sm_MsvcVersionName = "710";
#endif

#ifdef _WIN64
CMsvc7RegSettings::EMsvcPlatform CMsvc7RegSettings::sm_MsvcPlatform =
    CMsvc7RegSettings::eMsvcX64;
string CMsvc7RegSettings::sm_MsvcPlatformName = "x64";
#else
CMsvc7RegSettings::EMsvcPlatform CMsvc7RegSettings::sm_MsvcPlatform =
    CMsvc7RegSettings::eMsvcWin32;
string CMsvc7RegSettings::sm_MsvcPlatformName = "Win32";
#endif

#else // NCBI_COMPILER_MSVC

CMsvc7RegSettings::EMsvcVersion CMsvc7RegSettings::sm_MsvcVersion =
    CMsvc7RegSettings::eMsvcNone;
string CMsvc7RegSettings::sm_MsvcVersionName = "none";

CMsvc7RegSettings::EMsvcPlatform CMsvc7RegSettings::sm_MsvcPlatform =
    CMsvc7RegSettings::eUnix;
string CMsvc7RegSettings::sm_MsvcPlatformName = "Unix";

#endif // NCBI_COMPILER_MSVC


string CMsvc7RegSettings::GetMsvcSection(void)
{
    if (GetMsvcPlatform() == eUnix) {
        return UNIX_REG_SECTION;
    } else {
        string s = string(MSVC_REG_SECTION) + GetMsvcVersionName();
        if (GetMsvcPlatform() != eMsvcWin32) {
            s += "." + GetMsvcPlatformName();
        }
        return s;
    }
}

CMsvc7RegSettings::CMsvc7RegSettings(void)
{
}

string CMsvc7RegSettings::GetProjectFileFormatVersion(void) const
{
    if (GetMsvcVersion() == eMsvc710) {
        return "7.10";
    } else if (GetMsvcVersion() == eMsvc800) {
        return "8.00";
    }
    return "";
}
string CMsvc7RegSettings::GetSolutionFileFormatVersion(void) const
{
    if (GetMsvcVersion() == eMsvc710) {
        return "8.00";
    } else if (GetMsvcVersion() == eMsvc800) {
        return "9.00";
    }
    return "";
}


bool IsSubdir(const string& abs_parent_dir, const string& abs_dir)
{
    return NStr::StartsWith(abs_dir, abs_parent_dir);
}


string GetOpt(const CPtbRegistry& registry, 
              const string& section, 
              const string& opt, 
              const string& config)
{
    string section_spec = section + '.' + config;
    string val_spec = registry.Get(section_spec, opt);
    if ( !val_spec.empty() )
        return val_spec;

    return registry.Get(section, opt);
}


string GetOpt(const CPtbRegistry& registry, 
              const string&        section, 
              const string&        opt, 
              const SConfigInfo&   config)
{
    const string& version = CMsvc7RegSettings::GetMsvcVersionName();
    const string& platform = CMsvc7RegSettings::GetMsvcPlatformName();
    string build = GetApp().GetBuildType().GetTypeStr();
    string spec = config.m_Debug ? "debug": "release";
    string value, s;

    s.assign(section).append(1,'.').append(build).append(1,'.').append(spec).append(1,'.').append(config.m_Name);
    value = registry.Get(s, opt); if (!value.empty()) { return value;}

    s.assign(section).append(1,'.').append(spec).append(1,'.').append(config.m_Name);
    value = registry.Get(s, opt); if (!value.empty()) { return value;}

    s.assign(section).append(1,'.').append(build).append(1,'.').append(spec);
    value = registry.Get(s, opt); if (!value.empty()) { return value;}

    s.assign(section).append(1,'.').append(version).append(1,'.').append(spec);
    value = registry.Get(s, opt); if (!value.empty()) { return value;}

    s.assign(section).append(1,'.').append(spec);
    value = registry.Get(s, opt); if (!value.empty()) { return value;}

    s.assign(section).append(1,'.').append(build);
    value = registry.Get(s, opt); if (!value.empty()) { return value;}

    s.assign(section).append(1,'.').append(platform);
    value = registry.Get(s, opt); if (!value.empty()) { return value;}

    s.assign(section).append(1,'.').append(version);
    value = registry.Get(s, opt); if (!value.empty()) { return value;}

    s.assign(section);
    value = registry.Get(s, opt); if (!value.empty()) { return value;}
    return value;
}



string ConfigName(const string& config)
{
    return config +'|'+ CMsvc7RegSettings::GetMsvcPlatformName();
}


//-----------------------------------------------------------------------------
#if NCBI_COMPILER_MSVC

CSrcToFilterInserterWithPch::CSrcToFilterInserterWithPch
                                        (const string&            project_id,
                                         const list<SConfigInfo>& configs,
                                         const string&            project_dir)
    : m_ProjectId  (project_id),
      m_Configs    (configs),
      m_ProjectDir (project_dir)
{
}


CSrcToFilterInserterWithPch::~CSrcToFilterInserterWithPch(void)
{
}


void CSrcToFilterInserterWithPch::InsertFile(CRef<CFilter>&  filter, 
                                             const string&   rel_source_file,
                                             const string&   enable_cfg)
{
    CRef<CFFile> file(new CFFile());
    file->SetAttlist().SetRelativePath(rel_source_file);
    //
    TPch pch_usage = DefinePchUsage(m_ProjectDir, rel_source_file);
    //
    // For each configuration
    ITERATE(list<SConfigInfo>, iconfig, m_Configs) {
        const string& config = (*iconfig).GetConfigFullName();
        CRef<CFileConfiguration> file_config(new CFileConfiguration());
        file_config->SetAttlist().SetName(ConfigName(config));
        if ( !enable_cfg.empty()  &&  enable_cfg != config ) {
            file_config->SetAttlist().SetExcludedFromBuild("TRUE");
        }

        CRef<CTool> compilerl_tool(new CTool());
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
            if (CMsvc7RegSettings::GetMsvcVersion() == CMsvc7RegSettings::eMsvc800) {
                compilerl_tool->SetAttlist().SetUsePrecompiledHeader("2");
            } else {
                compilerl_tool->SetAttlist().SetUsePrecompiledHeader("3");
            }
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
    CRef< CFilter_Base::C_FF::C_E > ce(new CFilter_Base::C_FF::C_E());
    ce->SetFile(*file);
    filter->SetFF().SetFF().push_back(ce);
    return;
}


void 
CSrcToFilterInserterWithPch::operator()(CRef<CFilter>&  filter, 
                                        const string&   rel_source_file)
{
    if ( NStr::Find(rel_source_file, ".@config@") == NPOS ) {
        // Ordinary file
        InsertFile(filter, rel_source_file);
    } else {
        // Configurable file

        // Exclude from build all file versions
        // except one for current configuration.
        ITERATE(list<SConfigInfo>, icfg, m_Configs) {
            const string& cfg = (*icfg).GetConfigFullName();
            string source_file = NStr::Replace(rel_source_file,
                                               ".@config@", "." + cfg);
            InsertFile(filter, source_file, cfg);
        }
    }
}

CSrcToFilterInserterWithPch::TPch 
CSrcToFilterInserterWithPch::DefinePchUsage(const string& project_dir,
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


//-----------------------------------------------------------------------------

CBasicProjectsFilesInserter::CBasicProjectsFilesInserter
                                (CVisualStudioProject*    vcproj,
                                const string&            project_id,
                                const list<SConfigInfo>& configs,
                                const string&            project_dir)
    : m_Vcproj     (vcproj),
      m_SrcInserter(project_id, configs, project_dir),
      m_Filters    (project_dir)
{
    m_Filters.Initilize();
}


CBasicProjectsFilesInserter::~CBasicProjectsFilesInserter(void)
{
}

void CBasicProjectsFilesInserter::AddSourceFile(const string& rel_file_path)
{
    m_Filters.AddSourceFile(m_SrcInserter, rel_file_path);
}

void CBasicProjectsFilesInserter::AddHeaderFile(const string& rel_file_path)
{
    m_Filters.AddHeaderFile(rel_file_path);
}

void CBasicProjectsFilesInserter::AddInlineFile(const string& rel_file_path)
{
    m_Filters.AddInlineFile(rel_file_path);
}

void CBasicProjectsFilesInserter::Finalize(void)
{
    m_Vcproj->SetFiles().SetFilter().push_back(m_Filters.m_SourceFiles);
    if ( m_Filters.m_HeaderFilesPrivate->IsSetFF() ) {
        CRef< CFilter_Base::C_FF::C_E > ce(new CFilter_Base::C_FF::C_E());
        ce->SetFilter(*m_Filters.m_HeaderFilesPrivate);
        m_Filters.m_HeaderFiles->SetFF().SetFF().push_back(ce);
    }
    if ( m_Filters.m_HeaderFilesImpl->IsSetFF() )  {
        CRef< CFilter_Base::C_FF::C_E > ce(new CFilter_Base::C_FF::C_E());
        ce->SetFilter(*m_Filters.m_HeaderFilesImpl);
        m_Filters.m_HeaderFiles->SetFF().SetFF().push_back(ce);
    }
    m_Vcproj->SetFiles().SetFilter().push_back(m_Filters.m_HeaderFiles);
    m_Vcproj->SetFiles().SetFilter().push_back(m_Filters.m_InlineFiles);
}

//-----------------------------------------------------------------------------

static bool s_IsPrivateHeader(const string& header_abs_path)
{
    string src_dir = GetApp().GetProjectTreeInfo().m_Src;
    return header_abs_path.find(src_dir) != NPOS;

}

static bool s_IsImplHeader(const string& header_abs_path)
{
    string src_trait = CDirEntry::GetPathSeparator()        +
                       GetApp().GetProjectTreeInfo().m_Impl +
                       CDirEntry::GetPathSeparator();
    return header_abs_path.find(src_trait) != NPOS;
}

//-----------------------------------------------------------------------------

CBasicProjectsFilesInserter::SFiltersItem::SFiltersItem(void)
{
}
CBasicProjectsFilesInserter::SFiltersItem::SFiltersItem
                                                    (const string& project_dir)
    :m_ProjectDir(project_dir)
{
}

void CBasicProjectsFilesInserter::SFiltersItem::Initilize(void)
{
    m_SourceFiles.Reset(new CFilter());
    m_SourceFiles->SetAttlist().SetName("Source Files");
    m_SourceFiles->SetAttlist().SetFilter
            ("cpp;c;cxx;def;odl;idl;hpj;bat;asm;asmx");

    m_HeaderFiles.Reset(new CFilter());
    m_HeaderFiles->SetAttlist().SetName("Header Files");
    m_HeaderFiles->SetAttlist().SetFilter("h;hpp;hxx;hm;inc;xsd");

    m_HeaderFilesPrivate.Reset(new CFilter());
    m_HeaderFilesPrivate->SetAttlist().SetName("Private");
    m_HeaderFilesPrivate->SetAttlist().SetFilter("h;hpp;hxx;hm;inc;xsd");

    m_HeaderFilesImpl.Reset(new CFilter());
    m_HeaderFilesImpl->SetAttlist().SetName("Impl");
    m_HeaderFilesImpl->SetAttlist().SetFilter("h;hpp;hxx;hm;inc;xsd");

    m_InlineFiles.Reset(new CFilter());
    m_InlineFiles->SetAttlist().SetName("Inline Files");
    m_InlineFiles->SetAttlist().SetFilter("inl");
}


void CBasicProjectsFilesInserter::SFiltersItem::AddSourceFile
                           (CSrcToFilterInserterWithPch& inserter_w_pch,
                            const string&                rel_file_path)
{
    inserter_w_pch(m_SourceFiles, rel_file_path);
}

void CBasicProjectsFilesInserter::SFiltersItem::AddHeaderFile
                                                  (const string& rel_file_path)
{
    CRef< CFFile > file(new CFFile());
    file->SetAttlist().SetRelativePath(rel_file_path);

    CRef< CFilter_Base::C_FF::C_E > ce(new CFilter_Base::C_FF::C_E());
    ce->SetFile(*file);
    
    string abs_header_path = 
        CDirEntry::ConcatPath(m_ProjectDir, rel_file_path);
    abs_header_path = CDirEntry::NormalizePath(abs_header_path);
    if ( s_IsPrivateHeader(abs_header_path) ) {
        m_HeaderFilesPrivate->SetFF().SetFF().push_back(ce);
    } else if ( s_IsImplHeader(abs_header_path) ) {
        m_HeaderFilesImpl->SetFF().SetFF().push_back(ce);
    } else {
        m_HeaderFiles->SetFF().SetFF().push_back(ce);
    }
}


void CBasicProjectsFilesInserter::SFiltersItem::AddInlineFile
                                                  (const string& rel_file_path)
{
    CRef< CFFile > file(new CFFile());
    file->SetAttlist().SetRelativePath(rel_file_path);

    CRef< CFilter_Base::C_FF::C_E > ce(new CFilter_Base::C_FF::C_E());
    ce->SetFile(*file);
    m_InlineFiles->SetFF().SetFF().push_back(ce);
}


//-----------------------------------------------------------------------------

CDllProjectFilesInserter::CDllProjectFilesInserter
                                (CVisualStudioProject*    vcproj,
                                 const CProjKey           dll_project_key,
                                 const list<SConfigInfo>& configs,
                                 const string&            project_dir)
    :m_Vcproj        (vcproj),
     m_DllProjectKey (dll_project_key),
     m_SrcInserter   (dll_project_key.Id(), 
                      configs, 
                      project_dir),
     m_ProjectDir    (project_dir),
     m_PrivateFilters(project_dir)
{
    // Private filters initilization
    m_PrivateFilters.m_SourceFiles.Reset(new CFilter());
    m_PrivateFilters.m_SourceFiles->SetAttlist().SetName("Source Files");
    m_PrivateFilters.m_SourceFiles->SetAttlist().SetFilter
            ("cpp;c;cxx;def;odl;idl;hpj;bat;asm;asmx");

    m_PrivateFilters.m_HeaderFiles.Reset(new CFilter());
    m_PrivateFilters.m_HeaderFiles->SetAttlist().SetName("Header Files");
    m_PrivateFilters.m_HeaderFiles->SetAttlist().SetFilter("h;hpp;hxx;hm;inc;xsd");

    m_PrivateFilters.m_HeaderFilesPrivate.Reset(new CFilter());
    m_PrivateFilters.m_HeaderFilesPrivate->SetAttlist().SetName("Private");
    m_PrivateFilters.m_HeaderFilesPrivate->SetAttlist().SetFilter("h;hpp;hxx;hm;inc;xsd");
    
    m_PrivateFilters.m_HeaderFilesImpl.Reset(new CFilter());
    m_PrivateFilters.m_HeaderFilesImpl->SetAttlist().SetName("Impl");
    m_PrivateFilters.m_HeaderFilesImpl->SetAttlist().SetFilter("h;hpp;hxx;hm;inc;xsd");

    m_PrivateFilters.m_InlineFiles.Reset(new CFilter());
    m_PrivateFilters.m_InlineFiles->SetAttlist().SetName("Inline Files");
    m_PrivateFilters.m_InlineFiles->SetAttlist().SetFilter("inl");

    // Hosted Libraries filter (folder)
    m_HostedLibrariesRootFilter.Reset(new CFilter());
    m_HostedLibrariesRootFilter->SetAttlist().SetName("Hosted Libraries");
    m_HostedLibrariesRootFilter->SetAttlist().SetFilter("");
}


CDllProjectFilesInserter::~CDllProjectFilesInserter(void)
{
}


void CDllProjectFilesInserter::AddSourceFile (const string& rel_file_path)
{
    string abs_path = CDirEntry::ConcatPath(m_ProjectDir, rel_file_path);
    abs_path = CDirEntry::NormalizePath(abs_path);
    
    CProjKey proj_key = GetApp().GetDllFilesDistr().GetSourceLib(abs_path, m_DllProjectKey);
    
    if (proj_key == CProjKey()) {
        m_PrivateFilters.AddSourceFile(m_SrcInserter, rel_file_path);
        return;
    }

    THostedLibs::iterator p = m_HostedLibs.find(proj_key);
    if (p != m_HostedLibs.end()) {
        TFiltersItem& filters_item = p->second;
        filters_item.AddSourceFile(m_SrcInserter, rel_file_path);
        return;
    }

    TFiltersItem new_item(m_ProjectDir);
    new_item.Initilize();
    new_item.AddSourceFile(m_SrcInserter, rel_file_path);
    m_HostedLibs[proj_key] = new_item;
}

void CDllProjectFilesInserter::AddHeaderFile(const string& rel_file_path)
{
    string abs_path = CDirEntry::ConcatPath(m_ProjectDir, rel_file_path);
    abs_path = CDirEntry::NormalizePath(abs_path);
    
    CProjKey proj_key = GetApp().GetDllFilesDistr().GetHeaderLib(abs_path, m_DllProjectKey);
    
    if (proj_key == CProjKey()) {
        m_PrivateFilters.AddHeaderFile(rel_file_path);
        return;
    }

    THostedLibs::iterator p = m_HostedLibs.find(proj_key);
    if (p != m_HostedLibs.end()) {
        TFiltersItem& filters_item = p->second;
        filters_item.AddHeaderFile(rel_file_path);
        return;
    }

    TFiltersItem new_item(m_ProjectDir);
    new_item.Initilize();
    new_item.AddHeaderFile(rel_file_path);
    m_HostedLibs[proj_key] = new_item;
}

void CDllProjectFilesInserter::AddInlineFile(const string& rel_file_path)
{
    string abs_path = CDirEntry::ConcatPath(m_ProjectDir, rel_file_path);
    abs_path = CDirEntry::NormalizePath(abs_path);
    
    CProjKey proj_key = GetApp().GetDllFilesDistr().GetInlineLib(abs_path, m_DllProjectKey);
    
    if (proj_key == CProjKey()) {
        m_PrivateFilters.AddInlineFile(rel_file_path);
        return;
    }

    THostedLibs::iterator p = m_HostedLibs.find(proj_key);
    if (p != m_HostedLibs.end()) {
        TFiltersItem& filters_item = p->second;
        filters_item.AddInlineFile(rel_file_path);
        return;
    }

    TFiltersItem new_item(m_ProjectDir);
    new_item.Initilize();
    new_item.AddInlineFile(rel_file_path);
    m_HostedLibs[proj_key] = new_item;
}


void CDllProjectFilesInserter::Finalize(void)
{
    m_Vcproj->SetFiles().SetFilter().push_back(m_PrivateFilters.m_SourceFiles);

    if ( !m_PrivateFilters.m_HeaderFilesPrivate->IsSetFF() ) {
        CRef< CFilter_Base::C_FF::C_E > ce(new CFilter_Base::C_FF::C_E());
        ce->SetFilter(*m_PrivateFilters.m_HeaderFilesPrivate);
        m_PrivateFilters.m_HeaderFiles->SetFF().SetFF().push_back(ce);
    }
    if ( !m_PrivateFilters.m_HeaderFilesImpl->IsSetFF() ) {
        CRef< CFilter_Base::C_FF::C_E > ce(new CFilter_Base::C_FF::C_E());
        ce->SetFilter(*m_PrivateFilters.m_HeaderFilesImpl);
        m_PrivateFilters.m_HeaderFiles->SetFF().SetFF().push_back(ce);
    }
    m_Vcproj->SetFiles().SetFilter().push_back(m_PrivateFilters.m_HeaderFiles);

    m_Vcproj->SetFiles().SetFilter().push_back(m_PrivateFilters.m_InlineFiles);

    NON_CONST_ITERATE(THostedLibs, p, m_HostedLibs) {

        const CProjKey& proj_key     = p->first;
        TFiltersItem&   filters_item = p->second;

        CRef<CFilter> hosted_lib_filter(new CFilter());
        hosted_lib_filter->SetAttlist().SetName(CreateProjectName(proj_key));
        hosted_lib_filter->SetAttlist().SetFilter("");

        {{
            CRef< CFilter_Base::C_FF::C_E > ce(new CFilter_Base::C_FF::C_E());
            ce->SetFilter(*(filters_item.m_SourceFiles));
            hosted_lib_filter->SetFF().SetFF().push_back(ce);
        }}

        if ( filters_item.m_HeaderFilesPrivate->IsSetFF() ) {
            CRef< CFilter_Base::C_FF::C_E > ce(new CFilter_Base::C_FF::C_E());
            ce->SetFilter(*filters_item.m_HeaderFilesPrivate);
            filters_item.m_HeaderFiles->SetFF().SetFF().push_back(ce);
        }
        if ( filters_item.m_HeaderFilesImpl->IsSetFF() ) {
            CRef< CFilter_Base::C_FF::C_E > ce(new CFilter_Base::C_FF::C_E());
            ce->SetFilter(*filters_item.m_HeaderFilesImpl);
            filters_item.m_HeaderFiles->SetFF().SetFF().push_back(ce);
        }
        {{
            CRef< CFilter_Base::C_FF::C_E > ce(new CFilter_Base::C_FF::C_E());
            ce->SetFilter(*(filters_item.m_HeaderFiles));
            hosted_lib_filter->SetFF().SetFF().push_back(ce);
        }}
        {{
            CRef< CFilter_Base::C_FF::C_E > ce(new CFilter_Base::C_FF::C_E());
            ce->SetFilter(*(filters_item.m_InlineFiles));
            hosted_lib_filter->SetFF().SetFF().push_back(ce);
        }}
        {{
            CRef< CFilter_Base::C_FF::C_E > ce(new CFilter_Base::C_FF::C_E());
            ce->SetFilter(*hosted_lib_filter);
            m_HostedLibrariesRootFilter->SetFF().SetFF().push_back(ce);
        }}
    }
    m_Vcproj->SetFiles().SetFilter().push_back(m_HostedLibrariesRootFilter);
}


//-----------------------------------------------------------------------------

void AddCustomBuildFileToFilter(CRef<CFilter>&          filter, 
                                const list<SConfigInfo> configs,
                                const string&           project_dir,
                                const SCustomBuildInfo& build_info)
{
    CRef<CFFile> file(new CFFile());
    file->SetAttlist().SetRelativePath
        (CDirEntry::CreateRelativePath(project_dir, 
                                       build_info.m_SourceFile));

    ITERATE(list<SConfigInfo>, n, configs) {
        // Iterate all configurations
        const string& config = (*n).GetConfigFullName();

        CRef<CFileConfiguration> file_config(new CFileConfiguration());
        file_config->SetAttlist().SetName(ConfigName(config));

        CRef<CTool> custom_build(new CTool());
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
            SET_ATTRIBUTE(conf, ManagedExtensions,  "TRUE");
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
#endif //NCBI_COMPILER_MSVC


bool SameRootDirs(const string& dir1, const string& dir2)
{
    if ( dir1.empty() )
        return false;
    if ( dir2.empty() )
        return false;
    return dir1[0] == dir2[0];
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
        return project_id.Id();// + ".vcproj";
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


CBuildType::EBuildType CBuildType::GetType(void) const
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


//-----------------------------------------------------------------------------

CDllSrcFilesDistr::CDllSrcFilesDistr(void)
{
}

void CDllSrcFilesDistr::RegisterSource(const string&   src_file_path, 
                                       const CProjKey& dll_project_id,
                                       const CProjKey& lib_project_id)
{
    m_SourcesMap[ TDllSrcKey(src_file_path,dll_project_id) ] = lib_project_id;
}

void CDllSrcFilesDistr::RegisterHeader(const string&   hdr_file_path, 
                                       const CProjKey& dll_project_id,
                                       const CProjKey& lib_project_id)
{
    m_HeadersMap[ TDllSrcKey(hdr_file_path,dll_project_id) ] = lib_project_id;
}

void CDllSrcFilesDistr::RegisterInline(const string&   inl_file_path, 
                                       const CProjKey& dll_project_id,
                                       const CProjKey& lib_project_id)
{
    m_InlinesMap[ TDllSrcKey(inl_file_path,dll_project_id) ] = lib_project_id;
}

CProjKey CDllSrcFilesDistr::GetSourceLib(const string&   src_file_path, 
                                         const CProjKey& dll_project_id) const
{
    TDllSrcKey key(src_file_path, dll_project_id);
    TDistrMap::const_iterator p = m_SourcesMap.find(key);
    if (p != m_SourcesMap.end()) {
        const CProjKey& lib_id = p->second;
        return lib_id;
    }
    return CProjKey();
}


CProjKey CDllSrcFilesDistr::GetHeaderLib(const string&   hdr_file_path, 
                                         const CProjKey& dll_project_id) const
{
    TDllSrcKey key(hdr_file_path, dll_project_id);
    TDistrMap::const_iterator p = m_HeadersMap.find(key);
    if (p != m_HeadersMap.end()) {
        const CProjKey& lib_id = p->second;
        return lib_id;
    }
    return CProjKey();
}

CProjKey CDllSrcFilesDistr::GetInlineLib(const string&   inl_file_path, 
                                         const CProjKey& dll_project_id) const
{
    TDllSrcKey key(inl_file_path, dll_project_id);
    TDistrMap::const_iterator p = m_InlinesMap.find(key);
    if (p != m_InlinesMap.end()) {
        const CProjKey& lib_id = p->second;
        return lib_id;
    }
    return CProjKey();
}


END_NCBI_SCOPE
