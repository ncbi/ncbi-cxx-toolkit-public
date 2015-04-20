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
* Author:  Aaron Ucko
*
* File Description:
*   Database-centric load-balancer client (useful for scripts)
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <corelib/ncbiapp.hpp>
#include <util/static_map.hpp>
#include <util/xregexp/arg_regexp.hpp>
#include <connect/ncbi_service.h>
#include <connect/ncbi_socket.hpp>

#include <dbapi/driver/dbapi_conn_factory.hpp>
#include <dbapi/driver/dbapi_driver_conn_params.hpp>
#include <dbapi/simple/sdbapi.hpp>

#ifdef HAVE_LIBCONNEXT
#  include <connect/ext/ncbi_dblb_svcmapper.hpp>
#endif

BEGIN_NCBI_SCOPE

struct SCNIDeleter
{
    static void Delete(SConnNetInfo* info) { ConnNetInfo_Destroy(info); }
};

struct SServIterDeleter
{
    static void Delete(SERV_ITER iter) { SERV_Close(iter); }
};

inline
CNcbiOstream& operator<<(CNcbiOstream& os, CDBServer& server)
{
    os << server.GetName() << '\t' << CSocketAPI::ntoa(server.GetHost())
       << '\t' << server.GetPort();
    return os;
}

inline
static bool s_IsDBPort(unsigned short port)
{
    // MS SQL consistently uses port 1433, but Sybase is another matter.
    return (port == 1433  ||  (port >= 2131  &&  port <= 2198));
}

class CDBLBClientApp : public CNcbiApplication
{
public:
    void Init(void);
    int  Run(void);

private:
    void x_InitLookup (CArgDescriptions& arg_desc);
    void x_InitWhatIs (CArgDescriptions& arg_desc);
    void x_InitWhereIs(CArgDescriptions& arg_desc);
    int  x_RunLookup  (void);
    int  x_RunWhatIs  (void);
    int  x_RunWhereIs (void);

    IDBServiceMapper* x_GetServiceMapper(void);
};

void CDBLBClientApp::Init(void)
{
    auto_ptr<CCommandArgDescriptions> cmd_desc(new CCommandArgDescriptions);
    cmd_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "Simple database-centric load-balancer client");

    auto_ptr<CArgDescriptions> arg_desc;

    arg_desc.reset(new CArgDescriptions);
    x_InitLookup(*arg_desc);
    cmd_desc->AddCommand("lookup", arg_desc.release());

    arg_desc.reset(new CArgDescriptions);
    x_InitWhatIs(*arg_desc);
    cmd_desc->AddCommand("whatis", arg_desc.release());

    arg_desc.reset(new CArgDescriptions);
    x_InitWhereIs(*arg_desc);
    cmd_desc->AddCommand("whereis", arg_desc.release());

    SetupArgDescriptions(cmd_desc.release());
}

int CDBLBClientApp::Run(void)
{
    typedef int (CDBLBClientApp::* TRunner)(void);
    typedef SStaticPair<const char*, TRunner> TRunnerPair;
    static const TRunnerPair kRunners[] = {
        { "lookup",  &CDBLBClientApp::x_RunLookup  },
        { "whatis",  &CDBLBClientApp::x_RunWhatIs  },
        { "whereis", &CDBLBClientApp::x_RunWhereIs }
    };
    typedef CStaticArrayMap<const char*, TRunner, PNocase_CStr> TRunnerMap;
    DEFINE_STATIC_ARRAY_MAP(TRunnerMap, sc_Runners, kRunners);

    TRunnerMap::const_iterator it
        = sc_Runners.find(GetArgs().GetCommand().c_str());
    _ASSERT(it != sc_Runners.end());
    return (this->*it->second)();
}

void CDBLBClientApp::x_InitLookup(CArgDescriptions& arg_desc)
{
    arg_desc.SetUsageContext("lookup", "Look up a database server");

    arg_desc.AddKey
        ("service", "Service",
         "Desired service name; use -url to supply a full DBAPI URL.",
         CArgDescriptions::eString);
    arg_desc.AddDefaultKey("username", "Name",
                           "Username for confirming usability",
                           CArgDescriptions::eString, "anyone");
    arg_desc.SetDependency("username", CArgDescriptions::eRequires, "service");
    arg_desc.AddDefaultKey("password", "Password",
                           "Password corresponding to username",
                           CArgDescriptions::eString, "allowed");
    arg_desc.SetDependency("password", CArgDescriptions::eRequires, "service");
    arg_desc.AddOptionalKey("database", "Name", "Database name to try using",
                            CArgDescriptions::eString);
    arg_desc.SetDependency("database", CArgDescriptions::eRequires, "service");
    arg_desc.AddOptionalKey("url", "URL",
                            "DBAPI URL consolidating all of the above",
                            CArgDescriptions::eString);
    arg_desc.SetConstraint("url", new CArgAllow_Regexp("dbapi://.*"));
    arg_desc.SetDependency("url", CArgDescriptions::eExcludes, "service");
    arg_desc.AddFlag("kerberos",
                     "Authenticate via existing Kerberos credentials");
    arg_desc.SetDependency("kerberos", CArgDescriptions::eExcludes,
                           "username");
    arg_desc.SetDependency("kerberos", CArgDescriptions::eExcludes,
                           "password");
#if 0 // not yet implemented
    arg_desc.AddDefaultKey("limit", "N", "Maximum number of servers to return",
                           CArgDescriptions::eInteger, "1");
    arg_desc.SetConstraint("limit", new CArgAllow_Integers(1, kMax_Int));
#endif
}

int CDBLBClientApp::x_RunLookup(void)
{
    const CArgs& args = GetArgs();
    bool use_kerberos = args["kerberos"].HasValue();
    if (use_kerberos) {
        TDbapi_CanUseKerberos::SetDefault(true);
    }

    CSDB_ConnectionParam param;
    if (args["url"].HasValue()) {
        param.Set(args["url"].AsString());
    } else {
        param.Set(args["service"].AsString());
        if ( !use_kerberos  &&  args["username"].HasValue()) {
            param.Set(CSDB_ConnectionParam::eUsername,
                      args["username"].AsString());
        }
        if ( !use_kerberos  &&  args["password"].HasValue()) {
            param.Set(CSDB_ConnectionParam::ePassword,
                      args["password"].AsString());
        }
        if (args["database"].HasValue()) {
            param.Set(CSDB_ConnectionParam::eDatabase,
                      args["database"].AsString());
        }
    }

    CDatabase db(param);
    CQuery query;
    {{
        CDiagCollectGuard diag_guard(eDiag_Fatal, eDiag_Info,
                                     CDiagCollectGuard::ePrint);
        db.Connect();
        query = db.NewQuery();
        diag_guard.SetAction(CDiagCollectGuard::eDiscard);
    }}
    // Hack to obtain the actual server name
    try {
        query.Execute();
    } catch (CSDB_Exception& e) {
        const string& name = e.GetServerName();
        TSvrRef server,
                pref(new CDBServer(name, CSocketAPI::gethostbyname(name)));
        IDBServiceMapper* mapper = x_GetServiceMapper();
        if (mapper != NULL) {
            // server.Reset(mapper->GetServer(e.GetServerName()));
            const string& service = param.Get(CSDB_ConnectionParam::eService);
            mapper->SetPreference(service, pref);
            server.Reset(mapper->GetServer(service));
        }
        if (server.Empty()  ||  server->GetName().empty()) {
            server.Reset(pref);
        }
        cout << *server << '\n';
    }
    
    return 0;
}

void CDBLBClientApp::x_InitWhatIs(CArgDescriptions& arg_desc)
{
    arg_desc.SetUsageContext("whatis", "Identify a name");
    arg_desc.AddPositional("name", "Name to identify",
                           CArgDescriptions::eString);
}

int CDBLBClientApp::x_RunWhatIs(void)
{
    const CArgs&  args = GetArgs();
    const string& name = args["name"].AsString();

    AutoPtr<SConnNetInfo, SCNIDeleter> net_info
        (ConnNetInfo_Create(name.c_str()));
    AutoPtr<SSERV_IterTag, SServIterDeleter> iter
        (SERV_Open(name.c_str(), fSERV_Any, SERV_ANYHOST, net_info.get()));
    TSERV_Type types_seen = 0;
    for (;;) {
        SSERV_InfoCPtr serv_info = SERV_GetNextInfo(iter.get());
        if (serv_info == NULL) {
            break;
        } else if ((types_seen & serv_info->type) != 0) {
            continue;
        } else {
            types_seen |= serv_info->type;
        }
        
#ifdef HAVE_LIBCONNEXT
        if (serv_info->type == fSERV_Standalone) {
            IDBServiceMapper* mapper = x_GetServiceMapper();
            _ASSERT(mapper != NULL);
            TSvrRef ref = mapper->GetServer(name);
            if (ref.NotEmpty()  &&  ref->GetHost() != 0
                &&  !ref->GetName().empty()
                /* &&  s_IsDBPort(ref->GetPort()) */ ) {
                NcbiCout << name << " is a database service.\n";
                continue;
            }
        }
#endif
        NcbiCout << name << " is a service (type "
                 << SERV_TypeStr(serv_info->type) << ").\n";
    }

    iter.reset(SERV_Open(name.c_str(), fSERV_Dns, SERV_ANYHOST,
                         net_info.get()));
    SSERV_InfoCPtr serv_info = SERV_GetNextInfo(iter.get());
    if (serv_info != NULL) {
        types_seen |= serv_info->type;
        if (s_IsDBPort(serv_info->port)) {
            string hostname = CSocketAPI::gethostbyaddr(serv_info->host);
            if (hostname.empty()) {
                hostname = CSocketAPI::ntoa(serv_info->host);
            }
            NcbiCout << name << " is a registered database server alias on "
                     << hostname << ".\n";
        } else {
            NcbiCout << name << " is a \"DNS\" service record.\n";
        }
    }

    unsigned int ip = CSocketAPI::gethostbyname(name);
    if (ip != 0) {
        NcbiCout << name << " is a host (" << CSocketAPI::ntoa(ip) << ").\n";
    } else if (types_seen == 0) {
        NcbiCout << name << " is unknown to LBSM or DNS.\n";
        return 1;
    }

    return 0;
}

void CDBLBClientApp::x_InitWhereIs(CArgDescriptions& arg_desc)
{
    arg_desc.SetUsageContext("whereis",
                             "List all instances of a load-balanced service");

    arg_desc.AddDefaultKey
        ("type", "Type", "Service type (case-insensitive)",
         CArgDescriptions::eString, "DBLB");
    auto_ptr<CArgAllow_Strings> type_strings
        (&(*new CArgAllow_Strings(NStr::eNocase), "ANY", "DBLB", "HTTP"));
    for (TSERV_Type t = 1;  t <= fSERV_All;  t <<= 1) {
        const char* name = SERV_TypeStr(static_cast<ESERV_Type>(t));
        if (name == NULL  ||  name[0] == '\0') {
            break;
        } else {
            type_strings->Allow(name);
        }
    }
    arg_desc.SetConstraint("type", type_strings.release());
    
    arg_desc.AddPositional("service", "Service name",
                           CArgDescriptions::eString);
}

int CDBLBClientApp::x_RunWhereIs(void)
{
    const CArgs&  args     = GetArgs();
    const string& service  = args["service"].AsString();
    const string& type_str = args["type"   ].AsString();
    TSERV_Type    type     = fSERV_Any;
    int           status   = 1;

    if (NStr::EqualNocase(type_str, "dblb")) {
#ifndef HAVE_LIBCONNEXT
        type = fSERV_Standalone;
#else
        IDBServiceMapper* mapper = x_GetServiceMapper();
        _ASSERT(mapper != NULL);
        for (;;) {
            TSvrRef ref = mapper->GetServer(service);
            if (ref.Empty()  ||  ref->GetHost() == 0
                ||  ref->GetName().empty()) {
                break;
            }
            status = 0;
            NcbiCout << *ref << "\t# type=DBLB\n";
            mapper->Exclude(service, ref);
        }
        return status;
#endif
    } else if (SERV_ReadType(type_str.c_str(),
                             reinterpret_cast<ESERV_Type*>(&type)) == NULL) {
        _ASSERT(NStr::EqualNocase(type_str, "any"));
    }

    AutoPtr<SConnNetInfo, SCNIDeleter> net_info
        (ConnNetInfo_Create(service.c_str()));
    // Unavailable instances aren't showing up even with fSERV_Promiscuous,
    // but aren't strictly necessary to mention anyway.
    AutoPtr<SSERV_IterTag, SServIterDeleter> iter
        (SERV_Open(service.c_str(), type | fSERV_Promiscuous, SERV_ANYHOST,
                   net_info.get()));
    for (;;) {
        SSERV_InfoCPtr serv_info = SERV_GetNextInfo(iter.get());
        if (serv_info == NULL) {
            break;
        }
        status = 0;
        CDBServer server(CSocketAPI::gethostbyaddr(serv_info->host),
                         serv_info->host, serv_info->port);
        if (serv_info->rate <= 0.0) {
            NcbiCout << "# ";
        }
        NcbiCout << server << "\t# type=" << SERV_TypeStr(serv_info->type)
                 << '\n';
    }
    return status;
}

IDBServiceMapper* CDBLBClientApp::x_GetServiceMapper(void)
{
#if 0 // CRuntimeData and GetRuntimeData are protected
    return &(dynamic_cast<CDBConnectionFactory&>
             (*CDbapiConnMgr::Instance().GetConnectionFactory())
             .GetRuntimeData(kEmptyStr).GetDBServiceMapper());
#elif defined(HAVE_LIBCONNEXT)
    static CDBLB_ServiceMapper mapper(&GetConfig());
    return &mapper;
#else
    return NULL;
#endif
}

END_NCBI_SCOPE

USING_NCBI_SCOPE;

int main(int argc, const char** argv)
{
    return CDBLBClientApp().AppMain(argc, argv);
}
