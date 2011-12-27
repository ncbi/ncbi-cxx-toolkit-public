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
BOOST_AUTO_TEST_CASE(Test_Procedure)
{
    try {
        // Test a regular IStatement with "exec"
        // Parameters are not allowed with this construction.
        {
            auto_ptr<IStatement> auto_stmt( GetConnection().GetStatement() );

            // Execute it first time ...
            auto_stmt->SendSql( "exec sp_databases" );
            while( auto_stmt->HasMoreResults() ) {
                if( auto_stmt->HasRows() ) {
                    auto_ptr<IResultSet> rs( auto_stmt->GetResultSet() );

                    switch ( rs->GetResultType() ) {
                    case eDB_RowResult:
                        while( rs->Next() ) {
                            // int col1 = rs->GetVariant(1).GetInt4();
                        }
                        break;
                    case eDB_ParamResult:
                        while( rs->Next() ) {
                            // int col1 = rs->GetVariant(1).GetInt4();
                        }
                        break;
                    case eDB_StatusResult:
                        while( rs->Next() ) {
                            int status = rs->GetVariant(1).GetInt4();
                            status = status;
                        }
                        break;
                    case eDB_ComputeResult:
                    case eDB_CursorResult:
                        break;
                    }
                }
            }

            // Execute it second time ...
            auto_stmt->SendSql( "exec sp_databases" );
            while( auto_stmt->HasMoreResults() ) {
                if( auto_stmt->HasRows() ) {
                    auto_ptr<IResultSet> rs( auto_stmt->GetResultSet() );

                    switch ( rs->GetResultType() ) {
                    case eDB_RowResult:
                        while( rs->Next() ) {
                            // int col1 = rs->GetVariant(1).GetInt4();
                        }
                        break;
                    case eDB_ParamResult:
                        while( rs->Next() ) {
                            // int col1 = rs->GetVariant(1).GetInt4();
                        }
                        break;
                    case eDB_StatusResult:
                        while( rs->Next() ) {
                            int status = rs->GetVariant(1).GetInt4();
                            status = status;
                        }
                        break;
                    case eDB_ComputeResult:
                    case eDB_CursorResult:
                        break;
                    }
                }
            }

            // Same as before but do not retrieve data ...
            auto_stmt->SendSql( "exec sp_databases" );
            auto_stmt->SendSql( "exec sp_databases" );
        }

        // Test ICallableStatement
        // No parameters at this time.
        {
            // Execute it first time ...
            auto_ptr<ICallableStatement> auto_stmt(
                GetConnection().GetCallableStatement("sp_databases")
                );
            auto_stmt->Execute();
            while(auto_stmt->HasMoreResults()) {
                if( auto_stmt->HasRows() ) {
                    auto_ptr<IResultSet> rs( auto_stmt->GetResultSet() );

                    switch( rs->GetResultType() ) {
                    case eDB_RowResult:
                        while(rs->Next()) {
                            // retrieve row results
                        }
                        break;
                    case eDB_ParamResult:
                        while(rs->Next()) {
                            // Retrieve parameter row
                        }
                        break;
                    default:
                        break;
                    }
                }
            }
            // Get status
            auto_stmt->GetReturnStatus();


            // Execute it second time ...
            auto_stmt.reset( GetConnection().GetCallableStatement("sp_databases") );
            auto_stmt->Execute();
            while(auto_stmt->HasMoreResults()) {
                if( auto_stmt->HasRows() ) {
                    auto_ptr<IResultSet> rs( auto_stmt->GetResultSet() );

                    switch( rs->GetResultType() ) {
                    case eDB_RowResult:
                        while(rs->Next()) {
                            // retrieve row results
                        }
                        break;
                    case eDB_ParamResult:
                        while(rs->Next()) {
                            // Retrieve parameter row
                        }
                        break;
                    default:
                        break;
                    }
                }
            }
            // Get status
            auto_stmt->GetReturnStatus();


            // Same as before but do not retrieve data ...
            auto_stmt.reset( GetConnection().GetCallableStatement("sp_databases") );
            auto_stmt->Execute();
            auto_stmt.reset( GetConnection().GetCallableStatement("sp_databases") );
            auto_stmt->Execute();
        }

        // Temporary test ...
        // !!! This is a bug ...
        if (false) {
            auto_ptr<IConnection> conn( GetDS().CreateConnection( CONN_OWNERSHIP ) );
            BOOST_CHECK( conn.get() != NULL );

            conn->Connect(
                "anyone",
                "allowed",
                "PUBSEQ_OS_LXA",
                ""
                );

            auto_ptr<ICallableStatement> auto_stmt(
                conn->GetCallableStatement("id_seqid4gi")
                );
            auto_stmt->SetParam( CVariant(1), "@gi" );
            auto_stmt->Execute();
            while(auto_stmt->HasMoreResults()) {
                if( auto_stmt->HasRows() ) {
                    auto_ptr<IResultSet> rs( auto_stmt->GetResultSet() );

                    switch( rs->GetResultType() ) {
                    case eDB_RowResult:
                        while(rs->Next()) {
                            // retrieve row results
                        }
                        break;
                    case eDB_ParamResult:
                        _ASSERT(false);
                        while(rs->Next()) {
                            // Retrieve parameter row
                        }
                        break;
                    default:
                        break;
                    }
                }
            }
            // Get status
            int status = auto_stmt->GetReturnStatus();
            status = status; // Get rid of warnings.
        }

        if (false) {
            const string query("[db_alias] is not null");

            auto_ptr<IConnection> conn( GetDS().CreateConnection( CONN_OWNERSHIP ) );
            BOOST_CHECK( conn.get() != NULL );

            conn->Connect(
                "*****",
                "******",
                "MSSQL31",
                "AlignDb_Info"
                );

            auto_ptr<ICallableStatement> auto_stmt(
                conn->PrepareCall("[dbo].[FindAttributesEx]")
                );
            auto_stmt->SetParam( CVariant(1), "@userid" );
            auto_stmt->SetParam( CVariant("ALIGNDB"), "@application" );
            auto_stmt->SetParam( CVariant("AlignDbMasterInfo"), "@classname" );
            // LongChar doesn't work.
            // auto_stmt->SetParam( CVariant(new CDB_LongChar(query.length(), query)), "@query" );
            // auto_stmt->SetParam( CVariant::LongChar(query.c_str(), query.length()), "@query" );
            auto_stmt->SetParam( CVariant(query), "@query" );
            auto_stmt->SetParam( CVariant(1), "@max_results" );

            auto_stmt->Execute();
            while(auto_stmt->HasMoreResults()) {
                if( auto_stmt->HasRows() ) {
                    auto_ptr<IResultSet> rs( auto_stmt->GetResultSet() );

                    switch( rs->GetResultType() ) {
                    case eDB_RowResult:
                        while(rs->Next()) {
                            // retrieve row results
                        }
                        break;
                    case eDB_ParamResult:
                        _ASSERT(false);
                        while(rs->Next()) {
                            // Retrieve parameter row
                        }
                        break;
                    default:
                        break;
                    }
                }
            }
            // Get status
            int status = auto_stmt->GetReturnStatus();
            status = status; // Get rid of warnings.
        }


        // Test returned recordset ...
        {
            // In case of MS SQL 2005 sp_databases returns empty result set.
            // It is not a bug. It is a difference in setiings for MS SQL
            // 2005.
            if (GetArgs().GetServerType() != CDBConnParams::eMSSqlServer) {
                int num = 0;
                // Execute it first time ...
                auto_ptr<ICallableStatement> auto_stmt(
                    GetConnection().GetCallableStatement("sp_databases")
                    );

                auto_stmt->Execute();

                BOOST_CHECK(auto_stmt->HasMoreResults());
                BOOST_CHECK(auto_stmt->HasRows());
                auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());
                BOOST_CHECK(rs.get() != NULL);

                while (rs->Next()) {
                    BOOST_CHECK(rs->GetVariant(1).GetString().size() > 0);
                    BOOST_CHECK(rs->GetVariant(2).GetInt4() > 0);
                    BOOST_CHECK_EQUAL(rs->GetVariant(3).IsNull(), true);
                    ++num;
                }

                BOOST_CHECK(num > 0);

                DumpResults(auto_stmt.get());
            }

            {
                int num = 0;
                auto_ptr<ICallableStatement> auto_stmt(
                    GetConnection().GetCallableStatement("sp_server_info")
                    );

                auto_stmt->Execute();

                BOOST_CHECK(auto_stmt->HasMoreResults());
                BOOST_CHECK(auto_stmt->HasRows());
                auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());
                BOOST_CHECK(rs.get() != NULL);

                while (rs->Next()) {
                    BOOST_CHECK(rs->GetVariant(1).GetInt4() > 0);
                    BOOST_CHECK(rs->GetVariant(2).GetString().size() > 0);
                    BOOST_CHECK(rs->GetVariant(3).GetString().size() > 0);
                    ++num;
                }

                BOOST_CHECK(num > 0);

                DumpResults(auto_stmt.get());
            }
        }

        // Test ICallableStatement
        // With parameters.
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

            // NULL value with CVariant ...
            {
                auto_ptr<ICallableStatement> auto_stmt(
                    GetConnection().GetCallableStatement("sp_statistics")
                    );

                auto_stmt->SetParam(CVariant((const char*) NULL), "@table_name");
                auto_stmt->Execute();
                DumpResults(auto_stmt.get());
            }

            // Doesn't work for some reason ...
            if (false) {
                // Execute it first time ...
                auto_ptr<ICallableStatement> auto_stmt(
                    GetConnection().GetCallableStatement("sp_statistics")
                    );

                auto_stmt->SetParam(CVariant(GetTableName()), "@table_name");
                auto_stmt->Execute();

                {
                    BOOST_CHECK(auto_stmt->HasMoreResults());
                    BOOST_CHECK(auto_stmt->HasRows());
                    auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());
                    BOOST_CHECK(rs.get() != NULL);

                    BOOST_CHECK(rs->Next());
                    DumpResults(auto_stmt.get());
                }

                // Execute it second time ...
                auto_stmt->SetParam(CVariant("#bulk_insert_table"), "@table_name");
                auto_stmt->Execute();

                {
                    BOOST_CHECK(auto_stmt->HasMoreResults());
                    BOOST_CHECK(auto_stmt->HasRows());
                    auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());
                    BOOST_CHECK(rs.get() != NULL);

                    BOOST_CHECK(rs->Next());
                    DumpResults(auto_stmt.get());
                }
            }

            if (false) {
                auto_ptr<ICallableStatement> auto_stmt(
                    GetConnection().GetCallableStatement("DBAPI_Sample..TestBigIntProc")
                    );

                auto_stmt->SetParam(CVariant(Int8(1234567890)), "@num");
                auto_stmt->ExecuteUpdate();
            }
        }

        // Test output parameters ...
        if (false) {
            CRef<CDB_UserHandler_Diag> handler(new  CDB_UserHandler_Diag());
            I_DriverContext* drv_context = GetDS().GetDriverContext();

            drv_context->PushDefConnMsgHandler(handler);

            auto_ptr<ICallableStatement> auto_stmt(
                GetConnection().GetCallableStatement("DBAPI_Sample..SampleProc3")
                );
            auto_stmt->SetParam(CVariant(1), "@id");
            auto_stmt->SetParam(CVariant(2.0), "@f");
            auto_stmt->SetOutputParam(CVariant(eDB_Int), "@o");

            auto_stmt->Execute();
    //         auto_stmt->SendSql( "exec DBAPI_Sample..TestProc4 @test_out output" );

             while(auto_stmt->HasMoreResults()) {
                 if( auto_stmt->HasRows() ) {
                     auto_ptr<IResultSet> rs( auto_stmt->GetResultSet() );

                     switch( rs->GetResultType() ) {
                     case eDB_RowResult:
                         while(rs->Next()) {
                             // retrieve row results
                         }
                         break;
                     case eDB_ParamResult:
                         BOOST_CHECK(rs->Next());
                         NcbiCout << "Output param: "
                                  << rs->GetVariant(1).GetInt4()
                                  << endl;
                         break;
                     case eDB_ComputeResult:
                         break;
                     case eDB_StatusResult:
                         break;
                     case eDB_CursorResult:
                         break;
                     default:
                         break;
                     }
                 }
             }

    //         BOOST_CHECK(auto_stmt->HasMoreResults());
    //         BOOST_CHECK(auto_stmt->HasRows());
    //         auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());
    //         BOOST_CHECK(rs.get() != NULL);
    //
    //         while (rs->Next()) {
    //             BOOST_CHECK(rs->GetVariant(1).GetString().size() > 0);
    //             BOOST_CHECK(rs->GetVariant(2).GetInt4() > 0);
    //             BOOST_CHECK_EQUAL(rs->GetVariant(3).IsNull(), true);
    //             ++num;
    //         }
    //
    //         BOOST_CHECK(num > 0);

            DumpResults(auto_stmt.get());

            drv_context->PopDefConnMsgHandler(handler);
        }

        // Temporary test ...
        if (false) {
            auto_ptr<IConnection> conn( GetDS().CreateConnection( CONN_OWNERSHIP ) );
            BOOST_CHECK( conn.get() != NULL );

            conn->Connect(
                "anyone",
                "allowed",
                "",
                "GenomeHits"
                );

            auto_ptr<ICallableStatement> auto_stmt(
                conn->GetCallableStatement("NewSub")
                );
            auto_stmt->SetParam(CVariant("tsub2"), "@name");
            auto_stmt->SetParam(CVariant("tst"), "@center");
            auto_stmt->SetParam(CVariant("9606"), "@taxid");
            auto_stmt->SetParam(CVariant("Homo sapiens"), "@organism");
            auto_stmt->SetParam(CVariant(""), "@notes");
            auto_stmt->Execute();

            while(auto_stmt->HasMoreResults()) {
                if( auto_stmt->HasRows() ) {
                    auto_ptr<IResultSet> rs( auto_stmt->GetResultSet() );

                    switch( rs->GetResultType() ) {
                    case eDB_RowResult:
                        while(rs->Next()) {
                            // retrieve row results
                        }
                        break;
                    case eDB_ParamResult:
                        _ASSERT(false);
                        while(rs->Next()) {
                            // Retrieve parameter row
                        }
                        break;
                    default:
                        break;
                    }
                }
            }

            // Get status
            int status = auto_stmt->GetReturnStatus();
            status = status; // Get rid of warnings.
        }

        // Temporary test ...
        if (false && GetArgs().GetServerType() != CDBConnParams::eSybaseSQLServer) {
            auto_ptr<IConnection> conn( GetDS().CreateConnection( CONN_OWNERSHIP ) );
            BOOST_CHECK( conn.get() != NULL );

            conn->Connect(
                "pmcupdate",
                "*******",
                "PMC3QA",
                "PMC3QA"
                );

            auto_ptr<ICallableStatement> auto_stmt(
                conn->PrepareCall("id_new_id")
                );
            auto_stmt->SetParam(CVariant("tsub2"), "@IdName");

            auto_stmt->Execute();

            while(auto_stmt->HasMoreResults()) {
                if( auto_stmt->HasRows() ) {
                    auto_ptr<IResultSet> rs( auto_stmt->GetResultSet() );

                    switch( rs->GetResultType() ) {
                    case eDB_RowResult:
                        while(rs->Next()) {
                            // retrieve row results
                        }
                        break;
                    case eDB_ParamResult:
                        _ASSERT(false);
                        while(rs->Next()) {
                            // Retrieve parameter row
                        }
                        break;
                    default:
                        break;
                    }
                }
            }

            // Get status
            int status = auto_stmt->GetReturnStatus();
            status = status; // Get rid of warnings.
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
            auto_ptr<ICallableStatement> auto_stmt(
                    GetConnection().GetCallableStatement("sp_server_info")
                    );

            auto_stmt->Execute();

            while(auto_stmt->HasMoreResults()) {
                if( auto_stmt->HasRows() ) {
                    auto_ptr<IResultSet> rs( auto_stmt->GetResultSet() );

                    switch( rs->GetResultType() ) {
                    case eDB_RowResult:
                        while(rs->Next()) {
                            int value_int = rs->GetVariant(1).GetInt4();
                            BOOST_CHECK(value_int != 0);
                        }
                        break;
                    default:
                        break;
                    }
                }
            }
        }

        // Mismatched types of INT parameters ...
        {
            auto_ptr<ICallableStatement> auto_stmt(
                    GetConnection().GetCallableStatement("sp_server_info")
                    );

            // Set parameter to 2 ...
            // sp_server_info takes an INT parameter ...
            auto_stmt->SetParam( CVariant( Int8(2) ), "@attribute_id" );
            auto_stmt->Execute();

            BOOST_CHECK_EQUAL( size_t(1), GetNumOfRecords(auto_stmt) );
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
            auto_ptr<ICallableStatement> auto_stmt(
                    GetConnection().GetCallableStatement("sp_helpdb")
                    );

            auto_stmt->SetParam( CVariant( "master" ), "@dbname" );
            auto_stmt->Execute();

            int result_num = 0;

            while (auto_stmt->HasMoreResults()) {
                if ( auto_stmt->HasRows() ) {
                    auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());

                    while (rs->Next()) {
                        ;
                    }

                    ++result_num;
                }
            }

            /*if (GetArgs().GetServerType() == CDBConnParams::eSybaseSQLServer) {
                BOOST_CHECK_EQUAL(result_num, 3);
            } else {*/
                BOOST_CHECK_EQUAL(result_num, 2);
            //}
        }

        // The same as above, but using IStatement ...
        {
            auto_ptr<IStatement> auto_stmt(GetConnection().GetStatement());

            int result_num = 0;

            auto_stmt->SendSql("exec sp_helpdb 'master'");
            while (auto_stmt->HasMoreResults()) {
                if ( auto_stmt->HasRows() ) {
                    auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());

                    while (rs->Next()) {
                        ;
                    }

                    ++result_num;
                }
            }

            /*if (GetArgs().GetServerType() == CDBConnParams::eSybaseSQLServer) {
                BOOST_CHECK_EQUAL(result_num, 4);
            } else {*/
                BOOST_CHECK_EQUAL(result_num, 3);
            //}
        }

        // Multiple results plus column names with spaces.
        if (GetArgs().GetServerType() == CDBConnParams::eMSSqlServer) {

            auto_ptr<ICallableStatement> auto_stmt(
                    GetConnection().GetCallableStatement("sp_spaceused")
                    );

            auto_stmt->Execute();

            BOOST_CHECK(auto_stmt->HasMoreResults());

            string unallocSpace;

            if ( auto_stmt->HasRows() ) {
                auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());

                while (rs->Next()) {
                    unallocSpace = rs->GetVariant("unallocated space").GetString();
                }
            }

            BOOST_CHECK(auto_stmt->HasMoreResults());
            BOOST_CHECK(auto_stmt->HasMoreResults());
        }
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}


END_NCBI_SCOPE
