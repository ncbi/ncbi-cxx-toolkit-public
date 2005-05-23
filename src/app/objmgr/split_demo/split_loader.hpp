#ifndef SPLIT_LOADER__HPP_INCLUDED
#define SPLIT_LOADER__HPP_INCLUDED

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
*  ===========================================================================
*
*  Author: Aleksey Grichenko
*
*  File Description:
*   Sample split data loader
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <corelib/plugin_manager.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objmgr/data_loader.hpp>
#include <objmgr/impl/tse_chunk_info.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CSeq_entry;
class CSeq_literal;

/////////////////////////////////////////////////////////////////////////////////
//
// SplitDataLoader
//

class CSplitDataLoader : public CDataLoader
{
public:
    typedef SRegisterLoaderInfo<CSplitDataLoader> TRegisterLoaderInfo;
    static TRegisterLoaderInfo RegisterInObjectManager(
        CObjectManager& om,
        const string& data_file,
        CObjectManager::EIsDefault is_default = CObjectManager::eDefault,
        CObjectManager::TPriority priority = CObjectManager::kPriority_NotSet);
    static string GetLoaderNameFromArgs(const string& data_file);

    virtual ~CSplitDataLoader(void);

    // Unload the TSE, clear chunk mappings
    virtual void DropTSE(CRef<CTSE_Info> tse_info);

    // Load TSE core, register chunks of split data
    virtual TTSE_LockSet GetRecords(const CSeq_id_Handle& idh,
                                    EChoice choice);

    // Load a chunk with splitted data
    virtual void GetChunk(TChunk chunk);

    virtual TBlobId GetBlobId(const CSeq_id_Handle& idh);
    virtual bool CanGetBlobById(void) const;
    virtual TTSE_Lock GetBlobById(const TBlobId& blob_id);

private:
    typedef CParamLoaderMaker<CSplitDataLoader, const string&>
        TSplitLoaderMaker;
    friend class CParamLoaderMaker<CSplitDataLoader, const string&>;

    CSplitDataLoader(const string& loader_name,
                     const string& data_file);

    // Hide methods
    CSplitDataLoader(const CSplitDataLoader&);
    CSplitDataLoader& operator=(const CSplitDataLoader&);

    typedef CTSE_Chunk_Info::TPlace         TPlace;
    typedef vector< CRef<CTSE_Chunk_Info> > TChunks;

    // Load and split TSE from file, store detached data locally
    CTSE_LoadLock x_LoadData(void);
    // Split bioseq, store annotations, descriptors and seq-data
    void x_SplitSeq(TChunks& chunks, CBioseq& bioseq);
    // Split bioseq, store annotations and descriptors
    void x_SplitSet(TChunks& chunks, CBioseq_set& seqset);

    // For plain bioseqs: remove seq-data, store locally, register data chunk
    void x_SplitSeqData(TChunks& chunks, CSeq_id_Handle idh, CBioseq& bioseq);
    // Store seq-descr information locally, register descr chunk
    void x_SplitDescr(TChunks& chunks, TPlace place, CSeq_descr& descr);
    // Store annots locally, register annot chunk
    void x_SplitAnnot(TChunks& chunks, TPlace place, CBioseq::TAnnot& annots);
    // Register feat types and locations in the chunk
    void x_SplitFeats(CTSE_Chunk_Info& chunk, const CSeq_annot& annot);
    // Register align location in the chunk
    void x_SplitAligns(CTSE_Chunk_Info& chunk, const CSeq_annot& annot);
    // Register graph location in the chunk
    void x_SplitGraphs(CTSE_Chunk_Info& chunk, const CSeq_annot& annot);

    // Types for storing split data locally
    struct SAnnotData {
        TPlace           m_Place; // Where to attach the annot
        CRef<CSeq_annot> m_Annot;
    };
    struct SDescrData {
        TPlace           m_Place; // Where to attach the descr
        CRef<CSeq_descr> m_Descr;
    };
    struct SSeqData {
        TPlace             m_Place;  // Where to attach the data
        TSeqPos            m_Pos;    // Literal start position
        CRef<CSeq_literal> m_Literal;
    };
    // Set of annotations from a single chunk
    typedef vector<SAnnotData>   TAnnots;
    typedef map<int, TAnnots>    TAnnotChunks;
    typedef map<int, SDescrData> TDescrChunks;
    typedef map<int, SSeqData>   TSequenceChunks;
    typedef set<CSeq_id_Handle>  TIds;

    string              m_DataFile;     // original file with the TSE
    CRef<CSeq_entry>    m_TSE;          // TSE
    TIds                m_Ids;          // all ids found in the TSE
    TAnnotChunks        m_AnnotChunks;  // Split annotations by chunk id
    TDescrChunks        m_DescrChunks;  // Split descrs by chunk id
    TSequenceChunks     m_SeqChunks;    // Split seq-data by chunk id
    int                 m_NextSeqsetId; // Seq-set id counter
    int                 m_NextChunkId;  // Chunk id counter
};


END_SCOPE(objects)
END_NCBI_SCOPE

#endif // SPLIT_LOADER__HPP_INCLUDED
