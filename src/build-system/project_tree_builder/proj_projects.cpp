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
#include "proj_projects.hpp"
#include "proj_builder_app.hpp"
#include "proj_tree.hpp"

#include <corelib/ncbienv.hpp>
#include <util/xregexp/regexp.hpp>
BEGIN_NCBI_SCOPE

//-----------------------------------------------------------------------------
CProjectsLstFileFilter::CProjectsLstFileFilter(const string& root_src_dir,
                                               const string& file_full_path)
{
    m_PassAll = false;
    m_ExcludePotential = false;
    m_RootSrcDir = root_src_dir;
    if (CDirEntry(file_full_path).IsFile()) {
        InitFromFile(file_full_path);
    } else {
        InitFromString(file_full_path);
    }
    string dll_subtree = GetApp().GetConfig().Get("ProjectTree", "dll");
    string s = ConvertToMask("dll");
    if (CMsvc7RegSettings::GetMsvcPlatform() < CMsvc7RegSettings::eUnix) {
        if (GetApp().GetBuildType().GetType() == CBuildType::eDll) {
            m_listEnabled.push_back(s);
        }
    } else {
        m_listDisabled.push_back(s);
    }
}

string CProjectsLstFileFilter::ConvertToMask(const string& name)
{
    string s = NStr::Replace(name,"\\","/");
    if (NStr::EndsWith(s,'$')) {
        s.erase(s.size()-1,1);
        s += "/$";
    } else {
        s += '/';
    }
    return s;
}


void CProjectsLstFileFilter::InitFromString(const string& subtree)
{
    string sub = CDirEntry::AddTrailingPathSeparator(subtree);
    m_PassAll = NStr::CompareNocase(m_RootSrcDir, sub) == 0;
    if (!m_PassAll) {
        string s = CDirEntry::CreateRelativePath(m_RootSrcDir,subtree);
        NStr::ReplaceInPlace(s,"\\","/");
        if (NStr::EndsWith(s,'/')) {
            s.erase(s.size()-1,1);
        }
        m_listEnabled.push_back( s );
    }
    m_ExcludePotential = true;
}

void CProjectsLstFileFilter::InitFromFile(const string& file_full_path)
{
    CNcbiIfstream ifs(file_full_path.c_str(), IOS_BASE::in | IOS_BASE::binary);
    if ( !ifs )
        NCBI_THROW(CProjBulderAppException, eFileOpen, file_full_path);

    string strline;
    while ( NcbiGetlineEOL(ifs, strline) ) {
        
        // skip "update" statements
        if (strline.find(" update-only") != NPOS)
            continue;

        NStr::TruncateSpacesInPlace(strline);
        if (strline.empty()) {
            continue;
        }
        if ( NStr::StartsWith(strline, "#include") ) {
            NStr::ReplaceInPlace(strline,"#include","",0,1);
            NStr::TruncateSpacesInPlace(strline);
            NStr::ReplaceInPlace(strline,"\"","");
            string name = CDirEntry::ConvertToOSPath(strline);
            if (CDirEntry::IsAbsolutePath(name)) {
                InitFromFile( name );
            } else {
                CDirEntry d(file_full_path);
                InitFromFile( CDirEntry::ConcatPathEx( d.GetDir(), name) );
            }
        } else {
            if ( NStr::StartsWith(strline, "-") ) {
                strline.erase(0,1);
                string s = ConvertToMask( strline );
                m_listDisabled.push_back( s );
            } else {
                string s = ConvertToMask( strline );
                m_listEnabled.push_back( s );
            }
        }
    }
}

bool CProjectsLstFileFilter::CheckProject(const string& project_base_dir, bool* weak) const
{
    string proj_dir = CDirEntry::CreateRelativePath(m_RootSrcDir,project_base_dir);
    proj_dir = NStr::Replace(proj_dir,"\\","/");
    proj_dir += '/';
    bool include_ok = false;
    if (!m_PassAll) {
        ITERATE(list<string>, s, m_listEnabled) {
            string str(*s);
            CRegexp rx("^" + str);
            if (rx.IsMatch(proj_dir)) {
                include_ok =  true;
                break;
            } else if (weak) {
                string pd = "^" + proj_dir + ".*";
                CRegexp px(pd);
                *weak = px.IsMatch(str);
                if (*weak) {
                    return false;
                }
            }
        }
        if ( !include_ok )
            return false;
    }
    ITERATE(list<string>, s, m_listDisabled) {
        string str(*s);
        str += "$";
        CRegexp rx("^" + str);
        if (rx.IsMatch(proj_dir)) {
            return false;
        }
    }
    return true;
}


END_NCBI_SCOPE
