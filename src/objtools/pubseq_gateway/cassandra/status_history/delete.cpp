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
 * Cassandra insert status history record task.
 *
 */

#include <ncbi_pch.hpp>

#include <objtools/pubseq_gateway/impl/cassandra/status_history/delete.hpp>

#include <memory>
#include <string>
#include <utility>

#include <objtools/pubseq_gateway/impl/cassandra/cass_blob_op.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/cass_driver.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/IdCassScope.hpp>

BEGIN_IDBLOB_SCOPE
USING_NCBI_SCOPE;

CCassStatusHistoryTaskDelete::CCassStatusHistoryTaskDelete(
    unsigned int op_timeout_ms,
    shared_ptr<CCassConnection> conn,
    const string & keyspace,
    int32_t sat_key,
    int64_t done_when,
    unsigned int max_retries,
    TDataErrorCallback data_error_cb
)
    : CCassBlobWaiter(
        op_timeout_ms, conn, keyspace, sat_key,
        true, max_retries, move(data_error_cb)
      )
    , m_DoneWhen(done_when)
{}

void CCassStatusHistoryTaskDelete::Wait1()
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
                auto qry = m_QueryArr[0].query;
                string sql = "DELETE FROM " + GetKeySpace() + ".blob_status_history "
                      "WHERE sat_key = ? AND done_when = ?";
                qry->SetSQL(sql, 2);
                qry->BindInt32(0, m_Key);
                qry->BindInt64(1, m_DoneWhen);

                UpdateLastActivity();
                qry->Execute(CASS_CONSISTENCY_LOCAL_QUORUM, m_Async);

                SetupQueryCB3(qry);

                m_State = eWaitingDeleteRecord;
                break;
            }

            case eWaitingDeleteRecord: {
                if (!CheckReady(m_QueryArr[0])) {
                    break;
                }
                CloseAll();
                m_State = eDone;
                break;
            }

            default: {
                char msg[1024];
                snprintf(msg, sizeof(msg), "Failed to delete blob status history record (key=%s.%d.%ld) unexpected state (%d)",
                    m_Keyspace.c_str(), m_Key, m_DoneWhen, static_cast<int>(m_State));
                Error(CRequestStatus::e502_BadGateway, CCassandraException::eQueryFailed, eDiag_Error, msg);
            }
        }
    } while (b_need_repeat);
}

END_IDBLOB_SCOPE
