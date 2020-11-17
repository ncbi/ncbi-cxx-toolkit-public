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
 * Fetch operation for Cassandra blob split history
 *
 */

#include <ncbi_pch.hpp>

#include <objtools/pubseq_gateway/impl/cassandra/blob_task/fetch_split_history.hpp>

#include <memory>
#include <string>
#include <utility>

#include <objtools/pubseq_gateway/impl/cassandra/cass_blob_op.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/cass_driver.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/IdCassScope.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/blob_record.hpp>

BEGIN_IDBLOB_SCOPE
USING_NCBI_SCOPE;

CCassBlobTaskFetchSplitHistory::CCassBlobTaskFetchSplitHistory(
    unsigned int op_timeout_ms,
    unsigned int max_retries,
    shared_ptr<CCassConnection> conn,
    const string & keyspace,
    CBlobRecord::TSatKey sat_key,
    TConsumeCallback consume_callback,
    TDataErrorCallback data_error_cb
)
    : CCassBlobTaskFetchSplitHistory(
        op_timeout_ms, max_retries, conn, keyspace, sat_key, kAllVersions, move(consume_callback), data_error_cb
    )
{
}

CCassBlobTaskFetchSplitHistory::CCassBlobTaskFetchSplitHistory(
    unsigned int timeout_ms,
    unsigned int max_retries,
    shared_ptr<CCassConnection> connection,
    const string & keyspace,
    CBlobRecord::TSatKey sat_key,
    SSplitHistoryRecord::TSplitVersion split_version,
    TConsumeCallback consume_callback,
    TDataErrorCallback data_error_cb
)
    : CCassBlobWaiter(timeout_ms, connection, keyspace, sat_key, true, max_retries, move(data_error_cb))
    , m_SplitVersion(split_version)
    , m_ConsumeCallback(move(consume_callback))
    , m_RestartCounter(0)
{
}

void CCassBlobTaskFetchSplitHistory::SetConsumeCallback(TConsumeCallback callback)
{
    m_ConsumeCallback = move(callback);
}

void CCassBlobTaskFetchSplitHistory::SetDataReadyCB(TDataReadyCallback callback, void * data)
{
    if (callback && m_State != eInit) {
        NCBI_THROW(CCassandraException, eSeqFailed,
           "CCassBlobTaskFetchSplitHistory: DataReadyCB can't be assigned "
           "after the loading process has started");
    }
    CCassBlobWaiter::SetDataReadyCB(callback, data);
}


void CCassBlobTaskFetchSplitHistory::SetDataReadyCB(shared_ptr<CCassDataCallbackReceiver> callback)
{
    if (callback && m_State != eInit) {
        NCBI_THROW(CCassandraException, eSeqFailed,
           "CCassBlobTaskFetchSplitHistory: DataReadyCB can't be assigned "
           "after the loading process has started");
    }
    CCassBlobWaiter::SetDataReadyCB3(callback);
}

void CCassBlobTaskFetchSplitHistory::Wait1(void)
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
                m_QueryArr[0] = {m_Conn->NewQuery(), 0};
                auto query = m_QueryArr[0].query;
                string sql = "SELECT split_version, last_modified, id2_info FROM " + GetKeySpace() +
                    ".blob_split_history WHERE sat_key = ?";
                if (m_SplitVersion == kAllVersions) {
                    query->SetSQL(sql, 1);
                    query->BindInt32(0, m_Key);
                } else {
                    sql.append(" and split_version = ?");
                    query->SetSQL(sql, 2);
                    query->BindInt32(0, m_Key);
                    query->BindInt32(1, m_SplitVersion);
                }

                if (m_DataReadyCb) {
                    query->SetOnData2(m_DataReadyCb, m_DataReadyData);
                }
                {
                    auto DataReadyCb3 = m_DataReadyCb3.lock();
                    if (DataReadyCb3) {
                        query->SetOnData3(DataReadyCb3);
                    }
                }
                UpdateLastActivity();
                query->Query(GetQueryConsistency(), m_Async, true);
                m_State = eWaitingForFetch;
                break;
            }

            case eWaitingForFetch: {
                auto query = m_QueryArr[0].query;
                if (CheckReady(query, m_RestartCounter, restarted)) {
                    while (query->NextRow() == ar_dataready) {
                        size_t new_item_idx = m_Result.size();
                        m_Result.resize(new_item_idx + 1);
                        m_Result[new_item_idx].sat_key = m_Key;
                        m_Result[new_item_idx].split_version = query->FieldGetInt32Value(0, 0);
                        m_Result[new_item_idx].modified = query->FieldGetInt64Value(1, 0);
                        m_Result[new_item_idx].id2_info = query->FieldGetStrValueDef(2, "");
                    }
                    if (query->IsEOF()) {
                        if (m_ConsumeCallback) {
                            m_ConsumeCallback(move(m_Result));
                            m_Result.clear();
                        }

                        CloseAll();
                        m_State = eDone;
                    }
                } else if (restarted) {
                    ++m_RestartCounter;
                    query->Close();
                    m_State = eInit;
                    m_Result.clear();
                }
                UpdateLastActivity();
                break;
        }

            default: {
                char  msg[1024];
                snprintf(msg, sizeof(msg), "Failed to fetch bioseq info (key=%s.%d.%d) unexpected state (%d)",
                    m_Keyspace.c_str(),
                    m_Key,
                    static_cast<int>(m_SplitVersion),
                    static_cast<int>(m_State));
                Error(CRequestStatus::e502_BadGateway, CCassandraException::eQueryFailed, eDiag_Error, msg);
            }
        }
    } while(restarted);
}

END_IDBLOB_SCOPE
