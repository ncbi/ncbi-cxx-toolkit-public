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

#include <ncbi_pch.hpp>
#include <corelib/plugin_manager.hpp>
#include <corelib/ncbidll.hpp>

BEGIN_NCBI_SCOPE

/////////////////////////////////////////////////////////////////////////////
//  CPluginManager_DllResolver::
//

CPluginManager_DllResolver::CPluginManager_DllResolver(void)
 : m_DllNamePrefix("ncbi_plugin"),
   m_EntryPointPrefix("NCBI_EntryPoint"),
   m_Version(CVersionInfo::kAny),
   m_DllResolver(0)
{}

CPluginManager_DllResolver::CPluginManager_DllResolver(
                    const string& interface_name,
                    const string& driver_name,
                    const CVersionInfo& version)
 : m_DllNamePrefix("ncbi_plugin"),
   m_EntryPointPrefix("NCBI_EntryPoint"),
   m_InterfaceName(interface_name),
   m_DriverName(driver_name),
   m_Version(version),
   m_DllResolver(0)
{
}


CPluginManager_DllResolver::~CPluginManager_DllResolver(void)
{
    delete m_DllResolver;
}

CDllResolver& CPluginManager_DllResolver::Resolve(const vector<string>& paths)
{
    CDllResolver* resolver = GetCreateDllResolver();
    _ASSERT(resolver);

    // Generate DLL masks

    string mask = GetDllNameMask(m_InterfaceName, m_DriverName, m_Version);
    vector<string> masks;
    masks.push_back(mask);

    resolver->FindCandidates(paths, masks);
    return *resolver;
}

CDllResolver& CPluginManager_DllResolver::Resolve(const string& path)
{
    _ASSERT(!path.empty());
    vector<string> paths;
    paths.push_back(path);
    return Resolve(paths);
}


string 
CPluginManager_DllResolver::GetDllName(const string&       interface_name,
                                       const string&       driver_name,
                                       const CVersionInfo& version) const
{
    string prefix = GetDllNamePrefix();
    string name = prefix;
    if (!interface_name.empty()) {
        name.append("_");
        name.append(interface_name);
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
        name.append(NCBI_PLUGIN_SUFFIX);
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
        const string&       interface_name,
        const string&       driver_name,
        const CVersionInfo& version) const
{
    string prefix = GetDllNamePrefix();
    string name = prefix;

    name.append("_");
    if (interface_name.empty()) {
        name.append("*");
    } else {
        name.append(interface_name);
    }

    name.append("_");

    if (driver_name.empty()) {
        name.append("*");
    } else {
        name.append(driver_name);
    } 

    if (version.IsAny()) {
#if defined(NCBI_OS_MSWIN)
        name.append("*" NCBI_PLUGIN_SUFFIX);
#elif defined(NCBI_OS_UNIX)
        name.append(NCBI_PLUGIN_SUFFIX "*");
#endif
    } else {
        
#if defined(NCBI_OS_MSWIN)
        string delimiter = "_";

#elif defined(NCBI_OS_UNIX)
        string delimiter = ".";
        name.append(NCBI_PLUGIN_SUFFIX);
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

CDllResolver* CPluginManager_DllResolver::CreateDllResolver() const
{
    vector<string> entry_point_names;
    string entry_name;
    

    // Generate all variants of entry point names
    // some of them can duplicate, and that's legal. Resolver stops trying
    // after the first success.

    entry_name = GetEntryPointName(m_InterfaceName, m_DriverName);
    entry_point_names.push_back(entry_name);
    
    entry_name = GetEntryPointName(kEmptyStr, kEmptyStr);
    entry_point_names.push_back(entry_name);
    
    entry_name = GetEntryPointName(m_InterfaceName, kEmptyStr);
    entry_point_names.push_back(entry_name);
    
    entry_name = GetEntryPointName(kEmptyStr, m_DriverName);
    entry_point_names.push_back(entry_name);
    
    // Make the library dependent entry point templates
    string base_name_templ = "${basename}";
    string prefix = GetEntryPointPrefix();
    
    // Make "NCBI_EntryPoint_libname" EP name
    entry_name = prefix;
    entry_name.append("_");
    entry_name.append(base_name_templ);
    entry_point_names.push_back(entry_name);
        
    // Make "NCBI_EntryPoint_interface_libname" EP name
    if (!m_InterfaceName.empty()) {
        entry_name = prefix;
        entry_name.append("_");
        entry_name.append(m_InterfaceName);
        entry_name.append("_");        
        entry_name.append(base_name_templ);
        entry_point_names.push_back(entry_name);
    }
    
    // Make "NCBI_EntryPoint_driver_libname" EP name
    if (!m_DriverName.empty()) {
        entry_name = prefix;
        entry_name.append("_");
        entry_name.append(m_DriverName);
        entry_name.append("_");        
        entry_name.append(base_name_templ);
        entry_point_names.push_back(entry_name);
    }

    CDllResolver* resolver = new CDllResolver(entry_point_names);

    return resolver;
 }

CDllResolver* CPluginManager_DllResolver::GetCreateDllResolver()
{
    if (m_DllResolver == 0) {
        m_DllResolver = CreateDllResolver();
    }
    return m_DllResolver;
}


END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.7  2004/06/23 17:13:56  ucko
 * Centralize plugin naming in ncbidll.hpp.
 *
 * Revision 1.6  2004/05/14 13:59:27  gorelenk
 * Added include of ncbi_pch.hpp
 *
 * Revision 1.5  2003/12/09 13:25:59  kuznets
 * Added DLL name based entry point names
 *
 * Revision 1.4  2003/11/18 15:26:48  kuznets
 * Minor cosmetic changes
 *
 * Revision 1.3  2003/11/17 17:04:22  kuznets
 * Cosmetic fixes
 *
 * Revision 1.2  2003/11/12 18:57:21  kuznets
 * Implemented dll resolution.
 *
 *
 * ===========================================================================
 */

