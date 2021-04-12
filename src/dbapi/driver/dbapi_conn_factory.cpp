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
* Authors:  Sergey Sikorskiy, Aaron Ucko
*
*/

#include <ncbi_pch.hpp>

#include <dbapi/driver/dbapi_conn_factory.hpp>
#include <dbapi/driver/dbapi_svc_mapper.hpp>
#include <dbapi/driver/dbapi_driver_conn_params.hpp>
#include <dbapi/driver/impl/dbapi_driver_utils.hpp>
#include <dbapi/driver/impl/dbapi_impl_connection.hpp>
#include <dbapi/driver/impl/dbapi_impl_context.hpp>
#include <dbapi/driver/impl/dbapi_pool_balancer.hpp>
#include <dbapi/driver/public.hpp>
#include <dbapi/error_codes.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/request_ctx.hpp>

#include <list>


#define NCBI_USE_ERRCODE_X   Dbapi_ConnFactory

BEGIN_NCBI_SCOPE


///////////////////////////////////////////////////////////////////////////////
CDBConnectionFactory::CDBConnectionFactory(IDBServiceMapper::TFactory svc_mapper_factory,
                                           const IRegistry* registry,
                                           EDefaultMapping def_mapping) :
m_MapperFactory(svc_mapper_factory, registry, def_mapping),
m_MaxNumOfConnAttempts(1),
m_MaxNumOfValidationAttempts(1),
m_MaxNumOfServerAlternatives(32),
m_MaxNumOfDispatches(0),
m_ConnectionTimeout(0),
m_LoginTimeout(0),
m_TryServerToo(false)
{
    ConfigureFromRegistry(registry);
}

void
CDBConnectionFactory::ConfigureFromRegistry(const IRegistry* registry)
{
    const string section_name("DB_CONNECTION_FACTORY");

    // Get current registry ...
    if (!registry && CNcbiApplication::Instance()) {
        registry = &CNcbiApplication::Instance()->GetConfig();
    }

    if (registry) {
        m_MaxNumOfConnAttempts =
            registry->GetInt(section_name, "MAX_CONN_ATTEMPTS", 1);
        m_MaxNumOfValidationAttempts =
            registry->GetInt(section_name, "MAX_VALIDATION_ATTEMPTS", 1);
        m_MaxNumOfServerAlternatives =
            registry->GetInt(section_name, "MAX_SERVER_ALTERNATIVES", 32);
        m_MaxNumOfDispatches =
            registry->GetInt(section_name, "MAX_DISPATCHES", 0);
        m_ConnectionTimeout =
            registry->GetInt(section_name, "CONNECTION_TIMEOUT", 0);
        m_LoginTimeout =
            registry->GetInt(section_name, "LOGIN_TIMEOUT", 0);
        m_TryServerToo =
            registry->GetBool(section_name, "TRY_SERVER_AFTER_SERVICE", false);
    } else {
        m_MaxNumOfConnAttempts = 1;
        m_MaxNumOfValidationAttempts = 1;
        m_MaxNumOfServerAlternatives = 32;
        m_MaxNumOfDispatches = 0;
        m_ConnectionTimeout = 0;
        m_LoginTimeout = 0;
        m_TryServerToo = false;
    }
}

void
CDBConnectionFactory::Configure(const IRegistry* registry)
{
    CWriteLockGuard guard(m_Lock);

    ConfigureFromRegistry(registry);
}

void
CDBConnectionFactory::SetMaxNumOfConnAttempts(unsigned int max_num)
{
    CWriteLockGuard guard(m_Lock);

    m_MaxNumOfConnAttempts = max_num;
}

void
CDBConnectionFactory::SetMaxNumOfValidationAttempts(unsigned int max_num)
{
    CWriteLockGuard guard(m_Lock);

    m_MaxNumOfValidationAttempts = max_num;
}

void
CDBConnectionFactory::SetMaxNumOfServerAlternatives(unsigned int max_num)
{
    CWriteLockGuard guard(m_Lock);

    m_MaxNumOfServerAlternatives = max_num;
}

void
CDBConnectionFactory::SetMaxNumOfDispatches(unsigned int max_num)
{
    CWriteLockGuard guard(m_Lock);

    m_MaxNumOfDispatches = max_num;
}

void
CDBConnectionFactory::SetConnectionTimeout(unsigned int timeout)
{
    CWriteLockGuard guard(m_Lock);

    m_ConnectionTimeout = timeout;
}

void
CDBConnectionFactory::SetLoginTimeout(unsigned int timeout)
{
    CWriteLockGuard guard(m_Lock);

    m_LoginTimeout = timeout;
}

CDBConnectionFactory::CRuntimeData&
CDBConnectionFactory::GetRuntimeData(const CRef<IConnValidator> validator)
{
    string validator_name;
    if (validator) {
        validator_name = validator->GetName();
    }
    return GetRuntimeData(validator_name);
}

CDBConnectionFactory::CRuntimeData&
CDBConnectionFactory::GetRuntimeData(const string& validator_name)
{
    CFastMutexGuard guard(m_ValidatorSetMutex);
    TValidatorSet::iterator it = m_ValidatorSet.find(validator_name);
    if (it != m_ValidatorSet.end()) {
        return it->second;
    }

    return m_ValidatorSet.insert(TValidatorSet::value_type(
        validator_name,
        CRuntimeData(*this, CRef<IDBServiceMapper>(m_MapperFactory.Make()))
        )).first->second;
}


///////////////////////////////////////////////////////////////////////////////
class CDB_DBLB_Delegate : public CDBConnParamsDelegate
{
public:
    CDB_DBLB_Delegate(
            const string& srv_name,
            Uint4 host,
            Uint2 port,
            const CDBConnParams& other);
    virtual ~CDB_DBLB_Delegate(void);

public:
    virtual string GetServerName(void) const;
    virtual Uint4 GetHost(void) const;
    virtual Uint2  GetPort(void) const;
    virtual const impl::CDBHandlerStack& GetOpeningMsgHandlers(void) const;
    impl::CDBHandlerStack& SetOpeningMsgHandlers(void);

private:
    // Non-copyable.
    CDB_DBLB_Delegate(const CDB_DBLB_Delegate& other);
    CDB_DBLB_Delegate& operator =(const CDB_DBLB_Delegate& other);

private:
    const string m_ServerName;
    const Uint4  m_Host;
    const Uint2  m_Port;
    impl::CDBHandlerStack m_OpeningMsgHandlers;
};


CDB_DBLB_Delegate::CDB_DBLB_Delegate(
        const string& srv_name,
        Uint4 host,
        Uint2 port,
        const CDBConnParams& other)
: CDBConnParamsDelegate(other)
, m_ServerName(srv_name)
, m_Host(host)
, m_Port(port)
{
}

CDB_DBLB_Delegate::~CDB_DBLB_Delegate(void)
{
}


string CDB_DBLB_Delegate::GetServerName(void) const
{
    return m_ServerName;
}


Uint4 CDB_DBLB_Delegate::GetHost(void) const
{
    return m_Host;
}


Uint2 CDB_DBLB_Delegate::GetPort(void) const
{
    return m_Port;
}

const impl::CDBHandlerStack& CDB_DBLB_Delegate::GetOpeningMsgHandlers(void)
    const
{
    return m_OpeningMsgHandlers;
}

impl::CDBHandlerStack& CDB_DBLB_Delegate::SetOpeningMsgHandlers(void)
{
    return m_OpeningMsgHandlers;
}

///////////////////////////////////////////////////////////////////////////////
CDBConnectionFactory::SOpeningContext::SOpeningContext
(I_DriverContext& driver_ctx_in, CServiceInfo& service_info_in,
 CDB_UserHandler::TExceptions& exceptions)
    : driver_ctx(driver_ctx_in), service_info(service_info_in),
      conn_status(IConnValidator::eInvalidConn), errors(exceptions)
{
}

CDB_Connection*
CDBConnectionFactory::MakeDBConnection(
    I_DriverContext& ctx,
    const CDBConnParams& params,
    CDB_UserHandler::TExceptions& exceptions)
{
    CReadLockGuard guard(m_Lock);

    unique_ptr<CDB_Connection> t_con;
    CRuntimeData& rt_data = GetRuntimeData(params.GetConnValidator());
    CServiceInfo& service_info
        = rt_data.GetServiceInfo(params.GetServerName());
    CServiceInfo::TGuard service_guard(service_info);
    TSvrRef dsp_srv = service_info.GetDispatchedServer();

    // Prepare to collect messages, whose proper severity depends on whether
    // ANY attempt succeeds.
    impl::CDBHandlerStack ultimate_handlers;
    {{
        const impl::CDriverContext* ctx_impl
            = dynamic_cast<impl::CDriverContext*>(&ctx);
        if (params.GetOpeningMsgHandlers().GetSize() > 0) {
            ultimate_handlers = params.GetOpeningMsgHandlers();
        } else if (ctx_impl != NULL) {
            ultimate_handlers = ctx_impl->GetCtxHandlerStack();
        } else {
            ultimate_handlers.Push(&CDB_UserHandler::GetDefault());
        }
    }}
    SOpeningContext opening_ctx(ctx, service_info, exceptions);
    CRef<CDB_UserHandler_Deferred> handler
        (new CDB_UserHandler_Deferred(ultimate_handlers));
    opening_ctx.handlers.Push(&*handler, eTakeOwnership);
    
    if ( dsp_srv.Empty() ) {
        // We are here either because server name was never dispatched or
        // because a named connection pool has been used before.
        // Dispatch server name ...

        t_con.reset(DispatchServerName(opening_ctx, params));
    } else {
        // Server name is already dispatched ...
        string single_server(params.GetParam("single_server"));
        string is_pooled(params.GetParam("is_pooled"));

        // We probably need to re-dispatch it ...
        if (single_server != "true"
            &&  ((GetMaxNumOfDispatches()
                  &&  (service_info.GetNumOfDispatches()
                       >= GetMaxNumOfDispatches()))
                 || (dsp_srv->GetExpireTime() != 0
                     &&  (CurrentTime(CTime::eUTC).GetTimeT()
                          > dsp_srv->GetExpireTime())))) {
            // We definitely need to re-dispatch it ...

            // Clean previous info ...
            service_info.CleanExcluded();
            service_info.SetDispatchedServer(TSvrRef());
            if (is_pooled == "true") {
                service_info.ClearOptions();
            }
            t_con.reset(DispatchServerName(opening_ctx, params));
        } else if (single_server != "true"  &&  is_pooled == "true") {
            // Don't fully redispatch, but keep the pool balanced
            t_con.reset(DispatchServerName(opening_ctx, params));
        } else {
            // We do not need to re-dispatch it ...

            // Try to connect.
            try {
                opening_ctx.last_tried.Reset(dsp_srv); // provisionally
                // I_DriverContext::SConnAttr cur_conn_attr(conn_attr);
                // cur_conn_attr.srv_name = dsp_srv->GetName();
                CDB_DBLB_Delegate cur_params(
                        dsp_srv->GetName(),
                        dsp_srv->GetHost(),
                        dsp_srv->GetPort(),
                        params);
                cur_params.SetOpeningMsgHandlers() = opening_ctx.handlers;

                // MakeValidConnection may return NULL here because a newly
                // created connection may not pass validation.
                t_con.reset(MakeValidConnection(opening_ctx,
                                                // cur_conn_attr,
                                                cur_params));

            } catch (CDB_Exception& ex) {
                // opening_ctx.errors.push_back(ex.Clone());
                opening_ctx.handlers.PostMsg(&ex);
                if (params.GetConnValidator()) {
                    opening_ctx.conn_status
                        = params.GetConnValidator()->ValidateException(ex);
                }
            }

            if (t_con.get() == NULL) {
                // We couldn't connect ...
                if (single_server != "true") {
                    // Server might be temporarily unavailable ...
                    // Check conn_status ...
                    if (opening_ctx.conn_status
                        == IConnValidator::eTempInvalidConn) {
                        service_info.IncNumOfValidationFailures(
                            opening_ctx.last_tried);
                    }

                    // Re-dispatch ...
                    t_con.reset(DispatchServerName(opening_ctx, params));
                }
            } else {
                // Dispatched server is already set, but calling of this method
                // will increase number of successful dispatches.
                service_info.SetDispatchedServer(opening_ctx.last_tried);
            }
        }
    }

    x_LogConnection(opening_ctx, t_con.get(), params);
    handler->Flush((t_con.get() == NULL) ? eDiagSevMax : eDiag_Warning);

    return t_con.release();
}

CDB_Connection*
CDBConnectionFactory::DispatchServerName(
    SOpeningContext& ctx,
    const CDBConnParams& params)
{
    CStopWatchScope timing(ctx.dispatch_server_name_sw);
    CDB_Connection* t_con = NULL;
    // I_DriverContext::SConnAttr curr_conn_attr(conn_attr);
    const string service_name(params.GetServerName());
    bool do_not_dispatch = params.GetParam("do_not_dispatch") == "true";
    string cur_srv_name;
    Uint4 cur_host = 0;
    Uint2  cur_port = 0;

    // Try to connect up to a given number of alternative servers ...
    unsigned int alternatives = GetMaxNumOfServerAlternatives();
    list<TSvrRef> tried_servers;
    bool full_retry_made = false;
    CRef<CDBPoolBalancer> balancer;
    CServiceInfo& service_info = ctx.service_info;

    if ( !do_not_dispatch  &&  params.GetParam("is_pooled") == "true"
        &&  !service_name.empty()  ) {
        balancer.Reset(new CDBPoolBalancer
                       (service_info, params.GetParam("pool_name"),
                        &ctx.driver_ctx));
    }
    for ( ; !t_con && alternatives > 0; --alternatives ) {
        TSvrRef dsp_srv;

        if (do_not_dispatch) {
            cur_srv_name = params.GetServerName();
            cur_host = params.GetHost();
            cur_port = params.GetPort();
        }
        // It is possible that a server name won't be provided.
        // This is possible when somebody uses a named connection pool.
        // In this case we even won't try to map it.
        else if (!service_name.empty()) {
            if (balancer.NotEmpty()) {
                ctx.excluded = ctx.service_info.GetExcluded();
                dsp_srv = balancer->GetServer(&t_con, &params);
            }
            if (dsp_srv.Empty()) {
                dsp_srv = service_info.GetDispatchedServer();
            }
            if (dsp_srv.Empty()) {
                dsp_srv = service_info.GetMappedServer();
            }

            if (tried_servers.empty()
                &&  (dsp_srv.Empty()
                     ||  (dsp_srv->GetName() == service_name
                          &&  dsp_srv->GetHost() == 0
                          &&  dsp_srv->GetPort() == 0
                          &&  service_info.HasExclusions() ))) {
                _TRACE("List of servers for service " << service_name
                       << " is exhausted. Giving excluded a try.");
                service_info.CleanExcluded();
                dsp_srv = service_info.GetMappedServer();
            }

            if (dsp_srv.Empty()) {
                ctx.errors.push_back(
                    new CDB_Exception(
                        DIAG_COMPILE_INFO, NULL, CDB_Exception::EErrCode(0),
                        "Service mapper didn't return any server for "
                        + service_name,
                        eDiag_Error, 0));
                return NULL;
            }
            if (dsp_srv->GetName() == service_name
                &&  dsp_srv->GetHost() == 0  &&  dsp_srv->GetPort() == 0
                &&  !tried_servers.empty())
            {
                if (full_retry_made) {
                    if (!m_TryServerToo) {
                        ctx.errors.push_back(
                            new CDB_Exception(
                                DIAG_COMPILE_INFO, NULL,
                                CDB_Exception::EErrCode(0),
                                "No more servers to try (didn't try "
                                + service_name + " as server name)",
                                eDiag_Error, 0));
                        return NULL;
                    }
                }
                else {
                    _TRACE("List of servers for service " << service_name
                           << " is exhausted. Giving excluded a try.");

                    service_info.CleanExcluded();
                    ITERATE(list<TSvrRef>, it, tried_servers) {
                        service_info.Exclude(*it);
                    }
                    if (balancer.NotEmpty()) {
                        service_info.ClearOptions();
                        balancer.Reset
                            (new CDBPoolBalancer
                             (service_info, params.GetParam("pool_name"),
                              &ctx.driver_ctx));
                    }
                    full_retry_made = true;
                    continue;
                }
            }

            // If connection attempts will take too long mapper can return
            // the same server once more. Let's not try to connect to it
            // again.
            bool found = false;
            ITERATE(list<TSvrRef>, it, tried_servers) {
                if (**it == *dsp_srv) {
                    service_info.Exclude(dsp_srv);
                    found = true;
                    break;
                }
            }
            if (found)
                continue;

            // curr_conn_attr.srv_name = dsp_srv->GetName();
            cur_srv_name = dsp_srv->GetName();
            cur_host = dsp_srv->GetHost();
            cur_port = dsp_srv->GetPort();
        } else if (params.GetParam("pool_name").empty()) {
            DATABASE_DRIVER_ERROR
                ("Neither server name nor pool name provided.", 111000);
        } else {
            // Old-fashioned connection pool.
            // We do not change anything ...
            cur_srv_name = params.GetServerName();
            cur_host = params.GetHost();
            cur_port = params.GetPort();
        }

        // Try to connect up to a given number of attempts ...
        unsigned int attempts = GetMaxNumOfConnAttempts();
        ctx.conn_status = IConnValidator::eInvalidConn;

        // We don't check value of conn_status inside of a loop below by design.
        for (; attempts > 0; --attempts) {
            try {
                CDB_DBLB_Delegate cur_params(
                        cur_srv_name,
                        cur_host,
                        cur_port,
                        params);
                cur_params.SetOpeningMsgHandlers() = ctx.handlers;
                t_con = MakeValidConnection(ctx,
                                            // curr_conn_attr,
                                            cur_params,
                                            t_con);
                if (t_con != NULL) {
                    break;
                }
            } catch (CDB_Exception& ex) {
                // ctx.errors.push_back(ex.Clone());
                ctx.handlers.PostMsg(&ex);
                if (params.GetConnValidator()) {
                    ctx.conn_status
                        = params.GetConnValidator()->ValidateException(ex);
                }
            }
        }

        if (do_not_dispatch) {
            return t_con;
        }
        else if (!t_con) {
            bool need_exclude = true;
            if (cur_srv_name == service_name  &&  cur_host == 0  &&  cur_port == 0
                &&  (ctx.conn_status != IConnValidator::eTempInvalidConn
                        ||  (GetMaxNumOfValidationAttempts()
                             &&  service_info.GetNumOfValidationFailures()
                                               >= GetMaxNumOfValidationAttempts())))
            {
                ctx.errors.push_back(
                    new CDB_Exception(DIAG_COMPILE_INFO, NULL,
                                      CDB_Exception::EErrCode(0),
                                      "No more services/servers to try",
                                      eDiag_Error, 0));
                return NULL;
            }

            if (need_exclude) {
                // Server might be temporarily unavailable ...
                // Check conn_status ...
                if (ctx.conn_status == IConnValidator::eTempInvalidConn) {
                    service_info.IncNumOfValidationFailures(ctx.last_tried);
                } else {
                    // conn_status == IConnValidator::eInvalidConn
                    service_info.Exclude(ctx.last_tried);
                    tried_servers.push_back(ctx.last_tried);
                }
            }
        } else {
            service_info.SetDispatchedServer(dsp_srv);
        }
    }

    if (!t_con) {
        ctx.errors.push_back(
            new CDB_Exception(
                DIAG_COMPILE_INFO, NULL, CDB_Exception::EErrCode(0),
                "Can't try any more alternatives (max number is "
                + NStr::UIntToString(GetMaxNumOfServerAlternatives()) + ")",
                eDiag_Error, 0));
    }
    return t_con;
}

CDB_Connection*
CDBConnectionFactory::MakeValidConnection(
    SOpeningContext& ctx,
    const CDBConnParams& params,
    CDB_Connection* candidate)
{
    CStopWatchScope timing(ctx.make_valid_connection_sw);
    _TRACE("Trying to connect to server '" << params.GetServerName()
           << "', host " << impl::ConvertN2A(params.GetHost())
           << ", port " << params.GetPort());
    if (params.GetHost() != 0) {
        ctx.tried.push_back(impl::ConvertN2A(params.GetHost()) + ':'
                            + NStr::NumericToString(params.GetPort()));
    } else {
        ctx.tried.push_back(params.GetServerName());
    }

    unique_ptr<CDB_Connection> conn;
    try {
        if (candidate == NULL) {
            ctx.excluded = ctx.service_info.GetExcluded();
            CServiceInfo::TAntiGuard anti_guard(ctx.service_info);
            conn.reset(CtxMakeConnection(ctx.driver_ctx, params));
        } else {
            conn.reset(candidate);
        }
    } catch (...) {
        ctx.last_tried.Reset(ctx.service_info.GetDispatchedServer());
        if (ctx.last_tried.Empty()) {
            ctx.last_tried.Reset
                (new CDBServer(params.GetServerName(), params.GetHost(),
                               params.GetPort()));
        }
        throw;
    }

    if (conn.get())
    {
        ctx.last_tried.Reset(new CDBServer(conn->ServerName(),
                                           conn->Host(), conn->Port()));
        if (conn->IsReusable()  &&  !params.GetParam("pool_name").empty()) {
            if (conn->Host() != 0) {
                ctx.tried.back() = (impl::ConvertN2A(conn->Host()) + ':'
                                    + NStr::NumericToString(conn->Port()));
            } else {
                ctx.tried.back() = conn->ServerName();
            }
        }

        if (conn->Host() == 0) {
            GetRuntimeData(params.GetConnValidator()).GetDBServiceMapper()
                .RecordServer(conn->GetExtraFeatures());
        }
        CTrivialConnValidator use_db_validator(
            params.GetDatabaseName(),
            CTrivialConnValidator::eKeepModifiedConnection
            );
        CConnValidatorCoR validator;

        validator.Push(params.GetConnValidator());

        // Check "use <database>" command ...
        if (!params.GetDatabaseName().empty()) {
            validator.Push(CRef<IConnValidator>(&use_db_validator));
        }

        ctx.conn_status = IConnValidator::eInvalidConn;
        try {
            CServiceInfo::TAntiGuard anti_guard(ctx.service_info);
            ctx.conn_status = validator.Validate(*conn);
            if (ctx.conn_status != IConnValidator::eValidConn) {
                if (conn->IsReusable()) {
                    static_cast<impl::CConnection&>(conn->GetExtraFeatures())
                        .m_Reusable = false;
                }
                CDB_Exception ex(DIAG_COMPILE_INFO, NULL,
                                 CDB_Exception::EErrCode(0),
                                 "Validation failed against "
                                 + params.GetServerName(),
                                 eDiag_Error, 0);
                ex.SetRetriable(eRetriable_No);
                // ctx.errors.push_back(ex.Clone());
                ctx.handlers.PostMsg(&ex);
                return NULL;
            }
        } catch (CDB_Exception& ex) {
            if (params.GetConnValidator().NotNull()) {
                ctx.conn_status
                    = params.GetConnValidator()->ValidateException(ex);
            }
            if (ctx.conn_status != IConnValidator::eValidConn) {
                if (conn->IsReusable()) {
                    static_cast<impl::CConnection&>(conn->GetExtraFeatures())
                        .m_Reusable = false;
                }
                // ctx.errors.push_back(ex.Clone());
                ctx.handlers.PostMsg(&ex);
                return NULL;
            }
        } catch (const CException& ex) {
            ERR_POST_X(1, Warning << ex.ReportAll() << " when trying to connect to "
                       << "server '" << params.GetServerName() << "' as user '"
                       << params.GetUserName() << "'");
            return NULL;
        } catch (...) {
            ERR_POST_X(2, Warning << "Unknown exception when trying to connect to "
                       << "server '" << params.GetServerName() << "' as user '"
                       << params.GetUserName() << "'");
            throw;
        }
        ctx.tried.pop_back();
        conn->FinishOpening();
    }
    else {
        ctx.last_tried.Reset(ctx.service_info.GetDispatchedServer());
        if (ctx.last_tried.Empty()) {
            ctx.last_tried.Reset
                (new CDBServer(params.GetServerName(), params.GetHost(),
                               params.GetPort()));
        }

        ctx.errors.push_back(
            new CDB_Exception(DIAG_COMPILE_INFO, NULL,
                              CDB_Exception::EErrCode(0),
                              "Parameters prohibited creating connection",
                              eDiag_Error, 0));
    }
    return conn.release();
}

void
CDBConnectionFactory::GetServersList(const string& validator_name,
                                     const string& service_name,
                                     list<string>* serv_list)
{
    const IDBServiceMapper& mapper
                        = GetRuntimeData(validator_name).GetDBServiceMapper();
    mapper.GetServersList(service_name, serv_list);
}

void
CDBConnectionFactory::WorkWithSingleServer(const string& validator_name,
                                           const string& service_name,
                                           const string& server)
{
    CRuntimeData& rt_data = GetRuntimeData(validator_name);
    TSvrRef svr(new CDBServer(server, 0, 0, numeric_limits<unsigned int>::max()));
    auto& service_info = rt_data.GetServiceInfo(service_name);
    IDBServiceInfo::TGuard guard(service_info);
    service_info.SetDispatchedServer(svr);
}

// Make an effort to give each request its own connection numbering.
// The resulting field names can still technically repeat if an
// application reuses request IDs or tampers with named properties.
// However, that concern appears to be purely theoretical, and any
// duplication that does manage to occur will have fairly minor
// consequences.  In contrast, a global counter would cause trouble
// for log analyzers, and a map would either accumulate stale keys or
// risk duplication.
static string s_GetNextLogPrefix(void)
{
    CRequestContext& rctx = GetDiagContext().GetRequestContext();
    string rid_str = NStr::NumericToString(rctx.GetRequestID());
    if (rctx.GetProperty("_dbapi_last_rid") == rid_str) {
        string connection_no
            = NStr::UIntToString(
                NStr::StringToUInt(rctx.GetProperty("_dbapi_connection_no"))
                + 1U);
        rctx.SetProperty("_dbapi_connection_no", connection_no);
        return connection_no + ".dbapi_";
    } else {
        rctx.SetProperty("_dbapi_last_rid",      rid_str);
        rctx.SetProperty("_dbapi_connection_no", "1");
        return "1.dbapi_";
    }
}

void CDBConnectionFactory::x_LogConnection(const SOpeningContext& ctx,
                                           const CDB_Connection* connection,
                                           const CDBConnParams& params)
{
    CDBServer stub_dsp_srv;

    CRef<IConnValidator> validator = params.GetConnValidator();
    const string&        service   = params.GetServerName();
    CServiceInfo&        svc_info  = ctx.service_info;
    TSvrRef              dsp_srv   = ctx.last_tried;

    CDiagContext_Extra   extra     = GetDiagContext().Extra();
    string               prefix    = s_GetNextLogPrefix();

    if (dsp_srv.Empty()) {
        // Try rt_data.GetDispatchedServer(service) first?
        dsp_srv.Reset(&stub_dsp_srv);
    }

    {{
        const char* status_str = "???";
        switch (ctx.conn_status) {
        case IConnValidator::eValidConn:
            status_str = "valid";
            break;
        case IConnValidator::eInvalidConn:
            status_str = "invalid";
            break;
        case IConnValidator::eTempInvalidConn:
            status_str = "temporarily-invalid";
            break;
        }
        extra.Print(prefix + "conn_status", status_str);
    }}

    extra.Print(prefix + "resource", service);

    if ( !dsp_srv->GetName().empty() ) {
        extra.Print(prefix + "server_name", dsp_srv->GetName());
    } else if (connection != NULL  &&  !connection->ServerName().empty()) {
        extra.Print(prefix + "server_name", connection->ServerName());
    }

    if (dsp_srv->GetHost() != 0) {
        extra.Print(prefix + "server_ip",
                    impl::ConvertN2A(dsp_srv->GetHost()));
    } else if (params.GetHost() != 0) {
        extra.Print(prefix + "server_ip", impl::ConvertN2A(params.GetHost()));
    } else if (connection != NULL  &&  connection->Host() != 0) {
        extra.Print(prefix + "server_ip",
                    impl::ConvertN2A(connection->Host()));
    }

    if (dsp_srv->GetPort() != 0) {
        extra.Print(prefix + "server_port", dsp_srv->GetPort());
    } else if (params.GetPort() != 0) {
        extra.Print(prefix + "server_port", params.GetPort());
    } else if (connection != NULL  &&  connection->Port() != 0) {
        extra.Print(prefix + "server_port", connection->Port());
    }

    if ( !params.GetUserName().empty() ) {
        extra.Print(prefix + "username", params.GetUserName());
    }

    if ( !params.GetDatabaseName().empty() ) {
        extra.Print(prefix + "db_name", params.GetDatabaseName());
    } else {
        CTrivialConnValidator* tcv
            = dynamic_cast<CTrivialConnValidator*>(validator.GetPointer());
        if (tcv != NULL  &&  !tcv->GetDBName().empty()) {
            extra.Print(prefix + "db_name", tcv->GetDBName());
        }
    }

    if (connection != NULL  &&  !connection->PoolName().empty() ) {
        extra.Print(prefix + "pool", connection->PoolName());
    } else if ( !params.GetParam("pool_name").empty() ) {
        extra.Print(prefix + "pool", params.GetParam("pool_name"));
    }

    if (validator.NotEmpty()) {
        extra.Print(prefix + "validator", validator->GetName());
    }

    {{
        string driver_name = ((connection == NULL)
                              ? ctx.driver_ctx.GetDriverName()
                              : connection->GetDriverName());
        if ( !driver_name.empty() ) {
            extra.Print(prefix + "driver", driver_name);
        }
    }}

    extra.Print(prefix + "name_mapper", svc_info.GetMapperName());

    size_t retries = ctx.tried.size();
    if (ctx.conn_status != IConnValidator::eValidConn) {
        --retries;
    }
    extra.Print(prefix + "retries", retries);
    if ( !ctx.tried.empty() ) {
        extra.Print(prefix + "tried", NStr::Join(ctx.tried, " "));
    }
    if ( !ctx.excluded.empty() ) {
        extra.Print(prefix + "excluded", ctx.excluded);
    }
    if (connection != NULL) {
        extra.Print(prefix + "conn_reuse_count", connection->GetReuseCount());
    }

    double make_valid_connection_elapsed = ctx.make_valid_connection_sw.Elapsed();
    if (make_valid_connection_elapsed != 0.0) {
        extra.Print(prefix + "make_valid_connection_time",
                    make_valid_connection_elapsed);
    }

    double dispatch_server_name_elapsed = ctx.dispatch_server_name_sw.Elapsed();
    if (dispatch_server_name_elapsed != 0.0) {
        extra.Print(prefix + "dispatch_server_name_time",
                    dispatch_server_name_elapsed);
    }
}


///////////////////////////////////////////////////////////////////////////////
const IDBServiceMapper::TOptions&
CDBConnectionFactory::CServiceInfo::GetOptions(void)
{
    if (m_Options.empty()) {
        m_Mapper->GetServerOptions(m_ServiceName, &m_Options);
        // OK to leave empty if nothing turns up; service mappers can and
        // do take responsibility for temporarily remembering negative
        // results if checking from scratch is slow, and higher-level logic
        // on this end falls back on GetServer as needed.
    }
    return m_Options;
}

void CDBConnectionFactory::CServiceInfo::SetDispatchedServer(
    const TSvrRef& server)
{
    if (server.Empty()) {
        m_NumDispatches = 0;
    } else {
        ++m_NumDispatches;
    }

    m_Dispatched = server;
}

void CDBConnectionFactory::CServiceInfo::IncNumOfValidationFailures(
    const TSvrRef& dsp_srv)
{
    ++m_NumValidationFailures;
    auto limit = m_Factory.GetMaxNumOfValidationAttempts();

    if (limit != 0  &&  m_NumValidationFailures >= limit) {
        // It is time to finish with this server ...
        Exclude(dsp_srv);
    }
}

string CDBConnectionFactory::CServiceInfo::GetExcluded(void)
{
    if ( !m_Mapper->HasExclusions(m_ServiceName) ) {
        return kEmptyStr;
    }

    m_Mapper->GetServerOptions(m_ServiceName, &m_Options);
    string result, delim;
    for (const auto &it : m_Options) {
        if (it->IsExcluded()) {
            string exclusion;
            if (it->GetHost() != 0) {
                exclusion = impl::ConvertN2A(it->GetHost());
                if (it->GetPort() != 0) {
                    exclusion += ':' + NStr::NumericToString(it->GetPort());
                }
            } else if ( !it->GetName().empty() ) {
                exclusion = it->GetName();
            }
            if ( !exclusion.empty() ) {
                result += delim + exclusion;
                delim = " ";
            }
        }
    }
    return result;
}

///////////////////////////////////////////////////////////////////////////////
CDBConnectionFactory::CRuntimeData::CRuntimeData(
    const CDBConnectionFactory& parent,
    const CRef<IDBServiceMapper>& mapper
    ) :
m_Mutex(new CFastMutex),
m_Parent(&parent),
m_DBServiceMapper(mapper)
{
}

CDBConnectionFactory::CServiceInfo&
CDBConnectionFactory::CRuntimeData::GetServiceInfo(const string& service_name)
{
    CFastMutexGuard guard(*m_Mutex);
    auto& entry = m_ServiceInfoMap[service_name];
    if (entry.Empty()) {
        entry.Reset(
            new CServiceInfo(*m_Parent, *m_DBServiceMapper, service_name));
    }
    return *entry;
}

///////////////////////////////////////////////////////////////////////////////
CDBConnectionFactory::CMapperFactory::CMapperFactory(
    IDBServiceMapper::TFactory svc_mapper_factory,
    const IRegistry* registry,
    EDefaultMapping def_mapping
    ) :
    m_SvcMapperFactory(svc_mapper_factory),
    m_Registry(registry),
    m_DefMapping(def_mapping)
{
    CHECK_DRIVER_ERROR(!m_SvcMapperFactory && def_mapping != eUseDefaultMapper,
                       "Database service name to server name mapper was not "
                       "defined properly.",
                       0);
}


IDBServiceMapper*
CDBConnectionFactory::CMapperFactory::Make(void) const
{
    if (m_DefMapping == eUseDefaultMapper) {
        CRef<CDBServiceMapperCoR> mapper(new CDBServiceMapperCoR());

        mapper->Push(CRef<IDBServiceMapper>(new CDBDefaultServiceMapper()));
        if (m_SvcMapperFactory) {
            mapper->Push(CRef<IDBServiceMapper>(m_SvcMapperFactory(m_Registry)));
        }

        return mapper.Release();
    } else {
        if (m_SvcMapperFactory) {
            return m_SvcMapperFactory(m_Registry);
        }
    }

    return NULL;
}


///////////////////////////////////////////////////////////////////////////////
CDBGiveUpFactory::CDBGiveUpFactory(IDBServiceMapper::TFactory svc_mapper_factory,
                                   const IRegistry* registry,
                                   EDefaultMapping def_mapping)
: CDBConnectionFactory(svc_mapper_factory, registry, def_mapping)
{
    SetMaxNumOfConnAttempts(1); // This value is supposed to be default.
    SetMaxNumOfServerAlternatives(1); // Do not try other servers.
}

CDBGiveUpFactory::~CDBGiveUpFactory(void)
{
}

///////////////////////////////////////////////////////////////////////////////
CDBRedispatchFactory::CDBRedispatchFactory(IDBServiceMapper::TFactory svc_mapper_factory,
                                           const IRegistry* registry,
                                           EDefaultMapping def_mapping)
: CDBConnectionFactory(svc_mapper_factory, registry, def_mapping)
{
    SetMaxNumOfDispatches(1);
    SetMaxNumOfValidationAttempts(0); // Unlimited ...
}

CDBRedispatchFactory::~CDBRedispatchFactory(void)
{
}


///////////////////////////////////////////////////////////////////////////////
CConnValidatorCoR::CConnValidatorCoR(void)
{
}

CConnValidatorCoR::~CConnValidatorCoR(void)
{
}

IConnValidator::EConnStatus
CConnValidatorCoR::Validate(CDB_Connection& conn)
{
    CFastReadGuard guard(m_Lock);

    NON_CONST_ITERATE(TValidators, vr_it, m_Validators) {
        EConnStatus status = (*vr_it)->Validate(conn);

        // Exit if we met an invalid connection ...
        if (status != eValidConn) {
            return status;
        }
    }
    return eValidConn;
}

string
CConnValidatorCoR::GetName(void) const
{
    string result("CConnValidatorCoR");

    CFastReadGuard guard(m_Lock);

    ITERATE(TValidators, vr_it, m_Validators) {
        result += (*vr_it)->GetName();
    }

    return result;
}

void
CConnValidatorCoR::Push(const CRef<IConnValidator>& validator)
{
    if (validator.NotNull()) {
        CFastWriteGuard guard(m_Lock);

        m_Validators.push_back(validator);
    }
}

void
CConnValidatorCoR::Pop(void)
{
    CFastWriteGuard guard(m_Lock);

    m_Validators.pop_back();
}

CRef<IConnValidator>
CConnValidatorCoR::Top(void) const
{
    CFastReadGuard guard(m_Lock);

    return m_Validators.back();
}

bool
CConnValidatorCoR::Empty(void) const
{
    CFastReadGuard guard(m_Lock);

    return m_Validators.empty();
}

///////////////////////////////////////////////////////////////////////////////
CTrivialConnValidator::CTrivialConnValidator(const string& db_name,
                                             int attr) :
m_DBName(db_name),
m_Attr(attr)
{
}

CTrivialConnValidator::~CTrivialConnValidator(void)
{
}

/* Future development ...
IConnValidator::EConnStatus
CTrivialConnValidator::Validate(CDB_Connection& conn)
{
    string curr_dbname;

    // Get current database name ...
    {
        unique_ptr<CDB_LangCmd> auto_stmt(conn.LangCmd("select db_name()"));
        auto_stmt->Send();
        while (auto_stmt->HasMoreResults()) {
            unique_ptr<CDB_Result> rs(auto_stmt->Result());

            if (rs.get() == NULL) {
                continue;
            }

            if (rs->ResultType() != eDB_RowResult) {
                continue;
            }

            while (rs->Fetch()) {
                CDB_VarChar db_name;
                rs->GetItem(&db_name);
                curr_dbname = static_cast<const string&>(db_name);
            }
        }
    }
    // Try to change a database ...
    if (curr_dbname != GetDBName()) {
        conn.SetDatabaseName(GetDBName());
    }

    if (GetAttr() & eCheckSysobjects) {
        unique_ptr<CDB_LangCmd> set_cmd(conn.LangCmd("SELECT id FROM sysobjects"));
        set_cmd->Send();
        set_cmd->DumpResults();
    }

    // Go back to the original (master) database ...
    if (GetAttr() & eRestoreDefaultDB && curr_dbname != GetDBName()) {
        conn.SetDatabaseName(curr_dbname);
    }

    // All exceptions are supposed to be caught and processed by
    // CDBConnectionFactory ...
    return eValidConn;
}
*/

IConnValidator::EConnStatus
CTrivialConnValidator::Validate(CDB_Connection& conn)
{
    // Try to change a database ...
    conn.SetDatabaseName(GetDBName());

    if (GetAttr() & eCheckSysobjects) {
        unique_ptr<CDB_LangCmd> set_cmd(conn.LangCmd("SELECT id FROM sysobjects"));
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

string
CTrivialConnValidator::GetName(void) const
{
    string result("CTrivialConnValidator");

    result += (GetAttr() == eCheckSysobjects ? "CSO" : "");
    result += GetDBName();

    return result;
}

END_NCBI_SCOPE
