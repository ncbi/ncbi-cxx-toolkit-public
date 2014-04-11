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
 * Author: Sergey Sikorskiy
 *
 * File Description: DBAPI unit-test
 *
 * ===========================================================================
 */

#include "sdbapi_unit_test_pch.hpp"


BEGIN_NCBI_SCOPE

///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(Test_ConnParamsDatabase)
{
    try {
        const string target_db_name("DBAPI_Sample");

        // Check method Connect() ...
        {
            CSDB_ConnectionParam params(GetDatabase().GetConnectionParam());
            params.Set(CSDB_ConnectionParam::eDatabase, target_db_name);
            CDatabase db(params);
            BOOST_CHECK( !db.IsConnected(CDatabase::eFullCheck) );
            BOOST_CHECK( !db.IsConnected(CDatabase::eFastCheck) );
            BOOST_CHECK( !db.IsConnected(CDatabase::eNoCheck) );
            db.Connect();
            BOOST_CHECK(db.IsConnected(CDatabase::eNoCheck));
            BOOST_CHECK(db.IsConnected(CDatabase::eFastCheck));
            BOOST_CHECK(db.IsConnected(CDatabase::eFullCheck));

            CQuery query = db.NewQuery("select db_name()");

            query.Execute();
            query.RequireRowCount(1);
            BOOST_CHECK( query.HasMoreResultSets() );
            CQuery::iterator it = query.begin();
            BOOST_CHECK( it != query.end() );
            const  string db_name = it[1].AsString();
            BOOST_CHECK_EQUAL(db_name, target_db_name);
            BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));

            db.Close();
            BOOST_CHECK( !db.IsConnected(CDatabase::eFullCheck) );
            BOOST_CHECK( !db.IsConnected(CDatabase::eFastCheck) );
            BOOST_CHECK( !db.IsConnected(CDatabase::eNoCheck) );
        }
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}

///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(Test_CloneConnection)
{
    try {
        const string target_db_name("DBAPI_Sample");

        // Create new connection ...
        CSDB_ConnectionParam params(GetDatabase().GetConnectionParam());
        params.Set(CSDB_ConnectionParam::eDatabase, target_db_name);
        CDatabase db(params);
        db.Connect();

        // Clone connection ...
        CDatabase new_db = db.Clone();
        CQuery query = new_db.NewQuery("select db_name()");

        // Check that database was set correctly with the new connection ...
        {
            query.Execute();
            query.RequireRowCount(1);
            BOOST_CHECK( query.HasMoreResultSets() );
            CQuery::iterator it = query.begin();
            BOOST_CHECK( it != query.end() );
            const  string db_name = it[1].AsString();
            BOOST_CHECK_EQUAL(db_name, target_db_name);
        }
        BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}

////////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(Test_ConnParams)
{
    {
        CSDB_ConnectionParam params(
                "dbapi://" + GetArgs().GetUserName() +
                ":" + GetArgs().GetUserPassword() +
                "@" + GetArgs().GetServerName()
                );

        CDatabase db(params);
        db.Connect();
        CQuery query = db.NewQuery("SELECT @@version");
        query.Execute();
        query.RequireRowCount(1);
        query.PurgeResults();
    }

    {
        CSDB_ConnectionParam params(
                "dbapi://" + GetArgs().GetUserName() +
                ":" + GetArgs().GetUserPassword() +
                "@" + GetArgs().GetServerName() +
                "/" + GetArgs().GetDatabaseName()
                );

        CDatabase db(params);
        db.Connect();
        CQuery query = db.NewQuery("SELECT @@version");
        query.Execute();
        query.RequireRowCount(1);
        query.PurgeResults();
    }
}

END_NCBI_SCOPE
