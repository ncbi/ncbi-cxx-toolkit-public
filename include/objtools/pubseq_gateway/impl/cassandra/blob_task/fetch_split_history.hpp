#ifndef OBJTOOLS__PUBSEQ_GATEWAY__CASSANDRA__BLOB_TASK__FETCH_SPLIT_HISTORY_HPP_
#define OBJTOOLS__PUBSEQ_GATEWAY__CASSANDRA__BLOB_TASK__FETCH_SPLIT_HISTORY_HPP_

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

#include <objtools/pubseq_gateway/impl/cassandra/cass_blob_op.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/IdCassScope.hpp>

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

BEGIN_IDBLOB_SCOPE
USING_NCBI_SCOPE;

struct SSplitHistoryRecord
{
    using TSplitVersion = int32_t;

    CBlobRecord::TSatKey sat_key{0};
    CBlobRecord::TTimestamp modified{0};
    TSplitVersion split_version{0};
    string id2_info;
};

class CCassBlobTaskFetchSplitHistory
    : public CCassBlobWaiter
{
    enum ETaskState {
        eInit = 0,
        eWaitingForFetch,
        eDone = CCassBlobWaiter::eDone,
        eError = CCassBlobWaiter::eError
    };

 public:
    static const SSplitHistoryRecord::TSplitVersion kAllVersions = -1;
    using TConsumeCallback = function<void(vector<SSplitHistoryRecord> &&)>;

    CCassBlobTaskFetchSplitHistory(
        unsigned int op_timeout_ms,
        unsigned int max_retries,
        shared_ptr<CCassConnection> conn,
        const string & keyspace,
        CBlobRecord::TSatKey sat_key,
        TConsumeCallback consume_callback,
        TDataErrorCallback data_error_cb
    );

    CCassBlobTaskFetchSplitHistory(
        unsigned int op_timeout_ms,
        unsigned int max_retries,
        shared_ptr<CCassConnection> conn,
        const string & keyspace,
        CBlobRecord::TSatKey sat_key,
        SSplitHistoryRecord::TSplitVersion split_version,
        TConsumeCallback consume_callback,
        TDataErrorCallback data_error_cb
    );

    string GetKeyspace() const
    {
        return m_Keyspace;
    }

    CBlobRecord::TSatKey GetKey() const
    {
        return m_Key;
    }

    SSplitHistoryRecord::TSplitVersion GetSplitVersion() const
    {
        return m_SplitVersion;
    }

    void SetDataReadyCB(TDataReadyCallback callback, void * data);
    void SetDataReadyCB(shared_ptr<CCassDataCallbackReceiver> callback);
    void SetConsumeCallback(TConsumeCallback callback);

 protected:
    virtual void Wait1(void) override;

 private:
    SSplitHistoryRecord::TSplitVersion m_SplitVersion;
    vector<SSplitHistoryRecord> m_Result;
    TConsumeCallback m_ConsumeCallback;
    unsigned int m_RestartCounter;
};

END_IDBLOB_SCOPE

#endif  // OBJTOOLS__PUBSEQ_GATEWAY__CASSANDRA__BLOB_TASK__FETCH_SPLIT_HISTORY_HPP_
