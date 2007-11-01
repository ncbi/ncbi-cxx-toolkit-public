#ifndef PROJECT_TREE_BUILDER__PROJ_PROJECTS__HPP
#define PROJECT_TREE_BUILDER__PROJ_PROJECTS__HPP

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
//#include <app/project_tree_builder/proj_item.hpp>

#include <corelib/ncbienv.hpp>
#include <app/project_tree_builder/proj_utils.hpp>

BEGIN_NCBI_SCOPE

class CProjectsLstFileFilter : public IProjectFilter
{
public:
    // create from .lst file
    CProjectsLstFileFilter(const string& root_src_dir,
                           const string& file_full_path);

    virtual ~CProjectsLstFileFilter(void)
    {
    }

    virtual bool CheckProject(const string& project_base_dir, bool* weak=0) const;
    virtual bool PassAll     (void) const
    {
        return m_PassAll;
    }
    virtual bool ExcludePotential (void) const
    {
        return m_ExcludePotential;
    }
    void SetExcludePotential (bool excl)
    {
        m_ExcludePotential = excl;
    }

    typedef list<string> TPath;
private:
    string m_RootSrcDir;
    bool m_PassAll;
    bool m_ExcludePotential;

    list<string> m_listEnabled;
    list<string> m_listDisabled;
    string ConvertToMask(const string& name);

    void InitFromString(const string& subtree);
    void InitFromFile(const string& file_full_path);

    // Prohibited to:
    CProjectsLstFileFilter(void);
    CProjectsLstFileFilter(const CProjectsLstFileFilter&);
    CProjectsLstFileFilter& operator= (const CProjectsLstFileFilter&);   
};



END_NCBI_SCOPE

#endif //PROJECT_TREE_BUILDER__PROJ_PROJECTS__HPP
