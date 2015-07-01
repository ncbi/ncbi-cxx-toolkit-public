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
* Author: Maxim Didenko
*
* File Description:
*
*/


#include <ncbi_pch.hpp>

#include <objmgr/impl/tse_info.hpp>
#include <objmgr/impl/tse_assigner.hpp>
#include <objmgr/impl/bioseq_set_info.hpp>
#include <objmgr/impl/bioseq_info.hpp>
#include <objmgr/impl/seq_annot_info.hpp>
#include <objmgr/impl/data_source.hpp>

#include <objmgr/seq_map.hpp>
#include <objmgr/objmgr_exception.hpp>

#include <objects/seq/Seq_literal.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

CBioseq_Info& ITSE_Assigner::x_GetBioseq(CTSE_Info& tse_info,
                                         const TBioseqId& place_id)
{
    return tse_info.x_GetBioseq(place_id);
}


CBioseq_set_Info& ITSE_Assigner::x_GetBioseq_set(CTSE_Info& tse_info,
                                                 TBioseq_setId place_id)
{
    return tse_info.x_GetBioseq_set(place_id);
}


CBioseq_Base_Info& ITSE_Assigner::x_GetBase(CTSE_Info& tse_info,
                                            const TPlace& place)
{
    if ( place.first ) {
        return x_GetBioseq(tse_info, place.first);
    }
    else {
        return x_GetBioseq_set(tse_info, place.second);
    }
}


CBioseq_Info& ITSE_Assigner::x_GetBioseq(CTSE_Info& tse_info,
                                         const TPlace& place)
{
    if ( place.first ) {
        return x_GetBioseq(tse_info, place.first);
    }
    else {
        NCBI_THROW(CObjMgrException, eOtherError,
                   "Bioseq-set id where gi is expected");
    }
}


CBioseq_set_Info& ITSE_Assigner::x_GetBioseq_set(CTSE_Info& tse_info,
                                                 const TPlace& place)
{
    if ( place.first ) {
        NCBI_THROW(CObjMgrException, eOtherError,
                   "Gi where Bioseq-set id is expected");
    }
    else {
        return x_GetBioseq_set(tse_info, place.second);
    }
}
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
CTSE_Default_Assigner::CTSE_Default_Assigner() 
{
}

CTSE_Default_Assigner::~CTSE_Default_Assigner()
{
}

void CTSE_Default_Assigner::AddDescInfo(CTSE_Info& tse, 
                                        const TDescInfo& info, 
                                        TChunkId chunk_id)
{
    x_GetBase(tse, info.second)
              .x_AddDescrChunkId(info.first, chunk_id);
}

void CTSE_Default_Assigner::AddAnnotPlace(CTSE_Info& tse, 
                                          const TPlace& place, 
                                          TChunkId chunk_id)
{
    x_GetBase(tse, place)
              .x_AddAnnotChunkId(chunk_id);
}
void CTSE_Default_Assigner::AddBioseqPlace(CTSE_Info& tse, 
                                           TBioseq_setId place_id, 
                                           TChunkId chunk_id)
{
    if ( place_id == kTSE_Place_id ) {
        tse.x_SetBioseqChunkId(chunk_id);
    }
    else {
        x_GetBioseq_set(tse, place_id).x_AddBioseqChunkId(chunk_id);
    }
}
void CTSE_Default_Assigner::AddSeq_data(CTSE_Info& tse,
                                        const TLocationSet& locations,
                                        CTSE_Chunk_Info& chunk)
{
    CBioseq_Info* last_bioseq = 0, *bioseq;
    ITERATE ( TLocationSet, it, locations ) {
        bioseq = &x_GetBioseq(tse, it->first);
        if (bioseq != last_bioseq) {
            // Do not add duplicate chunks to the same bioseq
            bioseq->x_AddSeq_dataChunkId(chunk.GetChunkId());
        }
        last_bioseq = bioseq;

        CSeqMap& seq_map = const_cast<CSeqMap&>(bioseq->GetSeqMap());
        seq_map.SetRegionInChunk(chunk,
                                 it->second.GetFrom(),
                                 it->second.GetLength());
    }
}

void CTSE_Default_Assigner::AddAssemblyInfo(CTSE_Info& tse, 
                                            const TAssemblyInfo& info, 
                                            TChunkId chunk_id)
{
    x_GetBioseq(tse, info)
                .x_AddAssemblyChunkId(chunk_id);
}

void CTSE_Default_Assigner::UpdateAnnotIndex(CTSE_Info& tse, 
                                             CTSE_Chunk_Info& chunk)
{
    CDataSource::TAnnotLockWriteGuard guard1(eEmptyGuard);
    if( tse.HasDataSource() )
        guard1.Guard(tse.GetDataSource());
    CTSE_Info::TAnnotLockWriteGuard guard2(tse.GetAnnotLock());          
    chunk.x_UpdateAnnotIndex(tse);
}

    // loading results
void CTSE_Default_Assigner::LoadDescr(CTSE_Info& tse, 
                                      const TPlace& place, 
                                      const CSeq_descr& descr)
{
    x_GetBase(tse, place).AddSeq_descr(descr);
}

void CTSE_Default_Assigner::LoadAnnot(CTSE_Info& tse,
                                      const TPlace& place, 
                                      CRef<CSeq_annot> annot,
                                      int chunk_id)
{
    CRef<CSeq_annot_Info> annot_info;
    {{
        CDataSource::TMainLock::TWriteLockGuard guard(eEmptyGuard);
        if( tse.HasDataSource() )
            guard.Guard(tse.GetDataSource().GetMainLock());
        annot_info.Reset(x_GetBase(tse, place).AddAnnot(*annot, chunk_id));
    }}
    {{
        CDataSource::TAnnotLockWriteGuard guard(eEmptyGuard);
        if( tse.HasDataSource() )
            guard.Guard(tse.GetDataSource());
        //tse.UpdateAnnotIndex(*annot_info);
    }}
}

void CTSE_Default_Assigner::LoadBioseq(CTSE_Info& tse,
                                       const TPlace& place, 
                                       CRef<CSeq_entry> entry)
{
    CRef<CSeq_entry_Info> entry_info;
    {{
        CDataSource::TMainLock::TWriteLockGuard guard(eEmptyGuard);
        if( tse.HasDataSource() )
            guard.Guard(tse.GetDataSource().GetMainLock());
        if (place == TPlace(CSeq_id_Handle(), kTSE_Place_id)) {
            entry_info = new CSeq_entry_Info(*entry);
            tse.x_SetObject(*entry_info, 0); //???
        }
        else {
            entry_info = x_GetBioseq_set(tse, place).AddEntry(*entry);
        }
    }}
    if ( !entry_info->x_GetBaseInfo().GetAnnot().empty() ) {
        CDataSource::TAnnotLockWriteGuard guard(eEmptyGuard);
        if( tse.HasDataSource() )
            guard.Guard(tse.GetDataSource());
        //tse.UpdateAnnotIndex(*entry_info);
    }
}

void CTSE_Default_Assigner::LoadSequence(CTSE_Info& tse, 
                                         const TPlace& place, 
                                         TSeqPos pos, 
                                         const TSequence& sequence)
{
    CSeqMap& seq_map = const_cast<CSeqMap&>(x_GetBioseq(tse, place).GetSeqMap());;
    ITERATE ( TSequence, it, sequence ) {
        const CSeq_literal& literal = **it;
        seq_map.LoadSeq_data(pos, literal.GetLength(), literal.GetSeq_data());
        pos += literal.GetLength();
    }
}

void CTSE_Default_Assigner::LoadAssembly(CTSE_Info& tse,
                                         const TBioseqId& seq_id,
                                         const TAssembly& assembly)
{
    x_GetBioseq(tse, seq_id).SetInst_Hist_Assembly(assembly);
}

void CTSE_Default_Assigner::LoadSeq_entry(CTSE_Info& tse,
                                          CSeq_entry& entry, 
                                          CTSE_SetObjectInfo* set_info)
{
    tse.SetSeq_entry(entry, set_info);
}


/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////


END_SCOPE(objects)
END_NCBI_SCOPE
