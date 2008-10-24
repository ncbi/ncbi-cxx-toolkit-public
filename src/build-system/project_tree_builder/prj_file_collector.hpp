#ifndef PROJECT_TREE_BUILDER__PRJ_FILE_COLLECTOR__HPP
#define PROJECT_TREE_BUILDER__PRJ_FILE_COLLECTOR__HPP

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
 * Author:  Andrei Gourianov
 *
 */

#include "proj_item.hpp"
#include "msvc_project_context.hpp"


BEGIN_NCBI_SCOPE

#if defined(NCBI_XCODE_BUILD) || defined(PSEUDO_XCODE)

/////////////////////////////////////////////////////////////////////////////
class CProjectFileCollector
{
public:
    CProjectFileCollector(const CProjItem& prj,
        const list<SConfigInfo>& configs, const string& output_dir);
    ~CProjectFileCollector(void);

    bool CheckProjectConfigs(void);
    void DoCollect(void);
    
    const list<SConfigInfo>& GetEnabledConfigs(void) const
    {
        return m_EnabledConfigs;
    }
    const CMsvcPrjProjectContext& GetProjectContext(void) const
    {
        return m_ProjContext;
    }
    const list<string>& GetSources(void) const
    {
        return m_Sources;
    }
    const list<string>& GetConfigurableSources(void) const
    {
        return m_ConfigurableSources;
    }
    const list<string>& GetHeaders(void) const
    {
        return m_Headers;
    }
    const list<string>& GetIncludeDirs(void) const
    {
        return m_IncludeDirs;
    }
    const list<string>& GetDataSpecs(void) const
    {
        return m_DataSpecs;
    }
    string GetDataSpecImports(const string& spec) const;

    static string GetFileExtension(const string& file);
    static bool IsProducedByDatatool(
        const CProjItem& projitem, const string& file);
    static bool IsInsideDatatoolSourceDir(const string& file);

private:
    void CollectSources(void);
    void CollectHeaders(void);
    void CollectIncludeDirs(void);
    void CollectDataSpecs(void);

    const CProjItem& m_ProjItem;
    CMsvcPrjProjectContext m_ProjContext;
    list<SConfigInfo> m_Configs;
    list<SConfigInfo> m_EnabledConfigs;
    string m_OutputDir;
    list<string> m_Sources;
    list<string> m_ConfigurableSources;
    list<string> m_Headers;
    list<string> m_IncludeDirs;
    list<string> m_DataSpecs;
};
#endif

END_NCBI_SCOPE

#endif //PROJECT_TREE_BUILDER__PRJ_FILE_COLLECTOR__HPP
