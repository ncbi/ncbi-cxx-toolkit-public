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
    CDir(dir).CreatePath();

    CNcbiOfstream  ofs(file_path.c_str(), IOS_BASE::out | IOS_BASE::trunc);
    if (!ofs) {
	    NCBI_THROW(CProjBulderAppException, eFileCreation, file_path);
    }

    CObjectOStreamXml xs(ofs, false);
    xs.SetReferenceDTD(false);
    xs.SetEncoding(CObjectOStreamXml::eEncoding_Windows_1252);

    xs << project;
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
    string dir;
    string base;
    string ext;
    CDirEntry::SplitPath(file_path, &dir, &base, &ext);

    string source_file_prefix = CDirEntry::ConcatPath(dir, base);

    string file_path_cpp = source_file_prefix + ".cpp";
    if ( CFile(file_path_cpp).Exists() ) 
        return ".cpp";

    string file_path_c = source_file_prefix + ".c";
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


void AddCustomBuildFileToFilter(CRef<CFilter>&          filter, 
                                const list<SConfigInfo> configs,
                                const SCustomBuildInfo& build_info)
{
    CRef<CFFile> file(new CFFile());
    file->SetAttlist().SetRelativePath(build_info.m_SourceFile);

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
        file_config->SetTool(*custom_build);

        file->SetFileConfiguration().push_back(file_config);
    }
    CRef< CFilter_Base::C_FF::C_E > ce(new CFilter_Base::C_FF::C_E());
    ce->SetFile(*file);
    filter->SetFF().SetFF().push_back(ce);
}

END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
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
