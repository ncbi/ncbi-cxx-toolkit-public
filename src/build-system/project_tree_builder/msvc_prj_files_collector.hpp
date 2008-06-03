#ifndef PROJECT_TREE_BUILDER__MSVC_PRJ_FILES_COLLECTOR__HPP
#define PROJECT_TREE_BUILDER__MSVC_PRJ_FILES_COLLECTOR__HPP

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
#include <list>
#include <string>

#include "msvc_project_context.hpp"
#include "proj_item.hpp"

#include <corelib/ncbienv.hpp>
BEGIN_NCBI_SCOPE

class CMsvcPrjFilesCollector
{
public:
    CMsvcPrjFilesCollector(const CMsvcPrjProjectContext& project_context,
                           const list<SConfigInfo>&      project_configs,
                           const CProjItem&              project);

    ~CMsvcPrjFilesCollector(void);

    //All path are relative from ProjectDir
    const list<string>& SourceFiles   (void) const;
    const list<string>& HeaderFiles   (void) const;
    const list<string>& InlineFiles   (void) const;
    const list<string>& ResourceFiles (void) const;

private:
    // Prohibited to:
    CMsvcPrjFilesCollector(void);
    CMsvcPrjFilesCollector(const CMsvcPrjFilesCollector&);
    CMsvcPrjFilesCollector&	operator= (const CMsvcPrjFilesCollector&);

    void CollectSources(void);
    void CollectHeaders(void);
    void CollectInlines(void);
    void CollectResources(void);

private:
    const CMsvcPrjProjectContext* m_Context;
    const list<SConfigInfo>*      m_Configs;
    const CProjItem*              m_Project;

    list<string> m_SourceFiles;
    list<string> m_HeaderFiles;
    list<string> m_InlineFiles;
    list<string> m_ResourceFiles;
};


END_NCBI_SCOPE

#endif //PROJECT_TREE_BUILDER__MSVC_PRJ_FILES_COLLECTOR__HPP
