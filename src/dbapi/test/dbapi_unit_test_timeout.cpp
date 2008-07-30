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

#include <ncbi_pch.hpp>

#include "dbapi_unit_test.hpp"

#include <common/test_assert.h>  /* This header must go last */


BEGIN_NCBI_SCOPE

///////////////////////////////////////////////////////////////////////////////
void CDBAPIUnitTest::Test_Timeout(void)
{
    try {
        auto_ptr<IConnection> auto_conn;
        auto_ptr<IStatement> auto_stmt;

        // Alter DriverContext ...
        {
            I_DriverContext* dc = GetDS().GetDriverContext();
            unsigned int timeout = dc->GetTimeout();

            dc->SetTimeout(2);

            // Create connection ...
            auto_conn.reset(GetDS().CreateConnection());
            BOOST_CHECK(auto_conn.get() != NULL);

            auto_conn->Connect(GetArgs().GetConnParams());

            Test_WaitForDelay(auto_conn);

            // Crete new connection because some drivers (ftds8 for example)
            // can close connection in the Test_WaitForDelay test.
            auto_conn.reset(GetDS().CreateConnection());
            BOOST_CHECK(auto_conn.get() != NULL);

            auto_conn->Connect(GetArgs().GetConnParams());

            //
            // Check selecting from a huge table ...
            Test_HugeTableSelect(auto_conn);

            dc->SetTimeout(timeout);
        } // Alter DriverContext ...
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}

///////////////////////////////////////////////////////////////////////////////
void CDBAPIUnitTest::Test_Timeout2(void)
{
    try {
        auto_ptr<IConnection> auto_conn;
        auto_ptr<IStatement> auto_stmt;


        // Alter connection ...
        {
            auto_conn.reset(GetDS().CreateConnection());
            BOOST_CHECK(auto_conn.get() != NULL);

            auto_conn->Connect(GetArgs().GetConnParams());

            auto_conn->SetTimeout(2);

            Test_WaitForDelay(auto_conn);

            //
            // Check selecting from a huge table ...
            Test_HugeTableSelect(auto_conn);

        }
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}

////////////////////////////////////////////////////////////////////////////////
void CDBAPIUnitTest::Test_WaitForDelay(const auto_ptr<IConnection>& auto_conn)
{
    auto_ptr<IStatement> auto_stmt;
    bool timeout_was_reported = false;

    auto_stmt.reset(auto_conn->GetStatement());

    if(GetArgs().GetDriverName() == ftds8_driver) {
        try {
            auto_stmt->SendSql("waitfor delay '0:00:04'");
            BOOST_CHECK(auto_stmt->HasMoreResults());
        } catch(const CDB_Exception&) {
            timeout_was_reported = true;
            auto_stmt->Cancel();
        } catch(...) {
            BOOST_CHECK(false);
        }
    } else {
        try {
            auto_stmt->SendSql("waitfor delay '0:00:04'");
            BOOST_CHECK(auto_stmt->HasMoreResults());
        } catch(const CDB_TimeoutEx&) {
            timeout_was_reported = true;
            auto_stmt->Cancel();
        }
    }


    // Check if connection is alive ...
    if (GetArgs().GetDriverName() != ftds8_driver) {
        auto_stmt->SendSql("SELECT name FROM sysobjects");
        BOOST_CHECK( auto_stmt->HasMoreResults() );
        BOOST_CHECK( auto_stmt->HasRows() );
        auto_ptr<IResultSet> rs( auto_stmt->GetResultSet() );
        BOOST_CHECK( rs.get() != NULL );
    } else {
        GetArgs().PutMsgDisabled("Test_Timeout. Check if connection is alive.");
    }

    BOOST_CHECK(timeout_was_reported);
}

////////////////////////////////////////////////////////////////////////////////
void CDBAPIUnitTest::Test_HugeTableSelect(const auto_ptr<IConnection>& auto_conn)
{
    const string table_name("#huge_table");


    // Check selecting from a huge table ...
    const char* str_value = "Oops ...";
    size_t rec_num = 800000;
    string sql;
    auto_ptr<IStatement> auto_stmt;

    auto_stmt.reset(auto_conn->GetStatement());

    bool small_amount = !GetArgs().IsBCPAvailable()
                        ||  GetArgs().GetServerType() == CDBConnParams::eSybaseSQLServer;

    // Preparation ...
    {
        string sql;
        
        // Create table ...
        {
            sql  = " CREATE TABLE " + table_name + "( \n";
            sql += "    id INT PRIMARY KEY, \n";
            sql += "    vc8000_field VARCHAR(1500) NOT NULL, \n";
            sql += " )";

            // Create the table
            auto_stmt->ExecuteUpdate(sql);
        }
    }

    // Populate table with data ...
    {
        CStopWatch timer(CStopWatch::eStart);

        if (!small_amount) {
            CVariant col1(eDB_Int);
            CVariant col2(eDB_VarChar);

            auto_ptr<IBulkInsert> bi(
                    auto_conn->GetBulkInsert(table_name)
                    );

            bi->Bind(1, &col1);
            bi->Bind(2, &col2);

            col2 = str_value;

            for (size_t i = 0; i < rec_num; ++i) {
                col1 = Int4(i);

                bi->AddRow();

                if (i % 1000 == 0) {
                    bi->StoreBatch();
                    if (timer.Elapsed() > 4)
                        break;
                }
            }
            bi->Complete();
        } else {
            sql = "INSERT INTO " + table_name +
                "(id, vc8000_field) VALUES(@id, @vc_val)";

            //auto_stmt->ExecuteUpdate("BEGIN TRANSACTION");

            for (size_t i = 0; i < rec_num; ++i) {
                auto_stmt->SetParam( CVariant( Int4(i) ), "@id" );
                auto_stmt->SetParam( CVariant::VarChar(str_value), "@vc_val");

                auto_stmt->ExecuteUpdate( sql );

                /*if (i % 100 == 0) {
                    auto_stmt->ExecuteUpdate("COMMIT TRANSACTION");
                    auto_stmt->ExecuteUpdate("BEGIN TRANSACTION");
                    }*/
                if (timer.Elapsed() > 4)
                    break;
            }

            //auto_stmt->ExecuteUpdate("COMMIT TRANSACTION");
        }

        LOG_POST( "Huge table inserted in " << timer.Elapsed() << " sec." );
    }

    // Read data ...
    {
        size_t num = 0;

        auto_ptr<IStatement> auto_stmt(auto_conn->GetStatement());

        CStopWatch timer(CStopWatch::eStart);

        if (small_amount) {
            auto_stmt->SendSql("SELECT top 800000 * FROM " + table_name + " a, " + table_name + " b");
        }
        else {
            auto_stmt->SendSql("SELECT * FROM " + table_name);
        }

        while (auto_stmt->HasMoreResults()) {
            if (auto_stmt->HasRows()) {
                auto_ptr<IResultSet> rs( auto_stmt->GetResultSet() );
                BOOST_CHECK( rs.get() != NULL );

                while (rs->Next()) {
                    ++num;
                    if (timer.Elapsed() > 4)
                        break;
                }
            }
        }

        LOG_POST( "Huge table selected in " << timer.Elapsed() << " sec." );
    }

    {
        auto_ptr<IStatement> auto_stmt(auto_conn->GetStatement());
        auto_stmt->ExecuteUpdate("drop table " + table_name);
    }
}


END_NCBI_SCOPE
