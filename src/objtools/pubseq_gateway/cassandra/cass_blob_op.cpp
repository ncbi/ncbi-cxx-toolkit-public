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

#define KEYLOAD_SPLIT_COUNT 1000
#define KEYLOAD_CONCURRENCY 512
#define KEYLOAD_CONSISTENCY CassConsistency::CASS_CONSISTENCY_LOCAL_QUORUM
#define KEYLOAD_PAGESIZE 4096

#define MAX_ACTIVE_STATEMENTS 512
#define MAX_CHUNKS_AHEAD      4

static string KeySpaceDot(const string& keyspace)
{
    return keyspace.empty() ? keyspace : keyspace + ".";
}

static void StringFromFile(const string & filename, string & str)
{
    filebuf fb;
    if (fb.open(filename.c_str(), ios::in | ios::binary)) {
        streampos sz = fb.pubseekoff(0, ios_base::end);
        fb.pubseekoff(0, ios_base::beg);
        str.resize((size_t)sz);
        if (sz > 0) {
            fb.sgetn(&(str.front()), sz);
        }
        fb.close();
    }
}

static void StringReplace(string & where, const string & what,
                          const string & replace)
{
    size_t pos = 0;
    while (pos < where.size()) {
        size_t nx = where.find(what, pos);
        if (nx == string::npos) {
            break;
        }
        where.replace(nx, what.size(), replace);
        pos = nx + what.size();
    }
}

/** CCassBlobWaiter */

bool CCassBlobWaiter::CheckMaxActive()
{
    return (m_Conn->GetActiveStatements() < MAX_ACTIVE_STATEMENTS);
}


/*****************************************************

                BLOB    READ

*****************************************************/

/* CCassBlobLoader */

void CCassBlobLoader::x_RequestChunk(shared_ptr<CCassQuery> qry, int local_id)
{
    CassConsistency     c = (m_RestartCounter > 0 &&
                             m_Conn->GetFallBackRdConsistency()) ?
                                CASS_CONSISTENCY_LOCAL_QUORUM :
                                CASS_CONSISTENCY_LOCAL_ONE;
    string              sql = "SELECT data FROM " + KeySpaceDot(m_Keyspace) +
                              "largeentity WHERE ent = ? AND local_id = ?";

    ERR_POST(Trace << "reading LARGE blob part, key=" << m_Keyspace <<
             "." << m_Key << ", local_id=" << local_id << ", qry: " << qry.get());
    qry->SetSQL(sql, 2);
    qry->BindInt32(0, m_Key);
    qry->BindInt32(1, local_id);
    if (m_DataReadyCb)
        qry->SetOnData2(m_DataReadyCb, m_DataReadyData);
    UpdateLastActivity();
    qry->Query(c, true);
}


void CCassBlobLoader::x_RequestFlags(shared_ptr<CCassQuery>  qry,
                                   bool  with_data)
{
    CassConsistency     c = (m_RestartCounter > 0 &&
                             m_Conn->GetFallBackRdConsistency()) ?
                                CASS_CONSISTENCY_LOCAL_QUORUM :
                                CASS_CONSISTENCY_LOCAL_ONE;
    string              sql;

    if (with_data)
        sql = "SELECT modified, flags, size, large_parts, data FROM " +
              KeySpaceDot(m_Keyspace) + "entity WHERE ent = ?";
    else
        sql = "SELECT modified, flags, size, large_parts       FROM " +
              KeySpaceDot(m_Keyspace) + "entity WHERE ent = ?";

    qry->SetSQL(sql, 1);
    qry->BindInt32(0, m_Key);
    if (m_DataReadyCb)
        qry->SetOnData2(m_DataReadyCb, m_DataReadyData);
    UpdateLastActivity();
    qry->Query(c, m_Async);
}


void CCassBlobLoader::x_RequestChunksAhead(void)
{
    int             cnt = MAX_CHUNKS_AHEAD;

    for (int32_t local_id = 0;
         local_id < m_LargeParts && cnt > 0; ++local_id) {
        if (!m_ProcessedChunks[local_id]) {
            --cnt;

            auto    qry = m_QueryArr[local_id];
            if (!qry->IsActive()) {
                if (!CheckMaxActive())
                    break;
                x_RequestChunk(qry, local_id);
            }
        }
    }
}


void CCassBlobLoader::x_PrepareChunkRequests(void)
{
    // Two parts need to be prepared: the cassandra queries and the flags which
    // indicate the chunks readiness

    // +1 is for the finalizing request of the blob flags (to make sure the
    // blob has not been changed while it was read)
    m_QueryArr.reserve(m_LargeParts + 1);
    CloseAll();

    while ((int)m_QueryArr.size() < m_LargeParts)
        m_QueryArr.emplace_back(m_Conn->NewQuery());

    m_ProcessedChunks.clear();
    m_ProcessedChunks.reserve(m_LargeParts);
    while ((int)m_ProcessedChunks.size() < m_LargeParts)
        m_ProcessedChunks.emplace_back(false);
}


// -1 => no ready chunks
// >= 0 => chunk ready to be sent
int CCassBlobLoader::x_GetReadyChunkNo(bool &  have_inactive,
                                       bool &  need_repeat)
{
    have_inactive = false;
    need_repeat = false;

    for (int  index = 0; index < m_LargeParts; ++index) {
        if (!m_QueryArr[index]->IsActive()) {
            have_inactive = true;
            continue;
        }

        if (!m_ProcessedChunks[index]) {
            bool    local_need_repeat = false;
            bool    ready = CheckReady(m_QueryArr[index], eReadingChunks,
                                       &local_need_repeat);
            if (ready)
                return index;

            if (local_need_repeat) {
                need_repeat = true;
                ERR_POST(Info << "Restart stReadingChunks required for key=" <<
                         m_Keyspace << "." << m_Key << ", chunk=" << index);
            }
        }
    }

    return -1;  // no ready chunks
}


bool CCassBlobLoader::x_AreAllChunksProcessed(void)
{
    for (const auto &  is_ready :  m_ProcessedChunks) {
        if (!is_ready)
            return false;
    }
    return true;
}


void CCassBlobLoader::x_MarkChunkProcessed(size_t  chunk_no)
{
    m_ProcessedChunks[chunk_no] = true;
}


void CCassBlobLoader::Wait1(void)
{
    bool        b_need_repeat;
    char        msg[1024];
    msg[0] = '\0';

    if (m_Cancelled) {
        CloseAll();
        return;
    }

    do {
        b_need_repeat = false;
        switch (m_State) {
            case eError:
            case eDone:
                return;

            case eInit: {
                CloseAll();
                if (m_QueryArr.size() == 0)
                    m_QueryArr.emplace_back(m_Conn->NewQuery());
                auto qry = m_QueryArr[0];
                if (!CheckMaxActive())
                    break;
                x_RequestFlags(qry, true);
                m_State = eReadingEntity;
                break;
            }

            case eReadingEntity: {
                auto qry = m_QueryArr[0];
                if (!qry->IsActive()) {
                    ERR_POST(Warning << "re-starting initial query");
                    if (!CheckMaxActive())
                        break;
                    x_RequestFlags(qry, true);
                }

                if (!CheckReady(qry, eInit, &b_need_repeat)) {
                    if (b_need_repeat)
                        ERR_POST(Info << "Restart stReadingEntity key=" <<
                                 m_Keyspace << "." << m_Key);
                    break;
                }

                if (qry->IsEOF()) {
                    UpdateLastActivity();
                    CloseAll();
                    m_State = eDone; // no data
                } else {
                    async_rslt_t wr = qry->NextRow();
                    if (wr == ar_wait) // paging
                        break;
                    else if (wr == ar_dataready) {
                        UpdateLastActivity();
                        const unsigned char *   rawdata = nullptr;
                        m_BlobStat.modified = qry->FieldGetInt64Value(0);
                        m_BlobStat.flags = qry->FieldGetInt64Value(1);
                        m_ExpectedSize = m_RemainingSize = qry->FieldGetInt64Value(2);
                        m_LargeParts = qry->FieldGetInt32Value(3, 0);
                        int64_t len = qry->FieldGetBlobRaw(4, &rawdata);
                        m_RemainingSize -= len;
                        m_StatLoaded = true;
                        if ((m_BlobStat.flags & (bfCheckFailed | bfComplete)) != bfComplete) {
                            snprintf(msg, sizeof(msg),
                                    "Blob failed check or it's "
                                    "incomplete (key=%s.%d, flags=0x%lx)",
                                    m_Keyspace.c_str(), m_Key,
                                    m_BlobStat.flags);
                            Error(CRequestStatus::e502_BadGateway,
                                  CCassandraException::eInconsistentData,
                                  eDiag_Error, msg);
                            break;
                        }

                        ERR_POST(Trace << "BD blob fetching, key=" <<
                                 m_Keyspace << "." << m_Key <<
                                 ", sz: " << len << ", " <<
                                 "EOF: " << qry->IsEOF() <<
                                 ", large_parts: " << m_LargeParts);

                        if (m_LargeParts == 0) {
                            if (m_RemainingSize != 0) {
                                snprintf(msg, sizeof(msg),
                                         "Size mismatch for (key=%s.%d), "
                                         "expected: %ld, "
                                         "actual: %ld (singlechunk)",
                                         m_Keyspace.c_str(), m_Key,
                                         m_ExpectedSize, len);
                                Error(CRequestStatus::e502_BadGateway,
                                      CCassandraException::eInconsistentData,
                                      eDiag_Error, msg);
                            } else {
                                m_State = eDone;
                                m_DataCb(m_Context, rawdata, len, 0);
                                m_DataCb(m_Context, nullptr, 0, -1);
                            }
                            break;
                        } else { // multi-chunk
                            x_PrepareChunkRequests();
                            x_RequestChunksAhead();
                            m_State = eReadingChunks;
                        }
                    } else {
                        if (!m_StatLoaded) {
                            // No data at all, i.e. there is no such a blob
                            // in the DB
                            string  msg = "Blob not found, key: " + m_Keyspace +
                                          "." + NStr::NumericToString(m_Key);

                            // Call a CB which tells that a 404 reply should be
                            // sent
                            Error(CRequestStatus::e404_NotFound,
                                  CCassandraException::eNotFound,
                                  eDiag_Error, msg.c_str());

                            CloseAll();
                        }

                        UpdateLastActivity();
                        m_State = eDone; // no data
                    }
                }
                break;
            }

            case eReadingChunks: {
                while (!x_AreAllChunksProcessed()) {
                    x_RequestChunksAhead();

                    bool    have_inactive;
                    bool    need_repeat;
                    int     ready_chunk_no = x_GetReadyChunkNo(have_inactive,
                                                               need_repeat);

                    if (ready_chunk_no < 0) {
                        // no ready chunks
                        if (need_repeat) {
                            continue;
                        }

                        if (have_inactive) {
                            if (m_Async) {
                                return; // wasn't activated b'ze
                                        // too many active statements
                            }
                            usleep(1000);
                            continue;
                        }

                        return;
                    }

                    // here: there is a ready to transfer chunk
                    async_rslt_t        wr = ar_done;
                    auto                qry = m_QueryArr[ready_chunk_no];

                    if (!qry->IsEOF()) {
                        wr = qry->NextRow();
                        if (wr == ar_wait) // paging
                            return;

                        if (wr == ar_dataready) {
                            UpdateLastActivity();
                            const unsigned char *   rawdata = nullptr;
                            int64_t                 len = qry->FieldGetBlobRaw(0, &rawdata);
                            m_RemainingSize -= len;
                            if (m_RemainingSize < 0) {
                                snprintf(msg, sizeof(msg),
                                         "Failed to fetch blob chunk "
                                         "(key=%s.%d, chunk=%d) size %ld "
                                         "is too large",
                                         m_Keyspace.c_str(), m_Key,
                                         ready_chunk_no, len);
                                Error(CRequestStatus::e502_BadGateway,
                                      CCassandraException::eInconsistentData,
                                      eDiag_Error, msg);
                                return;
                            }

                            m_DataCb(m_Context, rawdata, len, ready_chunk_no);
                            x_MarkChunkProcessed(ready_chunk_no);
                            x_RequestChunksAhead();
                            continue;
                        }
                    }

                    assert(qry->IsEOF() || wr == ar_done);
                    if (CanRestart()) {
                        ERR_POST(Info << "Restarting key=" << m_Keyspace <<
                                 "." << m_Key <<
                                 ", chunk=" << ready_chunk_no <<" p2");
                        qry->Close();
                        continue;
                    } else {
                        snprintf(msg, sizeof(msg),
                                 "Failed to fetch blob chunk "
                                 "(key=%s.%d, chunk=%d) wr=%d",
                                 m_Keyspace.c_str(), m_Key, ready_chunk_no,
                                 static_cast<int>(wr));
                        Error(CRequestStatus::e502_BadGateway,
                              CCassandraException::eFetchFailed,
                              eDiag_Error, msg);
                        return;
                    }
                }

                if (x_AreAllChunksProcessed() &&
                    m_State != eError && m_State != eDone) {
                    if (m_RemainingSize > 0) {
                        snprintf(msg, sizeof(msg),
                                 "Failed to fetch blob (key=%s.%d) result is "
                                 "incomplete remaining %ld bytes",
                                 m_Keyspace.c_str(), m_Key, m_RemainingSize);
                        Error(CRequestStatus::e502_BadGateway,
                              CCassandraException::eInconsistentData,
                              eDiag_Error, msg);
                        break;
                    }

                    while (m_QueryArr.size() < (size_t)m_LargeParts + 1)
                        m_QueryArr.emplace_back(m_Conn->NewQuery());

                    if (!CheckMaxActive())
                        break;

                    auto    qry = m_QueryArr[m_LargeParts];
                    x_RequestFlags(qry, false);
                    m_State = eCheckingFlags;
                }
                break;
            }

            case eCheckingFlags: {
                bool        has_error = false;
                auto        qry = m_QueryArr[m_LargeParts];

                if (!qry->IsActive()) {
                    ERR_POST(Info << "running checkflag query");
                    if (!CheckMaxActive())
                        break;
                    x_RequestFlags(qry, false);
                }

                if (!CheckReady(qry, eCheckingFlags, &b_need_repeat)) {
                    if (b_need_repeat)
                        ERR_POST(Info << "Restart stCheckingFlags key=" <<
                                 m_Keyspace << "." << m_Key);
                    break;
                }

                async_rslt_t wr = ar_done;
                if (!qry->IsEOF()) {
                    wr = qry->NextRow();
                    if (wr == ar_dataready) {
                        UpdateLastActivity();
                        if (m_BlobStat.modified != qry->FieldGetInt64Value(0)) {
                            snprintf(msg, sizeof(msg),
                                     "Failed to re-confirm blob flags "
                                     "(key=%s.%d) modified column changed "
                                     "from %ld to %ld",
                                     m_Keyspace.c_str(), m_Key,
                                     m_BlobStat.modified,
                                     qry->FieldGetInt64Value(0));
                            has_error = true;
                        } else if (m_BlobStat.flags != qry->FieldGetInt64Value(1)) {
                            snprintf(msg, sizeof(msg),
                                     "Failed to re-confirm blob flags "
                                     "(key=%s.%d) flags column changed "
                                     "from 0x%lx to 0x%lx",
                                     m_Keyspace.c_str(), m_Key,
                                     m_BlobStat.flags,
                                     qry->FieldGetInt64Value(1));
                            has_error = true;
                        } else if (m_ExpectedSize != qry->FieldGetInt64Value(2)) {
                            snprintf(msg, sizeof(msg),
                                     "Failed to re-confirm blob flags "
                                     "(key=%s.%d) size column changed "
                                     "from %ld to %ld",
                                     m_Keyspace.c_str(), m_Key,
                                     m_ExpectedSize,
                                     qry->FieldGetInt64Value(2));
                            has_error = true;
                        } else if (m_LargeParts != qry->FieldGetInt32Value(3, 0)) {
                            snprintf(msg, sizeof(msg),
                                     "Failed to re-confirm blob flags "
                                     "(key=%s.%d) large_parts column changed "
                                     "from %d to %d",
                                     m_Keyspace.c_str(), m_Key, m_LargeParts,
                                     qry->FieldGetInt32Value(3, 0));
                            has_error = true;
                        }

                        if (has_error) {
                            Error(CRequestStatus::e502_BadGateway,
                                  CCassandraException::eInconsistentData,
                                  eDiag_Error, msg);
                        } else {
                            m_State = eDone;
                            m_DataCb(m_Context, nullptr, 0, -1);
                        }
                    } else if (wr == ar_wait) {
                        break;
                    }
                }
                if (qry->IsEOF() || wr == ar_done) {
                    UpdateLastActivity();
                    snprintf(msg, sizeof(msg),
                             "Failed to re-confirm blob flags (key=%s.%d) "
                             "query returned no data",
                             m_Keyspace.c_str(), m_Key);
                    Error(CRequestStatus::e502_BadGateway,
                          CCassandraException::eInconsistentData,
                          eDiag_Error, msg);
                    break;
                }
                break;
            }
            default: {
                snprintf(msg, sizeof(msg),
                         "Failed to get blob (key=%s.%d) unexpected state (%d)",
                         m_Keyspace.c_str(), m_Key, static_cast<int>(m_State));
                Error(CRequestStatus::e502_BadGateway,
                      CCassandraException::eQueryFailed,
                      eDiag_Error, msg);
            }
        }
    } while (b_need_repeat);
}


/*****************************************************

                BLOB    INSERT

*****************************************************/

/* CCassBlobInserter */

void CCassBlobInserter::Wait1()
{
    bool    b_need_repeat;

    do {
        b_need_repeat = false;
        switch (m_State) {
            case eError:
            case eDone:
                return;

            case eInit: {
                CloseAll();
                if (m_QueryArr.size() == 0) {
                    m_QueryArr.emplace_back(m_Conn->NewQuery());
                }
                m_LargeParts = m_Blob->GetSize() >= m_LargeTreshold ? m_Blob->GetNChunks() : 0;
                if (m_IsNew != eTrue) {
                    ERR_POST(Trace << "CCassBlobInserter: reading stat for "
                             "key=" << m_Keyspace << "." << m_Key <<
                             ", rslt=" << m_Blob);
                    auto qry = m_QueryArr[0];
                    CassConsistency c = (m_RestartCounter > 0 && m_Conn->GetFallBackRdConsistency()) ?
                        CASS_CONSISTENCY_LOCAL_QUORUM : CASS_CONSISTENCY_ONE;

                    // We have to check whether blob exists in largeentity
                    // and remove it if new blob has smaller number of parts
                    string  sql = "SELECT flags, large_parts FROM " + KeySpaceDot(m_Keyspace) + " entity WHERE ent = ?";
                    qry->SetSQL(sql, 1);
                    qry->BindInt32(0, m_Key);
                    UpdateLastActivity();
                    qry->Query(c, m_Async, true);
                    m_State = eFetchOldLargeParts;
                } else {
                    CloseAll();
                    m_State = eInsert;
                    b_need_repeat = true;
                }
                break;
            }

            case eFetchOldLargeParts: {
                auto qry = m_QueryArr[0];
                if (!CheckReady(qry, eInit, &b_need_repeat)) {
                    if (b_need_repeat) {
                        ERR_POST(Info << "Restart stFetchOldLargeParts key=" << m_Keyspace << "." << m_Key);
                    }
                    break;
                }

                async_rslt_t wr = (async_rslt_t) -1;
                if (!qry->IsEOF()) {
                    if ((wr = qry->NextRow()) == ar_dataready) {
                        UpdateLastActivity();
                        m_OldFlags = qry->FieldGetInt64Value(0);
                        m_OldLargeParts = qry->FieldGetInt32Value(1);
                        qry->Close();
                        ERR_POST(Trace << "CCassBlobInserter: old_large_parts=" <<
                                 m_OldLargeParts << ", old_flags=" << m_OldFlags <<
                                 ", key=" << m_Keyspace << "." << m_Key);
                    } else if (wr == ar_wait)
                        break;
                }
                if (qry->IsEOF() || wr == ar_done) {
                    m_OldLargeParts  = 0;
                    m_OldFlags = 0;
                }

                CloseAll();
                if (m_OldLargeParts > m_LargeParts)
                    m_State = eDeleteOldLargeParts;
                else
                    m_State = eInsert;
                b_need_repeat = true;
                break;
            }

            case eDeleteOldLargeParts: {
                // we have to delete old records in largeentity
                string          sql;
                auto            qry = m_QueryArr[0];

                qry->Close();
                qry->NewBatch();

                sql = "UPDATE " + KeySpaceDot(m_Keyspace) +
                      "entity SET flags = ? WHERE ent = ?";
                ERR_POST(Trace << "CCassBlobInserter: drop  flags "
                         "to invalidate key=" << m_Keyspace << "." << m_Key);
                qry->SetSQL(sql, 2);
                qry->BindInt64(0, m_OldFlags & ~(bfComplete | bfCheckFailed));
                qry->BindInt32(1, m_Key);
                UpdateLastActivity();
                qry->Execute(CASS_CONSISTENCY_LOCAL_QUORUM, m_Async);

                for (int  i = m_LargeParts; i < m_OldLargeParts; ++i) {
                    sql = "DELETE FROM " + KeySpaceDot(m_Keyspace) +
                          "largeentity WHERE ent = ? AND local_id = ?";
                    ERR_POST(Trace << "CCassBlobInserter: delete " << i <<
                             " old_large_part " << i << ", key=" <<
                             m_Keyspace << "." << m_Key);
                    qry->SetSQL(sql, 2);
                    qry->BindInt32(0, m_Key);
                    qry->BindInt32(1, i);
                    UpdateLastActivity();
                    qry->Execute(CASS_CONSISTENCY_LOCAL_QUORUM, m_Async);
                }
                qry->RunBatch();

                m_State = eWaitDeleteOldLargeParts;
                break;
            }

            case eWaitDeleteOldLargeParts: {
                auto            qry = m_QueryArr[0];

                if (!CheckReady(qry, eDeleteOldLargeParts, &b_need_repeat)) {
                    if (b_need_repeat)
                        ERR_POST(Info << "Restart stDeleteOldLargeParts "
                                 "key=" << m_Keyspace << "." << m_Key);
                    break;
                }
                UpdateLastActivity();
                CloseAll();
                m_State = eInsert;
                b_need_repeat = true;
                break;
            }

            case eInsert: {
                string sql;
                m_QueryArr.reserve(m_LargeParts + 1);
                auto qry = m_QueryArr[0];

                if (!qry->IsActive()) {
                    string sql =
                        "INSERT INTO " + KeySpaceDot(m_Keyspace) +
                        " entity (ent, modified, size, flags, "
                        " large_parts, data) VALUES(?, ?, ?, ?, ?, ?)";
                    qry->SetSQL(sql, 6);
                    qry->BindInt32(0, m_Key);
                    qry->BindInt64(1, m_Blob->GetModified());
                    qry->BindInt64(2, m_Blob->GetSize());

                    int64_t flags = m_Blob->GetFlags() & ~bfComplete;
                    if (m_LargeParts == 0)
                        flags = m_Blob->GetFlags() | bfComplete;
                    qry->BindInt64(3, flags);
                    qry->BindInt32(4, m_LargeParts);
                    if (m_LargeParts == 0) {
                        const CBlobRecord::TBlobChunk& chunk = m_Blob->GetChunk(0);
                        qry->BindBytes(5, chunk.data(), chunk.size());
                    }
                    else {
                        qry->BindNull(5);
                    }

                    ERR_POST(Trace << "CCassBlobInserter: inserting blob, "
                        "key=" << m_Keyspace << "." << m_Key <<
                        ", mod=" << m_Blob->GetModified());

                    UpdateLastActivity();
                    qry->Execute(CASS_CONSISTENCY_LOCAL_QUORUM, m_Async);

                }
                if (m_LargeParts != 0) {
                    for (int32_t  i = 0; i < m_LargeParts; ++i) {
                        while ((size_t)i + 1 >= m_QueryArr.size()) {
                            m_QueryArr.emplace_back(m_Conn->NewQuery());
                        }
                        qry = m_QueryArr[i + 1];
                        if (!qry->IsActive()) {
                            sql = "INSERT INTO " + KeySpaceDot(m_Keyspace) +
                                  "largeentity (ent, local_id, data) VALUES(?, ?, ?)";
                            qry->SetSQL(sql, 3);
                            qry->BindInt32(0, m_Key);
                            qry->BindInt32(1, i);
                            const CBlobRecord::TBlobChunk& chunk = m_Blob->GetChunk(i);
                            qry->BindBytes(2, chunk.data(), chunk.size());
                            ERR_POST(Trace << "CCassBlobInserter: inserting blob, "
                                     "key=" << m_Keyspace << "." << m_Key <<
                                     ", mod=" << m_Blob->GetModified() <<
                                     ", size:" << chunk.size());
                            UpdateLastActivity();
                            qry->Execute(CASS_CONSISTENCY_LOCAL_QUORUM, m_Async);
                        }
                    }
                }
                m_State = eWaitingInserted;
                break;
            }

            case eWaitingInserted: {
                bool anyrunning = false;
                int i = -1;
                for (auto qry: m_QueryArr) {
                    if (qry->IsActive()) {
                        if (!CheckReady(qry, eInsert, &b_need_repeat)) {
                            if (b_need_repeat)
                                ERR_POST(Info << "Restart stInsert key=" <<
                                    m_Keyspace << "." << m_Key <<
                                    ", chunk: " << i);
                            anyrunning = true;
                            break;
                        }
                    }
                    ++i;
                }
                if (!anyrunning) {
                    UpdateLastActivity();
                    CloseAll();
                    m_State = eUpdatingFlags;
                }
                break;
            }

            case eUpdatingFlags: {
                if (m_LargeParts > 0) {
                    int64_t flags = m_Blob->GetFlags() | bfComplete;
                    auto qry = m_QueryArr[0];
                    string sql =
                        "UPDATE " + KeySpaceDot(m_Keyspace) +
                        "entity set flags = ? where ent = ?";

                    qry->SetSQL(sql, 2);
                    qry->BindInt64(0, flags);
                    qry->BindInt32(1, m_Key);
                    ERR_POST(Trace << "CCassBlobInserter: updating flags for "
                             "key=" << m_Keyspace << "." << m_Key);
                    UpdateLastActivity();
                    qry->Execute(CASS_CONSISTENCY_LOCAL_QUORUM, m_Async);
                    m_State = eWaitingUpdateFlags;
                } else {
                    CloseAll();
                    m_State = eDone;
                }
                break;
            }

            case eWaitingUpdateFlags: {
                auto qry = m_QueryArr[0];
                if (!CheckReady(qry, eUpdatingFlags, &b_need_repeat)) {
                    if (b_need_repeat)
                        ERR_POST(Info << "Restart stUpdatingFlags key=" <<
                                 m_Keyspace << "." << m_Key);
                    break;
                }
                CloseAll();
                m_State = eDone;
                break;
            }

            default: {
                char msg[1024];
                snprintf(msg, sizeof(msg),
                    "Failed to insert blob (key=%s.%d) unexpected state (%d)",
                    m_Keyspace.c_str(), m_Key, static_cast<int>(m_State));
                Error(CRequestStatus::e502_BadGateway,
                    CCassandraException::eQueryFailed,
                    eDiag_Error, msg);
            }
        }
    } while (b_need_repeat);
}

/*****************************************************

                BLOB    DELETE

*****************************************************/

/* CCassBlobDeleter */

void CCassBlobDeleter::Wait1(void)
{
    bool b_need_repeat;
    do {
        b_need_repeat = false;
        switch (m_State) {
            case eError:
            case eDone:
                return;

            case eInit: {
                CloseAll();
                if (m_QueryArr.size() < 1) {
                    m_QueryArr.emplace_back(m_Conn->NewQuery());
                }

                auto qry = m_QueryArr[0];
                string sql = "SELECT large_parts FROM " +
                    KeySpaceDot(m_Keyspace) + "entity WHERE ent = ?";

                qry->SetSQL(sql, 1);
                qry->BindInt32(0, m_Key);
                ERR_POST(Trace << "DELETE: stReadingEntity blob, key=" <<
                     m_Keyspace << "." << m_Key);
                UpdateLastActivity();
                qry->Query(CASS_CONSISTENCY_LOCAL_QUORUM, m_Async);
                m_State = eReadingEntity;
                break;
            }

            case eReadingEntity: {
                string sql;
                auto qry = m_QueryArr[0];

                if (!CheckReady(qry, eInit, &b_need_repeat)) {
                    if (b_need_repeat)
                        ERR_POST(Info << "Restart stReadingEntity key=" <<
                                 m_Keyspace << "." << m_Key);
                    break;
                }
                async_rslt_t wr = (async_rslt_t)-1;
                if (!qry->IsEOF()) {
                    if ((wr = qry->NextRow()) == ar_dataready) {
                        UpdateLastActivity();
                        m_LargeParts = qry->FieldGetInt32Value(0, 0);
                        ERR_POST(Trace << "blob, key=" << m_Keyspace << "." <<
                                 m_Key << ", large_parts: " << m_LargeParts);
                    }
                    else if (wr == ar_wait) {
                        break;
                    }
                }
                if (qry->IsEOF() || wr == ar_done) {
                    m_LargeParts = 0;
                }
                CloseAll();
                if (m_LargeParts > 0) {
                    ERR_POST(Trace << "DELETE: =>stDeleteLargeEnt blob, "
                             "key=" << m_Keyspace << "." << m_Key <<
                             ", large_parts: " << m_LargeParts);
                    m_State = eDeleteLargeEnt;
                }
                else {
                    ERR_POST(Trace << "DELETE: =>stDeleteEnt blob, key=" <<
                             m_Keyspace << "." << m_Key <<", large_parts: 0");
                    m_State = eDeleteEnt;
                }
                b_need_repeat = true;
                break;
            }

            case eDeleteLargeEnt: {
                auto        qry = m_QueryArr[0];
                qry->NewBatch();
                string      sql = "UPDATE " + KeySpaceDot(m_Keyspace) +
                                  "entity SET flags = 0 WHERE ent = ?";

                qry->SetSQL(sql, 1);
                qry->BindInt32(0, m_Key);
                UpdateLastActivity();
                qry->Execute(CASS_CONSISTENCY_LOCAL_QUORUM, m_Async);

                for (int local_id = m_LargeParts - 1; local_id >= 0; local_id--) {
                    sql = "DELETE FROM " + KeySpaceDot(m_Keyspace) +
                          "largeentity WHERE ent = ? AND local_id = ?";
                    qry->SetSQL(sql, 2);
                    qry->BindInt32(0, m_Key);
                    qry->BindInt32(1, local_id);
                    ERR_POST(Trace << "deleting LARGE blob part, key=" <<
                             m_Keyspace << "." << m_Key <<
                             ", local_id=" << local_id);
                    UpdateLastActivity();
                    qry->Execute(CASS_CONSISTENCY_LOCAL_QUORUM, m_Async);
                }
                qry->RunBatch();
                ERR_POST(Trace << "DELETE: =>stWaitLargeEnt blob, key=" <<
                         m_Keyspace << "." << m_Key);
                m_State = eWaitLargeEnt;
                break;
            }

            case eWaitLargeEnt: {
                auto        qry = m_QueryArr[0];

                if (!CheckReady(qry, eDeleteLargeEnt, &b_need_repeat)) {
                    if (b_need_repeat)
                        ERR_POST(Info <<"Restart stDeleteLargeEnt key=" <<
                                 m_Keyspace << "." << m_Key);
                    break;
                }
                CloseAll();
                ERR_POST(Trace << "DELETE: =>stDeleteEnt blob, key=" <<
                         m_Keyspace << "." << m_Key);
                m_State = eDeleteEnt;
                b_need_repeat = true;
                break;
            }

            case eDeleteEnt: {
                auto        qry = m_QueryArr[0];
                string      sql = "DELETE FROM " + KeySpaceDot(m_Keyspace) +
                                  "entity WHERE ent = ?";

                qry->SetSQL(sql, 1);
                qry->BindInt32(0, m_Key);
                ERR_POST(Trace << "deleting blob, key=" <<
                         m_Keyspace << "." << m_Key <<
                         ", large_parts: " << m_LargeParts);
                UpdateLastActivity();
                qry->Execute(CASS_CONSISTENCY_LOCAL_QUORUM, m_Async);
                ERR_POST(Trace << "DELETE: =>stWaitingDone blob, key=" <<
                         m_Keyspace << "." << m_Key);
                m_State = eWaitingDone;
                break;
            }

            case eWaitingDone: {
                auto qry = m_QueryArr[0];
                if (!CheckReady(qry, eDeleteEnt, &b_need_repeat)) {
                    if (b_need_repeat)
                        ERR_POST(Info << "Restart stDeleteEnt key=" <<
                                 m_Keyspace << "." << m_Key);
                    break;
                }
                ERR_POST(Trace << "DELETE: =>stDone blob, key=" <<
                         m_Keyspace << "." << m_Key);
                m_State = eDone;
                break;
            }
            default: {
                char msg[1024];
                snprintf(msg, sizeof(msg),
                         "Failed to delete blob (key=%s.%d) unexpected state (%d)",
                         m_Keyspace.c_str(), m_Key, static_cast<int>(m_State));
                Error(CRequestStatus::e502_BadGateway,
                      CCassandraException::eQueryFailed,
                      eDiag_Error, msg);
            }
        }
    } while (b_need_repeat);
}


/* CCassBlobOp */

void CCassBlobOp::CassExecuteScript(const string &  scriptstr,
                                    CassConsistency  c)
{
    if (scriptstr.empty())
        return;

    const char *        hd = scriptstr.c_str();
    const char *        tl;

    while (*hd != 0) {
        while (*hd != 0 && (*hd == ' ' || *hd == '\t' || *hd == '\n' || *hd == '\r' ))
            hd++;

        tl = hd;
        while (*hd != 0 && *hd != ';')
            hd++;

        int         sz = hd - tl;
        if (sz > 0) {
            string qry;
            qry.assign(tl, sz);
            shared_ptr<CCassQuery> query(m_Conn->NewQuery());
            query->SetSQL(qry, 0);
            query->Execute(c, false, false);
        }
        if (*hd != 0) hd++; // skip ';'
    }
}


void CCassBlobOp::CreateScheme(const string &  filename,
                               const string &  keyspace)
{
    ERR_POST(Message << "Checking/Creating Cassandra Scheme "
             "id_blob_sync_cass.sql " << m_Keyspace);

    string      scheme;
    StringFromFile(filename, scheme);

    if (scheme.empty())
        NCBI_THROW(CCassandraException, eGeneric,
                   "cassandra scheme not found or file is empty");
    if (keyspace.empty())
        NCBI_THROW(CCassandraException, eGeneric,
                   "cassandra namespace is not specified");

    StringReplace(scheme, "%KeySpace%", keyspace);
    CassExecuteScript(scheme, CASS_CONSISTENCY_ALL);
    m_Conn->SetKeyspace(keyspace);
    ERR_POST(Message << "Updating default settings");
    UpdateSetting(0, SETTING_LARGE_TRESHOLD,
                  NStr::NumericToString(DFLT_LARGE_TRESHOLD));
    UpdateSetting(0, SETTING_LARGE_CHUNK_SZ,
                  NStr::NumericToString(DFLT_LARGE_CHUNK_SZ));
    ERR_POST(Message << "Cassandra Scheme is ok");
}


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
                          const DataChunkCB_t &  data_chunk_cb)
{
    string              errmsg;
    bool                is_error = false;

    CCassBlobLoader     loader(
        op_timeout_ms, m_Conn, m_Keyspace, key, false, max_retries,
        nullptr, data_chunk_cb,
        [&is_error, &errmsg](void *  context,
                             CRequestStatus::ECode  status,
                             int  code, EDiagSev  severity,
                             const string &  message)
        {
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
                               const DataChunkCB_t &  data_chunk_cb,
                               const DataErrorCB_t & error_cb,
                               unique_ptr<CCassBlobWaiter> &  Waiter)
{
    Waiter.reset(new CCassBlobLoader(op_timeout_ms, m_Conn, m_Keyspace,
                                     key, true, max_retries, nullptr,
                                     data_chunk_cb, error_cb));
}


void CCassBlobOp::InsertBlobAsync(unsigned int  op_timeout_ms,
                                  int32_t  key, unsigned int max_retries,
                                  CBlobRecord *  blob_rslt, ECassTristate  is_new,
                                  int64_t  LargeTreshold, int64_t LargeChunkSz,
                                  const DataErrorCB_t & error_cb,
                                  unique_ptr<CCassBlobWaiter> &  Waiter)
{
    if (m_ExtendedSchema) {
        Waiter.reset(
            new CCassBlobTaskInsertExtended(
                op_timeout_ms, m_Conn, m_Keyspace,
                blob_rslt, true, max_retries,
                nullptr, error_cb
            )
        );
    }
    else {
        Waiter.reset(
            new CCassBlobInserter(op_timeout_ms, m_Conn, m_Keyspace,
                key, blob_rslt, is_new, LargeTreshold,
                LargeChunkSz, true, max_retries,
                nullptr, error_cb)
        );
    }
}


void CCassBlobOp::DeleteBlobAsync(unsigned int  op_timeout_ms,
                                  int32_t  key, unsigned int  max_retries,
                                  const DataErrorCB_t & error_cb,
                                  unique_ptr<CCassBlobWaiter> &  Waiter)
{
    Waiter.reset(new CCassBlobDeleter(op_timeout_ms, m_Conn, m_Keyspace,
                                      key, true, max_retries,
                                      nullptr, error_cb));
}


struct SLoadKeysContext
{
    atomic_ulong                                m_max_running;
    atomic_ulong                                m_running_count;
    atomic_ulong                                m_fetched;
    vector<pair<int64_t, int64_t>> const &      m_ranges;
    string                                      m_sql;
    CBlobFullStatMap *                          m_keys;
    CFutex                                      m_wakeup;
    function<void()>                            m_tick;

    SLoadKeysContext() = delete;
    SLoadKeysContext(SLoadKeysContext&&) = delete;
    SLoadKeysContext(const SLoadKeysContext&) = delete;

    SLoadKeysContext(unsigned long  max_running,
                     unsigned long  running_count,
                     unsigned long  fetched,
                     vector< pair<int64_t, int64_t> > const &  ranges,
                     string const &  sql,
                     CBlobFullStatMap *  keys,
                     function<void()>  tick) :
        m_max_running(max_running),
        m_running_count(running_count),
        m_fetched(fetched),
        m_ranges(ranges),
        m_sql(sql),
        m_keys(keys),
        m_tick(tick)
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
    SLoadKeysContext &                      common;
    int64_t                                 lower_bound;
    int64_t                                 upper_bound;
    size_t                                  range_offset;
    unsigned int                            run_id;
    unsigned int                            restart_count;
    atomic_bool                             data_triggered;
    vector<pair<int32_t, SBlobFullStat>>    keys;

    SLoadKeysOnDataContext() = delete;
    SLoadKeysOnDataContext(SLoadKeysOnDataContext&&) = delete;
    SLoadKeysOnDataContext(const SLoadKeysOnDataContext&) = delete;

    SLoadKeysOnDataContext(SLoadKeysContext &  v_common,
                           int64_t  v_lower_bound, int64_t  v_upper_bound,
                           size_t  v_range_offset, unsigned int  v_run_id) :
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
    SLoadKeysOnDataContext *    context = static_cast<SLoadKeysOnDataContext*>(data);

    ERR_POST(Trace << "Query[" << context->run_id << "] wake: " << &query);
    context->data_triggered = true;
    context->common.m_wakeup.Inc();
}


void CCassBlobOp::x_LoadKeysScheduleNext(CCassQuery &  query, void *  data)
{
    SLoadKeysOnDataContext *    context = static_cast<SLoadKeysOnDataContext*>(data);
    bool                        restart_request = false;
    int32_t                     ent_current = 0;
    bool                        is_eof = false;

    try {
        unsigned int    counter = 0;
        while (true) {
            async_rslt_t    wr;

            if ((wr = query.NextRow()) == ar_dataready) {
                pair<int32_t, SBlobFullStat> row;
                row.first = query.FieldGetInt32Value(0);
                ent_current = row.first;
                row.second.modified = query.FieldIsNull(1) ?
                                        0 : query.FieldGetInt64Value(1);
                row.second.size = query.FieldIsNull(2) ?
                                        0 : query.FieldGetInt64Value(2);
                row.second.flags = query.FieldIsNull(3) ?
                                        (bfCheckFailed | bfComplete) :
                                        query.FieldGetInt64Value(3);
                row.second.seen = false;
                ERR_POST(Trace << "CASS got key=" <<
                         query.GetConnection()->Keyspace() << ":" <<
                         row.first <<" mod=" << row.second.modified <<
                         " sz=" << row.second.size <<
                         " flags=" << row.second.flags);
                context->keys.push_back(std::move(row));

                ++(context->common.m_fetched);
                ++counter;
                //ERR_POST(Message << "Fetched blob with key: " << key);
            } else if (wr == ar_wait) {
                ERR_POST(Trace << "Query[" << context->run_id << "] ar_wait:" << &query);
                break; // expecting next data event triggered on
                       // this same query rowset
            } else if (wr == ar_done) {
                is_eof = true;
                break; // EOF
            } else {
                NCBI_THROW(CCassandraException, eGeneric, "CASS unexpected condition");
            }
        }
        context->restart_count = 0;

        ERR_POST(Trace << "Fetched records by one query: " << counter);
        if (!is_eof)
            return;
    } catch (const CCassandraException& e) {
        if ((e.GetErrCode() == CCassandraException::eQueryTimeout ||
             e.GetErrCode() == CCassandraException::eQueryFailedRestartable) &&
                context->restart_count < 5) {
            restart_request = true;
        } else {
            ERR_POST(Info << "LoadKeys ... exception for entity: " <<
                     ent_current << ", " << e.what());
            throw;
        }
    } catch(...) {
        ERR_POST(Info << "LoadKeys ... exception for entity: " << ent_current);
        throw;
    }

    query.Close();

    if (restart_request) {
        ERR_POST(Info << "QUERY TIMEOUT! Offset " << context->lower_bound <<
                 " - " << context->upper_bound << ". Error count " <<
                 context->restart_count);
        query.SetSQL(context->common.m_sql, 2);
        query.BindInt64(0, context->lower_bound);
        query.BindInt64(1, context->upper_bound);
        query.Query(KEYLOAD_CONSISTENCY, true, true, KEYLOAD_PAGESIZE);
        ++(context->restart_count);
    } else {
        int64_t     upper_bound = context->common.m_ranges[context->range_offset].second;
        int64_t     step = context->common.getStep(context->range_offset);

        query.SetSQL(context->common.m_sql, 2);
        if (context->upper_bound < upper_bound) {
            int64_t     new_upper_bound = (upper_bound - context->upper_bound > step) ?
                                     context->upper_bound + step : upper_bound;

            context->lower_bound = context->upper_bound;
            context->upper_bound = new_upper_bound;
            query.BindInt64(0, context->lower_bound);
            query.BindInt64(1, context->upper_bound);
            query.Query(KEYLOAD_CONSISTENCY, true, true, KEYLOAD_PAGESIZE);
            // ERR_POST(Info << "QUERY DATA " << context->range_offset <<
            //          " " << context->lower_bound << " - " <<
            //          context->upper_bound);
        } else {
            bool    need_run = false;
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


void CCassBlobOp::LoadKeys(CBlobFullStatMap *  keys,
                           function<void()>  tick)
{
    unsigned int                        run_id;
    vector< pair<int64_t, int64_t> >    ranges;
    m_Conn->getTokenRanges(ranges);

    unsigned int concurrent = min<unsigned int>(KEYLOAD_CONCURRENCY,
                                                   static_cast<unsigned int>(ranges.size()));

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

    SLoadKeysContext loadkeys_context(0, concurrent, 0, ranges,
                                                     sql, keys, tick);
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

    ERR_POST(Trace << "LoadKeys: reloading keys from vectors to map");
    for (auto &  it: contexts) {
        CBlobFullStatMap * all_keys = it.common.m_keys;
        if (!it.keys.empty()) {
            for (auto const & row: it.keys) {
                auto it = all_keys->find(row.first);
                if (it != all_keys->end()) {
                    // Extended schema can have multiple versions
                    //  of blob_prop record with different 'modified' field
                    if (it->second.modified < row.second.modified) {
                        it->second = row.second;
                    }
                } else {
                    all_keys->insert(row);
                }
            }
            it.keys.clear();
            it.keys.resize(0);
        }
    }

    ERR_POST(Info << "LoadKeys: finished");
}

/*****************************************************

                ASYNC   DATA   REC

*****************************************************/

struct SAsyncData_rec_t
{
    SAsyncData_rec_t(
        CassConsistency cons = CASS_CONSISTENCY_LOCAL_QUORUM,
        bool run_async = false,
        shared_ptr<CCassConnection> conn = nullptr
    ) : m_run_async(run_async)
      , m_cons(cons)
      , m_activequeryidx(-1)
      , m_Conn(conn)
    {}

    virtual ~SAsyncData_rec_t() = default;

    shared_ptr<CCassQuery> ActiveQuery(void)
    {
        if (m_activequeryidx >= 0) {
            if (m_activequeryidx >= (int)m_queryarr.size()) {
                NCBI_THROW(CCassandraException, eGeneric,
                           "SAsyncData_rec_t::ActiveQuery: index out of range");
            }
            return m_queryarr[m_activequeryidx];
        }
        return nullptr;
    }

    void AddQuery(shared_ptr<CCassQuery>  qry)
    {
        m_queryarr.push_back(qry);
        if (m_activequeryidx < 0) {
            m_activequeryidx = 0;
        }
    }

    virtual async_rslt_t WaitAsync(unsigned int  timeoutmks)
    {
        shared_ptr<CCassQuery> aq = ActiveQuery();
        if (aq) {
            ERR_POST(Trace << "WA qry=" << aq.get() << " >>");
            async_rslt_t rv = aq->WaitAsync(timeoutmks);
            ERR_POST(Trace << "WA qry=" << aq.get() << " rv=" << rv << " <<");
            shared_ptr<CCassQuery> newaq = ActiveQuery();
            if (rv != ar_wait && newaq != nullptr && aq != newaq) {
                rv = ar_wait;
                ERR_POST("WA qry=" << aq.get() << " OVERRIDE rv=" << rv <<
                         ", new qry=" << newaq.get());
            }
            return rv;
        }
        return ar_done;
    }

    virtual bool IsAsync(void) const
    {
        return m_run_async;
    }

    virtual bool IsEOF(void) const
    {
        for(auto  q: m_queryarr)
            if (!q->IsEOF())
                return false;
        return true;
    }

    virtual void Close(void)
    {
        for(auto q : m_queryarr) {
            q->Close();
        }
        m_queryarr.clear();
        m_activequeryidx = -1;
    }

    virtual string ToString(void)
    {
        shared_ptr<CCassQuery>  aq = ActiveQuery();
        return aq ? aq->ToString() : "<nullptr>";
    }

public:
    bool                                m_run_async;
    CassConsistency                     m_cons;
    vector<shared_ptr<CCassQuery> >     m_queryarr;
    int                                 m_activequeryidx;
    shared_ptr<CCassConnection>         m_Conn;
};


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
