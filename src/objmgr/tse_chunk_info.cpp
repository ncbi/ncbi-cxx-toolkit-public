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
#include <objmgr/impl/tse_split_info.hpp>
#include <objmgr/impl/tse_info.hpp>
#include <objmgr/impl/seq_annot_info.hpp>
#include <objmgr/impl/bioseq_info.hpp>
#include <objmgr/impl/bioseq_set_info.hpp>
#include <objmgr/impl/data_source.hpp>
#include <objmgr/impl/annot_object.hpp>
#include <objects/seq/Seq_literal.hpp>
#include <objmgr/seq_map.hpp>
#include <algorithm>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

/////////////////////////////////////////////////////////////////////////////
// CTSE_Chunk_Info
/////////////////////////////////////////////////////////////////////////////


CTSE_Chunk_Info::CTSE_Chunk_Info(TChunkId id)
    : m_SplitInfo(0),
      m_ChunkId(id),
      m_AnnotIndexEnabled(false)
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

    // register annots places
    ITERATE ( TPlaces, it, m_AnnotPlaces ) {
        split_info.x_AddAnnotPlace(*it, chunk_id);
    }

    // register bioseq ids
    {{
        set<CSeq_id_Handle> ids;
        sort(m_BioseqIds.begin(), m_BioseqIds.end());
        ITERATE ( TBioseqIds, it, m_BioseqIds ) {
            split_info.x_SetContainedId(*it, chunk_id);
            _VERIFY(ids.insert(*it).second);
        }
        ITERATE ( TAnnotContents, it, m_AnnotContents ) {
            ITERATE ( TAnnotTypes, tit, it->second ) {
                ITERATE ( TLocationSet, lit, tit->second ) {
                    if ( ids.insert(lit->first).second ) {
                        split_info.x_SetContainedId(lit->first, chunk_id);
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
void CTSE_Chunk_Info::x_TSEAttach(CTSE_Info& tse_info)
{
    _ASSERT(x_Attached());

    TChunkId chunk_id = GetChunkId();

    // register descrs places
    ITERATE ( TDescInfos, it, m_DescInfos ) {
        m_SplitInfo->x_AddDescInfo(tse_info, *it, chunk_id);
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
        
        // first count object infos to store
        size_t count = 0;
        ITERATE ( TAnnotTypes, tit, it->second ) {
            count += tit->second.size();
        }
        infos.ReserveInfoSize(count);
        
        ITERATE ( TAnnotTypes, tit, it->second ) {
            infos.AddInfo(CAnnotObject_Info(*this, tit->first));
        }
    }

    // fill keys
    TObjectIndexList::iterator list_iter = m_ObjectIndexList.begin();
    ITERATE ( TAnnotContents, it, m_AnnotContents ) {
        _ASSERT(list_iter != m_ObjectIndexList.end());
        TObjectIndex& infos = *list_iter++;
        _ASSERT(infos.GetName() == it->first);
        
        size_t info_index = 0;
        ITERATE ( TAnnotTypes, tit, it->second ) {
            CAnnotObject_Info& info = infos.GetInfo(info_index++);
            _ASSERT(info.IsChunkStub() && &info.GetChunk_Info() == this);
            _ASSERT(info.GetAnnotType() == tit->first.GetAnnotType());
            _ASSERT(info.GetFeatType() == tit->first.GetFeatType());
            _ASSERT(info.GetFeatSubtype() == tit->first.GetFeatSubtype());
            SAnnotObject_Key key;
            SAnnotObject_Index index;
            key.m_AnnotObject_Info = index.m_AnnotObject_Info = &info;
            ITERATE ( TLocationSet, lit, tit->second ) {
                key.m_Handle = lit->first;
                key.m_Range = lit->second;
                infos.AddMap(key, index);
            }
        }
        infos.PackKeys();
        infos.PackIndices();
    }
}


void CTSE_Chunk_Info::x_UpdateAnnotIndexContents(CTSE_Info& tse)
{
    x_InitObjectIndexList();
    ITERATE ( TObjectIndexList, it, m_ObjectIndexList ) {
        tse.x_MapAnnotObjects(*it);
    }
}


void CTSE_Chunk_Info::x_UnmapAnnotObjects(CTSE_Info& tse)
{
    ITERATE ( TObjectIndexList, it, m_ObjectIndexList ) {
        tse.x_UnmapAnnotObjects(*it);
    }
}


void CTSE_Chunk_Info::x_DropAnnotObjects(CTSE_Info& /*tse*/)
{
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
                                  CRef<CSeq_annot_Info> annot)
{
    _ASSERT(x_Attached());
    m_SplitInfo->x_LoadAnnot(place, annot);
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


void CTSE_Chunk_Info::x_LoadAssembly(const TPlace& place,
                                     const TAssembly& assembly)
{
    _ASSERT(x_Attached());
    m_SplitInfo->x_LoadAssembly(place, assembly);
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.21  2005/03/15 19:14:27  vasilche
* Correctly update and check  bioseq ids in split blobs.
*
* Revision 1.20  2004/12/22 15:56:27  vasilche
* Use SAnnotObjectsIndex.
*
* Revision 1.19  2004/10/18 13:59:22  vasilche
* Added support for split history assembly.
* Added support for split non-gi sequences.
*
* Revision 1.18  2004/10/12 14:31:48  vasilche
* Fixed assertion expression - blob can be loaded already.
*
* Revision 1.17  2004/10/07 14:03:32  vasilche
* Use shared among TSEs CTSE_Split_Info.
* Use typedefs and methods for TSE and DataSource locking.
* Load split CSeqdesc on the fly in CSeqdesc_CI.
*
* Revision 1.16  2004/08/31 14:26:14  vasilche
* Postpone indexing of split blobs.
*
* Revision 1.15  2004/08/19 16:54:56  vasilche
* Treat Bioseq-set zero as anonymous set.
*
* Revision 1.14  2004/08/19 14:20:58  vasilche
* Added splitting of whole Bioseqs.
*
* Revision 1.13  2004/08/17 15:56:34  vasilche
* Use load mutex and correctly lock CDataSource mutex while loading chunk.
*
* Revision 1.12  2004/08/04 14:53:26  vasilche
* Revamped object manager:
* 1. Changed TSE locking scheme
* 2. TSE cache is maintained by CDataSource.
* 3. CObjectManager::GetInstance() doesn't hold CRef<> on the object manager.
* 4. Fixed processing of split data.
*
* Revision 1.11  2004/07/12 16:57:32  vasilche
* Fixed loading of split Seq-descr and Seq-data objects.
* They are loaded correctly now when GetCompleteXxx() method is called.
*
* Revision 1.10  2004/06/15 14:06:49  vasilche
* Added support to load split sequences.
*
* Revision 1.9  2004/05/21 21:42:13  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.8  2004/03/26 19:42:04  vasilche
* Fixed premature deletion of SNP annot info object.
* Removed obsolete references to chunk info.
*
* Revision 1.7  2004/03/16 15:47:28  vasilche
* Added CBioseq_set_Handle and set of EditHandles
*
* Revision 1.6  2004/01/22 20:10:41  vasilche
* 1. Splitted ID2 specs to two parts.
* ID2 now specifies only protocol.
* Specification of ID2 split data is moved to seqsplit ASN module.
* For now they are still reside in one resulting library as before - libid2.
* As the result split specific headers are now in objects/seqsplit.
* 2. Moved ID2 and ID1 specific code out of object manager.
* Protocol is processed by corresponding readers.
* ID2 split parsing is processed by ncbi_xreader library - used by all readers.
* 3. Updated OBJMGR_LIBS correspondingly.
*
* Revision 1.5  2003/11/26 18:59:56  ucko
* Remove stray semicolon after BEGIN_SCOPE(objects) to fix the WorkShop build.
*
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
