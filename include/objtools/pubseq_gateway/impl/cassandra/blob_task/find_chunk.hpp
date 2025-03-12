#ifndef OBJTOOLS__PUBSEQ_GATEWAY__CASSANDRA__BLOB_TASK__FIND_CHUNK_HPP_
#define OBJTOOLS__PUBSEQ_GATEWAY__CASSANDRA__BLOB_TASK__FIND_CHUNK_HPP_

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
 * Cassandra insert blob according to extended schema task.
 *
 */

#include <objtools/pubseq_gateway/impl/cassandra/cass_blob_op.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/IdCassScope.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/blob_record.hpp>

#include <functional>
#include <memory>
#include <vector>

BEGIN_IDBLOB_SCOPE
USING_NCBI_SCOPE;

class CCassBlobTaskFindChunk
    : public CCassBlobWaiter
{
    enum EBlobInserterState {
        eInit = 0,
        eWaitingForID2ChunkPacked,
        eDone = CCassBlobWaiter::eDone,
        eError = CCassBlobWaiter::eError
    };

public:
    using TFindID2ChunkIDCallback = function< void(bool& found, int& chunk_id, bool& packed)>;
    CCassBlobTaskFindChunk(
        shared_ptr<CCassConnection> conn,
        const string & keyspace,
        CBlobRecord::TSatKey id2_ent,
        CBlobRecord::TSatKey need_old,
        TDataErrorCallback data_error_cb
    );

    ~CCassBlobTaskFindChunk() override
    {
        for (auto & it : m_QueryArr) {
            if (it.query) {
                it.query->Close();
            }
        }
        m_QueryArr.clear();
    }

    void SetDataReadyCB(shared_ptr<CCassDataCallbackReceiver> callback);
    void SetFindID2ChunkIDCallback( TFindID2ChunkIDCallback callback);

 protected:
    void Wait1() override;

 private:
    void x_IsID2ChunkPacked_Query();
    bool x_IsID2ChunkPacked_Wait();

    TFindID2ChunkIDCallback m_FindID2ChunkIDCallback{nullptr};

    CBlobRecord::TSatKey m_id2_ent;
    CBlobRecord::TSatKey m_need_old;
};

END_IDBLOB_SCOPE

#endif  // OBJTOOLS__PUBSEQ_GATEWAY__CASSANDRA__BLOB_TASK__FIND_CHUNK_HPP_
