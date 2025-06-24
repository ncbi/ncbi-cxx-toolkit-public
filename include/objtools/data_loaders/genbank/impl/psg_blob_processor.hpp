#ifndef OBJTOOLS_DATA_LOADERS_PSG___PSG_BLOB_PROCESSOR__HPP
#define OBJTOOLS_DATA_LOADERS_PSG___PSG_BLOB_PROCESSOR__HPP

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
 * File Description: PSG reply processors
 *
 * ===========================================================================
 */

#include <corelib/ncbistd.hpp>
#include <objmgr/impl/tse_lock.hpp>
#include <objtools/data_loaders/genbank/impl/psg_processor.hpp>

#if defined(HAVE_PSG_LOADER)

BEGIN_NCBI_NAMESPACE;

class CObjectIStream;

BEGIN_NAMESPACE(objects);

class CTSE_Lock;
class CDataSource;
class CTSE_Chunk_Info;
class CPsgBlobId;
class CID2S_Chunk;

BEGIN_NAMESPACE(psgl);

struct SPsgBioseqInfo;
struct SPsgBlobInfo;
class CPSGCaches;
class CPSGL_Blob_Processor;


/////////////////////////////////////////////////////////////////////////////
// CPSGL_Blob_Processor
/////////////////////////////////////////////////////////////////////////////


class CPSGL_Blob_Processor : public CPSGL_Processor
{
public:
    explicit
    CPSGL_Blob_Processor(CDataSource* data_source,
                         CPSGCaches* caches = nullptr,
                         bool add_wgs_master = false);
    ~CPSGL_Blob_Processor() override;

    EProcessResult ProcessItemFast(EPSG_Status status,
                                   const shared_ptr<CPSG_ReplyItem>& item) override;
    EProcessResult ProcessItemSlow(EPSG_Status status,
                                   const shared_ptr<CPSG_ReplyItem>& item) override;

    typedef int TChunkId;
    static const TChunkId kSplitInfoChunkId = 999999999;
    
    void SetLockedDelayedChunkInfo(const string& psg_blob_id, CTSE_Chunk_Info& locked_chunk_info);
    void SetLockedChunkInfo(CTSE_Chunk_Info& locked_chunk_info);
    pair<CTSE_Chunk_Info*, CRef<CID2S_Chunk>> GetNextLoadedChunk();

    bool GotForbidden() const
    {
        return m_GotForbidden;
    }
    bool GotUnauthorized() const
    {
        return m_GotUnauthorized;
    }
    int GetBlobInfoState(const string& psg_blob_id);
    
protected:
    enum EWaitForLock {
        eNoWaitForLock,
        eWaitForLock
    };
    // for calling from ProcessReply()
    // may return eRequestedBackgroundProcessing with eNoWaitForLock
    EProcessResult ProcessTSE_Lock(const string& blob_id,
                                   CTSE_Lock& tse_lock,
                                   EWaitForLock wait_for_lock = eNoWaitForLock);
    
    struct SBlobSlot;
    struct STSESlot;
    struct SSplitSlot;
    struct SChunkSlot;
    typedef map<string, STSESlot> TTSEBlobMap; // by PSG blob_id
    typedef map<string, SSplitSlot> TSplitBlobMap; // by PSG id2_info

    // separate blob and chunk ids
    static tuple<const CPSG_BlobId*, const CPSG_ChunkId*> ParseId(const CPSG_DataId* id);
    
    // Set*Slot() methods always return non-null slot pointer, except SetBlobSlot(null, null)
    // Get*Slot() methods may return null if the slot was not created before
    STSESlot* GetTSESlot(const string& blob_id);
    STSESlot* SetTSESlot(const string& blob_id);
    STSESlot* GetTSESlot(const CPSG_BlobId& blob_id);
    STSESlot* SetTSESlot(const CPSG_BlobId& blob_id);
    SSplitSlot* GetSplitSlot(const string& id2_info);
    SSplitSlot* SetSplitSlot(const string& id2_info);
    SBlobSlot* GetChunkSlot(const string& id2_info, TChunkId chunk_id);
    SBlobSlot* SetChunkSlot(const string& id2_info, TChunkId chunk_id);
    SBlobSlot* GetChunkSlot(const CPSG_ChunkId& chunk_id);
    SBlobSlot* SetChunkSlot(const CPSG_ChunkId& chunk_id);
    
    // GetBlobSlot() and SetBlobSlot() may return null if all arguments are null
    // or if the slot was not created before
    SBlobSlot* GetBlobSlot(const CPSG_BlobId* blob_id, const CPSG_ChunkId* chunk_id);
    SBlobSlot* SetBlobSlot(const CPSG_BlobId* blob_id, const CPSG_ChunkId* chunk_id);
    
    unique_ptr<CDeadline> GetWaitDeadline(const CPSG_SkippedBlob& skipped) const;
    static const char* GetSkippedType(const CPSG_SkippedBlob& skipped);

    EProcessResult PreProcess(EPSG_Status status, const shared_ptr<CPSG_ReplyItem>& item); // fast part
    EProcessResult PostProcess(EPSG_Status status, const shared_ptr<CPSG_ReplyItem>& item); // slow part

    EProcessResult PostProcessBlob(const CPSG_DataId* id);
    EProcessResult PostProcessSkippedBlob(const CPSG_DataId* id);
    
    bool ParseTSE(const CPSG_BlobId* blob_id, SBlobSlot* data_slot);
    bool ParseSplitInfo(const CPSG_ChunkId* split_info_id, SBlobSlot* data_slot);
    bool ParseChunk(const CPSG_ChunkId* chunk_id, SBlobSlot* data_slot);
    
    EProcessResult TSE_ToOM(const CPSG_BlobId* blob_id,
                            const CPSG_ChunkId* split_info_id,
                            SBlobSlot* data_slot);
    EProcessResult Chunk_ToOM(const CPSG_ChunkId* chunk_id,
                              SChunkSlot* chunk_slot);

    bool HasChunksToAssign(const CTSE_Lock& tse);
    EProcessResult AssignChunks(STSESlot* tse_slot);

    void SetDLBlobId(const string& psg_blob_id, const CConstRef<CPsgBlobId>& dl_blob_id);
    CConstRef<CPsgBlobId> GetDLBlobId(STSESlot* tse_slot);
    
    virtual CConstRef<CPsgBlobId> CreateDLBlobId(STSESlot* tse_slot);
    
    // Call ObtainSkippedTSE_Lock() with mutex locked
    // return eFailed if TSE lock was not obtained
    // otherwise the lock was obtained and:
    //   return eRequestedBackgroundProcessing if there are chunks ready to be assigned
    //   return eProcessed if there are no chunks to be assigned
    EProcessResult ObtainSkippedTSE_Lock(STSESlot* slot,
                                         EWaitForLock wait_for_lock = eNoWaitForLock);
    
protected:
    CFastMutex m_BlobProcessorMutex;
    
    // processing data
    TTSEBlobMap m_TSEBlobMap;
    TSplitBlobMap m_SplitBlobMap;
    bool m_GotForbidden = false;
    bool m_GotUnauthorized = false;
    
    // cache pointers and other params
    bool m_AddWGSMasterDescr;
    CDataSource* m_DataSource; // OM data source to get TSE locks from
    CPSGCaches* m_Caches; // cache for blob info
};


END_NAMESPACE(psgl);
END_NAMESPACE(objects);
END_NCBI_NAMESPACE;

#endif // HAVE_PSG_LOADER

#endif  // OBJTOOLS_DATA_LOADERS_PSG___PSG_BLOB_PROCESSOR__HPP
