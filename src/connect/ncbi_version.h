#ifndef CONNECT___NCBI_VERSION__H
#define CONNECT___NCBI_VERSION__H

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
 * Author:  Anton Lavrentiev
 *
 * File Description:
 *   Daemon collection version number
 *
 */

#include <connect/connect_export.h>
#include "ncbi_config.h"

#ifdef   NCBI_CXX_TOOLKIT
#  include <common/ncbi_package_ver.h>
#endif /*NCBI_CXX_TOOLKIT*/


#ifdef NCBI_PACKAGE

#  define   NETDAEMONS_MAJOR        NCBI_PACKAGE_VERSION_MAJOR
#  define   NETDAEMONS_MINOR        NCBI_PACKAGE_VERSION_MINOR
#  define   NETDAEMONS_PATCH        NCBI_PACKAGE_VERSION_PATCH

#  define   NETDAEMONS_VERSION_STR  NCBI_PACKAGE_VERSION

#else

#  define   NETDAEMONS_MAJOR        2
#  define   NETDAEMONS_MINOR        0
#  define   NETDAEMONS_PATCH        4

#  ifdef NCBI_CXX_TOOLKIT
#    define NETDAEMONS_VERSION_STR  NCBI_PACKAGE_VERSION_COMPOSE_STR    \
                                   (NETDAEMONS_MAJOR,                   \
                                    NETDAEMONS_MINOR,                   \
                                    NETDAEMONS_PATCH)
#  else
#    define _NETDAEMONS_STRINGIFY(x)           #x
#    define _NETDAEMONS_CATENATE_STR(a, b, c)  _NETDAEMONS_STRINGIFY(a) "." \
                                               _NETDAEMONS_STRINGIFY(b) "." \
                                               _NETDAEMONS_STRINGIFY(c)
#    define NETDAEMONS_VERSION_STR _NETDAEMONS_CATENATE_STR             \
                                   (NETDAEMONS_MAJOR,                   \
                                    NETDAEMONS_MINOR,                   \
                                    NETDAEMONS_PATCH)
#  endif /*NCBI_CXX_TOOLKIT*/

#endif /*NCBI_PACKAGE*/


#define NETDAEMONS_VERSION_OF(ma, mi, pa)  ((unsigned int)              \
                                            ((ma)*100000 + (mi)*1000 + (pa)))

#define NETDAEMONS_MAJOR_OF(ver)           ( (ver) / 100000)
#define NETDAEMONS_MINOR_OF(ver)           (((ver) / 1000) % 100)
#define NETDAEMONS_PATCH_OF(ver)           ( (ver) % 1000)

#define NETDAEMONS_VERSION_INT             NETDAEMONS_VERSION_OF        \
                                           (NETDAEMONS_MAJOR,           \
                                            NETDAEMONS_MINOR,           \
                                            NETDAEMONS_PATCH)


#ifdef NCBI_CXX_TOOLKIT

#  if !defined(NDEBUG)  ||  defined(_DEBUG)
#    if NCBI_PLATFORM_BITS == 64
#      define NETDAEMONS_VERSION    NETDAEMONS_VERSION_STR "/64[DEBUG]"
#    else
#      define NETDAEMONS_VERSION    NETDAEMONS_VERSION_STR "[DEBUG]"
#    endif /*NCBI_PLATFORM_BITS==64*/
#  else
#    if NCBI_PLATFORM_BITS == 64
#      define NETDAEMONS_VERSION    NETDAEMONS_VERSION_STR "/64"
#    else
#      define NETDAEMONS_VERSION    NETDAEMONS_VERSION_STR
#    endif /*NCBI_PLATFORM_BITS==64*/
#  endif /*!NDEBUG || _DEBUG*/

#else

#  define NETDAEMONS_VERSION        NETDAEMONS_VERSION_STR

#endif /*NCBI_CXX_TOOLKIT*/


#ifdef __cplusplus
extern "C" {
#endif


extern NCBI_XCONNECT_EXPORT
const char* g_VersionStr(const char* revision);


#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /*CONNECT___NCBI_VERSION__H*/
