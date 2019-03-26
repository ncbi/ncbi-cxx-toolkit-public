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
 * Authors: Sergey Satskiy
 *
 * File Description:
 *
 * Cassandra fetch si2csi record
 *
 */

#include <ncbi_pch.hpp>

#include <objtools/pubseq_gateway/impl/cassandra/si2csi_task/fetch.hpp>

#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include <objtools/pubseq_gateway/impl/cassandra/cass_blob_op.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/cass_driver.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/IdCassScope.hpp>

#define CANONICAL_SEQ_ID_CONSISTENCY    CassConsistency::CASS_CONSISTENCY_LOCAL_QUORUM


BEGIN_IDBLOB_SCOPE
USING_NCBI_SCOPE;

CCassSI2CSITaskFetch::CCassSI2CSITaskFetch(
                            unsigned int                      timeout_ms,
                            unsigned int                      max_retries,
                            shared_ptr<CCassConnection>       connection,
                            const string &                    keyspace,
                            const CSI2CSIRecord::TSecSeqId &  sec_seq_id,
                            CSI2CSIRecord::TSecSeqIdType      sec_seq_id_type,
                            bool                              sec_seq_id_type_provided,
                            TSI2CSIConsumeCallback            consume_callback,
                            TDataErrorCallback                data_error_cb) :
    CCassBlobWaiter(timeout_ms, connection, keyspace, 0,
                    true, max_retries, move(data_error_cb)),
    m_SecSeqId(sec_seq_id),
    m_SecSeqIdType(sec_seq_id_type),
    m_SecSeqIdTypeProvided(sec_seq_id_type_provided),
    m_ConsumeCallback(consume_callback),
    m_RecordCount(0),
    m_PageSize(CCassQuery::DEFAULT_PAGE_SIZE),
    m_RestartCounter(0)
{}


void CCassSI2CSITaskFetch::SetConsumeCallback(TSI2CSIConsumeCallback  callback)
{
    m_ConsumeCallback = move(callback);
}


void CCassSI2CSITaskFetch::SetDataReadyCB(TDataReadyCallback  callback,
                                          void *              data)
{
    if (callback && m_State != eInit) {
        NCBI_THROW(CCassandraException, eSeqFailed,
           "CCassSI2CSITaskFetch: DataReadyCB can't be assigned "
           "after the loading process has started");
    }
    CCassBlobWaiter::SetDataReadyCB(callback, data);
}


void CCassSI2CSITaskFetch::SetDataReadyCB(shared_ptr<CCassDataCallbackReceiver>  callback)
{
    if (callback && m_State != eInit) {
        NCBI_THROW(CCassandraException, eSeqFailed,
           "CCassSI2CSITaskFetch: DataReadyCB can't be assigned "
           "after the loading process has started");
    }
    CCassBlobWaiter::SetDataReadyCB3(callback);
}


void CCassSI2CSITaskFetch::Cancel(void)
{
    if (m_State != eDone) {
        m_Cancelled = true;
        CloseAll();
        m_QueryArr.clear();
        m_State = eError;
    }
}


void CCassSI2CSITaskFetch::x_InitializeQuery(void)
{
    static const string     s_Select = "SELECT sec_seq_id_type, accession, "
                                              "id_sync, sec_seq_state, "
                                              "seq_id_type, version FROM ";
    static const string     s_Where_1 = ".SI2CSI WHERE sec_seq_id = ?";
    static const string     s_Where_2 = s_Where_1 + " AND sec_seq_id_type = ?";

    string      sql = s_Select;
    sql.append(GetKeySpace());
    if (m_SecSeqIdTypeProvided) {
        sql.append(s_Where_2);
        m_QueryArr[0].query->SetSQL(sql, 2);
        m_QueryArr[0].query->BindStr(0, m_SecSeqId);
        m_QueryArr[0].query->BindInt16(1, m_SecSeqIdType);
    } else {
        sql.append(s_Where_1);
        m_QueryArr[0].query->SetSQL(sql, 1);
        m_QueryArr[0].query->BindStr(0, m_SecSeqId);
    }
}


void CCassSI2CSITaskFetch::Wait1(void)
{
    bool    restarted;

    do {
        restarted = false;

        switch (m_State) {
            case eError:
            case eDone:
                return;

            case eInit:
                {
                    m_QueryArr.resize(1);
                    m_QueryArr[0] = {m_Conn->NewQuery(), 0};
                    x_InitializeQuery();

                    if (m_DataReadyCb) {
                        m_QueryArr[0].query->SetOnData2(m_DataReadyCb,
                                                        m_DataReadyData);
                    }

                    {
                        auto DataReadyCb3 = m_DataReadyCb3.lock();
                        if (DataReadyCb3) {
                            m_QueryArr[0].query->SetOnData3(DataReadyCb3);
                        }
                    }

                    UpdateLastActivity();
                    m_QueryArr[0].query->Query(CANONICAL_SEQ_ID_CONSISTENCY,
                                               m_Async, true, m_PageSize);
                    m_State = eFetchStarted;
                    break;
                }

            case eFetchStarted:
                {
                    if (CheckReady(m_QueryArr[0].query,
                                   m_RestartCounter, restarted)) {

                        while (m_QueryArr[0].query->NextRow() == ar_dataready) {
                            ++m_RecordCount;
                            if (m_RecordCount == 1) {
                                m_Record.SetSecSeqId(m_SecSeqId)
                                        .SetSecSeqIdType(m_QueryArr[0].query->FieldGetInt16Value(0))
                                        .SetAccession(m_QueryArr[0].query->FieldGetStrValue(1))
                                        .SetIdSync(m_QueryArr[0].query->FieldGetInt64Value(2))
                                        .SetSecSeqState(m_QueryArr[0].query->FieldGetInt8Value(3))
                                        .SetSeqIdType(m_QueryArr[0].query->FieldGetInt16Value(4))
                                        .SetVersion(m_QueryArr[0].query->FieldGetInt16Value(5));
                            }
                        }

                        if (m_QueryArr[0].query->IsEOF()) {
                            if (m_ConsumeCallback) {
                                switch (m_RecordCount) {
                                    case 0:
                                        m_ConsumeCallback(CSI2CSIRecord(),
                                                          CRequestStatus::e404_NotFound);
                                        break;
                                    case 1:
                                        m_ConsumeCallback(move(m_Record),
                                                          CRequestStatus::e200_Ok);
                                        break;
                                    default:
                                        m_ConsumeCallback(CSI2CSIRecord(),
                                                          CRequestStatus::e300_MultipleChoices);
                                }
                            }

                            CloseAll();
                            m_State = eDone;
                        }
                    } else if (restarted) {
                        ++m_RestartCounter;
                        m_QueryArr[0].query->Close();
                        m_State = eInit;
                        m_RecordCount = 0;
                    }
                    UpdateLastActivity();
                    break;
                }

            default: {
                char    msg[1024];
                snprintf(msg, sizeof(msg), "Failed to fetch canonical seq id "
                                           "(key=%s|%s|%d) unexpected state (%d)",
                    m_Keyspace.c_str(), m_SecSeqId.c_str(),
                    static_cast<int>(m_SecSeqIdType), static_cast<int>(m_State));
                Error(CRequestStatus::e502_BadGateway,
                      CCassandraException::eQueryFailed, eDiag_Error, msg);
            }
        }
    } while(restarted);
}

END_IDBLOB_SCOPE
