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
*   Splitted TSE chunk info
*
*/


#include <objmgr/impl/tse_chunk_info.hpp>
#include <objmgr/impl/tse_info.hpp>
#include <objmgr/impl/seq_annot_info.hpp>
#include <objmgr/impl/data_source.hpp>
#include <objmgr/impl/annot_object.hpp>

#include <objects/id2/ID2_Seq_loc.hpp>
#include <objects/id2/ID2_Interval.hpp>
#include <objects/id2/ID2_Packed_Seq_ints.hpp>
#include <objects/id2/ID2_Seq_range.hpp>
#include <objects/id2/ID2S_Split_Info.hpp>
#include <objects/id2/ID2S_Chunk_Info.hpp>
#include <objects/id2/ID2S_Chunk_Content.hpp>
#include <objects/id2/ID2S_Seq_annot_Info.hpp>
#include <objects/id2/ID2S_Feat_type_Info.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

/////////////////////////////////////////////////////////////////////////////
// CTSE_Chunk_Info
/////////////////////////////////////////////////////////////////////////////

CTSE_Chunk_Info::CTSE_Chunk_Info(CTSE_Info* tse_info, TChunkId chunk_id)
    : m_TSE_Info(tse_info),
      m_ChunkId(chunk_id),
      m_DirtyAnnotIndex(true),
      m_NotLoaded(true)
{
    tse_info->m_Chunks[chunk_id] = Ref(this);
}


CTSE_Chunk_Info::CTSE_Chunk_Info(CTSE_Info* tse_info,
                                 const CID2S_Chunk_Info& info)
    : m_TSE_Info(tse_info),
      m_ChunkId(info.GetId()),
      m_DirtyAnnotIndex(true),
      m_NotLoaded(true)
{
    _ASSERT(tse_info->m_Chunks.find(m_ChunkId) == tse_info->m_Chunks.end());
    tse_info->m_Chunks[m_ChunkId].Reset(this);
    ITERATE ( CID2S_Chunk_Info::TContent, it, info.GetContent() ) {
        x_AttachContent(**it);
    }
}


CTSE_Chunk_Info::~CTSE_Chunk_Info(void)
{
}


void CTSE_Chunk_Info::x_AttachContent(const CID2S_Chunk_Content& content)
{
    switch ( content.Which() ) {
    case CID2S_Chunk_Content::e_Seq_annot:
        x_AttachAnnot(content.GetSeq_annot());
        break;
    }
}

void CTSE_Chunk_Info::x_AttachAnnot(const CID2S_Seq_annot_Info& info)
{
    CConstRef<CID2_Seq_loc> loc(&info.GetSeq_loc());
    if ( info.IsSetAlign() ) {
        SAnnotTypeSelector sel(CSeq_annot::TData::e_Align);
        m_AnnotTypes[sel].push_back(loc);
    }
    if ( info.IsSetGraph() ) {
        SAnnotTypeSelector sel(CSeq_annot::TData::e_Graph);
        m_AnnotTypes[sel].push_back(loc);
    }
    ITERATE ( CID2S_Seq_annot_Info::TFeat, it, info.GetFeat() ) {
        const CID2S_Feat_type_Info& type = **it;
        if ( type.IsSetSubtypes() ) {
            ITERATE ( CID2S_Feat_type_Info::TSubtypes, sit,
                      type.GetSubtypes() ) {
                SAnnotTypeSelector sel(CSeqFeatData::ESubtype(+*sit));
                m_AnnotTypes[sel].push_back(loc);
            }
        }
        else {
            SAnnotTypeSelector sel(CSeqFeatData::E_Choice(type.GetType()));
            m_AnnotTypes[sel].push_back(loc);
        }
    }
}


void CTSE_Chunk_Info::Load(void)
{
    CFastMutexGuard guard(m_LoadLock);
    if ( m_NotLoaded ) {
        x_Load();
        m_NotLoaded = false;
    }
}


void CTSE_Chunk_Info::x_Load(void)
{
    GetTSE_Info().GetDataSource().GetDataLoader()->GetChunk(*this);
}


void CTSE_Chunk_Info::x_UpdateAnnotIndex(void)
{
    if ( m_DirtyAnnotIndex ) {
        x_UpdateAnnotIndexThis();
        m_DirtyAnnotIndex = false;
    }
}


void CTSE_Chunk_Info::x_UpdateAnnotIndexThis(void)
{
    CTSE_Info& tse_info = GetTSE_Info();

    SAnnotObject_Key key;
    SAnnotObject_Index annotRef;
    CSeq_id gi_id;
    ITERATE ( TAnnotTypes, it, m_AnnotTypes ) {
        m_ObjectInfos.push_back(CAnnotObject_Info(*this,
                                                  it->first.GetAnnotChoice(),
                                                  it->first.GetFeatChoice(),
                                                  it->first.GetFeatSubtype()));
        annotRef.m_AnnotObject_Info = &m_ObjectInfos.back();
        key.m_AnnotObject_Info = &m_ObjectInfos.back();
        ITERATE ( TLocation, lit, it->second ) {
            const CID2_Seq_loc& loc = **lit;
            switch ( loc.Which() ) {
            case CID2_Seq_loc::e_Whole:
                key.m_Range = CRange<TSeqPos>::GetWhole();
                gi_id.SetGi(loc.GetWhole());
                key.m_Handle = CSeq_id_Handle::GetHandle(gi_id);
                tse_info.x_MapAnnotObject(key, annotRef);
                break;
            case CID2_Seq_loc::e_Whole_set:
                key.m_Range = CRange<TSeqPos>::GetWhole();
                ITERATE ( CID2_Seq_loc::TWhole_set, wit, loc.GetWhole_set() ) {
                    gi_id.SetGi(*wit);
                    key.m_Handle = CSeq_id_Handle::GetHandle(gi_id);
                    tse_info.x_MapAnnotObject(key, annotRef);
                }
                break;
            case CID2_Seq_loc::e_Int:
            {
                const CID2_Interval& interval = loc.GetInt();
                key.m_Range.SetFrom(interval.GetStart());
                key.m_Range.SetLength(interval.GetLength());
                gi_id.SetGi(interval.GetGi());
                key.m_Handle = CSeq_id_Handle::GetHandle(gi_id);
                tse_info.x_MapAnnotObject(key, annotRef);
                break;
            }
            case CID2_Seq_loc::e_Int_set:
            {
                ITERATE ( CID2_Seq_loc::TInt_set, iit, loc.GetInt_set() ) {
                    const CID2_Packed_Seq_ints& ints = **iit;
                    gi_id.SetGi(ints.GetGi());
                    key.m_Handle = CSeq_id_Handle::GetHandle(gi_id);
                    ITERATE ( CID2_Packed_Seq_ints::TInts, rit,
                              ints.GetInts() ) {
                        const CID2_Seq_range& range = **rit;
                        key.m_Range.SetFrom(range.GetStart());
                        key.m_Range.SetLength(range.GetLength());
                        tse_info.x_MapAnnotObject(key, annotRef);
                    }
                }
                break;
            }
            }
        }
    }
}


CSeq_entry_Info& CTSE_Chunk_Info::GetBioseq_set(int id)
{
    return GetTSE_Info().GetBioseq_set(id);
}


CSeq_entry_Info& CTSE_Chunk_Info::GetBioseq(int gi)
{
    return GetTSE_Info().GetBioseq(gi).GetSeq_entry_Info();
}


void CTSE_Chunk_Info::LoadAnnotBioseq_set(int id, CSeq_annot_Info& annot_info)
{
    annot_info.x_Seq_entryAttach(GetBioseq_set(id));
    GetTSE_Info().UpdateAnnotIndex(annot_info);
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2003/09/30 16:22:04  vasilche
* Updated internal object manager classes to be able to load ID2 data.
* SNP blobs are loaded as ID2 split blobs - readers convert them automatically.
* Scope caches results of requests for data to data loaders.
* Optimized CSeq_id_Handle for gis.
* Optimized bioseq lookup in scope.
* Reduced object allocations in annotation iterators.
* CScope is allowed to be destroyed before other objects using this scope are
* deleted (feature iterators, bioseq handles etc).
* Optimized lookup for matching Seq-ids in CSeq_id_Mapper.
* Added 'adaptive' option to objmgr_demo application.
*
* ===========================================================================
*/
