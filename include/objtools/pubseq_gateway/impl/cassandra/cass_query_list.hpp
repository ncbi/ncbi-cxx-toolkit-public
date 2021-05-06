
#ifndef CASS_QUERY_LIST__HPP
#define CASS_QUERY_LIST__HPP

#include <memory>

#include <objtools/pubseq_gateway/impl/cassandra/cass_driver.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/mpmc_w.hpp>

BEGIN_IDBLOB_SCOPE

USING_NCBI_SCOPE;

class CCassQueryList;

using TCassQueryListTickCB = function<void()>;

class ICassQueryListConsumer {
public:
    ICassQueryListConsumer() = default;
    ICassQueryListConsumer(const ICassQueryListConsumer&) = delete;
    ICassQueryListConsumer(ICassQueryListConsumer&&) = delete;
    ICassQueryListConsumer& operator=(const ICassQueryListConsumer&) = delete;
    ICassQueryListConsumer& operator=(ICassQueryListConsumer&&) = delete;
    virtual ~ICassQueryListConsumer() = default;
    virtual bool Start(shared_ptr<CCassQuery> query, CCassQueryList& list, size_t query_idx) = 0;
    virtual bool Finish(shared_ptr<CCassQuery>, CCassQueryList&, size_t /*query_idx*/) {
        return true;
    }
    virtual bool ProcessRow(shared_ptr<CCassQuery> query, CCassQueryList& list, size_t query_idx) = 0;
    virtual void Reset(shared_ptr<CCassQuery>, CCassQueryList&, size_t /*query_idx*/) {}
    virtual void Failed(shared_ptr<CCassQuery> query, CCassQueryList& list, size_t query_idx, const exception* e) = 0;
};

class CCassQueryList {
public:
    static constexpr const unsigned int kDfltMaxQuery = 128;
    static constexpr const uint64_t kReadyPushWaitTimeout = 500000;
    static constexpr const uint64_t kReadyPopWaitTimeout = 500;
    static constexpr const size_t kNotifyQueueLen = 2048;
    static constexpr const unsigned int kResetRelaxTime = 10;
    static shared_ptr<CCassQueryList> Create(shared_ptr<CCassConnection> cass_conn) noexcept;
    virtual ~CCassQueryList();

    CCassQueryList& SetMaxQueries(size_t max_queries);
    CCassQueryList& SetKeyspace(const string& keyspace);
    CCassQueryList& SetTickCB(TCassQueryListTickCB cb);
    string GetKeyspace() const;
    size_t GetMaxQueries() const;
    bool HasError() const;
    size_t NumberOfActiveQueries() const;
    size_t NumberOfBusySlots() const;
    size_t NumberOfPendingSlots() const;
    string ToString() const;

    void Finalize();
    void Cancel(const exception* e = nullptr);
    void Cancel(ICassQueryListConsumer* consumer, const exception* e = nullptr);
    void Execute(unique_ptr<ICassQueryListConsumer> consumer, int retry_count, bool post_async = false);
    bool HasEmptySlot();
    void Yield(bool wait);
    shared_ptr<CCassQuery> Extract(size_t slot_index);
protected:
    CCassQueryList() :
        m_cnt(0),
        m_has_error(false),
        m_max_queries(kDfltMaxQuery),
        m_yield_in_progress(false),
        m_attached_slots(0),
        m_owning_thread{}
    {}
private:
    enum SQrySlotState {
        ssAvailable,
        ssAttached,
        ssReadingRow,
        ssReseting,
        ssReleasing,
        ssLast = ssReleasing + 1
    };
    static string SQrySlotStateStr[ssLast];
    struct SQrySlot {
        unique_ptr<ICassQueryListConsumer> m_consumer;
        shared_ptr<CCassQuery> m_qry;
        size_t m_index;
        int m_retry_count;
        SQrySlotState m_state;
    };
    class CQryNotification : public CCassDataCallbackReceiver {
    public:
        CQryNotification(shared_ptr<CCassQueryList> query_list, size_t index);
        virtual void OnData() override;
    private:
        weak_ptr<CCassQueryList> m_query_list;
        size_t m_index;
    };
    struct SPendingSlot {
        unique_ptr<ICassQueryListConsumer> m_consumer;
        int m_retry_count;
    };

    void Tick();
    SQrySlot* CheckSlots(bool discard, bool wait = true);
    SQrySlot* CheckSlot(size_t index, bool discard);
    void CheckPending(SQrySlot* slot);
    void AttachSlot(SQrySlot* slot, SPendingSlot&& pending_slot);
    void DetachSlot(SQrySlot* slot);
    void ReadRows(SQrySlot* slot);
    void Release(SQrySlot* slot);
    void CheckAccess();

    weak_ptr<CCassQueryList> m_self_weak;
    shared_ptr<CCassConnection> m_cass_conn;
    vector<SQrySlot> m_query_arr;
    mpmc_bounded_queue_w<size_t, kNotifyQueueLen> m_ready;
    int64_t m_cnt;
    bool m_has_error;
    size_t m_max_queries;
    vector<SPendingSlot> m_pending_arr;
    vector<shared_ptr<CQryNotification>> m_notification_arr;
    TCassQueryListTickCB m_tick_cb;
    atomic_bool m_yield_in_progress;
    string m_keyspace;
    atomic_size_t m_attached_slots;
    atomic<thread::id> m_owning_thread;
};

class CCassOneExecConsumer : public ICassQueryListConsumer {
public:
    CCassOneExecConsumer(function<bool(CCassQuery& query, CCassQueryList& list)> cb, function<void(CCassQuery& query, CCassQueryList& list, bool succeeded)> finish_cb = nullptr) :
        m_cb(cb),
        m_finish_cb(finish_cb),
        m_is_failed(false),
        m_is_started(false),
        m_is_finished(false)
    {}
    CCassOneExecConsumer(const CCassOneExecConsumer&) = delete;
    CCassOneExecConsumer(CCassOneExecConsumer&&) = delete;
    CCassOneExecConsumer& operator=(const CCassOneExecConsumer&) = delete;
    CCassOneExecConsumer& operator=(CCassOneExecConsumer&&) = delete;
    virtual bool Start(shared_ptr<CCassQuery> query, CCassQueryList& list, size_t /*qry_index*/) override {
        assert(!m_is_started);
        assert(!m_is_finished);
        m_is_started = true;
        return m_cb(*query, list);
    }
    virtual bool Finish(shared_ptr<CCassQuery> query, CCassQueryList& list, size_t /*qry_index*/) {
        assert(m_is_started);
        assert(!m_is_finished);
        m_is_finished = true;
        if (m_finish_cb)
            m_finish_cb(*query, list, !m_is_failed);
        return true;
    }
    virtual bool ProcessRow(shared_ptr<CCassQuery>, CCassQueryList&, size_t /*qry_index*/) {
        assert(false);
        return true;
    }
    virtual void Reset(shared_ptr<CCassQuery>, CCassQueryList&, size_t /*qry_index*/) {
    }
    virtual void Failed(shared_ptr<CCassQuery>, CCassQueryList&, size_t /*qry_index*/, const exception*) {
        m_is_failed = true;
    }
private:
    function<bool(CCassQuery& query, CCassQueryList& list)> m_cb;
    function<void(CCassQuery& query, CCassQueryList& list, bool succeeded)> m_finish_cb;
    bool m_is_failed;
    bool m_is_started;
    bool m_is_finished;
};

END_IDBLOB_SCOPE

#endif
