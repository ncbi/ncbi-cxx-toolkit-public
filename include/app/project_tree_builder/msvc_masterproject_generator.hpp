#ifndef PROJECT_TREE_BULDER__MSVC_MASTERPROJECT_GENERATOR_HPP
#define PROJECT_TREE_BULDER__MSVC_MASTERPROJECT_GENERATOR_HPP

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
#include <app/project_tree_builder/proj_tree.hpp>
#include <app/project_tree_builder/msvc_prj_utils.hpp>

#include <corelib/ncbienv.hpp>
BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

/////////////////////////////////////////////////////////////////////////////
///
/// CMsvcMasterProjectGenerator --
///
/// Generator of _MasterProject for MSVC 7.10 solution.
///
/// Generates utility project from the project tree. Projects hierarchy in the
/// project tree will be represented as folders hierarchy in _MasterProject.
/// Every project will be represented as a file <projectname>.bulid with 
/// attached custom build command that allows to build this project.

class CMsvcMasterProjectGenerator
{
public:
    CMsvcMasterProjectGenerator(const CProjectItemsTree& tree,
                                const list<SConfigInfo>& configs,
                                const string&            project_dir);

    ~CMsvcMasterProjectGenerator(void);

    void SaveProject();

    string GetPath() const;

private:
    const CProjectItemsTree& m_Tree;
    list<SConfigInfo> m_Configs;


  	const string m_Name;

    const string m_ProjectDir;

    const string m_ProjectItemExt;

    string m_CustomBuildCommand;

    const string m_FilesSubdir;

    
   
    void AddProjectToFilter(CRef<CFilter>& filter, const CProjKey& project_id);

    void CreateProjectFileItem(const CProjKey& project_id);

    void ProcessTreeFolder(const SProjectTreeFolder&  folder, 
                           CSerialObject*             parent);

    /// Prohibited to:
    CMsvcMasterProjectGenerator(void);
    CMsvcMasterProjectGenerator(const CMsvcMasterProjectGenerator&);
    CMsvcMasterProjectGenerator& operator= 
        (const CMsvcMasterProjectGenerator&);

};


END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.11  2004/06/10 15:12:55  gorelenk
 * Added newline at the file end to avoid GCC warning.
 *
 * Revision 1.10  2004/03/02 16:34:12  gorelenk
 * Added include to proj_tree.hpp.
 *
 * Revision 1.9  2004/02/20 22:54:45  gorelenk
 * Added analysis of ASN projects depends.
 *
 * Revision 1.8  2004/02/12 17:48:12  gorelenk
 * Re-designed of projects saving. Added member-function GetPath().
 *
 * Revision 1.7  2004/02/12 16:22:39  gorelenk
 * Changed generation of command line for custom build info.
 *
 * Revision 1.6  2004/02/10 18:14:31  gorelenk
 * Implemented overwriting of the _MasterProject only in case when it was
 * realy changed.
 *
 * Revision 1.5  2004/01/30 20:40:41  gorelenk
 * changed procedure of folders creation
 *
 * Revision 1.4  2004/01/28 17:55:05  gorelenk
 * += For msvc makefile support of :
 *                 Requires tag, ExcludeProject tag,
 *                 AddToProject section (SourceFiles and IncludeDirs),
 *                 CustomBuild section.
 * += For support of user local site.
 *
 * Revision 1.3  2004/01/26 19:25:41  gorelenk
 * += MSVC meta makefile support
 * += MSVC project makefile support
 *
 * Revision 1.2  2004/01/22 17:57:08  gorelenk
 * first version
 *
 * ===========================================================================
 */

#endif  //PROJECT_TREE_BULDER__MSVC_MASTERPROJECT_GENERATOR_HPP
