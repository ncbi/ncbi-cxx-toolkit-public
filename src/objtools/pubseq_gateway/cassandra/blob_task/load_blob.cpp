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
 * Cassandra load blob properties according to extended schema.
 *
 */

#include <ncbi_pch.hpp>

#include <objtools/pubseq_gateway/impl/cassandra/blob_task/load_blob.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/cass_blob_op.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/cass_driver.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/IdCassScope.hpp>

#include <vector>
#include <memory>
#include <utility>
#include <string>

#include <unistd.h>

BEGIN_IDBLOB_SCOPE
USING_NCBI_SCOPE;

CCassBlobTaskLoadBlob::CCassBlobTaskLoadBlob(
    unsigned int op_timeout_ms,
    unsigned int max_retries,
    shared_ptr<CCassConnection> conn,
    const string & keyspace,
    CBlobRecord::TSatKey sat_key,
    bool load_chunks,
    TDataErrorCallback data_error_cb
)
    : CCassBlobTaskLoadBlob(
        op_timeout_ms,
        max_retries,
        conn,
        keyspace,
        sat_key,
        kAnyModified,
        load_chunks,
        data_error_cb
    )
{
}

CCassBlobTaskLoadBlob::CCassBlobTaskLoadBlob(
    unsigned int op_timeout_ms,
    unsigned int max_retries,
    shared_ptr<CCassConnection> conn,
    const string & keyspace,
    CBlobRecord::TSatKey sat_key,
    CBlobRecord::TTimestamp modified,
    bool load_chunks,
    TDataErrorCallback data_error_cb
)
    : CCassBlobWaiter(
        op_timeout_ms, conn, keyspace, sat_key, true,
        max_retries, move(data_error_cb)
      )
    , m_ChunkCallback(nullptr)
    , m_PropsCallback(nullptr)
    , m_Blob(new CBlobRecord(sat_key))
    , m_Modified(modified)
    , m_LoadChunks(load_chunks)
    , m_PropsFound(false)
    , m_ActiveQueries(0)
    , m_RemainingSize(0)
{
    m_Blob->SetModified(modified);
}

CCassBlobTaskLoadBlob::CCassBlobTaskLoadBlob(
    unsigned int op_timeout_ms,
    unsigned int max_retries,
    shared_ptr<CCassConnection> conn,
    const string & keyspace,
    unique_ptr<CBlobRecord> blob_record,
    bool load_chunks,
    TDataErrorCallback data_error_cb
)
    : CCassBlobWaiter(
        op_timeout_ms, conn, keyspace, blob_record->GetKey(), true,
        max_retries, move(data_error_cb)
      )
    , m_ChunkCallback(nullptr)
    , m_PropsCallback(nullptr)
    , m_Blob(move(blob_record))
    , m_Modified(m_Blob->GetModified())
    , m_LoadChunks(load_chunks)
    , m_PropsFound(true)
    , m_ActiveQueries(0)
    , m_RemainingSize(0)
{
    m_State = eFinishedPropsFetch;
}

bool CCassBlobTaskLoadBlob::IsBlobPropsFound() const
{
    return m_PropsFound;
}

void CCassBlobTaskLoadBlob::SetChunkCallback(TBlobChunkCallback callback)
{
    m_ChunkCallback = move(callback);
}

void CCassBlobTaskLoadBlob::SetPropsCallback(TBlobPropsCallback callback)
{
    m_PropsCallback = move(callback);
}

void CCassBlobTaskLoadBlob::SetDataReadyCB(TDataReadyCallback callback, void * data)
{
    if (callback && m_State != eInit) {
        NCBI_THROW(CCassandraException, eSeqFailed,
           "CCassBlobTaskLoadBlob: DataReadyCB can't be assigned "
           "after the loading process has started");
    }
    CCassBlobWaiter::SetDataReadyCB(callback, data);
}

void CCassBlobTaskLoadBlob::Wait1()
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
                m_QueryArr.clear();
                m_QueryArr.emplace_back(m_Conn->NewQuery());
                auto qry = m_QueryArr[0];
                string sql = "SELECT "
                    "   last_modified,"
                    "   class,"
                    "   date_asn1,"
                    "   div,"
                    "   flags,"
                    "   hup_date,"
                    "   id2_info,"
                    "   n_chunks,"
                    "   owner,"
                    "   size,"
                    "   size_unpacked,"
                    "   username"
                    " FROM " + GetKeySpace() + ".blob_prop WHERE sat_key = ?";
                if (m_Blob->GetModified() == kAnyModified) {
                    sql += " LIMIT 1";
                    qry->SetSQL(sql, 1);
                    qry->BindInt32(0, m_Key);
                } else {
                    sql += " and last_modified = ?";
                    qry->SetSQL(sql, 2);
                    qry->BindInt32(0, m_Key);
                    qry->BindInt64(1, m_Blob->GetModified());
                }

                if (m_DataReadyCb) {
                    qry->SetOnData2(m_DataReadyCb, m_DataReadyData);
                }
                UpdateLastActivity();
                qry->Query(GetQueryConsistency(), m_Async, true);
                m_State = eWaitingForPropsFetch;
                break;
            }

            case eWaitingForPropsFetch: {
                auto qry = m_QueryArr[0];
                if (!CheckReady(qry, eInit, &b_need_repeat)) {
                    if (b_need_repeat) {
                        ERR_POST(Info << "Restart eWaitingForSelect key=" << m_Keyspace << "." << m_Key);
                    }
                    break;
                }

                if (!qry->IsEOF()) {
                    async_rslt_t wr = qry->NextRow();
                    if (wr == ar_dataready) {
                        UpdateLastActivity();
                        (*m_Blob)
                            .SetModified(qry->FieldGetInt64Value(0))
                            .SetClass(qry->FieldGetInt16Value(1))
                            .SetDateAsn1(qry->FieldGetInt64Value(2, 0))
                            .SetDiv(qry->FieldGetStrValueDef(3, ""))
                            .SetFlags(qry->FieldGetInt64Value(4))
                            .SetHupDate(qry->FieldGetInt64Value(5, 0))
                            .SetId2Info(qry->FieldGetStrValueDef(6, ""))
                            .SetNChunks(qry->FieldGetInt32Value(7))
                            .SetOwner(qry->FieldGetInt32Value(8))
                            .SetSize(qry->FieldGetInt64Value(9))
                            .SetSizeUnpacked(qry->FieldGetInt64Value(10))
                            .SetUserName(qry->FieldGetStrValueDef(11, ""));
                        m_RemainingSize = m_Blob->GetSize();
                        m_PropsFound = true;
                        qry->Close();
                    } else if (wr == ar_wait) {
                        break;
                    }
                }

                CloseAll();
                m_QueryArr.clear();
                m_State = eFinishedPropsFetch;
                b_need_repeat = true;
            }
            break;

            case eFinishedPropsFetch:
                if (m_PropsCallback) {
                    m_PropsCallback(*m_Blob, m_PropsFound);
                }
                if (m_LoadChunks) {
                    if (!m_PropsFound) {
                        if (!m_PropsCallback) {
                            string msg = "Blob not found, key: " + m_Keyspace + "." + NStr::NumericToString(m_Key);
                            // Call a CB which tells that a 404 reply should be sent
                            Error(
                                CRequestStatus::e404_NotFound,
                                CCassandraException::eNotFound,
                                eDiag_Error, msg.c_str()
                            );
                        } else {
                            m_State = eDone;
                        }
                    } else {
                        if (m_Blob->GetNChunks() < 0) {
                            string msg = "Inconsistent n_chunks value: " + NStr::NumericToString(m_Blob->GetNChunks());
                            Error(
                                CRequestStatus::e500_InternalServerError,
                                CCassandraException::eInconsistentData,
                                eDiag_Error,
                                msg
                            );
                        } else {
                            m_ProcessedChunks = vector<bool>(static_cast<size_t>(m_Blob->GetNChunks()), false);
                            m_QueryArr.resize(static_cast<size_t>(m_Blob->GetNChunks()));
                            m_State = eBeforeLoadingChunks;
                            b_need_repeat = true;
                        }
                    }
                } else {
                    m_State = eDone;
                }
            break;

            case eBeforeLoadingChunks:
                if (!m_ChunkCallback && m_LoadChunks) {
                    m_ChunkCallback =
                        [this] (const unsigned char * data, unsigned int size, int chunk_no) {
                            if (chunk_no >= 0) {
                                this->m_Blob->InsertBlobChunk(chunk_no, CBlobRecord::TBlobChunk(data, data + size));
                            }
                        };
                }
                m_State = eLoadingChunks;
                b_need_repeat = true;
            break;

            case eLoadingChunks: {
                while(!x_AreAllChunksProcessed()) {
                    x_RequestChunksAhead();
                    x_CheckChunksFinished(b_need_repeat);
                    if (b_need_repeat) {
                        continue;
                    }
                    return;
                }
                if (x_AreAllChunksProcessed() && m_State != eError) {
                    if (m_RemainingSize > 0) {
                        char msg[1024]; msg[0] = '\0';
                        snprintf(msg, sizeof(msg),
                             "Failed to fetch blob (key=%s.%d) result is "
                             "incomplete remaining %ld bytes",
                             m_Keyspace.c_str(), m_Key, m_RemainingSize);
                        Error(CRequestStatus::e502_BadGateway,
                              CCassandraException::eInconsistentData,
                              eDiag_Error, msg);
                        break;
                    }
                    m_ChunkCallback(nullptr, 0, -1);
                    m_State = eDone;
                }
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

void CCassBlobTaskLoadBlob::Cancel(void)
{
    if (m_State != eDone) {
        m_Cancelled = true;
        CloseAll();
        m_QueryArr.clear();
        m_State = eError;
    }
}

bool CCassBlobTaskLoadBlob::Restart()
{
    m_Blob.reset(new CBlobRecord(m_Key));
    m_Blob->SetModified(m_Modified);
    m_RemainingSize = 0;
    m_PropsFound = false;
    m_ActiveQueries = 0;
    m_ProcessedChunks = vector<bool>(static_cast<size_t>(m_Blob->GetNChunks()), false);
    CloseAll();
    m_QueryArr.clear();

    return CCassBlobWaiter::Restart();
}

bool CCassBlobTaskLoadBlob::x_AreAllChunksProcessed(void) const
{
    for (const auto & is_ready : m_ProcessedChunks) {
        if (!is_ready) {
            return false;
        }
    }
    return true;
}

void CCassBlobTaskLoadBlob::x_CheckChunksFinished(bool& need_repeat)
{
    for (
        int32_t chunk_no = 0;
        chunk_no < m_Blob->GetNChunks() && m_ActiveQueries > 0;
        ++chunk_no
    ) {
        if (!m_ProcessedChunks[chunk_no]) {
            auto& qry = m_QueryArr[chunk_no];
            if (qry) {
                bool chunk_repeat = false;
                bool ready = CheckReady(qry, eLoadingChunks, &chunk_repeat);
                if (ready) {
                    if (!qry->IsEOF()) {
                        async_rslt_t wr = qry->NextRow();
                        if (wr == ar_dataready) {
                            UpdateLastActivity();
                            const unsigned char * rawdata = nullptr;
                            int64_t len = qry->FieldGetBlobRaw(0, &rawdata);
                            m_RemainingSize -= len;
                            if (m_RemainingSize < 0) {
                                char msg[1024];
                                msg[0] = '\0';
                                snprintf(msg, sizeof(msg),
                                     "Failed to fetch blob chunk (key=%s.%d, chunk=%d) size %ld "
                                     "is too large", m_Keyspace.c_str(), m_Key, chunk_no, len);
                                Error(CRequestStatus::e502_BadGateway,
                                      CCassandraException::eInconsistentData,
                                      eDiag_Error, msg);
                                return;
                            }
                            m_ProcessedChunks[chunk_no] = true;
                            m_ChunkCallback(rawdata, len, chunk_no);
                            qry->Close();
                            qry = nullptr;
                            --m_ActiveQueries;
                            need_repeat = true;
                        } else if (wr == ar_wait) {
                            continue;
                        }
                    } else {
                        chunk_repeat = true;
                    }
                }

                if (chunk_repeat) {
                    qry = nullptr;
                    --m_ActiveQueries;
                    if (CanRestart()) {
                        need_repeat = true;
                        ERR_POST(Info << "Restart eLoadingChunks required for key=" <<
                                m_Keyspace << "." << m_Key << ", chunk=" << chunk_no);
                    } else {
                        char msg[1024];
                        msg[0] = '\0';
                        snprintf(msg, sizeof(msg),
                             "Failed to fetch blob chunk  (key=%s.%d, chunk=%d)",
                             m_Keyspace.c_str(), m_Key, chunk_no);
                        Error(CRequestStatus::e502_BadGateway,
                              CCassandraException::eFetchFailed,
                              eDiag_Error, msg);
                        return;
                    }
                }
            }
        }
    }
}

void CCassBlobTaskLoadBlob::x_RequestChunksAhead(void)
{
    for (
        int32_t chunk_no = 0;
        chunk_no < m_Blob->GetNChunks() && m_ActiveQueries < kMaxChunksAhead;
        ++chunk_no
    ) {
        if (!m_ProcessedChunks[chunk_no]) {
            if (!m_QueryArr[chunk_no]) {
                if (!CheckMaxActive()) {
                    break;
                }
                m_QueryArr[chunk_no] = m_Conn->NewQuery();
                x_RequestChunk(*m_QueryArr[chunk_no], chunk_no);
                ++m_ActiveQueries;
            }
        }
    }
}

void CCassBlobTaskLoadBlob::x_RequestChunk(CCassQuery& qry, int32_t chunk_no)
{
    string sql = "SELECT data FROM " + GetKeySpace() +
        ".blob_chunk WHERE sat_key = ? AND last_modified = ? AND chunk_no = ?";
    qry.SetSQL(sql, 3);
    qry.BindInt32(0, m_Key);
    qry.BindInt64(1, m_Blob->GetModified());
    qry.BindInt32(2, chunk_no);
    if (m_DataReadyCb) {
        qry.SetOnData2(m_DataReadyCb, m_DataReadyData);
    }
    UpdateLastActivity();
    qry.Query(GetQueryConsistency(), true);
}

unique_ptr<CBlobRecord> CCassBlobTaskLoadBlob::ConsumeBlobRecord()
{
    unique_ptr<CBlobRecord> tmp(new CBlobRecord(m_Key));
    tmp->SetModified(m_Blob->GetModified());
    tmp->SetNChunks(m_Blob->GetNChunks());
    m_PropsFound = false;
    swap(tmp, m_Blob);
    return tmp;
}

END_IDBLOB_SCOPE
