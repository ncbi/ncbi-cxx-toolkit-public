#ifndef OBJTOOLS__PUBSEQ_GATEWAY__IMPL__CASSANDRA__CASSBLOBOP_HPP
#define OBJTOOLS__PUBSEQ_GATEWAY__IMPL__CASSANDRA__CASSBLOBOP_HPP

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

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "cass_driver.hpp"
#include "cass_exception.hpp"
#include "IdCassScope.hpp"
#include "cass_util.hpp"
#include "blob_record.hpp"
#include "nannot/record.hpp"
#include "id2_split/record.hpp"
#include "acc_ver_hist/record.hpp"

BEGIN_IDBLOB_SCOPE
USING_NCBI_SCOPE;

using TBlobChunkCallback = function<void(const unsigned char * data, unsigned int size, int chunk_no)>;
using TDataErrorCallback = function<void(CRequestStatus::ECode status, int code, EDiagSev severity, const string & message)>;
using TLoggingCallback = function<void(EDiagSev severity, const string & message)>;
using TDataReadyCallback = void(*)(void*);

class CCassBlobWaiter
{
protected:
    CCassBlobWaiter(const CCassBlobWaiter& other)
        : m_ErrorCb(other.m_ErrorCb)
        , m_Conn(other.m_Conn)
        , m_QueryTimeout(other.m_QueryTimeout)
        , m_Async(other.m_Async)
        , m_Keyspace(other.m_Keyspace)
        , m_Key(other.m_Key)
        , m_MaxRetries(other.m_MaxRetries)
    {}
public:
    CCassBlobWaiter& operator=(const CCassBlobWaiter&) = delete;
    CCassBlobWaiter(CCassBlobWaiter&&) = delete;
    CCassBlobWaiter& operator=(CCassBlobWaiter&&) = delete;

    CCassBlobWaiter(
        shared_ptr<CCassConnection> conn,
        const string & keyspace,
        int32_t key,
        bool async,
        TDataErrorCallback error_cb
    )
      : m_ErrorCb(std::move(error_cb))
      , m_Conn(std::move(conn))
      , m_Async(async)
      , m_Keyspace(keyspace)
      , m_Key(key)
    {
        if (m_Conn == nullptr) {
            NCBI_THROW(CCassandraException, eFatal, "CCassBlobWaiter() Cassandra connection should not be nullptr");
        }
    }

    CCassBlobWaiter(
        shared_ptr<CCassConnection> conn,
        const string & keyspace,
        bool async,
        TDataErrorCallback error_cb
    )
      : m_ErrorCb(std::move(error_cb))
      , m_Conn(std::move(conn))
      , m_Async(async)
      , m_Keyspace(keyspace)
    {
        if (m_Conn == nullptr) {
            NCBI_THROW(CCassandraException, eFatal, "CCassBlobWaiter() Cassandra connection should not be nullptr");
        }
    }

    /// Experimental!!! May conflict with CCassConnection::SetQueryTimeoutRetry() when query timed out
    ///    and CCassQuery::RestartQuery() called to make another attempt
    ///    Currently required to test PubSeqGateway Casandra timeouts handling for multi-stage operations
    ///    (Resolution => Primary blob retrieval => ID2 split blob retrieval) when Cassandra timeout happens at
    ///    later stages of operation
    ///
    /// Setup individual operation timeout instead of using timeout configured for CCassConnection
    virtual void SetQueryTimeout(std::chrono::milliseconds value);
    virtual std::chrono::milliseconds GetQueryTimeout() const;

    virtual ~CCassBlobWaiter()
    {
        CloseAll();
    }

    bool Cancelled() const
    {
        return m_Cancelled;
    }

    bool Finished() const noexcept
    {
        auto state = m_State.load();
        return state == eDone || state == eError || m_Cancelled;
    }

    /// Get internal state for debug purposes.
    int32_t GetStateDebug() const noexcept
    {
        return m_State.load(std::memory_order_relaxed);
    }

    virtual void Cancel()
    {
        if (m_State != eDone) {
            m_Cancelled = true;
            m_State = eError;
        }
    }

    bool Wait()
    {
        bool finished = Finished();
        while (!finished) {
            try {
                Wait1();
            } catch (const CCassandraException& e) {
                // We will not re-throw here as CassandraException is not fatal
                Error(CRequestStatus::e500_InternalServerError, e.GetErrCode(), eDiag_Error, e.what());
            } catch (const exception& e) {
                // See ID-6241 There is a requirement to catch all exceptions and continue here
                Error(CRequestStatus::e500_InternalServerError, CCassandraException::eUnknown, eDiag_Error, e.what());
            } catch (...) {
                // See ID-6241 There is a requirement to catch all exceptions and continue here
                Error(CRequestStatus::e500_InternalServerError, CCassandraException::eUnknown, eDiag_Error, "Unknown exception");
            }
            finished = Finished();
            if (m_Async) {
                break;
            }
        }
        return finished;
    }

    bool HasError() const
    {
        return !m_LastError.empty();
    }

    string LastError() const
    {
        return m_LastError;
    }

    void ClearError()
    {
        m_LastError.clear();
    }

    string GetConnectionDatacenterName()
    {
        return m_Conn ? m_Conn->GetDatacenterName() : "";
    }

    string GetKeySpace() const
    {
        return m_Keyspace;
    }

    void SetKeySpace(string const & keyspace) {
        if (m_State != eInit) {
            NCBI_THROW(CCassandraException, eSeqFailed, "CCassBlobWaiter: Cannot change keyspace for started task");
        }
        m_Keyspace = keyspace;
    }

    int32_t GetKey() const
    {
        return m_Key;
    }

    void SetErrorCB(TDataErrorCallback error_cb)
    {
        m_ErrorCb = std::move(error_cb);
    }

    void SetLoggingCB(TLoggingCallback logging_cb)
    {
        m_LoggingCb = std::move(logging_cb);
    }

    /// Set connection point parameters.
    ///
    /// @param value
    ///   Max number of query retries operation allows.
    ///   < 0 means not configured. Will use value provided by CCassConnection
    ///   0 means no limit in auto-restart count,
    ///   1 means no 2nd start -> no re-starts at all
    ///   n > 1 means n-1 restart allowed
    void SetMaxRetries(int value)
    {
        m_MaxRetries = value < 0 ? -1 : value;
    }

    int GetMaxRetries() const
    {
        return m_MaxRetries < 0 ? m_Conn->GetMaxRetries() : m_MaxRetries;
    }

    void SetDataReadyCB3(shared_ptr<CCassDataCallbackReceiver> datareadycb3)
    {
        m_DataReadyCb3 = datareadycb3;
    }

    void SetReadConsistency(TCassConsistency value)
    {
        m_ReadConsistency = value;
    }

    void SetWriteConsistency(TCassConsistency value)
    {
        m_WriteConsistency = value;
    }

protected:
    enum EBlobWaiterState {
        eInit = 0,
        eDone = 10000,
        eError = -1
    };
    struct SQueryRec {
        shared_ptr<CCassQuery> query{nullptr};
        unsigned int restart_count{0};
    };

    void CloseAll()
    {
        for (auto & it : m_QueryArr) {
            it.query->Close();
            it.restart_count = 0;
        }
    }

    void SetupQueryCB3(shared_ptr<CCassQuery>& query)
    {
        auto DataReadyCb3 = m_DataReadyCb3.lock();
        if (DataReadyCb3) {
            query->SetOnData3(DataReadyCb3);
        } else if (IsDataReadyCallbackExpired()) {
            string msg{"Failed to setup data ready callback (expired)"};
            Error(CRequestStatus::e502_BadGateway, CCassandraException::eUnknown, eDiag_Error, msg);
        }
    }

    // Returns true for expired non empty weak pointers
    bool IsDataReadyCallbackExpired() const
    {
        using wt = weak_ptr<CCassDataCallbackReceiver>;
        return m_DataReadyCb3.owner_before(wt{}) || wt{}.owner_before(m_DataReadyCb3);
    }

    void Error(CRequestStatus::ECode status, int code, EDiagSev severity, const string & message)
    {
        m_State = eError;
        m_LastError = message;
        if (m_ErrorCb) {
            m_ErrorCb(status, code, severity, message);
        }
    }

    void Message(EDiagSev severity, const string & message) const
    {
        if (m_LoggingCb) {
            m_LoggingCb(severity, message);
        }
    }

    bool CanRestart(shared_ptr<CCassQuery> const& query, unsigned int restart_count) const
    {
        int max_retries = GetMaxRetries();
        bool is_out_of_retries = (max_retries > 0) &&
                                 (restart_count >= static_cast<unsigned int>(max_retries) - 1);
        return !is_out_of_retries && !m_Cancelled;
    }

    // @ToDo Switch all child classes to use this method to produce CCassQuery
    shared_ptr<CCassQuery> ProduceQuery() const;

    bool CanRestart(SQueryRec& it) const
    {
        return CanRestart(it.query, it.restart_count);
    }

    bool CheckReady(shared_ptr<CCassQuery> const& qry, unsigned int restart_counter, bool& need_repeat)
    {
        need_repeat = false;
        try {
            if (m_Async && qry->WaitAsync(0) == ar_wait) {
                return false;
            }
            return true;
        } catch (const CCassandraException& e) {
            vector<string> query_params;
            for (size_t i = 0; i < qry->ParamCount(); ++i) {
                query_params.push_back(qry->ParamAsStrForDebug(static_cast<int>(i)));
            }
            string retry_message = "CassandraQueryRetry: SQL - " + NStr::Quote(qry->GetSQL(), '\'');
            if (!query_params.empty()) {
                retry_message += "; params - (" + NStr::Join(query_params, ",") + ")";
            }
            retry_message += "; previous_retries=" + to_string(restart_counter)
                + "; max_retries=" + to_string(GetMaxRetries())
                + "; error_code=" + e.GetErrCodeString();
            if (e.GetCassDriverErrorCode() >= 0) {
                retry_message += "; driver_error=0x" + NStr::NumericToString(e.GetCassDriverErrorCode(), 0, 16);
            }
            bool driver_error_allows_restart = (e.GetErrCode() == CCassandraException::eQueryTimeout
                                         || e.GetErrCode() == CCassandraException::eQueryFailedRestartable);
            if (driver_error_allows_restart && CanRestart(qry, restart_counter)) {
                Message(eDiag_Warning, retry_message + "; decision=retry_allowed");
                need_repeat = true;
            }
            else {
                Message(eDiag_Error, retry_message + "; decision=retry_forbidden");
                Error(CRequestStatus::e502_BadGateway, e.GetErrCode(), eDiag_Error, e.what());
            }
        }
        return false;
    }

    bool CheckReady(SQueryRec& it)
    {
        bool need_restart = false;
        bool rv = CheckReady(it.query, it.restart_count, need_restart);
        if (!rv && need_restart) {
            try {
                ++it.restart_count;
                it.query->Restart();
            }
            catch (const exception& ex) {
                Message(eDiag_Error, string("CassandraQueryRestart thrown exception with message: ") + ex.what());
                throw;
            }
        }
        return rv;
    }

    TCassConsistency GetReadConsistency() const
    {
        return m_ReadConsistency;
    }

    TCassConsistency GetWriteConsistency() const
    {
        return m_WriteConsistency;
    }

    bool CheckMaxActive();
    virtual void Wait1() = 0;

    TDataErrorCallback              m_ErrorCb{nullptr};
    TLoggingCallback                m_LoggingCb{nullptr};
    weak_ptr<CCassDataCallbackReceiver> m_DataReadyCb3;

    // @ToDo Make it private with protected accessor
    shared_ptr<CCassConnection>     m_Conn;

    std::chrono::milliseconds       m_QueryTimeout{0};
    atomic<int32_t>                 m_State{eInit};
    string                          m_LastError;
    bool                            m_Async{true};
    atomic_bool                     m_Cancelled{false};
    vector<SQueryRec>               m_QueryArr;

private:
    string                          m_Keyspace;
    int32_t                         m_Key{0};
    int                             m_MaxRetries{-1};

    TCassConsistency                m_ReadConsistency{CCassConsistency::kLocalQuorum};
    TCassConsistency                m_WriteConsistency{CCassConsistency::kLocalQuorum};
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

    explicit CCassBlobOp(shared_ptr<CCassConnection> conn)
        : m_Conn(std::move(conn))
    {
        m_Keyspace = m_Conn->Keyspace();
    }

    virtual ~CCassBlobOp()
    {
        m_Conn = nullptr;
    }

    NCBI_STD_DEPRECATED("GetBlobChunkSize() is deprecated and will be deleted after 08/01/2023")
    void GetBlobChunkSize(unsigned int timeout_ms, const string & keyspace, int64_t * chunk_size);

    NCBI_STD_DEPRECATED("GetBigBlobSizeLimit() is deprecated and will be deleted after 08/01/2023")
    void GetBigBlobSizeLimit(unsigned int timeout_ms, const string & keyspace, int64_t * value);

    /// It is unsafe to use this function.
    /// Call should be protected in multi-threaded environment.
    void SetKeyspace(const string &  keyspace)
    {
        m_Keyspace = keyspace;
    }

    string GetKeyspace() const
    {
        return m_Keyspace;
    }

    NCBI_STD_DEPRECATED("GetSetting() is deprecated and will be deleted after 08/01/2023")
    bool GetSetting(unsigned int op_timeout_ms, const string & domain, const string & name, string & value);

    shared_ptr<CCassConnection> GetConn()
    {
        if (!m_Conn) {
            NCBI_THROW(CCassandraException, eSeqFailed, "CCassBlobOp instance is not initialized with DB connection");
        }
        return m_Conn;
    }

private:
    shared_ptr<CCassConnection> m_Conn{nullptr};
    string m_Keyspace;
};

END_IDBLOB_SCOPE

#endif  // OBJTOOLS__PUBSEQ_GATEWAY__IMPL__CASSANDRA__CASSBLOBOP_HPP
