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


class CProjectDummyFilter : public IProjectFilter
{
public:
    CProjectDummyFilter(void)
    {
    }
    virtual ~CProjectDummyFilter(void)
    {
    }
    virtual bool CheckProject(const string& project_base_dir, bool* weak=0) const
    {
        return true;
    }
    virtual bool PassAll (void) const
    {
        return true;
    }
private:
    // Prohibited to:
    CProjectDummyFilter(const CProjectDummyFilter&);
    CProjectDummyFilter& operator= (const CProjectDummyFilter&);   
};

class CProjectOneNodeFilter : public IProjectFilter
{
public:
    // 
    CProjectOneNodeFilter(const string& root_src_dir,
                          const string& subtree_dir);

    virtual ~CProjectOneNodeFilter(void)
    {
    }

    virtual bool CheckProject(const string& project_base_dir, bool* weak=0) const;
    virtual bool PassAll     (void) const
    {
        return m_PassAll;
    }

private:
    typedef list<string> TPath;

    string m_RootSrcDir;
    string m_SubtreeDir;

    TPath  m_SubtreePath;
    bool m_PassAll;

    // Prohibited to:
    CProjectOneNodeFilter(void);
    CProjectOneNodeFilter(const CProjectOneNodeFilter&);
    CProjectOneNodeFilter& operator= (const CProjectOneNodeFilter&);   
};


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
        return false;
    }

    typedef list<string> TPath;
private:
    string m_RootSrcDir;

    struct SLstElement
    {
        TPath m_Path;
        bool  m_Recursive;
    };
    /// One .lst file
    typedef list<SLstElement> TLstFileContents;
    TLstFileContents m_LstFileContentsInclude;
    TLstFileContents m_LstFileContentsExclude;
    

    void InitFromFile(const string& file_full_path);
    static bool CmpLstElementWithPath(const SLstElement& elt, 
                                      const TPath&       path,
                                      bool* weak);
    // Prohibited to:
    CProjectsLstFileFilter(void);
    CProjectsLstFileFilter(const CProjectsLstFileFilter&);
    CProjectsLstFileFilter& operator= (const CProjectsLstFileFilter&);   
};



END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.10  2005/03/29 20:40:35  gouriano
 * Allow inclusion of LST file into LST file
 *
 * Revision 1.9  2004/09/14 17:27:28  gouriano
 * Corrected ProjectsLstFileFilter
 *
 * Revision 1.8  2004/09/13 13:49:36  gouriano
 * Make it to rely more on UNIX makefiles
 *
 * Revision 1.7  2004/08/04 13:24:58  gouriano
 * Added processing of EXPENDABLE projects
 *
 * Revision 1.6  2004/06/10 15:12:55  gorelenk
 * Added newline at the file end to avoid GCC warning.
 *
 * Revision 1.5  2004/03/18 19:11:12  gorelenk
 * Added m_LstFileContentsExclude member to class CProjectsLstFileFilter.
 *
 * Revision 1.4  2004/02/26 21:26:11  gorelenk
 * Removed all older class declarations. Added declaration of classes that
 * implements IProjectFilter interface: CProjectDummyFilter,
 * CProjectOneNodeFilter and CProjectsLstFileFilter.
 *
 * Revision 1.3  2004/02/18 23:34:30  gorelenk
 * Added declaration of class CProjProjectsSets.
 *
 * ===========================================================================
 */


#endif //PROJECT_TREE_BUILDER__PROJ_PROJECTS__HPP
