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

#include "proj_item.hpp"
#include "proj_tree.hpp"
#include "msvc_prj_utils.hpp"

#include <corelib/ncbienv.hpp>
BEGIN_NCBI_SCOPE

#if NCBI_COMPILER_MSVC
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
    const CVisualStudioProject& GetVisualStudioProject(void) const
    {
        return m_Xmlprj;
    }

private:
    CVisualStudioProject m_Xmlprj;
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
#endif //NCBI_COMPILER_MSVC

END_NCBI_SCOPE

#endif  //PROJECT_TREE_BULDER__MSVC_MASTERPROJECT_GENERATOR_HPP
