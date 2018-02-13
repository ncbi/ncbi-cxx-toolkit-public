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

#include <objtools/pubseq_gateway/impl/diag/IdLogUtl.hpp>
#include "CassDriver.hpp"
#include "Key.hpp"
#include "IdCassScope.hpp"

BEGIN_IDBLOB_SCOPE
USING_NCBI_SCOPE;

class CCassBlobWaiter;

typedef enum {
    tt_unknown,
    tt_true,
    tt_false
} triple_t;

typedef function<void(const unsigned char *  Data,
                      unsigned int  Size, int  ChunkNo,
                      bool  IsLast)> DataChunkCB_t;
typedef function<void(const char *  Text,
                      CCassBlobWaiter *  waiter)> DataErrorCB_t;
typedef void(*DataReadyCB_t)(void*);


class CCassBlobWaiter
{
public:
    CCassBlobWaiter(const CCassBlobWaiter&) = delete;
    CCassBlobWaiter& operator=(const CCassBlobWaiter&) = delete;
    CCassBlobWaiter(CCassBlobWaiter&&) = default;
    CCassBlobWaiter& operator=(CCassBlobWaiter&&) = default;

    CCassBlobWaiter(IdLogUtil::CAppOp *  op, unsigned int  op_timeout_ms,
                    shared_ptr<CCassConnection>  conn,
                    const string &  keyspace, int32_t  key, bool  async,
                    unsigned int  max_retries, DataErrorCB_t  error_cb) :
        m_Op(op),
        m_ErrorCb(error_cb),
        m_DataReadyCb(nullptr),
        m_DataReadyData(nullptr),
        m_Conn(conn),
        m_Keyspace(keyspace),
        m_Key(key),
        m_State(stInit),
        m_OpTimeoutMs(op_timeout_ms),
        m_LastActivityMs(IdLogUtil::gettime() / 1000L),
        m_RestartCounter(0),
        m_MaxRetries(max_retries), // 0 means no limit in auto-restart count,
                                   // any other positive value is the limit,
                                   // 1 means no 2nd start -> no re-starts at all
        m_Async(async),
        m_Cancelled(false)
    {
        LOG5(("CCassBlobWaiter::CCassBlobWaiter this=%p", this));
    }

    virtual ~CCassBlobWaiter()
    {
        LOG5(("CCassBlobWaiter::~CCassBlobWaiter this=%p", this));
        CloseAll();
    }

    bool Wait(void)
    {
        while (m_State != stDone && m_State != stError && !m_Cancelled) {
            Wait1();
            if (m_Async)
                break;
        }
        return (m_State == stDone || m_State == stError);
    }

    virtual bool Restart(unsigned int  max_retries = 0)
    {
        CloseAll();
        m_State = stInit;
        if (CanRestart()) {
            ++m_RestartCounter;
            UpdateLastActivity();
            return true;
        }
        else
            return false;
    }

    bool HasError(void) const
    {
        return !m_last_error.empty();
    }

    string LastError(void) const
    {
        return m_last_error;
    }

    string GetKeySpace(void) const
    {
        return m_Keyspace;
    }

    int32_t key(void) const
    {
        return m_Key;
    }

    void SetErrorCB(DataErrorCB_t &&  error_cb)
    {
        m_ErrorCb = std::move(error_cb);
    }

    void SetDataReadyCB(DataReadyCB_t  datareadycb, void *  data)
    {
        LOG5(("CCassBlobWaiter::SetDataReadyCB this=%p CB: %p, DATA: %p",
              this, datareadycb, data));
        m_DataReadyCb = datareadycb;
        m_DataReadyData = data;
    }

protected:
    enum State {
        stInit = 0,
        stDone = 10000,
        stError = -1
    };

    void CloseAll(void)
    {
        for (auto &  it: m_QueryArr) {
            it->Close();
        }
    }

    void Error(const char *  msg)
    {
        m_State = stError;
        m_last_error = msg;
        if (m_ErrorCb)
            m_ErrorCb(msg, this);
        else
            RAISE_DB_ERROR(eQueryFailedRestartable, msg);
    }

    bool IsTimedOut(void) const
    {
        if (m_OpTimeoutMs > 0)
            return ((IdLogUtil::gettime() / 1000L - m_LastActivityMs) >
                    m_OpTimeoutMs);
        return false;
    }

    bool CanRestart(void) const
    {
        bool    is_timedout = IsTimedOut();
        bool    is_out_of_retries = (m_MaxRetries > 0) &&
                                    (m_RestartCounter >= m_MaxRetries - 1);

        LOG3(("CanRestart? t/o=%d, o/r=%d, last_active=%ld, time=%ld, timeout=%u",
              is_timedout, is_out_of_retries, m_LastActivityMs,
              IdLogUtil::gettime() / 1000L, m_OpTimeoutMs));
        return !is_out_of_retries && !is_timedout && !m_Cancelled;
    }

    bool CheckReady(shared_ptr<CCassQuery>  qry, int32_t  restart_to_state,
                    bool *  restarted)
    {
        *restarted = false;
        try {
            async_rslt_t wr;
            if (m_Async) {
                wr = qry->WaitAsync(0);
                if (wr == ar_wait)
                    return false;
            }
            return true;
        } catch (const EDatabase& e) {
            if ((e.GetErrCode() == EDatabase::eQueryTimeout ||
                 e.GetErrCode() == EDatabase::eQueryFailedRestartable) && CanRestart()) {
                LOG2(("In-place restart"));
                ++m_RestartCounter;
                qry->Close();
                *restarted = true;
                m_State = restart_to_state;
            } else {
                Error(e.what());
            }
        }
        return false;
    }

    void UpdateLastActivity(void)
    {
        m_LastActivityMs = IdLogUtil::gettime() / 1000L;
    }

    bool CheckMaxActive(void);
    virtual void Wait1(void) = 0;

protected:
    IdLogUtil::CAppOp *             m_Op;
    DataErrorCB_t                   m_ErrorCb;
    DataReadyCB_t                   m_DataReadyCb;
    void *                          m_DataReadyData;
    shared_ptr<CCassConnection>     m_Conn;
    string                          m_Keyspace;
    int32_t                         m_Key;
    int32_t                         m_State;
    unsigned int                    m_OpTimeoutMs;
    int64_t                         m_LastActivityMs;
    string                          m_last_error;
    unsigned int                    m_RestartCounter;
    unsigned int                    m_MaxRetries;
    bool                            m_Async;
    bool                            m_Cancelled;

    vector<shared_ptr<CCassQuery>>  m_QueryArr;
};



class CCassBlobLoader: public CCassBlobWaiter
{
public:
    CCassBlobLoader(const CCassBlobLoader&) = delete;
    CCassBlobLoader(CCassBlobLoader&&) = delete;
    CCassBlobLoader& operator=(const CCassBlobLoader&) = delete;
    CCassBlobLoader& operator=(CCassBlobLoader&&) = delete;

    CCassBlobLoader(IdLogUtil::CAppOp *  op, unsigned int  op_timeout_ms,
                    shared_ptr<CCassConnection>  conn, const string &  keyspace,
                    int32_t  key, bool  async, unsigned int  max_retries,
                    const DataChunkCB_t &  data_chunk_cb,
                    const DataErrorCB_t &  DataErrorCB) :
        CCassBlobWaiter(op, op_timeout_ms, conn, keyspace, key,
                        async, max_retries, DataErrorCB),
        m_StatLoaded(false),
        m_DataCb(data_chunk_cb),
        m_ExpectedSize(0),
        m_RemainingSize(0),
        m_LargeParts(0),
        m_CurrentIdx(-1)
    {
        LOG3(("CCassBlobLoader::CCassBlobLoader max_retries=%d", max_retries));
    }

    void SetDataChunkCB(DataChunkCB_t &&  datacb)
    {
        m_DataCb = std::move(datacb);
    }

    void SetDataReadyCB(DataReadyCB_t  datareadycb, void *  data)
    {
        if (datareadycb && m_State != stInit)
            RAISE_ERROR(eSeqFailed,
                        "CCassBlobLoader: DataReadyCB can't be assigned "
                        "after the loading process has started");
        CCassBlobWaiter::SetDataReadyCB(datareadycb, data);
    }

    SBlobStat GetBlobStat(void) const
    {
        if (!m_StatLoaded)
            RAISE_ERROR(eSeqFailed, "CCassBlobLoader: Blob stat can't be read");
        return m_BlobStat;
    }

    uint64_t GetBlobSize(void) const
    {
        if (!m_StatLoaded)
            RAISE_ERROR(eSeqFailed, "CCassBlobLoader: Blob stat can't be read");
        return m_ExpectedSize;

    }

    void Cancel(void)
    {
        if (m_State == stDone)
            return;

        m_Cancelled = true;
        CloseAll();
        m_State = stError;
    }

    virtual bool Restart(unsigned int  max_retries = 0) override
    {
        m_StatLoaded = false;
        m_BlobStat.Reset();
        m_ExpectedSize = 0;
        m_RemainingSize = 0;
        m_LargeParts = 0;
        m_CurrentIdx = 0;
        return CCassBlobWaiter::Restart(max_retries);
    }

protected:
    virtual void Wait1(void) override;

private:
    enum State {
        stInit = CCassBlobWaiter::stInit,
        stReadingEntity,
        stReadingChunks,
        stCheckingFlags,
        stDone = CCassBlobWaiter::stDone,
        stError = CCassBlobWaiter::stError
    };

    bool                m_StatLoaded;
    SBlobStat           m_BlobStat;
    DataChunkCB_t       m_DataCb;
    int64_t             m_ExpectedSize;
    int64_t             m_RemainingSize;
    int32_t             m_LargeParts;
    int32_t             m_CurrentIdx;

    void RequestFlags(shared_ptr<CCassQuery>  qry, bool  with_data);
    void RequestChunk(shared_ptr<CCassQuery>  qry, int  local_id);
    void RequestChunksAhead(void);
};



class CCassBlobInserter: public CCassBlobWaiter
{
public:
    CCassBlobInserter(IdLogUtil::CAppOp *  op, unsigned int  op_timeout_ms,
                      shared_ptr<CCassConnection>  conn,
                      const string &  keyspace, int32_t  key, CBlob *  blob,
                      triple_t  is_new, int64_t  large_treshold,
                      int64_t  large_chunk_sz, bool  async,
                      unsigned int  max_retries,
                      const DataErrorCB_t &  data_error_cb) :
        CCassBlobWaiter(op, op_timeout_ms, conn, keyspace, key, async,
                        max_retries, data_error_cb),
        m_large_treshold(large_treshold),
        m_large_chunk_sz(large_chunk_sz),
        m_LargeParts(0),
        m_blob(blob),
        m_is_new(is_new),
        m_old_large_parts(0),
        m_old_flags(0)
    {}

protected:
    virtual void Wait1(void) override;

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

    int64_t         m_large_treshold;
    int64_t         m_large_chunk_sz;
    int32_t         m_LargeParts;
    CBlob *         m_blob;
    triple_t        m_is_new;
    int32_t         m_old_large_parts;
    int64_t         m_old_flags;
};



class CCassBlobDeleter: public CCassBlobWaiter
{
public:
    CCassBlobDeleter(IdLogUtil::CAppOp *  op, unsigned int  op_timeout_ms,
                     shared_ptr<CCassConnection>  conn,
                     const string &  keyspace, int32_t  key, bool  async,
                     unsigned int  max_retries, DataErrorCB_t  error_cb) :
        CCassBlobWaiter(op, op_timeout_ms, conn, keyspace, key,
                        async, max_retries, error_cb),
        m_LargeParts(0)
    {}

protected:
    virtual void Wait1(void) override;

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

    int32_t     m_LargeParts;
};


class CCassBlobOp: public enable_shared_from_this<CCassBlobOp>
{
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
        m_Conn(conn)
    {
        m_Keyspace = m_Conn->Keyspace();
    }

    ~CCassBlobOp()
    {
        m_Conn = NULL;
    }

    // MAIN API >
    void GetBlobChunkTresholds(unsigned int  op_timeout_ms,
                               int64_t *  LargeTreshold,
                               int64_t *  LargeChunkSize);
    void CassExecuteScript(const string &  scriptstr, CassConsistency  c);
    void CreateScheme(const string &  filename, const string &  keyspace);

    void SetKeyspace(const string &  keyspace)
    {
        m_Keyspace = keyspace;
    }

    string GetKeyspace(void) const
    {
        return m_Keyspace;
    }

    void LoadKeys(IdLogUtil::CAppOp &  op, CBlobFullStatMap *  keys,
                  function<void()> tick = nullptr);

    void GetBlob(IdLogUtil::CAppOp &  op, unsigned int  op_timeout_ms,
                 int32_t  key, unsigned int  max_retries,
                 SBlobStat *  blob_stat, const DataChunkCB_t &  data_chunk_cb);
    void GetBlobAsync(IdLogUtil::CAppOp &  op, unsigned int  op_timeout_ms,
                      int32_t  key, unsigned int  max_retries,
                      const DataChunkCB_t &  data_chunk_cb,
                      unique_ptr<CCassBlobWaiter> &  waiter);
    void InsertBlobAsync(IdLogUtil::CAppOp &  op, unsigned int  op_timeout_ms,
                         int32_t  key, unsigned int  max_retries,
                         CBlob *  blob_rslt, triple_t  is_new,
                         int64_t  LargeTreshold, int64_t  LargeChunkSz,
                         unique_ptr<CCassBlobWaiter> &  waiter);
    void DeleteBlobAsync(IdLogUtil::CAppOp &  op, unsigned int  op_timeout_ms,
                         int32_t  key, unsigned int  max_retries,
                         unique_ptr<CCassBlobWaiter> &  waiter);

    void UpdateBlobFlags(IdLogUtil::CAppOp &  op, unsigned int  op_timeout_ms,
                         int32_t  key, uint64_t  flags, flagop_t  flag_op);

    bool GetSetting(IdLogUtil::CAppOp &  op, unsigned int  op_timeout_ms,
                    const string &  name, string &  value);
    void UpdateSetting(IdLogUtil::CAppOp &  op, unsigned int  op_timeout_ms,
                       const string &  name, const string &  value);

    shared_ptr<CCassConnection> GetConn(void)
    {
        if (!m_Conn)
            RAISE_ERROR(eSeqFailed,
                        "CCassBlobOp instance is not initialized "
                        "with DB connection");
        return m_Conn;
    }
private:
    shared_ptr<CCassConnection>     m_Conn;
    string                          m_Keyspace;

    void LoadKeysScheduleNext(CCassQuery &  query, void *  data);
};

END_IDBLOB_SCOPE

#endif
