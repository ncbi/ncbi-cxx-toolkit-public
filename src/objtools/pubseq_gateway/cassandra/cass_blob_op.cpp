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
 * Authors: Dmitri Dmitrienko
 *
 * File Description:
 *
 * cassandra high-level functionality around blobs
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbireg.hpp>

#include "id2_split_task/insert.hpp"
#include <objtools/pubseq_gateway/impl/cassandra/blob_task/insert_extended.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/blob_task/delete.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/nannot_task/insert.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/nannot_task/delete.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/blob_task/load_blob.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/blob_task/delete_expired.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/nannot_task/fetch.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/acc_ver_hist/tasks.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/cass_blob_op.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/SyncObj.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/cass_driver.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/IdCassScope.hpp>

#include <algorithm>
#include <mutex>
#include <atomic>
#include <fstream>
#include <vector>
#include <list>
#include <utility>
#include <memory>
#include <string>

BEGIN_IDBLOB_SCOPE
USING_NCBI_SCOPE;

BEGIN_SCOPE()

constexpr const char * kSettingLargeChunkSize = "LARGE_CHUNK_SZ";
constexpr const char * kSettingBigBlobSize = "BIG_BLOB_SZ";
constexpr int64_t kChunkSizeMin = 4 * 1024;
constexpr int64_t kChunkSizeDefault = 512 * 1024;
constexpr int64_t kActiveStatementsMax = 512;

string KeySpaceDot(const string& keyspace)
{
    return keyspace.empty() ? keyspace : keyspace + ".";
}

END_SCOPE()

/** CCassBlobWaiter */

bool CCassBlobWaiter::CheckMaxActive()
{
    return (m_Conn->GetActiveStatements() < kActiveStatementsMax);
}

void CCassBlobOp::GetBlobChunkSize(unsigned int timeout_ms, const string & keyspace, int64_t * chunk_size)
{
    string s;
    if (
        !GetSetting(timeout_ms, keyspace, kSettingLargeChunkSize, s)
        || !NStr::StringToNumeric(s, chunk_size)
        || *chunk_size < kChunkSizeMin
    ) {
        *chunk_size = kChunkSizeDefault;
        UpdateSetting(timeout_ms, keyspace, kSettingLargeChunkSize, NStr::NumericToString(*chunk_size));
    }
}

void CCassBlobOp::GetBigBlobSizeLimit(unsigned int timeout_ms, const string & keyspace, int64_t * value)
{
    string s;
    if (!GetSetting(timeout_ms, keyspace, kSettingBigBlobSize, s) || !NStr::StringToNumeric(s, value)) {
        *value = -1;
    }
}

void CCassBlobOp::InsertID2Split(
    unsigned int op_timeout_ms,
    unsigned int max_retries,
    CBlobRecord * blob, CID2SplitRecord* id2_split,
    TDataErrorCallback error_cb,
    unique_ptr<CCassBlobWaiter> & waiter
)
{
    waiter = make_unique<CCassID2SplitTaskInsert>(
        op_timeout_ms, m_Conn, m_Keyspace,
        blob, id2_split, max_retries,
        move(error_cb)
    );
}

void CCassBlobOp::UpdateBlobFlagsExtended(
    unsigned int op_timeout_ms,
    CBlobRecord::TSatKey key,
    EBlobFlags flag,
    bool set_flag
) {
    CCassConnection::Perform(
        op_timeout_ms, nullptr, nullptr,
        [this, key, flag,  set_flag] (bool) -> bool {
            int64_t new_flags = 0;
            CBlobRecord::TTimestamp last_modified;
            shared_ptr<CCassQuery> qry = m_Conn->NewQuery();
            string sql = "SELECT last_modified, flags FROM "
                + KeySpaceDot(m_Keyspace) + "blob_prop WHERE sat_key = ? limit 1";
            qry->SetSQL(sql, 1);
            qry->BindInt32(0, key);
            qry->Query(CASS_CONSISTENCY_LOCAL_QUORUM);
            if (!qry->IsEOF() && qry->NextRow() == ar_dataready) {
                last_modified = qry->FieldGetInt64Value(0);
                if (set_flag) {
                    new_flags = qry->FieldGetInt64Value(1) | static_cast<TBlobFlagBase>(flag);
                } else {
                    new_flags = qry->FieldGetInt64Value(1) & ~(static_cast<TBlobFlagBase>(flag));
                }
                qry->Close();
            } else {
                return false;
            }
            sql = "UPDATE " + KeySpaceDot(m_Keyspace)
                + "blob_prop SET flags = ? WHERE sat_key = ? and last_modified = ?";
            qry->SetSQL(sql, 3);
            qry->BindInt64(0, new_flags);
            qry->BindInt32(1, key);
            qry->BindInt64(2, last_modified);
            qry->Execute(CASS_CONSISTENCY_LOCAL_QUORUM);
            return true;
        }
    );
}

/*****************************************************

                IN-TABLE    SETTINGS

*****************************************************/

void CCassBlobOp::UpdateSetting(unsigned int timeout_ms, const string & domain, const string & name, const string & value)
{
    CCassConnection::Perform(timeout_ms, nullptr, nullptr,
        [this, domain, name, value](bool /*is_repeated*/) {
            auto qry = m_Conn->NewQuery();
            qry->SetSQL("INSERT INTO maintenance.settings (domain, name, value) VALUES(?, ?, ?)", 3);
            qry->BindStr(0, domain);
            qry->BindStr(1, name);
            qry->BindStr(2, value);
            qry->Execute(CASS_CONSISTENCY_LOCAL_QUORUM, false, false);
            return true;
        }
    );
}

bool CCassBlobOp::GetSetting(unsigned int op_timeout_ms, const string & domain, const string & name, string & value)
{
    bool rslt = false;
    CCassConnection::Perform(op_timeout_ms, nullptr, nullptr,
        [this, domain, name, &value, &rslt]
        (bool is_repeated)
        {
            auto qry = m_Conn->NewQuery();
            qry->SetSQL("SELECT value FROM maintenance.settings WHERE domain = ? AND name = ?", 2);
            qry->BindStr(0, domain);
            qry->BindStr(1, name);
            CassConsistency cons = is_repeated && m_Conn->GetFallBackRdConsistency() ?
                CASS_CONSISTENCY_LOCAL_ONE : CASS_CONSISTENCY_LOCAL_QUORUM;
            qry->Query(cons, false, false);
            if (qry->NextRow() == ar_dataready) {
                qry->FieldGetStrValue(0, value);
                rslt = true;
            }
            return true;
        }
    );

    return rslt;
}

END_IDBLOB_SCOPE
