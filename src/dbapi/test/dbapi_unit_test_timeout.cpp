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

#include "dbapi_unit_test_pch.hpp"

#include <connect/ncbi_socket.hpp>

BEGIN_NCBI_SCOPE

class CTimeoutGuard
{
public:
    CTimeoutGuard(I_DriverContext& dc, unsigned int timeout)
        : m_DriverContext(dc), m_SavedTimeout(dc.GetTimeout())
        { dc.SetTimeout(timeout); }
    ~CTimeoutGuard()
        {
            try {
                m_DriverContext.SetTimeout(m_SavedTimeout);
            } NCBI_CATCH_ALL("CTimeoutGuard")
        }

private:
    I_DriverContext& m_DriverContext;
    unsigned int     m_SavedTimeout;
};


////////////////////////////////////////////////////////////////////////////////
static void s_WaitForDelay(IConnection& conn)
{
    auto_ptr<IStatement> auto_stmt;
    bool timeout_was_reported = false;

    auto_stmt.reset(conn.GetStatement());

    try {
        auto_stmt->SendSql("waitfor delay '0:00:04'");
        BOOST_CHECK(auto_stmt->HasMoreResults());
    } catch(const CDB_TimeoutEx& ex) {
        _TRACE(ex);
        timeout_was_reported = true;
        auto_stmt->Cancel();
    }


    // Check if connection is alive ...
    auto_stmt->SendSql("SELECT name FROM sysobjects");
    BOOST_CHECK( auto_stmt->HasMoreResults() );
    BOOST_CHECK( auto_stmt->HasRows() );
    auto_ptr<IResultSet> rs( auto_stmt->GetResultSet() );
    BOOST_CHECK( rs.get() != NULL );

    BOOST_CHECK(timeout_was_reported);
}

////////////////////////////////////////////////////////////////////////////////
static void s_HugeTableSelect(IConnection& conn)
{
    const string table_name("#huge_table");


    // Check selecting from a huge table ...
    const char* str_value = "Oops ...";
    size_t rec_num = 800000;
    string sql;
    auto_ptr<IStatement> auto_stmt;

    auto_stmt.reset(conn.GetStatement());

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
                    conn.GetBulkInsert(table_name)
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

        auto_ptr<IStatement> auto_stmt(conn.GetStatement());

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
        auto_ptr<IStatement> auto_stmt(conn.GetStatement());
        auto_stmt->ExecuteUpdate("drop table " + table_name);
    }
}

///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(Test_Timeout)
{
    try {
        auto_ptr<IConnection> conn;
        auto_ptr<IStatement> auto_stmt;

        // Alter DriverContext ...
        {
            CTimeoutGuard GUARD(*GetDS().GetDriverContext(), 2);

            // Create connection ...
            conn.reset(GetDS().CreateConnection());
            BOOST_CHECK(conn.get() != NULL);

            conn->Connect(GetArgs().GetConnParams());

            s_WaitForDelay(*conn);

            // Crete new connection because some drivers
            // can close connection in the Test_WaitForDelay test.
            conn.reset(GetDS().CreateConnection());
            BOOST_CHECK(conn.get() != NULL);

            conn->Connect(GetArgs().GetConnParams());

            //
            // Check selecting from a huge table ...
            s_HugeTableSelect(*conn);
        } // Alter DriverContext ...
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}

///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(Test_Timeout2)
{
    try {
        auto_ptr<IConnection> conn;
        auto_ptr<IStatement> auto_stmt;

        // Alter connection ...
        {
            CTimeoutGuard GUARD(*GetDS().GetDriverContext(), 2);

            conn.reset(GetDS().CreateConnection());
            BOOST_CHECK(conn.get() != NULL);

            conn->Connect(GetArgs().GetConnParams());

            s_WaitForDelay(*conn);

            //
            // Check selecting from a huge table ...
            s_HugeTableSelect(*conn);

        }
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}

///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(Test_Heavy_Load)
{
    try {
        auto_ptr<IConnection> conn;

        CTimeoutGuard GUARD(*GetDS().GetDriverContext(), 2);

        conn.reset(GetDS().CreateConnection());

        // Heavy bulk-insert
        s_HugeTableSelect(*conn);

        string table_name = "#test_heavy_load";
        enum {num_tests = 30000};
        {
            string sql = "create table " + table_name + " ("
                "int_field int,"
                "flt_field float,"
                "date_field datetime,"
                "vc200_field varchar(200),"
                "vc2000_field varchar(2000),"
                "txt_field text"
                ")";

            auto_ptr<IStatement> auto_stmt( conn->GetStatement() );
            auto_stmt->ExecuteUpdate( sql );
        }

        // Heavy insert with parameters
        {
            auto_ptr<IStatement> auto_stmt( conn->GetStatement() );
            auto_ptr<IStatement> auto_stmt2( conn->GetStatement() );

            string sql = "INSERT INTO " + table_name +
                " VALUES(@int_field, @flt_field, @date_field, "
                "@vc200_field, @vc2000_field, @txt_field)";

            string vc200_val = string(190, 'a');
            string vc2000_val = string(1500, 'z');
            string txt_val = string(2000, 'q');

            CStopWatch timer(CStopWatch::eStart);

            auto_stmt->SetParam( CVariant( Int4(123456) ), "@int_field" );
            auto_stmt->SetParam( CVariant(654.321), "@flt_field" );
            auto_stmt->SetParam( CVariant(CTime(CTime::eCurrent)),
                "@date_field" );
            auto_stmt->SetParam( CVariant::VarChar(vc200_val.data(),
                vc200_val.size()),
                "@vc200_field" );
            auto_stmt->SetParam( CVariant::LongChar(vc2000_val.data(),
                vc2000_val.size()),
                "@vc2000_field"
                );
            auto_stmt->SetParam( CVariant::VarChar(txt_val.data(),
                txt_val.size()),
                "@txt_field" );

            auto_stmt2->ExecuteUpdate("BEGIN TRAN");
            for (int i = 0; i < num_tests; ++i) {
                auto_stmt->ExecuteUpdate( sql );

                if (i % 1000 == 0) {
                    auto_stmt2->ExecuteUpdate("COMMIT TRAN");
                    auto_stmt2->ExecuteUpdate("BEGIN TRAN");
                }
            }
            auto_stmt2->ExecuteUpdate("COMMIT TRAN");
            LOG_POST( "Inserts made in " << timer.Elapsed() << " sec." );
        }

        // Heavy select
        {
            auto_ptr<IStatement> auto_stmt( conn->GetStatement() );

            string sql = "select * from " + table_name;

            IResultSet* rs;

            CStopWatch timer(CStopWatch::eStart);

            rs = auto_stmt->ExecuteQuery(sql);

            while (rs->Next()) {
                /*int int_val =*/ rs->GetVariant(1).GetInt4();
                /*double flt_val =*/ rs->GetVariant(2).GetDouble();
                /*CTime date_val =*/ rs->GetVariant(3).GetCTime();
                /*string vc1_val =*/ rs->GetVariant(4).GetString();
                /*string vc2_val =*/ rs->GetVariant(5).GetString();
                const CVariant& txt_var = rs->GetVariant(6);
                string txt_val;
                txt_val.resize(txt_var.GetBlobSize());
                txt_var.Read(&txt_val[0], txt_val.size());
            }

            LOG_POST( "Select finished in " << timer.Elapsed() << " sec." );
        }
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}


///////////////////////////////////////////////////////////////////////////////
#define NUM_CANARIES 1024
#ifndef CHAR_BIT
#  define CHAR_BIT 8
#endif
#define NUM_CANARY_BITS NUM_CANARIES * CHAR_BIT

BOOST_AUTO_TEST_CASE(Test_High_FDs)
{
    try {
        // canary_bufN should surround the other local variables, so
        // whichever winds up at a lower address will take any hits.
        char                    canary_buf1[NUM_CANARIES];
        CTempString             canary1(canary_buf1, NUM_CANARIES), canary2;
        int                     i, j;
        auto_ptr<CSocket>       socks[NUM_CANARY_BITS];
        auto_ptr<CTimeoutGuard> GUARD;
        auto_ptr<IConnection>   conn;
        char                    canary_buf2[NUM_CANARIES];

        canary2.assign(canary_buf2, NUM_CANARIES);
        for (i = 0;  i < NUM_CANARIES * 8;  ++i) {
            socks[i].reset(new CDatagramSocket(fSOCK_KeepOnExec));
            if (socks[i]->GetStatus(eIO_Open) != eIO_Success) {
                break;
            }
        }
        LOG_POST("Opened " << i << " extra sockets; releasing the last 10.");
        for (j = i - 1;  j >= max(i - 10, 0);  --j) {
            socks[j].reset();
        }
        
        for (i = 0;  i < 2;  ++i) {
            switch (i) {
            case 0: // all zeroes
                memset(canary_buf1, 0, NUM_CANARIES);
                memset(canary_buf2, 0, NUM_CANARIES);
                break;
            case 1: // all ones
                memset(canary_buf1, ~(char)0, NUM_CANARIES);
                memset(canary_buf2, ~(char)0, NUM_CANARIES);
                break;
            }

            BOOST_CHECK_EQUAL(canary1, canary2);
            GUARD.reset(new CTimeoutGuard(*GetDS().GetDriverContext(), 2));
            conn.reset(GetDS().CreateConnection());
            conn->Connect(GetArgs().GetConnParams());
            s_WaitForDelay(*conn);
            GUARD.reset();
            conn.reset();
            BOOST_CHECK_EQUAL(canary1, canary2);
        }
    } catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}

END_NCBI_SCOPE
