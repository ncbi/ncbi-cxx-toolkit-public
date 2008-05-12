#ifndef CONNECT_DAEMONS___VERSION__H
#define CONNECT_DAEMONS___VERSION__H

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
 *   UNIX only !!!
 *
 */

#include <common/ncbi_package_ver.h>

#ifdef NCBI_PACKAGE

#  define NETDAEMONS_MAJOR        NCBI_PACKAGE_VERSION_MAJOR
#  define NETDAEMONS_MINOR        NCBI_PACKAGE_VERSION_MINOR
#  define NETDAEMONS_PATCH        NCBI_PACKAGE_VERSION_PATCH

#  define NETDAEMONS_VERSION_STR  NCBI_PACKAGE_VERSION

#else

#  define NETDAEMONS_MAJOR        1
#  define NETDAEMONS_MINOR        99
#  define NETDAEMONS_PATCH        999

#  define NETDAEMONS_VERSION_STR  NCBI_PACKAGE_VERSION_COMPOSE_STR  \
                                  (NETDAEMONS_MAJOR,                \
                                   NETDAEMONS_MINOR,                \
                                   NETDAEMONS_PATCH)
#endif

#define NETDAEMONS_VERSION_OF(ma, mi, pa)  ((unsigned int)               \
                                            ((ma)*100000 + (mi)*1000 + (pa)))

#define NETDAEMONS_MAJOR_OF(ver)           ( (ver) / 100000)
#define NETDAEMONS_MINOR_OF(ver)           (((ver) / 1000) % 100)
#define NETDAEMONS_PATCH_OF(ver)           ( (ver) % 1000)

#define NETDAEMONS_VERSION_INT  NETDAEMONS_VERSION_OF(NETDAEMONS_MAJOR, \
                                                      NETDAEMONS_MINOR, \
                                                      NETDAEMONS_PATCH)

#endif /*CONNECT_DAEMONS___VERSION__H*/
