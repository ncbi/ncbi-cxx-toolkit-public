#ifndef MSVC_SLN_GENERATOR_HEADER
#define MSVC_SLN_GENERATOR_HEADER

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

class CMsvcSolutionGenerator
{
public:
    CMsvcSolutionGenerator(const list<SConfigInfo>& configs);
    ~CMsvcSolutionGenerator(void);
    
    void AddProject(const CProjItem& project);
    
    void AddMasterProject   (const string& full_path);
    void AddConfigureProject(const string& full_path);

    void SaveSolution(const string& file_path);
    
private:
    list<SConfigInfo> m_Configs;

    string m_SolutionDir;

    /// Basename / GUID
    typedef pair<string, string> TUtilityProject;
    TUtilityProject m_MasterProject;
    bool IsSetMasterProject(void) const;

    /// Basename / GUID
    TUtilityProject m_ConfigureProject;
    bool IsSetConfigureProject(void) const;

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

    typedef map<string, CPrjContext> TProjects;
    TProjects m_Projects;

    //Writers:
    void WriteProjectAndSection(CNcbiOfstream&     ofs, 
                                const CPrjContext& project);
    
    void WriteUtilityProject   (const TUtilityProject& project, 
                                CNcbiOfstream& ofs);

    void WriteProjectConfigurations(CNcbiOfstream&     ofs, 
                                    const CPrjContext& project);

    void WriteUtilityProjectConfiguration(const TUtilityProject& project, 
                                          CNcbiOfstream& ofs);

    // Prohibited to:
    CMsvcSolutionGenerator(void);
    CMsvcSolutionGenerator(const CMsvcSolutionGenerator&);
    CMsvcSolutionGenerator& operator= (const CMsvcSolutionGenerator&);
};


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
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

#endif // MSVC_SLN_GENERATOR_HEADER