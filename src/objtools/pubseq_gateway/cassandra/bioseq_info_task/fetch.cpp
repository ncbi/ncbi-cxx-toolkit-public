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

#include <objtools/pubseq_gateway/impl/cassandra/bioseq_info_task/fetch.hpp>

#include <memory>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include <objtools/pubseq_gateway/impl/cassandra/cass_blob_op.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/cass_driver.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/IdCassScope.hpp>

BEGIN_IDBLOB_SCOPE
USING_NCBI_SCOPE;

BEGIN_SCOPE()
    const CassConsistency kBioSeqInfoConsistency = CassConsistency::CASS_CONSISTENCY_LOCAL_QUORUM;

    // Select field numbers; must be in sync with the select statements
    const int fnVersion = 0;
    const int fnSeqIdType = 1;
    const int fnDateChanged = 2;
    const int fnHash = 3;
    const int fnGI = 4;
    const int fnLength = 5;
    const int fnMol = 6;
    const int fnSat = 7;
    const int fnSatKey = 8;
    const int fnSeqIds = 9;
    const int fnSeqState = 10;
    const int fnState = 11;
    const int fnTaxId = 12;

    const string kSelectStatement = "SELECT version, seq_id_type, "
        "date_changed, hash, gi, length, mol, sat, "
        "sat_key, seq_ids, seq_state, state, tax_id FROM ";
END_SCOPE()

CCassBioseqInfoTaskFetch::CCassBioseqInfoTaskFetch(
                            unsigned int                           timeout_ms,
                            unsigned int                           max_retries,
                            shared_ptr<CCassConnection>            connection,
                            const string &                         keyspace,
                            const CBioseqInfoRecord::TAccession &  accession,
                            CBioseqInfoRecord::TVersion            version,
                            bool                                   version_provided,
                            CBioseqInfoRecord::TSeqIdType          seq_id_type,
                            bool                                   seq_id_type_provided,
                            CBioseqInfoRecord::TGI                 gi,
                            bool                                   gi_provided,
                            TBioseqInfoConsumeCallback             consume_callback,
                            TDataErrorCallback                     data_error_cb) :
    CCassBlobWaiter(timeout_ms, connection, keyspace, 0,
                    true, max_retries, move(data_error_cb)),
    m_Accession(accession),
    m_Version(version),
    m_VersionProvided(version_provided),
    m_SeqIdType(seq_id_type),
    m_SeqIdTypeProvided(seq_id_type_provided),
    m_GI(gi),
    m_GIProvided(gi_provided),
    m_ConsumeCallback(move(consume_callback)),
    m_RecordCount(0),
    m_PageSize(CCassQuery::DEFAULT_PAGE_SIZE),
    m_RestartCounter(0)
{}


void CCassBioseqInfoTaskFetch::SetConsumeCallback(TBioseqInfoConsumeCallback  callback)
{
    m_ConsumeCallback = move(callback);
}


void CCassBioseqInfoTaskFetch::SetDataReadyCB(TDataReadyCallback  callback,
                                              void *              data)
{
    if (callback && m_State != eInit) {
        NCBI_THROW(CCassandraException, eSeqFailed,
           "CCassBioseqInfoTaskFetch: DataReadyCB can't be assigned "
           "after the loading process has started");
    }
    CCassBlobWaiter::SetDataReadyCB(callback, data);
}


void CCassBioseqInfoTaskFetch::SetDataReadyCB(shared_ptr<CCassDataCallbackReceiver>  callback)
{
    if (callback && m_State != eInit) {
        NCBI_THROW(CCassandraException, eSeqFailed,
           "CCassBioseqInfoTaskFetch: DataReadyCB can't be assigned "
           "after the loading process has started");
    }
    CCassBlobWaiter::SetDataReadyCB3(callback);
}


void CCassBioseqInfoTaskFetch::Cancel(void)
{
    if (m_State != eDone) {
        m_Cancelled = true;
        CloseAll();
        m_QueryArr.clear();
        m_State = eError;
    }
}


void CCassBioseqInfoTaskFetch::x_InitializeQuery(void)
{
    static const string s_Where_1 = ".bioseq_info WHERE accession = ?";
    static const string s_Where_2 = s_Where_1 + " AND version = ?";
    static const string s_Where_3 = s_Where_2 + " AND seq_id_type = ?";
    static const string s_Where_4 = s_Where_3 + " AND gi = ?";

    string sql = kSelectStatement;
    sql.append(GetKeySpace());
    if (m_VersionProvided && m_SeqIdTypeProvided && m_GIProvided) {
        sql.append(s_Where_4);
        m_QueryArr[0].query->SetSQL(sql, 4);
        m_QueryArr[0].query->BindStr(0, m_Accession);
        m_QueryArr[0].query->BindInt16(1, m_Version);
        m_QueryArr[0].query->BindInt16(2, m_SeqIdType);
        m_QueryArr[0].query->BindInt64(3, m_GI);
    } else if (m_VersionProvided && m_SeqIdTypeProvided) {
        sql.append(s_Where_3);
        m_QueryArr[0].query->SetSQL(sql, 3);
        m_QueryArr[0].query->BindStr(0, m_Accession);
        m_QueryArr[0].query->BindInt16(1, m_Version);
        m_QueryArr[0].query->BindInt16(2, m_SeqIdType);
    } else if (m_VersionProvided) {
        sql.append(s_Where_2);
        m_QueryArr[0].query->SetSQL(sql, 2);
        m_QueryArr[0].query->BindStr(0, m_Accession);
        m_QueryArr[0].query->BindInt16(1, m_Version);
    } else {
        sql.append(s_Where_1);
        m_QueryArr[0].query->SetSQL(sql, 1);
        m_QueryArr[0].query->BindStr(0, m_Accession);
    }
}

void CCassBioseqInfoTaskFetch::x_InitializeAliveRecordQuery(void)
{
    string sql = kSelectStatement + GetKeySpace() + ".bioseq_info WHERE accession = ?";
    m_QueryArr[0].query->SetSQL(sql, 1);
    m_QueryArr[0].query->BindStr(0, m_Accession);
}

void CCassBioseqInfoTaskFetch::x_StartQuery(void)
{
    if (m_DataReadyCb) {
        m_QueryArr[0].query->SetOnData2(m_DataReadyCb, m_DataReadyData);
    }
    {
        auto DataReadyCb3 = m_DataReadyCb3.lock();
        if (DataReadyCb3) {
            m_QueryArr[0].query->SetOnData3(DataReadyCb3);
        }
    }

    UpdateLastActivity();
    m_QueryArr[0].query->Query(kBioSeqInfoConsistency, m_Async, true, m_PageSize);
}

bool CCassBioseqInfoTaskFetch::x_IsMatchingRecord()
{
    int version = m_QueryArr[0].query->FieldGetInt16Value(fnVersion);
    int seq_id_type = m_QueryArr[0].query->FieldGetInt16Value(fnSeqIdType);
    CBioseqInfoRecord::TGI gi = m_QueryArr[0].query->FieldGetInt64Value(fnGI);

    bool acceptable = true;
    if (m_GIProvided) {
        acceptable = acceptable && (gi == m_GI);
    }
    if (m_VersionProvided) {
        acceptable = acceptable && (version == m_Version);
    }
    if (m_SeqIdTypeProvided) {
        acceptable = acceptable && (seq_id_type == m_SeqIdType);
    }
    return acceptable;
}

bool CCassBioseqInfoTaskFetch::x_IsMatchingAliveRecord()
{
    return m_QueryArr[0].query->FieldGetInt16Value(fnSeqIdType) == m_Record.GetSeqIdType();
}

void CCassBioseqInfoTaskFetch::x_PopulateRecord(CBioseqInfoRecord& record) const
{
    record.SetAccession(m_Accession)
        .SetVersion(m_QueryArr[0].query->FieldGetInt16Value(fnVersion))
        .SetSeqIdType(m_QueryArr[0].query->FieldGetInt16Value(fnSeqIdType))
        .SetDateChanged(m_QueryArr[0].query->FieldGetInt64Value(fnDateChanged))
        .SetHash(m_QueryArr[0].query->FieldGetInt32Value(fnHash))
        .SetGI(m_QueryArr[0].query->FieldGetInt64Value(fnGI))
        .SetLength(m_QueryArr[0].query->FieldGetInt32Value(fnLength))
        .SetMol(m_QueryArr[0].query->FieldGetInt8Value(fnMol))
        .SetSat(m_QueryArr[0].query->FieldGetInt16Value(fnSat))
        .SetSatKey(m_QueryArr[0].query->FieldGetInt32Value(fnSatKey))
        .SetSeqState(m_QueryArr[0].query->FieldGetInt8Value(fnSeqState))
        .SetState(m_QueryArr[0].query->FieldGetInt8Value(fnState))
        .SetTaxId(m_QueryArr[0].query->FieldGetInt32Value(fnTaxId));
    m_QueryArr[0].query->FieldGetContainerValue(
        fnSeqIds, inserter(record.GetSeqIds(), record.GetSeqIds().end()));
}

void CCassBioseqInfoTaskFetch::x_ReadingLoop(void)
{
    bool stop_fetch = false;
    while (m_QueryArr[0].query->NextRow() == ar_dataready) {
        if (!stop_fetch && x_IsMatchingRecord()) {
            // GI privided: should return just one natching record
            if (m_GIProvided) {
                x_PopulateRecord(m_Record);
                m_RecordCount = 1;
                stop_fetch = true;
            }
            // Case 3: accession and seq_id_type are provided.
            // So the latest matching version has to be retrieved
            else if (m_SeqIdTypeProvided) {
                x_PopulateRecord(m_Record);
                m_RecordCount = 1;
                stop_fetch = true;
            }
            // Case 1: accession+version+seq_id_type provided
            // Case 2: accession+version are provided
            //
            // need to count matching records and return if there is just one
            else if (m_VersionProvided) {
                if (++m_RecordCount == 1) {
                    x_PopulateRecord(m_Record);
                }
            }
            // Case 4: only accession is provided;
            //
            // need to count matching records and return if there is just one
            else {
                if (++m_RecordCount == 1) {
                    x_PopulateRecord(m_Record);
                }
            }
        }
    }
}

void CCassBioseqInfoTaskFetch::x_MergeSeqIds(void)
{
    if (m_LatestRecord.GetState() == CBioseqInfoRecord::kStateAlive) {
        set<int16_t> seq_id_types;
        for (auto const & seq_id : m_Record.GetSeqIds()) {
            seq_id_types.insert(get<0>(seq_id));
        }

        for (auto const & seq_id : m_LatestRecord.GetSeqIds()) {
            if (seq_id_types.count(get<0>(seq_id)) == 0) {
                m_Record.GetSeqIds().insert(seq_id);
            }
        }
    }
}


void CCassBioseqInfoTaskFetch::Wait1(void)
{
    bool restarted;
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
                    x_StartQuery();
                    m_State = eFetchStarted;
                    break;
                }

            case eFetchStarted:
                {
                    if (CheckReady(m_QueryArr[0].query, m_RestartCounter, restarted)) {
                        x_ReadingLoop();
                        if (m_QueryArr[0].query->IsEOF()) {
                            bool final_state = true;
                            if (m_ConsumeCallback) {
                                switch (m_RecordCount) {
                                    case 0:
                                        m_ConsumeCallback(CBioseqInfoRecord(), CRequestStatus::e404_NotFound);
                                        break;
                                    case 1:
                                        if (m_Record.GetState() == CBioseqInfoRecord::kStateAlive) {
                                            m_ConsumeCallback(move(m_Record), CRequestStatus::e200_Ok);
                                        } else {
                                            final_state = false;
                                            m_QueryArr[0].query->Close();
                                            x_InitializeAliveRecordQuery();
                                            x_StartQuery();
                                            m_State = eFetchAliveVersionStarted;
                                        }
                                        break;
                                    default:
                                        m_ConsumeCallback(CBioseqInfoRecord(), CRequestStatus::e300_MultipleChoices);
                                }
                            }
                            if (final_state) {
                                CloseAll();
                                m_State = eDone;
                            }
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
            // We can assume that getting into this state we have m_ConsumeCallback
            case eFetchAliveVersionStarted:
                {
                    if (CheckReady(m_QueryArr[0])) {
                        while (m_QueryArr[0].query->NextRow() == ar_dataready) {
                            if (x_IsMatchingAliveRecord()) {
                                x_PopulateRecord(m_LatestRecord);
                                x_MergeSeqIds();
                                m_ConsumeCallback(move(m_Record), CRequestStatus::e200_Ok);
                                CloseAll();
                                m_State = eDone;
                                break;
                            }
                        }
                        if (m_QueryArr[0].query->IsEOF()) {
                            m_ConsumeCallback(move(m_Record), CRequestStatus::e200_Ok);
                            CloseAll();
                            m_State = eDone;
                        }
                    }
                    UpdateLastActivity();
                    break;
                }
            default: {
                char msg[1024];
                snprintf(msg, sizeof(msg), "Failed to fetch bioseq info (key=%s.%s.%d.%d) unexpected state (%d)",
                    m_Keyspace.c_str(), m_Accession.c_str(),
                    static_cast<int>(m_Version), static_cast<int>(m_SeqIdType),
                    static_cast<int>(m_State));
                Error(CRequestStatus::e502_BadGateway, CCassandraException::eQueryFailed, eDiag_Error, msg);
            }
        }
    } while(restarted);
}

END_IDBLOB_SCOPE
