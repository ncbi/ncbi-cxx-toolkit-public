
#ifndef CASS_QUERY_LIST__HPP
#define CASS_QUERY_LIST__HPP

#include <memory>

#include <objtools/pubseq_gateway/impl/cassandra/cass_driver.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/mpmc_w.hpp>

BEGIN_IDBLOB_SCOPE

USING_NCBI_SCOPE;

class CCassQueryList;

const int k_ready_wait_timeout = 1000000;
constexpr const size_t k_max_queries = 1024;

using TCassQueryListTickCB = function<void()>;

class ICassQueryListConsumer {
public:
    virtual ~ICassQueryListConsumer() = default;
    virtual bool Start(shared_ptr<CCassQuery> query, CCassQueryList& list, size_t query_idx) = 0;
    virtual bool Finish(shared_ptr<CCassQuery> query, CCassQueryList& list, size_t query_idx) {
        return true;
    }
    virtual bool ProcessRow(shared_ptr<CCassQuery> query, CCassQueryList& list, size_t query_idx) = 0;
    virtual void Reset(shared_ptr<CCassQuery> query, CCassQueryList& list, size_t query_idx) {
    }
    virtual void Failed(shared_ptr<CCassQuery> query, CCassQueryList& list, size_t query_idx, const exception* e) = 0;
};

class CCassQueryList {
public:
    static constexpr const unsigned int k_dflt_max_stmt = 128;
    CCassQueryList(shared_ptr<CCassConnection> cass_conn) :
        m_cass_conn(cass_conn),
        m_cnt(0),
        m_has_error(false),
        m_max_queries(k_dflt_max_stmt)
    {}

    CCassQueryList& SetMaxQueries(size_t max_queries) {
        assert(max_queries <= k_max_queries); // can't have more than k_max_queries because m_ready is a static array of this size * 2
        assert(m_query_arr.empty());          // can't resize m_notification_arr if there are potentially pending query callbacks
        if (m_query_arr.empty()) {
            m_max_queries = min(max_queries, k_max_queries);
            if (m_notification_arr.size() < m_max_queries) {
                m_notification_arr.reserve(m_max_queries);
                while (m_notification_arr.size() < m_max_queries) {
                    m_notification_arr.push_back({this, m_notification_arr.size()});
                }
            }
        }
        return *this;
    }
    CCassQueryList& SetKeyspace(const string& keyspace) {
        m_cass_conn->SetKeyspace(keyspace);
        return *this;
    }
    CCassQueryList& SetTickCB(TCassQueryListTickCB cb) {
        m_tick_cb = cb;
        return *this;
    }
    size_t GetMaxQueries() const {
        return m_max_queries;
    }
    bool HasError() const {
        return m_has_error;
    }
    void Wait();
    void Cancel(const exception* e = nullptr);
    void Cancel(ICassQueryListConsumer* consumer, const exception* e = nullptr);
    void Execute(unique_ptr<ICassQueryListConsumer> consumer, int retry_count, bool post_async = false);
    bool HasEmptySlot();
    void Yield();
    shared_ptr<CCassQuery> Extract(size_t slot_index);
private:
    enum SQrySlotState {
        ssAvailable,
        ssAttached,
        ssReadingRows,
        ssReseting,
        ssReleasing,
    };
    struct SQrySlot {
        unique_ptr<ICassQueryListConsumer> m_consumer;
        shared_ptr<CCassQuery> m_qry;
        size_t m_index;
        int m_retry_count;
        SQrySlotState m_state;
    };
    struct SQryNotification {
        CCassQueryList* m_query_list;
        size_t m_index;
    };
    struct SPendingSlot {
        unique_ptr<ICassQueryListConsumer> m_consumer;
        int m_retry_count;
    };
    static void SOnDataCB(CCassQuery& qry, void* data) {
        SQryNotification* notification = static_cast<SQryNotification*>(data);
        while (!notification->m_query_list->m_ready.push_wait(notification->m_index, k_ready_wait_timeout)) {
            
        }
    }
    void Tick() {
        if (m_tick_cb)
            m_tick_cb();
    }

    SQrySlot* WaitAny(bool discard, bool wait = true);
    SQrySlot* WaitOne(size_t index, bool discard);
    void CheckPending(SQrySlot* slot);
    void AttachSlot(SQrySlot* slot, SPendingSlot&& pending_slot);
    void DetachSlot(SQrySlot* slot);
    void ReadRows(SQrySlot* slot);
    void Release(SQrySlot* slot);

    shared_ptr<CCassConnection> m_cass_conn;
    vector<SQrySlot> m_query_arr;
    mpmc_bounded_queue_w<size_t, k_max_queries * 2> m_ready;
    int64_t m_cnt;
    bool m_has_error;
    size_t m_max_queries;
    vector<SPendingSlot> m_pending_arr;
    vector<SQryNotification> m_notification_arr;
    TCassQueryListTickCB m_tick_cb;
};

class CCassOneExecConsumer : public ICassQueryListConsumer {
public:
    CCassOneExecConsumer(function<bool(CCassQuery& query, CCassQueryList& list)> cb) :
        m_cb(cb)
    {}
    virtual bool Start(shared_ptr<CCassQuery> query, CCassQueryList& list, size_t qry_index) override {
        return m_cb(*query, list);
    }
    virtual bool Finish(shared_ptr<CCassQuery> query, CCassQueryList& list, size_t qry_index) {
        return true;
    }
    virtual bool ProcessRow(shared_ptr<CCassQuery> query, CCassQueryList& list, size_t qry_index) {
        assert(false);
        return true;
    }
    virtual void Reset(shared_ptr<CCassQuery> query, CCassQueryList& list, size_t qry_index) {
    }
    virtual void Failed(shared_ptr<CCassQuery> query, CCassQueryList& list, size_t qry_index, const exception* e) {
    }
private:
    function<bool(CCassQuery& query, CCassQueryList& list)> m_cb;
};

END_IDBLOB_SCOPE

#endif
