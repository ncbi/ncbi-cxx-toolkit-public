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

#include <objtools/pubseq_gateway/impl/cassandra/blob_task/delete.hpp>

#include <memory>
#include <string>
#include <utility>

#include <objtools/pubseq_gateway/impl/cassandra/blob_storage.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/cass_blob_op.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/cass_driver.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/IdCassScope.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/changelog/writer.hpp>

BEGIN_IDBLOB_SCOPE
USING_NCBI_SCOPE;

CCassBlobTaskDelete::CCassBlobTaskDelete(
        shared_ptr<CCassConnection> conn,
        const string & keyspace,
        int32_t key,
        bool async,
        TDataErrorCallback error_cb
)
    : CCassBlobWaiter(conn, keyspace, key, async, move(error_cb))
{}

void CCassBlobTaskDelete::Wait1()
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
                m_QueryArr.push_back({m_Conn->NewQuery(), 0});
                auto qry = m_QueryArr[0].query;
                string sql = "SELECT flags, n_chunks, last_modified FROM " + GetKeySpace() + ".blob_prop WHERE sat_key = ?";
                qry->SetSQL(sql, 1);
                qry->BindInt32(0, GetKey());
                SetupQueryCB3(qry);
                qry->Query(CASS_CONSISTENCY_LOCAL_QUORUM, m_Async);
                m_State = eReadingVersions;
                break;
            }

            case eReadingVersions: {
                string sql;
                auto& it = m_QueryArr[0];
                if (!CheckReady(it)) {
                    break;
                }
                async_rslt_t wr = static_cast<async_rslt_t>(-1);
                if (!it.query->IsEOF()) {
                    while ((wr = it.query->NextRow()) == ar_dataready) {
                        SBlobVersionInfo version_info;
                        version_info.flags = it.query->FieldGetInt64Value(0);
                        version_info.n_chunks = it.query->FieldGetInt32Value(1);
                        version_info.last_modified = it.query->FieldGetInt64Value(2);
                        m_ExtendedVersions.push_back(version_info);
                    }
                    if (wr == ar_wait) {
                        break;
                    }
                }

                CloseAll();
                if (m_ExtendedVersions.empty()) {
                    RAISE_DB_ERROR(eInconsistentData, "Blob versions not found. key: "
                        + GetKeySpace() + "." + to_string(GetKey())
                    );
                }
                m_State = eDeleteData;
                b_need_repeat = true;
                break;
            }

            case eDeleteData: {
                auto qry = m_QueryArr[0].query;
                qry->NewBatch();
                CBlobChangelogWriter changelog;
                for (auto version : m_ExtendedVersions) {
                    string sql = "DELETE FROM " + GetKeySpace() + "." + SBlobStorageConstants::kChunkTableDefault
                        + " WHERE sat_key = ? and last_modified = ?";
                    qry->SetSQL(sql, 2);
                    qry->BindInt32(0, GetKey());
                    qry->BindInt64(1, version.last_modified);
                    qry->Execute(CASS_CONSISTENCY_LOCAL_QUORUM, m_Async);

                    // Need to delete data from big_blob_chunk table if blob marked
                    // Delete from ".blob_chunk" is executed in any case (it is cheap - one range tombstone)
                    if (version.flags & static_cast<TBlobFlagBase>(EBlobFlags::eBigBlobSchema)) {
                        sql = "DELETE FROM " + GetKeySpace() + "." + SBlobStorageConstants::kChunkTableBig
                            + " WHERE sat_key = ? and last_modified = ? and chunk_no = ?";
                        for (int32_t i = 0; i < version.n_chunks; ++i) {
                            qry->SetSQL(sql, 3);
                            qry->BindInt32(0, GetKey());
                            qry->BindInt64(1, version.last_modified);
                            qry->BindInt32(2, i);
                            qry->Execute(CASS_CONSISTENCY_LOCAL_QUORUM, m_Async);
                        }
                    }

                    changelog.WriteChangelogEvent(
                        qry.get(),
                        GetKeySpace(),
                        CBlobChangelogRecord(GetKey(), version.last_modified, TChangelogOperation::eDeleted)
                    );
                }
                string sql = "DELETE FROM " + GetKeySpace() + ".blob_prop WHERE sat_key = ?";
                qry->SetSQL(sql, 1);
                qry->BindInt32(0, GetKey());
                qry->Execute(CASS_CONSISTENCY_LOCAL_QUORUM, m_Async);
                SetupQueryCB3(qry);
                qry->RunBatch();
                m_State = eWaitDeleteData;
                break;
            }

            case eWaitDeleteData: {
                if (!CheckReady(m_QueryArr[0])) {
                    break;
                }
                CloseAll();
                m_State = eDone;
                break;
            }

            default: {
                char msg[1024];
                string keyspace = GetKeySpace();
                snprintf(msg, sizeof(msg),
                         "Failed to delete blob (key=%s.%d) unexpected state (%d)",
                         keyspace.c_str(), GetKey(), static_cast<int>(m_State));
                Error(CRequestStatus::e502_BadGateway, CCassandraException::eQueryFailed, eDiag_Error, msg);
            }
        }
    } while (b_need_repeat);
}

END_IDBLOB_SCOPE
