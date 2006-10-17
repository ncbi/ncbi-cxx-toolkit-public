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
* Author:  Sergey Sikorskiy
*
*/

#include <ncbi_pch.hpp>

#include <dbapi/driver/dbapi_conn_factory.hpp>
#include <dbapi/driver/dbapi_svc_mapper.hpp>
#include <dbapi/driver/public.hpp>
#include <corelib/ncbiapp.hpp>

BEGIN_NCBI_SCOPE


///////////////////////////////////////////////////////////////////////////////
CDBConnectionFactory::CDBConnectionFactory(IDBServiceMapper* svc_mapper,
                                           const IRegistry* registry,
                                           EDefaultMapping def_mapping) :
m_MaxNumOfConnAttempts( 1 ),
m_MaxNumOfServerAlternatives( 32 ),
m_MaxNumOfDispatches( 0 ),
m_ConnectionTimeout( 0 ),
m_LoginTimeout( 0 )
{
    CHECK_DRIVER_ERROR(!svc_mapper && def_mapping != eUseDefaultMapper,
                       "Database service name to server name mapper was not "
                       "defined properly.",
                       0);

    if (def_mapping == eUseDefaultMapper) {
        CDBServiceMapperCoR* mapper = new CDBServiceMapperCoR();

        mapper->Push(CRef<IDBServiceMapper>(new CDBDefaultServiceMapper()));
        mapper->Push(CRef<IDBServiceMapper>(svc_mapper));

        m_DBServiceMapper.reset(mapper);
    } else {
        m_DBServiceMapper.reset(svc_mapper);
    }

    ConfigureFromRegistry(registry);
}

CDBConnectionFactory::~CDBConnectionFactory(void)
{
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
        m_MaxNumOfServerAlternatives =
            registry->GetInt(section_name, "MAX_SERVER_ALTERNATIVES", 32);
        m_MaxNumOfDispatches =
            registry->GetInt(section_name, "MAX_DISPATCHES", 0);
        m_ConnectionTimeout =
            registry->GetInt(section_name, "CONNECTION_TIMEOUT", 0);
        m_LoginTimeout =
            registry->GetInt(section_name, "LOGIN_TIMEOUT", 0);
    } else {
        m_MaxNumOfConnAttempts = 1;
        m_MaxNumOfServerAlternatives = 32;
        m_MaxNumOfDispatches = 0;
        m_ConnectionTimeout = 0;
        m_LoginTimeout = 0;
    }
}

void
CDBConnectionFactory::Configure(const IRegistry* registry)
{
    CFastMutexGuard mg(m_Mtx);

    ConfigureFromRegistry(registry);
}

void
CDBConnectionFactory::SetMaxNumOfConnAttempts(unsigned int max_num)
{
    CFastMutexGuard mg(m_Mtx);

    m_MaxNumOfConnAttempts = max_num;
}

void
CDBConnectionFactory::SetMaxNumOfServerAlternatives(unsigned int max_num)
{
    CFastMutexGuard mg(m_Mtx);

    m_MaxNumOfServerAlternatives = max_num;
}

void
CDBConnectionFactory::SetMaxNumOfDispatches(unsigned int max_num)
{
    CFastMutexGuard mg(m_Mtx);

    m_MaxNumOfDispatches = max_num;
}

void
CDBConnectionFactory::SetConnectionTimeout(unsigned int timeout)
{
    CFastMutexGuard mg(m_Mtx);

    m_ConnectionTimeout = timeout;
}

void
CDBConnectionFactory::SetLoginTimeout(unsigned int timeout)
{
    CFastMutexGuard mg(m_Mtx);

    m_LoginTimeout = timeout;
}

unsigned int
CDBConnectionFactory::CalculateConnectionTimeout
(const I_DriverContext& ctx) const
{
    unsigned int timeout = 3;

    if (GetConnectionTimeout()) {
        timeout = GetConnectionTimeout();
    } else if (ctx.GetTimeout()) {
        timeout = ctx.GetTimeout();
    }

    return timeout;
}

unsigned int
CDBConnectionFactory::CalculateLoginTimeout(const I_DriverContext& ctx) const
{
    unsigned int timeout = 3;

    if (GetLoginTimeout()) {
        timeout = GetLoginTimeout();
    } else if (ctx.GetLoginTimeout()) {
        timeout = ctx.GetLoginTimeout();
    }

    return timeout;
}

CDB_Connection*
CDBConnectionFactory::MakeDBConnection(
    I_DriverContext& ctx,
    const I_DriverContext::SConnAttr& conn_attr,
    IConnValidator* validator)
{
    CFastMutexGuard mg(m_Mtx);

    CDB_Connection* t_con = NULL;
    TSvrRef dsp_srv = GetDispatchedServer(conn_attr.srv_name);

    // Set timeouts ...
    ctx.SetTimeout(CalculateConnectionTimeout(ctx));
    ctx.SetLoginTimeout(CalculateLoginTimeout(ctx));

    if ( dsp_srv.Empty() ) {
        // We are here either because server name was never dispatched or
        // because a named coonnection pool has been used before.
        // Dispatch server name ...

        t_con = DispatchServerName(ctx, conn_attr, validator);
    } else {
        // Server name is already dispatched ...

        // We probably need to redispatch it ...
        if (GetMaxNumOfDispatches() &&
            GetNumOfDispatches(conn_attr.srv_name) >= GetMaxNumOfDispatches() ) {

            // We definitely need to redispatch it ...
            m_DispatchNumMap[conn_attr.srv_name] = 0;
            t_con = DispatchServerName(ctx, conn_attr, validator);
        } else {
            // We do not need to redispatch it ...

            // Try to connect.
            try {
                I_DriverContext::SConnAttr cur_conn_attr(conn_attr);

                cur_conn_attr.srv_name = dsp_srv->GetName();

                // MakeValidConnection may return NULL here because a newly
                // create connection may not pass validation.
                t_con = MakeValidConnection(ctx, cur_conn_attr, validator);

            } catch(const CDB_Exception&) {
                // Ignore exceptions. We still can redispatch ...
            }

            if ( !t_con ) {
                // We couldn't connect ...
                // Redispach ...
                t_con = DispatchServerName(ctx, conn_attr, validator);
            } else {
                ++m_DispatchNumMap[conn_attr.srv_name];
            }
        }
    }

    return t_con;
}

CDB_Connection*
CDBConnectionFactory::DispatchServerName(
    I_DriverContext& ctx,
    const I_DriverContext::SConnAttr& conn_attr,
    IConnValidator* validator)
{
    CDB_Connection* t_con = NULL;
    I_DriverContext::SConnAttr curr_conn_attr(conn_attr);

    // Try to connect up to a given number of alternative servers ...
    unsigned int alternatives = GetMaxNumOfServerAlternatives();
    for ( ; !t_con && alternatives > 0; --alternatives ) {
        TSvrRef dsp_srv;

        // It is possible that a server name won't be provided.
        // This is possible when somebody uses a named connection pool.
        // In this case we even won't try to map it.
        if (!conn_attr.srv_name.empty()) {
            dsp_srv =
                m_DBServiceMapper->GetServer(conn_attr.srv_name);

            if (dsp_srv.Empty()) {
                return NULL;
            }
        } else if (conn_attr.pool_name.empty()) {
            DATABASE_DRIVER_ERROR
                ("Neither server name nor pool name provided.", 111000);
        }

        curr_conn_attr.srv_name = dsp_srv->GetName();

        // Try to connect up to a given number of attempts ...
        unsigned int attepmpts = GetMaxNumOfConnAttempts();
        for ( ; !t_con && attepmpts > 0; --attepmpts ) {
            try {
                t_con = MakeValidConnection(ctx, curr_conn_attr, validator);
            } catch(const CDB_Exception&) {
                // Do nothing,
            }
        }

        if (!t_con) {
            // Restore previous server name.
            curr_conn_attr.srv_name = conn_attr.srv_name;
            m_DBServiceMapper->Exclude(conn_attr.srv_name, dsp_srv);
        } else {
            ++m_DispatchNumMap[conn_attr.srv_name];
            SetDispatchedServer(conn_attr.srv_name, dsp_srv);
        }
    }

    return t_con;
}

CDB_Connection*
CDBConnectionFactory::MakeValidConnection(
    I_DriverContext& ctx,
    const I_DriverContext::SConnAttr& conn_attr,
    IConnValidator* validator) const
{
    auto_ptr<CDB_Connection> conn(CtxMakeConnection(ctx, conn_attr));

    if (conn.get() &&
        validator &&
        validator->Validate(*conn) == IConnValidator::eInvalidConn) {
        return NULL;
    }
    return conn.release();
}


TSvrRef
CDBConnectionFactory::GetDispatchedServer(const string& service_name)
{
    return m_DispatchedSet[service_name];
}

void
CDBConnectionFactory::SetDispatchedServer(const string& service_name,
                                          const TSvrRef& server)
{
    m_DispatchedSet[service_name] = server;
}

unsigned int
CDBConnectionFactory::GetNumOfDispatches(const string& service_name)
{
    return m_DispatchNumMap[service_name];
}

///////////////////////////////////////////////////////////////////////////////
CDBGiveUpFactory::CDBGiveUpFactory(IDBServiceMapper* svc_mapper,
                                   const IRegistry* registry,
                                   EDefaultMapping def_mapping)
: CDBConnectionFactory(svc_mapper, registry, def_mapping)
{
    SetMaxNumOfConnAttempts(1); // This value is supposed to be default.
    SetMaxNumOfServerAlternatives(1);
}

CDBGiveUpFactory::~CDBGiveUpFactory(void)
{
}

///////////////////////////////////////////////////////////////////////////////
CDBRedispatchFactory::CDBRedispatchFactory(IDBServiceMapper* svc_mapper,
                                           const IRegistry* registry,
                                           EDefaultMapping def_mapping)
: CDBConnectionFactory(svc_mapper, registry, def_mapping)
{
    SetMaxNumOfDispatches( 1 );
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
    NON_CONST_ITERATE(TValidators, vr_it, m_Validators) {
        if ((*vr_it)->Validate(conn) == eInvalidConn) {
            return eInvalidConn;
        }
    }
    return eValidConn;
}

void
CConnValidatorCoR::Push(const CRef<IConnValidator>& validator)
{
    if (validator.NotNull()) {
        CFastMutexGuard mg(m_Mtx);

        m_Validators.push_back(validator);
    }
}

void
CConnValidatorCoR::Pop(void)
{
    CFastMutexGuard mg(m_Mtx);

    m_Validators.pop_back();
}

CRef<IConnValidator>
CConnValidatorCoR::Top(void) const
{
    CFastMutexGuard mg(m_Mtx);

    return m_Validators.back();
}

bool
CConnValidatorCoR::Empty(void) const
{
    CFastMutexGuard mg(m_Mtx);

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

IConnValidator::EConnStatus
CTrivialConnValidator::Validate(CDB_Connection& conn)
{
    try {
        // Try to change a database ...
        {
            auto_ptr<CDB_LangCmd> set_cmd(conn.LangCmd("use " + GetDBName()));
            set_cmd->Send();
            set_cmd->DumpResults();
        }

        if (m_Attr & eCheckSysobjects) {
            auto_ptr<CDB_LangCmd> set_cmd(conn.LangCmd("SELECT id FROM sysobjects"));
            set_cmd->Send();
            set_cmd->DumpResults();
        }

        // Go back to the original (master) database ...
        if (m_Attr & eRestoreDefaultDB) {
            auto_ptr<CDB_LangCmd> set_cmd(conn.LangCmd("use master"));
            set_cmd->Send();
            set_cmd->DumpResults();
        }
    }
    catch (const CDB_Exception&) {
        // Database exceptions -- validation failed...
        return eInvalidConn;
    }

    return eValidConn;
}

END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.11  2006/10/17 22:10:17  ssikorsk
 * Set timeout to 3 sec. in CDBConnectionFactory::CalculateConnectionTimeout
 * if it wasn't set anywhere.
 *
 * Revision 1.10  2006/10/16 18:54:10  ssikorsk
 * Handle case with CNcbiApplication::Instance() == NULL in ConfigureFromRegistry.
 *
 * Revision 1.9  2006/07/12 16:29:30  ssikorsk
 * Separated interface and implementation of CDB classes.
 *
 * Revision 1.8  2006/05/18 18:45:58  ssikorsk
 * Added const to parameters of CalculateConnectionTimeout and CalculateLoginTimeout.
 *
 * Revision 1.7  2006/05/18 17:02:50  ssikorsk
 * Implemented and utilized CalculateConnectionTimeout and CalculateLoginTimeout methods.
 *
 * Revision 1.6  2006/02/28 16:02:31  ssikorsk
 * Redispatch connection if previously successfully validated connection
 * parameters are not valid with currently created connection.
 *
 * Revision 1.5  2006/02/22 16:05:35  ssikorsk
 * DATABASE_DRIVER_FALAL --> DATABASE_DRIVER_ERROR
 *
 * Revision 1.4  2006/01/26 12:08:27  ssikorsk
 * Improved implementation of the class CTrivialConnValidator.
 *
 * Revision 1.3  2006/01/23 13:32:36  ssikorsk
 * CDBConnectionFactory class: improved methods MakeDBConnection and
 * DispathServerName, implemented MakeValidConnection;
 * Implemented CConnValidatorCoR and CTrivialValidator;
 *
 * Revision 1.2  2006/01/06 18:52:12  ssikorsk
 * Increase default connection and login timeouts up to 2 sec.
 *
 * Revision 1.1  2006/01/03 19:44:08  ssikorsk
 * Initial implementation
 *
 * ===========================================================================
 */
