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

/// Filter for projects in project tree
class IProjectFilter
{
public:
    virtual ~IProjectFilter(void)
    {
    }
    virtual bool CheckProject(const string& project_base_dir, bool* weak=0) const = 0;
    virtual bool PassAll     (void)                                         const = 0;
    virtual bool ExcludePotential (void)                                    const = 0;
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
    
    string m_CustomMetaData;
    string m_CustomConfH;
};

// Get parent directory
string ParentDir (const string& dir_abs);


END_NCBI_SCOPE

#endif //PROJECT_TREE_BUILDER__PROJ_UTILS__HPP
