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

#include <common/ncbi_export.h>
#include <common/ncbi_sc_version.h>

/* Defined in ncbicfg.c(.in). */

#ifdef __cplusplus
extern "C" {
#endif

#if defined(NCBI_OS_MSWIN)  &&  defined(NCBI_DLL_BUILD)
#  define DECLARE_NCBI_BUILD_METADATA(type, name) \
    extern NCBI_XNCBI_EXPORT type g_NCBI_##name(void)
#  define NCBI_BUILD_METADATA(name)               (g_NCBI_##name())
#else
#  define DECLARE_NCBI_BUILD_METADATA(type, name) extern const type kNCBI_##name
#  define NCBI_BUILD_METADATA(name)               kNCBI_##name
#endif

typedef const char* TStringLiteral;

DECLARE_NCBI_BUILD_METADATA(TStringLiteral, GitBranch);
DECLARE_NCBI_BUILD_METADATA(TStringLiteral, Revision);
DECLARE_NCBI_BUILD_METADATA(int,            SubversionRevision);
DECLARE_NCBI_BUILD_METADATA(TStringLiteral, TeamCityBuildConfName);
DECLARE_NCBI_BUILD_METADATA(TStringLiteral, TeamCityBuildID);
DECLARE_NCBI_BUILD_METADATA(int,            TeamCityBuildNumber);
DECLARE_NCBI_BUILD_METADATA(TStringLiteral, TeamCityProjectName);

#define NCBI_GIT_BRANCH              NCBI_BUILD_METADATA(GitBranch)
#define NCBI_REVISION                NCBI_BUILD_METADATA(Revision)
#define NCBI_SUBVERSION_REVISION     NCBI_BUILD_METADATA(SubversionRevision)
#define NCBI_TEAMCITY_BUILDCONF_NAME NCBI_BUILD_METADATA(TeamCityBuildConfName)
#define NCBI_TEAMCITY_BUILD_ID       NCBI_BUILD_METADATA(TeamCityBuildID)
#define NCBI_TEAMCITY_BUILD_NUMBER   NCBI_BUILD_METADATA(TeamCityBuildNumber)
#define NCBI_TEAMCITY_PROJECT_NAME   NCBI_BUILD_METADATA(TeamCityProjectName)

#ifdef NCBI_SIGNATURE
#  define NCBI_LEGACY_SIGNATURE_LOCATION 1
#else
DECLARE_NCBI_BUILD_METADATA(TStringLiteral, Signature);
#  define NCBI_SIGNATURE NCBI_BUILD_METADATA(Signature)
#endif

#ifdef __cplusplus
}  /* extern "C" */
#endif
