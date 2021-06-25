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
 * Authors: Evgueni Belyi based on Dmitrii Saprykin's code
 *
 * File Description:
 *
 * Cassandra fetch named annot record
 *
 */

#include <ncbi_pch.hpp>

#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

//#include <objtools/pubseq_gateway/impl/cassandra/acc_ver_hist/record.hpp>
//#include <objtools/pubseq_gateway/impl/cassandra/cass_blob_op.hpp>
//#include <objtools/pubseq_gateway/impl/cassandra/cass_driver.hpp>
//#include <objtools/pubseq_gateway/impl/cassandra/IdCassScope.hpp>

#include <objtools/pubseq_gateway/impl/cassandra/acc_ver_hist/tasks.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/cass_driver.hpp>

BEGIN_IDBLOB_SCOPE
USING_NCBI_SCOPE;

CCassAccVerHistoryTaskFetch::CCassAccVerHistoryTaskFetch(
    unsigned int timeout_ms,
    unsigned int max_retries,
    shared_ptr<CCassConnection> connection,
    const string & keyspace,
    string accession,
    TAccVerHistConsumeCallback consume_callback,
    TDataErrorCallback data_error_cb,
    int16_t version,
    int16_t seq_id_type
)
    : CCassBlobWaiter(
        timeout_ms, connection, keyspace,
        0, true, max_retries, move(data_error_cb)
    )
    , m_Accession( move( accession))
    , m_Version( version)
    , m_SeqIdType( seq_id_type)
    , m_Consume( move( consume_callback))
    , m_PageSize( CCassQuery::DEFAULT_PAGE_SIZE)
    , m_RestartCounter( 0)
{}

void CCassAccVerHistoryTaskFetch::SetConsumeCallback( TAccVerHistConsumeCallback callback)
{
    m_Consume = move(callback);
}

void CCassAccVerHistoryTaskFetch::SetDataReadyCB(
    shared_ptr<CCassDataCallbackReceiver> callback)
{
    if( callback && m_State != eInit)
    {
        NCBI_THROW(CCassandraException, eSeqFailed,
           "CCassAccVerHistoryTaskFetch: DataReadyCB can't be assigned "
           "after the loading process has started");
    }
    CCassBlobWaiter::SetDataReadyCB3(callback);
}

void CCassAccVerHistoryTaskFetch::Wait1()
{
    bool restarted = false;
    do
    {
        restarted = false;
        switch (m_State)
        {
        case eError:
        case eDone:
            return;

        case eInit:
        {
            m_QueryArr.resize(1);
            m_QueryArr[0] = { m_Conn->NewQuery(), 0};

            string sql =
                " SELECT"
                "     version, gi, date, chain, id_type, sat, sat_key"
                " FROM " + GetKeySpace() + ".acc_ver_hist "
                " WHERE"
                "     accession = ? ";
            unsigned int params = 1;
            if( m_Version > 0)
            {
                sql += " AND version = ?";
                ++params;
            }
            if( m_SeqIdType > 0)
            {
                sql += " AND id_type = ?";
                ++params;
            }
            
            m_QueryArr[0].query->SetSQL(sql, params);
            m_QueryArr[0].query->BindStr(0, m_Accession);
            
            unsigned int param = 1;
            if( m_Version > 0)
            {
                m_QueryArr[0].query->BindInt16( param, m_Version); param++;
            }
            if( m_SeqIdType > 0)
            {
                m_QueryArr[0].query->BindInt16( param, m_SeqIdType);
            }

            SetupQueryCB3(m_QueryArr[0].query);
            UpdateLastActivity();
            m_QueryArr[0].query->Query( CASS_CONSISTENCY_LOCAL_QUORUM,
                                        m_Async, true, m_PageSize);
            restarted = true;
            m_State = eFetchStarted;
            break;
        }

        case eFetchStarted:
        {
            if( CheckReady( m_QueryArr[0].query, m_RestartCounter, restarted))
            {
                bool do_next = true;
                auto state = m_QueryArr[0].query->NextRow();
                while( do_next && state == ar_dataready)
                {
                    SAccVerHistRec record;
                    record.accession = m_Accession;
                    record.version   = m_QueryArr[0].query->FieldGetInt16Value( 0, 0);
                    record.gi        = m_QueryArr[0].query->FieldGetInt64Value( 1, 0);
                    record.date      = m_QueryArr[0].query->FieldGetInt64Value( 2, 0);
                    record.chain     = m_QueryArr[0].query->FieldGetInt64Value( 3, 0);
                    record.seq_id_type = m_QueryArr[0].query->FieldGetInt16Value( 4, 0);
                    record.sat       = m_QueryArr[0].query->FieldGetInt16Value( 5, 0);
                    record.sat_key   = m_QueryArr[0].query->FieldGetInt32Value( 6, 0);

                    if( m_Consume)
                    {
                        do_next = m_Consume( move( record), false);
                    }
                    if( do_next)
                    {
                        state = m_QueryArr[0].query->NextRow();
                    }
                }
                if( !do_next || m_QueryArr[0].query->IsEOF())
                {
                    if( m_Consume)
                    {
                        m_Consume( SAccVerHistRec(), true);
                    }
                    CloseAll();
                    m_State = eDone;
                }
            }
            else if( restarted)
            {
                ++m_RestartCounter;
                m_QueryArr[0].query->Close();
                m_State = eInit;
            }
            UpdateLastActivity();
            break;
        }
        
        default:
        {
            char msg[1024];
            snprintf(msg, sizeof(msg),
                "Failed to fetch accession history (key=%s.%s|%hd|%hd) unexpected state (%d)",
                     m_Keyspace.c_str(), m_Accession.c_str(), m_Version, m_SeqIdType,
                     static_cast<int>(m_State));
            Error( CRequestStatus::e502_BadGateway,
                   CCassandraException::eQueryFailed, eDiag_Error, msg);
        }
        }
    } while( restarted);
}

END_IDBLOB_SCOPE
