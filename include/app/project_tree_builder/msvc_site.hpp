#ifndef PROJECT_TREE_BUILDER_MSVC_SITE__HPP
#define PROJECT_TREE_BUILDER_MSVC_SITE__HPP
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
#include <corelib/ncbireg.hpp>
#include <set>
#include <app/project_tree_builder/msvc_prj_utils.hpp>
#include <app/project_tree_builder/resolver.hpp>
#include <corelib/ncbienv.hpp>
BEGIN_NCBI_SCOPE

/////////////////////////////////////////////////////////////////////////////
///
/// SLibInfo --
///
/// Abstraction of lib description in site.
///
/// Provides information about 
/// additional include dir, library path and libs list.

struct SLibInfo
{
    list<string> m_IncludeDir;
    list<string> m_LibDefines;
    string       m_LibPath;
    list<string> m_Libs;
    list<string> m_StdLibs;
    list<string> m_Macro;

    bool IsEmpty(void) const
    {
        return m_IncludeDir.empty() &&
               m_LibDefines.empty() &&
               m_LibPath.empty()    && 
               m_Libs.empty()       &&
               m_StdLibs.empty()    &&
               m_Macro.empty();
    }
    void Clear(void)
    {
        m_IncludeDir.clear();
        m_LibDefines.clear();
        m_LibPath.erase();
        m_Libs.clear();
        m_StdLibs.clear();
        m_Macro.clear();
    }
};

/////////////////////////////////////////////////////////////////////////////
///
/// CMsvcSite --
///
/// Abstraction of user site for building of C++ projects.
///
/// Provides information about libraries availability as well as implicit
/// exclusion of some branches from project tree.

class CMsvcSite
{
public:
    CMsvcSite(const CNcbiRegistry& registry);

    // Is REQUIRES provided?
    bool IsProvided(const string& thing) const;

    /// Get components from site
    void GetComponents(const string& entry, list<string>* components) const;

    /// Is section present in site registry?
    bool IsDescribed(const string& section) const;

    string ProcessMacros(string data) const;
    // Get library (LIBS) description
    void GetLibInfo(const string& lib, 
                    const SConfigInfo& config, SLibInfo* libinfo) const;
    // Is this lib available in certain config
    bool IsLibEnabledInConfig(const string&      lib, 
                              const SConfigInfo& config) const;
    
    // Resolve define (now from CPPFLAGS)
    string ResolveDefine(const string& define) const;

    // Configure related:
    // Path from tree root to file where configure defines must be.
    string GetConfigureDefinesPath(void) const;
    // What we have to define:
    void   GetConfigureDefines    (list<string>* defines) const;

    // Lib Choices related:
    enum ELibChoice {
        eUnknown,
        eLib,
        e3PartyLib
    };
    struct SLibChoice
    {
        SLibChoice(void);

        SLibChoice(const CMsvcSite& site,
                   const string&    lib,
                   const string&    lib_3party);

        ELibChoice m_Choice;
        string     m_LibId;
        string     m_3PartyLib;
    };
    bool IsLibWithChoice            (const string& lib_id) const;
    bool Is3PartyLibWithChoice      (const string& lib3party_id) const;

    ELibChoice GetChoiceForLib      (const string& lib_id) const;
    ELibChoice GetChoiceFor3PartyLib(const string& lib3party_id,
                                     const SConfigInfo& cfg_info) const;

    void GetLibChoiceIncludes(const string& cpp_flags_define,
                              list<string>* abs_includes) const;
    void GetLibChoiceIncludes(const string& cpp_flags_define,
                              const SConfigInfo& cfg_info,
                              list<string>* abs_includes) const;
    void GetLibInclude(const string& lib_id,
                       const SConfigInfo& cfg_info,
                       list<string>* includes) const;

    SLibChoice GetLibChoiceForLib   (const string& lib_id) const;
    SLibChoice GetLibChoiceFor3PartyLib   (const string& lib3party_id) const;


    
    string GetAppDefaultResource(void) const;


    void   GetThirdPartyLibsToInstall    (list<string>* libs) const;
    string GetThirdPartyLibsBinPathSuffix(void) const;
    string GetThirdPartyLibsBinSubDir    (void) const;
    void   GetStandardFeatures           (list<string>& features) const;
    
    void ProcessMacros (const list<SConfigInfo>& configs);
    const CSymResolver& GetMacros(void) const
    {
        return m_Macros;
    }

    static bool IsLibOk(const SLibInfo& lib_info, bool silent = false);
private:
    const CNcbiRegistry& m_Registry;
    
    set<string> m_NotProvidedThing;

    list<SLibChoice> m_LibChoices;

    CSymResolver m_Macros;

    /// Prohibited to:
    CMsvcSite(void);
    CMsvcSite(const CMsvcSite&);
    CMsvcSite& operator= (const CMsvcSite&);
};

END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.22  2005/02/15 19:03:51  gouriano
 * Added list of standard features
 *
 * Revision 1.21  2004/12/30 17:46:46  gouriano
 * Added StdLibs list
 *
 * Revision 1.20  2004/11/23 20:12:48  gouriano
 * Tune libraries with the choice for each configuration independently
 *
 * Revision 1.19  2004/11/02 18:09:51  gouriano
 * Allow macros in the definition of 3rd party library paths
 *
 * Revision 1.18  2004/08/25 19:39:04  gouriano
 * Implemented optional dependency on a third party library
 *
 * Revision 1.17  2004/07/20 13:39:29  gouriano
 * Added conditional macro definition
 *
 * Revision 1.16  2004/06/10 15:12:55  gorelenk
 * Added newline at the file end to avoid GCC warning.
 *
 * Revision 1.15  2004/06/01 16:02:40  gorelenk
 * + GetLibChoiceForLib to class CMsvcSite,
 * + default constructor for struct SLibChoice.
 *
 * Revision 1.14  2004/05/24 14:39:17  gorelenk
 * Added declaration member-functions for third-party libs
 * auto-install support: GetThirdPartyLibsToInstall,
 * GetThirdPartyLibsBinPathSuffix, GetThirdPartyLibsBinSubDir
 * to class CMsvcSite.
 *
 * Revision 1.13  2004/05/17 14:35:21  gorelenk
 * Added declaration of GetAppDefaultResource to class CMsvcSite.
 *
 * Revision 1.12  2004/05/13 14:54:01  gorelenk
 * Added GetLibChoiceIncludes to class CMsvcSite.
 *
 * Revision 1.11  2004/04/19 15:37:43  gorelenk
 * Added lib choice related members : enum ELibChoice, struct SLibChoice,
 * functions IsLibWithChoice, Is3PartyLibWithChoice,
 * GetChoiceForLib and GetChoiceFor3PartyLib to class CMsvcSite.
 *
 * Revision 1.10  2004/03/22 14:45:38  gorelenk
 * Added member m_LibDefines to struct SLibInfo.
 *
 * Revision 1.9  2004/02/24 20:48:49  gorelenk
 * Added declaration of member-function IsLibEnabledInConfig
 * to class CMsvcSite.
 *
 * Revision 1.8  2004/02/24 17:16:30  gorelenk
 * Redesigned member-function IsEmpty of struct SLibInfo.
 *
 * Revision 1.7  2004/02/06 23:15:40  gorelenk
 * Implemented support of ASN projects, semi-auto configure,
 * CPPFLAGS support. Second working version.
 *
 * Revision 1.6  2004/02/05 16:32:22  gorelenk
 * Added declaration of function GetComponents.
 *
 * Revision 1.5  2004/02/05 15:28:14  gorelenk
 * + Configuration information provision.
 *
 * Revision 1.4  2004/02/04 23:16:25  gorelenk
 * To class CMsvcSite added member function ResolveDefine.
 *
 * Revision 1.3  2004/02/03 17:06:44  gorelenk
 * Removed members from class CMsvcSite.
 *
 * Revision 1.2  2004/01/28 17:55:06  gorelenk
 * += For msvc makefile support of :
 *                 Requires tag, ExcludeProject tag,
 *                 AddToProject section (SourceFiles and IncludeDirs),
 *                 CustomBuild section.
 * += For support of user local site.
 *
 * Revision 1.1  2004/01/26 19:25:41  gorelenk
 * += MSVC meta makefile support
 * += MSVC project makefile support
 *
 * ===========================================================================
 */

#endif //PROJECT_TREE_BUILDER_MSVC_SITE__HPP
