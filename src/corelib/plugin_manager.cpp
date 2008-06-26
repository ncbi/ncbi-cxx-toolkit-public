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
                    const CVersionInfo& version,
                    CDll::EAutoUnload unload_dll)
 : m_DllNamePrefix("ncbi_plugin"),
   m_EntryPointPrefix("NCBI_EntryPoint"),
   m_InterfaceName(interface_name),
   m_DriverName(driver_name),
   m_Version(version),
   m_DllResolver(0),
   m_AutoUnloadDll(unload_dll)
{
}


CPluginManager_DllResolver::~CPluginManager_DllResolver(void)
{
    delete m_DllResolver;
}

CDllResolver& 
CPluginManager_DllResolver::ResolveFile(const TSearchPaths&   paths,
                                        const string&         driver_name,
                                        const CVersionInfo&   version,
                                        CDllResolver::TExtraDllPath
                                                              std_path)
{
    CDllResolver* resolver = GetCreateDllResolver();
    _ASSERT(resolver);

    const string& drv = driver_name.empty() ? m_DriverName : driver_name;
    const CVersionInfo& ver = version.IsAny() ? m_Version : version;

    // Generate DLL masks

    // Ignore version to find dlls having no version in their names
    vector<string> masks;
    string mask = GetDllNameMask(m_InterfaceName, drv, ver);
    masks.push_back(mask);

    if ( version == CVersionInfo::kAny ) {
        mask = GetDllNameMask(m_InterfaceName, drv, CVersionInfo::kLatest);
        masks.push_back(mask);
        
#if defined(NCBI_OS_UNIX)
        mask = GetDllNameMask(m_InterfaceName, drv, CVersionInfo::kLatest, 
                              eAfterSuffix);
        masks.push_back(mask);
#endif        
    }
    
    resolver->FindCandidates(paths, masks, std_path, drv);
    
    return *resolver;
}

CDllResolver& CPluginManager_DllResolver::Resolve(const string& path)
{
    _ASSERT(!path.empty());
    vector<string> paths;
    paths.push_back(path);
    return ResolveFile(paths);
}


string 
CPluginManager_DllResolver::GetDllName(const string&       interface_name,
                                       const string&       driver_name,
                                       const CVersionInfo& version) const
{
    string name = GetDllNamePrefix();

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
        const CVersionInfo& version,
        EVersionLocation    ver_lct) const
{
    string name = GetDllNamePrefix();

    if ( !name.empty() ) {
        name.append("_");
    }
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
        name.append(NCBI_PLUGIN_SUFFIX);
    } else {
        
        string delimiter;
        
#if defined(NCBI_OS_MSWIN)
        delimiter = "_";

#elif defined(NCBI_OS_UNIX)
        if ( ver_lct != eAfterSuffix ) {
            delimiter = "_";
        } else {
            delimiter = ".";
        }
#endif

        if ( ver_lct == eAfterSuffix ) {
            name.append(NCBI_PLUGIN_SUFFIX);
        }
        
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
        
        if ( ver_lct != eAfterSuffix ) {
            name.append(NCBI_PLUGIN_SUFFIX);
        }
    }

    return name;
}

string 
CPluginManager_DllResolver::GetEntryPointName(
                      const string&       interface_name,
                      const string&       driver_name) const
{
    string name = GetEntryPointPrefix();

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
    return NCBI_PLUGIN_PREFIX + m_DllNamePrefix; 
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

    entry_name = GetEntryPointName(m_InterfaceName, "${driver}");
    entry_point_names.push_back(entry_name);
    
    entry_name = GetEntryPointName(kEmptyStr, kEmptyStr);
    entry_point_names.push_back(entry_name);
    
    entry_name = GetEntryPointName(m_InterfaceName, kEmptyStr);
    entry_point_names.push_back(entry_name);
    
    entry_name = GetEntryPointName(kEmptyStr, "${driver}");
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

    CDllResolver* resolver = new CDllResolver(entry_point_names, 
                                              m_AutoUnloadDll);

    return resolver;
 }

CDllResolver* CPluginManager_DllResolver::GetCreateDllResolver()
{
    if (m_DllResolver == 0) {
        m_DllResolver = CreateDllResolver();
    }
    return m_DllResolver;
}


#if defined(NCBI_PLUGIN_AUTO_LOAD)  ||  defined(NCBI_DLL_BUILD)
#  define LOAD_PLUGINS_FROM_DLLS_BY_DEFAULT true
#else
#  define LOAD_PLUGINS_FROM_DLLS_BY_DEFAULT false
#endif
NCBI_PARAM_DECL(bool, NCBI, Load_Plugins_From_DLLs);
NCBI_PARAM_DEF_EX(bool, NCBI, Load_Plugins_From_DLLs,
                  LOAD_PLUGINS_FROM_DLLS_BY_DEFAULT,
                  eParam_NoThread, NCBI_LOAD_PLUGINS_FROM_DLLS);
typedef NCBI_PARAM_TYPE(NCBI, Load_Plugins_From_DLLs) TLoadPluginsFromDLLsParam;

bool CPluginManager_DllResolver::IsEnabledGlobally()
{
    return TLoadPluginsFromDLLsParam::GetDefault();
}

bool CPluginManager_DllResolver::IsEnabledGloballyByDefault()
{
    return LOAD_PLUGINS_FROM_DLLS_BY_DEFAULT;
}

void CPluginManager_DllResolver::EnableGlobally(bool enable)
{
    TLoadPluginsFromDLLsParam::SetDefault(enable);
}

const char* CPluginManagerException::GetErrCodeString(void) const
{
    switch (GetErrCode()) {
    case eResolveFailure:   return "eResolveFailure";
    case eParameterMissing: return "eParameterMissing";
    default:    return CException::GetErrCodeString();
    }
}


END_NCBI_SCOPE
