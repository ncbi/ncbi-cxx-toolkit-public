#ifndef PROJ_PROJECTS_HEADER
#define PROJ_PROJECTS_HEADER

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
#include <app/project_tree_builder/proj_item.hpp>

#include <corelib/ncbienv.hpp>
BEGIN_NCBI_SCOPE

/////////////////////////////////////////////////////////////////////////////
///
/// CProjProjects --
///
/// Abstraction of .lst files set inside scripts/projects.
///
/// 

class CProjProjects
{
public:
    CProjProjects(void);
    CProjProjects(const CProjProjects& projects);

    CProjProjects& operator= (const CProjProjects& projects);

    CProjProjects(const string& dir_path);

    ~CProjProjects(void);

    static void LoadFrom(const string& dir_path, CProjProjects* pr);

    typedef CProjectTreeFolders::TPath TPath;

    struct SLstElement
    {
        TPath m_Path;
        bool  m_Recursive;
    };

    /// One .lst file
    typedef list<SLstElement> TFileContents;
    /// Project name/ Project contents
    typedef map<string, TFileContents> TProjects;
    TProjects m_Projects;


private:
    void Clear(void);

    void SetFrom(const CProjProjects& contents);

    static void LoadFrom(const string&  file_path, 
                         TFileContents* fc);
};


END_NCBI_SCOPE

/*
 * ===========================================================================
 * $$
 * ===========================================================================
 */


#endif //PROJ_PROJECTS_HEADER