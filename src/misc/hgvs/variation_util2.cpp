/*  $Id: variation_util2.cpp 415910 2013-10-22 16:06:39Z astashya 
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
 * File Description:
 *
 */

#include <ncbi_pch.hpp>
#include <objects/seqloc/Seq_point.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/general/Dbtag.hpp>

#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Spliced_seg.hpp>
#include <objects/seqalign/Spliced_exon.hpp>
#include <objects/seqalign/Product_pos.hpp>
#include <objects/seqalign/Prot_pos.hpp>
#include <objmgr/seq_loc_mapper.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/align_ci.hpp>


#include <objects/seq/seqport_util.hpp>
#include <objects/seqblock/GB_block.hpp>

#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/Variation_ref.hpp>
#include <objects/seqfeat/Phenotype.hpp>
#include <objects/seqfeat/Variation_inst.hpp>
#include <objects/seqfeat/VariantProperties.hpp>
#include <objects/seqfeat/Delta_item.hpp>
#include <objects/seqfeat/Ext_loc.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/SubSource.hpp>
#include <objects/seqfeat/SeqFeatXref.hpp>

#include <objects/pub/Pub_set.hpp>
#include <objects/pub/Pub.hpp>
#include <objects/biblio/Cit_art.hpp>
#include <objects/biblio/Cit_book.hpp>
#include <objects/biblio/Cit_jour.hpp>


#include <objects/variation/VariationMethod.hpp>
#include <objects/variation/VariationException.hpp>

#include <objects/seq/Seq_literal.hpp>
#include <objects/seq/Seq_data.hpp>
#include <objects/seq/Annot_descr.hpp>
#include <objects/seq/Annotdesc.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/Seqdesc.hpp>

#include <objects/general/User_object.hpp>
#include <objects/general/Object_id.hpp>

#include <objects/seqloc/Seq_loc_equiv.hpp>

#include <serial/iterator.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/util/seq_loc_util.hpp>
#include <misc/hgvs/variation_util2.hpp>

#include <objects/seqloc/Na_strand.hpp>

BEGIN_NCBI_SCOPE

namespace variation {

CRef<CVariationException> CreateException(const string& message, 
                                          CVariationException::ECode code = static_cast<CVariationException::ECode>(0))
{
    CRef<CVariationException> e(new CVariationException);
    e->SetMessage(message);
    if(code) {
        e->SetCode(code);
    }
    return e;
}


template<typename T>
void ChangeIdsInPlace(T& container, sequence::EGetIdType id_type, CScope& scope)
{
    for(CTypeIterator<CSeq_id> it = Begin(container); it; ++it) {
        CSeq_id& id = *it;
        if(    (id_type == sequence::eGetId_ForceAcc && id.GetTextseq_Id())
            || (id_type == sequence::eGetId_ForceGi && id.IsGi()))
        {
            continue;
        } else {
            CSeq_id_Handle idh = sequence::GetId(id, scope, id_type);
            id.Assign(*idh.GetSeqId());
        }
    }
}

TSeqPos CVariationUtil::s_GetLength(const CVariantPlacement& p, CScope* scope)
{
    int start_offset = p.IsSetStart_offset() ? p.GetStart_offset() : 0;
    int stop_offset = p.IsSetStop_offset() ? p.GetStop_offset() : p.GetLoc().IsPnt() ? start_offset : 0;
    return ncbi::sequence::GetLength(p.GetLoc(), scope) + stop_offset - start_offset;
}


CVariationUtil::ETestStatus CVariationUtil::CheckExonBoundary(const CVariantPlacement& p)
{
    const CSeq_id* id = p.GetLoc().GetId();
    if(!id  
       || !(p.IsSetStart_offset() || p.IsSetStop_offset())
       || !(p.IsSetMol() && (p.GetMol() == CVariantPlacement::eMol_cdna || p.GetMol() == CVariantPlacement::eMol_rna)))
    {
        return eNotApplicable;
    }

    SAnnotSelector sel;
    sel.IncludeFeatSubtype(CSeqFeatData::eSubtype_exon);
    CBioseq_Handle bsh = m_scope->GetBioseqHandle(*id);
    
    set<TSeqPos> exon_terminal_pts;
    for(CFeat_CI ci(bsh, sel); ci; ++ci) {
        const CMappedFeat& mf = *ci;
        exon_terminal_pts.insert(sequence::GetStart(mf.GetLocation(), NULL));
        exon_terminal_pts.insert(sequence::GetStop(mf.GetLocation(), NULL));
    }
    if(exon_terminal_pts.size() == 0) {
        return eNotApplicable;
    }

    if(p.IsSetStart_offset()) {
        TSeqPos pos = sequence::GetStart(p.GetLoc(), NULL, eExtreme_Biological);
        if(exon_terminal_pts.find(pos) == exon_terminal_pts.end()) {
            return eFail;
        }
    }

    if(p.IsSetStop_offset()) {
        TSeqPos pos = sequence::GetStop(p.GetLoc(), NULL, eExtreme_Biological);
        if(exon_terminal_pts.find(pos) == exon_terminal_pts.end()) {
            return eFail;
        }
    }

    return ePass;
}

CVariationUtil::ETestStatus CVariationUtil::CheckExonBoundary(const CVariantPlacement& p, const CSeq_align& aln)
{
    const CSeq_id* id = p.GetLoc().GetId();
    if(!aln.GetSegs().IsSpliced()
       || !id
       || !sequence::IsSameBioseq(aln.GetSeq_id(0), *id, m_scope)
       || !(p.IsSetStart_offset() || p.IsSetStop_offset())
       || (p.IsSetMol() && p.GetMol() == CVariantPlacement::eMol_genomic)
    ) {
        return eNotApplicable;
    }

    set<TSeqPos> exon_terminal_pts;
    ITERATE(CSpliced_seg::TExons, it, aln.GetSegs().GetSpliced().GetExons()) {
        const CSpliced_exon& exon = **it;
        exon_terminal_pts.insert(exon.GetProduct_start().IsNucpos() ?
                                     exon.GetProduct_start().GetNucpos() :
                                     exon.GetProduct_start().GetProtpos().GetAmin());
        exon_terminal_pts.insert(exon.GetProduct_end().IsNucpos() ?
                                     exon.GetProduct_end().GetNucpos() :
                                     exon.GetProduct_end().GetProtpos().GetAmin());
    }

    if(p.IsSetStart_offset()) {
        TSeqPos pos = sequence::GetStart(p.GetLoc(), NULL, eExtreme_Biological);
        if(exon_terminal_pts.find(pos) == exon_terminal_pts.end()) {
            return eFail;
        }
    }

    if(p.IsSetStop_offset()) {
        TSeqPos pos = sequence::GetStop(p.GetLoc(), NULL, eExtreme_Biological);
        if(exon_terminal_pts.find(pos) == exon_terminal_pts.end()) {
            return eFail;
        }
    }

    return ePass;
}


void CVariationUtil::s_ResolveIntronicOffsets(CVariantPlacement& p)
{
    if(!p.IsSetStart_offset() && !p.IsSetStop_offset()) {
        return;
    }

    if(p.GetLoc().IsPnt() && !p.IsSetStop_offset() && p.IsSetStart_offset()) {
        //In case of point, only start-offset may be specified. In this case
        //temporarily set stop-offset to be the same (it will be reset below),
        //such that start and stop of point-as-interval are adjusted consistently.
        p.SetStop_offset(p.GetStart_offset());
    }

    //Convert to interval.
    //Need to do for a point so that we can deal with start and stop independently.
    //Another scenario is when start/stop in CDNA coords lie in different exons, so
    //the mapping is across one or more intron - we want to collapse the genomic loc
    //(do we want to preselve the gaps caused by indels rather than introns?)
    CRef<CSeq_loc> loc = sequence::Seq_loc_Merge(p.GetLoc(), CSeq_loc::fMerge_SingleRange, NULL);

    if(!loc->IsInt() && (p.IsSetStart_offset() || p.IsSetStop_offset())) {
        NcbiCerr << MSerial_AsnText << p;
        NCBI_THROW(CException, eUnknown, "Complex location");
    }

    if(p.IsSetStart_offset()) {
        TSeqPos& bio_start_ref = loc->GetStrand() == eNa_strand_minus ? loc->SetInt().SetTo() : loc->SetInt().SetFrom();
        bio_start_ref += p.GetStart_offset() * (loc->GetStrand() == eNa_strand_minus ? -1 : 1);
        p.ResetStart_offset();
        p.ResetStart_offset_fuzz(); //note: might need to combine with loc's fuzz somehow
    }

    if(p.IsSetStop_offset()) {
        TSeqPos& bio_stop_ref = loc->GetStrand() == eNa_strand_minus ? loc->SetInt().SetFrom() : loc->SetInt().SetTo();
        bio_stop_ref += p.GetStop_offset() * (loc->GetStrand() == eNa_strand_minus ? -1 : 1);
        p.ResetStop_offset();
        p.ResetStop_offset_fuzz();
    }

    if(loc->GetTotalRange().GetLength() == 1) {
        loc = sequence::Seq_loc_Merge(*loc, CSeq_loc::fSortAndMerge_All, NULL);
    }

    p.SetLoc().Assign(*loc);
}


void CVariationUtil::s_AddIntronicOffsets(CVariantPlacement& p, const CSpliced_seg& ss, CScope* scope)
{
#if 0
    if(!p.GetLoc().IsPnt() && !p.GetLoc().IsInt()) {
        NCBI_THROW(CException, eUnknown, "Expected simple loc (int or pnt)");
    }
#endif

    if(p.IsSetStart_offset() || p.IsSetStop_offset() || p.IsSetStart_offset_fuzz() || p.IsSetStop_offset_fuzz()) {
        NCBI_THROW(CException, eUnknown, "Expected offset-free placement");
    }

    if(!p.GetLoc().GetId() || !sequence::IsSameBioseq(*p.GetLoc().GetId(), ss.GetGenomic_id(), scope)) {
        NCBI_THROW(CException, eUnknown, "Expected genomic_id in the variation to be the same as in spliced-seg");
    }

    long start = p.GetLoc().GetStart(eExtreme_Positional);
    long stop = p.GetLoc().GetStop(eExtreme_Positional);

    long closest_start = ss.GetExons().front()->GetGenomic_start(); //closest-exon-boundary for bio-start of variation location
    long closest_stop = ss.GetExons().front()->GetGenomic_start(); //closest-exon-boundary for bio-stop of variation location

    ITERATE(CSpliced_seg::TExons, it, ss.GetExons()) {
        const CSpliced_exon& se = **it;

        if(se.GetGenomic_end() >= start && se.GetGenomic_start() <= start) {
            closest_start = start; //start is within exon - use itself.
        } else {
            if(abs((long)se.GetGenomic_end() - start) < abs(closest_start - start)) {
                closest_start = (long)se.GetGenomic_end();
            }
            if(abs((long)se.GetGenomic_start() - start) < abs(closest_start - start)) {
                closest_start = (long)se.GetGenomic_start();
            }
        }

        if(se.GetGenomic_end() >= stop && se.GetGenomic_start() <= stop) {
            closest_stop = stop; //end is within exon - use itself.
        } else {
            if(abs((long)se.GetGenomic_end() - stop) < abs(closest_stop - stop)) {
                closest_stop = (long)se.GetGenomic_end();
            }
            if(abs((long)se.GetGenomic_start() - stop) < abs(closest_stop - stop)) {
                closest_stop = (long)se.GetGenomic_start();
            }
        }
    }

    //convert to int, so we can set start and stop
    CRef<CSeq_loc> int_loc = sequence::Seq_loc_Merge(p.GetLoc(), CSeq_loc::fMerge_SingleRange, NULL);

    //adjust location
    if(start != closest_start || stop != closest_stop) {
        int_loc->SetInt().SetFrom(closest_start);
        int_loc->SetInt().SetTo(closest_stop);
    }
    p.SetLoc(*int_loc);

    //Add offsets. Note that start-offset and stop-offset are always biological.
    //(e.g. start-offset applies to bio-start; value > 0 means downstream, and value <0 means upstream)

    //Note: qry/sbj strandedness in the alignment is irrelevent - we're simply adjusting the
    //location in the genomic coordinate system, conserving the strand
    if(start != closest_start) {
        long offset = (start - closest_start);
        if(p.GetLoc().GetStrand() == eNa_strand_minus) {
            p.SetStop_offset(offset * -1);
        } else {
            p.SetStart_offset(offset);
        }
    }

    if(stop != closest_stop) {
        long offset = (stop - closest_stop);
        if(p.GetLoc().GetStrand() == eNa_strand_minus) {
            p.SetStart_offset(offset * -1);
        } else {
            p.SetStop_offset(offset);
        }
    }
}


CRef<CVariantPlacement> CVariationUtil::Remap(const CVariantPlacement& p, const CSeq_align& aln)
{
    if(!p.GetLoc().GetId()) {
        NCBI_THROW(CException, eUnknown, "Can't get seq-id");
    }

    //note: the operations here are scope-free (i.e. seq-id types in aln must be consistent with p,
    //or else will throw; using a scope will impose major slowdown, which is problematic if we're
    //dealing with tens or hundreds of thousands of remappings.

    CRef<CVariantPlacement> p2(new CVariantPlacement);
    p2->Assign(p);

    if(aln.GetSegs().IsSpliced()
       && sequence::IsSameBioseq(aln.GetSegs().GetSpliced().GetGenomic_id(),
                                 *p2->GetLoc().GetId(), NULL))
    {
        s_AddIntronicOffsets(*p2, aln.GetSegs().GetSpliced(), NULL);
    }

    CSeq_align::TDim target_row = -1;
    for(int i = 0; i < 2; i++) {
        if(sequence::IsSameBioseq(*p2->GetLoc().GetId(), aln.GetSeq_id(i), NULL )) {
            target_row = 1 - i;
        }
    }
    if(target_row == -1) {
        NCBI_THROW(CException, eUnknown, "The alignment has no row for seq-id " + p2->GetLoc().GetId()->AsFastaString());
    }

    CRef<CSeq_loc_Mapper> mapper(new CSeq_loc_Mapper(aln, target_row, NULL));
    mapper->SetMergeAll();

    CRef<CVariantPlacement> p3 = x_Remap(*p2, *mapper);

    if(p3->GetLoc().GetId()
       && aln.GetSegs().IsSpliced()
       && sequence::IsSameBioseq(aln.GetSegs().GetSpliced().GetGenomic_id(),
                                 *p3->GetLoc().GetId(), NULL))
    {
        if(CheckExonBoundary(p, aln) == eFail) {
            CRef<CVariationException> exception(new CVariationException);
            exception->SetCode(CVariationException::eCode_hgvs_exon_boundary);
            exception->SetMessage("HGVS exon-boundary position not found in alignment of " + aln.GetSeq_id(0).AsFastaString() + "-vs-" + aln.GetSeq_id(1).AsFastaString());
            p3->SetExceptions().push_back(exception);
        }
            
        s_ResolveIntronicOffsets(*p3);
    }

    //note: AttachSeq happens only after the intronic offsets are resolved.
    AttachSeq(*p3);

    //Note: as per SNP-5641 - will check for mismatches even in the presence of other more severe exceptions
    //NcbiCerr << "Checking for mismatches-in-mapping" << MSerial_AsnText << p << MSerial_AsnText << *p3 << "\n\n";
    if(p.IsSetSeq() && p3->IsSetSeq() 
       && p.GetSeq().GetLength() == p3->GetSeq().GetLength()
       && p.GetSeq().IsSetSeq_data() && p3->GetSeq().IsSetSeq_data()
       && p.GetSeq().GetSeq_data().Which() == p3->GetSeq().GetSeq_data().Which()
       && !p.GetSeq().GetSeq_data().Equals(p3->GetSeq().GetSeq_data())) 
    {    
        p3->SetExceptions().push_back(CreateException("Mismatches in mapping", CVariationException::eCode_mismatches_in_mapping));
    }    

    CheckPlacement(*p3);
    return p3;
}


CRef<CSeq_align> CreateSplicedSeqAlignFromFeat(const CSeq_feat& rna_feat)
{   
    CRef<CSeq_align> aln(new CSeq_align);
    aln->SetType(CSeq_align::eType_other);
    CSpliced_seg& ss = aln->SetSegs().SetSpliced();
    ss.SetProduct_id().Assign(sequence::GetId(rna_feat.GetProduct(), NULL));
    ss.SetGenomic_id().Assign(sequence::GetId(rna_feat.GetLocation(), NULL));
    ss.SetExons();
    ss.SetProduct_type(CSpliced_seg::eProduct_type_transcript);
    ss.SetProduct_strand(eNa_strand_plus);
    ss.SetGenomic_strand(sequence::GetStrand(rna_feat.GetLocation()));

    TSignedSeqPos product_pos(0);
    for(CSeq_loc_CI ci(rna_feat.GetLocation(), CSeq_loc_CI::eEmpty_Skip, CSeq_loc_CI::eOrder_Biological); ci; ++ci) {
        CRef<CSpliced_exon> se(new CSpliced_exon);
        se->SetGenomic_start(ci.GetRange().GetFrom());
        se->SetGenomic_end(ci.GetRange().GetTo());
        se->SetProduct_start().SetNucpos(product_pos);
        se->SetProduct_end().SetNucpos(product_pos + ci.GetRange().GetLength() - 1); 
        product_pos += ci.GetRange().GetLength();
        ss.SetExons().push_back(se);
    }   

    return aln;
}   


//todo: 1. what if we don't have the requested version of the product annotated?
//      2. do we need to support NGs, etc (will need to search all alignments on sequnece, as we can't find NG by product-id)
//      3. Do we want to return VariantPlacement instead, such that we can communicate auxiliary info?
CRef<CVariantPlacement> CVariationUtil::RemapToAnnotatedTarget(const CVariation& v, const CSeq_id& target)
{
    CRef<CVariantPlacement> result(new CVariantPlacement());
    result->SetLoc().SetNull();
    result->SetMol(CVariantPlacement::eMol_unknown);
    CBioseq_Handle bsh = m_scope->GetBioseqHandle(target);

    if(v.IsSetPlacements()) {
        //First see if we already have location on target in one of the placements
        ITERATE(CVariation::TPlacements, it, v.GetPlacements()) {
            const CVariantPlacement& p = **it;
            if(p.GetLoc().GetId() && sequence::IsSameBioseq(target, *p.GetLoc().GetId(), m_scope)) {
                result->Assign(p);
                break;
            }
        }

        if(result->GetLoc().IsNull()) {
            //did not find an existing placement for the target; will try to remap
            ITERATE(CVariation::TPlacements, it, v.GetPlacements()) {
                const CVariantPlacement& placement = **it;
                if(!placement.GetLoc().GetId()) {
                    continue;
                } else if(placement.GetMol() == CVariantPlacement::eMol_protein) {
                    CRef<CVariation> precursor = InferNAfromAA(v);
                    result = RemapToAnnotatedTarget(*precursor, target);
                } else {
                    //the annotation is gi-based, so we'll make sure the target is gi-based so we won't need to do conversion for all products while searching.
                    CSeq_id_Handle product_idh = sequence::GetId(*placement.GetLoc().GetId(), *m_scope, sequence::eGetId_ForceGi);
                    if(!product_idh) {
                        continue;
                    }

                    //note that here we are looking for annotated (packaged) alignment at the
                    //feature's location first (we could have searched the whole sequence instead, but it is quite slow for NCs)
                    //If we have the feature but no alignment, it is assumed that the alignment is perfect, and a synthetic seq-align
                    //is created based on RNA feature.
                    CRef<CSeq_align> aln;
                    {{
                        CRef<CSeq_loc> target_loc = bsh.GetRangeSeq_loc(0, 0); //this returns "whole"
                        //will try to constrain target_loc from whole to product location only

                        SAnnotSelector sel;
                        sel.SetResolveAll();
                        sel.SetAdaptiveDepth(true);
                        sel.IncludeFeatType(CSeqFeatData::e_Rna);

                        for(CFeat_CI ci(bsh, sel); ci; ++ci) {
                            const CMappedFeat& mf = *ci;
                            if(!mf.IsSetProduct()) {
                                continue;
                            }
                            const CSeq_id& product_id = sequence::GetId(mf.GetProduct(), NULL);
                            if(product_id.Equals(*product_idh.GetSeqId())) {
                                target_loc->Assign(mf.GetLocation());
                                aln = CreateSplicedSeqAlignFromFeat(mf.GetMappedFeature());
                                break;
                            }
                        }

                        //try to find explicit seq_align at the target-loc (if not found, will use the synthetic one)
                        for(CAlign_CI ci2(*m_scope, *target_loc, sel); ci2; ++ci2) {
                            const CSeq_align& current_aln = *ci2;
                            if(current_aln.GetSeq_id(0).Equals(*product_idh.GetSeqId())) {
                                aln = SerialClone<CSeq_align>(current_aln);
                            }
                        } 
                    }}

                    if(aln) {
                        CRef<CVariantPlacement> p2(SerialClone<CVariantPlacement>(placement));
                        ChangeIdsInPlace(*p2, sequence::eGetId_ForceAcc, *m_scope);
                        ChangeIdsInPlace(*aln, sequence::eGetId_ForceAcc, *m_scope);
                        result = Remap(*p2, *aln);
                    }
                }
                if(!result->GetLoc().IsNull()) {
                    break;
                }
            }
        }
    } else if(v.GetData().IsSet()) { //no placements at this level - try in sub-variations
        ITERATE(CVariation::TData::TSet::TVariations, it, v.GetData().GetSet().GetVariations()) {
            const CVariation& v2 = **it;
            CRef<CVariantPlacement> mapped_placement = RemapToAnnotatedTarget(v2, target);
            if(!result->GetLoc().IsNull()) { //already have somthing - append
                result->SetLoc().Add(mapped_placement->GetLoc());
            } else {
                result->Assign(*mapped_placement);
            }
        }
    }

    CRef<CSeq_loc> loc = sequence::Seq_loc_Merge(result->GetLoc(), CSeq_loc::fSortAndMerge_All, NULL);
    result->SetLoc().Assign(*loc);
    return result;
}



template<typename T>
bool ContainsIupacNaAmbiguities(const T& obj)
{
    for(CTypeConstIterator<CSeq_data> it(Begin(obj)); it; ++it) {
        const CSeq_data& sd = *it;
        if(!sd.IsIupacna()) {
            continue;
        }
        ITERATE(string, it2, sd.GetIupacna().Get()) {
            char c = *it2;
            if(c != 'A' &&  c != 'C' && c != 'G' && c != 'T') {
                return true;
            }
        }
    }
    return false;
}

bool CVariationUtil::CheckAmbiguitiesInLiterals(CVariation& v)
{
    bool had_ambiguities = false;
 
    if(v.IsSetPlacements() && v.GetPlacements().size() > 0) {
        if(ContainsIupacNaAmbiguities(v.GetData())) {
            had_ambiguities = true;
            v.SetPlacements().front()->SetExceptions().push_back(CreateException("Ambiguous residues in variation", CVariationException::eCode_ambiguous_sequence));
        }        
    }

    if(v.GetData().IsSet()) {
        NON_CONST_ITERATE(CVariation::TData::TSet::TVariations, it, v.SetData().SetSet().SetVariations()) {
            CVariation& v2 = **it;
            had_ambiguities = had_ambiguities || CheckAmbiguitiesInLiterals(v2);
        }
    }
    return had_ambiguities;
}

bool CVariationUtil::CheckPlacement(CVariantPlacement& p)
{
    bool invalid_location = false;
    bool out_of_order = false;

    if(sequence::SeqLocCheck(p.GetLoc(), m_scope) == sequence::eSeqLocCheck_error) {
        invalid_location = true;
    }

    if(p.GetLoc().GetId() && !p.GetLoc().IsEmpty()) {
        if(sequence::GetStart(p.GetLoc(), NULL) > sequence::GetStop(p.GetLoc(), NULL)) {
            out_of_order = true;    
        }

#if 0
        //Note: not comparing offsets, as it may be legitimately out-of-order. JIRA:SNP-5238
        if(p.IsSetStart_offset() && p.IsSetStop_offset() && sequence::GetLength(p.GetLoc(), NULL) == 1) {
            if(p.GetStart_offset() > p.GetStop_offset()) {
                //note: not checking for strand, as offsets are always in the order of transcription
                out_of_order = true;
                invalid_location = true;
            }
        }
#endif
    }

    if(invalid_location) {
        p.SetExceptions().push_back(CreateException(
            out_of_order ? "Invalid location - start and stop are out of order" 
                         : "Invalid location", 
            CVariationException::eCode_hgvs_parsing));
    }

    if(p.GetLoc().GetId()) {
        CBioseq_Handle bsh = m_scope->GetBioseqHandle(*p.GetLoc().GetId());
        if(bsh && bsh.GetState() != 0 && bsh.GetState() != CBioseq_Handle::fState_dead) { //dead is OK, beacuse supporting old versions
            p.SetExceptions().push_back(CreateException("Bioseq is suppressed or withdrawn", CVariationException::eCode_bioseq_state));
        }
    }

    return invalid_location;
}


CRef<CVariantPlacement> CVariationUtil::Remap(const CVariantPlacement& p, CSeq_loc_Mapper& mapper)
{
    CRef<CVariantPlacement> p2 = x_Remap(p, mapper);
    if(((p.IsSetStart_offset() || p.IsSetStop_offset()) && p2->GetMol() == CVariantPlacement::eMol_genomic)
       || (p.GetMol() == CVariantPlacement::eMol_genomic
           && p2->GetMol() != CVariantPlacement::eMol_genomic
           && p2->GetMol() != CVariantPlacement::eMol_unknown) /*in case remapped to nothing*/)
    {
        NcbiCerr << "Mapped: " << MSerial_AsnText << *p2;
        //When mapping an offset-placement to a genomic placement, may need to resolve offsets.
        //or, when mapping from genomic to a product coordinates, may need to add offsets. In above cases
        //we need to use the seq-align-based mapping.
        NCBI_THROW(CException, eUnknown, "Cannot use Mapper-based method to remap intronic cases; must remap via spliced-seg alignment instead.");
    }


    AttachSeq(*p2);
    if(p.IsSetSeq() && p2->IsSetSeq() 
       && p.GetSeq().GetLength() == p2->GetSeq().GetLength()
       && p.GetSeq().IsSetSeq_data() && p2->GetSeq().IsSetSeq_data()
       && p.GetSeq().GetSeq_data().Which() == p2->GetSeq().GetSeq_data().Which()
       && !p.GetSeq().GetSeq_data().Equals(p2->GetSeq().GetSeq_data())) 
    {    
        p2->SetExceptions().push_back(CreateException("Mismatches in mapping", CVariationException::eCode_mismatches_in_mapping));
    }    

    CheckPlacement(*p2);
    return p2;
}

CRef<CVariantPlacement> CVariationUtil::x_Remap(const CVariantPlacement& p, CSeq_loc_Mapper& mapper)
{
    CRef<CVariantPlacement> p2(new CVariantPlacement);


    if(p.IsSetStart_offset()) {
        p2->SetStart_offset() = p.GetStart_offset();
    }
    if(p.IsSetStop_offset()) {
        p2->SetStop_offset() = p.GetStop_offset();
    }
    if(p.IsSetStart_offset_fuzz()) {
        p2->SetStart_offset_fuzz().Assign(p.GetStart_offset_fuzz());
    }
    if(p.IsSetStop_offset_fuzz()) {
        p2->SetStop_offset_fuzz().Assign(p.GetStop_offset_fuzz());
    }

    CRef<CSeq_loc> mapped_loc = mapper.Map(p.GetLoc());

    {{
        //If we have offsets, then the distinction between point and one-point interval is important, e.g.
        //NM_000155.3:c.-116-3_-116 - the location is an interval, but the anchor point is the same; we need to 
        //keep it as interval, as if we represent it as a point, then the corresponding HGVS is also a point: NM_000155.3:c.-116-3
        bool equal_offsets = (!p2->IsSetStart_offset() && !p2->IsSetStop_offset())
                          || ( p2->IsSetStart_offset() &&  p2->IsSetStop_offset() && p2->GetStart_offset() == p2->GetStop_offset());

        bool merge_single_range = p.GetLoc().IsInt() && mapped_loc->IsPnt() && !equal_offsets;

        if(   mapped_loc->IsInt() 
           && mapped_loc->GetInt().IsSetFuzz_from() 
           && mapped_loc->GetInt().IsSetFuzz_to() 
           && sequence::GetLength(*mapped_loc, NULL) == 1)
        {
            //workaround for CXX-4376. single-nt locations will not be collapsed to a point if both From and To fuzz is present
            mapped_loc->SetInt().ResetFuzz_to();
        }

        mapped_loc = sequence::Seq_loc_Merge(*mapped_loc, 
                                             merge_single_range ? CSeq_loc::fMerge_SingleRange : CSeq_loc::fSortAndMerge_All, 
                                             NULL);
    }}

#if 1
    //SNP-5148
    if(mapped_loc->IsNull() && p.GetLoc().GetId() && !p.GetLoc().IsEmpty()) {
        //If mapped to nothing, expand the loc and try again. The purpose is to know the seq-id of the 
        //neighborhood that we couldn't map to, so we can produce and Empty seq-loc that contains
        //a seq-id, reporting that we tried to map to this sequence, but there's no mapping
        CRef<CSeq_loc> loc1 = sequence::Seq_loc_Merge(p.GetLoc(), CSeq_loc::fMerge_SingleRange, NULL);
        loc1->SetInt().SetFrom() = loc1->GetInt().GetFrom() < 500 ? 0 : loc1->SetInt().GetFrom() - 500;
        loc1->SetInt().SetTo() += 500;
        CRef<CSeq_loc> tmp_mapped_loc = mapper.Map(*loc1);

        if(tmp_mapped_loc->GetId()) {
            mapped_loc->SetEmpty().Assign(*tmp_mapped_loc->GetId());
        }
 
   }
    if(p2->GetLoc().IsEmpty()) {
        p2->SetExceptions().push_back(CreateException("No mapping", CVariationException::eCode_no_mapping));
    }
#endif


    p2->SetLoc(*mapped_loc);
    p2->SetPlacement_method(CVariantPlacement::ePlacement_method_projected);

    {{
        TSeqPos orig_len = p.GetLoc().Which() ? sequence::GetLength(p.GetLoc(), NULL) : 0;
        TSeqPos mapped_len = mapped_loc->Which() ? sequence::GetLength(*mapped_loc, NULL) : 0;
        bool orig_is_compound = p.GetLoc().IsMix() || p.GetLoc().IsPacked_int();
        bool mapped_is_compound = mapped_loc->IsMix() || mapped_loc->IsPacked_int();
	    CRef<CVariationException> exception;
	    if(mapped_len == 0) {
	        exception.Reset(new CVariationException);
	        exception->SetCode(CVariationException::eCode_no_mapping);
	    } else if(mapped_len < orig_len) {
	        exception.Reset(new CVariationException);
	        exception->SetCode(CVariationException::eCode_partial_mapping);
	    } else if(!orig_is_compound && mapped_is_compound) {
	        exception.Reset(new CVariationException);
	        exception->SetCode(CVariationException::eCode_split_mapping);
	    }
	    if(exception) {
	        exception->SetMessage("");
	        p2->SetExceptions().push_back(exception);
	    }
    }}

    if(p2->GetLoc().GetId()) {
        p2->SetMol(GetMolType(sequence::GetId(p2->GetLoc(), NULL)));
    } else {
        p2->SetMol(CVariantPlacement::eMol_unknown);
    }

    ChangeIdsInPlace(*p2, sequence::eGetId_ForceAcc, *m_scope);

    return p2;
}

bool CVariationUtil::AttachSeq(CVariantPlacement& p, TSeqPos max_len)
{
    p.ResetSeq();
    bool ret = false;
    TSeqPos length = s_GetLength(p, m_scope);

    p.SetSeq().SetLength(length);

    if(   (p.IsSetStart_offset() && p.GetStart_offset() != 0)
       || (p.IsSetStop_offset()  && p.GetStop_offset()  != 0))
    {    
        p.SetExceptions().push_back(CreateException("Can't get sequence for an offset-based location", CVariationException::eCode_seqfetch_intronic));
        ret = false;
    } else if(length > max_len) {
        p.SetExceptions().push_back(CreateException("Sequence is longer than the cutoff threshold", CVariationException::eCode_seqfetch_too_long));
        ret = false;
    } else {
        try {
            CRef<CSeq_literal> literal = x_GetLiteralAtLoc(p.GetLoc());
            p.SetSeq(*literal);

            if(ContainsIupacNaAmbiguities(*literal)) {
                p.SetExceptions().push_back(CreateException("Ambiguous residues in reference", CVariationException::eCode_ambiguous_sequence));
            }    
            ret = true;
        } catch(CException&) { 
            //location can be invalid - SNP-5510
            p.SetExceptions().push_back(CreateException("Cannot fetch sequence at location", CVariationException::eCode_seqfetch_invalid));
            ret = false;
        }    
    }    
    return ret; 
}

bool CVariationUtil::AttachSeq(CVariation& v, TSeqPos max_len)
{
    v.Index();
    bool had_exceptions = false;
    if(v.IsSetPlacements()) {

        CConstRef<CSeq_literal> asserted_literal;
        CConstRef<CSeq_literal> variant_literal; //will only fetch for snp/mnp cases, as ref_same_as_variant test is only applicable for these

        //Find the asserted and variant literal for this variation 
        //(don't look into consequence-variations that may also contain protein-specific asserted and variant sequence)
        for(CTypeConstIterator<CVariation> it(Begin(v)); it; ++it) {
            const CVariation& v2 = *it;
            if(!v2.GetData().IsInstance() || (v2.GetConsequenceParent() && &v != &v2)) {
                continue;
            }

            const CVariation_inst& inst = v2.GetData().GetInstance();
            if(   inst.GetDelta().size() == 1
               && inst.GetDelta().front()->IsSetSeq()
               && inst.GetDelta().front()->GetSeq().IsLiteral())
            {
                CConstRef<CSeq_literal> literal(&inst.GetDelta().front()->GetSeq().GetLiteral());
                if(!asserted_literal && inst.IsSetObservation() && inst.GetObservation() == CVariation_inst::eObservation_asserted) {
                    asserted_literal = literal;
                } else if(!variant_literal //note: finding the very first one; will that work always?
                          && (!inst.IsSetObservation() || inst.GetObservation() == CVariation_inst::eObservation_variant)
                          && (!inst.GetDelta().front()->IsSetAction() || inst.GetDelta().front()->GetAction() == CDelta_item::eAction_morph)
                          && (!inst.GetDelta().front()->IsSetMultiplier() && !inst.GetDelta().front()->IsSetMultiplier_fuzz())
                        )
                {
                    variant_literal = literal;
                }
            }
        }

#if 0
        if(variant_literal) {
            LOG_POST("Found variant-literal");
            NcbiCout << MSerial_AsnText << *variant_literal;
        } else { 
            LOG_POST("Did not find variant-literal");
        }
#endif

        NON_CONST_ITERATE(CVariation::TPlacements, it, v.SetPlacements()) {
            CVariantPlacement& p = **it;
            bool attached = AttachSeq(p, max_len);
            
            if(attached 
               && asserted_literal
               && (p.GetSeq().GetLength() != asserted_literal->GetLength()
                   ||  (    p.GetSeq().IsSetSeq_data() && asserted_literal->IsSetSeq_data()
                         && p.GetSeq().GetSeq_data().Which() == asserted_literal->GetSeq_data().Which()
                         && !p.GetSeq().GetSeq_data().Equals(asserted_literal->GetSeq_data()))))
            {
                p.SetExceptions().push_back(CreateException("Asserted sequence is inconsistent with reference", 
                                                            CVariationException::eCode_inconsistent_asserted_allele));
                had_exceptions = true;
            }

            if(attached
               && variant_literal
               && variant_literal->Equals(p.GetSeq()))
            {
                p.SetExceptions().push_back(CreateException("Reference sequence is the same as variant", 
                                                            CVariationException::eCode_ref_same_as_variant));
                had_exceptions = true;
            }
        }
    }

    if(v.GetData().IsSet()) {
        NON_CONST_ITERATE(CVariation::TData::TSet::TVariations, it, v.SetData().SetSet().SetVariations()) {
            CVariation& v2 = **it;
            had_exceptions = had_exceptions || AttachSeq(v2, max_len);
        }
    }
    return !had_exceptions;
}


CVariantPlacement::TMol CVariationUtil::GetMolType(const CSeq_id& id)
{
    //Most of the time can figure out from seq-id. Must be careful not to
    //automatically treat NC as "genomic", as we need to differentiate from
    //mitochondrion.
    CSeq_id::EAccessionInfo acinf = id.IdentifyAccession();
    CVariantPlacement::TMol moltype = CVariantPlacement::eMol_unknown;

    if(acinf == CSeq_id::eAcc_refseq_prot || acinf == CSeq_id::eAcc_refseq_prot_predicted) {
        moltype = CVariantPlacement::eMol_protein;
    } else if(acinf == CSeq_id::eAcc_refseq_mrna || acinf == CSeq_id::eAcc_refseq_mrna_predicted) {
        moltype = CVariantPlacement::eMol_cdna;
    } else if(acinf == CSeq_id::eAcc_refseq_ncrna || acinf == CSeq_id::eAcc_refseq_ncrna_predicted) {
        moltype = CVariantPlacement::eMol_rna;
    } else if(acinf == CSeq_id::eAcc_refseq_genomic
              || acinf == CSeq_id::eAcc_refseq_contig
              || acinf == CSeq_id::eAcc_refseq_wgs_intermed)
    {
        moltype = CVariantPlacement::eMol_genomic;
    } else if(id.IdentifyAccession() == CSeq_id::eAcc_refseq_chromosome
              && NStr::StartsWith(id.GetTextseq_Id()->GetAccession(), "AC_"))
    {
        moltype = CVariantPlacement::eMol_genomic;
    } else if(id.IdentifyAccession() == CSeq_id::eAcc_refseq_chromosome
              && (   (id.GetTextseq_Id()->GetAccession() >= "NC_000001" && id.GetTextseq_Id()->GetAccession() <= "NC_000024")
                  || (id.GetTextseq_Id()->GetAccession() >= "NC_000067" && id.GetTextseq_Id()->GetAccession() <= "NC_000087")))
    {
        //hardcode most "popular" chromosomes that are not mitochondrions.
        moltype = CVariantPlacement::eMol_genomic;
    } else {
        CBioseq_Handle bsh = m_scope->GetBioseqHandle(id);
        if(!bsh) {
            moltype = CVariantPlacement::eMol_unknown;
        } else {
            //check if mitochondrion
            if(bsh.GetTopLevelEntry().IsSetDescr()) {
                ITERATE(CBioseq_Handle::TDescr::Tdata, it, bsh.GetTopLevelEntry().GetDescr().Get()) {
                    const CSeqdesc& desc = **it;
                    if(!desc.IsSource()) {
                        continue;
                    }
                    if(desc.GetSource().IsSetGenome() && desc.GetSource().GetGenome() == CBioSource::eGenome_mitochondrion) {
                        moltype = CVariantPlacement::eMol_mitochondrion;
                    }
                }
            }

            //check molinfo
            if(moltype == CVariantPlacement::eMol_unknown) {
                const CMolInfo* m = sequence::GetMolInfo(bsh);
                if(!m) {
                    moltype = CVariantPlacement::eMol_unknown;
                } else if(m->GetBiomol() == CMolInfo::eBiomol_genomic
                       || m->GetBiomol() == CMolInfo::eBiomol_other_genetic)
                {
                    moltype = CVariantPlacement::eMol_genomic;
                } else if(m->GetBiomol() == CMolInfo::eBiomol_mRNA
                       || m->GetBiomol() == CMolInfo::eBiomol_genomic_mRNA)
                {
                    moltype = CVariantPlacement::eMol_cdna;
                } else if(m->GetBiomol() == CMolInfo::eBiomol_ncRNA
                       || m->GetBiomol() == CMolInfo::eBiomol_rRNA
                       || m->GetBiomol() == CMolInfo::eBiomol_scRNA
                       || m->GetBiomol() == CMolInfo::eBiomol_snRNA
                       || m->GetBiomol() == CMolInfo::eBiomol_snoRNA
                       || m->GetBiomol() == CMolInfo::eBiomol_pre_RNA
                       || m->GetBiomol() == CMolInfo::eBiomol_tmRNA
                       || m->GetBiomol() == CMolInfo::eBiomol_cRNA
                       || m->GetBiomol() == CMolInfo::eBiomol_tRNA
                       || m->GetBiomol() == CMolInfo::eBiomol_transcribed_RNA)
                {
                    moltype = CVariantPlacement::eMol_rna;
                } else if(m->GetBiomol() == CMolInfo::eBiomol_peptide) {
                    moltype = CVariantPlacement::eMol_protein;
                } else {
                    moltype = CVariantPlacement::eMol_unknown;
                }
            }
        }
    }
    return moltype;
}


CVariationUtil::SFlankLocs CVariationUtil::CreateFlankLocs(const CSeq_loc& loc, TSeqPos len)
{
    SFlankLocs flanks;
    flanks.upstream.Reset(new CSeq_loc(CSeq_loc::e_Null));
    flanks.downstream.Reset(new CSeq_loc(CSeq_loc::e_Null));
    if(!loc.GetId()) {
        return flanks;
    }

    TSignedSeqPos start = sequence::GetStart(loc, m_scope, eExtreme_Positional);
    TSignedSeqPos stop = sequence::GetStop(loc, m_scope, eExtreme_Positional);

    CBioseq_Handle bsh = m_scope->GetBioseqHandle(sequence::GetId(loc, NULL));
    TSignedSeqPos max_pos = bsh.GetInst_Length() - 1;

    flanks.upstream->SetInt().SetId().Assign(sequence::GetId(loc, NULL));
    flanks.upstream->SetInt().SetStrand(sequence::GetStrand(loc, NULL));
    flanks.upstream->SetInt().SetTo(min(max_pos, stop + (TSignedSeqPos)len));
    flanks.upstream->SetInt().SetFrom(max((TSignedSeqPos)0, start - (TSignedSeqPos)len));

    flanks.downstream->Assign(*flanks.upstream);

    CSeq_loc& second = sequence::GetStrand(loc, NULL) == eNa_strand_minus ? *flanks.upstream : *flanks.downstream;
    CSeq_loc& first = sequence::GetStrand(loc, NULL) == eNa_strand_minus ? *flanks.downstream : *flanks.upstream;

    if(start == 0) {
        first.SetNull();
    } else {
        first.SetInt().SetTo(start - 1);
    }

    if(stop == max_pos) {
        second.SetNull();
    } else {
        second.SetInt().SetFrom(stop + 1);
    }

    return flanks;
}



///////////////////////////////////////////////////////////////////////////////
//
// Methods and functions pertaining to converting protein variation in precursor coords
//
///////////////////////////////////////////////////////////////////////////////
void CVariationUtil::s_UntranslateProt(const string& prot_str, vector<string>& codons)
{
    if(prot_str.size() != 1) {
        NCBI_THROW(CException, eUnknown, "Expected prot_str of length 1");
    }

    static const char* alphabet = "ACGT";
    string codon = "AAA";
    for(size_t i0 = 0; i0 < 4; i0++) {
        codon[0] = alphabet[i0];
        for(size_t i1 = 0; i1 < 4; i1++) {
            codon[1] = alphabet[i1];
            for(size_t i2 = 0; i2 < 4; i2++) {
                codon[2] = alphabet[i2];
                string prot("");
                CSeqTranslator::Translate(codon, prot, CSeqTranslator::fIs5PrimePartial);
                NStr::ReplaceInPlace(prot, "*", "X"); //Conversion to IUPAC produces "X", but Translate produces "*"

                //LOG_POST(">>>" << codon << " " << prot << " " << prot_str);
                if(prot == prot_str) {
                    codons.push_back(codon);
                }
            }
        }
    }
}

size_t CVariationUtil::s_CountMatches(const string& a, const string& b)
{
    size_t count(0);
    for(size_t i = 0; i <  min(a.size(), b.size()); i++ ) {
        if(a[i] == b[i]) {
            count++;
        }
    }
    return count;
}

void CVariationUtil::s_CalcPrecursorVariationCodon(
        const string& codon_from, //codon on cDNA
        const string& prot_to,    //missense/nonsense AA
        vector<string>& codons_to)  //calculated variation-codons
{
    vector<string> candidates1;
    size_t max_matches(0);
    s_UntranslateProt(prot_to, candidates1);
    codons_to.clear();

    ITERATE(vector<string>, it1, candidates1) {
        size_t matches = s_CountMatches(codon_from, *it1);

//        LOG_POST("CalcPrecursorVariationCodon:" << codon_from << " " << prot_to << " " << *it1 << " " << matches);
        if(matches == 3) {
            //all three bases in a codon matched - we must be processing a silent mutation.
            //in this case we want to consider candidate codons other than itself.
            continue;
        }

        if(matches >= max_matches) {
            if(matches > max_matches) {
                codons_to.clear();
            }
            codons_to.push_back(*it1);
            max_matches = matches;
        }
    }
}

string CVariationUtil::s_CollapseAmbiguities(const vector<string>& seqs)
{
    string collapsed_seq;

    vector<int> bits; //4-bit bitmask denoting whether a nucleotide occurs at this pos at any seq in seqs

    typedef const vector<string> TConstStrs;
    ITERATE(TConstStrs, it, seqs) {
        const string& seq = *it;
        if(seq.size() > bits.size()) {
            bits.resize(seq.size());
        }

        for(size_t i= 0; i < seq.size(); i++) {
            char nt = seq[i];
            int m =    (nt == 'T' ? 1
                      : nt == 'G' ? 2
                      : nt == 'C' ? 4
                      : nt == 'A' ? 8 : 0);
            if(!m) {
                NCBI_THROW(CException, eUnknown, "Expected [ACGT] alphabet");
            }

            bits[i] |= m;
        }
    }

    static const char* iupac_nuc_ambiguity_codes = "NTGKCYSBAWRDMHVN";
    collapsed_seq.resize(bits.size());
    for(size_t i = 0; i < collapsed_seq.size(); i++) {
        collapsed_seq[i] = iupac_nuc_ambiguity_codes[bits[i]];
    }
    return collapsed_seq;
}


void CVariationUtil::x_InferNAfromAA(CVariation& v, TAA2NAFlags flags)
{
    if(v.GetData().IsSet()) {
        CVariation::TData::TSet::TVariations variations = v.SetData().SetSet().SetVariations();
        v.SetData().SetSet().SetVariations().clear();

        NON_CONST_ITERATE(CVariation::TData::TSet::TVariations, it, variations) {
            CRef<CVariation> v2 = *it;

            if(v2->GetData().IsInstance()
               && v2->GetData().GetInstance().IsSetObservation()
               && !(v2->GetData().GetInstance().GetObservation() & CVariation_inst::eObservation_variant))
            {
                //only variant observations are propagated to the nuc level
                continue;
            }
            x_InferNAfromAA(*v2, flags);
            v.SetData().SetSet().SetVariations().push_back(v2);
        }
        v.ResetPlacements(); //nucleotide placement will be computed for each instance and attached at its level

        if(v.GetData().GetSet().GetVariations().size() == 1) {
            v.Assign(*v.GetData().GetSet().GetVariations().front());
        }
        return;
    }

    if(!v.GetData().IsInstance()) {
        return;
    }


    //Remap the placement to nucleotide

    const CVariation::TPlacements* placements = s_GetPlacements(v);
    if(!placements || placements->size() == 0) {
        NCBI_THROW(CException, eUnknown, "Expected a placement");
    }

    const CVariantPlacement& placement = *placements->front();

    if(placement.IsSetStart_offset() || placement.IsSetStop_offset()) {
        NCBI_THROW(CException, eUnknown, "Expected offset-free placement");
    }

    if(placement.IsSetMol() && placement.GetMol() != CVariantPlacement::eMol_protein) {
        NCBI_THROW(CException, eUnknown, "Expected a protein-type placement");
    }

    SAnnotSelector sel(CSeqFeatData::e_Cdregion, true);
       //note: sel by product; location is prot; found feature is mrna having this prot as product
    CRef<CSeq_loc_Mapper> prot2precursor_mapper;
    for(CFeat_CI ci(*m_scope, placement.GetLoc(), sel); ci; ++ci) {
        try {
            prot2precursor_mapper.Reset(new CSeq_loc_Mapper(ci->GetMappedFeature(), CSeq_loc_Mapper::eProductToLocation, m_scope));
        } catch(CException&) {
            ;// may legitimately throw if feature is not good for mapping
        }
        break;
    }

    if(!prot2precursor_mapper) {
        NCBI_THROW(CException, eUnknown, "Can't create prot2precursor mapper. Is this a prot?");
    }

    CRef<CSeq_loc> nuc_loc = prot2precursor_mapper->Map(placement.GetLoc());
    ChangeIdsInPlace(*nuc_loc, sequence::eGetId_ForceAcc, *m_scope);

    CConstRef<CDelta_item> delta =
            v.GetData().GetInstance().GetDelta().size() != 1 ? CConstRef<CDelta_item>(NULL)
          : v.GetData().GetInstance().GetDelta().front();

    //See if the variation is simple-enough to process; if too complex, return "unknown-variation"
    if(!nuc_loc->IsInt() //complex nucleotide location
       || sequence::GetLength(*nuc_loc, NULL) != 3 //does not remap to single codon
       || !delta //delta is not a single-element item
       || (delta->IsSetAction() && delta->GetAction() != CDelta_item::eAction_morph) //not a replace-at-location delta-type
       || !delta->IsSetSeq() //don't know what's the variant allele
       || !delta->GetSeq().IsLiteral()
       || delta->GetSeq().GetLiteral().GetLength() != 1)  //not single-AA missense
    {
        CRef<CVariantPlacement> p(new CVariantPlacement);
        p->SetLoc(*nuc_loc);
        p->SetMol(nuc_loc->GetId() ? GetMolType(*nuc_loc->GetId()) : CVariantPlacement::eMol_unknown);
        CRef<CVariation> v2(new CVariation);
        v2->SetData().SetUnknown();
        v2->SetPlacements().push_back(p);
        v.Assign(*v2);
        return;
    }


    CSeq_data variant_prot_seq;
    CSeqportUtil::Convert(delta->GetSeq().GetLiteral().GetSeq_data(), &variant_prot_seq, CSeq_data::e_Iupacaa);

    CSeqVector seqv(*nuc_loc, m_scope, CBioseq_Handle::eCoding_Iupac);
    string original_allele_codon; //nucleotide allele on the sequence
    seqv.GetSeqData(seqv.begin(), seqv.end(), original_allele_codon);

    vector<string> variant_codons;

    if(v.GetData().GetInstance().IsSetObservation()
       && v.GetData().GetInstance().GetObservation() == CVariation_inst::eObservation_asserted)
    {
        s_UntranslateProt(variant_prot_seq.GetIupacaa(), variant_codons);
    } else {
        s_CalcPrecursorVariationCodon(original_allele_codon, variant_prot_seq.GetIupacaa(), variant_codons);
    }

    string variant_codon = s_CollapseAmbiguities(variant_codons);

    if(flags & fAA2NA_truncate_common_prefix_and_suffix
        && !v.IsSetFrameshift()
        && variant_codon != original_allele_codon)
    {
        while(variant_codon.length() > 0
              && original_allele_codon.length() > 0
              && variant_codon.at(0) == original_allele_codon.at(0))
        {
            variant_codon = variant_codon.substr(1);
            original_allele_codon = original_allele_codon.substr(1);
            if(nuc_loc->GetStrand() == eNa_strand_minus) {
                nuc_loc->SetInt().SetTo()--;
            } else {
                nuc_loc->SetInt().SetFrom()++;
            }
        }

        while(variant_codon.length() > 0
              && original_allele_codon.length() > 0
              &&    variant_codon.at(variant_codon.length() - 1)
                 == original_allele_codon.at(original_allele_codon.length() - 1))
        {
            variant_codon.resize(variant_codon.length() - 1);
            original_allele_codon.resize(original_allele_codon.length() - 1);
            //Note: normally given a protein, the parent will be a mRNA and the CDS location
            //will have plus strand; however, the parent could be MT, so we can't assume plus strand
            if(nuc_loc->GetStrand() == eNa_strand_minus) {
                nuc_loc->SetInt().SetFrom()++;
            } else {
                nuc_loc->SetInt().SetTo()--;
            }
        }
    }

    CRef<CDelta_item> delta2(new CDelta_item);
    delta2->SetSeq().SetLiteral().SetLength(variant_codon.length());
    delta2->SetSeq().SetLiteral().SetSeq_data().SetIupacna().Set(variant_codon);

    CRef<CVariation> v2(new CVariation);

    //set placement
    CRef<CVariantPlacement> p2(new CVariantPlacement);
    //merge loc to convert int of length 1 to a pnt as necessary
    p2->SetLoc(*sequence::Seq_loc_Merge(*nuc_loc, CSeq_loc::fSortAndMerge_All, NULL));
    p2->SetMol(GetMolType(sequence::GetId(*nuc_loc, NULL)));
    AttachSeq(*p2);
    v2->SetPlacements().push_back(p2);


    if(v.IsSetFrameshift()) {
        v2->SetData().SetUnknown(); //don't know what is happening at nucleotide level
    } else {
        CVariation_inst& inst2 = v2->SetData().SetInstance();
        inst2.SetType(variant_codon.length() == 1 ? CVariation_inst::eType_snv : CVariation_inst::eType_mnp);
        inst2.SetDelta().push_back(delta2);

        if(v.GetData().GetInstance().IsSetObservation()) {
            inst2.SetObservation(v.GetData().GetInstance().GetObservation());
        }
    }

    v.Assign(*v2);
}

CRef<CVariation> CVariationUtil::InferNAfromAA(const CVariation& v, TAA2NAFlags flags)
{
    CRef<CVariation> v2(new CVariation);
    v2->Assign(v);
    v2->Index();
    x_InferNAfromAA(*v2, flags);
    s_FactorOutPlacements(*v2);
    v2->Index();

    CheckAmbiguitiesInLiterals(*v2);

    //Note: The result describes whole codons, even for point mutations, i.e. common suffx/prefix are not truncated.
    return v2;
}


CRef<CSeq_literal> CVariationUtil::GetLiteralAtLoc(const CSeq_loc& loc)
{
    return x_GetLiteralAtLoc(loc);
}


CRef<CSeq_literal> CVariationUtil::x_GetLiteralAtLoc(const CSeq_loc& loc)
{
    CRef<CSeq_literal> literal(new CSeq_literal);
    if(sequence::GetLength(loc, NULL) == 0) {
        literal->SetLength(0);
    } else {
        CRef<CSeq_literal> cached_literal = m_cdregion_index.GetCachedLiteralAtLoc(loc);
        if(cached_literal) {
            literal = cached_literal;
        } else {
            string seq;
            try {
                CSeqVector v(loc, *m_scope, CBioseq_Handle::eCoding_Iupac);

                if(v.IsProtein()) {
                    //if loc extends by 1 past the end protein - we'll need to
                    //truncate the loc to the boundaries of prot, and then add "*" manually,
                    //as otherwise fetching seq will throw.
                    CBioseq_Handle bsh = m_scope->GetBioseqHandle(sequence::GetId(loc, NULL));
                    if(bsh.GetInst_Length() == loc.GetStop(eExtreme_Positional)) {
                        CRef<CSeq_loc> range_loc = sequence::Seq_loc_Merge(loc, CSeq_loc::fMerge_SingleRange, NULL);
                        range_loc->SetInt().SetTo(bsh.GetInst_Length() - 1);
                        CRef<CSeq_loc> truncated_loc = range_loc->Intersect(loc, 0, NULL);
                        CRef<CSeq_literal>  literal = x_GetLiteralAtLoc(*truncated_loc);
                        literal->SetLength() += 1;
                        literal->SetSeq_data().SetNcbieaa().Set().push_back('*');
                        return literal;
                    }
                }

                v.GetSeqData(v.begin(), v.end(), seq);
                literal->SetLength(seq.size());
                if(v.IsProtein()) {
                    literal->SetSeq_data().SetNcbieaa().Set(seq);
                } else if (v.IsNucleotide()) {
                    literal->SetSeq_data().SetIupacna().Set(seq);
                }
            } catch(CException& e) {
                string loc_label;
                loc.GetLabel(&loc_label);
                NCBI_RETHROW_SAME(e, "Can't get literal for " + loc_label);
            }
        }
    }
    return literal;
}


CRef<CSeq_literal> CVariationUtil::s_CatLiterals(const CSeq_literal& a, const CSeq_literal& b)
{
    CRef<CSeq_literal> c(new CSeq_literal);

    if(b.GetLength() == 0) {
        c->Assign(a);
    } else if(a.GetLength() == 0) {
        c->Assign(b);
    } else {
        c->SetLength(a.GetLength() + b.GetLength());

        if(a.IsSetFuzz() || b.IsSetFuzz()) {
            c->SetFuzz().SetLim(CInt_fuzz::eLim_unk);
        }

        if(a.IsSetSeq_data() && b.IsSetSeq_data()) {
            CSeqportUtil::Append(&(c->SetSeq_data()),
                                 a.GetSeq_data(), 0, a.GetLength(),
                                 b.GetSeq_data(), 0, b.GetLength());
        }
    }
    return c;
}

CRef<CSeq_literal> CVariationUtil::s_SpliceLiterals(const CSeq_literal& payload, const CSeq_literal& ref, TSeqPos pos)
{
    CRef<CSeq_literal> result;
    if(pos == 0) {
        result = s_CatLiterals(payload, ref);
    } else if(pos == ref.GetLength()) {
        result = s_CatLiterals(ref, payload);
    } else {
        CRef<CSeq_literal> prefix(new CSeq_literal);
        prefix->Assign(ref);
        prefix->SetLength(pos);
        CSeqportUtil::Keep(&prefix->SetSeq_data(), 0, prefix->GetLength());

        CRef<CSeq_literal> suffix(new CSeq_literal);
        suffix->Assign(ref);
        suffix->SetLength(ref.GetLength() - pos);
        CSeqportUtil::Keep(&suffix->SetSeq_data(), pos, suffix->GetLength());

        CRef<CSeq_literal> prefix_and_payload = s_CatLiterals(*prefix, payload);
        result = s_CatLiterals(*prefix_and_payload, *suffix);
    }
    return result;
}




CConstRef<CSeq_literal>  CVariationUtil::x_FindOrCreateLiteral(const CVariation& v)
{
    CConstRef<CSeq_literal> literal = s_FindFirstLiteral(v);
    if(!literal) {
        //instantiate a literal from a placement (make sure there are no offsets)
        const CVariation::TPlacements* placements = s_GetPlacements(v);
        if(placements) {
            ITERATE(CVariation::TPlacements, it, *placements) {
                const CVariantPlacement& p = **it;
                if(p.IsSetStart_offset() || p.IsSetStop_offset()) {
                    continue;
                } else {
                    literal = x_GetLiteralAtLoc(p.GetLoc());
                    break;
                }
            }
        }
    }
    return literal;
}


/*!
 * Convert any simple nucleotide variation to delins form, if possible; throw if not.
 * Precondition: location must be set.
 */
void CVariationUtil::ChangeToDelins(CVariation& v)
{
    v.Index();

    if(v.GetData().IsSet()) {
        NON_CONST_ITERATE(CVariation::TData::TSet::TVariations, it, v.SetData().SetSet().SetVariations()) {
            ChangeToDelins(**it);
        }
    } else if(v.GetData().IsInstance()) {
        CVariation_inst& inst = v.SetData().SetInstance();
        inst.SetType(CVariation_inst::eType_delins);

        if(inst.GetDelta().size() == 0) {
            CRef<CDelta_item> di(new CDelta_item);
            di->SetSeq().SetLiteral().SetLength(0);
            di->SetSeq().SetLiteral().SetSeq_data().SetIupacna().Set("");
            inst.SetDelta().push_back(di);
        } else if(inst.GetDelta().size() > 1) {
            NCBI_THROW(CException, eUnknown, "Deltas of length >1 are not supported");
        } else {
            CDelta_item& di = *inst.SetDelta().front();

            //convert 'del' to 'replace-with-empty-literal'
            if(di.GetAction() == CDelta_item::eAction_del_at) {
                di.ResetAction();
                di.SetSeq().SetLiteral().SetLength(0);
                di.SetSeq().SetLiteral().SetSeq_data().SetIupacna().Set("");
            }

            //convert 'loc' or 'this'-based deltas to literals
            if(di.GetSeq().IsLoc()) {
                CRef<CSeq_literal> literal = x_GetLiteralAtLoc(di.GetSeq().GetLoc());
                di.SetSeq().SetLiteral(*literal);
            } else if(di.GetSeq().IsThis()) {
                CConstRef<CSeq_literal> this_literal = x_FindOrCreateLiteral(v);
                if(!this_literal) {
                    NcbiCerr << MSerial_AsnText << v;
                    NCBI_THROW(CException, eUnknown, "Could not find literal for 'this' location in placements");
                } else {
                    di.SetSeq().SetLiteral().Assign(*this_literal);
                }
            }

            //expand multipliers.
            if(di.IsSetMultiplier()) {
                if(di.GetMultiplier() < 0) {
                    NCBI_THROW(CException, eUnknown, "Encountered negative multiplier");
                } else {
                    CSeq_literal& literal = di.SetSeq().SetLiteral();

                    if(!literal.IsSetSeq_data() || !literal.GetSeq_data().IsIupacna()) {
                        NCBI_THROW(CException, eUnknown, "Expected IUPACNA-type seq-literal");
                    }
                    string str_kernel =  literal.GetSeq_data().GetIupacna().Get();
                    literal.SetSeq_data().SetIupacna().Set("");
                    for(int i = 0; i < di.GetMultiplier(); i++) {
                        literal.SetSeq_data().SetIupacna().Set() += str_kernel;
                    }
                    literal.SetLength(literal.GetSeq_data().GetIupacna().Get().size());
                    if(literal.IsSetFuzz()) {
                        literal.SetFuzz().SetLim(CInt_fuzz::eLim_unk);
                    }

                    di.ResetMultiplier();
                    if(di.IsSetMultiplier_fuzz()) {
                        di.SetMultiplier_fuzz().SetLim(CInt_fuzz::eLim_unk);
                    }
                }
            }

            //Convert ins-X-before-loc to 'replace seq@loc with X + seq@loc'
            if(!di.IsSetAction() || di.GetAction() == CDelta_item::eAction_morph) {
                ;  //already done
            } else if(di.GetAction() == CDelta_item::eAction_ins_before) {
                di.ResetAction();
                CConstRef<CSeq_literal> this_literal = x_FindOrCreateLiteral(v);


                if(!this_literal) {
                    NCBI_THROW(CException, eUnknown, "Could not find literal for 'this' location in placements");
                } else if(this_literal->GetLength() == 0) {
                    NCBI_THROW(CException, eUnknown, "Encountered empty literal");
                } else {
                    //Note: need to be careful here: if dinucleotide or multinucleotide, "before" really means "before last pos"
                    //      (as dinucleotide placement is used for insertions in order to be strand-flippable)

                    CRef<CSeq_literal> literal = s_SpliceLiterals(di.GetSeq().GetLiteral(),
                                                                  *this_literal,
                                                                  this_literal->GetLength() - 1);
                    di.SetSeq().SetLiteral().Assign(*literal);
                }
            }
        }
    }
}



void CVariationUtil::AttachProteinConsequences(CVariation& v, const CSeq_id* target_id, bool ignore_genomic)
{
    v.Index();

    if(v.GetData().IsSet()) {
        NON_CONST_ITERATE(CVariation::TData::TSet::TVariations, it, v.SetData().SetSet().SetVariations()) {
            AttachProteinConsequences(**it, target_id, ignore_genomic);
        }
        return;
    }

    if(!v.GetData().IsInstance()) {
        return;
    }


    const CVariation::TPlacements* placements = s_GetPlacements(v);

    if(!placements || placements->size() == 0) {
        return;
    }


    if(v.GetData().GetInstance().IsSetObservation()
       && !(v.GetData().GetInstance().GetObservation() & CVariation_inst::eObservation_variant))
    {
        //only compute placements for variant instances; (as for asserted/reference there's no change)
        return;
    }

    ITERATE(CVariation::TPlacements, it, *placements) {
        const CVariantPlacement& placement = **it;

        if(!placement.GetLoc().GetId()
           || (target_id && !sequence::IsSameBioseq(*target_id,
                                                   sequence::GetId(placement.GetLoc(), NULL),
                                                   m_scope)))
        {
            continue;
        }

        if(placement.IsSetStart_offset() && (placement.IsSetStop_offset() || placement.GetLoc().IsPnt())) {
            continue; //intronic.
        }

        if(   placement.GetMol() != CVariantPlacement::eMol_cdna
           && placement.GetMol() != CVariantPlacement::eMol_mitochondrion
           && (placement.GetMol() != CVariantPlacement::eMol_genomic || ignore_genomic))
        {
            continue;
        }


        CCdregionIndex::TCdregions cdregions;
        m_cdregion_index.Get(placement.GetLoc(), cdregions);

        ITERATE(CCdregionIndex::TCdregions, it, cdregions) {
            CRef<CVariation> prot_variation = TranslateNAtoAA(v.GetData().GetInstance(), placement, *it->cdregion_feat);
            CVariation::TConsequence::value_type consequence(new CVariation::TConsequence::value_type::TObjectType);
            consequence->SetVariation(*prot_variation);
            v.SetConsequence().push_back(consequence);
        }
    }
}

//translate IUPACNA literal; do not translate last partial codon.
string Translate(const string& nuc_str)
{
    string prot_str;
    CSeqTranslator::Translate(
            nuc_str,
            prot_str,
            CSeqTranslator::fIs5PrimePartial);

    //truncate everything past the first stop
    size_t stop_pos = prot_str.find('*');
    if(stop_pos != NPOS) {
        prot_str.resize(stop_pos + 1);
    }

    return prot_str;
}

CVariation_inst::EType CalcInstTypeForAA(const string& prot_ref_str, const string& prot_delta_str)
{
    CVariation_inst::EType inst_type;
    if(prot_delta_str.size() == 0 && prot_ref_str.size() > 0) {
        inst_type = CVariation_inst::eType_del;
    } else if(prot_delta_str.size() > 0 && prot_ref_str.size() == 0) {
        inst_type = CVariation_inst::eType_ins;
    } else if(prot_delta_str.size() != prot_ref_str.size()) {
        inst_type = CVariation_inst::eType_prot_other;
    } else if(prot_ref_str == prot_delta_str) {
        inst_type = CVariation_inst::eType_prot_silent;
    } else if(prot_ref_str.size() > 1) {
        inst_type = CVariation_inst::eType_prot_other;
    } else if(NStr::Find(prot_delta_str, "*") != NPOS) {
        inst_type = CVariation_inst::eType_prot_nonsense;
    } else {
        inst_type = CVariation_inst::eType_prot_missense;
    }
    return inst_type;
}

CVariantProperties::TEffect CalcEffectForProt(const string& prot_ref_str, const string& prot_delta_str)
{
    CVariantProperties::TEffect effect = 0;
    for(size_t i = 0; i < prot_ref_str.size() && i < prot_delta_str.size(); i++) {
        if(prot_ref_str[i] == prot_delta_str[i]) {
            effect |= CVariantProperties::eEffect_synonymous;
        } else if(prot_ref_str[i] == '*') {
            effect |= CVariantProperties::eEffect_stop_loss;
        } else if(prot_delta_str[i] == '*') {
            effect |= CVariantProperties::eEffect_stop_gain;
        } else {
            effect |= CVariantProperties::eEffect_missense;
        }
    }
    return effect;
}

CVariationUtil::TSOTerms CalcSOTermsForProt(TSignedSeqPos nuc_delta_len,
                                            const string& prot_ref_str,
                                            const string& prot_variant_str)
{
    CVariationUtil::TSOTerms terms;

    bool stop_gain = false;
    bool stop_loss = false;
    for(size_t i = 0; i < max(prot_ref_str.size(), prot_variant_str.size()); i++) {
        char r = i >= prot_ref_str.size() ? '-' : prot_ref_str[i];
        char v = i >= prot_variant_str.size() ? '-' : prot_variant_str[i];

        if(r == '*' && v != '*') {
            stop_loss = true;
        }

        if(r != '*' && v == '*') {
            stop_gain = true;
        }
    }
    if(stop_gain) {
        terms.push_back(CVariationUtil::eSO_stop_gained);
    }
    if(stop_loss) {
        terms.push_back(CVariationUtil::eSO_stop_lost);
    }

    if(nuc_delta_len == 0) {
        if(!stop_gain && !stop_loss) {
            terms.push_back(prot_ref_str == prot_variant_str ? CVariationUtil::eSO_synonymous_variant 
                                                             : CVariationUtil::eSO_missense_variant);
        }
    } else if(nuc_delta_len % 3 == 0) {
        terms.push_back(CVariationUtil::eSO_inframe_indel);
    } else {
        terms.push_back(CVariationUtil::eSO_frameshift_variant);
    }

    return terms;
}


void CVariationUtil::FlipStrand(CVariantPlacement& vp) const
{
    vp.SetLoc().FlipStrand();
    if(vp.IsSetSeq() && vp.GetSeq().IsSetSeq_data()) {
        CSeqportUtil::ReverseComplement(vp.SetSeq().SetSeq_data(), &vp.SetSeq().SetSeq_data(), 0, vp.GetSeq().GetLength());
    }

    CRef<CVariantPlacement> tmp(new CVariantPlacement);
    tmp->Assign(vp);

    if(tmp->IsSetStart_offset()) {
        vp.SetStop_offset(tmp->GetStart_offset() * -1);
    } else {
        vp.ResetStop_offset();
    }

    if(tmp->IsSetStop_offset()) {
        vp.SetStart_offset(tmp->GetStop_offset() * -1);
    } else {
        vp.ResetStart_offset();
    }

    if(tmp->IsSetStart_offset_fuzz()) {
        vp.SetStop_offset_fuzz().Assign(tmp->GetStart_offset_fuzz());
    } else {
        vp.ResetStop_offset_fuzz();
    }

    if(tmp->IsSetStop_offset_fuzz()) {
        vp.SetStart_offset_fuzz().Assign(tmp->GetStop_offset_fuzz());
    } else {
        vp.ResetStart_offset_fuzz();
    }
}


bool CVariationUtil::s_IsInstStrandFlippable(const CVariation& v, const CVariation_inst& inst)
{
    bool ret = true;
    ITERATE(CVariation_inst::TDelta, it, v.GetData().GetInstance().GetDelta()) {
        const CDelta_item& di = **it;
        if(di.IsSetAction() && di.GetAction() == CDelta_item::eAction_ins_before) {
            const CVariation::TPlacements* p = s_GetPlacements(v);
            ret = false; //will set back to true if found acceptable placement
            if(p) {
                ITERATE(CVariation::TPlacements, it, *p) {
                    if(s_GetLength(**it, NULL) >= 2 ) {
                        ret = true;
                    }
                }
            }
        }
    }
    return ret;
}

void CVariationUtil::FlipStrand(CVariation& v) const
{
    v.Index(); //required so that can get factored placements from sub-variations.
    if(v.IsSetPlacements()) {
        NON_CONST_ITERATE(CVariation::TPlacements, it, v.SetPlacements()) {
            FlipStrand(**it);
        }
    }

    if(v.GetData().IsSet()) {
        NON_CONST_ITERATE(CVariation::TData::TSet::TVariations, it, v.SetData().SetSet().SetVariations()) {
            FlipStrand(**it);
        }
    } else if(v.GetData().IsInstance()) {
        if(!s_IsInstStrandFlippable(v, v.GetData().GetInstance())) {
            NCBI_THROW(CException, eUnknown, "Variation is not strand-flippable");
        } else {
            FlipStrand(v.SetData().SetInstance());
        }
    }
}


void CVariationUtil::FlipStrand(CDelta_item& di) const
{
    if(!di.IsSetSeq()) {
        return;
    }
    if(di.GetSeq().IsLoc()) {
        di.SetSeq().SetLoc().FlipStrand();
    } else if(di.GetSeq().IsLiteral() && di.GetSeq().GetLiteral().IsSetSeq_data()) {
        CSeqportUtil::ReverseComplement(
                di.SetSeq().GetLiteral().GetSeq_data(),
                &di.SetSeq().SetLiteral().SetSeq_data(),
                0, di.GetSeq().GetLiteral().GetLength());
    }
}


void CVariationUtil::FlipStrand(CVariation_inst& inst) const
{
    if(!inst.IsSetDelta()) {
        return;
    }

    NON_CONST_ITERATE(CVariation_inst::TDelta, it, inst.SetDelta()) {
        FlipStrand(**it);
    }
    reverse(inst.SetDelta().begin(), inst.SetDelta().end());
}


void CVariationUtil::x_AdjustDelinsToInterval(CVariation& v, const CSeq_loc& loc)
{
    //note: loc may be a split loc when part of a codon is in another exon

    if(!v.IsSetPlacements() || v.GetPlacements().size() != 1) {
        NCBI_THROW(CException, eUnknown, "Expected single placement");
    }

    CRef<CSeq_loc> range_loc = sequence::Seq_loc_Merge(loc, CSeq_loc::fMerge_SingleRange, NULL);

    const CSeq_loc& orig_loc = v.GetPlacements().front()->GetLoc();
    CRef<CSeq_loc> sub_loc = orig_loc.Subtract(loc, 0, NULL, NULL);
    if(sub_loc->Which() && sequence::GetLength(*sub_loc, NULL) > 0) {
        //NcbiCerr << MSerial_AsnText << v;
        //NcbiCerr << MSerial_AsnText << loc;
        NCBI_THROW(CException, eUnknown, "Location must be a superset of the variation's loc");
    }

    sub_loc->Assign(*range_loc);
    sub_loc->SetInt().SetTo(sequence::GetStop(orig_loc, NULL, eExtreme_Positional));
    CRef<CSeq_loc> suffix_loc = sequence::Seq_loc_Subtract(loc, *sub_loc, CSeq_loc::fSortAndMerge_All, NULL);
    if(!suffix_loc->Which()) {
        suffix_loc->SetNull(); //bug in subtract
    }

    sub_loc->Assign(*range_loc);
    sub_loc->SetInt().SetFrom(sequence::GetStart(orig_loc, NULL, eExtreme_Positional));
    CRef<CSeq_loc> prefix_loc = sequence::Seq_loc_Subtract(loc, *sub_loc, CSeq_loc::fSortAndMerge_All, NULL);
    if(!prefix_loc->Which()) {
        prefix_loc->SetNull();
    }

    if(sequence::GetStrand(loc, NULL) == eNa_strand_minus) {
        swap(prefix_loc, suffix_loc);
    }

    //NcbiCerr << "prefix loc" << MSerial_AsnText << *prefix_loc;
    //NcbiCerr << "suffix loc" << MSerial_AsnText << *suffix_loc;


    CRef<CSeq_literal> prefix_literal = x_GetLiteralAtLoc(*prefix_loc);
    CRef<CSeq_literal> suffix_literal = x_GetLiteralAtLoc(*suffix_loc);

    for(CTypeIterator<CVariation_inst> it(Begin(v)); it; ++it) {
        CVariation_inst& inst = *it;
        if(inst.GetDelta().size() != 1) {
            NCBI_THROW(CException, eUnknown, "Expected single-element delta");
        }

        CDelta_item& delta = *inst.SetDelta().front();

        if(!delta.IsSetSeq() || !delta.GetSeq().IsLiteral()) {
            NCBI_THROW(CException, eUnknown, "Expected literal");
        }

        CRef<CSeq_literal> tmp_literal1 = s_CatLiterals(*prefix_literal, delta.SetSeq().SetLiteral());
        CRef<CSeq_literal> tmp_literal2 = s_CatLiterals(*tmp_literal1, *suffix_literal);
        delta.SetSeq().SetLiteral(*tmp_literal2);
    }

    v.SetPlacements().front()->SetLoc().Assign(loc);
}

bool Contains(const CSeq_loc& a, const CSeq_loc& b, CScope* scope)
{
    CRef<CSeq_loc> a1(new CSeq_loc);
    a1->Assign(a);
    a1->ResetStrand();

    CRef<CSeq_loc> b1(new CSeq_loc);
    b1->Assign(b);
    b1->ResetStrand();

    return sequence::Compare(*a1, *b1, scope) == sequence::eContains;
}

CRef<CVariation> CVariationUtil::x_CreateUnknownVariation(const CSeq_id& id, CVariantPlacement::TMol mol)
{
    CRef<CVariantPlacement> p(new CVariantPlacement);
    p->SetLoc().SetWhole().Assign(id);
    p->SetMol(mol);
    CRef<CVariation> v(new CVariation);
    v->SetData().SetUnknown();
    v->SetPlacements().push_back(p);
    ChangeIdsInPlace(*v, sequence::eGetId_ForceAcc, *m_scope);
    return v;
}

size_t GetCommonPrefixLen(const string& a, const string& b)
{
    size_t i(0);
    while(i < a.size() && i < b.size() && a[i] == b[i]) {
        i++;
    }
    return i;
}

size_t GetCommonSuffixLen(const string& a, const string& b)
{
    size_t i(0);
    while(i < a.size() && i < b.size() && a[a.size() - 1 - i] == b[b.size() - 1 - i]) {
        i++;
    }
    return i;
}

CRef<CVariation> CVariationUtil::TranslateNAtoAA(
        const CVariation_inst& nuc_inst,
        const CVariantPlacement& nuc_p,
        const CSeq_feat& cds_feat)
{
    bool verbose = false;

    if(!sequence::IsSameBioseq(sequence::GetId(nuc_p.GetLoc(), NULL),
                               sequence::GetId(cds_feat.GetLocation(), NULL),
                               m_scope))
    {
        NCBI_THROW(CException, eUnknown, "Placement and CDS are on different seqs");
    }


    //if placement is not a proper subset of CDS, create and return "unknown effect" variation
    if(nuc_p.IsSetStart_offset()
      || nuc_p.IsSetStop_offset()
      || !Contains(cds_feat.GetLocation(), nuc_p.GetLoc(), m_scope))
    {
        return x_CreateUnknownVariation(sequence::GetId(cds_feat.GetProduct(), NULL), CVariantPlacement::eMol_protein);
    }

    //create an inst-variation from the provided inst with the single specified placement
    CRef<CVariation> v(new CVariation);
    v->SetData().SetInstance().Assign(nuc_inst);
    v->ResetPlacements();

    CRef<CVariantPlacement> p(new CVariantPlacement);
    p->Assign(nuc_p);
    p->ResetSeq(); //Will recalculate after adjusting to codon boundary
    v->SetPlacements().push_back(p);

    //normalize to delins form so we can deal with it uniformly
    ChangeToDelins(*v);
    const CDelta_item& nuc_delta = *v->GetData().GetInstance().GetDelta().front();

    //note: using type long instead of TSignedSeqPos is a bug on 64-bit systems: the result of the subtraction
    //that's expected to be negative would be a BIG_NUMBER that is cast to type long without the wrap-around
    //into negatives.
    TSignedSeqPos nuc_delta_len = nuc_delta.GetSeq().GetLiteral().GetLength() - sequence::GetLength(p->GetLoc(), NULL);

    int frameshift_phase = nuc_delta_len % 3;
    if(frameshift_phase < 0) {
        frameshift_phase += 3;
    }

    if(!SameOrientation(sequence::GetStrand(cds_feat.GetLocation(), NULL),
                        sequence::GetStrand(p->GetLoc(), NULL)))
    {
        //need the variation and cds to be in the same orientation, such that
        //sequence represents the actual codons and AAs when mapped to protein
        FlipStrand(*v);
    }

    if(verbose) NcbiCerr << "Normalized variation: " << MSerial_AsnText << *v;

    if(!nuc_delta.GetSeq().IsLiteral() || !nuc_delta.GetSeq().GetLiteral().IsSetSeq_data()) {
        return x_CreateUnknownVariation(sequence::GetId(cds_feat.GetProduct(), NULL), CVariantPlacement::eMol_protein);
    }

    //Map to protein and back to get the affected codons.
    //Note: need the scope because the cds_feat is probably gi-based, while our locs are probably accver-based

    CRef<CSeq_loc_Mapper> nuc2prot_mapper;
    CRef<CSeq_loc_Mapper> prot2nuc_mapper;
    try {
        nuc2prot_mapper.Reset(new CSeq_loc_Mapper(cds_feat, CSeq_loc_Mapper::eLocationToProduct, m_scope));
        prot2nuc_mapper.Reset(new CSeq_loc_Mapper(cds_feat, CSeq_loc_Mapper::eProductToLocation, m_scope));
    } catch (CException&) {
        //may legitimately throw if feature is not good for mapping (e.g. partial).
        return x_CreateUnknownVariation(sequence::GetId(cds_feat.GetProduct(), NULL), CVariantPlacement::eMol_protein);
    }

    CRef<CSeq_loc> prot_loc = nuc2prot_mapper->Map(p->GetLoc());
    CRef<CSeq_loc> codons_loc = prot2nuc_mapper->Map(*prot_loc);

    if(codons_loc->IsNull()) {
        //normally shouldn't happen, but may happen with dubious annotation, e.g. BC149603.1:c.1A>G.
        //Mapping to protein coordinates returns NULL, probably because protein and cds lengths are inconsistent.
        return x_CreateUnknownVariation(sequence::GetId(cds_feat.GetProduct(), NULL), CVariantPlacement::eMol_protein);
    }

    string downstream_cds_suffix_seq_str; //cds sequence downstream of affected codons
    {{
        //extend cds-loc downstream by a little bit to allow inferring changes in the stop-codon
        //(e.g. base in the stop-codon is deleted, and the sequence downstream of cds is now involved)
        CRef<CSeq_loc> ext_cds_loc;
        {{
            SFlankLocs flocs = CreateFlankLocs(cds_feat.GetLocation(), 6);
            ext_cds_loc = sequence::Seq_loc_Add(cds_feat.GetLocation(), *flocs.downstream, CSeq_loc::fSortAndMerge_All, NULL);
        }}

        CRef<CSeq_loc> downstream_cds_loc;
        {{
            SFlankLocs flocs = CreateFlankLocs(*codons_loc, 1000);
            downstream_cds_loc = ext_cds_loc->Intersect(*flocs.downstream, CSeq_loc::fSortAndMerge_All, NULL);
        }}

        CRef<CSeq_literal> literal = x_GetLiteralAtLoc(*downstream_cds_loc);
        if(verbose) NcbiCerr << "Downstream-cds: " << MSerial_AsnText << *literal;

        if(literal->GetLength() > 0) {
            downstream_cds_suffix_seq_str = literal->GetSeq_data().GetIupacna().Get(); 
        }
    }}

    TSignedSeqPos frame = abs(
              (TSignedSeqPos)p->GetLoc().GetStart(eExtreme_Biological)
            - (TSignedSeqPos)codons_loc->GetStart(eExtreme_Biological));

    codons_loc->SetId(sequence::GetId(p->GetLoc(), NULL)); //restore the original id, as mapping forward and back may have changed the type
    ChangeIdsInPlace(*prot_loc, sequence::eGetId_ForceAcc, *m_scope); //not strictly required, but generally prefer accvers in the output for readability
                                                                      //as we're dealing with various types of mols

    x_AdjustDelinsToInterval(*v, *codons_loc);
    AttachSeq(*v, 100000); //need enough sequnece to get create reference and variant translations downstream
    if(!v->GetPlacements().front()->GetSeq().IsSetSeq_data()) {
        return x_CreateUnknownVariation(sequence::GetId(cds_feat.GetProduct(), NULL), CVariantPlacement::eMol_protein);
    }
    if(verbose) NcbiCerr << "Adjusted-for-codons " << MSerial_AsnText << *v;

    string nuc_ref_prefix = v->GetPlacements().front()->GetSeq().GetSeq_data().GetIupacna().Get();

    const CSeq_literal& nuc_var_literal =  v->GetData().GetInstance().GetDelta().front()->GetSeq().GetLiteral();
    string nuc_var_prefix = nuc_var_literal.GetLength() == 0 ? "" : nuc_var_literal.GetSeq_data().GetIupacna().Get();

    string nuc_ref_str = nuc_ref_prefix + downstream_cds_suffix_seq_str;
    string nuc_var_str = nuc_var_prefix + downstream_cds_suffix_seq_str;
    string prot_ref_str = Translate(nuc_ref_str);
    string prot_var_str = Translate(nuc_var_str);
    int num_ref_codons = (nuc_ref_prefix.size() + 2) / 3;
    int num_var_codons = (nuc_var_prefix.size() + 2) / 3;

    if(verbose) NcbiCerr << "prot_ref_str: " << prot_ref_str << "\n"
                         << "prot_var_str: " << prot_var_str << "\n";


    int common_prot_prefix_len(0); //will calculate length of unchanged leading AAs in translations (starting with codons of interest)


    if(prot_ref_str == prot_var_str) {
        //No-change variation. Will truncate to original counts of affected codons, such that the 
        //no-change variation describes the original codons that did not affect the translation.
        prot_ref_str.resize(min(static_cast<int>(prot_ref_str.size()), num_ref_codons));
        prot_var_str.resize(prot_ref_str.size());

        if(prot_ref_str.size() > 0 && *prot_ref_str.rbegin() == '*') {
            //If translated all the way to stop with no-changes, this is no longer a frameshift, even though the indel is not mod%3
            frameshift_phase = 0;
        }

    } else {

        //Justify the variation to first affected AA.
        //
        //Truncate common prefix of translations; truncate the 5'-end of the prot-loc and recalculate codons-loc accordingly.
        //Rationale: nucleotide level might not affect translation at the affected codons' location, but change the translation
        //downstream. That's why we have to skip unchanged translation to find the first affected AA.
        //
        //This does not apply to synonymous changes where there's no change at all rather than a downstream change.

        common_prot_prefix_len = GetCommonPrefixLen(prot_ref_str, prot_var_str);
        if(common_prot_prefix_len == static_cast<int>(prot_ref_str.size())) {
            common_prot_prefix_len -= 1; //when truncating common prefx, don't want to truncate all the way 
                                     //will leave last position as necessary
        }

        prot_ref_str = prot_ref_str.substr(common_prot_prefix_len);
        prot_var_str = prot_var_str.substr(common_prot_prefix_len);
      
        if(frameshift_phase == 0) {

            //if translations' common suffix is long, will capture the sequence change up to it
            //  prot_ref_str:  CWDADPLKRPTFKQIVQLIEKQISES*  -> C
            //  prot_var_str: LPWDADPLKRPTFKQIVQLIEKQISES*  -> LP
            //
            //Otherwise will capture the change up to terminal codon in either reference or variant translation
            //  prot_ref_str: WS*                           -> WS*
            //  prot_var_str: CWDADPLKRPTFKQIVQLIEKQISES*   -> CWD
            //  or
            //  prot_ref_str: CWDADPLKRPTFKQIVQLIEKQISES*   -> CWD
            //  prot_var_str: WS*                           -> WS*
            size_t suffix_len = GetCommonSuffixLen(prot_ref_str, prot_var_str);
            if(2 * suffix_len >= max(prot_ref_str.size(), prot_var_str.size())) {
                //Note: '>=' because in edge-case X* -> Y* the common suffix is '*' and we want to truncate. VAR-699

                prot_ref_str.resize(prot_ref_str.size() - suffix_len);
                prot_var_str.resize(prot_var_str.size() - suffix_len);
            } else if(NStr::EndsWith(prot_var_str, "*") && prot_ref_str.size() > prot_var_str.size()) {
                prot_ref_str.resize(prot_var_str.size());
            } else if(NStr::EndsWith(prot_ref_str, "*") && prot_var_str.size() > prot_ref_str.size()) {
                prot_var_str.resize(prot_ref_str.size());
            }
        } else {
            //Keep the frst AA in case of frameshifts
            //NM_000492.3:c.3528delC -> NP_000483.3:p.Lys1177Serfs  instead of NP_000483.3:p.Lys1177delfs 
            prot_ref_str.resize(min(static_cast<size_t>(1), prot_ref_str.size()));
            prot_var_str.resize(min(static_cast<size_t>(1), prot_ref_str.size()));
        }

        //adjust the protein location. 
        prot_loc = sequence::Seq_loc_Merge(*prot_loc, CSeq_loc::fMerge_SingleRange, NULL); //to convert point to interval
        if(prot_ref_str.size() == 0) {
            //After truncating common prefix and suffix the reference is reduced to nothing - we have pure insertion.
            //We need 2-AA location describing the point of insertion
            prot_loc->SetInt().SetFrom() += common_prot_prefix_len - 1;
            prot_loc->SetInt().SetTo(prot_loc->SetInt().SetFrom() + 1);
        } else {
            //the location describes the sequence that's being changed
            prot_loc->SetInt().SetFrom() += common_prot_prefix_len;
            prot_loc->SetInt().SetTo(prot_loc->SetInt().SetFrom() + prot_ref_str.size() - 1);
        }
        prot_loc = sequence::Seq_loc_Merge(*prot_loc, 0, NULL); //to convert single-pos int to point, as appropriate

        codons_loc = prot2nuc_mapper->Map(*prot_loc); 
        codons_loc->SetId(sequence::GetId(p->GetLoc(), NULL));

        if(verbose) NcbiCerr << "prot_ref_str: " << prot_ref_str << "\n"
                             << "prot_var_str: " << prot_var_str << "\n";
    }


    if(verbose)  NcbiCerr << "reference codons: " << num_ref_codons << "; variant codons: " << num_var_codons << "; common prefix: " << common_prot_prefix_len << "\n";


    //if(verbose) NcbiCerr << "prot_ref  : " << prot_ref_str << "\n" << "prot_delta: " << prot_var_str << "\ntruncated prefix: " << common_prot_prefix_len << "\n Adjusted codons loc: " << MSerial_AsnText << *codons_loc;

    //Constructing protein-variation

    CRef<CVariation> prot_v(new CVariation);

    //will have two placements: one describing the AA, and the other describing codons
    CRef<CVariantPlacement> prot_p(new CVariantPlacement);
    if(cds_feat.IsSetExcept() && cds_feat.GetExcept()
       && !(cds_feat.IsSetExcept_text() && cds_feat.GetExcept_text() == "mismatches in translation"))
    {
        //mapped protein position is bogus, as cds is either partial or there are indels.
        prot_p->SetLoc().SetEmpty().Assign(sequence::GetId(*prot_loc, NULL));
        prot_p->SetMol(GetMolType(sequence::GetId(prot_p->GetLoc(), NULL)));
        prot_p->SetSeq().SetLength(prot_ref_str.size());
        prot_p->SetSeq().SetSeq_data().SetNcbieaa().Set(prot_ref_str);
    } else {
        prot_p->SetLoc(*prot_loc);
        prot_p->SetMol(GetMolType(sequence::GetId(prot_p->GetLoc(), NULL)));
        AttachSeq(*prot_p);
    }
    prot_v->SetPlacements().push_back(prot_p);


    CRef<CVariantPlacement> codons_p(new CVariantPlacement);
    codons_p->SetLoc(*codons_loc);
    codons_p->SetMol(GetMolType(sequence::GetId(codons_p->GetLoc(), NULL)));
    codons_p->SetFrame(frame);
    AttachSeq(*codons_p);
    prot_v->SetPlacements().push_back(codons_p);

  
    //calculate properties 
    {{
        if(frameshift_phase == 0 && prot_ref_str.size() == prot_var_str.size()) { 
            //VAR-267 - calculate missense/synonymous/stop-gain/loss for non-frameshifting and non-length-changing cases only
            CVariantProperties::TEffect prop = CalcEffectForProt(prot_ref_str, prot_var_str);
            if(prop != 0) {
                prot_v->SetVariant_prop().SetEffect(prop);
            }
        }

        TSOTerms so_terms = CalcSOTermsForProt(nuc_delta_len,
                                               prot_ref_str,
                                               prot_var_str);
        copy(so_terms.begin(), so_terms.end(), back_inserter(prot_v->SetSo_terms()));
    }}


    prot_v->SetData().SetInstance().SetType(CalcInstTypeForAA(prot_ref_str, prot_var_str));



    //set inst data
    CRef<CDelta_item> di(new CDelta_item);
    prot_v->SetData().SetInstance().SetDelta().push_back(di);

    if(prot_var_str.size() > 0) {

        //Use NA alphabet instead of AA, as the dbSNP requested original codons.
        //The downstream process can always translate this to AAs.
        //
        //Note, however, that we cannot always use the original variant codons sequence, because
        //the location may have been adjusted downstream (see comments at TruncateCommonPrefixAndSuffix).
        //So we'll have to get this from nuc_var_str.
        if(false && common_prot_prefix_len == 0) {
            di->SetSeq().Assign(v->GetData().GetInstance().GetDelta().front()->GetSeq());
        } else {
            string adjusted_codons_str = nuc_var_str.substr(common_prot_prefix_len * 3, prot_var_str.size() * 3);
            di->SetSeq().SetLiteral().SetLength(adjusted_codons_str.size());
            di->SetSeq().SetLiteral().SetSeq_data().SetIupacna().Set() = adjusted_codons_str;
        }

        if(prot_ref_str.size() == 0) {
            di->SetAction(CDelta_item::eAction_ins_before);
        }
    } else {
        di->SetSeq().SetThis();
        di->SetAction(CDelta_item::eAction_del_at);
    }

    //set frameshift
    if(frameshift_phase != 0) {
        prot_v->SetVariant_prop().SetEffect() |= CVariantProperties::eEffect_frameshift;
        prot_v->SetFrameshift().SetPhase(frameshift_phase);
    }

    if(verbose) NcbiCerr << "protein variation:"  << MSerial_AsnText << *prot_v;
    if(verbose) NcbiCerr << "Done with protein variation\n";

    return prot_v;
}



////////////////////////////////////////////////////////////////////////////////
//
// SO-terms calculations
//
////////////////////////////////////////////////////////////////////////////////


void CVariationUtil::AsSOTerms(const CVariantProperties& p, TSOTerms& terms)
{
    if(p.GetGene_location() & CVariantProperties::eGene_location_intergenic) {
        terms.push_back(eSO_intergenic_variant);
    }
    if(p.GetGene_location() & CVariantProperties::eGene_location_near_gene_5) {
        terms.push_back(eSO_2KB_upstream_variant);
    }
    if(p.GetGene_location() & CVariantProperties::eGene_location_near_gene_3) {
        terms.push_back(eSO_500B_downstream_variant);
    }
    if(p.GetGene_location() & CVariantProperties::eGene_location_donor) {
        terms.push_back(eSO_splice_donor_variant);
    }
    if(p.GetGene_location() & CVariantProperties::eGene_location_acceptor) {
        terms.push_back(eSO_splice_acceptor_variant);
    }
    if(p.GetGene_location() & CVariantProperties::eGene_location_intron) {
        terms.push_back(eSO_intron_variant);
    }
    if(p.GetGene_location() & CVariantProperties::eGene_location_utr_5) {
        terms.push_back(eSO_5_prime_UTR_variant);
    }
    if(p.GetGene_location() & CVariantProperties::eGene_location_utr_3) {
        terms.push_back(eSO_3_prime_UTR_variant);
    }
    if(p.GetGene_location() & CVariantProperties::eGene_location_conserved_noncoding) {
        terms.push_back(eSO_nc_transcript_variant);
    }
    if(p.GetGene_location() & CVariantProperties::eGene_location_in_start_codon) {
        terms.push_back(eSO_initiator_codon_variant);
    }
    if(p.GetGene_location() & CVariantProperties::eGene_location_in_stop_codon) {
        terms.push_back(eSO_terminator_codon_variant);
    }

    if(p.GetEffect() & CVariantProperties::eEffect_frameshift) {
        terms.push_back(eSO_frameshift_variant);
    }
    if(p.GetEffect() & CVariantProperties::eEffect_missense) {
        terms.push_back(eSO_missense_variant);
    }

    if(p.GetEffect() & CVariantProperties::eEffect_nonsense || p.GetEffect() & CVariantProperties::eEffect_stop_gain) {
        terms.push_back(eSO_stop_gained);
    }
    if(p.GetEffect() & CVariantProperties::eEffect_nonsense || p.GetEffect() & CVariantProperties::eEffect_stop_loss) {
        terms.push_back(eSO_stop_lost);
    }
    if(p.GetEffect() & CVariantProperties::eEffect_synonymous) {
        terms.push_back(eSO_synonymous_variant);
    }
}

string CVariationUtil::AsString(ESOTerm term)
{
    return 
      term == eSO_intergenic_variant      ?  "intergenic_variant"    
    : term == eSO_2KB_upstream_variant    ?  "2KB_upstream_variant"    
    : term == eSO_500B_downstream_variant ?  "500B_downstream_variant"
    : term == eSO_splice_donor_variant    ?  "splice_donor_variant"    
    : term == eSO_splice_acceptor_variant ?  "splice_acceptor_varian"
    : term == eSO_intron_variant          ?  "intron_variant"    
    : term == eSO_5_prime_UTR_variant     ?  "5_prime_UTR_variant"    
    : term == eSO_3_prime_UTR_variant     ?  "3_prime_UTR_variant"    
    : term == eSO_coding_sequence_variant ?  "coding_sequence_variant"
    : term == eSO_nc_transcript_variant   ?  "nc_transcript_variant"
    : term == eSO_initiator_codon_variant ?  "initiator_codon_variant"
    : term == eSO_terminator_codon_variant?  "terminator_codon_variant"

    : term == eSO_synonymous_variant      ?  "synonymous_variant"    
    : term == eSO_missense_variant        ?  "missense_variant"    
    : term == eSO_frameshift_variant      ?  "frameshift_variant"    
    : term == eSO_inframe_indel           ?  "inframe_indel"
    : term == eSO_stop_gained             ?  "stop_gained"    
    : term == eSO_stop_lost               ?  "stop_lost"    
    :                                        "other_variant";
};



const CVariation::TPlacements* CVariationUtil::s_GetPlacements(const CVariation& v)
{
    if(v.IsSetPlacements()) {
        return &v.GetPlacements();
    } else if(v.GetParent()) {
        return s_GetPlacements(*v.GetParent());
    } else {
        return NULL;
    }
}

const CConstRef<CSeq_literal> CVariationUtil::s_FindFirstLiteral(const CVariation& v)
{
    const CVariation::TPlacements* placements = s_GetPlacements(v);
    ITERATE(CVariation::TPlacements, it, *placements) {
        const CVariantPlacement& p = **it;
        if(p.IsSetSeq()) {
            return CConstRef<CSeq_literal>(&p.GetSeq());
        }
    }
    return CConstRef<CSeq_literal>(NULL);
}

const CConstRef<CSeq_literal> CVariationUtil::s_FindAssertedLiteral(const CVariation& v)
{
    const CVariation* parent = v.GetParent();
    if(parent == NULL) {
        return CConstRef<CSeq_literal>(NULL);
    } else {
        if(parent->GetData().IsSet()) {
            ITERATE(CVariation::TData::TSet::TVariations, it, parent->GetData().GetSet().GetVariations()) {
                const CVariation& sibling = **it;
                if(sibling.GetData().IsInstance()
                   && sibling.GetData().GetInstance().IsSetObservation()
                   && sibling.GetData().GetInstance().GetObservation() == CVariation_inst::eObservation_asserted
                   && sibling.GetData().GetInstance().GetDelta().size() == 1
                   && sibling.GetData().GetInstance().GetDelta().front()->IsSetSeq()
                   && sibling.GetData().GetInstance().GetDelta().front()->GetSeq().IsLiteral())
                {
                    return CConstRef<CSeq_literal>(&sibling.GetData().GetInstance().GetDelta().front()->GetSeq().GetLiteral());
                }
            }
        }

        return s_FindAssertedLiteral(*parent);
    }
}


bool Equals(const CVariation::TPlacements& p1, const CVariation::TPlacements& p2)
{
    if(p1.size() != p2.size()) {
        return false;
    }
    CVariation::TPlacements::const_iterator it1 = p1.begin();
    CVariation::TPlacements::const_iterator it2 = p2.begin();

    for(; it1 != p1.end() && it2 != p2.end(); ++it1, ++it2) {
        const CVariantPlacement& p1 = **it1;
        const CVariantPlacement& p2 = **it2;
        if(!p1.Equals(p2)) {
            return false;
        }
    }
    return true;
}

void CVariationUtil::s_FactorOutPlacements(CVariation& v)
{
    if(!v.GetData().IsSet()) {
        return;
    }
    NON_CONST_ITERATE(CVariation::TData::TSet::TVariations, it, v.SetData().SetSet().SetVariations()) {
        s_FactorOutPlacements(**it);
    }

    CVariation::TPlacements* p1 = NULL;

    bool ok = true;
    NON_CONST_ITERATE(CVariation::TData::TSet::TVariations, it, v.SetData().SetSet().SetVariations()) {
        CVariation& v2 = **it;
        if(!v2.IsSetPlacements()) {
            ok = false;
            break;
        } else if(!p1) {
            p1 = &v2.SetPlacements();
            continue;
        } else {
            if(!Equals(*p1, v2.GetPlacements())) {
                ok = false;
                break;
            }
        }
    }

    if(ok && p1) {
        //transfer p1 placements to this level
        NON_CONST_ITERATE(CVariation::TPlacements, it, *p1) {
            v.SetPlacements().push_back(*it);
        }

        //reset placements at the children level
        NON_CONST_ITERATE(CVariation::TData::TSet::TVariations, it, v.SetData().SetSet().SetVariations()) {
            CVariation& v2 = **it;
            v2.ResetPlacements();
        }
    }
}


CConstRef<CVariation> CVariationUtil::s_FindConsequenceForPlacement(const CVariation& v, const CVariantPlacement& p)
{
    CConstRef<CVariation> cons_v(NULL);
    if(v.IsSetConsequence() && p.GetLoc().GetId()) {
        ITERATE(CVariation::TConsequence, it, v.GetConsequence()) {
            const CVariation::TConsequence::value_type::TObjectType& cons = **it;
            if(cons.IsVariation() && cons.GetVariation().IsSetPlacements()) {
                ITERATE(CVariation::TPlacements, it2, cons.GetVariation().GetPlacements()) {
                    const CVariantPlacement& vp = **it2;
                    if(vp.GetLoc().GetId() && p.GetLoc().GetId() && vp.GetLoc().GetId()->Equals(*p.GetLoc().GetId())) {
                        cons_v.Reset(&cons.GetVariation());
                        break;
                    }
                }
            }
        }
    }

    if(!cons_v && v.GetData().IsSet()) {
        ITERATE(CVariation::TData::TSet::TVariations, it, v.GetData().GetSet().GetVariations()) {
            CConstRef<CVariation> cons_v1 = s_FindConsequenceForPlacement(**it, p);
            if(cons_v1) {
                cons_v = cons_v1;
                break;
            }
        }
    }

    return cons_v;
}





void CVariationUtil::s_AttachGeneIDdbxref(CVariantPlacement& p, int gene_id)
{
    ITERATE(CVariantPlacement::TDbxrefs, it, p.SetDbxrefs()) {
        const CDbtag& dbtag = **it;
        if(dbtag.GetDb() == "GeneID" && dbtag.GetTag().IsId() && dbtag.GetTag().GetId() == gene_id) {
            return;
        }
    }
    CRef<CDbtag> dbtag(new CDbtag());
    dbtag->SetDb("GeneID");
    dbtag->SetTag().SetId(gene_id);
    p.SetDbxrefs().push_back(dbtag);
}

void CVariationUtil::SetPlacementProperties(CVariantPlacement& placement)
{
    if(!placement.IsSetGene_location()) {
        //need to zero-out the bitmask, otherwise in debug mode it will be preset to a magic value,
        //and then modifying it with "|=" will produce garbage.
        placement.SetGene_location(0);
    }


    //for offset-style intronic locations (not genomic and have offset), can infer where we are based on offset
    if(!placement.IsSetMol() || placement.GetMol() != CVariantPlacement::eMol_genomic) {
        CBioseq_Handle bsh = m_scope->GetBioseqHandle(sequence::GetId(placement.GetLoc(), NULL));
        if(placement.IsSetStart_offset() && placement.GetStart_offset() != 0) {
            x_SetVariantPropertiesForIntronic(placement, placement.GetStart_offset(), placement.GetLoc(), bsh);
        }
        if(placement.IsSetStop_offset() && placement.GetStop_offset() != 0) {
            x_SetVariantPropertiesForIntronic(placement, placement.GetStop_offset(), placement.GetLoc(), bsh);
        }
    }

    CVariantPropertiesIndex::TGeneIDAndPropVector v;
    m_variant_properties_index.GetLocationProperties(placement.GetLoc(), v);

    //note: this assumes that the offsets are HGVS-spec compliant: anchor locations are at the exon terminals, and the
    //offset values point into the intron.
    bool is_completely_intronic = false;
    {{
        bool is_start_offset = placement.IsSetStart_offset() && placement.GetStart_offset() != 0;
        bool is_stop_offset = placement.IsSetStop_offset() && placement.GetStop_offset() != 0;

        //Single anchor point, and have any offset.
        bool is_case1 = sequence::GetLength(placement.GetLoc(), NULL) == 1 && (is_start_offset || is_stop_offset);

        //Other possibility is when start and stop are addressed from different exons:
        //The location length must be 2 (end of one exon and start of the other), and the offsets point inwards.
        bool is_case2 = sequence::GetLength(placement.GetLoc(), NULL) == 2
                        && is_start_offset && placement.GetStart_offset() > 0
                        && is_stop_offset && placement.GetStop_offset() < 0;

        is_completely_intronic = is_case1 || is_case2;
    }}

    //collapse all gene-specific location properties into prop
    ITERATE(CVariantPropertiesIndex::TGeneIDAndPropVector, it, v) {
        int gene_id = it->first;
        CVariantProperties::TGene_location loc_prop = it->second;
        if(gene_id != 0) {
            s_AttachGeneIDdbxref(placement, gene_id);
        }

        if(!is_completely_intronic) {
            //we don't want to compute properties for intronic anchor points. VAR-149
            placement.SetGene_location() |= loc_prop;
        }
    }
}


void CVariationUtil::FindLocationProperties(const CSeq_align& transcript_aln,
                                            const CSeq_loc& query_loc,
                                            TSOTerms& terms)
{
    //note: initializing mapper with scope because the annotation from scope is gi-based,
    //while the parameters are normaly accver-based
    CRef<CSeq_loc_Mapper> mapper(new CSeq_loc_Mapper(transcript_aln, 1, m_scope));

    CConstRef<CSeq_loc> genomic_query_loc;
    if(query_loc.GetId() && query_loc.GetId()->Equals(transcript_aln.GetSeq_id(0))) {
        genomic_query_loc = mapper->Map(query_loc);
    } else {
        genomic_query_loc.Reset(&query_loc);
    }

    CRef<CSeq_loc> rna_loc = transcript_aln.CreateRowSeq_loc(1);

    CRef<CSeq_loc> cds_loc;
    {{
        CBioseq_Handle bsh = m_scope->GetBioseqHandle(transcript_aln.GetSeq_id(0));
        for(CFeat_CI ci(bsh, SAnnotSelector(CSeqFeatData::e_Cdregion)); ci; ++ci) {
            const CMappedFeat& mf = *ci;
            cds_loc = mapper->Map(mf.GetLocation());

            //remove indels from the mapped cds loc
            cds_loc = sequence::Seq_loc_Merge(*cds_loc, CSeq_loc::fMerge_SingleRange, NULL);
            cds_loc = rna_loc->Intersect(*cds_loc, 0, NULL);
            break;
        }
    }}        
 
    s_FindLocationProperties(rna_loc, cds_loc, *genomic_query_loc, terms);
}
        

void CVariationUtil::s_FindLocationProperties(CConstRef<CSeq_loc> rna_loc,
                                              CConstRef<CSeq_loc> cds_loc,
                                              const CSeq_loc& query_loc,
                                              CVariationUtil::TSOTerms& terms)
{
    struct SPropsMap 
    {
        typedef CRangeMap<CVariationUtil::ESOTerm, TSeqPos> TRangeMap;
        typedef map<CSeq_id_Handle, TRangeMap> TIdRangeMap;
        TIdRangeMap loc_map;
        void Add(CVariationUtil::ESOTerm term, const CSeq_loc& loc)
        {
            for(CSeq_loc_CI ci(loc); ci; ++ci) {
                loc_map[ci.GetSeq_id_Handle()][ci.GetRange()] = term;
            }
        }
    } props_map;

    typedef pair<CRef<CSeq_loc>, CRef<CSeq_loc> > TLocsPair;

    if(!rna_loc && !cds_loc) {
        return;
    }

    const CSeq_loc& main_loc = rna_loc ? *rna_loc : *cds_loc;  

    {{
        TLocsPair p = CVariantPropertiesIndex::s_GetNeighborhoodLocs(main_loc, main_loc.GetStop(eExtreme_Positional) + 100000);
        props_map.Add(eSO_2KB_upstream_variant, *p.first);    
        props_map.Add(eSO_500B_downstream_variant, *p.second);    
    }}

    {{
        TLocsPair p = CVariantPropertiesIndex::s_GetIntronsAndSpliceSiteLocs(main_loc);
        props_map.Add(eSO_intron_variant, *p.first);
        size_t i(0);
        for(CSeq_loc_CI ci(*p.second, CSeq_loc_CI::eEmpty_Skip, CSeq_loc_CI::eOrder_Biological); ci; ++ci) {
            props_map.Add((i%2 ? eSO_splice_acceptor_variant : eSO_splice_donor_variant), *ci.GetRangeAsSeq_loc());
            i++;
        }
    }}

    if(!cds_loc) {
        props_map.Add(eSO_nc_transcript_variant, *rna_loc);
    } else {
        props_map.Add(eSO_coding_sequence_variant, *cds_loc);

        {{
            TLocsPair p = CVariantPropertiesIndex::s_GetStartAndStopCodonsLocs(*cds_loc);
            props_map.Add(eSO_initiator_codon_variant, *p.first);
            props_map.Add(eSO_terminator_codon_variant, *p.second);
        }}

        if(rna_loc) {
            TLocsPair p = CVariantPropertiesIndex::s_GetUTRLocs(*cds_loc, *rna_loc);
            props_map.Add(eSO_5_prime_UTR_variant, *p.first);
            props_map.Add(eSO_3_prime_UTR_variant, *p.second);
        }
    }    

    set<CVariationUtil::ESOTerm> terms_set;
    for(CSeq_loc_CI ci(query_loc, CSeq_loc_CI::eEmpty_Skip); ci; ++ci) {
        const SPropsMap::TRangeMap& rm = props_map.loc_map[ci.GetSeq_id_Handle()];
        for(SPropsMap::TRangeMap::const_iterator it2 = rm.begin(ci.GetRange()); it2.Valid(); ++it2) {
            terms_set.insert(it2->second);
        }
    }
    copy(terms_set.begin(), terms_set.end(), back_inserter(terms));
}





//transcript length less polyA
TSeqPos CVariationUtil::GetEffectiveTranscriptLength(const CBioseq_Handle& bsh)
{
    CVariantPlacement::TMol mol = GetMolType(*bsh.GetSeqId());
    if(mol != CVariantPlacement::eMol_rna && mol != CVariantPlacement::eMol_cdna) {
        return bsh.GetInst_Length();
    }

    SAnnotSelector sel;
    sel.IncludeFeatSubtype(CSeqFeatData::eSubtype_exon);
    sel.IncludeFeatSubtype(CSeqFeatData::eSubtype_polyA_site);

    TSeqPos last_exon_pos(0);
    TSeqPos last_polyA_pos(0);
    for(CFeat_CI ci(bsh, sel); ci; ++ci) {
        const CMappedFeat& mf = *ci;
        if(mf.GetData().GetSubtype() == CSeqFeatData::eSubtype_exon) {
            last_exon_pos = max(last_exon_pos, sequence::GetStop(mf.GetLocation(), NULL));
        } else {
            last_polyA_pos = max(last_polyA_pos, sequence::GetStop(mf.GetLocation(), NULL));
        }
    }
    
    return last_exon_pos ? last_exon_pos + 1
         : last_polyA_pos ? last_polyA_pos + 1
         : bsh.GetInst_Length();
}


void CVariationUtil::x_SetVariantPropertiesForIntronic(CVariantPlacement& p, int offset, const CSeq_loc& loc, CBioseq_Handle& bsh)
{
    if(!p.IsSetGene_location()) {
        //need to zero-out the bitmask, otherwise in debug mode it will be preset to a magic value,
        //and then modifying it with "|=" will produce garbage.
        p.SetGene_location(0);
    }

    if(sequence::GetStrand(loc, NULL) == eNa_strand_minus) {
        offset *= -1;
    }

    if(loc.GetStop(eExtreme_Positional) + 1 >= GetEffectiveTranscriptLength(bsh) && offset > 0) {
        //at the 3'-end; check if near-gene or intergenic
        if(offset <= 500) {
            p.SetGene_location() |= CVariantProperties::eGene_location_near_gene_3;
        } else {
            p.SetGene_location() |= CVariantProperties::eGene_location_intergenic;
        }
    } else if(loc.GetStart(eExtreme_Positional) == 0 && offset < 0) {
        //at the 5'-end; check if near-gene or intergenic
        if(offset >= -2000) {
            p.SetGene_location() |= CVariantProperties::eGene_location_near_gene_5;
        } else {
            p.SetGene_location() |= CVariantProperties::eGene_location_intergenic;
        }
    } else {
        //intronic or splice
        if(offset < 0 && offset >= -2) {
            p.SetGene_location() |= CVariantProperties::eGene_location_acceptor;
        } else if(offset > 0 && offset <= 2) {
            p.SetGene_location() |= CVariantProperties::eGene_location_donor;
        } else {
            p.SetGene_location() |= CVariantProperties::eGene_location_intron;
        }
    }

    if(p.GetGene_location() == 0) {
        p.ResetGene_location();
    }
}


void CVariationUtil::SetVariantProperties(CVariation& v)
{
    v.Index();

    for(CTypeIterator<CVariation> it(Begin(v)); it; ++it) {
        CVariation& v2 = *it;
        if(!v2.IsSetPlacements()) {
            continue;
        }

        //will collapse placement-specific gene-location properties onto variation properties
        if(!v2.SetVariant_prop().IsSetGene_location()) {
            v2.SetVariant_prop().SetGene_location(0); //clear out the magic-value
        }

        NON_CONST_ITERATE(CVariation::TPlacements, it2, v2.SetPlacements()) {
            CVariantPlacement& p = **it2;
            SetPlacementProperties(p);
            
            if(v2.GetConsequenceParent()) {
                //If this variation is a consequence of a parent variation, we are only interested 
                //in the product-specific properties (as the consequence variation will have nucleotide-level placement
                //but we're not interested in recomputing multiple-genes-specific properties here.
                break;
            }
            
            v2.SetVariant_prop().SetGene_location() |= p.GetGene_location();
        }
    }
}


void CVariationUtil::CVariantPropertiesIndex::GetLocationProperties(
        const CSeq_loc& loc,
        CVariantPropertiesIndex::TGeneIDAndPropVector& v)
{
    typedef map<int, CVariantProperties::TGene_location> TMap;
    TMap m; //will collapse properties per gene-id (TGene_location is a bitmask)

    CRef<CSeq_loc> loc2(new CSeq_loc);
    loc2->Assign(loc);
    ChangeIdsInPlace(*loc2, sequence::eGetId_Canonical, *m_scope);

    for(CSeq_loc_CI ci(*loc2); ci; ++ci) {
        CSeq_id_Handle idh = ci.GetSeq_id_Handle();

        if(m_loc2prop.find(idh) == m_loc2prop.end()) {
            //first time seeing this seq-id - index it
            try {
                x_Index(idh);
            } catch (CException& e) {
                NCBI_RETHROW_SAME(e, "Can't Index " + idh.AsString());
            }
        }

        TIdRangeMap::const_iterator it = m_loc2prop.find(idh);
        if(it == m_loc2prop.end()) {
            return;
        }
        const TRangeMap& rm = it->second;

        for(TRangeMap::const_iterator it2 = rm.begin(ci.GetRange()); it2.Valid(); ++it2) {
            ITERATE(TGeneIDAndPropVector, it3, it2->second) {
                TGeneIDAndProp gene_id_and_prop = *it3;
                int gene_id = gene_id_and_prop.first;
                CVariantProperties::TGene_location properties = gene_id_and_prop.second;

                if(m.find(gene_id) == m.end()) {
                    m[gene_id] = 0; //need to zero-out the bitmask in debug mode
                }
                m[gene_id] |= properties;
            }
        }
    }

    ITERATE(TMap, it, m) {
        v.push_back(TGeneIDAndProp(it->first, it->second));
    }
}

void CVariationUtil::CVariantPropertiesIndex::x_Add(const CSeq_loc& loc, int gene_id, CVariantProperties::TGene_location prop)
{
    for(CSeq_loc_CI ci(loc); ci; ++ci) {
        try {
            m_loc2prop[ci.GetSeq_id_Handle()][ci.GetRange()].push_back(TGeneIDAndProp(gene_id, prop));
        } catch (CException& e) {
            NcbiCerr << MSerial_AsnText << loc << gene_id << " " <<  prop;
            NCBI_RETHROW_SAME(e, "Can't index location");
        }
    }
}

///Note: this is strand-agnostic
class SFastLocSubtract
{
public:
    SFastLocSubtract(const CSeq_loc& loc)
    {
        for(CSeq_loc_CI ci(loc); ci; ++ci) {
            m_rangemap[ci.GetSeq_id_Handle()][ci.GetRange()] = ci.GetRangeAsSeq_loc();
        }
    }

    void operator()(CSeq_loc& container_loc) const
    {
        for(CTypeIterator<CSeq_loc> it(Begin(container_loc)); it; ++it) {
            CSeq_loc& loc = *it;
            if(!loc.IsInt() && !loc.IsPnt()) {
                continue;
            }
            CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*loc.GetId());
            TIdRangeMap::const_iterator it2 = m_rangemap.find(idh);
            if(it2 == m_rangemap.end()) {
                continue;
            }
            const TRangeMap& rm = it2->second;

            for(TRangeMap::const_iterator it3 = rm.begin(loc.GetTotalRange()); it3.Valid(); ++it3) {
                CConstRef<CSeq_loc> loc2 = it3->second;
                CRef<CSeq_loc> loc3 = sequence::Seq_loc_Subtract(loc, *loc2, 0, NULL);
                loc.Assign(*loc3);
            }
        }
    }
private:
    typedef CRangeMap<CConstRef<CSeq_loc>, TSeqPos> TRangeMap;
    typedef map<CSeq_id_Handle, TRangeMap> TIdRangeMap;
    TIdRangeMap m_rangemap;
};


bool IsRefSeqGene(const CBioseq_Handle& bsh)
{
    if(!bsh.CanGetDescr()) {
        return false;
    }   
    ITERATE(CSeq_descr::Tdata, it, bsh.GetDescr().Get()) {
        const CSeqdesc& desc = **it;
        if(desc.IsGenbank()) {
            const CGB_block::TKeywords& k = desc.GetGenbank().GetKeywords();
            if(std::find(k.begin(), k.end(), "RefSeqGene") != k.end()) {
                return true;
            }   
        }   
    }   
    return false;
}

//VAR-239
//If RefSeqGene NG, Return GeneIDs for loci having alignments to transcripts.
//Otherwise, return empty set
set<int> GetFocusLocusIDs(const CBioseq_Handle& bsh)
{
    set<int> gene_ids;
    if(!IsRefSeqGene(bsh)) {
        return gene_ids;
    }

    set<CSeq_id_Handle> transcript_seq_ids;
    for(CAlign_CI ci(bsh); ci; ++ci) {
        const CSeq_align& aln = *ci;
        if(aln.GetSegs().IsSpliced()) {
            transcript_seq_ids.insert(CSeq_id_Handle::GetHandle(aln.GetSeq_id(0)));
        }
    }

    SAnnotSelector sel;
    sel.IncludeFeatType(CSeqFeatData::e_Gene);
    sel.IncludeFeatType(CSeqFeatData::e_Rna);
    sel.IncludeFeatType(CSeqFeatData::e_Cdregion);
    for(CFeat_CI ci(bsh, sel); ci; ++ci) {
        const CMappedFeat& mf = *ci;
        if(!mf.IsSetProduct() || !mf.IsSetDbxref()) {
            continue;
        }
        CSeq_id_Handle product_id = CSeq_id_Handle::GetHandle(sequence::GetId(mf.GetProduct(), NULL));
        if(transcript_seq_ids.find(product_id) == transcript_seq_ids.end()) {
            continue;
        }

        //note: using temporary reference "dbxrefs" is necessary because
        //"ITERATE(CSeq_feat::TDbxref, it, mf.GetDbxref())", assert-fails in stl-safe-iter mode
        const CSeq_feat::TDbxref& dbxrefs = mf.GetDbxref(); 
        ITERATE(CSeq_feat::TDbxref, it, dbxrefs) {
            const CDbtag& dbtag = **it;
            if(dbtag.GetDb() == "GeneID" || dbtag.GetDb() == "LocusID") {
                gene_ids.insert(dbtag.GetTag().GetId());
            }   
        } 
    }
    return gene_ids; 
}


// There's no GeneID on a protein bioseq. 
// Instead, need to find the corresponding cdregion (which may or may not have GeneID dbxref), and get GeneID based on parent gene feature.
int CVariationUtil::CVariantPropertiesIndex::s_GetGeneIdForProduct(CBioseq_Handle orig_bsh)
{
    CBioseq_Handle bsh = orig_bsh.GetInst_Mol() == CSeq_inst::eMol_aa ?
            sequence::GetNucleotideParent(orig_bsh) : orig_bsh;

    SAnnotSelector sel;
    sel.SetResolveTSE();
    sel.SetAdaptiveDepth();
    sel.IncludeFeatType(CSeqFeatData::e_Gene);
    sel.IncludeFeatType(CSeqFeatData::e_Rna);
    sel.IncludeFeatType(CSeqFeatData::e_Cdregion);
    CFeat_CI ci(bsh, sel);
    feature::CFeatTree ft(ci);
 
    for(ci.Rewind(); ci; ++ci) {
        const CMappedFeat& mf = *ci;
        if(   mf.IsSetProduct() 
           && mf.GetProduct().GetId()
           && sequence::IsSameBioseq(*orig_bsh.GetSeqId(), *mf.GetProduct().GetId(), &orig_bsh.GetScope()))
        {
            return s_GetGeneID(mf, ft);
        }
    }
    return 0;
}


void CVariationUtil::CVariantPropertiesIndex::x_Index(const CSeq_id_Handle& idh)
{
    SAnnotSelector sel;
    sel.SetResolveAll(); //needs to work on NCs
    sel.SetAdaptiveDepth();
    sel.IncludeFeatType(CSeqFeatData::e_Gene);
    sel.IncludeFeatType(CSeqFeatData::e_Rna);
    sel.IncludeFeatType(CSeqFeatData::e_Cdregion);

    CBioseq_Handle bsh = m_scope->GetBioseqHandle(idh);
    CFeat_CI ci(bsh, sel);
    feature::CFeatTree ft(ci);

    m_loc2prop[idh].size(); //in case there's no annotation, simply add the entry to the map
                            //se we know it has been indexed.

    set<int> focus_loci = GetFocusLocusIDs(bsh);

    //Note: A location can have in-neighborhood == true only if it is NOT in
    //range of another locus. However, if we're dealing with in-focus/out-of-focus
    //genes on NG, then for the purpose of calculating in-neighborhood flags we need to
    //put the non-focus genes out-of-scope. On the other hand, when calculating is-intergenic
    //flag, we have to assume that the non-focus genes are in scope (VAR-239). 
    //That's why we need to collect focus_gene_ranges and non_focus_gene_ranges separately.
    //(See the code below that sets CVariantProperties::eGene_location_intergenic)

    CRef<CSeq_loc> focus_gene_ranges(new CSeq_loc(CSeq_loc::e_Mix));
    CRef<CSeq_loc> non_focus_gene_ranges(new CSeq_loc(CSeq_loc::e_Mix));
    for(ci.Rewind(); ci; ++ci) {
        const CMappedFeat& mf = *ci;
        if(mf.GetData().IsGene()) {

            int gene_id = s_GetGeneID(mf, ft);
            bool is_focus_locus = focus_loci.empty() || focus_loci.find(gene_id) != focus_loci.end();

            (is_focus_locus ? focus_gene_ranges : non_focus_gene_ranges)->SetMix().Set().push_back(
                    sequence::Seq_loc_Merge(mf.GetLocation(), CSeq_loc::fMerge_SingleRange, NULL));
        }
    }
    focus_gene_ranges->ResetStrand();
    non_focus_gene_ranges->ResetStrand();
    SFastLocSubtract subtract_gene_ranges_from(*focus_gene_ranges);

    CRef<CSeq_loc> all_gene_neighborhoods(new CSeq_loc(CSeq_loc::e_Mix));

    bool found_some_gene_ids = false;

    for(ci.Rewind(); ci; ++ci) {
        const CMappedFeat& mf = *ci;
        CMappedFeat parent_mf = ft.GetParent(mf);

        if(!mf.GetLocation().GetId()) {
            continue;
        }

        int gene_id = s_GetGeneID(mf, ft);
        if(!focus_loci.empty() && focus_loci.find(gene_id) == focus_loci.end()) {
            continue; //VAR-239
        }

        if(!parent_mf && gene_id) {
            //Some locations currently may not be covered by any variant-properties values (e.g. in-cds)
            //and yet we need to index gene_ids at those locations. Since variant-properties is a bitmask,
            //we can simply use the value 0 for that.
            //Only need to do that for parent locs.
            x_Add(mf.GetLocation(), gene_id, 0);
            found_some_gene_ids = true;
        }

        //compute neighbborhood locations.
        //Normally for gene features, or rna features lacking a parent gene feature.
        //Applicable for dna context only.
        if((mf.GetData().IsGene() || (!parent_mf && mf.GetData().IsRna()))
            && bsh.GetBioseqMolType() == CSeq_inst::eMol_dna
        ) {
            TLocsPair p = s_GetNeighborhoodLocs(mf.GetLocation(), bsh.GetInst_Length() - 1);

            //exclude any gene overlap from neighborhood (i.e. we don't care if location is in
            //neighborhood of gene-A if it is within gene-B.

            //First, reset strands (as we did with gene_ranges), as we need to do this in strand-agnostic
            //manner. Note: a reasonable thing to do would be to set all-gene-ranges strand to "both", with an
            //expectation that subtracting such a loc from either plus or minus loc would work, but unfortunately
            //it doesn't. Resetting strand, however, is fine, because our indexing is strand-agnostic.
            p.first->ResetStrand();
            p.second->ResetStrand();

#if 0
  //Note: disabling subtraction of gene ranges because want to report neargene-ness regardless of other loci SNP-5000
#if
            //all_gene_ranges is a big complex loc, and subtracting with Seq_loc_Subtract
            //for every neighborhood is slow (takes almost 10 seconds for NC_000001),
            //so we'll use fast map-based subtractor instead
            p.first = sequence::Seq_loc_Subtract(*p.first, *all_gene_ranges, CSeq_loc::fSortAndMerge_All, NULL);
            p.second = sequence::Seq_loc_Subtract(*p.second, *all_gene_ranges, CSeq_loc::fSortAndMerge_All, NULL);
#else
            subtract_gene_ranges_from(*p.first);
            subtract_gene_ranges_from(*p.second);
#endif
#endif
            x_Add(*p.first, gene_id, CVariantProperties::eGene_location_near_gene_5);
            x_Add(*p.second, gene_id, CVariantProperties::eGene_location_near_gene_3);

            all_gene_neighborhoods->SetMix().Set().push_back(p.first);
            all_gene_neighborhoods->SetMix().Set().push_back(p.second);
        }

        if(mf.GetData().IsCdregion()) {
            TLocsPair p = s_GetStartAndStopCodonsLocs(mf.GetLocation());
            x_Add(*p.first, gene_id, CVariantProperties::eGene_location_in_start_codon);
            x_Add(*p.second, gene_id, CVariantProperties::eGene_location_in_stop_codon);

            CRef<CSeq_loc> rna_loc(new CSeq_loc(CSeq_loc::e_Null));
            {{
                if(parent_mf) {
                    rna_loc->Assign(parent_mf.GetLocation());
                } else if(bsh.GetBioseqMolType() == CSeq_inst::eMol_rna) {
                    //refseqs have gene feature annotated on rnas spanning the whole sequence,
                    //but in general there may be free-floating CDS on an rna, in which case
                    //parent loc is the whole seq.
                    rna_loc = bsh.GetRangeSeq_loc(0, bsh.GetInst_Length() - 1, mf.GetLocation().GetStrand());
                }
            }}

            p = s_GetUTRLocs(mf.GetLocation(), *rna_loc);
            x_Add(*p.first, gene_id, CVariantProperties::eGene_location_utr_5);
            x_Add(*p.second, gene_id, CVariantProperties::eGene_location_utr_3);
        } else if(mf.GetData().IsRna()) {
            if(mf.GetData().GetSubtype() != CSeqFeatData::eSubtype_mRNA) {
                x_Add(mf.GetLocation(), gene_id, CVariantProperties::eGene_location_conserved_noncoding);
            }
            TLocsPair p = s_GetIntronsAndSpliceSiteLocs(mf.GetLocation());
            x_Add(*p.first, gene_id, CVariantProperties::eGene_location_intron);
            size_t i(0);
            for(CSeq_loc_CI ci(*p.second, CSeq_loc_CI::eEmpty_Skip, CSeq_loc_CI::eOrder_Biological); ci; ++ci) {
                x_Add(*ci.GetRangeAsSeq_loc(),
                      gene_id,
                      i%2 ? CVariantProperties::eGene_location_acceptor : CVariantProperties::eGene_location_donor);
                ++i;
            }
        } else if(mf.GetData().IsGene()) {
            if(ft.GetChildren(mf).size() == 0) {
                x_Add(mf.GetLocation(),
                      gene_id,
                      bsh.GetBioseqMolType() == CSeq_inst::eMol_rna ?
                              CVariantProperties::eGene_location_conserved_noncoding
                            : CVariantProperties::eGene_location_in_gene);
            }
        }
    }

    if(bsh.GetBioseqMolType() == CSeq_inst::eMol_dna) {
        CRef<CSeq_loc> range_loc = bsh.GetRangeSeq_loc(0, bsh.GetInst_Length() - 1, eNa_strand_plus);

        CRef<CSeq_loc> genes_and_neighborhoods_loc = sequence::Seq_loc_Add(*all_gene_neighborhoods, *focus_gene_ranges, 0, NULL);
        genes_and_neighborhoods_loc = sequence::Seq_loc_Add(*genes_and_neighborhoods_loc, *non_focus_gene_ranges, 0, NULL);

        genes_and_neighborhoods_loc->ResetStrand();
        CRef<CSeq_loc> intergenic_loc = sequence::Seq_loc_Subtract(
                *range_loc,
                *genes_and_neighborhoods_loc,
                CSeq_loc::fSortAndMerge_All,
                NULL);
        x_Add(*intergenic_loc, 0, CVariantProperties::eGene_location_intergenic);
    }

    if(bsh.GetBioseqMolType() == CSeq_inst::eMol_aa && !found_some_gene_ids) {
        //JIRA:SNP-5390
        //in it's normal configuration the iterator won't find a feature with GeneID on a protein, and so
        //it would not be reported in the protein placement. If a CDS is annotated directly on a DNA molecule,
        //there would be no product-specific GeneID at all (
        CRef<CSeq_loc> whole_range_loc = bsh.GetRangeSeq_loc(0, bsh.GetInst_Length() - 1, eNa_strand_plus);
        int gene_id = s_GetGeneIdForProduct(bsh);
        if(gene_id) {
            x_Add(*whole_range_loc, gene_id, 0);
        }
    }
}

int CVariationUtil::CVariantPropertiesIndex::s_GetGeneID(const CMappedFeat& mf, feature::CFeatTree& ft)
{
    int gene_id = 0;
    if(mf.IsSetDbxref()) {

        //note: using temporary reference "dbxrefs" is necessary because
        //"ITERATE(CSeq_feat::TDbxref, it, mf.GetDbxref())", assert-fails in stl-safe-iter mode
        const CSeq_feat::TDbxref& dbxrefs = mf.GetDbxref(); 
        ITERATE(CSeq_feat::TDbxref, it, dbxrefs) {
            const CDbtag& dbtag = **it;
            if(dbtag.GetDb() == "GeneID" && dbtag.GetTag().IsId()) {
                gene_id = dbtag.GetTag().GetId();
            }
        }
    }
    if(gene_id == 0) {
        CMappedFeat parent = ft.GetParent(mf);
        return parent ? s_GetGeneID(parent, ft) : gene_id;
    } else {
        return gene_id;
    }
}


CVariationUtil::CVariantPropertiesIndex::TLocsPair
CVariationUtil::CVariantPropertiesIndex::s_GetStartAndStopCodonsLocs(const CSeq_loc& cds_loc)
{
    TLocsPair p;
    p.first = sequence::Seq_loc_Merge(cds_loc, CSeq_loc::fMerge_SingleRange, NULL);
    p.second.Reset(new CSeq_loc);
    p.second->Assign(*p.first);
    p.first->SetInt().SetTo(p.first->GetInt().GetFrom() + 2);
    p.second->SetInt().SetFrom(p.second->GetInt().GetTo() - 2);

    if(IsReverse(cds_loc.GetStrand())) {
        swap(p.first, p.second);
    }

    if(cds_loc.IsPartialStart(eExtreme_Biological) || cds_loc.IsTruncatedStart(eExtreme_Biological)) {
        p.first->SetNull();
    }
    if(cds_loc.IsPartialStop(eExtreme_Biological) || cds_loc.IsTruncatedStop(eExtreme_Biological)) {
        p.second->SetNull();
    }

    return p;
}

CVariationUtil::CVariantPropertiesIndex::TLocsPair
CVariationUtil::CVariantPropertiesIndex::s_GetUTRLocs(const CSeq_loc& cds_loc, const CSeq_loc& parent_loc)
{
    TLocsPair p;
    CRef<CSeq_loc> sub_loc1 = sequence::Seq_loc_Merge(cds_loc, CSeq_loc::fMerge_SingleRange, NULL);
    CRef<CSeq_loc> sub_loc2(new CSeq_loc);
    sub_loc2->Assign(*sub_loc1);

    sub_loc1->SetInt().SetTo(parent_loc.GetTotalRange().GetTo());
    sub_loc2->SetInt().SetFrom(parent_loc.GetTotalRange().GetFrom());

    p.first = sequence::Seq_loc_Subtract(parent_loc, *sub_loc1, CSeq_loc::fSortAndMerge_All, NULL);
    p.second = sequence::Seq_loc_Subtract(parent_loc, *sub_loc2, CSeq_loc::fSortAndMerge_All, NULL);

    if(IsReverse(cds_loc.GetStrand())) {
        swap(p.first, p.second);
    }
    return p;
}

CVariationUtil::CVariantPropertiesIndex::TLocsPair
CVariationUtil::CVariantPropertiesIndex::s_GetNeighborhoodLocs(const CSeq_loc& gene_loc, TSeqPos max_pos)
{
    TSeqPos flank1_len(2000), flank2_len(500);
    if(IsReverse(gene_loc.GetStrand())) {
        swap(flank1_len, flank2_len);
    }

    TLocsPair p;
    p.first = sequence::Seq_loc_Merge(gene_loc, CSeq_loc::fMerge_SingleRange, NULL);
    p.second.Reset(new CSeq_loc);
    p.second->Assign(*p.first);

    if(p.first->GetTotalRange().GetFrom() == 0) {
        p.first->SetNull();
    } else {
        p.first->SetInt().SetTo(p.first->GetTotalRange().GetFrom() - 1);
        p.first->SetInt().SetFrom(p.first->GetTotalRange().GetFrom() < flank1_len ? 0 : p.first->GetTotalRange().GetFrom() - flank1_len); 
    }

    if(p.second->GetTotalRange().GetTo() == max_pos) {
        p.second->SetNull();
    } else {
        p.second->SetInt().SetFrom(p.second->GetTotalRange().GetTo() + 1);
        p.second->SetInt().SetTo(p.second->GetTotalRange().GetTo() > max_pos ? max_pos : p.second->GetTotalRange().GetTo() + flank2_len);
    }

    if(IsReverse(gene_loc.GetStrand())) {
        swap(p.first, p.second);
    }
    return p;
}


CVariationUtil::CVariantPropertiesIndex::TLocsPair
CVariationUtil::CVariantPropertiesIndex::s_GetIntronsAndSpliceSiteLocs(const CSeq_loc& rna_loc)
{
    CRef<CSeq_loc> range_loc = sequence::Seq_loc_Merge(rna_loc, CSeq_loc::fMerge_SingleRange, NULL);

    CRef<CSeq_loc> introns_loc_with_splice_sites = sequence::Seq_loc_Subtract(*range_loc, rna_loc, 0, NULL);
    CRef<CSeq_loc> introns_loc_without_splice_sites(new CSeq_loc);
    introns_loc_without_splice_sites->Assign(*introns_loc_with_splice_sites);

    //truncate each terminal by two bases from each end to exclude splice sites.
    for(CTypeIterator<CSeq_interval> it(Begin(*introns_loc_without_splice_sites)); it; ++it) {
        CSeq_interval& seqint = *it;
        if(seqint.GetLength() >= 5) {
            seqint.SetFrom() += 2;
            seqint.SetTo() -= 2;
        }
    }

    TLocsPair p;
    p.first = introns_loc_without_splice_sites;
    p.second = sequence::Seq_loc_Subtract(*introns_loc_with_splice_sites,
                                          *introns_loc_without_splice_sites,
                                          CSeq_loc::fSortAndMerge_All,
                                          NULL);
    return p;
}






void CVariationUtil::CCdregionIndex::x_Index(const CSeq_id_Handle& idh)
{
    SAnnotSelector sel;
    sel.SetResolveAll(); //needs to work on NCs
    sel.SetAdaptiveDepth();
    sel.IncludeFeatType(CSeqFeatData::e_Cdregion);
    sel.IncludeFeatType(CSeqFeatData::e_Rna);
    CBioseq_Handle bsh = m_scope->GetBioseqHandle(idh);

    m_data[idh].size(); //this will create m_data[idh] entry so that next time we don't try to reindex it if there are no cdregions
    m_seq_data_map[idh].mapper.Reset();

    CRef<CSeq_loc> all_rna_loc(new CSeq_loc(CSeq_loc::e_Null));

    for(CFeat_CI ci(bsh, sel); ci; ++ci) {
        const CMappedFeat& mf = *ci;
        if(!mf.IsSetProduct() || !mf.GetProduct().GetId()) {
            continue; //could be segment
        }

        if(mf.GetData().IsRna()) {
            all_rna_loc->Add(mf.GetLocation());
        } else {
            all_rna_loc->Add(mf.GetLocation()); //if on mRNA seq, there's no annotated mRNA feature - will just index cds

            SCdregion s;
            s.cdregion_feat.Reset(mf.GetSeq_feat());
            for(CSeq_loc_CI ci(mf.GetLocation()); ci; ++ci) {
                m_data[ci.GetSeq_id_Handle()][ci.GetRange()].push_back(s);
            }

#if 0
            //could possibly cache product sequence as well
            if(m_options && CVariationUtil::fOpt_cache_exon_sequence) {
                x_CacheSeqData(mf.GetProduct(), CSeq_id_Handle::GetHandle(*mf.GetProduct().GetId()));
            }
#endif
        }
    }

    all_rna_loc = sequence::Seq_loc_Merge(*all_rna_loc, CSeq_loc::fSortAndMerge_All, NULL);

    if(m_options && CVariationUtil::fOpt_cache_exon_sequence) {
        x_CacheSeqData(*all_rna_loc, idh);
    }
}

void CVariationUtil::CCdregionIndex::x_CacheSeqData(const CSeq_loc& loc, const CSeq_id_Handle& idh)
{
    CSeq_id_Handle idh2 = sequence::GetId(idh, *m_scope, sequence::eGetId_Canonical);
    SSeqData& d = m_seq_data_map[idh2];

    if(loc.IsNull() || loc.IsEmpty()) {
        return;
    }

    CRef<CSeq_loc> loc2(new CSeq_loc);
    loc2->Assign(loc);

    CSeqVector seqv;
    if(loc.IsWhole()) {
        CBioseq_Handle bsh = m_scope->GetBioseqHandle(idh);
        loc2 = bsh.GetRangeSeq_loc(0, bsh.GetInst_Length() - 1, eNa_strand_plus);
        seqv = bsh.GetSeqVector(CBioseq_Handle::eCoding_Iupac);
    } else {
        seqv = CSeqVector(*loc2, m_scope, CBioseq_Handle::eCoding_Iupac);
    }

    seqv.GetSeqData(seqv.begin(), seqv.end(), d.seq_data);

    if(d.seq_data.size() != sequence::GetLength(*loc2, m_scope) /*could be whole, so need scope*/) {
        //expecting the length of returned sequence to be exactly the same as location;
        //otherwise can't associate sub-location with position in returned sequence
        d.seq_data = "";
        d.mapper.Reset();
        return;
    }

    CRef<CSeq_loc> target_loc(new CSeq_loc);
    target_loc->SetInt().SetId().SetLocal().SetStr("all_cds");
    target_loc->SetInt().SetFrom(0);
    target_loc->SetInt().SetTo(d.seq_data.size() - 1);
    target_loc->SetInt().SetStrand(eNa_strand_plus);

    d.mapper.Reset(new CSeq_loc_Mapper(*loc2, *target_loc, NULL));
}

CRef<CSeq_literal> CVariationUtil::CCdregionIndex::GetCachedLiteralAtLoc(const CSeq_loc& loc)
{
    CRef<CSeq_literal> literal(new CSeq_literal);
    literal->SetSeq_data().SetIupacna().Set("");


    CRef<CSeq_loc> loc2(new CSeq_loc);
    loc2->Assign(loc);
    ChangeIdsInPlace(*loc2, sequence::eGetId_Canonical, *m_scope);

    for(CSeq_loc_CI ci(*loc2); ci; ++ci) {
        if(m_seq_data_map.find(ci.GetSeq_id_Handle()) == m_seq_data_map.end()) {
            return CRef<CSeq_literal>(NULL);
        }
        const SSeqData& d = m_seq_data_map.find(ci.GetSeq_id_Handle())->second;
        if(!d.mapper) {
            return CRef<CSeq_literal>(NULL);
        }

        CRef<CSeq_loc> mapped_loc = d.mapper->Map(*ci.GetRangeAsSeq_loc());

        string s = "";
        mapped_loc->GetLabel(&s);
        if((!mapped_loc->IsInt() && !mapped_loc->IsPnt()) //split mapping
           || mapped_loc->GetTotalRange().GetLength() != ci.GetRange().GetLength()
           || mapped_loc->GetStrand() != eNa_strand_plus) //todo: can accomodate minus by reverse-complementing the chunk
        {
            return CRef<CSeq_literal>(NULL);
        }

        string seq_chunk = d.seq_data.substr(mapped_loc->GetStart(eExtreme_Positional), mapped_loc->GetTotalRange().GetLength());
        literal->SetSeq_data().SetIupacna().Set() += seq_chunk;
    }

    literal->SetLength(literal->GetSeq_data().GetIupacna().Get().size());
    return literal;
}

void CVariationUtil::CCdregionIndex::Get(const CSeq_loc& loc, TCdregions& cdregions)
{
    CRef<CSeq_loc> loc2(new CSeq_loc);
    loc2->Assign(loc);
    ChangeIdsInPlace(*loc2, sequence::eGetId_Canonical, *m_scope);

    for(CSeq_loc_CI ci(*loc2); ci; ++ci) {
        CSeq_id_Handle idh = ci.GetSeq_id_Handle();

        if(m_data.find(idh) == m_data.end()) {
            //first time seeing this seq-id - index it
            x_Index(idh);
        }

        TIdRangeMap::const_iterator it = m_data.find(idh);
        if(it == m_data.end()) {
            return;
        }
        const TRangeMap& rm = it->second;

        set<SCdregion> results;
        for(TRangeMap::const_iterator it2 = rm.begin(ci.GetRange()); it2.Valid(); ++it2) {
            ITERATE(TCdregions, it3, it2->second) {
                results.insert(*it3);
            }
        }
        ITERATE(set<SCdregion>, it, results) {
            cdregions.push_back(*it);
        }
    }
}



//
//
//  Methods to suuport Variation / Variation_ref conversions.
//
//

//Make a copy of the child and inherit relevant (SNP-5716) fields from parent that are not set in the child
CRef<CVariation> InheritParentAttributes(const CVariation& child, const CVariation& parent)
{
    CRef<CVariation> v(SerialClone(child));

    if(!v->IsSetId() && parent.IsSetId()) {
        v->SetId().Assign(parent.GetId());
    }

    if(!v->IsSetParent_id() && parent.IsSetParent_id()) {
        v->SetParent_id().Assign(parent.GetParent_id());
    }

    if(!v->IsSetSample_id() && parent.IsSetSample_id()) {
        ITERATE(CVariation::TSample_id, it, parent.GetSample_id()) {
            v->SetSample_id().push_back(CRef<CObject_id>(SerialClone(**it)));
        }
    }

    if(!v->IsSetOther_ids() && parent.IsSetOther_ids()) {
        ITERATE(CVariation::TOther_ids, it, parent.GetOther_ids()) {
            v->SetOther_ids().push_back(CRef<CDbtag>(SerialClone(**it)));
        }
    }

    return v;
}

void CVariationUtil::AsVariation_feats(const CVariation& v, CSeq_annot::TData::TFtable& out_feats)
{
    CSeq_annot::TData::TFtable feats;
    if(v.IsSetPlacements()) {
        ITERATE(CVariation::TPlacements, it, v.GetPlacements()) {
            const CVariantPlacement& p = **it;
            CRef<CVariation_ref> vr = x_AsVariation_ref(v, p);
            CRef<CSeq_feat> feat(new CSeq_feat);
            feat->SetLocation().Assign(p.GetLoc());
            feat->SetData().SetVariation(*vr);
            feats.push_back(feat);
        }
    } else if(v.GetData().IsSet()) {
        ITERATE(CVariation::TData::TSet::TVariations, it, v.GetData().GetSet().GetVariations()) {
            AsVariation_feats(*InheritParentAttributes(**it, v), feats);
        }
    }

    NON_CONST_ITERATE(CSeq_annot::TData::TFtable, it, feats) {
        CSeq_feat& feat = **it;
        if(v.IsSetPub() && !feat.IsSetCit()) {
            feat.SetCit().Assign(v.GetPub());
        }
        if(v.IsSetExt()) {
            ITERATE(CVariation::TExt, it2, v.GetExt()) {
                CRef<CUser_object> uo(new CUser_object);
                uo->Assign(**it2);
                feat.SetExts().push_back(uo);
            }
        }
    }

    //note: we can't attach user-objects to out_feats directly because it will result in duplicates
    //when out_feats recurses into next subvariation. Hence we are working with our own ftable of
    //feats, and attach the results in the end.
    out_feats.insert(out_feats.end(), feats.begin(), feats.end());
}

CRef<CVariation_ref> CVariationUtil::x_AsVariation_ref(const CVariation& v, const CVariantPlacement& p)
{
    CRef<CVariation_ref> vr(new CVariation_ref);

    if(v.IsSetId()) {
        vr->SetId().Assign(v.GetId());
    }

    if(v.IsSetParent_id()) {
        vr->SetParent_id().Assign(v.GetParent_id());
    }

    if(v.IsSetSample_id() && v.GetSample_id().size() > 0) {
        vr->SetSample_id().Assign(*v.GetSample_id().front());
    }

    if(v.IsSetOther_ids()) {
        ITERATE(CVariation::TOther_ids, it, v.GetOther_ids()) {
            vr->SetOther_ids().push_back(CRef<CDbtag>(SerialClone(**it)));
        }
    }

    if(v.IsSetName()) {
        vr->SetName(v.GetName());
    }

    if(v.IsSetSynonyms()) {
        vr->SetSynonyms() = v.GetSynonyms();
    }

    if(v.IsSetDescription()) {
        vr->SetName(v.GetDescription());
    }

    if(v.IsSetPhenotype()) {
        ITERATE(CVariation::TPhenotype, it, v.GetPhenotype()) {
            CRef<CPhenotype> p(new CPhenotype);
            p->Assign(**it);
            vr->SetPhenotype().push_back(p);
        }
    }

    if(v.IsSetMethod()) {
        vr->SetMethod() = v.GetMethod().GetMethod();
    }

    if(v.IsSetVariant_prop()) {
        vr->SetVariant_prop().Assign(v.GetVariant_prop());
    }

    if(v.GetData().IsComplex()) {
        vr->SetData().SetComplex();
    } else if(v.GetData().IsInstance()) {
        vr->SetData().SetInstance().Assign(v.GetData().GetInstance());
        s_AddInstOffsetsFromPlacementOffsets(vr->SetData().SetInstance(), p);
    } else if(v.GetData().IsNote()) {
        vr->SetData().SetNote() = v.GetData().GetNote();
    } else if(v.GetData().IsUniparental_disomy()) {
        vr->SetData().SetUniparental_disomy();
    } else if(v.GetData().IsUnknown()) {
        vr->SetData().SetUnknown();
    } else if(v.GetData().IsSet()) {
        const CVariation::TData::TSet& v_set = v.GetData().GetSet();
        CVariation_ref::TData::TSet& vr_set = vr->SetData().SetSet();
        vr_set.SetType(v_set.GetType());
        if(v_set.IsSetName()) {
            vr_set.SetName(v_set.GetName());
        }
        ITERATE(CVariation::TData::TSet::TVariations, it, v_set.GetVariations()) {
            vr_set.SetVariations().push_back(x_AsVariation_ref(**it, p));
        }
    } else {
        NCBI_THROW(CException, eUnknown, "Unhandled Variation_ref::TData::E_CChoice");
    }

    if(v.IsSetConsequence()) {
        vr->SetConsequence();
        ITERATE(CVariation::TConsequence, it, v.GetConsequence()) {
            const CVariation::TConsequence::value_type::TObjectType& v_cons = **it;
            CVariation_ref::TConsequence::value_type vr_cons(new CVariation_ref::TConsequence::value_type::TObjectType);
            vr->SetConsequence().push_back(vr_cons);
            vr_cons->SetUnknown();

            if(v_cons.IsSplicing()) {
                vr_cons->SetSplicing();
            } else if(v_cons.IsNote()) {
                vr_cons->SetNote(v_cons.GetNote());
            } else if(v_cons.IsVariation()) {
                CRef<CVariation_ref> cons_variation = x_AsVariation_ref(v_cons.GetVariation(), p);
                vr_cons->SetVariation(*cons_variation);

                if(v_cons.GetVariation().IsSetFrameshift()) {
                    CVariation_ref::TConsequence::value_type fr_cons(new CVariation_ref::TConsequence::value_type::TObjectType);
                    vr->SetConsequence().push_back(fr_cons);
                    fr_cons->SetFrameshift();
                    if(v_cons.GetVariation().GetFrameshift().IsSetPhase()) {
                        fr_cons->SetFrameshift().SetPhase(v_cons.GetVariation().GetFrameshift().GetPhase());
                    }
                    if(v_cons.GetVariation().GetFrameshift().IsSetX_length()) {
                        fr_cons->SetFrameshift().SetX_length(v_cons.GetVariation().GetFrameshift().GetPhase());
                    }
                }
            } else if(v_cons.IsLoss_of_heterozygosity()) {
                vr_cons->SetLoss_of_heterozygosity();
                if(v_cons.GetLoss_of_heterozygosity().IsSetReference()) {
                    vr_cons->SetLoss_of_heterozygosity().SetReference(v_cons.GetLoss_of_heterozygosity().GetReference());
                }
                if(v_cons.GetLoss_of_heterozygosity().IsSetTest()) {
                    vr_cons->SetLoss_of_heterozygosity().SetTest(v_cons.GetLoss_of_heterozygosity().GetTest());
                }
            }
        }
    }

    if(v.IsSetSomatic_origin()) {
        vr->SetSomatic_origin();
        ITERATE(CVariation::TSomatic_origin, it, v.GetSomatic_origin()) {
            const CVariation::TSomatic_origin::value_type::TObjectType& v_so = **it;
            CVariation_ref::TSomatic_origin::value_type vr_so(new CVariation_ref::TSomatic_origin::value_type::TObjectType);

            if(v_so.IsSetSource()) {
                vr_so->SetSource().Assign(v_so.GetSource());
            }

            if(v_so.IsSetCondition()) {
                vr_so->SetCondition();
                if(v_so.GetCondition().IsSetDescription()) {
                    vr_so->SetCondition().SetDescription(v_so.GetCondition().GetDescription());
                }
                if(v_so.GetCondition().IsSetObject_id()) {
                    vr_so->SetCondition().SetObject_id();
                    ITERATE(CVariation::TSomatic_origin::value_type::TObjectType::TCondition::TObject_id,
                            it,
                            v_so.GetCondition().GetObject_id())
                    {
                        CRef<CDbtag> dbtag(new CDbtag);
                        dbtag->Assign(**it);
                        vr_so->SetCondition().SetObject_id().push_back(dbtag);
                    }
                }
            }

            vr->SetSomatic_origin().push_back(vr_so);
        }
    }


    return vr;
}

CRef<CDelta_item> CreateDeltaForOffset(int offset, const CVariantPlacement& p)
{
    CRef<CDelta_item> delta(new CDelta_item);
    delta->SetAction(CDelta_item::eAction_offset);
    delta->SetSeq().SetLiteral().SetLength(abs(offset));
    int sign = (p.GetLoc().GetStrand() == eNa_strand_minus ? -1 : 1) * (offset < 0 ? -1 : 1);
    if(sign < 0) {
        delta->SetMultiplier(-1);
    }
    return delta;
}

void CVariationUtil::s_AddInstOffsetsFromPlacementOffsets(CVariation_inst& vi, const CVariantPlacement& p)
{
    if(p.IsSetStart_offset()) {
        vi.SetDelta().push_front(CreateDeltaForOffset(p.GetStart_offset(), p));
    }
    if(p.IsSetStop_offset()) {
        vi.SetDelta().push_back(CreateDeltaForOffset(p.GetStop_offset(), p));
    }
}


CRef<CVariation> CVariationUtil::AsVariation(const CSeq_feat& variation_feat)
{
    if(!variation_feat.GetData().IsVariation()) {
        NCBI_THROW(CException, eUnknown, "Expected variation-ref feature");
    }

    CRef<CVariation> v = x_AsVariation(variation_feat.GetData().GetVariation());

    CRef<CVariantPlacement> p(new CVariantPlacement);
    p->SetLoc().Assign(variation_feat.GetLocation());
    if(p->GetLoc().GetId()) {
        p->SetMol(GetMolType(sequence::GetId(p->GetLoc(), NULL)));
    } else {
        p->SetMol(CVariantPlacement::eMol_unknown);
    }
    v->SetPlacements().push_back(p);


    if(variation_feat.IsSetCit()) {
       v->SetPub().Assign(variation_feat.GetCit());
    }
    if(variation_feat.IsSetExt()) {
        v->SetExt();
        CRef<CUser_object> uo(new CUser_object);
        uo->Assign(variation_feat.GetExt());
        v->SetExt().push_back(uo);
    }
    if(variation_feat.IsSetExts()) {
        v->SetExt();
        ITERATE(CSeq_feat::TExts, it, variation_feat.GetExts()) {
            CRef<CUser_object> uo(new CUser_object);
            uo->Assign(**it);
            v->SetExt().push_back(uo);
        }
    }

    s_ConvertInstOffsetsToPlacementOffsets(*v, *p);

    return v;
}

CRef<CVariation> CVariationUtil::x_AsVariation(const CVariation_ref& vr)
{
    CRef<CVariation> v(new CVariation);
    if(vr.IsSetId()) {
        v->SetId().Assign(vr.GetId());
    }

    if(vr.IsSetParent_id()) {
        v->SetParent_id().Assign(vr.GetParent_id());
    }

    if(vr.IsSetSample_id()) {
        v->SetSample_id().push_back(CRef<CObject_id>(SerialClone(vr.GetSample_id())));
    }

    if(vr.IsSetOther_ids()) {
        ITERATE(CVariation_ref::TOther_ids, it, vr.GetOther_ids()) {
            v->SetOther_ids().push_back(CRef<CDbtag>(SerialClone(**it)));
        }
    }

    if(vr.IsSetName()) {
        v->SetName(vr.GetName());
    }

    if(vr.IsSetSynonyms()) {
        v->SetSynonyms() = vr.GetSynonyms();
    }

    if(vr.IsSetDescription()) {
        v->SetName(vr.GetDescription());
    }

    if(vr.IsSetPhenotype()) {
        ITERATE(CVariation_ref::TPhenotype, it, vr.GetPhenotype()) {
            CRef<CPhenotype> p(new CPhenotype);
            p->Assign(**it);
            v->SetPhenotype().push_back(p);
        }
    }

    if(vr.IsSetMethod()) {
        v->SetMethod().SetMethod() = vr.GetMethod();
    }

    if(vr.IsSetVariant_prop()) {
        v->SetVariant_prop().Assign(vr.GetVariant_prop());
    }

    if(vr.GetData().IsComplex()) {
        v->SetData().SetComplex();
    } else if(vr.GetData().IsInstance()) {
        v->SetData().SetInstance().Assign(vr.GetData().GetInstance());
    } else if(vr.GetData().IsNote()) {
        v->SetData().SetNote() = vr.GetData().GetNote();
    } else if(vr.GetData().IsUniparental_disomy()) {
        v->SetData().SetUniparental_disomy();
    } else if(vr.GetData().IsUnknown()) {
        v->SetData().SetUnknown();
    } else if(vr.GetData().IsSet()) {
        const CVariation_ref::TData::TSet& vr_set = vr.GetData().GetSet();
        CVariation::TData::TSet& v_set = v->SetData().SetSet();
        v_set.SetType(vr_set.GetType());
        if(vr_set.IsSetName()) {
            v_set.SetName(vr_set.GetName());
        }
        ITERATE(CVariation_ref::TData::TSet::TVariations, it, vr_set.GetVariations()) {
            v_set.SetVariations().push_back(x_AsVariation(**it));
        }
    } else {
        NCBI_THROW(CException, eUnknown, "Unhandled Variation_ref::TData::E_CChoice");
    }

    if(vr.IsSetConsequence()) {
        v->SetConsequence();
        ITERATE(CVariation_ref::TConsequence, it, vr.GetConsequence()) {
            const CVariation_ref::TConsequence::value_type::TObjectType& vr_cons = **it;
            CVariation::TConsequence::value_type v_cons(new CVariation::TConsequence::value_type::TObjectType);

            if(vr_cons.IsUnknown()) {
                v_cons->SetUnknown();
            } else if(vr_cons.IsSplicing()) {
                v_cons->SetSplicing();
            } else if(vr_cons.IsNote()) {
                v_cons->SetNote(vr_cons.GetNote());
            } else if(vr_cons.IsVariation()) {
                CRef<CVariation> cons_variation = x_AsVariation(vr_cons.GetVariation());
                v_cons->SetVariation(*cons_variation);
            } else if(vr_cons.IsFrameshift()) {
                //expecting previous consequnece to be a protein variation.
                if(v->GetConsequence().size() == 0 || !v->GetConsequence().back()->IsVariation()) {
                    NCBI_THROW(CException, eUnknown, "Did not find target variation to attach frameshift");
                }
                CVariation& cons_variation = v->SetConsequence().back()->SetVariation();
                cons_variation.SetFrameshift();
                if(vr_cons.GetFrameshift().IsSetPhase()) {
                    cons_variation.SetFrameshift().SetPhase(vr_cons.GetFrameshift().GetPhase());
                }
                if(vr_cons.GetFrameshift().IsSetX_length()) {
                    cons_variation.SetFrameshift().SetX_length(vr_cons.GetFrameshift().GetPhase());
                }
            } else if(vr_cons.IsLoss_of_heterozygosity()) {
                v_cons->SetLoss_of_heterozygosity();
                if(vr_cons.GetLoss_of_heterozygosity().IsSetReference()) {
                    v_cons->SetLoss_of_heterozygosity().SetReference(vr_cons.GetLoss_of_heterozygosity().GetReference());
                }
                if(vr_cons.GetLoss_of_heterozygosity().IsSetTest()) {
                    v_cons->SetLoss_of_heterozygosity().SetTest(vr_cons.GetLoss_of_heterozygosity().GetTest());
                }
            }

            v->SetConsequence().push_back(v_cons);
        }
    }

    if(vr.IsSetSomatic_origin()) {
        v->SetSomatic_origin();
        ITERATE(CVariation_ref::TSomatic_origin, it, vr.GetSomatic_origin()) {
            const CVariation_ref::TSomatic_origin::value_type::TObjectType& vr_so = **it;
            CVariation::TSomatic_origin::value_type v_so(new CVariation::TSomatic_origin::value_type::TObjectType);

            if(vr_so.IsSetSource()) {
                v_so->SetSource().Assign(vr_so.GetSource());
            }

            if(vr_so.IsSetCondition()) {
                v_so->SetCondition();
                if(vr_so.GetCondition().IsSetDescription()) {
                    v_so->SetCondition().SetDescription(vr_so.GetCondition().GetDescription());
                }
                if(vr_so.GetCondition().IsSetObject_id()) {
                    v_so->SetCondition().SetObject_id();
                    ITERATE(CVariation_ref::TSomatic_origin::value_type::TObjectType::TCondition::TObject_id,
                            it,
                            vr_so.GetCondition().GetObject_id())
                    {
                        CRef<CDbtag> dbtag(new CDbtag);
                        dbtag->Assign(**it);
                        v_so->SetCondition().SetObject_id().push_back(dbtag);
                    }
                }
            }

            v->SetSomatic_origin().push_back(v_so);
        }
    }

    return v;
}


void CVariationUtil::s_ConvertInstOffsetsToPlacementOffsets(CVariation& v, CVariantPlacement& p)
{
    if(v.GetData().IsSet()) {
        NON_CONST_ITERATE(CVariation::TData::TSet::TVariations, it, v.SetData().SetSet().SetVariations()) {
            s_ConvertInstOffsetsToPlacementOffsets(**it, p);
        }
    } else if(v.GetData().IsInstance() && v.GetData().GetInstance().GetDelta().size() > 0) {
        const CDelta_item& delta_first = *v.GetData().GetInstance().GetDelta().front();

        if(p.GetLoc().IsPnt() && delta_first.IsSetAction() && delta_first.GetAction() == CDelta_item::eAction_offset) {
            int offset = delta_first.GetSeq().GetLiteral().GetLength()
                       * (delta_first.IsSetMultiplier() ? delta_first.GetMultiplier() : 1)
                       * (p.GetLoc().GetStrand() == eNa_strand_minus ? -1 : 1);
            if(!p.IsSetStart_offset() || offset == p.GetStart_offset()) {
                p.SetStart_offset(offset);
                v.SetData().SetInstance().SetDelta().pop_front();
            }
        } else {
            //If the location is not a point, then the offset(s) apply to start and/or stop individually
            if(delta_first.IsSetAction() && delta_first.GetAction() == CDelta_item::eAction_offset) {
                CRef<CSeq_loc> range_loc = sequence::Seq_loc_Merge(p.GetLoc(), CSeq_loc::fMerge_SingleRange, NULL);
                int offset = delta_first.GetSeq().GetLiteral().GetLength()
                           * (delta_first.IsSetMultiplier() ? delta_first.GetMultiplier() : 1)
                           * (range_loc->GetStrand() == eNa_strand_minus ? -1 : 1);
                if(!p.IsSetStart_offset() || offset == p.GetStart_offset()) {
                    p.SetStart_offset(offset);
                    v.SetData().SetInstance().SetDelta().pop_front();
                }
            }

            const CDelta_item& delta_last = *v.GetData().GetInstance().GetDelta().back();
            if(delta_last.IsSetAction() && delta_last.GetAction() == CDelta_item::eAction_offset) {
                CRef<CSeq_loc> range_loc = sequence::Seq_loc_Merge(p.GetLoc(), CSeq_loc::fMerge_SingleRange, NULL);
                int offset = delta_last.GetSeq().GetLiteral().GetLength()
                           * (delta_last.IsSetMultiplier() ? delta_last.GetMultiplier() : 1)
                           * (range_loc->GetStrand() == eNa_strand_minus ? -1 : 1);
                if(!p.IsSetStop_offset() || offset == p.GetStop_offset()) {
                    p.SetStop_offset(offset);
                    v.SetData().SetInstance().SetDelta().pop_front();
                }
            }
        }
    }
}



#if 0
CVariationUtil::ETestStatus CVariationUtil::CheckAssertedAllele(
        const CSeq_feat& variation_feat,
        string* asserted_out,
        string* actual_out)
{
    return eNotApplicable;
    if(!variation_feat.GetData().IsVariation()) {
        return eNotApplicable;
    }

    CVariation_ref vr;
    vr.Assign(variation_feat.GetData().GetVariation());
    if(!vr.IsSetLocation()) {
        vr.SetLocation().Assign(variation_feat.GetLocation());
    }
    s_PropagateLocsInPlace(vr);


    bool have_asserted_seq = false;
    bool is_ok = true;
    for(CTypeIterator<CVariation_ref> it1(Begin(vr)); it1; ++it1) {
        const CVariation_ref& vr1 = *it1;
        if(vr1.GetData().IsInstance()
           && vr1.GetData().GetInstance().IsSetObservation()
           && vr1.GetData().GetInstance().GetObservation() == CVariation_inst::eObservation_asserted)
        {
            string asserted_seq;
            const CSeq_literal& literal = vr1.GetData().GetInstance().GetDelta().front()->GetSeq().GetLiteral();
            if(literal.GetSeq_data().IsIupacna()) {
                asserted_seq = literal.GetSeq_data().GetIupacna();
                have_asserted_seq = true;
            } else if(literal.GetSeq_data().IsNcbieaa()) {
                asserted_seq = literal.GetSeq_data().GetNcbieaa();
                have_asserted_seq = true;
            }

            //an asserted sequnece may be of the form "A..BC", where ".." is to be interpreted as a
            //gap of arbitrary length - we need to match prefix and suffix separately
            string prefix, suffix;
            string str_tmp = NStr::Replace(asserted_seq, "..", "\t"); //SplitInTwo's delimiter must be single-character
            NStr::SplitInTwo(str_tmp, "\t", prefix, suffix);

            CSeqVector v(vr1.GetLocation(), *m_scope, CBioseq_Handle::eCoding_Iupac);
            string actual_seq;
            v.GetSeqData(v.begin(), v.end(), actual_seq);

            if(   prefix.size() > 0 && !NStr::StartsWith(actual_seq, prefix)
               || suffix.size() > 0 && !NStr::EndsWith(actual_seq, suffix))
            {
                is_ok = false;
                if(asserted_out) {
                    *asserted_out = asserted_seq;
                }
                if(actual_out) {
                    *actual_out = actual_seq;
                }
                break;
            }
        }
    }

    return !have_asserted_seq ? eNotApplicable : is_ok ? ePass : eFail;
}
#endif


};

END_NCBI_SCOPE

