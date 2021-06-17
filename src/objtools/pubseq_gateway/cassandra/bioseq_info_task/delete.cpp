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
 * Cassandra blob storage task to delete BioseqInfo record
 *
 */

#include <ncbi_pch.hpp>

#include <objtools/pubseq_gateway/impl/cassandra/bioseq_info_task/delete.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/changelog/bisi_writer.hpp>

#include <memory>
#include <set>
#include <string>
#include <sstream>
#include <tuple>
#include <utility>
#include <vector>

#include <objtools/pubseq_gateway/impl/cassandra/cass_blob_op.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/cass_driver.hpp>

BEGIN_IDBLOB_SCOPE


CCassBioseqInfoTaskDelete::CCassBioseqInfoTaskDelete(
    unsigned int op_timeout_ms,
    shared_ptr<CCassConnection> connection,
    const string & keyspace,
    CBioseqInfoRecord::TAccession accession,
    CBioseqInfoRecord::TVersion version,
    CBioseqInfoRecord::TSeqIdType seq_id_type,
    CBioseqInfoRecord::TGI gi,
    unsigned int max_retries,
    TDataErrorCallback data_error_cb
)
    : CCassBlobWaiter(op_timeout_ms, connection, keyspace, 0, true, max_retries, move(data_error_cb))
    , m_Accession(move(accession))
    , m_Version(version)
    , m_SeqIdType(seq_id_type)
    , m_GI(gi)
{}


void CCassBioseqInfoTaskDelete::Wait1(void)
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
                string sql = "DELETE FROM " + GetKeySpace() + ".bioseq_info WHERE accession = ? AND version = ? AND seq_id_type = ? AND gi = ?";
                qry->NewBatch();

                qry->SetSQL(sql, 4);
                qry->BindStr(0, m_Accession);
                qry->BindInt16(1, m_Version);
                qry->BindInt16(2, m_SeqIdType);
                qry->BindInt64(3, m_GI);
                UpdateLastActivity();
                qry->Execute(CASS_CONSISTENCY_LOCAL_QUORUM, m_Async);

                int64_t partition = CBiSiPartitionMaker().GetCurrentPartition();
                CBiChangelogRecord changelog_record(
                    partition, m_Accession, m_Version, m_SeqIdType, m_GI,
                    TBiSiChangelogOperation::eChangeLogOpErase
                );
                CBiSiChangelogWriter().WriteBiEvent(*qry, GetKeySpace(), changelog_record, CBiSiPartitionMaker().GetTTLSec());
                CBiSiChangelogWriter().WriteBiPartition(*qry, GetKeySpace(), partition, CBiSiPartitionMaker().GetTTLSec());
                SetupQueryCB3(qry);
                qry->RunBatch();
                m_State = eWaitingPropsDeleted;
                break;
            }

            case eWaitingPropsDeleted: {
                if (!CheckReady(m_QueryArr[0])) {
                    break;
                }
                CloseAll();
                m_State = eDone;
                break;
            }

            default: {
                stringstream msg;
                msg << "Failed to delete bioseq_info record (key=" << m_Keyspace << "." << m_Accession << "."
                    << m_Version << "." << m_SeqIdType << "." << m_GI << ") unexpected state (" << static_cast<int>(m_State) << ")";
                Error(CRequestStatus::e502_BadGateway, CCassandraException::eQueryFailed, eDiag_Error, msg.str());
            }
        }
    } while(restarted);
}

END_IDBLOB_SCOPE
