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
#include "proj_src_resolver.hpp"
#include "proj_builder_app.hpp"
#include "proj_tree_builder.hpp"

BEGIN_NCBI_SCOPE

// collect include statements and resolve to abs pathes 
static void s_GetMakefileIncludes(const string& applib_mfilepath,
                                  const string& source_base_dir,
                                  list<string>* includes)
{
    includes->clear();

    CNcbiIfstream ifs(applib_mfilepath.c_str(), 
                      IOS_BASE::in | IOS_BASE::binary);
    if ( !ifs )
        NCBI_THROW(CProjBulderAppException, eFileOpen, applib_mfilepath);

    string strline;
    while ( getline(ifs, strline) ) {
        const string include_token("include ");
        strline = NStr::TruncateSpaces(strline);
        if ( NStr::StartsWith(strline, include_token) ) {
            string include = strline.substr(include_token.length());
            
            string root;
            string srcdir_token("$(srcdir)");
            if (include.find(srcdir_token) == 0) {
                root = source_base_dir;
            } else {
                srcdir_token = "$(top_srcdir)";
                if (include.find(srcdir_token) == 0) {
                    root = GetApp().GetProjectTreeInfo().m_Root;
                }
                else {
#if 0
                    srcdir_token = "$(builddir)";
                    if (NStr::StartsWith(include, srcdir_token)) {
                        root = GetApp().GetBuildRoot();
                        if (root.empty()) {
                            root = CDirEntry(GetApp().m_Solution).GetDir();
                        }
                    }
#endif
                }
            }
            if (!root.empty()) {
                include = include.substr(srcdir_token.length());
                include = CDirEntry::ConcatPath(root, include);
                include = CDirEntry::NormalizePath(include);
            }
            includes->push_back(include);
        }
    }
}


//-----------------------------------------------------------------------------
CProjSRCResolver::CProjSRCResolver(const string&       applib_mfilepath,
                             const string&       source_base_dir,
                             const list<string>& sources_src)
 :m_MakefilePath    (applib_mfilepath),
  m_SourcesBaseDir(source_base_dir),
  m_SourcesSrc      (sources_src)
{
}
    

CProjSRCResolver::~CProjSRCResolver(void)
{
}

static bool s_SourceFileExists(const string& dir, const string& name)
{
    string path = CDirEntry::ConcatPath(dir, name);

    if ( CDirEntry(path + ".cpp").Exists() )
        return true;
    if ( CDirEntry(path + ".c").Exists() )
        return true;
    
    return false;
}

void CProjSRCResolver::ResolveTo(list<string>* sources_dst)
{
    sources_dst->clear();
    PrepareResolver();
    
    list<string> sources_names;
    ITERATE(list<string>, p, m_SourcesSrc) {
        const string& src = *p;
        string src_define;
        if (SMakeProjectT::IsConfigurableDefine(src)) {
            src_define = "$(" + SMakeProjectT::StripConfigurableDefine(src) + ")";
        } else if (CSymResolver::IsDefine(src)) {
            src_define = FilterDefine(src);
        } else {
            sources_names.push_back(src);
            continue;
        }
        list<string> resolved_src_defines;
        m_Resolver.Resolve(src_define, &resolved_src_defines);
        if (resolved_src_defines.size() == 1 && resolved_src_defines.front() == src_define) {
            continue;
        }
        copy(resolved_src_defines.begin(),
                resolved_src_defines.end(),
                back_inserter(sources_names));
    }

    ITERATE(list<string>, p, sources_names) {
        const string& src = *p;
        bool found = false;
        if ( s_SourceFileExists(m_SourcesBaseDir, src) ) {
            sources_dst->push_back(src);
            found = true;
        } else {
            ITERATE(list<string>, n, m_MakefileDirs) {
                const string& dir = *n;
                if ( s_SourceFileExists(dir, src) ) {
                    string path = 
                        CDirEntry::CreateRelativePath(m_SourcesBaseDir, dir);
                    path = CDirEntry::ConcatPath(path, src);
                    sources_dst->push_back(path);
                    found = true;
                    break;
                }
            }
        }
        if ( !found ) 
            sources_dst->push_back(src);
    }
}


void CProjSRCResolver::PrepareResolver(void)
{
    if ( !m_Resolver.IsEmpty() )
        return;

    list<string> include_makefiles;
    s_GetMakefileIncludes(m_MakefilePath, 
                          m_SourcesBaseDir, &include_makefiles);

    ITERATE(list<string>, p, include_makefiles) {
        m_MakefileDirs.push_back( CDirEntry(*p).GetDir());
    }
    CSymResolver::LoadFrom(m_MakefilePath, &m_Resolver);
    m_Resolver.Append( GetApp().GetSite().GetMacros());
}

END_NCBI_SCOPE
