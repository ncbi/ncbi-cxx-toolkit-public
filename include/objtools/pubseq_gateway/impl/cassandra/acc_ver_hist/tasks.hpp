/*****************************************************************************
 *  $Id$
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
 *  Blob storage: blob status history record
 *
 *****************************************************************************/

#ifndef OBJTOOLS__PUBSEQ_GATEWAY__IMPL__CASSANDRA__ACC_VER_HISTORY__TASKS_HPP
#define OBJTOOLS__PUBSEQ_GATEWAY__IMPL__CASSANDRA__ACC_VER_HISTORY__TASKS_HPP

#include <corelib/ncbistd.hpp>

#include <sstream>
#include <memory>
#include <string>

#include <corelib/request_status.hpp>
#include <corelib/ncbidiag.hpp>

#include <objtools/pubseq_gateway/impl/cassandra/cass_blob_op.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/IdCassScope.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/acc_ver_hist/record.hpp>

BEGIN_IDBLOB_SCOPE
USING_NCBI_SCOPE;

//::::::::
class CCassAccVerHistoryTaskInsert
    : public CCassBlobWaiter
{
    enum EAccVerHistoryInserterState
    {
        eInit = 0,
        eWaitingRecordInserted,
        eDone = CCassBlobWaiter::eDone,
        eError = CCassBlobWaiter::eError
    };

 public:
    CCassAccVerHistoryTaskInsert(
        unsigned int op_timeout_ms,
        shared_ptr<CCassConnection> conn,
        const string & keyspace,
        SAccVerHistRec * record,
        unsigned int max_retries,
        TDataErrorCallback data_error_cb
    );

 protected:
    virtual void Wait1(void) override;

 private:
    SAccVerHistRec * m_Record;
};

//::::::::
class CCassAccVerHistoryTaskFetch
    : public CCassBlobWaiter
{
    enum EBlobFetchState
    {
        eInit = 0,
        eFetchStarted,
        eDone = CCassBlobWaiter::eDone,
        eError = CCassBlobWaiter::eError
    };

public:
    CCassAccVerHistoryTaskFetch(
        unsigned int timeout_ms,
        unsigned int max_retries,
        shared_ptr<CCassConnection> connection,
        const string & keyspace,
        string accession,
        TAccVerHistConsumeCallback consume_callback,
        TDataErrorCallback data_error_cb,
        int16_t version = 0,
        int16_t seq_id_type = 0
    );

    void SetDataReadyCB(shared_ptr<CCassDataCallbackReceiver> callback);
    void SetConsumeCallback(TAccVerHistConsumeCallback callback);

    string GetKeyspace() const
    {
        return m_Keyspace;
    }

protected:
    virtual void Wait1(void) override;

private:
    string m_Accession;
    int16_t m_Version;
    int16_t m_SeqIdType;

    TAccVerHistConsumeCallback m_Consume;
protected:
    unsigned int m_PageSize;
    unsigned int m_RestartCounter;
};

END_IDBLOB_SCOPE

#endif  // OBJTOOLS__PUBSEQ_GATEWAY__IMPL__CASSANDRA__ACC_VER_HISTORY__TASKS_HPP
