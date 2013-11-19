#ifndef NETCACHE_VERSION__HPP
#define NETCACHE_VERSION__HPP

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

#include <corelib/ncbistl.hpp>
#include <common/ncbi_package_ver.h>

#define NETCACHED_VERSION NCBI_PACKAGE_VERSION
#define NETCACHED_STORAGE_VERSION_MAJOR 6
#define NETCACHED_STORAGE_VERSION_MINOR 3
#define NETCACHED_STORAGE_VERSION_PATCH 0
#define NETCACHED_PROTOCOL_VERSION_MAJOR 6
#define NETCACHED_PROTOCOL_VERSION_MINOR 3
#define NETCACHED_PROTOCOL_VERSION_PATCH 0
#define NETCACHED_STORAGE_VERSION                           \
    BOOST_STRINGIZE(NETCACHED_STORAGE_VERSION_MAJOR) "."    \
    BOOST_STRINGIZE(NETCACHED_STORAGE_VERSION_MINOR) "."    \
    BOOST_STRINGIZE(NETCACHED_STORAGE_VERSION_PATCH)
#define NETCACHED_PROTOCOL_VERSION                          \
    BOOST_STRINGIZE(NETCACHED_PROTOCOL_VERSION_MAJOR) "."   \
    BOOST_STRINGIZE(NETCACHED_PROTOCOL_VERSION_MINOR) "."   \
    BOOST_STRINGIZE(NETCACHED_PROTOCOL_VERSION_PATCH)

#define NETCACHED_BUILD_DATE     __DATE__ " " __TIME__

#define NETCACHED_HUMAN_VERSION \
    "NCBI NetCache server Version " NETCACHED_VERSION \
    " build " NETCACHED_BUILD_DATE

#define NETCACHED_FULL_VERSION \
    "NCBI NetCache Server version " NETCACHED_VERSION \
    " Storage version " NETCACHED_STORAGE_VERSION \
    " Protocol version " NETCACHED_PROTOCOL_VERSION \
    " build " NETCACHED_BUILD_DATE

#endif /* NETCACHE_VERSION__HPP */
