/*  $Id$
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
 * Author:  Anatoliy Kuznetsov
 *
 * File Description:
 *   Plugin manager implementations
 *
 * ===========================================================================
 */

#include <corelib/plugin_manager.hpp>
#include <corelib/ncbidll.hpp>

BEGIN_NCBI_SCOPE

/////////////////////////////////////////////////////////////////////////////
//  CPluginManager_DllResolver::
//


CPluginManager_DllResolver::CPluginManager_DllResolver(void)
 : m_DllNamePrefix("ncbi_plugin"),
   m_EntryPointPrefix("NCBI_EntryPoint")
{
}

CPluginManager_DllResolver::~CPluginManager_DllResolver(void)
{}

string 
CPluginManager_DllResolver::GetDllName(
                                       const string&       plugin_name,
                                       const string&       driver_name,
                                       const CVersionInfo& version) const
{
    string prefix = GetDllNamePrefix();
    string name = prefix;
    if (!plugin_name.empty()) {
        name.append("_");
        name.append(plugin_name);
    }

    if (!driver_name.empty()) {
        name.append("_");
        name.append(driver_name);
    }

    if (version.IsAny()) {
        return name;
    } else {
        
#if defined(NCBI_OS_MSWIN)
        string delimiter = "_";

#elif defined(NCBI_OS_UNIX)
        string delimiter = ".";
        name.append(".so");
#endif

        name.append(delimiter);
        name.append(NStr::IntToString(version.GetMajor()));
        name.append(delimiter);
        name.append(NStr::IntToString(version.GetMinor()));
        name.append(delimiter);
        name.append(NStr::IntToString(version.GetPatchLevel()));
    }

    return name;
}

string 
CPluginManager_DllResolver::GetDllNameMask(
        const string&       plugin_name,
        const string&       driver_name,
        const CVersionInfo& version) const
{
    string prefix = GetDllNamePrefix();
    string name = prefix;

    name.append("_");
    if (plugin_name.empty()) {
        name.append("*");
    } else {
        name.append(plugin_name);
    }

    name.append("_");

    if (driver_name.empty()) {
        name.append("*");
    } else {
        name.append(driver_name);
    } 

    if (version.IsAny()) {
#if defined(NCBI_OS_MSWIN)
        name.append("*.dll");
#elif defined(NCBI_OS_UNIX)
        name.append(".so*");
#endif
    } else {
        
#if defined(NCBI_OS_MSWIN)
        string delimiter = "_";

#elif defined(NCBI_OS_UNIX)
        string delimiter = ".";
        name.append(".so");
#endif

        name.append(delimiter);
        if (version.GetMajor() <= 0) {
            name.append("*");
        } else {
            name.append(NStr::IntToString(version.GetMajor()));
        }

        name.append(delimiter);

        if (version.GetMinor() <= 0) {
            name.append("*");
        } else {
            name.append(NStr::IntToString(version.GetMinor()));
        }

        name.append(delimiter);
        name.append("*");  // always get the best patch level
        
    }

    return name;
}

string 
CPluginManager_DllResolver::GetEntryPointName(
                      const string&       interface_name,
                      const string&       driver_name) const
{
    string prefix = GetEntryPointPrefix();
    string name = prefix;
    if (!interface_name.empty()) {
        name.append("_");
        name.append(interface_name);
    }

    if (!driver_name.empty()) {
        name.append("_");
        name.append(driver_name);
    }

    return name;
}


string CPluginManager_DllResolver::GetEntryPointPrefix() const 
{ 
    return m_EntryPointPrefix; 
}

string CPluginManager_DllResolver::GetDllNamePrefix() const 
{ 
    return m_DllNamePrefix; 
}

void CPluginManager_DllResolver::SetDllNamePrefix(const string& prefix)
{ 
    m_DllNamePrefix = prefix; 
}


END_NCBI_SCOPE
