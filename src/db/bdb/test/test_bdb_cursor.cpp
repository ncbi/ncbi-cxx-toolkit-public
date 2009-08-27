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
* Author:  Victor Joukov
*
* File Description:
*   Test for BDB wrapper's cursor.
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <corelib/ncbi_system.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbifile.hpp>

#include <db/bdb/bdb_env.hpp>
#include <db/bdb/bdb_blob.hpp>
#include <db/bdb/bdb_cursor.hpp>


// This header must be included before all Boost.Test headers if there are any
#include <corelib/test_boost.hpp>


USING_NCBI_SCOPE;


const int k_DataSizeStep = 256;
const int k_MaxDataSize = 2048;

const char k_DatabaseName[] = "test_cursor.db";

struct STestDB : public CBDB_BLobFile {
    CBDB_FieldInt4   key;

    STestDB()
    {
        BindKey("key",   &key);
    }
};


void s_Test(bool forward)
{

    // Create database
    STestDB test_db;
    test_db.Open(k_DatabaseName, CBDB_File::eCreate);

    char data[k_MaxDataSize];
    for (int i = 0; i < k_MaxDataSize; ++i) data[i] = ' ';

    // Fill with ascending/descending size keys
    for (int n = 0; n <= k_MaxDataSize; n += k_DataSizeStep) {
        int data_size = forward ? n : k_MaxDataSize - n;
        test_db.key = n;
        EBDB_ErrCode err = test_db.Insert(data, data_size);
        BOOST_CHECK_EQUAL(err, eBDB_Ok);
        // cout << data_size << endl;
    }

    // Create cursor with very small buffer
    CBDB_FileCursor cur(test_db);
    if (forward) {
        cur.SetCondition(CBDB_FileCursor::eFirst);
        cur.SetFetchDirection(CBDB_FileCursor::eForward);
    } else {
        cur.SetCondition(CBDB_FileCursor::eLast);
        cur.SetFetchDirection(CBDB_FileCursor::eBackward);
    }
    CBDB_RawFile::TBuffer buf(k_DataSizeStep/2);

    // Read it with cursor, checking that the data is read only once
    int n = 0;
    while (cur.Fetch(&buf) == eBDB_Ok) {
        int key;

        key = test_db.key;
        cout << key << ' ' << buf.size() << endl;
        BOOST_CHECK_EQUAL(n, buf.size());
        n += k_DataSizeStep;
    }
    BOOST_CHECK_EQUAL(n, k_MaxDataSize+k_DataSizeStep);

    // Test cleanup
    cur.Close();
    test_db.Close();
    CDirEntry db_file(k_DatabaseName);
    db_file.Remove();
    cout << endl;
}

BOOST_AUTO_TEST_CASE(TestAscending)
{
    s_Test(true);
}

BOOST_AUTO_TEST_CASE(TestDescending)
{
    s_Test(false);
}
