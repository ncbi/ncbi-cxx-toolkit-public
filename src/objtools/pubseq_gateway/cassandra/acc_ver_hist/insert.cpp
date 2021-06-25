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
 * Authors: Evgueni Belyi based on Dmitrii Saprykin's original code
 *
 * File Description:
 *
 * Cassandra insert accession/version history record task.
 *
 */

#include <ncbi_pch.hpp>

#include <memory>
#include <string>
#include <utility>

#include <objtools/pubseq_gateway/impl/cassandra/acc_ver_hist/tasks.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/cass_driver.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/changelog/writer.hpp>

BEGIN_IDBLOB_SCOPE
USING_NCBI_SCOPE;

CCassAccVerHistoryTaskInsert::CCassAccVerHistoryTaskInsert(
    unsigned int op_timeout_ms,
    shared_ptr<CCassConnection> conn,
    const string & keyspace,
    SAccVerHistRec * record,
    unsigned int max_retries,
    TDataErrorCallback data_error_cb
)
    : CCassBlobWaiter(
        op_timeout_ms, conn, keyspace, 0, //@@@@  record->GetSatKey(),
        true, max_retries, move(data_error_cb)
      )
    , m_Record(record)
{}

void CCassAccVerHistoryTaskInsert::Wait1()
{
    bool b_need_repeat;
    do
    {
        b_need_repeat = false;
        switch (m_State)
        {
        case eError:
        case eDone:
            return;

        case eInit:
        {
            m_QueryArr.resize(1);
            m_QueryArr[0] = { m_Conn->NewQuery(), 0};
            auto query = m_QueryArr[0].query;
            string sql = "INSERT INTO " + GetKeySpace() +
                ".acc_ver_hist"
                " (accession, version, gi, chain, sat, sat_key, id_type, date)"
                " VALUES (?,?,?,?,?,?,?,?)";

            query->NewBatch();
            query->SetSQL( sql, 8);
            query->BindStr(   0, m_Record->accession);
            query->BindInt16( 1, m_Record->version);
            query->BindInt64( 2, m_Record->gi);
            query->BindInt64( 3, m_Record->chain);
            query->BindInt16( 4, m_Record->sat);
            query->BindInt32( 5, m_Record->sat_key);
            query->BindInt16( 6, m_Record->seq_id_type);
            query->BindInt64( 7, m_Record->date);
               
            UpdateLastActivity();
            query->Execute( CASS_CONSISTENCY_LOCAL_QUORUM, m_Async);
#if 000
            @@@
            CBlobChangelogWriter().WriteChangelogEvent(
                query.get(),
                GetKeySpace(),
                CBlobChangelogRecord(m_Record->GetSatKey(), m_Record->GetDoneWhen(), TChangelogOperation::eStatusHistoryUpdated)
            );
#endif
            SetupQueryCB3(query);
            query->RunBatch();
            m_State = eWaitingRecordInserted;
            break;
        }

        case eWaitingRecordInserted:
        {
            if (!CheckReady(m_QueryArr[0]))
            {
                break;
            }
            CloseAll();
            m_State = eDone;
            break;
        }

        default:
        {
            char msg[1024];
            snprintf(msg, sizeof(msg), "Failed to insert blob status history record (key=%s.%ld) unexpected state (%d)",
                     m_Keyspace.c_str(), m_Record->chain,
                     static_cast<int>(m_State));
            Error(CRequestStatus::e502_BadGateway,
                  CCassandraException::eQueryFailed, eDiag_Error, msg);
        }
        }
    } while (b_need_repeat);
}

END_IDBLOB_SCOPE
