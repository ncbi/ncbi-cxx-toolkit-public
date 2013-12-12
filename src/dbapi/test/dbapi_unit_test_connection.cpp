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

#include <corelib/ncbi_system.hpp>
#include <corelib/resource_info.hpp>

#include <dbapi/driver/impl/dbapi_driver_utils.hpp>
#include <dbapi/driver/dbapi_svc_mapper.hpp>

#ifdef HAVE_LIBCONNEXT
#  include <connect/ext/ncbi_crypt.h>
#endif


BEGIN_NCBI_SCOPE

///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(Test_Multiple_Close)
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

            // Close a statement first ...
            stmt->Close();
            stmt->Close();
            stmt->Close();

            local_conn->Close();
            local_conn->Close();
            local_conn->Close();
        }

        {
            auto_ptr<IConnection> local_conn(
                GetDS().CreateConnection(eTakeOwnership)
                );
            local_conn->Connect(GetArgs().GetConnParams());

            auto_ptr<IStatement> stmt(local_conn->CreateStatement());
            stmt->SendSql( "SELECT name FROM sysobjects" );
            DumpResults(stmt.get());

            // Close a connection first ...
            local_conn->Close();
            local_conn->Close();
            local_conn->Close();

            stmt->Close();
            stmt->Close();
            stmt->Close();
        }

        // Do not destroy a statement, let it be destroyed ...
        {
            auto_ptr<IConnection> local_conn(
                GetDS().CreateConnection(eTakeOwnership)
                );
            local_conn->Connect(GetArgs().GetConnParams());

            IStatement* stmt(local_conn->CreateStatement());
            stmt->SendSql( "SELECT name FROM sysobjects" );
            DumpResults(stmt);

            // Close a statement first ...
            stmt->Close();
            stmt->Close();
            stmt->Close();

            local_conn->Close();
            local_conn->Close();
            local_conn->Close();
        }

        {
            auto_ptr<IConnection> local_conn(
                GetDS().CreateConnection(eTakeOwnership)
                );
            local_conn->Connect(GetArgs().GetConnParams());

            IStatement* stmt(local_conn->CreateStatement());
            stmt->SendSql( "SELECT name FROM sysobjects" );
            DumpResults(stmt);

            // Close a connection first ...
            local_conn->Close();
            local_conn->Close();
            local_conn->Close();

            stmt->Close();
            stmt->Close();
            stmt->Close();
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

            // Close a statement first ...
            stmt->Close();
            stmt->Close();
            stmt->Close();

            local_conn->Close();
            local_conn->Close();
            local_conn->Close();
        }

        {
            auto_ptr<IConnection> local_conn(
                GetDS().CreateConnection(eTakeOwnership)
                );
            local_conn->Connect(GetArgs().GetConnParams());

            auto_ptr<IStatement> stmt(local_conn->GetStatement());
            stmt->SendSql( "SELECT name FROM sysobjects" );
            DumpResults(stmt.get());

            // Close a connection first ...
            local_conn->Close();
            local_conn->Close();
            local_conn->Close();

            stmt->Close();
            stmt->Close();
            stmt->Close();
        }

        // Do not destroy a statement, let it be destroyed ...
        {
            auto_ptr<IConnection> local_conn(
                GetDS().CreateConnection(eTakeOwnership)
                );
            local_conn->Connect(GetArgs().GetConnParams());

            IStatement* stmt(local_conn->GetStatement());
            stmt->SendSql( "SELECT name FROM sysobjects" );
            DumpResults(stmt);

            // Close a statement first ...
            stmt->Close();
            stmt->Close();
            stmt->Close();

            local_conn->Close();
            local_conn->Close();
            local_conn->Close();
        }

        {
            auto_ptr<IConnection> local_conn(
                GetDS().CreateConnection(eTakeOwnership)
                );
            local_conn->Connect(GetArgs().GetConnParams());

            IStatement* stmt(local_conn->GetStatement());
            stmt->SendSql( "SELECT name FROM sysobjects" );
            DumpResults(stmt);

            // Close a connection first ...
            local_conn->Close();
            local_conn->Close();
            local_conn->Close();

            stmt->Close();
            stmt->Close();
            stmt->Close();
        }
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}

////////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(Test_DropConnection)
{
    string sql;
    enum {
        num_of_tests = 31
    };

    try {
        CMsgHandlerGuard guard(*GetDS().GetDriverContext());

        sql = "sleep 0 kill";
        const unsigned orig_conn_num = GetDS().GetDriverContext()->NofConnections();

        CDBSetConnParams params(
            "LINK_OS_INTERNAL",
            "anyone",
            "allowed",
            125,
            GetArgs().GetConnParams()
        );

        for (unsigned i = 0; i < num_of_tests; ++i) {
            // Start a connection scope ...
            {
                bool conn_killed = false;

                auto_ptr<CDB_Connection> auto_conn;

                auto_conn.reset(GetDS().GetDriverContext()->MakeConnection(params));

                // kill connection ...
                try {
                    auto_ptr<CDB_LangCmd> auto_stmt(auto_conn->LangCmd(sql));
                    auto_stmt->Send();
                    auto_stmt->DumpResults();
                } catch (const CDB_Exception& _DEBUG_ARG(ex)) {
                    // Ignore it ...
                    _TRACE(ex);
                }

                try {
                    auto_ptr<CDB_RPCCmd> auto_stmt(auto_conn->RPC("sp_who"));
                    auto_stmt->Send();
                    auto_stmt->DumpResults();
                } catch (const CDB_Exception& _DEBUG_ARG(ex)) {
                    _TRACE(ex);
                    conn_killed = true;
                }

                BOOST_CHECK(conn_killed);
            }

            BOOST_CHECK_EQUAL(
                orig_conn_num,
                GetDS().GetDriverContext()->NofConnections()
                );
        }
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}

////////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(Test_N_Connections)
{
    {
        enum {
            eN = 5
        };

        auto_ptr<IConnection> auto_conns[eN + 1];

        // There is already 1 connections - so +1
        CDriverManager::GetInstance().SetMaxConnect(eN + 1);
        for (int i = 0; i < eN; ++i) {
            auto_ptr<IConnection> auto_conn(GetDS().CreateConnection(CONN_OWNERSHIP));
            auto_conn->Connect(GetArgs().GetConnParams());

            auto_ptr<IStatement> auto_stmt(auto_conn->GetStatement());
            auto_stmt->ExecuteUpdate("SELECT @@version");

            auto_conns[i] = auto_conn;
        }

        try {
            auto_conns[eN].reset(GetDS().CreateConnection(CONN_OWNERSHIP));
            auto_conns[eN]->Connect(GetArgs().GetConnParams());

            BOOST_FAIL("Connection above limit is created");
        }
        catch (CDB_Exception&) {
            // exception thrown - it's ok
            LOG_POST("Connection above limit is rejected - that is ok");
        }
    }

    // This test is not supposed to be run every day.
    if (false) {
        enum {
            eN = 50
        };

        auto_ptr<IConnection> auto_conns[eN];

        // There are already 2 connections - so +2
        CDriverManager::GetInstance().SetMaxConnect(eN + 2);
        for (int i = 0; i < eN; ++i) {
            auto_ptr<IConnection> auto_conn(GetDS().CreateConnection(CONN_OWNERSHIP));
            auto_conn->Connect(GetArgs().GetConnParams());

            auto_ptr<IStatement> auto_stmt(auto_conn->GetStatement());
            auto_stmt->ExecuteUpdate("SELECT @@version");

            auto_conns[i] = auto_conn;
        }
    }
    else {
        GetArgs().PutMsgDisabled("Large connections amount");
    }
}

////////////////////////////////////////////////////////////////////////////////
static
IDBServiceMapper*
MakeCDBUDPriorityMapper01(const IRegistry* registry)
{
    TSvrRef server01(new CDBServer("MSDEV1"));
    TSvrRef server02(new CDBServer("MSDEV2"));
    const string service_name("TEST_SERVICE_01");

    auto_ptr<CDBUDPriorityMapper> mapper(new CDBUDPriorityMapper(registry));

    mapper->Add(service_name, server01);
    mapper->Add(service_name, server02);
    mapper->Add(service_name, server01);
    mapper->Add(service_name, server01);
//     mapper->Add(service_name, server01);
//     mapper->Add(service_name, server01);
//     mapper->Add(service_name, server01);
//     mapper->Add(service_name, server01);
//     mapper->Add(service_name, server01);

    return mapper.release();
}

typedef IDBConnectionFactory* (*TDBConnectionFactoryFactory)
            (IDBServiceMapper::TFactory svc_mapper_factory);

////////////////////////////////////////////////////////////////////////////////
static void
s_Check_Validator(TDBConnectionFactoryFactory factory,
                  IConnValidator& validator)
{
    TSvrRef server01(new CDBServer("MSDEV2"));
    const string service_name("TEST_SERVICE_01");
    auto_ptr<IConnection> conn;

    // Install new mapper ...
    ncbi::CDbapiConnMgr::Instance().SetConnectionFactory(
        factory(MakeCDBUDPriorityMapper01)
    );

    // Create connection ...
    conn.reset(GetDS().CreateConnection(CONN_OWNERSHIP));
    BOOST_CHECK(conn.get() != NULL);

    // There are only 3 of server01 ...
    for (int i = 0; i < 12; ++i) {
        conn->ConnectValidated(
            validator,
            GetArgs().GetUserName(),
            GetArgs().GetUserPassword(),
            service_name
            );

        string server_name = conn->GetCDB_Connection()->ServerName();
//         LOG_POST(Warning << "Connected to: " << server_name);
        // server02 shouldn't be reported ...
        BOOST_CHECK_EQUAL(server_name, server01->GetName());
        conn->Close();
    }
}

////////////////////////////////////////////////////////////////////////////////
static
IDBConnectionFactory*
CDBConnectionFactoryFactory(IDBServiceMapper::TFactory svc_mapper_factory)
{
    return new CDBConnectionFactory(svc_mapper_factory);
}

///////////////////////////////////////////////////////////////////////////////
static
IDBConnectionFactory*
CDBRedispatchFactoryFactory(IDBServiceMapper::TFactory svc_mapper_factory)
{
    return new CDBRedispatchFactory(svc_mapper_factory);
}

////////////////////////////////////////////////////////////////////////////////
class CValidator : public CTrivialConnValidator
{
public:
    CValidator(const string& db_name,
               int attr = eDefaultValidateAttr)
        : CTrivialConnValidator(db_name, attr)
        {
        }

    virtual EConnStatus ValidateException(const CDB_Exception& ex)
        {
            return eTempInvalidConn;
        }
};

///////////////////////////////////////////////////////////////////////////////
class CValidator01 : public CTrivialConnValidator
{
public:
    CValidator01(const string& db_name,
                int attr = eDefaultValidateAttr)
    : CTrivialConnValidator(db_name, attr)
    {
    }

public:
    virtual IConnValidator::EConnStatus Validate(CDB_Connection& conn)
    {
        // Try to change a database ...
        try {
            conn.SetDatabaseName(GetDBName());
        }
        catch(const CDB_Exception& _DEBUG_ARG(ex)) {
            // LOG_POST(Warning << "Db not accessible: " << GetDBName() <<
            //             ", Server name: " << conn.ServerName());
            _TRACE(ex);
            return eTempInvalidConn;
            // return eInvalidConn;
        }

        if (GetAttr() & eCheckSysobjects) {
            auto_ptr<CDB_LangCmd> set_cmd(conn.LangCmd("SELECT id FROM sysobjects"));
            set_cmd->Send();
            set_cmd->DumpResults();
        }

        // Go back to the original (master) database ...
        if (GetAttr() & eRestoreDefaultDB) {
            conn.SetDatabaseName("master");
        }

        // All exceptions are supposed to be caught and processed by
        // CDBConnectionFactory ...
        return eValidConn;
    }

    virtual EConnStatus ValidateException(const CDB_Exception& ex)
    {
        return eTempInvalidConn;
    }

    virtual string GetName(void) const
    {
        return "CValidator01" + GetDBName();
    }

};

////////////////////////////////////////////////////////////////////////////////
class CValidator02 : public CTrivialConnValidator
{
public:
    CValidator02(const string& db_name,
                int attr = eDefaultValidateAttr)
    : CTrivialConnValidator(db_name, attr)
    {
    }

public:
    virtual IConnValidator::EConnStatus Validate(CDB_Connection& conn)
    {
        // Try to change a database ...
        try {
            conn.SetDatabaseName(GetDBName());
        }
        catch(const CDB_Exception& _DEBUG_ARG(ex)) {
            // LOG_POST(Warning << "Db not accessible: " << GetDBName() <<
            //             ", Server name: " << conn.ServerName());
            _TRACE(ex);
            return eTempInvalidConn;
            // return eInvalidConn;
        }

        if (GetAttr() & eCheckSysobjects) {
            auto_ptr<CDB_LangCmd> set_cmd(conn.LangCmd("SELECT id FROM sysobjects"));
            set_cmd->Send();
            set_cmd->DumpResults();
        }

        // Go back to the original (master) database ...
        if (GetAttr() & eRestoreDefaultDB) {
            conn.SetDatabaseName("master");
        }

        // All exceptions are supposed to be caught and processed by
        // CDBConnectionFactory ...
        return eValidConn;
    }

    virtual EConnStatus ValidateException(const CDB_Exception& ex)
    {
        return eInvalidConn;
    }

    virtual string GetName(void) const
    {
        return "CValidator02" + GetDBName();
    }
};

////////////////////////////////////////////////////////////////////////////////
static void
s_CheckConnFactory(TDBConnectionFactoryFactory factory_factory)
{
    const string db_name("CppDocTest"); // This database should exist in MSDEV2, and not in MSDEV1

    // CTrivialConnValidator ...
    {
        CTrivialConnValidator validator(db_name);

        s_Check_Validator(factory_factory, validator);
    }

    // Same as before but with a customized validator ...
    {
        // Connection validator to check against DBAPI_Sample ...
        CValidator validator(db_name);

        s_Check_Validator(factory_factory, validator);
    }

    // Another customized validator ...
    {

        // Connection validator to check against DBAPI_Sample ...
        CValidator01 validator(db_name);

        s_Check_Validator(factory_factory, validator);
    }

    // One more ...
    {
        // Connection validator to check against DBAPI_Sample ...
        CValidator02 validator(db_name);

        s_Check_Validator(factory_factory, validator);
    }
}

////////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(Test_ConnFactory)
{
    enum {num_of_tests = 128};

    try {
        TSvrRef server01(new CDBServer("msdev2"));
        TSvrRef server02(new CDBServer("msdev1"));
        TSvrRef server03(new CDBServer("mssql67"));

        // Check CDBUDPriorityMapper ...
        {
            const string service_name("TEST_SERVICE_01");
            auto_ptr<CDBUDPriorityMapper> mapper(new CDBUDPriorityMapper());

            mapper->Add(service_name, server01);
            mapper->Add(service_name, server02);
            mapper->Add(service_name, server03);

            for (int i = 0; i < num_of_tests; ++i) {
                BOOST_CHECK(mapper->GetServer(service_name) == server01);
                BOOST_CHECK(mapper->GetServer(service_name) == server02);
                BOOST_CHECK(mapper->GetServer(service_name) == server03);

                BOOST_CHECK(mapper->GetServer(service_name) != server02);
                BOOST_CHECK(mapper->GetServer(service_name) != server03);
                BOOST_CHECK(mapper->GetServer(service_name) != server01);
            }
        }

        // Check CDBUDRandomMapper ...
        // DBUDRandomMapper is currently brocken ...
        if (false) {
            const string service_name("TEST_SERVICE_02");
            auto_ptr<CDBUDRandomMapper> mapper(new CDBUDRandomMapper());

            mapper->Add(service_name, server01);
            mapper->Add(service_name, server02);
            mapper->Add(service_name, server03);

            size_t num_server01 = 0;
            size_t num_server02 = 0;
            size_t num_server03 = 0;

            for (int i = 0; i < num_of_tests; ++i) {
                TSvrRef cur_server = mapper->GetServer(service_name);

                if (cur_server == server01) {
                    ++num_server01;
                } else if (cur_server == server02) {
                    ++num_server02;
                } else if (cur_server == server03) {
                    ++num_server03;
                } else {
                    BOOST_FAIL("Unknown server.");
                }
            }

            BOOST_CHECK_EQUAL(num_server01, num_server02);
            BOOST_CHECK_EQUAL(num_server02, num_server03);
            BOOST_CHECK_EQUAL(num_server03, num_server01);
        }

        // Check CDBConnectionFactory ...
        s_CheckConnFactory(CDBConnectionFactoryFactory);

        // Check CDBRedispatchFactory ...
        s_CheckConnFactory(CDBRedispatchFactoryFactory);

        // Future development ...
//         ncbi::CDbapiConnMgr::Instance().SetConnectionFactory(
//             new CDBConnectionFactory(
//                 mapper.release()
//             )
//         );

        // Restore original state ...
        DBLB_INSTALL_DEFAULT();
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}

////////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(Test_ConnPool)
{

    const string pool_name("test_pool");
    string sql;

    try {
        // Create pooled connection ...
        {
            auto_ptr<CDB_Connection> auto_conn(
                GetDS().GetDriverContext()->Connect(
                    GetArgs().GetServerName(),
                    GetArgs().GetUserName(),
                    GetArgs().GetUserPassword(),
                    0,
                    true, // reusable
                    pool_name
                    )
                );
            BOOST_CHECK( auto_conn.get() != NULL );

            sql  = "select @@version";

            auto_ptr<CDB_LangCmd> auto_stmt( auto_conn->LangCmd(sql) );
            BOOST_CHECK( auto_stmt.get() != NULL );

            bool rc = auto_stmt->Send();
            BOOST_CHECK( rc );
            auto_stmt->DumpResults();
        }

        // Get pooled connection ...
        {
            auto_ptr<CDB_Connection> auto_conn(
                GetDS().GetDriverContext()->Connect(
                    "",
                    "",
                    "",
                    0,
                    true, // reusable
                    pool_name
                    )
                );
            BOOST_CHECK( auto_conn.get() != NULL );

            sql  = "select @@version";

            auto_ptr<CDB_LangCmd> auto_stmt( auto_conn->LangCmd(sql) );
            BOOST_CHECK( auto_stmt.get() != NULL );

            bool rc = auto_stmt->Send();
            BOOST_CHECK( rc );
            auto_stmt->DumpResults();
        }
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}

////////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(Test_ConnParams)
{
    // Checking parser ...
    if (true) {

        // CDBUriConnParams ...
        {
            CDBUriConnParams params("dbapi:ctlib://myuser:mypswd@MAPVIEW_LOAD:1433/AlignModel?tds_version=42&client_charset=UTF8");

            BOOST_CHECK_EQUAL(params.GetDriverName(), string("ctlib"));
            BOOST_CHECK_EQUAL(params.GetUserName(), string("myuser"));
            BOOST_CHECK_EQUAL(params.GetPassword(), string("mypswd"));
            BOOST_CHECK_EQUAL(params.GetServerName(), string("MAPVIEW_LOAD"));
            BOOST_CHECK_EQUAL(params.GetPort(), 1433U);
            BOOST_CHECK_EQUAL(params.GetDatabaseName(), string("AlignModel"));
        }

        {
            CDBUriConnParams params("dbapi:ftds://myuser@MAPVIEW_LOAD/AlignModel");

            BOOST_CHECK_EQUAL(params.GetDriverName(), string("ftds"));
            BOOST_CHECK_EQUAL(params.GetUserName(), string("myuser"));
            BOOST_CHECK_EQUAL(params.GetPassword(), string("allowed"));
            BOOST_CHECK_EQUAL(params.GetServerName(), string("MAPVIEW_LOAD"));
            BOOST_CHECK_EQUAL(params.GetPort(), 1433U);
            BOOST_CHECK_EQUAL(params.GetDatabaseName(), string("AlignModel"));
        }

        {
            CDBUriConnParams params("dbapi://myuser@MAPVIEW_LOAD/AlignModel");

            BOOST_CHECK_EQUAL(params.GetUserName(), string("myuser"));
            BOOST_CHECK_EQUAL(params.GetPassword(), string("allowed"));
            BOOST_CHECK_EQUAL(params.GetServerName(), string("MAPVIEW_LOAD"));
            BOOST_CHECK_EQUAL(params.GetPort(), 1433U);
            BOOST_CHECK_EQUAL(params.GetDatabaseName(), string("AlignModel"));
        }

        {
            CDBUriConnParams params("dbapi://myuser:allowed@MAPVIEW_LOAD/AlignModel");

            BOOST_CHECK_EQUAL(params.GetUserName(), string("myuser"));
            BOOST_CHECK_EQUAL(params.GetPassword(), string("allowed"));
            BOOST_CHECK_EQUAL(params.GetServerName(), string("MAPVIEW_LOAD"));
            BOOST_CHECK_EQUAL(params.GetPort(), 1433U);
            BOOST_CHECK_EQUAL(params.GetDatabaseName(), string("AlignModel"));
        }

        {
            CDBUriConnParams params("dbapi://myuser:allowed@MAPVIEW_LOAD");

            BOOST_CHECK_EQUAL(params.GetUserName(), string("myuser"));
            BOOST_CHECK_EQUAL(params.GetPassword(), string("allowed"));
            BOOST_CHECK_EQUAL(params.GetServerName(), string("MAPVIEW_LOAD"));
            BOOST_CHECK_EQUAL(params.GetPort(), 1433U);
            BOOST_CHECK_EQUAL(params.GetDatabaseName(), string(""));
        }

        // CDB_ODBC_ConnParams ..
        {
            CDB_ODBC_ConnParams params("Driver={SQLServer};Server=DBAPI_MS_TEST;Database=DBAPI_Sample;Uid=anyone;Pwd=allowed;");

            BOOST_CHECK_EQUAL(params.GetDriverName(), string("{SQLServer}"));
            BOOST_CHECK_EQUAL(params.GetUserName(), string("anyone"));
            BOOST_CHECK_EQUAL(params.GetPassword(), string("allowed"));
            BOOST_CHECK_EQUAL(params.GetServerName(), string("DBAPI_MS_TEST"));
            BOOST_CHECK_EQUAL(params.GetPort(), 1433U);
            BOOST_CHECK_EQUAL(params.GetDatabaseName(), string("DBAPI_Sample"));
        }
    }


    // Checking ability to connect using different connection-parameter classes ...
    if (true) {
        // CDBUriConnParams ...
        {
            {
                CDBUriConnParams params(
                        "dbapi:" + GetArgs().GetDriverName() +
                        "://" + GetArgs().GetUserName() +
                        ":" + GetArgs().GetUserPassword() +
                        "@" + GetArgs().GetServerName()
                        );

                IDataSource* ds = GetDM().MakeDs(params);
                auto_ptr<IConnection> conn(ds->CreateConnection(eTakeOwnership));
                conn->Connect(params);
                auto_ptr<IStatement> auto_stmt(conn->GetStatement());
                auto_stmt->ExecuteUpdate("SELECT @@version");
                // m_DM.DestroyDs(ds); // DO NOT destroy data source! That will
                // crash application.
            }

            {
                CDBUriConnParams params(
                        "dbapi:" // No driver name ...
                        "//" + GetArgs().GetUserName() +
                        ":" + GetArgs().GetUserPassword() +
                        "@" + GetArgs().GetServerName()
                        );

                IDataSource* ds = GetDM().MakeDs(params);
                auto_ptr<IConnection> conn(ds->CreateConnection(eTakeOwnership));
                conn->Connect(params);
                auto_ptr<IStatement> auto_stmt(conn->GetStatement());
                auto_stmt->ExecuteUpdate("SELECT @@version");
                // m_DM.DestroyDs(ds); // DO NOT destroy data source! That will
                // crash application.
            }

            {
                CDBUriConnParams params(
                        "dbapi:" // No driver name ...
                        "//" + GetArgs().GetUserName() +
                        // No password ...
                        "@" + GetArgs().GetServerName()
                        );

                IDataSource* ds = GetDM().MakeDs(params);
                auto_ptr<IConnection> conn(ds->CreateConnection(eTakeOwnership));
                conn->Connect(params);
                auto_ptr<IStatement> auto_stmt(conn->GetStatement());
                auto_stmt->ExecuteUpdate("SELECT @@version");
                // m_DM.DestroyDs(ds); // DO NOT destroy data source! That will
                // crash application.
            }

        }

        // CDB_ODBC_ConnParams ...
        {
            {
                CDB_ODBC_ConnParams params(
                        "DRIVER=" + GetArgs().GetDriverName() +
                        ";UID=" + GetArgs().GetUserName() +
                        ";PWD=" + GetArgs().GetUserPassword() +
                        ";SERVER=" + GetArgs().GetServerName()
                        );

                IDataSource* ds = GetDM().MakeDs(params);
                auto_ptr<IConnection> conn(ds->CreateConnection(eTakeOwnership));
                conn->Connect(params);
                auto_ptr<IStatement> auto_stmt(conn->GetStatement());
                auto_stmt->ExecuteUpdate("SELECT @@version");
                // m_DM.DestroyDs(ds); // DO NOT destroy data source! That will
                // crash application.
            }

            {
                CDB_ODBC_ConnParams params(
                        // No driver ...
                        ";UID=" + GetArgs().GetUserName() +
                        ";PWD=" + GetArgs().GetUserPassword() +
                        ";SERVER=" + GetArgs().GetServerName()
                        );

                IDataSource* ds = GetDM().MakeDs(params);
                auto_ptr<IConnection> conn(ds->CreateConnection(eTakeOwnership));
                conn->Connect(params);
                auto_ptr<IStatement> auto_stmt(conn->GetStatement());
                auto_stmt->ExecuteUpdate("SELECT @@version");
                // m_DM.DestroyDs(ds); // DO NOT destroy data source! That will
                // crash application.
            }

            {
                CDB_ODBC_ConnParams params(
                        // No driver ...
                        ";UID=" + GetArgs().GetUserName() +
                        // No password ...
                        ";SERVER=" + GetArgs().GetServerName()
                        );

                IDataSource* ds = GetDM().MakeDs(params);
                auto_ptr<IConnection> conn(ds->CreateConnection(eTakeOwnership));
                conn->Connect(params);
                auto_ptr<IStatement> auto_stmt(conn->GetStatement());
                auto_stmt->ExecuteUpdate("SELECT @@version");
                // m_DM.DestroyDs(ds); // DO NOT destroy data source! That will
                // crash application.
            }

        }
    }

    // Check CDBEnvConnParams ...
    {
        CDBUriConnParams uri_params("dbapi://wrong_user:wrong_pswd@wrong_server/wrong_db");
        CDBEnvConnParams params(uri_params);
        CNcbiEnvironment env;

        env.Set("DBAPI_SERVER", GetArgs().GetServerName());
        env.Set("DBAPI_DATABASE", "DBAPI_Sample");
        env.Set("DBAPI_USER", GetArgs().GetUserName());
        env.Set("DBAPI_PASSWORD", GetArgs().GetUserPassword());

        IDataSource* ds = GetDM().MakeDs(params);
        auto_ptr<IConnection> conn(ds->CreateConnection(eTakeOwnership));
        conn->Connect(params);
        auto_ptr<IStatement> auto_stmt(conn->GetStatement());
        auto_stmt->ExecuteUpdate("SELECT @@version");

        env.Reset();
    }

    // CDBInterfacesFileConnParams ...
    {
        CDBUriConnParams uri_params(
                "dbapi:" // No driver name ...
                "//" + GetArgs().GetUserName() +
                ":" + GetArgs().GetUserPassword() +
                "@" + GetArgs().GetServerName()
                );

        CDBInterfacesFileConnParams params(uri_params);

        IDataSource* ds = GetDM().MakeDs(params);
        auto_ptr<IConnection> conn(ds->CreateConnection(eTakeOwnership));
        conn->Connect(params);
        auto_ptr<IStatement> auto_stmt(conn->GetStatement());
        auto_stmt->ExecuteUpdate("SELECT @@version");
    }

}

///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(Test_ConnParamsDatabase)
{
    try {
        const string target_db_name("DBAPI_Sample");

        // Check method Connect() ...
        {
            auto_ptr<IConnection> conn(GetDS().CreateConnection( eTakeOwnership ));
            conn->Connect(
                GetArgs().GetConnParams().GetUserName(),
                GetArgs().GetConnParams().GetPassword(),
                GetArgs().GetConnParams().GetServerName(),
                target_db_name);

            auto_ptr<IStatement> auto_stmt( conn->GetStatement() );

            IResultSet* rs = auto_stmt->ExecuteQuery("select db_name()");
            BOOST_CHECK( rs != NULL );
            BOOST_CHECK( rs->Next() );
            const  string db_name = rs->GetVariant(1).GetString();
            BOOST_CHECK_EQUAL(db_name, target_db_name);
        }

        // Check method ConnectValidated() ...
        {
            auto_ptr<IConnection> conn(GetDS().CreateConnection( eTakeOwnership ));
            CTrivialConnValidator validator(target_db_name);
            conn->ConnectValidated(
                validator,
                GetArgs().GetConnParams().GetUserName(),
                GetArgs().GetConnParams().GetPassword(),
                GetArgs().GetConnParams().GetServerName(),
                target_db_name);

            auto_ptr<IStatement> auto_stmt( conn->GetStatement() );

            IResultSet* rs = auto_stmt->ExecuteQuery("select db_name()");
            BOOST_CHECK( rs != NULL );
            BOOST_CHECK( rs->Next() );
            const  string db_name = rs->GetVariant(1).GetString();
            BOOST_CHECK_EQUAL(db_name, target_db_name);
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
        auto_ptr<IConnection> conn(GetDS().CreateConnection( CONN_OWNERSHIP ));
        conn->Connect(
            GetArgs().GetConnParams().GetUserName(),
            GetArgs().GetConnParams().GetPassword(),
            GetArgs().GetConnParams().GetServerName(),
            target_db_name);

        // Clone connection ...
        auto_ptr<IConnection> new_conn(conn->CloneConnection(eTakeOwnership));
        auto_ptr<IStatement> auto_stmt( new_conn->GetStatement() );

        // Check that database was set correctly with the new connection ...
        {
            IResultSet* rs = auto_stmt->ExecuteQuery("select db_name()");
            BOOST_CHECK( rs != NULL );
            BOOST_CHECK( rs->Next() );
            const  string db_name = rs->GetVariant(1).GetString();
            BOOST_CHECK_EQUAL(db_name, target_db_name);
        }
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}

class CMirrorValidator : public CTrivialConnValidator
{
public:
    CMirrorValidator(const string& db_name, int attr = eDefaultValidateAttr)
        : CTrivialConnValidator(db_name, attr)
    {
    }

    virtual string GetName(void) const
    {
        return "CMirrorValidator";
    }
};

BOOST_AUTO_TEST_CASE(Test_Mirrors)
{
    CMirrorValidator validator1("DBAPI_ConnectionTest1");
    CMirrorValidator validator2("DBAPI_ConnectionTestt2");

    string username = "DBAPI_test";
    string password = "allowed";
    string server = "MSDEVVV";

    // Create new connection ...
    AutoPtr<IConnection> conn;

    int i = 0;
    LOG_POST(++i << " connection");
    try {
        // Create new connection ...
        conn = GetDS().CreateConnection( CONN_OWNERSHIP );
        conn->ConnectValidated(validator1, username, password, server);
    }
    catch(const CException& ex) {
        BOOST_ERROR(ex.what());
    }

    LOG_POST(++i << " connection");
    try {
        // Create new connection ...
        conn = GetDS().CreateConnection( CONN_OWNERSHIP );
        conn->ConnectValidated(validator1, username, password, server);
    }
    catch(const CException& ex) {
        BOOST_ERROR(ex.what());
    }

    LOG_POST(++i << " connection");
    try {
        // Create new connection ...
        conn = GetDS().CreateConnection( CONN_OWNERSHIP );
        conn->ConnectValidated(validator2, username, password, server);
    }
    catch(const CException& ex) {
        BOOST_ERROR(ex.what());
    }

    SleepSec(11);
    LOG_POST(++i << " connection");
    try {
        // Create new connection ...
        conn = GetDS().CreateConnection( CONN_OWNERSHIP );
        conn->ConnectValidated(validator2, username, password, server);
    }
    catch(const CException& ex) {
        BOOST_ERROR(ex.what());
    }

    LOG_POST(++i << " connection");
    try {
        // Create new connection ...
        conn = GetDS().CreateConnection( CONN_OWNERSHIP );
        conn->ConnectValidated(validator1, username, password, server);
    }
    catch(const CException& ex) {
        BOOST_ERROR(ex.what());
    }

    SleepSec(31);
    LOG_POST(++i << " connection");
    try {
        // Create new connection ...
        conn = GetDS().CreateConnection( CONN_OWNERSHIP );
        conn->ConnectValidated(validator1, username, password, server);
    }
    catch(const CException& ex) {
        BOOST_ERROR(ex.what());
    }

    SleepSec(11);
    LOG_POST(++i << " connection");
    try {
        // Create new connection ...
        conn = GetDS().CreateConnection( CONN_OWNERSHIP );
        conn->ConnectValidated(validator2, username, password, server);
    }
    catch(const CException& ex) {
        BOOST_ERROR(ex.what());
    }

    LOG_POST(++i << " connection");
    try {
        // Create new connection ...
        conn = GetDS().CreateConnection( CONN_OWNERSHIP );
        conn->ConnectValidated(validator2, username, password, server);
    }
    catch(const CException& ex) {
        BOOST_ERROR(ex.what());
    }
}

// For this test to work one should change kDefaultResourceInfoPath inside
// util/resource_info.cpp to appropriate test value and set
// conn_use_encrypt_data value in section dbapi to true in ini-file.
BOOST_AUTO_TEST_CASE(Test_EncryptData)
{
    try {
        TDbapi_ConnUseEncryptData::SetDefault(true);

        CNcbiResourceInfoFile file(CNcbiResourceInfoFile::GetDefaultFileName());
        string app_name = CNcbiApplication::Instance()->GetProgramDisplayName();

        typedef CNcbiResourceInfo::TExtraValuesMap TExtraMap;
        typedef TExtraMap::value_type              TExtraPair;

        {{
            CNcbiResourceInfo& info = file.GetResourceInfo_NC(app_name + "/some_user@some_server", "some_passwd");
            info.SetValue("allowed");
            TExtraMap& extra = info.GetExtraValues_NC().GetPairs();
            extra.insert(TExtraPair("username", "DBAPI_test"));
            extra.insert(TExtraPair("server",   "MSDEV1"));
            extra.insert(TExtraPair("database", ""));
            file.SaveFile();

            CDBDefaultConnParams params("some_server", "some_user", "some_passwd");
            params.SetDriverName(GetArgs().GetDriverName());

            IDataSource* ds = GetDM().MakeDs(params);
            auto_ptr<IConnection> conn(ds->CreateConnection(eTakeOwnership));
            conn->Connect(params);
            auto_ptr<IStatement> auto_stmt(conn->GetStatement());
            auto_stmt->ExecuteUpdate("SELECT @@version");

            BOOST_CHECK_EQUAL(conn->GetDatabase(), "");
            BOOST_CHECK_EQUAL(conn->GetCDB_Connection()->Password(), "allowed");
            BOOST_CHECK_EQUAL(conn->GetCDB_Connection()->ServerName(), "MSDEV1");
            BOOST_CHECK_EQUAL(conn->GetCDB_Connection()->UserName(), "DBAPI_test");
        }}

        {{
            CNcbiResourceInfo& info = file.GetResourceInfo_NC(app_name + "/other_user@other_server:other_db", "other_passwd");
            info.SetValue("some_passwd");
            TExtraMap& extra = info.GetExtraValues_NC().GetPairs();
            extra.insert(TExtraPair("username", "some_user"));
            extra.insert(TExtraPair("server",   "some_server"));
            extra.insert(TExtraPair("database", ""));
            file.SaveFile();

            CDBDefaultConnParams params("other_server", "other_user", "other_passwd");
            params.SetDatabaseName("other_db");
            params.SetDriverName(GetArgs().GetDriverName());

            IDataSource* ds = GetDM().MakeDs(params);
            auto_ptr<IConnection> conn(ds->CreateConnection(eTakeOwnership));
            conn->Connect(params);
            auto_ptr<IStatement> auto_stmt(conn->GetStatement());
            auto_stmt->ExecuteUpdate("SELECT @@version");

            BOOST_CHECK_EQUAL(conn->GetDatabase(), "");
            BOOST_CHECK_EQUAL(conn->GetCDB_Connection()->Password(), "allowed");
            BOOST_CHECK_EQUAL(conn->GetCDB_Connection()->ServerName(), "MSDEV1");
            BOOST_CHECK_EQUAL(conn->GetCDB_Connection()->UserName(), "DBAPI_test");
        }}
    }
    catch (CException& ex) {
        BOOST_ERROR(ex.what());
    }

    TDbapi_ConnUseEncryptData::SetDefault(false);
}


END_NCBI_SCOPE
