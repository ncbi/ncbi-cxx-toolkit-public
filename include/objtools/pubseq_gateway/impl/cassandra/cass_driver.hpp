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

#include <string>
#include <memory>
#include <unordered_map>
#include <tuple>
#include <cassert>
#include <atomic>
#include <utility>
#include <vector>
#include <limits>

#include "IdCassScope.hpp"
#include "cass_exception.hpp"

BEGIN_IDBLOB_SCOPE
USING_NCBI_SCOPE;

#define CASS_PREPARED_Q 1
#define CASS_DRV_TIMEOUT_MS 2000U
#define CASS_ERROR_SERVER_CONSISTENCY_NOT_ACHIEVED 0xC40063C0

#define ASYNC_RESULT_MAP(XX) \
    XX(ar_wait, "Wait") \
    XX(ar_done, "Done") \
    XX(ar_dataready, "DataReady")

typedef enum {
    #define XX(name, _) name,
    ASYNC_RESULT_MAP(XX)
    #undef XX
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
    dtCustom
} CCassDataType;

class CCassQuery;
class CCassQueryCbRef;
class CCassConnection;

using TCassQueryOnDataCallback = void(*)(CCassQuery&, void *);
using TCassQueryOnExecuteCallback = void(*)(CCassQuery&, void *);
using TCassQueryOnData2Callback = void(*)(void *);


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

    void CloseSession(void);

 protected:
    CCassConnection();
    const CassPrepared * Prepare(const string & sql);

 public:
    using TTokenValue = int64_t;
    using TTokenRanges = vector<pair<TTokenValue,TTokenValue>>;

    CCassConnection(const CCassConnection&) = delete;
    CCassConnection& operator=(const CCassConnection&) = delete;

    static shared_ptr<CCassConnection> Create(void);
    virtual ~CCassConnection(void);
    void Connect(void);
    void Reconnect(void);
    void Close(void);

    bool IsConnected(void);
    int64_t GetActiveStatements(void) const;
    CassMetrics GetMetrics(void);
    void SetConnProp(const string & host, const string & user, const string & pwd, int16_t port = 0);

    void SetLoadBalancing(loadbalancing_policy_t policy);
    void SetTokenAware(bool value);
    void SetLatencyAware(bool value);

    void SetTimeouts(unsigned int ConnTimeoutMs);
    void SetTimeouts(unsigned int ConnTimeoutMs, unsigned int QryTimeoutMs);

    void SetFallBackRdConsistency(bool value);
    bool GetFallBackRdConsistency(void) const;

    void SetFallBackWrConsistency(unsigned int  value);
    unsigned int GetFallBackWrConsistency(void) const;

    static void SetLogging(EDiagSev  severity);
    static void DisableLogging(void);
    static void UpdateLogging(void);

    unsigned int QryTimeout(void) const;
    unsigned int QryTimeoutMks(void) const;
    void SetRtLimits(unsigned int numThreadsIo, unsigned int numConnPerHost, unsigned int maxConnPerHost);

    void SetKeepAlive(unsigned int keepalive);

    void SetKeyspace(const string & keyspace);
    string Keyspace(void) const;

    shared_ptr<CCassQuery> NewQuery();
    void GetTokenRanges(TTokenRanges &ranges);
    // @deprecated
    void getTokenRanges(TTokenRanges &ranges);
    vector<string> GetPartitionKeyColumnNames(string const & keyspace, string const & table) const;
    static string NewTimeUUID();
    static void Perform(
        unsigned int optimeoutms,
        const std::function<bool()>& PreLoopCB,
        const std::function<void(const CCassandraException&)>& DbExceptCB,
        const std::function<bool(bool)>& OpCB
    );

 private:
    string                          m_host;
    int16_t                         m_port;
    string                          m_user;
    string                          m_pwd;
    string                          m_keyspace;
    CassCluster *                   m_cluster;
    CassSession *                   m_session;
    unsigned int                    m_ctimeoutms;
    unsigned int                    m_qtimeoutms;
    unsigned int                    m_last_query_cnt;
    loadbalancing_policy_t          m_loadbalancing;
    bool                            m_tokenaware;
    bool                            m_latencyaware;
    unsigned int                    m_numThreadsIo;
    unsigned int                    m_numConnPerHost;
    unsigned int                    m_maxConnPerHost;
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


template<typename T>
void CassDataToCollection(CassCollection* coll, const T& v)
{
    static_assert(sizeof(T) == 0,
                  "CassDataToCollection() is not implemented for this type");
}

template<> void CassDataToCollection<bool>(CassCollection* coll, const bool& v);
template<> void CassDataToCollection<int32_t>(CassCollection* coll, const int32_t& v);
template<> void CassDataToCollection<int64_t>(CassCollection* coll, const int64_t& v);
template<> void CassDataToCollection<double>(CassCollection* coll, const double& v);
template<> void CassDataToCollection<string>(CassCollection* coll, const string& v);


class CCassPrm
{
 protected:
    CassValueType   m_type;
    bool            m_assigned;

    union simpleval_t {
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

    void Assign(int64_t  v)
    {
        m_type = CASS_VALUE_TYPE_BIGINT;
        m_simpleval.i64 = v;
        m_assigned = true;
    }

    template<
        typename T,
        typename = typename enable_if<
            is_constructible<string, T>::value
        >::type
    >
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
        m_collection = unique_ptr<CassCollection,
                                  function<void(CassCollection*)> >
                            (cass_collection_new(CASS_COLLECTION_TYPE_MAP,
                                                 v.size() * 2),
                             [](CassCollection* coll)
                             {
                                cass_collection_free(coll);
                             });

        for (const auto &  it : v) {
            CassDataToCollection<K>(m_collection.get(), it.first);
            CassDataToCollection<V>(m_collection.get(), it.second);
        }
        m_assigned = true;
    }

    template<typename I>
    void AssignList(I begin, I end, size_t sz)
    {
        m_type = CASS_VALUE_TYPE_LIST;
        m_collection = unique_ptr<CassCollection,
                                  function<void(CassCollection*)> >
                        (cass_collection_new(CASS_COLLECTION_TYPE_LIST, sz),
                         [](CassCollection* coll)
                         {
                            cass_collection_free(coll);
                         });

        for (; begin != end; ++begin) {
            CassDataToCollection(m_collection.get(), *begin);
        }
        m_assigned = true;
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
        typedef typename conditional<sizeof...(T) == 0,
                                     char, Int8>::type TSwitch;
        AssignTupleImpl< decltype(t), sizeof...(T) > (t, TSwitch());
        m_assigned = true;
    }

    void AssignNull(void)
    {
        m_type = CASS_VALUE_TYPE_UNKNOWN;
        m_assigned = true;
    }

    bool IsAssigned(void)
    {
        return m_assigned;
    }

    int32_t AsInt32(void) const
    {
        switch (m_type) {
            case CASS_VALUE_TYPE_INT:
                return m_simpleval.i32;
            case CASS_VALUE_TYPE_BIGINT:
                return m_simpleval.i64;
            case CASS_VALUE_TYPE_VARCHAR: {
                int32_t     i32;
                if (NStr::StringToNumeric(m_bytes, &i32))
                    return i32;
                else
                    RAISE_DB_ERROR(eBindFailed,
                                   string("Can't convert Param to int32"));
            }
            // no break
            default:
                RAISE_DB_ERROR(eBindFailed,
                               string("Can't convert Param to int32"));
        }
    }

    int64_t AsInt64(void) const
    {
        switch (m_type) {
            case CASS_VALUE_TYPE_INT:
                return m_simpleval.i32;
            case CASS_VALUE_TYPE_BIGINT:
                return m_simpleval.i64;
            case CASS_VALUE_TYPE_VARCHAR: {
                int64_t     i64;
                if (NStr::StringToNumeric(m_bytes, &i64))
                    return i64;
                else
                    RAISE_DB_ERROR(eBindFailed,
                                   string("Can't convert Param to int64"));
            }
            // no break
            default:
                RAISE_DB_ERROR(eBindFailed,
                               string("Can't convert Param to int64"));
        }
    }

    void AsString(string & value) const
    {
        switch (m_type) {
            case CASS_VALUE_TYPE_INT:
                value = NStr::NumericToString(m_simpleval.i32);
                break;
            case CASS_VALUE_TYPE_BIGINT:
                value = NStr::NumericToString(m_simpleval.i64);
                break;
            case CASS_VALUE_TYPE_VARCHAR:
                value = m_bytes;
                break;
            default:
                RAISE_DB_ERROR(eBindFailed, string("Can't convert Param to string"));
        }
    }

    friend class CCassQuery;

 private:
    void AssignTupleItem(const Int4 &  v, size_t  idx)
    {
        cass_tuple_set_int32(m_tuple.get(), idx, v);
    }

    template<typename T, size_t N>
    void AssignTupleImpl(const T &  t, Int8)
    {
        AssignTupleItem(get<N - 1>(t), N - 1);
        typedef typename conditional< (N - 1) == 0, char, Int8>::type TSwitch;
        AssignTupleImpl<decltype(t), N - 1>(t, TSwitch());
    }

    template<typename T, size_t N>
    void AssignTupleImpl(const T& t, char)
    {
    }
};


class CCassParams: public vector<CCassPrm>
{};


template <typename T>
T CassValueConvert(const CassValue* Val)
{
    static_assert(sizeof(T) == 0,
                  "Conversion for this type is not implemented yet");
}


template<> bool CassValueConvert<bool>(const CassValue* Val);
template<> int16_t CassValueConvert<int16_t>(const CassValue* Val);
template<> int32_t CassValueConvert<int32_t>(const CassValue* Val);
template<> int64_t CassValueConvert<int64_t>(const CassValue* Val);
template<> double CassValueConvert<double>(const CassValue* Val);
template<> string CassValueConvert<string>(const CassValue* Val);


template<class T>
T CassValueConvertDef(const CassValue* Val, const T& _default)
{
    if (Val && cass_value_is_null(Val))
        return _default;
    else
        return CassValueConvert<T>(Val);
}


class CCassQueryCbRef: public std::enable_shared_from_this<CCassQueryCbRef>
{
 public:
    explicit CCassQueryCbRef(shared_ptr<CCassQuery> query)
        : m_ondata(nullptr)
        , m_data(nullptr)
        , m_ondata2(nullptr)
        , m_data2(nullptr)
        , m_query(query)
    {
        ERR_POST(Trace << "CCassQueryCbRef::CCassQueryCbRef " << this);
    }

    virtual ~CCassQueryCbRef()
    {
        ERR_POST(Trace << "CCassQueryCbRef::~CCassQueryCbRef " << this);
    }

    void Attach(void (*ondata)(CCassQuery&, void*),
                void* data, void (*ondata2)(void*), void* data2)
    {
        m_self = shared_from_this();
        m_ondata = ondata;
        m_data = data;
        m_ondata2 = ondata2;
        m_data2 = data2;
    }

    void Detach(bool remove_self_ref = false)
    {
        ERR_POST(Trace << "CCassQueryCbRef::Detach " << this << ", Query: " <<
                 m_query.lock().get());
        m_query.reset();
        if (remove_self_ref)
            m_self = nullptr;
    }

    static void s_OnFutureCb(CassFuture* future, void* data)
    {
        try {
            ERR_POST(Trace << "CCassQueryCbRef::s_OnFutureCb " << data);
            shared_ptr<CCassQueryCbRef>     self(
                    static_cast<CCassQueryCbRef*>(data)->shared_from_this());

            assert(self->m_self);
            self->m_self = nullptr;

            auto    query = self->m_query.lock();
            if (query != nullptr) {
                ERR_POST(Trace << "CCassQuery::OnDataReady this: " <<
                         query.get() << ", fut: " << future);
                if (self->m_ondata)
                    self->m_ondata(*query.get(), self->m_data);
                if (self->m_ondata2)
                    self->m_ondata2(self->m_data2);
            }
        }
        catch (const CException& e) {
            ERR_POST("CException caught! Message: " << e.GetMsg());
            abort();
        }
        catch (const exception& e) {
            ERR_POST("exception caught! Message: " << e.what());
            abort();
        }
    }

 private:
    TCassQueryOnDataCallback        m_ondata;
    void*                           m_data;

    TCassQueryOnData2Callback       m_ondata2;
    void*                           m_data2;

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
    shared_ptr<CCassConnection>     m_connection;
    unsigned int                    m_qtimeoutms;
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
    CassConsistency                 m_serial_consistency;

    shared_ptr<CCassQueryCbRef>     m_cb_ref;

    TCassQueryOnDataCallback        m_ondata;
    void*                           m_ondata_data;

    TCassQueryOnData2Callback       m_ondata2;
    void*                           m_ondata2_data;

    TCassQueryOnExecuteCallback     m_onexecute;
    void*                           m_onexecute_data;

    async_rslt_t Wait(unsigned int timeoutmks);
    void Bind(void);

    template<typename F>
    const CassValue* GetColumn(F ifld) const
    {
        static_assert(sizeof(F) == 0, "Columns can be accessed by either index or name");
        return CNotImplemented<F>::GetColumn_works_by_name_and_by_index();
    }

    template <typename F = int>
    CassIterator* GetTupleIterator(F ifld) const
    {
        if (!m_row)
            NCBI_THROW(CCassandraException, eSeqFailed, "query row is not fetched");
        CassIterator* it = cass_iterator_from_tuple(GetColumn(ifld));
        if (!it) {
            NCBI_THROW(CCassandraException, eSeqFailed,
                "cannot resolve tuple iterator for column");
        }
        return it;
    }

    const CassValue* GetTupleIteratorValue(CassIterator* itr) const;

    template<typename T, size_t C, size_t N>
    void FieldGetTupleValue(CassIterator* itr, T& t, Int8) const
    {
        typedef typename tuple_element< C, T >::type TItemType;
        get<C>(t) = CassValueConvert< TItemType >(GetTupleIteratorValue(itr));
        typedef typename conditional< C + 1 == N, char, Int8>::type TSwitch;
        FieldGetTupleValue<T, C + 1, N>(itr, t, TSwitch());
    }

    template<typename T, size_t C, size_t N>
    void FieldGetTupleValue(CassIterator*, T&, char) const
    {
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
        m_allow_prepare(CASS_PREPARED_Q),
        m_is_prepared(false),
        m_serial_consistency(CASS_CONSISTENCY_ANY),
        m_ondata(nullptr),
        m_ondata_data(nullptr),
        m_ondata2(nullptr),
        m_ondata2_data(nullptr),
        m_onexecute(nullptr),
        m_onexecute_data(nullptr)
    {
        ERR_POST(Trace << "CCassQuery::CCassQuery this=" << this);
        if (!m_connection) {
            NCBI_THROW(CCassandraException, eFatal, "Cassandra connection is not established");
        }
    }

 public:
    virtual ~CCassQuery();
    virtual void Close(void);

    void SetTimeout();
    void SetTimeout(unsigned int t);

    unsigned int Timeout(void) const;

    bool IsReady(void);
    void NewBatch(void);
    async_rslt_t RunBatch();
    void SetSQL(const string& sql, unsigned int PrmCount);

    /* returns resultset */
    void Query(CassConsistency c = CASS_CONSISTENCY_LOCAL_QUORUM,
               bool run_async = false, bool allow_prepare = CASS_PREPARED_Q,
               unsigned int page_size = DEFAULT_PAGE_SIZE);

    void RestartQuery(CassConsistency c = CASS_CONSISTENCY_LOCAL_QUORUM);

    /* returns no resultset */
    void Execute(CassConsistency c = CASS_CONSISTENCY_LOCAL_QUORUM,
                 bool run_async = false, bool allow_prepare = CASS_PREPARED_Q);
    void SetSerialConsistency(CassConsistency c);

    bool IsActive(void) const
    {
        return (m_row != nullptr) || (m_statement != nullptr) || (m_batch != nullptr);
    }

    virtual async_rslt_t WaitAsync(unsigned int timeoutmks);
    virtual string ToString(void) const;

    virtual bool IsEOF(void) const;
    virtual bool IsAsync(void) const;

    void BindNull(int iprm);
    void BindInt16(int iprm, int16_t value);
    void BindInt32(int iprm, int32_t value);
    void BindInt64(int iprm, int64_t value);
    void BindStr(int iprm, const string& value);
    void BindStr(int iprm, const char* value);
    void BindBytes(int iprm, const unsigned char* buf, size_t len);

    template<typename K, typename V>
    void BindMap(int iprm, const map<K, V>& value)
    {
        if (iprm < 0 || (unsigned int)iprm >= m_params.size())
            RAISE_DB_ERROR(eBindFailed, string("Param index is out of range"));
        m_params[iprm].Assign<K,V>(value);
    }

    template<typename I>
    void BindList(int iprm, I begin, I end, size_t sz)
    {
        if (iprm < 0 || (unsigned int)iprm >= m_params.size())
            RAISE_DB_ERROR(eBindFailed, string("Param index is out of range"));
        m_params[iprm].AssignList(begin, end, sz);
    }

    template<typename ...T>
    void BindTuple(int iprm, const tuple<T...>& value)
    {
        if (iprm < 0 || (unsigned int)iprm >= m_params.size())
            RAISE_DB_ERROR(eBindFailed, string("Param index is out of range"));
        m_params[iprm].Assign(value);
    }

    size_t ParamCount(void) const
    {
        return m_params.size();
    }

    int32_t ParamAsInt32(int iprm);
    int64_t ParamAsInt64(int iprm);
    string ParamAsStr(int iprm);
    void ParamAsStr(int iprm, string& value);

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
        if (FieldIsNull(ifld))
            return dtNull;

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
            case CASS_VALUE_TYPE_UNKNOWN:
            default:
                return dtUnknown;
        }
    }

    template <typename F = int>
    bool FieldGetBoolValue(F ifld) const
    {
        return CassValueConvert<bool>(GetColumn(ifld));
    }

    template <typename F = int>
    bool FieldGetBoolValue(F ifld, bool _default) const
    {
        return CassValueConvertDef<bool>(GetColumn(ifld), _default);
    }

    template <typename F = int>
    int32_t FieldGetInt16Value(F ifld) const
    {
        return CassValueConvert<int16_t>(GetColumn(ifld));
    }

    template <typename F = int>
    int32_t FieldGetInt16Value(F ifld, int16_t _default) const
    {
        return CassValueConvertDef<int16_t>(GetColumn(ifld), _default);
    }

    template <typename F = int>
    int32_t FieldGetInt32Value(F ifld) const
    {
        return CassValueConvert<int32_t>(GetColumn(ifld));
    }

    template <typename F = int>
    int32_t FieldGetInt32Value(F ifld, int32_t _default) const
    {
        return CassValueConvertDef<int32_t>(GetColumn(ifld), _default);
    }

    template <typename F = int>
    int64_t FieldGetInt64Value(F ifld) const
    {
        return CassValueConvert<int64_t>(GetColumn(ifld));
    }

    template <typename F = int>
    int64_t FieldGetInt64Value(F ifld, int64_t _default) const
    {
        return CassValueConvertDef<int64_t>(GetColumn(ifld), _default);
    }

    template <typename F = int>
    double FieldGetFloatValue(F ifld) const
    {
        return CassValueConvert<double>(GetColumn(ifld));
    }

    template <typename F = int>
    double FieldGetFloatValue(F ifld, double _default) const
    {
        return CassValueConvertDef<double>(GetColumn(ifld), _default);
    }

    template <typename F = int>
    string FieldGetStrValue(F ifld) const
    {
        return CassValueConvert<string>(GetColumn(ifld));
    }

    template <typename F = int>
    string FieldGetStrValueDef(F ifld, const string& _default) const
    {
        return CassValueConvertDef<string>(GetColumn(ifld), _default);
    }

    template <typename I, typename F = int>
    void FieldGetContainerValue(F  ifld, I  insert_iterator) const
    {
        const CassValue * clm = GetColumn(ifld);
        CassValueType type = cass_value_type(clm);

        switch (type) {
            case CASS_COLLECTION_TYPE_LIST:
            case CASS_COLLECTION_TYPE_SET:
            {
                if (!cass_value_is_null(clm)) {
                    CassIterator* items_iterator = cass_iterator_from_collection(clm);
                    unique_ptr<CassIterator,
                               function<void(CassIterator*)> > items_iterator_ptr(
                            items_iterator, [](CassIterator* itr)
                            {
                                cass_iterator_free(itr);
                            });
                    while (cass_iterator_next(items_iterator)) {
                        *insert_iterator = CassValueConvert<
                                 typename I::container_type::value_type >(
                                         cass_iterator_get_value(items_iterator));
                        ++insert_iterator;
                    }
                }
            }
            break;
            default:
                if (!cass_value_is_null(clm)) {
                    RAISE_DB_ERROR(eFetchFailed,
                                   string("failed to fetch: unsupported (") +
                                   NStr::NumericToString(
                                       static_cast<int>(type)) +
                                   ") data type");
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
        if (!cass_value_is_null(GetColumn(ifld))) {
            T t;
            using TSwitch = typename conditional<tuple_size<T>::value == 0, char, Int8>::type;
            FieldGetTupleValue< T, 0, tuple_size<T>::value > (GetTupleIterator(ifld), t, TSwitch());
            return t;
        }
        return T();
    }

    template<typename T, typename F = int>
    void FieldGetSetValues(F ifld, std::vector<T>& values) const
    {
        const CassValue *   clm = GetColumn(ifld);
        CassValueType       type = cass_value_type(clm);

        switch (type) {
            case CASS_VALUE_TYPE_SET: {
                CassIterator* items_iterator = cass_iterator_from_collection(clm);
                unique_ptr<CassIterator, function<void(CassIterator*)> > items_iterator_ptr(
                    items_iterator,
                    [](CassIterator* itr)
                    {
                        cass_iterator_free(itr);
                    }
                );
                while (cass_iterator_next(items_iterator)) {
                    values.emplace_back(
                        CassValueConvert<T>(cass_iterator_get_value(items_iterator))
                    );
                }
                break;
            }
            default:
                RAISE_DB_ERROR(eFetchFailed,
                    string("failed to fetch: unsupported (") +
                    NStr::NumericToString(static_cast<int>(type)) +
                    ") data type"
                );
        }
    }

    template <typename K, typename V, typename F = int>
    void FieldGetMapValue(F ifld, map<K, V>& result) const
    {
        const CassValue *   clm = GetColumn(ifld);
        CassValueType       type = cass_value_type(clm);

        switch (type) {
            case CASS_VALUE_TYPE_MAP: {
                result.clear();
                CassIterator* items_iterator = cass_iterator_from_map(clm);
                unique_ptr<CassIterator,
                           function<void(CassIterator*)> > items_iterator_ptr(
                                   items_iterator,
                                   [](CassIterator* itr)
                                   {
                                        cass_iterator_free(itr);
                                   });
                while (cass_iterator_next(items_iterator)) {
                    const CassValue* key = cass_iterator_get_map_key(items_iterator);
                    const CassValue* val = cass_iterator_get_map_value(items_iterator);
                    result.emplace(CassValueConvert<K>(key), CassValueConvert<V>(val));
                }
                break;
            }
            default:
                RAISE_DB_ERROR(eFetchFailed,
                               string("failed to fetch: unsupported (") +
                               NStr::NumericToString(static_cast<int>(type)) +
                               ") data type");
        }
    }

    template<typename F = int>
    size_t FieldGetBlobValue(F ifld, unsigned char* buf, size_t len) const
    {
        const CassValue *  clm = GetColumn(ifld);
        const unsigned char * output = nullptr;
        size_t outlen = 0;
        if (cass_value_get_bytes(clm, &output, &outlen) != CASS_OK) {
            RAISE_DB_ERROR(eFetchFailed, string("failed to fetch blob data"));
        }
        if (len < outlen) {
            RAISE_DB_ERROR(eFetchFailed,
                string("failed to fetch blob data, insufficient buffer provided")
           );
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
            RAISE_DB_ERROR(eFetchFailed, string("failed to fetch blob data"));
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
        if (rc != CASS_ERROR_LIB_NULL_VALUE) {
            if (rc != CASS_OK) {
                RAISE_DB_ERROR(eFetchFailed, string("failed to fetch blob data"));
            }
        }
        return outlen;
    }

    shared_ptr<CCassConnection> GetConnection(void)
    {
        return m_connection;
    }

    void SetOnData(TCassQueryOnDataCallback cb, void* data)
    {
        if (m_ondata == cb && m_ondata_data == data) {
            return;
        }

        if (m_future) {
            if (m_ondata) {
                NCBI_THROW(CCassandraException, eSeqFailed,
                    "Future callback has already been assigned");
            }
            if (!cb && m_future) {
                NCBI_THROW(CCassandraException, eSeqFailed,
                    "Future callback can not be reset");
            }
        }
        m_ondata = move(cb);
        m_ondata_data = data;
        if (m_future) {
            SetupOnDataCallback();
        }
    }

    void SetOnData2(TCassQueryOnData2Callback cb, void* Data)
    {
        if (m_ondata2 == cb && m_ondata2_data == Data) {
            return;
        }

        if (m_future) {
            if (m_ondata2) {
                NCBI_THROW(CCassandraException, eSeqFailed,
                           "Future callback has already been assigned");
            }
            if (!cb) {
                NCBI_THROW(CCassandraException, eSeqFailed,
                           "Future callback can not be reset");
            }
        }
        m_ondata2 = cb;
        m_ondata2_data = Data;
        if (m_future) {
            SetupOnDataCallback();
        }
    }

    void SetOnExecute(void (*Cb)(CCassQuery&, void*), void* Data)
    {
        m_onexecute = Cb;
        m_onexecute_data = Data;
    }

    static const unsigned int DEFAULT_PAGE_SIZE;
};

template<>
const CassValue* CCassQuery::GetColumn(int ifld) const;
template<>
const CassValue* CCassQuery::GetColumn(const string& name) const;
template<>
const CassValue* CCassQuery::GetColumn(const char* name) const;

END_IDBLOB_SCOPE

#endif // OBJTOOLS__PUBSEQ_GATEWAY__IMPL__CASSANDRA__CASS_DRIVER_HPP_
