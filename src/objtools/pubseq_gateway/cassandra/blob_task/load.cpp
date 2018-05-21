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
 * Authors: Dmitrii Saprykin
 *
 * File Description:
 *
 * Cassandra delete blob task.
 *
 */

#include <ncbi_pch.hpp>

#include <objtools/pubseq_gateway/impl/cassandra/cass_blob_op.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/cass_driver.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/IdCassScope.hpp>

#include <unistd.h>

#define MAX_CHUNKS_AHEAD 4

BEGIN_IDBLOB_SCOPE
USING_NCBI_SCOPE;

CCassBlobLoader::CCassBlobLoader(
    unsigned int op_timeout_ms,
    shared_ptr<CCassConnection> conn,
    const string & keyspace,
    int32_t key,
    bool async,
    unsigned int max_retries,
    void * context,
    const DataChunkCB_t & data_chunk_cb,
    const DataErrorCB_t & DataErrorCB
)
    : CCassBlobWaiter(
        op_timeout_ms,
        conn,
        keyspace,
        key,
        async,
        max_retries,
        context,
        DataErrorCB
    )
    , m_StatLoaded(false)
    , m_DataCb(data_chunk_cb)
    , m_ExpectedSize(0)
    , m_RemainingSize(0)
    , m_LargeParts(0)
{
    ERR_POST(Info << "CCassBlobLoader::CCassBlobLoader max_retries=" <<
             max_retries);
}

void CCassBlobLoader::SetDataChunkCB(DataChunkCB_t &&  datacb)
{
    m_DataCb = std::move(datacb);
}

void CCassBlobLoader::SetDataReadyCB(DataReadyCB_t  datareadycb, void *  data)
{
    if (datareadycb && m_State != eInit)
        NCBI_THROW(CCassandraException, eSeqFailed,
                   "CCassBlobLoader: DataReadyCB can't be assigned "
                   "after the loading process has started");
    CCassBlobWaiter::SetDataReadyCB(datareadycb, data);
}

SBlobStat CCassBlobLoader::GetBlobStat(void) const
{
    if (!m_StatLoaded)
        NCBI_THROW(CCassandraException, eSeqFailed,
                   "CCassBlobLoader: Blob stat can't be read");
    return m_BlobStat;
}

uint64_t CCassBlobLoader::GetBlobSize(void) const
{
    if (!m_StatLoaded)
        NCBI_THROW(CCassandraException, eSeqFailed,
                   "CCassBlobLoader: Blob stat can't be read");
    return m_ExpectedSize;

}

void CCassBlobLoader::Cancel(void)
{
    if (m_State == eDone)
        return;

    m_Cancelled = true;
    CloseAll();
    m_State = eError;
}

bool CCassBlobLoader::Restart(unsigned int max_retries)
{
    m_StatLoaded = false;
    m_BlobStat.Reset();
    m_ExpectedSize = 0;
    m_RemainingSize = 0;
    m_LargeParts = 0;
    return CCassBlobWaiter::Restart(max_retries);
}

int32_t CCassBlobLoader::GetTotalChunksInBlob(void) const
{
    return m_LargeParts;
}

void CCassBlobLoader::x_RequestChunk(shared_ptr<CCassQuery> qry, int local_id)
{
    CassConsistency c = (m_RestartCounter > 0 && m_Conn->GetFallBackRdConsistency()) ?
        CASS_CONSISTENCY_LOCAL_QUORUM : CASS_CONSISTENCY_LOCAL_ONE;
    string sql = "SELECT data FROM " + GetKeySpace() + ".largeentity WHERE ent = ? AND local_id = ?";
    ERR_POST(Trace << "reading LARGE blob part, key=" << m_Keyspace <<
             "." << m_Key << ", local_id=" << local_id << ", qry: " << qry.get());
    qry->SetSQL(sql, 2);
    qry->BindInt32(0, m_Key);
    qry->BindInt32(1, local_id);
    if (m_DataReadyCb) {
        qry->SetOnData2(m_DataReadyCb, m_DataReadyData);
    }
    UpdateLastActivity();
    qry->Query(c, true);
}

void CCassBlobLoader::x_RequestFlags(shared_ptr<CCassQuery> qry, bool with_data)
{
    CassConsistency c = (m_RestartCounter > 0 && m_Conn->GetFallBackRdConsistency()) ?
        CASS_CONSISTENCY_LOCAL_QUORUM : CASS_CONSISTENCY_LOCAL_ONE;
    string sql;
    if (with_data) {
        sql = "SELECT modified, flags, size, large_parts, data FROM " +
            GetKeySpace() + ".entity WHERE ent = ?";
    }
    else {
        sql = "SELECT modified, flags, size, large_parts       FROM " +
            GetKeySpace() + ".entity WHERE ent = ?";
    }

    qry->SetSQL(sql, 1);
    qry->BindInt32(0, m_Key);
    if (m_DataReadyCb) {
        qry->SetOnData2(m_DataReadyCb, m_DataReadyData);
    }
    UpdateLastActivity();
    qry->Query(c, m_Async);
}


void CCassBlobLoader::x_RequestChunksAhead(void)
{
    int cnt = MAX_CHUNKS_AHEAD;
    for (int32_t local_id = 0; local_id < m_LargeParts && cnt > 0; ++local_id) {
        if (!m_ProcessedChunks[local_id]) {
            --cnt;
            auto qry = m_QueryArr[local_id];
            if (!qry->IsActive()) {
                if (!CheckMaxActive()) {
                    break;
                }
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

    while ((int)m_QueryArr.size() < m_LargeParts) {
        m_QueryArr.emplace_back(m_Conn->NewQuery());
    }

    m_ProcessedChunks.clear();
    m_ProcessedChunks.reserve(m_LargeParts);
    while ((int)m_ProcessedChunks.size() < m_LargeParts) {
        m_ProcessedChunks.emplace_back(false);
    }
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
            bool local_need_repeat = false;
            bool ready = CheckReady(
                m_QueryArr[index],
                eReadingChunks,
                &local_need_repeat
            );
            if (ready) {
                return index;
            }

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
        if (!is_ready) {
            return false;
        }
    }
    return true;
}

void CCassBlobLoader::x_MarkChunkProcessed(size_t  chunk_no)
{
    m_ProcessedChunks[chunk_no] = true;
}

void CCassBlobLoader::Wait1(void)
{
    bool b_need_repeat;
    char msg[1024];
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
                if (!CheckMaxActive()) {
                    break;
                }
                x_RequestFlags(qry, true);
                m_State = eReadingEntity;
                break;
            }

            case eReadingEntity: {
                auto qry = m_QueryArr[0];
                if (!qry->IsActive()) {
                    ERR_POST(Warning << "re-starting initial query");
                    if (!CheckMaxActive()) {
                        break;
                    }
                    x_RequestFlags(qry, true);
                }

                if (!CheckReady(qry, eInit, &b_need_repeat)) {
                    if (b_need_repeat) {
                        ERR_POST(Info << "Restart stReadingEntity key=" <<
                            m_Keyspace << "." << m_Key);
                    }
                    break;
                }

                if (qry->IsEOF()) {
                    UpdateLastActivity();
                    CloseAll();
                    m_State = eDone; // no data
                } else {
                    async_rslt_t wr = qry->NextRow();
                    if (wr == ar_wait) {// paging
                        break;
                    }
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
                    bool have_inactive;
                    bool need_repeat;
                    int ready_chunk_no = x_GetReadyChunkNo(have_inactive, need_repeat);
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
                    async_rslt_t wr = ar_done;
                    auto qry = m_QueryArr[ready_chunk_no];
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

END_IDBLOB_SCOPE
