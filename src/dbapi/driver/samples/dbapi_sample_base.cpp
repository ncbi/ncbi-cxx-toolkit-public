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
* Author:  Denis Vakatov
*
* File Description:
*    CDbapiSampleApp_Base
*      A base class for various DBAPI tests and sample applications.
*
*   Test the implementation of BCP by DBAPI driver(s):
*     1) CREATE a table with 5 rows
*     2) make BCP of this table
*     3) PRINT the table on screen (use <ROW> and </ROW> to separate the rows)
*     4) DELETE the table from the database
*
*/


#include <ncbi_pch.hpp>

#include "dbapi_sample_base.hpp"
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbi_process.hpp>
#include <util/smalldns.hpp>
#include <dbapi/driver/driver_mgr.hpp>
#include <dbapi/driver/dbapi_svc_mapper.hpp>
#include <dbapi/driver/drivers.hpp>
#include <dbapi/error_codes.hpp>
#include <util/random_gen.hpp>

#include <algorithm>


#define NCBI_USE_ERRCODE_X   Dbapi_SampleBase

BEGIN_NCBI_SCOPE


const char*  file_name[] = {
    "../test1.txt",
    "../test2.txt",
    "../test3.txt",
    "../test4.txt",
    "../test5.txt",
    ""
};


/////////////////////////////////////////////////////////////////////////////
//  CDbapiSampleApp::
//


CDbapiSampleApp::CDbapiSampleApp(EUseSampleDatabase sd)
    : CNcbiApplication(),
      m_DriverContext(0),
      m_UseSampleDatabase(sd),
      m_UseSvcMapper(false)
{
    m_TableUID += "_" + CSmallDNS::GetLocalHost() + "_";
    m_TableUID += NStr::NumericToString(CProcess::GetCurrentPid()) + "_";
    m_TableUID += CTime(CTime::eCurrent).AsString("MDy");
    replace( m_TableUID.begin(), m_TableUID.end(), '-', '_' );

#ifdef NCBI_DLL_SUPPORT
    CPluginManager_DllResolver::EnableGlobally(true);
#else

#ifdef HAVE_LIBSYBASE
    DBAPI_RegisterDriver_CTLIB();
#endif

#ifdef HAVE_ODBC
    DBAPI_RegisterDriver_ODBC();
#endif

    DBAPI_RegisterDriver_FTDS();

#endif // NCBI_DLL_BUILD
}


CDbapiSampleApp::~CDbapiSampleApp()
{
    return;
}


// Default implementation ...
void
CDbapiSampleApp::InitSample(CArgDescriptions&)
{
}


// Default implementation ...
void
CDbapiSampleApp::ExitSample(void)
{
}


CDbapiSampleApp::EServerType
CDbapiSampleApp::GetServerType(void) const
{
    if ( GetServerName() == "STRAUSS"
         || GetServerName() == "MOZART"
         || GetServerName() == "SCHUMANN"
         || GetServerName() == "CLEMENTI"
         || GetServerName() == "DBAPI_DEV1"
         || GetServerName() == "OBERON"
         || GetServerName() == "TAPER"
         || GetServerName() == "THALBERG"
         || GetServerName() == "DBAPI_SYB_TEST"
         || NStr::StartsWith(GetServerName(), "DBAPI_DEV")
         || NStr::StartsWith(GetServerName(), "BARTOK")
         ) {
        return eSybase;
    } else if (NStr::StartsWith(GetServerName(), "MS_DEV")
               || NStr::StartsWith(GetServerName(), "MSSQL")
               || GetServerName() == "DBAPI_MS_TEST"
               ) {
        return eMsSql;
    }

    return eUnknown;
}


void
CDbapiSampleApp::Init()
{
    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "DBAPI Sample Application");

    // Describe the expected command-line arguments
#if defined(NCBI_OS_MSWIN)
#define DEF_SERVER    "MS_DEV1"
#define DEF_DRIVER    "ftds"
#define ALL_DRIVERS   "ctlib", "dblib", "ftds", \
                      "gateway", "odbc"
#elif defined(HAVE_LIBSYBASE)
#define DEF_SERVER    "DBAPI_DEV1"
#define DEF_DRIVER    "ctlib"
#define ALL_DRIVERS   "ctlib", "dblib", "ftds", "gateway", \
                      "odbc"
#else
#define DEF_SERVER    "MS_DEV1"
#define DEF_DRIVER    "ftds"
#define ALL_DRIVERS   "ftds", "gateway", \
                      "odbc"
#endif

    arg_desc->AddDefaultKey("S", "server",
                            "Name of the SQL server to connect to",
                            CArgDescriptions::eString, DEF_SERVER);

    arg_desc->AddDefaultKey("d", "driver",
                            "Name of the DBAPI driver to use",
                            CArgDescriptions::eString,
                            DEF_DRIVER);
    arg_desc->SetConstraint("d", &(*new CArgAllow_Strings, ALL_DRIVERS));

    arg_desc->AddDefaultKey("U", "username",
                            "User name",
                            CArgDescriptions::eString, "DBAPI_test");

    arg_desc->AddDefaultKey("P", "password",
                            "Password",
                            CArgDescriptions::eString, "allowed");

    arg_desc->AddOptionalKey("v", "version",
                            "TDS protocol version",
                            CArgDescriptions::eInteger);

    arg_desc->AddDefaultKey("lb", "use_load_balancer",
                            "Use load balancer for service mapping",
                            CArgDescriptions::eString, "off");
    arg_desc->SetConstraint("lb", &(*new CArgAllow_Strings,
                                    "on", "off", "random"));

    // Add user's command-line argument descriptions
    InitSample(*arg_desc);

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}


int
CDbapiSampleApp::Run()
{

    const CArgs& args = GetArgs();

    // Get command-line arguments ...
    m_DriverName = args["d"].AsString();
    m_ServerName = args["S"].AsString();
    m_UserName   = args["U"].AsString();
    m_Password   = args["P"].AsString();
    if ( args["v"].HasValue() ) {
        m_TDSVersion = args["v"].AsString();
    } else {
        m_TDSVersion.erase();
    }

    string service_mapping = args["lb"].AsString();
    if (service_mapping == "on") {
        m_UseSvcMapper = true;
    } else if (service_mapping == "random") {
        static Uint4   rdm_seed = static_cast<Uint4>(time(NULL));
        static CRandom rdm_gen(rdm_seed);
        m_UseSvcMapper = (rdm_gen.GetRand(0, 1) != 0);
    }

    if (UseSvcMapper()) {
        DBLB_INSTALL_DEFAULT();
#ifdef HAVE_LIBCONNEXT
        LOG_POST_X(1, "Using load-balancer service to server mapper ...");
#else
        ERR_POST_X(2, "Load balancing requested, but not available in this build");
#endif
    }

    if ( m_TDSVersion.empty() ) {
        // Setup some driver-specific attributes
        if ( GetDriverName() == "dblib" &&
             GetServerType() == eSybase ) {
            // Due to the bug in the Sybase 12.5 server, DBLIB cannot do
            // BcpIn to it using protocol version other than "100".
            SetDatabaseParameter("version", "100");
//         } else if ( GetDriverName() == "ftds"  &&  GetServerType() == eMsSql ) {
//             SetDatabaseParameter("version", "100");
        }
    } else {
        SetDatabaseParameter("version", m_TDSVersion);
    }

    try {
        return RunSample();
    } catch ( CDB_Exception& e ) {
        CDB_UserHandler::GetDefault().HandleIt(&e);
        return 1;
    }

    return 0;
}


void
CDbapiSampleApp::Exit()
{
    ExitSample();
}


I_DriverContext&
CDbapiSampleApp::GetDriverContext(void)
{
    if ( m_DriverContext.get() )
        return *m_DriverContext;

    // Create driver context
    C_DriverMgr driver_mgr;
    string      err_msg;
    m_DriverContext.reset
        (driver_mgr.GetDriverContext(GetDriverName(),
                                     &err_msg,
                                     &GetDatabaseParameters()));

    if ( !m_DriverContext.get() ) {
        ERR_POST_X(3, Fatal << "Cannot load driver: " << GetDriverName() << " ["
                      << err_msg << "] ");
    }

    return *m_DriverContext;
}


CDB_Connection*
CDbapiSampleApp::CreateConnection(IConnValidator*                  validator,
                                  I_DriverContext::TConnectionMode mode,
                                  bool                             reusable,
                                  const string&                    pool_name)
{
    auto_ptr<CDB_Connection> conn;
    I_DriverContext& dc(GetDriverContext());

    if (validator) {
        conn.reset(dc.ConnectValidated(GetServerName(),
                              GetUserName(),
                              GetPassword(),
                              *validator,
                              mode,
                              reusable,
                              pool_name));
    } else {
        conn.reset(dc.Connect(GetServerName(),
                              GetUserName(),
                              GetPassword(),
                              mode,
                              reusable,
                              pool_name));
    }

    if ( !conn.get() ) {
        ERR_POST_X(4, Fatal << "Cannot open connection to the server: "
                      << GetServerName());
    }

    // Print database name
    string sql("select @@servername");

    auto_ptr<CDB_LangCmd> stmt(conn->LangCmd(sql));
    stmt->Send();

    while ( stmt->HasMoreResults() ) {
        auto_ptr<CDB_Result> result(stmt->Result());
        if ( !result.get() )
            continue;

        if ( result->ResultType() == eDB_RowResult ) {
            if ( result->Fetch() ) {
                CDB_LongChar v;
                result->GetItem(&v);
                LOG_POST_X(5, "Connected to the server '" << v.Value() << "'");
            }
        }
    }

    if ( m_UseSampleDatabase == eUseSampleDatabase ) {
        conn->SetDatabaseName("DBAPI_Sample");
    }

    return conn.release();
}

void
CDbapiSampleApp::DeleteTable(const string& table_name)
{
    try {
        string sql;

        // Drop the table.
        sql = "DROP TABLE " + table_name;
        auto_ptr<CDB_LangCmd> lcmd(GetConnection().LangCmd(sql));
        lcmd->Send();
        lcmd->DumpResults();
    } catch ( const CDB_Exception& ) {
    }
}


void
CDbapiSampleApp::DeleteLostTables(void)
{
    string sql = "select name from sysobjects WHERE type = 'U'";
    typedef list<string> table_name_list_t;
    table_name_list_t table_name_list;
    table_name_list_t::const_iterator citer;

    auto_ptr<CDB_LangCmd> lcmd(GetConnection().LangCmd(sql));
    lcmd->Send();

    while ( lcmd->HasMoreResults() ) {

        auto_ptr<CDB_Result> r(lcmd->Result());
        if ( !r.get() ) continue;
        if ( r->ResultType() == eDB_RowResult ) {

            while ( r->Fetch() ) {
                EDB_Type rt = r->ItemDataType(0);

                if ( !(rt == eDB_Char || rt == eDB_VarChar) ) continue;
                CDB_VarChar str_val;
                r->GetItem(&str_val);

                if ( str_val.IsNULL() ) continue;
                string table_creation_date;
                string table_name = str_val.Value();
                string::size_type pos = table_name.find_last_of('_');

                if ( pos == string::npos ) continue;
                table_creation_date = table_name.substr(pos + 1);
                try {
                    // Table name suffix may be not a date ...
                    CTime creation_date(table_creation_date, "MDy");

                    if ( CTimeSpan(3, 0, 0, 0) < (CTime(CTime::eCurrent) - creation_date) ) {
              table_name_list.push_back(table_name);
                    }
                }
                catch(CException&)
                {
                    // Ignore any problems ...
                }
            }
        }
    }

    for(citer = table_name_list.begin(); citer != table_name_list.end(); ++citer)
    {
      DeleteTable(*citer);
    }
}

void
CDbapiSampleApp::ShowResults (const string& query)
{
    auto_ptr<CDB_LangCmd> lcmd(GetConnection().LangCmd(query));
    lcmd->Send();

    while ( lcmd->HasMoreResults() ) {
        auto_ptr<CDB_Result> r(lcmd->Result());
        if ( !r.get() )
            continue;

        if ( r->ResultType() == eDB_RowResult ) {
            while ( r->Fetch() ) {
                cout << "<ROW>";
                for ( unsigned int j = 0;  j < r->NofItems(); j++ ) {
                    EDB_Type rt = r->ItemDataType(j);
                    const string iname = r->ItemName(j);
                    cout << '<' << iname << '>';
                    if ( rt == eDB_Char || rt == eDB_VarChar ) {
                        CDB_VarChar v;
                        r->GetItem(&v);
                        cout << (v.IsNULL()? "{NULL}" : v.Value());
                    } else if ( rt == eDB_Int ||
                                rt == eDB_SmallInt ||
                                rt == eDB_TinyInt ) {
                        CDB_Int v;
                        r->GetItem(&v);
                        if ( v.IsNULL() ) {
                            cout << "{NULL}";
                        } else {
                            cout << v.Value();
                        }
                    } else if ( rt == eDB_Float ) {
                        CDB_Float v;
                        r->GetItem(&v);
                        if ( v.IsNULL() ) {
                            cout << "{NULL}";
                        } else {
                            cout << v.Value();
                        }
                    } else if ( rt == eDB_Double ) {
                        CDB_Double v;
                        r->GetItem(&v);
                        if ( v.IsNULL() ) {
                            cout << "{NULL}";
                        } else {
                            cout << v.Value();
                        }
                    } else if ( rt == eDB_DateTime ||
                                rt == eDB_SmallDateTime ) {
                        CDB_DateTime v;
                        r->GetItem(&v);
                        if ( v.IsNULL() ) {
                            cout << "{NULL}";
                        } else {
                            cout << v.Value().AsString();
                        }
                    } else if ( rt == eDB_Numeric ) {
                        CDB_Numeric v;
                        r->GetItem(&v);
                        if ( v.IsNULL() ) {
                            cout << "{NULL}";
                        } else {
                            cout << v.Value();
                        }
                    } else if ( rt == eDB_Text ) {
                        CDB_Text v;
                        r->GetItem(&v);
                        cout << "{text (" << v.Size() << " bytes)}";
                    } else if ( rt == eDB_Image ) {
                        CDB_Image v;
                        r->GetItem(&v);
                        cout << "{image (" << v.Size() << " bytes)}";
                    } else {
                        r->SkipItem();
                        cout << "{unprintable}";
                    }
                    cout << "</" << iname << '>';
                }
                cout << "</ROW>" << endl << endl;
            }
        }
    }

}

void
CDbapiSampleApp::CreateTable (const string& table_name)
{
    string sql;

    // Drop a table with same name.
    sql  = string(" IF EXISTS (select * from sysobjects WHERE name = '");
    sql += table_name + "' AND type = 'U') begin ";
    sql += " DROP TABLE " + table_name + " end ";

    auto_ptr<CDB_LangCmd> lcmd(GetConnection().LangCmd (sql));
    lcmd->Send();
    lcmd->DumpResults();

    // Create a new table.
    sql  = " create table " + table_name + "( \n";
    sql += "    int_val int not null, \n";
    sql += "    fl_val real not null, \n";
    sql += "    date_val datetime not null, \n";
    sql += "    str_val varchar(255) null, \n";
    sql += "    txt_val text null \n";
//     sql += "    primary key clustered(int_val) \n";
    sql += ")";

    lcmd.reset(GetConnection().LangCmd ( sql ));
    lcmd->Send();
    lcmd->DumpResults();
}


CDB_Connection&
CDbapiSampleApp::GetConnection(void)
{
    if ( !m_Connection.get() ) {
        m_Connection.reset(CreateConnection());
    }

    return *m_Connection;
}


///////////////////////////////////////////////////////////////////////////////
CDbapiSampleErrHandler::CDbapiSampleErrHandler(void)
{
}

CDbapiSampleErrHandler::~CDbapiSampleErrHandler(void)
{
}

// Return TRUE if "ex" is processed, FALSE if not (or if "ex" is NULL)
bool
CDbapiSampleErrHandler::HandleIt(CDB_Exception* ex)
{
    if ( !ex )
        return false;

    // Ignore errors with ErrorCode set to 0
    // (this is related mostly to the FreeTDS driver)
    if (ex->GetDBErrCode() == 0)
        return true;

    // On other errors, throw an exception (was not allowed before!)
    throw *ex;
}

END_NCBI_SCOPE


