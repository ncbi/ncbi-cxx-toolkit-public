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
BEGIN_NCBI_SCOPE

//-----------------------------------------------------------------------------
CProjectOneNodeFilter::CProjectOneNodeFilter(const string& root_src_dir,
                                             const string& subtree_dir)
    :m_RootSrcDir(root_src_dir),
     m_SubtreeDir(subtree_dir)
{
    CProjectTreeFolders::CreatePath(m_RootSrcDir, 
                                    m_SubtreeDir, 
                                    &m_SubtreePath);
    m_PassAll = NStr::CompareNocase(m_RootSrcDir, m_SubtreeDir) == 0;
}


bool CProjectOneNodeFilter::CheckProject(const string& project_base_dir, bool* weak) const
{
    TPath project_path;
    CProjectTreeFolders::CreatePath(m_RootSrcDir, 
                                    project_base_dir, 
                                    &project_path);

    if (project_path.size() < m_SubtreePath.size()) {
        if (!weak) {
            return false;
        }
    }

    TPath::const_iterator i = project_path.begin();
    TPath::const_iterator p = m_SubtreePath.begin();
    for (; i != project_path.end() && p != m_SubtreePath.end(); ++i, ++p) {
        const string& subtree_i  = *p;
        const string& project_i = *i;
        if (NStr::CompareNocase(subtree_i, project_i) != 0) {
            if (weak) {*weak=false;}
            return false;
        }
    }
    if (weak) {*weak = (i == project_path.end());}
    return p == m_SubtreePath.end();
}

//-----------------------------------------------------------------------------
CProjectsLstFileFilter::CProjectsLstFileFilter(const string& root_src_dir,
                                               const string& file_full_path)
{
    m_RootSrcDir = root_src_dir;
    InitFromFile(file_full_path);
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
        if (NStr::CompareNocase(elt_i, path_i) != 0) {
            if (weak) {*weak=false;}
            return false;
        }
    }
    if (weak) {*weak = (i == path.end());}
    if ( !elt.m_Recursive && path.size() > elt.m_Path.size()) {
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

/*
 * ===========================================================================
 * $Log$
 * Revision 1.13  2005/03/30 21:12:19  gouriano
 * Added quotes to LST include file name
 *
 * Revision 1.12  2005/03/30 14:11:30  gouriano
 * Fixed LST file parsing
 *
 * Revision 1.11  2005/03/29 20:41:18  gouriano
 * Allow inclusion of LST file into LST file
 *
 * Revision 1.10  2004/12/20 15:29:39  gouriano
 * Fixed errors in project list processing
 *
 * Revision 1.9  2004/09/14 17:28:07  gouriano
 * Corrected ProjectsLstFileFilter
 *
 * Revision 1.8  2004/09/13 13:49:08  gouriano
 * Make it to rely more on UNIX makefiles
 *
 * Revision 1.7  2004/05/21 21:41:41  gorelenk
 * Added PCH ncbi_pch.hpp
 *
 * Revision 1.6  2004/03/18 19:13:08  gorelenk
 * Changed imlementations of class CProjectsLstFileFilter constructor and
 * CProjectsLstFileFilter::CheckProject .
 *
 * Revision 1.5  2004/03/02 16:27:10  gorelenk
 * Added include for proj_tree.hpp.
 *
 * Revision 1.4  2004/02/27 18:09:14  gorelenk
 * Changed implementation of CProjectsLstFileFilter::CmpLstElementWithPath.
 *
 * Revision 1.3  2004/02/26 21:27:43  gorelenk
 * Removed all older class implementations. Added implementations of classes:
 * CProjectDummyFilter, CProjectOneNodeFilter and CProjectsLstFileFilter.
 *
 * Revision 1.2  2004/02/18 23:38:06  gorelenk
 * Added definitions of class CProjProjectsSets member-functions.
 *
 * ===========================================================================
 */
