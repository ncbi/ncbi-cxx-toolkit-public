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
*   Split TSE chunk info
*
*/


#include <ncbi_pch.hpp>
#include <objmgr/impl/tse_chunk_info.hpp>
#include <objmgr/impl/tse_chunk_info.hpp>
#include <objmgr/impl/tse_split_info.hpp>
#include <objmgr/impl/tse_info.hpp>
#include <objmgr/impl/tse_assigner.hpp>
#include <objmgr/impl/seq_annot_info.hpp>
#include <objmgr/impl/bioseq_info.hpp>
#include <objmgr/impl/bioseq_set_info.hpp>
#include <objmgr/impl/data_source.hpp>
#include <objmgr/impl/annot_object.hpp>
#include <objmgr/impl/annot_type_index.hpp>
#include <objects/seq/Seq_literal.hpp>
#include <objmgr/seq_map.hpp>
#include <algorithm>
#include <objmgr/error_codes.hpp>

#define NCBI_USE_ERRCODE_X   ObjMgr_ObjSplitInfo

BEGIN_NCBI_SCOPE

NCBI_DEFINE_ERR_SUBCODE_X(11);

BEGIN_SCOPE(objects)

/////////////////////////////////////////////////////////////////////////////
// CTSE_Chunk_Info
/////////////////////////////////////////////////////////////////////////////


CTSE_Chunk_Info::CTSE_Chunk_Info(TChunkId id)
    : m_SplitInfo(0),
      m_ChunkId(id),
      m_AnnotIndexEnabled(false),
      m_ExplicitFeatIds(false)
{
}


CTSE_Chunk_Info::~CTSE_Chunk_Info(void)
{
}


bool CTSE_Chunk_Info::x_Attached(void) const
{
    return m_SplitInfo != 0;
}


/////////////////////////////////////////////////////////////////////////////
// chunk identification getters
CTSE_Chunk_Info::TBlobId CTSE_Chunk_Info::GetBlobId(void) const
{
    _ASSERT(x_Attached());
    return m_SplitInfo->GetBlobId();
}


CTSE_Chunk_Info::TBlobVersion CTSE_Chunk_Info::GetBlobVersion(void) const
{
    _ASSERT(x_Attached());
    return m_SplitInfo->GetBlobVersion();
}


/////////////////////////////////////////////////////////////////////////////
// attach chunk to CTSE_Split_Info
void CTSE_Chunk_Info::x_SplitAttach(CTSE_Split_Info& split_info)
{
    _ASSERT(!x_Attached());
    m_SplitInfo = &split_info;

    TChunkId chunk_id = GetChunkId();

    // register descrs places
    ITERATE ( TDescInfos, it, m_DescInfos ) {
        split_info.x_AddDescInfo(*it, chunk_id);
    }

    // register assembly places
    ITERATE ( TAssemblyInfos, it, m_AssemblyInfos ) {
        split_info.x_AddAssemblyInfo(*it, chunk_id);
    }

    // register annots places
    ITERATE ( TPlaces, it, m_AnnotPlaces ) {
        split_info.x_AddAnnotPlace(*it, chunk_id);
    }

    // register bioseq ids
    {{
        set<CSeq_id_Handle> ids;
        TBioseqIds(m_BioseqIds).swap(m_BioseqIds);
        sort(m_BioseqIds.begin(), m_BioseqIds.end());
        ITERATE ( TBioseqIds, it, m_BioseqIds ) {
            split_info.x_SetContainedId(*it, chunk_id, true);
            _VERIFY(ids.insert(*it).second);
        }
        ITERATE ( TAnnotContents, it, m_AnnotContents ) {
            ITERATE ( TAnnotTypes, tit, it->second ) {
                ITERATE ( TLocationSet, lit, tit->second ) {
                    if ( ids.insert(lit->first).second ) {
                        split_info.x_SetContainedId(lit->first, chunk_id, false);
                    }
                }
            }
        }
    }}

    // register bioseqs places
    ITERATE ( TBioseqPlaces, it, m_BioseqPlaces ) {
        split_info.x_AddBioseqPlace(*it, chunk_id);
    }

    // register seq-data
    split_info.x_AddSeq_data(m_Seq_data, *this);
}


// attach chunk to CTSE_Info
/*
void CTSE_Chunk_Info::x_TSEAttach(CTSE_Info& tse_info)
{
    _ASSERT(x_Attached());

    TChunkId chunk_id = GetChunkId();

    // register descrs places
    ITERATE ( TDescInfos, it, m_DescInfos ) {
        m_SplitInfo->x_AddDescInfo(tse_info, *it, chunk_id);
    }

    // register assembly places
    ITERATE ( TAssemblyInfos, it, m_AssemblyInfos ) {
        m_SplitInfo->x_AddAssemblyInfo(tse_info, *it, chunk_id);
    }

    // register annots places
    ITERATE ( TPlaces, it, m_AnnotPlaces ) {
        m_SplitInfo->x_AddAnnotPlace(tse_info, *it, chunk_id);
    }

    // register bioseqs places
    ITERATE ( TBioseqPlaces, it, m_BioseqPlaces ) {
        m_SplitInfo->x_AddBioseqPlace(tse_info, *it, chunk_id);
    }

    // register seq-data
    m_SplitInfo->x_AddSeq_data(tse_info, m_Seq_data, *this);

    if ( m_AnnotIndexEnabled ) {
        x_UpdateAnnotIndex(tse_info);
    }
}
*/

void CTSE_Chunk_Info::x_TSEAttach(CTSE_Info& tse, ITSE_Assigner& lsnr)
{
    _ASSERT(x_Attached());

    if ( NotLoaded() ) {
        TChunkId chunk_id = GetChunkId();

        // register descrs places
        ITERATE ( TDescInfos, it, m_DescInfos ) {
            lsnr.AddDescInfo(tse, *it, chunk_id);
        }

        // register assembly places
        ITERATE ( TAssemblyInfos, it, m_AssemblyInfos ) {
            lsnr.AddAssemblyInfo(tse, *it, chunk_id);
        }

        // register annots places
        ITERATE ( TPlaces, it, m_AnnotPlaces ) {
            lsnr.AddAnnotPlace(tse, *it, chunk_id);
        }

        // register bioseqs places
        ITERATE ( TBioseqPlaces, it, m_BioseqPlaces ) {
            lsnr.AddBioseqPlace(tse, *it, chunk_id);
        }

        // register seq-data
        lsnr.AddSeq_data(tse, m_Seq_data, *this);
    }

    if ( m_AnnotIndexEnabled ) {
        x_UpdateAnnotIndex(tse);
    }
}



/////////////////////////////////////////////////////////////////////////////
// loading methods

void CTSE_Chunk_Info::GetBioseqsIds(TBioseqIds& ids) const
{
    ids.insert(ids.end(), m_BioseqIds.begin(), m_BioseqIds.end());
}


bool CTSE_Chunk_Info::ContainsBioseq(const CSeq_id_Handle& id) const
{
    return binary_search(m_BioseqIds.begin(), m_BioseqIds.end(), id);
}


bool CTSE_Chunk_Info::x_GetRecords(const CSeq_id_Handle& id, bool bioseq) const
{
    if ( IsLoaded() ) {
        return true;
    }
    if ( ContainsBioseq(id) ) {
        // contains Bioseq -> always load
        Load();
        return true;
    }
    if ( !bioseq ) {
        // we are requested to index annotations
        const_cast<CTSE_Chunk_Info*>(this)->x_EnableAnnotIndex();
    }
    return false;
}


CInitGuard* CTSE_Chunk_Info::GetLoadInitGuard(void)
{
    if ( IsLoaded() ) {
        return 0;
    }
    return new CInitGuard(m_LoadLock, m_SplitInfo->GetMutexPool());
}


void CTSE_Chunk_Info::Load(void) const
{
    CTSE_Chunk_Info* chunk = const_cast<CTSE_Chunk_Info*>(this);
    _ASSERT(x_Attached());
    CInitGuard init(chunk->m_LoadLock, m_SplitInfo->GetMutexPool());
    if ( init ) {
        m_SplitInfo->GetDataLoader().GetChunk(Ref(chunk));
        chunk->x_DisableAnnotIndexWhenLoaded();
    }
}


void CTSE_Chunk_Info::SetLoaded(CObject* obj)
{
    if ( !obj ) {
        obj = new CObject;
    }
    m_LoadLock.Reset(obj);
    x_DisableAnnotIndexWhenLoaded();
}


/////////////////////////////////////////////////////////////////////////////
// chunk content description
void CTSE_Chunk_Info::x_AddDescInfo(TDescTypeMask type_mask,
                                    const TBioseqId& id)
{
    x_AddDescInfo(TDescInfo(type_mask, TPlace(id, 0)));
}


void CTSE_Chunk_Info::x_AddDescInfo(TDescTypeMask type_mask,
                                    TBioseq_setId id)
{
    x_AddDescInfo(TDescInfo(type_mask, TPlace(CSeq_id_Handle(), id)));
}


void CTSE_Chunk_Info::x_AddDescInfo(const TDescInfo& info)
{
    m_DescInfos.push_back(info);
    if ( m_SplitInfo ) {
        m_SplitInfo->x_AddDescInfo(info, GetChunkId());
    }
}


void CTSE_Chunk_Info::x_AddAssemblyInfo(const TBioseqId& id)
{
    m_AssemblyInfos.push_back(id);
    if ( m_SplitInfo ) {
        m_SplitInfo->x_AddAssemblyInfo(id, GetChunkId());
    }
}


void CTSE_Chunk_Info::x_AddAnnotPlace(const TBioseqId& id)
{
    x_AddAnnotPlace(TPlace(id, 0));
}


void CTSE_Chunk_Info::x_AddAnnotPlace(TBioseq_setId id)
{
    x_AddAnnotPlace(TPlace(CSeq_id_Handle(), id));
}


void CTSE_Chunk_Info::x_AddAnnotPlace(const TPlace& place)
{
    m_AnnotPlaces.push_back(place);
    if ( m_SplitInfo ) {
        m_SplitInfo->x_AddAnnotPlace(place, GetChunkId());
    }
}


void CTSE_Chunk_Info::x_AddBioseqPlace(TBioseq_setId id)
{
    m_BioseqPlaces.push_back(id);
    if ( m_SplitInfo ) {
        m_SplitInfo->x_AddBioseqPlace(id, GetChunkId());
    }
}


void CTSE_Chunk_Info::x_AddBioseqId(const TBioseqId& id)
{
    _ASSERT(!x_Attached());
    m_BioseqIds.push_back(id);
}


void CTSE_Chunk_Info::x_AddSeq_data(const TLocationSet& location)
{
    m_Seq_data.insert(m_Seq_data.end(), location.begin(), location.end());
    if ( m_SplitInfo ) {
        m_SplitInfo->x_AddSeq_data(location, *this);
    }
}


void CTSE_Chunk_Info::x_AddAnnotType(const CAnnotName& annot_name,
                                     const SAnnotTypeSelector& annot_type,
                                     const TLocationId& location_id,
                                     const TLocationRange& location_range)
{
    _ASSERT(!x_Attached());
    TLocationSet& dst = m_AnnotContents[annot_name][annot_type];
    dst.push_back(TLocation(location_id, location_range));
}


void CTSE_Chunk_Info::x_AddAnnotType(const CAnnotName& annot_name,
                                     const SAnnotTypeSelector& annot_type,
                                     const TLocationId& location_id)
{
    _ASSERT(!x_Attached());
    TLocationSet& dst = m_AnnotContents[annot_name][annot_type];
    TLocation location(location_id, TLocationRange::GetWhole());
    dst.push_back(location);
}


void CTSE_Chunk_Info::x_AddAnnotType(const CAnnotName& annot_name,
                                     const SAnnotTypeSelector& annot_type,
                                     const TLocationSet& location)
{
    _ASSERT(!x_Attached());
    TLocationSet& dst = m_AnnotContents[annot_name][annot_type];
    dst.insert(dst.end(), location.begin(), location.end());
}


void CTSE_Chunk_Info::x_AddFeat_ids(void)
{
    m_ExplicitFeatIds = true;
}


void CTSE_Chunk_Info::x_AddFeat_ids(const SAnnotTypeSelector& type,
                                    const TFeatIdIntList& ids)
{
    m_ExplicitFeatIds = true;
    TFeatIdIntList& dst = m_FeatIds[type].m_IntList;
    dst.insert(dst.end(), ids.begin(), ids.end());
}


void CTSE_Chunk_Info::x_AddXref_ids(const SAnnotTypeSelector& type,
                                    const TFeatIdIntList& ids)
{
    m_ExplicitFeatIds = true;
    TFeatIdIntList& dst = m_XrefIds[type].m_IntList;
    dst.insert(dst.end(), ids.begin(), ids.end());
}


void CTSE_Chunk_Info::x_AddFeat_ids(const SAnnotTypeSelector& type,
                                    const TFeatIdStrList& ids)
{
    m_ExplicitFeatIds = true;
    TFeatIdStrList& dst = m_FeatIds[type].m_StrList;
    dst.insert(dst.end(), ids.begin(), ids.end());
}


void CTSE_Chunk_Info::x_AddXref_ids(const SAnnotTypeSelector& type,
                                    const TFeatIdStrList& ids)
{
    m_ExplicitFeatIds = true;
    TFeatIdStrList& dst = m_XrefIds[type].m_StrList;
    dst.insert(dst.end(), ids.begin(), ids.end());
}


/////////////////////////////////////////////////////////////////////////////
// annot index maintainance
void CTSE_Chunk_Info::x_EnableAnnotIndex(void)
{
    if ( !m_AnnotIndexEnabled ) {
        // enable index
        if ( !m_AnnotContents.empty() ) {
            m_SplitInfo->x_UpdateAnnotIndex(*this);
        }
        else {
            m_AnnotIndexEnabled = true;
        }
    }
    _ASSERT(m_AnnotIndexEnabled || IsLoaded());
}


void CTSE_Chunk_Info::x_DisableAnnotIndexWhenLoaded(void)
{
    _ASSERT(IsLoaded());
    m_AnnotIndexEnabled = false;
    _ASSERT(!m_AnnotIndexEnabled);
}


void CTSE_Chunk_Info::x_UpdateAnnotIndex(CTSE_Info& tse)
{
    x_UpdateAnnotIndexContents(tse);
}


void CTSE_Chunk_Info::x_InitObjectIndexList(void)
{
    if ( !m_ObjectIndexList.empty() ) {
        return;
    }

    ITERATE ( TAnnotContents, it, m_AnnotContents ) {
        m_ObjectIndexList.push_back(TObjectIndex(it->first));
        TObjectIndex& infos = m_ObjectIndexList.back();
        _ASSERT(infos.GetName() == it->first);
        ITERATE ( TAnnotTypes, tit, it->second ) {
            infos.AddInfo(CAnnotObject_Info(*this, tit->first));
            CAnnotObject_Info& info = infos.GetInfos().back();
            _ASSERT(info.IsChunkStub() && &info.GetChunk_Info() == this);
            _ASSERT(info.GetTypeSelector() == tit->first);
            SAnnotObject_Key key;
            SAnnotObject_Index index;
            index.m_AnnotObject_Info = &info;
            size_t keys_begin = infos.GetKeys().size();
            ITERATE ( TLocationSet, lit, tit->second ) {
                key.m_Handle = lit->first;
                key.m_Range = lit->second;
                if ( key.m_Range.Empty() ) {
                    string name;
                    if ( it->first.IsNamed() ) {
                        name = " "+it->first.GetName();
                    }
                    ERR_POST_X(11, "Empty region in split-info on "<<
                               key.m_Handle<<name<<" "<<
                               GetSplitInfo().GetBlobId());
                    // do not believe empty range - it must be an error
                    key.m_Range = key.m_Range.GetWhole();
                }
                infos.AddMap(key, index);
            }
            size_t keys_end = infos.GetKeys().size();
            if ( keys_begin+1 == keys_end &&
                 infos.GetKey(keys_begin).IsSingle() ) {
                info.SetKey(infos.GetKey(keys_begin));
                infos.RemoveLastMap();
            }
            else {
                info.SetKeys(keys_begin, keys_end);
            }
        }
        infos.PackKeys();
        infos.SetIndexed();
    }
}


static
bool x_HasFeatType(const CTSE_Chunk_Info::TAnnotTypes& types,
                   CSeqFeatData::E_Choice type)
{
    if ( type == CSeqFeatData::e_not_set ) {
        return !types.empty();
    }
    if ( types.find(SAnnotTypeSelector(type)) != types.end() ) {
        return true;
    }
    CAnnotType_Index::TIndexRange range =
        CAnnotType_Index::GetFeatTypeRange(type);
    for ( size_t index = range.first; index < range.second; ++index ) {
        CSeqFeatData::ESubtype subtype =
            CAnnotType_Index::GetSubtypeForIndex(index);
        if ( types.find(SAnnotTypeSelector(subtype)) != types.end() ) {
            return true;
        }
    }
    return false;
}


static
bool x_HasFeatType(const CTSE_Chunk_Info::TAnnotTypes& types,
                   CSeqFeatData::ESubtype subtype)
{
    if ( subtype == CSeqFeatData::eSubtype_any ) {
        return !types.empty();
    }
    if ( types.find(SAnnotTypeSelector(subtype)) != types.end() ) {
        return true;
    }
    CSeqFeatData::E_Choice type = CSeqFeatData::GetTypeFromSubtype(subtype);
    if ( types.find(SAnnotTypeSelector(type)) != types.end() ) {
        return true;
    }
    return false;
}


static
bool x_HasFeatIds(const CTSE_Chunk_Info::TFeatIdsMap& types,
                  CSeqFeatData::E_Choice type)
{
    if ( type == CSeqFeatData::e_not_set ) {
        return !types.empty();
    }
    if ( types.find(SAnnotTypeSelector(type)) != types.end() ) {
        return true;
    }
    CAnnotType_Index::TIndexRange range =
        CAnnotType_Index::GetFeatTypeRange(type);
    for ( size_t index = range.first; index < range.second; ++index ) {
        CSeqFeatData::ESubtype subtype =
            CAnnotType_Index::GetSubtypeForIndex(index);
        if ( types.find(SAnnotTypeSelector(subtype)) != types.end() ) {
            return true;
        }
    }
    return false;
}


static
bool x_HasFeatIds(const CTSE_Chunk_Info::TFeatIdsMap& types,
                  CSeqFeatData::ESubtype subtype)
{
    if ( subtype == CSeqFeatData::eSubtype_any ) {
        return !types.empty();
    }
    if ( types.find(SAnnotTypeSelector(subtype)) != types.end() ) {
        return true;
    }
    CSeqFeatData::E_Choice type = CSeqFeatData::GetTypeFromSubtype(subtype);
    if ( types.find(SAnnotTypeSelector(type)) != types.end() ) {
        return true;
    }
    return false;
}


bool CTSE_Chunk_Info::x_ContainsFeatType(CSeqFeatData::E_Choice type) const
{
    ITERATE ( TAnnotContents, it, m_AnnotContents ) {
        if ( x_HasFeatType(it->second, type) ) {
            return true;
        }
    }
    return false;
}


bool CTSE_Chunk_Info::x_ContainsFeatType(CSeqFeatData::ESubtype subtype) const
{
    ITERATE ( TAnnotContents, it, m_AnnotContents ) {
        if ( x_HasFeatType(it->second, subtype) ) {
            return true;
        }
    }
    return false;
}


bool CTSE_Chunk_Info::x_ContainsFeatIds(CSeqFeatData::E_Choice type,
                                        EFeatIdType id_type) const
{
    if ( !x_ContainsFeatType(type) ) {
        return false;
    }
    if ( !m_ExplicitFeatIds ) {
        return true;
    }
    return x_HasFeatIds(id_type == eFeatId_id? m_FeatIds: m_XrefIds, type);
}


bool CTSE_Chunk_Info::x_ContainsFeatIds(CSeqFeatData::ESubtype subtype,
                                        EFeatIdType id_type) const
{
    if ( !x_ContainsFeatType(subtype) ) {
        return false;
    }
    if ( !m_ExplicitFeatIds ) {
        return true;
    }
    return x_HasFeatIds(id_type == eFeatId_id? m_FeatIds: m_XrefIds, subtype);
}


void CTSE_Chunk_Info::x_UpdateAnnotIndexContents(CTSE_Info& tse)
{
    x_InitObjectIndexList();

    SAnnotObject_Index index;
    ITERATE ( TObjectIndexList, it, m_ObjectIndexList ) {
        CTSEAnnotObjectMapper mapper(tse, it->GetName());
        ITERATE ( SAnnotObjectsIndex::TObjectInfos, info, it->GetInfos() ) {
            index.m_AnnotObject_Info = const_cast<CAnnotObject_Info*>(&*info);
            if ( info->HasSingleKey() ) {
                mapper.Map(info->GetKey(), index);
            }
            else {
                for ( size_t i = info->GetKeysBegin();
                      i < info->GetKeysEnd(); ++i ) {
                    mapper.Map(it->GetKey(i), index);
                }
            }
        }
    }

    if ( m_ExplicitFeatIds ) {
        ITERATE ( TFeatIdsMap, it, m_FeatIds ) {
            ITERATE ( TFeatIdIntList, it2, it->second.m_IntList ) {
                tse.x_MapChunkByFeatId(*it2, it->first, GetChunkId(), eFeatId_id);
            }
            ITERATE ( TFeatIdStrList, it2, it->second.m_StrList ) {
                tse.x_MapChunkByFeatId(*it2, it->first, GetChunkId(), eFeatId_id);
            }
        }
        ITERATE ( TFeatIdsMap, it, m_XrefIds ) {
            ITERATE ( TFeatIdIntList, it2, it->second.m_IntList ) {
                tse.x_MapChunkByFeatId(*it2, it->first, GetChunkId(), eFeatId_xref);
            }
            ITERATE ( TFeatIdStrList, it2, it->second.m_StrList ) {
                tse.x_MapChunkByFeatId(*it2, it->first, GetChunkId(), eFeatId_xref);
            }
        }
    }
    else {
        ITERATE ( TAnnotContents, it, m_AnnotContents ) {
            ITERATE ( TAnnotTypes, it2, it->second ) {
                const SAnnotTypeSelector& type = it2->first;
                if ( type.GetAnnotType() == CSeq_annot::C_Data::e_Ftable ) {
                    tse.x_MapChunkByFeatType(type, GetChunkId());
                }
            }
        }
    }
}


void CTSE_Chunk_Info::x_DropAnnotObjects(void)
{
     m_ObjectIndexList.clear();
}


/////////////////////////////////////////////////////////////////////////////
// interface load methods
void CTSE_Chunk_Info::x_LoadDescr(const TPlace& place,
                                  const CSeq_descr& descr)
{
    _ASSERT(x_Attached());
    m_SplitInfo->x_LoadDescr(place, descr);
}


void CTSE_Chunk_Info::x_LoadAnnot(const TPlace& place,
                                  const CSeq_annot& annot)
{
    _ASSERT(x_Attached());
    m_SplitInfo->x_LoadAnnot(place, annot, GetChunkId());
}


void CTSE_Chunk_Info::x_LoadBioseq(const TPlace& place,
                                   const CBioseq& bioseq)
{
    _ASSERT(x_Attached());
    m_SplitInfo->x_LoadBioseq(place, bioseq);
}


void CTSE_Chunk_Info::x_LoadSequence(const TPlace& place, TSeqPos pos,
                                     const TSequence& sequence)
{
    _ASSERT(x_Attached());
    m_SplitInfo->x_LoadSequence(place, pos, sequence);
}


void CTSE_Chunk_Info::x_LoadAssembly(const TBioseqId& seq_id,
                                     const TAssembly& assembly)
{
    _ASSERT(x_Attached());
    m_SplitInfo->x_LoadAssembly(seq_id, assembly);
}


void CTSE_Chunk_Info::x_LoadSeq_entry(CSeq_entry& entry,
                                      CTSE_SetObjectInfo* set_info)
{
    _ASSERT(x_Attached());
    m_SplitInfo->x_LoadSeq_entry(entry, set_info);
}


END_SCOPE(objects)
END_NCBI_SCOPE
