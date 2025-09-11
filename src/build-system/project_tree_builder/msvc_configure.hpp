#ifndef PROJECT_TREE_BULDER__MSVC_CONFIGURE__HPP
#define PROJECT_TREE_BULDER__MSVC_CONFIGURE__HPP

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


/// Configurator for msvc projects:

#include "msvc_site.hpp"
#include <corelib/ncbienv.hpp>

BEGIN_NCBI_SCOPE

class CMsvcConfigure
{
public:
    CMsvcConfigure(void);
    ~CMsvcConfigure(void);
    
    void Configure(CMsvcSite&               site, 
                   const list<SConfigInfo>& configs,
                   const string&            root_dir);

    void CreateConfH(CMsvcSite&             site, 
                   const list<SConfigInfo>& configs,
                   const string&            root_dir);

private:
    string       m_ConfigureDefinesPath;
    list<string> m_ConfigureDefines;
    typedef map<string, char> TConfigSite;
    TConfigSite  m_ConfigSite;
    bool         m_HaveBuildVer;
    bool         m_HaveRevision;
    bool         m_HaveSignature;

    void InitializeFrom(const CMsvcSite& site);
    bool ProcessDefine (const string& define, const CMsvcSite& site, const SConfigInfo& config) const;
    void AnalyzeDefines(CMsvcSite& site, const string& root_dir, const SConfigInfo& config, const CBuildType& build_type);
    string GetSignature(const SConfigInfo& config);

    CNcbiOfstream& WriteNcbiconfHeader(CNcbiOfstream& ofs) const;
    void WriteExtraDefines(CMsvcSite& site, const string& root_dir, const SConfigInfo& config);
    void WriteBuildVer(CMsvcSite& site, const string& root_dir, const SConfigInfo& config);
    void WriteNcbiconfMsvcSite(const string& full_path, const string& signature) const;

    // No value semantics
    CMsvcConfigure(const CMsvcConfigure&);
    CMsvcConfigure& operator= (CMsvcConfigure&);
};

END_NCBI_SCOPE

#endif //PROJECT_TREE_BULDER__MSVC_CONFIGURE__HPP
