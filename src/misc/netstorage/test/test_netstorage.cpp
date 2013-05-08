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
 * File Description:  Simple test for NetStorage.
 *
 */

#include <ncbi_pch.hpp>

#include <misc/netstorage/netstorage.hpp>

#include <corelib/test_boost.hpp>

#include <common/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;

static const char* s_AppName = "test_netstorage";

static const char* s_NetCacheService = "NC_UnitTest";

static void s_ReadAndCompare(CNetFile file, const CTempString& contents)
{
    char buffer[10];
    const char* expected = contents.data();
    size_t remaining_size = contents.length();

    while (!file.Eof()) {
        size_t bytes_read = file.Read(buffer, sizeof(buffer));

        BOOST_CHECK_MESSAGE(bytes_read <= remaining_size,
                "Got more data than expected");
        BOOST_CHECK_MESSAGE(memcmp(buffer, expected, bytes_read) == 0,
                "Read() returned corrupted data");

        expected += bytes_read;
        remaining_size -= bytes_read;
    }

    file.Close();

    BOOST_CHECK_MESSAGE(remaining_size == 0, "Got less data than expected");
}

BOOST_AUTO_TEST_CASE(TestNetStorage)
{
    CNetICacheClient icache_client(s_NetCacheService, s_AppName, s_AppName);

    const string test_data("The quick brown fox jumps over the lazy dog.");

    CNetStorage netstorage(g_CreateNetStorage(icache_client));

    string file_id;

    {{
        // Create a file that should to go to NetCache.
        CNetFile new_file = netstorage.Create(fNST_Fast | fNST_Movable);

        file_id = new_file.GetID();

        new_file.Write(test_data);

        // Now close the file and then read it entirely
        // into a string.
        new_file.Close();

        string data;
        data.reserve(10);

        new_file.Read(&data);

        // Make sure the data is not corrupted.
        BOOST_CHECK_MESSAGE(data == test_data, "Read(string*) is broken");
    }}

    // Now read the whole file using the buffered version
    // of Read(). Verify that the contents of each buffer
    // match the original data.
    CNetFile orig_file = netstorage.Open(file_id);

    s_ReadAndCompare(orig_file, test_data);

    // Generate a "non-movable" file ID by calling Relocate()
    // with the same storage preferences (so the file should not
    // be actually relocated).
    string fast_storage_file_id = netstorage.Relocate(file_id, fNST_Fast);

    // Relocate the file to a persistent storage.
    string persistent_id = netstorage.Relocate(file_id, fNST_Persistent);

    // Verify that the file has disappeared from the "fast" storage.
    CNetFile fast_storage_file = netstorage.Open(fast_storage_file_id);

    // Make sure the relocated file does not exists in the
    // original storage anymore.
    BOOST_CHECK_THROW(s_ReadAndCompare(orig_file, test_data),
            CNetStorageException);
    BOOST_CHECK_THROW(s_ReadAndCompare(fast_storage_file, test_data),
            CNetStorageException);

    // However, the file must still be accessible
    // either using the original ID:
    s_ReadAndCompare(netstorage.Open(file_id), test_data);
    // or using the newly generated persistent storage ID:
    s_ReadAndCompare(netstorage.Open(persistent_id), test_data);
}

NCBITEST_AUTO_INIT()
{
    boost::unit_test::framework::master_test_suite().p_name->assign(
            "CNetStorage Unit Test");
}
