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
#include <dbapi/driver/dbapi_conn_factory.hpp>
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

inline
static bool s_Matches(CConnection* conn, const string& pool_name,
                      const string& server_name)
{
    if (pool_name.empty()) {
        // There is no server name check here.  We assume that a connection
        // pool contains connections with appropriate server names only.
        return conn->ServerName() == server_name
            ||  conn->GetRequestedServer() == server_name;
    } else {
        return conn->PoolName() == pool_name;
    }
}

///////////////////////////////////////////////////////////////////////////
//  CDriverContext::
//

CDriverContext::CDriverContext(void) :
    m_PoolSem(0, 1),
    m_LoginTimeout(0),
    m_Timeout(0),
    m_CancelTimeout(0),
    m_MaxBlobSize(0),
    m_ClientEncoding(eEncoding_ISO8859_1)
{
    PushCntxMsgHandler    ( &CDB_UserHandler::GetDefault(), eTakeOwnership );
    PushDefConnMsgHandler ( &CDB_UserHandler::GetDefault(), eTakeOwnership );
}

CDriverContext::~CDriverContext(void)
{
    try {
        DeleteAllConn();
    } STD_CATCH_ALL("CDriverContext::DeleteAllConn");
}

void
CDriverContext::SetApplicationName(const string& app_name)
{
    CWriteLockGuard guard(x_GetCtxLock());

    m_AppName = app_name;
}

string
CDriverContext::GetApplicationName(void) const
{
    CReadLockGuard guard(x_GetCtxLock());

    return m_AppName;
}

void
CDriverContext::SetHostName(const string& host_name)
{
    CWriteLockGuard guard(x_GetCtxLock());

    m_HostName = host_name;
}

string
CDriverContext::GetHostName(void) const
{
    CReadLockGuard guard(x_GetCtxLock());

    return m_HostName;
}

unsigned int CDriverContext::GetLoginTimeout(void) const 
{ 
    CReadLockGuard guard(x_GetCtxLock());

    return m_LoginTimeout; 
}

bool CDriverContext::SetLoginTimeout (unsigned int nof_secs)
{
    CWriteLockGuard guard(x_GetCtxLock());

    m_LoginTimeout = nof_secs;

    return true;
}

unsigned int CDriverContext::GetTimeout(void) const 
{ 
    CReadLockGuard guard(x_GetCtxLock());

    return m_Timeout; 
}

bool CDriverContext::SetTimeout(unsigned int nof_secs)
{
    bool success = true;
    CWriteLockGuard guard(x_GetCtxLock());

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
    CReadLockGuard guard(x_GetCtxLock());

    return m_CancelTimeout;
}

bool CDriverContext::SetCancelTimeout(unsigned int nof_secs)
{
    bool success = true;
    CWriteLockGuard guard(x_GetCtxLock());

    try {
        m_CancelTimeout = nof_secs;
    } catch (...) {
        success = false;
    }

    return success;
}

bool CDriverContext::SetMaxBlobSize(size_t nof_bytes)
{
    CWriteLockGuard guard(x_GetCtxLock());

    m_MaxBlobSize = nof_bytes;

    UpdateConnMaxBlobSize();

    return true;
}

void CDriverContext::PushCntxMsgHandler(CDB_UserHandler* h,
                                         EOwnership ownership)
{
    CWriteLockGuard guard(x_GetCtxLock());
    m_CntxHandlers.Push(h, ownership);
}

void CDriverContext::PopCntxMsgHandler(CDB_UserHandler* h)
{
    CWriteLockGuard guard(x_GetCtxLock());
    m_CntxHandlers.Pop(h);
}

void CDriverContext::PushDefConnMsgHandler(CDB_UserHandler* h,
                                            EOwnership ownership)
{
    CWriteLockGuard guard(x_GetCtxLock());
    m_ConnHandlers.Push(h, ownership);
}

void CDriverContext::PopDefConnMsgHandler(CDB_UserHandler* h)
{
    CWriteLockGuard guard(x_GetCtxLock());
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
        if (env.Get("LC_ALL") == "POSIX") {
            // Canonicalize, since locales.dat might list C but not POSIX.
            env.Set("LC_ALL", "C");
        }
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
    CWriteLockGuard guard(m_PoolLock);

    TConnPool::iterator it = find(m_InUse.begin(), m_InUse.end(), conn);

    if (it != m_InUse.end()) {
        m_InUse.erase(it);
    }

#ifdef NCBI_THREADS
    CConnection** recipient = nullptr;
    for (auto &consumer : m_PoolSemConsumers) {
        if ( !consumer.selected
            &&  (consumer.subject_is_pool
                 ? conn->PoolName() == consumer.subject
                 : s_Matches(conn, kEmptyStr, consumer.subject))) {
            consumer.selected = true;
            recipient = &consumer.conn;
            // probably redundant, but ensures Post will work
            m_PoolSem.TryWait();
            m_PoolSem.Post();
            break;
        }
    }
#endif

    bool keep = false;
    if (conn_reusable  &&  conn->IsOpeningFinished()  &&  conn->IsValid()) {
        keep = true;
        // Tentatively; account for temporary overflow below, bearing
        // in mind that that conn is already gone from m_InUse and not
        // yet in m_NotInUse, and as such uncounted.
        if (conn->m_PoolMaxSize <= m_InUse.size() + m_NotInUse.size()) {
            unsigned int n;
            if (conn->m_Pool.empty()) {
                n = NofConnections(conn->m_RequestedServer);
            } else {
                n = NofConnections(kEmptyStr, conn->m_Pool);
            }
            if (n >= conn->m_PoolMaxSize) {
                keep = false;
            }
        }

        if (keep && conn->m_ReuseCount + 1 > conn->m_PoolMaxConnUse) {
            // CXX-4420: limit the number of time the connection is reused
            keep = false;
        }
    }

    if (keep) {
        if (conn->m_PoolIdleTimeParam.GetSign() != eNegative) {
            CTime now(CTime::eCurrent);
            conn->m_CleanupTime = now + conn->m_PoolIdleTimeParam;
        }
        m_NotInUse.push_back(conn);
#ifdef NCBI_THREADS
        if (recipient != nullptr) {
            *recipient = conn;
        }
#endif
    } else {
        x_AdjustCounts(conn, -1);
        delete conn;
    }

    CloseOldIdleConns(1);
}


void CDriverContext::x_AdjustCounts(const CConnection* conn, int delta)
{
    if (delta != 0  &&  conn->IsReusable()) {
        CWriteLockGuard guard(m_PoolLock);
        string server_name = conn->GetServerName();
        if (conn->Host() != 0  &&  server_name.find('@') == NPOS) {
            server_name += '@' + ConvertN2A(conn->Host());
            if (conn->Port() != 0) {
                server_name += ':' + NStr::NumericToString(conn->Port());
            }
        }
        _DEBUG_ARG(unsigned int pool_count =)
        m_CountsByPool[conn->PoolName()][server_name] += delta;
        _DEBUG_ARG(unsigned int service_count =)
        m_CountsByService[conn->GetRequestedServer()][server_name] += delta;
        _TRACE(server_name << " count += " << delta << " for pool "
               << conn->PoolName() << " (" << pool_count << ") and service "
               << conn->GetRequestedServer() << " (" << service_count << ')');
    }
}


void CDriverContext::x_GetCounts(const TCountsMap& main_map,
                                 const string& name, TCounts* counts) const
{
    CReadLockGuard guard(m_PoolLock);
    auto it = main_map.find(name);
    if (it == main_map.end()) {
        counts->clear();
    } else {
        *counts = it->second;
    }
}


void CDriverContext::CloseUnusedConnections(const string&   srv_name,
                                            const string&   pool_name,
                                            unsigned int    max_closings)
{
    CWriteLockGuard guard(m_PoolLock);

    TConnPool::value_type con;

    // close all connections first
    ERASE_ITERATE(TConnPool, it, m_NotInUse) {
        con = *it;

        if ( !srv_name.empty()  && srv_name != con->ServerName()
            &&  srv_name != con->GetRequestedServer() )
            continue;
        if((!pool_name.empty()) && pool_name.compare(con->PoolName())) continue;

        it = m_NotInUse.erase(it);
        x_AdjustCounts(con, -1);
        delete con;
        if (--max_closings == 0) {
            break;
        }
    }
}

unsigned int CDriverContext::NofConnections(const TSvrRef& svr_ref,
                                            const string& pool_name) const
{
    CReadLockGuard guard(m_PoolLock);

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
                if (server != con->ServerName()
                    &&  server != con->GetRequestedServer())
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

CDB_Connection* CDriverContext::MakeCDBConnection(CConnection* connection,
                                                  int delta)
{
    connection->m_CleanupTime.Clear();
    CWriteLockGuard guard(m_PoolLock);
    m_InUse.push_back(connection);
    x_AdjustCounts(connection, delta);

    return new CDB_Connection(connection);
}

CDB_Connection*
CDriverContext::MakePooledConnection(const CDBConnParams& params)
{
    if (params.GetParam("is_pooled") == "true") {
        CWriteLockGuard guard(m_PoolLock);

        string pool_name  (params.GetParam("pool_name")),
               server_name(params.GetServerName());

        ERASE_ITERATE(TConnPool, it, m_NotInUse) {
            CConnection* t_con(*it);
            if (s_Matches(t_con, pool_name, server_name)) {
                it = m_NotInUse.erase(it);
                CDB_Connection* dbcon = MakeCDBConnection(t_con, 0);
                if (dbcon->Refresh()) {
                    /* Future development ...
                       if (!params.GetDatabaseName().empty()) {
                           return SetDatabase(dbcon, params);
                       } else {
                           return dbcon;
                       }
                    */
 
                    return dbcon;
                }
                else {
                    x_AdjustCounts(t_con, -1);
                    t_con->m_Reusable = false;
                    delete dbcon;
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
                    if (s_Matches(t_con, pool_name, server_name)) {
                        ++total_cnt;
                    }
                }
                if (total_cnt >= pool_max) {
#ifdef NCBI_THREADS
                    string timeout_str(params.GetParam("pool_wait_time"));
                    double timeout_val = 0.0;
                    if ( !timeout_str.empty()  &&  timeout_str != "default") {
                        timeout_val = NStr::StringToDouble(timeout_str);
                    }
                    CDeadline deadline{CTimeout(timeout_val)};
                    CTempString subject;
                    if (pool_name.empty()) {
                        subject = server_name;
                    } else {
                        subject = pool_name;
                    }
                    auto target = (m_PoolSemConsumers
                                   .emplace(m_PoolSemConsumers.end(),
                                            subject, !pool_name.empty()));
                    guard.Release();
                    bool timed_out = true;
                    while (m_PoolSem.TryWait(deadline.GetRemainingTime())) {
                        guard.Guard(m_PoolLock);
                        if (target->selected) {
                            timed_out = false;
                            for (const auto &it : m_PoolSemConsumers) {
                                if (&it != &*target  &&  it.selected) {
                                    m_PoolSem.TryWait();
                                    m_PoolSem.Post();
                                    break;
                                }
                            }
                        } else {
                            m_PoolSem.TryWait();
                            m_PoolSem.Post();
                            guard.Release();
                            continue;
                        }
                        CConnection* t_con = NULL;
                        NON_CONST_REVERSE_ITERATE(TConnPool, it, m_NotInUse) {
                            if (*it == target->conn) {
                                t_con = target->conn;
                                m_NotInUse.erase((++it).base());
                                break;
                            }
                        }
                        if (t_con != NULL) {
                            m_PoolSemConsumers.erase(target);
                            return MakeCDBConnection(t_con, 0);
                        }
                        break;
                    }
                    m_PoolSemConsumers.erase(target);
                    if (timed_out)
#endif
                    if (params.GetParam("pool_allow_temp_overflow")
                        != "true") {
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
    if (!TDbapi_CanUseKerberos::GetDefault()
        &&  (params.GetUserName().empty()
             ||  params.GetPassword().empty()))
    {
        string err_msg("Insufficient credentials to connect.");

        if (params.GetUserName().empty()) {
            err_msg += " User name has not been set.";
        }
        if (params.GetPassword().empty()) {
            err_msg += " Password has not been set.";
        }

        DATABASE_DRIVER_ERROR( err_msg, 200010 );
    }

    CConnection* t_con = MakeIConnection(params);

    return MakeCDBConnection(t_con, 1);
}

void
CDriverContext::CloseAllConn(void)
{
    CWriteLockGuard guard(m_PoolLock);
    // close all connections first
    ITERATE(TConnPool, it, m_NotInUse) {
        x_AdjustCounts(*it, -1);
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
    CWriteLockGuard guard(m_PoolLock);
    // close all connections first
    ITERATE(TConnPool, it, m_NotInUse) {
        x_AdjustCounts(*it, -1);
        delete *it;
    }
    m_NotInUse.clear();

    ITERATE(TConnPool, it, m_InUse) {
        x_AdjustCounts(*it, -1);
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
    pool_max_conn_use.clear();
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
    if (reg.HasEntry(section_name, "password_key", IRegistry::fCountCleared)) {
        params->flags += SDBConfParams::fPasswordKeySet;
        params->password_key_id = reg.Get(section_name, "password_key");
    }
    if (reg.HasEntry(section_name, "password", IRegistry::fCountCleared)) {
        params->flags += SDBConfParams::fPasswordSet;
        params->password = reg.Get(section_name, "password");
        if (CNcbiEncrypt::IsEncrypted(params->password)) {
            try {
                params->password = CNcbiEncrypt::Decrypt(params->password);
            } NCBI_CATCH("Password decryption for " + service_name);
        } else if (params->password_key_id.empty()) {
            ERR_POST(Warning
                     << "Using unencrypted password for " + service_name);
        }
    }
    if (reg.HasEntry(section_name, "password_file",
                     IRegistry::fCountCleared)) {
        // password and password_file are mutually exclusive, but only SDBAPI
        // actually honors the latter, so it will take care of throwing
        // exceptions as necessary.
        params->flags += SDBConfParams::fPasswordFileSet;
        params->password_file = reg.Get(section_name, "password_file");
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
    if (reg.HasEntry(section_name, "conn_pool_name", IRegistry::fCountCleared)) {
        // params->flags += SDBConfParams::fPoolNameSet;
        params->pool_name = reg.Get(section_name, "conn_pool_name");
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
    if (reg.HasEntry(section_name, "continue_after_raiserror",
                     IRegistry::fCountCleared)) {
        params->flags += SDBConfParams::fContRaiserrorSet;
        params->continue_after_raiserror
            = reg.Get(section_name, "continue_after_raiserror");
    }
    if (reg.HasEntry(section_name, "conn_pool_max_conn_use",
                     IRegistry::fCountCleared)) {
        params->flags += SDBConfParams::fPoolMaxConnUseSet;
        params->pool_max_conn_use
            = reg.Get(section_name, "conn_pool_max_conn_use");
    }
    if (reg.HasEntry(section_name, "args", IRegistry::fCountCleared)) {
        params->flags += SDBConfParams::fArgsSet;
        params->args = reg.Get(section_name, "args");
    }
}

bool
CDriverContext::SatisfyPoolMinimum(const CDBConnParams& params)
{
    CWriteLockGuard guard(m_PoolLock);

    string pool_min_str = params.GetParam("pool_minsize");
    if (pool_min_str.empty()  ||  pool_min_str == "default")
        return true;
    int pool_min = NStr::StringToInt(pool_min_str);
    if (pool_min <= 0)
        return true;

    string pool_name   = params.GetParam("pool_name"),
           server_name = params.GetServerName();
    int total_cnt = 0;
    ITERATE(TConnPool, it, m_InUse) {
        CConnection* t_con(*it);
        if (t_con->IsReusable()  &&  s_Matches(t_con, pool_name, server_name)
            &&  t_con->IsValid()  &&  t_con->IsAlive())
        {
            ++total_cnt;
        }
    }
    ITERATE(TConnPool, it, m_NotInUse) {
        CConnection* t_con(*it);
        if (t_con->IsReusable()  &&  s_Matches(t_con, pool_name, server_name)
            &&  t_con->IsAlive())
        {
            ++total_cnt;
        }
    }
    guard.Release();
    vector< AutoPtr<CDB_Connection> > conns(pool_min);
    for (int i = total_cnt; i < pool_min; ++i) {
        try {
            conns.push_back(MakeConnection(params));
        }
        catch (CDB_Exception& ex) {
            ERR_POST_X(1, "Error filling connection pool: " << ex);
            return false;
        }
    }
    return true;
}

CDB_Connection* 
CDriverContext::MakeConnection(const CDBConnParams& params)
{
    CMakeConnActualParams act_params(params);

    SDBConfParams conf_params;
    conf_params.Clear();
    if (params.GetParam("do_not_read_conf") != "true") {
        ReadDBConfParams(params.GetServerName(), &conf_params);
    }

    unique_ptr<CDB_Connection> t_con;
    {
        string server_name = (conf_params.IsServerSet()?   conf_params.server:
                                                           params.GetServerName());
        string user_name   = (conf_params.IsUsernameSet()? conf_params.username:
                                                           params.GetUserName());
        string db_name     = (conf_params.IsDatabaseSet()? conf_params.database:
                                                           params.GetDatabaseName());
        string password    = (conf_params.IsPasswordSet()? conf_params.password:
                                                           params.GetPassword());
        CRef<IDBConnectionFactory> factory
            = CDbapiConnMgr::Instance().GetConnectionFactory();
        auto cfactory
            = dynamic_cast<const CDBConnectionFactory*>(
                factory.GetPointerOrNull());
        unsigned int timeout;
        if (cfactory != nullptr) {
            timeout = cfactory->GetLoginTimeout();
        } else if (conf_params.IsLoginTimeoutSet()) {
            if (conf_params.login_timeout.empty()) {
                timeout = 0;
            } else {
                NStr::StringToNumeric(conf_params.login_timeout, &timeout);
            }
        } else {
            string value(params.GetParam("login_timeout"));
            if (value == "default") {
                timeout = 0;
            } else if (!value.empty()) {
                NStr::StringToNumeric(value, &timeout);
            } else {
                timeout = GetLoginTimeout();
            }
        }
        if (timeout == 0  &&  cfactory != nullptr) {
            timeout = 30;
        }
        act_params.SetParam("login_timeout", NStr::NumericToString(timeout));

        if (conf_params.IsIOTimeoutSet()) {
            if (conf_params.io_timeout.empty()) {
                timeout = 0;
            } else {
                NStr::StringToNumeric(conf_params.io_timeout, &timeout);
            }
        } else {
            string value(params.GetParam("timeout"));
            if (value == "default") {
                timeout = 0;
            } else if (!value.empty()) {
                NStr::StringToNumeric(value, &timeout);
            } else {
                timeout = GetTimeout();
            }
        }
        if (cfactory != nullptr) {
            auto validation_timeout = cfactory->GetConnectionTimeout();
            if (validation_timeout == 0) {
                validation_timeout = timeout ? timeout : 30;
            }
            act_params.SetParam("timeout",
                                NStr::NumericToString(validation_timeout));
        } else {
            act_params.SetParam("timeout", NStr::NumericToString(timeout));
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
        if (conf_params.IsPoolMaxConnUseSet())
            act_params.SetParam("pool_max_conn_use",
                                conf_params.pool_max_conn_use);
        else if (params.GetParam("pool_max_conn_use") == "default") {
            act_params.SetParam("pool_max_conn_use", "0");
        }
        if (conf_params.IsContinueAfterRaiserrorSet()) {
            if (conf_params.continue_after_raiserror.empty()) {
                act_params.SetParam("continue_after_raiserror", "false");
            }
            else {
                act_params.SetParam
                    ("continue_after_raiserror", 
                     NStr::BoolToString(
                         NStr::StringToBool(
                             conf_params.continue_after_raiserror)));
            }
        }
        else if (params.GetParam("continue_after_raiserror") == "default") {
            act_params.SetParam("continue_after_raiserror", "false");
        }

        s_TransformLoginData(server_name, user_name, db_name, password);
        act_params.SetServerName(server_name);
        act_params.SetUserName(user_name);
        act_params.SetDatabaseName(db_name);
        act_params.SetPassword(password);

        CDB_UserHandler::TExceptions expts;
        t_con.reset(factory->MakeDBConnection(*this, act_params, expts));

        if (t_con.get() == NULL) {
            if (act_params.GetParam("do_not_connect") == "true") {
                return NULL;
            }

            string err;
            err += "Cannot connect to the server '" + act_params.GetServerName();
            err += "' as user '" + act_params.GetUserName() + "'";

            CDB_ClientEx ex(DIAG_COMPILE_INFO, NULL, err, eDiag_Error, 100011);
            ex.SetRetriable(eRetriable_No);
            NON_CONST_REVERSE_ITERATE(CDB_UserHandler::TExceptions, it, expts)
            {
                ex.AddPrevious(*it);
            }
            throw ex;
        }

        t_con->SetTimeout(timeout);
        // Set database ...
        t_con->SetDatabaseName(act_params.GetDatabaseName());

    }

    return t_con.release();
}

size_t CDriverContext::CloseConnsForPool(const string& pool_name,
                                         Uint4 keep_host_ip, Uint2 keep_port)
{
    size_t      invalidated_count = 0;
    CWriteLockGuard guard(m_PoolLock);

    ITERATE(TConnPool, it, m_InUse) {
        CConnection* t_con(*it);
        if (t_con->IsReusable()  &&  pool_name == t_con->PoolName()) {
            if (t_con->Host() != keep_host_ip || t_con->Port() != keep_port) {
                t_con->Invalidate();
                ++invalidated_count;
            }
        }
    }
    ERASE_ITERATE(TConnPool, it, m_NotInUse) {
        CConnection* t_con(*it);
        if (t_con->IsReusable()  &&  pool_name == t_con->PoolName()) {
            if (t_con->Host() != keep_host_ip || t_con->Port() != keep_port) {
                m_NotInUse.erase(it);
                x_AdjustCounts(t_con, -1);
                delete t_con;
            }
        }
    }
    return invalidated_count;
}


void CDriverContext::CloseOldIdleConns(unsigned int max_closings,
                                       const string& pool_name)
{
    CWriteLockGuard guard(m_PoolLock);
    if (max_closings == 0) {
        return;
    }

    set<string> at_min_by_pool, at_min_by_server;
    CTime now(CTime::eCurrent);
    ERASE_ITERATE (TConnPool, it, m_NotInUse) {
        const string& pool_name_2 = (*it)->PoolName();
        set<string>& at_min
            = pool_name_2.empty() ? at_min_by_server : at_min_by_pool;
        if (pool_name_2.empty()) {
            if ( !pool_name.empty()
                ||  at_min.find((*it)->GetRequestedServer()) != at_min.end()) {
                continue;
            }
        } else if (( !pool_name.empty()  &&  pool_name != pool_name_2)
                   ||  at_min.find(pool_name_2) != at_min.end()) {
            continue;
        }
        if ((*it)->m_CleanupTime.IsEmpty()  ||  (*it)->m_CleanupTime > now) {
            continue;
        }
        unsigned int n;
        if (pool_name_2.empty()) {
            n = NofConnections((*it)->GetRequestedServer());
        } else {
            n = NofConnections(TSvrRef(), pool_name_2);
        }
        if (n > (*it)->m_PoolMinSize) {
            x_AdjustCounts(*it, -1);
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
    CWriteLockGuard guard(x_GetCtxLock());

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


void CDriverContext::UpdateConnMaxBlobSize(void) const
{
    // Do not protect this method. It is protected.

    ITERATE(TConnPool, it, m_NotInUse) {
        CConnection* t_con = *it;
        if (!t_con) continue;

        t_con->SetBlobSize(GetMaxBlobSize());
    }

    ITERATE(TConnPool, it, m_InUse) {
        CConnection* t_con = *it;
        if (!t_con) continue;

        t_con->SetBlobSize(GetMaxBlobSize());
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


