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
 * Authors: Dmitri Dmitrienko
 *
 * File Description:
 *
 *  Wrapper class around cassandra "C"-API
 *
 */

#include <ncbi_pch.hpp>

#include <objtools/pubseq_gateway/impl/cassandra/cass_driver.hpp>

#include <unistd.h>
#include <objtools/pubseq_gateway/impl/cassandra/IdCassScope.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/cass_exception.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/cass_util.hpp>

#include <atomic>
#include <memory>
#include <sstream>
#include <set>
#include <vector>
#include <string>
#include <limits>
#include <utility>

#include "corelib/ncbitime.hpp"
#include "corelib/ncbistr.hpp"

BEGIN_IDBLOB_SCOPE
USING_NCBI_SCOPE;

#define DISCONNECT_TIMEOUT_MS           5000L
#define CORE_CONNECTIONS_PER_HOST       2
#define MAX_CONNECTIONS_PER_HOST        4
#define IO_THREADS                      32
#define CORE_MAX_REQ_PER_FLUSH          128

const unsigned int CCassQuery::DEFAULT_PAGE_SIZE = 4096;


bool CCassConnection::m_LoggingInitialized = false;
bool CCassConnection::m_LoggingEnabled = false;
EDiagSev CCassConnection::m_LoggingLevel = eDiag_Error;


static void LogCallback(const CassLogMessage * message, void * /*data*/)
{
    switch (message->severity) {
        case CASS_LOG_CRITICAL:
            ERR_POST(Critical << message->message);
            break;
        case CASS_LOG_WARN:
            ERR_POST(Warning << message->message);
            break;
        case CASS_LOG_INFO:
            ERR_POST(Info << message->message);
            break;
        case CASS_LOG_DEBUG:
            ERR_POST(Trace << message->message);
            break;
        case CASS_LOG_TRACE:
            ERR_POST(Trace << message->message);
            break;
        case CASS_LOG_ERROR:
        default:
            ERR_POST(Error << message->message);
    }
}


/** CCassConnection */

CCassConnection::CCassConnection()
    : m_port(0)
    , m_cluster(nullptr)
    , m_session(nullptr)
    , m_ctimeoutms(0)
    , m_qtimeoutms(0)
    , m_last_query_cnt(0)
    , m_loadbalancing(LB_DCAWARE)
    , m_tokenaware(true)
    , m_latencyaware(false)
    , m_numThreadsIo(0)
    , m_numConnPerHost(0)
    , m_maxConnPerHost(0)
    , m_keepalive(0)
    , m_fallback_readconsistency(false)
    , m_FallbackWriteConsistency(0)
    , m_active_statements(0)
{}


static CassLogLevel s_MapFromToolkitSeverity(EDiagSev  severity)
{
    switch (severity) {
        case eDiag_Info:        return CASS_LOG_INFO;
        case eDiag_Warning:     return CASS_LOG_WARN;
        case eDiag_Error:       return CASS_LOG_ERROR;
        case eDiag_Critical:    return CASS_LOG_CRITICAL;
        case eDiag_Fatal:       return CASS_LOG_CRITICAL;
        case eDiag_Trace:       return CASS_LOG_TRACE;
    }
    return CASS_LOG_ERROR;
}


CCassConnection::~CCassConnection()
{
    Close();
}


void CCassConnection::SetLogging(EDiagSev  severity)
{
    cass_log_set_level(s_MapFromToolkitSeverity(severity));
    cass_log_set_callback(LogCallback, nullptr);
    m_LoggingEnabled = true;
    m_LoggingLevel = severity;
    m_LoggingInitialized = true;
}


void CCassConnection::DisableLogging(void)
{
    cass_log_set_level(CASS_LOG_DISABLED);
    m_LoggingEnabled = false;
    m_LoggingInitialized = true;
}


void CCassConnection::UpdateLogging(void)
{
    if (m_LoggingInitialized) {
        if (m_LoggingEnabled)
            SetLogging(m_LoggingLevel);
        else
            DisableLogging();
    }
}

unsigned int CCassConnection::QryTimeout(void) const
{
    return m_qtimeoutms;
}

unsigned int CCassConnection::QryTimeoutMks(void) const
{
    return QryTimeout() * 1000;
}

void CCassConnection::SetRtLimits(unsigned int  numThreadsIo, unsigned int  numConnPerHost,
                 unsigned int  maxConnPerHost)
{
    m_numThreadsIo = numThreadsIo;
    m_numConnPerHost = numConnPerHost;
    m_maxConnPerHost = maxConnPerHost;
}

void CCassConnection::SetKeepAlive(unsigned int keepalive)
{
    m_keepalive = keepalive;
}


shared_ptr<CCassConnection> CCassConnection::Create()
{
    return shared_ptr<CCassConnection>(new CCassConnection());
}


void CCassConnection::SetLoadBalancing(loadbalancing_policy_t  policy)
{
    m_loadbalancing = policy;
}


void CCassConnection::SetTokenAware(bool  value)
{
    m_tokenaware = value;
}


void CCassConnection::SetLatencyAware(bool  value)
{
    m_latencyaware = value;
}

void CCassConnection::SetTimeouts(unsigned int ConnTimeoutMs)
{
    SetTimeouts(ConnTimeoutMs, CASS_DRV_TIMEOUT_MS);
}

void CCassConnection::SetTimeouts(unsigned int ConnTimeoutMs, unsigned int QryTimeoutMs)
{
    if (ConnTimeoutMs == 0 || ConnTimeoutMs > kCassMaxTimeout) {
        ConnTimeoutMs = kCassMaxTimeout;
    }
    if (QryTimeoutMs == 0 || QryTimeoutMs > kCassMaxTimeout) {
        QryTimeoutMs = kCassMaxTimeout;
    }
    m_qtimeoutms = QryTimeoutMs;
    m_ctimeoutms = ConnTimeoutMs;
    if (m_cluster) {
        cass_cluster_set_request_timeout(m_cluster, m_qtimeoutms);
    }
}

void CCassConnection::SetFallBackRdConsistency(bool value)
{
    m_fallback_readconsistency = value;
}

bool CCassConnection::GetFallBackRdConsistency(void) const
{
    return m_fallback_readconsistency;
}

void CCassConnection::SetFallBackWrConsistency(unsigned int  value)
{
    m_FallbackWriteConsistency = value;
}

unsigned int CCassConnection::GetFallBackWrConsistency(void) const
{
    return m_FallbackWriteConsistency;
}


atomic<CassUuidGen*>    CCassConnection::m_CassUuidGen(nullptr);


string CCassConnection::NewTimeUUID()
{
    char buf[CASS_UUID_STRING_LENGTH];
    if (!m_CassUuidGen.load()) {
        CassUuidGen *gen, *nothing = nullptr;
        gen = cass_uuid_gen_new();
        if (!m_CassUuidGen.compare_exchange_weak(nothing, gen)) {
            cass_uuid_gen_free(gen);
        }
    }

    CassUuid uuid;
    cass_uuid_gen_time(static_cast<CassUuidGen*>(m_CassUuidGen.load()), &uuid);
    cass_uuid_string(uuid, buf);
    return string(buf);
}

void CCassConnection::Connect()
{
    if (IsConnected()) {
        NCBI_THROW(CCassandraException, eSeqFailed, "cassandra driver has already been connected");
    }
    if (m_host.empty()) {
        NCBI_THROW(CCassandraException, eSeqFailed, "cassandra host list is empty");
    }

    UpdateLogging();
    m_cluster = cass_cluster_new();

    cass_cluster_set_connect_timeout(m_cluster, m_ctimeoutms);
    cass_cluster_set_contact_points(m_cluster, m_host.c_str());
    if (m_port > 0) {
        cass_cluster_set_port(m_cluster, m_port);
    }
    if (m_qtimeoutms > 0) {
        cass_cluster_set_request_timeout(m_cluster, m_qtimeoutms);
    }

    if (!m_user.empty())
        cass_cluster_set_credentials(m_cluster, m_user.c_str(), m_pwd.c_str());

    if (m_loadbalancing != LB_DCAWARE) {
        if (m_loadbalancing == LB_ROUNDROBIN) {
            cass_cluster_set_load_balance_round_robin(m_cluster);
        }
    }

    cass_cluster_set_token_aware_routing(m_cluster, m_tokenaware ? cass_true : cass_false);
    cass_cluster_set_latency_aware_routing(m_cluster, m_latencyaware ? cass_true : cass_false);

    if (m_numThreadsIo) {
        cass_cluster_set_num_threads_io(m_cluster, m_numThreadsIo);
    } else {
        cass_cluster_set_num_threads_io(m_cluster, IO_THREADS);
    }

    if (m_maxConnPerHost) {
        cass_cluster_set_max_connections_per_host(m_cluster, m_maxConnPerHost);
        cass_cluster_set_pending_requests_low_water_mark(m_cluster, 64 * m_maxConnPerHost);
        cass_cluster_set_pending_requests_high_water_mark(m_cluster, 128 * m_maxConnPerHost);
    }

    if (m_numConnPerHost) {
        cass_cluster_set_core_connections_per_host(m_cluster, m_numConnPerHost);
    }

    if (m_keepalive > 0) {
        cass_cluster_set_tcp_keepalive(m_cluster, cass_true, m_keepalive);
    }

    if (!m_blacklist.empty()) {
        cass_cluster_set_blacklist_filtering(m_cluster, m_blacklist.c_str());
    }

    try {
        Reconnect();
    } catch (CCassandraException const &) {
        Close();
        throw;
    }
}


void CCassConnection::CloseSession()
{
    {
        CSpinGuard guard(m_prepared_mux);
        for(auto & item : m_prepared) {
            if (item.second) {
                cass_prepared_free(item.second);
                item.second = nullptr;
            }
        }
        m_prepared.clear();
    }

    if (m_session) {
        CassFuture * close_future;
        bool free = false;
        close_future = cass_session_close(m_session);
        if (close_future) {
            free = cass_future_wait_timed(close_future, DISCONNECT_TIMEOUT_MS * 1000L);
            cass_future_free(close_future);
        }
        // otherwise we can't free it, let better leak than crash
        if (free) {
            cass_session_free(m_session);
        }
        m_session = nullptr;
    }
}


void CCassConnection::Reconnect()
{
    if (!m_cluster) {
        NCBI_THROW(CCassandraException, eSeqFailed,
                   "invalid sequence of operations, driver is not connected, can't re-connect");
    }

    if (m_session) {
        CloseSession();
    }

    m_session = cass_session_new();
    if (!m_session) {
        RAISE_DB_ERROR(eRsrcFailed, "failed to get cassandra session handle");
    }

    CassFuture * __future = cass_session_connect(m_session, m_cluster);
    if (!__future) {
        RAISE_DB_ERROR(eRsrcFailed, "failed to obtain cassandra connection future");
    }

    unique_ptr<CassFuture, function<void(CassFuture*)>> future(
        __future,
        [](CassFuture* future)
        {
            cass_future_free(future);
        }
    );

    CassError rc = CASS_OK;
    cass_future_wait(future.get());
    rc = cass_future_error_code(future.get());
    if (rc != CASS_OK) {
        RAISE_CASS_CONN_ERROR(future.get(), string(""));
    }

    if (!m_keyspace.empty()) {
        string _sav = m_keyspace;
        m_keyspace.clear();
        SetKeyspace(_sav);
    }
}


void CCassConnection::Close()
{
    CloseSession();
    if (m_cluster) {
        cass_cluster_free(m_cluster);
        m_cluster = nullptr;
    }
}

bool CCassConnection::IsConnected(void)
{
    return (m_cluster || m_session);
}

int64_t CCassConnection::GetActiveStatements(void) const
{
    return m_active_statements;
}

CassMetrics CCassConnection::GetMetrics(void)
{
    CassMetrics metrics;
    if(m_session) {
        cass_session_get_metrics(m_session, &metrics);
    }
    return metrics;
}

void CCassConnection::SetConnProp(
    const string & host,
    const string & user,
    const string & pwd,
    int16_t port)
{
    m_host = host;
    m_user = user;
    m_pwd = pwd;
    m_port = port;
    if (IsConnected()) {
        Close();
    }
}


void CCassConnection::SetKeyspace(const string &  keyspace)
{
    if (m_keyspace != keyspace) {
        if (IsConnected()) {
            shared_ptr<CCassQuery>      query(NewQuery());
            query->SetSQL(string("USE ") + keyspace, 0);
            query->Execute(CASS_CONSISTENCY_LOCAL_QUORUM);
        }
        m_keyspace = keyspace;
    }
}

string CCassConnection::Keyspace(void) const
{
    return m_keyspace;
}

void CCassConnection::SetBlackList(const string & blacklist)
{
    m_blacklist = blacklist;
}

shared_ptr<CCassQuery> CCassConnection::NewQuery()
{
    if (!m_session)
        NCBI_THROW(CCassandraException, eSeqFailed,
                   "invalid sequence of operations, driver is not connected");

    shared_ptr<CCassQuery> rv(new CCassQuery(shared_from_this()));
    rv->SetTimeout(m_qtimeoutms);
    return rv;
}

void CCassConnection::getTokenRanges(TTokenRanges &ranges)
{
    GetTokenRanges(ranges);
}

void CCassConnection::GetTokenRanges(TTokenRanges &ranges)
{
    set<TTokenValue>                    cluster_tokens;
    map<string, vector<TTokenValue> >   peer_tokens;

    shared_ptr<CCassQuery>          query(NewQuery());
    query->SetSQL("select data_center, schema_version, host_id, tokens "
                  "from system.local", 0);
    query->Query(CassConsistency::CASS_CONSISTENCY_LOCAL_ONE, false, false);

    string              host_id,  datacenter, schema;
    vector<string>      tokens;

    query->NextRow();
    query->FieldGetStrValue(0, datacenter);
    query->FieldGetStrValue(1, schema);
    query->FieldGetStrValue(2, host_id);
    query->FieldGetSetValues(3, tokens);
    auto itr = peer_tokens.insert(make_pair(host_id,vector<TTokenValue>())).first;
    for(const auto & item: tokens) {
        TTokenValue value = strtol(item.c_str(), nullptr, 10);
        itr->second.push_back(value);
        cluster_tokens.insert(value);
    }

    ERR_POST(Info << "GET_TOKEN_MAP: Schema version is " << schema);
    ERR_POST(Info << "GET_TOKEN_MAP: datacenter is " << datacenter);
    ERR_POST(Info << "GET_TOKEN_MAP: host_id is " << host_id);

    unsigned int query_host_count = 0;
    unsigned int retries = 0;
    set<string> query_hosts;
    while (retries < 3 && (query_host_count == 0 || peer_tokens.size() == query_host_count)) {
        retries++;
        if (query_host_count != 0) {
            ERR_POST(Info << "GET_TOKEN_MAP: Host_id count is too small. "
                "Retrying system.peers fetch. " << retries);
        }

        query_host_count = 0;
        query_hosts.clear();
        query->SetSQL("select host_id, data_center, schema_version, tokens from system.peers", 0);
        query->Query(CassConsistency::CASS_CONSISTENCY_LOCAL_ONE, false, false);
        while (query->NextRow() == ar_dataready) {
            string peer_host_id, peer_dc, peer_schema;
            vector<string> tokens;
            query->FieldGetStrValue(1, peer_dc);
            query->FieldGetStrValue(2, peer_schema);
            if (datacenter == peer_dc && schema == peer_schema) {
                query->FieldGetStrValue(0, peer_host_id);
                if (query_hosts.find(peer_host_id) == query_hosts.end()) {
                    query_hosts.insert(peer_host_id);
                    query_host_count++;
                }
                query->FieldGetSetValues(3, tokens);
                ERR_POST(Info << "GET_TOKEN_MAP: host is " << peer_host_id);
                ERR_POST(Info << "GET_TOKEN_MAP: tokens " << tokens.size());
                auto itr = peer_tokens.find(peer_host_id);
                if (itr == peer_tokens.end()) {
                    itr = peer_tokens.insert(make_pair(peer_host_id,
                                                       vector<TTokenValue>())).first;
                    for (const auto & item : tokens) {
                        TTokenValue value = strtol(item.c_str(), nullptr, 10);
                        itr->second.push_back(value);
                        cluster_tokens.insert(value);
                    }
                }
            }
        }
        ERR_POST(Info << "GET_TOKEN_MAP: PEERS HOST COUNT IS " << peer_tokens.size());
        ERR_POST(Info << "GET_TOKEN_MAP: QUERY HOST COUNT IS " << query_host_count);
    }

    for(const auto& token : cluster_tokens) {
        ERR_POST(Trace << "GET_TOKEN_MAP: \ttoken " << token);
    }

    ERR_POST(Info << "GET_TOKEN_MAP: tokens size " << cluster_tokens.size());

    ranges.reserve(cluster_tokens.size() + 1);

    TTokenValue lower_bound = numeric_limits<TTokenValue>::min();
    for (int64_t token : cluster_tokens) {
        ERR_POST(Trace << "GET_TOKEN_MAP: token " << token << " : " << token - lower_bound);
        ranges.push_back(make_pair(lower_bound, token));
        lower_bound = token;
    }
    ranges.push_back(make_pair(lower_bound, numeric_limits<TTokenValue>::max()));
}

vector<string> CCassConnection::GetPartitionKeyColumnNames(string const & keyspace, string const & table) const
{
    if (!m_session) {
        NCBI_THROW(CCassandraException, eSeqFailed,
           "invalid sequence of operations, driver is not connected");
    }
    using TMeta = const CassSchemaMeta;
    unique_ptr<TMeta, function<void(TMeta*)>> schema_meta(
        cass_session_get_schema_meta(m_session),
        [](TMeta* meta)->void {
            cass_schema_meta_free(meta);
        }
    );
    if (!schema_meta) {
        NCBI_THROW(CCassandraException, eFatal, "Cluster metadata is not resolved");
    }

    const CassKeyspaceMeta* keyspace_meta = cass_schema_meta_keyspace_by_name_n(
        schema_meta.get(), keyspace.c_str(), keyspace.size()
    );
    if (!keyspace_meta) {
        NCBI_THROW(CCassandraException, eNotFound, "Keyspace not found");
    }

    const CassTableMeta* table_meta = cass_keyspace_meta_table_by_name_n(
        keyspace_meta, table.c_str(), table.size()
    );
    if (!table_meta) {
        NCBI_THROW(CCassandraException, eNotFound, "Table not found");
    }

    size_t partition_key_count = cass_table_meta_partition_key_count(table_meta);
    vector<string> result;
    result.reserve(partition_key_count);
    for (size_t i = 0; i < partition_key_count; ++i) {
        const CassColumnMeta* column_meta = cass_table_meta_partition_key(table_meta, i);
        if (column_meta) {
            const char* name;
            size_t name_length;

            cass_column_meta_name(column_meta, &name, &name_length);
            result.emplace_back(name, name_length);
        }
    }

    return result;
}

const CassPrepared * CCassConnection::Prepare(const string &  sql)
{
    const CassPrepared * rv = nullptr;
#if CASS_PREPARED_Q
    {
        CSpinGuard guard(m_prepared_mux);
        auto it = m_prepared.find(sql);
        if (it != m_prepared.end()) {
            rv = it->second;
            return rv;
        }
    }

    CSpinGuard guard(m_prepared_mux);
    auto it = m_prepared.find(sql);
    if (it == m_prepared.end())
    {
        const char * query = sql.c_str();
        CassFuture * __future = cass_session_prepare(m_session, query);
        if (!__future) {
            RAISE_DB_ERROR(eRsrcFailed, string("failed to obtain cassandra query future"));
        }

        unique_ptr<CassFuture, function<void(CassFuture*)>> future(
            __future,
            [](CassFuture* future)
            {
                cass_future_free(future);
            }
        );

        bool b;
        b = cass_future_wait_timed(future.get(), m_qtimeoutms * 1000L);
        if (!b) {
            RAISE_DB_QRY_TIMEOUT(m_qtimeoutms, 0, string("failed to prepare query \"") + sql + "\"");
        }
        CassError rc = cass_future_error_code(future.get());
        if (rc != CASS_OK) {
            string msg = (rc == CASS_ERROR_SERVER_SYNTAX_ERROR || rc == CASS_ERROR_SERVER_INVALID_QUERY) ?
                            string(", sql: ") + sql : string("");
            RAISE_CASS_QRY_ERROR(future.get(), msg);
        }

        rv = cass_future_get_prepared(future.get());
        if (!rv) {
            RAISE_DB_ERROR(eRsrcFailed, string("failed to obtain prepared handle for sql: ") + sql);
        }
        m_prepared.emplace(sql, rv);
    } else {
        rv = it->second;
        return rv;
    }
#endif
    return rv;
}


void CCassConnection::Perform(
                unsigned int optimeoutms,
                const std::function<bool()> &  PreLoopCB,
                const std::function<void(const CCassandraException&)> &  DbExceptCB,
                const std::function<bool(bool)> &  OpCB)
{
    int err_cnt = 0;
    bool is_repeated = false;
    int64_t op_begin = gettime();

    while (!PreLoopCB || PreLoopCB()) {
        try {
            if (OpCB(is_repeated))
                break;
            err_cnt = 0;
        } catch (const CCassandraException &  e) {
            // log and ignore, app-specific layer is responsible for
            // re-connetion if needed
            if (DbExceptCB)
                DbExceptCB(e);

            if (e.GetErrCode() == CCassandraException::eQueryTimeout && ++err_cnt < 10) {
                ERR_POST(Info << "CAPTURED TIMEOUT: " << e.TimeoutMsg() << ", RESTARTING OP");
            } else if (e.GetErrCode() == CCassandraException::eQueryFailedRestartable) {
                ERR_POST(Info << "CAPTURED RESTARTABLE EXCEPTION: " << e.what() << ", RESTARTING OP");
            } else {
                // timer exceeded (10 times we got timeout and havn't read
                // anyting, or got another error -> try to reconnect
                ERR_POST("2. CAPTURED " << e.what());
                throw;
            }

            int64_t     op_time_ms = (gettime() - op_begin) / 1000;
            if (optimeoutms != 0 && op_time_ms > optimeoutms) {
                throw;
            }
        } catch(...) {
            throw;
        }
        is_repeated = true;
    }
}

/** CCassPrm */

void CCassPrm::Bind(CassStatement *  statement, unsigned int  idx)
{
    CassError       rc = CASS_OK;

    switch (m_type) {
        case CASS_VALUE_TYPE_UNKNOWN:
            if (!IsAssigned())
                NCBI_THROW(CCassandraException, eSeqFailed,
                           "invalid sequence of operations, Param #" +
                           NStr::NumericToString(idx) + " is not assigned");
            rc = cass_statement_bind_null(statement, idx);
            break;
        case CASS_VALUE_TYPE_TINY_INT:
            rc = cass_statement_bind_int8(statement, idx, static_cast<cass_int8_t>(m_simpleval.i8));
            break;
        case CASS_VALUE_TYPE_SMALL_INT:
            rc = cass_statement_bind_int16(statement, idx, static_cast<cass_int16_t>(m_simpleval.i16));
            break;
        case CASS_VALUE_TYPE_INT:
            rc = cass_statement_bind_int32(statement, idx, static_cast<cass_int32_t>(m_simpleval.i32));
            break;
        case CASS_VALUE_TYPE_BIGINT:
            rc = cass_statement_bind_int64(statement, idx, static_cast<cass_int64_t>(m_simpleval.i64));
            break;

        /*
         * @saprykin
         * Removed silent binding null if string is empty. It creates a lot of
         * tombstones in storage. Sometimes if user wants to write empty string
         * it means that we should write empty string.
         * There is a proper method for writing nulls => CCassQuery::BindNull
         */
        case CASS_VALUE_TYPE_VARCHAR:
            rc = cass_statement_bind_string(statement, idx, m_bytes.c_str());
            break;
        case CASS_VALUE_TYPE_BLOB:
            if (m_bytes.size() > 0)
                rc = cass_statement_bind_bytes(
                    statement, idx,
                    reinterpret_cast<const unsigned char*>(m_bytes.c_str()),
                    m_bytes.size());
            else
                rc = cass_statement_bind_null(statement, idx);
            break;
        case CASS_VALUE_TYPE_SET:
        case CASS_VALUE_TYPE_LIST:
        case CASS_VALUE_TYPE_MAP:
            if (m_collection.get())
                rc = cass_statement_bind_collection(statement, idx, m_collection.get());
            else
                rc = cass_statement_bind_null(statement, idx);
            break;
        case CASS_VALUE_TYPE_TUPLE:
            if (m_tuple.get())
                rc = cass_statement_bind_tuple(statement, idx, m_tuple.get());
            else
                rc = cass_statement_bind_null(statement, idx);
            break;
        default:
            RAISE_DB_ERROR(eBindFailed, string("Bind for (") + NStr::NumericToString(static_cast<int>(m_type)) + ") type is not implemented");
    }
    if (rc != CASS_OK)
        RAISE_CASS_ERROR(rc, eBindFailed,
                         string("Bind for (") + NStr::NumericToString(static_cast<int>(m_type)) + ") failed with rc= " +
                         NStr::NumericToString(static_cast<int>(rc)));
}


/**  CCassQuery */

void CCassQuery::SetTimeout()
{
    SetTimeout(CASS_DRV_TIMEOUT_MS);
}

void CCassQuery::SetTimeout(unsigned int t)
{
    m_qtimeoutms = t;
}

unsigned int CCassQuery::Timeout(void) const
{
    return m_qtimeoutms;
}

void CCassQuery::Close(void)
{
    InternalClose(true);
}

CCassQuery::~CCassQuery()
{
    Close();
    m_ondata_data = nullptr;
    m_ondata2_data = nullptr;
    m_onexecute_data = nullptr;
}

void CCassQuery::InternalClose(bool  closebatch)
{
    m_params.clear();
    if (m_future) {
        cass_future_free(m_future);
        --m_connection->m_active_statements;
        m_future = nullptr;
        m_futuretime = 0;
    }
    if (m_statement) {
        cass_statement_free(m_statement);
        m_statement = nullptr;
    }
    if (m_batch && closebatch) {
        cass_batch_free(m_batch);
        m_batch = nullptr;
    }
    if (m_iterator) {
        cass_iterator_free(m_iterator);
        m_iterator = nullptr;
    }
    if (m_result) {
        cass_result_free(m_result);
        m_result = nullptr;
    }
    /*
     * @saprykin
     * Commented out.
     * InternalClose is called from SetSQL and reseting SerialConsistency
     * level adds SetSQL very nasty side effect.
     * Consider not reusing CCassQuery for different types of requests.
     */
    // m_serial_consistency = CASS_CONSISTENCY_ANY;
    m_row = nullptr;
    m_page_start = false;
    m_page_size = 0;
    m_results_expected = false;
    m_async = false;
    m_allow_prepare = CASS_PREPARED_Q;
    m_is_prepared = false;

    if (m_cb_ref) {
        m_cb_ref->Detach();
        m_cb_ref = nullptr;
    }
}

string CCassQuery::GetSQL() const {
    return m_sql;
}

void CCassQuery::SetSQL(const string &  sql, unsigned int  PrmCount)
{
    InternalClose(false);
    m_sql = sql;
    m_params.resize(PrmCount);
}


void CCassQuery::Bind()
{
    if (!m_statement)
        NCBI_THROW(CCassandraException, eSeqFailed,
                   "invalid sequence of operations, query is closed");

    int     cnt = 0;
    for (auto &  it: m_params) {
        it.Bind(m_statement, cnt);
        cnt++;
    }
}


void CCassQuery::BindNull(int iprm)
{
    if (iprm < 0 || (unsigned int)iprm >= m_params.size())
        RAISE_DB_ERROR(eBindFailed, string("Param index is out of range"));
    m_params[iprm].AssignNull();
}

void CCassQuery::BindInt8(int iprm, int8_t value)
{
    if (iprm < 0 || static_cast<size_t>(iprm) >= m_params.size()) {
        RAISE_DB_ERROR(eBindFailed, string("Param index is out of range"));
    }
    m_params[iprm].Assign(value);
}

void CCassQuery::BindInt16(int iprm, int16_t value)
{
    if (iprm < 0 || static_cast<size_t>(iprm) >= m_params.size()) {
        RAISE_DB_ERROR(eBindFailed, string("Param index is out of range"));
    }
    m_params[iprm].Assign(value);
}

void CCassQuery::BindInt32(int iprm, int32_t value)
{
    if (iprm < 0 || (unsigned int)iprm >= m_params.size())
        RAISE_DB_ERROR(eBindFailed, string("Param index is out of range"));
    m_params[iprm].Assign(value);
}


void CCassQuery::BindInt64(int  iprm, int64_t  value)
{
    if (iprm < 0 || (unsigned int)iprm >= m_params.size())
        RAISE_DB_ERROR(eBindFailed, string("Param index is out of range"));
    m_params[iprm].Assign(value);
}


void CCassQuery::BindStr(int  iprm, const string &  value)
{
    if (iprm < 0 || (unsigned int)iprm >= m_params.size())
        RAISE_DB_ERROR(eBindFailed, string("Param index is out of range"));
    m_params[iprm].Assign(value);
}


void CCassQuery::BindStr(int  iprm, const char *  value)
{
    if (iprm < 0 || (unsigned int)iprm >= m_params.size())
        RAISE_DB_ERROR(eBindFailed, string("Param index is out of range"));
    m_params[iprm].Assign(value);
}


void CCassQuery::BindBytes(int  iprm, const unsigned char *  buf, size_t  len)
{
    if (iprm < 0 || (unsigned int)iprm >= m_params.size())
        RAISE_DB_ERROR(eBindFailed, string("Param index is out of range"));
    m_params[iprm].Assign(buf, len);
}


int32_t CCassQuery::ParamAsInt32(int  iprm)
{
    CheckParamAssigned(iprm);
    return m_params[iprm].AsInt32();
}


int64_t CCassQuery::ParamAsInt64(int  iprm)
{
    CheckParamAssigned(iprm);
    return m_params[iprm].AsInt64();
}


string CCassQuery::ParamAsStr(int  iprm) const
{
    CheckParamAssigned(iprm);
    return m_params[iprm].AsString();
}


void CCassQuery::ParamAsStr(int iprm, string & value) const
{
    CheckParamAssigned(iprm);
    m_params[iprm].AsString(value);
}

string CCassQuery::ParamAsStrForDebug(int iprm) const
{
    CheckParamAssigned(iprm);
    return m_params[iprm].AsStringForDebug();
}

CassValueType CCassQuery::ParamType(int iprm) const
{
    CheckParamAssigned(iprm);
    return m_params[iprm].GetType();
}

void CCassQuery::SetEOF(bool value)
{
    if (m_EOF != value) {
        m_EOF = value;
    }
}

void CCassQuery::NewBatch()
{
    if (!m_connection)
        NCBI_THROW(CCassandraException, eSeqFailed,
                   "invalid sequence of operations, DB connection closed");
    if (m_batch)
        NCBI_THROW(CCassandraException, eSeqFailed,
                   "invalid sequence of operations, batch has already been started");
    if (IsActive())
        NCBI_THROW(CCassandraException, eSeqFailed,
                   "invalid sequence of operations, query is active");

    m_batch = cass_batch_new(CASS_BATCH_TYPE_LOGGED);
    if (!m_batch)
        RAISE_DB_ERROR(eRsrcFailed, string("failed to create batch"));
}


void CCassQuery::Query(CassConsistency  c, bool  run_async,
                       bool  allow_prepared, unsigned int  page_size)
{
    if (!m_connection) {
        NCBI_THROW(CCassandraException, eSeqFailed, "invalid sequence of operations, DB connection closed");
    }
    if (m_sql.empty()) {
        NCBI_THROW(CCassandraException, eSeqFailed, "invalid sequence of operations, SQL is not set");
    }
    if (m_batch) {
        NCBI_THROW(CCassandraException, eSeqFailed, "invalid sequence of operations, can't run select in batch mode");
    }
    if (IsActive()) {
        NCBI_THROW(CCassandraException, eSeqFailed, "invalid sequence of operations, Query is active");
    }
    const CassPrepared * prepared = nullptr;
    if (allow_prepared) {
        prepared = m_connection->Prepare(m_sql);
        m_is_prepared = prepared != nullptr;
    }
    if (m_is_prepared) {
        m_statement = cass_prepared_bind(prepared);
    } else {
        m_statement = cass_statement_new(m_sql.c_str(), m_params.size());
    }

    if (!m_statement) {
        RAISE_DB_ERROR(eRsrcFailed, string("failed to create cassandra query"));
    }

    try {
        CassError rc;
        Bind();
        rc = cass_statement_set_consistency(m_statement, c);
        if (rc != CASS_OK)
            RAISE_CASS_ERROR(rc, eQueryFailed,
                             string("Failed to set consistency level ") +
                             NStr::NumericToString(static_cast<int>(c)));
        if (m_serial_consistency != CASS_CONSISTENCY_ANY) {
            rc = cass_statement_set_serial_consistency(m_statement, m_serial_consistency);
            if (rc != CASS_OK)
                RAISE_CASS_ERROR(rc, eQueryFailed,
                    string("Failed to set serial consistency level ") +
                    NStr::NumericToString(static_cast<int>(m_serial_consistency)));
        }
        if (page_size > 0) {
            rc = cass_statement_set_paging_size(m_statement, page_size);
            if (rc != CASS_OK)
                RAISE_CASS_ERROR(rc, eQueryFailed,
                    string("Failed to set page size to ") +
                    NStr::NumericToString(static_cast<int>(page_size)));
        }

        m_page_start = true;
        m_page_size = page_size;
        SetEOF(false);
        m_results_expected = true;
        m_async = run_async;
        if (!m_batch) {
            if (run_async) {
                GetFuture();
            } else {
                Wait(m_qtimeoutms * 1000L);
            }
        }
    } catch(...) {
        Close();
        throw;
    }
}

void CCassQuery::RestartQuery(CassConsistency c)
{
    if (!m_future) {
        NCBI_THROW(CCassandraException, eSeqFailed, "Query is is not in restartable state");
    }
    unsigned int page_size = m_page_size;
    CCassParams params = move(m_params);
    bool async = m_async, allow_prepared = m_is_prepared;
    Close();
    m_params = move(params);
    Query(c, async, allow_prepared, page_size);
}

void CCassQuery::Execute(CassConsistency c, bool run_async, bool allow_prepared)
{
    if (!m_connection)
        NCBI_THROW(CCassandraException, eSeqFailed,
                   "invalid sequence of operations, DB connection closed");
    if (m_sql.empty())
        NCBI_THROW(CCassandraException, eSeqFailed,
                   "invalid sequence of operations, SQL is not set");
    if (m_row != nullptr || m_statement != nullptr)
        NCBI_THROW(CCassandraException, eSeqFailed,
                   "invalid sequence of operations, Query is active");

    const CassPrepared *    prepared = nullptr;
    if (allow_prepared) {
        prepared = m_connection->Prepare(m_sql);
        m_is_prepared = prepared != nullptr;
    }

    if (m_is_prepared)
        m_statement = cass_prepared_bind(prepared);
    else
        m_statement = cass_statement_new(m_sql.c_str(), m_params.size());

    if (!m_statement)
        RAISE_DB_ERROR(eRsrcFailed, string("failed to create cassandra query"));

    try {
        CassError       rc;
        Bind();
        rc = cass_statement_set_consistency(m_statement, c);
        if (rc != CASS_OK)
            RAISE_CASS_ERROR(rc, eQueryFailed,
                             string("Failed to set consistency level ") +
                             NStr::NumericToString(static_cast<int>(c)));
        if (m_serial_consistency != CASS_CONSISTENCY_ANY) {
            rc = cass_statement_set_serial_consistency(m_statement,
                                                       m_serial_consistency);
            if (rc != CASS_OK)
                RAISE_CASS_ERROR(rc, eQueryFailed,
                                 string("Failed to set serial consistency level ") +
                                 NStr::NumericToString(static_cast<int>(m_serial_consistency)));
        }

        if (m_batch) {
            CassError   rc = cass_batch_add_statement(m_batch, m_statement);
            if (rc != CASS_OK)
                RAISE_CASS_ERROR(rc, eQueryFailed,
                                 string("Failed to add statement to batch, sql: ") +
                                 m_sql);
            cass_statement_free(m_statement);
            m_statement = nullptr;
        }

        m_page_start = false;
        m_page_size = 0;
        SetEOF(false);
        m_results_expected = false;
        m_async = run_async;

        if (!m_batch) {
            if (run_async) {
                GetFuture();
            } else {
                Wait(0);
            }
        }
    } catch(...) {
        if (m_statement) {
            cass_statement_free(m_statement);
            m_statement = nullptr;
        }
        throw;
    }
}

void CCassQuery::RestartExecute(CassConsistency c)
{
    if (!m_future)
        NCBI_THROW(CCassandraException, eSeqFailed, "Query is is not in restartable state");

    CCassParams params = move(m_params);
    bool async = m_async, allow_prepared = m_is_prepared;

    Close();
    m_params = move(params);
    Execute(c, async, allow_prepared);
}

void CCassQuery::Restart(CassConsistency c)
{
    if (!m_future)
        NCBI_THROW(CCassandraException, eSeqFailed, "Query is is not in restartable state");

    if (m_results_expected)
        RestartQuery(c);
    else
        RestartExecute(c);
}


void CCassQuery::SetSerialConsistency(CassConsistency  c)
{
    m_serial_consistency = c;
}


async_rslt_t  CCassQuery::RunBatch()
{
    async_rslt_t rv = ar_wait;
    if (!m_batch)
        NCBI_THROW(CCassandraException, eSeqFailed,
                   "invalid sequence of operations, batch is not created");

    if (m_async) {
        GetFuture();
    } else {
        rv = Wait(m_qtimeoutms * 1000L);
        if (m_batch) {
            cass_batch_free(m_batch);
            m_batch = nullptr;
        }
    }
    return rv;
}


async_rslt_t CCassQuery::WaitAsync(unsigned int  timeoutmks)
{
    if (!m_async)
        NCBI_THROW(CCassandraException, eSeqFailed,
                   "attempt to wait on query in non-async state");
    return Wait(timeoutmks);
}


bool CCassQuery::IsReady()
{
    GetFuture();
    return cass_future_ready(m_future) == cass_true;
}


void CCassQuery::GetFuture()
{
    if (!m_connection)
        NCBI_THROW(CCassandraException, eSeqFailed,
                   "invalid sequence of operations, DB connection closed");
    if (!IsActive() && !m_batch)
        NCBI_THROW(CCassandraException, eSeqFailed,
                   "invalid sequence of operations, Query is not active");
    if (m_iterator || m_result)
        NCBI_THROW(CCassandraException, eSeqFailed,
                   "invalid sequence of operations, results already obtained");

    if (!m_future) {
        ++m_connection->m_active_statements;
        if (m_batch)
            m_future = cass_session_execute_batch(m_connection->m_session,
                                                  m_batch);
        else
            m_future = cass_session_execute(m_connection->m_session,
                                            m_statement);
        if (!m_future) {
            --m_connection->m_active_statements;
            RAISE_DB_ERROR(eRsrcFailed, string("failed to obtain cassandra query future"));
        }
        m_futuretime = gettime();
        if (m_ondata || m_ondata2 || m_ondata3.lock()) {
            SetupOnDataCallback();
        }
    }
}


async_rslt_t  CCassQuery::Wait(unsigned int  timeoutmks)
{
    if (m_results_expected && m_result) {
        if (m_async) {
            return ar_dataready;
        }
        NCBI_THROW(CCassandraException, eSeqFailed, "result has already been allocated");
    }

    if (m_EOF && m_results_expected) {
        return ar_done;
    }

    GetFuture();
    bool rv;
    if (timeoutmks != 0) {
        rv = cass_future_wait_timed(m_future, timeoutmks);
    } else if (!m_async) {
        cass_future_wait(m_future);
        rv = true;
    } else {
        rv = cass_future_ready(m_future);
    }

    if (!rv) {
        if (!m_async && timeoutmks > 0) {
            int64_t t = (gettime() - m_futuretime) / 1000L;
            RAISE_DB_QRY_TIMEOUT(t, timeoutmks / 1000L, string("failed to perform query \"") + m_sql + "\"");
        } else {
            return ar_wait;
        }
    } else {
        ProcessFutureResult();
        if ((m_statement || m_batch) && !m_results_expected) {
            // next request was run in m_ondata event handler
            return ar_wait;
        } else if (m_EOF || !m_results_expected) {
            return ar_done;
        } else if (m_result) {
            return ar_dataready;
        } else {
            return ar_wait;
        }
    }
}

void CCassQuery::SetupOnDataCallback()
{
    if (!m_future) {
        NCBI_THROW(CCassandraException, eSeqFailed, "Future is not assigned");
    }
    if (!m_ondata && !m_ondata2 && !m_ondata3.lock()) {
        NCBI_THROW(CCassandraException, eSeqFailed, "invalid sequence of operations, m_ondata is not set");
    }
    if (m_cb_ref) {
        m_cb_ref->Detach();
    }

    m_cb_ref.reset(new CCassQueryCbRef(shared_from_this()));
    m_cb_ref->Attach(m_ondata, m_ondata_data, m_ondata2, m_ondata2_data, m_ondata3.lock());

    CassError rv = cass_future_set_callback(m_future, CCassQueryCbRef::s_OnFutureCb, m_cb_ref.get());
    if (rv != CASS_OK) {
        m_cb_ref->Detach(true);
        m_cb_ref = nullptr;
        NCBI_THROW(CCassandraException, eSeqFailed, "failed to assign future callback");
    }
}

void CCassQuery::ProcessFutureResult()
{
    if (!m_future) {
        NCBI_THROW(CCassandraException, eSeqFailed, "invalid sequence of operations, m_future is not set");
    }
    if (m_iterator || m_result) {
        NCBI_THROW(CCassandraException, eSeqFailed, "invalid sequence of operations, results already obtained");
    }

    CassError rc = cass_future_error_code(m_future);
    if (rc != CASS_OK) {
        if (m_statement) {
            cass_statement_free(m_statement);
            m_statement = nullptr;
        }
        string msg = (rc == CASS_ERROR_SERVER_SYNTAX_ERROR || rc == CASS_ERROR_SERVER_INVALID_QUERY) ?
            string(", sql: ") + m_sql : string("");
        RAISE_CASS_QRY_ERROR(m_future, msg);
    }

    if (m_results_expected) {
        m_result = cass_future_get_result(m_future);
        if (!m_result) {
            RAISE_DB_ERROR(eRsrcFailed, string("failed to obtain cassandra query result"));
        }
        if (m_iterator) {
            NCBI_THROW(CCassandraException, eSeqFailed, "iterator has already been allocated");
        }

        m_iterator = cass_iterator_from_result(m_result);
        if (!m_iterator)
            RAISE_DB_ERROR(eRsrcFailed, string("failed to obtain cassandra query iterator"));
        /*
            we release m_future here for statements that return dataset,
            otherwise we free it in the destructor, keeping m_future not null
            as an indication that we have already waited for query to finish
        */
        cass_future_free(m_future);
        --m_connection->m_active_statements;
        m_future = nullptr;
        if (m_cb_ref) {
            m_cb_ref->Detach();
            m_cb_ref = nullptr;
        }
        m_futuretime = 0;
        m_page_start = false;
    } else {
        if (m_statement) {
            cass_statement_free(m_statement);
            m_statement = nullptr;
        }
        if (m_batch) {
            cass_batch_free(m_batch);
            m_batch = nullptr;
        }
        SetEOF(true);
        if (m_onexecute)
            m_onexecute(*this, m_onexecute_data);
    }
}


async_rslt_t CCassQuery::NextRow()
{
    if (!IsActive()) {
        NCBI_THROW(CCassandraException, eSeqFailed, "invalid sequence of operations, Query is not active");
    }
    while(1) {
        m_row = nullptr;
        if (m_result == nullptr) {
            async_rslt_t wr;
            if (m_async) {
                wr = Wait(0);
            } else {
                wr = Wait(m_qtimeoutms * 1000L);
            }

            if (wr != ar_dataready) {
                return wr;
            }
        }

        if (m_iterator && m_result) {
            bool b = cass_iterator_next(m_iterator);
            if (!b) {
                if (m_page_size > 0) {
                    bool has_more_pages = cass_result_has_more_pages(m_result);
                    if (has_more_pages) {
                        CassError err;
                        if ((err = cass_statement_set_paging_state(m_statement, m_result)) != CASS_OK) {
                            RAISE_CASS_ERROR(err, eFetchFailed, string("failed to retrive next page"));
                        }
                    }

                    cass_iterator_free(m_iterator);
                    m_iterator = nullptr;
                    cass_result_free(m_result);
                    m_result = nullptr;

                    if (!has_more_pages) {
                        SetEOF(true);
                        return ar_done;
                    }
                    // go to above
                    m_page_start = true;
                } else {
                    SetEOF(true);
                    return ar_done;
                }
            } else {
                m_row = cass_iterator_get_row(m_iterator);
                if (!m_row) {
                    RAISE_DB_ERROR(eRsrcFailed, string("failed to obtain cassandra query result row"));
                }
                return ar_dataready;
            }
        } else {
            NCBI_THROW(CCassandraException, eSeqFailed,
                "invalid sequence of operations, "
                "attempt to fetch next row on a closed query");
        }
    }
}


template<>
const CassValue * CCassQuery::GetColumn(int ifld) const
{
    if (!m_row) {
        NCBI_THROW(CCassandraException, eSeqFailed, "query row is not fetched");
    }

    const CassValue * clm = cass_row_get_column(m_row, ifld);
    if (!clm) {
        NCBI_THROW(CCassandraException, eSeqFailed,
            "column is not fetched (index " +
            NStr::NumericToString(ifld) + " beyound the range?)"
        );
    }
    return clm;
}


template<>
const CassValue * CCassQuery::GetColumn(const string & name) const
{
    if (!m_row) {
        NCBI_THROW(CCassandraException, eSeqFailed, "query row is not fetched");
    }

    const CassValue * clm = cass_row_get_column_by_name_n(m_row, name.c_str(), name.size());
    if (!clm) {
        NCBI_THROW(CCassandraException, eSeqFailed, "column " + name + " is not available");
    }
    return clm;
}

template<>
const CassValue * CCassQuery::GetColumn(const char * name) const
{
    if (!m_row) {
        NCBI_THROW(CCassandraException, eSeqFailed, "query row is not fetched");
    }

    const CassValue * clm = cass_row_get_column_by_name(m_row, name);
    if (!clm) {
        NCBI_THROW(CCassandraException, eSeqFailed, "column " + string(name) + " is not available");
    }
    return clm;
}

template<>
string CCassQuery::GetColumnDef(int ifld) const {
    return ToString() + "\ncolumn: " + NStr::NumericToString(ifld);
}

template<>
string CCassQuery::GetColumnDef(const string& name) const {
    return ToString() + "\ncolumn: " + name;
}

template<>
string CCassQuery::GetColumnDef(const char* name) const {
    return ToString() + "\ncolumn: " + string(name);
}

string CCassQuery::ToString() const
{
    string params;
    for (size_t i = 0; i < ParamCount(); ++i) {
        if (!params.empty())
            params.append(", ");
        switch (ParamType(i)) {
            case CASS_VALUE_TYPE_SET:
                params.append("?SET");
                break;
            case CASS_VALUE_TYPE_LIST:
                params.append("?LIST");
                break;
            case CASS_VALUE_TYPE_MAP:
                params.append("?MAP");
                break;
            case CASS_VALUE_TYPE_TUPLE:
                params.append("?TUPLE");
                break;
            default: {
                string prm;
                try {
                    prm = ParamAsStr(i);
                }
                catch(const CCassandraException&) {
                    prm = "???[" + NStr::NumericToString(i) + "]";
                }
                params.append(prm);
            }
        }
    }

    return m_sql.empty() ? "<>" : m_sql + "\nparams: " + params;
}

bool CCassQuery::IsEOF(void) const
{
    return m_EOF;
}

bool CCassQuery::IsAsync(void) const
{
    return m_async;
}

END_IDBLOB_SCOPE
