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
#include <objtools/data_loaders/genbank/psg_loader.hpp>
#include <objtools/data_loaders/genbank/blob_id.hpp>
#include <memory>
#include <vector>

#if defined(HAVE_PSG_LOADER)

BEGIN_NCBI_SCOPE

class CThreadPool;

BEGIN_SCOPE(objects)

class CBlobId;

class CPsgBlobId : public CBlobId
{
public:
    explicit CPsgBlobId(const string& id);
    virtual ~CPsgBlobId();
    
    const string& ToPsgId() const
        {
            return m_Id;
        }
    
    virtual string ToString(void) const override;
    virtual bool operator<(const CBlobId& id) const override;
    virtual bool operator==(const CBlobId& id) const override;
    
private:
    string m_Id;
};

struct SPsgBioseqInfo
{
    SPsgBioseqInfo(const CPSG_BioseqInfo& bioseq_info);

    typedef vector<CSeq_id_Handle> TIds;
    CSeq_inst::TMol molecule_type;
    Uint8 length;
    CPSG_BioseqInfo::TState state;
    TTaxId tax_id;
    int hash;
    TGi gi;
    CSeq_id_Handle canonical;
    TIds ids;
    string blob_id;

    CDeadline deadline;

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
    int blob_state;
    int blob_version;
    int split_version;
    bool use_get_blob_for_chunks;
    TChunks chunks;

    const string& GetBlobIdForChunk(TChunkId chunk_id) const;

    const string& GetDataBlobId() const { return IsSplit() ? blob_id_split : blob_id_main; }
    bool IsSplit() const { return !blob_id_split.empty(); }

private:
    SPsgBlobInfo(const SPsgBlobInfo&);
    SPsgBlobInfo& operator=(const SPsgBlobInfo&);
};


class CPsgClientThread;
class CPSGBioseqCache;
class CPSGBlobMap;
class CPsgClientContext_Bulk;
class CPSG_Blob_Task;


class CPSGDataLoader_Impl : public CObject
{
public:
    explicit CPSGDataLoader_Impl(const CGBLoaderParams& params);
    ~CPSGDataLoader_Impl(void);

    typedef CDataLoader::TIds TIds;
    typedef CDataLoader::TGis TGis;
    typedef CDataLoader::TLoaded TLoaded;
    typedef CDataLoader::TTSE_LockSets TTSE_LockSets;

    void GetIds(const CSeq_id_Handle& idh, TIds& ids);
    TTaxId GetTaxId(const CSeq_id_Handle& idh);
    TSeqPos GetSequenceLength(const CSeq_id_Handle& idh);
    CDataLoader::SHashFound GetSequenceHash(const CSeq_id_Handle& idh);
    CDataLoader::STypeFound GetSequenceType(const CSeq_id_Handle& idh);
    int GetSequenceState(const CSeq_id_Handle& idh);

    CDataLoader::TTSE_LockSet GetRecords(CDataSource* data_source,
                                         const CSeq_id_Handle& idh,
                                         CDataLoader::EChoice choice);
    CRef<CPsgBlobId> GetBlobId(const CSeq_id_Handle& idh);
    CTSE_Lock GetBlobById(CDataSource* data_source,
                              const CPsgBlobId& blob_id);
    void LoadChunk(CTSE_Chunk_Info& chunk_info);
    void LoadChunks(const CDataLoader::TChunkSet& chunks);

    void GetBlobs(CDataSource* data_source, TTSE_LockSets& tse_sets);

    CDataLoader::TTSE_LockSet GetAnnotRecordsNA(CDataSource* data_source,
                                                const CSeq_id_Handle& idh,
                                                const SAnnotSelector* sel,
                                                CDataLoader::TProcessedNAs* processed_nas);

    void DropTSE(const CPsgBlobId& blob_id);

    void GetAccVers(const TIds& ids, TLoaded& loaded, TIds& ret);
    void GetGis(const TIds& ids, TLoaded& loaded, TGis& ret);

    static CObjectIStream* GetBlobDataStream(const CPSG_BlobInfo& blob_info, const CPSG_BlobData& blob_data);

    typedef vector<shared_ptr<SPsgBioseqInfo>> TBioseqInfos;

    struct SReplyResult {
        CTSE_Lock lock;
        string blob_id;
    };

private:
    friend class CPSG_Blob_Task;

    void x_SendRequest(shared_ptr<CPSG_Request> request);
    CPSG_BioId x_GetBioId(const CSeq_id_Handle& idh);
    shared_ptr<CPSG_Reply> x_ProcessRequest(shared_ptr<CPSG_Request> request);
    SReplyResult x_ProcessBlobReply(shared_ptr<CPSG_Reply> reply, CDataSource* data_source, CSeq_id_Handle req_idh, bool retry);
    SReplyResult x_RetryBlobRequest(const string& blob_id, CDataSource* data_source, CSeq_id_Handle req_idh);
    shared_ptr<SPsgBioseqInfo> x_GetBioseqInfo(const CSeq_id_Handle& idh);
    CTSE_Lock x_LoadBlob(const SPsgBlobInfo& psg_blob_info, CDataSource& data_source);
    void x_ReadBlobData(
        const SPsgBlobInfo& psg_blob_info,
        const CPSG_BlobInfo& blob_info,
        const CPSG_BlobData& blob_data,
        CTSE_LoadLock& load_lock);

    typedef map<void*, size_t> TIdxMap;

    void x_GetBulkBioseqInfo(CPSG_Request_Resolve::EIncludeInfo info,
        const TIds& ids,
        TLoaded& loaded,
        TBioseqInfos& ret);

    // Map seq-id to bioseq info.
    typedef map<CSeq_id_Handle, shared_ptr<SPsgBioseqInfo> > TBioseqCache;

    bool m_NoSplit = false;
    shared_ptr<CPSG_Queue> m_Queue;
    CRef<CPsgClientThread> m_Thread;
    unique_ptr<CPSGBlobMap> m_BlobMap;
    unique_ptr<CPSGBioseqCache> m_BioseqCache;
    unique_ptr<CThreadPool> m_ThreadPool;
};

END_SCOPE(objects)
END_NCBI_SCOPE

#endif // HAVE_PSG_LOADER

#endif  // OBJTOOLS_DATA_LOADERS_PSG___PSG_LOADER_IMPL__HPP
