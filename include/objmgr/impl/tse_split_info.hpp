#ifndef OBJECTS_OBJMGR_IMPL___TSE_SPLIT_INFO__HPP
#define OBJECTS_OBJMGR_IMPL___TSE_SPLIT_INFO__HPP

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
* Author: Eugene Vasilchenko
*
* File Description:
*   Split TSE info
*
*/


#include <corelib/ncbiobj.hpp>
#include <corelib/ncbimtx.hpp>

#include <objects/seq/seq_id_handle.hpp>

#include <objmgr/impl/tse_chunk_info.hpp>

#include <vector>
#include <map>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CTSE_Info;
class CTSE_Chunk_Info;
class CBioseq_Info;
class CSeq_entry_Info;
class CDataSource;
class CDataLoader;

class NCBI_XOBJMGR_EXPORT CTSE_Split_Info : public CObject
{
public:
    typedef CConstRef<CObject>                      TBlobId;
    typedef int                                     TBlobVersion;
    typedef int                                     TSplitVersion;
    typedef CTSE_Chunk_Info::TChunkId               TChunkId;
    typedef vector<TChunkId>                        TChunkIds;
    typedef vector<CTSE_Info*>                      TTSE_Set;
    typedef vector<pair<CSeq_id_Handle, TChunkId> > TSeqIdToChunks;
    typedef map<TChunkId, CRef<CTSE_Chunk_Info> >   TChunks;
    typedef CTSE_Chunk_Info::TBioseqId              TBioseqId;
    typedef CTSE_Chunk_Info::TBioseq_setId          TBioseq_setId;
    typedef CTSE_Chunk_Info::TPlace                 TPlace;
    typedef CTSE_Chunk_Info::TDescInfo              TDescInfo;
    typedef CTSE_Chunk_Info::TSequence              TSequence;
    typedef CTSE_Chunk_Info::TAssembly              TAssembly;
    typedef CTSE_Chunk_Info::TLocationSet           TLocationSet;
    typedef vector<CSeq_id_Handle>                  TBioseqsIds;

    CTSE_Split_Info(void);
    ~CTSE_Split_Info(void);

    // interface to TSE
    TBlobId GetBlobId(void) const;
    TBlobVersion GetBlobVersion(void) const;
    TSplitVersion GetSplitVersion(void) const;
    void SetSplitVersion(TSplitVersion version);
    CInitMutexPool& GetMutexPool(void);
    CDataLoader& GetDataLoader(void);

    // TSE connection
    void x_DSAttach(CDataSource& ds);
    void x_TSEAttach(CTSE_Info& tse_info);

    // chunk connection
    void AddChunk(CTSE_Chunk_Info& chunk_info);
    CTSE_Chunk_Info& GetChunk(TChunkId chunk_id);
    const CTSE_Chunk_Info& GetChunk(TChunkId chunk_id) const;
    CTSE_Chunk_Info& GetSkeletonChunk(void);

    // get attach points from CTSE_Info
    CBioseq_Base_Info& x_GetBase(CTSE_Info& tse, const TPlace& place);
    CBioseq_Info& x_GetBioseq(CTSE_Info& tse, const TPlace& place);
    CBioseq_set_Info& x_GetBioseq_set(CTSE_Info& tse, const TPlace& place);
    CBioseq_Info& x_GetBioseq(CTSE_Info& tse, const TBioseqId& id);
    CBioseq_set_Info& x_GetBioseq_set(CTSE_Info& tse, TBioseq_setId id);

    // split information
    void x_AddDescInfo(const TDescInfo& info, TChunkId chunk_id);
    void x_AddAnnotPlace(const TPlace& place, TChunkId chunk_id);
    void x_AddBioseqPlace(TBioseq_setId place_id, TChunkId chunk_id);
    void x_AddSeq_data(const TLocationSet& location, CTSE_Chunk_Info& chunk);

    void x_AddDescInfo(CTSE_Info& tse_info,
                       const TDescInfo& info, TChunkId chunk_id);
    void x_AddAnnotPlace(CTSE_Info& tse_info,
                         const TPlace& place, TChunkId chunk_id);
    void x_AddBioseqPlace(CTSE_Info& tse_info,
                          TBioseq_setId place_id, TChunkId chunk_id);
    void x_AddSeq_data(CTSE_Info& tse_info,
                       const TLocationSet& location, CTSE_Chunk_Info& chunk);

    // id indexing
    void x_UpdateCore(void);
    void x_SetBioseqChunkId(TChunkId chunk_id);
    void x_SetContainedId(const TBioseqId& id, TChunkId chunk_id);

    void x_UpdateAnnotIndex(CTSE_Chunk_Info& chunk);
    
    // append ids with all Bioseqs Seq-ids from this Split-Info
    void GetBioseqsIds(TBioseqsIds& ids) const;

    // bioseq lookup
    bool ContainsBioseq(const CSeq_id_Handle& id) const;

    // loading requests
    void x_GetRecords(const CSeq_id_Handle& id, bool bioseq) const;
    void x_LoadChunk(TChunkId chunk_id) const;
    void x_LoadChunks(const TChunkIds& chunk_ids) const;

    // loading results
    void x_LoadDescr(const TPlace& place, const CSeq_descr& descr);
    void x_LoadAnnot(const TPlace& place, CRef<CSeq_annot_Info> annot);
    void x_LoadBioseq(const TPlace& place, const CBioseq& bioseq);
    void x_LoadSequence(const TPlace& place, TSeqPos pos,
                        const TSequence& sequence);
    void x_LoadAssembly(const TPlace& place,
                        const TAssembly& assembly);
    void x_LoadDescr(CTSE_Info& tse_info,
                     const TPlace& place, const CSeq_descr& descr);
    void x_LoadAnnot(CTSE_Info& tse_info,
                     const TPlace& place, CRef<CSeq_annot_Info> annot);
    void x_LoadBioseq(CTSE_Info& tse_info,
                      const TPlace& place, CRef<CSeq_entry_Info> entry);
    void x_LoadSequence(CTSE_Info& tse_info,
                        const TPlace& place, TSeqPos pos,
                        const TSequence& sequence);
    void x_LoadAssembly(CTSE_Info& tse_info,
                        const TPlace& place,
                        const TAssembly& assembly);

protected:
    TSeqIdToChunks::const_iterator x_FindChunk(const CSeq_id_Handle& id) const;

private:
    // identification of the blob
    CRef<CDataLoader>      m_DataLoader;
    TBlobId                m_BlobId;
    TBlobVersion           m_BlobVersion;
    TSplitVersion          m_SplitVersion;

    // all TSEs using this split info
    TTSE_Set               m_TSE_Set;
    
    // Split chunks
    TChunks                m_Chunks;
    TChunkId               m_BioseqChunkId;

    // loading
    CInitMutexPool         m_MutexPool;
    mutable CFastMutex     m_SeqIdToChunksMutex;
    mutable bool           m_SeqIdToChunksSorted;
    mutable TSeqIdToChunks m_SeqIdToChunks;

private:
    CTSE_Split_Info(const CTSE_Split_Info&);
    CTSE_Split_Info& operator=(const CTSE_Split_Info&);
};


END_SCOPE(objects)
END_NCBI_SCOPE

#endif//OBJECTS_OBJMGR_IMPL___TSE_SPLIT_INFO__HPP
