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
 */

#include <ncbiconf.h>

/* #undef NCBI_PRODUCTION_VER */
#define NCBI_DEVELOPMENT_VER 20180404

#ifdef HAVE_COMMON_NCBI_BUILD_VER_H
#  include <common/ncbi_build_ver.h>
#endif


#if defined(NCBI_TEAMCITY_BUILD_NUMBER)
#  define NCBI_TEAMCITY_BUILD_NUMBER_PROXY NCBI_TEAMCITY_BUILD_NUMBER
#else
#  define NCBI_TEAMCITY_BUILD_NUMBER_PROXY 0
#endif

#if defined (NCBI_TEAMCITY_PROJECT_NAME)
#  define NCBI_TEAMCITY_PROJECT_NAME_PROXY NCBI_TEAMCITY_PROJECT_NAME
#else
#  define NCBI_TEAMCITY_PROJECT_NAME_PROXY ""
#endif

#if defined (NCBI_TEAMCITY_BUILDCONF_NAME)
#  define NCBI_TEAMCITY_BUILDCONF_NAME_PROXY NCBI_TEAMCITY_BUILDCONF_NAME
#else
#  define NCBI_TEAMCITY_BUILDCONF_NAME_PROXY ""
#endif

#if defined(NCBI_SUBVERSION_REVISION)
#  define NCBI_SUBVERSION_REVISION_PROXY NCBI_SUBVERSION_REVISION
#else
#  define NCBI_SUBVERSION_REVISION_PROXY 0
#endif

#if defined(NCBI_SC_VERSION)
#  define NCBI_SC_VERSION_PROXY NCBI_SC_VERSION
#else
#  define NCBI_SC_VERSION_PROXY 0
#endif

#if defined(NCBI_PRODUCTION_VER)
#  define NCBI_SRCTREE_VER_PROXY  NCBI_PRODUCTION_VER
#  define NCBI_SRCTREE_NAME_PROXY SBuildInfo::eProductionVersion
#elif defined(NCBI_DEVELOPMENT_VER)
#  define NCBI_SRCTREE_VER_PROXY  NCBI_DEVELOPMENT_VER
#  define NCBI_SRCTREE_NAME_PROXY SBuildInfo::eDevelopmentVersion
#else
#  define NCBI_SRCTREE_VER_PROXY  0
#  define NCBI_SRCTREE_NAME_PROXY SBuildInfo::eDevelopmentVersion
#endif
