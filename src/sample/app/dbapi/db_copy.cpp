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
 * File Description: Example of usage of DBAPI
 *
 * ===========================================================================
 */

#include <ncbi_pch.hpp>

#include <corelib/ncbiapp.hpp>
#include <dbapi/dbapi.hpp>

#include <common/test_assert.h>  /* This header must go last */

BEGIN_NCBI_SCOPE


class CDbCopyApp : public CNcbiApplication
{
public:
    virtual int  Run(void);
};


class CErrHandler : public CDB_UserHandler
{
public:
    // Return TRUE if "ex" is processed, FALSE if not (or if "ex" is NULL)
    virtual bool HandleIt(CDB_Exception* ex);
};


bool CErrHandler::HandleIt(CDB_Exception* ex)
{
    if ( !ex )
        return false;

    // Ignore errors with ErrorCode set to 0
    // (this is related mostly to the FreeTDS driver)
    if (ex->GetDBErrCode() == 0)
        return true;

    throw *ex;
}



int CDbCopyApp::Run(void)
{
    string sql;
    I_DriverContext* drv_context = NULL;

    try {
        CDriverManager& dm = CDriverManager::GetInstance();
        IDataSource* ds_from = dm.CreateDs( "ctlib" );
        IDataSource* ds_to = dm.CreateDs( "ftds" );

        drv_context = ds_from->GetDriverContext();
        drv_context->PushCntxMsgHandler(new CErrHandler, eTakeOwnership);
        drv_context->PushDefConnMsgHandler(new CErrHandler, eTakeOwnership);

        drv_context = ds_to->GetDriverContext();
        drv_context->PushCntxMsgHandler(new CErrHandler, eTakeOwnership);
        drv_context->PushDefConnMsgHandler(new CErrHandler, eTakeOwnership);

        auto_ptr<IConnection> conn_from(ds_from->CreateConnection());
        auto_ptr<IConnection> conn_to(ds_to->CreateConnection());

        conn_to->SetMode(IConnection::eBulkInsert);

        conn_from->Connect("anyone",
                           "allowed",
                           "TAPER",
                           "DBAPI_Sample");

        conn_to->Connect("anyone",
                         "allowed",
                         "MS_DEV1",
                         "DBAPI_Sample");

        auto_ptr<IStatement> stmt_from(conn_from->GetStatement());
        auto_ptr<IStatement> stmt_to(conn_to->GetStatement());

        // Create a source table ...
        {
            sql = " CREATE TABLE #source_table( \n"
                "attribute_id INT, \n"
                "attribute_name VARCHAR(128), \n"
                "attribute_value VARCHAR(255) \n"
                ")";

            stmt_from->ExecuteUpdate(sql);
        }

        // Create a destination table ...
        {
            sql = " CREATE TABLE #destination_table( \n"
                "attribute_id INT, \n"
                "attribute_name VARCHAR(128), \n"
                "attribute_value VARCHAR(255) \n"
                ")";

            stmt_to->ExecuteUpdate(sql);
        }

        // Fill up the #source_table with some data ...
        {
            auto_ptr<IConnection> conn_tmp(ds_from->CreateConnection());

            conn_tmp->Connect("anyone",
                              "allowed",
                              "TAPER");

            // Prepare an INSERT stattement ...
            sql = "INSERT INTO #source_table VALUES(@attr_id, @attr_name, @attr_value)";

            // Prepare a stored procedure ...
            auto_ptr<ICallableStatement> auto_stmt(conn_tmp->GetCallableStatement("sp_server_info"));
            auto_stmt->Execute();

            // Insert data ...
            stmt_from->ExecuteUpdate("BEGIN TRANSACTION");
            while(auto_stmt->HasMoreResults()) {
                auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());

                if (rs.get()) {
                    while(rs->Next()) {
                        stmt_from->SetParam(rs->GetVariant(1), "@attr_id");
                        stmt_from->SetParam(rs->GetVariant(2), "@attr_name");
                        stmt_from->SetParam(rs->GetVariant(3), "@attr_value");

                        stmt_from->ExecuteUpdate(sql);
                        stmt_from->ClearParamList();
                    }
                }
            }
            stmt_from->ExecuteUpdate("COMMIT TRANSACTION");
        }

        // Copy data from the #source_table into the #destination table ...
        {
            // Prepare an INSERT stattement ...
            sql = "INSERT INTO #destination_table VALUES(@attr_id, @attr_name, @attr_value)";

            // Prepare a SELECT statement ...
            stmt_from->SendSql("SELECT * FROM #source_table");

            // Insert data ...
            stmt_to->ExecuteUpdate("BEGIN TRANSACTION");
            while(stmt_from->HasMoreResults()) {
                auto_ptr<IResultSet> rs(stmt_from->GetResultSet());

                if (rs.get()) {
                    while(rs->Next()) {
                        stmt_to->SetParam(rs->GetVariant(1), "@attr_id");
                        stmt_to->SetParam(rs->GetVariant(2), "@attr_name");
                        stmt_to->SetParam(rs->GetVariant(3), "@attr_value");

                        stmt_to->ExecuteUpdate(sql);
                        stmt_to->ClearParamList();
                    }
                }
            }
            stmt_to->ExecuteUpdate("COMMIT TRANSACTION");
        }

        // Print out the copyed data ...
        {
            stmt_to->SendSql("SELECT * FROM #destination_table");
            while(stmt_to->HasMoreResults()) {
                auto_ptr<IResultSet> rs(stmt_to->GetResultSet());

                if (rs.get()) {
                    while(rs->Next()) {
                        cout <<
                            rs->GetVariant(1).GetInt4() << '\t' <<
                            rs->GetVariant(2).GetString() << '\t' <<
                            rs->GetVariant(3).GetString() <<
                            endl;
                    }
                }
            }
        }
    } catch(const CException& ex) {
        cerr << ex.GetMsg() << endl;
        return 1;
    } catch(...) {
        cerr << "Unknown error" << endl;
        return 1;
    }

    return 0;
}


END_NCBI_SCOPE


int main(int argc, const char* argv[])
{
    return ncbi::CDbCopyApp().AppMain(argc, argv);
}
