#ifndef OBJTOOLS__PUBSEQ_GATEWAY__CASSANDRA__STATUS_HISTORY__GET_PUBLIC_COMMENT_HPP
#define OBJTOOLS__PUBSEQ_GATEWAY__CASSANDRA__STATUS_HISTORY__GET_PUBLIC_COMMENT_HPP

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

#include <corelib/request_status.hpp>
#include <corelib/ncbidiag.hpp>

#include <memory>
#include <string>

#include <objtools/pubseq_gateway/impl/cassandra/cass_blob_op.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/IdCassScope.hpp>

#include <objtools/pubseq_gateway/impl/cassandra/blob_record.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/status_history/record.hpp>

BEGIN_IDBLOB_SCOPE
USING_NCBI_SCOPE;

class CCassStatusHistoryTaskGetPublicComment
    : public CCassBlobWaiter
{
    enum EBlobInserterState {
        eInit = 0,
        eStartReading,
        eReadingHistory,
        eDone = CCassBlobWaiter::eDone,
        eError = CCassBlobWaiter::eError
    };
    static constexpr int64_t kMaxReplacesRetries = 5;

 public:
    static constexpr TBlobStatusFlagsBase kWithdrawnMask =
        static_cast<TBlobStatusFlagsBase>(EBlobStatusFlags::eWithdrawn) +
        static_cast<TBlobStatusFlagsBase>(EBlobStatusFlags::eWithdrawnPermanently);

    CCassStatusHistoryTaskGetPublicComment(
        unsigned int op_timeout_ms,
        unsigned int max_retries,
        shared_ptr<CCassConnection> conn,
        const string & keyspace,
        CBlobRecord const &blob,
        TDataErrorCallback data_error_cb
    );

    string GetComment()
    {
        if (m_State != eDone && m_State != eError && !m_Cancelled) {
            Error(
                CRequestStatus::e500_InternalServerError, CCassandraException::eGeneric,
                eDiag_Error, "GetComment() called before task completion"
            );
        }
        return m_PublicComment;
    }

    void SetDataReadyCB(TDataReadyCallback callback, void * data);

 protected:
    virtual void Wait1(void) override;

 private:
    void JumpToReplaced(CBlobRecord::TSatKey replaced);

    TBlobFlagBase m_BlobFlags;
    TBlobStatusFlagsBase m_FirstHistoryFlags;
    bool m_MatchingStatusRowFound;
    int64_t m_ReplacesRetries;
    string m_PublicComment;
};

END_IDBLOB_SCOPE

#endif  // OBJTOOLS__PUBSEQ_GATEWAY__CASSANDRA__STATUS_HISTORY__GET_PUBLIC_COMMENT_HPP
