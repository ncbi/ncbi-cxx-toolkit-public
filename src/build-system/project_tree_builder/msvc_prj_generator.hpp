#ifndef PROJECT_TREE_BUILDER__MSVC_PRJ_GENERATOR__HPP
#define PROJECT_TREE_BUILDER__MSVC_PRJ_GENERATOR__HPP

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
#include "msvc_project_context.hpp"
#include "msvc_prj_utils.hpp"
#include "msvc_prj_files_collector.hpp"

#include <corelib/ncbienv.hpp>


BEGIN_NCBI_SCOPE

#if NCBI_COMPILER_MSVC
/////////////////////////////////////////////////////////////////////////////
///
/// CMsvcProjectGenerator --
///
/// Generator MSVC 7.10 project (*.vcproj file).
///
/// Generates MSVC 7.10 C++ project from the project tree item and save this
/// project to the appropriate place in "compilers" branch of the build tree.

class CMsvcProjectGenerator
{
public:
    CMsvcProjectGenerator(const list<SConfigInfo>& configs);
    ~CMsvcProjectGenerator(void);
    
    // Check configuration availability
    bool CheckProjectConfigs(
                            CMsvcPrjProjectContext& project_context,
                            CProjItem& prj);
    void AnalyzePackageExport(
                            CMsvcPrjProjectContext& project_context,
                            CProjItem& prj);
    void GetDefaultPch(
                            CMsvcPrjProjectContext& project_context,
                            CProjItem& prj);

    void Generate(CProjItem& prj);
    void GenerateMsbuild(
                            CMsvcPrjFilesCollector& collector,
                            CMsvcPrjProjectContext& project_context,
                            CProjItem& prj);
    void GenerateMsbuildFilters(
                            CMsvcPrjFilesCollector& collector,
                            CMsvcPrjProjectContext& project_context,
                            CProjItem& prj);
    void GenerateMsbuildUser(
                            CMsvcPrjFilesCollector& collector,
                            CMsvcPrjProjectContext& project_context,
                            CProjItem& prj);

    static void GeneratePropertySheets(const string& solution);
private:
    list<SConfigInfo> m_Configs;
    list<SConfigInfo> m_project_configs;
    string m_pkg_export_command;
    string m_pkg_export_output;
    string m_pkg_export_input;
    string m_pch_default;
    string m_pch_define;

    /// Prohibited to.
    CMsvcProjectGenerator(void);
    CMsvcProjectGenerator(const CMsvcProjectGenerator&);
    CMsvcProjectGenerator& operator= (const CMsvcProjectGenerator&);
};

/// Fill-In MSVC Utility project
void CreateUtilityProject(const string&            name, 
                          const list<SConfigInfo>& configs, 
                          CVisualStudioProject*    project);

#endif //NCBI_COMPILER_MSVC

END_NCBI_SCOPE

#endif //PROJECT_TREE_BUILDER__MSVC_PRJ_GENERATOR__HPP
