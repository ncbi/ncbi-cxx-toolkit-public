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


#include <ncbi_pch.hpp>
#include <objmgr/impl/tse_chunk_info.hpp>
#include <objmgr/impl/tse_info.hpp>
#include <objmgr/impl/seq_annot_info.hpp>
#include <objmgr/impl/bioseq_info.hpp>
#include <objmgr/impl/bioseq_set_info.hpp>
#include <objmgr/impl/data_source.hpp>
#include <objmgr/impl/annot_object.hpp>
#include <objects/seq/Seq_literal.hpp>
#include <objmgr/seq_map.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

/////////////////////////////////////////////////////////////////////////////
// CTSE_Chunk_Info
/////////////////////////////////////////////////////////////////////////////


CTSE_Chunk_Info::CTSE_Chunk_Info(TChunkId id)
    : m_TSE_Info(0),
      m_ChunkId(id),
      m_DirtyAnnotIndex(true),
      m_NotLoaded(true)
{
}

CTSE_Chunk_Info::~CTSE_Chunk_Info(void)
{
}


void CTSE_Chunk_Info::x_TSEAttach(CTSE_Info& tse_info)
{
    _ASSERT(!m_TSE_Info);
    _ASSERT(tse_info.m_Chunks.find(GetChunkId()) == tse_info.m_Chunks.end());
    m_TSE_Info = &tse_info;
    tse_info.m_Chunks[GetChunkId()].Reset(this);
    tse_info.x_SetDirtyAnnotIndex();
    x_TSEAttachSeq_data();
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


void CTSE_Chunk_Info::x_UpdateAnnotIndex(CTSE_Info& tse)
{
    if ( m_DirtyAnnotIndex ) {
        x_UpdateAnnotIndexContents(tse);
        m_DirtyAnnotIndex = false;
    }
}


void CTSE_Chunk_Info::x_AddAnnotPlace(EPlaceType place_type, TPlaceId place_id)
{
    m_AnnotPlaces.push_back(TPlace(place_type, place_id));
}


void CTSE_Chunk_Info::x_AddAnnotType(const CAnnotName& annot_name,
                                     const SAnnotTypeSelector& annot_type,
                                     const TLocationId& location_id,
                                     const TLocationRange& location_range)
{
    TLocationSet& dst = m_AnnotContents[annot_name][annot_type];
    dst.push_back(TLocation(location_id, location_range));
}


void CTSE_Chunk_Info::x_AddAnnotType(const CAnnotName& annot_name,
                                     const SAnnotTypeSelector& annot_type,
                                     const TLocationSet& location)
{
    TLocationSet& dst = m_AnnotContents[annot_name][annot_type];
    dst.insert(dst.end(), location.begin(), location.end());
}


void CTSE_Chunk_Info::x_AddSeq_data(const TLocationSet& location)
{
    m_Seq_data.insert(m_Seq_data.end(), location.begin(), location.end());
}


void CTSE_Chunk_Info::x_UpdateAnnotIndexContents(CTSE_Info& tse)
{
    ITERATE ( TAnnotContents, it, m_AnnotContents ) {
        m_ObjectInfosList.push_back(TObjectInfos(it->first));
        TObjectInfos& infos = m_ObjectInfosList.back();
        _ASSERT(infos.GetName() == it->first);
        
        // first count object infos to store
        size_t count = 0;
        ITERATE ( TAnnotTypes, tit, it->second ) {
            count += tit->second.size();
        }
        infos.Reserve(count);
        
        ITERATE ( TAnnotTypes, tit, it->second ) {
            CAnnotObject_Info* info =
                infos.AddInfo(CAnnotObject_Info(*this, tit->first));
            ITERATE ( TLocationSet, lit, tit->second ) {
                SAnnotObject_Key key;
                SAnnotObject_Index annotRef;
                key.m_AnnotObject_Info = annotRef.m_AnnotObject_Info = info;
                key.m_Handle = lit->first;
                key.m_Range = lit->second;
                tse.x_MapAnnotObject(key, annotRef, infos);
            }
        }
    }
}


void CTSE_Chunk_Info::x_UnmapAnnotObjects(CTSE_Info& tse)
{
    NON_CONST_ITERATE ( TObjectInfosList, it, m_ObjectInfosList ) {
        tse.x_UnmapAnnotObjects(*it);
    }
    m_ObjectInfosList.clear();
}


void CTSE_Chunk_Info::x_DropAnnotObjects(CTSE_Info& /*tse*/)
{
    m_ObjectInfosList.clear();
}


CBioseq_Base_Info& CTSE_Chunk_Info::x_GetBase(const TPlace& place)
{
    if ( place.first == eBioseq ) {
        return GetTSE_Info().GetBioseq(place.second);
    }
    else {
        return GetTSE_Info().GetBioseq_set(place.second);
    }
}


CBioseq_Info& CTSE_Chunk_Info::x_GetBioseq(const TPlace& place)
{
    if ( place.first == eBioseq ) {
        return GetTSE_Info().GetBioseq(place.second);
    }
    else {
        NCBI_THROW(CObjMgrException, eOtherError,
                   "Bioseq-set id where gi is expected");
    }
}


void CTSE_Chunk_Info::x_LoadAnnot(const TPlace& place,
                                  CRef<CSeq_annot_Info> annot)
{
    x_GetBase(place).AddAnnot(annot);
    GetTSE_Info().UpdateAnnotIndex(*annot);
}


void CTSE_Chunk_Info::x_TSEAttachSeq_data(void)
{
    ITERATE ( TLocationSet, it, m_Seq_data ) {
        const CSeq_id_Handle& id = it->first;
        const TLocationRange& range = it->second;
        CConstRef<CBioseq_Info> bioseq = GetTSE_Info().FindBioseq(id);
        if ( !bioseq ) {
            NCBI_THROW(CObjMgrException, eOtherError,
                       "Chunk-Info Seq-data has bad Seq-id: "+id.AsString());
        }
        const CSeqMap& seq_map = bioseq->GetSeqMap();
        const_cast<CSeqMap&>(seq_map).SetRegionInChunk(*this,
                                                       range.GetFrom(),
                                                       range.GetLength());
    }
}


void CTSE_Chunk_Info::x_LoadSequence(const TPlace& place, TSeqPos pos,
                                     const TSequence& sequence)
{
    if ( place.first != eBioseq ) {
        NCBI_THROW(CObjMgrException, eOtherError,
                   "cannot add Seq-data to Bioseq-set");
    }
    CSeqMap& seq_map = const_cast<CSeqMap&>(x_GetBioseq(place).GetSeqMap());
    ITERATE ( TSequence, it, sequence ) {
        const CSeq_literal& literal = **it;
        seq_map.LoadSeq_data(pos, literal.GetLength(), literal.GetSeq_data());
        pos += literal.GetLength();
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
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
