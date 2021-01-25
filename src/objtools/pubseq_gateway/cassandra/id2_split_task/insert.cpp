/*
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
 * Authors: Evgueni Belyi (based on Dmitrii Saprykin's code)
 *
 * File Description:
 *
 * Cassandra insert named annotation task.
 *
 */

#include <ncbi_pch.hpp>

#include "insert.hpp"

#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include <objtools/pubseq_gateway/impl/cassandra/cass_blob_op.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/cass_driver.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/IdCassScope.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/changelog/writer.hpp>

BEGIN_IDBLOB_SCOPE
USING_NCBI_SCOPE;

CCassID2SplitTaskInsert::CCassID2SplitTaskInsert(
        unsigned int op_timeout_ms,
        shared_ptr<CCassConnection> conn,
        const string & keyspace,
        CBlobRecord * blob,
        CID2SplitRecord* id2_split,
        unsigned int max_retries,
        TDataErrorCallback data_error_cb
)
    : CCassBlobWaiter(
        op_timeout_ms, conn, keyspace, blob->GetKey(), true,
        max_retries, move(data_error_cb)
      )
    , m_Blob(blob)
    , m_Id2Split( id2_split)
{}

void CCassID2SplitTaskInsert::Wait1()
{
    bool b_need_repeat;
    do {
        b_need_repeat = false;
        switch (m_State) {
            case eError:
            case eDone:
                return;

        case eInit:

          {
                m_BlobInsertTask = unique_ptr<CCassBlobTaskInsertExtended>(
                     new CCassBlobTaskInsertExtended(
                         m_OpTimeoutMs, m_Conn, m_Keyspace,
                         m_Blob, true, m_MaxRetries,
                         [this]
                         (CRequestStatus::ECode status, int code, EDiagSev severity,
                          const string & message)
                         {this->m_ErrorCb(status, code, severity, message);}
                     )
                );
                m_State = eWaitingBlobInserted;
                break;
            }

            case eWaitingBlobInserted: {
                if (m_BlobInsertTask->Wait()) {
                    if (m_BlobInsertTask->HasError()) {
                        m_State = eError;
                        m_LastError = m_BlobInsertTask->LastError();
                    } else {
                        m_State = eInsertId2SplitInfo;
                        b_need_repeat = true;
                    }
                    m_BlobInsertTask.reset();
                }
                break;
            }

            case eInsertId2SplitInfo: {
                m_QueryArr.resize(1);
                m_QueryArr[0] = { m_Conn->NewQuery(), 0};
                auto qry = m_QueryArr[0].query;
                string sql = "INSERT INTO " + GetKeySpace() + ".split"
                  " (ent, sat, sat_key, ent_type, split_id)"
                  " VALUES(?, ?, ?, ?, ?)";
                qry->SetSQL( sql, 5);
                qry->BindInt32( 0, m_Id2Split->GetUniqueId());
                qry->BindInt16( 1, m_Id2Split->GetSat());
                qry->BindInt32( 2, m_Id2Split->GetSatKey());
                qry->BindInt32( 3, m_Id2Split->GetEntType());
                qry->BindInt32( 4, m_Id2Split->GetSplitId());

                UpdateLastActivity();
                qry->Execute(CASS_CONSISTENCY_LOCAL_QUORUM, m_Async);

                CBlobChangelogWriter().WriteChangelogEvent(
                    qry.get(),
                    GetKeySpace(),
                    CBlobChangelogRecord(
                        m_Id2Split->GetUniqueId(),
                        m_Blob->GetModified(),
                        TChangelogOperation::eUpdated)
                );

                m_State = eWaitingId2SplitInfoInserted;
                break;
            }

            case eWaitingId2SplitInfoInserted: {
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
                    "Failed to insert named annot (key=%s.%d) unexpected state (%d)",
                    m_Keyspace.c_str(), m_Key, static_cast<int>(m_State));
                Error( CRequestStatus::e502_BadGateway,
                       CCassandraException::eQueryFailed, eDiag_Error, msg);
            }
        }
    } while (b_need_repeat);
}

END_IDBLOB_SCOPE
