/* $Id$
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
 * Author:  Sergey Sikorskiy
 *
 * This simple program illustrates how to use the RPC command
 *
 */

#include <ncbi_pch.hpp>

#include "../../driver/dbapi_driver_sample_base.hpp"
#include <dbapi/dbapi.hpp>
#include <dbapi/driver/exception.hpp>
#include <dbapi/driver/dbapi_svc_mapper.hpp>
#include <dbapi/driver/drivers.hpp>
#include <common/test_assert.h>  /* This header must go last */


USING_NCBI_SCOPE;

class CDemoApp : public CDbapiDriverSampleApp
{
public:
    CDemoApp(const string& server_name,
             int tds_version = 110);
    virtual ~CDemoApp(void);

    virtual int RunSample(void);
};

CDemoApp::CDemoApp(const string& server_name, int tds_version) :
    CDbapiDriverSampleApp(server_name, tds_version)
{
}


CDemoApp::~CDemoApp(void)
{
    return;
}


int
CDemoApp::RunSample(void)
{
    IDataSource* data_source = NULL;

    try {
        DBLB_INSTALL_DEFAULT();

        CDriverManager& dm = CDriverManager::GetInstance();

#ifdef FTDS_IN_USE
        DBAPI_RegisterDriver_FTDS();
        data_source = dm.CreateDs("ftds");
#else
        DBAPI_RegisterDriver_CTLIB();
        data_source = dm.CreateDs("ctlib");
#endif


        auto_ptr<IConnection> conn(data_source->CreateConnection());
        conn->Connect(GetUserName(),
                      GetPassword(),
                      GetServerName()
                      );

        auto_ptr<ICallableStatement> auto_stmt(conn->GetCallableStatement("sp_databases"));
        // auto_ptr<ICallableStatement> auto_stmt(conn->GetCallableStatement("sp_who"));
        auto_stmt->Execute();

        while(auto_stmt->HasMoreResults()) {
            if(auto_stmt->HasRows()) {
                auto_ptr<IResultSet> rs(auto_stmt->GetResultSet());
                auto_ptr<const IResultSetMetaData> md(rs->GetMetaData());

                switch(rs->GetResultType()) {
                case eDB_RowResult:
                    while(rs->Next()) {
                        for (unsigned int j = 1; j <= rs->GetTotalColumns(); ++j) {
                            const CVariant& value(rs->GetVariant(j));
                            const EDB_Type rt(md->GetType(j));

                            if (rt == eDB_Char || rt == eDB_VarChar) {
                                cout << md->GetName(j) << ": "
                                     << (value.IsNull()? "" : value.GetString())
                                     << " \t";
                            } else if (rt == eDB_Int ||
                                       rt == eDB_SmallInt ||
                                       rt == eDB_TinyInt) {
                                cout << md->GetName(j) << ": " << value.GetInt8()
                                     << ' ';
                            }
                        }
                        cout << endl;
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
    } catch (CDB_Exception& e) {
        CDB_UserHandler_Stream myExHandler(&cerr);

        myExHandler.HandleIt(&e);
        return 1;
    } catch (const CException&) {
        return 1;
    }

    return 0;
}

int main(int argc, const char* argv[])
{
    return CDemoApp("DBAPI_DEV3").AppMain(argc, argv);
}


