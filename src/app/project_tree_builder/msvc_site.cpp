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
#include <corelib/ncbistr.hpp>

BEGIN_NCBI_SCOPE


static void s_LoadSet(const CNcbiRegistry& registry, 
                      const string& section, 
                      const string& name, set<string>* values)
{
    values->clear();

    string vals_str = 
        registry.GetString(section, name, "");
    list<string> vals_list;
    NStr::Split(vals_str, " \t,", vals_list);
    ITERATE(list<string>, p, vals_list)
        values->insert(*p);
}


CMsvcSite::CMsvcSite(const CNcbiRegistry& registry)
{
    s_LoadSet(registry, 
              "ProjectTree", "NotProvidedRequests", &m_NotProvidedThing);

    s_LoadSet(registry, 
              "ProjectTree", "ImplicitExclude", &m_ImplicitExcludeNodes);
}


bool CMsvcSite::IsProvided(const string& thing) const
{
    return m_NotProvidedThing.find(thing) == m_NotProvidedThing.end();
}


bool CMsvcSite::IsImplicitExclude(const string& node) const
{
    return m_ImplicitExcludeNodes.find(node) != m_ImplicitExcludeNodes.end();
}


END_NCBI_SCOPE
/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2004/01/26 19:27:29  gorelenk
 * += MSVC meta makefile support
 * += MSVC project makefile support
 *
 * ===========================================================================
 */
