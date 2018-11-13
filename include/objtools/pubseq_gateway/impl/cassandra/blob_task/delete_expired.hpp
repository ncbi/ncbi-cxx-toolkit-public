#ifndef OBJTOOLS__PUBSEQ_GATEWAY__CASSANDRA__BLOB_TASK__DELETE_EXPIRED_HPP_
#define OBJTOOLS__PUBSEQ_GATEWAY__CASSANDRA__BLOB_TASK__DELETE_EXPIRED_HPP_

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
 * Cassandra delete blob expired version task.
 *
 */

#include <memory>
#include <string>

#include <corelib/request_status.hpp>
#include <corelib/ncbidiag.hpp>

#include <objtools/pubseq_gateway/impl/cassandra/cass_blob_op.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/IdCassScope.hpp>

BEGIN_IDBLOB_SCOPE
USING_NCBI_SCOPE;

class CCassBlobTaskDeleteExpired: public CCassBlobWaiter
{
 public:
    CCassBlobTaskDeleteExpired(
        unsigned int op_timeout_ms,
        shared_ptr<CCassConnection> conn,
        const string & keyspace,
        int32_t key,
        CBlobRecord::TTimestamp last_modified,
        CBlobRecord::TTimestamp expiration,
        unsigned int max_retries,
        TDataErrorCallback error_cb
    );

    bool IsExpiredVersionDeleted() const;

 protected:
    virtual void Wait1(void) override;

 private:
    enum EBlobDeleteState {
        eInit = 0,
        eReadingWriteTime,
        eDeleteData,
        eWaitDeleteData,
        eDone = CCassBlobWaiter::eDone,
        eError = CCassBlobWaiter::eError
    };

    CBlobRecord::TTimestamp m_LastModified;
    CBlobRecord::TTimestamp m_Expiration;
    bool m_ExpiredVersionDeleted;
};

END_IDBLOB_SCOPE

#endif  // OBJTOOLS__PUBSEQ_GATEWAY__CASSANDRA__BLOB_TASK__DELETE_EXPIRED_HPP_
