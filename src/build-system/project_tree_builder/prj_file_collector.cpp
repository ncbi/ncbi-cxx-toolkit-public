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
 * Author:  Andrei Gourianov
 *
 */

#include <ncbi_pch.hpp>

#include "proj_builder_app.hpp"
#include "prj_file_collector.hpp"
#include "configurable_file.hpp"
#include "ptb_err_codes.hpp"
#include "msvc_prj_defines.hpp"

BEGIN_NCBI_SCOPE

#if defined(NCBI_XCODE_BUILD) || defined(PSEUDO_XCODE)

bool s_Name_less(const string& x, const string& y)
{
    string base_x, base_y;
    CDirEntry::SplitPath(x, NULL, &base_x);
    CDirEntry::SplitPath(y, NULL, &base_y);
    return NStr::CompareNocase(base_x, base_y) < 0;
}

CProjectFileCollector::CProjectFileCollector(const CProjItem& prj,
    const list<SConfigInfo>& configs, const string& output_dir)
    :m_ProjItem(prj), m_ProjContext(prj), m_Configs(configs),
     m_OutputDir(output_dir)
{
}

CProjectFileCollector::~CProjectFileCollector(void)
{
}

bool CProjectFileCollector::CheckProjectConfigs(void)
{
    string str_log, req_log;
    int failed=0;
    m_EnabledConfigs.clear();
    ITERATE(list<SConfigInfo>, p , m_Configs) {
        const SConfigInfo& cfg_info = *p;
        string unmet, unmet_req;
        // Check config availability
        if ( !m_ProjContext.IsConfigEnabled(cfg_info, &unmet, &unmet_req) ) {
            str_log += " " + cfg_info.GetConfigFullName() + "(because of " + unmet + ")";
        } else {
            if (!unmet_req.empty()) {
                ++failed;
                req_log += " " + cfg_info.GetConfigFullName() + "(because of " + unmet_req + ")";
            }
            m_EnabledConfigs.push_back(cfg_info);
        }
    }
    string path = CDirEntry::ConcatPath(m_ProjItem.m_SourcesBaseDir, "Makefile.");
    path += m_ProjItem.m_Name;
    switch (m_ProjItem.m_ProjType) {
    case CProjKey::eApp:
        path += ".app";
        break;
    case CProjKey::eLib:
        path += ".lib";
        break;
    case CProjKey::eDll:
        path += ".dll";
        break;
    default:
        break;
    }
    if (!str_log.empty()) {
        PTB_WARNING_EX(path, ePTB_ConfigurationError,
                       "Disabled configurations: " << str_log);
    }
    if (!req_log.empty()) {
        PTB_WARNING_EX(path, ePTB_ConfigurationError,
                       "Invalid configurations: " << req_log);
    }
    if (m_EnabledConfigs.empty()) {
        PTB_WARNING_EX(path, ePTB_ConfigurationError,
                       "Disabled all configurations for project " << m_ProjItem.m_Name);
    }
    if (failed == m_Configs.size()) {
//        m_ProjItem.m_MakeType = eMakeType_ExcludedByReq;
        PTB_WARNING_EX(path, ePTB_ConfigurationError,
                       "All build configurations are invalid, project excluded: " << m_ProjItem.m_Name);
    }
    return !m_EnabledConfigs.empty() && failed != m_Configs.size();
}

void CProjectFileCollector::DoCollect(void)
{
    CollectSources();
    CollectHeaders();
    CollectDataSpecs();
}

void CProjectFileCollector::CollectSources(void)
{
    list<string> sources;
    ITERATE(list<string>, p, m_ProjItem.m_Sources) {
        string src_path = CDirEntry::NormalizePath(
            CDirEntry::ConcatPath(m_ProjItem.m_SourcesBaseDir, *p));
        string ext(GetFileExtension(src_path));
        if (ext.empty() && 
            (IsProducedByDatatool(m_ProjItem,src_path) ||
             IsInsideDatatoolSourceDir(src_path))) {
            ext = ".cpp";
        }
        if (!ext.empty()) {
            src_path += ext;
            sources.push_back(src_path);
        }
    }

    list<string> included_sources;
    m_ProjContext.GetMsvcProjectMakefile().GetAdditionalSourceFiles(
        SConfigInfo(),&included_sources);
    ITERATE(list<string>, p, included_sources) {
        string src_path = CDirEntry::NormalizePath(
            CDirEntry::ConcatPath(m_ProjItem.m_SourcesBaseDir, *p));
        string ext(GetFileExtension(src_path));
        if (ext.empty() && IsProducedByDatatool(m_ProjItem,src_path)) {
            ext = ".cpp";
        }
        if (!ext.empty()) {
            src_path += ext;
            sources.push_back(src_path);
        }
    }
    m_Sources.clear();
    m_ConfigurableSources.clear();
    ITERATE(list<string>, p, sources) {
        if ( NStr::EndsWith(*p, ".in") ) {
            CDirEntry ent( NStr::Replace( *p, ".in", ""));
            string dest_path = CDirEntry::ConcatPath( m_OutputDir, m_ProjItem.m_ID);
            dest_path = CDirEntry::ConcatPath( dest_path, ent.GetBase());
            GetApp().SetConfFileData(*p, dest_path);
            ITERATE(list<SConfigInfo>, cfg, m_Configs) {
                const SConfigInfo& cfg_info = *cfg;
                string dest_file = dest_path + "." +
                    ConfigurableFileSuffix(cfg_info.GetConfigFullName())+
                    ent.GetExt();
#if 0
                CreateConfigurableFile(*p, dest_file, cfg_info.GetConfigFullName());
#else
// we postpone creation until later
// here we only create placeholders
                if (!CFile(dest_file).Exists()) {
                    CNcbiOfstream os(dest_file.c_str(),
                                     IOS_BASE::out | IOS_BASE::binary | IOS_BASE::trunc);
                }
#endif
            }
            dest_path += ent.GetExt();
            m_ConfigurableSources.push_back( dest_path );
            m_Sources.push_back( dest_path );
        } else {
            m_Sources.push_back( *p);
        }
    }
    m_Sources.sort(s_Name_less);
}

void CProjectFileCollector::CollectHeaders(void)
{
    m_Headers.clear();
    list<string> all_headers;
    copy(m_ProjContext.IncludeDirsAbs().begin(),
         m_ProjContext.IncludeDirsAbs().end(), 
         back_inserter(all_headers));
    copy(m_ProjContext.InlineDirsAbs().begin(),
         m_ProjContext.InlineDirsAbs().end(), 
         back_inserter(all_headers));

    ITERATE(list<string>, f, all_headers) {
        string value(*f), pdir, base, ext;
        if (value.empty()) {
            continue;
        }
        SIZE_TYPE negation_pos = value.find('!');
        bool remove = negation_pos != NPOS;
        if (remove) {
            value = NStr::Replace(value, "!", kEmptyStr);
            if (value.empty() ||
                value[value.length()-1] == CDirEntry::GetPathSeparator()) {
                continue;
            }
        }
        CDirEntry::SplitPath(value, &pdir, &base, &ext);
        CDir dir(pdir);
        if ( !dir.Exists() ) {
            continue;
        }
        CDir::TEntries contents = dir.GetEntries(base + ext);
        ITERATE(CDir::TEntries, i, contents) {
            if ( (*i)->IsFile() ) {
                if (remove) {
                    m_Headers.remove( (*i)->GetPath() );
                } else {
                    m_Headers.push_back( (*i)->GetPath() );
                }
            }
        }
    }
    m_Headers.sort(s_Name_less);
}

void CProjectFileCollector::CollectDataSpecs(void)
{
    m_DataSpecs.clear();
    ITERATE(list<CDataToolGeneratedSrc>, d, m_ProjItem.m_DatatoolSources) {
        m_DataSpecs.push_back(CDirEntry::ConcatPath(d->m_SourceBaseDir, d->m_SourceFile));
            NStr::Join(d->m_ImportModules, " ");
    }
}

bool CProjectFileCollector::GetIncludeDirs(list<string>& inc_dirs, const SConfigInfo& cfg) const
{
    inc_dirs.clear();
    string alldirs = m_ProjContext.AdditionalIncludeDirectories(cfg);
    list<string> dirs;
    NStr::Split(alldirs, LIST_SEPARATOR, dirs);
    ITERATE( list<string>, i, dirs) {
        string dir;
#ifdef PSEUDO_XCODE
        if (NStr::StartsWith(*i, '/')) {
#else
        if (CDirEntry::IsAbsolutePath(*i)) {
#endif
            dir = *i;
        } else {
            dir = CDirEntry::NormalizePath(
                CDirEntry::ConcatPath( m_ProjContext.ProjectDir(), *i));
        }
        inc_dirs.push_back( dir);
    }
    return !inc_dirs.empty();
}

bool CProjectFileCollector::GetLibraryDirs(list<string>& lib_dirs, const SConfigInfo& cfg) const
{
    lib_dirs.clear();
    string alldirs = m_ProjContext.AdditionalLibraryDirectories(cfg);
    list<string> dirs;
    NStr::Split(alldirs, LIST_SEPARATOR, dirs);
    ITERATE( list<string>, i, dirs) {
        string dir;
#ifdef PSEUDO_XCODE
        if (NStr::StartsWith(*i, '/')) {
#else
        if (CDirEntry::IsAbsolutePath(*i)) {
#endif
            dir = *i;
        } else {
            dir = CDirEntry::NormalizePath(
                CDirEntry::ConcatPath( m_ProjContext.ProjectDir(), *i));
        }
        lib_dirs.push_back( dir);
    }
    return !lib_dirs.empty();
}

string CProjectFileCollector::GetDataSpecImports(const string& spec) const
{
    string file( CDirEntry(spec).GetName());
    ITERATE(list<CDataToolGeneratedSrc>, d, m_ProjItem.m_DatatoolSources) {
        if (d->m_SourceFile == file) {
            return NStr::Join(d->m_ImportModules, " ");
        }
    }
    return kEmptyStr;
}

string CProjectFileCollector::GetFileExtension(const string& file)
{
    string ext;
    CDirEntry::SplitPath(file, NULL, NULL, &ext);
    
    if (!ext.empty()) {
        bool explicit_c   = NStr::CompareNocase(ext, ".c"  )== 0;
        if (explicit_c  &&  CFile(file).Exists()) {
            return ".c";
        }
        bool explicit_cpp = NStr::CompareNocase(ext, ".cpp")== 0;
        if (explicit_cpp  &&  CFile(file).Exists()) {
            return ".cpp";
        }
    }
    string ext_in[]  = {".cpp", ".cpp.in", ".c", ".c.in", kEmptyStr};
    for (int i=0; !ext_in[i].empty(); ++i) {
        if ( CFile(file + ext_in[i]).Exists() ) {
            return ext_in[i];
        }
    }
    return kEmptyStr;
}

bool CProjectFileCollector::IsProducedByDatatool(
        const CProjItem& projitem, const string& file)
{
    if ( projitem.m_DatatoolSources.empty() )
        return false;

    string src_base;
    CDirEntry::SplitPath(file, NULL, &src_base);

    // guess name.asn file name from name__ or name___
    string asn_base;
    if ( NStr::EndsWith(src_base, "___") ) {
        asn_base = src_base.substr(0, src_base.length() -3);
    } else if ( NStr::EndsWith(src_base, "__") ) {
        asn_base = src_base.substr(0, src_base.length() -2);
    } else {
        return false;
    }
    string asn_name = asn_base + ".asn";
    string dtd_name = asn_base + ".dtd";
    string xsd_name = asn_base + ".xsd";
    string wsdl_name = asn_base + ".wsdl";

    // find this name in datatool generated sources container
    ITERATE(list<CDataToolGeneratedSrc>, p, projitem.m_DatatoolSources) {
        const CDataToolGeneratedSrc& asn = *p;
        if ((asn.m_SourceFile == asn_name) ||
            (asn.m_SourceFile == dtd_name) ||
            (asn.m_SourceFile == xsd_name) ||
            (asn.m_SourceFile == wsdl_name))
            return true;
    }
    return false;
}

bool CProjectFileCollector::IsInsideDatatoolSourceDir(
    const string& file)
{
    string dir_name;
    CDirEntry::SplitPath(file, &dir_name);

    //This files must be inside datatool src dir
    CDir dir(dir_name);
    if ( dir.GetEntries("*.module").empty() ) 
        return false;
    if ( dir.GetEntries("*.asn").empty() &&
         dir.GetEntries("*.dtd").empty() &&
         dir.GetEntries("*.xsd").empty() ) 
        return false;

    return true;
}

#endif


END_NCBI_SCOPE
