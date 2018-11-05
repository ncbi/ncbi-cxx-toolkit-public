#ifndef OBJTOOLS_DATA_LOADERS_PSG___PSG_LOADER_IMPL__HPP
#define OBJTOOLS_DATA_LOADERS_PSG___PSG_LOADER_IMPL__HPP

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
 * Author: Eugene Vasilchenko, Aleksey Grichenko
 *
 * File Description: PSG data loader
 *
 * ===========================================================================
 */

#include <corelib/ncbistd.hpp>
#include <objtools/pubseq_gateway/client/psg_client.hpp>
#include <objtools/data_loaders/psg/psg_loader.hpp>
#include <memory>
#include <vector>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


class CPsgBlobId : public CBlobId
{
public:
    // Create blob-id from a string including blob type.
    explicit CPsgBlobId(CTempString str);
    ~CPsgBlobId(void);

    string ToString(void) const override;

    bool operator<(const CBlobId& id) const override;
    bool operator==(const CBlobId& id) const override;

private:
    string m_Id;
};


struct SPsgBioseqInfo
{
    SPsgBioseqInfo(const CPSG_BioseqInfo& bioseq_info);

    typedef vector<CSeq_id_Handle> TIds;
    CSeq_inst::TMol molecule_type;
    Uint8 length;
    CPSG_BioseqInfo::TBioseqState state;
    TTaxId tax_id;
    int hash;
    TIds ids;
    string blob_id;

private:
    SPsgBioseqInfo(const SPsgBioseqInfo&);
    SPsgBioseqInfo& operator=(const SPsgBioseqInfo&);
};


struct SPsgBlobInfo
{
    SPsgBlobInfo(const CPSG_BlobInfo& blob_info);

    typedef int TChunkId;
    typedef vector<string> TChunks;

    string blob_id_main;
    string blob_id_split;
    TChunks chunks;

    const string& GetBlobIdForChunk(TChunkId chunk_id) const;

    const string& GetDataBlobId() const { return IsSplit() ? blob_id_split : blob_id_main; }
    bool IsSplit() const { return !blob_id_split.empty(); }

private:
    SPsgBlobInfo(const SPsgBlobInfo&);
    SPsgBlobInfo& operator=(const SPsgBlobInfo&);
};


class CPsgClientThread;
class CBioseqCache;


class CPSGDataLoader_Impl : public CObject
{
public:
    explicit CPSGDataLoader_Impl(const SPSGLoaderParams& params);
    ~CPSGDataLoader_Impl(void);

    typedef vector<CSeq_id_Handle> TIds;

    void GetIds(const CSeq_id_Handle& idh, TIds& ids);
    int GetTaxId(const CSeq_id_Handle& idh);
    TSeqPos GetSequenceLength(const CSeq_id_Handle& idh);
    CDataLoader::SHashFound GetSequenceHash(const CSeq_id_Handle& idh);
    CDataLoader::STypeFound GetSequenceType(const CSeq_id_Handle& idh);

    CDataLoader::TTSE_LockSet GetRecords(CDataSource* data_source,
                                         const CSeq_id_Handle& idh,
                                         CDataLoader::EChoice choice);
    CRef<CPsgBlobId> GetBlobId(const CSeq_id_Handle& idh);
    CTSE_LoadLock GetBlobById(CDataSource* data_source,
                              const CPsgBlobId& blob_id);
    void LoadChunk(const CPsgBlobId& blob_id, CTSE_Chunk_Info& chunk_info);

    void DropTSE(const CPsgBlobId& blob_id);

private:
    struct SReplyResult {
        CTSE_LoadLock lock;
        shared_ptr<CPSG_BioseqInfo> bioseq_info;
        string blob_id;
        shared_ptr<SPsgBlobInfo> blob_info;
    };

    CPSG_BioId x_GetBioId(const CSeq_id_Handle& idh);
    shared_ptr<CPSG_Reply> x_ProcessRequest(shared_ptr<CPSG_Request> request);
    SReplyResult x_ProcessReply(shared_ptr<CPSG_Reply> reply, CDataSource* data_source);
    shared_ptr<SPsgBioseqInfo> x_GetBioseqInfo(const CSeq_id_Handle& idh);
    shared_ptr<SPsgBlobInfo> x_FindBlob(const string& bid);
    void x_AddBlob(const string& bid, shared_ptr<SPsgBlobInfo> blob);
    CTSE_LoadLock x_LoadBlob(const SPsgBlobInfo& psg_blob_info, CDataSource& data_source);
    void x_GetBlobInfoAndData(
        const string& psg_blob_id,
        shared_ptr<CPSG_BlobInfo>& blob_info,
        shared_ptr<CPSG_BlobData>& blob_data);
    void x_ReadBlobData(
        const SPsgBlobInfo& psg_blob_info,
        const CPSG_BlobInfo& blob_info,
        const CPSG_BlobData& blob_data,
        CTSE_LoadLock& load_lock);
    CObjectIStream* x_GetBlobDataStream(const CPSG_BlobInfo& blob_info, const CPSG_BlobData& blob_data);

    template<class TReply> bool x_CheckStatus(TReply reply) const
    {
        if (!reply) {
            ERR_POST("Request failed: null reply");
            return false;
        }
        EPSG_Status status = reply->GetStatus(CDeadline::eInfinite);
        if (status == EPSG_Status::eSuccess) return true;
        ERR_POST("Request failed: " << (int)status << " - " << reply->GetNextMessage());
        return false;
    }

    // Map seq-id to bioseq info.
    typedef map<CSeq_id_Handle, shared_ptr<SPsgBioseqInfo> > TBioseqCache;
    // Map blob-id to blob info
    typedef map<string, shared_ptr<SPsgBlobInfo> > TBlobs;

    CFastMutex m_Mutex;
    bool m_NoSplit = false;
    shared_ptr<CPSG_Queue> m_Queue;
    CRef<CPsgClientThread> m_Thread;
    TBlobs m_Blobs;
    unique_ptr<CBioseqCache> m_BioseqCache;
};


END_SCOPE(objects)
END_NCBI_SCOPE

#endif  // OBJTOOLS_DATA_LOADERS_PSG___PSG_LOADER_IMPL__HPP
