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
 * Cassandra insert blob according to extended schema task.
 *
 */

#include <ncbi_pch.hpp>

#include <memory>
#include <string>
#include <utility>

#include <objtools/pubseq_gateway/impl/cassandra/blob_task/insert_extended.hpp>

#include <objtools/pubseq_gateway/impl/cassandra/cass_blob_op.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/cass_driver.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/IdCassScope.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/changelog/writer.hpp>

BEGIN_IDBLOB_SCOPE
USING_NCBI_SCOPE;

static int32_t ExtractNth(string const& str, char separator, size_t n)
{
    const char * current = str.c_str();
    while (*current && n) {
        n -= (*current++ == separator);
    }
    if (n == 0 && *current && *current >= '0' && *current <= '9') {
        return atoi(current);
    }
    return 0;
}

CCassBlobTaskInsertExtended::CCassBlobTaskInsertExtended(
    unsigned int op_timeout_ms, shared_ptr<CCassConnection>  conn,
    const string & keyspace, CBlobRecord * blob,
    bool async, unsigned int max_retries,
    TDataErrorCallback data_error_cb
)
    : CCassBlobWaiter(
        op_timeout_ms, conn, keyspace, blob->GetKey(), async,
        max_retries, move(data_error_cb)
    )
    , m_Blob(blob)
{}

void CCassBlobTaskInsertExtended::Wait1()
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
                m_QueryArr.reserve(m_Blob->GetNChunks());
                m_State = eInsertChunks;
                b_need_repeat = true;
                break;
            }

            case eInsertChunks: {
                string sql;
                while (static_cast<size_t>(m_Blob->GetNChunks()) > m_QueryArr.size()) {
                    m_QueryArr.push_back({m_Conn->NewQuery(), 0});
                }
                for (int32_t i = 0; i < m_Blob->GetNChunks(); ++i) {
                    auto qry = m_QueryArr[i].query;
                    if (!qry->IsActive()) {
                        sql = "INSERT INTO " + GetKeySpace() + ".blob_chunk "
                              "(sat_key, last_modified, chunk_no, data) VALUES(?, ?, ?, ?)";
                        qry->SetSQL(sql, 4);
                        qry->BindInt32(0, m_Blob->GetKey());
                        qry->BindInt64(1, m_Blob->GetModified());
                        qry->BindInt32(2, i);
                        const CBlobRecord::TBlobChunk& chunk = m_Blob->GetChunk(i);
                        qry->BindBytes(3, chunk.data(), chunk.size());
                        UpdateLastActivity();
                        SetupQueryCB3(qry);
                        qry->Execute(CASS_CONSISTENCY_LOCAL_QUORUM, m_Async);
                    }
                }
                m_State = eWaitingChunksInserted;
                break;
            }
            case eWaitingChunksInserted: {
                bool anyrunning = false;
                int i = 0;
                for (auto& it : m_QueryArr) {
                    if (it.query->IsActive()) {
                        if (!CheckReady(it)) {
                            anyrunning = true;
                            break;
                        }
                    }
                    ++i;
                }
                if (!anyrunning) {
                    UpdateLastActivity();
                    CloseAll();
                    m_State = eInsertProps;
                    b_need_repeat = true;
                }
                break;
            }

            case eInsertProps: {
                m_QueryArr.resize(1);
                m_QueryArr[0].query = m_Conn->NewQuery();
                m_QueryArr[0].restart_count = 0;
                auto qry = m_QueryArr[0].query;
                string sql = "INSERT INTO " + GetKeySpace() + ".blob_prop "
                    "("
                    " sat_key,"
                    " last_modified,"
                    " class,"
                    " date_asn1,"
                    " div,"
                    " flags,"
                    " hup_date,"
                    " id2_info,"
                    " n_chunks,"
                    " owner,"
                    " size,"
                    " size_unpacked,"
                    " username"
                    ")"
                    "VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";
                qry->NewBatch();

                int32_t split_version = ExtractNth(m_Blob->GetId2Info(), '.', 3);
                if (split_version > 0) {
                    string history_sql = "INSERT INTO " + GetKeySpace() + ".blob_split_history "
                        "(sat_key, split_version, last_modified, id2_info) VALUES(?, ?, ?, ?)";
                    qry->SetSQL(history_sql, 4);
                    qry->BindInt32(0, m_Blob->GetKey());
                    qry->BindInt32(1, split_version);
                    qry->BindInt64(2, m_Blob->GetModified());
                    qry->BindStr(3, m_Blob->GetId2Info());
                    UpdateLastActivity();
                    qry->Execute(CASS_CONSISTENCY_LOCAL_QUORUM, m_Async);
                }

                qry->SetSQL(sql, 13);
                qry->BindInt32(0, m_Blob->GetKey());
                qry->BindInt64(1, m_Blob->GetModified());
                qry->BindInt16(2, m_Blob->GetClass());
                qry->BindInt64(3, m_Blob->GetDateAsn1());
                qry->BindStr(4, m_Blob->GetDiv());
                qry->BindInt64(5, m_Blob->GetFlags());
                qry->BindInt64(6, m_Blob->GetHupDate());
                qry->BindStr(7, m_Blob->GetId2Info());
                qry->BindInt32(8, m_Blob->GetNChunks());
                qry->BindInt32(9, m_Blob->GetOwner());
                qry->BindInt64(10, m_Blob->GetSize());
                qry->BindInt64(11, m_Blob->GetSizeUnpacked());
                qry->BindStr(12, m_Blob->GetUserName());
                UpdateLastActivity();
                qry->Execute(CASS_CONSISTENCY_LOCAL_QUORUM, m_Async);

                CBlobChangelogWriter().WriteChangelogEvent(
                    qry.get(),
                    GetKeySpace(),
                    CBlobChangelogRecord(m_Blob->GetKey(), m_Blob->GetModified(), TChangelogOperation::eUpdated)
                );
                SetupQueryCB3(qry);
                qry->RunBatch();
                m_State = eWaitingPropsInserted;
                break;
            }

            case eWaitingPropsInserted: {
                if (!CheckReady(m_QueryArr[0])) {
                    break;
                }
                CloseAll();
                m_State = eDone;
                break;
            }

            default: {
                char msg[1024];
                snprintf(msg, sizeof(msg),
                    "Failed to insert extended blob (key=%s.%d) unexpected state (%d)",
                    m_Keyspace.c_str(), m_Key, static_cast<int>(m_State));
                Error(CRequestStatus::e502_BadGateway,
                    CCassandraException::eQueryFailed,
                    eDiag_Error, msg);
            }
        }
    } while (b_need_repeat);
}

END_IDBLOB_SCOPE
