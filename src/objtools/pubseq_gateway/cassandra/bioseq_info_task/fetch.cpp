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
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include <objtools/pubseq_gateway/impl/cassandra/cass_blob_op.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/cass_driver.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/IdCassScope.hpp>

#define BIOSEQ_INFO_CONSISTENCY     CassConsistency::CASS_CONSISTENCY_LOCAL_QUORUM


BEGIN_IDBLOB_SCOPE
USING_NCBI_SCOPE;


// Select field numbers; must be in sync with the select statemens
static const int    fnVersion = 0;
static const int    fnSeqIdType = 1;
static const int    fnDateChanged = 2;
static const int    fnHash = 3;
static const int    fnIdSync = 4;
static const int    fnLength = 5;
static const int    fnMol = 6;
static const int    fnSat = 7;
static const int    fnSatKey = 8;
static const int    fnSeqIds = 9;
static const int    fnSeqState = 10;
static const int    fnState = 11;
static const int    fnTaxId = 12;

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
                            TBioseqInfoConsumeCallback             consume_callback,
                            TDataErrorCallback                     data_error_cb) :
    CCassBlobWaiter(timeout_ms, connection, keyspace, 0,
                    true, max_retries, move(data_error_cb)),
    m_Accession(accession),
    m_Version(version),
    m_VersionProvided(version_provided),
    m_SeqIdType(seq_id_type),
    m_SeqIdTypeProvided(seq_id_type_provided),
    m_ConsumeCallback(consume_callback),
    m_RecordCount(0),
    m_SelectedVersion(INT_MIN),
    m_SelectedSeqIdType(INT_MIN),
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
    static const string     s_Select = "SELECT version, seq_id_type, "
                                              "date_changed, hash, id_sync, "
                                              "length, mol, sat, sat_key, "
                                              "seq_ids, seq_state, state, "
                                              "tax_id FROM ";
    static const string     s_Where_1 = ".BIOSEQ_INFO WHERE accession = ?";
    static const string     s_Where_2 = s_Where_1 + " AND version = ?";
    static const string     s_Where_3 = s_Where_2 + " AND seq_id_type = ?";

    string      sql = s_Select;
    sql.append(GetKeySpace());

    if (m_VersionProvided && m_SeqIdTypeProvided) {
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


void CCassBioseqInfoTaskFetch::x_PopulateRecord(void)
{
    m_Record.SetAccession(m_Accession)
            .SetVersion(m_QueryArr[0].query->FieldGetInt16Value(fnVersion))
            .SetSeqIdType(m_QueryArr[0].query->FieldGetInt16Value(fnSeqIdType))
            .SetDateChanged(m_QueryArr[0].query->FieldGetInt64Value(fnDateChanged))
            .SetHash(m_QueryArr[0].query->FieldGetInt32Value(fnHash))
            .SetIdSync(m_QueryArr[0].query->FieldGetInt64Value(fnIdSync))
            .SetLength(m_QueryArr[0].query->FieldGetInt32Value(fnLength))
            .SetMol(m_QueryArr[0].query->FieldGetInt8Value(fnMol))
            .SetSat(m_QueryArr[0].query->FieldGetInt16Value(fnSat))
            .SetSatKey(m_QueryArr[0].query->FieldGetInt32Value(fnSatKey))

            .SetSeqState(m_QueryArr[0].query->FieldGetInt8Value(fnSeqState))
            .SetState(m_QueryArr[0].query->FieldGetInt8Value(fnState))
            .SetTaxId(m_QueryArr[0].query->FieldGetInt32Value(fnTaxId));

    m_QueryArr[0].query->FieldGetContainerValue(
                            fnSeqIds, inserter(m_Record.GetSeqIds(),
                                               m_Record.GetSeqIds().end()));
}


void CCassBioseqInfoTaskFetch::x_ReadingLoop(void)
{
    if (m_VersionProvided) {
        // Case 1: all three fields are provided
        // Case 2: accession and version are provided
        // not found; more than one, exactly one
        while (m_QueryArr[0].query->NextRow() == ar_dataready) {
            ++m_RecordCount;
            if (m_RecordCount == 1) {
                x_PopulateRecord();
            }
        }
        return;
    }

    if (m_SeqIdTypeProvided) {
        // Case 3: accession and seq_id_type are provided.
        // So the latest version has to be retrieved
        while (m_QueryArr[0].query->NextRow() == ar_dataready) {
            int   seq_id_type = m_QueryArr[0].query->FieldGetInt16Value(fnSeqIdType);
            if (m_SeqIdType != seq_id_type)
                continue;

            int     version = m_QueryArr[0].query->FieldGetInt16Value(fnVersion);
            if (m_RecordCount == 0 || version > m_SelectedVersion) {
                x_PopulateRecord();
                m_SelectedVersion = version;
                m_RecordCount = 1;
            }
        }
        return;
    }

    // Case 4: only accession is provided;
    //         select the latest version;
    //         check that there is exactly one seq_it_type
    while (m_QueryArr[0].query->NextRow() == ar_dataready) {
        if (m_RecordCount == 0) {
            x_PopulateRecord();
            m_SelectedVersion = m_Record.GetVersion();
            m_SelectedSeqIdType = m_Record.GetSeqIdType();
            m_RecordCount = 1;
            continue;
        }

        int     seq_id_type = m_QueryArr[0].query->FieldGetInt16Value(fnSeqIdType);
        if (m_RecordCount == 1 && m_SelectedSeqIdType == seq_id_type) {
            // Take the latest version
            int     version = m_QueryArr[0].query->FieldGetInt16Value(fnVersion);
            if (version > m_SelectedVersion) {
                x_PopulateRecord();
                m_SelectedVersion = m_Record.GetVersion();
            }
            continue;
        }

        ++m_RecordCount;
    }
}


void CCassBioseqInfoTaskFetch::Wait1(void)
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
                    m_QueryArr[0].query->Query(BIOSEQ_INFO_CONSISTENCY,
                                               m_Async, true, m_PageSize);
                    m_State = eFetchStarted;
                    break;
                }

            case eFetchStarted:
                {
                    if (CheckReady(m_QueryArr[0].query,
                                   m_RestartCounter, restarted)) {

                        x_ReadingLoop();

                        if (m_QueryArr[0].query->IsEOF()) {
                            if (m_ConsumeCallback) {
                                switch (m_RecordCount) {
                                    case 0:
                                        m_ConsumeCallback(CBioseqInfoRecord(),
                                                          CRequestStatus::e404_NotFound);
                                        break;
                                    case 1:
                                        m_ConsumeCallback(move(m_Record),
                                                          CRequestStatus::e200_Ok);
                                        break;
                                    default:
                                        m_ConsumeCallback(CBioseqInfoRecord(),
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
                snprintf(msg, sizeof(msg), "Failed to fetch bioseq info "
                                           "(key=%s.%s.%d.%d) unexpected state (%d)",
                    m_Keyspace.c_str(), m_Accession.c_str(),
                    static_cast<int>(m_Version), static_cast<int>(m_SeqIdType),
                    static_cast<int>(m_State));
                Error(CRequestStatus::e502_BadGateway,
                      CCassandraException::eQueryFailed, eDiag_Error, msg);
            }
        }
    } while(restarted);
}

END_IDBLOB_SCOPE
