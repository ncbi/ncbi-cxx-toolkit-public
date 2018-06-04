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
 * cassandra high-level functionality around blobs
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbireg.hpp>

#include "blob_task/insert_extended.hpp"
#include "blob_task/insert.hpp"
#include "blob_task/delete.hpp"
#include <objtools/pubseq_gateway/impl/cassandra/blob_task/load_blob.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/cass_blob_op.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/SyncObj.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/cass_driver.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/IdCassScope.hpp>

#include <unistd.h>
#include <algorithm>
#include <mutex>
#include <atomic>
#include <cassert>
#include <fstream>

BEGIN_IDBLOB_SCOPE
USING_NCBI_SCOPE;

#define SETTING_LARGE_TRESHOLD "LARGE_TRESHOLD"
#define SETTING_LARGE_CHUNK_SZ "LARGE_CHUNK_SZ"

#define ABS_MIN_LARGE_TRESHOLD  (4 * 1024)
#define ABS_MIN_LARGE_CHUNK_SZ  (4 * 1024)
#define DFLT_LARGE_TRESHOLD     (64 * 1024)
#define DFLT_LARGE_CHUNK_SZ     (512 * 1024)

#define ASYNC_QUEUE_TIMESLICE_MKS 300

#define KEYLOAD_SPLIT_COUNT 500
#define KEYLOAD_CONCURRENCY 512
#define KEYLOAD_CONSISTENCY CassConsistency::CASS_CONSISTENCY_LOCAL_QUORUM
#define KEYLOAD_PAGESIZE 4096

#define MAX_ACTIVE_STATEMENTS 512

static string KeySpaceDot(const string& keyspace)
{
    return keyspace.empty() ? keyspace : keyspace + ".";
}

/** CCassBlobWaiter */

bool CCassBlobWaiter::CheckMaxActive()
{
    return (m_Conn->GetActiveStatements() < MAX_ACTIVE_STATEMENTS);
}


/*****************************************************

                BLOB    READ

*****************************************************/

/* CCassBlobOp */

void CCassBlobOp::GetBlobChunkTresholds(unsigned int  op_timeout_ms,
                                        int64_t *     LargeTreshold,
                                        int64_t *     LargeChunkSize)
{
    string      s;

    if (!GetSetting(op_timeout_ms, SETTING_LARGE_TRESHOLD, s) ||
        !NStr::StringToNumeric(s, LargeTreshold) ||
        *LargeTreshold < ABS_MIN_LARGE_TRESHOLD) {
        *LargeTreshold = DFLT_LARGE_TRESHOLD;
        UpdateSetting(op_timeout_ms, SETTING_LARGE_TRESHOLD,
                      NStr::NumericToString(*LargeTreshold));
    }
    if (!GetSetting(op_timeout_ms, SETTING_LARGE_CHUNK_SZ, s) ||
        !NStr::StringToNumeric(s, LargeChunkSize) ||
        *LargeChunkSize < ABS_MIN_LARGE_CHUNK_SZ) {
        *LargeChunkSize = DFLT_LARGE_CHUNK_SZ;
        UpdateSetting(op_timeout_ms, SETTING_LARGE_CHUNK_SZ,
                      NStr::NumericToString(*LargeChunkSize));
    }
}

void CCassBlobOp::GetBlob(unsigned int  op_timeout_ms,
                          int32_t  key, unsigned int  max_retries,
                          SBlobStat *  blob_stat,
                          TBlobChunkCallback data_chunk_cb)
{
    string errmsg;
    bool is_error = false;

    CCassBlobLoader loader(
        op_timeout_ms, m_Conn, m_Keyspace, key, false, max_retries,
        move(data_chunk_cb),
        [&is_error, &errmsg](
            CRequestStatus::ECode status,
            int code,
            EDiagSev severity,
            const string & message
        ) {
            is_error = 1;
            errmsg = message;
        }
    );

    CCassConnection::Perform(op_timeout_ms, nullptr, nullptr,
        [&loader](bool is_repeated)
        {
            if (is_repeated)
                loader.Restart();
            bool b = loader.Wait();
            return b;
        }
    );

    if (is_error)
        RAISE_DB_ERROR(eQueryFailedRestartable, errmsg);
    if (blob_stat)
        *blob_stat = loader.GetBlobStat();
}


void CCassBlobOp::GetBlobAsync(unsigned int  op_timeout_ms,
                               int32_t  key, unsigned int  max_retries,
                               TBlobChunkCallback data_chunk_cb,
                               TDataErrorCallback error_cb,
                               unique_ptr<CCassBlobWaiter> &  Waiter)
{
    Waiter.reset(new CCassBlobLoader(
        op_timeout_ms, m_Conn, m_Keyspace,
        key, true, max_retries,
        move(data_chunk_cb), move(error_cb)
    ));
}


void CCassBlobOp::InsertBlobAsync(unsigned int  op_timeout_ms,
                                  int32_t  key, unsigned int max_retries,
                                  CBlobRecord *  blob_rslt, ECassTristate  is_new,
                                  int64_t  LargeTreshold, int64_t LargeChunkSz,
                                  TDataErrorCallback error_cb,
                                  unique_ptr<CCassBlobWaiter> &  Waiter)
{
    if (m_ExtendedSchema) {
        Waiter.reset(
            new CCassBlobTaskInsertExtended(
                op_timeout_ms, m_Conn, m_Keyspace,
                blob_rslt, true, max_retries,
                move(error_cb)
            )
        );
    }
    else {
        Waiter.reset(
            new CCassBlobTaskInsert(op_timeout_ms, m_Conn, m_Keyspace,
                key, blob_rslt, is_new, LargeTreshold,
                LargeChunkSz, true, max_retries,
                move(error_cb))
        );
    }
}


void CCassBlobOp::DeleteBlobAsync(unsigned int  op_timeout_ms,
                                  int32_t  key, unsigned int  max_retries,
                                  TDataErrorCallback error_cb,
                                  unique_ptr<CCassBlobWaiter> &  Waiter)
{
    Waiter.reset(new CCassBlobTaskDelete(
        op_timeout_ms, m_Conn, m_Keyspace, m_ExtendedSchema,
        key, true, max_retries, move(error_cb)
    ));
}

unique_ptr<CCassBlobTaskLoadBlob> CCassBlobOp::GetBlobExtended(
    unsigned int op_timeout_ms,
    unsigned int max_retries,
    CBlobRecord::TSatKey sat_key,
    bool load_chunks,
    TDataErrorCallback error_cb
) {
    return unique_ptr<CCassBlobTaskLoadBlob>(
        new CCassBlobTaskLoadBlob(
            op_timeout_ms,
            max_retries,
            m_Conn,
            m_Keyspace,
            sat_key,
            load_chunks,
            move(error_cb)
        )
    );
}

unique_ptr<CCassBlobTaskLoadBlob> CCassBlobOp::GetBlobExtended(
    unsigned int op_timeout_ms,
    unsigned int max_retries,
    CBlobRecord::TSatKey sat_key,
    CBlobRecord::TTimestamp modified,
    bool load_chunks,
    TDataErrorCallback error_cb
) {
    return unique_ptr<CCassBlobTaskLoadBlob>(
        new CCassBlobTaskLoadBlob(
            op_timeout_ms,
            max_retries,
            m_Conn,
            m_Keyspace,
            sat_key,
            modified,
            load_chunks,
            move(error_cb)
        )
    );
}


struct SLoadKeysContext
{
    atomic_ulong                                m_max_running;
    atomic_ulong                                m_running_count;
    atomic_ulong                                m_fetched;
    vector<pair<int64_t, int64_t>> const &      m_ranges;
    string                                      m_sql;
    TBlobFullStatVec *                          m_keys;
    CFutex                                      m_wakeup;

    SLoadKeysContext() = delete;
    SLoadKeysContext(SLoadKeysContext&&) = delete;
    SLoadKeysContext(const SLoadKeysContext&) = delete;

    SLoadKeysContext(unsigned long  max_running,
                     unsigned long  running_count,
                     unsigned long  fetched,
                     vector< pair<int64_t, int64_t> > const &  ranges,
                     string const &  sql,
                     TBlobFullStatVec *  keys,
                     function<void()>  tick) :
        m_max_running(max_running),
        m_running_count(running_count),
        m_fetched(fetched),
        m_ranges(ranges),
        m_sql(sql),
        m_keys(keys)
    {}

    int64_t getStep(size_t  range_id) const
    {
        int64_t     lower_bound = m_ranges[range_id].first;
        int64_t     upper_bound = m_ranges[range_id].second;

        if (upper_bound - lower_bound < KEYLOAD_SPLIT_COUNT) {
            return upper_bound - lower_bound;
        }

        return (upper_bound - lower_bound) / KEYLOAD_SPLIT_COUNT;
    }
};



struct SLoadKeysOnDataContext
{
    SLoadKeysContext & common;
    int64_t lower_bound;
    int64_t upper_bound;
    size_t range_offset;
    unsigned int run_id;
    unsigned int restart_count;
    atomic_bool data_triggered;

    SLoadKeysOnDataContext() = delete;
    SLoadKeysOnDataContext(SLoadKeysOnDataContext&&) = delete;
    SLoadKeysOnDataContext(const SLoadKeysOnDataContext&) = delete;

    SLoadKeysOnDataContext(
        SLoadKeysContext & v_common,
        int64_t v_lower_bound,
        int64_t v_upper_bound,
        size_t v_range_offset,
        unsigned int  v_run_id
    ) :
        common(v_common),
        lower_bound(v_lower_bound),
        upper_bound(v_upper_bound),
        range_offset(v_range_offset),
        run_id(v_run_id),
        restart_count(0),
        data_triggered(false)
    {}
};



static void InternalLoadKeys_ondata(CCassQuery &  query, void *  data)
{
    SLoadKeysOnDataContext * context = static_cast<SLoadKeysOnDataContext*>(data);

    ERR_POST(Trace << "Query[" << context->run_id << "] wake: " << &query);
    context->data_triggered = true;
    context->common.m_wakeup.Inc();
}

void CCassBlobOp::x_LoadKeysScheduleNext(CCassQuery & query, void * data)
{
    SLoadKeysOnDataContext * context = static_cast<SLoadKeysOnDataContext*>(data);
    bool restart_request = false;
    int32_t ent_current = 0;
    bool is_eof = false;

    try {
        using TRowType = SBlobFullStat;
        unsigned int counter = 0;
        async_rslt_t wr = ar_dataready;
        vector<TRowType> record_buffer;
        record_buffer.reserve(2 * KEYLOAD_PAGESIZE);
        while (wr == ar_dataready) {
            wr = query.NextRow();
            switch(wr) {
                case ar_dataready: {
                    auto & key_map = *(context->common.m_keys);
                    key_map.resize(key_map.size() + 1);
                    auto& row = key_map[key_map.size() - 1];
                    row.sat_key = ent_current = query.FieldGetInt32Value(0);
                    row.modified = query.FieldGetInt64Value(1, 0);
                    row.size = query.FieldGetInt64Value(2, 0);
                    row.flags = query.FieldGetInt64Value(3, 0);
                    ERR_POST(Trace << "CASS got key=" << query.GetConnection()->Keyspace() <<
                        ":" << ent_current << " mod=" << row.modified <<
                        " sz=" << row.size << " flags=" << row.flags
                    );
                    ++(context->common.m_fetched);
                    ++counter;
                    //ERR_POST(Message << "Fetched blob with key: " << key);
                    break;
                }
                case ar_wait:
                    ERR_POST(Trace << "Query[" << context->run_id << "] ar_wait:" << &query);
                    // expecting next data event triggered on this same query rowset
                    break;
                case ar_done:
                    is_eof = true;
                    break;
                default:
                    NCBI_THROW(CCassandraException, eGeneric, "CASS unexpected condition");
            }
        }
        context->restart_count = 0;

        ERR_POST(Trace << "Fetched records by one query: " << counter);
        if (!is_eof) {
            return;
        }
    } catch (const CCassandraException& e) {
        if ((
                e.GetErrCode() == CCassandraException::eQueryTimeout
                || e.GetErrCode() == CCassandraException::eQueryFailedRestartable
            )
            && context->restart_count < 5
        ) {
            restart_request = true;
        } else {
            ERR_POST(Info << "LoadKeys ... exception for entity: " << ent_current << ", " << e.what());
            throw;
        }
    } catch(...) {
        ERR_POST(Info << "LoadKeys ... exception for entity: " << ent_current);
        throw;
    }

    query.Close();

    if (restart_request) {
        ERR_POST(
            Info << "QUERY TIMEOUT! Offset " << context->lower_bound
            << " - " << context->upper_bound << ". Error count "
            << context->restart_count
        );
        query.SetSQL(context->common.m_sql, 2);
        query.BindInt64(0, context->lower_bound);
        query.BindInt64(1, context->upper_bound);
        query.Query(KEYLOAD_CONSISTENCY, true, true, KEYLOAD_PAGESIZE);
        ++(context->restart_count);
    } else {
        int64_t upper_bound = context->common.m_ranges[context->range_offset].second;
        int64_t step = context->common.getStep(context->range_offset);

        query.SetSQL(context->common.m_sql, 2);
        if (context->upper_bound < upper_bound) {
            int64_t new_upper_bound =
                (upper_bound - context->upper_bound > step)
                ? context->upper_bound + step
                : upper_bound;

            context->lower_bound = context->upper_bound;
            context->upper_bound = new_upper_bound;
            query.BindInt64(0, context->lower_bound);
            query.BindInt64(1, context->upper_bound);
            query.Query(KEYLOAD_CONSISTENCY, true, true, KEYLOAD_PAGESIZE);
            // ERR_POST(Info << "QUERY DATA " << context->range_offset <<
            //          " " << context->lower_bound << " - " <<
            //          context->upper_bound);
        } else {
            bool need_run = false;
            {
                if( (context->common.m_max_running + 1) < context->common.m_ranges.size()) {
                    context->range_offset = ++(context->common.m_max_running);
                    need_run = true;
                }
            }

            if (need_run) {
                int64_t     lower_bound = context->common.m_ranges[context->range_offset].first;
                int64_t     step = context->common.getStep(context->range_offset);

                context->lower_bound = lower_bound;
                context->upper_bound = lower_bound + step;
                query.BindInt64(0, context->lower_bound);
                query.BindInt64(1, context->upper_bound);
                query.Query(KEYLOAD_CONSISTENCY, true, true, KEYLOAD_PAGESIZE);
                // ERR_POST(Info << "QUERY DATA " << context->range_offset <<
                //          " " << context->lower_bound << " - " <<
                //          context->upper_bound);
            }
            else {
                query.SetOnData(nullptr, nullptr);
                --(context->common.m_running_count);
            }
        }
    }
}


void CCassBlobOp::LoadKeys(TBlobFullStatVec * keys,
                           function<void()>  tick)
{
    unsigned int run_id;
    vector< pair<int64_t, int64_t> > ranges;
    m_Conn->getTokenRanges(ranges);

    unsigned int concurrent = min<unsigned int>(
        KEYLOAD_CONCURRENCY,
        static_cast<unsigned int>(ranges.size())
    );
    if (!tick) {
        tick = [](){};
    }

    string common;
    string fsql;
    string sql;
    string error;
    if (m_ExtendedSchema) {
        common = "SELECT sat_key, last_modified, size, flags FROM " + KeySpaceDot(m_Keyspace) + "blob_prop WHERE";
        fsql = common + " TOKEN(sat_key) >= ? and TOKEN(sat_key) <= ?";
        sql = common + " TOKEN(sat_key) > ? and TOKEN(sat_key) <= ?";
    }
    else {
        common = "SELECT ent, modified, size, flags FROM " + KeySpaceDot(m_Keyspace) + "entity WHERE";
        fsql = common + " TOKEN(ent) >= ? and TOKEN(ent) <= ?";
        sql = common + " TOKEN(ent) > ? and TOKEN(ent) <= ?";
    }

    SLoadKeysContext loadkeys_context(0, concurrent, 0, ranges, sql, keys, tick);
    int val = loadkeys_context.m_wakeup.Value();
    vector<shared_ptr<CCassQuery>> queries;
    list<SLoadKeysOnDataContext> contexts;
    queries.resize(concurrent);

    try {
        for (run_id = 0; run_id < concurrent; ++run_id) {
            int64_t lower_bound = ranges[run_id].first;
            int64_t step = loadkeys_context.getStep(run_id);
            int64_t upper_bound = lower_bound + step;

            loadkeys_context.m_max_running = run_id;

            shared_ptr<CCassQuery> query(m_Conn->NewQuery());
            queries[run_id].swap(query);
            if (run_id == 0) {
                queries[run_id]->SetSQL(fsql, 2);
            } else {
                queries[run_id]->SetSQL(sql, 2);
            }
            contexts.emplace_back(loadkeys_context, lower_bound,
                                  upper_bound, run_id, run_id);

            SLoadKeysOnDataContext *    context = &contexts.back();
            queries[run_id]->BindInt64(0, lower_bound);
            queries[run_id]->BindInt64(1, upper_bound);
            queries[run_id]->SetOnData(InternalLoadKeys_ondata, context);
            queries[run_id]->Query(KEYLOAD_CONSISTENCY, true, true,
                                   KEYLOAD_PAGESIZE);
            // ERR_POST(Message << "QUERY DATA " << run_id << " " <<
            //          ranges[run_id].first << " - " << running[run_id].first);
        }

        string  error;
        while (!SSignalHandler::s_CtrlCPressed() &&
               loadkeys_context.m_running_count.load() > 0) {
            loadkeys_context.m_wakeup.WaitWhile(val);
            val = loadkeys_context.m_wakeup.Value();
            for (auto &  it: contexts) {
                run_id = it.run_id;
                if (!SSignalHandler::s_CtrlCPressed() && it.data_triggered) {
                    it.data_triggered = false;
                    x_LoadKeysScheduleNext(*(queries[run_id].get()), &it);
                }
            }
            tick();
        }
        if (SSignalHandler::s_CtrlCPressed())
            NCBI_THROW(CCassandraException, eFatal, "SIGINT delivered");
    } catch (const exception &  e) {    // don't re-throw, we have to wait until
                                        // all data events are triggered
        if (error.empty())
            error = e.what();
    }

    if (!error.empty()) {
        for (auto &  it: queries) {
            it->Close();
        }
        NCBI_THROW(CCassandraException, eGeneric, error);
    }

    ERR_POST(Info << "LoadKeys: finished");
}

/*****************************************************

                UPDATE    FLAGS

*****************************************************/

void CCassBlobOp::UpdateBlobFlags(unsigned int  op_timeout_ms,
                                  int32_t  key, uint64_t  flags,
                                  EBlopOpFlag  flag_op)
{
    CCassConnection::Perform(op_timeout_ms, nullptr, nullptr,
        [this, flags, flag_op, key](bool is_repeated) {
            int64_t                 new_flags = 0;
            string                  sql;
            shared_ptr<CCassQuery>  qry = m_Conn->NewQuery();

            switch (flag_op) {
                case eFlagOpOr:
                case eFlagOpAnd: {
                    sql = "SELECT flags FROM " + KeySpaceDot(m_Keyspace) +
                          "entity WHERE ent = ?";
                    qry->SetSQL(sql, 1);
                    qry->BindInt32(0, key);
                    qry->Query(CASS_CONSISTENCY_LOCAL_QUORUM);
                    if (!qry->IsEOF() && qry->NextRow() == ar_dataready) {
                        switch (flag_op) {
                            case eFlagOpOr:
                                new_flags = qry->FieldGetInt64Value(0) | flags;
                                break;
                            case eFlagOpAnd:
                                new_flags = qry->FieldGetInt64Value(0) & flags;
                                break;
                            default:
                                NCBI_THROW(CCassandraException, eFatal,
                                           "Unexpected flag operation");
                        }
                        qry->Close();
                    }
                    break;
                }
                case eFlagOpSet:
                    new_flags = flags;
                    break;
                default:
                    NCBI_THROW(CCassandraException, eFatal,
                               "Unexpected flag operation");
            }
            sql = "UPDATE " + KeySpaceDot(m_Keyspace) +
                  "entity SET flags = ? WHERE ent = ?";
            qry->SetSQL(sql, 2);
            qry->BindInt64(0, new_flags);
            qry->BindInt32(1, key);
            qry->Execute(CASS_CONSISTENCY_LOCAL_QUORUM);
            return true;
        }
    );
}

void CCassBlobOp::UpdateBlobFlagsExtended(
    unsigned int op_timeout_ms,
    CBlobRecord::TSatKey key,
    EBlobFlags flag,
    bool set_flag
) {
    CCassConnection::Perform(
        op_timeout_ms, nullptr, nullptr,
        [this, key, flag,  set_flag] (bool) -> bool {
            int64_t new_flags = 0;
            CBlobRecord::TTimestamp last_modified;
            shared_ptr<CCassQuery> qry = m_Conn->NewQuery();
            string sql = "SELECT last_modified, flags FROM " + KeySpaceDot(m_Keyspace) + "blob_prop WHERE sat_key = ? limit 1";
            qry->SetSQL(sql, 1);
            qry->BindInt32(0, key);
            qry->Query(CASS_CONSISTENCY_LOCAL_QUORUM);
            if (!qry->IsEOF() && qry->NextRow() == ar_dataready) {
                last_modified = qry->FieldGetInt64Value(0);
                if (set_flag) {
                    new_flags = qry->FieldGetInt64Value(1) | static_cast<TBlobFlagBase>(flag);
                }
                else {
                    new_flags = qry->FieldGetInt64Value(1) & ~(static_cast<TBlobFlagBase>(flag));
                }
                qry->Close();
            }
            else {
                return false;
            }
            sql = "UPDATE " + KeySpaceDot(m_Keyspace) + "blob_prop SET flags = ? WHERE sat_key = ? and last_modified = ?";
            qry->SetSQL(sql, 3);
            qry->BindInt64(0, new_flags);
            qry->BindInt32(1, key);
            qry->BindInt64(2, last_modified);
            qry->Execute(CASS_CONSISTENCY_LOCAL_QUORUM);
            return true;
        }
    );
}

/*****************************************************

                IN-TABLE    SETTINGS

*****************************************************/

void CCassBlobOp::UpdateSetting(unsigned int  op_timeout_ms,
                                const string &  name, const string &  value)
{
    CCassConnection::Perform(op_timeout_ms, nullptr, nullptr,
        [this, name, value](bool is_repeated) {
            string      sql;

            sql = "INSERT INTO " + KeySpaceDot(m_Keyspace) +
                  "settings (name, value) VALUES(?, ?)";
            shared_ptr<CCassQuery>qry(m_Conn->NewQuery());
            qry->SetSQL(sql, 2);
            ERR_POST(Trace << "InternalUpdateSetting: " << name << "=>" <<
                     value << ", " << sql);
            qry->BindStr(0, name);
            qry->BindStr(1, value);
            qry->Execute(CASS_CONSISTENCY_LOCAL_QUORUM, false, false);
            return true;
        }
    );
}


bool CCassBlobOp::GetSetting(unsigned int  op_timeout_ms,
                             const string &  name, string &  value)
{
    bool rslt = false;
    CCassConnection::Perform(op_timeout_ms, nullptr, nullptr,
        [this, name, &value, &rslt](bool is_repeated) {
            string sql;
            sql = "SELECT value FROM " + KeySpaceDot(m_Keyspace) +
                  "settings WHERE name=?";
            shared_ptr<CCassQuery>qry(m_Conn->NewQuery());
            ERR_POST(Trace << "InternalGetSetting: " << name << ": " << sql);
            qry->SetSQL(sql, 1);
            qry->BindStr(0, name);

            CassConsistency cons = is_repeated &&
                                       m_Conn->GetFallBackRdConsistency() ?
                                            CASS_CONSISTENCY_LOCAL_ONE :
                                            CASS_CONSISTENCY_LOCAL_QUORUM;
            qry->Query(cons, false, false);
            async_rslt_t rv = qry->NextRow();
            if (rv == ar_dataready) {
                qry->FieldGetStrValue(0, value);
                rslt = true;
            }
            return true;
        }
    );

    return rslt;
}


END_IDBLOB_SCOPE
