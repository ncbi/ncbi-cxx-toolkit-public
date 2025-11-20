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
 *  Wrapper classes around cassandra "C"-API
 *
 */

#ifndef OBJTOOLS__PUBSEQ_GATEWAY__IMPL__CASSANDRA__CASS_DRIVER_HPP_
#define OBJTOOLS__PUBSEQ_GATEWAY__IMPL__CASSANDRA__CASS_DRIVER_HPP_

#include <cassandra.h>
#include <corelib/ncbistre.hpp>
#include <corelib/ncbistr.hpp>
#include <corelib/ncbiexpt.hpp>
#include <corelib/ncbimtx.hpp>

#include <atomic>
#include <cassert>
#include <chrono>
#include <functional>
#include <limits>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

#include "IdCassScope.hpp"
#include "cass_exception.hpp"

#include <objtools/pubseq_gateway/impl/cassandra/cass_conv.hpp>

BEGIN_IDBLOB_SCOPE
USING_NCBI_SCOPE;

#define CASS_DRV_TIMEOUT_MS 2000U

typedef enum {
    ar_wait,
    ar_done,
    ar_dataready,
    ASYNC_RSLT_LAST_ENTRY
} async_rslt_t;


typedef enum {
    LB_DCAWARE = 0,     // the default, per CPP driver comments on
                        // cass_cluster_set_load_balance_dc_aware function
    LB_ROUNDROBIN
} loadbalancing_policy_t;


typedef enum {
    dtNull,
    dtUnknown,
    dtBoolean,
    dtInteger,
    dtFloat,
    dtText,
    dtTimestamp,
    dtCounter,
    dtUuid,
    dtBlob,
    dtNetwork,
    dtCollection,
    dtCustom,
    dtDate
} CCassDataType;

using TCassConsistency = CassConsistency;
class CCassConsistency final
{
 public:
    CCassConsistency() = delete;

    static constexpr TCassConsistency kUnknown = CASS_CONSISTENCY_UNKNOWN;
    static constexpr TCassConsistency kAny = CASS_CONSISTENCY_ANY;
    static constexpr TCassConsistency kOne = CASS_CONSISTENCY_ONE;
    static constexpr TCassConsistency kTwo = CASS_CONSISTENCY_TWO;
    static constexpr TCassConsistency kThree = CASS_CONSISTENCY_THREE;
    static constexpr TCassConsistency kQuorum = CASS_CONSISTENCY_QUORUM;
    static constexpr TCassConsistency kAll = CASS_CONSISTENCY_ALL;
    static constexpr TCassConsistency kLocalQuorum = CASS_CONSISTENCY_LOCAL_QUORUM;
    static constexpr TCassConsistency kEachQuorum = CASS_CONSISTENCY_EACH_QUORUM;
    static constexpr TCassConsistency kSerial = CASS_CONSISTENCY_SERIAL;
    static constexpr TCassConsistency kLocalSerial = CASS_CONSISTENCY_LOCAL_SERIAL;
    static constexpr TCassConsistency kLocalOne = CASS_CONSISTENCY_LOCAL_ONE;

private:
    static constexpr array<pair<string_view, TCassConsistency>, 11> sx_ConsistencyNames{
        {
            {string_view{"LOCAL_QUORUM"}, kLocalQuorum},
            {string_view{"LOCAL_ONE"}, kLocalOne},
            {string_view{"EACH_QUORUM"}, kEachQuorum},
            {string_view{"ANY"}, kAny},
            {string_view{"ONE"}, kOne},
            {string_view{"TWO"}, kTwo},
            {string_view{"THREE"}, kThree},
            {string_view{"ALL"}, kAll},
            {string_view{"QUORUM"}, kQuorum},
            {string_view{"SERIAL"}, kSerial},
            {string_view{"LOCAL_SERIAL"}, kLocalSerial},
        }
    };

public:
    static TCassConsistency FromString(string_view value)
    {
        const auto itr = find_if(
            begin(sx_ConsistencyNames), end(sx_ConsistencyNames),
            [&value](const auto &v) { return v.first == value; }
        );
        if (itr != end(sx_ConsistencyNames)) {
            return itr->second;
        }
        return kUnknown;
    }
};

/**
 * @see http://datastax.github.io/cpp-driver/api/struct.CassMetrics/
 */
struct SCassMetrics final
{
    struct {
        chrono::microseconds min{0};
        chrono::microseconds max{0};
        chrono::microseconds mean{0};
        chrono::microseconds stddev{0};
        chrono::microseconds median{0};
        chrono::microseconds percentile_75th{0};
        chrono::microseconds percentile_95th{0};
        chrono::microseconds percentile_98th{0};
        chrono::microseconds percentile_99th{0};
        chrono::microseconds percentile_999th{0};
        double mean_rate{0};
        double one_minute_rate{0};
        double five_minute_rate{0};
        double fifteen_minute_rate{0};
    } requests;

    struct {
        uint64_t total_connections{0};
    } stats;

    struct {
        uint64_t connection_timeouts{0};
        uint64_t request_timeouts{0};
    } errors;
};

class CCassQuery;
class CCassQueryCbRef;
class CCassConnection;
class CCassDataCallbackReceiver;

using TCassQueryOnExecuteCallback = void(*)(CCassQuery&, void *);

struct SCassSizeEstimate {
    int64_t range_start{0};
    int64_t range_end{0};
    int64_t mean_partition_size{0};
    int64_t partitions_count{0};
};

class CCassConnection: public std::enable_shared_from_this<CCassConnection>
{
    // cass_cluster_set_request_timeout takes in ms while
    // cass_future_wait_timed takes in mks
    // to avoid overflow we have to adjust to mks
    // ~35minutes
    static const unsigned int kCassMaxTimeout = numeric_limits<unsigned int>::max() / 1000;

    using TPreparedList = unordered_map<string, const CassPrepared *>;

    friend class CCassQuery;
    friend class CCassConnectionFactory;

    void CloseSession();

protected:
    CCassConnection();
    const CassPrepared * Prepare(const string & sql);

public:

    static constexpr int16_t kCassDefaultPort = 9042;

    using TTokenValue = int64_t;
    using TTokenRanges = vector<pair<TTokenValue, TTokenValue>>;

    CCassConnection(const CCassConnection&) = delete;
    CCassConnection& operator=(const CCassConnection&) = delete;

    static shared_ptr<CCassConnection> Create();
    virtual ~CCassConnection();
    void Connect();
    void Reconnect();
    void Close();

    bool IsConnected();
    int64_t GetActiveStatements() const;
    SCassMetrics GetMetrics();

    /// Deprecated. Use SetConnectionPoint() + SetCredentials()
    NCBI_DEPRECATED void SetConnProp(const string & host, const string & user, const string & pwd, int16_t port = 0);

    /// Set connection point parameters.
    ///
    /// @param hostlist
    ///   Comma separated list of IPs/hostnames used to connect to Cassandra cluster.
    ///   Driver uses one alive host at random.
    ///   It will retry connection attempts if first pick is dead.
    /// @param port
    ///   Port number used to connect to Cassandra native interface.
    void SetConnectionPoint(string const& hostlist, int16_t port = 0);

    /// Set credentials to use connecting to Cassandra cluster.
    /// Empty username means no auth required.
    ///
    /// @param username
    ///   Username to connect to Cassandra cluster
    /// @param password
    ///   Password to connect to Cassandra cluster
    void SetCredentials(string const& username, string const& password);

    /// Get port number this connection uses to access cluster
    int16_t GetPort() const
    {
        return m_port ? m_port : kCassDefaultPort;
    }

    void SetLoadBalancing(loadbalancing_policy_t policy);
    void SetTokenAware(bool value);
    void SetLatencyAware(bool value);

    void SetTimeouts(unsigned int ConnTimeoutMs);
    void SetTimeouts(unsigned int ConnTimeoutMs, unsigned int QryTimeoutMs);
    void SetQueryTimeoutRetry(unsigned int timeout_ms);

    void SetFallBackRdConsistency(bool value);
    bool GetFallBackRdConsistency() const;

    void SetFallBackWrConsistency(unsigned int  value);
    unsigned int GetFallBackWrConsistency() const;

    static void SetLogging(EDiagSev  severity);
    static void DisableLogging();
    static void UpdateLogging();

    unsigned int QryTimeoutRetryMs() const;
    unsigned int QryTimeoutMs() const;
    unsigned int QryTimeoutMks() const;

    int GetMaxRetries() const
    {
        return m_maxretries;
    }

    void SetMaxRetries(int value)
    {
        m_maxretries = value < 0 ? 1 : value;
    }

    void SetRtLimits(unsigned int numThreadsIo, unsigned int numConnPerHost);

    void SetKeepAlive(unsigned int keepalive);

    /// Warning! Not suitable for usage in multi-threaded environment in case of multiple keyspaces.
    void SetKeyspace(const string & keyspace);
    string Keyspace() const;

    /// Add the list of hosts denied to be used. Empty list means cleanup blacklist.
    /// Should be used only before Connect() call or it will be noop after host connection established
    ///
    /// @param hosts
    ///   Comma separated list of host addresses
    void SetBlackList(const string & blacklist);

    shared_ptr<CCassQuery> NewQuery();
    void GetTokenRanges(TTokenRanges &ranges);
    vector<string> GetKeyspaces() const;
    vector<string> GetTables(string const& keyspace) const;
    vector<string> GetColumnNames(string const & keyspace, string const & table) const;
    vector<string> GetPartitionKeyColumnNames(string const & keyspace, string const & table) const;
    vector<string> GetClusteringKeyColumnNames(string const & keyspace, string const & table) const;

    // For multi-datacenter environment picture will be not complete
    // system.size_estimates contains data for PRIMARY range only and other datacenter hosts
    // are most likely not available for CQL (e.g. COLO from Bethesda)
    // So maybe there will be necessary to make second level of estimations based on available data
    vector<SCassSizeEstimate> GetSizeEstimates(string const& datacenter, string const& keyspace, string const& table);
    vector<string> GetLocalPeersAddressList(string const & datacenter, unsigned int timeout = 0);
    string GetDatacenterName();
    static string NewTimeUUID();
    static void Perform(
        unsigned int optimeoutms,
        const std::function<bool()>& PreLoopCB,
        const std::function<void(const CCassandraException&)>& DbExceptCB,
        const std::function<bool(bool)>& OpCB
    );

private:
    string                          m_hostlist;
    int16_t                         m_port;
    string                          m_user;
    string                          m_pwd;
    string                          m_blacklist;
    string                          m_keyspace;
    string                          m_datacenter;
    CassCluster *                   m_cluster;
    CassSession *                   m_session;
    unsigned int                    m_ctimeoutms;
    unsigned int                    m_qtimeoutms;
    unsigned int                    m_qtimeout_retry_ms{0};
    int                             m_maxretries{1};
    unsigned int                    m_last_query_cnt;
    loadbalancing_policy_t          m_loadbalancing;
    bool                            m_tokenaware;
    bool                            m_latencyaware;
    unsigned int                    m_numThreadsIo;
    unsigned int                    m_numConnPerHost;
    unsigned int                    m_keepalive;
    bool                            m_fallback_readconsistency;
    unsigned int                    m_FallbackWriteConsistency;
    static atomic<CassUuidGen*>     m_CassUuidGen;
    CSpinLock                       m_prepared_mux;
    TPreparedList                   m_prepared;
    atomic<int64_t>                 m_active_statements;

    static bool                     m_LoggingInitialized;
    static bool                     m_LoggingEnabled;
    static EDiagSev                 m_LoggingLevel;
};

class CCassPrm
{
    static constexpr size_t kMaxVarcharDebugBytes = 10;
protected:
    CassValueType   m_type;
    bool            m_assigned;

    union simpleval_t {
        int8_t  i8;
        int16_t i16;
        int32_t i32;
        int64_t i64;
        simpleval_t() {
            memset(this, 0, sizeof(union simpleval_t));
        }
    } m_simpleval;

    string          m_bytes;
    unique_ptr<CassCollection, function<void(CassCollection*)> >    m_collection;
    unique_ptr<CassTuple, function<void(CassTuple*)> >              m_tuple;

    void Bind(CassStatement *  statement, unsigned int  idx);

public:
    CCassPrm() :
        m_simpleval(), m_collection(nullptr, nullptr)
    {
        Clear();
    }

    CCassPrm(CCassPrm&&) = default;

    ~CCassPrm()
    {
        Clear();
    }

    void Clear(void)
    {
        m_type = CASS_VALUE_TYPE_UNKNOWN;
        m_assigned = false;
        m_collection = nullptr;
        m_bytes.clear();
    }

    void Assign(int8_t  v)
    {
        m_type = CASS_VALUE_TYPE_TINY_INT;
        m_simpleval.i8 = v;
        m_assigned = true;
    }

    void Assign(int16_t  v)
    {
        m_type = CASS_VALUE_TYPE_SMALL_INT;
        m_simpleval.i16 = v;
        m_assigned = true;
    }

    void Assign(int32_t  v)
    {
        m_type = CASS_VALUE_TYPE_INT;
        m_simpleval.i32 = v;
        m_assigned = true;
    }

    void AssignDate(int64_t v)
    {
        m_type = CASS_VALUE_TYPE_DATE;
        m_simpleval.i64 = v;
        m_assigned = true;
    }

    void Assign(int64_t  v)
    {
        m_type = CASS_VALUE_TYPE_BIGINT;
        m_simpleval.i64 = v;
        m_assigned = true;
    }

    template<typename T, typename = typename enable_if<is_constructible<string, T>::value>::type >
    // string, string&, string&&, char *
    void Assign(T v) {
        m_type = CASS_VALUE_TYPE_VARCHAR;
        m_bytes = string(v);
        m_assigned = true;
    }

    void Assign(const unsigned char *  buf, size_t  len)
    {
        m_type = CASS_VALUE_TYPE_BLOB;
        m_bytes = string(reinterpret_cast<const char *>(buf), len);
        m_assigned = true;
    }

    template<typename K, typename V>
    void Assign(const map<K, V> &  v)
    {
        m_type = CASS_VALUE_TYPE_MAP;
        m_collection = unique_ptr<CassCollection, function<void(CassCollection*)> > (
            cass_collection_new(CASS_COLLECTION_TYPE_MAP, v.size() * 2),
            [](CassCollection* coll) {
                cass_collection_free(coll);
            }
        );

        for (const auto &  it : v) {
            Convert::ValueToCassCollection(it.first, m_collection.get());
            Convert::ValueToCassCollection(it.second, m_collection.get());
        }
        m_assigned = true;
    }

    template<typename I>
    void AssignList(I begin, I end, size_t sz)
    {
        m_type = CASS_VALUE_TYPE_LIST;
        m_collection = unique_ptr<CassCollection, function<void(CassCollection*)> > (
            cass_collection_new(CASS_COLLECTION_TYPE_LIST, sz),
            [](CassCollection* coll) {
                cass_collection_free(coll);
            }
        );

        for (; begin != end; ++begin) {
            Convert::ValueToCassCollection(*begin, m_collection.get());
        }
        m_assigned = true;
    }

    template<typename I>
    void AssignSet(I begin, I end, size_t sz)
    {
        m_type = CASS_VALUE_TYPE_SET;
        m_collection = unique_ptr<CassCollection, function<void(CassCollection*)> > (
            cass_collection_new(CASS_COLLECTION_TYPE_SET, sz),
            [](CassCollection* coll) {
                cass_collection_free(coll);
            }
        );

        for (; begin != end; ++begin) {
            Convert::ValueToCassCollection(*begin, m_collection.get());
        }
        m_assigned = true;
    }

    template<typename I>
    void AssignSet(const set<I>& v) {
        AssignSet<typename set<I>::const_iterator>(v.cbegin(), v.cend(), v.size());
    }

    template<typename ...T>
    void Assign(const tuple<T...>& t)
    {
        m_type = CASS_VALUE_TYPE_TUPLE;
        m_assigned = false;
        m_tuple = unique_ptr<CassTuple, function<void(CassTuple*)> >(
            cass_tuple_new(tuple_size< tuple<T...> >::value),
            [](CassTuple* coll){
                cass_tuple_free(coll);
            }
        );
        Convert::ValueToCassTuple(t, m_tuple.get());
        m_assigned = true;
    }

    void AssignNull(void)
    {
        m_type = CASS_VALUE_TYPE_UNKNOWN;
        m_assigned = true;
    }

    bool IsAssigned(void) const
    {
        return m_assigned;
    }

    CassValueType GetType() const
    {
        return m_type;
    }

    int32_t AsInt8(void) const
    {
        switch (m_type) {
            case CASS_VALUE_TYPE_TINY_INT:
                return m_simpleval.i8;
            case CASS_VALUE_TYPE_VARCHAR: {
                int8_t     i8;
                if (NStr::StringToNumeric(m_bytes, &i8))
                    return i8;
                else
                    RAISE_DB_ERROR(eBindFailed, string("Can't convert Param to int8 (type=") + NStr::NumericToString(static_cast<int>(m_type)) + ")");
            }
            // no break
            default:
                RAISE_DB_ERROR(eBindFailed, string("Can't convert Param to int8 (type=") + NStr::NumericToString(static_cast<int>(m_type)) + ")");
        }
    }

    int32_t AsInt16(void) const
    {
        switch (m_type) {
            case CASS_VALUE_TYPE_TINY_INT:
                return m_simpleval.i8;
            case CASS_VALUE_TYPE_SMALL_INT:
                return m_simpleval.i16;
            case CASS_VALUE_TYPE_VARCHAR: {
                int16_t     i16;
                if (NStr::StringToNumeric(m_bytes, &i16))
                    return i16;
                else
                    RAISE_DB_ERROR(eBindFailed, string("Can't convert Param to int16 (type=") + NStr::NumericToString(static_cast<int>(m_type)) + ")");
            }
            // no break
            default:
                RAISE_DB_ERROR(eBindFailed, string("Can't convert Param to int16 (type=") + NStr::NumericToString(static_cast<int>(m_type)) + ")");
        }
    }

    int32_t AsInt32(void) const
    {
        switch (m_type) {
            case CASS_VALUE_TYPE_TINY_INT:
                return m_simpleval.i8;
            case CASS_VALUE_TYPE_SMALL_INT:
                return m_simpleval.i16;
            case CASS_VALUE_TYPE_INT:
                return m_simpleval.i32;
            case CASS_VALUE_TYPE_BIGINT:
            case CASS_VALUE_TYPE_DATE:
                return m_simpleval.i64;
            case CASS_VALUE_TYPE_VARCHAR: {
                int32_t     i32;
                if (NStr::StringToNumeric(m_bytes, &i32))
                    return i32;
                else
                    RAISE_DB_ERROR(eBindFailed, string("Can't convert Param to int32 (type=") + NStr::NumericToString(static_cast<int>(m_type)) + ")");
            }
            // no break
            default:
                RAISE_DB_ERROR(eBindFailed, string("Can't convert Param to int32 (type=") + NStr::NumericToString(static_cast<int>(m_type)) + ")");
        }
    }

    int64_t AsInt64(void) const
    {
        switch (m_type) {
            case CASS_VALUE_TYPE_TINY_INT:
                return m_simpleval.i8;
            case CASS_VALUE_TYPE_SMALL_INT:
                return m_simpleval.i16;
            case CASS_VALUE_TYPE_INT:
                return m_simpleval.i32;
            case CASS_VALUE_TYPE_BIGINT:
            case CASS_VALUE_TYPE_DATE:
                return m_simpleval.i64;
            case CASS_VALUE_TYPE_VARCHAR: {
                int64_t     i64;
                if (NStr::StringToNumeric(m_bytes, &i64))
                    return i64;
                else
                    RAISE_DB_ERROR(eBindFailed, string("Can't convert Param to int64 (type=") + NStr::NumericToString(static_cast<int>(m_type)) + ")");
            }
            // no break
            default:
                RAISE_DB_ERROR(eBindFailed, string("Can't convert Param to int64 (type=") + NStr::NumericToString(static_cast<int>(m_type)) + ")");
        }
    }

    void AsString(string & value) const
    {
        switch (m_type) {
            case CASS_VALUE_TYPE_TINY_INT:
                value = NStr::NumericToString(m_simpleval.i8);
                break;
            case CASS_VALUE_TYPE_SMALL_INT:
                value = NStr::NumericToString(m_simpleval.i16);
                break;
            case CASS_VALUE_TYPE_INT:
                value = NStr::NumericToString(m_simpleval.i32);
                break;
            case CASS_VALUE_TYPE_BIGINT:
            case CASS_VALUE_TYPE_DATE:
                value = NStr::NumericToString(m_simpleval.i64);
                break;
            case CASS_VALUE_TYPE_VARCHAR:
                value = m_bytes;
                break;
            case CASS_VALUE_TYPE_SET:
            default:
                RAISE_DB_ERROR(eBindFailed, string("Can't convert Param to string (type=") + NStr::NumericToString(static_cast<int>(m_type)) + ")");
        }
    }

    string AsString() const
    {
        string rv;
        AsString(rv);
        return rv;
    }

    string AsStringForDebug() const
    {
        switch (m_type) {
            case CASS_VALUE_TYPE_TINY_INT:
            case CASS_VALUE_TYPE_SMALL_INT:
            case CASS_VALUE_TYPE_INT:
            case CASS_VALUE_TYPE_BIGINT:
            case CASS_VALUE_TYPE_DATE:
                return AsString();
            case CASS_VALUE_TYPE_SET:
                return "set(...)";
            case CASS_VALUE_TYPE_MAP:
                return "map(...)";
            case CASS_VALUE_TYPE_LIST:
                return "list(...)";
            case CASS_VALUE_TYPE_TUPLE:
                return "tuple(...)";
            case CASS_VALUE_TYPE_UNKNOWN:
                return "Null";
            case CASS_VALUE_TYPE_VARCHAR:
                if (m_bytes.size() > kMaxVarcharDebugBytes) {
                    return NStr::Quote(m_bytes.substr(0, kMaxVarcharDebugBytes) + "...", '\'');
                } else {
                    return NStr::Quote(m_bytes, '\'');
                }
            case CASS_VALUE_TYPE_BLOB: {
                stringstream s;
                s << "'0x";
                for (size_t i = 0; i < m_bytes.size() && i < kMaxVarcharDebugBytes; ++i) {
                    s << hex << setfill('0') << setw(2) << static_cast<int16_t>(m_bytes[i]);
                }
                if (m_bytes.size() > kMaxVarcharDebugBytes) {
                    s << "...'";
                } else {
                    s << "'";
                }
                return s.str();
            }
            default:
                return "AsStringForDebugNotImplemented(t:" + to_string(m_type) + ")";
        }
    }

    friend class CCassQuery;
};


class CCassParams: public vector<CCassPrm>
{};

class CCassDataCallbackReceiver {
public:
    virtual ~CCassDataCallbackReceiver() = default;
    virtual void OnData() = 0;
};

class CCassQueryCbRef: public std::enable_shared_from_this<CCassQueryCbRef>
{
public:
    explicit CCassQueryCbRef(shared_ptr<CCassQuery> query)
        : m_query(query)
    {}

    virtual ~CCassQueryCbRef()
    {}

    void Attach(shared_ptr<CCassDataCallbackReceiver> ondata3)
    {
        m_self = shared_from_this();
        m_ondata3 = ondata3;
    }

    void Detach(bool remove_self_ref = false)
    {
        if (remove_self_ref) {
            m_self = nullptr;
        }
    }

    static void s_OnFutureCb(CassFuture*, void* data)
    {
        try {
            shared_ptr<CCassQueryCbRef> self(static_cast<CCassQueryCbRef*>(data)->shared_from_this());
            assert(self->m_self);
            self->m_self = nullptr;
            auto query = self->m_query.lock();
            if (query != nullptr) {
                auto ondata3 = self->m_ondata3.lock();
                if (ondata3) {
                    ondata3->OnData();
                }
            }
        } catch (const CException& e) {
            ERR_POST("CException caught! Message: " << e.GetMsg());
            abort();
        }
        catch (const exception& e) {
            ERR_POST("exception caught! Message: " << e.what());
            abort();
        }
    }

 private:
    weak_ptr<CCassDataCallbackReceiver> m_ondata3;
    weak_ptr<CCassQuery>            m_query;
    shared_ptr<CCassQueryCbRef>     m_self;
};

class CCassQuery: public std::enable_shared_from_this<CCassQuery>
{
private:
    CCassQuery(const CCassQuery&) = delete;
    CCassQuery& operator=(const CCassQuery&) = delete;

    friend class CCassConnection;
    friend class CCassQueryCbRef;

private:
    void CheckParamAssigned(int iprm) const
    {
        if (iprm < 0 || iprm >= static_cast<int64_t>(m_params.size())) {
            RAISE_DB_ERROR(eBindFailed, "Param index #" + to_string(iprm) + " is out of range");
        }
        if (!m_params[iprm].IsAssigned()) {
            RAISE_DB_ERROR(eSeqFailed, "invalid sequence of operations, Param #" + to_string(iprm) + " is not assigned");
        }
    }

    void CheckParamExists(int iprm) const
    {
        if (iprm < 0 || (unsigned int)iprm >= m_params.size()) {
            RAISE_DB_ERROR(eBindFailed, "Param index #" + to_string(iprm) + " is out of range");
        }
    }

    shared_ptr<CCassConnection>     m_connection;
    unsigned int                    m_qtimeoutms;
    bool                            m_use_per_request_timeout{false};
    int64_t                         m_futuretime;
    CassFuture *                    m_future;
    CassBatch *                     m_batch;
    CassStatement *                 m_statement;
    const CassResult *              m_result;
    CassIterator *                  m_iterator;
    const CassRow *                 m_row;
    unsigned int                    m_page_size;
    bool                            m_EOF;
    bool                            m_page_start;
    CCassParams                     m_params;
    string                          m_sql;
    bool                            m_results_expected;
    bool                            m_async;
    bool                            m_allow_prepare;
    bool                            m_is_prepared;
    TCassConsistency                m_serial_consistency;

    shared_ptr<CCassQueryCbRef>     m_cb_ref;

    weak_ptr<CCassDataCallbackReceiver> m_ondata3;

    TCassQueryOnExecuteCallback     m_onexecute;
    void*                           m_onexecute_data;

    string                          m_execution_host;

    async_rslt_t Wait(unsigned int timeoutmks);
    void Bind(void);

    template<typename F>
    const CassValue* GetColumn(F ifld) const
    {
        static_assert(sizeof(F) == 0, "Columns can be accessed by either index or name");
        return CNotImplemented<F>::GetColumn_works_by_name_and_by_index();
    }

    template<typename F>
    string GetColumnDef(F ifld) const
    {
        static_assert(sizeof(F) == 0, "Columns can be accessed by either index or name");
        return CNotImplemented<F>::GetColumn_works_by_name_and_by_index();
    }

    void GetFuture();
    void ProcessFutureResult();
    void SetupOnDataCallback();
    void InternalClose(bool closebatch);
    void SetEOF(bool Value);

    template<class T>
    class CNotImplemented
    {};

protected:
    explicit CCassQuery(const shared_ptr<CCassConnection>& connection) :
        m_connection(connection),
        m_qtimeoutms(0),
        m_futuretime(0),
        m_future(nullptr),
        m_batch(nullptr),
        m_statement(nullptr),
        m_result(nullptr),
        m_iterator(nullptr),
        m_row(nullptr),
        m_page_size(0),
        m_EOF(false),
        m_page_start(false),
        m_results_expected(false),
        m_async(false),
        m_allow_prepare(true),
        m_is_prepared(false),
        m_serial_consistency(CASS_CONSISTENCY_ANY),
        m_onexecute(nullptr),
        m_onexecute_data(nullptr)
    {
        if (!m_connection) {
            RAISE_DB_ERROR(eFatal, "Cassandra connection is not established");
        }
    }

public:
    virtual ~CCassQuery();
    virtual void Close(void);

    void SetTimeout();
    void SetTimeout(unsigned int t);

    unsigned int Timeout(void) const;

    // Switch to use per request timeout. Currently used just for Retry operations
    // @todo Switch to ExecutionProfiles
    void UsePerRequestTimeout(bool value);
    unsigned int GetRequestTimeoutMs() const;

    bool IsReady(void);
    void NewBatch(void);
    async_rslt_t RunBatch();

    void SetSQL(const string& sql, unsigned int PrmCount);

    // Set Cassandra host to execute current query.
    // Useful for schema explorations using system.* tables
    //   without configuring whitelists for connection.
    // Works for SELECT queries only.
    //
    // Empty $hostname turns off filtering
    void SetHost(const string& hostname);

    void Query(TCassConsistency c = CCassConsistency::kLocalQuorum,
               bool run_async = false, bool allow_prepare = true,
               unsigned int page_size = DEFAULT_PAGE_SIZE);

    void RestartQuery(TCassConsistency c = CCassConsistency::kLocalQuorum);

    void Execute(TCassConsistency c = CCassConsistency::kLocalQuorum,
                 bool run_async = false, bool allow_prepare = true);
    void RestartExecute(TCassConsistency c = CCassConsistency::kLocalQuorum);
    void Restart(TCassConsistency c = CCassConsistency::kLocalQuorum);

    void SetSerialConsistency(TCassConsistency c);

    bool IsActive(void) const
    {
        return (m_row != nullptr) || (m_statement != nullptr) || (m_batch != nullptr);
    }

    virtual async_rslt_t WaitAsync(unsigned int timeoutmks);
    string GetSQL() const;
    virtual string ToString(void) const;

    virtual bool IsEOF(void) const;
    virtual bool IsAsync(void) const;

    void BindNull(int iprm);
    void BindInt8(int iprm, int8_t value);
    void BindInt16(int iprm, int16_t value);
    void BindInt32(int iprm, int32_t value);
    void BindInt64(int iprm, int64_t value);
    void BindStr(int iprm, const string& value);
    void BindStr(int iprm, const char* value);
    void BindBytes(int iprm, const unsigned char* buf, size_t len);
    void BindDate(int iprm, int64_t value);

    template<typename K, typename V>
    void BindMap(int iprm, const map<K, V>& value)
    {
        CheckParamExists(iprm);
        m_params[iprm].Assign<K,V>(value);
    }

    template<typename I>
    void BindList(int iprm, I begin, I end, size_t sz)
    {
        CheckParamExists(iprm);
        m_params[iprm].AssignList(begin, end, sz);
    }

    template<typename I>
    void BindSet(int iprm, I begin, I end, size_t sz)
    {
        CheckParamExists(iprm);
        m_params[iprm].AssignSet<I>(begin, end, sz);
    }

    template<typename I>
    void BindSet(int iprm, const set<I>& v)
    {
        BindSet(iprm, v.begin(), v.end(), v.size());
    }

    template<typename ...T>
    void BindTuple(int iprm, const tuple<T...>& value)
    {
        CheckParamExists(iprm);
        m_params[iprm].Assign(value);
    }

    size_t ParamCount(void) const
    {
        return m_params.size();
    }

    CassValueType ParamType(int  iprm) const;
    int32_t ParamAsInt32(int iprm);
    int64_t ParamAsInt64(int iprm);
    string ParamAsStr(int iprm) const;
    void ParamAsStr(int iprm, string& value) const;
    string ParamAsStrForDebug(int iprm) const;

    async_rslt_t NextRow();

    template <typename F = int>
    bool FieldIsNull(F ifld) const
    {
        const CassValue* clm = GetColumn(ifld);
        return !clm || cass_value_is_null(clm);
    }

    template <typename F = int>
    CCassDataType FieldType(F ifld) const
    {
        if (FieldIsNull(ifld)) {
            return dtNull;
        }

        const CassValue* clm = GetColumn(ifld);
        CassValueType type = cass_value_type(clm);
        switch (type) {
            case CASS_VALUE_TYPE_CUSTOM:
                return dtCustom;
            case CASS_VALUE_TYPE_TEXT:
            case CASS_VALUE_TYPE_ASCII:
            case CASS_VALUE_TYPE_VARCHAR:
                return dtText;
            case CASS_VALUE_TYPE_VARINT:
            case CASS_VALUE_TYPE_DECIMAL:
            case CASS_VALUE_TYPE_TINY_INT:
            case CASS_VALUE_TYPE_SMALL_INT:
            case CASS_VALUE_TYPE_INT:
            case CASS_VALUE_TYPE_BIGINT:
                return dtInteger;
            case CASS_VALUE_TYPE_DOUBLE:
            case CASS_VALUE_TYPE_FLOAT:
                return dtFloat;

            case CASS_VALUE_TYPE_BLOB:
                return dtBlob;
            case CASS_VALUE_TYPE_BOOLEAN:
                return dtBoolean;
            case CASS_VALUE_TYPE_TIMESTAMP:
                return dtTimestamp;
            case CASS_VALUE_TYPE_COUNTER:
                return dtCounter;
            case CASS_VALUE_TYPE_UUID:
            case CASS_VALUE_TYPE_TIMEUUID:
                return dtUuid;
            case CASS_VALUE_TYPE_LIST:
            case CASS_VALUE_TYPE_MAP:
            case CASS_VALUE_TYPE_SET:
                return dtCollection;
            case CASS_VALUE_TYPE_INET:
                return dtNetwork;
            case CASS_VALUE_TYPE_DATE:
                return dtDate;
            case CASS_VALUE_TYPE_UNKNOWN:
            default:
                return dtUnknown;
        }
    }

    template <typename F = int>
    bool FieldGetBoolValue(F ifld) const
    {
        bool rv;
        try {
            Convert::CassValueConvert<bool>(GetColumn(ifld), rv);
        }
        catch (CCassandraException& ex) {
            ex.AddToMessage(GetColumnDef(ifld));
            throw;
        }
        return rv;
    }

    template <typename F = int>
    bool FieldGetBoolValue(F ifld, bool _default) const
    {
        bool rv;
        Convert::CassValueConvertDef<bool>(GetColumn(ifld), _default, rv);
        return rv;
    }

    template <typename F = int>
    int8_t FieldGetInt8Value(F ifld) const
    {
        int8_t rv;
        try {
            Convert::CassValueConvert<int8_t>(GetColumn(ifld), rv);
        }
        catch (CCassandraException& ex) {
            ex.AddToMessage(GetColumnDef(ifld));
            throw;
        }
        return rv;
    }

    template <typename F = int>
    int8_t FieldGetInt8Value(F ifld, int8_t _default) const
    {
        int8_t rv;
        Convert::CassValueConvertDef<int8_t>(GetColumn(ifld), _default, rv);
        return rv;
    }

    template <typename F = int>
    int16_t FieldGetInt16Value(F ifld) const
    {
        int16_t rv;
        try {
            Convert::CassValueConvert<int16_t>(GetColumn(ifld), rv);
        }
        catch (CCassandraException& ex) {
            ex.AddToMessage(GetColumnDef(ifld));
            throw;
        }
        return rv;
    }

    template <typename F = int>
    int16_t FieldGetInt16Value(F ifld, int16_t _default) const
    {
        int16_t rv;
        Convert::CassValueConvertDef<int16_t>(GetColumn(ifld), _default, rv);
        return rv;
    }

    template <typename F = int>
    int32_t FieldGetInt32Value(F ifld) const
    {
        int32_t rv;
        try {
            Convert::CassValueConvert<int32_t>(GetColumn(ifld), rv);
        }
        catch (CCassandraException& ex) {
            ex.AddToMessage(GetColumnDef(ifld));
            throw;
        }
        return rv;
    }

    template <typename F = int>
    int32_t FieldGetInt32Value(F ifld, int32_t _default) const
    {
        int32_t rv;
        Convert::CassValueConvertDef<int32_t>(GetColumn(ifld), _default, rv);
        return rv;
    }

    template <typename F = int>
    int64_t FieldGetInt64Value(F ifld) const
    {
        int64_t rv;
        try {
            Convert::CassValueConvert<int64_t>(GetColumn(ifld), rv);
        }
        catch (CCassandraException& ex) {
            ex.AddToMessage(GetColumnDef(ifld));
            throw;
        }
        return rv;
    }

    template <typename F = int>
    int64_t FieldGetInt64Value(F ifld, int64_t _default) const
    {
        int64_t rv;
        Convert::CassValueConvertDef<int64_t>(GetColumn(ifld), _default, rv);
        return rv;
    }

    template <typename F = int>
    double FieldGetFloatValue(F ifld) const
    {
        double rv;
        try {
            Convert::CassValueConvert<double>(GetColumn(ifld), rv);
        }
        catch (CCassandraException& ex) {
            ex.AddToMessage(GetColumnDef(ifld));
            throw;
        }

        return rv;
    }

    template <typename F = int>
    double FieldGetFloatValue(F ifld, double _default) const
    {
        double rv;
        Convert::CassValueConvertDef<double>(GetColumn(ifld), _default, rv);
        return rv;
    }

    template <typename F = int>
    string FieldGetStrValue(F ifld) const
    {
        string rv;
        try {
            Convert::CassValueConvert<string>(GetColumn(ifld), rv);
        }
        catch (CCassandraException& ex) {
            ex.AddToMessage(GetColumnDef(ifld));
            throw;
        }
        return rv;
    }

    template <typename F = int>
    string FieldGetStrValueDef(F ifld, const string& _default) const
    {
        string rv;
        Convert::CassValueConvertDef<string>(GetColumn(ifld), _default, rv);
        return rv;
    }

    template <typename I, typename F = int>
    void FieldGetContainerValue(F ifld, I insert_iterator) const
    {
        using TValueType = typename I::container_type::value_type;
        const CassValue * clm = GetColumn(ifld);
        CassValueType type = cass_value_type(clm);

        switch (type) {
            case CASS_VALUE_TYPE_LIST:
            case CASS_VALUE_TYPE_SET:
            {
                if (!cass_value_is_null(clm)) {
                    unique_ptr<CassIterator, function<void(CassIterator*)> > items_iterator_ptr(
                        cass_iterator_from_collection(clm),
                        [](CassIterator* itr) {
                            cass_iterator_free(itr);
                        }
                    );
                    while (cass_iterator_next(items_iterator_ptr.get())) {
                        const CassValue* value = cass_iterator_get_value(items_iterator_ptr.get());
                        if (!value) {
                            RAISE_DB_ERROR(eSeqFailed, "cass iterator fetch failed");
                        }

                        TValueType v;
                        try {
                            Convert::CassValueConvert(value, v);
                        }
                        catch (CCassandraException& ex) {
                            ex.AddToMessage(GetColumnDef(ifld));
                            throw;
                        }
                        insert_iterator++ = std::move(v);
                    }
                }
            }
            break;
            default:
                if (!cass_value_is_null(clm)) {
                    RAISE_DB_ERROR(eFetchFailed,
                        "Failed to convert Cassandra value to collection: unsupported data type (Cassandra type - " +
                        to_string(static_cast<int>(type)) + ")"
                    );
                }
        }
    }

    template <typename F = int>
    void FieldGetStrValue(F ifld, string & value) const
    {
        value = FieldGetStrValue(ifld);
    }

    template <typename F = int>
    void FieldGetStrValueDef(F ifld, string & value, const string & _default) const
    {
        value = FieldGetStrValueDef(ifld, _default);
    }

    template<typename T, typename F = int>
    T FieldGetTupleValue(F ifld) const
    {
        const CassValue * clm = GetColumn(ifld);
        if (!cass_value_is_null(clm)) {
            T t;
            try {
                Convert::CassValueConvert(clm, t);
            }
            catch (CCassandraException& ex) {
                ex.AddToMessage(GetColumnDef(ifld));
                throw;
            }
            return t;
        }
        return T();
    }

    template<typename T, typename F = int>
    void FieldGetSetValues(F ifld, std::vector<T>& values) const
    {
        const CassValue * clm = GetColumn(ifld);
        CassValueType type = cass_value_type(clm);
        switch (type) {
            case CASS_VALUE_TYPE_SET: {
                unique_ptr<CassIterator, function<void(CassIterator*)> > items_iterator_ptr(
                    cass_iterator_from_collection(clm),
                    [](CassIterator* itr) {
                        cass_iterator_free(itr);
                    }
                );
                while (cass_iterator_next(items_iterator_ptr.get())) {
                    T v;
                    const CassValue* value = cass_iterator_get_value(items_iterator_ptr.get());
                    if (!value) {
                        RAISE_DB_ERROR(eSeqFailed, "cass iterator fetch failed");
                    }
                    try {
                        Convert::CassValueConvert(value, v);
                    }
                    catch (CCassandraException& ex) {
                        ex.AddToMessage(GetColumnDef(ifld));
                        throw;
                    }
                    values.push_back(std::move(v));
                }
                break;
            }
            default:
                RAISE_DB_ERROR(eFetchFailed,
                    "Failed to convert Cassandra value to set<>: unsupported data type (Cassandra type - " +
                    to_string(static_cast<int>(type)) + ")"
                );
        }
    }

    template<typename T, typename F = int>
    void FieldGetSetValues(F ifld, std::set<T>& values) const
    {
        const CassValue * clm = GetColumn(ifld);
        CassValueType type = cass_value_type(clm);
        switch (type) {
            case CASS_VALUE_TYPE_SET: {
                unique_ptr<CassIterator, function<void(CassIterator*)>> items_iterator(
                    cass_iterator_from_collection(clm),
                    [](CassIterator* itr) {
                        cass_iterator_free(itr);
                    }
                );
                if (items_iterator != nullptr) {
                    while (cass_iterator_next(items_iterator.get())) {
                        T v;
                        const CassValue* value = cass_iterator_get_value(items_iterator.get());
                        if (!value) {
                            RAISE_DB_ERROR(eSeqFailed, "cass iterator fetch failed");
                        }
                        try {
                            Convert::CassValueConvert(value, v);
                        }
                        catch (CCassandraException& ex) {
                            ex.AddToMessage(GetColumnDef(ifld));
                            throw;
                        }
                        values.insert(std::move(v));
                    }
                }
                break;
            }
            default:
                RAISE_DB_ERROR(eFetchFailed,
                    "Failed to convert Cassandra value to set<>: unsupported data type (Cassandra type - " +
                    to_string(static_cast<int>(type)) + ")"
                );
        }
    }

    template <typename K, typename V, typename F = int>
    void FieldGetMapValue(F ifld, map<K, V>& result) const
    {
        const CassValue * clm = GetColumn(ifld);
        CassValueType type = cass_value_type(clm);
        switch (type) {
            case CASS_VALUE_TYPE_MAP: {
                result.clear();
                unique_ptr<CassIterator, function<void(CassIterator*)>> items_iterator(
                    cass_iterator_from_map(clm),
                    [](CassIterator* itr) {
                        cass_iterator_free(itr);
                    }
                );
                if (items_iterator != nullptr) {
                    while (cass_iterator_next(items_iterator.get())) {
                        const CassValue* key = cass_iterator_get_map_key(items_iterator.get());
                        const CassValue* val = cass_iterator_get_map_value(items_iterator.get());
                        K k;
                        try {
                            Convert::CassValueConvert(key, k);
                        }
                        catch (CCassandraException& ex) {
                            ex.AddToMessage(GetColumnDef(ifld));
                            throw;
                        }
                        V v;
                        try {
                            Convert::CassValueConvert(val, v);
                        }
                        catch (CCassandraException& ex) {
                            ex.AddToMessage(GetColumnDef(ifld));
                            throw;
                        }
                        result.emplace(std::move(k), std::move(v));
                    }
                }
                break;
            }
            default:
                RAISE_DB_ERROR(eFetchFailed,
                    "Failed to convert Cassandra value to map<>: unsupported data type (Cassandra type - " +
                    to_string(static_cast<int>(type)) + ")"
                );
        }
    }

    template<typename F = int>
    size_t FieldGetBlobValue(F ifld, unsigned char* buf, size_t len) const
    {
        const CassValue * clm = GetColumn(ifld);
        const unsigned char * output = nullptr;
        size_t outlen = 0;
        CassError err = cass_value_get_bytes(clm, &output, &outlen);
        if (err != CASS_OK) {
            RAISE_CASS_ERROR(err, eFetchFailed, "Failed to fetch blob data:");
        }
        if (len < outlen) {
            RAISE_DB_ERROR(eFetchFailed, "Failed to fetch blob data: insufficient buffer provided");
        }
        memcpy(buf, output, outlen);
        return outlen;
    }

    template<typename F = int>
    size_t FieldGetBlobRaw(F ifld, const unsigned char** rawbuf) const
    {
        if (rawbuf) {
            *rawbuf = nullptr;
        }

        const CassValue * clm = GetColumn(ifld);
        const unsigned char * output = nullptr;
        size_t outlen = 0;
        CassError rc = cass_value_get_bytes(clm, &output, &outlen);
        if (rc == CASS_ERROR_LIB_NULL_VALUE) {
            return 0;
        }

        if (rc != CASS_OK) {
            RAISE_CASS_ERROR(rc, eFetchFailed, "Failed to fetch blob data:");
        }
        if (rawbuf) {
            *rawbuf = output;
        }
        return outlen;
    }

    template<typename F = int>
    size_t FieldGetBlobSize(F ifld) const
    {
        const CassValue * clm = GetColumn(ifld);
        const unsigned char * output = nullptr;
        size_t outlen = 0;
        CassError rc = cass_value_get_bytes(clm, &output, &outlen);
        if (rc != CASS_ERROR_LIB_NULL_VALUE && rc != CASS_OK) {
            RAISE_CASS_ERROR(rc, eFetchFailed, "Failed to fetch blob data:");
        }
        return outlen;
    }

    shared_ptr<CCassConnection> GetConnection(void)
    {
        return m_connection;
    }

    void SetOnData3(shared_ptr<CCassDataCallbackReceiver> cb)
    {
        auto ondata3 = m_ondata3.lock();
        if (ondata3 == cb) {
            return;
        }

        if (m_future) {
            if (ondata3) {
                RAISE_DB_ERROR(eSeqFailed, "Future callback has already been assigned");
            }
            if (!cb) {
                RAISE_DB_ERROR(eSeqFailed, "Future callback can not be reset");
            }
        }
        m_ondata3 = cb;
        if (m_future) {
            SetupOnDataCallback();
        }
    }

    static const unsigned int DEFAULT_PAGE_SIZE;
};

template<>
const CassValue* CCassQuery::GetColumn(int ifld) const;
template<>
const CassValue* CCassQuery::GetColumn(const string& name) const;
template<>
const CassValue* CCassQuery::GetColumn(const char* name) const;
template<>
string CCassQuery::GetColumnDef(int ifld) const;
template<>
string CCassQuery::GetColumnDef(const string& name) const;
template<>
string CCassQuery::GetColumnDef(const char* name) const;

END_IDBLOB_SCOPE

#endif // OBJTOOLS__PUBSEQ_GATEWAY__IMPL__CASSANDRA__CASS_DRIVER_HPP_
