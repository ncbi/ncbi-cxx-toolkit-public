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
 * Authors: Evgueni Belyi, based on Dmitrii Saprykin's code
 *
 * File Description:
 *
 * Cassandra load blob properties according to extended schema.
 *
 */

#include <ncbi_pch.hpp>

#include <objtools/pubseq_gateway/impl/cassandra/blob_task/find_chunk.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/cass_blob_op.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/cass_driver.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/IdCassScope.hpp>

#include <vector>
#include <memory>
#include <utility>
#include <string>

#include <unistd.h>

BEGIN_IDBLOB_SCOPE
USING_NCBI_SCOPE;

//:::::::
CCassBlobTaskFindChunk::CCassBlobTaskFindChunk(
    shared_ptr<CCassConnection> conn,
    const string & keyspace,
    CBlobRecord::TSatKey id2_ent,
    CBlobRecord::TSatKey need_old,
    TDataErrorCallback data_error_cb
)
    : CCassBlobWaiter(std::move(conn), keyspace, 0, true, std::move(data_error_cb))
{
    m_id2_ent  = id2_ent;
    m_need_old = need_old;
}

void CCassBlobTaskFindChunk::SetFindID2ChunkIDCallback( TFindID2ChunkIDCallback callback)
{
    m_FindID2ChunkIDCallback = std::move(callback);
}

void CCassBlobTaskFindChunk::SetDataReadyCB(shared_ptr<CCassDataCallbackReceiver> callback)
{
    if (callback && m_State != eInit) {
        NCBI_THROW(CCassandraException, eSeqFailed,
           "CCassBlobTaskFindChunk: DataReadyCB can't be assigned "
           "after the loading process has started");
    }
    CCassBlobWaiter::SetDataReadyCB3(std::move(callback));
}

void CCassBlobTaskFindChunk::Wait1()
{
    bool b_need_repeat{false};
    do
    {
        b_need_repeat = false;
        switch (m_State)
        {
        case eError:
        case eDone:
            return;

        case eInit:
        {
           x_IsID2ChunkPacked_Query();
           m_State = eWaitingForID2ChunkPacked;
           break;
        }
            
        case eWaitingForID2ChunkPacked:
        {
            if( x_IsID2ChunkPacked_Wait() ) { m_State = eDone; }
            break;
        }
            
        default:
        {
            char msg[1024];
            string keyspace = GetKeySpace();
            snprintf( msg, sizeof(msg),
                      "Failed to fetch blob (key=%s.%d) unexpected state (%d)",
                      keyspace.c_str(), GetKey(), static_cast<int>(m_State));
            Error(CRequestStatus::e502_BadGateway,
                  CCassandraException::eQueryFailed,
                  eDiag_Error, msg);
        }
        }
    } while( b_need_repeat);
}

//:::::::
void CCassBlobTaskFindChunk::x_IsID2ChunkPacked_Query()
{
    CloseAll();
    m_QueryArr.clear();
    m_QueryArr.push_back({ProduceQuery(), 0});
    auto qry = m_QueryArr[0].query;
    string sql = "SELECT size, size_unpacked FROM " + GetKeySpace() + ".blob_prop WHERE sat_key = ?";
    qry->SetSQL( sql, 1);
    qry->BindInt32( 0, m_id2_ent);
    SetupQueryCB3( qry);
    qry->Query(GetReadConsistency(), m_Async, false);
}

//:::::::
bool CCassBlobTaskFindChunk::x_IsID2ChunkPacked_Wait()
{
    auto& it = m_QueryArr[0];
    if( !CheckReady(it)) return false;

    bool Found = false;
    bool Packed = false;
    
    while( 1)
    {
        it.query->NextRow();

        if( it.query->IsEOF()) break;
        int64_t size          = it.query->FieldGetInt64Value(0);
        int64_t size_unpacked = it.query->FieldGetInt64Value(1);
        Packed = (size < size_unpacked);
        Found = true;
    }
    
    it.query->Close();
    CloseAll();
    m_QueryArr.clear();

    if (m_FindID2ChunkIDCallback) {
        m_FindID2ChunkIDCallback(Found, m_id2_ent, Packed);
    }

    return true;
}

END_IDBLOB_SCOPE
