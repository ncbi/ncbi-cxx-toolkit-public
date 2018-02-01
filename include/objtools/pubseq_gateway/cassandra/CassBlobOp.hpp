#ifndef CASSBLOBOP__HPP
#define CASSBLOBOP__HPP

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
 *  BigData application layer
 *
 */

#include <objtools/pubseq_gateway/diag/IdLogUtl.hpp>
#include "CassDriver.hpp"
#include "Key.hpp"
#include "IdCassScope.hpp"

BEGIN_IDBLOB_SCOPE
USING_NCBI_SCOPE;

class CCassBlobWaiter;

typedef enum {tt_unknown, tt_true, tt_false} triple_t;
typedef function<void(const unsigned char *  Data, unsigned int Size, int ChunkNo, bool IsLast)> DataChunkCB_t;
typedef function<void(const char* Text, CCassBlobWaiter* waiter)> DataErrorCB_t;
typedef void(*DataReadyCB_t)(void*);

class CCassBlobWaiter {
public:
    CCassBlobWaiter(const CCassBlobWaiter&) = delete;
    CCassBlobWaiter& operator=(const CCassBlobWaiter&) = delete;
    CCassBlobWaiter(CCassBlobWaiter&&) = default;
    CCassBlobWaiter& operator=(CCassBlobWaiter&&) = default;
    CCassBlobWaiter(IdLogUtil::CAppOp* op, unsigned int op_timeout_ms, shared_ptr<CCassConnection> conn, const string& keyspace, int32_t key, bool async, unsigned int max_retries, DataErrorCB_t error_cb) :
        m_op(op),
        m_error_cb(error_cb),
        m_data_ready_cb(nullptr),
        m_data_ready_data(nullptr),
        m_conn(conn),
        m_keyspace(keyspace),
        m_key(key),
        m_state(stInit),
        m_op_timeout_ms(op_timeout_ms),
        m_last_activity_ms(IdLogUtil::gettime() / 1000L),
        m_restart_counter(0),
        m_max_retries(max_retries), // 0 means no limit in auto-restart count, any other positive value is the limit, 1 means no 2nd start -> no re-starts at all
        m_async(async),
        m_cancelled(false)
    {
        LOG5(("CCassBlobWaiter::CCassBlobWaiter this=%p", this));
    }
    virtual ~CCassBlobWaiter() {
        LOG5(("CCassBlobWaiter::~CCassBlobWaiter this=%p", this));
        CloseAll();
    }
    bool Wait() {
        while (m_state != stDone && m_state != stError && !m_cancelled) {
            Wait1();
            if (m_async)
                break;
        }
        return (m_state == stDone || m_state == stError);
    }
    virtual bool Restart(unsigned int max_retries = 0) {
        CloseAll();
        m_state = stInit;
        if (CanRestart()) {
            ++m_restart_counter;
            UpdateLastActivity();
            return true;
        }
        else
            return false;
    }
    bool HasError() const {
        return !m_last_error.empty();
    }
    string LastError() const {
        return m_last_error;
    }
    string GetKeySpace() const {
        return m_keyspace;
    }
    int32_t key() {
        return m_key;
    }
    void SetErrorCB(DataErrorCB_t&& error_cb) {
        m_error_cb = std::move(error_cb);
    }
    void SetDataReadyCB(DataReadyCB_t datareadycb, void *data) {
        LOG5(("CCassBlobWaiter::SetDataReadyCB this=%p CB: %p, DATA: %p", this, datareadycb, data));
        m_data_ready_cb = datareadycb;
        m_data_ready_data = data;
    }
protected:
    enum State {
        stInit = 0,
        stDone = 10000,
        stError = -1
    };

    IdLogUtil::CAppOp* m_op;
    DataErrorCB_t m_error_cb;
    DataReadyCB_t m_data_ready_cb;
    void *m_data_ready_data;
    shared_ptr<CCassConnection> m_conn;
    string m_keyspace;
    int32_t m_key;
    int32_t m_state;
    unsigned int m_op_timeout_ms;
    int64_t m_last_activity_ms;
    string m_last_error;
    unsigned int m_restart_counter;
    unsigned int m_max_retries;
    bool m_async;
    bool m_cancelled;

    vector<shared_ptr<CCassQuery>> m_query_arr;
    void CloseAll() {
        for (auto& it : m_query_arr) {
            it->Close();
        }
    }
    void Error(const char* msg) {
        m_state = stError;
        m_last_error = msg;
        if (m_error_cb)
            m_error_cb(msg, this);
        else
            RAISE_DB_ERROR(eQueryFailedRestartable, msg);
    }
    bool IsTimedOut() const {
        if (m_op_timeout_ms > 0) 
            return ((IdLogUtil::gettime() / 1000L - m_last_activity_ms) > m_op_timeout_ms);
        else
            return false;
    }

    bool CanRestart() const {
        bool is_timedout = IsTimedOut(); 
        bool is_out_of_retries = (m_max_retries > 0) && (m_restart_counter >= m_max_retries - 1);
        LOG3(("CanRestart? t/o=%d, o/r=%d, last_active=%ld, time=%ld, timeout=%u", is_timedout, is_out_of_retries, m_last_activity_ms, IdLogUtil::gettime() / 1000L, m_op_timeout_ms));
        return !is_out_of_retries && !is_timedout && !m_cancelled;
    }

    bool CheckReady(shared_ptr<CCassQuery> qry, int32_t restart_to_state, bool* restarted) {
        *restarted = false;
        try {
            async_rslt_t wr;
            if (m_async) {
                wr = qry->WaitAsync(0);
                if (wr == ar_wait)
                    return false;
            }
            return true;
        }
        catch (const EDatabase& e) {
            if ((e.GetErrCode() == EDatabase::eQueryTimeout || e.GetErrCode() == EDatabase::eQueryFailedRestartable) && CanRestart()) {
                LOG2(("In-place restart"));
                ++m_restart_counter;
                qry->Close();
                *restarted = true;
                m_state = restart_to_state;
            }
            else {
                Error(e.what());
            }
        }
        return false;
    }
    void UpdateLastActivity() {
        m_last_activity_ms = IdLogUtil::gettime() / 1000L;
    }
    bool CheckMaxActive();
    virtual void Wait1() = 0;
};

class CCassBlobLoader: public CCassBlobWaiter {
public:
    CCassBlobLoader(const CCassBlobLoader&) = delete;
    CCassBlobLoader(CCassBlobLoader&&) = delete;
    CCassBlobLoader& operator=(const CCassBlobLoader&) = delete;
    CCassBlobLoader& operator=(CCassBlobLoader&&) = delete;
    CCassBlobLoader(IdLogUtil::CAppOp* op, unsigned int op_timeout_ms, shared_ptr<CCassConnection> conn, const string& keyspace, int32_t key, bool async, unsigned int max_retries, const DataChunkCB_t& data_chunk_cb, const DataErrorCB_t& DataErrorCB) :
        CCassBlobWaiter(op, op_timeout_ms, conn, keyspace, key, async, max_retries, DataErrorCB),
        m_stat_loaded(false),
        m_data_cb(data_chunk_cb),
        m_expected_size(0),
        m_remaining_size(0),
        m_large_parts(0),
        m_current_idx(-1)
    {
        LOG3(("CCassBlobLoader::CCassBlobLoader max_retries=%d", max_retries));
    }
    void SetDataChunkCB(DataChunkCB_t&& datacb) {
        m_data_cb = std::move(datacb);
    }
    void SetDataReadyCB(DataReadyCB_t datareadycb, void *data) {
        if (datareadycb && m_state != stInit)
            RAISE_ERROR(eSeqFailed, "CCassBlobLoader: DataReadyCB can't be assigned after the loading process has started");
        CCassBlobWaiter::SetDataReadyCB(datareadycb, data);
    }
    SBlobStat GetBlobStat() const {
        if (!m_stat_loaded)
            RAISE_ERROR(eSeqFailed, "CCassBlobLoader: Blob stat can't be read");
        return m_blob_stat;
    }
    uint64_t GetBlobSize() const {
        if (!m_stat_loaded)
            RAISE_ERROR(eSeqFailed, "CCassBlobLoader: Blob stat can't be read");
        return m_expected_size;

    }
    void Cancel() {
        if (m_state == stDone)
            return;

        m_cancelled = true;
        CloseAll();
        m_state = stError;
    }
    virtual bool Restart(unsigned int max_retries = 0) override {
        m_stat_loaded = false;
        m_blob_stat.Reset();
        m_expected_size = 0;
        m_remaining_size = 0;
        m_large_parts = 0;
        m_current_idx = 0;
        return CCassBlobWaiter::Restart(max_retries);
    }
protected:
    virtual void Wait1() override;
private:
    enum State {
        stInit = CCassBlobWaiter::stInit,
        stReadingEntity,
        stReadingChunks,
        stCheckingFlags,
        stDone = CCassBlobWaiter::stDone,
        stError = CCassBlobWaiter::stError
    };
    bool m_stat_loaded;
    SBlobStat m_blob_stat;
    DataChunkCB_t m_data_cb;
    int64_t m_expected_size;
    int64_t m_remaining_size;
    int32_t m_large_parts;
    int32_t m_current_idx;
    void RequestFlags(shared_ptr<CCassQuery> qry, bool with_data);
    void RequestChunk(shared_ptr<CCassQuery> qry, int local_id);
    void RequestChunksAhead();
};

class CCassBlobInserter: public CCassBlobWaiter {
public:
    CCassBlobInserter(IdLogUtil::CAppOp* op, unsigned int op_timeout_ms, shared_ptr<CCassConnection> conn, const string& keyspace, int32_t key, CBlob* blob, triple_t is_new, int64_t large_treshold, int64_t large_chunk_sz, bool async, unsigned int max_retries, const DataErrorCB_t& data_error_cb) :
        CCassBlobWaiter(op, op_timeout_ms, conn, keyspace, key, async, max_retries, data_error_cb),
        m_large_treshold(large_treshold),
        m_large_chunk_sz(large_chunk_sz),
        m_large_parts(0),
        m_blob(blob),
        m_is_new(is_new),
        m_old_large_parts(0),
        m_old_flags(0)
    {}
protected:
    virtual void Wait1() override;
private:
    enum State {
        stInit = 0,
        stFetchOldLargeParts,
        stDeleteOldLargeParts,
        stWaitDeleteOldLargeParts,
        stInsert,
        stWaitingInserted,
        stUpdatingFlags,
        stWaitingUpdateFlags,
        stDone = CCassBlobWaiter::stDone,
        stError = CCassBlobWaiter::stError
    };
    int64_t m_large_treshold;
    int64_t m_large_chunk_sz;
    int32_t m_large_parts;
    CBlob* m_blob;
    triple_t m_is_new;
    int32_t m_old_large_parts;
    int64_t m_old_flags;
};

class CCassBlobDeleter: public CCassBlobWaiter {
public:
    CCassBlobDeleter(IdLogUtil::CAppOp* op, unsigned int op_timeout_ms, shared_ptr<CCassConnection> conn, const string& keyspace, int32_t key, bool async, unsigned int max_retries, DataErrorCB_t error_cb) :
        CCassBlobWaiter(op, op_timeout_ms, conn, keyspace, key, async, max_retries, error_cb),
        m_large_parts(0)
    {}
protected:
    virtual void Wait1() override;
private:
    enum State {
        stInit = 0,
        stReadingEntity,
        stDeleteLargeEnt,
        stWaitLargeEnt,
        stDeleteEnt,
        stWaitingDone,
        stDone = CCassBlobWaiter::stDone,
        stError = CCassBlobWaiter::stError
    };
    int32_t m_large_parts;
};

class CCassBlobOp: public enable_shared_from_this<CCassBlobOp> {
public:
    enum flagop_t {
        FLAG_OP_OR, 
        FLAG_OP_AND, 
        FLAG_OP_SET
    };
public:
    CCassBlobOp(CCassBlobOp&&) = default;
    CCassBlobOp& operator=(CCassBlobOp&&) = default;
    CCassBlobOp(const CCassBlobOp&) = delete;
    CCassBlobOp& operator=(const CCassBlobOp&) = delete;
    CCassBlobOp(shared_ptr<CCassConnection> conn) : 
        m_conn(conn)
    {
        m_keyspace = m_conn->Keyspace();
    }
    ~CCassBlobOp() {
        m_conn = NULL;
    }
// MAIN API >
    void GetBlobChunkTresholds(unsigned int op_timeout_ms, int64_t *LargeTreshold, int64_t *LargeChunkSize);
    void CassExecuteScript(const string& scriptstr, CassConsistency c);
    void CreateScheme(const string& filename, const string& keyspace);
    void SetKeyspace(const string& keyspace) {
        m_keyspace = keyspace;
    }
    string GetKeyspace() const {
        return m_keyspace;
    }
    void LoadKeys(IdLogUtil::CAppOp& op, CBlobFullStatMap* keys, function<void()> tick = nullptr);

    void GetBlob(IdLogUtil::CAppOp& op, unsigned int op_timeout_ms, int32_t key, unsigned int max_retries, SBlobStat* blob_stat, const DataChunkCB_t& data_chunk_cb);
    void GetBlobAsync(IdLogUtil::CAppOp& op, unsigned int op_timeout_ms, int32_t key, unsigned int max_retries, const DataChunkCB_t& data_chunk_cb, unique_ptr<CCassBlobWaiter>& waiter);
    void InsertBlobAsync(IdLogUtil::CAppOp& op, unsigned int op_timeout_ms, int32_t key, unsigned int max_retries, CBlob* blob_rslt, triple_t is_new, int64_t LargeTreshold, int64_t LargeChunkSz, unique_ptr<CCassBlobWaiter>& waiter);
    void DeleteBlobAsync(IdLogUtil::CAppOp& op, unsigned int op_timeout_ms, int32_t key, unsigned int max_retries, unique_ptr<CCassBlobWaiter>& waiter);

    void UpdateBlobFlags(IdLogUtil::CAppOp& op, unsigned int op_timeout_ms, int32_t key, uint64_t flags, flagop_t flag_op);

    bool GetSetting(IdLogUtil::CAppOp& op, unsigned int op_timeout_ms, const string& name, string& value);
    void UpdateSetting(IdLogUtil::CAppOp& op, unsigned int op_timeout_ms, const string& name, const string& value);
    shared_ptr<CCassConnection> GetConn() {
        if (!m_conn)
            RAISE_ERROR(eSeqFailed, "CCassBlobOp instance is not initialized with DB connection");
        return m_conn;
    }
private:
    shared_ptr<CCassConnection> m_conn;
    string m_keyspace;
    void LoadKeysScheduleNext(CCassQuery& query, void* data);
};

END_IDBLOB_SCOPE

#endif
