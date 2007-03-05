#ifndef NCBI_OBJMGR_SPLIT_BLOB_SPLITTER_IMPL__HPP
#define NCBI_OBJMGR_SPLIT_BLOB_SPLITTER_IMPL__HPP

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
* Author:  Eugene Vasilchenko
*
* File Description:
*   Application for splitting blobs withing ID1 cache
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>

#include <memory>
#include <map>
#include <set>
#include <vector>
#include <list>

#include <objmgr/split/blob_splitter_params.hpp>
#include <objmgr/split/split_blob.hpp>
#include <objmgr/split/chunk_info.hpp>
#include <objmgr/split/object_splitinfo.hpp>
#include <objmgr/split/size.hpp>

BEGIN_NCBI_SCOPE

class CObjectOStream;

BEGIN_SCOPE(objects)

class CSeq_entry;
class CBioseq;
class CBioseq_set;
class CSeq_descr;
class CSeq_inst;
class CSeq_annot;
class CSeq_feat;
class CSeq_align;
class CSeq_graph;
class CSeq_data;
class CSeq_literal;
class CDelta_seq;
class CDelta_ext;
class CID2S_Split_Info;
class CID2S_Chunk_Id;
class CID2S_Chunk;
class CID2S_Chunk_Data;
class CID2S_Chunk_Content;
class CID2S_Seq_descr_Info;
class CID2S_Seq_annot_place_Info;
class CID2S_Bioseq_set_Ids;
class CID2S_Bioseq_Ids;
class CID2S_Seq_loc;
class CBlobSplitter;
class CBlobSplitterImpl;
class CAnnotObject_SplitInfo;
class CLocObjects_SplitInfo;
class CSeq_annot_SplitInfo;
class CPlace_SplitInfo;
class CHandleRangeMap;

struct SAnnotPiece;
struct SIdAnnotPieces;
class CAnnotPieces;
struct SChunkInfo;

class CBlobSplitterImpl
{
public:
    CBlobSplitterImpl(const SSplitterParams& params);
    ~CBlobSplitterImpl(void);

    typedef map<CPlaceId, CPlace_SplitInfo> TEntries;
    typedef int TChunkId;
    typedef map<TChunkId, SChunkInfo> TChunks;
    typedef map<CID2S_Chunk_Id, CRef<CID2S_Chunk> > TID2Chunks;
    typedef vector< CRef<CAnnotPieces> > TPieces;
    typedef CSeqsRange::TRange TRange;

    bool Split(const CSeq_entry& entry);

    const CSplitBlob& GetBlob(void) const
        {
            return m_SplitBlob;
        }

    void Reset(void);

    void CopySkeleton(CSeq_entry& dst, const CSeq_entry& src);
    void CopySkeleton(CBioseq_set& dst, const CBioseq_set& src);
    void CopySkeleton(CBioseq& dst, const CBioseq& src);

    bool CopyDescr(CPlace_SplitInfo& place_info,
                   TSeqPos seq_length,
                   const CSeq_descr& descr);
    bool CopyHist(CPlace_SplitInfo& place_info,
                  const CSeq_hist& hist);
    bool CopySequence(CPlace_SplitInfo& place_info,
                      TSeqPos seq_length,
                      CSeq_inst& dst, const CSeq_inst& src);
    bool CopyAnnot(CPlace_SplitInfo& place_info, const CSeq_annot& annot);

    bool CanSplitBioseq(const CBioseq& bioseq) const;
    bool SplitBioseq(CPlace_SplitInfo& place_info, const CBioseq& bioseq);

    void CollectPieces(void);
    void CollectPieces(const CPlace_SplitInfo& info);
    void CollectPieces(const CPlaceId& place_id,
                       const CSeq_annot_SplitInfo& info);
    void CollectPieces(const CPlaceId& place_id,
                       const CSeq_descr_SplitInfo& info);
    void CollectPieces(const CPlaceId& place_id,
                       const CSeq_hist_SplitInfo& info);
    void Add(const SAnnotPiece& piece);
    void SplitPieces(void);
    void AddToSkeleton(CAnnotPieces& pieces);
    void SplitPieces(CAnnotPieces& pieces);
    void MakeID2SObjects(void);
    void AttachToSkeleton(const SChunkInfo& info);

    static size_t CountAnnotObjects(const CSeq_annot& annot);
    static size_t CountAnnotObjects(const CSeq_entry& entry);
    static size_t CountAnnotObjects(const CID2S_Chunk& chunk);
    static size_t CountAnnotObjects(const TID2Chunks& chunks);

    TSeqPos GetLength(const CSeq_data& src) const;
    TSeqPos GetLength(const CDelta_seq& src) const;
    TSeqPos GetLength(const CDelta_ext& src) const;
    TSeqPos GetLength(const CSeq_ext& src) const;
    TSeqPos GetLength(const CSeq_inst& src) const;

    TSeqPos GetLength(const CSeq_id_Handle& id) const;
    bool IsWhole(const CSeq_id_Handle& id, const TRange& range) const;

    void SetLoc(CID2S_Seq_loc& loc,
                const CHandleRangeMap& ranges) const;
    void SetLoc(CID2S_Seq_loc& loc,
                const CSeqsRange& ranges) const;
    void SetLoc(CID2S_Seq_loc& loc,
                const CSeq_id_Handle& id, const TRange& range) const;
    CRef<CID2S_Seq_loc> MakeLoc(const CSeqsRange& range) const;
    CRef<CID2S_Seq_loc> MakeLoc(const CSeq_id_Handle& id,
                                const TRange& range) const;

    CRef<CID2S_Bioseq_Ids> MakeBioseqIds(const set<CSeq_id_Handle>& ids) const;
    CRef<CID2S_Bioseq_set_Ids> MakeBioseq_setIds(const set<int>& ids) const;

    typedef vector<CAnnotObject_SplitInfo> TAnnotObjects;
    CRef<CSeq_annot> MakeSeq_annot(const CSeq_annot& src,
                                   const TAnnotObjects& objs);
    
    typedef map<CPlaceId, CRef<CID2S_Chunk_Data> > TChunkData;
    typedef vector< CRef<CID2S_Chunk_Content> > TChunkContent;
    
    CID2S_Chunk_Data& GetChunkData(TChunkData& chunk_data,
                                   const CPlaceId& place_id);

    void MakeID2Chunk(TChunkId id, const SChunkInfo& info);

    SChunkInfo* NextChunk(void);
    SChunkInfo* NextChunk(SChunkInfo* chunk, const CSize& size);

private:
    // params
    SSplitterParams m_Params;

    // split result
    CSplitBlob m_SplitBlob;

    // split state
    CRef<CSeq_entry> m_Skeleton;
    CRef<CID2S_Split_Info> m_Split_Info;
    TID2Chunks m_ID2_Chunks;

    int m_NextBioseq_set_Id;

    TEntries m_Entries;

    TPieces m_Pieces;

    TChunks m_Chunks;
};


END_SCOPE(objects)
END_NCBI_SCOPE

#endif//NCBI_OBJMGR_SPLIT_BLOB_SPLITTER_IMPL__HPP
