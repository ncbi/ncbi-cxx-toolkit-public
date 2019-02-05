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
 * Cassandra insert blob according to normal schema task.
 *
 */

#include <ncbi_pch.hpp>

#include "insert.hpp"

#include <objtools/pubseq_gateway/impl/cassandra/cass_blob_op.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/cass_driver.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/IdCassScope.hpp>

BEGIN_IDBLOB_SCOPE
USING_NCBI_SCOPE;

CCassBlobTaskInsert::CCassBlobTaskInsert(
        unsigned int op_timeout_ms,
        shared_ptr<CCassConnection> conn,
        const string & keyspace,
        int32_t key,
        CBlobRecord * blob,
        ECassTristate is_new,
        int64_t large_treshold,
        int64_t large_chunk_sz,
        bool async,
        unsigned int max_retries,
        TDataErrorCallback data_error_cb
)
    : CCassBlobWaiter(
        op_timeout_ms, conn, keyspace, key, async,
        max_retries, move(data_error_cb)
      )
    , m_LargeTreshold(large_treshold)
    , m_LargeChunkSize(large_chunk_sz)
    , m_LargeParts(0)
    , m_Blob(blob)
    , m_IsNew(is_new)
    , m_OldLargeParts(0)
    , m_OldFlags(0)
{}

void CCassBlobTaskInsert::Wait1()
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
                    m_QueryArr.push_back({m_Conn->NewQuery(), 0});
                }
                m_LargeParts = m_Blob->GetSize() >= m_LargeTreshold ? m_Blob->GetNChunks() : 0;
                if (m_IsNew != eTrue) {
                    ERR_POST(Trace << "CCassBlobInserter: reading stat for "
                             "key=" << m_Keyspace << "." << m_Key <<
                             ", rslt=" << m_Blob);
                    auto qry = m_QueryArr[0].query;
                    CassConsistency c = CASS_CONSISTENCY_LOCAL_QUORUM;

                    // We have to check whether blob exists in largeentity
                    // and remove it if new blob has smaller number of parts
                    string  sql = "SELECT flags, large_parts FROM " + GetKeySpace() + ". entity WHERE ent = ?";
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
                auto& it = m_QueryArr[0];
                if (!CheckReadyEx(it)) {
                    break;
                }

                async_rslt_t wr = (async_rslt_t) -1;
                if (!it.query->IsEOF()) {
                    if ((wr = it.query->NextRow()) == ar_dataready) {
                        UpdateLastActivity();
                        m_OldFlags = it.query->FieldGetInt64Value(0);
                        m_OldLargeParts = it.query->FieldGetInt32Value(1);
                        it.query->Close();
                        it.restart_count = 0;
                        ERR_POST(Trace << "CCassBlobInserter: old_large_parts=" <<
                                 m_OldLargeParts << ", old_flags=" << m_OldFlags <<
                                 ", key=" << m_Keyspace << "." << m_Key);
                    } else if (wr == ar_wait)
                        break;
                }
                if (it.query->IsEOF() || wr == ar_done) {
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
                auto            qry = m_QueryArr[0].query;

                qry->Close();
                qry->NewBatch();

                sql = "UPDATE " + GetKeySpace() + ". entity SET flags = ? WHERE ent = ?";
                ERR_POST(Trace << "CCassBlobInserter: drop  flags "
                         "to invalidate key=" << m_Keyspace << "." << m_Key);
                qry->SetSQL(sql, 2);
                qry->BindInt64(0, m_OldFlags & ~(bfComplete | bfCheckFailed));
                qry->BindInt32(1, m_Key);
                UpdateLastActivity();
                qry->Execute(CASS_CONSISTENCY_LOCAL_QUORUM, m_Async);

                for (int  i = m_LargeParts; i < m_OldLargeParts; ++i) {
                    sql = "DELETE FROM " + GetKeySpace() + ".largeentity WHERE ent = ? AND local_id = ?";
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
                auto&           it = m_QueryArr[0];

                if (!CheckReadyEx(it)) {
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
                auto qry = m_QueryArr[0].query;

                if (!qry->IsActive()) {
                    string sql =
                        "INSERT INTO " + GetKeySpace() +
                        ".entity (ent, modified, size, flags, "
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
                            m_QueryArr.push_back({m_Conn->NewQuery(), 0});
                        }
                        qry = m_QueryArr[i + 1].query;
                        if (!qry->IsActive()) {
                            m_QueryArr[i + 1].restart_count = 0;
                            sql = "INSERT INTO " + GetKeySpace() +
                                  ".largeentity (ent, local_id, data) VALUES(?, ?, ?)";
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
                for (auto& it: m_QueryArr) {
                    if (it.query->IsActive()) {
                        if (!CheckReadyEx(it)) {
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
                    auto qry = m_QueryArr[0].query;
                    string sql = "UPDATE " + GetKeySpace() + ".entity set flags = ? where ent = ?";
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
                auto& it = m_QueryArr[0];
                if (!CheckReadyEx(it)) {
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

END_IDBLOB_SCOPE
