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
 * Authors:  Dmitry Kazimirov
 *
 * File Description:  Simple test for the NetStorage API
 *      (implemented via RPC to NetStorage servers).
 *
 */

#include <ncbi_pch.hpp>

#include <connect/services/netstorage.hpp>

#include <corelib/test_boost.hpp>

#include <common/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;

#define APP_NAME "test_netstorage_rpc"
#define NETSTORAGE_SERVICE_NAME "ST_Test"
#define NETCACHE_SERVICE_NAME "NC_UnitTest"
#define CACHE_NAME "nst_test"

void g_TestNetStorage(CNetStorage netstorage);

BOOST_AUTO_TEST_CASE(TestNetStorage)
{
    CNetStorage netstorage("nst=" NETSTORAGE_SERVICE_NAME
            "&nc=" NETCACHE_SERVICE_NAME
            "&cache=" CACHE_NAME
            "&client=" APP_NAME);

    g_TestNetStorage(netstorage);
}

void g_TestNetStorageByKey(CNetStorageByKey netstorage);

BOOST_AUTO_TEST_CASE(TestNetStorageByKey)
{
    CNetStorageByKey netstorage("nst=" NETSTORAGE_SERVICE_NAME
            "&nc=" NETCACHE_SERVICE_NAME
            "&cache=" CACHE_NAME
            "&client=" APP_NAME
            "&domain=" CACHE_NAME);

    g_TestNetStorageByKey(netstorage);
}
