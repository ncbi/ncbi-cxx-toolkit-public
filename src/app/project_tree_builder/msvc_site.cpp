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

#include <app/project_tree_builder/stl_msvc_usage.hpp>
#include <app/project_tree_builder/msvc_site.hpp>
#include <app/project_tree_builder/proj_builder_app.hpp>
#include <app/project_tree_builder/msvc_prj_defines.hpp>

#include <algorithm>

#include <corelib/ncbistr.hpp>

BEGIN_NCBI_SCOPE


//-----------------------------------------------------------------------------
CMsvcSite::CMsvcSite(const CNcbiRegistry& registry)
    :m_Registry(registry)
{
    // Not provided requests
    string not_provided_requests_str = 
        m_Registry.GetString("Configure", "NotProvidedRequests", "");
    
    list<string> not_provided_requests_list;
    NStr::Split(not_provided_requests_str, LIST_SEPARATOR, 
                not_provided_requests_list);

    copy(not_provided_requests_list.begin(),
         not_provided_requests_list.end(),
         inserter(m_NotProvidedThing, m_NotProvidedThing.end()));
}


bool CMsvcSite::IsProvided(const string& thing) const
{
    return m_NotProvidedThing.find(thing) == m_NotProvidedThing.end();
}


bool CMsvcSite::IsDescribed(const string& section) const
{
    list<string> sections;
    m_Registry.EnumerateSections(&sections);
    return find(sections.begin(), sections.end(), section) != sections.end();
}


void CMsvcSite::GetComponents(const string& entry, 
                              list<string>* components) const
{
    components->clear();
    string comp_str = m_Registry.GetString(entry, "Component", "");
    NStr::Split(comp_str, " ,\t", *components);
}


void CMsvcSite::GetLibInfo(const string& lib, 
                           const SConfigInfo& config, SLibInfo* libinfo) const
{
    libinfo->Clear();

    libinfo->m_IncludeDir = GetOpt(m_Registry, lib, "INCLUDE", config);
    
    string defines_str    = GetOpt(m_Registry, lib, "DEFINES", config);
    NStr::Split(defines_str, LIST_SEPARATOR, libinfo->m_LibDefines);

    libinfo->m_LibPath    = GetOpt(m_Registry, lib, "LIBPATH", config);

    string libs_str = GetOpt(m_Registry, lib, "LIB", config);
    NStr::Split(libs_str, LIST_SEPARATOR, libinfo->m_Libs); //TODO
}


bool CMsvcSite::IsLibEnabledInConfig(const string&      lib, 
                                     const SConfigInfo& config) const
{
    string enabled_configs_str = m_Registry.GetString(lib, "CONFS", "");
    list<string> enabled_configs;
    NStr::Split(enabled_configs_str, 
                LIST_SEPARATOR, enabled_configs);

    return find(enabled_configs.begin(), 
                enabled_configs.end(), 
                config.m_Name) != enabled_configs.end();
}


string CMsvcSite::ResolveDefine(const string& define) const
{
    return m_Registry.GetString("Defines", define, "");
}


string CMsvcSite::GetConfigureDefinesPath(void) const
{
    return m_Registry.GetString("Configure", "DefinesPath", "");
}


void CMsvcSite::GetConfigureDefines(list<string>* defines) const
{
    defines->clear();
    string defines_str = m_Registry.GetString("Configure", "Defines", "");
    NStr::Split(defines_str, LIST_SEPARATOR, *defines);
}

END_NCBI_SCOPE
/*
 * ===========================================================================
 * $Log$
 * Revision 1.9  2004/03/22 14:48:38  gorelenk
 * Changed implementation of CMsvcSite::GetLibInfo .
 *
 * Revision 1.8  2004/02/24 20:53:16  gorelenk
 * Added implementation of member function IsLibEnabledInConfig
 * of class  CMsvcSite.
 *
 * Revision 1.7  2004/02/06 23:14:59  gorelenk
 * Implemented support of ASN projects, semi-auto configure,
 * CPPFLAGS support. Second working version.
 *
 * Revision 1.6  2004/02/05 16:31:19  gorelenk
 * Added definition of function GetComponents.
 *
 * Revision 1.5  2004/02/05 15:29:06  gorelenk
 * + Configuration information provision.
 *
 * Revision 1.4  2004/02/05 00:02:09  gorelenk
 * Added support of user site and  Makefile defines.
 *
 * Revision 1.3  2004/02/03 17:16:43  gorelenk
 * Removed implementation of method IsExplicitExclude from class CMsvcSite.
 *
 * Revision 1.2  2004/01/28 17:55:49  gorelenk
 * += For msvc makefile support of :
 *                 Requires tag, ExcludeProject tag,
 *                 AddToProject section (SourceFiles and IncludeDirs),
 *                 CustomBuild section.
 * += For support of user local site.
 *
 * Revision 1.1  2004/01/26 19:27:29  gorelenk
 * += MSVC meta makefile support
 * += MSVC project makefile support
 *
 * ===========================================================================
 */
