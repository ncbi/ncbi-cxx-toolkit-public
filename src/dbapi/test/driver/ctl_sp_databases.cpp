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
#include <test/test_assert.h>  /* This header must go last */


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
    map<string, string> attr;
    IDataSource* data_source = NULL;

    try {
        DBLB_INSTALL_DEFAULT();

        CDriverManager& dm = CDriverManager::GetInstance();

        attr["version"] = NStr::IntToString(GetTDSVersion());

#ifdef FTDS_IN_USE
        DBAPI_RegisterDriver_FTDS();
        data_source = dm.CreateDs("ftds", &attr);
#else
        DBAPI_RegisterDriver_CTLIB();
        data_source = dm.CreateDs("ctlib", &attr);
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
    return CDemoApp("SCHUMANN").AppMain(argc, argv);
}


/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2007/01/03 19:44:04  ssikorsk
 * Added new tests for DriverManager and statically linked drivers.
 *
 * Revision 1.15  2006/12/28 22:21:45  ssikorsk
 * Make code compatible with the ftds driver.
 *
 * Revision 1.14  2006/11/16 22:31:40  ssikorsk
 * Revamp code to use CDbapiDriverSampleApp.
 *
 * Revision 1.13  2006/08/31 18:46:11  ssikorsk
 * Get rid of unused variables.
 *
 * Revision 1.12  2006/02/24 19:36:12  ssikorsk
 * Added #include <test/test_assert.h> for test-suite sake
 *
 * Revision 1.11  2006/01/26 12:15:36  ssikorsk
 * Revamp code to include <dbapi/driver/dbapi_svc_mapper.hpp>;
 * Removed protection of DBLB_INSTALL_DEFAULT;
 *
 * Revision 1.10  2006/01/24 14:05:27  ssikorsk
 * Protect DBLB_INSTALL_DEFAULT with HAVE_LIBCONNEXT
 *
 * Revision 1.9  2006/01/24 12:53:24  ssikorsk
 * Revamp demo applications to use CNcbiApplication;
 * Use load balancer and configuration in an ini-file to connect to a
 * secondary server in case of problems with a primary server;
 *
 * Revision 1.8  2005/08/16 11:13:27  ssikorsk
 * Use SCHUMANN instead of BARTOK as a Sybase server.
 *
 * Revision 1.7  2004/09/01 21:31:29  vakatov
 * Use BARTOK instead of MOZART as the test Sybase-11.0.3 SQL server
 *
 * Revision 1.6  2004/05/17 21:12:13  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 1.5  2002/04/25 16:43:18  soussov
 * makes it plain
 *
 * Revision 1.4  2001/11/06 17:59:57  lavr
 * Formatted uniformly as the rest of the library
 *
 * Revision 1.3  2001/10/25 00:18:02  vakatov
 * SampleDBAPI_XXX() to accept yet another arg -- server name
 *
 * Revision 1.2  2001/10/24 16:37:19  lavr
 * Finish log with horizontal rule
 *
 * Revision 1.1  2001/10/23 20:52:11  lavr
 * Initial revision (derived from former sample programs)
 *
 * Revision 1.3  2001/10/05 20:27:18  vakatov
 * Minor fix (by V.Soussov)
 *
 * Revision 1.2  2001/10/04 20:28:49  vakatov
 * Grooming...
 *
 * Revision 1.1  2001/10/02 20:34:28  soussov
 * Initial revision
 *
 * ===========================================================================
 */
