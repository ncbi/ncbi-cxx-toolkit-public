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

#include <objtools/pubseq_gateway/impl/cassandra/blob_task/delete_expired.hpp>

#include <memory>
#include <string>
#include <utility>

#include <inttypes.h>

#include <objtools/pubseq_gateway/impl/cassandra/blob_storage.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/cass_blob_op.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/cass_driver.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/IdCassScope.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/changelog/writer.hpp>

BEGIN_IDBLOB_SCOPE
USING_NCBI_SCOPE;

CCassBlobTaskDeleteExpired::CCassBlobTaskDeleteExpired(
    shared_ptr<CCassConnection> conn,
    const string & keyspace,
    int32_t key,
    CBlobRecord::TTimestamp last_modified,
    CBlobRecord::TTimestamp expiration,
    TDataErrorCallback error_cb
)
    : CCassBlobWaiter(move(conn), keyspace, key, true, move(error_cb))
    , m_LastModified(last_modified)
    , m_Expiration(expiration)
{}

bool CCassBlobTaskDeleteExpired::IsExpiredVersionDeleted() const
{
    return m_ExpiredVersionDeleted;
}

void CCassBlobTaskDeleteExpired::Wait1()
{
    bool b_need_repeat;
    do {
        b_need_repeat = false;
        switch (m_State) {
            case eError:
            case eDone:
                return;

            case eInit: {
                m_ExpiredVersionDeleted = false;
                CloseAll();
                m_QueryArr.clear();
                m_QueryArr.push_back({m_Conn->NewQuery(), 0});
                string sql = "SELECT flags, n_chunks, writetime(n_chunks) FROM " + GetKeySpace()
                    + ".blob_prop WHERE sat_key = ? and last_modified = ?";
                auto qry = m_QueryArr[0].query;
                qry->SetSQL(sql, 2);
                qry->BindInt32(0, GetKey());
                qry->BindInt64(1, m_LastModified);
                SetupQueryCB3(qry);
                qry->Query(CASS_CONSISTENCY_LOCAL_QUORUM, m_Async);
                m_State = eReadingBlobProps;
                break;
            }

            case eReadingBlobProps: {
                string sql;
                auto& it = m_QueryArr[0];
                if (!CheckReady(it)) {
                    break;
                }
                it.query->NextRow();
                if (!it.query->IsEOF()) {
                    m_BlobFlags = it.query->FieldGetInt64Value(0);
                    m_BlobNChunks = it.query->FieldGetInt32Value(1);
                    CBlobRecord::TTimestamp write_timestamp = it.query->FieldGetInt64Value(2);
                    CloseAll();
                    CTime expiration(CTime::eEmpty, CTime::eUTC);
                    expiration.SetTimeT(write_timestamp / 1000000);
                    expiration.SetMicroSecond(write_timestamp % 1000000);
                    expiration.AddSecond(m_Expiration);
                    if (CTime(CTime::eCurrent, CTime::eUTC) > expiration) {
                        m_State = eDeleteData;
                        b_need_repeat = true;
                    } else {
                        m_State = eDone;
                    }
                } else {
                    CloseAll();
                    ERR_POST(Warning << "BlobDeleteExpired key=" << GetKeySpace() << "." << GetKey()
                             << ", last_modified = " << m_LastModified << " is not found"
                    );
                    m_State = eDone;
                }
                break;
            }

            case eDeleteData: {
                auto& qry = m_QueryArr[0].query;
                qry->NewBatch();
                string sql = "DELETE FROM " + GetKeySpace() + "." + SBlobStorageConstants::kChunkTableDefault
                    + " WHERE sat_key = ? and last_modified = ?";
                qry->SetSQL(sql, 2);
                qry->BindInt32(0, GetKey());
                qry->BindInt64(1, m_LastModified);
                qry->Execute(CASS_CONSISTENCY_LOCAL_QUORUM, m_Async);

                // Need to delete data from big_blob_chunk table if blob marked
                // Delete from ".blob_chunk" is executed in any case (it is cheap - one range tombstone)
                if (m_BlobFlags & static_cast<TBlobFlagBase>(EBlobFlags::eBigBlobSchema)) {
                    sql = "DELETE FROM " + GetKeySpace() + "." + SBlobStorageConstants::kChunkTableBig
                        + " WHERE sat_key = ? and last_modified = ? and chunk_no = ?";
                    for (int32_t i = 0; i < m_BlobNChunks; ++i) {
                        qry->SetSQL(sql, 3);
                        qry->BindInt32(0, GetKey());
                        qry->BindInt64(1, m_LastModified);
                        qry->BindInt32(2, i);
                        qry->Execute(CASS_CONSISTENCY_LOCAL_QUORUM, m_Async);
                    }
                }

                sql = "DELETE FROM " + GetKeySpace() + ".blob_prop WHERE sat_key = ? and last_modified = ?";
                qry->SetSQL(sql, 2);
                qry->BindInt32(0, GetKey());
                qry->BindInt64(1, m_LastModified);
                qry->Execute(CASS_CONSISTENCY_LOCAL_QUORUM, m_Async);

                CBlobChangelogWriter().WriteChangelogEvent(
                    qry.get(),
                    GetKeySpace(),
                    CBlobChangelogRecord(GetKey(), m_LastModified, TChangelogOperation::eDeleted)
                );

                SetupQueryCB3(qry);
                qry->RunBatch();
                m_State = eWaitDeleteData;
                break;
            }

            case eWaitDeleteData: {
                if (!CheckReady(m_QueryArr[0])) {
                    break;
                }
                m_ExpiredVersionDeleted = true;
                CloseAll();
                m_State = eDone;
                break;
            }

            default: {
                char msg[1024];
                string keyspace = GetKeySpace();
                snprintf(msg, sizeof(msg),
                         "Failed to delete expired version of blob (key=%s.%d, v= %" PRId64 ") unexpected state (%d)",
                         keyspace.c_str(), GetKey(), m_LastModified, static_cast<int>(m_State));
                Error(CRequestStatus::e502_BadGateway, CCassandraException::eQueryFailed, eDiag_Error, msg);
            }
        }
    } while (b_need_repeat);
}

END_IDBLOB_SCOPE
