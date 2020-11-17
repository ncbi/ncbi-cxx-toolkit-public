#ifndef OBJTOOLS__PUBSEQ_GATEWAY__CASSANDRA__BIOSEQ_INFO_TASK__FETCH_HPP_
#define OBJTOOLS__PUBSEQ_GATEWAY__CASSANDRA__BIOSEQ_INFO_TASK__FETCH_HPP_

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
 * Cassandra select from BIOSEQ_INFO
 *
 */

#include <functional>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include <corelib/ncbistr.hpp>

#include <objtools/pubseq_gateway/impl/cassandra/request.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/cass_blob_op.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/IdCassScope.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/bioseq_info/record.hpp>

BEGIN_IDBLOB_SCOPE
USING_IDBLOB_SCOPE;

class CCassBioseqInfoTaskFetch : public CCassBlobWaiter
{
    enum EBlobFetchState {
        eInit = CCassBlobWaiter::eInit,
        eFetchStarted,
        eFetchAliveVersionStarted,
        eDone = CCassBlobWaiter::eDone,
        eError = CCassBlobWaiter::eError
    };

 public:
    CCassBioseqInfoTaskFetch(unsigned int timeout_ms,
                             unsigned int max_retries,
                             shared_ptr<CCassConnection> connection,
                             const string & keyspace,
                             CBioseqInfoFetchRequest const& request,
                             TBioseqInfoConsumeCallback consume_callback,
                             TDataErrorCallback data_error_cb);

    void SetDataReadyCB(TDataReadyCallback callback, void * data);
    void SetDataReadyCB(shared_ptr<CCassDataCallbackReceiver> callback);
    void SetConsumeCallback(TBioseqInfoConsumeCallback callback);
    void AllowInheritance(bool value);

 protected:
    virtual void Wait1(void) override;

 private:
    void x_InitializeQuery(void);
    void x_InitializeAliveRecordQuery(void);
    void x_PopulateRecord(CBioseqInfoRecord& record) const;
    bool x_IsMatchingRecord(void);
    bool x_IsMatchingNewRecord(CBioseqInfoRecord const& old_record, CBioseqInfoRecord const& new_record);
    void x_ReadingLoop(void);
    void x_StartQuery(void);
    void x_MergeSeqIds(CBioseqInfoRecord& target, CBioseqInfoRecord const& source);
    bool x_InheritanceRequired();

 private:
    CBioseqInfoFetchRequest m_Request;
    string m_Accession;
    TBioseqInfoConsumeCallback m_ConsumeCallback;
    vector<CBioseqInfoRecord> m_Records;
    set<size_t> m_InheritanceRequired;
    bool m_InheritanceAllowed;

 protected:
    unsigned int m_PageSize;
    unsigned int m_RestartCounter;
};

END_IDBLOB_SCOPE

#endif  // OBJTOOLS__PUBSEQ_GATEWAY__CASSANDRA__BIOSEQ_INFO_TASK__FETCH_HPP_
