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


bool CProjectOneNodeFilter::CheckProject(const string& project_base_dir) const
{
    TPath project_path;
    CProjectTreeFolders::CreatePath(m_RootSrcDir, 
                                    project_base_dir, 
                                    &project_path);

    if (project_path.size() < m_SubtreePath.size())
        return false;

    TPath::const_iterator i = project_path.begin();
    ITERATE(TPath, p, m_SubtreePath) {
        const string& subtree_i  = *p;
        const string& project_i = *i;
        if (NStr::CompareNocase(subtree_i, project_i) != 0)
            return false;
        ++i;
    }
    return true;

}

//-----------------------------------------------------------------------------
CProjectsLstFileFilter::CProjectsLstFileFilter(const string& root_src_dir,
                                               const string& file_full_path)
{
    m_RootSrcDir = root_src_dir;

    CNcbiIfstream ifs(file_full_path.c_str(), IOS_BASE::in | IOS_BASE::binary);
    if ( !ifs )
        NCBI_THROW(CProjBulderAppException, eFileOpen, file_full_path);

    string strline;
    while ( NcbiGetlineEOL(ifs, strline) ) {
        
        // skip "update" statements
        if (strline.find(" update-only") != NPOS)
            continue;

        TPath project_path;
        NStr::Split(strline, " \r\n\t$/", project_path);
        if ( !project_path.empty() ) {
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


bool CProjectsLstFileFilter::CmpLstElementWithPath(const SLstElement& elt, 
                                                   const TPath&       path)
{
    if (path.size() < elt.m_Path.size())
        return false;

    if ( !elt.m_Recursive  && path.size() != elt.m_Path.size())
        return false;

    TPath::const_iterator i = path.begin();
    ITERATE(TPath, p, elt.m_Path) {
        const string& elt_i  = *p;
        const string& path_i = *i;
        if (NStr::CompareNocase(elt_i, path_i) != 0)
            return false;
        ++i;
    }
    return true;
}


bool CProjectsLstFileFilter::CheckProject(const string& project_base_dir) const
{
    TPath project_path;
    CProjectTreeFolders::CreatePath(m_RootSrcDir, 
                                    project_base_dir, 
                                    &project_path);

    bool include_ok = false;
    ITERATE(TLstFileContents, p, m_LstFileContentsInclude) {
        const SLstElement& elt = *p;
        if ( CmpLstElementWithPath(elt, project_path) ) {
            include_ok =  true;
            break;
        }
    }
    if ( !include_ok )
        return false;

    ITERATE(TLstFileContents, p, m_LstFileContentsExclude) {
        const SLstElement& elt = *p;
        if ( CmpLstElementWithPath(elt, project_path) ) {
            return false;
        }
    }

    return true;
}


END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
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
