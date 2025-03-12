#ifndef OBJTOOLS__PUBSEQ_GATEWAY__CASSANDRA__BLOB_TASK__LOAD_BLOB_PROPS_HPP_
#define OBJTOOLS__PUBSEQ_GATEWAY__CASSANDRA__BLOB_TASK__LOAD_BLOB_PROPS_HPP_

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

#include <objtools/pubseq_gateway/impl/cassandra/cass_blob_op.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/IdCassScope.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/blob_record.hpp>

#include <functional>
#include <memory>
#include <vector>

BEGIN_IDBLOB_SCOPE
USING_NCBI_SCOPE;

class CCassBlobTaskLoadBlob
    : public CCassBlobWaiter
{
    static constexpr size_t kMaxChunksAhead{4};
    enum EBlobInserterState {
        eInit = 0,
        eWaitingForPropsFetch,
        eFinishedPropsFetch,
        eBeforeLoadingChunks,
        eLoadingChunks,
        eDone = CCassBlobWaiter::eDone,
        eError = CCassBlobWaiter::eError
    };

public:

    enum EBlobTaskMode
    {
        eBlobTaskFindID2Chunk,
        eBlobTaskModeDefault
    };

    static constexpr CBlobRecord::TTimestamp kAnyModified{-1};

    using TBlobPropsCallback = function<void(CBlobRecord const & blob, bool isFound)>;
    using TBlobChunkCallbackEx = function<void(CBlobRecord const & blob, const unsigned char * data,
                                               unsigned int size, int chunk_no)>;

    static void InitBlobChunkDataQuery(CCassQuery* query, string const& keyspace, CBlobRecord const& blob, int32_t chunk_no);

    CCassBlobTaskLoadBlob(
        shared_ptr<CCassConnection> conn,
        const string & keyspace,
        CBlobRecord::TSatKey sat_key,
        bool load_chunks,
        TDataErrorCallback data_error_cb
    );

    CCassBlobTaskLoadBlob(
        shared_ptr<CCassConnection> conn,
        const string & keyspace,
        CBlobRecord::TSatKey sat_key,
        CBlobRecord::TTimestamp modified,
        bool load_chunks,
        TDataErrorCallback data_error_cb
    );

    CCassBlobTaskLoadBlob(
        shared_ptr<CCassConnection> conn,
        const string & keyspace,
        unique_ptr<CBlobRecord> blob_record,
        bool load_chunks,
        TDataErrorCallback data_error_cb
    );
  
    ~CCassBlobTaskLoadBlob() override
    {
        for (auto & it : m_QueryArr) {
            if (it.query) {
                it.query->Close();
            }
        }
        m_QueryArr.clear();
    }

    CBlobRecord::TSatKey GetSatKey() const
    {
        return m_Blob->GetKey();
    }

    CBlobRecord::TTimestamp GetModified() const
    {
        return m_Modified;
    }

    bool LoadChunks() const
    {
        return m_LoadChunks;
    }

    bool BlobPropsProvided() const
    {
        return m_ExplicitBlob;
    }

    unique_ptr<CBlobRecord> ConsumeBlobRecord();
    bool IsBlobPropsFound() const;
    void SetChunkCallback(TBlobChunkCallbackEx callback);
    void SetPropsCallback(TBlobPropsCallback callback);
    void SetDataReadyCB(shared_ptr<CCassDataCallbackReceiver> callback);

    // Required to test variable timeouts.
    // Prepared statements interfere with testing process.
    void SetUsePrepared(bool value)
    {
        m_UsePrepared = value;
    }

 protected:
    void Wait1() override;

 private:
    bool x_AreAllChunksProcessed() const;
    void x_CheckChunksFinished(bool& need_repeat);
    void x_RequestChunksAhead();
    void x_RequestChunk(CCassQuery& qry, int32_t chunk_no);

    TBlobChunkCallbackEx m_ChunkCallback{nullptr};
    TBlobPropsCallback m_PropsCallback{nullptr};
  
    unique_ptr<CBlobRecord> m_Blob;
    CBlobRecord::TTimestamp m_Modified{kAnyModified};
    bool m_LoadChunks{false};
    bool m_PropsFound{false};
    vector<bool> m_ProcessedChunks;
    size_t m_ActiveQueries{0};
    CBlobRecord::TSize m_RemainingSize{0};
    bool m_ExplicitBlob{false};
    bool m_UsePrepared{true};
    EBlobTaskMode m_Mode;
};

END_IDBLOB_SCOPE

#endif  // OBJTOOLS__PUBSEQ_GATEWAY__CASSANDRA__BLOB_TASK__LOAD_BLOB_PROPS_HPP_
