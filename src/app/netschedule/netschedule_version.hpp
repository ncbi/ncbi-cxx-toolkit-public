#ifndef NETSCHEDULE_VERSION__HPP
#define NETSCHEDULE_VERSION__HPP

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
 * Authors:  Victor Joukov
 *
 * File Description: Network scheduler daemon version
 *
 */
#include <common/ncbi_package_ver.h>

#define NETSCHEDULE_STRINGIFY(x)    #x
#define NETSCHEDULE_VERSION_COMPOSE_STR(a, b, c)  \
            NETSCHEDULE_STRINGIFY(a) "." \
            NETSCHEDULE_STRINGIFY(b) "." \
            NETSCHEDULE_STRINGIFY(c)

#define NETSCHEDULED_VERSION        NCBI_PACKAGE_VERSION
#define NETSCHEDULED_BUILD_DATE     __DATE__ " " __TIME__

// Storage
#define NETSCHEDULED_STORAGE_VERSION_MAJOR  4
#define NETSCHEDULED_STORAGE_VERSION_MINOR  4
#define NETSCHEDULED_STORAGE_VERSION_PATCH  6
#define NETSCHEDULED_STORAGE_VERSION        \
            NETSCHEDULE_VERSION_COMPOSE_STR( \
                NETSCHEDULED_STORAGE_VERSION_MAJOR, \
                NETSCHEDULED_STORAGE_VERSION_MINOR, \
                NETSCHEDULED_STORAGE_VERSION_PATCH)


// Protocol
#define NETSCHEDULED_PROTOCOL_VERSION_MAJOR 1
#define NETSCHEDULED_PROTOCOL_VERSION_MINOR 4
#define NETSCHEDULED_PROTOCOL_VERSION_PATCH 4
#define NETSCHEDULED_PROTOCOL_VERSION       \
            NETSCHEDULE_VERSION_COMPOSE_STR( \
                NETSCHEDULED_PROTOCOL_VERSION_MAJOR, \
                NETSCHEDULED_PROTOCOL_VERSION_MINOR, \
                NETSCHEDULED_PROTOCOL_VERSION_PATCH)


#define NETSCHEDULED_FULL_VERSION \
    "NCBI NetSchedule server Version " NETSCHEDULED_VERSION \
    " Storage version " NETSCHEDULED_STORAGE_VERSION \
    " Protocol version " NETSCHEDULED_PROTOCOL_VERSION \
    " build " NETSCHEDULED_BUILD_DATE

#define NETSCHEDULED_FEATURES \
    "fast_status=1;dyn_queues=1;read_confirm=1;version=" NETSCHEDULED_VERSION


#endif /* NETSCHEDULE_VERSION__HPP */

