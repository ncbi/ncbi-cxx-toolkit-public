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


#include <corelib/version_api.hpp>
#include <common/ncbi_build_info.h>



BEGIN_NCBI_SCOPE

/** @addtogroup Version
 *
 * @{
 */

// Define macros to set application version
#ifndef NCBI_BUILD_TIME
# define NCBI_BUILD_TIME __DATE__ " " __TIME__
#endif

#ifdef NCBI_BUILD_TAG
#  define NCBI_BUILD_TAG_PROXY  NCBI_AS_STRING(NCBI_BUILD_TAG)
#else
#  define NCBI_BUILD_TAG_PROXY  ""
#endif

// Cope with potentially having an older ncbi_build_info.h
#ifndef NCBI_TEAMCITY_PROJECT_NAME_SBUILDINFO
#  define NCBI_TEAMCITY_PROJECT_NAME_SBUILDINFO \
    .Extra(SBuildInfo::eTeamCityProjectName, NCBI_TEAMCITY_PROJECT_NAME_PROXY)
#  define NCBI_TEAMCITY_BUILDCONF_NAME_SBUILDINFO \
    .Extra(SBuildInfo::eTeamCityBuildConf, NCBI_TEAMCITY_BUILDCONF_NAME_PROXY)
#  define NCBI_TEAMCITY_BUILD_NUMBER_SBUILDINFO \
    .Extra(SBuildInfo::eTeamCityBuildNumber, NCBI_TEAMCITY_BUILD_NUMBER_PROXY)
#  define NCBI_SUBVERSION_REVISION_SBUILDINFO \
    .Extra(SBuildInfo::eSubversionRevision, NCBI_SUBVERSION_REVISION_PROXY)
#  define NCBI_SC_VERSION_SBUILDINFO \
    .Extra(SBuildInfo::eStableComponentsVersion, NCBI_SC_VERSION_PROXY)
#endif

// Cope with potentially having an older ncbi_source_ver.h
#ifndef NCBI_SRCTREE_VER_SBUILDINFO
#  ifdef NCBI_SRCTREE_NAME_PROXY
#    define NCBI_SRCTREE_VER_SBUILDINFO \
    .Extra(NCBI_SRCTREE_NAME_PROXY, NCBI_SRCTREE_VER_PROXY)
#  else
#    define NCBI_SRCTREE_VER_SBUILDINFO /* empty */
#  endif
#endif

#ifdef NCBI_APP_BUILT_AS
#  define NCBI_BUILT_AS_SBUILDINFO \
    .Extra(SBuildInfo::eBuiltAs, NCBI_AS_STRING(NCBI_APP_BUILT_AS))
#else
#  define NCBI_BUILT_AS_SBUILDINFO /* empty */
#endif

#ifdef NCBI_TEAMCITY_BUILD_ID
#  define NCBI_BUILD_ID NCBI_TEAMCITY_BUILD_ID
#elif defined(NCBI_BUILD_SESSION_ID)
#  define NCBI_BUILD_ID NCBI_AS_STRING(NCBI_BUILD_SESSION_ID)
#endif
#ifdef NCBI_BUILD_ID
#  define NCBI_BUILD_ID_SBUILDINFO .Extra(SBuildInfo::eBuildID, NCBI_BUILD_ID)
#else
#  define NCBI_BUILD_ID_SBUILDINFO /* empty */
#endif

#define NCBI_SBUILDINFO_DEFAULT_IMPL() \
    NCBI_SBUILDINFO_DEFAULT_INSTANCE() \
        NCBI_TEAMCITY_PROJECT_NAME_SBUILDINFO \
        NCBI_TEAMCITY_BUILDCONF_NAME_SBUILDINFO \
        NCBI_TEAMCITY_BUILD_NUMBER_SBUILDINFO \
        NCBI_BUILD_ID_SBUILDINFO \
        NCBI_SUBVERSION_REVISION_SBUILDINFO \
        NCBI_SC_VERSION_SBUILDINFO \
        NCBI_SRCTREE_VER_SBUILDINFO \
        NCBI_BUILT_AS_SBUILDINFO

#if defined(NCBI_USE_PCH) && !defined(NCBI_TEAMCITY_BUILD_NUMBER)
#define NCBI_SBUILDINFO_DEFAULT() SBuildInfo()
#else
#define NCBI_SBUILDINFO_DEFAULT() NCBI_SBUILDINFO_DEFAULT_IMPL()
#endif
#define NCBI_APP_SBUILDINFO_DEFAULT() NCBI_SBUILDINFO_DEFAULT_IMPL()

#ifdef NCBI_SBUILDINFO_DEFAULT_INSTANCE
# undef NCBI_SBUILDINFO_DEFAULT_INSTANCE
#endif
#define NCBI_SBUILDINFO_DEFAULT_INSTANCE() SBuildInfo(NCBI_BUILD_TIME, NCBI_BUILD_TAG_PROXY)

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
