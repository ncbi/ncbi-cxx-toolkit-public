#ifndef PROJECT_TREE_BULDER__MSVC_CONFIGURE_PRJ_GENERATOR__HPP
#define PROJECT_TREE_BULDER__MSVC_CONFIGURE_PRJ_GENERATOR__HPP

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

#include <set>

#include <app/project_tree_builder/msvc_prj_utils.hpp>

#include <corelib/ncbienv.hpp>
BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

/////////////////////////////////////////////////////////////////////////////
///
/// CMsvcConfigureProjectGenerator --
///
/// Generator of _MasterProject for MSVC 7.10 solution.
///
/// Generates utility project from the project tree. Projects hierarchy in the
/// project tree will be represented as folders hierarchy in _MasterProject.
/// Every project will be represented as a file <projectname>.bulid with 
/// attached custom build command that allows to build this project.

class CMsvcConfigureProjectGenerator
{
public:
    CMsvcConfigureProjectGenerator(const string&            output_dir,
                                   const list<SConfigInfo>& configs,
                                   bool                     dll_build,
                                   const string&            project_dir,
                                   const string&            tree_root,
                                   const string&            subtree_to_build,
                                   const string&            solution_to_build,
                                   bool  build_ptb);

    ~CMsvcConfigureProjectGenerator(void);


    void SaveProject(bool with_gui);
    string GetPath(bool with_gui) const;

private:
 
  	const string m_Name;
  	const string m_NameGui;

    const string      m_OutputDir;
    list<SConfigInfo> m_Configs;
    bool              m_DllBuild;

    const string      m_ProjectDir;
    const string      m_TreeRoot;
    const string      m_SubtreeToBuild;
    const string      m_SolutionToBuild;

    const string m_ProjectItemExt;

    string       m_CustomBuildCommand;

    const string m_SrcFileName;
    const string m_SrcFileNameGui;

    const string m_FilesSubdir;
    bool m_BuildPtb;

    void CreateProjectFileItem(bool with_gui) const;    

    /// Prohibited to:
    CMsvcConfigureProjectGenerator(void);
    CMsvcConfigureProjectGenerator(const CMsvcConfigureProjectGenerator&);
    CMsvcConfigureProjectGenerator& operator= 
        (const CMsvcConfigureProjectGenerator&);

};


END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.6  2005/05/10 17:29:09  gouriano
 * Added configure_dialog project
 *
 * Revision 1.5  2005/03/23 19:32:32  gouriano
 * Make it possible to exclude PTB build when configuring
 *
 * Revision 1.4  2004/06/10 15:12:55  gorelenk
 * Added newline at the file end to avoid GCC warning.
 *
 * Revision 1.3  2004/03/10 21:26:19  gorelenk
 * Changed CMsvcConfigureProjectGenerator constructor.
 *
 * Revision 1.2  2004/02/12 17:48:11  gorelenk
 * Re-designed of projects saving. Added member-function GetPath().
 *
 * Revision 1.1  2004/02/12 16:21:12  gorelenk
 * Initial Revision.
 *
 * ===========================================================================
 */


#endif //PROJECT_TREE_BULDER__MSVC_CONFIGURE_PRJ_GENERATOR__HPP
