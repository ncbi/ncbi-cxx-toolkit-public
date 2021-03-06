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
 * Authors:  David McElhany
 *
 * File Description:
 *  Test service resolution via DBAPI, SDBAPI, and DBLB.
 *
 *  Test for basic DB communication after service name resolution using all
 *  valid combinations of:
 *      - APIs (DBAPI, SDBAPI, DBLB)
 *      - DB drivers (FTDS, CTLIB) - only for DBAPI
 *      - service mappers (at least LBSMD and namerd)
 *      - service setups:
 *          - presence in LBSMD (and by implication namerd) data
 *          - reverse DNS presence in LBSMD
 *          - presence in the interfaces file
 *      - single-server setup conditions:
 *          - up
 *          - down
 *          - broken
 *          - non-existent
 *      - two-server setups - an up second server and first server:
 *          - down
 *          - broken
 *          - non-existent
 *
 *  Broken and down servers are simulated with companion scripts:
 *      dbapi_svc_test_broken.bash
 *      dbapi_svc_test_down.bash
 *  These scripts should be invoked from a crontab - see config params like:
 *      s_BrokenHost
 *
 */

#include <ncbi_pch.hpp>     // This header must go first

#include <connect/ncbi_socket.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbidiag.hpp>
#include <corelib/ncbistr.hpp>

// "new" DBAPI
#include <dbapi/dbapi.hpp>
#include <dbapi/simple/sdbapi.hpp>

// "old" DBAPI
#include <dbapi/driver/dbapi_conn_factory.hpp>  // CTrivialConnValidator
#include <dbapi/driver/dbapi_svc_mapper.hpp>    // DBLB_INSTALL_DEFAULT
#include <dbapi/driver/drivers.hpp>             // DBAPI_RegisterDriver_*
#include <dbapi/driver/exception.hpp>           // CDB_UserHandler

#ifdef HAVE_LIBCONNEXT
#  include <connect/ext/ncbi_dblb_svcmapper.hpp>
#endif

#ifdef _MSC_VER
#define unsetenv(n)     _putenv_s(n,"")
#define setenv(n,v,w)   _putenv_s(n,v)
#endif


#define NCBI_USE_ERRCODE_X   TestNcbiDblbSvcRes

BEGIN_NCBI_SCOPE
NCBI_DEFINE_ERRCODE_X(TestNcbiDblbSvcRes, 2500, 1);
END_NCBI_SCOPE

USING_NCBI_SCOPE;


// BK, DN, NE, UP (broken, down, non-existent, up) host defaults

NCBI_PARAM_DECL(string, server, broken_message);
NCBI_PARAM_DEF_EX(string, server, broken_message, "I am a broken server.", 0, DBAPI_SVC_TEST_BROKEN_MESSAGE);
typedef NCBI_PARAM_TYPE(server, broken_message) TParamServerBrokenMessage;
static CSafeStatic<TParamServerBrokenMessage> s_BrokenMessage;

NCBI_PARAM_DECL(string, server, broken_host);
NCBI_PARAM_DEF_EX(string, server, broken_host, "coremake11", 0, DBAPI_SVC_TEST_BROKEN_HOST);
typedef NCBI_PARAM_TYPE(server, broken_host) TParamServerBrokenHost;
static CSafeStatic<TParamServerBrokenHost> s_BrokenHost;

NCBI_PARAM_DECL(unsigned short, server, broken_port);
NCBI_PARAM_DEF_EX(unsigned short, server, broken_port, 1444, 0, DBAPI_SVC_TEST_BROKEN_PORT);
typedef NCBI_PARAM_TYPE(server, broken_port) TParamServerBrokenPort;
static CSafeStatic<TParamServerBrokenPort> s_BrokenPort;

NCBI_PARAM_DECL(string, server, down_message);
NCBI_PARAM_DEF_EX(string, server, down_message, "I am a mirror.", 0, DBAPI_SVC_TEST_DOWN_MESSAGE);
typedef NCBI_PARAM_TYPE(server, down_message) TParamServerDownMessage;
static CSafeStatic<TParamServerDownMessage> s_DownMessage;

NCBI_PARAM_DECL(string, server, down_host);
NCBI_PARAM_DEF_EX(string, server, down_host, "coremake22", 0, DBAPI_SVC_TEST_DOWN_HOST);
typedef NCBI_PARAM_TYPE(server, down_host) TParamServerDownHost;
static CSafeStatic<TParamServerDownHost> s_DownHost;

NCBI_PARAM_DECL(unsigned short, server, down_port);
NCBI_PARAM_DEF_EX(unsigned short, server, down_port, 1444, 0, DBAPI_SVC_TEST_DOWN_PORT);
typedef NCBI_PARAM_TYPE(server, down_port) TParamServerDownPort;
static CSafeStatic<TParamServerDownPort> s_DownPort;

NCBI_PARAM_DECL(string, server, nonexist_host);
NCBI_PARAM_DEF_EX(string, server, nonexist_host, "nosuchhostoivk3qaaa0mzoqgif6tbbsrsgza", 0, DBAPI_SVC_TEST_NONEXIST_HOST);
typedef NCBI_PARAM_TYPE(server, nonexist_host) TParamServerNonexistHost;
static CSafeStatic<TParamServerNonexistHost> s_NonexistHost;

NCBI_PARAM_DECL(unsigned short, server, nonexist_port);
NCBI_PARAM_DEF_EX(unsigned short, server, nonexist_port, 1444, 0, DBAPI_SVC_TEST_NONEXIST_PORT);
typedef NCBI_PARAM_TYPE(server, nonexist_port) TParamServerNonexistPort;
static CSafeStatic<TParamServerNonexistPort> s_NonexistPort;

NCBI_PARAM_DECL(string, server, up_host);
NCBI_PARAM_DEF_EX(string, server, up_host, "mssql76", 0, DBAPI_SVC_TEST_UP_HOST);
typedef NCBI_PARAM_TYPE(server, up_host) TParamServerUpHost;
static CSafeStatic<TParamServerUpHost> s_UpHost;

NCBI_PARAM_DECL(unsigned short, server, up_port);
NCBI_PARAM_DEF_EX(unsigned short, server, up_port, 1433, 0, DBAPI_SVC_TEST_UP_PORT);
typedef NCBI_PARAM_TYPE(server, up_port) TParamServerUpPort;
static CSafeStatic<TParamServerUpPort> s_UpPort;


// Results
enum : size_t {
    eResult_NONE,
    eResult_ERROR,
    eResult_BK,
    eResult_DN,
    eResult_NE,
    eResult_UP,
    eResult_SKIP_FAIL,
    eResult_SKIP_USER,
    eResult_SKIP_CTLIB,
    eResult_SKIP_INTF,
};
static string   s_Results[] = {
    string("NONE"),
    string("ERROR"),
    string("BK"),
    string("DN"),
    string("NE"),
    string("UP"),
    string("SKIP_FAIL"),
    string("SKIP_USER"),
    string("SKIP_CTLIB"),
    string("SKIP_INTF"),
};


// Database connection parameters
static const string skDbParamUser("anyone");
static const string skDbParamPassword("allowed");
static const string skDbParamDb("master");


// API functions

typedef size_t  (*FApiCheck)(const string& service);
typedef bool    (*FApiInit)(const string& service);
typedef bool    (*FApiQuit)(void);

enum : size_t {
    eApi_DbapiFtds, eApi_DbapiCtlib
};
static const string skDbapiNames[] = {"ftds", "ctlib"};

static IDataSource*             s_Dbapi_Ds;
static IConnection*             s_Dbapi_Conn;
static CNcbiOfstream            s_Dbapi_Logstream;
static string                   s_Dbapi_LogFileName;
static CDB_UserHandler_Stream   s_Dbapi_ExHandler;


class CErrHandler : public CDB_UserHandler
{
public:
    CErrHandler(const string& type, CNcbiOfstream* log_stream)
        : m_Type(type), s_Dbapi_Logstream(log_stream)
    {
    }

    virtual bool HandleIt(CDB_Exception* ex)
    {
        // Use the default handler when no exception info is available.
        if (!ex) {
            return false;
        }

        // Log all exceptions coming from the server.
        *s_Dbapi_Logstream << m_Type << " " << ex->SeverityString() << ": ";
        *s_Dbapi_Logstream << ex->ReportThis() << NcbiEndl;

        // NOTE: DBAPI converts server exceptions into C++ exceptions with
        // severity eDiag_Error, and less severe server errors into C++
        // exceptions with severity eDiag_Info.

        // Only throw for the more severe server exceptions.
        if (ex->GetSeverity() >= eDiag_Error) {
            throw *ex; // Handle in the main client code.
        }

        // No need for further processing by other handlers.
        return true;
    }

private:
    string          m_Type;
    CNcbiOstream*   s_Dbapi_Logstream;
};


static size_t s_Api_DBAPI_Check(const string& service, size_t api_id)
{
    size_t result = eResult_ERROR;

    string key("12345");
    string sql(string("SELECT '") + key + "'");
    unique_ptr<IStatement> stmt(s_Dbapi_Conn->CreateStatement());
    stmt->Execute(sql);

    while (stmt->HasMoreResults()) {
        unique_ptr<IResultSet> rs(stmt->GetResultSet());
        if ( ! stmt->HasRows() ) {
            continue;
        }
        switch (rs->GetResultType()) {
        case eDB_RowResult: {
            const IResultSetMetaData* rsMeta = rs->GetMetaData();
            while (rs->Next()) {
                for (unsigned i = 1; i <= rsMeta->GetTotalColumns(); ++i) {
                    string qry_result(rs->GetVariant(i).GetString());
                    if (qry_result == key) {
                        result = eResult_UP;
                    } else {
                        ERR_POST(Error << "\n\ns_Api_DBAPI_" << skDbapiNames[api_id] << "_Check() expected '" << key << "' but got '" << qry_result << "'.\n\n");
                    }
                }
            }
            break;
        }
        default:
            ERR_POST_X(1, Critical << "Unexpected results type:"
                << rs->GetResultType());
        }
    }

    return result;
}

static bool s_Api_DBAPI_Init(const string& service, size_t api_id)
{
    ERR_POST(Info << "s_Api_DBAPI_" << skDbapiNames[api_id] << "_Init() opening database, service=" << service);

    DBLB_INSTALL_DEFAULT();
                                                                                               ERR_POST(Info << "s_Api_DBAPI_" << skDbapiNames[api_id] << "_Init() opening database, service=" << service);
    if (api_id == eApi_DbapiCtlib) {
        DBAPI_RegisterDriver_CTLIB();
    } else {
        DBAPI_RegisterDriver_FTDS();
    }
                                                                                               ERR_POST(Info << "s_Api_DBAPI_" << skDbapiNames[api_id] << "_Init() opening database, service=" << service);

    try {
        CDriverManager& dm(CDriverManager::GetInstance());
                                                                                               ERR_POST(Info << "s_Api_DBAPI_" << skDbapiNames[api_id] << "_Init() opening database, service=" << service);
        IDataSource *s_Dbapi_Ds = dm.CreateDs(skDbapiNames[api_id]);
                                                                                               ERR_POST(Info << "s_Api_DBAPI_" << skDbapiNames[api_id] << "_Init() opening database, service=" << service);
        s_Dbapi_Logstream.open(s_Dbapi_LogFileName.c_str());
                                                                                               ERR_POST(Info << "s_Api_DBAPI_" << skDbapiNames[api_id] << "_Init() opening database, service=" << service);
        s_Dbapi_Ds->SetLogStream(&s_Dbapi_Logstream);
                                                                                               ERR_POST(Info << "s_Api_DBAPI_" << skDbapiNames[api_id] << "_Init() opening database, service=" << service);
        I_DriverContext* drv_context = s_Dbapi_Ds->GetDriverContext();
                                                                                               ERR_POST(Info << "s_Api_DBAPI_" << skDbapiNames[api_id] << "_Init() opening database, service=" << service);
        drv_context->PushCntxMsgHandler(
            new CErrHandler("General", &s_Dbapi_Logstream),
            eTakeOwnership);
                                                                                               ERR_POST(Info << "s_Api_DBAPI_" << skDbapiNames[api_id] << "_Init() opening database, service=" << service);
        drv_context->PushDefConnMsgHandler(
            new CErrHandler("Connection", &s_Dbapi_Logstream),
            eTakeOwnership);
                                                                                               ERR_POST(Info << "s_Api_DBAPI_" << skDbapiNames[api_id] << "_Init() opening database, service=" << service);
        drv_context->SetLoginTimeout(2);
                                                                                               ERR_POST(Info << "s_Api_DBAPI_" << skDbapiNames[api_id] << "_Init() opening database, service=" << service);
        drv_context->SetTimeout(3);
                                                                                               ERR_POST(Info << "s_Api_DBAPI_" << skDbapiNames[api_id] << "_Init() opening database, service=" << service);
        // Use the registry for these values (or add command-line argument):
        // SetMaxNumOfConnAttempts(1);
        // SetMaxNumOfServerAlternatives(1);
        s_Dbapi_Conn = s_Dbapi_Ds->CreateConnection();
                                                                                               ERR_POST(Info << "s_Api_DBAPI_" << skDbapiNames[api_id] << "_Init() opening database, service=" << service);
        if (s_Dbapi_Conn == NULL) {
            ERR_POST(Fatal << "Could not create connection.");
        }
                                                                                               ERR_POST(Info << "s_Api_DBAPI_" << skDbapiNames[api_id] << "_Init() opening database, service=" << service);
        CTrivialConnValidator val(skDbParamDb);
                                                                                               ERR_POST(Info << "s_Api_DBAPI_" << skDbapiNames[api_id] << "_Init() opening database, service=" << service);
        ///conn->Connect(skDbParamUser, skDbParamPassword, service, skDbParamDb);
        s_Dbapi_Conn->ConnectValidated(val, skDbParamUser, skDbParamPassword, service, skDbParamDb);
                                                                                               ERR_POST(Info << "s_Api_DBAPI_" << skDbapiNames[api_id] << "_Init() opening database, service=" << service);
    } catch (CException& exc) {
        ERR_POST(Critical << "s_Api_DBAPI_" << skDbapiNames[api_id] << "_Init() caught exception '" << exc.what() << "'");
        throw;
    } catch (...) {
        ERR_POST(Critical << "s_Api_DBAPI_" << skDbapiNames[api_id] << "_Init() caught unknown exception.");
        throw;
    }

    return true;
}

static bool s_Api_DBAPI_Quit(size_t api_id)
{
    ERR_POST(Info << "s_Api_DBAPI_" << skDbapiNames[api_id] << "_Quit() closing database.");
    CDriverManager& dm(CDriverManager::GetInstance());
    dm.DestroyDs(s_Dbapi_Ds);
    s_Dbapi_Logstream.flush();
    return true;
}


static size_t s_Api_DBAPI_FTDS_Check(const string& service)
{
    return s_Api_DBAPI_Check(service, eApi_DbapiFtds);
}

static bool s_Api_DBAPI_FTDS_Init(const string& service)
{
    return s_Api_DBAPI_Init(service, eApi_DbapiFtds);
}

static bool s_Api_DBAPI_FTDS_Quit(void)
{
    return s_Api_DBAPI_Quit(eApi_DbapiFtds);
}


static size_t s_Api_DBAPI_CTLIB_Check(const string& service)
{
    return s_Api_DBAPI_Check(service, eApi_DbapiCtlib);
}

static bool s_Api_DBAPI_CTLIB_Init(const string& service)
{
    return s_Api_DBAPI_Init(service, eApi_DbapiCtlib);
}

static bool s_Api_DBAPI_CTLIB_Quit(void)
{
    return s_Api_DBAPI_Quit(eApi_DbapiCtlib);
}


enum : size_t {
    eApi_Sdbapi, eApi_Dblb
};
static const string skSdbapiDblbNames[] = {"SDBAPI", "DBLB"};

static CDatabase    s_Api_SDBAPI_db;

static size_t s_Api_SDBAPI_DBLB_Check(const string& service, size_t api_id)
{
    size_t result = eResult_ERROR;

    try {
        string key("12345");
        string sql(string("SELECT '") + key + "'");

        CQuery query = s_Api_SDBAPI_db.NewQuery(sql);
        query.Execute();
        query.RequireRowCount(1);
        string qry_result;
        for (const auto& row: query.SingleSet()) {
            qry_result = row[1].AsString();
            break;
        }
        query.VerifyDone();

        if (qry_result == key) {
            result = eResult_UP;
        } else {
            ERR_POST(Error << "s_Api_" << skSdbapiDblbNames[api_id] << "_Check() expected '" << key << "' but got '" << qry_result << "'.");
        }
    } catch (CException& exc) {
        ERR_POST(Critical << "s_Api_" << skSdbapiDblbNames[api_id] << "_Check() caught exception '" << exc.what() << "'");
    } catch (...) {
        ERR_POST(Critical << "s_Api_" << skSdbapiDblbNames[api_id] << "_Check() caught unknown exception.");
    }

    return result;
}

static bool s_Api_SDBAPI_DBLB_Init(const string& service, size_t api_id)
{
    ERR_POST(Info << "s_Api_" << skSdbapiDblbNames[api_id] << "_Init() opening database, service=" << service);
    CSDB_ConnectionParam    db_params;
    db_params
        .Set(CSDB_ConnectionParam::eUsername, skDbParamUser)
        .Set(CSDB_ConnectionParam::ePassword, skDbParamPassword)
        .Set(CSDB_ConnectionParam::eService,  service)
        .Set(CSDB_ConnectionParam::eDatabase, skDbParamDb)
        .Set(CSDB_ConnectionParam::eLoginTimeout, "2")
        .Set(CSDB_ConnectionParam::eIOTimeout, "3");
    s_Api_SDBAPI_db = CDatabase(db_params);
    s_Api_SDBAPI_db.Connect();

    return true;
}

static bool s_Api_SDBAPI_DBLB_Quit(size_t api_id)
{
    ERR_POST(Info << "s_Api_" << skSdbapiDblbNames[api_id] << "_Quit() closing database.");
    s_Api_SDBAPI_db.Close();
    return true;
}


static size_t s_Api_SDBAPI_Check(const string& service)
{
    return s_Api_SDBAPI_DBLB_Check(service, eApi_Sdbapi);
}

static bool s_Api_SDBAPI_Init(const string& service)
{
    return s_Api_SDBAPI_DBLB_Init(service, eApi_Sdbapi);
}

static bool s_Api_SDBAPI_Quit(void)
{
    return s_Api_SDBAPI_DBLB_Quit(eApi_Sdbapi);
}


static size_t s_Api_DBLB_Check(const string& service)
{
    return s_Api_SDBAPI_DBLB_Check(service, eApi_Dblb);
}


static IDBServiceMapper* s_GetServiceMapper(void)
{
#if 0 // CRuntimeData and GetRuntimeData are protected
    ERR_POST(Info << "s_GetServiceMapper() - CRuntimeData and GetRuntimeData are protected");
    return &(dynamic_cast<CDBConnectionFactory&>
             (*CDbapiConnMgr::Instance().GetConnectionFactory())
             .GetRuntimeData(kEmptyStr).GetDBServiceMapper());
#elif defined(HAVE_LIBCONNEXT)
    ERR_POST(Info << "s_GetServiceMapper() - HAVE_LIBCONNEXT");
    static CDBLB_ServiceMapper mapper(&CNcbiApplication::Instance()->GetConfig());
    return &mapper;
#else
    ERR_POST(Info << "s_GetServiceMapper() - else");
    return NULL;
#endif
}

static bool s_Api_DBLB_Init(const string& service)
{
    ERR_POST(Info << "s_Api_DBLB_Init() incoming service = '" << service << "'");

    CSDB_ConnectionParam param;
    param.Set(service);
    param.Set(CSDB_ConnectionParam::eUsername, skDbParamUser);
    param.Set(CSDB_ConnectionParam::ePassword, skDbParamPassword);
    param.Set(CSDB_ConnectionParam::eDatabase, skDbParamDb);

    CDatabase db(param);
    CQuery query;
    {{
        CDiagCollectGuard diag_guard(eDiag_Fatal, eDiag_Info,
                                     CDiagCollectGuard::ePrint);
        db.Connect();
        query = db.NewQuery();
        diag_guard.SetAction(CDiagCollectGuard::eDiscard);
    }}
    // Hack to obtain the actual server name - cause an exception and get the
    // server name from the exception info.
    string  server_name;
    try {
        query.Execute();
    } catch (CSDB_Exception& e) {
        const string& name = e.GetServerName();
        ERR_POST(Info << "s_Api_DBLB_Init() e.GetServerName() => '" << name << "'");
        ERR_POST(Info << "s_Api_DBLB_Init() e => '" << e << "'");
        TSvrRef server;
        TSvrRef pref(new CDBServer(name, CSocketAPI::gethostbyname(name)));
        IDBServiceMapper* mapper = s_GetServiceMapper();
        if (mapper != NULL) {
            // server.Reset(mapper->GetServer(e.GetServerName()));
            const string& service = param.Get(CSDB_ConnectionParam::eService);
            mapper->SetPreference(service, pref);
            server.Reset(mapper->GetServer(service));
        } else {
            ERR_POST(Error << "s_Api_DBLB_Init() - NO MAPPER!");
        }
        if (server.Empty()  ||  server->GetName().empty()) {
            ERR_POST(Info << "s_Api_DBLB_Init() pref = '" << pref->GetName() << "'");
            server.Reset(pref);
        }
        server_name = server->GetName();
        ERR_POST(Info << "s_Api_DBLB_Init() got server name '" << server_name << "'");
    }

    return s_Api_SDBAPI_DBLB_Init(server_name, eApi_Dblb);
}

static bool s_Api_DBLB_Quit(void)
{
    return s_Api_SDBAPI_DBLB_Quit(eApi_Dblb);
}


// Test data structures
struct STest {
    size_t      m_Id;
    size_t      m_ApiId;
    size_t      m_SetupId;
    size_t      m_MapperId;
    size_t      m_ResultExpectedId;
    size_t      m_ResultActualId;
    const char* m_Result;
};
static vector<STest>    s_Tests;
static vector<size_t>   s_SelectedTestIds;
static vector<size_t>   s_SkippedTestIds;


// APIs
class CApi
{
public:
    string      m_Name;
    bool        m_Inited;

    FApiCheck   m_Check;
    FApiInit    m_Init;
    FApiQuit    m_Quit;

    void    Check(STest& test);
    bool    Init(const STest& test);
    bool    Quit(void);
};
static CApi s_Apis[] = {
    {string("DBAPI_FTDS"),  false, s_Api_DBAPI_FTDS_Check , s_Api_DBAPI_FTDS_Init , s_Api_DBAPI_FTDS_Quit },
    {string("DBAPI_CTLIB"), false, s_Api_DBAPI_CTLIB_Check, s_Api_DBAPI_CTLIB_Init, s_Api_DBAPI_CTLIB_Quit},
    {string("SDBAPI"),      false, s_Api_SDBAPI_Check     , s_Api_SDBAPI_Init     , s_Api_SDBAPI_Quit     },
    {string("DBLB"),        false, s_Api_DBLB_Check       , s_Api_DBLB_Init       , s_Api_DBLB_Quit       }
};
static size_t           s_NumApis = sizeof(s_Apis) / sizeof(*s_Apis);
static vector<size_t>   s_SelectedApiIds;
static size_t           s_ApiInitedId = 0;
static bool             s_ApiInited = false;


// Name resolution sources - LBSMD/namerd
enum : size_t {
    eResLbNm_NOLB,
    eResLbNm_LB
};
static string   s_ResLbNms[] = {
    string("NOLB"),
    string("LB")
};

// Name resolution sources - Reverse DNS
enum : size_t {
    eResRev_NOREV,
    eResRev_REV
};
static string   s_ResRevs[] = {
    string("NOREV"),
    string("REV")
};

// Name resolution sources - Interfaces/DNS
enum : size_t {
    eResIdns_NOI,
    eResIdns_I
};
static string   s_ResIdnss[] = {
    string("NOI"),
    string("I")
};

// Server setups
enum : size_t {
    eServer_NONE,
    eServer_BK,
    eServer_DN,
    eServer_NE,
    eServer_UP,
};
static string   s_Servers[] = {
    string("NONE"),
    string("BK"),
    string("DN"),
    string("NE"),
    string("UP"),
};
static size_t           s_NumServers = sizeof(s_Servers) / sizeof(*s_Servers);
static vector<size_t>   s_SelectedServer1Ids;
static vector<size_t>   s_SelectedServer2Ids;


// Service setups
enum : size_t {
    eSetup_NOLB_NOI,
    eSetup_NOLB_I_BK,
    eSetup_NOLB_I_DN,
    eSetup_NOLB_I_NE,
    eSetup_NOLB_I_UP,
    eSetup_LB_NOI_BK,
    eSetup_LB_NOI_DN,
    eSetup_LB_NOI_NE,
    eSetup_LB_NOI_UP,
    eSetup_LB_I_BK,
    eSetup_LB_I_DN,
    eSetup_LB_I_NE,
    eSetup_LB_I_UP,
    eSetup_LBR_NOI_BK,
    eSetup_LBR_NOI_DN,
    eSetup_LBR_NOI_NE,
    eSetup_LBR_NOI_UP,
    eSetup_LBR_I_BK,
    eSetup_LBR_I_DN,
    eSetup_LBR_I_NE,
    eSetup_LBR_I_UP,
    eSetup_NOLB_I_BK_UP,
    eSetup_NOLB_I_DN_UP,
    eSetup_NOLB_I_NE_UP,
    eSetup_LB_NOI_BK_UP,
    eSetup_LB_NOI_DN_UP,
    eSetup_LB_NOI_NE_UP,
    eSetup_LB_I_BK_UP,
    eSetup_LB_I_DN_UP,
    eSetup_LB_I_NE_UP,
    eSetup_LBR_NOI_BK_UP,
    eSetup_LBR_NOI_DN_UP,
    eSetup_LBR_NOI_NE_UP,
    eSetup_LBR_I_BK_UP,
    eSetup_LBR_I_DN_UP,
    eSetup_LBR_I_NE_UP,
};
// NOTE: For the foreseeable future (as of 2019-10-25), namerd will be getting
// its data from LBSMD, and down servers are filtered out of that data.  Thus,
// for eSetup_LBR_NOI_DN_UP and eSetup_LBR_I_DN_UP, the down server will not be
// returned by namerd and only the up server will be returned.  Thus, these test
// cases won't really test a down-up scenario and should always pass.  As such,
// they are benign extraneous test cases.  However, when (if) namerd gets its
// data independently (or includes down servers), these test cases will become
// relevant.  So they have been left in.
struct SSetup
{
    size_t  m_Id;
    string  m_Service;
    bool    m_Sybase;
    size_t  m_ResLbNm;
    size_t  m_ResRev;
    size_t  m_ResIdns;
    size_t  m_Server1;
    size_t  m_Server2;
    size_t  m_ResultExpectedId;
};
static SSetup   s_Setups[] = {
    ///{eSetup_??????????    , "DBAPI_SYB160_TEST", true , eResLbNm_LB  , eResRev_REV  , eResIdns_I  , eServer_UP  , eServer_NONE, eResult_UP          },
    ///{eSetup_??????????    , "CXX_TESTSUITE", false, eResLbNm_LB  , eResRev_REV  , eResIdns_I  , eServer_UP  , eServer_NONE, eResult_UP          },

    { eSetup_NOLB_NOI     , "DT_NOI"          , false, eResLbNm_NOLB, eResRev_NOREV, eResIdns_NOI, eServer_NONE, eServer_NONE, eResult_ERROR       },
    { eSetup_NOLB_I_BK    , "DT_I_BK"         , false, eResLbNm_NOLB, eResRev_NOREV, eResIdns_I  , eServer_BK  , eServer_NONE, eResult_ERROR       /* eResult_BK */ },
    { eSetup_NOLB_I_DN    , "DT_I_DN"         , false, eResLbNm_NOLB, eResRev_NOREV, eResIdns_I  , eServer_DN  , eServer_NONE, eResult_ERROR       /* eResult_DN */ },
    { eSetup_NOLB_I_NE    , "DT_I_NE"         , false, eResLbNm_NOLB, eResRev_NOREV, eResIdns_I  , eServer_NE  , eServer_NONE, eResult_ERROR       },
    { eSetup_NOLB_I_UP    , "DT_I_UP"         , false, eResLbNm_NOLB, eResRev_NOREV, eResIdns_I  , eServer_UP  , eServer_NONE, eResult_UP          },
    { eSetup_LB_NOI_BK    , "DT_LB_NOI_BK"    , false, eResLbNm_LB  , eResRev_NOREV, eResIdns_NOI, eServer_BK  , eServer_NONE, eResult_ERROR       /* eResult_BK */ },
    { eSetup_LB_NOI_DN    , "DT_LB_NOI_DN"    , false, eResLbNm_LB  , eResRev_NOREV, eResIdns_NOI, eServer_DN  , eServer_NONE, eResult_ERROR       /* eResult_DN */ },
    { eSetup_LB_NOI_NE    , "DT_LB_NOI_NE"    , false, eResLbNm_LB  , eResRev_NOREV, eResIdns_NOI, eServer_NE  , eServer_NONE, eResult_ERROR       },
    { eSetup_LB_NOI_UP    , "DT_LB_NOI_UP"    , false, eResLbNm_LB  , eResRev_NOREV, eResIdns_NOI, eServer_UP  , eServer_NONE, eResult_UP          },
    { eSetup_LB_I_BK      , "DT_LB_I_BK"      , false, eResLbNm_LB  , eResRev_NOREV, eResIdns_I  , eServer_BK  , eServer_NONE, eResult_ERROR       /* eResult_BK */ },
    { eSetup_LB_I_DN      , "DT_LB_I_DN"      , false, eResLbNm_LB  , eResRev_NOREV, eResIdns_I  , eServer_DN  , eServer_NONE, eResult_ERROR       /* eResult_DN */ },
    { eSetup_LB_I_NE      , "DT_LB_I_NE"      , false, eResLbNm_LB  , eResRev_NOREV, eResIdns_I  , eServer_NE  , eServer_NONE, eResult_ERROR       },
    { eSetup_LB_I_UP      , "DT_LB_I_UP"      , false, eResLbNm_LB  , eResRev_NOREV, eResIdns_I  , eServer_UP  , eServer_NONE, eResult_UP          },
    { eSetup_LBR_NOI_BK   , "DT_LBR_NOI_BK"   , false, eResLbNm_LB  , eResRev_REV  , eResIdns_NOI, eServer_BK  , eServer_NONE, eResult_ERROR       /* eResult_BK */ },
    { eSetup_LBR_NOI_DN   , "DT_LBR_NOI_DN"   , false, eResLbNm_LB  , eResRev_REV  , eResIdns_NOI, eServer_DN  , eServer_NONE, eResult_ERROR       /* eResult_DN */ },
    { eSetup_LBR_NOI_NE   , "DT_LBR_NOI_NE"   , false, eResLbNm_LB  , eResRev_REV  , eResIdns_NOI, eServer_NE  , eServer_NONE, eResult_ERROR       },
    { eSetup_LBR_NOI_UP   , "DT_LBR_NOI_UP"   , false, eResLbNm_LB  , eResRev_REV  , eResIdns_NOI, eServer_UP  , eServer_NONE, eResult_UP          },
    { eSetup_LBR_I_BK     , "DT_LBR_I_BK"     , false, eResLbNm_LB  , eResRev_REV  , eResIdns_I  , eServer_BK  , eServer_NONE, eResult_ERROR       /* eResult_BK */ },
    { eSetup_LBR_I_DN     , "DT_LBR_I_DN"     , false, eResLbNm_LB  , eResRev_REV  , eResIdns_I  , eServer_DN  , eServer_NONE, eResult_ERROR       /* eResult_DN */ },
    { eSetup_LBR_I_NE     , "DT_LBR_I_NE"     , false, eResLbNm_LB  , eResRev_REV  , eResIdns_I  , eServer_NE  , eServer_NONE, eResult_ERROR       },
    { eSetup_LBR_I_UP     , "DT_LBR_I_UP"     , false, eResLbNm_LB  , eResRev_REV  , eResIdns_I  , eServer_UP  , eServer_NONE, eResult_UP          },
    { eSetup_NOLB_I_BK_UP , "DT_I_BK_UP"      , false, eResLbNm_NOLB, eResRev_NOREV, eResIdns_I  , eServer_BK  , eServer_UP  , eResult_UP          },
    { eSetup_NOLB_I_DN_UP , "DT_I_DN_UP"      , false, eResLbNm_NOLB, eResRev_NOREV, eResIdns_I  , eServer_DN  , eServer_UP  , eResult_UP          },
    { eSetup_NOLB_I_NE_UP , "DT_I_NE_UP"      , false, eResLbNm_NOLB, eResRev_NOREV, eResIdns_I  , eServer_NE  , eServer_UP  , eResult_UP          },
    { eSetup_LB_NOI_BK_UP , "DT_LB_NOI_BK_UP" , false, eResLbNm_LB  , eResRev_NOREV, eResIdns_NOI, eServer_BK  , eServer_UP  , eResult_UP          },
    { eSetup_LB_NOI_DN_UP , "DT_LB_NOI_DN_UP" , false, eResLbNm_LB  , eResRev_NOREV, eResIdns_NOI, eServer_DN  , eServer_UP  , eResult_UP          },
    { eSetup_LB_NOI_NE_UP , "DT_LB_NOI_NE_UP" , false, eResLbNm_LB  , eResRev_NOREV, eResIdns_NOI, eServer_NE  , eServer_UP  , eResult_UP          },
    { eSetup_LB_I_BK_UP   , "DT_LB_I_BK_UP"   , false, eResLbNm_LB  , eResRev_NOREV, eResIdns_I  , eServer_BK  , eServer_UP  , eResult_UP          },
    { eSetup_LB_I_DN_UP   , "DT_LB_I_DN_UP"   , false, eResLbNm_LB  , eResRev_NOREV, eResIdns_I  , eServer_DN  , eServer_UP  , eResult_UP          },
    { eSetup_LB_I_NE_UP   , "DT_LB_I_NE_UP"   , false, eResLbNm_LB  , eResRev_NOREV, eResIdns_I  , eServer_NE  , eServer_UP  , eResult_UP          },
    { eSetup_LBR_NOI_BK_UP, "DT_LBR_NOI_BK_UP", false, eResLbNm_LB  , eResRev_REV  , eResIdns_NOI, eServer_BK  , eServer_UP  , eResult_UP          },
    { eSetup_LBR_NOI_DN_UP, "DT_LBR_NOI_DN_UP", false, eResLbNm_LB  , eResRev_REV  , eResIdns_NOI, eServer_DN  , eServer_UP  , eResult_UP          },
    { eSetup_LBR_NOI_NE_UP, "DT_LBR_NOI_NE_UP", false, eResLbNm_LB  , eResRev_REV  , eResIdns_NOI, eServer_NE  , eServer_UP  , eResult_UP          },
    { eSetup_LBR_I_BK_UP  , "DT_LBR_I_BK_UP"  , false, eResLbNm_LB  , eResRev_REV  , eResIdns_I  , eServer_BK  , eServer_UP  , eResult_UP          },
    { eSetup_LBR_I_DN_UP  , "DT_LBR_I_DN_UP"  , false, eResLbNm_LB  , eResRev_REV  , eResIdns_I  , eServer_DN  , eServer_UP  , eResult_UP          },
    { eSetup_LBR_I_NE_UP  , "DT_LBR_I_NE_UP"  , false, eResLbNm_LB  , eResRev_REV  , eResIdns_I  , eServer_NE  , eServer_UP  , eResult_UP          },
};
static size_t           s_NumSetups = sizeof(s_Setups) / sizeof(*s_Setups);
static vector<size_t>   s_SelectedSetupIds;


// Mappers
static string s_LocalServerSpec(const string& host, int port)
{
    return string("STANDALONE ") + host + ":" + NStr::IntToString(port) + " L=yes";
}
class CMapper
{
public:
    static void Init(vector<CMapper>& mappers)
    {
        {{
            CMapper mapper;
            mapper.m_Name = "DISPD";
            mapper.m_EnvSet["CONN_DISPD_DISABLE"] = "0";
            mapper.m_EnvSet["CONN_LBSMD_DISABLE"] = "1";
            mapper.m_EnvSet["CONN_LINKERD_ENABLE"] = "0";
            mapper.m_EnvSet["CONN_LOCAL_ENABLE"] = "0";
            mapper.m_EnvSet["CONN_NAMERD_ENABLE"] = "0";
            mapper.m_EnvUnset.push_back("CONN_LOCAL_SERVICES");
            mapper.m_EnvUnset.push_back("TEST_CONN_LOCAL_SERVER_1");
            mapper.m_EnvUnset.push_back("BOUNCEHTTP_CONN_LOCAL_SERVER_1");
            mappers.push_back(mapper);
        }}
        {{
            CMapper mapper;
            mapper.m_Name = "LBSMD";
            mapper.m_EnvSet["CONN_DISPD_DISABLE"] = "1";
            mapper.m_EnvSet["CONN_LBSMD_DISABLE"] = "0";
            mapper.m_EnvSet["CONN_LINKERD_ENABLE"] = "0";
            mapper.m_EnvSet["CONN_LOCAL_ENABLE"] = "0";
            mapper.m_EnvSet["CONN_NAMERD_ENABLE"] = "0";
            mapper.m_EnvUnset.push_back("CONN_LOCAL_SERVICES");
            mapper.m_EnvUnset.push_back("TEST_CONN_LOCAL_SERVER_1");
            mapper.m_EnvUnset.push_back("BOUNCEHTTP_CONN_LOCAL_SERVER_1");
            mappers.push_back(mapper);
        }}
        {{
            CMapper mapper;
            mapper.m_Name = "Linkerd";
            mapper.m_EnvSet["CONN_DISPD_DISABLE"] = "1";
            mapper.m_EnvSet["CONN_LBSMD_DISABLE"] = "1";
            mapper.m_EnvSet["CONN_LINKERD_ENABLE"] = "1";
            mapper.m_EnvSet["CONN_LOCAL_ENABLE"] = "0";
            mapper.m_EnvSet["CONN_NAMERD_ENABLE"] = "0";
            mapper.m_EnvUnset.push_back("CONN_LOCAL_SERVICES");
            mapper.m_EnvUnset.push_back("TEST_CONN_LOCAL_SERVER_1");
            mapper.m_EnvUnset.push_back("BOUNCEHTTP_CONN_LOCAL_SERVER_1");
            mappers.push_back(mapper);
        }}
        {{
            string bk_env(s_LocalServerSpec(s_BrokenHost->Get(), s_BrokenPort->Get()));
            string dn_env(s_LocalServerSpec(s_DownHost->Get(), s_DownPort->Get()));
            string ne_env(s_LocalServerSpec(s_NonexistHost->Get(), s_NonexistPort->Get()));
            string up_env(s_LocalServerSpec(s_UpHost->Get(), s_UpPort->Get()));
            CMapper mapper;
            mapper.m_Name = "LOCAL";
            mapper.m_EnvSet["CONN_DISPD_DISABLE"] = "1";
            mapper.m_EnvSet["CONN_LBSMD_DISABLE"] = "1";
            mapper.m_EnvSet["CONN_LINKERD_ENABLE"] = "0";
            mapper.m_EnvSet["CONN_LOCAL_ENABLE"] = "1";
            mapper.m_EnvSet["CONN_NAMERD_ENABLE"] = "0";
            mapper.m_EnvSet["CONN_LOCAL_SERVICES"] = "LB_NOI_BK LB_NOI_DN LB_NOI_NE LB_NOI_UP";
            mapper.m_EnvSet["LB_NOI_BK_CONN_LOCAL_SERVER_1"] = bk_env.c_str();
            mapper.m_EnvSet["LB_NOI_DN_CONN_LOCAL_SERVER_1"] = dn_env.c_str();
            mapper.m_EnvSet["LB_NOI_NE_CONN_LOCAL_SERVER_1"] = ne_env.c_str();
            mapper.m_EnvSet["LB_NOI_UP_CONN_LOCAL_SERVER_1"] = up_env.c_str();
            mappers.push_back(mapper);
        }}
        {{
            CMapper mapper;
            mapper.m_Name = "namerd";
            mapper.m_EnvSet["CONN_DISPD_DISABLE"] = "1";
            mapper.m_EnvSet["CONN_LBSMD_DISABLE"] = "1";
            mapper.m_EnvSet["CONN_LINKERD_ENABLE"] = "0";
            mapper.m_EnvSet["CONN_LOCAL_ENABLE"] = "0";
            mapper.m_EnvSet["CONN_NAMERD_ENABLE"] = "1";
            mapper.m_EnvUnset.push_back("CONN_LOCAL_SERVICES");
            mapper.m_EnvUnset.push_back("TEST_CONN_LOCAL_SERVER_1");
            mapper.m_EnvUnset.push_back("BOUNCEHTTP_CONN_LOCAL_SERVER_1");
            mappers.push_back(mapper);
        }}
    }

    static void Select(size_t mapper_id);

    bool                m_Enabled;
    string              m_Name;
    map<string, string> m_EnvSet;
    list<string>        m_EnvUnset;
};
static vector<CMapper>  s_Mappers;
static vector<size_t>   s_SelectedMapperIds;

void CMapper::Select(size_t mapper_id)
{
    static bool     first = true;
    static size_t   last_id;

    if (first  ||  last_id != mapper_id) {
        for (const auto& env : s_Mappers[mapper_id].m_EnvSet) {
            setenv(env.first.c_str(), env.second.c_str(), 1);
        }
        for (const string& env : s_Mappers[mapper_id].m_EnvUnset) {
            unsetenv(env.c_str());
        }
    }
    last_id = mapper_id;
    first = false;
}


void CApi::Check(STest& test)
{
    test.m_ResultActualId = eResult_ERROR;
    try {
        if (Init(test)) {
            test.m_ResultActualId = (*m_Check)(s_Setups[test.m_SetupId].m_Service);
        }
    } catch (CException& exc) {
        ERR_POST(Critical << "API Check() caught exception '" << exc.what() << "'");
    } catch (...) {
        ERR_POST(Critical << "API Check() caught unknown exception.");
    }
}

bool CApi::Init(const STest& test)
{
    bool success = false;
    const string& service(s_Setups[test.m_SetupId].m_Service);

    // Quit the last API if there is one still inited.
    if (s_ApiInited) {
        try {
            if ( ! s_Apis[s_ApiInitedId].Quit()) {
                ERR_POST(Critical << "API Init() failed to quit last API.");
                return false;
            }
        } catch (CException& exc) {
            ERR_POST(Critical << "API Init() caught exception '" << exc.what() << "'");
            return false;
        } catch (...) {
            ERR_POST(Critical << "API Init() caught unknown exception.");
            return false;
        }
    }

    try {
        if ((*m_Init)(service)) {
            s_ApiInited = true;
            s_ApiInitedId = test.m_ApiId;
            m_Inited = true;
            success = true;
        } else {
            ERR_POST(Error << "API Init() failed for API=" << m_Name << " and service=" << service);
        }
    } catch (CException& exc) {
        ERR_POST(Critical << "API Init() caught exception '" << exc.what() << "'");
    } catch (...) {
        ERR_POST(Critical << "API Init() caught unknown exception.");
    }

    return success;
}

bool CApi::Quit(void)
{
    bool success = true;
    if (s_ApiInited) {
        s_ApiInited = false;
        try {
            CApi& api(s_Apis[s_ApiInitedId]);    // quit last inited api, not current
            api.m_Inited = false;
            if ( ! (*api.m_Quit)()) {
                success = false;
                ERR_POST(Error << "API Quit() failed for API=" << api.m_Name);
            }
        } catch (CException& exc) {
            ERR_POST(Critical << "API Quit() caught exception '" << exc.what() << "'");
        } catch (...) {
            ERR_POST(Critical << "API Quit() caught unknown exception.");
        }
    }
    return success;
}



class CTestNcbiDblbSvcResApp : public CNcbiApplication
{
public:
    virtual void Init(void);
    virtual int  Run(void);
    virtual void Exit(void);

private:
    void SelectApis(void);
    void SelectMappers(void);
    void SelectServers(void);
    void SelectSetups(void);
    void SelectSkips(void);
    void SelectTests(void);

    int  SanityCheck(void);

    void ReportResults(size_t num_possible, size_t num_skipped, size_t num_selected, size_t num_notrun, size_t num_run, size_t num_passed, size_t num_failed);

    void TestCaseLine(STest&        test,
                      const string& header,
                      const string& footer,
                      const string& prefix,
                      const string& sep,
                      bool          show_success,
                      bool          success);
    void TestCaseStart(STest& test);
    void TestCaseEnd(STest& test, bool success);
    bool Test(STest& test);

    // Application data
    bool    m_ProcessAll;
    string  m_Report;
    string  m_SelectedService;
};


int CTestNcbiDblbSvcResApp::SanityCheck(void)
{
    int num_failed = 0;

    // Verify BK server gets the appropriate error message back.
    {{
        string          exp_host(s_BrokenHost->Get());
        string          exp_msg (s_BrokenMessage->Get());
        unsigned short  exp_port(s_BrokenPort->Get());

        STimeout timeout = {1, 0};
        CSocket sock(exp_host, exp_port, &timeout, fSOCK_LogOn);
        string  got_msg;
        if (sock.ReadLine(got_msg) == eIO_Success) {
            if (got_msg == exp_msg) {
                ERR_POST(Info <<
                         "Sanity test passed for broken server (" <<
                         exp_host << ":" << NStr::IntToString(exp_port) << ").");
            } else {
                ++num_failed;
                ERR_POST(Critical <<
                         "Sanity test failed for broken server (" <<
                         exp_host << ":" << NStr::IntToString(exp_port) << ") - got '" <<
                         got_msg << "', but expected '" << exp_msg << "'.");
            }
        } else {
            ++num_failed;
            ERR_POST(Critical <<
                     "Sanity test failed for broken server (" <<
                     exp_host << ":" << NStr::IntToString(exp_port) << ") - unable to read.");
        }
    }}

    // Verify DN server gets the appropriate error message back.
    {{
        string          exp_host(s_DownHost->Get());
        string          exp_msg (s_DownMessage->Get());
        unsigned short  exp_port(s_DownPort->Get());

        STimeout timeout = {1, 0};
        CSocket sock(s_DownHost->Get(), exp_port, &timeout, fSOCK_LogOn);
        string  got_msg;
        if (sock.ReadLine(got_msg) == eIO_Success) {
            if (got_msg == exp_msg) {
                ERR_POST(Info <<
                         "Sanity test passed for down server (" <<
                         exp_host << ":" << NStr::IntToString(exp_port) << ").");
            } else {
                ++num_failed;
                ERR_POST(Critical <<
                         "Sanity test failed for down server (" <<
                         exp_host << ":" << NStr::IntToString(exp_port) << ") - got '" <<
                         got_msg << "', but expected '" << exp_msg << "'.");
            }
        } else {
            ++num_failed;
            ERR_POST(Critical <<
                     "Sanity test failed for down server (" <<
                     exp_host << ":" << NStr::IntToString(exp_port) << ") - unable to read.");
        }
    }}

    // Verify NE server can't be connected to at all.
    {{
        string          exp_host(s_NonexistHost->Get());
        unsigned short  exp_port(s_NonexistPort->Get());

        STimeout timeout = {1, 0};
        CSocket sock(exp_host, exp_port, &timeout, fSOCK_LogOn);
        if (sock.Wait(eIO_Write, &timeout) == eIO_Success) {
            ++num_failed;
            ERR_POST(Critical <<
                     "Sanity failure - able to connect to non-existent server (" <<
                     exp_host << ":" << NStr::IntToString(exp_port) << ").");
        } else {
            ERR_POST(Info <<
                     "Sanity test passed for non-existent server (" <<
                     exp_host << ":" << NStr::IntToString(exp_port) << ").");
        }
    }}

    // Verify UP server can be connected to (using the host:port from a known UP server).
    // TODO: In the future, possibly make sure it communicates properly via TDS protocol,
    // but by sending/receiving TDS protocol via socket, not via DB software layers?
    {{
        string          exp_host(s_UpHost->Get());
        unsigned short  exp_port(s_UpPort->Get());

        STimeout timeout = {1, 0};
        CSocket sock(exp_host, exp_port, &timeout, fSOCK_LogOn);
        if (sock.Wait(eIO_Write, &timeout) == eIO_Success) {
            ERR_POST(Info <<
                     "Sanity test passed for up server (" <<
                     exp_host << ":" << NStr::IntToString(exp_port) << ").");
        } else {
            ++num_failed;
            ERR_POST(Critical <<
                     "Sanity test failed for up server (" <<
                     exp_host << ":" << NStr::IntToString(exp_port) << ") - unable to connect.");
        }
    }}

    return num_failed;
}


void CTestNcbiDblbSvcResApp::SelectApis(void)
{
    if (GetArgs()["api"].HasValue()) {
        vector<string> api_strs;
        NStr::Split(GetArgs()["api"].AsString(), ",", api_strs);
        for (const string& api_user : api_strs) {
            bool found = false;
            for (size_t api_id = 0; api_id < s_NumApis; ++api_id) {
                if (NStr::EqualNocase(s_Apis[api_id].m_Name, api_user)) {
                    s_SelectedApiIds.push_back(api_id);
                    found = true;
                    break;
                }
            }
            if ( ! found) {
                NCBI_USER_THROW(string("Invalid API name '") + api_user + "'.");
            }
        }
    } else {
        for (size_t api_id = 0; api_id < s_NumApis; ++api_id) {
            s_SelectedApiIds.push_back(api_id);
        }
    }
}


void CTestNcbiDblbSvcResApp::SelectMappers(void)
{
    CMapper::Init(s_Mappers);

    if (GetArgs()["mapper"].HasValue()) {
        vector<string> mapper_strs;
        NStr::Split(GetArgs()["mapper"].AsString(), ",", mapper_strs);
        if (mapper_strs.size() != 1) {
            // TODO: find out why DBLB seems to latch the service mapper the first time
            // the CONN_ evn variables are checked - at least that seems to be
            // the explanation for the first mapper always being used despite
            // changing it for subsequent test cases.  This prevents supporting
            // looping through multiple mappers.
            NCBI_USER_THROW("Currently only one service mapper is supported.");
        }
        for (const string& mapper_user : mapper_strs) {
            bool found = false;
            for (size_t mapper_id = 0; mapper_id < s_Mappers.size(); ++mapper_id) {
                if (NStr::EqualNocase(s_Mappers[mapper_id].m_Name, mapper_user)) {
                    s_SelectedMapperIds.push_back(mapper_id);
                    found = true;
                    break;
                }
            }
            if ( ! found) {
                NCBI_USER_THROW(string("Invalid mapper name '") + mapper_user + "'.");
            }
        }
    } else {
        NCBI_USER_THROW("Currently exactly one service mapper must be supplied.");
        for (size_t mapper_id = 0; mapper_id < s_Mappers.size(); ++mapper_id) {
            s_SelectedMapperIds.push_back(mapper_id);
        }
    }
}


void CTestNcbiDblbSvcResApp::SelectServers(void)
{
    if (GetArgs()["srv1"].HasValue()) {
        vector<string> srv1_strs;
        NStr::Split(GetArgs()["srv1"].AsString(), ",", srv1_strs);
        for (const string& srv1_name : srv1_strs) {
            bool found = false;
            for (size_t srv1_id = 0; srv1_id < s_NumServers; ++srv1_id) {
                if (NStr::EqualNocase(s_Servers[srv1_id], srv1_name)) {
                    s_SelectedServer1Ids.push_back(srv1_id);
                    found = true;
                    break;
                }
            }
            if ( ! found) {
                NCBI_USER_THROW(string("Invalid srv1 name '") + srv1_name + "'.");
            }
        }
    } else {
        for (size_t srv1_id = 0; srv1_id < s_NumServers; ++srv1_id) {
            s_SelectedServer1Ids.push_back(srv1_id);
        }
    }
    if (GetArgs()["srv2"].HasValue()) {
        vector<string> srv2_strs;
        NStr::Split(GetArgs()["srv2"].AsString(), ",", srv2_strs);
        for (const string& srv2_name : srv2_strs) {
            bool found = false;
            for (size_t srv2_id = 0; srv2_id < s_NumServers; ++srv2_id) {
                if (NStr::EqualNocase(s_Servers[srv2_id], srv2_name)) {
                    s_SelectedServer2Ids.push_back(srv2_id);
                    found = true;
                    break;
                }
            }
            if ( ! found) {
                NCBI_USER_THROW(string("Invalid srv2 name '") + srv2_name + "'.");
            }
        }
    } else {
        for (size_t srv2_id = 0; srv2_id < s_NumServers; ++srv2_id) {
            s_SelectedServer2Ids.push_back(srv2_id);
        }
    }
}


void CTestNcbiDblbSvcResApp::SelectSetups(void)
{
    if (GetArgs()["service"].HasValue()) {
        vector<string> setup_strs;
        NStr::Split(GetArgs()["service"].AsString(), ",", setup_strs);
        for (const string& setup_name : setup_strs) {
            bool found = false;
            for (size_t setup_id = 0; setup_id < s_NumSetups; ++setup_id) {
                if (NStr::EqualNocase(s_Setups[setup_id].m_Service, setup_name)) {
                    s_SelectedSetupIds.push_back(setup_id);
                    found = true;
                    break;
                }
            }
            if ( ! found) {
                NCBI_USER_THROW(string("Invalid service name '") + setup_name + "'.");
            }
        }
    } else {
        for (size_t setup_id = 0; setup_id < s_NumSetups; ++setup_id) {
            s_SelectedSetupIds.push_back(setup_id);
        }
    }
}


void CTestNcbiDblbSvcResApp::SelectSkips(void)
{
    if (GetArgs()["skip"].HasValue()) {
        vector<string> skip_strs;
        NStr::Split(GetArgs()["skip"].AsString(), ",", skip_strs);
        for (const string& skip_id : skip_strs) {
            s_SkippedTestIds.push_back(NStr::StringToSizet(skip_id));
        }
    }
}


void CTestNcbiDblbSvcResApp::SelectTests(void)
{
    if (GetArgs()["test"].HasValue()) {
        vector<string> test_strs;
        NStr::Split(GetArgs()["test"].AsString(), ",", test_strs);
        for (const string& test_id : test_strs) {
            s_SelectedTestIds.push_back(NStr::StringToSizet(test_id));
        }
    }
}


void CTestNcbiDblbSvcResApp::TestCaseLine(
    STest&        test,
    const string& header,
    const string& footer,
    const string& prefix,
    const string& sep,
    bool          show_success,
    bool          success)
{
    const SSetup& setup(s_Setups[test.m_SetupId]);
    string msg("\n");
    msg += header + prefix;
    msg += sep + "TestId=" + NStr::SizetToString(test.m_Id);
    msg += sep + "Mapper=" + s_Mappers[s_SelectedMapperIds[0]].m_Name;
    msg += sep + "API=" + s_Apis[test.m_ApiId].m_Name;
    msg += sep + "Service=" + setup.m_Service;
    msg += sep + "Server1=" + s_Servers[setup.m_Server1];
    msg += sep + "Server2=" + s_Servers[setup.m_Server2];
    msg += sep + "ResLbNm=" + s_ResLbNms[setup.m_ResLbNm];
    msg += sep + "ResRev=" + s_ResRevs[setup.m_ResRev];
    msg += sep + "ResIdns=" + s_ResIdnss[setup.m_ResIdns];
    msg += sep + "SetupExpected=" + s_Results[setup.m_ResultExpectedId];
    msg += sep + "TestExpected=" + s_Results[test.m_ResultExpectedId];
    if (show_success) {
        msg += sep + "Actual=" + s_Results[test.m_ResultActualId];
        msg += sep + "Result=" + (success ? "PASS" : "FAIL");
    }
    msg += footer;
    ERR_POST(Info << msg);
}


void CTestNcbiDblbSvcResApp::TestCaseStart(STest& test)
{
    TestCaseLine(test, string(80, '=') + "\n", "",
                 "TestCaseStart", "\t", false, false);
}


// Result records can easily be transformed into a CSV.  For example:
//      ./dbapi_svc_test | grep -P '^TestCaseEnd\t' | tr '\t' ,
void CTestNcbiDblbSvcResApp::TestCaseEnd(STest& test, bool success)
{
    TestCaseLine(test, "", string("\n") + string(80, '-'),
                 "TestCaseEnd", "\t", true, success);
}


bool CTestNcbiDblbSvcResApp::Test(STest& test)
{
    TestCaseStart(test);
    bool success = false;
    CMapper::Select(test.m_MapperId);
    s_Apis[test.m_ApiId].Check(test);
    if (test.m_ResultActualId == test.m_ResultExpectedId) {
        success = true;
    }
    TestCaseEnd(test, success);
    return (success);
}


void CTestNcbiDblbSvcResApp::Init(void)
{
    unique_ptr<CArgDescriptions> args(new CArgDescriptions);
    args->SetUsageContext(GetArguments().GetProgramBasename(),
                          "Test service resolution via DBAPI, SDBAPI, and DBLB.");

    args->AddFlag("process_all", "Process all selected tests regardless of failures");

    args->AddDefaultKey("report", "report", "Desired level of results reporting from least to most (one of 'fail'*, 'run', 'selected', 'skip', 'all')",
                        CArgDescriptions::eString, "fail");
    args->SetConstraint("report", 
                        &(*new CArgAllow_Strings,
                          "fail", "run", "selected", "skip", "all",
                          "FAIL", "RUN", "SELECTED", "SKIP", "ALL"),
                        CArgDescriptions::eConstraint);

    args->AddOptionalKey("api", "api", "Comma-separated list of APIs (from dbapi_ftds,dbapi_ctlib,sdbapi,dblb)",
                         CArgDescriptions::eString);

    args->AddKey("mapper", "mapper", "The service mapper to use (from dispd,lbsmd,linkerd,local,namerd)",
                 CArgDescriptions::eString);

    args->AddOptionalKey("srv1", "srv1", "Comma-separated list of first server states (from none,bk,dn,ne,up)",
                         CArgDescriptions::eString);
    args->SetConstraint("srv1", 
                        &(*new CArgAllow_Strings,
                          "none", "bk", "dn", "ne", "up",
                          "NONE", "BK", "DN", "NE", "UP"),
                        CArgDescriptions::eConstraint);

    args->AddOptionalKey("srv2", "srv2", "Comma-separated list of second server states (from none,bk,dn,ne,up)",
                         CArgDescriptions::eString);
    args->SetConstraint("srv2", 
                        &(*new CArgAllow_Strings,
                          "none", "bk", "dn", "ne", "up",
                          "NONE", "BK", "DN", "NE", "UP"),
                        CArgDescriptions::eConstraint);

    args->AddOptionalKey("service", "service", "Comma-separated list of services (e.g. DT_LB_NOI_BK,DT_LB_NOI_NE)",
                         CArgDescriptions::eString);

    args->AddOptionalKey("skip", "skip", "Comma-separated list of skipped test IDs (e.g. 0,4,5,6)",
                         CArgDescriptions::eString);

    args->AddOptionalKey("test", "test", "Comma-separated list of test IDs (e.g. 0,4,5,6)",
                         CArgDescriptions::eString);

    SetupArgDescriptions(args.release());

    m_ProcessAll = GetArgs()["process_all"];
    m_Report = GetArgs()["report"].AsString();

    SelectApis();
    SelectMappers();
    SelectServers();
    SelectSetups();
    SelectSkips();
    SelectTests();

    // Verify integrity of s_Setups.
    _ASSERT(sizeof(s_Setups) / sizeof(s_Setups[0]) == s_NumSetups);
    for (size_t setup_id = 0; setup_id < s_NumSetups; ++setup_id) {
        _ASSERT(s_Setups[setup_id].m_Id == setup_id);
    }

    // Name the log file after the service, if a single service is selected.
    if (GetArgs()["service"].HasValue()  &&  s_SelectedSetupIds.size() == 1) {
        s_Dbapi_LogFileName = GetArgs()["service"].AsString();
    } else {
        s_Dbapi_LogFileName = GetAppName() + ".log";
    }
}


int CTestNcbiDblbSvcResApp::Run(void)
{
    ERR_POST(Info << GetArguments().GetProgramBasename() << ":   $Id$");

    // Run sanity check.  Any sanity check failure should terminate the test
    // unless the user has requested a specific service.
    int sanity_fails = SanityCheck();
    if (s_SelectedSetupIds.size() == 0  &&  sanity_fails != 0) {
        ERR_POST(Critical << "Sanity check failed, ending test.");
        return -1;
    }

    size_t test_id = 0, num_skipped = 0, num_selected = 0, num_notrun = 0, num_run = 0, num_passed = 0, num_failed = 0;
    for (size_t api_id = 0; api_id < s_NumApis; ++api_id) {
        for (size_t mapper_id : s_SelectedMapperIds) {
            for (size_t setup_id = 0; setup_id < s_NumSetups; ++setup_id) {

                STest test = {test_id, api_id, setup_id, mapper_id, eResult_NONE, eResult_NONE, NULL};

                // This test result should match the setup expected result, except in some cases.
                test.m_ResultExpectedId = s_Setups[setup_id].m_ResultExpectedId;

                // Expect test cases to fail where not LBR and RESET_SYBASE env var not set,
                // because FreeTDS won't try the interfaces file unless RESET_SYBASE is true.
                // TODO: NEED A BETTER WAY TO TWEAK EXPECTED RESULT BASED ON CUSTOM SYBASE.
                if (s_Setups[test.m_SetupId].m_ResLbNm == eResLbNm_NOLB  &&  getenv("RESET_SYBASE") == NULL) {
                    test.m_ResultExpectedId = eResult_ERROR;
                    test.m_Result = "not LBR and RESET_SYBASE env var not set";
                }

                // Skip test cases deselected by command-line options.
                bool deselected = false;
                deselected = deselected  ||  (find(s_SelectedApiIds.begin(), s_SelectedApiIds.end(), api_id) == s_SelectedApiIds.end());
                deselected = deselected  ||  (find(s_SelectedTestIds.begin(), s_SelectedTestIds.end(), test_id) == s_SelectedTestIds.end()  &&  s_SelectedTestIds.size() > 0);
                deselected = deselected  ||  (find(s_SelectedSetupIds.begin(), s_SelectedSetupIds.end(), setup_id) == s_SelectedSetupIds.end());
                deselected = deselected  ||  (find(s_SelectedServer1Ids.begin(), s_SelectedServer1Ids.end(), s_Setups[setup_id].m_Server1) == s_SelectedServer1Ids.end());
                deselected = deselected  ||  (find(s_SelectedServer2Ids.begin(), s_SelectedServer2Ids.end(), s_Setups[setup_id].m_Server2) == s_SelectedServer2Ids.end());
                deselected = deselected  ||   find(s_SkippedTestIds.begin(), s_SkippedTestIds.end(), test_id) != s_SkippedTestIds.end();
                if (deselected) {
                    test.m_ResultActualId = eResult_SKIP_USER;
                    test.m_Result = "user de-selected";
                    ++num_skipped;
                } else {
                    // Skip test cases using DBAPI/CTLIB to connect to a non-Sybase service.
                    if (api_id == eApi_DbapiCtlib  &&  ! s_Setups[setup_id].m_Sybase) {
                        test.m_ResultActualId = eResult_SKIP_CTLIB;
                        test.m_Result = "DBAPI/CTLIB to connect to a non-Sybase service";
                        ++num_skipped;
                    }
                    // Skip test cases using DBAPI/CTLIB and interfaces file but not LBSM.
                    // The problem with these cases is that they need the SYBASE env var to point
                    // to our custom interfaces file, but for CTLIB that var also points to where
                    // the binaries are.  Since we can't tweak the installation interfaces file
                    // or place the binaries in our test directory we can't customize this case.
                    // So we skip it.  It's an exceedingly unlikely scenario anyway.
                    // Cases using LBSM are not skipped because they should be resolved by
                    // LBSM and therefore should be unaffected by the interfaces file.
                    // Command-line pattern: -api DBAPI_CTLIB -service DT_I_*
                    else if (api_id == eApi_DbapiCtlib  &&
                             s_Setups[setup_id].m_ResLbNm == eResLbNm_NOLB  &&
                             s_Setups[setup_id].m_ResIdns == eResIdns_I)
                    {
                        test.m_ResultActualId = eResult_SKIP_CTLIB;
                        test.m_Result = "using DBAPI/CTLIB and interfaces file but not LBSM";
                        ++num_skipped;
                    }
                    // Skip test cases where UP and LB but not LBR.  This is because if service is UP,
                    // then it talks to mssql, and that is managed by dbhelp, and dbhelp always has
                    // reverse dns setup, so LB but not LBR for UP is invalid in practice.
                    else if (s_Setups[setup_id].m_ResLbNm == eResLbNm_LB  &&
                             s_Setups[setup_id].m_ResRev == eResRev_NOREV  &&
                             (s_Setups[setup_id].m_Server1 == eServer_UP  ||
                              s_Setups[setup_id].m_Server2 == eServer_UP))
                    {
                        test.m_Result = "UP and LB but not LBR";
                        test.m_ResultActualId = eResult_SKIP_INTF;
                        ++num_skipped;
                    }
                    // Skip 2-server test cases using interfaces file but not reverse DNS.
                    // The problem in these cases is that FreeTDS has a known bug wherein it
                    // only checks the last entry in the interfaces file, so there's no way to
                    // try a second server if the first one fails; thus 2-server setups can't work.
                    // Cases using reverse DNS are not skipped because they should be resolved by
                    // LBSM and therefore should be unaffected by the interfaces file.
                     // See https://jira.ncbi.nlm.nih.gov/browse/CXX-4186
                    // Command-line pattern: -service DT(_LB)?_I_*_UP
                    else if (s_Setups[setup_id].m_ResRev == eResRev_NOREV  &&
                             s_Setups[setup_id].m_ResIdns == eResIdns_I  &&
                             s_Setups[setup_id].m_Server2 != eServer_NONE)
                    {
                        test.m_ResultActualId = eResult_SKIP_INTF;
                        test.m_Result = "2-server test cases using interfaces file but not reverse DNS";
                        ++num_skipped;
                    } else {
                        // This test case is selected, even if skipped due to prior failure.
                        ++num_selected;

                        // Skip remaining test cases if any have failed and -all option not used.
                        if (num_failed > 0  &&  ! m_ProcessAll) {
                            test.m_ResultActualId = eResult_SKIP_FAIL;
                            test.m_Result = "prior fails";
                            ++num_notrun;
                        } else {
                            // Ok, let's do the test!
                            bool success = Test(test);  // side-effect: records test result
                            ++num_run;
                            if (success) {
                                ++num_passed;
                            } else {
                                ++num_failed;
                            }
                        }
                    }
                }
                s_Tests.push_back(test);
                ++test_id;
            }
        }
    }

    // Quit last API.
    s_Apis[s_ApiInitedId].Quit();

    // Report results.
    size_t num_possible = s_Tests.size();
    ReportResults(num_possible, num_skipped, num_selected, num_notrun, num_run, num_passed, num_failed);

    // Make sure all the counts make sense.
    if (num_skipped + num_selected != num_possible) {
        ERR_POST(Critical << "num_skipped (" << num_skipped << ") + num_selected (" << num_selected << ") != num_possible (" << num_possible << ")");
    }
    if (num_notrun + num_run != num_selected) {
        ERR_POST(Critical << "num_notrun (" << num_notrun << ") + num_run (" << num_run << ") != num_selected (" << num_selected << ")");
    }
    if (num_passed + num_failed != num_run) {
        ERR_POST(Critical << "num_passed (" << num_passed << ") + num_failed (" << num_failed << ") != num_run (" << num_run << ")");
    }

    return num_run > 0 ? (num_failed > 0 ? num_failed : sanity_fails) : -1;
}


void CTestNcbiDblbSvcResApp::ReportResults(size_t num_possible, size_t num_skipped, size_t num_selected, size_t num_notrun, size_t num_run, size_t num_passed, size_t num_failed)
{
    ERR_POST(Info << "\nPossible: " << num_possible
                  << "\nSkipped:    " << num_skipped
                  << "\nSelected:   " << num_selected
                  << "\nNot run:      " << num_notrun
                  << "\nRun:          " << num_run
                  << "\nPassed:         " << num_passed
                  << "\nFailed:         " << num_failed);

    string results;
    for (const STest& test : s_Tests) {
        const SSetup& setup(s_Setups[test.m_SetupId]);
        string status;

        if (NStr::StartsWith(s_Results[test.m_ResultActualId], "SKIP")  ||
            NStr::StartsWith(s_Results[test.m_ResultExpectedId], "SKIP"))
        {
            status = "skip";
        } else if (test.m_ResultActualId == test.m_ResultExpectedId) {
            status = "pass";
        } else if (test.m_ResultExpectedId != eResult_NONE) {
            status = "FAIL";
        } else {
            status = "none";
        }
        if (status == "FAIL"  ||
            (m_Report == "run"  &&  status == "pass")  ||
            (m_Report == "selected"  &&
                (status == "FAIL"  ||
                 status == "pass"  ||
                (status == "skip"  &&  test.m_ResultActualId == eResult_SKIP_FAIL)))  ||
            (m_Report == "skip"  &&  status != "none")  ||
            (m_Report == "all"))
        {
            string sep("    ");
            results += "\n";
            results += sep + status;
            results += sep + "-test " + NStr::SizetToString(test.m_Id);
            results += sep + "-api " + s_Apis[test.m_ApiId].m_Name;
            results += sep + "-mapper " + s_Mappers[s_SelectedMapperIds[0]].m_Name;
            results += sep + "-service " + setup.m_Service;
            results += sep + "-srv1 " + s_Servers[setup.m_Server1];
            results += sep + "-srv2 " + s_Servers[setup.m_Server2];
            results += sep + "ResLbNm=" + s_ResLbNms[setup.m_ResLbNm];
            results += sep + "ResRev=" + s_ResRevs[setup.m_ResRev];
            results += sep + "ResIdns=" + s_ResIdnss[setup.m_ResIdns];
            results += sep + "SetupExpected=" + s_Results[setup.m_ResultExpectedId];
            results += sep + "TestExpected=" + s_Results[test.m_ResultExpectedId];
            results += sep + "Actual=" + s_Results[test.m_ResultActualId];
            if (test.m_Result) {
                results += sep + "Reason=" + test.m_Result;
            }
        }
    }
    if (results.empty()) {
        if (num_selected == 0) {
            ERR_POST(Info << "\nNo selected tests!");
        } else if (num_run == 0) {
            ERR_POST(Info << "\nNo run tests!");
        } else if (m_Report == "fail") {
            ERR_POST(Info << "\nNo failed tests!");
        } else if (m_Report == "run") {
            ERR_POST(Info << "\nNo run tests!");
        } else if (m_Report == "selected") {
            ERR_POST(Info << "\nNo selected tests!");
        } else if (m_Report == "skip") {
            ERR_POST(Info << "\nNo skipped or failed tests!");
        } else {
            ERR_POST(Info << "\nNo tests!");
        }
    } else {
        ERR_POST(Info << "\nTest results:" + results);
    }
}


void CTestNcbiDblbSvcResApp::Exit(void)
{
    SetDiagStream(0);
}


int main(int argc, char* argv[])
{
    int exit_code = 1;

    SetDiagTrace(eDT_Enable);
    SetDiagPostLevel(eDiag_Info);
    SetDiagPostAllFlags((SetDiagPostAllFlags(eDPF_Default) & ~eDPF_All)
                        | eDPF_Severity | eDPF_ErrorID | eDPF_Prefix | eDPF_File | eDPF_Line);
    SetDiagTraceAllFlags(SetDiagPostAllFlags(eDPF_Default) | eDPF_File | eDPF_Line);

    try {
        exit_code = CTestNcbiDblbSvcResApp().AppMain(argc, argv);
    } catch (CException& exc) {
        // ERR_POST may not work
        NcbiCerr << "main() caught exception '" << exc.what() << "'" << NcbiEndl;
    } catch (...) {
        NcbiCerr << "main() caught unknown exception." << NcbiEndl;
    }

    return exit_code;
}
