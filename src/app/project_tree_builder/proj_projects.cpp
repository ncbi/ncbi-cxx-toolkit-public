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
#include <app/project_tree_builder/proj_projects.hpp>
#include <app/project_tree_builder/proj_builder_app.hpp>
#include <app/project_tree_builder/proj_tree.hpp>

#include <corelib/ncbienv.hpp>
//#include <util/regexp.hpp>
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
    TPath project_path;
    NStr::Split(dll_subtree, "\\/", project_path);
    SLstElement elt;
    elt.m_Path      = project_path;
    elt.m_Recursive = true;
    if (CMsvc7RegSettings::GetMsvcVersion() < CMsvc7RegSettings::eMsvcNone) {
        if (GetApp().GetBuildType().GetType() == CBuildType::eDll) {
            m_LstFileContentsInclude.push_back(elt);
        }
    } else {
        m_LstFileContentsExclude.push_back(elt);
    }
}

void CProjectsLstFileFilter::InitFromString(const string& subtree)
{
    string sub = CDirEntry::AddTrailingPathSeparator(subtree);
    TPath project_path;
    CProjectTreeFolders::CreatePath(m_RootSrcDir, sub, &project_path);

    SLstElement elt;
    elt.m_Path      = project_path;
    elt.m_Recursive = subtree.find("$") == NPOS;
    m_LstFileContentsInclude.push_back(elt);

    m_PassAll = NStr::CompareNocase(m_RootSrcDir, sub) == 0;
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

        TPath project_path;
        NStr::Split(strline, " \r\n\t/$", project_path);
        if ( !project_path.empty() ) {
            if (project_path.front() == "#include") {
                project_path.erase( project_path.begin() );
                string tmp = NStr::Join(project_path,"/");
                string name = NStr::Replace(tmp,"\"","");
                if (CDirEntry::IsAbsolutePath(name)) {
                    InitFromFile( name );
                } else {
                    CDirEntry d(file_full_path);
                    InitFromFile( CDirEntry::ConcatPathEx( d.GetDir(), name) );
                }
            } else {
                if ( NStr::StartsWith(project_path.front(), "-") ) {
                    
                    // exclusion statement
                    SLstElement elt;
                    // erase '-'
                    project_path.front().erase(0, 1);
                    elt.m_Path      = project_path;
                    elt.m_Recursive = true;
                    m_LstFileContentsExclude.push_back(elt);
                } else {
                    // inclusion statement
                    SLstElement elt;
                    elt.m_Path      = project_path;
                    elt.m_Recursive = strline.find("$") == NPOS;
                    m_LstFileContentsInclude.push_back(elt);
                }
            }
        }
    }
}


bool CProjectsLstFileFilter::CmpLstElementWithPath(const SLstElement& elt, 
                                                   const TPath&       path,
                                                   bool* weak)
{
    if (path.size() < elt.m_Path.size())
        if (!weak) {
            return false;
        }

    if ( !elt.m_Recursive  && path.size() != elt.m_Path.size()) {
        if (!weak) {
            return false;
        }
    }

    TPath::const_iterator i = path.begin();
    TPath::const_iterator p = elt.m_Path.begin();
    for (; i != path.end() && p != elt.m_Path.end(); ++i, ++p) {
        const string& elt_i  = *p;
        const string& path_i = *i;
        if (elt_i == ".*") {
            ++p;
            if (p != elt.m_Path.end()) {
                while (++i != path.end() && *i != *p)
                    ;
                if (i != path.end()) {
                    continue;
                }
            }
            break;
        }
        if (NStr::CompareNocase(elt_i, path_i) != 0) {
            if (weak) {*weak=false;}
            return false;
        }
    }
    if (weak) {*weak = (i == path.end());}
    if ( !elt.m_Recursive && i != path.end() && p == elt.m_Path.end()) {
        return false;
    }
    return p == elt.m_Path.end();
}


bool CProjectsLstFileFilter::CheckProject(const string& project_base_dir, bool* weak) const
{
    TPath project_path;
    CProjectTreeFolders::CreatePath(m_RootSrcDir, 
                                    project_base_dir, 
                                    &project_path);

    bool include_ok = false;
    ITERATE(TLstFileContents, p, m_LstFileContentsInclude) {
        const SLstElement& elt = *p;
        if ( CmpLstElementWithPath(elt, project_path, weak) ) {
            include_ok =  true;
            break;
        } else if (weak && *weak) {
            return false;
        }
    }
    if ( !include_ok )
        return false;

    ITERATE(TLstFileContents, p, m_LstFileContentsExclude) {
        const SLstElement& elt = *p;
        if ( CmpLstElementWithPath(elt, project_path, weak) ) {
            return false;
        }
    }

    return true;
}


END_NCBI_SCOPE
