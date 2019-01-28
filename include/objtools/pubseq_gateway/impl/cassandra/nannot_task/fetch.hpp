#ifndef OBJTOOLS__PUBSEQ_GATEWAY__CASSANDRA__BLOB_TASK__FETCH_HPP_
#define OBJTOOLS__PUBSEQ_GATEWAY__CASSANDRA__BLOB_TASK__FETCH_HPP_

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
 * Cassandra insert blob according to extended schema task.
 *
 */

#include <functional>
#include <string>
#include <memory>
#include <vector>

#include <corelib/ncbistr.hpp>

#include <objtools/pubseq_gateway/impl/cassandra/cass_blob_op.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/IdCassScope.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/nannot/record.hpp>

BEGIN_IDBLOB_SCOPE
USING_NCBI_SCOPE;

class CCassNAnnotTaskFetch
    : public CCassBlobWaiter
{
    union UAnnotNameBox {
        UAnnotNameBox(const vector<string>& value)
            : names(value)
        {}
        UAnnotNameBox(const vector<CTempString>& value)
            : names_temp(value)
        {}
        ~UAnnotNameBox() {}
        vector<string> names;
        vector<CTempString> names_temp;
    };
    enum EBlobFetchState {
        eInit = 0,
        eFetchStarted,
        eDone = CCassBlobWaiter::eDone,
        eError = CCassBlobWaiter::eError
    };

 public:
    CCassNAnnotTaskFetch(
        unsigned int timeout_ms,
        shared_ptr<CCassConnection> connection,
        const string & keyspace,
        string accession,
        int16_t version,
        int16_t seq_id_type,
        const vector<string> & annot_names,
        TNAnnotConsumeCallback consume_callback,
        TDataErrorCallback data_error_cb
    );

    CCassNAnnotTaskFetch(
        unsigned int timeout_ms,
        shared_ptr<CCassConnection> connection,
        const string & keyspace,
        string accession,
        int16_t version,
        int16_t seq_id_type,
        const vector<CTempString> & annot_names,
        TNAnnotConsumeCallback consume_callback,
        TDataErrorCallback data_error_cb
    );

    CCassNAnnotTaskFetch(
        unsigned int timeout_ms,
        shared_ptr<CCassConnection> connection,
        const string & keyspace,
        string accession,
        int16_t version,
        int16_t seq_id_type,
        TNAnnotConsumeCallback consume_callback,
        TDataErrorCallback data_error_cb
    );

    virtual ~CCassNAnnotTaskFetch();

    void SetConsumeCallback(TNAnnotConsumeCallback callback);
    void Cancel(void);

 protected:
    virtual void Wait1(void) override;

 private:
    string m_Accession;
    int16_t m_Version;
    int16_t m_SeqIdType;
    UAnnotNameBox m_AnnotNameBox;
    bool m_AnnotNameTempStrings;
    TNAnnotConsumeCallback m_Consume;
};

END_IDBLOB_SCOPE

#endif  // OBJTOOLS__PUBSEQ_GATEWAY__CASSANDRA__BLOB_TASK__FETCH_HPP_
