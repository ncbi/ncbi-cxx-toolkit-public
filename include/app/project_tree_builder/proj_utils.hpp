#ifndef PROJECT_TREE_BUILDER__PROJ_UTILS__HPP
#define PROJECT_TREE_BUILDER__PROJ_UTILS__HPP

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


/// Utilits for Project Tree Builder:

#include <corelib/ncbienv.hpp>
BEGIN_NCBI_SCOPE

/// Key Value struct
struct SKeyValue
{
    string m_Key;
    string m_Value;
};

/// Filter for projects in project tree
class IProjectFilter
{
public:
    virtual ~IProjectFilter(void)
    {
    }
    virtual bool CheckProject(const string& project_base_dir) const = 0;
    virtual bool PassAll     (void)                           const = 0;
};


/// Abstraction of project tree general information
struct SProjectTreeInfo
{
    /// Root of the project tree
    string m_Root;

    /// Subtree to buil (default is m_Src).
    /// More enhanced version of "subtree to build"
    auto_ptr<IProjectFilter> m_IProjectFilter;

    /// Branch of tree to be implicit exclude from build
    list<string> m_ImplicitExcludedAbsDirs;

    /// <include> branch of tree
    string m_Include;

    /// <src> branch of tree
    string m_Src;

    /// <compilers> branch of tree
    string m_Compilers;

    /// <projects> branch of tree (scripts\projects)
    string m_Projects;

    /// <impl> sub-branch of include/* project path
    string m_Impl;

    /// Makefile in the tree node 
    string m_TreeNode;
};

// Get parent directory
string ParentDir (const string& dir_abs);

END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.13  2004/06/14 14:14:58  gorelenk
 * Added m_TreeNode to struct SProjectTreeInfo .
 *
 * Revision 1.12  2004/06/10 15:10:17  gorelenk
 * Changed function template EraseIf to be comply with standard STL map .
 *
 * Revision 1.11  2004/06/07 19:13:02  gorelenk
 * + m_Impl in struct SProjectTreeInfo .
 *
 * Revision 1.10  2004/02/26 21:20:58  gorelenk
 * Added declaration of IProjectFilter interface. auto_ptr<IProjectFilter>
 * member used instead of m_Subtree in struct SProjectTreeInfo.
 *
 * Revision 1.9  2004/02/18 23:33:35  gorelenk
 * Added m_Projects member to struct SProjectTreeInfo.
 *
 * Revision 1.8  2004/02/11 15:39:53  gorelenk
 * Added support for multiple implicit excludes from source tree.
 *
 * Revision 1.7  2004/02/06 23:15:40  gorelenk
 * Implemented support of ASN projects, semi-auto configure,
 * CPPFLAGS support. Second working version.
 *
 * Revision 1.6  2004/02/04 23:13:24  gorelenk
 * struct SProjectTreeInfo redesigned.
 *
 * Revision 1.5  2004/02/03 17:03:39  gorelenk
 * Added members to struct SProjectTreeInfo
 *
 * Revision 1.4  2004/01/28 17:55:07  gorelenk
 * += For msvc makefile support of :
 *                 Requires tag, ExcludeProject tag,
 *                 AddToProject section (SourceFiles and IncludeDirs),
 *                 CustomBuild section.
 * += For support of user local site.
 *
 * Revision 1.3  2004/01/22 17:57:09  gorelenk
 * first version
 *
 * ===========================================================================
 */

#endif //PROJECT_TREE_BUILDER__PROJ_UTILS__HPP
