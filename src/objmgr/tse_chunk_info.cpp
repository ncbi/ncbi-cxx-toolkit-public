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
#include <objects/id2/ID2_Id_Range.hpp>
#include <objects/id2/ID2_Interval.hpp>
#include <objects/id2/ID2_Packed_Seq_ints.hpp>
#include <objects/id2/ID2_Seq_range.hpp>
#include <objects/id2/ID2S_Split_Info.hpp>
#include <objects/id2/ID2S_Chunk_Info.hpp>
#include <objects/id2/ID2S_Chunk.hpp>
#include <objects/id2/ID2S_Chunk_Data.hpp>
#include <objects/id2/ID2S_Chunk_Content.hpp>
#include <objects/id2/ID2S_Seq_annot_Info.hpp>
#include <objects/id2/ID2S_Feat_type_Info.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects);

/////////////////////////////////////////////////////////////////////////////
// CTSE_Chunk_Info
/////////////////////////////////////////////////////////////////////////////

CTSE_Chunk_Info::CTSE_Chunk_Info(CTSE_Info* tse_info,
                                 const CID2S_Chunk_Info& info)
    : m_TSE_Info(tse_info),
      m_ChunkInfo(&info),
      m_DirtyAnnotIndex(true),
      m_NotLoaded(true)
{
    _ASSERT(tse_info->m_Chunks.find(GetChunkId()) == tse_info->m_Chunks.end());
    tse_info->m_Chunks[GetChunkId()].Reset(this);
    tse_info->x_SetDirtyAnnotIndex();

    ITERATE ( CID2S_Chunk_Info::TContent, it, info.GetContent() ) {
        x_AttachContent(**it);
    }
}


CTSE_Chunk_Info::~CTSE_Chunk_Info(void)
{
}


CTSE_Chunk_Info::TChunkId CTSE_Chunk_Info::GetChunkId(void) const
{
    return m_ChunkInfo->GetId();
}


void CTSE_Chunk_Info::x_AttachContent(const CID2S_Chunk_Content& /*content*/)
{
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
    ITERATE ( CID2S_Chunk_Info::TContent, ctit, m_ChunkInfo->GetContent() ) {
        const CID2S_Chunk_Content& content = **ctit;
        if ( content.Which() != CID2S_Chunk_Content::e_Seq_annot ) {
            continue;
        }

        const CID2S_Seq_annot_Info& annot_info = content.GetSeq_annot();
        CAnnotName name;
        if ( annot_info.IsSetName() ) {
            name.SetNamed(annot_info.GetName());
        }
        m_ObjectInfosList.push_back(TObjectInfos(name));
        TObjectInfos& infos = m_ObjectInfosList.back();
        _ASSERT(infos.GetName() == name);
        
        // first count object infos to store
        size_t count = 0;
        if ( annot_info.IsSetAlign() ) {
            ++count;
        }
        if ( annot_info.IsSetGraph() ) {
            ++count;
        }
        ITERATE ( CID2S_Seq_annot_Info::TFeat, it, annot_info.GetFeat() ) {
            const CID2S_Feat_type_Info& feat_type = **it;
            if ( feat_type.IsSetSubtypes() ) {
                count += feat_type.GetSubtypes().size();
            }
            else {
                ++count;
            }
        }
        infos.Reserve(count);
        
        const CID2_Seq_loc& loc = annot_info.GetSeq_loc();

        if ( annot_info.IsSetAlign() ) {
            SAnnotTypeSelector sel(CSeq_annot::TData::e_Align);
            x_MapAnnotStub(sel, loc, infos);
        }
        if ( annot_info.IsSetGraph() ) {
            SAnnotTypeSelector sel(CSeq_annot::TData::e_Graph);
            x_MapAnnotStub(sel, loc, infos);
        }
        
        ITERATE ( CID2S_Seq_annot_Info::TFeat, it, annot_info.GetFeat() ) {
            const CID2S_Feat_type_Info& feat_type = **it;
            if ( feat_type.IsSetSubtypes() ) {
                ITERATE ( CID2S_Feat_type_Info::TSubtypes, sit,
                          feat_type.GetSubtypes() ) {
                    SAnnotTypeSelector sel(CSeqFeatData::ESubtype(+*sit));
                    x_MapAnnotStub(sel, loc, infos);
                }
            }
            else {
                SAnnotTypeSelector sel
                    (CSeqFeatData::E_Choice(feat_type.GetType()));
                x_MapAnnotStub(sel, loc, infos);
            }
        }
    }
}


void CTSE_Chunk_Info::x_MapAnnotStub(const SAnnotTypeSelector& sel,
                                     const CID2_Seq_loc& loc,
                                     TObjectInfos& infos)
{
    x_MapAnnotStub(infos.AddInfo(CAnnotObject_Info(*this, sel)), infos, loc);
}


void CTSE_Chunk_Info::x_MapAnnotStub(CAnnotObject_Info* info,
                                     TObjectInfos& infos,
                                     const CID2_Seq_loc& loc)
{
    switch ( loc.Which() ) {
    case CID2_Seq_loc::e_Whole:
    {
        CRange<TSeqPos> range = CRange<TSeqPos>::GetWhole();
        x_MapAnnotStub(info, infos, loc.GetWhole(), range);
        break;
    }
    
    case CID2_Seq_loc::e_Whole_range:
    {
        const CID2_Id_Range& id_range = loc.GetWhole_range();
        CRange<TSeqPos> range = CRange<TSeqPos>::GetWhole();
        for ( int cnt = id_range.GetCount(), gi = id_range.GetStart();
              cnt > 0;  --cnt, ++gi ) {
            x_MapAnnotStub(info, infos, gi, range);
        }
        break;
    }

    case CID2_Seq_loc::e_Int:
    {
        const CID2_Interval& interval = loc.GetInt();
        CRange<TSeqPos> range;
        range.SetFrom(interval.GetStart());
        range.SetLength(interval.GetLength());
        x_MapAnnotStub(info, infos, interval.GetGi(), range);
        break;
    }

    case CID2_Seq_loc::e_Int_set:
    {
        const CID2_Packed_Seq_ints& ints = loc.GetInt_set();
        ITERATE ( CID2_Packed_Seq_ints::TInts, rit, ints.GetInts() ) {
            const CID2_Seq_range& interval = **rit;
            CRange<TSeqPos> range;
            range.SetFrom(interval.GetStart());
            range.SetLength(interval.GetLength());
            x_MapAnnotStub(info, infos, ints.GetGi(), range);
        }
        break;
    }

    case CID2_Seq_loc::e_Loc_set:
    {
        const CID2_Seq_loc::TLoc_set& loc_set = loc.GetLoc_set();
        ITERATE ( CID2_Seq_loc::TLoc_set, it, loc_set ) {
            x_MapAnnotStub(info, infos, **it);
        }
        break;
    }

    }
}


void CTSE_Chunk_Info::x_MapAnnotStub(CAnnotObject_Info* info,
                                     TObjectInfos& infos,
                                     int gi,
                                     const CRange<TSeqPos>& range)
{
    SAnnotObject_Key key;
    SAnnotObject_Index annotRef;
    key.m_AnnotObject_Info = annotRef.m_AnnotObject_Info = info;
    key.m_Range = range;
    CSeq_id gi_id;
    gi_id.SetGi(gi);
    key.m_Handle = CSeq_id_Handle::GetHandle(gi_id);
    GetTSE_Info().x_MapAnnotObject(key, annotRef, infos);
}


void CTSE_Chunk_Info::x_UnmapAnnotObjects(void)
{
    NON_CONST_ITERATE ( TObjectInfosList, it, m_ObjectInfosList ) {
        GetTSE_Info().x_UnmapAnnotObjects(*it);
    }
    m_ObjectInfosList.clear();
}


void CTSE_Chunk_Info::x_DropAnnotObjects(void)
{
    m_ObjectInfosList.clear();
}


CSeq_entry_Info& CTSE_Chunk_Info::GetBioseq_set(int id)
{
    return GetTSE_Info().GetBioseq_set(id);
}


CSeq_entry_Info& CTSE_Chunk_Info::GetBioseq(int gi)
{
    return GetTSE_Info().GetBioseq(gi).GetSeq_entry_Info();
}


void CTSE_Chunk_Info::Load(const CID2S_Chunk& chunk)
{
    ITERATE ( CID2S_Chunk::TData, it, chunk.GetData() ) {
        x_Add(**it);
    }
}


void CTSE_Chunk_Info::x_Add(const CID2S_Chunk_Data& data)
{
    CRef<CSeq_entry_Info> entry;
    switch ( data.GetId().Which() ) {
    case CID2S_Chunk_Data::TId::e_Bioseq_set:
        entry.Reset(&GetBioseq_set(data.GetId().GetBioseq_set()));
        break;
    case CID2S_Chunk_Data::TId::e_Gi:
        entry.Reset(&GetBioseq(data.GetId().GetGi()));
        break;
    }
    ITERATE ( CID2S_Chunk_Data::TAnnots, it, data.GetAnnots() ) {
        CSeq_annot& annot = const_cast<CSeq_annot&>(**it);
        CRef<CSeq_annot_Info> annot_info(new CSeq_annot_Info(annot));
        annot_info->x_Seq_entryAttach(*entry);
        if ( GetTSE_Info().HaveDataSource() ) {
            annot_info->x_DSAttach();
        }
        GetTSE_Info().UpdateAnnotIndex(*annot_info);
    }
}


void CTSE_Chunk_Info::LoadAnnotBioseq_set(int id, CSeq_annot_Info& annot_info)
{
    annot_info.x_Seq_entryAttach(GetBioseq_set(id));
    if ( GetTSE_Info().HaveDataSource() ) {
        annot_info.x_DSAttach();
    }
    GetTSE_Info().UpdateAnnotIndex(annot_info);
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.4  2003/11/26 17:56:00  vasilche
* Implemented ID2 split in ID1 cache.
* Fixed loading of splitted annotations.
*
* Revision 1.3  2003/10/07 13:43:23  vasilche
* Added proper handling of named Seq-annots.
* Added feature search from named Seq-annots.
* Added configurable adaptive annotation search (default: gene, cds, mrna).
* Fixed selection of blobs for loading from GenBank.
* Added debug checks to CSeq_id_Mapper for easier finding lost CSeq_id_Handles.
* Fixed leaked split chunks annotation stubs.
* Moved some classes definitions in separate *.cpp files.
*
* Revision 1.2  2003/10/01 00:24:34  ucko
* x_UpdateAnnotIndexThis: fix handling of whole-set (caught by MIPSpro)
*
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
