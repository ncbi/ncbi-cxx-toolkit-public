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
#include <objmgr/seq_id_translator.hpp>
#include <objmgr/objmgr_exception.hpp>

#include <objects/seq/Seq_literal.hpp>

#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Packed_seqint.hpp>
#include <objects/seqloc/Seq_point.hpp>
#include <objects/seqloc/Packed_seqpnt.hpp>
#include <objects/seqloc/Seq_loc_mix.hpp>
#include <objects/seqloc/Seq_loc_equiv.hpp>
#include <objects/seqloc/Seq_bond.hpp>

#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objects/seqalign/Dense_diag.hpp>
#include <objects/seqalign/Std_seg.hpp>
#include <objects/seqalign/Packed_seg.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/seqres/Seq_graph.hpp>

#include <objects/seq/Seq_annot.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seqset/Seq_entry.hpp>


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
    if (!m_SeqIdTranslator)
        x_GetBase(tse, info.second).x_AddDescrChunkId(info.first, chunk_id);
    else {
        x_GetBase(tse, PatchSeqId(info.second, *m_SeqIdTranslator))
                  .x_AddDescrChunkId(info.first, chunk_id);
    }
}

void CTSE_Default_Assigner::AddAnnotPlace(CTSE_Info& tse, 
                                          const TPlace& place, 
                                          TChunkId chunk_id)
{
    if (!m_SeqIdTranslator)
        x_GetBase(tse, place).x_AddAnnotChunkId(chunk_id);
    else {
        x_GetBase(tse, PatchSeqId(place, *m_SeqIdTranslator))
                  .x_AddAnnotChunkId(chunk_id);
    }
    
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
        if (!m_SeqIdTranslator)
            bioseq = &x_GetBioseq(tse, it->first);
        else 
            bioseq = &x_GetBioseq(tse, PatchSeqId(it->first, *m_SeqIdTranslator));
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
    if (!m_SeqIdTranslator)
        x_GetBioseq(tse, info).x_AddAssemblyChunkId(chunk_id);
    else { 
        x_GetBioseq(tse, PatchSeqId(info, *m_SeqIdTranslator))
            .x_AddAssemblyChunkId(chunk_id);
    }
}

void CTSE_Default_Assigner::UpdateAnnotIndex(CTSE_Info& tse, 
                                             CTSE_Chunk_Info& chunk)
{
    CDataSource::TAnnotLockWriteGuard guard1(tse.GetDataSource());
    CTSE_Info::TAnnotLockWriteGuard guard2(tse.GetAnnotLock());          
    chunk.x_UpdateAnnotIndex(tse);
}

    // loading results
void CTSE_Default_Assigner::LoadDescr(CTSE_Info& tse, 
                                      const TPlace& place, 
                                      const CSeq_descr& descr)
{
    if (!m_SeqIdTranslator)
        x_GetBase(tse, place).AddSeq_descr(descr);
    else {
        x_GetBase(tse, PatchSeqId(place, *m_SeqIdTranslator))
            .AddSeq_descr(descr);
    }
}
void CTSE_Default_Assigner::LoadAnnot(CTSE_Info& tse,
                                      const TPlace& place, 
                                      CRef<CSeq_annot_Info> annot)
{
    {{
        CDataSource::TMainLock::TWriteLockGuard guard
            (tse.GetDataSource().GetMainLock());
        if (!m_SeqIdTranslator)
            x_GetBase(tse, place).AddAnnot(annot);
        else {
            CRef<CSeq_annot> patched_annot = PatchSeqId(*annot->GetSeq_annotSkeleton(), 
                                                        *m_SeqIdTranslator);
            CRef<CSeq_annot_Info> patched_info( new CSeq_annot_Info(*patched_annot) );
            x_GetBase(tse, PatchSeqId(place, *m_SeqIdTranslator))
                .AddAnnot(patched_info);
        }

    }}
    {{
        CDataSource::TAnnotLockWriteGuard guard(tse.GetDataSource());
        tse.UpdateAnnotIndex(*annot);
    }}
}
void CTSE_Default_Assigner::LoadBioseq(CTSE_Info& tse,
                                       const TPlace& place, 
                                       CRef<CSeq_entry_Info> entry)
{
    {{
        CDataSource::TMainLock::TWriteLockGuard guard
            (tse.GetDataSource().GetMainLock());
        if (place == TPlace(CSeq_id_Handle(), kTSE_Place_id)) {
            tse.x_SetObject(*entry, 0); //???
        }
        else {
            if (!m_SeqIdTranslator)
                x_GetBioseq_set(tse, place).AddEntry(entry);
            else {
                CRef<CSeq_entry> patched_entry = PatchSeqId(*entry->GetSeq_entrySkeleton(),
                                                            *m_SeqIdTranslator);
                CRef<CSeq_entry_Info> patched_info(new CSeq_entry_Info(*patched_entry));
                x_GetBioseq_set(tse, PatchSeqId(place, *m_SeqIdTranslator))
                    .AddEntry(patched_info);
            }
        }
    }}

}
void CTSE_Default_Assigner::LoadSequence(CTSE_Info& tse, const TPlace& place, 
                                         TSeqPos pos, const TSequence& sequence)
{
    CSeqMap* seq_map = 0;
    if (!m_SeqIdTranslator)
        seq_map = const_cast<CSeqMap*>(&x_GetBioseq(tse, place).GetSeqMap());
    else {
        seq_map = const_cast<CSeqMap*>(&x_GetBioseq(tse, PatchSeqId(place, *m_SeqIdTranslator))
                                       .GetSeqMap());
    }
    ITERATE ( TSequence, it, sequence ) {
        const CSeq_literal& literal = **it;
        seq_map->LoadSeq_data(pos, literal.GetLength(), literal.GetSeq_data());
        pos += literal.GetLength();
    }
}
void CTSE_Default_Assigner::LoadAssembly(CTSE_Info& tse,
                                         const TBioseqId& seq_id,
                                         const TAssembly& assembly)
{
    if (!m_SeqIdTranslator)
        x_GetBioseq(tse, seq_id).SetInst_Hist_Assembly(assembly);
    else {
        TAssembly patched_assembly(assembly);
        PatchSeqIds(patched_assembly, *m_SeqIdTranslator);
        x_GetBioseq(tse, PatchSeqId(seq_id, *m_SeqIdTranslator))
            .SetInst_Hist_Assembly(patched_assembly);
    }
}
void CTSE_Default_Assigner::LoadSeq_entry(CTSE_Info& tse,
                                          CSeq_entry& entry, 
                                          CTSE_SNP_InfoMap* snps)
{
    if (!m_SeqIdTranslator)
        tse.SetSeq_entry(entry, snps);
    else {       
        CRef<CSeq_entry> patched = PatchSeqId(entry, *m_SeqIdTranslator);
        tse.SetSeq_entry( *patched, snps);
    }
}

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

CRef<CSeq_feat> PatchSeqId(const CSeq_feat& feat, const ISeq_id_Translator& tr)
{
    CRef<CSeq_feat> ret(new CSeq_feat);
    ret->Assign(feat);
    if (ret->IsSetProduct())
        ret->SetProduct( *PatchSeqId(ret->GetProduct(), tr));
    if (ret->IsSetLocation())
        ret->SetLocation( *PatchSeqId(ret->GetLocation(), tr));
    if (ret->IsSetData()) {
        CRef<CSeqFeatData> fd(new CSeqFeatData);
        fd->Assign(ret->GetData());
        if( ret->GetData().IsSeq() )
            fd->SetSeq( *PatchSeqId(ret->GetData().GetSeq(), tr));
        ret->SetData( *fd );
    }
    return ret;
}
/////////////////////////////////////////////////////////////////////////////

template<class T>
inline CRef<T> PatchSeqId_CallSetIds(const T& orig, const ISeq_id_Translator& tr)
{
    CRef<T> ret(new T);
    ret->Assign(orig);
    if (ret->IsSetIds())
        PatchSeqIds(ret->SetIds(), tr);
    return ret;   
}
template<class T>
inline CRef<T> PatchSeqId_CallSetId(const T& orig, const ISeq_id_Translator& tr)
{
    CRef<T> ret(new T);
    ret->Assign(orig);
    if (ret->IsSetId())
        ret->SetId( *PatchSeqId(ret->GetId(), tr));
    return ret;   
}

template<class T>
inline CRef<T> PatchSeqId_CallSet(const T& orig, const ISeq_id_Translator& tr)
{
    CRef<T> ret(new T);
    ret->Assign(orig);
    if (ret->IsSet())
        PatchSeqIds(ret->Set(), tr);
    return ret;   
}

inline
CRef<CSeq_interval> PatchSeqId(const CSeq_interval& orig, const ISeq_id_Translator& tr)
{
    return PatchSeqId_CallSetId(orig, tr);
}

inline
CRef<CPacked_seqint> PatchSeqId(const CPacked_seqint& orig, const ISeq_id_Translator& tr)
{
    return PatchSeqId_CallSet(orig, tr);
}
inline
CRef<CSeq_point> PatchSeqId(const CSeq_point& orig, const ISeq_id_Translator& tr)
{
    return PatchSeqId_CallSetId(orig, tr);
}
inline
CRef<CPacked_seqpnt> PatchSeqId(const CPacked_seqpnt& orig, const ISeq_id_Translator& tr)
{
    return PatchSeqId_CallSetId(orig, tr);
}
inline
CRef<CSeq_loc_mix> PatchSeqId(const CSeq_loc_mix& orig, const ISeq_id_Translator& tr)
{
    return PatchSeqId_CallSet(orig, tr);
}
inline
CRef<CSeq_loc_equiv> PatchSeqId(const CSeq_loc_equiv& orig, const ISeq_id_Translator& tr)
{
    return PatchSeqId_CallSet(orig, tr);
}
inline 
CRef<CSeq_bond> PatchSeqId(const CSeq_bond& orig, const ISeq_id_Translator& tr)
{
    CRef<CSeq_bond> ret(new CSeq_bond);
    ret->Assign(orig);
    if (ret->IsSetA())
        ret->SetA( *PatchSeqId(ret->GetA(), tr));
    if (ret->IsSetB())
        ret->SetB( *PatchSeqId(ret->GetB(), tr));
    return ret;   
}


CRef<CSeq_loc> PatchSeqId(const CSeq_loc& orig, const ISeq_id_Translator& tr)
{
    CRef<CSeq_loc> ret(new CSeq_loc);
    ret->Assign(orig);
    ret->InvalidateIdCache();
    if (ret->IsEmpty())
        ret->SetEmpty( *PatchSeqId(ret->GetEmpty(), tr));
    if (ret->IsWhole())
        ret->SetWhole( *PatchSeqId(ret->GetWhole(), tr));
    if (ret->IsInt())
        ret->SetInt( *PatchSeqId(ret->GetInt(), tr));
    if (ret->IsPacked_int())
        ret->SetPacked_int( *PatchSeqId(ret->GetPacked_int(), tr));
    if (ret->IsPnt()) 
        ret->SetPnt( *PatchSeqId(ret->GetPnt(), tr));
    if (ret->IsPacked_pnt())
        ret->SetPacked_pnt( *PatchSeqId(ret->GetPacked_pnt(), tr));
    if (ret->IsMix())
        ret->SetMix( *PatchSeqId(ret->GetMix(), tr));
    if (ret->IsEquiv())
        ret->SetEquiv( *PatchSeqId(ret->GetEquiv(), tr));
    if (ret->IsBond())
        ret->SetBond( *PatchSeqId(ret->GetBond(), tr));
    return ret;
}

inline 
CRef<CDense_diag> PatchSeqId(const CDense_diag& orig, const ISeq_id_Translator& tr)
{
    return PatchSeqId_CallSetIds(orig, tr);
}

inline 
CRef<CDense_seg> PatchSeqId(const CDense_seg& orig, const ISeq_id_Translator& tr)
{
    return PatchSeqId_CallSetIds(orig, tr);
}

inline 
CRef<CStd_seg> PatchSeqId(const CStd_seg& orig, const ISeq_id_Translator& tr)
{
    CRef<CStd_seg> ret(new CStd_seg);
    ret->Assign(orig);
    if (ret->IsSetIds())
        PatchSeqIds(ret->SetIds(), tr);
    if (ret->IsSetLoc())
        PatchSeqIds(ret->SetLoc(), tr);  
    return ret;   
}

inline 
CRef<CPacked_seg> PatchSeqId(const CPacked_seg& orig, const ISeq_id_Translator& tr)
{
    return PatchSeqId_CallSetIds(orig, tr);
}

inline 
CRef<CSeq_align_set> PatchSeqId(const CSeq_align_set& orig, 
                                const ISeq_id_Translator& tr)
{
    return PatchSeqId_CallSet(orig, tr);
}

CRef<CSeq_align> PatchSeqId(const CSeq_align& align, const ISeq_id_Translator& tr)
{
    CRef<CSeq_align> ret(new CSeq_align);
    ret->Assign(align);
    if (ret->IsSetBounds()) {
        PatchSeqIds(ret->SetBounds(), tr);
    }
    
    if (ret->IsSetSegs()) {
        CRef<CSeq_align::TSegs> segs(new CSeq_align::TSegs);
        segs->Assign(ret->GetSegs());
        if (segs->IsDendiag()) 
            PatchSeqIds(segs->SetDendiag(), tr);

        if (segs->IsDenseg()) 
            segs->SetDenseg( *PatchSeqId(segs->GetDenseg(), tr ));
        if (segs->IsStd()) 
            PatchSeqIds(segs->SetStd(), tr);
        if (segs->IsPacked()) 
            segs->SetPacked( *PatchSeqId(segs->GetPacked(), tr));
        if (segs->IsDisc()) 
            segs->SetDisc( *PatchSeqId(segs->GetDisc(), tr));
        ret->SetSegs(*segs);
    }
    return ret;
}
/////////////////////////////////////////////////////////////////////////////
CRef<CSeq_graph> PatchSeqId(const CSeq_graph& graph, 
                            const ISeq_id_Translator& tr)
{
    CRef<CSeq_graph> ret(new CSeq_graph);
    ret->Assign(graph);
    if (ret->IsSetLoc())
        ret->SetLoc( *PatchSeqId(ret->GetLoc(), tr));

    return ret;
}

/////////////////////////////////////////////////////////////////////////////
CRef<CSeq_annot> PatchSeqId(const CSeq_annot& annot, 
                            const ISeq_id_Translator& tr)
{
    CRef<CSeq_annot> ret(new CSeq_annot);
    ret->Assign(annot);
    if (ret->IsSetData()) {
        CRef<CSeq_annot::TData> data(new CSeq_annot::TData);
        data->Assign(ret->GetData());
        if (data->IsFtable()) 
            PatchSeqIds( data->SetFtable(), tr);
        if (data->IsAlign())
            PatchSeqIds( data->SetAlign(), tr);
        if (data->IsGraph())
            PatchSeqIds( data->SetGraph(), tr);
        if (data->IsIds()) 
            PatchSeqIds( data->SetIds(), tr);
        if (data->IsLocs())
            PatchSeqIds( data->SetLocs(), tr);

        ret->SetData(*data);
    }
    return ret;
}
/////////////////////////////////////////////////////////////////////////////

CRef<CBioseq> PatchSeqId(const CBioseq& bioseq, const ISeq_id_Translator& tr)
{
    CRef<CBioseq> ret(new CBioseq);
    ret->Assign(bioseq);
    if (ret->IsSetId())
        PatchSeqIds( ret->SetId(), tr);
    if (ret->IsSetAnnot())
        PatchSeqIds( ret->SetAnnot(), tr);
    return ret;
}

CRef<CBioseq_set> PatchSeqId(const CBioseq_set& bioseq_set, 
                             const ISeq_id_Translator& tr)
{
    CRef<CBioseq_set> ret(new CBioseq_set);
    ret->Assign(bioseq_set);
    if (ret->IsSetSeq_set())
        PatchSeqIds( ret->SetSeq_set(), tr);
    if (ret->IsSetAnnot())
        PatchSeqIds( ret->SetAnnot(), tr);
    return ret;
}

CRef<CSeq_entry> PatchSeqId(const CSeq_entry& entry, 
                            const ISeq_id_Translator& tr)
{
    CRef<CSeq_entry> ret(new CSeq_entry);
    ret->Assign(entry);
    if (ret->IsSeq())
        ret->SetSeq( *PatchSeqId(ret->GetSeq(), tr));
    if (ret->IsSet())
        ret->SetSet( *PatchSeqId(ret->GetSet(), tr));
    return ret;
    
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.2  2005/08/25 15:37:29  didenko
 * Added call to InvalidateCache() method when the CSeq_los is bieng patched
 *
 * Revision 1.1  2005/08/25 14:05:37  didenko
 * Restructured TSE loading process
 *
 *
 * ===========================================================================
 */
