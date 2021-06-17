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


#include "msvc_project_context.hpp"
#include "msvc_prj_utils.hpp"

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

//---------------------------------------------------------------------------
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

//---------------------------------------------------------------------------
class CUtilityProject : public CObject
{
public:
    CUtilityProject(
        const string& full_path, const string& guid, const string& name);
    virtual ~CUtilityProject(void);

    virtual bool IsIncluded(const CPrjContext& prj) const;
    
    bool HasFilter(void) const {
        return m_HasFilter;
    }
    bool SaveEmpty(void) const {
        return m_SaveEmpty;
    }
    const string& Path(void) const {
        return m_Full_path;
    }
    const string& Guid(void) const {
        return m_Guid;
    }
    const string& Name(void) const {
        return m_Name;
    }
protected:
    bool m_HasFilter;
    bool m_SaveEmpty;
    const string m_Full_path;
    const string m_Guid;
    const string m_Name;
};

//---------------------------------------------------------------------------
class CMsvcSolutionGenerator
{
public:
    CMsvcSolutionGenerator(const list<SConfigInfo>& configs);
    ~CMsvcSolutionGenerator(void);
    
    void AddProject(const CProjItem& project);
    
    void AddUtilityProject  (const string& full_path, const string& guid, const string& name);
    void AddConfigureProject(const string& full_path, const string& guid, const string& name);
    void AddBuildAllProject (const string& full_path, const string& guid, const string& name);
    void AddAsnAllProject   (const string& full_path, const string& guid, const string& name);
    void AddLibsAllProject  (const string& full_path, const string& guid, const string& name);
    void AddTagProject      (const string& full_path, const string& guid, const string& name,
                             const string& tags);

    void VerifyProjectDependencies(void);
    void SaveSolution(const string& file_path);
    
private:
    list<SConfigInfo> m_Configs;

    string m_SolutionDir;


    // Basename / GUID
    typedef pair<string, string> TUtilityProject;
    // Utility projects
    map<string, string> m_PathToName;
    
    vector< CRef<CUtilityProject> > m_Utils;

    // Real projects
    typedef map<CProjKey, CPrjContext> TProjects;
    TProjects m_Projects;

    // Writers:
    void CollectLibToLibDependencies(
        set<string>& dep, set<string>& visited,
        const CPrjContext& lib, const CPrjContext& lib_dep);

    void WriteProjectAndSection(CNcbiOfstream&     ofs, 
                                const CPrjContext& project);
    
    void BeginUtilityProject   (const CUtilityProject& project, 
                                CNcbiOfstream& ofs);
    void EndUtilityProject   (const CUtilityProject& project, 
                                CNcbiOfstream& ofs);
    bool WriteUtilityProject   (const CUtilityProject& project, 
                                CNcbiOfstream& ofs);

    void WriteProjectConfigurations(CNcbiOfstream&     ofs, 
                                    const CPrjContext& project);
    void WriteProjectConfigurations(CNcbiOfstream&     ofs, 
                                    const list<string>& project);

    // Prohibited to:
    CMsvcSolutionGenerator(void);
    CMsvcSolutionGenerator(const CMsvcSolutionGenerator&);
    CMsvcSolutionGenerator& operator= (const CMsvcSolutionGenerator&);
};
#endif //NCBI_COMPILER_MSVC

END_NCBI_SCOPE

#endif //PROJECT_TREE_BUILDER__MSVC_SLN_GENERATOR__HPP
