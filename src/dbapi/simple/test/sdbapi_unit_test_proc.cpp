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
BOOST_AUTO_TEST_CASE(Test_Procedure)
{
    try {
        // Test a regular statement with "exec"
        {
            CQuery query = GetDatabase().NewQuery();

            // Execute it first time ...
            query.SetSql("exec sp_databases");
            query.Execute();
            ITERATE(CQuery, it, query.SingleSet()) {
            }
            BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));

            // Execute it second time ...
            query.SetSql("exec sp_databases");
            query.Execute();
            ITERATE(CQuery, it, query.SingleSet()) {
            }
            BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));

            // Same as before but do not retrieve data ...
            query.Execute();
            query.PurgeResults();
            query.Execute();
            ERR_POST(Warning
                     << "Skipped retrieving data; expect a library warning.");
        }

        {
            // Execute it first time ...
            CQuery query = GetDatabase().NewQuery();
            query.ExecuteSP("sp_databases");
            ITERATE(CQuery, it, query.SingleSet()) {
            }
            BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));
            BOOST_CHECK_EQUAL(query.GetStatus(), 0);

            // Execute it second time ...
            query.ExecuteSP("sp_databases");
            ITERATE(CQuery, it, query.SingleSet()) {
            }
            BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));
            BOOST_CHECK_EQUAL(query.GetStatus(), 0);

            // Same as before but do not retrieve data ...
            query.ExecuteSP("sp_databases");
            query.PurgeResults();
            query.ExecuteSP("sp_databases");
            ERR_POST(Warning
                     << "Skipped retrieving data; expect a library warning.");
        }

        // Test returned recordset ...
        {
            int num = 0;
            CQuery query = GetDatabase().NewQuery();

            query.ExecuteSP("sp_server_info");
            query.RequireRowCount(s_ServerInfoRows());

            BOOST_CHECK(query.HasMoreResultSets());

            ITERATE(CQuery, it, query.MultiSet()) {
                BOOST_CHECK(it[1].AsInt4() > 0);
                BOOST_CHECK(it[2].AsString().size() > 0);
                BOOST_CHECK(it[3].AsString().size() > 0);
                ++num;
            }

            BOOST_CHECK(num > 0);

            BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));
            BOOST_CHECK_EQUAL(query.GetStatus(), 0);
        }

        // With parameters.
        {
            CQuery query = GetDatabase().NewQuery();
            int rows_expected = s_ServerInfoRows();

            // Set parameter to NULL ...
            query.SetNullParameter( "@attribute_id", eSDB_Int4 );
            query.ExecuteSP("sp_server_info");
            query.RequireRowCount(rows_expected);
            query.PurgeResults();

            BOOST_CHECK_EQUAL( rows_expected, query.GetRowCount() );
            BOOST_CHECK_EQUAL(query.GetStatus(), 0);
            BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));

            // Set parameter to 1 ...
            query.SetParameter( "@attribute_id", Int4(1) );
            query.ExecuteSP("sp_server_info");
            query.RequireRowCount(1);
            query.PurgeResults();

            BOOST_CHECK_EQUAL( 1, query.GetRowCount() );
            BOOST_CHECK_EQUAL(query.GetStatus(), 0);
            BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));
        }
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}


///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(Test_Procedure2)
{
    try {
        {
            CQuery query = GetDatabase().NewQuery();

            query.ExecuteSP("sp_server_info");
            query.RequireRowCount(s_ServerInfoRows());

            ITERATE(CQuery, it, query.SingleSet()) {
                BOOST_CHECK(it[1].AsInt4() != 0);
            }
            BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));
            BOOST_CHECK_EQUAL(query.GetStatus(), 0);
        }

        // Mismatched types of INT parameters ...
        {
            CQuery query = GetDatabase().NewQuery();

            // Set parameter to 2 ...
            // sp_server_info takes an INT parameter ...
            query.SetParameter( "@attribute_id", Int8(2) );
            query.ExecuteSP("sp_server_info");
            query.RequireRowCount(1);
            query.PurgeResults();

            BOOST_CHECK_EQUAL( 1, query.GetRowCount() );
            BOOST_CHECK_EQUAL(query.GetStatus(), 0);
            BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));
        }
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}


///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(Test_Procedure3)
{
    try {
        // Reading multiple result-sets from a stored procedure ...
        {
            CQuery query = GetDatabase().NewQuery();

            query.SetParameter( "@dbname", "master" );
            query.ExecuteSP("sp_helpdb");
            query.RequireRowCount(1);

            int result_num = 0;

            while (query.HasMoreResultSets()) {
                if (++result_num > 1) {
                    query.RequireRowCount(1, kMax_Auto);
                }
                query.begin();
            }

            BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));
            BOOST_CHECK_EQUAL(result_num, 2);
            BOOST_CHECK_EQUAL(query.GetStatus(), 0);
        }

        // The same as above, but using statement ...
        {
            CQuery query = GetDatabase().NewQuery();

            int result_num = 0;

            query.SetSql("exec sp_helpdb 'master'");
            query.Execute();
            query.RequireRowCount(1);
            while (query.HasMoreResultSets()) {
                if (++result_num > 1) {
                    query.RequireRowCount(1, kMax_Auto);
                }
                query.begin();
            }

            BOOST_CHECK_NO_THROW(query.VerifyDone(CQuery::eAllResultSets));
            BOOST_CHECK_EQUAL(result_num, 2);
        }

        // Multiple results plus column names with spaces.
        if (GetArgs().GetServerType() == eSqlSrvMsSql) {
            CQuery query = GetDatabase().NewQuery();

            query.ExecuteSP("sp_spaceused");
            query.RequireRowCount(1);

            BOOST_CHECK(query.HasMoreResultSets());

            string unallocSpace;

            ITERATE(CQuery, it, query) {
                unallocSpace = it["unallocated space"].AsString();
            }

            BOOST_CHECK(query.HasMoreResultSets());
            ERR_POST(Warning
                     << "Skipped retrieving data; expect a library warning.");
        }
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}

END_NCBI_SCOPE
