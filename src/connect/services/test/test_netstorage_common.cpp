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
 * File Description:  Simple test of NetStorage API (Common parts
 *                    shared between the RPC-based and the direct
 *                    access implementations).
 *
 */

#include <ncbi_pch.hpp>

#include <connect/services/netstorage.hpp>

#include <corelib/test_boost.hpp>

#include <util/random_gen.hpp>

#include <common/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;

static const string s_TestData("The quick brown fox jumps over the lazy dog.");

static void s_ReadAndCompare(CNetStorageObject file)
{
    char buffer[10];
    const char* expected = s_TestData.data();
    size_t remaining_size = s_TestData.length();

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

static string s_WriteAndRead(CNetStorageObject new_file)
{
    new_file.Write(s_TestData);

    // Now close the file and then read it entirely
    // into a string.
    new_file.Close();

    string data;
    data.reserve(10);

    new_file.Read(&data);

    // Make sure the data is not corrupted.
    BOOST_CHECK_MESSAGE(data == s_TestData, "Read(string*) is broken");

    return new_file.GetID();
}

void g_TestNetStorage(CNetStorage netstorage)
{
    // Create a file that should to go to NetCache.
    string object_id = s_WriteAndRead(
            netstorage.Create(fNST_Fast | fNST_Movable));

    // Now read the whole file using the buffered version
    // of Read(). Verify that the contents of each buffer
    // match the original data.
    CNetStorageObject orig_file = netstorage.Open(object_id);

    s_ReadAndCompare(orig_file);

    // Generate a "non-movable" file ID by calling Relocate()
    // with the same storage preferences (so the file should not
    // be actually relocated).
    string fast_storage_object_id = netstorage.Relocate(object_id, fNST_Fast);

    // Relocate the file to a persistent storage.
    string persistent_id = netstorage.Relocate(object_id, fNST_Persistent);

    // Verify that the file has disappeared from the "fast" storage.
    CNetStorageObject fast_storage_file =
            netstorage.Open(fast_storage_object_id);

    // Make sure the relocated file does not exists in the
    // original storage anymore.
    BOOST_CHECK_THROW(s_ReadAndCompare(fast_storage_file),
            CNetStorageException);

    // However, the file must still be accessible
    // either using the original ID:
    s_ReadAndCompare(netstorage.Open(object_id));
    // or using the newly generated persistent storage ID:
    s_ReadAndCompare(netstorage.Open(persistent_id));
}

void g_TestNetStorageByKey(CNetStorageByKey netstorage)
{
    CRandom random_gen;

    random_gen.Randomize();

    string unique_key = NStr::NumericToString(
            random_gen.GetRand() * random_gen.GetRand());

    // Write and read test data using a user-defined key.
    s_WriteAndRead(netstorage.Open(unique_key, fNST_Fast | fNST_Movable));

    // Make sure the file is readable with a different combination of flags.
    s_ReadAndCompare(netstorage.Open(unique_key,
            fNST_Persistent | fNST_Movable));

    // Relocate the file to FileTrack and make sure
    // it can be read from there.
    netstorage.Relocate(unique_key, fNST_Persistent, fNST_Movable);

    s_ReadAndCompare(netstorage.Open(unique_key, fNST_Persistent));
}

NCBITEST_AUTO_INIT()
{
    boost::unit_test::framework::master_test_suite().p_name->assign(
            "CNetStorage Unit Test");
}
