#ifndef CORELIB___VERSION__HPP
#define CORELIB___VERSION__HPP

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
 * Authors:  Denis Vakatov, Vladimir Ivanov, Anatoliy Kuznetsov
 *
 *
 */

/// @file version.hpp
/// Define CVersionInfo, a version info storage class.


#include <corelib/ncbiobj.hpp>
#include <corelib/version_api.hpp>



BEGIN_NCBI_SCOPE

/** @addtogroup Version
 *
 * @{
 */

// Define macros to set application version
#ifndef NCBI_BUILD_TIME
# define NCBI_BUILD_TIME __DATE__ " " __TIME__
#endif

#ifdef CVersion
# undef CVersion
#endif
#ifdef CComponentVersionInfo
# undef CComponentVersionInfo
#endif

class CComponentVersionInfo : public CComponentVersionInfoAPI
{
public:
    /// Constructor
    CComponentVersionInfo(const string& component_name,
                          int  ver_major,
                          int  ver_minor,
                          int  patch_level = 0,
                          const string& ver_name = kEmptyStr,
                          const SBuildInfo& build_info = NCBI_SBUILDINFO_DEFAULT())
        : CComponentVersionInfoAPI(component_name, ver_major, ver_minor, patch_level, ver_name, build_info)
    {
    }

    /// Constructor
    ///
    /// @param component_name
    ///    component name
    /// @param version
    ///    version string (eg, 1.2.4)
    /// @param ver_name
    ///    version name
    CComponentVersionInfo(const string& component_name,
                          const string& version,
                          const string& ver_name = kEmptyStr,
                          const SBuildInfo& build_info = NCBI_SBUILDINFO_DEFAULT())
        : CComponentVersionInfoAPI(component_name, version, ver_name, build_info)
    {
    }
};


class CVersion : public CVersionAPI
{
public:

    explicit
    CVersion(const SBuildInfo& build_info = NCBI_SBUILDINFO_DEFAULT())
        : CVersionAPI(build_info)
    {
    }

    explicit
    CVersion(const CVersionInfo& version,
             const SBuildInfo& build_info = NCBI_SBUILDINFO_DEFAULT())
        : CVersionAPI(version, build_info)
    {
    }

    CVersion(const CVersion& version)
        : CVersionAPI(version)
    {
    }

    using CVersionAPI::AddComponentVersion;
    
    /// Add component version information
    void AddComponentVersion(const string& component_name,
                             int           ver_major,
                             int           ver_minor,
                             int           patch_level = 0,
                             const string& ver_name = kEmptyStr,
                             const SBuildInfo& build_info = NCBI_SBUILDINFO_DEFAULT())
    {
        CVersionAPI::AddComponentVersion(component_name, ver_major, ver_minor, patch_level, ver_name, build_info);
    }
};


/* @} */


END_NCBI_SCOPE

#endif // CORELIB___VERSION__HPP
