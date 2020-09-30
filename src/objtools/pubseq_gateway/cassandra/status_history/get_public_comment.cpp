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
 * Task to resolve blob public comment from Cassandra
 *
 */

#include <ncbi_pch.hpp>

#include <objtools/pubseq_gateway/impl/cassandra/status_history/get_public_comment.hpp>

#include <memory>
#include <string>
#include <utility>

#include <objtools/pubseq_gateway/impl/cassandra/cass_blob_op.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/cass_driver.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/IdCassScope.hpp>

BEGIN_IDBLOB_SCOPE
USING_NCBI_SCOPE;

BEGIN_SCOPE()

bool IsBlobWithdrawn(TBlobFlagBase flags) {
    return (flags & static_cast<TBlobFlagBase>(EBlobFlags::eWithdrawn)) != 0;
}

bool IsBlobSuppressed(TBlobFlagBase flags) {
    return (flags & static_cast<TBlobFlagBase>(EBlobFlags::eSuppress)) != 0;
}

bool SameWithdrawn(TBlobStatusFlagsBase a, TBlobStatusFlagsBase b) {
    using TTask = CCassStatusHistoryTaskGetPublicComment;
    return (a & TTask::kWithdrawnMask) == (b & TTask::kWithdrawnMask);
}

bool IsHistorySuppressed(TBlobStatusFlagsBase flags) {
    return (flags & static_cast<TBlobStatusFlagsBase>(EBlobStatusFlags::eSuppressPermanently)) != 0;
}

END_SCOPE()

CCassStatusHistoryTaskGetPublicComment::CCassStatusHistoryTaskGetPublicComment(
    unsigned int op_timeout_ms,
    unsigned int max_retries,
    shared_ptr<CCassConnection> conn,
    const string & keyspace,
    CBlobRecord const &blob,
    TDataErrorCallback data_error_cb
)
    : CCassBlobWaiter(
        op_timeout_ms, conn, keyspace, blob.GetKey(),
        true, max_retries, move(data_error_cb)
      )
    , m_BlobFlags(blob.GetFlags())
    , m_FirstHistoryFlags(-1)
    , m_MatchingStatusRowFound(false)
    , m_ReplacesRetries(kMaxReplacesRetries)
{}

void CCassStatusHistoryTaskGetPublicComment::SetDataReadyCB(TDataReadyCallback callback, void * data)
{
    if (callback && m_State != eInit) {
        NCBI_THROW(CCassandraException, eSeqFailed,
           "CCassBlobTaskLoadBlob: DataReadyCB can't be assigned "
           "after the loading process has started");
    }
    CCassBlobWaiter::SetDataReadyCB(callback, data);
}

void CCassStatusHistoryTaskGetPublicComment::JumpToReplaced(CBlobRecord::TSatKey replaced)
{
    --m_ReplacesRetries;
    m_Key = replaced;
    m_MatchingStatusRowFound = false;
    m_State = eStartReading;
    m_PublicComment.clear();
}

void CCassStatusHistoryTaskGetPublicComment::Wait1()
{
    bool b_need_repeat;
    do {
        b_need_repeat = false;
        switch (m_State) {
            case eError:
            case eDone:
                return;

            case eInit: {
                if (!IsBlobSuppressed(m_BlobFlags) && !IsBlobWithdrawn(m_BlobFlags)) {
                    m_State = eDone;
                    return;
                }
                m_State = eStartReading;
                b_need_repeat = true;
                break;
            }

            case eStartReading: {
                CloseAll();
                m_QueryArr.clear();
                m_QueryArr.push_back({m_Conn->NewQuery(), 0});
                auto qry = m_QueryArr[0].query;
                string sql =
                    "SELECT flags, public_comment, replaces "
                    "FROM " + GetKeySpace() + ".blob_status_history WHERE sat_key = ?";
                qry->SetSQL(sql, 1);
                qry->BindInt32(0, m_Key);
                SetupQueryCB3(qry);
                UpdateLastActivity();
                qry->Query(GetQueryConsistency(), m_Async, true);
                m_State = eReadingHistory;
                break;
            }

            case eReadingHistory: {
                auto query = m_QueryArr[0].query;
                if (CheckReady(m_QueryArr[0])) {
                    while (query->NextRow() == ar_dataready) {
                        int64_t flags = query->FieldGetInt64Value(0, 0);
                        string comment = query->FieldGetStrValueDef(1, "");
                        CBlobRecord::TSatKey replaces = query->FieldGetInt32Value(2, 0);

                        // blob_prop does not have full withdrawn representation so
                        // as a workaround we use first history record flags
                        if (m_FirstHistoryFlags == -1) {
                            m_FirstHistoryFlags = flags;
                        }
                        // blob is withdrawn
                        if (IsBlobWithdrawn(m_BlobFlags)) {
                            if (!SameWithdrawn(flags, m_FirstHistoryFlags)) {
                                if (m_MatchingStatusRowFound) {
                                    m_State = eDone;
                                    return;
                                } else if (replaces > 0 && m_ReplacesRetries > 0) {
                                    JumpToReplaced(replaces);
                                    b_need_repeat = true;
                                } else {
                                    m_State = eDone;
                                    return;
                                }
                            } else {
                                m_MatchingStatusRowFound = true;
                                m_PublicComment = comment;
                            }
                        }
                        // blob is suppressed
                        else {
                            if (!IsHistorySuppressed(flags)) {
                                if (m_MatchingStatusRowFound) {
                                    m_State = eDone;
                                    return;
                                } else if (replaces > 0 && m_ReplacesRetries > 0) {
                                    JumpToReplaced(replaces);
                                    b_need_repeat = true;
                                } else {
                                    m_State = eDone;
                                    return;
                                }
                            } else {
                                m_MatchingStatusRowFound = true;
                                m_PublicComment = comment;
                            }
                        }
                    }
                    if (query->IsEOF()) {
                        CloseAll();
                        m_State = eDone;
                    }
                }
                break;
            }

            default: {
                char msg[1024];
                snprintf(msg, sizeof(msg), "Failed to get public comment for record (key=%s.%d) unexpected state (%d)",
                    m_Keyspace.c_str(), m_Key, static_cast<int>(m_State));
                Error(CRequestStatus::e502_BadGateway, CCassandraException::eQueryFailed, eDiag_Error, msg);
            }
        }
    } while (b_need_repeat);
}

END_IDBLOB_SCOPE
