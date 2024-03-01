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

#include <objtools/pubseq_gateway/impl/cassandra/cass_blob_op.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/cass_driver.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/IdCassScope.hpp>

#include <mutex>
#include <string>

BEGIN_IDBLOB_SCOPE
USING_NCBI_SCOPE;

BEGIN_SCOPE()

constexpr const char * kSettingLargeChunkSize = "LARGE_CHUNK_SZ";
constexpr const char * kSettingBigBlobSize = "BIG_BLOB_SZ";
constexpr int64_t kChunkSizeMin = 4 * 1024;
constexpr int64_t kChunkSizeDefault = 512 * 1024;
constexpr int64_t kActiveStatementsMax = 512;

END_SCOPE()

/** CCassBlobWaiter */

bool CCassBlobWaiter::CheckMaxActive()
{
    return (m_Conn->GetActiveStatements() < kActiveStatementsMax);
}

void CCassBlobWaiter::SetQueryTimeout(std::chrono::milliseconds value)
{
    m_QueryTimeout = value;
}

std::chrono::milliseconds CCassBlobWaiter::GetQueryTimeout() const
{
    return m_QueryTimeout;
}

shared_ptr<CCassQuery> CCassBlobWaiter::ProduceQuery() const
{
    auto query = m_Conn->NewQuery();
    if (m_QueryTimeout.count() > 0) {
        query->SetTimeout(m_QueryTimeout.count());
        query->UsePerRequestTimeout(true);
    }
    return query;
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
    }
}

void CCassBlobOp::GetBigBlobSizeLimit(unsigned int timeout_ms, const string & keyspace, int64_t * value)
{
    string s;
    if (!GetSetting(timeout_ms, keyspace, kSettingBigBlobSize, s) || !NStr::StringToNumeric(s, value)) {
        *value = -1;
    }
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
