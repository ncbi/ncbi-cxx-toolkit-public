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
 * Cassandra blob storage task to update Si2Csi record
 *
 */

#include <ncbi_pch.hpp>

#include <objtools/pubseq_gateway/impl/cassandra/si2csi_task/update.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/changelog/bisi_writer.hpp>

#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include <objtools/pubseq_gateway/impl/cassandra/cass_blob_op.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/cass_driver.hpp>

BEGIN_IDBLOB_SCOPE

CCassSI2CSITaskUpdate::CCassSI2CSITaskUpdate(
    unsigned int op_timeout_ms,
    shared_ptr<CCassConnection> connection,
    const string & keyspace,
    CSI2CSIRecord * record,
    unsigned int max_retries,
    TDataErrorCallback data_error_cb
)
    : CCassBlobWaiter(op_timeout_ms, connection, keyspace, 0, true, max_retries, move(data_error_cb))
    , m_Record(record)
    , m_UseWritetime(false)
{}

void CCassSI2CSITaskUpdate::Wait1(void)
{
    bool restarted;
    do {
        restarted = false;
        switch (m_State) {
            case eError:
            case eDone:
                return;

            case eInit: {
                m_QueryArr.resize(1);
                m_QueryArr[0].query = m_Conn->NewQuery();
                m_QueryArr[0].restart_count = 0;
                auto qry = m_QueryArr[0].query;
                string sql = "INSERT INTO " + GetKeySpace() + ".si2csi "
                    "("
                    " sec_seq_id, sec_seq_id_type,"
                    " accession, gi, sec_seq_state, seq_id_type, version"
                    ")"
                    "VALUES(?, ?, ?, ?, ?, ?, ?)";
                if (m_UseWritetime && m_Record->GetWritetime() > 0) {
                    sql += " USING TIMESTAMP " + to_string(m_Record->GetWritetime());
                }
                qry->NewBatch();
                qry->SetSQL(sql, 7);
                qry->BindStr(0, m_Record->GetSecSeqId());
                qry->BindInt16(1, m_Record->GetSecSeqIdType());
                qry->BindStr(2, m_Record->GetAccession());
                qry->BindInt64(3, m_Record->GetGI());
                qry->BindInt8(4, m_Record->GetSecSeqState());
                qry->BindInt16(5, m_Record->GetSeqIdType());
                qry->BindInt16(6, m_Record->GetVersion());
                UpdateLastActivity();
                qry->Execute(CASS_CONSISTENCY_LOCAL_QUORUM, m_Async);

                int64_t partition = CBiSiPartitionMaker().GetCurrentPartition();
                CSiChangelogRecord changelog_record(
                    partition,
                    m_Record->GetSecSeqId(), m_Record->GetSecSeqIdType(),
                    TBiSiChangelogOperation::eChangeLogOpInsert
                );
                CBiSiChangelogWriter().WriteSiEvent(*qry, GetKeySpace(), changelog_record, CBiSiPartitionMaker().GetTTLSec());
                CBiSiChangelogWriter().WriteSiPartition(*qry, GetKeySpace(), partition, CBiSiPartitionMaker().GetTTLSec());
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
                stringstream msg;
                msg << "Failed to update si2csi record (key=" << m_Keyspace << "." << m_Record->GetSecSeqId() << "."
                    << m_Record->GetSecSeqIdType() << ") unexpected state (" << static_cast<int>(m_State) << ")";
                Error(CRequestStatus::e502_BadGateway, CCassandraException::eQueryFailed, eDiag_Error, msg.str());
            }
        }
    } while(restarted);
}

END_IDBLOB_SCOPE
