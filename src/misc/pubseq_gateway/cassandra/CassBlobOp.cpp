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

#include <algorithm>
#include <mutex>
#include <atomic>
#include <cassert>

#include <corelib/ncbireg.hpp>

#include "IdLogUtil/AppPerf.hpp"
#include "IdLogUtil/IdLogUtl.hpp"
#include "Cass/CassDriver.hpp"
#include "CassBlobOp.hpp"
#include "IdCassScope.hpp"
#include "SyncObj.hpp"

BEGIN_IDBLOB_SCOPE
USING_NCBI_SCOPE;
using namespace IdLogUtil;

#define SETTING_LARGE_TRESHOLD "LARGE_TRESHOLD"
#define SETTING_LARGE_CHUNK_SZ "LARGE_CHUNK_SZ"

#define ABS_MIN_LARGE_TRESHOLD  (4 * 1024)
#define ABS_MIN_LARGE_CHUNK_SZ  (4 * 1024)
#define DFLT_LARGE_TRESHOLD     (64 * 1024)
#define DFLT_LARGE_CHUNK_SZ     (512 * 1024)

#define ASYNC_QUEUE_TIMESLICE_MKS 300

#define KEYLOAD_SPLIT_COUNT 1000
#define KEYLOAD_CONCURRENCY 512
#define KEYLOAD_CONSISTENCY CassConsistency::CASS_CONSISTENCY_LOCAL_QUORUM
#define KEYLOAD_PAGESIZE 4096

#define MAX_ACTIVE_STATEMENTS 512
#define MAX_CHUNKS_AHEAD      4



static string KeySpaceDot(const string& keyspace) {
	return keyspace.empty() ? keyspace : keyspace + ".";
}


/** CCassBlobWaiter */

bool CCassBlobWaiter::CheckMaxActive() {
    return (m_conn->GetActiveStatements() < MAX_ACTIVE_STATEMENTS);
}


/*****************************************************

				BLOB    READ
  
*****************************************************/

/* CCassBlobLoader */

void CCassBlobLoader::RequestChunk(shared_ptr<CCassQuery> qry, int local_id) {
	CassConsistency c = (m_restart_counter > 0 && m_conn->GetFallBackRdConsistency()) ? CASS_CONSISTENCY_LOCAL_QUORUM : CASS_CONSISTENCY_LOCAL_ONE;
	string sql = "SELECT data FROM " + KeySpaceDot(m_keyspace) + "largeentity WHERE ent = ? AND local_id = ?";
	LOG5(("reading LARGE blob part, key=%s.%d, local_id=%d, qry: %p", m_keyspace.c_str(), m_key, local_id, LOG_SPTR(qry)));
	qry->SetSQL(sql, 2);
	qry->BindInt32(0, m_key);
	qry->BindInt32(1, local_id);
    if (m_data_ready_cb)
        qry->SetOnData2(m_data_ready_cb, m_data_ready_data);
    UpdateLastActivity();
	qry->Query(c, true);
}

void CCassBlobLoader::RequestFlags(shared_ptr<CCassQuery> qry, bool with_data) {
    CassConsistency c = (m_restart_counter > 0 && m_conn->GetFallBackRdConsistency()) ? CASS_CONSISTENCY_LOCAL_QUORUM : CASS_CONSISTENCY_LOCAL_ONE;
    string sql;
    if (with_data)
        sql = "SELECT modified, flags, size, large_parts, data FROM " + KeySpaceDot(m_keyspace) + "entity WHERE ent = ?";
    else
        sql = "SELECT modified, flags, size, large_parts       FROM " + KeySpaceDot(m_keyspace) + "entity WHERE ent = ?";

    qry->SetSQL(sql, 1);
    qry->BindInt32(0, m_key);
    if (m_data_ready_cb)
        qry->SetOnData2(m_data_ready_cb, m_data_ready_data);
    UpdateLastActivity();
    qry->Query(c, m_async);
}

void CCassBlobLoader::RequestChunksAhead() {
    int32_t local_id;
    int cnt = MAX_CHUNKS_AHEAD;
    for (local_id = m_current_idx; local_id < m_large_parts && cnt > 0; ++local_id, --cnt) {
        auto qry = m_query_arr[local_id];
        if (!qry->IsActive()) {
            if (!CheckMaxActive())
                break;
            RequestChunk(qry, local_id);
        }
    }
}


void CCassBlobLoader::Wait1() {
	char msg[1024];
    msg[0] = '\0';
    bool b_need_repeat;

    if (m_cancelled) {
        CloseAll();
        return;
    }

    do {
        b_need_repeat = false;
        switch (m_state) {
            case stError:
            case stDone:
                return;

            case stInit: {
                CloseAll();
                if (m_query_arr.size() == 0)
                    m_query_arr.emplace_back(m_conn->NewQuery());
                auto qry = m_query_arr[0];
                if (!CheckMaxActive())
                    break;
                RequestFlags(qry, true);
                m_state = stReadingEntity;
                break;
            }

            case stReadingEntity: {
                auto qry = m_query_arr[0];
                if (!qry->IsActive()) {
                    LOG2(("re-starting initial query"));
                    if (!CheckMaxActive())
                        break;
                    RequestFlags(qry, true);
                }

                if (!CheckReady(qry, stInit, &b_need_repeat)) {
                    if (b_need_repeat)
                        LOG3(("Restart stReadingEntity key=%s.%d", m_keyspace.c_str(), m_key));
                    break;
                }

                if (qry->IsEOF()) {
                    UpdateLastActivity();
                    CloseAll();
                    m_state = stDone; // no data
                }
                else {
                    async_rslt_t wr = qry->NextRow();
                    if (wr == ar_wait) // paging
                        break;
                    else if (wr == ar_dataready) {
                        UpdateLastActivity();
                        const unsigned char* rawdata = nullptr;
                        m_blob_stat.modified = qry->FieldGetInt64Value(0);
                        m_blob_stat.flags = qry->FieldGetInt64Value(1);
                        m_expected_size = m_remaining_size = qry->FieldGetInt64Value(2);
                        m_large_parts = qry->FieldGetInt32Value(3, 0);
                        int64_t len = qry->FieldGetBlobRaw(4, &rawdata);					
                        m_remaining_size -= len;
                        m_stat_loaded = true;
                        if ((m_blob_stat.flags & (bfCheckFailed | bfComplete)) != bfComplete) {
                            snprintf(msg, sizeof(msg), "Blob failed check or it's incomplete (key=%s.%d, flags=0x%lx)", m_keyspace.c_str(), m_key, m_blob_stat.flags);
                            Error(msg);
                            break;
                        }

                        LOG5(("BD blob fetching, key=%s.%d, sz: %ld, EOF: %d, large_parts: %d", m_keyspace.c_str(), m_key, len, qry->IsEOF(), m_large_parts));

                        if (m_large_parts == 0) {
                            if (m_remaining_size != 0) {
                                snprintf(msg, sizeof(msg), "Size mismatch for (key=%s.%d), expected: %ld, actual: %ld (singlechunk)", m_keyspace.c_str(), m_key, m_expected_size, len);
                                Error(msg);
                            }
                            else {
                                m_state = stDone;
                                m_data_cb(rawdata, len, 0, true);
                            }
                            break;
                        }
                        else { // multi-chunk
                            m_query_arr.reserve(m_large_parts + 1);
                            CloseAll();
                            while ((int)m_query_arr.size() < m_large_parts)
                                m_query_arr.emplace_back(m_conn->NewQuery());
                            m_current_idx = 0;
                            RequestChunksAhead();
                            m_state = stReadingChunks;
                        }
                    }
                    else {
                        UpdateLastActivity();
                        m_state = stDone; // no data
                    }
                }
                break;
            }

            case stReadingChunks: {
                int32_t local_id;
                if (m_current_idx >= 0 && m_current_idx < m_large_parts) {

                    local_id = m_current_idx;
                    while (local_id < m_large_parts) {
                        RequestChunksAhead();
                        auto qry = m_query_arr[m_current_idx];
                        if (!qry->IsActive()) {
                            if (m_async)
                                return; // wasn't activated b'ze too many active statements
                            else {
                                usleep(1000);
                                continue;
                            }
                        }

                        if (!CheckReady(qry, stReadingChunks, &b_need_repeat)) {
                            if (b_need_repeat) {
                                LOG3(("Restart stReadingChunks key=%s.%d, chunk=%d", m_keyspace.c_str(), m_key, local_id));
                                continue;
                            }
                            else
                                return;
                        }

                        async_rslt_t wr = ar_done;
                        if (!qry->IsEOF()) {
                            wr = qry->NextRow();
                            if (wr == ar_wait) // paging
                                return;
                            else if (wr == ar_dataready) {
                                UpdateLastActivity();
                                const unsigned char* rawdata = nullptr;
                                int64_t len = qry->FieldGetBlobRaw(0, &rawdata);
                                m_remaining_size -= len;
                                if (m_remaining_size < 0) {
                                    snprintf(msg, sizeof(msg), "Failed to fetch blob chunk (key=%s.%d, chunk=%d) size %ld is too large", m_keyspace.c_str(), m_key, local_id, len);
                                    Error(msg);
                                    return;
                                }
                                m_current_idx++;
                                local_id++;
                                m_data_cb(rawdata, len, local_id, false);
                                RequestChunksAhead();
                                continue; // continue with next m_current_idx
                            }
                        }

                        assert(qry->IsEOF() || wr == ar_done);
                        if (CanRestart()) {
                            LOG3(("Restarting key=%s.%d, chunk=%d p2", m_keyspace.c_str(), m_key, local_id));
                            qry->Close();
                            continue;
                        }
                        else {
                            snprintf(msg, sizeof(msg), "Failed to fetch blob chunk (key=%s.%d, chunk=%d) wr=%d", m_keyspace.c_str(), m_key, local_id, static_cast<int>(wr));
                            Error(msg);
                            return;
                        }
                    }
                }

                if (m_current_idx >= m_large_parts && m_state != stError && m_state != stDone) {
                    if (m_remaining_size > 0) {
                        snprintf(msg, sizeof(msg), "Failed to fetch blob (key=%s.%d) result is imcomplete remaining %ldbytes", m_keyspace.c_str(), m_key, m_remaining_size);
                        Error(msg);
                        break;
                    }

                    while (m_query_arr.size() < (size_t)m_large_parts + 1)
                        m_query_arr.emplace_back(m_conn->NewQuery());
                    auto qry = m_query_arr[m_large_parts];

                    if (!CheckMaxActive())
                        break;
                    RequestFlags(qry, false);
                    m_state = stCheckingFlags;
                }
                break;
            }

            case stCheckingFlags: {
                bool has_error = false;

                auto qry = m_query_arr[m_large_parts];

                if (!qry->IsActive()) {
                    LOG3(("running checkflag query"));
                    if (!CheckMaxActive())
                        break;
                    RequestFlags(qry, false);
                }

                if (!CheckReady(qry, stCheckingFlags, &b_need_repeat)) {
                    if (b_need_repeat)
                        LOG3(("Restart stCheckingFlags key=%s.%d", m_keyspace.c_str(), m_key));
                    break;
                }

                async_rslt_t wr = ar_done;
                if (!qry->IsEOF()) {
                    wr = qry->NextRow(); 
                    if (wr == ar_dataready) {
                        UpdateLastActivity();
                        if (m_blob_stat.modified != qry->FieldGetInt64Value(0)) {
                            snprintf(msg, sizeof(msg), "Failed to re-confirm blob flags (key=%s.%d) modified column changed from %ld to %ld", m_keyspace.c_str(), m_key, m_blob_stat.modified, qry->FieldGetInt64Value(0));
                            has_error = true;
                        }
                        else if (m_blob_stat.flags != qry->FieldGetInt64Value(1)) {
                            snprintf(msg, sizeof(msg), "Failed to re-confirm blob flags (key=%s.%d) flags column changed from 0x%lx to 0x%lx", m_keyspace.c_str(), m_key, m_blob_stat.flags, qry->FieldGetInt64Value(1));
                            has_error = true;
                        }
                        else if (m_expected_size != qry->FieldGetInt64Value(2)) {
                            snprintf(msg, sizeof(msg), "Failed to re-confirm blob flags (key=%s.%d) size column changed from %ld to %ld", m_keyspace.c_str(), m_key, m_expected_size, qry->FieldGetInt64Value(2));
                            has_error = true;
                        }
                        else if (m_large_parts != qry->FieldGetInt32Value(3, 0)) {
                            snprintf(msg, sizeof(msg), "Failed to re-confirm blob flags (key=%s.%d) large_parts column changed from %d to %d", m_keyspace.c_str(), m_key, m_large_parts, qry->FieldGetInt32Value(3, 0));
                            has_error = true;
                        }

                        if (has_error) {
                            Error(msg);
                        }
                        else {
                            m_state = stDone;
                            m_data_cb(nullptr, 0, -1, true);
                        }
                    }
                    else if (wr == ar_wait) {
                        break;
                    }
                }
                if (qry->IsEOF() || wr == ar_done) {
                    UpdateLastActivity();
                    snprintf(msg, sizeof(msg), "Failed to re-confirm blob flags (key=%s.%d) query returned no data", m_keyspace.c_str(), m_key);
                    Error(msg);
                    break;
                }
                break;
            }
            default: {
                snprintf(msg, sizeof(msg), "Failed to get blob (key=%s.%d) unexpected state (%d)", m_keyspace.c_str(), m_key, static_cast<int>(m_state));
                Error(msg);
            }
        }
    } while (b_need_repeat);
}


/*****************************************************

				BLOB    INSERT
  
*****************************************************/

/* CCassBlobInserter */

void CCassBlobInserter::Wait1() {
    bool b_need_repeat;
    do {
        b_need_repeat = false;
        switch (m_state) {
            case stError:
            case stDone:
                return;

            case stInit: {
                CloseAll();
                if (m_query_arr.size() == 0)
                    m_query_arr.emplace_back(m_conn->NewQuery());
                int64_t sz = m_blob->blob.Size();
                bool is_large = sz >= m_large_treshold;
                m_large_parts = is_large ? ((sz + m_large_chunk_sz - 1) / m_large_chunk_sz) : 0;
                if (m_is_new != tt_true) {
                    LOG4(("CCassBlobInserter: reading stat for key=%s.%d, rslt=%p", m_keyspace.c_str(), m_key, LOG_CPTR(m_blob)));
                    auto qry = m_query_arr[0];
                    CassConsistency c = (m_restart_counter > 0 && m_conn->GetFallBackRdConsistency()) ? CASS_CONSISTENCY_LOCAL_QUORUM : CASS_CONSISTENCY_ONE;
                    // We have to check whether blob exists in largeentity and remove it if new blob has smaller number of parts
                    string sql = "SELECT flags, large_parts FROM " + KeySpaceDot(m_keyspace) + "entity WHERE ent = ?";
                    qry->SetSQL(sql, 1);
                    qry->BindInt32(0, m_key);
                    UpdateLastActivity();
                    qry->Query(c, m_async, true);
                    m_state = stFetchOldLargeParts;
                }
                else {
                    CloseAll();
                    m_state = stInsert;
                    b_need_repeat = true;
                }
                break;
            }

            case stFetchOldLargeParts: {
                auto qry = m_query_arr[0];
                

                if (!CheckReady(qry, stInit, &b_need_repeat)) {
                    if (b_need_repeat)
                        LOG3(("Restart stFetchOldLargeParts key=%s.%d", m_keyspace.c_str(), m_key));
                    break;
                }

                async_rslt_t wr = (async_rslt_t) -1;
                if (!qry->IsEOF()) {
                    if ((wr = qry->NextRow()) == ar_dataready) {
                        UpdateLastActivity();
                        m_old_flags = qry->FieldGetInt64Value(0);
                        m_old_large_parts = qry->FieldGetInt32Value(1);
                        qry->Close();
                        LOG5(("CCassBlobInserter: old_large_parts=%d, old_flags=%ld, key=%s.%d", m_old_large_parts, m_old_flags, m_keyspace.c_str(), m_key));
                    }
                    else if (wr == ar_wait)
                        break;
                }
                if (qry->IsEOF() || wr == ar_done) {
                    m_old_large_parts  = 0;
                    m_old_flags = 0;
                }

                CloseAll();
                if (m_old_large_parts > m_large_parts)
                    m_state = stDeleteOldLargeParts;
                else
                    m_state = stInsert;
                b_need_repeat = true;
                break;
            }

            case stDeleteOldLargeParts: {
                // we have to delete old records in largeentity
                int i;
                string sql;
                auto qry = m_query_arr[0];
                qry->Close();
                qry->NewBatch();

                sql = "UPDATE " + KeySpaceDot(m_keyspace) + "entity SET flags = ? WHERE ent = ?";
                LOG5(("CCassBlobInserter: drop  flags to invalidate key=%s.%d", m_keyspace.c_str(), m_key));
                qry->SetSQL(sql, 2);
                qry->BindInt64(0, m_old_flags & ~(bfComplete | bfCheckFailed));
                qry->BindInt32(1, m_key);
                UpdateLastActivity();
                qry->Execute(CASS_CONSISTENCY_LOCAL_QUORUM, m_async);

                for (i = m_large_parts; i < m_old_large_parts; i++) {
                    sql = "DELETE FROM " + KeySpaceDot(m_keyspace) + "largeentity WHERE ent = ? AND local_id = ?";
                    LOG5(("CCassBlobInserter: delete %d old_large_part %d, key=%s.%d", i, i, m_keyspace.c_str(), m_key));
                    qry->SetSQL(sql, 2);
                    qry->BindInt32(0, m_key);
                    qry->BindInt32(1, i);
                    UpdateLastActivity();
                    qry->Execute(CASS_CONSISTENCY_LOCAL_QUORUM, m_async);
                }
                qry->RunBatch();

                m_state = stWaitDeleteOldLargeParts;
                break;
            }

            case stWaitDeleteOldLargeParts: {
                auto qry = m_query_arr[0];
                if (!CheckReady(qry, stDeleteOldLargeParts, &b_need_repeat)) {
                    if (b_need_repeat)
                        LOG3(("Restart stDeleteOldLargeParts key=%s.%d", m_keyspace.c_str(), m_key));
                    break;
                }
                UpdateLastActivity();
                CloseAll();
                m_state = stInsert;
                b_need_repeat = true;
                break;
            }

            case stInsert: {
                string sql;
                m_query_arr.reserve(m_large_parts + 1);
                auto qry = m_query_arr[0];
                if (!qry->IsActive()) {
                    string sql = "INSERT INTO " + KeySpaceDot(m_keyspace) + "entity (ent, modified, size, flags, large_parts, data) VALUES(?, ?, ?, ?, ?, ?)";
                    qry->SetSQL(sql, 6);
                    qry->BindInt32(0, m_key);
                    qry->BindInt64(1, m_blob->stat.modified);
                    qry->BindInt64(2, m_blob->blob.Size());
                    int64_t flags = m_blob->stat.flags & ~bfComplete;
                    if (m_large_parts == 0)
                        flags = m_blob->stat.flags | bfComplete;
                    qry->BindInt64(3, flags);
                    qry->BindInt32(4, m_large_parts);
                    qry->BindBytes(5, m_blob->blob.Data(), m_large_parts == 0 ? m_blob->blob.Size() : 0);
                    LOG5(("CCassBlobInserter: inserting blob, key=%s.%d, mod=%ld", m_keyspace.c_str(), m_key, m_blob->stat.modified));

                    UpdateLastActivity();
                    qry->Execute(CASS_CONSISTENCY_LOCAL_QUORUM, m_async);
                    
                }
                if (m_large_parts != 0) {
                    int32_t i;
                    int64_t ofs = 0;
                    int64_t sz;
                    int64_t rem_sz = m_blob->blob.Size();
                    for (i = 0; i < m_large_parts; i++) {
                        while ((size_t)i + 1 >= m_query_arr.size())
                            m_query_arr.emplace_back(m_conn->NewQuery());
                        qry = m_query_arr[i + 1];
                        if (!qry->IsActive()) {
                            sz = rem_sz;
                            if (sz > m_large_chunk_sz)
                                sz = m_large_chunk_sz;

                            sql = "INSERT INTO " + KeySpaceDot(m_keyspace) + "largeentity (ent, local_id, data) VALUES(?, ?, ?)";
                            qry->SetSQL(sql, 3);
                            qry->BindInt32(0, m_key);
                            qry->BindInt32(1, i);
                            qry->BindBytes(2, m_blob->blob.Data() + ofs, sz);
                            ofs += sz;
                            rem_sz -= sz;
                            LOG5(("CCassBlobInserter: inserting blob, key=%s.%d, mod=%ld, size: %ld", m_keyspace.c_str(), m_key, m_blob->stat.modified, sz));
                            UpdateLastActivity();
                            qry->Execute(CASS_CONSISTENCY_LOCAL_QUORUM, m_async);
                        }
                    }
                }
                m_state = stWaitingInserted;
                break;
            }

            case stWaitingInserted: {
                bool anyrunning = false;
                int i = -1;
                for (auto qry : m_query_arr) {
                    if (qry->IsActive()) {
                        if (!CheckReady(qry, stInsert, &b_need_repeat)) {
                            if (b_need_repeat)
                                LOG3(("Restart stInsert key=%s.%d, chunk: %d", m_keyspace.c_str(), m_key, i));
                            anyrunning = true;
                            break;
                        }
                    }
                    ++i;
                }
                if (!anyrunning) {
                    UpdateLastActivity();
                    CloseAll();
                    m_state = stUpdatingFlags;
                }
                break;
            }

            case stUpdatingFlags: {
                if (m_large_parts > 0) {
                    int64_t flags;
                    flags = m_blob->stat.flags | bfComplete;
                    auto qry = m_query_arr[0];
                    string sql = "UPDATE " + KeySpaceDot(m_keyspace) + "entity set flags = ? where ent = ?";
                    qry->SetSQL(sql, 2);
                    qry->BindInt64(0, flags);
                    qry->BindInt32(1, m_key);
                    LOG5(("CCassBlobInserter: updating flags for key=%s.%d", m_keyspace.c_str(), m_key));
                    UpdateLastActivity();
                    qry->Execute(CASS_CONSISTENCY_LOCAL_QUORUM, m_async);
                    m_state = stWaitingUpdateFlags;
                }
                else {
                    CloseAll();
                    m_state = stDone;
                }
                break;
            }

            case stWaitingUpdateFlags: {
                auto qry = m_query_arr[0];
                if (!CheckReady(qry, stUpdatingFlags, &b_need_repeat)) {
                    if (b_need_repeat)
                        LOG3(("Restart stUpdatingFlags key=%s.%d", m_keyspace.c_str(), m_key));
                    break;
                }
                CloseAll();
                m_state = stDone;
                break;
            }

            default: {
                char msg[1024];
                snprintf(msg, sizeof(msg), "Failed to insert blob (key=%s.%d) unexpected state (%d)", m_keyspace.c_str(), m_key, static_cast<int>(m_state));
                Error(msg);
            }
        }
    } while (b_need_repeat);
}


/*****************************************************

				BLOB    DELETE
  
*****************************************************/

/* CCassBlobDeleter */

void CCassBlobDeleter::Wait1() {
    bool b_need_repeat;
    do {
        b_need_repeat = false; 
        switch (m_state) {
            case stError:
            case stDone:
                return;

            case stInit: {
                CloseAll();
                if (m_query_arr.size() < 1)
                    m_query_arr.emplace_back(m_conn->NewQuery());
                auto qry = m_query_arr[0];
                string sql = "SELECT large_parts FROM " + KeySpaceDot(m_keyspace) + "entity WHERE ent = ?";
                qry->SetSQL(sql, 1);
                qry->BindInt32(0, m_key);
                LOG5(("DELETE: stReadingEntity blob, key=%s.%d", m_keyspace.c_str(), m_key));
                UpdateLastActivity();
                qry->Query(CASS_CONSISTENCY_LOCAL_QUORUM, m_async);
                m_state = stReadingEntity;
                break;
            }

            case stReadingEntity: {
                string sql;
                auto qry = m_query_arr[0];
                if (!CheckReady(qry, stInit, &b_need_repeat)) {
                    if (b_need_repeat)
                        LOG3(("Restart stReadingEntity key=%s.%d", m_keyspace.c_str(), m_key));
                    break;
                }
                async_rslt_t wr = (async_rslt_t)-1;
                if (!qry->IsEOF()) {
                    if ((wr = qry->NextRow()) == ar_dataready) {
                        UpdateLastActivity();
                        m_large_parts = qry->FieldGetInt32Value(0, 0);
                        LOG5(("blob, key=%s.%d, large_parts: %d", m_keyspace.c_str(), m_key, m_large_parts));
                    }
                    else if (wr == ar_wait)
                        break;
                }
                if (qry->IsEOF() || wr == ar_done) {
                    m_large_parts = 0;
                }
                CloseAll();
                if (m_large_parts > 0) {
                    LOG5(("DELETE: =>stDeleteLargeEnt blob, key=%s.%d, large_parts: %d", m_keyspace.c_str(), m_key, m_large_parts));
                    m_state = stDeleteLargeEnt;
                }
                else {
                    LOG5(("DELETE: =>stDeleteEnt blob, key=%s.%d, large_parts: 0", m_keyspace.c_str(), m_key));
                    m_state = stDeleteEnt;
                }
                b_need_repeat = true;
                break;
            }

            case stDeleteLargeEnt: {
                auto qry = m_query_arr[0];
                qry->NewBatch();
                string sql = "UPDATE " + KeySpaceDot(m_keyspace) + "entity SET flags = 0 WHERE ent = ?";
                qry->SetSQL(sql, 1);
                qry->BindInt32(0, m_key);
                UpdateLastActivity();
                qry->Execute(CASS_CONSISTENCY_LOCAL_QUORUM, m_async);
                int local_id;
                for (local_id = m_large_parts - 1; local_id >= 0; local_id--) {
                    sql = "DELETE FROM " + KeySpaceDot(m_keyspace) + "largeentity WHERE ent = ? AND local_id = ?";
                    qry->SetSQL(sql, 2);
                    qry->BindInt32(0, m_key);
                    qry->BindInt32(1, local_id);
                    LOG5(("deleting LARGE blob part, key=%s.%d, local_id=%d", m_keyspace.c_str(), m_key, local_id));
                    UpdateLastActivity();
                    qry->Execute(CASS_CONSISTENCY_LOCAL_QUORUM, m_async);
                }
                qry->RunBatch();
                LOG5(("DELETE: =>stWaitLargeEnt blob, key=%s.%d", m_keyspace.c_str(), m_key));
                m_state = stWaitLargeEnt;
                break;
            }

            case stWaitLargeEnt: {
                auto qry = m_query_arr[0];
                if (!CheckReady(qry, stDeleteLargeEnt, &b_need_repeat)) {
                    if (b_need_repeat)
                        LOG3(("Restart stDeleteLargeEnt key=%s.%d", m_keyspace.c_str(), m_key));
                    break;
                }
                CloseAll();
                LOG5(("DELETE: =>stDeleteEnt blob, key=%s.%d", m_keyspace.c_str(), m_key));
                m_state = stDeleteEnt;
                b_need_repeat = true;
                break;
            }   

            case stDeleteEnt: {
                auto qry = m_query_arr[0];
                string sql = "DELETE FROM " + KeySpaceDot(m_keyspace) + "entity WHERE ent = ?";
                qry->SetSQL(sql, 1);
                qry->BindInt32(0, m_key);
                LOG5(("deleting blob, key=%s.%d, large_parts: %d", m_keyspace.c_str(), m_key, m_large_parts));
                UpdateLastActivity();
                qry->Execute(CASS_CONSISTENCY_LOCAL_QUORUM, m_async);
                LOG5(("DELETE: =>stWaitingDone blob, key=%s.%d", m_keyspace.c_str(), m_key));
                m_state = stWaitingDone;
                break;
            }

            case stWaitingDone: {
                auto qry = m_query_arr[0];
                if (!CheckReady(qry, stDeleteEnt, &b_need_repeat)) {
                    if (b_need_repeat)
                        LOG3(("Restart stDeleteEnt key=%s.%d", m_keyspace.c_str(), m_key));
                    break;
                }
                LOG5(("DELETE: =>stDone blob, key=%s.%d", m_keyspace.c_str(), m_key));
                m_state = stDone;
                break;
            }
            default: {
                char msg[1024];
                snprintf(msg, sizeof(msg), "Failed to delete blob (key=%s.%d) unexpected state (%d)", m_keyspace.c_str(), m_key, static_cast<int>(m_state));
                Error(msg);
            }
        }
    } while (b_need_repeat);
}


/* CCassBlobOp */

void CCassBlobOp::CassExecuteScript(const string& scriptstr, CassConsistency c) {
	if (scriptstr.empty())
		return;

	const char* hd = scriptstr.c_str();
	const char* tl;
	while (*hd != 0) {
		while (*hd != 0 && (*hd == ' ' || *hd == '\t' || *hd == '\n' || *hd == '\r' )) hd++;

		tl = hd;
		while (*hd != 0 && *hd != ';') hd++;
		int sz = hd - tl;
		if (sz > 0) {
			string qry;
			qry.assign(tl, sz);
#if 0
			NcbiCout << NcbiEndl << "SCRIPTSQL(" << sz << "):" << NcbiEndl << qry << NcbiEndl;
#endif
			shared_ptr<CCassQuery> query(m_conn->NewQuery());
			query->SetSQL(qry, 0);
			query->Execute(c, false, false);
		}
		if (*hd != 0) hd++; // skip ';'
	}
}

void CCassBlobOp::CreateScheme(const string& filename, const string& keyspace) {
    LOG1(("Checking/Creating Cassandra Scheme id_blob_sync_cass.sql %s", m_keyspace.c_str()));
    string scheme;
    StringFromFile(filename, scheme);
    if (scheme.empty())
    	RAISE_ERROR(eGeneric, string("cassandra scheme not found or file is empty"));
	if (keyspace.empty())
    	RAISE_ERROR(eGeneric, string("cassandra namespace is not specified"));
	StringReplace(scheme, "%KeySpace%", keyspace);
	CAppOp op;
	CAppPerf::StartOp(&op);
    CassExecuteScript(scheme, CASS_CONSISTENCY_ALL);
    m_conn->SetKeyspace(keyspace);
	LOG1(("Updating default settings"));
	UpdateSetting(op, 0, SETTING_LARGE_TRESHOLD, NStr::NumericToString(DFLT_LARGE_TRESHOLD));
	UpdateSetting(op, 0, SETTING_LARGE_CHUNK_SZ, NStr::NumericToString(DFLT_LARGE_CHUNK_SZ));
	CAppPerf::StopOp(&op);
    LOG1(("Cassandra Scheme is ok"));
}

void CCassBlobOp::GetBlobChunkTresholds(unsigned int op_timeout_ms, int64_t *LargeTreshold, int64_t *LargeChunkSize) {
	string s;
	CAppOp op; 
	if (!GetSetting(op, op_timeout_ms, SETTING_LARGE_TRESHOLD, s) || !NStr::StringToNumeric(s, LargeTreshold) || *LargeTreshold < ABS_MIN_LARGE_TRESHOLD) {
		*LargeTreshold = DFLT_LARGE_TRESHOLD;
		UpdateSetting(op, op_timeout_ms, SETTING_LARGE_TRESHOLD, NStr::NumericToString(*LargeTreshold));
	}
	if (!GetSetting(op, op_timeout_ms, SETTING_LARGE_CHUNK_SZ, s) || !NStr::StringToNumeric(s, LargeChunkSize) || *LargeChunkSize < ABS_MIN_LARGE_CHUNK_SZ) {
		*LargeChunkSize = DFLT_LARGE_CHUNK_SZ;
		UpdateSetting(op, op_timeout_ms, SETTING_LARGE_CHUNK_SZ, NStr::NumericToString(*LargeChunkSize));
	}
}

void CCassBlobOp::GetBlob(CAppOp& op, unsigned int op_timeout_ms, int32_t key, unsigned int max_retries, SBlobStat* blob_stat, const DataChunkCB_t& data_chunk_cb) {
	string errmsg;
	bool is_error = false;
	CCassBlobLoader loader(&op, op_timeout_ms, m_conn, m_keyspace, key, false, true, data_chunk_cb, 
		[&is_error, &errmsg](const char* Text, CCassBlobWaiter* Waiter) {
			is_error = 1;
			errmsg = Text;
		}
	);
	CCassConnection::Perform(op, op_timeout_ms, nullptr, nullptr, 
		[&loader](bool is_repeated) {
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

void CCassBlobOp::GetBlobAsync(CAppOp& op, unsigned int op_timeout_ms, int32_t key, unsigned int max_retries, const DataChunkCB_t& data_chunk_cb, unique_ptr<CCassBlobWaiter>& Waiter) {
	Waiter.reset(new CCassBlobLoader(&op, op_timeout_ms, m_conn, m_keyspace, key, true, max_retries, data_chunk_cb, nullptr));
}

void CCassBlobOp::InsertBlobAsync(CAppOp& op, unsigned int op_timeout_ms, int32_t key, unsigned int max_retries, CBlob* blob_rslt, triple_t is_new, int64_t LargeTreshold, int64_t LargeChunkSz, unique_ptr<CCassBlobWaiter>& Waiter) {
	Waiter.reset(new CCassBlobInserter(&op, op_timeout_ms, m_conn, m_keyspace, key, blob_rslt, is_new, LargeTreshold, LargeChunkSz, true, max_retries, nullptr));
}

void CCassBlobOp::DeleteBlobAsync(CAppOp& op, unsigned int op_timeout_ms, int32_t key, unsigned int max_retries, unique_ptr<CCassBlobWaiter>& Waiter) {
	Waiter.reset(new CCassBlobDeleter(&op, op_timeout_ms, m_conn, m_keyspace, key, true, max_retries, nullptr));
}

struct SLoadKeysContext
{
	atomic_ulong m_max_running;
	atomic_ulong m_running_count;
	atomic_ulong m_fetched;
	vector<pair<int64_t, int64_t>> const &m_ranges;
	string m_sql;
	CBlobFullStatMap* m_keys;
    CFutex m_wakeup;
    function<void()> m_tick;

	SLoadKeysContext() = delete;
	SLoadKeysContext(SLoadKeysContext&&) = delete;
	SLoadKeysContext(const SLoadKeysContext&) = delete;
	SLoadKeysContext(
		unsigned long max_running,
		unsigned long running_count,
		unsigned long fetched,
		vector< pair<int64_t, int64_t> > const &ranges,
		string const & sql,
		CBlobFullStatMap* keys,
        function<void()> tick
	) :
        m_max_running(max_running),
        m_running_count(running_count),
        m_fetched(fetched),
        m_ranges(ranges),
        m_sql(sql),
        m_keys(keys),
        m_tick(tick)
	{}

	int64_t getStep(size_t range_id) const {
		int64_t lower_bound = m_ranges[range_id].first;
		int64_t upper_bound = m_ranges[range_id].second;
		if (upper_bound - lower_bound < KEYLOAD_SPLIT_COUNT) {
			return upper_bound - lower_bound;
		}
		else {
			return (upper_bound - lower_bound) / KEYLOAD_SPLIT_COUNT;
		}
	}
};

struct SLoadKeysOnDataContext {
	SLoadKeysContext& common;
	CAppOp op;
	int64_t lower_bound;
	int64_t upper_bound;
	size_t range_offset;
    unsigned int run_id;
	unsigned int restart_count;
    atomic_bool data_triggered;
    vector<pair<int32_t, SBlobFullStat>> keys;

	SLoadKeysOnDataContext() = delete;
	SLoadKeysOnDataContext(SLoadKeysOnDataContext&&) = delete;
	SLoadKeysOnDataContext(const SLoadKeysOnDataContext&) = delete;
	SLoadKeysOnDataContext(SLoadKeysContext & v_common, CAppOp* parent, int64_t v_lower_bound, int64_t v_upper_bound, size_t v_range_offset, unsigned int v_run_id) : 
		common(v_common),
		op(parent),
		lower_bound(v_lower_bound),
		upper_bound(v_upper_bound),
		range_offset(v_range_offset),
        run_id(v_run_id),
		restart_count(0),
        data_triggered(false)
	{}
};

static void InternalLoadKeys_ondata(CCassQuery& query, void* data) {
	SLoadKeysOnDataContext* context = static_cast<SLoadKeysOnDataContext*>(data);
    LOG5(("Query[%d] wake: %p", context->run_id, &query));
    context->data_triggered = true;
    context->common.m_wakeup.Inc();
}

void CCassBlobOp::LoadKeysScheduleNext(CCassQuery& query, void* data) {
	SLoadKeysOnDataContext* context = static_cast<SLoadKeysOnDataContext*>(data);
	bool restart_request = false;
	int32_t ent_current = 0;
    bool is_eof = false;
	try {
//		query.ProcessFutureResult(false);
		unsigned int counter = 0;
        CAppPerf::BeginRow(&context->op);
		while (true) {
			async_rslt_t wr;
			if ((wr = query.NextRow()) == ar_dataready) {
				pair<int32_t, SBlobFullStat> row;
				row.first = query.FieldGetInt32Value(0);
				ent_current = row.first;
				row.second.modified = query.FieldIsNull(1) ? 0 : query.FieldGetInt64Value(1);
				row.second.size = query.FieldIsNull(2) ? 0 : query.FieldGetInt64Value(2);
				row.second.flags = query.FieldIsNull(3) ? (bfCheckFailed | bfComplete) : query.FieldGetInt64Value(3);
				row.second.seen = false;
                LOG4(("CASS got key=%s:%-9d mod=%ld sz=%-5ld flags=%lx", query.GetConnection()->Keyspace().c_str(), row.first, row.second.modified, row.second.size, row.second.flags));
				context->keys.push_back(std::move(row));

				++(context->common.m_fetched);
				++counter;
				//LOG1(("Fetched blob with key: %i", key));
			}
			else if (wr == ar_wait) {
                LOG5(("Query[%d] ar_wait: %p", context->run_id, &query));
				break; // expecting next data event triggered on this same query rowset
            }
			else if (wr == ar_done) {
                is_eof = true;
				break; // EOF
            }
            else {
                RAISE_ERROR(eGeneric, "CASS unexpected condition");
            }
		}
        context->restart_count = 0;
        CAppPerf::EndRow(&context->op, context->common.m_fetched.load(), counter, -1, true);

		LOG5(("Fetched records by one query: %u", counter));
        if (!is_eof)
            return;
	}
	catch (const EDatabase& e) {
		if ((e.GetErrCode() == EDatabase::eQueryTimeout || e.GetErrCode() == EDatabase::eQueryFailedRestartable) && context->restart_count < 5) {
			restart_request = true;
		}
		else {
            LOG3(("LoadKeys ... exception for entity: %d, %s", ent_current, e.what()));
			throw;
		}
	}
	catch(...) {
	    LOG3(("LoadKeys ... exception for entity: %d", ent_current));
	    throw;
	}

	query.Close();

	if (restart_request) {
		CAppPerf::Relax(&context->op);
		LOG3(("QUERY TIMEOUT! Offset %lu - %lu. Error count %u", context->lower_bound, context->upper_bound , context->restart_count));
		query.SetSQL(context->common.m_sql, 2);
		query.BindInt64(0, context->lower_bound);
		query.BindInt64(1, context->upper_bound);
		query.Query(KEYLOAD_CONSISTENCY, true, true, KEYLOAD_PAGESIZE);
		++(context->restart_count);
	}
	else {        
		int64_t upper_bound = context->common.m_ranges[context->range_offset].second;
		int64_t step = context->common.getStep(context->range_offset);

		query.SetSQL(context->common.m_sql, 2);
		if (context->upper_bound < upper_bound) {
			int64_t new_upper_bound = (upper_bound - context->upper_bound > step) ? context->upper_bound + step : upper_bound;
			context->lower_bound = context->upper_bound;
			context->upper_bound = new_upper_bound;
			query.BindInt64(0, context->lower_bound);
			query.BindInt64(1, context->upper_bound);
			query.Query(KEYLOAD_CONSISTENCY, true, true, KEYLOAD_PAGESIZE);
			//LOG3(("QUERY DATA %lu %li - %li", context->range_offset, context->lower_bound, context->upper_bound));
		}
		else {
			bool need_run = false;
			{
				if( (context->common.m_max_running + 1) < context->common.m_ranges.size()) {
					context->range_offset = ++(context->common.m_max_running);
					need_run = true;
				}
			}

			if (need_run) {
				int64_t lower_bound = context->common.m_ranges[context->range_offset].first;
				int64_t step = context->common.getStep(context->range_offset);
				context->lower_bound = lower_bound;
				context->upper_bound = lower_bound + step;
				query.BindInt64(0, context->lower_bound);
				query.BindInt64(1, context->upper_bound);
				query.Query(KEYLOAD_CONSISTENCY, true, true, KEYLOAD_PAGESIZE);
				//LOG3(("QUERY DATA %lu %li - %li", context->range_offset, context->lower_bound, context->upper_bound));
			}
			else {
				query.SetOnData(nullptr, nullptr);
				--(context->common.m_running_count);
			}
		}
	}
}

void CCassBlobOp::LoadKeys(CAppOp& op, CBlobFullStatMap* keys, function<void()> tick) {
    unsigned int run_id;
	vector< pair<int64_t, int64_t> > ranges;
	m_conn->getTokenRanges(ranges);
	unsigned int concurrent = min<unsigned int>(KEYLOAD_CONCURRENCY, static_cast<unsigned int>(ranges.size()));

	string common = "SELECT ent, modified, size, flags FROM " + KeySpaceDot(m_keyspace) + "entity WHERE";
	string fsql = common + " TOKEN(ent) >= ? and TOKEN(ent) <= ?";
	string sql = common	+ " TOKEN(ent) > ? and TOKEN(ent) <= ?";
    string error;

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

            shared_ptr<CCassQuery> query(m_conn->NewQuery());
            queries[run_id].swap(query);
            if (run_id == 0) {
                queries[run_id]->SetSQL(fsql, 2);
            }
            else {
                queries[run_id]->SetSQL(sql, 2);
            }
            contexts.emplace_back(loadkeys_context, &op, lower_bound, upper_bound, run_id, run_id);
            SLoadKeysOnDataContext* context = &contexts.back();
            queries[run_id]->BindInt64(0, lower_bound);
            queries[run_id]->BindInt64(1, upper_bound);
            queries[run_id]->SetOnData(InternalLoadKeys_ondata, context);
            queries[run_id]->Query(KEYLOAD_CONSISTENCY, true, true, KEYLOAD_PAGESIZE);
            //LOG1(("QUERY DATA %u %li - %li", run_id, ranges[run_id].first, running[run_id].first));
        }

        string error;
        while (!SSignalHandler::CtrlCPressed() && loadkeys_context.m_running_count.load() > 0) {
            loadkeys_context.m_wakeup.WaitWhile(val);
            val = loadkeys_context.m_wakeup.Value();
            for (auto& it : contexts) {
                run_id = it.run_id;
                if (!SSignalHandler::CtrlCPressed() && it.data_triggered) {
                    it.data_triggered = false;
                    LoadKeysScheduleNext(*(queries[run_id].get()), &it);
                }
            }
        }
        if (SSignalHandler::CtrlCPressed())
            RAISE_ERROR(eFatal, "SIGINT delivered");
    }
    catch (const exception& e) { // don't re-throw, we have to wait until all data events are triggered
        if (error.empty())
            error = e.what();
    }

    if (!error.empty()) {
        for (auto& it : queries) {
            it->Close();
        }
        RAISE_ERROR(eGeneric, error);
    }
    
    LOG5(("LoadKeys: reloading keys from vectors to map"));
    for (auto& it : contexts) {
		CBlobFullStatMap* all_keys = it.common.m_keys;
		if (!it.keys.empty()) {
            for (auto const& row : it.keys) {
                auto it = all_keys->find(row.first);
                if (it != all_keys->end()) {
                    ERRLOG1(("key=%d already exists, mod: %ld, fetched: %ld", row.first, it->second.modified, row.second.modified));
                } else {
                    all_keys->insert(row);
                    //LOG5(("fetched key=%d - %ld, idx: %ld", row.first, it->second.modified, row.second.modified));
                }
            }
            it.keys.clear();
            it.keys.resize(0);
		}
    }


	LOG3(("LoadKeys: finished"));
}

/*****************************************************

				ASYNC   DATA   REC
  
*****************************************************/

struct SAsyncData_rec_t {
	bool m_run_async;
	CassConsistency m_cons;
	vector<shared_ptr<CCassQuery> > m_queryarr;
	int m_activequeryidx;
	shared_ptr<CCassConnection> m_conn;
	SAsyncData_rec_t(CassConsistency cons = CASS_CONSISTENCY_LOCAL_QUORUM, bool run_async = false, shared_ptr<CCassConnection> conn = nullptr) :
		m_run_async(run_async), 
		m_cons(cons), 
		m_activequeryidx(-1),
		m_conn(conn)
		{}
	shared_ptr<CCassQuery> ActiveQuery() {
		if (m_activequeryidx >= 0) {
			if (m_activequeryidx >= (int)m_queryarr.size()) {
				RAISE_ERROR(eGeneric, "SAsyncData_rec_t::ActiveQuery: index out of range");
			}
			return m_queryarr[m_activequeryidx];
		}
		else
			return nullptr;
	};
	void AddQuery(shared_ptr<CCassQuery> qry) {
		m_queryarr.push_back(qry);
		if (m_activequeryidx < 0)
			m_activequeryidx = 0;
	}
	virtual async_rslt_t WaitAsync(unsigned int timeoutmks) {
		shared_ptr<CCassQuery> aq = ActiveQuery();
		if (aq) {
			LOG5(("WA qry=%p >>", LOG_SPTR(aq)));
			async_rslt_t rv = aq->WaitAsync(timeoutmks);
			LOG5(("WA qry=%p rv=%d <<", LOG_SPTR(aq), rv));
			shared_ptr<CCassQuery> newaq = ActiveQuery();
			if (rv != ar_wait && newaq != nullptr && aq != newaq) {
				rv = ar_wait;
				LOG5(("WA qry=%p OVERRIDE rv=%d, new qry=%p", LOG_SPTR(aq), rv, LOG_SPTR(newaq)));
			}
			return rv;
		}
		else
			return ar_done;
	}
	virtual bool IsAsync() {
		return m_run_async;
	}
	virtual bool IsEOF() {
		for(auto q : m_queryarr)
			if (!q->IsEOF())
				return false;
		return true;
	}
	virtual void Close() {
		for(auto q : m_queryarr)
			q->Close();
		m_queryarr.clear();
		m_activequeryidx = -1;
	}
	virtual string ToString() {
		shared_ptr<CCassQuery> aq = ActiveQuery();
		return aq ? aq->ToString() : "<nullptr>";
	}
};


/*****************************************************

				UPDATE    FLAGS
  
*****************************************************/

void CCassBlobOp::UpdateBlobFlags(CAppOp& op, unsigned int op_timeout_ms, int32_t key, uint64_t flags, flagop_t flag_op) {
	CCassConnection::Perform(op, op_timeout_ms, nullptr, nullptr, 
		[this, flags, flag_op, key](bool is_repeated) {
			int64_t new_flags = 0;
			string sql;
			shared_ptr<CCassQuery> qry = m_conn->NewQuery();
			switch (flag_op) {
				case FLAG_OP_OR:
				case FLAG_OP_AND: {
					sql = "SELECT flags FROM " + KeySpaceDot(m_keyspace) + "entity WHERE ent = ?";
					qry->SetSQL(sql, 1);
					qry->BindInt32(0, key);
					qry->Query(CASS_CONSISTENCY_LOCAL_QUORUM);
					if (!qry->IsEOF() && qry->NextRow() == ar_dataready) {
						switch (flag_op) {
							case FLAG_OP_OR:
								new_flags = qry->FieldGetInt64Value(0) | flags;
								break;
							case FLAG_OP_AND:
								new_flags = qry->FieldGetInt64Value(0) & flags;
								break;
							default:				
								RAISE_ERROR(eFatal, "Unexpected flag operation");
						}
						qry->Close();
					}
					break;
				}
				case FLAG_OP_SET:
					new_flags = flags;
					break;
				default:
					RAISE_ERROR(eFatal, "Unexpected flag operation");
			}
			sql = "UPDATE " + KeySpaceDot(m_keyspace) + "entity SET flags = ? WHERE ent = ?";
			qry->SetSQL(sql, 2);
			qry->BindInt64(0, new_flags);
			qry->BindInt32(1, key);
			qry->Execute(CASS_CONSISTENCY_LOCAL_QUORUM);
			return true;
		}
	);
}

/*****************************************************

				IN-TABLE    SETTINGS
  
*****************************************************/

void CCassBlobOp::UpdateSetting(CAppOp& op, unsigned int op_timeout_ms, const string& name, const string& value) {
	CCassConnection::Perform(op, op_timeout_ms, nullptr, nullptr, 
		[this, name, value](bool is_repeated) {
			string sql;
			sql = "INSERT INTO " + KeySpaceDot(m_keyspace) + "settings (name, value) VALUES(?, ?)";
			shared_ptr<CCassQuery>qry(m_conn->NewQuery());
			qry->SetSQL(sql, 2);
			LOG5(("InternalUpdateSetting: %s=>%s, %s", name.c_str(), value.c_str(), sql.c_str()));
			qry->BindStr(0, name);
			qry->BindStr(1, value);
			qry->Execute(CASS_CONSISTENCY_LOCAL_QUORUM, false, false);
			return true;
		}
	);
}

bool CCassBlobOp::GetSetting(CAppOp& op, unsigned int op_timeout_ms, const string& name, string& value) {
	bool rslt = false;
	CCassConnection::Perform(op, op_timeout_ms, nullptr, nullptr, 
		[this, name, &value, &rslt](bool is_repeated) {
			string sql;
			sql = "SELECT value FROM " + KeySpaceDot(m_keyspace) + "settings WHERE name=?";
			shared_ptr<CCassQuery>qry(m_conn->NewQuery());
			LOG5(("InternalGetSetting: %s: %s", name.c_str(), sql.c_str()));
			qry->SetSQL(sql, 1);
			qry->BindStr(0, name);
			CassConsistency cons = is_repeated && m_conn->GetFallBackRdConsistency() ? CASS_CONSISTENCY_LOCAL_ONE : CASS_CONSISTENCY_LOCAL_QUORUM;
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
