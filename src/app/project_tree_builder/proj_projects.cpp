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
#include <app/project_tree_builder/proj_projects.hpp>
#include <app/project_tree_builder/proj_builder_app.hpp>

#include <corelib/ncbienv.hpp>
BEGIN_NCBI_SCOPE


CProjProjects::CProjProjects(void)
{
    Clear();
}


CProjProjects::CProjProjects(const CProjProjects& projects)
{
    SetFrom(projects);
}


CProjProjects& CProjProjects::operator= (const CProjProjects& projects)
{
    if (this != &projects) {
        Clear();
        SetFrom(projects);
    }
    return *this;
}


CProjProjects::CProjProjects(const string& dir_path)
{
    LoadFrom(dir_path, this);
}


CProjProjects::~CProjProjects(void)
{
}


void CProjProjects::LoadFrom(const string& dir_path, CProjProjects* pr)
{
    pr->Clear();

    CDir dir(dir_path);
    CDir::TEntries contents = dir.GetEntries("*.lst");
    ITERATE(CDir::TEntries, i, contents) {
        string name  = (*i)->GetName();
        if ( name == "."  ||  name == ".."  ||  
             name == string(1,CDir::GetPathSeparator()) ) {
            continue;
        }
        string path = (*i)->GetPath();

        if ( (*i)->IsFile() ) {
            // Only files from this dir
            TFileContents fc;
            LoadFrom(path, &fc);
            if ( !fc.empty() ) {
                string base;
                CDirEntry::SplitPath(path, NULL, &base);
                pr->m_Projects[base] = fc;
            }
        } 
    }

}


void CProjProjects::Clear(void)
{
    m_Projects.clear();
}


void CProjProjects::SetFrom(const CProjProjects& contents)
{
    m_Projects = contents.m_Projects;
}


void CProjProjects::LoadFrom(const string&  file_path, 
                             TFileContents* fc)
{
    fc->clear();

    CNcbiIfstream ifs(file_path.c_str(), IOS_BASE::in | IOS_BASE::binary);
    if ( !ifs )
        NCBI_THROW(CProjBulderAppException, eFileOpen, file_path);

    string strline;
    while ( NcbiGetlineEOL(ifs, strline) ) {
        TPath project_path;
        NStr::Split(strline, " \r\n\t$/", project_path);
        if ( !project_path.empty() ) {
            SLstElement elt;
            elt.m_Path      = project_path;
            elt.m_Recursive = strline.find("$") != NPOS;
            fc->push_back(elt);
        }
    }
}


END_NCBI_SCOPE

/*
 * ===========================================================================
 * $$
 * ===========================================================================
 */
