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

#include <app/project_tree_builder/msvc_site.hpp>

#include <corelib/ncbienv.hpp>
BEGIN_NCBI_SCOPE

class CMsvcConfigure
{
public:
    CMsvcConfigure(void);
    ~CMsvcConfigure(void);
    
    void operator()   (CMsvcSite&         site, 
                       const list<SConfigInfo>& configs,
                       const string&            root_dir);

private:
    string       m_ConfigureDefinesPath;
    list<string> m_ConfigureDefines;

    void InitializeFrom(const CMsvcSite& site);

    bool ProcessDefine (const string&            define, 
                        const CMsvcSite&         site, 
                        const list<SConfigInfo>& configs) const;

    typedef map<string, char> TConfigSite;
    TConfigSite m_ConfigSite;
    void WriteNcbiconfMsvcSite(const string& full_path) const;

    // No value semantics
    CMsvcConfigure(const CMsvcConfigure&);
    CMsvcConfigure& operator= (CMsvcConfigure&);

};

END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.5  2004/07/20 13:39:29  gouriano
 * Added conditional macro definition
 *
 * Revision 1.4  2004/06/10 15:12:55  gorelenk
 * Added newline at the file end to avoid GCC warning.
 *
 * Revision 1.3  2004/03/18 17:41:03  gorelenk
 * Aligned classes member-functions parameters inside declarations.
 *
 * Revision 1.2  2004/02/06 23:15:39  gorelenk
 * Implemented support of ASN projects, semi-auto configure,
 * CPPFLAGS support. Second working version.
 *
 * Revision 1.1  2004/02/04 23:05:48  gorelenk
 * Initial revision
 *
 * ===========================================================================
 */

#endif //PROJECT_TREE_BULDER__MSVC_CONFIGURE__HPP
