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


BEGIN_NCBI_SCOPE

///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(Test_SelectStmt)
{
    try {
        // Scenario:
        // 1) Select recordset with just one record
        // 2) Retrive only one record.
        // 3) Select another recordset with just one record
        {
            auto_ptr<IStatement> auto_stmt( GetConnection().GetStatement() );
            IResultSet* rs;

            // 1) Select recordset with just one record
            rs = auto_stmt->ExecuteQuery( "select qq = 57 + 33" );
            BOOST_CHECK( rs != NULL );

            // 2) Retrive a record.
            BOOST_CHECK( rs->Next() );
            BOOST_CHECK( !rs->Next() );

            // 3) Select another recordset with just one record
            rs = auto_stmt->ExecuteQuery( "select qq = 57.55 + 0.0033" );
            BOOST_CHECK( rs != NULL );
            BOOST_CHECK( rs->Next() );
            BOOST_CHECK( !rs->Next() );
        }

        // Same as before but uses two differenr connections ...
        if (false) {
            auto_ptr<IStatement> auto_stmt( GetConnection().GetStatement() );
            IResultSet* rs;

            // 1) Select recordset with just one record
            rs = auto_stmt->ExecuteQuery( "select qq = 57 + 33" );
            BOOST_CHECK( rs != NULL );

            // 2) Retrive only one record.
            BOOST_CHECK( rs->Next() );
            BOOST_CHECK( !rs->Next() );

            // 3) Select another recordset with just one record
            auto_ptr<IStatement> auto_stmt2( GetConnection().CreateStatement() );
            rs = auto_stmt2->ExecuteQuery( "select qq = 57.55 + 0.0033" );
            BOOST_CHECK( rs != NULL );
            BOOST_CHECK( rs->Next() );
            BOOST_CHECK( !rs->Next() );
        }

        // Check column name ...
        {
            auto_ptr<IStatement> auto_stmt( GetConnection().GetStatement() );

            IResultSet* rs(
                auto_stmt->ExecuteQuery( "select @@version as oops" )
                );
            BOOST_CHECK( rs != NULL );
            BOOST_CHECK( rs->Next() );
            auto_ptr<const IResultSetMetaData> col_metadata(rs->GetMetaData());
            BOOST_CHECK_EQUAL( string("oops"), col_metadata->GetName(1) );
        }

        // Check resultset ...
        {
            int num = 0;
            string sql = "select user_id(), convert(varchar(64), user_name()), "
                "convert(nvarchar(64), user_name())";

            auto_ptr<IStatement> auto_stmt( GetConnection().GetStatement() );

            // 1) Select recordset with just one record
            IResultSet* rs( auto_stmt->ExecuteQuery( sql ) );
            BOOST_CHECK( rs != NULL );

            while (rs->Next()) {
                BOOST_CHECK(rs->GetVariant(1).GetInt4() > 0);
                BOOST_CHECK(rs->GetVariant(2).GetString().size() > 0);
                BOOST_CHECK(rs->GetVariant(3).GetString().size() > 0);
                ++num;
            }

            BOOST_CHECK_EQUAL(num, 1);
        }

        // Check sequent call of ExecuteQuery ...
        if (true) {
            IResultSet* rs = NULL;
            auto_ptr<IStatement> auto_stmt( GetConnection().GetStatement() );

            // Run first time ...
            rs = auto_stmt->ExecuteQuery( "select @@version as oops" );
            BOOST_CHECK( rs != NULL );
            BOOST_CHECK( rs->Next() );

            // Run second time ...
            rs = auto_stmt->ExecuteQuery( "select @@version as oops" );
            BOOST_CHECK( rs != NULL );
            BOOST_CHECK( rs->Next() );

            // Run third time ...
            rs = auto_stmt->ExecuteQuery( "select @@version as oops" );
            BOOST_CHECK( rs != NULL );
            BOOST_CHECK( rs->Next() );
        } else {
            GetArgs().PutMsgDisabled("Check sequent call of ExecuteQuery");
        }

        // Select NULL values and empty strings ...
        {
            auto_ptr<IStatement> auto_stmt( GetConnection().GetStatement() );
            auto_ptr<IResultSet> rs;

            rs.reset(
                auto_stmt->ExecuteQuery( "SELECT '', NULL, NULL, NULL" )
                );
            BOOST_CHECK( rs.get() != NULL );
            BOOST_CHECK( rs->Next() );

            rs.reset(
                auto_stmt->ExecuteQuery( "SELECT NULL, '', NULL, NULL" )
                );
            BOOST_CHECK( rs.get() != NULL );
            BOOST_CHECK( rs->Next() );

            rs.reset(
                auto_stmt->ExecuteQuery( "SELECT NULL, NULL, '', NULL" )
                );
            BOOST_CHECK( rs.get() != NULL );
            BOOST_CHECK( rs->Next() );

            rs.reset(
                auto_stmt->ExecuteQuery( "SELECT NULL, NULL, NULL, ''" )
                );
            BOOST_CHECK( rs.get() != NULL );
            BOOST_CHECK( rs->Next() );

            rs.reset(
                auto_stmt->ExecuteQuery( "SELECT '', '', NULL, NULL" )
                );
            BOOST_CHECK( rs.get() != NULL );
            BOOST_CHECK( rs->Next() );

            rs.reset(
                auto_stmt->ExecuteQuery( "SELECT NULL, '', '', NULL" )
                );
            BOOST_CHECK( rs.get() != NULL );
            BOOST_CHECK( rs->Next() );

            rs.reset(
                auto_stmt->ExecuteQuery( "SELECT NULL, NULL, '', ''" )
                );
            BOOST_CHECK( rs.get() != NULL );
            BOOST_CHECK( rs->Next() );
        }


    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}


///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(Test_SelectStmt2)
{
    string sql;
    string table_name("#select_stmt2");

    try {
        auto_ptr<IStatement> auto_stmt( GetConnection().GetStatement() );

        // Running parametrized statement ...
        {
            int result_num = 0;

            auto_stmt->SetParam( CVariant( "master" ), "@dbname" );

            auto_stmt->SendSql("exec sp_helpdb @dbname");

            while(auto_stmt->HasMoreResults()) {
                if( auto_stmt->HasRows() ) {
                    ++result_num;
                }
            }

            auto_stmt->ClearParamList();
        }

        // Create table ...
        {
            sql =
                "CREATE TABLE " + table_name + " ( \n"
                "id INT NOT NULL PRIMARY KEY, \n"
                "name VARCHAR(255) NOT NULL \n"
                ")"
                ;

            auto_stmt->ExecuteUpdate(sql);
        }

        // Insert data ...
        {
            sql  = "insert into ";
            sql += table_name + "( id, name ) values ( 1, 'one' )";

            auto_stmt->ExecuteUpdate(sql);
        }

        // Query data ...
        {
            sql  = "select id, name from ";
            sql += table_name;

            auto_stmt->SendSql(sql);
            while( auto_stmt->HasMoreResults() ) {
                if( auto_stmt->HasRows() ) {
                    auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());
                    while( rs->Next() ) {
                        ;
                    }
                }
            }
        }
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}

///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(Test_SelectStmtXML)
{
    try {
        // SQL + XML
        {
            string sql;
            auto_ptr<IStatement> auto_stmt( GetConnection().GetStatement() );
            IResultSet* rs;

            sql = "select 1 as Tag, null as Parent, 1 as [x!1!id] for xml explicit";
            rs = auto_stmt->ExecuteQuery( sql );
            BOOST_CHECK( rs != NULL );

            if ( !rs->Next() ) {
                BOOST_FAIL( msg_record_expected );
            }

            // Same but call Execute instead of ExecuteQuery.
            auto_stmt->SendSql( sql );
        }
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}

///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(Test_Recordset)
{
    try {
        auto_ptr<IStatement> auto_stmt(GetConnection().GetStatement());
        IResultSet* rs;

        // First test ...
        {
            // bit
            {
                rs = auto_stmt->ExecuteQuery("select convert(bit, 1)");
                BOOST_CHECK(rs != NULL);

                BOOST_CHECK(rs->Next());
                BOOST_CHECK(!rs->Next());

                DumpResults(auto_stmt.get());
            }

            // tinyint
            {
                rs = auto_stmt->ExecuteQuery("select convert(tinyint, 1)");
                BOOST_CHECK(rs != NULL);

                BOOST_CHECK(rs->Next());
                BOOST_CHECK(!rs->Next());

                DumpResults(auto_stmt.get());
            }

            // smallint
            {
                rs = auto_stmt->ExecuteQuery("select convert(smallint, 1)");
                BOOST_CHECK(rs != NULL);

                BOOST_CHECK(rs->Next());
                BOOST_CHECK(!rs->Next());

                DumpResults(auto_stmt.get());
            }

            // int
            {
                rs = auto_stmt->ExecuteQuery("select convert(int, 1)");
                BOOST_CHECK(rs != NULL);

                BOOST_CHECK(rs->Next());
                BOOST_CHECK(!rs->Next());

                DumpResults(auto_stmt.get());
            }

            // numeric
            if (GetArgs().GetDriverName() != dblib_driver  ||  GetArgs().GetServerType() == CDBConnParams::eSybaseSQLServer)
            {
                //
                {
                    rs = auto_stmt->ExecuteQuery("select convert(numeric(38, 0), 1)");
                    BOOST_CHECK(rs != NULL);

                    BOOST_CHECK(rs->Next());
                    BOOST_CHECK(!rs->Next());

                    DumpResults(auto_stmt.get());
                }

                //
                {
                    rs = auto_stmt->ExecuteQuery("select convert(numeric(18, 2), 2843113322)");
                    BOOST_CHECK(rs != NULL);

                    BOOST_CHECK(rs->Next());
                    BOOST_CHECK(!rs->Next());

                    DumpResults(auto_stmt.get());
                }
            }

            // decimal
            if (GetArgs().GetDriverName() != dblib_driver  ||  GetArgs().GetServerType() == CDBConnParams::eSybaseSQLServer)
            {
                rs = auto_stmt->ExecuteQuery("select convert(decimal(38, 0), 1)");
                BOOST_CHECK(rs != NULL);

                BOOST_CHECK(rs->Next());
                BOOST_CHECK(!rs->Next());

                DumpResults(auto_stmt.get());
            }

            // float
            {
                rs = auto_stmt->ExecuteQuery("select convert(float(4), 1)");
                BOOST_CHECK(rs != NULL);

                BOOST_CHECK(rs->Next());
                BOOST_CHECK(!rs->Next());

                DumpResults(auto_stmt.get());
            }

            // double
            {
                rs = auto_stmt->ExecuteQuery("select convert(double precision, 1)");
                BOOST_CHECK(rs != NULL);

                BOOST_CHECK(rs->Next());
                BOOST_CHECK(!rs->Next());

                DumpResults(auto_stmt.get());
            }

            // real
            {
                rs = auto_stmt->ExecuteQuery("select convert(real, 1)");
                BOOST_CHECK(rs != NULL);

                BOOST_CHECK(rs->Next());
                BOOST_CHECK(!rs->Next());

                DumpResults(auto_stmt.get());
            }

            // smallmoney
            // Unsupported type ...
        //     {
        //         rs = auto_stmt->ExecuteQuery("select convert(smallmoney, 1)");
        //         BOOST_CHECK(rs != NULL);
        //
        //         BOOST_CHECK(rs->Next());
        //         BOOST_CHECK(!rs->Next());
        //
        //         DumpResults(auto_stmt.get());
        //     }

            // money
            // Unsupported type ...
        //     {
        //         rs = auto_stmt->ExecuteQuery("select convert(money, 1)");
        //         BOOST_CHECK(rs != NULL);
        //
        //         BOOST_CHECK(rs->Next());
        //         BOOST_CHECK(!rs->Next());
        //
        //         DumpResults(auto_stmt.get());
        //     }

            // smalldatetime
            {
                rs = auto_stmt->ExecuteQuery("select convert(smalldatetime, 'January 1, 1900')");
                BOOST_CHECK(rs != NULL);

                BOOST_CHECK(rs->Next());
                BOOST_CHECK(!rs->Next());

                DumpResults(auto_stmt.get());
            }

            // datetime
            {
                rs = auto_stmt->ExecuteQuery("select convert(datetime, 'January 1, 1753')");
                BOOST_CHECK(rs != NULL);

                BOOST_CHECK(rs->Next());
                BOOST_CHECK(!rs->Next());

                DumpResults(auto_stmt.get());
            }

            // char
            {
                rs = auto_stmt->ExecuteQuery("select convert(char(32), '12345')");
                BOOST_CHECK(rs != NULL);

                BOOST_CHECK(rs->Next());
                BOOST_CHECK(!rs->Next());

                DumpResults(auto_stmt.get());
            }

            // varchar
            {
                rs = auto_stmt->ExecuteQuery("select convert(varchar(32), '12345')");
                BOOST_CHECK(rs != NULL);

                BOOST_CHECK(rs->Next());
                BOOST_CHECK(!rs->Next());

                DumpResults(auto_stmt.get());
            }

            // nchar
            {
                rs = auto_stmt->ExecuteQuery("select convert(nchar(32), '12345')");
                BOOST_CHECK(rs != NULL);

                BOOST_CHECK(rs->Next());
                BOOST_CHECK(!rs->Next());

                DumpResults(auto_stmt.get());
            }

            // nvarchar
            {
                rs = auto_stmt->ExecuteQuery("select convert(nvarchar(32), '12345')");
                BOOST_CHECK(rs != NULL);

                BOOST_CHECK(rs->Next());
                BOOST_CHECK(!rs->Next());

                DumpResults(auto_stmt.get());
            }

            // binary
            {
                rs = auto_stmt->ExecuteQuery("select convert(binary(32), '12345')");
                BOOST_CHECK(rs != NULL);

                BOOST_CHECK(rs->Next());
                BOOST_CHECK(!rs->Next());

                DumpResults(auto_stmt.get());
            }

            // varbinary
            {
                rs = auto_stmt->ExecuteQuery("select convert(varbinary(32), '12345')");
                BOOST_CHECK(rs != NULL);

                BOOST_CHECK(rs->Next());
                BOOST_CHECK(!rs->Next());

                DumpResults(auto_stmt.get());
            }

            // text
            {
                rs = auto_stmt->ExecuteQuery("select convert(text, '12345')");
                BOOST_CHECK(rs != NULL);

                BOOST_CHECK(rs->Next());
                BOOST_CHECK(!rs->Next());

                DumpResults(auto_stmt.get());
            }

            // image
            {
                rs = auto_stmt->ExecuteQuery("select convert(image, '12345')");
                BOOST_CHECK(rs != NULL);

                BOOST_CHECK(rs->Next());
                BOOST_CHECK(!rs->Next());

                DumpResults(auto_stmt.get());
            }
        }

        // Second test ...
        {
            // bit
            {
                rs = auto_stmt->ExecuteQuery("select convert(bit, 1)");
                BOOST_CHECK(rs != NULL);

                BOOST_CHECK(rs->Next());
                const CVariant& variant = rs->GetVariant(1);

                CDB_Bit* data = dynamic_cast<CDB_Bit*>(variant.GetData());
                BOOST_CHECK(data);

                BOOST_CHECK_EQUAL(data->Value(), 1);

                DumpResults(auto_stmt.get());
            }

            // tinyint
            {
                rs = auto_stmt->ExecuteQuery("select convert(tinyint, 1)");
                BOOST_CHECK(rs != NULL);

                BOOST_CHECK(rs->Next());
                const CVariant& variant = rs->GetVariant(1);

                CDB_TinyInt* data = dynamic_cast<CDB_TinyInt*>(variant.GetData());
                BOOST_CHECK(data);

                BOOST_CHECK_EQUAL(data->Value(), 1);

                DumpResults(auto_stmt.get());
            }

            // smallint
            {
                rs = auto_stmt->ExecuteQuery("select convert(smallint, 1)");
                BOOST_CHECK(rs != NULL);

                BOOST_CHECK(rs->Next());
                const CVariant& variant = rs->GetVariant(1);

                CDB_SmallInt* data = dynamic_cast<CDB_SmallInt*>(variant.GetData());
                BOOST_CHECK(data);

                BOOST_CHECK_EQUAL(data->Value(), 1);

                DumpResults(auto_stmt.get());
            }

            // int
            {
                rs = auto_stmt->ExecuteQuery("select convert(int, 1)");
                BOOST_CHECK(rs != NULL);

                BOOST_CHECK(rs->Next());
                const CVariant& variant = rs->GetVariant(1);

                CDB_Int* data = dynamic_cast<CDB_Int*>(variant.GetData());
                BOOST_CHECK(data);

                BOOST_CHECK_EQUAL(data->Value(), 1);

                DumpResults(auto_stmt.get());
            }

            // numeric
            if (GetArgs().GetDriverName() != dblib_driver  ||  GetArgs().GetServerType() == CDBConnParams::eSybaseSQLServer)
            {
                //
                {
                    rs = auto_stmt->ExecuteQuery("select convert(numeric(38, 0), 1)");
                    BOOST_CHECK(rs != NULL);

                    BOOST_CHECK(rs->Next());
                    const CVariant& variant = rs->GetVariant(1);

                    switch(variant.GetData()->GetType()) {
                        case eDB_Numeric:
                            {
                                CDB_Numeric* data = static_cast<CDB_Numeric*>(variant.GetData());
                                BOOST_CHECK_EQUAL(data->Value(), string("1"));
                            }
                            break;
                        case eDB_Double:
                            {
                                GetArgs().PutMsgExpected("CDB_Numeric", "CDB_Double");

                                CDB_Double* data = static_cast<CDB_Double*>(variant.GetData());
                                BOOST_CHECK_EQUAL(data->Value(), 1);
                            }
                            break;
                        default:
                            BOOST_FAIL("Invalid data type.");
                    }

                    DumpResults(auto_stmt.get());
                }

                //
                {
                    rs = auto_stmt->ExecuteQuery("select convert(numeric(18, 2), 2843113322)");
                    BOOST_CHECK(rs != NULL);

                    BOOST_CHECK(rs->Next());
                    const CVariant& variant = rs->GetVariant(1);

                    switch(variant.GetData()->GetType()) {
                        case eDB_Numeric:
                            {
                                CDB_Numeric* data = static_cast<CDB_Numeric*>(variant.GetData());

                                if (GetArgs().IsODBCBased()) {
                                    BOOST_CHECK_EQUAL(data->Value(), string("2843113322"));
                                } else {
                                    BOOST_CHECK_EQUAL(data->Value(), string("2843113322.00"));
                                }
                            }
                            break;
                        case eDB_Double:
                            {
                                GetArgs().PutMsgExpected("CDB_Numeric", "CDB_Double");

                                CDB_Double* data = static_cast<CDB_Double*>(variant.GetData());
                                BOOST_CHECK_EQUAL(data->Value(), 2843113322U);
                            }
                            break;
                        default:
                            BOOST_FAIL("Invalid data type.");
                    }

                    DumpResults(auto_stmt.get());
                }
            }

            // decimal
            if (GetArgs().GetDriverName() != dblib_driver  ||  GetArgs().GetServerType() == CDBConnParams::eSybaseSQLServer)
            {
                rs = auto_stmt->ExecuteQuery("select convert(decimal(38, 0), 1)");
                BOOST_CHECK(rs != NULL);

                BOOST_CHECK(rs->Next());
                const CVariant& variant = rs->GetVariant(1);

                switch(variant.GetData()->GetType()) {
                    case eDB_Numeric:
                        {
                            CDB_Numeric* data = static_cast<CDB_Numeric*>(variant.GetData());
                            BOOST_CHECK_EQUAL(data->Value(), string("1"));
                        }
                        break;
                    case eDB_Double:
                        {
                            GetArgs().PutMsgExpected("CDB_Numeric", "CDB_Double");

                            CDB_Double* data = static_cast<CDB_Double*>(variant.GetData());
                            BOOST_CHECK_EQUAL(data->Value(), 1);
                        }
                        break;
                    default:
                        BOOST_FAIL("Invalid data type.");
                }

                // CDB_Numeric* data = dynamic_cast<CDB_Numeric*>(variant.GetData());
                // BOOST_CHECK(data);

                // BOOST_CHECK_EQUAL(data->Value(), string("1"));

                DumpResults(auto_stmt.get());
            }

            // float
            {
                rs = auto_stmt->ExecuteQuery("select convert(float(4), 1)");
                BOOST_CHECK(rs != NULL);

                BOOST_CHECK(rs->Next());
                const CVariant& variant = rs->GetVariant(1);

                //
                CDB_Float* float_data = NULL;
                float_data = dynamic_cast<CDB_Float*>(variant.GetData());

                if (float_data) {
                    // GetArgs().PutMsgExpected("CDB_Double", "CDB_Float");
                    BOOST_CHECK_EQUAL(float_data->Value(), 1);
                }

                //
                CDB_Double* double_data = NULL;
                double_data = dynamic_cast<CDB_Double*>(variant.GetData());

                if (double_data) {
                    GetArgs().PutMsgExpected("CDB_Float", "CDB_Double");
                    BOOST_CHECK_EQUAL(double_data->Value(), 1);
                }

                DumpResults(auto_stmt.get());
            }

            // double
            {
                //
                {
                    rs = auto_stmt->ExecuteQuery("select convert(double precision, 1)");
                    BOOST_CHECK(rs != NULL);

                    BOOST_CHECK(rs->Next());
                    const CVariant& variant = rs->GetVariant(1);

                    CDB_Double* data = dynamic_cast<CDB_Double*>(variant.GetData());
                    BOOST_CHECK(data);

                    BOOST_CHECK_EQUAL(data->Value(), 1);

                    DumpResults(auto_stmt.get());
                }

                //
                {
                    rs = auto_stmt->ExecuteQuery("select convert(double precision, 2843113322)");
                    BOOST_CHECK(rs != NULL);

                    BOOST_CHECK(rs->Next());
                    const CVariant& variant = rs->GetVariant(1);

                    CDB_Double* data = dynamic_cast<CDB_Double*>(variant.GetData());
                    BOOST_CHECK(data);

                    BOOST_CHECK_EQUAL(data->Value(), 2843113322U);

                    DumpResults(auto_stmt.get());
                }
            }

            // real
            {
                rs = auto_stmt->ExecuteQuery("select convert(real, 1)");
                BOOST_CHECK(rs != NULL);

                BOOST_CHECK(rs->Next());
                const CVariant& variant = rs->GetVariant(1);

                CDB_Float* data = dynamic_cast<CDB_Float*>(variant.GetData());
                BOOST_CHECK(data);

                BOOST_CHECK_EQUAL(data->Value(), 1);

                DumpResults(auto_stmt.get());
            }

            // smallmoney
            // Unsupported type ...
        //     {
        //         rs = auto_stmt->ExecuteQuery("select convert(smallmoney, 1)");
        //         BOOST_CHECK(rs != NULL);
        //
        //         BOOST_CHECK(rs->Next());
        //         BOOST_CHECK(!rs->Next());
        //
        //         DumpResults(auto_stmt.get());
        //     }

            // money
            // Unsupported type ...
        //     {
        //         rs = auto_stmt->ExecuteQuery("select convert(money, 1)");
        //         BOOST_CHECK(rs != NULL);
        //
        //         BOOST_CHECK(rs->Next());
        //         BOOST_CHECK(!rs->Next());
        //
        //         DumpResults(auto_stmt.get());
        //     }

            // smalldatetime
            {
                rs = auto_stmt->ExecuteQuery("select convert(smalldatetime, 'January 1, 1900')");
                BOOST_CHECK(rs != NULL);

                BOOST_CHECK(rs->Next());
                const CVariant& variant = rs->GetVariant(1);

                CDB_SmallDateTime* data = dynamic_cast<CDB_SmallDateTime*>(variant.GetData());
                BOOST_CHECK(data);

                // BOOST_CHECK_EQUAL(data->Value(), 1);

                DumpResults(auto_stmt.get());
            }

            // datetime
            {
                rs = auto_stmt->ExecuteQuery("select convert(datetime, 'January 1, 1753')");
                BOOST_CHECK(rs != NULL);

                BOOST_CHECK(rs->Next());
                const CVariant& variant = rs->GetVariant(1);

                CDB_DateTime* dt_data = NULL;
                dt_data = dynamic_cast<CDB_DateTime*>(variant.GetData());

                if (!dt_data){
                    GetArgs().PutMsgExpected("CDB_DateTime", "CDB_SmallDateTime");

                    CDB_SmallDateTime* data = dynamic_cast<CDB_SmallDateTime*>(variant.GetData());
                    BOOST_CHECK(data);
                }

                // BOOST_CHECK_EQUAL(data->Value(), 1);

                DumpResults(auto_stmt.get());
            }

            // char
            {
                rs = auto_stmt->ExecuteQuery("select convert(char(10), '12345')");
                BOOST_CHECK(rs != NULL);

                BOOST_CHECK(rs->Next());
                const CVariant& variant = rs->GetVariant(1);

                //
                CDB_Char* char_data = NULL;
                char_data = dynamic_cast<CDB_Char*>(variant.GetData());

                if(char_data) {
                    BOOST_CHECK_EQUAL(char_data->AsString(),
                                      string("12345     "));
                }

                //
                CDB_VarChar* varchar_data = NULL;
                varchar_data = dynamic_cast<CDB_VarChar*>(variant.GetData());

                if(varchar_data) {
                    GetArgs().PutMsgExpected("CDB_Char", "CDB_VarChar");

                    BOOST_CHECK_EQUAL(varchar_data->AsString(),
                                      string("12345     "));
                }

                //
                CDB_LongChar* longchar_data = NULL;
                longchar_data = dynamic_cast<CDB_LongChar*>(variant.GetData());

                if(longchar_data) {
                    GetArgs().PutMsgExpected("CDB_Char", "CDB_LongChar");

                    BOOST_CHECK_EQUAL(longchar_data->Size(), size_t(10));
                    BOOST_CHECK_EQUAL(longchar_data->AsString(),
                            string("12345     ")
                            );
                }


                DumpResults(auto_stmt.get());
            }

            // varchar
            {
                rs = auto_stmt->ExecuteQuery("select convert(varchar(10), '12345')");
                BOOST_CHECK(rs != NULL);

                BOOST_CHECK(rs->Next());
                const CVariant& variant = rs->GetVariant(1);

                //
                CDB_Char* char_data = NULL;
                char_data = dynamic_cast<CDB_Char*>(variant.GetData());

                if(char_data) {
                    GetArgs().PutMsgExpected("CDB_VarChar", "CDB_Char");

                    BOOST_CHECK_EQUAL(char_data->AsString(),
                                      string("12345     "));
                }

                //
                CDB_VarChar* varchar_data = NULL;
                varchar_data = dynamic_cast<CDB_VarChar*>(variant.GetData());

                if (varchar_data) {
                    BOOST_CHECK_EQUAL(varchar_data->AsString(),
                                      string("12345"));
                }

                //
                CDB_LongChar* longchar_data = NULL;
                longchar_data = dynamic_cast<CDB_LongChar*>(variant.GetData());

                if(longchar_data) {
                    GetArgs().PutMsgExpected("CDB_VarChar", "CDB_LongChar");

                    BOOST_CHECK_EQUAL(longchar_data->Size(), size_t(32));
                    BOOST_CHECK_EQUAL(longchar_data->AsString(),
                            string("12345     ")
                            );
                }

                DumpResults(auto_stmt.get());
            }

            // nchar
            {
                rs = auto_stmt->ExecuteQuery("select convert(nchar(10), '12345')");
                BOOST_CHECK(rs != NULL);

                BOOST_CHECK(rs->Next());
                const CVariant& variant = rs->GetVariant(1);

                CDB_Char* char_data = NULL;
                char_data = dynamic_cast<CDB_Char*>(variant.GetData());

                if(char_data) {
                    BOOST_CHECK_EQUAL(char_data->AsString(),
                                      string("12345     "));
                }

                CDB_VarChar* varchar_data = NULL;
                varchar_data = dynamic_cast<CDB_VarChar*>(variant.GetData());

                if(varchar_data) {
                    GetArgs().PutMsgExpected("CDB_Char", "CDB_VarChar");

                    BOOST_CHECK_EQUAL(varchar_data->AsString(),
                                      string("12345     "));
                }

                CDB_LongChar* longchar_data = NULL;
                longchar_data = dynamic_cast<CDB_LongChar*>(variant.GetData());

                if(longchar_data) {
                    GetArgs().PutMsgExpected("CDB_Char", "CDB_LongChar");

                    BOOST_CHECK_EQUAL(longchar_data->Size(), size_t(10));
                    BOOST_CHECK_EQUAL(longchar_data->AsString(),
                            string("12345     ")
                            );
                }

                DumpResults(auto_stmt.get());
            }

            // nvarchar
            {
                rs = auto_stmt->ExecuteQuery("select convert(nvarchar(10), '12345')");
                BOOST_CHECK(rs != NULL);

                BOOST_CHECK(rs->Next());
                const CVariant& variant = rs->GetVariant(1);

                //
                CDB_Char* char_data = NULL;
                char_data = dynamic_cast<CDB_Char*>(variant.GetData());

                if(char_data) {
                    GetArgs().PutMsgExpected("CDB_VarChar", "CDB_Char");

                    BOOST_CHECK_EQUAL(char_data->AsString(),
                                      string("12345     "));
                }

                //
                CDB_VarChar* varchar_data = NULL;
                varchar_data = dynamic_cast<CDB_VarChar*>(variant.GetData());

                if (varchar_data) {
                    BOOST_CHECK_EQUAL(varchar_data->AsString(),
                                      string("12345"));
                }

                //
                CDB_LongChar* longchar_data = NULL;
                longchar_data = dynamic_cast<CDB_LongChar*>(variant.GetData());

                if(longchar_data) {
                    GetArgs().PutMsgExpected("CDB_VarChar", "CDB_LongChar");

                    BOOST_CHECK_EQUAL(longchar_data->Size(), size_t(32));
                    BOOST_CHECK_EQUAL(longchar_data->AsString(),
                            string("12345     ")
                            );
                }

                DumpResults(auto_stmt.get());
            }

            // binary
            {
                rs = auto_stmt->ExecuteQuery("select convert(binary(10), '12345')");
                BOOST_CHECK(rs != NULL);

                BOOST_CHECK(rs->Next());
                const CVariant& variant = rs->GetVariant(1);

                //
                CDB_Binary* char_data = NULL;
                char_data = dynamic_cast<CDB_Binary*>(variant.GetData());

                if(char_data) {
                    BOOST_CHECK_EQUAL(char_data->Size(), size_t(10));
                    // BOOST_CHECK_EQUAL(string(static_cast<const char*>(char_data->Value()),
                    //             char_data->Size()),
                    //         string("12345")
                    //         );
                    BOOST_CHECK_EQUAL(
                        memcmp(
                            char_data->Value(),
                            "12345\0\0\0\0\0",
                            char_data->Size()
                            ),
                        0
                        );
                }

                //
                CDB_VarBinary* varchar_data = NULL;
                varchar_data = dynamic_cast<CDB_VarBinary*>(variant.GetData());

                if(varchar_data) {
                    GetArgs().PutMsgExpected("CDB_Binary", "CDB_VarBinary");

                    BOOST_CHECK_EQUAL(varchar_data->Size(), size_t(10));
                    // BOOST_CHECK_EQUAL(string(static_cast<const char*>(varchar_data->Value()),
                    //             varchar_data->Size()),
                    //         string("12345")
                    //         );
                    BOOST_CHECK_EQUAL(
                        memcmp(
                            varchar_data->Value(),
                            "12345\0\0\0\0\0",
                            varchar_data->Size()
                            ),
                        0
                        );
                }

                //
                CDB_LongBinary* longchar_data = NULL;
                longchar_data = dynamic_cast<CDB_LongBinary*>(variant.GetData());

                if(longchar_data) {
                    GetArgs().PutMsgExpected("CDB_Binary", "CDB_LongBinary");

                    BOOST_CHECK_EQUAL(longchar_data->Size(), size_t(32));
                    BOOST_CHECK_EQUAL(string(static_cast<const char*>(longchar_data->Value()),
                                longchar_data->Size()),
                            string("12345     ")
                            );
                }

                DumpResults(auto_stmt.get());
            }

            // long binary
            // dblib cannot transfer more than 255 bytes ...
            if (GetArgs().GetDriverName() != dblib_driver
                ) {
                rs = auto_stmt->ExecuteQuery("select convert(binary(1000), '12345')");
                BOOST_CHECK(rs != NULL);

                BOOST_CHECK(rs->Next());
                const CVariant& variant = rs->GetVariant(1);

                //
                CDB_Binary* char_data = NULL;
                char_data = dynamic_cast<CDB_Binary*>(variant.GetData());

                if(char_data) {
                    GetArgs().PutMsgExpected("CDB_LongBinary", "CDB_Binary");

                    BOOST_CHECK_EQUAL(string(static_cast<const char*>(char_data->Value()),
                                char_data->Size()),
                            string("12345")
                            );
                }

                //
                CDB_VarBinary* varchar_data = NULL;
                varchar_data = dynamic_cast<CDB_VarBinary*>(variant.GetData());

                if(varchar_data) {
                    GetArgs().PutMsgExpected("CDB_LongBinary", "CDB_VarBinary");

                    BOOST_CHECK_EQUAL(varchar_data->Size(), size_t(1000));
                    // BOOST_CHECK_EQUAL(string(static_cast<const char*>(varchar_data->Value()),
                    //             varchar_data->Size()),
                    //         string("12345")
                    //         );
                    BOOST_CHECK_EQUAL(
                        memcmp(
                            varchar_data->Value(),
                            "12345\0\0\0\0\0",
                            10
                            ),
                        0
                        );
                }

                //
                CDB_LongBinary* longchar_data = NULL;
                longchar_data = dynamic_cast<CDB_LongBinary*>(variant.GetData());

                if(longchar_data) {
                    BOOST_CHECK_EQUAL(longchar_data->Size(), size_t(1000));
                    // BOOST_CHECK_EQUAL(string(static_cast<const char*>(longchar_data->Value()),
                    //             longchar_data->Size()),
                    //         string("12345")
                    //         );
                    BOOST_CHECK_EQUAL(
                        memcmp(
                            longchar_data->Value(),
                            "12345\0\0\0\0\0",
                            10
                            ),
                        0
                        );
                }

                DumpResults(auto_stmt.get());
            } else {
                GetArgs().PutMsgDisabled("Test_Recordset: Second test - long binary");
            }

            // varbinary
            {
                rs = auto_stmt->ExecuteQuery("select convert(varbinary(10), '12345')");
                BOOST_CHECK(rs != NULL);

                BOOST_CHECK(rs->Next());
                const CVariant& variant = rs->GetVariant(1);

                //
                CDB_Binary* char_data = NULL;
                char_data = dynamic_cast<CDB_Binary*>(variant.GetData());

                if(char_data) {
                    GetArgs().PutMsgExpected("CDB_VarBinary", "CDB_Binary");

                    BOOST_CHECK_EQUAL(string(static_cast<const char*>(char_data->Value()),
                                char_data->Size()),
                            string("12345")
                            );
                }

                //
                CDB_VarBinary* data = dynamic_cast<CDB_VarBinary*>(variant.GetData());
                BOOST_CHECK(data);

                BOOST_CHECK_EQUAL(data->Size(), size_t(5));
                BOOST_CHECK_EQUAL(string(static_cast<const char*>(data->Value()),
                            data->Size()),
                        string("12345")
                        );

                //
                CDB_LongBinary* longchar_data = NULL;
                longchar_data = dynamic_cast<CDB_LongBinary*>(variant.GetData());

                if(longchar_data) {
                    GetArgs().PutMsgExpected("CDB_VarBinary", "CDB_LongBinary");

                    BOOST_CHECK_EQUAL(longchar_data->Size(), size_t(10));
                    BOOST_CHECK_EQUAL(string(static_cast<const char*>(longchar_data->Value()),
                                longchar_data->Size()),
                            string("12345")
                            );
                }

                DumpResults(auto_stmt.get());
            }

            // text
            if (GetArgs().GetDriverName() != dblib_driver) {
                rs = auto_stmt->ExecuteQuery("select convert(text, '12345')");
                BOOST_CHECK(rs != NULL);

                BOOST_CHECK(rs->Next());
                const CVariant& variant = rs->GetVariant(1);

                CDB_Text* data = dynamic_cast<CDB_Text*>(variant.GetData());
                BOOST_CHECK(data);

                // BOOST_CHECK_EQUAL(data->Value(), 1);

                DumpResults(auto_stmt.get());
            } else {
                GetArgs().PutMsgDisabled("Test_Recordset: Second test - text");
            }

            // image
            if (GetArgs().GetDriverName() != dblib_driver) {
                rs = auto_stmt->ExecuteQuery("select convert(image, '12345')");
                BOOST_CHECK(rs != NULL);

                BOOST_CHECK(rs->Next());
                const CVariant& variant = rs->GetVariant(1);

                CDB_Image* data = dynamic_cast<CDB_Image*>(variant.GetData());
                BOOST_CHECK(data);

                // BOOST_CHECK_EQUAL(data->Value(), 1);

                DumpResults(auto_stmt.get());
            } else {
                GetArgs().PutMsgDisabled("Test_Recordset: Second test - image");
            }
        }

    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}

///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(Test_ResultsetMetaData)
{
    try {
        auto_ptr<IStatement> auto_stmt(GetConnection().GetStatement());
        IResultSet* rs;
        const IResultSetMetaData* md = NULL;

        // First test ...
        // Check different data types ...
        // Check non-empty results ...
        {
            // bit
            {
                rs = auto_stmt->ExecuteQuery("select convert(bit, 1)");
                BOOST_CHECK(rs != NULL);
                md = rs->GetMetaData();
                BOOST_CHECK(md);

                EDB_Type curr_type = md->GetType(1);
                BOOST_CHECK_EQUAL(curr_type, eDB_Bit);
                BOOST_CHECK_EQUAL(md->GetMaxSize(1), 1);

                DumpResults(auto_stmt.get());
            }

            // tinyint
            {
                rs = auto_stmt->ExecuteQuery("select convert(tinyint, 1)");
                BOOST_CHECK(rs != NULL);
                md = rs->GetMetaData();
                BOOST_CHECK(md);

                EDB_Type curr_type = md->GetType(1);
                BOOST_CHECK_EQUAL(curr_type, eDB_TinyInt);
                if (GetArgs().GetDriverName() == odbc_driver) {
                    BOOST_CHECK_EQUAL(md->GetMaxSize(1), 3);
                } else {
                    BOOST_CHECK_EQUAL(md->GetMaxSize(1), 1);
                }
                DumpResults(auto_stmt.get());
            }

            // smallint
            {
                rs = auto_stmt->ExecuteQuery("select convert(smallint, 1)");
                BOOST_CHECK(rs != NULL);
                md = rs->GetMetaData();
                BOOST_CHECK(md);

                EDB_Type curr_type = md->GetType(1);
                BOOST_CHECK_EQUAL(curr_type, eDB_SmallInt);

                if (GetArgs().GetDriverName() == odbc_driver) {
                    BOOST_CHECK_EQUAL(md->GetMaxSize(1), 5);
                } else {
                    BOOST_CHECK_EQUAL(md->GetMaxSize(1), 2);
                }

                DumpResults(auto_stmt.get());
            }

            // int
            {
                rs = auto_stmt->ExecuteQuery("select convert(int, 1)");
                BOOST_CHECK(rs != NULL);
                md = rs->GetMetaData();
                BOOST_CHECK(md);

                EDB_Type curr_type = md->GetType(1);
                BOOST_CHECK_EQUAL(curr_type, eDB_Int);

                if (GetArgs().GetDriverName() == odbc_driver) {
                    BOOST_CHECK_EQUAL(md->GetMaxSize(1), 10);
                } else {
                    BOOST_CHECK_EQUAL(md->GetMaxSize(1), 4);
                }

                DumpResults(auto_stmt.get());
            }

            // numeric
            {
                //
                {
                    rs = auto_stmt->ExecuteQuery("select convert(numeric(38, 0), 1)");
                    BOOST_CHECK(rs != NULL);
                    md = rs->GetMetaData();
                    BOOST_CHECK(md);

                    EDB_Type curr_type = md->GetType(1);
                    BOOST_CHECK_EQUAL(curr_type, eDB_Numeric);

                    if (GetArgs().GetDriverName() == odbc_driver) {
                        BOOST_CHECK_EQUAL(md->GetMaxSize(1), 38);
                    } else {
                        BOOST_CHECK_EQUAL(md->GetMaxSize(1), 35);
                    }

                    DumpResults(auto_stmt.get());
                }

                //
                {
                    rs = auto_stmt->ExecuteQuery("select convert(numeric(18, 2), 2843113322)");
                    BOOST_CHECK(rs != NULL);
                    md = rs->GetMetaData();
                    BOOST_CHECK(md);

                    EDB_Type curr_type = md->GetType(1);
                    BOOST_CHECK_EQUAL(curr_type, eDB_Numeric);

                    if (GetArgs().GetDriverName() == odbc_driver) {
                        BOOST_CHECK_EQUAL(md->GetMaxSize(1), 18);
                    } else {
                        BOOST_CHECK_EQUAL(md->GetMaxSize(1), 35);
                    }

                    DumpResults(auto_stmt.get());
                }
            }

            // decimal
            // There is no eDB_Decimal ...
            if (false) {
                rs = auto_stmt->ExecuteQuery("select convert(decimal(38, 0), 1)");
                BOOST_CHECK(rs != NULL);
                md = rs->GetMetaData();
                BOOST_CHECK(md);

                EDB_Type curr_type = md->GetType(1);
                BOOST_CHECK_EQUAL(curr_type, eDB_Numeric);

                DumpResults(auto_stmt.get());
            }

            // float
            {
                rs = auto_stmt->ExecuteQuery("select convert(float(4), 1)");
                BOOST_CHECK(rs != NULL);
                md = rs->GetMetaData();
                BOOST_CHECK(md);

                EDB_Type curr_type = md->GetType(1);
                BOOST_CHECK_EQUAL(curr_type, eDB_Float);

                if (GetArgs().GetDriverName() == odbc_driver) {
                    BOOST_CHECK_EQUAL(md->GetMaxSize(1), 24);
                } else {
                    BOOST_CHECK_EQUAL(md->GetMaxSize(1), 4);
                }

                DumpResults(auto_stmt.get());
            }

            // double
            {
                rs = auto_stmt->ExecuteQuery("select convert(double precision, 1)");
                BOOST_CHECK(rs != NULL);
                md = rs->GetMetaData();
                BOOST_CHECK(md);

                EDB_Type curr_type = md->GetType(1);
                BOOST_CHECK_EQUAL(curr_type, eDB_Double);

                if (GetArgs().GetDriverName() == odbc_driver) {
                    BOOST_CHECK_EQUAL(md->GetMaxSize(1), 53);
                } else {
                    BOOST_CHECK_EQUAL(md->GetMaxSize(1), 8);
                }

                DumpResults(auto_stmt.get());
            }

            // real
            {
                rs = auto_stmt->ExecuteQuery("select convert(real, 1)");
                BOOST_CHECK(rs != NULL);
                md = rs->GetMetaData();
                BOOST_CHECK(md);

                EDB_Type curr_type = md->GetType(1);
                BOOST_CHECK_EQUAL(curr_type, eDB_Float);

                if (GetArgs().GetDriverName() == odbc_driver) {
                    BOOST_CHECK_EQUAL(md->GetMaxSize(1), 24);
                } else {
                    BOOST_CHECK_EQUAL(md->GetMaxSize(1), 4);
                }

                DumpResults(auto_stmt.get());
            }

            // smallmoney
            // Unsupported type ...
            if (false) {
                rs = auto_stmt->ExecuteQuery("select convert(smallmoney, 1)");
                BOOST_CHECK(rs != NULL);
                md = rs->GetMetaData();
                BOOST_CHECK(md);

                EDB_Type curr_type = md->GetType(1);
                BOOST_CHECK_EQUAL(curr_type, eDB_Double);
                BOOST_CHECK_EQUAL(md->GetMaxSize(1), 2);

                DumpResults(auto_stmt.get());
            }

            // money
            // Unsupported type ...
            if (false) {
                rs = auto_stmt->ExecuteQuery("select convert(money, 1)");
                BOOST_CHECK(rs != NULL);
                md = rs->GetMetaData();
                BOOST_CHECK(md);

                EDB_Type curr_type = md->GetType(1);
                BOOST_CHECK_EQUAL(curr_type, eDB_Double);
                BOOST_CHECK_EQUAL(md->GetMaxSize(1), 2);

                DumpResults(auto_stmt.get());
            }

            // smalldatetime
            {
                rs = auto_stmt->ExecuteQuery("select convert(smalldatetime, 'January 1, 1900')");
                BOOST_CHECK(rs != NULL);
                md = rs->GetMetaData();
                BOOST_CHECK(md);

                EDB_Type curr_type = md->GetType(1);
                BOOST_CHECK_EQUAL(curr_type, eDB_SmallDateTime);

                if (GetArgs().GetDriverName() == odbc_driver) {
                    BOOST_CHECK_EQUAL(md->GetMaxSize(1), 16);
                } else {
                    BOOST_CHECK_EQUAL(md->GetMaxSize(1), 4);
                }

                DumpResults(auto_stmt.get());
            }

            // datetime
            {
                rs = auto_stmt->ExecuteQuery("select convert(datetime, 'January 1, 1753')");
                BOOST_CHECK(rs != NULL);
                md = rs->GetMetaData();
                BOOST_CHECK(md);

                EDB_Type curr_type = md->GetType(1);
                BOOST_CHECK_EQUAL(curr_type, eDB_DateTime);

                if (GetArgs().GetDriverName() == odbc_driver) {
                    BOOST_CHECK_EQUAL(md->GetMaxSize(1), 23);
                } else {
                    BOOST_CHECK_EQUAL(md->GetMaxSize(1), 8);
                }

                DumpResults(auto_stmt.get());
            }

            // char
            {
                rs = auto_stmt->ExecuteQuery("select convert(char(32), '12345')");
                BOOST_CHECK(rs != NULL);
                md = rs->GetMetaData();
                BOOST_CHECK(md);

                EDB_Type curr_type = md->GetType(1);
                if (GetArgs().GetDriverName() == odbc_driver) {
                    BOOST_CHECK_EQUAL(curr_type, eDB_Char);
                } else {
                    BOOST_CHECK_EQUAL(curr_type, eDB_VarChar);
                }

                // BOOST_CHECK_EQUAL(md->GetMaxSize(1), 32);

                DumpResults(auto_stmt.get());
            }

            // long char
            {
                rs = auto_stmt->ExecuteQuery("select convert(char(8000), '12345')");
                BOOST_CHECK(rs != NULL);
                md = rs->GetMetaData();
                BOOST_CHECK(md);

                EDB_Type curr_type = md->GetType(1);
                if (GetArgs().GetDriverName() == ftds_driver ||
                    GetArgs().GetDriverName() == dblib_driver ||
                    GetArgs().GetDriverName() == ctlib_driver
                    ) {
                    BOOST_CHECK_EQUAL(curr_type, eDB_VarChar);
                } else if (GetArgs().GetDriverName() == odbc_driver) {
                    BOOST_CHECK_EQUAL(curr_type, eDB_LongChar);
                } else {
                    BOOST_CHECK_EQUAL(curr_type, eDB_Char);
                }

                if (GetArgs().GetDriverName() == dblib_driver) {
                    BOOST_CHECK_EQUAL(md->GetMaxSize(1), 255);
                } else {
                    // BOOST_CHECK_EQUAL(md->GetMaxSize(1), 8000);
                }

                DumpResults(auto_stmt.get());
            }

            // varchar
            {
                rs = auto_stmt->ExecuteQuery("select convert(varchar(32), '12345')");
                BOOST_CHECK(rs != NULL);
                md = rs->GetMetaData();
                BOOST_CHECK(md);

                EDB_Type curr_type = md->GetType(1);
                BOOST_CHECK_EQUAL(curr_type, eDB_VarChar);
                // BOOST_CHECK_EQUAL(md->GetMaxSize(1), 32);

                DumpResults(auto_stmt.get());
            }

            // nchar
            {
                rs = auto_stmt->ExecuteQuery("select convert(nchar(32), '12345')");
                BOOST_CHECK(rs != NULL);
                md = rs->GetMetaData();
                BOOST_CHECK(md);

                EDB_Type curr_type = md->GetType(1);
                if (GetArgs().GetDriverName() == odbc_driver) {
                    BOOST_CHECK_EQUAL(curr_type, eDB_Char);
                } else {
                    BOOST_CHECK_EQUAL(curr_type, eDB_VarChar);
                }
                // BOOST_CHECK_EQUAL(md->GetMaxSize(1), 32);

                DumpResults(auto_stmt.get());
            }

            // nvarchar
            {
                rs = auto_stmt->ExecuteQuery("select convert(nvarchar(32), '12345')");
                BOOST_CHECK(rs != NULL);
                md = rs->GetMetaData();
                BOOST_CHECK(md);

                EDB_Type curr_type = md->GetType(1);
                BOOST_CHECK_EQUAL(curr_type, eDB_VarChar);
                // BOOST_CHECK_EQUAL(md->GetMaxSize(1), 32);

                DumpResults(auto_stmt.get());
            }

            // binary
            {
                rs = auto_stmt->ExecuteQuery("select convert(binary(32), '12345')");
                BOOST_CHECK(rs != NULL);
                md = rs->GetMetaData();
                BOOST_CHECK(md);

                EDB_Type curr_type = md->GetType(1);
                if (GetArgs().GetDriverName() == odbc_driver) {
                    BOOST_CHECK_EQUAL(curr_type, eDB_Binary);
                } else {
                    BOOST_CHECK_EQUAL(curr_type, eDB_VarBinary);
                }
                BOOST_CHECK_EQUAL(md->GetMaxSize(1), 32);

                DumpResults(auto_stmt.get());
            }

            // long binary
            {
                rs = auto_stmt->ExecuteQuery("select convert(binary(8000), '12345')");
                BOOST_CHECK(rs != NULL);
                md = rs->GetMetaData();
                BOOST_CHECK(md);

                EDB_Type curr_type = md->GetType(1);
                if ((GetArgs().GetDriverName() == ftds_driver &&
                        GetArgs().GetServerType() == CDBConnParams::eMSSqlServer) ||
                    GetArgs().GetDriverName() == dblib_driver ||
                    (GetArgs().GetDriverName() == ctlib_driver &&
                        NStr::CompareNocase(GetSybaseClientVersion(), 0, 4, "12.0") == 0)
                    ) {
                    BOOST_CHECK_EQUAL(curr_type, eDB_VarBinary);
                } else if ((GetArgs().GetDriverName() == ftds_driver &&
                        GetArgs().GetServerType() == CDBConnParams::eSybaseSQLServer) ||
                    GetArgs().GetDriverName() == ctlib_driver ||
                    GetArgs().GetDriverName() == odbc_driver
                    ) {
                    BOOST_CHECK_EQUAL(curr_type, eDB_LongBinary);
                } else {
                    BOOST_CHECK_EQUAL(curr_type, eDB_Binary);
                }

                if (GetArgs().GetDriverName() == dblib_driver ||
                    (GetArgs().GetDriverName() == ctlib_driver &&
                        NStr::CompareNocase(GetSybaseClientVersion(), 0, 4, "12.0") == 0)
                    ) {
                    BOOST_CHECK_EQUAL(md->GetMaxSize(1), 255);
                } else {
                    BOOST_CHECK_EQUAL(md->GetMaxSize(1), 8000);
                }

                DumpResults(auto_stmt.get());
            }

            // varbinary
            {
                rs = auto_stmt->ExecuteQuery("select convert(varbinary(32), '12345')");
                BOOST_CHECK(rs != NULL);
                md = rs->GetMetaData();
                BOOST_CHECK(md);

                EDB_Type curr_type = md->GetType(1);
                BOOST_CHECK_EQUAL(curr_type, eDB_VarBinary);
                BOOST_CHECK_EQUAL(md->GetMaxSize(1), 32);

                DumpResults(auto_stmt.get());
            }

            // Explicitly set max LOB size ...
            {
                I_DriverContext* drv_context = GetDS().GetDriverContext();

                if ( drv_context == NULL ) {
                    BOOST_FAIL("FATAL: Unable to load context for dbdriver " +
                            GetArgs().GetDriverName());
                }

                drv_context->SetMaxTextImageSize( 0x7fffffff );
            }

            // Set text size manually ...
            // SetMaxTextImageSize() method doesn't work in case of ctlib
            // driver.
            {
                rs = auto_stmt->ExecuteQuery("set textsize 2147483647");
                DumpResults(auto_stmt.get());
            }

            // text
            {
                rs = auto_stmt->ExecuteQuery("select convert(text, '12345')");
                BOOST_CHECK(rs != NULL);
                md = rs->GetMetaData();
                BOOST_CHECK(md);

                EDB_Type curr_type = md->GetType(1);
                BOOST_CHECK_EQUAL(curr_type, eDB_Text);
                BOOST_CHECK_EQUAL(md->GetMaxSize(1), 2147483647);

                DumpResults(auto_stmt.get());
            }

            // image
            {
                rs = auto_stmt->ExecuteQuery("select convert(image, '12345')");
                BOOST_CHECK(rs != NULL);
                md = rs->GetMetaData();
                BOOST_CHECK(md);

                EDB_Type curr_type = md->GetType(1);
                BOOST_CHECK_EQUAL(curr_type, eDB_Image);
                BOOST_CHECK_EQUAL(md->GetMaxSize(1), 2147483647);

                DumpResults(auto_stmt.get());
            }
        }

        // Second test ...
        {
        }
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}

///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(Test_StmtMetaData)
{
    try {
        auto_ptr<ICallableStatement> auto_stmt;
        unsigned int col_num = 0;

        if (false) {
            auto_ptr<IStatement> auto_stmt01(GetConnection().GetStatement());
            auto_stmt01->ExecuteUpdate("USE DBAPI_Sample");
        }

        //////////////////////////////////////////////////////////////////////
        {
            auto_stmt.reset(
                    GetConnection().GetCallableStatement("sp_columns")
                    );

            const IResultSetMetaData& mi = auto_stmt->GetParamsMetaData();

            col_num = mi.GetTotalColumns();
            if (GetArgs().GetServerType() == CDBConnParams::eSybaseSQLServer) {
                BOOST_CHECK_EQUAL(col_num, 5U);
            } else {
                BOOST_CHECK_EQUAL(col_num, 6U);
            }
        }

        //////////////////////////////////////////////////////////////////////
        {
            auto_stmt.reset(
                    GetConnection().GetCallableStatement(".sp_columns")
                    );

            const IResultSetMetaData& mi = auto_stmt->GetParamsMetaData();

            col_num = mi.GetTotalColumns();
            if (GetArgs().GetServerType() == CDBConnParams::eSybaseSQLServer) {
                BOOST_CHECK_EQUAL(col_num, 5U);
            } else {
                BOOST_CHECK_EQUAL(col_num, 6U);
            }
        }

        //////////////////////////////////////////////////////////////////////
        {
            auto_stmt.reset(
                    GetConnection().GetCallableStatement("..sp_columns")
                    );

            const IResultSetMetaData& mi = auto_stmt->GetParamsMetaData();

            col_num = mi.GetTotalColumns();
            if (GetArgs().GetServerType() == CDBConnParams::eSybaseSQLServer) {
                BOOST_CHECK_EQUAL(col_num, 5U);
            } else {
                BOOST_CHECK_EQUAL(col_num, 6U);
            }
        }

        //////////////////////////////////////////////////////////////////////
        {
            auto_stmt.reset(
                    GetConnection().GetCallableStatement("sp_sproc_columns")
                    );

            const IResultSetMetaData& mi = auto_stmt->GetParamsMetaData();

            col_num = mi.GetTotalColumns();
            switch (GetArgs().GetServerType()) {
                case CDBConnParams::eSybaseSQLServer:
                    BOOST_CHECK_EQUAL(col_num, 5U);
                    break;
                case CDBConnParams::eMSSqlServer:
                    BOOST_CHECK_EQUAL(col_num, 7U);
                    break;
                default:
                    break;
            }
        }

        //////////////////////////////////////////////////////////////////////
        {
            auto_stmt.reset(
                    GetConnection().GetCallableStatement("sp_server_info")
                    );

            const IResultSetMetaData& mi = auto_stmt->GetParamsMetaData();

            col_num = mi.GetTotalColumns();
            BOOST_CHECK_EQUAL(col_num, 2U);
        }

        //////////////////////////////////////////////////////////////////////
        {
            auto_stmt.reset(
                    GetConnection().GetCallableStatement("sp_tables")
                    );

            const IResultSetMetaData& mi = auto_stmt->GetParamsMetaData();

            col_num = mi.GetTotalColumns();
            if (GetArgs().GetServerType() == CDBConnParams::eMSSqlServer) {
                BOOST_CHECK_EQUAL(col_num, 6U);
            } else {
                BOOST_CHECK_EQUAL(col_num, 5U);
            }
        }

        //////////////////////////////////////////////////////////////////////
        {
            auto_stmt.reset(
                    GetConnection().GetCallableStatement("sp_stored_procedures")
                    );

            const IResultSetMetaData& mi = auto_stmt->GetParamsMetaData();

            col_num = mi.GetTotalColumns();
            if (GetArgs().GetServerType() == CDBConnParams::eMSSqlServer) {
                BOOST_CHECK_EQUAL(col_num, 5U);
            } else {
                BOOST_CHECK_EQUAL(col_num, 4U);
            }
        }

        //////////////////////////////////////////////////////////////////////
        {
            if (GetArgs().GetServerType() == CDBConnParams::eSybaseSQLServer) {
        auto_stmt.reset(
            GetConnection().GetCallableStatement("sybsystemprocs..sp_stored_procedures")
            );
            } else {
        auto_stmt.reset(
            GetConnection().GetCallableStatement("master..sp_stored_procedures")
            );
            }

            const IResultSetMetaData& mi = auto_stmt->GetParamsMetaData();

            col_num = mi.GetTotalColumns();
            if (GetArgs().GetServerType() == CDBConnParams::eMSSqlServer) {
                BOOST_CHECK_EQUAL(col_num, 5U);
            } else {
                BOOST_CHECK_EQUAL(col_num, 4U);
            }
        }

        //////////////////////////////////////////////////////////////////////
        {
            if (GetArgs().GetServerType() == CDBConnParams::eSybaseSQLServer) {
        auto_stmt.reset(
            GetConnection().GetCallableStatement("sybsystemprocs.dbo.sp_stored_procedures")
            );
            } else {
        auto_stmt.reset(
            GetConnection().GetCallableStatement("master.dbo.sp_stored_procedures")
            );
            }

            const IResultSetMetaData& mi = auto_stmt->GetParamsMetaData();

            col_num = mi.GetTotalColumns();
            if (GetArgs().GetServerType() == CDBConnParams::eMSSqlServer) {
                BOOST_CHECK_EQUAL(col_num, 5U);
            } else {
                BOOST_CHECK_EQUAL(col_num, 4U);
            }
        }

    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}

////////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(Test_NULL)
{
    const string table_name = GetTableName();
    // const string table_name("DBAPI_Sample..dbapi_unit_test");

    enum {rec_num = 10};
    string sql;

    try {
        // Initialize data (strings are NOT empty) ...
        {
            auto_ptr<IStatement> auto_stmt( GetConnection().GetStatement() );

            {
                // Drop all records ...
                sql  = " DELETE FROM " + table_name;
                auto_stmt->ExecuteUpdate(sql);

                sql  = " INSERT INTO " + table_name +
                    "(int_field, vc1000_field) "
                    "VALUES(@int_field, @vc1000_field) \n";

                // CVariant variant(eDB_Text);
                // variant.Append(" ", 1);

                // Insert data ...
                for (long ind = 0; ind < rec_num; ++ind) {
                    if (ind % 2 == 0) {
                        auto_stmt->SetParam( CVariant( Int4(ind) ), "@int_field" );
                        auto_stmt->SetParam(CVariant(eDB_VarChar), "@vc1000_field");
                    } else {
                        auto_stmt->SetParam( CVariant(eDB_Int), "@int_field" );
                        auto_stmt->SetParam( CVariant(NStr::NumericToString(ind)),
                                             "@vc1000_field"
                                             );
                    }

                    // Execute a statement with parameters ...
                    auto_stmt->ExecuteUpdate( sql );
                }

                // Check record number ...
                BOOST_CHECK_EQUAL(size_t(rec_num),
                                  GetNumOfRecords(auto_stmt, table_name)
                                  );
            }

            {
                // !!! Do not forget to clear a parameter list ....
                // Workaround for the ctlib driver ...
                auto_stmt->ClearParamList();

                // Drop all records ...
                sql  = " DELETE FROM #test_unicode_table";
                auto_stmt->ExecuteUpdate(sql);

                sql  = " INSERT INTO #test_unicode_table"
                    "(nvc255_field) VALUES(@nvc255_field)";

                // Insert data ...
                for (long ind = 0; ind < rec_num; ++ind) {
                    if (ind % 2 == 0) {
                        auto_stmt->SetParam(CVariant(eDB_VarChar), "@nvc255_field");
                    } else {
                        auto_stmt->SetParam( CVariant(NStr::NumericToString(ind)),
                                             "@nvc255_field");
                    }

                    // Execute a statement with parameters ...
                    auto_stmt->ExecuteUpdate( sql );
                }

                // Check record number ...
                BOOST_CHECK_EQUAL(size_t(rec_num),
                                  GetNumOfRecords(auto_stmt, "#test_unicode_table"));
            }
        }

        // Check ...
        if (true) {
            auto_ptr<IStatement> auto_stmt( GetConnection().GetStatement() );

            {
                sql = "SELECT int_field, vc1000_field FROM " + table_name +
                    " ORDER BY id";

                auto_stmt->SendSql( sql );
                BOOST_CHECK(auto_stmt->HasMoreResults());
                BOOST_CHECK(auto_stmt->HasRows());
                auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());
                BOOST_CHECK(rs.get() != NULL);

                for (long ind = 0; ind < rec_num; ++ind) {
                    BOOST_CHECK(rs->Next());

                    const CVariant& int_field = rs->GetVariant(1);
                    const CVariant& vc1000_field = rs->GetVariant(2);

                    if (ind % 2 == 0) {
                        BOOST_CHECK( !int_field.IsNull() );
                        BOOST_CHECK_EQUAL( int_field.GetInt4(), ind );

                        BOOST_CHECK( vc1000_field.IsNull() );
                    } else {
                        BOOST_CHECK( int_field.IsNull() );

                        BOOST_CHECK( !vc1000_field.IsNull() );
                        BOOST_CHECK_EQUAL( vc1000_field.GetString(),
                                           NStr::NumericToString(ind) );
                    }
                }

                DumpResults(auto_stmt.get());
            }

            {
                sql = "SELECT nvc255_field FROM #test_unicode_table ORDER BY id";

                auto_stmt->SendSql( sql );
                BOOST_CHECK(auto_stmt->HasMoreResults());
                BOOST_CHECK(auto_stmt->HasRows());
                auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());
                BOOST_CHECK(rs.get() != NULL);

                for (long ind = 0; ind < rec_num; ++ind) {
                    BOOST_CHECK(rs->Next());

                    const CVariant& nvc255_field = rs->GetVariant(1);

                    if (ind % 2 == 0) {
                        BOOST_CHECK( nvc255_field.IsNull() );
                    } else {
                        BOOST_CHECK( !nvc255_field.IsNull() );
                        BOOST_CHECK_EQUAL( NStr::TruncateSpaces(
                                                nvc255_field.GetString()
                                                ),
                                           NStr::NumericToString(ind) );
                    }
                }

                DumpResults(auto_stmt.get());
            }
        }

        // Check NULL with stored procedures ...
#ifdef NCBI_OS_SOLARIS
        // On Solaris GetNumOfRecords gives an error with MS SQL database
        if (GetArgs().GetDriverName() != dblib_driver
            ||  GetArgs().GetServerType() == CDBConnParams::eSybaseSQLServer)
#endif
        {
            {
                auto_ptr<ICallableStatement> auto_stmt(
                    GetConnection().GetCallableStatement("sp_server_info")
                    );

                // Set parameter to NULL ...
                auto_stmt->SetParam( CVariant(eDB_Int), "@attribute_id" );
                auto_stmt->Execute();

                if (GetArgs().GetServerType() == CDBConnParams::eSybaseSQLServer) {
                    BOOST_CHECK_EQUAL( size_t(30), GetNumOfRecords(auto_stmt) );
                } else {
                    BOOST_CHECK_EQUAL( size_t(29), GetNumOfRecords(auto_stmt) );
                }

                // Set parameter to 1 ...
                auto_stmt->SetParam( CVariant( Int4(1) ), "@attribute_id" );
                auto_stmt->Execute();

                BOOST_CHECK_EQUAL( size_t(1), GetNumOfRecords(auto_stmt) );
            }

            {
            }
        }


        // Special case: empty strings and strings with spaces.
        {
            auto_ptr<IStatement> auto_stmt( GetConnection().GetStatement() );

            // Initialize data (strings are EMPTY) ...
            {
                // Drop all records ...
                sql  = " DELETE FROM " + table_name;
                auto_stmt->ExecuteUpdate(sql);

                sql  = " INSERT INTO " + table_name +
                    "(int_field, vc1000_field) "
                    "VALUES(@int_field, @vc1000_field) \n";

                // CVariant variant(eDB_Text);
                // variant.Append(" ", 1);

                // Insert data ...
                for (long ind = 0; ind < rec_num; ++ind) {
                    if (ind % 2 == 0) {
                        auto_stmt->SetParam( CVariant( Int4(ind) ), "@int_field" );
                        auto_stmt->SetParam(CVariant(eDB_VarChar), "@vc1000_field");
                    } else {
                        auto_stmt->SetParam( CVariant(eDB_Int), "@int_field" );
                        auto_stmt->SetParam( CVariant(string()), "@vc1000_field" );
                    }

                    // Execute a statement with parameters ...
                    auto_stmt->ExecuteUpdate( sql );
                }

                // Check record number ...
                BOOST_CHECK_EQUAL(size_t(rec_num), GetNumOfRecords(auto_stmt,
                                                                table_name));
            }

            // Check ...
            {
                sql = "SELECT int_field, vc1000_field FROM " + table_name +
                    " ORDER BY id";

                auto_stmt->SendSql( sql );
                BOOST_CHECK(auto_stmt->HasMoreResults());
                BOOST_CHECK(auto_stmt->HasRows());
                auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());
                BOOST_CHECK(rs.get() != NULL);

                for (long ind = 0; ind < rec_num; ++ind) {
                    BOOST_CHECK(rs->Next());

                    const CVariant& int_field = rs->GetVariant(1);
                    const CVariant& vc1000_field = rs->GetVariant(2);

                    if (ind % 2 == 0) {
                        BOOST_CHECK( !int_field.IsNull() );
                        BOOST_CHECK_EQUAL( int_field.GetInt4(), ind );

                        BOOST_CHECK( vc1000_field.IsNull() );
                    } else {
                        BOOST_CHECK( int_field.IsNull() );

#ifdef NCBI_OS_SOLARIS
                        // Another version of Sybase client is used on Solaris,
                        // ctlib driver works there differently
                        if (GetArgs().GetDriverName() == ctlib_driver) {
                            const string sybase_version = GetSybaseClientVersion();
                            if (NStr::CompareNocase(sybase_version, 0, 4, "12.0") == 0) {
                                BOOST_CHECK( vc1000_field.IsNull() );
                                continue;
                            }
                        }
#endif

                        BOOST_CHECK( !vc1000_field.IsNull() );
                        // Old protocol version has this strange feature
                        if (GetArgs().GetServerType() == CDBConnParams::eSybaseSQLServer
                            || GetArgs().GetDriverName() == dblib_driver
                           )
                        {
                            BOOST_CHECK_EQUAL( vc1000_field.GetString(), string(" ") );
                        }
                        else {
                            BOOST_CHECK_EQUAL( vc1000_field.GetString(), string() );
                        }
                    }
                }

                DumpResults(auto_stmt.get());
            }

            // Initialize data (strings are full of spaces) ...
            {
                // Drop all records ...
                sql  = " DELETE FROM " + GetTableName();
                auto_stmt->ExecuteUpdate(sql);

                sql  = " INSERT INTO " + GetTableName() +
                    "(int_field, vc1000_field) "
                    "VALUES(@int_field, @vc1000_field) \n";

                // CVariant variant(eDB_Text);
                // variant.Append(" ", 1);

                // !!! Do not forget to clear a parameter list ....
                // Workaround for the ctlib driver ...
                auto_stmt->ClearParamList();

                // Insert data ...
                for (long ind = 0; ind < rec_num; ++ind) {
                    if (ind % 2 == 0) {
                        auto_stmt->SetParam( CVariant( Int4(ind) ), "@int_field" );
                        auto_stmt->SetParam(CVariant(eDB_VarChar), "@vc1000_field");
                    } else {
                        auto_stmt->SetParam( CVariant(eDB_Int), "@int_field" );
                        auto_stmt->SetParam( CVariant(string("    ")), "@vc1000_field" );
                    }

                    // Execute a statement with parameters ...
                    auto_stmt->ExecuteUpdate( sql );
                }

                // Check record number ...
                BOOST_CHECK_EQUAL(size_t(rec_num), GetNumOfRecords(auto_stmt,
                                                                GetTableName()));
            }

            // Check ...
            {
                sql = "SELECT int_field, vc1000_field FROM " + GetTableName() +
                    " ORDER BY id";

                auto_stmt->SendSql( sql );
                BOOST_CHECK(auto_stmt->HasMoreResults());
                BOOST_CHECK(auto_stmt->HasRows());
                auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());
                BOOST_CHECK(rs.get() != NULL);

                for (long ind = 0; ind < rec_num; ++ind) {
                    BOOST_CHECK(rs->Next());

                    const CVariant& int_field = rs->GetVariant(1);
                    const CVariant& vc1000_field = rs->GetVariant(2);

                    if (ind % 2 == 0) {
                        BOOST_CHECK( !int_field.IsNull() );
                        BOOST_CHECK_EQUAL( int_field.GetInt4(), ind );

                        BOOST_CHECK( vc1000_field.IsNull() );
                    } else {
                        BOOST_CHECK( int_field.IsNull() );

                        BOOST_CHECK( !vc1000_field.IsNull() );
                        // Old protocol version has this strange feature
                        if (GetArgs().GetServerType() == CDBConnParams::eSybaseSQLServer
                            || GetArgs().GetDriverName() == dblib_driver
                           )
                        {
                            BOOST_CHECK_EQUAL( vc1000_field.GetString(), string(" ") );
                        }
                        else {
                            BOOST_CHECK_EQUAL( vc1000_field.GetString(), string("    ") );
                        }
                    }
                }

                DumpResults(auto_stmt.get());
            }
        }
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}

////////////////////////////////////////////////////////////////////////////////
static void
s_CheckGetRowCount(
    int row_count,
    ETransBehavior tb = eNoTrans,
    IStatement* stmt = NULL
    )
{
    // Transaction ...
    CTestTransaction transaction(GetConnection(), tb);
    string sql;
    sql  = " INSERT INTO " + GetTableName() + "(int_field) VALUES( 1 ) \n";

    // Insert row_count records into the table ...
    for ( int i = 0; i < row_count; ++i ) {
        IStatement* curr_stmt = NULL;
        auto_ptr<IStatement> auto_stmt;
        if ( !stmt ) {
            auto_stmt.reset( GetConnection().GetStatement() );
            curr_stmt = auto_stmt.get();
        } else {
            curr_stmt = stmt;
        }

        curr_stmt->ExecuteUpdate(sql);
        int nRows = curr_stmt->GetRowCount();
        BOOST_CHECK_EQUAL( nRows, 1 );
    }

    // Check a SELECT statement
    {
        IStatement* curr_stmt = NULL;
        auto_ptr<IStatement> auto_stmt;
        if ( !stmt ) {
            auto_stmt.reset( GetConnection().GetStatement() );
            curr_stmt = auto_stmt.get();
        } else {
            curr_stmt = stmt;
        }

        sql  = " SELECT * FROM " + GetTableName();
        curr_stmt->ExecuteUpdate(sql);

        int nRows = curr_stmt->GetRowCount();

        BOOST_CHECK_EQUAL( row_count, nRows );
    }

    // Check an UPDATE statement
    {
        IStatement* curr_stmt = NULL;
        auto_ptr<IStatement> auto_stmt;
        if ( !stmt ) {
            auto_stmt.reset( GetConnection().GetStatement() );
            curr_stmt = auto_stmt.get();
        } else {
            curr_stmt = stmt;
        }

        sql  = " UPDATE " + GetTableName() + " SET int_field = 0 ";
        curr_stmt->ExecuteUpdate(sql);

        int nRows = curr_stmt->GetRowCount();

        BOOST_CHECK_EQUAL( row_count, nRows );
    }

    // Check a SELECT statement again
    {
        IStatement* curr_stmt = NULL;
        auto_ptr<IStatement> auto_stmt;
        if ( !stmt ) {
            auto_stmt.reset( GetConnection().GetStatement() );
            curr_stmt = auto_stmt.get();
        } else {
            curr_stmt = stmt;
        }

        sql  = " SELECT * FROM " + GetTableName() + " WHERE int_field = 0";
        curr_stmt->ExecuteUpdate(sql);

        int nRows = curr_stmt->GetRowCount();

        BOOST_CHECK_EQUAL( row_count, nRows );
    }

    // Check a DELETE statement
    {
        IStatement* curr_stmt = NULL;
        auto_ptr<IStatement> auto_stmt;
        if ( !stmt ) {
            auto_stmt.reset( GetConnection().GetStatement() );
            curr_stmt = auto_stmt.get();
        } else {
            curr_stmt = stmt;
        }

        sql  = " DELETE FROM " + GetTableName();
        curr_stmt->ExecuteUpdate(sql);

        int nRows = curr_stmt->GetRowCount();

        BOOST_CHECK_EQUAL( row_count, nRows );
    }

    // Check a SELECT statement again and again ...
    {
        IStatement* curr_stmt = NULL;
        auto_ptr<IStatement> auto_stmt;
        if ( !stmt ) {
            auto_stmt.reset( GetConnection().GetStatement() );
            curr_stmt = auto_stmt.get();
        } else {
            curr_stmt = stmt;
        }

        sql  = " SELECT * FROM " + GetTableName();
        curr_stmt->ExecuteUpdate(sql);

        int nRows = curr_stmt->GetRowCount();

        BOOST_CHECK_EQUAL( 0, nRows );
    }

}

////////////////////////////////////////////////////////////////////////////////
static void
s_CheckGetRowCount2(
    int row_count,
    ETransBehavior tb = eNoTrans,
    IStatement* stmt = NULL
    )
{
    // Transaction ...
    CTestTransaction transaction(GetConnection(), tb);
    // auto_ptr<IStatement> stmt( GetConnection().CreateStatement() );
    // _ASSERT(curr_stmt->get());
    string sql;
    sql  = " INSERT INTO " + GetTableName() + "(int_field) VALUES( @value ) \n";

    // Insert row_count records into the table ...
    for ( Int4 i = 0; i < row_count; ++i ) {
        IStatement* curr_stmt = NULL;
        auto_ptr<IStatement> auto_stmt;
        if ( !stmt ) {
            auto_stmt.reset( GetConnection().GetStatement() );
            curr_stmt = auto_stmt.get();
        } else {
            curr_stmt = stmt;
        }

        curr_stmt->SetParam(CVariant(i), "@value");
        curr_stmt->ExecuteUpdate(sql);

        int nRows = curr_stmt->GetRowCount();
        BOOST_CHECK_EQUAL( nRows, 1 );

        int nRows2 = curr_stmt->GetRowCount();
        BOOST_CHECK_EQUAL( nRows2, 1 );
    }

    // Workaround for the CTLIB driver ...
    {
        IStatement* curr_stmt = NULL;
        auto_ptr<IStatement> auto_stmt;
        if ( !stmt ) {
            auto_stmt.reset( GetConnection().GetStatement() );
            curr_stmt = auto_stmt.get();
        } else {
            curr_stmt = stmt;
        }

        curr_stmt->ClearParamList();
    }

    // Check a SELECT statement
    {
        IStatement* curr_stmt = NULL;
        auto_ptr<IStatement> auto_stmt;
        if ( !stmt ) {
            auto_stmt.reset( GetConnection().GetStatement() );
            curr_stmt = auto_stmt.get();
        } else {
            curr_stmt = stmt;
        }

        sql  = " SELECT int_field FROM " + GetTableName() +
            " ORDER BY int_field";
        curr_stmt->ExecuteUpdate(sql);

        int nRows = curr_stmt->GetRowCount();
        BOOST_CHECK_EQUAL( row_count, nRows );

        int nRows2 = curr_stmt->GetRowCount();
        BOOST_CHECK_EQUAL( row_count, nRows2 );
    }

    // Check an UPDATE statement
    {
        IStatement* curr_stmt = NULL;
        auto_ptr<IStatement> auto_stmt;
        if ( !stmt ) {
            auto_stmt.reset( GetConnection().GetStatement() );
            curr_stmt = auto_stmt.get();
        } else {
            curr_stmt = stmt;
        }

        sql  = " UPDATE " + GetTableName() + " SET int_field = 0 ";
        curr_stmt->ExecuteUpdate(sql);

        int nRows = curr_stmt->GetRowCount();
        BOOST_CHECK_EQUAL( row_count, nRows );

        int nRows2 = curr_stmt->GetRowCount();
        BOOST_CHECK_EQUAL( row_count, nRows2 );
    }

    // Check a SELECT statement again
    {
        IStatement* curr_stmt = NULL;
        auto_ptr<IStatement> auto_stmt;
        if ( !stmt ) {
            auto_stmt.reset( GetConnection().GetStatement() );
            curr_stmt = auto_stmt.get();
        } else {
            curr_stmt = stmt;
        }

        sql  = " SELECT int_field FROM " + GetTableName() +
            " WHERE int_field = 0";
        curr_stmt->ExecuteUpdate(sql);

        int nRows = curr_stmt->GetRowCount();
        BOOST_CHECK_EQUAL( row_count, nRows );

        int nRows2 = curr_stmt->GetRowCount();
        BOOST_CHECK_EQUAL( row_count, nRows2 );
    }

    // Check a DELETE statement
    {
        IStatement* curr_stmt = NULL;
        auto_ptr<IStatement> auto_stmt;
        if ( !stmt ) {
            auto_stmt.reset( GetConnection().GetStatement() );
            curr_stmt = auto_stmt.get();
        } else {
            curr_stmt = stmt;
        }

        sql  = " DELETE FROM " + GetTableName();
        curr_stmt->ExecuteUpdate(sql);

        int nRows = curr_stmt->GetRowCount();
        BOOST_CHECK_EQUAL( row_count, nRows );

        int nRows2 = curr_stmt->GetRowCount();
        BOOST_CHECK_EQUAL( row_count, nRows2 );
    }

    // Check a DELETE statement again
    {
        IStatement* curr_stmt = NULL;
        auto_ptr<IStatement> auto_stmt;
        if ( !stmt ) {
            auto_stmt.reset( GetConnection().GetStatement() );
            curr_stmt = auto_stmt.get();
        } else {
            curr_stmt = stmt;
        }

        sql  = " DELETE FROM " + GetTableName();
        curr_stmt->ExecuteUpdate(sql);

        int nRows = curr_stmt->GetRowCount();
        BOOST_CHECK_EQUAL( 0, nRows );

        int nRows2 = curr_stmt->GetRowCount();
        BOOST_CHECK_EQUAL( 0, nRows2 );
    }

    // Check a SELECT statement again and again ...
    {
        IStatement* curr_stmt = NULL;
        auto_ptr<IStatement> auto_stmt;
        if ( !stmt ) {
            auto_stmt.reset( GetConnection().GetStatement() );
            curr_stmt = auto_stmt.get();
        } else {
            curr_stmt = stmt;
        }

        sql  = " SELECT int_field FROM " + GetTableName() + " ORDER BY int_field";
        curr_stmt->ExecuteUpdate(sql);

        int nRows = curr_stmt->GetRowCount();
        BOOST_CHECK_EQUAL( 0, nRows );

        int nRows2 = curr_stmt->GetRowCount();
        BOOST_CHECK_EQUAL( 0, nRows2 );
    }

}

////////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(Test_GetRowCount)
{
    try {
        {
            auto_ptr<IStatement> stmt( GetConnection().GetStatement() );

            stmt->ExecuteUpdate("DELETE FROM " + GetTableName());
        }

        enum {repeat_num = 5};
        for ( int i = 0; i < repeat_num; ++i ) {
            // Shared/Reusable statement
            {
                auto_ptr<IStatement> stmt( GetConnection().GetStatement() );

                s_CheckGetRowCount( i, eNoTrans, stmt.get() );
                s_CheckGetRowCount( i, eTransCommit, stmt.get() );
                s_CheckGetRowCount( i, eTransRollback, stmt.get() );
            }
            // Dedicated statement
            {
                s_CheckGetRowCount( i, eNoTrans );
                s_CheckGetRowCount( i, eTransCommit );
                s_CheckGetRowCount( i, eTransRollback );
            }
        }

        for ( int i = 0; i < repeat_num; ++i ) {
            // Shared/Reusable statement
            {
                auto_ptr<IStatement> stmt( GetConnection().GetStatement() );

                s_CheckGetRowCount2( i, eNoTrans, stmt.get() );
                s_CheckGetRowCount2( i, eTransCommit, stmt.get() );
                s_CheckGetRowCount2( i, eTransRollback, stmt.get() );
            }
            // Dedicated statement
            {
                s_CheckGetRowCount2( i, eNoTrans );
                s_CheckGetRowCount2( i, eTransCommit );
                s_CheckGetRowCount2( i, eTransRollback );
            }
        }
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}

///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(Test_HasMoreResults)
{
    string sql;
    auto_ptr<IStatement> auto_stmt( GetConnection().GetStatement() );

    // First test ...
    // This test shouldn't throw.
    // Sergey Koshelkov complained that this code throws an exception with
    // info-level severity.
    // ftds (windows):  HasMoreResults() never returns;
    // odbs (windows):  HasMoreResults() throws exception:
    try {
        auto_stmt->SendSql( "exec sp_spaceused @updateusage='true'" );

        BOOST_CHECK( auto_stmt->HasMoreResults() );
        BOOST_CHECK( auto_stmt->HasRows() );
        auto_ptr<IResultSet> rs( auto_stmt->GetResultSet() );
        BOOST_CHECK( rs.get() != NULL );

        while (auto_stmt->HasMoreResults()) {
            if (auto_stmt->HasRows()) {
                auto_ptr<IResultSet> rs( auto_stmt->GetResultSet() );
            }
        }
    }
    catch( const CDB_Exception& ex ) {
        DBAPI_BOOST_FAIL(ex);
    }
    catch(...) {
        BOOST_FAIL("Unknown error in Test_HasMoreResults");
    }

    // Second test ...
    {
        try {
            auto_stmt->SendSql( "exec sp_spaceused @updateusage='true'" );

            BOOST_CHECK( auto_stmt->HasMoreResults() );
            BOOST_CHECK( auto_stmt->HasRows() );
            auto_ptr<IResultSet> rs( auto_stmt->GetResultSet() );
            BOOST_CHECK( rs.get() != NULL );

            while (auto_stmt->HasMoreResults()) {
                if (auto_stmt->HasRows()) {
                    auto_ptr<IResultSet> rs( auto_stmt->GetResultSet() );
                }
            }
        }
        catch( const CDB_Exception& ex ) {
            DBAPI_BOOST_FAIL(ex);
        }
    }
}

///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(Test_GetTotalColumns)
{
    string sql;

    try {
        auto_ptr<IStatement> auto_stmt( GetConnection().GetStatement() );

        // Create table ...
        {
            sql =
                "CREATE TABLE #Overlaps ( \n"
                "   pairId int NOT NULL , \n"
                "   overlapNum smallint NOT NULL , \n"
                "   start1 int NOT NULL , \n"
                "   start2 int NOT NULL , \n"
                "   stop1 int NOT NULL , \n"
                "   stop2 int NOT NULL , \n"
                "   orient char (2) NOT NULL , \n"
                "   gaps int NOT NULL , \n"
                "   mismatches int NOT NULL , \n"
                "   adjustedLen int NOT NULL , \n"
                "   length int NOT NULL , \n"
                "   contained tinyint NOT NULL , \n"
                "   seq_align text  NULL , \n"
                "   merged_sa char (1) NOT NULL , \n"
                "   PRIMARY KEY  \n"
                "   ( \n"
                "       pairId, \n"
                "       overlapNum \n"
                "   ) \n"
                ") \n";

            auto_stmt->ExecuteUpdate( sql );
        }

        // Insert data into the table ...
        {
            sql =
                "INSERT INTO #Overlaps VALUES( \n"
                "1, 1, 0, 25794, 7126, 32916, '--', 1, 21, 7124, 7127, 0, \n"
                "'Seq-align ::= { }', 'n')";

            auto_stmt->ExecuteUpdate( sql );
        }

        // Actual check ...
        {
            sql = "SELECT * FROM #Overlaps";

            auto_stmt->SendSql( sql );
            while( auto_stmt->HasMoreResults() ) {
                if( auto_stmt->HasRows() ) {
                    auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());
                    int col_num = -2;

    //                 switch ( rs->GetResultType() ) {
    //                 case eDB_RowResult:
    //                     col_num = rs->GetColumnNo();
    //                     break;
    //                 case eDB_ParamResult:
    //                     col_num = rs->GetColumnNo();
    //                     break;
    //                 case eDB_StatusResult:
    //                     col_num = rs->GetColumnNo();
    //                     break;
    //                 case eDB_ComputeResult:
    //                 case eDB_CursorResult:
    //                     col_num = rs->GetColumnNo();
    //                     break;
    //                 }

                    col_num = rs->GetTotalColumns();
                    BOOST_CHECK_EQUAL( 14, col_num );
                }
            }
        }
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}

///////////////////////////////////////////////////////////////////////////////
Int8
GetIdentity(const auto_ptr<IStatement>& auto_stmt)
{
    Int8 identity = 0;

    DumpResults(auto_stmt.get());
    // ClearParamList is necessary here ...
    auto_stmt->ClearParamList();
    auto_stmt->SendSql( "select CONVERT(NUMERIC(18, 0), @@identity)");
    while (auto_stmt->HasMoreResults()) {
        if (auto_stmt->HasRows()) {
            auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());
            if (rs.get() != NULL) {
                while (rs->Next()) {
                    identity = rs->GetVariant(1).GetInt8();
                }
            }
        }
    }

    return identity;
}

////////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(Test_Identity)
{
    string sql;
    Int8 table_id = 0;
    Int8 identity_id = 0;
    EDB_Type curr_type = eDB_UnsupportedType;
    const IResultSetMetaData* md = NULL;

    try {
        auto_ptr<IStatement> auto_stmt(GetConnection().GetStatement());

        // Clean table ...
        auto_stmt->ExecuteUpdate("DELETE FROM " + GetTableName());

        // Insert data ...
        {
            // Pure SQL ...
            auto_stmt->ExecuteUpdate(
                "INSERT INTO " + GetTableName() + "(int_field) VALUES(1)");
        }

        // Retrieve data ...
        {
            sql  = " SELECT id, int_field FROM " + GetTableName();

            auto_stmt->SendSql( sql );

            BOOST_CHECK(auto_stmt->HasMoreResults());
            BOOST_CHECK(auto_stmt->HasRows());
            auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());
            BOOST_CHECK(rs.get() != NULL);
            BOOST_CHECK(rs->Next());

            md = rs->GetMetaData();
            BOOST_CHECK(md);
            curr_type = md->GetType(1);
            BOOST_CHECK_EQUAL(curr_type, eDB_BigInt);

            const CVariant& id_value = rs->GetVariant(1);
            BOOST_CHECK(!id_value.IsNull());
            table_id = id_value.GetInt8();
            BOOST_CHECK(!rs->Next());
        }

        // Retrieve identity ...
        {
            sql  = " SELECT CONVERT(NUMERIC(18, 0), @@identity) ";

            auto_stmt->SendSql(sql);

            BOOST_CHECK(auto_stmt->HasMoreResults());
            BOOST_CHECK(auto_stmt->HasRows());
            auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());
            BOOST_CHECK(rs.get() != NULL);
            BOOST_CHECK(rs->Next());
            const CVariant& id_value = rs->GetVariant(1);
            BOOST_CHECK(!id_value.IsNull());
            identity_id = id_value.GetInt8();
            BOOST_CHECK(!rs->Next());
        }

        BOOST_CHECK_EQUAL(table_id, identity_id);

        // Check identity type ...
        {
            sql  = " SELECT @@identity ";

            auto_stmt->SendSql(sql);

            BOOST_CHECK(auto_stmt->HasMoreResults());
            BOOST_CHECK(auto_stmt->HasRows());
            auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());
            BOOST_CHECK(rs.get() != NULL);
            BOOST_CHECK(rs->Next());

            md = rs->GetMetaData();
            BOOST_CHECK(md);
            curr_type = md->GetType(1);
            BOOST_CHECK_EQUAL(curr_type, eDB_Numeric);

            const CVariant& id_value = rs->GetVariant(1);
            BOOST_CHECK(!id_value.IsNull());
            string id_str = rs->GetVariant(1).GetString();
            string id_num = rs->GetVariant(1).GetNumeric();
            BOOST_CHECK_EQUAL(id_str, id_num);

            BOOST_CHECK(!rs->Next());
        }

    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}

////////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(Test_StatementParameters)
{
    try {
        // Very first test ...
        {
            const string table_name(GetTableName());
//             const string table_name("DBAPI_Sample..tmp_table_01");
            string sql;
            auto_ptr<IStatement> auto_stmt( GetConnection().GetStatement() );

            if (false) {
                sql  = " CREATE TABLE " + table_name + "( \n";
                sql += "    id NUMERIC(18, 0) IDENTITY NOT NULL, \n";
                sql += "    int_field INT NULL \n";
                sql += " )";

                auto_stmt->ExecuteUpdate( sql );
            }

            // Clean previously inserted data ...
            {
                auto_stmt->ExecuteUpdate( "DELETE FROM " + table_name );
            }

            {
                sql  = " INSERT INTO " + table_name +
                    "(int_field) VALUES( @value ) \n";

                auto_stmt->SetParam( CVariant(0), "@value" );
                // Execute a statement with parameters ...
                auto_stmt->ExecuteUpdate( sql );

                auto_stmt->SetParam( CVariant(1), "@value" );
                // Execute a statement with parameters ...
                auto_stmt->ExecuteUpdate( sql );

                // !!! Do not forget to clear a parameter list ....
                // Workaround for the ctlib driver ...
                // ClearParamList is necessary here ...
                auto_stmt->ClearParamList();
            }


            {
                sql  = " SELECT int_field FROM " + table_name +
                    " ORDER BY int_field";
                // Execute a statement without parameters ...
                auto_stmt->ExecuteUpdate( sql );
            }

            // Get number of records ...
            {
                auto_stmt->SendSql( "SELECT COUNT(*) FROM " + table_name );
                BOOST_CHECK( auto_stmt->HasMoreResults() );
                BOOST_CHECK( auto_stmt->HasRows() );
                auto_ptr<IResultSet> rs( auto_stmt->GetResultSet() );
                BOOST_CHECK( rs->Next() );
                BOOST_CHECK_EQUAL( rs->GetVariant(1).GetInt4(), 2 );
                DumpResults(auto_stmt.get());
            }

            // Read first inserted value back ...
            {
                // !!! Do not forget to clear a parameter list ....
                // Workaround for the ctlib driver ...
                // ClearParamList is necessary here ...
                auto_stmt->ClearParamList();

                auto_stmt->SetParam( CVariant(0), "@value" );
                auto_stmt->SendSql( " SELECT int_field FROM " + table_name +
                                    " WHERE int_field = @value");
                BOOST_CHECK( auto_stmt->HasMoreResults() );
                BOOST_CHECK( auto_stmt->HasRows() );
                auto_ptr<IResultSet> rs( auto_stmt->GetResultSet() );
                BOOST_CHECK( rs->Next() );
                BOOST_CHECK_EQUAL( rs->GetVariant(1).GetInt4(), 0 );
                DumpResults(auto_stmt.get());
            }

            // Read second inserted value back ...
            {
                // !!! Do not forget to clear a parameter list ....
                // Workaround for the ctlib driver ...
                // ClearParamList is necessary here ...
                auto_stmt->ClearParamList();

                auto_stmt->SetParam( CVariant(1), "@value" );
                auto_stmt->SendSql( " SELECT int_field FROM " + table_name +
                                    " WHERE int_field = @value");
                BOOST_CHECK( auto_stmt->HasMoreResults() );
                BOOST_CHECK( auto_stmt->HasRows() );
                auto_ptr<IResultSet> rs( auto_stmt->GetResultSet() );
                BOOST_CHECK( rs->Next() );
                BOOST_CHECK_EQUAL( rs->GetVariant(1).GetInt4(), 1 );
                DumpResults(auto_stmt.get());
            }

            // Clean previously inserted data ...
            {
                // !!! Do not forget to clear a parameter list ....
                // Workaround for the ctlib driver ...
                // ClearParamList is necessary here ...
                auto_stmt->ClearParamList();

                auto_stmt->ExecuteUpdate( "DELETE FROM " + table_name );
            }
        }

        // Use NULL with parameters ...
        if (GetArgs().GetServerType() == CDBConnParams::eSybaseSQLServer) {
            auto_ptr<IStatement> auto_stmt( GetConnection().GetStatement() );

            auto_stmt->SetParam( CVariant(eDB_VarChar), "@printfmt_par" );
            auto_stmt->SendSql( " SELECT name FROM syscolumns"
//                                 " WHERE id = 4 AND printfmt = @printfmt_par");
                                " WHERE printfmt = @printfmt_par");
            BOOST_CHECK( auto_stmt->HasMoreResults() );
            BOOST_CHECK( auto_stmt->HasRows() );
            auto_ptr<IResultSet> rs( auto_stmt->GetResultSet() );
            BOOST_CHECK( rs->Next() );
            DumpResults(auto_stmt.get());
        } else {
            GetArgs().PutMsgDisabled("Use NULL with parameters");
        }
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}

///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(Test_Query_Cancelation)
{
    string sql;

    try {
        // Cancel without a previously thrown exception.
        {
            // IStatement
            {
                auto_ptr<IStatement> auto_stmt(GetConnection().GetStatement());

                // 1
                auto_stmt->SendSql("SELECT name FROM sysobjects");
                auto_stmt->Cancel();
                auto_stmt->Cancel();

                // 2
                auto_stmt->SendSql("SELECT name FROM sysobjects");
                BOOST_CHECK( auto_stmt->HasMoreResults() );
                BOOST_CHECK( auto_stmt->HasRows() );
                {
                    auto_ptr<IResultSet> rs( auto_stmt->GetResultSet() );
                    BOOST_CHECK( rs.get() != NULL );
                }
                auto_stmt->Cancel();
                auto_stmt->Cancel();

                // 3
                auto_stmt->SendSql("SELECT name FROM sysobjects");
                BOOST_CHECK( auto_stmt->HasMoreResults() );
                BOOST_CHECK( auto_stmt->HasRows() );
                {
                    auto_ptr<IResultSet> rs( auto_stmt->GetResultSet() );
                    BOOST_CHECK( rs.get() != NULL );
                    BOOST_CHECK( rs->Next() );
                }
                auto_stmt->Cancel();
                auto_stmt->Cancel();

                // 4
                auto_stmt->SendSql("SELECT name FROM sysobjects");
                BOOST_CHECK(auto_stmt->HasMoreResults());
                BOOST_CHECK(auto_stmt->HasRows());
                {
                    int i = 0;
                    auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());
                    BOOST_CHECK(rs.get() != NULL);
                    while (rs->Next()) {
                        ++i;
                    }
                    BOOST_CHECK(i > 0);
                }
                auto_stmt->Cancel();
                auto_stmt->Cancel();
                auto_stmt->Cancel();
            }

            // IColableStatement
            {
                auto_ptr<ICallableStatement> auto_stmt;

                const char *proc_name =
                    GetArgs().GetServerType() == CDBConnParams::eMSSqlServer ?
                                                     "sp_oledb_database" :
                                                     "sp_databases";

                // 1
                auto_stmt.reset(GetConnection().GetCallableStatement(proc_name));
                auto_stmt->Execute();
                DumpResults(auto_stmt.get());
                auto_stmt->Execute();
                DumpResults(auto_stmt.get());

                // 2
                auto_stmt->Execute();
                auto_stmt->Cancel();
                auto_stmt->Execute();
                auto_stmt->Cancel();
                auto_stmt->Cancel();

                // 3
                auto_stmt.reset(GetConnection().GetCallableStatement(proc_name));
                auto_stmt->Execute();
                auto_stmt->Cancel();
                auto_stmt.reset(GetConnection().GetCallableStatement(proc_name));
                auto_stmt->Execute();
                auto_stmt->Cancel();
                auto_stmt->Cancel();

                // 4
                auto_stmt.reset(GetConnection().GetCallableStatement(proc_name));
                auto_stmt->Execute();
                BOOST_CHECK(auto_stmt->HasMoreResults());
                BOOST_CHECK(auto_stmt->HasRows());
                {
                    auto_ptr<IResultSet> rs( auto_stmt->GetResultSet() );
                    BOOST_CHECK(rs.get() != NULL);
                }
                auto_stmt->Cancel();
                auto_stmt->Cancel();

                // 5
                auto_stmt.reset(GetConnection().GetCallableStatement(proc_name));
                auto_stmt->Execute();
                BOOST_CHECK(auto_stmt->HasMoreResults());
                BOOST_CHECK(auto_stmt->HasRows());
                {
                    auto_ptr<IResultSet> rs( auto_stmt->GetResultSet() );
                    BOOST_CHECK(rs.get() != NULL);
                    BOOST_CHECK(rs->Next());
                }
                auto_stmt->Cancel();
                auto_stmt->Cancel();

                // 6
                auto_stmt.reset(GetConnection().GetCallableStatement(proc_name));
                auto_stmt->Execute();
                BOOST_CHECK(auto_stmt->HasMoreResults());
                BOOST_CHECK(auto_stmt->HasRows());
                {
                    int i = 0;
                    auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());
                    BOOST_CHECK(rs.get() != NULL);
                    while (rs->Next()) {
                        ++i;
                    }
                    BOOST_CHECK(i > 0);
                }
                auto_stmt->Cancel();
                auto_stmt->Cancel();
                auto_stmt->Cancel();
            }

            // BCP
            {
            }
        }

        // Cancel with a previously thrown exception.
        {
            // IStatement
            {
                auto_ptr<IStatement> auto_stmt(GetConnection().GetStatement());

                // 1
                try {
                    auto_stmt->SendSql("SELECT oops FROM sysobjects");
                    BOOST_CHECK( auto_stmt->HasMoreResults() );
                } catch(const CDB_Exception& _DEBUG_ARG(ex))
                {
                    _TRACE(ex);
                    auto_stmt->Cancel();
                }
                auto_stmt->Cancel();

                // Check that everything is fine ...
                auto_stmt->SendSql("SELECT name FROM sysobjects");
                DumpResults(auto_stmt.get());

                // 2
                try {
                    sql = "SELECT name FROM sysobjects WHERE name = 'oops'";
                    auto_stmt->SendSql(sql);
                    BOOST_CHECK( auto_stmt->HasMoreResults() );
                    BOOST_CHECK( auto_stmt->HasRows() );
                    auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());
                    BOOST_CHECK(rs.get() != NULL);
                    rs->Next();
                } catch(const CDB_Exception& _DEBUG_ARG(ex))
                {
                    _TRACE(ex);
                    auto_stmt->Cancel();
                }
                auto_stmt->Cancel();

                // Check that everything is fine ...
                auto_stmt->SendSql("SELECT name FROM sysobjects");
                DumpResults(auto_stmt.get());

                // 3
                try {
                    int i = 0;
                    auto_stmt->SendSql("SELECT name FROM sysobjects");
                    BOOST_CHECK( auto_stmt->HasMoreResults() );
                    BOOST_CHECK( auto_stmt->HasRows() );
                    auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());
                    BOOST_CHECK(rs.get() != NULL);
                    while (rs->Next()) {
                        ++i;
                    }
                    BOOST_CHECK(i > 0);
                    rs->Next();
                } catch(const CDB_Exception& _DEBUG_ARG(ex))
                {
                    _TRACE(ex);
                    auto_stmt->Cancel();
                }
                auto_stmt->Cancel();

                // Check that everything is fine ...
                auto_stmt->SendSql("SELECT name FROM sysobjects");
                DumpResults(auto_stmt.get());
            }


            // IColableStatement
            {
                auto_ptr<ICallableStatement> auto_stmt;

                // 1
                try {
                    auto_stmt.reset(GetConnection().GetCallableStatement("sp_wrong"));
                    auto_stmt->ExecuteUpdate();
                } catch(const CDB_Exception& _DEBUG_ARG(ex))
                {
                    _TRACE(ex);
                    auto_stmt->Cancel();
                }

                try {
                    auto_stmt->ExecuteUpdate();
                } catch(const CDB_Exception& _DEBUG_ARG(ex))
                {
                    _TRACE(ex);
                    auto_stmt->Cancel();
                }
            }
        }
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}

////////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(Test_BindByPos)
{
    const long rec_num  = 10;
    const string table_name = GetTableName();
    const string str_value = "asdfghjkl";
    string sql;

    // Initialize data (strings are full of spaces) ...
    try {
        auto_ptr<IStatement> auto_stmt( GetConnection().GetStatement() );

        // First test ...
        {
            {
                // Drop all records ...
                sql  = " DELETE FROM " + table_name;
                auto_stmt->ExecuteUpdate(sql);
            }

            {
                sql  = " INSERT INTO " + table_name +
                    "(int_field, vc1000_field) "
                    "VALUES(@int_field, @vc1000_field) \n";

                // !!! Do not forget to clear a parameter list ....
                // Workaround for the ctlib driver ...
                auto_stmt->ClearParamList();

                auto_stmt->SetParam( CVariant(string(str_value)), 2);

                // Insert data ...
                for (long ind = 0; ind < rec_num; ++ind) {
                    if (ind % 2 == 0) {
                        auto_stmt->SetParam( CVariant( Int4(ind) ), 1);
                    } else {
                        auto_stmt->SetParam( CVariant(eDB_Int), 1);
                    }

                    // Execute a statement with parameters ...
                    auto_stmt->ExecuteUpdate( sql );
                }

                // Check record number ...
                BOOST_CHECK_EQUAL(size_t(rec_num), GetNumOfRecords(auto_stmt,
                            GetTableName()));
            }

            // Check ...
            {
                sql = "SELECT int_field, vc1000_field FROM " + table_name +
                    " ORDER BY id";

                auto_stmt->SendSql( sql );
                BOOST_CHECK(auto_stmt->HasMoreResults());
                BOOST_CHECK(auto_stmt->HasRows());
                auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());
                BOOST_CHECK(rs.get() != NULL);

                for (long ind = 0; ind < rec_num; ++ind) {
                    BOOST_CHECK(rs->Next());

                    // const CVariant& int_field = rs->GetVariant(1);
                    const CVariant& vc1000_field = rs->GetVariant(2);

                    BOOST_CHECK( !vc1000_field.IsNull() );
                    BOOST_CHECK_EQUAL(vc1000_field.GetString(), str_value);
                }

                DumpResults(auto_stmt.get());
            }
        }
    }
    catch(const CDB_Exception& ex) {
        DBAPI_BOOST_FAIL(ex);
    }

}

////////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(Test_ClearParamList)
{
    const long rec_num  = 10;
    const string table_name = GetTableName();
    const string str_value = "asdfghjkl";
    string sql;

    // Initialize data (strings are full of spaces) ...
    try {
        auto_ptr<IStatement> auto_stmt( GetConnection().GetStatement() );

        // First test ...
        {
            {
                // Drop all records ...
                sql  = " DELETE FROM " + GetTableName();
                auto_stmt->ExecuteUpdate(sql);
            }

            {
                sql  = " INSERT INTO " + GetTableName() +
                    "(int_field, vc1000_field) "
                    "VALUES(@int_field, @vc1000_field) \n";

                // !!! Do not forget to clear a parameter list ....
                // Workaround for the ctlib driver ...
                auto_stmt->ClearParamList();

                auto_stmt->SetParam( CVariant(string(str_value)), "@vc1000_field" );

                // Insert data ...
                for (long ind = 0; ind < rec_num; ++ind) {
                    if (ind % 2 == 0) {
                        auto_stmt->SetParam( CVariant( Int4(ind) ), "@int_field" );
                    } else {
                        auto_stmt->SetParam( CVariant(eDB_Int), "@int_field" );
                    }

                    // Execute a statement with parameters ...
                    auto_stmt->ExecuteUpdate( sql );
                }

                // Check record number ...
                BOOST_CHECK_EQUAL(size_t(rec_num), GetNumOfRecords(auto_stmt,
                            GetTableName()));
            }

            // Check ...
            {
                sql = "SELECT int_field, vc1000_field FROM " + table_name +
                    " ORDER BY id";

                auto_stmt->SendSql( sql );
                BOOST_CHECK(auto_stmt->HasMoreResults());
                BOOST_CHECK(auto_stmt->HasRows());
                auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());
                BOOST_CHECK(rs.get() != NULL);

                for (long ind = 0; ind < rec_num; ++ind) {
                    BOOST_CHECK(rs->Next());

                    // const CVariant& int_field = rs->GetVariant(1);
                    const CVariant& vc1000_field = rs->GetVariant(2);

                    BOOST_CHECK( !vc1000_field.IsNull() );
                    BOOST_CHECK_EQUAL(vc1000_field.GetString(), str_value);
                    /*
                    if (ind % 2 == 0) {
                        BOOST_CHECK( !int_field.IsNull() );
                        BOOST_CHECK_EQUAL( int_field.GetInt4(), ind );

                        BOOST_CHECK( vc1000_field.IsNull() );
                    } else {
                        BOOST_CHECK( int_field.IsNull() );

                        BOOST_CHECK( !vc1000_field.IsNull() );
                        BOOST_CHECK_EQUAL( vc1000_field.GetString(),
                                           NStr::IntToString(ind) );
                    }
                    */
                }

                DumpResults(auto_stmt.get());
            }
        }
    }
    catch(const CDB_Exception& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}

///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(Test_Create_Destroy)
{
    try {
        ///////////////////////
        // CreateStatement
        ///////////////////////

        // Destroy a statement before a connection get destroyed ...
        {
            auto_ptr<IConnection> local_conn(
                GetDS().CreateConnection(eTakeOwnership)
                );
            local_conn->Connect(GetArgs().GetConnParams());

            auto_ptr<IStatement> stmt(local_conn->CreateStatement());
            stmt->SendSql( "SELECT name FROM sysobjects" );
            DumpResults(stmt.get());
        }

        // Do not destroy statement, let it be destroyed ...
        {
            auto_ptr<IConnection> local_conn(
                GetDS().CreateConnection(eTakeOwnership)
                );
            local_conn->Connect(GetArgs().GetConnParams());

            IStatement* stmt(local_conn->CreateStatement());
            stmt->SendSql( "SELECT name FROM sysobjects" );
            DumpResults(stmt);
        }

        ///////////////////////
        // GetStatement
        ///////////////////////

        // Destroy a statement before a connection get destroyed ...
        {
            auto_ptr<IConnection> local_conn(
                GetDS().CreateConnection(eTakeOwnership)
                );
            local_conn->Connect(GetArgs().GetConnParams());

            auto_ptr<IStatement> stmt(local_conn->GetStatement());
            stmt->SendSql( "SELECT name FROM sysobjects" );
            DumpResults(stmt.get());
        }

        // Do not destroy statement, let it be destroyed ...
        {
            auto_ptr<IConnection> local_conn(
                GetDS().CreateConnection(eTakeOwnership)
                );
            local_conn->Connect(GetArgs().GetConnParams());

            IStatement* stmt(local_conn->GetStatement());
            stmt->SendSql( "SELECT name FROM sysobjects" );
            DumpResults(stmt);
        }


        ///////////////////////////
        // Close connection several times ...
        ///////////////////////////
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}


END_NCBI_SCOPE
