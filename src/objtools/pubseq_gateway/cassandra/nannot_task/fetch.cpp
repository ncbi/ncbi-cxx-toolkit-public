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
 * Cassandra fetch named annot record
 *
 */

#include <ncbi_pch.hpp>

#include <objtools/pubseq_gateway/impl/cassandra/nannot_task/fetch.hpp>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <objtools/pubseq_gateway/impl/cassandra/cass_blob_op.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/cass_driver.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/IdCassScope.hpp>

BEGIN_IDBLOB_SCOPE
USING_NCBI_SCOPE;

CCassNAnnotTaskFetch::CCassNAnnotTaskFetch(
    unsigned int timeout_ms,
    shared_ptr<CCassConnection> connection,
    const string & keyspace,
    string accession,
    int16_t version,
    int16_t seq_id_type,
    const vector<string> & annot_names,
    TNAnnotConsumeCallback consume_callback,
    TDataErrorCallback data_error_cb
)
    : CCassBlobWaiter(
        timeout_ms, connection, keyspace,
        0, true, 0, move(data_error_cb)
    )
    , m_Accession(move(accession))
    , m_Version(version)
    , m_SeqIdType(seq_id_type)
    , m_AnnotNameBox(annot_names)
    , m_AnnotNameTempStrings(false)
    , m_Consume(move(consume_callback))
{}

CCassNAnnotTaskFetch::CCassNAnnotTaskFetch(
    unsigned int timeout_ms,
    shared_ptr<CCassConnection> connection,
    const string & keyspace,
    string accession,
    int16_t version,
    int16_t seq_id_type,
    const vector<CTempString> & annot_names,
    TNAnnotConsumeCallback consume_callback,
    TDataErrorCallback data_error_cb
)
    : CCassBlobWaiter(
        timeout_ms, connection, keyspace,
        0, true, 0, move(data_error_cb)
    )
    , m_Accession(move(accession))
    , m_Version(version)
    , m_SeqIdType(seq_id_type)
    , m_AnnotNameBox(annot_names)
    , m_AnnotNameTempStrings(true)
    , m_Consume(move(consume_callback))
{}

CCassNAnnotTaskFetch::CCassNAnnotTaskFetch(
    unsigned int timeout_ms,
    shared_ptr<CCassConnection> connection,
    const string & keyspace,
    string accession,
    int16_t version,
    int16_t seq_id_type,
    TNAnnotConsumeCallback consume_callback,
    TDataErrorCallback data_error_cb
)
    : CCassBlobWaiter(
        timeout_ms, connection, keyspace,
        0, true, 0, move(data_error_cb)
    )
    , m_Accession(move(accession))
    , m_Version(version)
    , m_SeqIdType(seq_id_type)
    , m_AnnotNameBox(vector<string>())
    , m_AnnotNameTempStrings(false)
    , m_Consume(move(consume_callback))
{}

CCassNAnnotTaskFetch::~CCassNAnnotTaskFetch()
{
    if (m_AnnotNameTempStrings) {
        m_AnnotNameBox.names_temp.~vector<CTempString>();
    } else {
        m_AnnotNameBox.names.~vector<string>();
    }
}

void CCassNAnnotTaskFetch::Wait1()
{
    switch (m_State) {
        case eError:
        case eDone:
            return;

        case eInit: {
            m_QueryArr.resize(1);
            m_QueryArr[0] = m_Conn->NewQuery();
            string sql =
                " SELECT "
                "  annot_name, sat_key, last_modified, start, stop "
                " FROM" + GetKeySpace() + ".bioseq_na "
                " WHERE"
                "  accession = ? AND version = ? AND seq_id_type = ?";
            unsigned int params = 3;
            unsigned int names_count = 0;
            if (m_AnnotNameTempStrings && !m_AnnotNameBox.names_temp.empty()) {
                names_count = m_AnnotNameBox.names_temp.size();
            } else if (!m_AnnotNameTempStrings && !m_AnnotNameBox.names.empty()) {
                names_count = m_AnnotNameBox.names.size();
            }
            if (names_count > 0) {
                sql += "AND annot_name in (" + NStr::Join(vector<string>(names_count, "?"), ",") + ")";
            }
            m_QueryArr[0]->SetSQL(sql, params + names_count);
            m_QueryArr[0]->BindStr(0, m_Accession);
            m_QueryArr[0]->BindInt16(1, m_Version);
            m_QueryArr[0]->BindInt16(2, m_SeqIdType);
            for (unsigned int i = params; i < names_count + params; ++i) {
                if (m_AnnotNameTempStrings) {
                    m_QueryArr[0]->BindStr(i, m_AnnotNameBox.names_temp[i-params]);
                } else {
                    m_QueryArr[0]->BindStr(i, m_AnnotNameBox.names[i-params]);
                }
            }
            UpdateLastActivity();
            m_QueryArr[0]->Execute(CASS_CONSISTENCY_LOCAL_QUORUM, m_Async);
            m_State = eFetchStarted;
            break;
        }

        case eFetchStarted: {
            if (m_QueryArr[0]->IsReady()) {
                bool do_next = true;
                auto state = m_QueryArr[0]->NextRow();
                while (do_next && state == ar_dataready) {
                    CNAnnotRecord record;
                    record
                        .SetAccession(m_Accession)
                        .SetVersion(m_Version)
                        .SetSeqIdType(m_SeqIdType)
                        .SetAnnotName(m_QueryArr[0]->FieldGetStrValueDef(0, ""))
                        .SetSatKey(m_QueryArr[0]->FieldGetInt32Value(1, 0))
                        .SetModified(m_QueryArr[0]->FieldGetInt64Value(2, 0))
                        .SetStart(m_QueryArr[0]->FieldGetInt32Value(3, 0))
                        .SetStop(m_QueryArr[0]->FieldGetInt32Value(4, 0));
                    do_next = m_Consume(move(record), false);
                    if (do_next) {
                        state = m_QueryArr[0]->NextRow();
                    }
                }
                if (!do_next || m_QueryArr[0]->IsEOF()) {
                    CloseAll();
                    m_State = eDone;
                }
                m_Consume(CNAnnotRecord(), true);
            }
            break;
        }

        default: {
            char msg[1024];
            snprintf(msg, sizeof(msg), "Failed to fetch named annot (key=%s.%s|%hd|%hd) unexpected state (%d)",
                m_Keyspace.c_str(), m_Accession.c_str(), m_Version, m_SeqIdType, static_cast<int>(m_State));
            Error(CRequestStatus::e502_BadGateway, CCassandraException::eQueryFailed, eDiag_Error, msg);
        }
    }
}

END_IDBLOB_SCOPE
