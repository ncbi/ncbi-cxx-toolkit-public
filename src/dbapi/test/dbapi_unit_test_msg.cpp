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

////////////////////////////////////////////////////////////////////////////////
class CDB_UserHandler_Exception_NoThrow :
    public CDB_UserHandler
{
public:
    virtual ~CDB_UserHandler_Exception_NoThrow(void);

    virtual bool HandleIt(CDB_Exception* ex);
};


CDB_UserHandler_Exception_NoThrow::~CDB_UserHandler_Exception_NoThrow(void)
{
}

bool
CDB_UserHandler_Exception_NoThrow::HandleIt(CDB_Exception* ex)
{
    if (!ex)
        return false;

    // Ignore errors with ErrorCode set to 0
    // (this is related mostly to the FreeTDS driver)
    if (ex->GetDBErrCode() == 0)
        return true;

    // Do not throw exceptions ...
    // ex->Throw();

    return true;
}


///////////////////////////////////////////////////////////////////////////////
class CTestErrHandler : public CDB_UserHandler
{
public:
    // c-tor
    CTestErrHandler();
    // d-tor
    virtual ~CTestErrHandler();

public:
    // Return TRUE if "ex" is processed, FALSE if not (or if "ex" is NULL)
    virtual bool HandleIt(CDB_Exception* ex);
    virtual bool HandleAll(const TExceptions& exceptions);

    // Get current global "last-resort" error handler.
    // If not set, then the default will be "CDB_UserHandler_Default".
    // This handler is guaranteed to be valid up to the program termination,
    // and it will call the user-defined hs_Procandler last set by SetDefault().
    // NOTE:  never pass it to SetDefault, like:  "SetDefault(&GetDefault())"!
    static CDB_UserHandler& GetDefault(void);

    // Alternate the default global "last-resort" error handler.
    // Passing NULL will mean to ignore all errors that reach it.
    // Return previously set (or default-default if not set yet) handler.
    // The returned handler should be delete'd by the caller; the last set
    // handler will be delete'd automagically on the program termination.
    static CDB_UserHandler* SetDefault(CDB_UserHandler* h);

public:
    EDiagSev getMaxSeverity() {
        return m_max_severity;
    }
    bool GetSucceed(void) const
    {
        return m_Succeed;
    }
    void Init(void)
    {
        m_Succeed = false;
    }

private:
    EDiagSev m_max_severity;
    bool     m_Succeed;
};


CTestErrHandler::CTestErrHandler()
: m_max_severity(eDiag_Info)
, m_Succeed(false)
{
}


CTestErrHandler::~CTestErrHandler()
{
   ;
}


bool CTestErrHandler::HandleIt(CDB_Exception* ex)
{
    m_Succeed = true;

    if (ex && ex->GetSeverity() > m_max_severity)
    {
        m_max_severity = ex->GetSeverity();
    }

    // return false to find the next handler on the stack.
    // There is always one default stack

    return false;
}

bool CTestErrHandler::HandleAll(const TExceptions& exceptions)
{
    m_Succeed = true;
    ITERATE(TExceptions, it, exceptions) {
        CDB_Exception* ex = *it;
        if (ex && ex->GetSeverity() > m_max_severity)
        {
            m_max_severity = ex->GetSeverity();
        }
    }
    return false;
}


///////////////////////////////////////////////////////////////////////////////
// Throw CDB_Exception ...
static void
s_ES_01_Internal(IConnection& conn)
{
    auto_ptr<IStatement> auto_stmt( conn.GetStatement() );

    auto_stmt->SendSql( "select name from wrong table" );
    while( auto_stmt->HasMoreResults() ) {
        if( auto_stmt->HasRows() ) {
            auto_ptr<IResultSet> rs( auto_stmt->GetResultSet() );

            while( rs->Next() ) {
                // int col1 = rs->GetVariant(1).GetInt4();
            }
        }
    }
}


///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(Test_UserErrorHandler)
{
    try {
        // Set up an user-defined error handler ..

        // Push message handler into a context ...
        I_DriverContext* drv_context = GetDS().GetDriverContext();

        // Check PushCntxMsgHandler ...
        // PushCntxMsgHandler - Add message handler "h" to process 'context-wide' (not bound
        // to any particular connection) error messages.
        {
            auto_ptr<CTestErrHandler> drv_err_handler(new CTestErrHandler());

            auto_ptr<IConnection> local_conn( GetDS().CreateConnection() );
            local_conn->Connect(GetArgs().GetConnParams());

            drv_context->PushCntxMsgHandler( drv_err_handler.get() );

            // Connection process should be affected ...
            {
                // Create a new connection ...
                auto_ptr<IConnection> conn( GetDS().CreateConnection() );

                try {
                    conn->Connect(
                        "unknown",
                        "invalid",
                        GetArgs().GetServerName()
                        );
                }
                catch( const CDB_Exception& _DEBUG_ARG(ex)) {
                    // Ignore it
                    _TRACE(ex);
                    BOOST_CHECK( drv_err_handler->GetSucceed() );
                }
                catch( const CException& ) {
                    BOOST_FAIL( "CException was catched instead of CDB_Exception." );
                }
                catch( ... ) {
                    BOOST_FAIL( "... was catched instead of CDB_Exception." );
                }

                drv_err_handler->Init();
            }

            // Current connection should not be affected ...
            {
                try {
                    s_ES_01_Internal(*local_conn);
                }
                catch( const CDB_Exception& _DEBUG_ARG(ex)) {
                    // Ignore it
                    _TRACE(ex);
                    BOOST_CHECK( !drv_err_handler->GetSucceed() );
                }
            }

            // New connection should not be affected ...
            {
                // Create a new connection ...
                auto_ptr<IConnection> conn( GetDS().CreateConnection() );
                conn->Connect(GetArgs().GetConnParams());

                // Reinit the errot handler because it can be affected during connection.
                drv_err_handler->Init();

                try {
                    s_ES_01_Internal(*conn);
                }
                catch( const CDB_Exception& _DEBUG_ARG(ex)) {
                    // Ignore it
                    _TRACE(ex);
                    BOOST_CHECK( !drv_err_handler->GetSucceed() );
                }
            }

            // Push a message handler into a connection ...
            {
                auto_ptr<CTestErrHandler> msg_handler(new CTestErrHandler());

                local_conn->GetCDB_Connection()->PushMsgHandler(msg_handler.get());

                try {
                    s_ES_01_Internal(*local_conn);
                }
                catch( const CDB_Exception& _DEBUG_ARG(ex)) {
                    // Ignore it
                    _TRACE(ex);
                    BOOST_CHECK( msg_handler->GetSucceed() );
                }

                // Remove handler ...
                local_conn->GetCDB_Connection()->PopMsgHandler( msg_handler.get() );
            }

            // New connection should not be affected
            // after pushing a message handler into another connection
            {
                // Create a new connection ...
                auto_ptr<IConnection> conn( GetDS().CreateConnection() );
                conn->Connect(GetArgs().GetConnParams());

                // Reinit the errot handler because it can be affected during connection.
                drv_err_handler->Init();

                try {
                    s_ES_01_Internal(*conn);
                }
                catch( const CDB_Exception& _DEBUG_ARG(ex)) {
                    // Ignore it
                    _TRACE(ex);
                    BOOST_CHECK( !drv_err_handler->GetSucceed() );
                }
            }

            // Remove all inserted handlers ...
            drv_context->PopCntxMsgHandler( drv_err_handler.get() );
        }


        // Check PushDefConnMsgHandler ...
        // PushDefConnMsgHandler - Add `per-connection' err.message handler "h" to the stack of default
        // handlers which are inherited by all newly created connections.
        {
            auto_ptr<CTestErrHandler> drv_err_handler(new CTestErrHandler());


            auto_ptr<IConnection> local_conn( GetDS().CreateConnection() );
            local_conn->Connect(GetArgs().GetConnParams());

            drv_context->PushDefConnMsgHandler( drv_err_handler.get() );

            // Current connection should not be affected ...
            {
                try {
                    s_ES_01_Internal(*local_conn);
                }
                catch( const CDB_Exception& _DEBUG_ARG(ex)) {
                    // Ignore it
                    _TRACE(ex);
                    BOOST_CHECK( !drv_err_handler->GetSucceed() );
                }
            }

            // Push a message handler into a connection ...
            // This is supposed to be okay.
            {
                auto_ptr<CTestErrHandler> msg_handler(new CTestErrHandler());

                local_conn->GetCDB_Connection()->PushMsgHandler(msg_handler.get());

                try {
                    s_ES_01_Internal(*local_conn);
                }
                catch( const CDB_Exception& _DEBUG_ARG(ex)) {
                    // Ignore it
                    _TRACE(ex);
                    BOOST_CHECK( msg_handler->GetSucceed() );
                }

                // Remove handler ...
                local_conn->GetCDB_Connection()->PopMsgHandler(msg_handler.get());
            }


            ////////////////////////////////////////////////////////////////////////
            // Create a new connection.
            auto_ptr<IConnection> new_conn( GetDS().CreateConnection() );
            new_conn->Connect(GetArgs().GetConnParams());

            // New connection should be affected ...
            {
                try {
                    s_ES_01_Internal(*new_conn);
                }
                catch( const CDB_Exception& _DEBUG_ARG(ex)) {
                    // Ignore it
                    _TRACE(ex);
                    BOOST_CHECK( drv_err_handler->GetSucceed() );
                }
            }

            // Push a message handler into a connection ...
            // This is supposed to be okay.
            {
                auto_ptr<CTestErrHandler> msg_handler(new CTestErrHandler());

                new_conn->GetCDB_Connection()->PushMsgHandler( msg_handler.get() );

                try {
                    s_ES_01_Internal(*new_conn);
                }
                catch( const CDB_Exception& _DEBUG_ARG(ex)) {
                    // Ignore it
                    _TRACE(ex);
                    BOOST_CHECK( msg_handler->GetSucceed() );
                }

                // Remove handler ...
                new_conn->GetCDB_Connection()->PopMsgHandler( msg_handler.get() );
            }

            // New connection should be affected
            // after pushing a message handler into another connection
            {
                // Create a new connection ...
                auto_ptr<IConnection> conn( GetDS().CreateConnection() );
                conn->Connect(GetArgs().GetConnParams());

                try {
                    s_ES_01_Internal(*conn);
                }
                catch( const CDB_Exception& _DEBUG_ARG(ex)) {
                    // Ignore it
                    _TRACE(ex);
                    BOOST_CHECK( drv_err_handler->GetSucceed() );
                }
            }

            // Remove handlers ...
            drv_context->PopDefConnMsgHandler( drv_err_handler.get() );
        }

        // SetLogStream ...
        {
            {
                IConnection* conn = NULL;

                // Enable multiexception ...
                GetDS().SetLogStream(0);

                try {
                    // Create a new connection ...
                    conn = GetDS().CreateConnection();
                } catch(...)
                {
                    delete conn;
                }

                GetDS().SetLogStream(&cerr);
            }

            {
                IConnection* conn = NULL;

                // Enable multiexception ...
                GetDS().SetLogStream(0);

                try {
                    // Create a new connection ...
                    conn = GetDS().CreateConnection();

                    GetDS().SetLogStream(&cerr);
                } catch(...)
                {
                    delete conn;
                }
            }
        }
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}

///////////////////////////////////////////////////////////////////////////////
// User error handler life-time ...
BOOST_AUTO_TEST_CASE(Test_UserErrorHandler_LT)
{
    I_DriverContext* drv_context = GetDS().GetDriverContext();

    // Context. eNoOwnership ...
    {
        // auto_ptr
        // Use it once ...
        {
            auto_ptr<CTestErrHandler> drv_err_handler(new CTestErrHandler());

            // Add handler ...
            drv_context->PushCntxMsgHandler(drv_err_handler.get());
            // Remove handler ...
            drv_context->PopCntxMsgHandler(drv_err_handler.get());
        }

        // auto_ptr
        // Use it twice ...
        {
            auto_ptr<CTestErrHandler> drv_err_handler(new CTestErrHandler());

            // Add handler ...
            drv_context->PushCntxMsgHandler(drv_err_handler.get());
            drv_context->PushDefConnMsgHandler(drv_err_handler.get());
            // Remove handler ...
            drv_context->PopCntxMsgHandler(drv_err_handler.get());
            drv_context->PopDefConnMsgHandler(drv_err_handler.get());
        }

        // CRef
        // Use it once ...
        {
            CRef<CTestErrHandler> drv_err_handler(new CTestErrHandler());

            // Add handler ...
            drv_context->PushCntxMsgHandler(drv_err_handler);
            // Remove handler ...
            drv_context->PopCntxMsgHandler(drv_err_handler);
        }

        // CRef
        // Use it twice ...
        {
            CRef<CTestErrHandler> drv_err_handler(new CTestErrHandler());

            // Add handler ...
            drv_context->PushCntxMsgHandler(drv_err_handler);
            drv_context->PushDefConnMsgHandler(drv_err_handler);
            // Remove handler ...
            drv_context->PopCntxMsgHandler(drv_err_handler);
            drv_context->PopDefConnMsgHandler(drv_err_handler);
        }
    }

    // Connection. eNoOwnership ...
    {
        // auto_ptr
        {
            auto_ptr<CTestErrHandler> auto_msg_handler(new CTestErrHandler());

            // One connection ...
            {
                // Create new connection ...
                auto_ptr<IConnection> new_conn(GetDS().CreateConnection());
                auto_ptr<CTestErrHandler> msg_handler(new CTestErrHandler());

                new_conn->Connect(GetArgs().GetConnParams());

                // Add handler ...
                new_conn->GetCDB_Connection()->PushMsgHandler(msg_handler.get());
                // Remove handler ...
                new_conn->GetCDB_Connection()->PopMsgHandler(msg_handler.get());
            }

            // Two connections ...
            {
                // Create new connection ...
                auto_ptr<IConnection> new_conn1(GetDS().CreateConnection());
                auto_ptr<IConnection> new_conn2(GetDS().CreateConnection());
                auto_ptr<CTestErrHandler> msg_handler(new CTestErrHandler());

                new_conn1->Connect(GetArgs().GetConnParams());
                new_conn2->Connect(GetArgs().GetConnParams());

                // Add handler ...
                new_conn1->GetCDB_Connection()->PushMsgHandler(msg_handler.get());
                new_conn2->GetCDB_Connection()->PushMsgHandler(msg_handler.get());
                // Remove handler ...
                new_conn1->GetCDB_Connection()->PopMsgHandler(msg_handler.get());
                new_conn2->GetCDB_Connection()->PopMsgHandler(msg_handler.get());
            }

            // One connection ...
            {
                auto_ptr<IConnection> conn1(GetDS().CreateConnection());

                conn1->Connect(GetArgs().GetConnParams());

                // Add handler ...
                conn1->GetCDB_Connection()->PushMsgHandler(auto_msg_handler.get());
                // Remove handler ...
                conn1->GetCDB_Connection()->PopMsgHandler(auto_msg_handler.get());
            }

            // Two connections ...
            {
                auto_ptr<IConnection> conn1(GetDS().CreateConnection());
                auto_ptr<IConnection> conn2(GetDS().CreateConnection());

                conn1->Connect(GetArgs().GetConnParams());
                conn2->Connect(GetArgs().GetConnParams());

                // Add handler ...
                conn1->GetCDB_Connection()->PushMsgHandler(auto_msg_handler.get());
                conn2->GetCDB_Connection()->PushMsgHandler(auto_msg_handler.get());
                // Remove handler ...
                conn1->GetCDB_Connection()->PopMsgHandler(auto_msg_handler.get());
                conn2->GetCDB_Connection()->PopMsgHandler(auto_msg_handler.get());
            }
        }

        // CRef
        {
            CRef<CTestErrHandler> cref_msg_handler(new CTestErrHandler());

            // One connection ...
            {
                // Create new connection ...
                auto_ptr<IConnection> new_conn(GetDS().CreateConnection());
                CRef<CTestErrHandler> msg_handler(new CTestErrHandler());

                new_conn->Connect(GetArgs().GetConnParams());

                // Add handler ...
                new_conn->GetCDB_Connection()->PushMsgHandler(msg_handler);
                // Remove handler ...
                new_conn->GetCDB_Connection()->PopMsgHandler(msg_handler);
            }

            // Two connections ...
            {
                // Create new connection ...
                auto_ptr<IConnection> new_conn1(GetDS().CreateConnection());
                auto_ptr<IConnection> new_conn2(GetDS().CreateConnection());
                CRef<CTestErrHandler> msg_handler(new CTestErrHandler());

                new_conn1->Connect(GetArgs().GetConnParams());
                new_conn2->Connect(GetArgs().GetConnParams());

                // Add handler ...
                new_conn1->GetCDB_Connection()->PushMsgHandler(msg_handler);
                new_conn2->GetCDB_Connection()->PushMsgHandler(msg_handler);
                // Remove handler ...
                new_conn1->GetCDB_Connection()->PopMsgHandler(msg_handler);
                new_conn2->GetCDB_Connection()->PopMsgHandler(msg_handler);
            }

            // One connection ...
            {
                auto_ptr<IConnection> conn1(GetDS().CreateConnection());

                conn1->Connect(GetArgs().GetConnParams());

                // Add handler ...
                conn1->GetCDB_Connection()->PushMsgHandler(cref_msg_handler);
                // Remove handler ...
                conn1->GetCDB_Connection()->PopMsgHandler(cref_msg_handler);
            }

            // Two connections ...
            {
                auto_ptr<IConnection> conn1(GetDS().CreateConnection());
                auto_ptr<IConnection> conn2(GetDS().CreateConnection());

                conn1->Connect(GetArgs().GetConnParams());
                conn2->Connect(GetArgs().GetConnParams());

                // Add handler ...
                conn1->GetCDB_Connection()->PushMsgHandler(cref_msg_handler);
                conn2->GetCDB_Connection()->PushMsgHandler(cref_msg_handler);
                // Remove handler ...
                conn1->GetCDB_Connection()->PopMsgHandler(cref_msg_handler);
                conn2->GetCDB_Connection()->PopMsgHandler(cref_msg_handler);
            }
        }
    }

    // Connection. eTakeOwnership ...
    {
        CRef<CTestErrHandler> cref_msg_handler(new CTestErrHandler());

        // Raw pointer ...
        {
            // One connection ...
            {
                auto_ptr<IConnection> new_conn(GetDS().CreateConnection());
                CTestErrHandler* msg_handler = new CTestErrHandler();

                new_conn->Connect(GetArgs().GetConnParams());

                // Add handler ...
                new_conn->GetCDB_Connection()->PushMsgHandler(msg_handler,
                                                              eTakeOwnership);
                // We do not remove msg_handler here because it is supposed to be
                // deleted by new_conn ...
            }

            // Raw pointer ...
            // Two connections ...
            {
                auto_ptr<IConnection> new_conn1(GetDS().CreateConnection());
                auto_ptr<IConnection> new_conn2(GetDS().CreateConnection());
                CTestErrHandler* msg_handler = new CTestErrHandler();

                new_conn1->Connect(GetArgs().GetConnParams());
                new_conn2->Connect(GetArgs().GetConnParams());

                // Add handler ...
                new_conn1->GetCDB_Connection()->PushMsgHandler(msg_handler,
                                                               eTakeOwnership);
                new_conn2->GetCDB_Connection()->PushMsgHandler(msg_handler,
                                                               eTakeOwnership);
                // We do not remove msg_handler here because it is supposed to be
                // deleted by new_conn ...
            }
        }

        // CRef
        {
            // One connection ...
            {
                auto_ptr<IConnection> new_conn(GetDS().CreateConnection());
                CRef<CTestErrHandler> msg_handler(new CTestErrHandler());

                new_conn->Connect(GetArgs().GetConnParams());

                // Add handler ...
                new_conn->GetCDB_Connection()->PushMsgHandler(msg_handler,
                                                              eTakeOwnership);
                // We do not remove msg_handler here because it is supposed to be
                // deleted by new_conn ...
            }

            // Two connections ...
            {
                auto_ptr<IConnection> new_conn1(GetDS().CreateConnection());
                auto_ptr<IConnection> new_conn2(GetDS().CreateConnection());
                CRef<CTestErrHandler> msg_handler(new CTestErrHandler());

                new_conn1->Connect(GetArgs().GetConnParams());
                new_conn2->Connect(GetArgs().GetConnParams());

                // Add handler ...
                new_conn1->GetCDB_Connection()->PushMsgHandler(msg_handler,
                                                               eTakeOwnership);
                new_conn2->GetCDB_Connection()->PushMsgHandler(msg_handler,
                                                               eTakeOwnership);
                // We do not remove msg_handler here because it is supposed to be
                // deleted by new_conn ...
            }

            // One connection ...
            {
                auto_ptr<IConnection> new_conn(GetDS().CreateConnection());

                new_conn->Connect(GetArgs().GetConnParams());

                // Add handler ...
                new_conn->GetCDB_Connection()->PushMsgHandler(cref_msg_handler,
                                                              eTakeOwnership);
                // We do not remove msg_handler here because it is supposed to be
                // deleted by new_conn ...
            }

            // Two connections ...
            {
                auto_ptr<IConnection> new_conn1(GetDS().CreateConnection());
                auto_ptr<IConnection> new_conn2(GetDS().CreateConnection());

                new_conn1->Connect(GetArgs().GetConnParams());
                new_conn2->Connect(GetArgs().GetConnParams());

                // Add handler ...
                new_conn1->GetCDB_Connection()->PushMsgHandler(cref_msg_handler,
                                                               eTakeOwnership);
                new_conn2->GetCDB_Connection()->PushMsgHandler(cref_msg_handler,
                                                               eTakeOwnership);
                // We do not remove msg_handler here because it is supposed to be
                // deleted by new_conn ...
            }
        }
    }

    // Context. eTakeOwnership ...
    {
        // Raw pointer ...
        {
            CTestErrHandler* msg_handler = new CTestErrHandler();

            // Add handler ...
            drv_context->PushCntxMsgHandler(msg_handler,
                                            eTakeOwnership);
            drv_context->PushDefConnMsgHandler(msg_handler,
                                               eTakeOwnership);
            // We do not remove msg_handler here because it is supposed to be
            // deleted by drv_context ...
        }

        // CRef
        {
            CRef<CTestErrHandler> msg_handler(new CTestErrHandler());

            // Add handler ...
            drv_context->PushCntxMsgHandler(msg_handler,
                                            eTakeOwnership);
            drv_context->PushDefConnMsgHandler(msg_handler,
                                               eTakeOwnership);
            // We do not remove msg_handler here because it is supposed to be
            // deleted by drv_context ...
        }
    }
}


///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(Test_Exception_Safety)
{
    string sql;
    string table_name = "#exception_table";
    // string table_name = "exception_table";

    // Very first test ...
    {
        // Try to catch a base class ...
        BOOST_CHECK_THROW(s_ES_01_Internal(GetConnection()), CDB_Exception);
    }

    // Second test ...
    // Restore after invalid statement ...
    try {
        auto_ptr<IStatement> auto_stmt(GetConnection().GetStatement());

        if (table_name[0] == '#') {
            sql  = " CREATE TABLE " + table_name + "( \n";
            sql += "    id INT PRIMARY KEY \n";
            sql += " )";

            // Create the table
            auto_stmt->ExecuteUpdate(sql);
        }


        auto_stmt->ExecuteUpdate("INSERT " + table_name + "(id) VALUES(17)");

        try {
            // Try to insert duplicate value ...
            auto_stmt->ExecuteUpdate("INSERT " + table_name + "(id) VALUES(17)");
        } catch (const CDB_Exception& _DEBUG_ARG(ex)) {
            // ignore it ...
            _TRACE(ex);
        }

        try {
            // execute invalid statement ...
            auto_stmt->ExecuteUpdate("ISERT " + table_name + "(id) VALUES(17)");
        } catch (const CDB_Exception& _DEBUG_ARG(ex)) {
            // ignore it ...
            _TRACE(ex);
        }

        // Check status of the statement ...
        if (false) {
            auto_stmt->ExecuteUpdate("SELECT max(id) FROM " + table_name);
        } else {
            auto_stmt->SendSql("SELECT max(id) FROM " + table_name);
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
        }
    }
    catch(const CException& ex) {
        DBAPI_BOOST_FAIL(ex);
    }
}


///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(Test_MsgToEx)
{
    // First test ...
    {
        int msg_num = 0;
        CDB_Exception* dbex = NULL;

        auto_ptr<IConnection> local_conn(
            GetDS().CreateConnection( CONN_OWNERSHIP )
            );
        local_conn->Connect(GetArgs().GetConnParams());

        // 1.
        {
            msg_num = 0;

            BOOST_CHECK_THROW(s_ES_01_Internal(*local_conn), CDB_Exception);

            while((dbex = local_conn->GetErrorAsEx()->Pop())) {
                ++msg_num;
            }

            BOOST_CHECK_EQUAL(msg_num, 0);
        }

        // 1a.
        {
            msg_num = 0;

            // Enable MsgToEx ...
            local_conn->MsgToEx(false);

            BOOST_CHECK_THROW(s_ES_01_Internal(*local_conn), CDB_Exception);

            while ((dbex = local_conn->GetErrorAsEx()->Pop())) {
                ++msg_num;
            }

            BOOST_CHECK_EQUAL(msg_num, 0);
        }

        // 2.
        {
            msg_num = 0;

            // Enable MsgToEx ...
            local_conn->MsgToEx(true);

            BOOST_CHECK_THROW(s_ES_01_Internal(*local_conn), CDB_Exception);

            while ((dbex = local_conn->GetErrorAsEx()->Pop())) {
                ++msg_num;
            }

            BOOST_CHECK_EQUAL(msg_num, 1);
        }

        // 3.
        if (true) {
            msg_num = 0;

            // Enable MsgToEx ...
            local_conn->MsgToEx(false);

            BOOST_CHECK_THROW(s_ES_01_Internal(*local_conn), CDB_Exception);

            while ((dbex = local_conn->GetErrorAsEx()->Pop())) {
                ++msg_num;
            }

            BOOST_CHECK_EQUAL(msg_num, 0);
        }

        // 4.
        {
            msg_num = 0;

            // Enable MsgToEx ...
            local_conn->MsgToEx(true);
            // Disable MsgToEx ...
            local_conn->MsgToEx(false);

            BOOST_CHECK_THROW(s_ES_01_Internal(*local_conn), CDB_Exception);

            while ((dbex = local_conn->GetErrorAsEx()->Pop())) {
                ++msg_num;
            }

            BOOST_CHECK_EQUAL(msg_num, 0);
        }
    }

    // Second test ...
    {
        auto_ptr<IConnection> local_conn(
            GetDS().CreateConnection( CONN_OWNERSHIP )
            );
        local_conn->Connect(GetArgs().GetConnParams());

        local_conn->GetCDB_Connection()->PushMsgHandler(
            new CDB_UserHandler_Exception_NoThrow,
            eTakeOwnership
            );

        // Enable MsgToEx ...
        // local_conn->MsgToEx(true);
        // Disable MsgToEx ...
        // local_conn->MsgToEx(false);

        BOOST_CHECK_THROW(s_ES_01_Internal(*local_conn), CDB_Exception);
    }
}


///////////////////////////////////////////////////////////////////////////////
// This code is based on a problem reported by Baoshan Gu.


///////////////////////////////////////////////////////////////////////////////
class BaoshanGu_handler : public CDB_UserHandler
{
public:
     virtual ~BaoshanGu_handler(void)
     {
     };

     virtual bool HandleIt(CDB_Exception* ex)
     {
         if (!ex)  return false;

         // Ignore errors with ErrorCode set to 0
         // (this is related mostly to the FreeTDS driver)
         if (ex->GetDBErrCode() == 0
             // || ex->Message().find("ERROR") == string::npos
            ) {
             return true;
         }

         NcbiCout << "BaoshanGu_handler called." << endl;
         throw *ex;
         //ex->Throw();

         return true;
     }
};


///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(Test_MsgToEx2)
{
    CRef<BaoshanGu_handler> handler(new BaoshanGu_handler);
    I_DriverContext* drv_context = GetDS().GetDriverContext();

    drv_context->PushCntxMsgHandler(handler);
    drv_context->PushDefConnMsgHandler(handler);

    auto_ptr<IConnection> conn(GetDS().CreateConnection());
    conn->Connect("anyone","allowed", "REFTRACK_DEV", "x_locus");

    // conn->MsgToEx(true);
    conn->MsgToEx(false);

    auto_ptr<IStatement> stmt(conn->GetStatement());

    try {
        // string sql = " EXEC locusXref..undelete_locus_id @locus_id = 135896, @login='guba'";
        string sql = "EXEC locusXref..dbapitest";

        stmt->ExecuteUpdate(sql); //or SendSql
    } catch (const CDB_Exception& e) {
        NcbiCout << "CDB_Exception: " << e.what() << endl;
    }

    CDB_Exception* dbex = NULL;
    while ((dbex = conn->GetErrorAsEx()->Pop())){
        cout << "MSG:" << dbex->Message() << endl;
    }

    drv_context->PopCntxMsgHandler(handler);
    drv_context->PopDefConnMsgHandler(handler);
}


END_NCBI_SCOPE

