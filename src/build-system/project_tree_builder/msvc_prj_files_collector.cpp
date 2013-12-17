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
#include "stl_msvc_usage.hpp"
#include "msvc_prj_files_collector.hpp"
#include "msvc_prj_utils.hpp"
#include "configurable_file.hpp"
#include "ptb_err_codes.hpp"
#include "proj_tree_builder.hpp"


BEGIN_NCBI_SCOPE


static void s_CollectRelPathes(const string&        path_from,
                               const list<string>&  abs_dirs,
                               list<string>*        rel_pathes);

// sort files by base file name, ignoring path and extension
bool s_FileName_less(const string& x, const string& y)
{
    string base_x, base_y;
    CDirEntry::SplitPath(x, NULL, &base_x);
    CDirEntry::SplitPath(y, NULL, &base_y);
    return NStr::CompareNocase(base_x, base_y) < 0;
}

//-----------------------------------------------------------------------------

CMsvcPrjFilesCollector::CMsvcPrjFilesCollector
                                (const CMsvcPrjProjectContext& project_context,
                                 const list<SConfigInfo>&      project_configs,
                                 const CProjItem&              project)
    : m_Context(&project_context),
      m_Configs(&project_configs),
      m_Project(&project)
{
    CollectSources();
    CollectHeaders();
    CollectInlines();
    CollectResources();
    CollectExtra();
}


CMsvcPrjFilesCollector::~CMsvcPrjFilesCollector(void)
{
}


const list<string>& CMsvcPrjFilesCollector::SourceFiles(void) const
{
    return m_SourceFiles;
}


const list<string>& CMsvcPrjFilesCollector::HeaderFiles(void) const
{
    return m_HeaderFiles;
}


const list<string>& CMsvcPrjFilesCollector::InlineFiles(void) const
{
    return m_InlineFiles;
}


// source files helpers -------------------------------------------------------

const list<string>& CMsvcPrjFilesCollector::ResourceFiles(void) const
{
    return m_ResourceFiles;
}

const map<string, list<string> > CMsvcPrjFilesCollector::GetExtraFiles(void) const
{
    return m_ExtraFiles;
}

struct PSourcesExclude
{
    PSourcesExclude(const string& prj_id, const list<string>& excluded_sources)
        : m_Prj(prj_id)
    {
        copy(excluded_sources.begin(), excluded_sources.end(), 
             inserter(m_ExcludedSources, m_ExcludedSources.end()) );
    }

    bool operator() (const string& src) const
    {
        string src_base;
        CDirEntry::SplitPath(src, NULL, &src_base);
        if (m_ExcludedSources.find(src_base) != m_ExcludedSources.end()) {
            PTB_WARNING_EX(src, ePTB_FileExcluded,
                           "Project " << m_Prj << ": source file excluded");
            return true;
        }
        return false;
    }

private:
    string m_Prj;
    set<string> m_ExcludedSources;
};

static bool s_IsInsideDatatoolSourceDir(const string& src_path_abs)
{
    string dir_name;
    CDirEntry::SplitPath(src_path_abs, &dir_name);

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


void 
CMsvcPrjFilesCollector::CollectSources(void)
{
    m_SourceFiles.clear();

    list<string> sources;
    ITERATE(list<string>, p, m_Project->m_Sources) {

        const string& src_rel = *p;
        string src_path = 
            CDirEntry::ConcatPath(m_Project->m_SourcesBaseDir, src_rel);
        src_path = CDirEntry::NormalizePath(src_path);

        sources.push_back(src_path);
    }

    list<string> included_sources;
    m_Context->GetMsvcProjectMakefile().GetAdditionalSourceFiles //TODO
                                            (SConfigInfo(),&included_sources);

    ITERATE(list<string>, p, included_sources) {
        string fullpath = CDirEntry::NormalizePath(
            CDirEntry::ConcatPath(m_Project->m_SourcesBaseDir, *p));
        string ext = SourceFileExt(fullpath);
        if (ext.empty() &&
            CDirEntry::IsAbsolutePath(fullpath) &&
            !GetApp().GetExtSrcRoot().empty()) {
            string tpath = CDirEntry::CreateRelativePath( GetApp().m_Root, fullpath);
            tpath = CDirEntry::ConcatPath(GetApp().GetExtSrcRoot(), tpath);
            ext = SourceFileExt(tpath);
            if (!ext.empty()) {
                fullpath = tpath;
            }
        }
        sources.push_back(fullpath);
    }

    list<string> excluded_sources;
    m_Context->GetMsvcProjectMakefile().GetExcludedSourceFiles //TODO
                                            (SConfigInfo(), &excluded_sources);
    if (!excluded_sources.empty()) {
        PSourcesExclude pred(m_Project->m_ID, excluded_sources);
        EraseIf(sources, pred);
    }

    ITERATE(list<string>, p, sources) {

        const string& abs_path = *p; // whithout ext.

        string ext = SourceFileExt(abs_path);
        if ( NStr::EndsWith(ext, ".in") ) {
            // Special case: skip configurable file generation
            // if configurations was not specified
            if ( m_Configs->empty() ) {
                m_SourceFiles.push_back(
                    CDirEntry::CreateRelativePath(m_Context->ProjectDir(),
                                                  abs_path));
            } else {
                // configurable source file
                string orig_ext = NStr::Replace(ext, ".in", "");
                string dst_path;
                CDirEntry::SplitPath(abs_path, NULL, &dst_path, NULL);
                dst_path = CDirEntry::MakePath(m_Context->ProjectDir(), dst_path);
                GetApp().SetConfFileData(abs_path + ext, dst_path);

                // Create configurable file for each enabled configuration
                ITERATE(list<SConfigInfo>, p , *m_Configs) {
                    const SConfigInfo& cfg_info = *p;
                    string file_dst_path;
                    file_dst_path = dst_path + "." +
                                    ConfigurableFileSuffix(cfg_info.GetConfigFullName())+
                                    orig_ext;
#if 0
                    CreateConfigurableFile(abs_path + ext, file_dst_path,
                                           cfg_info.GetConfigFullName());
#else
// we postpone creation until later
// here we only create placeholders
                    if (!CFile(file_dst_path).Exists()) {
                        CNcbiOfstream os(file_dst_path.c_str(),
                                         IOS_BASE::out | IOS_BASE::binary | IOS_BASE::trunc);
                    }
#endif
                }
                dst_path += ".@config@" + orig_ext;
                m_SourceFiles.push_back(
                    CDirEntry::CreateRelativePath(m_Context->ProjectDir(),
                                                  dst_path));
                }
        }
        else if ( !ext.empty() ) {
            // add ext to file
            string source_file_abs_path = abs_path + ext;
            string t;
            try {
                t = CDirEntry::CreateRelativePath(
                    m_Context->ProjectDir(), source_file_abs_path);
            } catch (CFileException&) {
                t = source_file_abs_path;
            }
            m_SourceFiles.push_back(t);
        } 
        else if ( IsProducedByDatatool(abs_path, *m_Project) ||
                  s_IsInsideDatatoolSourceDir(abs_path) ) {
            // .cpp file extension
            m_SourceFiles.push_back(
                CDirEntry::CreateRelativePath(m_Context->ProjectDir(), 
                                              abs_path + ".cpp"));
        } else {
            if (m_Project->m_MakeType >= eMakeType_Excluded ||
                SMakeProjectT::IsConfigurableDefine(CDirEntry(abs_path).GetBase()) ||
                m_Project->HasDataspecDependency()) {
                PTB_WARNING_EX(abs_path, ePTB_FileNotFound,
                            "Source file not found");
            } else {
                PTB_ERROR_EX(abs_path, ePTB_FileNotFound,
                           "Source file not found");
            }
        }
    }
    m_SourceFiles.sort(s_FileName_less);
    m_SourceFiles.unique();
}


// header files helpers -------------------------------------------------------
void 
CMsvcPrjFilesCollector::CollectHeaders(void)
{
    m_HeaderFiles.clear();
    s_CollectRelPathes(m_Context->ProjectDir(), m_Context->IncludeDirsAbs(),
                       &m_HeaderFiles);
    m_HeaderFiles.sort(s_FileName_less);
    m_HeaderFiles.unique();
}


// inline files helpers -------------------------------------------------------

void 
CMsvcPrjFilesCollector::CollectInlines(void)
{
    m_InlineFiles.clear();
    s_CollectRelPathes(m_Context->ProjectDir(), m_Context->InlineDirsAbs(),
                       &m_InlineFiles);
    m_InlineFiles.sort(s_FileName_less);
    m_InlineFiles.unique();
}


// resource files helpers -------------------------------------------------------

void 
CMsvcPrjFilesCollector::CollectResources(void)
{
    m_ResourceFiles.clear();

    // resources from msvc makefile - first priority
    list<string> included_sources;
    m_Context->GetMsvcProjectMakefile().GetResourceFiles
                                            (SConfigInfo(),&included_sources);
    list<string> sources;
    ITERATE(list<string>, p, included_sources) {
        sources.push_back(CDirEntry::NormalizePath
                                        (CDirEntry::ConcatPath
                                              (m_Project->m_SourcesBaseDir, *p)));
    }

    ITERATE(list<string>, p, sources) {

        const string& abs_path = *p; // with ext.
        m_ResourceFiles.push_back(
            CDirEntry::CreateRelativePath(m_Context->ProjectDir(), 
                                          abs_path));
    }
    if ( m_ResourceFiles.empty() ) {
        // if there is no makefile resources - use defaults
        string default_rc;
        if (m_Project->m_ProjType == CProjKey::eApp) {
            default_rc = GetApp().GetSite().GetAppDefaultResource();
        }
        if ( !default_rc.empty() ) {
            string abs_path = GetApp().GetProjectTreeInfo().m_Compilers;
            abs_path = 
                CDirEntry::ConcatPath(abs_path, 
                                    GetApp().GetRegSettings().m_CompilersSubdir);
            abs_path = CDirEntry::ConcatPath(abs_path, default_rc);
            abs_path = CDirEntry::NormalizePath(abs_path);
            m_ResourceFiles.push_back(
                CDirEntry::CreateRelativePath(m_Context->ProjectDir(), 
                                            abs_path));
        }
    }
    m_ResourceFiles.sort(s_FileName_less);
    m_ResourceFiles.unique();
}

void 
CMsvcPrjFilesCollector::CollectExtra(void)
{
    m_ExtraFiles.clear();

    const map<string, list<string> >& extra = m_Project->m_ExtraFiles;
    for (map<string, list<string> >::const_iterator g = extra.begin(); g != extra.end(); ++g) {
        const list<string>& lst(g->second);
        ITERATE( list<string>, f, lst) {
            m_ExtraFiles[g->first].push_back(
                CDirEntry::CreateRelativePath(m_Context->ProjectDir(), 
                    CDirEntry::NormalizePath(CDirEntry::ConcatPath(m_Project->m_SourcesBaseDir, *f))));
        }
    }
    
    map<string, list<string> > files;
    m_Context->GetMsvcProjectMakefile().GetExtraFiles(&files);
    for (map<string, list<string> >::const_iterator g = files.begin(); g != files.end(); ++g) {
        const list<string>& lst(g->second);
        ITERATE( list<string>, f, lst) {
            m_ExtraFiles[g->first].push_back(
                CDirEntry::CreateRelativePath(m_Context->ProjectDir(), 
                    CDirEntry::NormalizePath(CDirEntry::ConcatPath(m_Project->m_SourcesBaseDir, *f))));
        }
    }
}



//-----------------------------------------------------------------------------
// Collect all files from specified dirs having specified exts
static void s_CollectRelPathes(const string&        path_from,
                               const list<string>&  abs_dirs,
                               list<string>*        rel_pathes)
{
    rel_pathes->clear();

    set<string> toremove;
    list<string> pathes;
    ITERATE(list<string>, n, abs_dirs) {
        string value(*n), pdir, base, ext;
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
        if ( !dir.Exists() )
            continue;
        CDir::TEntries contents = dir.GetEntries(base + ext);
        ITERATE(CDir::TEntries, i, contents) {
            if ( (*i)->IsFile() ) {
                string path  = (*i)->GetPath();
                if ( NStr::EndsWith(path, ext, NStr::eNocase) ) {
                    if (remove) {
                        toremove.insert(path);
                    } else {
                        pathes.push_back(path);
                    }
                }
            }
        }
    }
    ITERATE(set<string>, r, toremove) {
        pathes.remove(*r);
    }

    ITERATE(list<string>, p, pathes)
        rel_pathes->push_back(CDirEntry::CreateRelativePath(path_from, *p));
}


END_NCBI_SCOPE
