#ifndef PROJECT_TREE_BUILDER__MSVC_SLN_GENERATOR__HPP
#define PROJECT_TREE_BUILDER__MSVC_SLN_GENERATOR__HPP

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


#include <app/project_tree_builder/msvc_project_context.hpp>
#include <app/project_tree_builder/msvc_prj_utils.hpp>

#include <corelib/ncbienv.hpp>
BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
///
/// CMsvcSolutionGenerator --
///
/// Generator of MSVC 7.10 solution file.
///
/// Generates solution file from projects set.

#if NCBI_COMPILER_MSVC

class CMsvcSolutionGenerator
{
public:
    CMsvcSolutionGenerator(const list<SConfigInfo>& configs);
    ~CMsvcSolutionGenerator(void);
    
    void AddProject(const CProjItem& project);
    
    void AddUtilityProject  (const string& full_path, const CVisualStudioProject& prj);
    void AddConfigureProject(const string& full_path, const CVisualStudioProject& prj);
    void AddBuildAllProject (const string& full_path, const CVisualStudioProject& prj);
    void AddAsnAllProject   (const string& full_path, const CVisualStudioProject& prj);

    void VerifyProjectDependencies(void);
    void SaveSolution(const string& file_path);
    
private:
    list<SConfigInfo> m_Configs;

    string m_SolutionDir;


    // Basename / GUID
    typedef pair<string, string> TUtilityProject;
    // Utility projects
    list<TUtilityProject> m_UtilityProjects;
    list<TUtilityProject> m_ConfigureProjects;
    // BuildAll utility project
    TUtilityProject m_BuildAllProject; 
    TUtilityProject m_AsnAllProject; 
    map<string, string> m_PathToName;

    class CPrjContext
    {
    public:

        CPrjContext(void);
        CPrjContext(const CPrjContext& context);
        CPrjContext(const CProjItem& project);
        CPrjContext& operator= (const CPrjContext& context);
        ~CPrjContext(void);

        CProjItem m_Project;

        string    m_GUID;
        string    m_ProjectName;
        string    m_ProjectPath;
    
    private:
        void Clear(void);
        void SetFrom(const CPrjContext& project_context);
    };
    // Real projects
    typedef map<CProjKey, CPrjContext> TProjects;
    TProjects m_Projects;

    // Writers:
    void WriteProjectAndSection(CNcbiOfstream&     ofs, 
                                const CPrjContext& project);
    
    void BeginUtilityProject   (const TUtilityProject& project, 
                                CNcbiOfstream& ofs);
    void EndUtilityProject   (const TUtilityProject& project, 
                                CNcbiOfstream& ofs);

    void WriteUtilityProject   (const TUtilityProject& project, 
                                CNcbiOfstream& ofs);

    void WriteConfigureProject (const TUtilityProject& project, 
                                CNcbiOfstream& ofs);

    void WriteBuildAllProject  (const TUtilityProject& project, 
                                CNcbiOfstream& ofs);

    void WriteAsnAllProject    (const TUtilityProject& project, 
                                CNcbiOfstream& ofs);

    void WriteProjectConfigurations(CNcbiOfstream&     ofs, 
                                    const CPrjContext& project);
    void WriteProjectConfigurations(CNcbiOfstream&     ofs, 
                                    const list<string>& project);

    void WriteUtilityProjectConfiguration(const TUtilityProject& project, 
                                          CNcbiOfstream&         ofs);

    // Prohibited to:
    CMsvcSolutionGenerator(void);
    CMsvcSolutionGenerator(const CMsvcSolutionGenerator&);
    CMsvcSolutionGenerator& operator= (const CMsvcSolutionGenerator&);
};
#endif //NCBI_COMPILER_MSVC

END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.18  2006/09/07 15:09:24  gouriano
 * Disable MS Visual Studio-specific code on UNIX
 *
 * Revision 1.17  2006/02/21 19:14:18  gouriano
 * Added DATASPEC_ALL project
 *
 * Revision 1.16  2006/02/15 19:47:44  gouriano
 * Exclude projects with unmet requirements from BUILD-ALL
 *
 * Revision 1.15  2006/01/23 18:26:33  gouriano
 * Generate project GUID early, sort projects in solution by GUID
 *
 * Revision 1.14  2006/01/10 17:39:42  gouriano
 * Corrected solution generation for MSVC 2005 Express
 *
 * Revision 1.13  2005/12/27 14:58:14  gouriano
 * Adjustments for MSVC 2005 Express
 *
 * Revision 1.12  2004/06/10 15:12:55  gorelenk
 * Added newline at the file end to avoid GCC warning.
 *
 * Revision 1.11  2004/03/18 17:41:03  gorelenk
 * Aligned classes member-functions parameters inside declarations.
 *
 * Revision 1.10  2004/02/25 19:45:38  gorelenk
 * +BuildAll utility project.
 *
 * Revision 1.9  2004/02/20 22:54:46  gorelenk
 * Added analysis of ASN projects depends.
 *
 * Revision 1.8  2004/02/12 23:13:49  gorelenk
 * Declared function for MSVC utility project creation.
 *
 * Revision 1.7  2004/02/12 17:46:35  gorelenk
 * Re-designed addition of utility projects.
 *
 * Revision 1.6  2004/02/12 16:22:40  gorelenk
 * Changed generation of command line for custom build info.
 *
 * Revision 1.5  2004/01/26 19:25:42  gorelenk
 * += MSVC meta makefile support
 * += MSVC project makefile support
 *
 * Revision 1.4  2004/01/22 17:57:09  gorelenk
 * first version
 *
 * ===========================================================================
 */

#endif //PROJECT_TREE_BUILDER__MSVC_SLN_GENERATOR__HPP
