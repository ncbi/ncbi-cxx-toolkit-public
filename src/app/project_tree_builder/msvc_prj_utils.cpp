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
#include <serial/objostrxml.hpp>
#include <serial/objistr.hpp>
#include <serial/serial.hpp>



BEGIN_NCBI_SCOPE


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
    xs.SetEncoding(CObjectOStreamXml::eEncoding_Windows_1252);

    xs << project;
}


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


void SaveIfNewer(const string&               file_path, 
                 const CVisualStudioProject& project)
{
    // If no such file then simple write it
    if ( !CDirEntry(file_path).Exists() ) {
        SaveToXmlFile(file_path, project);
        LOG_POST(Info << "Created    : " << project.GetAttlist().GetName());
        return;
    }

    // Save new file to tmp path.
    string candidate_file_path = file_path + ".candidate";
    SaveToXmlFile(candidate_file_path, project);
    if (PromoteIfDifferent(file_path, candidate_file_path)) {
        LOG_POST(Info << "Modified   : " << project.GetAttlist().GetName());
    } else {
        LOG_POST(Info << "Left intact: " << project.GetAttlist().GetName());
    }
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
    ost.unsetf(ios::showbase);
    ost.setf  (ios::uppercase);
    ost << hex  << setw(12) << setfill('A') << m_Seed++ << ends << flush;
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
    :m_Debug(false), m_rtType(rtUnknown)
{
}

SConfigInfo::SConfigInfo(const string& name, 
                         bool debug, 
                         const string& runtime_library)
    :m_Name          (name),
     m_Debug         (debug),
     m_RuntimeLibrary(runtime_library),
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
        const string& config = (*iconfig).m_Name;
        CRef<CFileConfiguration> file_config(new CFileConfiguration());
        file_config->SetAttlist().SetName(ConfigName(config));
        if ( !enable_cfg.empty()  &&  enable_cfg != config ) {
            file_config->SetAttlist().SetExcludedFromBuild("TRUE");
        }

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
            const string& cfg = (*icfg).m_Name;
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

/*
 * ===========================================================================
 * $Log$
 * Revision 1.36  2005/04/29 14:11:12  gouriano
 * Added definition of runtime library type
 *
 * Revision 1.35  2004/12/20 21:07:33  gouriano
 * Eliminate compiler warnings
 *
 * Revision 1.34  2004/12/06 18:12:20  gouriano
 * Improved diagnostics
 *
 * Revision 1.33  2004/10/26 14:40:31  gouriano
 * Update ncbicfg.*.c only when needed
 *
 * Revision 1.32  2004/10/12 16:18:26  ivanov
 * Added configurable file support
 *
 * Revision 1.31  2004/06/15 19:01:08  gorelenk
 * Changed CGuidGenerator::Generate12Chars to fix compilation errors on
 * GCC 2.95.
 *
 * Revision 1.30  2004/06/14 20:59:23  gorelenk
 * Added *correct* conversion from streampos to size_t .
 *
 * Revision 1.29  2004/06/14 20:41:20  gorelenk
 * Changed PromoteIfDifferent : added conversion state to avoid compilation
 * errors on GCC 3.3.3 .
 *
 * Revision 1.28  2004/06/14 19:19:08  gorelenk
 * Changed definition of enum in CBuildType .
 *
 * Revision 1.27  2004/06/14 18:02:29  gorelenk
 * Changed size_t to streampos in PromoteIfDifferent .
 *
 * Revision 1.26  2004/06/08 16:32:25  gorelenk
 * Added static functions s_IsPrivateHeader and s_IsImplHeader.
 * Changed imlementation of SFiltersItem ( to take into account
 * new struct members ). Changed implementation of classes
 * CBasicProjectsFilesInserter and CDllProjectFilesInserter.
 *
 * Revision 1.25  2004/05/26 17:59:07  gorelenk
 * Old inserter -> CSrcToFilterInserterWithPch.
 * Implemented CBasicProjectsFilesInserter and CDllProjectFilesInserter .
 *
 * Revision 1.24  2004/05/21 21:41:41  gorelenk
 * Added PCH ncbi_pch.hpp
 *
 * Revision 1.23  2004/05/17 16:14:17  gorelenk
 * Implemented class CDllSrcFilesDistr .
 *
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
