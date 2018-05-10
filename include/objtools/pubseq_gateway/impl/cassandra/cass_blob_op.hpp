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

#include <corelib/request_status.hpp>
#include <corelib/ncbidiag.hpp>

#include "cass_driver.hpp"
#include "cass_exception.hpp"
#include "Key.hpp"
#include "IdCassScope.hpp"
#include "cass_util.hpp"


BEGIN_IDBLOB_SCOPE
USING_NCBI_SCOPE;

class CCassBlobWaiter;

enum ECassTristate {
    eUnknown,
    eTrue,
    eFalse
};


typedef function<void(void *  context,
                      const unsigned char *  data,
                      unsigned int  size,
                      int  chunk_no)>               DataChunkCB_t;
typedef function<void(void *  context,
                      CRequestStatus::ECode  status,
                      int  code,
                      EDiagSev  severity,
                      const string &  message)>     DataErrorCB_t;
typedef void(*DataReadyCB_t)(void*);


class CCassBlobWaiter
{
public:
    CCassBlobWaiter(const CCassBlobWaiter&) = delete;
    CCassBlobWaiter& operator=(const CCassBlobWaiter&) = delete;
    CCassBlobWaiter(CCassBlobWaiter&&) = default;
    CCassBlobWaiter& operator=(CCassBlobWaiter&&) = default;

    CCassBlobWaiter(unsigned int  op_timeout_ms,
                    shared_ptr<CCassConnection>  conn,
                    const string &  keyspace, int32_t  key, bool  async,
                    unsigned int  max_retries, void *  context,
                    DataErrorCB_t  error_cb) :
        m_ErrorCb(error_cb),
        m_DataReadyCb(nullptr),
        m_DataReadyData(nullptr),
        m_Conn(conn),
        m_Keyspace(keyspace),
        m_Key(key),
        m_State(eInit),
        m_OpTimeoutMs(op_timeout_ms),
        m_LastActivityMs(gettime() / 1000L),
        m_RestartCounter(0),
        m_MaxRetries(max_retries), // 0 means no limit in auto-restart count,
                                   // any other positive value is the limit,
                                   // 1 means no 2nd start -> no re-starts at all
        m_Async(async),
        m_Cancelled(false),
        m_Context(context)
    {
        ERR_POST(Trace << "CCassBlobWaiter::CCassBlobWaiter this=" << this);
    }

    virtual ~CCassBlobWaiter()
    {
        ERR_POST(Trace << "CCassBlobWaiter::~CCassBlobWaiter this=" << this);
        CloseAll();
    }

    bool Wait(void)
    {
        while (m_State != eDone && m_State != eError && !m_Cancelled) {
            Wait1();
            if (m_Async)
                break;
        }
        return (m_State == eDone || m_State == eError);
    }

    virtual bool Restart(unsigned int  max_retries = 0)
    {
        CloseAll();
        m_State = eInit;
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
        return !m_LastError.empty();
    }

    string LastError(void) const
    {
        return m_LastError;
    }

    void ClearError(void)
    {
        m_LastError.clear();
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
        ERR_POST(Trace << "CCassBlobWaiter::SetDataReadyCB this=" << this <<
                 " CB: " << datareadycb << ", DATA: " << data);
        m_DataReadyCb = datareadycb;
        m_DataReadyData = data;
    }

    void * GetContext(void)
    {
        return m_Context;
    }

protected:
    enum EBlobWaiterState {
        eInit = 0,
        eDone = 10000,
        eError = -1
    };

    void CloseAll(void)
    {
        for (auto &  it: m_QueryArr) {
            it->Close();
        }
    }

    void Error(CRequestStatus::ECode  status,
               int  code,
               EDiagSev  severity,
               const string &  message)
    {
        assert(m_ErrorCb != nullptr);
        m_State = eError;
        m_LastError = message;
        m_ErrorCb(m_Context, status, code, severity, message);
    }

    bool IsTimedOut(void) const
    {
        if (m_OpTimeoutMs > 0)
            return ((gettime() / 1000L - m_LastActivityMs) >
                    m_OpTimeoutMs);
        return false;
    }

    bool CanRestart(void) const
    {
        bool    is_timedout = IsTimedOut();
        bool    is_out_of_retries = (m_MaxRetries > 0) &&
                                    (m_RestartCounter >= m_MaxRetries - 1);

        ERR_POST(Info << "CanRestart? t/o=" << is_timedout <<
                 ", o/r=" << is_out_of_retries <<
                 ", last_active=" << m_LastActivityMs <<
                 ", time=" << gettime() / 1000L <<
                 ", timeout=" << m_OpTimeoutMs);
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
        } catch (const CCassandraException& e) {
            if ((e.GetErrCode() == CCassandraException::eQueryTimeout ||
                 e.GetErrCode() == CCassandraException::eQueryFailedRestartable) && CanRestart()) {
                ERR_POST(Info << "In-place restart");
                ++m_RestartCounter;
                qry->Close();
                *restarted = true;
                m_State = restart_to_state;
            } else {
                Error(CRequestStatus::e502_BadGateway,
                      e.GetErrCode(), eDiag_Error, e.what());
            }
        }
        return false;
    }

    void UpdateLastActivity(void)
    {
        m_LastActivityMs = gettime() / 1000L;
    }

    bool CheckMaxActive(void);
    virtual void Wait1(void) = 0;

protected:
    DataErrorCB_t                   m_ErrorCb;
    DataReadyCB_t                   m_DataReadyCb;
    void *                          m_DataReadyData;
    shared_ptr<CCassConnection>     m_Conn;
    string                          m_Keyspace;
    int32_t                         m_Key;
    int32_t                         m_State;
    unsigned int                    m_OpTimeoutMs;
    int64_t                         m_LastActivityMs;
    string                          m_LastError;
    unsigned int                    m_RestartCounter;
    unsigned int                    m_MaxRetries;
    bool                            m_Async;
    bool                            m_Cancelled;
    void *                          m_Context;

    vector<shared_ptr<CCassQuery>>  m_QueryArr;
};



class CCassBlobLoader: public CCassBlobWaiter
{
public:
    CCassBlobLoader(const CCassBlobLoader&) = delete;
    CCassBlobLoader(CCassBlobLoader&&) = delete;
    CCassBlobLoader& operator=(const CCassBlobLoader&) = delete;
    CCassBlobLoader& operator=(CCassBlobLoader&&) = delete;

    CCassBlobLoader(unsigned int  op_timeout_ms,
                    shared_ptr<CCassConnection>  conn, const string &  keyspace,
                    int32_t  key, bool  async, unsigned int  max_retries,
                    void *  context,
                    const DataChunkCB_t &  data_chunk_cb,
                    const DataErrorCB_t &  DataErrorCB) :
        CCassBlobWaiter(op_timeout_ms, conn, keyspace, key,
                        async, max_retries, context, DataErrorCB),
        m_StatLoaded(false),
        m_DataCb(data_chunk_cb),
        m_ExpectedSize(0),
        m_RemainingSize(0),
        m_LargeParts(0)
    {
        ERR_POST(Info << "CCassBlobLoader::CCassBlobLoader max_retries=" <<
                 max_retries);
    }

    void SetDataChunkCB(DataChunkCB_t &&  datacb)
    {
        m_DataCb = std::move(datacb);
    }

    void SetDataReadyCB(DataReadyCB_t  datareadycb, void *  data)
    {
        if (datareadycb && m_State != eInit)
            NCBI_THROW(CCassandraException, eSeqFailed,
                       "CCassBlobLoader: DataReadyCB can't be assigned "
                       "after the loading process has started");
        CCassBlobWaiter::SetDataReadyCB(datareadycb, data);
    }

    SBlobStat GetBlobStat(void) const
    {
        if (!m_StatLoaded)
            NCBI_THROW(CCassandraException, eSeqFailed,
                       "CCassBlobLoader: Blob stat can't be read");
        return m_BlobStat;
    }

    uint64_t GetBlobSize(void) const
    {
        if (!m_StatLoaded)
            NCBI_THROW(CCassandraException, eSeqFailed,
                       "CCassBlobLoader: Blob stat can't be read");
        return m_ExpectedSize;

    }

    void Cancel(void)
    {
        if (m_State == eDone)
            return;

        m_Cancelled = true;
        CloseAll();
        m_State = eError;
    }

    virtual bool Restart(unsigned int  max_retries = 0) override
    {
        m_StatLoaded = false;
        m_BlobStat.Reset();
        m_ExpectedSize = 0;
        m_RemainingSize = 0;
        m_LargeParts = 0;
        return CCassBlobWaiter::Restart(max_retries);
    }

    int32_t GetTotalChunksInBlob(void) const
    {
        return m_LargeParts;
    }

protected:
    virtual void Wait1(void) override;

private:
    enum EBlobLoaderState {
        eInit = CCassBlobWaiter::eInit,
        eReadingEntity,
        eReadingChunks,
        eCheckingFlags,
        eDone = CCassBlobWaiter::eDone,
        eError = CCassBlobWaiter::eError
    };

    bool                m_StatLoaded;
    SBlobStat           m_BlobStat;
    DataChunkCB_t       m_DataCb;
    int64_t             m_ExpectedSize;
    int64_t             m_RemainingSize;
    int32_t             m_LargeParts;

    void x_RequestFlags(shared_ptr<CCassQuery>  qry, bool  with_data);
    void x_RequestChunk(shared_ptr<CCassQuery>  qry, int  local_id);
    void x_RequestChunksAhead(void);

    // Support for sending chunks as soon as they are ready regardless of the
    // order they are delivered from Cassandra
    vector<bool>        m_ProcessedChunks;
    void x_PrepareChunkRequests(void);
    int x_GetReadyChunkNo(bool &  have_inactive, bool &  need_repeat);
    bool x_AreAllChunksProcessed(void);
    void x_MarkChunkProcessed(size_t  chunk_no);
};



class CCassBlobInserter: public CCassBlobWaiter
{
public:
    CCassBlobInserter(unsigned int  op_timeout_ms,
                      shared_ptr<CCassConnection>  conn,
                      const string &  keyspace, int32_t  key, CBlob *  blob,
                      ECassTristate  is_new, int64_t  large_treshold,
                      int64_t  large_chunk_sz, bool  async,
                      unsigned int  max_retries, void *  context,
                      const DataErrorCB_t &  data_error_cb) :
        CCassBlobWaiter(op_timeout_ms, conn, keyspace, key, async,
                        max_retries, context, data_error_cb),
        m_LargeTreshold(large_treshold),
        m_LargeChunkSize(large_chunk_sz),
        m_LargeParts(0),
        m_Blob(blob),
        m_IsNew(is_new),
        m_OldLargeParts(0),
        m_OldFlags(0)
    {}

protected:
    virtual void Wait1(void) override;

private:
    enum EBlobInserterState {
        eInit = 0,
        eFetchOldLargeParts,
        eDeleteOldLargeParts,
        eWaitDeleteOldLargeParts,
        eInsert,
        eWaitingInserted,
        eUpdatingFlags,
        eWaitingUpdateFlags,
        eDone = CCassBlobWaiter::eDone,
        eError = CCassBlobWaiter::eError
    };

    int64_t         m_LargeTreshold;
    int64_t         m_LargeChunkSize;
    int32_t         m_LargeParts;
    CBlob *         m_Blob;
    ECassTristate   m_IsNew;
    int32_t         m_OldLargeParts;
    int64_t         m_OldFlags;
};



class CCassBlobDeleter: public CCassBlobWaiter
{
public:
    CCassBlobDeleter(unsigned int  op_timeout_ms,
                     shared_ptr<CCassConnection>  conn,
                     const string &  keyspace, int32_t  key, bool  async,
                     unsigned int  max_retries, void *  context,
                     DataErrorCB_t  error_cb) :
        CCassBlobWaiter(op_timeout_ms, conn, keyspace, key,
                        async, max_retries, context, error_cb),
        m_LargeParts(0)
    {}

protected:
    virtual void Wait1(void) override;

private:
    enum EBlobDeleterState {
        eInit = 0,
        eReadingEntity,
        eDeleteLargeEnt,
        eWaitLargeEnt,
        eDeleteEnt,
        eWaitingDone,
        eDone = CCassBlobWaiter::eDone,
        eError = CCassBlobWaiter::eError
    };

    int32_t     m_LargeParts;
};


class CCassBlobOp: public enable_shared_from_this<CCassBlobOp>
{
public:
    enum EBlopOpFlag {
        eFlagOpOr,
        eFlagOpAnd,
        eFlagOpSet
    };

public:
    CCassBlobOp(CCassBlobOp&&) = default;
    CCassBlobOp& operator=(CCassBlobOp&&) = default;
    CCassBlobOp(const CCassBlobOp&) = delete;
    CCassBlobOp& operator=(const CCassBlobOp&) = delete;

    CCassBlobOp(shared_ptr<CCassConnection> conn)
        : m_Conn(conn)
        , m_ExtendedSchema(false)
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

    void SetExtendedSchema(bool value)
    {
        m_ExtendedSchema = value;
    }

    bool IsExtendedSchema() const
    {
        return m_ExtendedSchema;
    }

    string GetKeyspace(void) const
    {
        return m_Keyspace;
    }

    void LoadKeys(CBlobFullStatMap *  keys,
                  function<void()> tick = nullptr);

    void GetBlob(unsigned int  op_timeout_ms,
                 int32_t  key, unsigned int  max_retries,
                 SBlobStat *  blob_stat, const DataChunkCB_t &  data_chunk_cb);
    void GetBlobAsync(unsigned int  op_timeout_ms,
                      int32_t  key, unsigned int  max_retries,
                      const DataChunkCB_t &  data_chunk_cb,
                      const DataErrorCB_t & error_cb,
                      unique_ptr<CCassBlobWaiter> &  waiter);
    void InsertBlobAsync(unsigned int  op_timeout_ms,
                         int32_t  key, unsigned int  max_retries,
                         CBlob *  blob_rslt, ECassTristate  is_new,
                         int64_t  LargeTreshold, int64_t  LargeChunkSz,
                         const DataErrorCB_t & error_cb,
                         unique_ptr<CCassBlobWaiter> &  waiter);
    void DeleteBlobAsync(unsigned int  op_timeout_ms,
                         int32_t  key, unsigned int  max_retries,
                         const DataErrorCB_t & error_cb,
                         unique_ptr<CCassBlobWaiter> &  waiter);

    void UpdateBlobFlags(unsigned int  op_timeout_ms,
                         int32_t  key, uint64_t  flags, EBlopOpFlag  flag_op);

    bool GetSetting(unsigned int  op_timeout_ms,
                    const string &  name, string &  value);
    void UpdateSetting(unsigned int  op_timeout_ms,
                       const string &  name, const string &  value);

    shared_ptr<CCassConnection> GetConn(void)
    {
        if (!m_Conn)
            NCBI_THROW(CCassandraException, eSeqFailed,
                       "CCassBlobOp instance is not initialized "
                       "with DB connection");
        return m_Conn;
    }

private:
    shared_ptr<CCassConnection>     m_Conn;
    string                          m_Keyspace;
    bool                            m_ExtendedSchema;

    void x_LoadKeysScheduleNext(CCassQuery &  query, void *  data);
};

END_IDBLOB_SCOPE

#endif
