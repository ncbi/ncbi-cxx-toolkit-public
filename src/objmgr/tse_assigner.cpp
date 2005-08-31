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

CSeq_id_Handle ITSE_Assigner::PatchId(const CSeq_id_Handle& orig) const
{
    if (!m_SeqIdTranslator)
        return orig;
    return PatchSeqId(orig, *m_SeqIdTranslator);
}
ITSE_Assigner::TPlace ITSE_Assigner::PatchId(const TPlace& orig) const
{
    if (!m_SeqIdTranslator)
        return orig;
    return PatchSeqId(orig, *m_SeqIdTranslator);
}
void ITSE_Assigner::PatchId(CSeq_entry& orig) const
{
    if (m_SeqIdTranslator)
        PatchSeqId(orig, *m_SeqIdTranslator);
}

void ITSE_Assigner::PatchId(CSeq_annot& orig) const
{
    if (m_SeqIdTranslator)
        PatchSeqId_Copy(orig, *m_SeqIdTranslator);
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
    x_GetBase(tse, PatchId(info.second))
              .x_AddDescrChunkId(info.first, chunk_id);
}

void CTSE_Default_Assigner::AddAnnotPlace(CTSE_Info& tse, 
                                          const TPlace& place, 
                                          TChunkId chunk_id)
{
    x_GetBase(tse, PatchId(place))
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
        bioseq = &x_GetBioseq(tse, PatchId(it->first));
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
    x_GetBioseq(tse, PatchId(info))
                .x_AddAssemblyChunkId(chunk_id);
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
    LoadDescr_NoPatch(tse, PatchId(place), descr);
}

void CTSE_Default_Assigner::LoadDescr_NoPatch(CTSE_Info& tse, 
                                              const TPlace& place, 
                                              const CSeq_descr& descr)
{
    x_GetBase(tse, place).AddSeq_descr(descr);
}

void CTSE_Default_Assigner::LoadAnnot(CTSE_Info& tse,
                                      const TPlace& place, 
                                      CRef<CSeq_annot> annot)
{
    PatchId(*annot);
    LoadAnnot_NoPatch(tse, PatchId(place), annot);
}

void CTSE_Default_Assigner::LoadAnnot_NoPatch(CTSE_Info& tse,
                                              const TPlace& place, 
                                              CRef<CSeq_annot> annot)
{
    CRef<CSeq_annot_Info> annot_info;
    {{
        CDataSource::TMainLock::TWriteLockGuard guard
            (tse.GetDataSource().GetMainLock());
        annot_info.Reset(x_GetBase(tse, place).AddAnnot(*annot));
    }}
    {{
        CDataSource::TAnnotLockWriteGuard guard(tse.GetDataSource());
        tse.UpdateAnnotIndex(*annot_info);
    }}
}

void CTSE_Default_Assigner::LoadBioseq(CTSE_Info& tse,
                                       const TPlace& place, 
                                       CRef<CSeq_entry> entry)
{
    PatchId(*entry);
    LoadBioseq_NoPatch(tse, PatchId(place), entry);
}

void CTSE_Default_Assigner::LoadBioseq_NoPatch(CTSE_Info& tse,
                                               const TPlace& place, 
                                               CRef<CSeq_entry> entry)
{
    {{
        CDataSource::TMainLock::TWriteLockGuard guard
            (tse.GetDataSource().GetMainLock());
        if (place == TPlace(CSeq_id_Handle(), kTSE_Place_id)) {
            CRef<CSeq_entry_Info> entry_info(new CSeq_entry_Info(*entry));
            tse.x_SetObject(*entry_info, 0); //???
        }
        else {
            x_GetBioseq_set(tse, place).AddEntry(*entry);
        }
    }}

}

void CTSE_Default_Assigner::LoadSequence(CTSE_Info& tse, const TPlace& place, 
                                         TSeqPos pos, const TSequence& sequence)
{
    LoadSequence_NoPatch(tse, PatchId(place), pos, sequence);
}

void CTSE_Default_Assigner::LoadSequence_NoPatch(CTSE_Info& tse, 
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
                                          CTSE_SNP_InfoMap* snps)
{
    PatchId(entry);
    LoadSeq_entry_NoPatch(tse, entry, snps);
}

void CTSE_Default_Assigner::LoadSeq_entry_NoPatch(CTSE_Info& tse,
                                                  CSeq_entry& entry, 
                                                  CTSE_SNP_InfoMap* snps)
{
    tse.SetSeq_entry(entry, snps);
}

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

void PatchSeqId(CSeq_feat& feat, const ISeq_id_Translator& tr)
{
    if (feat.IsSetProduct())
        PatchSeqId(feat.SetProduct(), tr);
    if (feat.IsSetLocation())
        PatchSeqId(feat.SetLocation(), tr);
    if (feat.IsSetData()) {
        if( feat.GetData().IsSeq() )
            PatchSeqId(feat.SetData().SetSeq(), tr);
    }
}

template<class T>
inline void PatchSeqId_CallSetIds(T& orig, const ISeq_id_Translator& tr)
{
    if (orig.IsSetIds())
        PatchSeqIds(orig.SetIds(), tr);
}
template<class T>
inline void PatchSeqId_CallSetId(T& orig, const ISeq_id_Translator& tr)
{
    if (orig.IsSetId())
        PatchSeqId(orig.SetId(), tr);
}
template<class T>
inline void PatchSeqId_CallSet(T& orig, const ISeq_id_Translator& tr)
{
    if (orig.IsSet())
        PatchSeqIds(orig.Set(), tr);
}

inline void PatchSeqId(CSeq_interval& orig, const ISeq_id_Translator& tr)
{
    PatchSeqId_CallSetId(orig, tr);
}
inline void PatchSeqId(CPacked_seqint& orig, const ISeq_id_Translator& tr)
{
    PatchSeqId_CallSet(orig, tr);
}
inline void PatchSeqId(CSeq_point& orig, const ISeq_id_Translator& tr)
{
    PatchSeqId_CallSetId(orig, tr);
}
inline void PatchSeqId(CSeq_bond& orig, const ISeq_id_Translator& tr)
{
    if (orig.IsSetA())
        PatchSeqId(orig.SetA(), tr);
    if (orig.IsSetB())
        PatchSeqId(orig.SetB(), tr);
}
inline void PatchSeqId(CSeq_loc_mix& orig, const ISeq_id_Translator& tr)
{
    PatchSeqId_CallSet(orig, tr);
}
inline void PatchSeqId(CSeq_loc_equiv& orig, const ISeq_id_Translator& tr)
{
    PatchSeqId_CallSet(orig, tr);
}
inline void PatchSeqId(CPacked_seqpnt& orig, const ISeq_id_Translator& tr)
{
    PatchSeqId_CallSetId(orig, tr);
}
void PatchSeqId(CSeq_loc& orig, const ISeq_id_Translator& tr)
{
    orig.InvalidateIdCache();
    if (orig.IsEmpty())
        PatchSeqId(orig.SetEmpty(), tr);
    if (orig.IsWhole())
        PatchSeqId(orig.SetWhole(), tr);
    if (orig.IsInt())
        PatchSeqId(orig.SetInt(), tr);
    if (orig.IsPacked_int())
        PatchSeqId(orig.SetPacked_int(), tr);
    if (orig.IsPnt()) 
        PatchSeqId(orig.SetPnt(), tr);
    if (orig.IsPacked_pnt())
        PatchSeqId(orig.SetPacked_pnt(), tr);
    if (orig.IsMix())
        PatchSeqId(orig.SetMix(), tr);
    if (orig.IsEquiv())
        PatchSeqId(orig.SetEquiv(), tr);
    if (orig.IsBond())
        PatchSeqId(orig.SetBond(), tr);
}
inline void PatchSeqId(CDense_diag& orig, const ISeq_id_Translator& tr)
{
    PatchSeqId_CallSetIds(orig, tr);
}
inline void PatchSeqId(CPacked_seg& orig, const ISeq_id_Translator& tr)
{
    PatchSeqId_CallSetIds(orig, tr);
}
inline void PatchSeqId(CDense_seg& orig, const ISeq_id_Translator& tr)
{
    PatchSeqId_CallSetIds(orig, tr);
}

inline void PatchSeqId(CStd_seg& orig, const ISeq_id_Translator& tr)
{
    if (orig.IsSetIds())
        PatchSeqIds(orig.SetIds(), tr);
    if (orig.IsSetLoc())
        PatchSeqIds(orig.SetLoc(), tr);  
}
inline void PatchSeqId(CSeq_align_set& orig, const ISeq_id_Translator& tr)
{
    PatchSeqId_CallSet(orig, tr);
}
void PatchSeqId(CSeq_align& align, const ISeq_id_Translator& tr)
{
    if (align.IsSetBounds()) {
        PatchSeqIds(align.SetBounds(), tr);
    }
    if (align.IsSetSegs()) {
        CSeq_align::TSegs& segs = align.SetSegs();
        if (segs.IsDendiag()) 
            PatchSeqIds(segs.SetDendiag(), tr);
        
        if (segs.IsDenseg()) 
            PatchSeqId(segs.SetDenseg(), tr);
        if (segs.IsStd()) 
            PatchSeqIds(segs.SetStd(), tr);
        if (segs.IsPacked()) 
            PatchSeqId(segs.SetPacked(), tr);
        if (segs.IsDisc()) 
            PatchSeqId(segs.SetDisc(), tr);
    }
}
void PatchSeqId(CSeq_graph& graph, const ISeq_id_Translator& tr)
{
    if (graph.IsSetLoc())
        PatchSeqId(graph.SetLoc(), tr);
}

void PatchSeqId(CSeq_annot& annot, const ISeq_id_Translator& tr)
{
    if (annot.IsSetData()) {
        CSeq_annot::TData& data = annot.SetData();
        if (data.IsFtable()) 
            PatchSeqIds( data.SetFtable(), tr);
        if (data.IsAlign())
            PatchSeqIds( data.SetAlign(), tr);
        if (data.IsGraph())
            PatchSeqIds( data.SetGraph(), tr);
        if (data.IsIds()) 
            PatchSeqIds( data.SetIds(), tr);
        if (data.IsLocs())
            PatchSeqIds( data.SetLocs(), tr);
    }
}

void PatchSeqId(CBioseq& bioseq, const ISeq_id_Translator& tr)
{
    if (bioseq.IsSetId())
        PatchSeqIds( bioseq.SetId(), tr);
    if (bioseq.IsSetAnnot())
        PatchSeqIds( bioseq.SetAnnot(), tr);
}

void PatchSeqId(CBioseq_set& bioseq_set, const ISeq_id_Translator& tr)
{
    if (bioseq_set.IsSetSeq_set())
        PatchSeqIds( bioseq_set.SetSeq_set(), tr);
    if (bioseq_set.IsSetAnnot())
        PatchSeqIds( bioseq_set.SetAnnot(), tr);
}

void PatchSeqId(CSeq_entry& entry, const ISeq_id_Translator& tr)
{
    if (entry.IsSeq())
        PatchSeqId(entry.SetSeq(), tr);
    if (entry.IsSet())
        PatchSeqId(entry.SetSet(), tr);
}

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////


END_SCOPE(objects)
END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.5  2005/08/31 19:36:44  didenko
 * Reduced the number of objects copies which are being created while doing PatchSeqIds
 *
 * Revision 1.4  2005/08/31 14:47:14  didenko
 * Changed the object parameter type for LoadAnnot and LoadBioseq methods
 *
 * Revision 1.3  2005/08/29 16:15:01  didenko
 * Modified default implementation of ITSE_Assigner in a way that it can be used as base class for
 * the user's implementations of this interface
 *
 * Revision 1.2  2005/08/25 15:37:29  didenko
 * Added call to InvalidateCache() method when the CSeq_los is bieng patched
 *
 * Revision 1.1  2005/08/25 14:05:37  didenko
 * Restructured TSE loading process
 *
 *
 * ===========================================================================
 */
