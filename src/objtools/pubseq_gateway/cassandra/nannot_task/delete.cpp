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
 * Cassandra detele named annotation task.
 *
 */

#include <ncbi_pch.hpp>

#include "delete.hpp"

#include <memory>
#include <string>
#include <utility>

#include <objtools/pubseq_gateway/impl/cassandra/cass_blob_op.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/cass_driver.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/IdCassScope.hpp>

BEGIN_IDBLOB_SCOPE
USING_NCBI_SCOPE;

CCassNAnnotTaskDelete::CCassNAnnotTaskDelete(
        unsigned int op_timeout_ms,
        shared_ptr<CCassConnection> conn,
        const string & keyspace,
        CNAnnotRecord * annot,
        unsigned int max_retries,
        TDataErrorCallback data_error_cb
)
    : CCassBlobWaiter(
        op_timeout_ms, conn, keyspace, annot->GetSatKey(), true,
        max_retries, move(data_error_cb)
      )
    , m_Annot(annot)
{}

void CCassNAnnotTaskDelete::Wait1()
{
    bool b_need_repeat;
    do {
        b_need_repeat = false;
        switch (m_State) {
            case eError:
            case eDone:
                return;

            case eInit: {
                m_QueryArr.resize(1);
                m_QueryArr[0] = { m_Conn->NewQuery(), 0};
                auto query = m_QueryArr[0].query;
                string sql = "DELETE FROM " + GetKeySpace() + ".bioseq_na "
                    "WHERE accession = ? AND version = ? AND seq_id_type = ? AND annot_name = ? "
                    "IF sat_key = ? AND modified = ?";
                query->SetSQL(sql, 6);
                query->SetSerialConsistency(CASS_CONSISTENCY_LOCAL_SERIAL);
                query->BindStr(0, m_Annot->GetAccession());
                query->BindInt16(1, m_Annot->GetVersion());
                query->BindInt16(2, m_Annot->GetSeqIdType());
                query->BindStr(3, m_Annot->GetAnnotName());
                query->BindInt32(4, m_Annot->GetSatKey());
                query->BindInt64(5, m_Annot->GetModified());

                UpdateLastActivity();
                query->Query(CASS_CONSISTENCY_LOCAL_QUORUM, m_Async, false);
                m_State = eWaitingBioseqNADeleted;
                break;
            }

            case eWaitingBioseqNADeleted: {
                auto query = m_QueryArr[0].query;
                if (!CheckReady(m_QueryArr[0])) {
                    break;
                }
                query->NextRow();
                bool applied = query->FieldGetBoolValue(0);
                m_State = applied ? eDeleteBlobRecord : eDone;
                b_need_repeat = true;
                break;
            }

            case eDeleteBlobRecord: {
                m_BlobDeleteTask = unique_ptr<CCassBlobTaskDelete>(
                    new CCassBlobTaskDelete(
                         m_OpTimeoutMs, m_Conn, m_Keyspace,
                         true, m_Annot->GetSatKey(), true,  m_MaxRetries,
                         [this]
                         (CRequestStatus::ECode status, int code, EDiagSev severity, const string & message)
                         {this->m_ErrorCb(status, code, severity, message);}
                    )
                );
                break;
            }

            case eWaitingDeleteBlobRecord:
                if (m_BlobDeleteTask->Wait()) {
                    if (m_BlobDeleteTask->HasError()) {
                        m_State = eError;
                        m_LastError = m_BlobDeleteTask->LastError();
                    } else {
                        CloseAll();
                        m_State = eDone;
                    }
                    m_BlobDeleteTask.reset();
                }
                break;

            default: {
                char msg[1024];
                snprintf(msg, sizeof(msg), "Failed to insert named annot (key=%s.%d) unexpected state (%d)",
                    m_Keyspace.c_str(), m_Key, static_cast<int>(m_State));
                Error(CRequestStatus::e502_BadGateway, CCassandraException::eQueryFailed, eDiag_Error, msg);
            }
        }
    } while (b_need_repeat);
}

END_IDBLOB_SCOPE
