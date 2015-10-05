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

#include <dbapi/driver/dbapi_driver_conn_mgr.hpp>
#include <dbapi/driver/impl/dbapi_impl_context.hpp>
#include <dbapi/driver/impl/dbapi_impl_connection.hpp>
#include <dbapi/driver/dbapi_driver_conn_params.hpp>
#include <dbapi/error_codes.hpp>

#include <corelib/resource_info.hpp>
#include <corelib/ncbifile.hpp>

#include <algorithm>
#include <set>


#if defined(NCBI_OS_MSWIN)
#  include <winsock2.h>
#endif


BEGIN_NCBI_SCOPE

#define NCBI_USE_ERRCODE_X  Dbapi_DrvrContext


NCBI_PARAM_DEF_EX(bool, dbapi, conn_use_encrypt_data, false, eParam_NoThread, NULL);


namespace impl
{

///////////////////////////////////////////////////////////////////////////
//  CDriverContext::
//

CDriverContext::CDriverContext(void) :
#ifdef NCBI_THREADS
    m_PoolSem(0, 1),
#endif
    m_LoginTimeout(0),
    m_Timeout(0),
    m_CancelTimeout(0),
    m_MaxTextImageSize(0),
    m_ClientEncoding(eEncoding_ISO8859_1)
{
    PushCntxMsgHandler    ( &CDB_UserHandler::GetDefault(), eTakeOwnership );
    PushDefConnMsgHandler ( &CDB_UserHandler::GetDefault(), eTakeOwnership );
}

CDriverContext::~CDriverContext(void)
{
    return;
}

void
CDriverContext::SetApplicationName(const string& app_name)
{
    CMutexGuard mg(x_GetCtxMtx());

    m_AppName = app_name;
}

string
CDriverContext::GetApplicationName(void) const
{
    CMutexGuard mg(x_GetCtxMtx());

    return m_AppName;
}

void
CDriverContext::SetHostName(const string& host_name)
{
    CMutexGuard mg(x_GetCtxMtx());

    m_HostName = host_name;
}

string
CDriverContext::GetHostName(void) const
{
    CMutexGuard mg(x_GetCtxMtx());

    return m_HostName;
}

unsigned int CDriverContext::GetLoginTimeout(void) const 
{ 
    CMutexGuard mg(x_GetCtxMtx());

    return m_LoginTimeout; 
}

bool CDriverContext::SetLoginTimeout (unsigned int nof_secs)
{
    CMutexGuard mg(x_GetCtxMtx());

    m_LoginTimeout = nof_secs;

    return true;
}

unsigned int CDriverContext::GetTimeout(void) const 
{ 
    CMutexGuard mg(x_GetCtxMtx());

    return m_Timeout; 
}

bool CDriverContext::SetTimeout(unsigned int nof_secs)
{
    bool success = true;
    CMutexGuard mg(x_GetCtxMtx());

    try {
        m_Timeout = nof_secs;
        
        // We do not have to update query/connection timeout in context
        // any more. Each connection can be updated explicitly now.
        // UpdateConnTimeout();
    } catch (...) {
        success = false;
    }

    return success;
}

unsigned int CDriverContext::GetCancelTimeout(void) const 
{ 
    CMutexGuard mg(x_GetCtxMtx());

    return m_CancelTimeout;
}

bool CDriverContext::SetCancelTimeout(unsigned int nof_secs)
{
    bool success = true;
    CMutexGuard mg(x_GetCtxMtx());

    try {
        m_CancelTimeout = nof_secs;
    } catch (...) {
        success = false;
    }

    return success;
}

bool CDriverContext::SetMaxTextImageSize(size_t nof_bytes)
{
    CMutexGuard mg(x_GetCtxMtx());

    m_MaxTextImageSize = nof_bytes;

    UpdateConnMaxTextImageSize();

    return true;
}

void CDriverContext::PushCntxMsgHandler(CDB_UserHandler* h,
                                         EOwnership ownership)
{
    CMutexGuard mg(x_GetCtxMtx());
    m_CntxHandlers.Push(h, ownership);
}

void CDriverContext::PopCntxMsgHandler(CDB_UserHandler* h)
{
    CMutexGuard mg(x_GetCtxMtx());
    m_CntxHandlers.Pop(h);
}

void CDriverContext::PushDefConnMsgHandler(CDB_UserHandler* h,
                                            EOwnership ownership)
{
    CMutexGuard mg(x_GetCtxMtx());
    m_ConnHandlers.Push(h, ownership);
}

void CDriverContext::PopDefConnMsgHandler(CDB_UserHandler* h)
{
    CMutexGuard mg(x_GetCtxMtx());
    m_ConnHandlers.Pop(h);

    // Remove this handler from all connections
    TConnPool::value_type con = NULL;
    ITERATE(TConnPool, it, m_NotInUse) {
        con = *it;
        con->PopMsgHandler(h);
    }

    ITERATE(TConnPool, it, m_InUse) {
        con = *it;
        con->PopMsgHandler(h);
    }
}


void CDriverContext::ResetEnvSybase(void)
{
    DEFINE_STATIC_MUTEX(env_mtx);

    CMutexGuard mg(env_mtx);
    CNcbiEnvironment env;

    // If user forces his own Sybase client path using $RESET_SYBASE
    // and $SYBASE -- use that unconditionally.
    try {
        if (!env.Get("SYBASE").empty()) {
            string reset = env.Get("RESET_SYBASE");
            if ( !reset.empty() && NStr::StringToBool(reset)) {
                return;
            }
        }
        // ...else try hardcoded paths 
    } catch (const CStringException&) {
        // Conversion error -- try hardcoded paths too
    }

    // User-set or default hardcoded path
    if ( CDir(NCBI_GetSybasePath()).CheckAccess(CDirEntry::fRead) ) {
        env.Set("SYBASE", NCBI_GetSybasePath());
        return;
    }

    // If NCBI_SetSybasePath() was used to set up the Sybase path, and it is
    // not right, then use the very Sybase client against which the code was
    // compiled
    if ( !NStr::Equal(NCBI_GetSybasePath(), NCBI_GetDefaultSybasePath())  &&
         CDir(NCBI_GetDefaultSybasePath()).CheckAccess(CDirEntry::fRead) ) {
        env.Set("SYBASE", NCBI_GetDefaultSybasePath());
    }

    // Else, well, use whatever $SYBASE there is
}


void CDriverContext::x_Recycle(CConnection* conn, bool conn_reusable)
{
    CMutexGuard mg(x_GetCtxMtx());

    TConnPool::iterator it = find(m_InUse.begin(), m_InUse.end(), conn);

    if (it != m_InUse.end()) {
        m_InUse.erase(it);
    }

#ifdef NCBI_THREADS
    bool for_sem = false;
    if ( !m_PoolSemSubject.empty()  &&  conn->PoolName() == m_PoolSemSubject) {
        for_sem = true;
        m_PoolSemSubject.erase();
        m_PoolSemConn = NULL;
        m_PoolSem.TryWait(); // probably redundant, but ensures Post will work
        m_PoolSem.Post();
    }
#endif

    if (conn_reusable  &&  conn->IsOpeningFinished()  &&  conn->IsValid()) {
        if (conn->m_PoolIdleTimeParam.GetSign() != eNegative) {
            CTime now(CTime::eCurrent);
            conn->m_CleanupTime = now + conn->m_PoolIdleTimeParam;
        }
        m_NotInUse.push_back(conn);
#ifdef NCBI_THREADS
        if (for_sem) {
            m_PoolSemConn = conn;
        }
#endif
    } else {
        delete conn;
    }

    CloseOldIdleConns(1);
}

void CDriverContext::CloseUnusedConnections(const string&   srv_name,
                                             const string&   pool_name)
{
    CMutexGuard mg(x_GetCtxMtx());

    TConnPool::value_type con;

    // close all connections first
    NON_CONST_ITERATE(TConnPool, it, m_NotInUse) {
        con = *it;

        if((!srv_name.empty()) && srv_name.compare(con->ServerName())) continue;
        if((!pool_name.empty()) && pool_name.compare(con->PoolName())) continue;

        it = m_NotInUse.erase(it);
        --it;
        delete con;
    }
}

unsigned int CDriverContext::NofConnections(const TSvrRef& svr_ref,
                                            const string& pool_name) const
{
    CMutexGuard mg(x_GetCtxMtx());

    if ((!svr_ref  ||  !svr_ref->IsValid())  &&  pool_name.empty()) {
        return static_cast<unsigned int>(m_InUse.size() + m_NotInUse.size());
    }

    string server;
    Uint4 host = 0;
    Uint2 port = 0;
    if (svr_ref) {
        host = svr_ref->GetHost();
        port = svr_ref->GetPort();
        if (host == 0)
            server = svr_ref->GetName();
    }

    const TConnPool* pools[] = {&m_NotInUse, &m_InUse};
    int n = 0;
    for (size_t i = 0; i < ArraySize(pools); ++i) {
        ITERATE(TConnPool, it, (*pools[i])) {
            TConnPool::value_type con = *it;
            if(!server.empty()) {
                if (server.compare(con->ServerName()))
                    continue;
            }
            else if (host != 0) {
                if (host != con->Host()  ||  port != con->Port())
                    continue;
            }
            if((!pool_name.empty()) && pool_name.compare(con->PoolName())) continue;
            ++n;
        }
    }

    return n;
}

unsigned int CDriverContext::NofConnections(const string& srv_name,
                                            const string& pool_name) const
{
    TSvrRef svr_ref(new CDBServer(srv_name, 0, 0));
    return NofConnections(svr_ref, pool_name);
}

CDB_Connection* CDriverContext::MakeCDBConnection(CConnection* connection)
{
    connection->m_CleanupTime.Clear();
    m_InUse.push_back(connection);

    return new CDB_Connection(connection);
}

class CDBConnParams_Unpooled : public CDBConnParamsDelegate
{
public:
    CDBConnParams_Unpooled(const CDBConnParams& other)
        : CDBConnParamsDelegate(other)
        { }

    string GetParam(const string& key) const
        {
            if (key == "is_pooled") {
                return "false";
            } else {
                return CDBConnParamsDelegate::GetParam(key);
            }
        }
};
    
CDB_Connection*
CDriverContext::MakePooledConnection(const CDBConnParams& params)
{
    if (params.GetParam("is_pooled") == "true") {
        CMutexGuard mg(x_GetCtxMtx());

        string pool_name(params.GetParam("pool_name"));
        if (!m_NotInUse.empty()) {
            if (!pool_name.empty()) {
                // use a pool name
                ERASE_ITERATE(TConnPool, it, m_NotInUse) {
                    CConnection* t_con(*it);

                    // There is no pool name check here. We assume that a connection
                    // pool contains connections with appropriate server names only.
                    if (pool_name == t_con->PoolName()) {
                        it = m_NotInUse.erase(it);
                        if(t_con->Refresh()) {
                            /* Future development ...
                            if (!params.GetDatabaseName().empty()) {
                                return SetDatabase(MakeCDBConnection(t_con), params);
                            } else {
                                return MakeCDBConnection(t_con);
                            }
                            */
                            
                            return MakeCDBConnection(t_con);
                        }
                        else {
                            delete t_con;
                        }
                    }
                }
            }
            else {

                if ( params.GetServerName().empty() ) {
                    return NULL;
                }

                // try to use a server name
                ERASE_ITERATE(TConnPool, it, m_NotInUse) {
                    CConnection* t_con(*it);

                    if (params.GetServerName() == t_con->ServerName()) {
                        it = m_NotInUse.erase(it);
                        if (t_con->Refresh()) {
                            /* Future development ...
                            if (!params.GetDatabaseName().empty()) {
                                return SetDatabase(MakeCDBConnection(t_con), params);
                            } else {
                                return MakeCDBConnection(t_con);
                            }
                            */

                            return MakeCDBConnection(t_con);
                        }
                        else {
                            delete t_con;
                        }
                    }
                }
            }
        }

        // Connection should be created, but we can have limit on number of
        // connections in the pool.
        string pool_max_str(params.GetParam("pool_maxsize"));
        if (!pool_max_str.empty()  &&  pool_max_str != "default") {
            int pool_max = NStr::StringToInt(pool_max_str);
            if (pool_max != 0) {
                int total_cnt = 0;
                ITERATE(TConnPool, it, m_InUse) {
                    CConnection* t_con(*it);
                    if (pool_name == t_con->PoolName())
                        ++total_cnt;
                }
                if (total_cnt >= pool_max) {
#ifdef NCBI_THREADS
                    string timeout_str(params.GetParam("pool_wait_time"));
                    double timeout_val = 0.0;
                    if ( !timeout_str.empty()  &&  timeout_str != "default") {
                        timeout_val = NStr::StringToDouble(timeout_str);
                    }
                    CTimeout timeout(timeout_val);
                    m_PoolSemSubject = pool_name;
                    mg.Release();
                    if (m_PoolSem.TryWait(timeout)) {
                        mg.Guard(x_GetCtxMtx());
                        CConnection* t_con = NULL;
                        NON_CONST_REVERSE_ITERATE(TConnPool, it, m_NotInUse) {
                            if (*it == m_PoolSemConn) {
                                t_con = m_PoolSemConn;
                                m_PoolSemConn = NULL;
                                m_NotInUse.erase((++it).base());
                            }
                        }
                        if (t_con != NULL) {
                            return MakeCDBConnection(t_con);
                        }
                    } else
#endif
                    if (params.GetParam("pool_allow_temp_overflow")
                        == "true") {
                        return MakePooledConnection
                            (CDBConnParams_Unpooled(params));
                    } else {
                        return NULL;
                    }
                }
            }
        }
    }

    if (params.GetParam("do_not_connect") == "true") {
        return NULL;
    }

    // Precondition check.
    if (params.GetServerName().empty() ||
        (!TDbapi_CanUseKerberos::GetDefault()
         &&  (params.GetUserName().empty()
              ||  params.GetPassword().empty())))
    {
        string err_msg("Insufficient info/credentials to connect.");

        if (params.GetServerName().empty()) {
            err_msg += " Server name has not been set.";
        }
        if (params.GetUserName().empty()) {
            err_msg += " User name has not been set.";
        }
        if (params.GetPassword().empty()) {
            err_msg += " Password has not been set.";
        }

        DATABASE_DRIVER_ERROR( err_msg, 200010 );
    }

    CConnection* t_con = MakeIConnection(params);

    return MakeCDBConnection(t_con);
}

void
CDriverContext::CloseAllConn(void)
{
    // close all connections first
    ITERATE(TConnPool, it, m_NotInUse) {
        delete *it;
    }
    m_NotInUse.clear();

    ITERATE(TConnPool, it, m_InUse) {
        (*it)->Close();
    }
}

void
CDriverContext::DeleteAllConn(void)
{
    // close all connections first
    ITERATE(TConnPool, it, m_NotInUse) {
        delete *it;
    }
    m_NotInUse.clear();

    ITERATE(TConnPool, it, m_InUse) {
        delete *it;
    }
    m_InUse.clear();
}


class CMakeConnActualParams: public CDBConnParamsBase
{
public:
    CMakeConnActualParams(const CDBConnParams& other)
        : m_Other(other)
    {
        // Override what is set in CDBConnParamsBase constructor
        SetParam("secure_login", kEmptyStr);
        SetParam("is_pooled", kEmptyStr);
        SetParam("do_not_connect", kEmptyStr);
    }

    ~CMakeConnActualParams(void)
    {}

    virtual Uint4 GetProtocolVersion(void) const
    {
        return m_Other.GetProtocolVersion();
    }

    virtual EEncoding GetEncoding(void) const
    {
        return m_Other.GetEncoding();
    }

    virtual string GetServerName(void) const
    {
        return CDBConnParamsBase::GetServerName();
    }

    virtual string GetDatabaseName(void) const
    {
        return CDBConnParamsBase::GetDatabaseName();
    }

    virtual string GetUserName(void) const
    {
        return CDBConnParamsBase::GetUserName();
    }

    virtual string GetPassword(void) const
    {
        return CDBConnParamsBase::GetPassword();
    }

    virtual EServerType GetServerType(void) const
    {
        return m_Other.GetServerType();
    }

    virtual Uint4 GetHost(void) const
    {
        return m_Other.GetHost();
    }

    virtual Uint2 GetPort(void) const
    {
        return m_Other.GetPort();
    }

    virtual CRef<IConnValidator> GetConnValidator(void) const
    {
        return m_Other.GetConnValidator();
    }

    virtual string GetParam(const string& key) const
    {
        string result(CDBConnParamsBase::GetParam(key));
        if (result.empty())
            return m_Other.GetParam(key);
        else
            return result;
    }

    using CDBConnParamsBase::SetServerName;
    using CDBConnParamsBase::SetUserName;
    using CDBConnParamsBase::SetDatabaseName;
    using CDBConnParamsBase::SetPassword;
    using CDBConnParamsBase::SetParam;

private:
    const CDBConnParams& m_Other;
};


struct SLoginData
{
    string server_name;
    string user_name;
    string db_name;
    string password;

    SLoginData(const string& sn, const string& un,
               const string& dn, const string& pass)
        : server_name(sn), user_name(un), db_name(dn), password(pass)
    {}

    bool operator< (const SLoginData& right) const
    {
        if (server_name != right.server_name)
            return server_name < right.server_name;
        else if (user_name != right.user_name)
            return user_name < right.user_name;
        else if (db_name != right.db_name)
            return db_name < right.db_name;
        else
            return password < right.password;
    }
};


static void
s_TransformLoginData(string& server_name, string& user_name,
                     string& db_name,     string& password)
{
    if (!TDbapi_ConnUseEncryptData::GetDefault())
        return;

    string app_name = CNcbiApplication::Instance()->GetProgramDisplayName();
    set<SLoginData> visited;
    CNcbiResourceInfoFile res_file(CNcbiResourceInfoFile::GetDefaultFileName());

    visited.insert(SLoginData(server_name, user_name, db_name, password));
    for (;;) {
        string res_name = app_name;
        if (!user_name.empty()) {
            res_name += "/";
            res_name += user_name;
        }
        if (!server_name.empty()) {
            res_name += "@";
            res_name += server_name;
        }
        if (!db_name.empty()) {
            res_name += ":";
            res_name += db_name;
        }
        const CNcbiResourceInfo& info
                               = res_file.GetResourceInfo(res_name, password);
        if (!info)
            break;

        password = info.GetValue();
        typedef CNcbiResourceInfo::TExtraValuesMap  TExtraMap;
        typedef TExtraMap::const_iterator           TExtraMapIt;
        const TExtraMap& extra = info.GetExtraValues().GetPairs();

        TExtraMapIt it = extra.find("server");
        if (it != extra.end())
            server_name = it->second;
        it = extra.find("username");
        if (it != extra.end())
            user_name = it->second;
        it = extra.find("database");
        if (it != extra.end())
            db_name = it->second;

        if (!visited.insert(
                SLoginData(server_name, user_name, db_name, password)).second)
        {
            DATABASE_DRIVER_ERROR(
                   "Circular dependency inside resources info file.", 100012);
        }
    }
}


void
SDBConfParams::Clear(void)
{
    flags = 0;
    server.clear();
    port.clear();
    database.clear();
    username.clear();
    password.clear();
    login_timeout.clear();
    io_timeout.clear();
    single_server.clear();
    is_pooled.clear();
    pool_name.clear();
    pool_maxsize.clear();
    pool_idle_time.clear();
    pool_wait_time.clear();
    pool_allow_temp_overflow.clear();
    args.clear();
}

void
CDriverContext::ReadDBConfParams(const string&  service_name,
                                 SDBConfParams* params)
{
    params->Clear();
    if (service_name.empty())
        return;

    CNcbiApplication* app = CNcbiApplication::Instance();
    if (!app)
        return;

    const IRegistry& reg = app->GetConfig();
    string section_name(service_name);
    section_name.append(1, '.');
    section_name.append("dbservice");
    if (!reg.HasEntry(section_name, kEmptyStr))
        return;

    if (reg.HasEntry(section_name, "service", IRegistry::fCountCleared)) {
        params->flags += SDBConfParams::fServerSet;
        params->server = reg.Get(section_name, "service");
    }
    if (reg.HasEntry(section_name, "port", IRegistry::fCountCleared)) {
        params->flags += SDBConfParams::fPortSet;
        params->port = reg.Get(section_name, "port");
    }
    if (reg.HasEntry(section_name, "database", IRegistry::fCountCleared)) {
        params->flags += SDBConfParams::fDatabaseSet;
        params->database = reg.Get(section_name, "database");
    }
    if (reg.HasEntry(section_name, "username", IRegistry::fCountCleared)) {
        params->flags += SDBConfParams::fUsernameSet;
        params->username = reg.Get(section_name, "username");
    }
    if (reg.HasEntry(section_name, "password", IRegistry::fCountCleared)) {
        params->flags += SDBConfParams::fPasswordSet;
        params->password = reg.Get(section_name, "password");
    }
    if (reg.HasEntry(section_name, "password_file",
                     IRegistry::fCountCleared)) {
        // password and password_file are mutually exclusive, but only SDBAPI
        // actually honors the latter, so it will take care of throwing
        // exceptions as necessary.
        params->flags += SDBConfParams::fPasswordFileSet;
        params->password_file = reg.Get(section_name, "password_file");
    }
    if (reg.HasEntry(section_name, "password_key", IRegistry::fCountCleared)) {
        params->flags += SDBConfParams::fPasswordKeySet;
        params->password_key_id = reg.Get(section_name, "password_key");
    }
    if (reg.HasEntry(section_name, "login_timeout", IRegistry::fCountCleared)) {
        params->flags += SDBConfParams::fLoginTimeoutSet;
        params->login_timeout = reg.Get(section_name, "login_timeout");
    }
    if (reg.HasEntry(section_name, "io_timeout", IRegistry::fCountCleared)) {
        params->flags += SDBConfParams::fIOTimeoutSet;
        params->io_timeout = reg.Get(section_name, "io_timeout");
    }
    if (reg.HasEntry(section_name, "cancel_timeout", IRegistry::fCountCleared)) {
        params->flags += SDBConfParams::fCancelTimeoutSet;
        params->cancel_timeout = reg.Get(section_name, "cancel_timeout");
    }
    if (reg.HasEntry(section_name, "exclusive_server", IRegistry::fCountCleared)) {
        params->flags += SDBConfParams::fSingleServerSet;
        params->single_server = reg.Get(section_name, "exclusive_server");
    }
    if (reg.HasEntry(section_name, "use_conn_pool", IRegistry::fCountCleared)) {
        params->flags += SDBConfParams::fIsPooledSet;
        params->is_pooled = reg.Get(section_name, "use_conn_pool");
        params->pool_name = section_name;
        params->pool_name.append(1, '.');
        params->pool_name.append("pool");
    }
    if (reg.HasEntry(section_name, "conn_pool_minsize", IRegistry::fCountCleared)) {
        params->flags += SDBConfParams::fPoolMinSizeSet;
        params->pool_minsize = reg.Get(section_name, "conn_pool_minsize");
    }
    if (reg.HasEntry(section_name, "conn_pool_maxsize", IRegistry::fCountCleared)) {
        params->flags += SDBConfParams::fPoolMaxSizeSet;
        params->pool_maxsize = reg.Get(section_name, "conn_pool_maxsize");
    }
    if (reg.HasEntry(section_name, "conn_pool_idle_time",
                     IRegistry::fCountCleared)) {
        params->flags += SDBConfParams::fPoolIdleTimeSet;
        params->pool_idle_time = reg.Get(section_name, "conn_pool_idle_time");
    }
    if (reg.HasEntry(section_name, "conn_pool_wait_time",
                     IRegistry::fCountCleared)) {
        params->flags += SDBConfParams::fPoolWaitTimeSet;
        params->pool_wait_time = reg.Get(section_name, "conn_pool_wait_time");
    }
    if (reg.HasEntry(section_name, "conn_pool_allow_temp_overflow",
                     IRegistry::fCountCleared)) {
        params->flags += SDBConfParams::fPoolAllowTempSet;
        params->pool_allow_temp_overflow
            = reg.Get(section_name, "conn_pool_allow_temp_overflow");
    }
    if (reg.HasEntry(section_name, "args", IRegistry::fCountCleared)) {
        params->flags += SDBConfParams::fArgsSet;
        params->args = reg.Get(section_name, "args");
    }
}

bool
CDriverContext::SatisfyPoolMinimum(const CDBConnParams& params)
{
    CMutexGuard mg(x_GetCtxMtx());

    string pool_min_str = params.GetParam("pool_minsize");
    if (pool_min_str.empty()  ||  pool_min_str == "default")
        return true;
    int pool_min = NStr::StringToInt(pool_min_str);
    if (pool_min <= 0)
        return true;

    string pool_name = params.GetParam("pool_name");
    int total_cnt = 0;
    ITERATE(TConnPool, it, m_InUse) {
        CConnection* t_con(*it);
        if (t_con->IsReusable()  &&  pool_name == t_con->PoolName()
            &&  t_con->IsValid()  &&  t_con->IsAlive())
        {
            ++total_cnt;
        }
    }
    ITERATE(TConnPool, it, m_NotInUse) {
        CConnection* t_con(*it);
        if (t_con->IsReusable()  &&  pool_name == t_con->PoolName()
            &&  t_con->IsAlive())
        {
            ++total_cnt;
        }
    }
    vector< AutoPtr<CDB_Connection> > conns(pool_min);
    for (int i = total_cnt; i < pool_min; ++i) {
        try {
            conns.push_back(MakeConnection(params));
        }
        catch (CDB_Exception& ex) {
            LOG_POST_X(1, "Error filling connection pool: " << ex);
            return false;
        }
    }
    return true;
}

CDB_Connection* 
CDriverContext::MakeConnection(const CDBConnParams& params)
{
    CMutexGuard mg(x_GetCtxMtx());

    CMakeConnActualParams act_params(params);
    SDBConfParams conf_params;
    conf_params.Clear();
    if (params.GetParam("do_not_read_conf") != "true") {
        ReadDBConfParams(params.GetServerName(), &conf_params);
    }

    int was_timeout = GetTimeout();
    int was_login_timeout = GetLoginTimeout();
    CDB_Connection* t_con = NULL;
    try {
        string server_name = (conf_params.IsServerSet()?   conf_params.server:
                                                           params.GetServerName());
        string user_name   = (conf_params.IsUsernameSet()? conf_params.username:
                                                           params.GetUserName());
        string db_name     = (conf_params.IsDatabaseSet()? conf_params.database:
                                                           params.GetDatabaseName());
        string password    = (conf_params.IsPasswordSet()? conf_params.password:
                                                           params.GetPassword());
        if (conf_params.IsLoginTimeoutSet()) {
            if (conf_params.login_timeout.empty()) {
                SetLoginTimeout(0);
            }
            else {
                SetLoginTimeout(NStr::StringToInt(conf_params.login_timeout));
            }
        }
        else {
            string value(params.GetParam("login_timeout"));
            if (value == "default") {
                SetLoginTimeout(0);
            }
            else if (!value.empty()) {
                SetLoginTimeout(NStr::StringToInt(value));
            }
        }
        if (conf_params.IsIOTimeoutSet()) {
            if (conf_params.io_timeout.empty()) {
                SetTimeout(0);
            }
            else {
                SetTimeout(NStr::StringToInt(conf_params.io_timeout));
            }
        }
        else {
            string value(params.GetParam("io_timeout"));
            if (value == "default") {
                SetTimeout(0);
            }
            else if (!value.empty()) {
                SetTimeout(NStr::StringToInt(value));
            }
        }
        if (conf_params.IsCancelTimeoutSet()) {
            if (conf_params.cancel_timeout.empty()) {
                SetCancelTimeout(0);
            }
            else {
                SetCancelTimeout(NStr::StringToInt(conf_params.cancel_timeout));
            }
        }
        else {
            string value(params.GetParam("cancel_timeout"));
            if (value == "default") {
                SetCancelTimeout(10);
            }
            else if (!value.empty()) {
                SetCancelTimeout(NStr::StringToInt(value));
            }
        }
        if (conf_params.IsSingleServerSet()) {
            if (conf_params.single_server.empty()) {
                act_params.SetParam("single_server", "true");
            }
            else {
                act_params.SetParam("single_server",
                                    NStr::BoolToString(NStr::StringToBool(
                                                conf_params.single_server)));
            }
        }
        else if (params.GetParam("single_server") == "default") {
            act_params.SetParam("single_server", "true");
        }
        if (conf_params.IsPooledSet()) {
            if (conf_params.is_pooled.empty()) {
                act_params.SetParam("is_pooled", "false");
            }
            else {
                act_params.SetParam("is_pooled", 
                                    NStr::BoolToString(NStr::StringToBool(
                                                    conf_params.is_pooled)));
                act_params.SetParam("pool_name", conf_params.pool_name);
            }
        }
        else if (params.GetParam("is_pooled") == "default") {
            act_params.SetParam("is_pooled", "false");
        }
        if (conf_params.IsPoolMinSizeSet())
            act_params.SetParam("pool_minsize", conf_params.pool_minsize);
        else if (params.GetParam("pool_minsize") == "default") {
            act_params.SetParam("pool_minsize", "0");
        }
        if (conf_params.IsPoolMaxSizeSet())
            act_params.SetParam("pool_maxsize", conf_params.pool_maxsize);
        else if (params.GetParam("pool_maxsize") == "default") {
            act_params.SetParam("pool_maxsize", "");
        }
        if (conf_params.IsPoolIdleTimeSet())
            act_params.SetParam("pool_idle_time", conf_params.pool_idle_time);
        else if (params.GetParam("pool_idle_time") == "default") {
            act_params.SetParam("pool_idle_time", "");
        }
        if (conf_params.IsPoolWaitTimeSet())
            act_params.SetParam("pool_wait_time", conf_params.pool_wait_time);
        else if (params.GetParam("pool_wait_time") == "default") {
            act_params.SetParam("pool_wait_time", "0");
        }
        if (conf_params.IsPoolAllowTempOverflowSet()) {
            if (conf_params.pool_allow_temp_overflow.empty()) {
                act_params.SetParam("pool_allow_temp_overflow", "false");
            }
            else {
                act_params.SetParam
                    ("pool_allow_temp_overflow", 
                     NStr::BoolToString(
                         NStr::StringToBool(
                             conf_params.pool_allow_temp_overflow)));
            }
        }
        else if (params.GetParam("pool_allow_temp_overflow") == "default") {
            act_params.SetParam("pool_allow_temp_overflow", "false");
        }

        s_TransformLoginData(server_name, user_name, db_name, password);
        act_params.SetServerName(server_name);
        act_params.SetUserName(user_name);
        act_params.SetDatabaseName(db_name);
        act_params.SetPassword(password);

        CRef<IDBConnectionFactory> factory = CDbapiConnMgr::Instance().GetConnectionFactory();
        t_con = factory->MakeDBConnection(*this, act_params);

        if((!t_con && act_params.GetParam("do_not_connect") == "true")) {
            return NULL;
        }

        if (!t_con) {
            string err;
            err += "Cannot connect to the server '" + act_params.GetServerName();
            err += "' as user '" + act_params.GetUserName() + "'";

            CDB_ClientEx ex(DIAG_COMPILE_INFO, NULL, err, eDiag_Error, 100011);
            CDB_UserHandler::TExceptions* expts = factory->GetExceptions();
            if (expts) {
                NON_CONST_REVERSE_ITERATE(CDB_UserHandler::TExceptions, it, *expts) {
                    ex.AddPrevious(*it);
                }
            }
            throw ex;
        }

        // Set database ...
        t_con->SetDatabaseName(act_params.GetDatabaseName());

    }
    catch (exception&) {
        SetTimeout(was_timeout);
        SetLoginTimeout(was_login_timeout);
        throw;
    }
    SetTimeout(was_timeout);
    SetLoginTimeout(was_login_timeout);

    return t_con;
}

void CDriverContext::CloseConnsForPool(const string& pool_name)
{
    CMutexGuard mg(x_GetCtxMtx());

    ITERATE(TConnPool, it, m_InUse) {
        CConnection* t_con(*it);
        if (t_con->IsReusable()  &&  pool_name == t_con->PoolName()) {
            t_con->Invalidate();
        }
    }
    ERASE_ITERATE(TConnPool, it, m_NotInUse) {
        CConnection* t_con(*it);
        if (t_con->IsReusable()  &&  pool_name == t_con->PoolName()) {
            m_NotInUse.erase(it);
            delete t_con;
        }
    }
}


void CDriverContext::CloseOldIdleConns(unsigned int max_closings,
                                       const string& pool_name)
{
    if (max_closings == 0) {
        return;
    }

    set<string> at_min;
    CTime now(CTime::eCurrent);
    ERASE_ITERATE (TConnPool, it, m_NotInUse) {
        const string& pool_name_2 = (*it)->PoolName();
        if (pool_name_2.empty()
            ||  ( !pool_name.empty()  &&  pool_name != pool_name_2)
            ||  at_min.find(pool_name_2) != at_min.end()
            ||  (*it)->m_CleanupTime.IsEmpty()
            ||  (*it)->m_CleanupTime > now) {
            continue;
        }
        unsigned int n = NofConnections(TSvrRef(), pool_name_2);
        if (n > (*it)->m_PoolMinSize) {
            delete *it;
            m_NotInUse.erase(it);
            if (--max_closings == 0) {
                break;
            }
        } else {
            at_min.insert(pool_name_2);
        }
    }
}


void CDriverContext::DestroyConnImpl(CConnection* impl)
{
    if (impl) {
        impl->ReleaseInterface();
        x_Recycle(impl, impl->IsReusable());
    }
}

void CDriverContext::SetClientCharset(const string& charset)
{
    CMutexGuard mg(x_GetCtxMtx());

    m_ClientCharset = charset;
    m_ClientEncoding = eEncoding_Unknown;

    if (NStr::CompareNocase(charset, "UTF-8") == 0 ||
        NStr::CompareNocase(charset, "UTF8") == 0) {
        m_ClientEncoding = eEncoding_UTF8;
    } else if (NStr::CompareNocase(charset, "Ascii") == 0) {
        m_ClientEncoding = eEncoding_Ascii;
    } else if (NStr::CompareNocase(charset, "ISO8859_1") == 0 ||
               NStr::CompareNocase(charset, "ISO8859-1") == 0
               ) {
        m_ClientEncoding = eEncoding_ISO8859_1;
    } else if (NStr::CompareNocase(charset, "Windows_1252") == 0 ||
               NStr::CompareNocase(charset, "Windows-1252") == 0) {
        m_ClientEncoding = eEncoding_Windows_1252;
    }
}

void CDriverContext::UpdateConnTimeout(void) const
{
    // Do not protect this method. It is already protected.

    ITERATE(TConnPool, it, m_NotInUse) {
        CConnection* t_con = *it;
        if (!t_con) continue;

        t_con->SetTimeout(GetTimeout());
    }

    ITERATE(TConnPool, it, m_InUse) {
        CConnection* t_con = *it;
        if (!t_con) continue;

        t_con->SetTimeout(GetTimeout());
    }
}


void CDriverContext::UpdateConnMaxTextImageSize(void) const
{
    // Do not protect this method. It is protected.

    ITERATE(TConnPool, it, m_NotInUse) {
        CConnection* t_con = *it;
        if (!t_con) continue;

        t_con->SetTextImageSize(GetMaxTextImageSize());
    }

    ITERATE(TConnPool, it, m_InUse) {
        CConnection* t_con = *it;
        if (!t_con) continue;

        t_con->SetTextImageSize(GetMaxTextImageSize());
    }
}


///////////////////////////////////////////////////////////////////////////
CWinSock::CWinSock(void)
{
#if defined(NCBI_OS_MSWIN)
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(1, 1), &wsaData) != 0)
    {
        DATABASE_DRIVER_ERROR( "winsock initialization failed", 200001 );
    }
#endif
}

CWinSock::~CWinSock(void)
{
#if defined(NCBI_OS_MSWIN)
        WSACleanup();
#endif
}

} // namespace impl

END_NCBI_SCOPE


