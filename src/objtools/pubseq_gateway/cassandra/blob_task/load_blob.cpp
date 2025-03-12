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

#include <objtools/pubseq_gateway/impl/cassandra/blob_storage.hpp>
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

void CCassBlobTaskLoadBlob::InitBlobChunkDataQuery(
    CCassQuery* query, string const& keyspace, CBlobRecord const& blob, int32_t chunk_no
) {
    if (query != nullptr) {
        const char* const chunk_table_name = blob.GetFlag(EBlobFlags::eBigBlobSchema)
             ? SBlobStorageConstants::kChunkTableBig
             : SBlobStorageConstants::kChunkTableDefault;
        query->SetSQL("SELECT data FROM " + keyspace + "." + chunk_table_name
            + " WHERE sat_key = ? AND last_modified = ? AND chunk_no = ?", 3);
        query->BindInt32(0, blob.GetKey());
        query->BindInt64(1, blob.GetModified());
        query->BindInt32(2, chunk_no);
    }
}

CCassBlobTaskLoadBlob::CCassBlobTaskLoadBlob(
    shared_ptr<CCassConnection> conn,
    const string & keyspace,
    CBlobRecord::TSatKey sat_key,
    bool load_chunks,
    TDataErrorCallback data_error_cb
)
    : CCassBlobTaskLoadBlob(
        std::move(conn),
        keyspace,
        sat_key,
        kAnyModified,
        load_chunks,
        std::move(data_error_cb)
    )
{
}

CCassBlobTaskLoadBlob::CCassBlobTaskLoadBlob(
    shared_ptr<CCassConnection> conn,
    const string & keyspace,
    CBlobRecord::TSatKey sat_key,
    CBlobRecord::TTimestamp modified,
    bool load_chunks,
    TDataErrorCallback data_error_cb
)
    : CCassBlobWaiter(std::move(conn), keyspace, sat_key, true, std::move(data_error_cb))
    , m_Blob(make_unique<CBlobRecord>(sat_key))
    , m_Modified(modified)
    , m_LoadChunks(load_chunks)
    , m_Mode(eBlobTaskModeDefault)
{
    m_Blob->SetModified(modified);
}

CCassBlobTaskLoadBlob::CCassBlobTaskLoadBlob(
    shared_ptr<CCassConnection> conn,
    const string & keyspace,
    unique_ptr<CBlobRecord> blob_record,
    bool load_chunks,
    TDataErrorCallback data_error_cb
)
    : CCassBlobWaiter(std::move(conn), keyspace, blob_record->GetKey(), true, std::move(data_error_cb))
    , m_Blob(std::move(blob_record))
    , m_Modified(m_Blob->GetModified())
    , m_LoadChunks(load_chunks)
    , m_PropsFound(true)
    , m_ExplicitBlob(true)
    , m_Mode(eBlobTaskModeDefault)
{}

bool CCassBlobTaskLoadBlob::IsBlobPropsFound() const
{
    return m_PropsFound;
}

void CCassBlobTaskLoadBlob::SetChunkCallback(TBlobChunkCallbackEx callback)
{
    m_ChunkCallback = std::move(callback);
}

void CCassBlobTaskLoadBlob::SetPropsCallback(TBlobPropsCallback callback)
{
    m_PropsCallback = std::move(callback);
}

void CCassBlobTaskLoadBlob::SetDataReadyCB(shared_ptr<CCassDataCallbackReceiver> callback)
{
    if (callback && m_State != eInit) {
        NCBI_THROW(CCassandraException, eSeqFailed,
           "CCassBlobTaskLoadBlob: DataReadyCB can't be assigned after the loading process has started");
    }
    CCassBlobWaiter::SetDataReadyCB3(std::move(callback));
}

void CCassBlobTaskLoadBlob::Wait1()
{
    bool b_need_repeat{false};
    do {
        b_need_repeat = false;
        switch (m_State) {
            case eError:
            case eDone:
                return;
            case eInit:
                // m_Blob has already been provided explicitly so we can move to eFinishedPropsFetch
                if (m_ExplicitBlob) {
                    m_State = eFinishedPropsFetch;
                    m_RemainingSize = m_Blob->GetSize();
                    b_need_repeat = true;
                } else {
                    CloseAll();
                    m_QueryArr.clear();
                    m_QueryArr.push_back({ProduceQuery(), 0});
                    auto qry = m_QueryArr[0].query;
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
                        qry->BindInt32(0, GetKey());
                    } else {
                        sql += " and last_modified = ?";
                        qry->SetSQL(sql, 2);
                        qry->BindInt32(0, GetKey());
                        qry->BindInt64(1, m_Blob->GetModified());
                    }

                    SetupQueryCB3(qry);
                    qry->Query(GetReadConsistency(), m_Async, m_UsePrepared);
                    m_State = eWaitingForPropsFetch;
                }
                break;

            case eWaitingForPropsFetch: {
                auto& it = m_QueryArr[0];
                if (CheckReady(it)) {
                    it.query->NextRow();
                    if (!it.query->IsEOF()) {
                        (*m_Blob)
                            .SetModified(it.query->FieldGetInt64Value(0))
                            .SetClass(it.query->FieldGetInt16Value(1))
                            .SetDateAsn1(it.query->FieldGetInt64Value(2, 0))
                            .SetDiv(it.query->FieldGetStrValueDef(3, ""))
                            .SetFlags(it.query->FieldGetInt64Value(4))
                            .SetHupDate(it.query->FieldGetInt64Value(5, 0))
                            .SetId2Info(it.query->FieldGetStrValueDef(6, ""))
                            .SetNChunks(it.query->FieldGetInt32Value(7))
                            .SetOwner(it.query->FieldGetInt32Value(8))
                            .SetSize(it.query->FieldGetInt64Value(9))
                            .SetSizeUnpacked(it.query->FieldGetInt64Value(10))
                            .SetUserName(it.query->FieldGetStrValueDef(11, ""));
                        m_RemainingSize = m_Blob->GetSize();
                        m_PropsFound = true;
                        it.query->Close();
                    }
                    CloseAll();
                    m_QueryArr.clear();
                    m_State = eFinishedPropsFetch;
                    b_need_repeat = true;
                }
            }
            break;

            case eFinishedPropsFetch:
                if (m_PropsCallback) {
                    m_PropsCallback(*m_Blob, m_PropsFound);
                    // ID-8131 If Cancel() call was performed during blob_prop callback we need to stop processing
                    // m_State is expected to have a final state value after CCassBlobTaskLoadBlob::Cancel() call
                    if (m_Cancelled) {
                        return;
                    }
                }
                if (m_LoadChunks) {
                    if (!m_PropsFound) {
                        if (!m_PropsCallback) {
                            string msg = "Blob not found, key: " + GetKeySpace() + "." + to_string(GetKey());
                            // Call a CB which tells that a 404 reply should be sent
                            Error(CRequestStatus::e404_NotFound, CCassandraException::eNotFound, eDiag_Error, msg);
                        } else {
                            m_State = eDone;
                        }
                    } else {
                        TBlobFlagBase flags = m_Blob->GetFlags();
                        if ((flags & static_cast<TBlobFlagBase>(EBlobFlags::eCheckFailed)) != 0) {
                            string msg = "Blob failed check or it's "
                                         "incomplete ("
                                         "key=" + GetKeySpace() + "." + to_string(GetKey()) +
                                         ", modified=" + to_string(m_Blob->GetModified()) +
                                         ", flags=0x" + NStr::NumericToString(flags, 16) + ")";
                            Error(CRequestStatus::e502_BadGateway,
                                  CCassandraException::eInconsistentData,
                                  eDiag_Error, msg);
                        }
                        else if (m_Blob->GetNChunks() < 0) {
                            string msg = "Inconsistent n_chunks value: " + to_string(m_Blob->GetNChunks()) +
                                         " (key=" + GetKeySpace() + "." + to_string(GetKey()) + ")";
                            Error(
                                CRequestStatus::e500_InternalServerError,
                                CCassandraException::eInconsistentData,
                                eDiag_Error,
                                msg
                            );
                        } else {
                            m_ProcessedChunks = vector(static_cast<size_t>(m_Blob->GetNChunks()), false);
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
                        [this] (CBlobRecord const &, const unsigned char * data, unsigned int size, int chunk_no) {
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
                    x_CheckChunksFinished(b_need_repeat);
                    x_RequestChunksAhead();
                    if (b_need_repeat) {
                        b_need_repeat = false;
                        continue;
                    }
                    return;
                }
                if (x_AreAllChunksProcessed() && m_State != eError) {
                    if (m_RemainingSize > 0) {
                        char msg[1024]; msg[0] = '\0';
                        string keyspace = GetKeySpace();
                        snprintf(msg, sizeof(msg),
                             "Failed to fetch blob (key=%s.%d) result is "
                             "incomplete remaining %ld bytes",
                             keyspace.c_str(), GetKey(), m_RemainingSize);
                        Error(CRequestStatus::e502_BadGateway,
                              CCassandraException::eInconsistentData,
                              eDiag_Error, msg);
                        break;
                    }
                    m_ChunkCallback(*m_Blob, nullptr, 0, -1);
                    m_State = eDone;
                }
                break;
            }
            
            default: {
                char msg[1024];
                string keyspace = GetKeySpace();
                snprintf(msg, sizeof(msg),
                    "Failed to fetch blob (key=%s.%d) unexpected state (%d)",
                    keyspace.c_str(), GetKey(), static_cast<int>(m_State));
                Error(CRequestStatus::e502_BadGateway, CCassandraException::eQueryFailed, eDiag_Error, msg);
            }
        }
    } while( b_need_repeat);
}

bool CCassBlobTaskLoadBlob::x_AreAllChunksProcessed() const
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
    auto n_chunks = m_Blob->GetNChunks();
    for (int32_t chunk_no = 0; chunk_no < n_chunks && m_ActiveQueries > 0; ++chunk_no) {
        if (!m_ProcessedChunks[chunk_no]) {
            if (m_State == eDone || m_State == eError || m_Cancelled) {
                break;
            }
            auto& it = m_QueryArr[chunk_no];
            if (it.query) {
                if (CheckReady(it)) {
                    it.query->NextRow();
                    if (it.query->IsEOF()) {
                        char msg[1024];
                        string keyspace = GetKeySpace();
                        snprintf(msg, sizeof(msg),
                             "Failed to fetch blob chunk (key=%s.%d, chunk=%d)", keyspace.c_str(), GetKey(), chunk_no);
                        Error(CRequestStatus::e502_BadGateway, CCassandraException::eInconsistentData, eDiag_Error, msg);
                        return;
                    } else {
                        const unsigned char * rawdata = nullptr;
                        int64_t len = it.query->FieldGetBlobRaw(0, &rawdata);
                        m_RemainingSize -= len;
                        if (m_RemainingSize < 0) {
                            char msg[1024];
                            msg[0] = '\0';
                            string keyspace = GetKeySpace();
                            snprintf(msg, sizeof(msg),
                                 "Failed to fetch blob chunk (key=%s.%d, chunk=%d) size %ld "
                                 "is too large", keyspace.c_str(), GetKey(), chunk_no, len);
                            Error(CRequestStatus::e502_BadGateway, CCassandraException::eInconsistentData, eDiag_Error, msg);
                            return;
                        }
                        m_ProcessedChunks[chunk_no] = true;
                        m_ChunkCallback(*m_Blob, rawdata, len, chunk_no);
                        it.query->Close();
                        it.query = nullptr;
                        --m_ActiveQueries;
                        need_repeat = true;
                    }
                }
            }
        }
    }
}

void CCassBlobTaskLoadBlob::x_RequestChunksAhead()
{
    auto n_chunks = m_Blob->GetNChunks();
    bool passed_active_check = true;
    for (int32_t chunk_no = 0;
        chunk_no < n_chunks && m_ActiveQueries < kMaxChunksAhead && passed_active_check;
        ++chunk_no
    ) {
        if (!m_ProcessedChunks[chunk_no]) {
            auto& it = m_QueryArr[chunk_no];
            if (!it.query) {
                passed_active_check = CheckMaxActive();
                if (!passed_active_check) {
                    ERR_POST(Trace << "Max active queries level reached while fetching blob chunk");
                }
                // We should not skip sending query if active count is 0.
                // There will not be data ready callback in that case
                // Operation will hang
                if (passed_active_check || m_ActiveQueries < 1) {
                    it.query = ProduceQuery();
                    it.restart_count = 0;
                    x_RequestChunk(*it.query, chunk_no);
                    ++m_ActiveQueries;
                }
            }
        }
    }
}

void CCassBlobTaskLoadBlob::x_RequestChunk(CCassQuery& qry, int32_t chunk_no)
{
    InitBlobChunkDataQuery(&qry, GetKeySpace(), *m_Blob, chunk_no);
    {
        auto DataReadyCb3 = m_DataReadyCb3.lock();
        if (DataReadyCb3) {
            qry.SetOnData3(DataReadyCb3);
        } else if (IsDataReadyCallbackExpired()) {
            char msg[1024];
            string keyspace = GetKeySpace();
            snprintf(msg, sizeof(msg),
                 "Failed to setup data ready callback (expired) for blob chunk (key=%s.%d, chunk=%d)",
                 keyspace.c_str(), GetKey(), chunk_no);
            Error(CRequestStatus::e502_BadGateway, CCassandraException::eUnknown, eDiag_Error, msg);
        }
    }
    qry.Query(GetReadConsistency(), true, m_UsePrepared);
}

unique_ptr<CBlobRecord> CCassBlobTaskLoadBlob::ConsumeBlobRecord()
{
    auto tmp = make_unique<CBlobRecord>(GetKey());
    tmp->SetModified(m_Blob->GetModified());
    tmp->SetNChunks(m_Blob->GetNChunks());
    m_PropsFound = false;
    swap(tmp, m_Blob);
    return tmp;
}

END_IDBLOB_SCOPE
