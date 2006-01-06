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
#include <corelib/ncbiapp.hpp>

USING_NCBI_SCOPE;


///////////////////////////////////////////////////////////////////////////////
CDBConnectionFactory::CDBConnectionFactory(IDBServiceMapper* svc_mapper, 
                                           const IRegistry* registry,
                                           EDefaultMapping def_mapping) : 
m_MaxNumOfConnAttempts( 1 ),
m_MaxNumOfServerAlternatives( 32 ),
m_MaxNumOfDispatches( 0 ),
m_ConnectionTimeout( 1 ),
m_LoginTimeout( 1 )
{
    CHECK_DRIVER_ERROR(!svc_mapper && def_mapping != eUseDefaultMapper, 
                       "Database service name to server name mapper was not "
                       "defined properly.", 
                       0);
    
    if (def_mapping == eUseDefaultMapper) {
        CDBServiceMapperCoR* mapper = new CDBServiceMapperCoR();
        
        mapper->push(new CDBDefaultServiceMapper());
        mapper->push(svc_mapper);
        
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
    if (!registry) {
        registry = &CNcbiApplication::Instance()->GetConfig();
    }
    
    _ASSERT(registry);
    
    m_MaxNumOfConnAttempts = 
        registry->GetInt(section_name, "MAX_CONN_ATTEMPTS", 1);
    m_MaxNumOfServerAlternatives = 
        registry->GetInt(section_name, "MAX_SERVER_ALTERNATIVES", 32);
    m_MaxNumOfDispatches = 
        registry->GetInt(section_name, "MAX_DISPATCHES", 0);
    m_ConnectionTimeout = 
        registry->GetInt(section_name, "CONNECTION_TIMEOUT", 2);
    m_LoginTimeout = 
        registry->GetInt(section_name, "LOGIN_TIMEOUT", 2);
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

I_Connection* 
CDBConnectionFactory::MakeConnection(
    I_DriverContext& ctx,
    const I_DriverContext::SConnAttr& conn_attr)
{
    CFastMutexGuard mg(m_Mtx); 
     
    I_Connection* t_con = NULL;
    TSvrRef dsp_srv = GetDispatchedServer(conn_attr.srv_name);
    
    // Set timeouts ...
    ctx.SetTimeout(m_ConnectionTimeout);
    ctx.SetLoginTimeout(m_LoginTimeout);
    
    if ( dsp_srv.Empty() ) {
        // We are here either because server name was never dispatched or 
        // because a named coonnection pool has been used before.
        // Dispatch server name ...
        
        t_con = DispatchServerName(ctx, conn_attr);
    } else {
        // Server name is already dispatched ...
        
        // We probably need to redispatch it ...
        if (GetMaxNumOfDispatches() && 
            GetNumOfDispatches(conn_attr.srv_name) >= GetMaxNumOfDispatches() ) {
            
            // We definitely need to redispatch it ...
            m_DispatchNumMap[conn_attr.srv_name] = 0;
            t_con = DispatchServerName(ctx, conn_attr);
        } else {
            // We do not need to redispatch it ...
            
            // Try to connect.
            try {
                I_DriverContext::SConnAttr cur_conn_attr(conn_attr);

                cur_conn_attr.srv_name = dsp_srv->GetName();
                t_con = CtxMakeConnection(ctx, cur_conn_attr);

                _ASSERT(t_con);
                
                ++m_DispatchNumMap[conn_attr.srv_name];
            } catch(const CDB_Exception&) {
                // Ignore exceptions. We still can redispatch ...
            }

            if ( !t_con ) {
                // We couldn't connect ...
                // Redispach ...
                t_con = DispatchServerName(ctx, conn_attr);
            }
        }
    }

    return t_con;
}

I_Connection*
CDBConnectionFactory::DispatchServerName(
    I_DriverContext& ctx,
    const I_DriverContext::SConnAttr& conn_attr)
{
    I_Connection* t_con = NULL;
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
            DATABASE_DRIVER_FATAL
                ("Neither server name nor pool name provided.", 111000);
        }

        curr_conn_attr.srv_name = dsp_srv->GetName();
        
        // Try to connect up to a given number of attempts ...
        unsigned int attepmpts = GetMaxNumOfConnAttempts();
        for ( ; !t_con && attepmpts > 0; --attepmpts ) {
            try {
                t_con = CtxMakeConnection(ctx, curr_conn_attr);
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


/*
 * ===========================================================================
 * $Log$
 * Revision 1.2  2006/01/06 18:52:12  ssikorsk
 * Increase default connection and login timeouts up to 2 sec.
 *
 * Revision 1.1  2006/01/03 19:44:08  ssikorsk
 * Initial implementation
 *
 * ===========================================================================
 */
