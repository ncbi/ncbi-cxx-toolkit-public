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
 * File Description:
 *   Sample library
 *
 */

#include <ncbi_pch.hpp>
#include <objects/seqloc/Seq_point.hpp>
#include <objects/general/Object_id.hpp>

#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Spliced_seg.hpp>
#include <objects/seqalign/Spliced_exon.hpp>
#include <objects/seqalign/Product_pos.hpp>
#include <objects/seqalign/Prot_pos.hpp>
#include <objmgr/seq_loc_mapper.hpp>


#include <objects/seq/seqport_util.hpp>

#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/Variation_ref.hpp>
#include <objects/seqfeat/Variation_inst.hpp>
#include <objects/seqfeat/Delta_item.hpp>
#include <objects/seqfeat/Ext_loc.hpp>


#include <objects/seqfeat/SeqFeatXref.hpp>

#include <objects/seq/Seq_literal.hpp>
#include <objects/seq/Seq_data.hpp>
#include <objects/seq/Numbering.hpp>
#include <objects/seq/Num_ref.hpp>
#include <objects/seq/Annot_descr.hpp>
#include <objects/seq/Annotdesc.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seqfeat/BioSource.hpp>

#include <objects/seqloc/Seq_loc_equiv.hpp>

#include <serial/iterator.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/seq_vector.hpp>
#include <objtools/hgvs/variation_util.hpp>


BEGIN_NCBI_SCOPE


void CVariationUtil::s_FactorOutLocsInPlace(CVariation_ref& v)
{
    if(!v.GetData().IsSet()) {
        return;
    }

    //round-1: calculate this loc as union of the members
    CRef<CSeq_loc> aggregate_loc(new CSeq_loc(CSeq_loc::e_Mix));
    NON_CONST_ITERATE(CVariation_ref::TData::TSet::TVariations, it, v.SetData().SetSet().SetVariations()) {
        CVariation_ref& vr = **it;
        s_FactorOutLocsInPlace(vr);
        if(vr.IsSetLocation()) {
            aggregate_loc->Add(vr.GetLocation());
        }
    }
    aggregate_loc = aggregate_loc->Merge(CSeq_loc::fSortAndMerge_All, NULL);
    v.SetLocation(*aggregate_loc);

    //round-2: reset the set-member locatinos if they are the same as this
    NON_CONST_ITERATE(CVariation_ref::TData::TSet::TVariations, it, v.SetData().SetSet().SetVariations()) {
        CVariation_ref& vr = **it;
        if(vr.IsSetLocation() && vr.GetLocation().Equals(v.GetLocation())) {
            vr.ResetLocation();
        }
    }
}

void CVariationUtil::s_PropagateLocsInPlace(CVariation_ref& v)
{
    if(!v.GetData().IsSet()) {
        return;
    }

    NON_CONST_ITERATE(CVariation_ref::TData::TSet::TVariations, it, v.SetData().SetSet().SetVariations()) {
        CVariation_ref& vr = **it;
        if(!vr.IsSetLocation()) {
            vr.SetLocation().Assign(v.GetLocation());
        }
        s_PropagateLocsInPlace(vr);
    }
}



void CVariationUtil::s_ResolveIntronicOffsets(CVariation_ref& v)
{
    if(!v.GetData().IsInstance()) {
        NCBI_THROW(CArgException, CArgException::eInvalidArg, "Expected inst");
    }

    if(!v.IsSetLocation()) {
        NCBI_THROW(CArgException, CArgException::eInvalidArg, "Expected location to be set");
    }

    const CDelta_item& delta_first = *v.GetData().GetInstance().GetDelta().front();
    if(delta_first.IsSetAction() && delta_first.GetAction() == CDelta_item::eAction_offset) {
        CRef<CSeq_loc> range_loc = sequence::Seq_loc_Merge(v.GetLocation(), CSeq_loc::fMerge_SingleRange, NULL);
        TSeqPos& bio_start = range_loc->GetStrand() == eNa_strand_minus ? range_loc->SetInt().SetTo() : range_loc->SetInt().SetFrom();
        int offset = delta_first.GetSeq().GetLiteral().GetLength()
                   * (delta_first.IsSetMultiplier() ? delta_first.GetMultiplier() : 1)
                   * (range_loc->GetStrand() == eNa_strand_minus ? -1 : 1);
        bio_start += offset;
        v.SetLocation().Assign(*range_loc);
        v.SetData().SetInstance().SetDelta().pop_front();
    }

    const CDelta_item& delta_last = *v.GetData().GetInstance().GetDelta().back();
    if(delta_last.IsSetAction() && delta_last.GetAction() == CDelta_item::eAction_offset) {
        CRef<CSeq_loc> range_loc = sequence::Seq_loc_Merge(v.GetLocation(), CSeq_loc::fMerge_SingleRange, NULL);
        TSeqPos& bio_end = range_loc->GetStrand() == eNa_strand_minus ? range_loc->SetInt().SetFrom() : range_loc->SetInt().SetTo();
        int offset = delta_last.GetSeq().GetLiteral().GetLength()
                   * (delta_last.IsSetMultiplier() ? delta_last.GetMultiplier() : 1)
                   * (range_loc->GetStrand() == eNa_strand_minus ? -1 : 1);
        bio_end += offset;
        v.SetLocation().Assign(*range_loc);
        v.SetData().SetInstance().SetDelta().pop_back();
    }
}


void CVariationUtil::s_AddIntronicOffsets(CVariation_ref& v, const CSpliced_seg& ss)
{
    if(!v.GetData().IsInstance()) {
        NCBI_THROW(CArgException, CArgException::eInvalidArg, "Expected inst");
    }

    if(!v.IsSetLocation()) {
        NCBI_THROW(CArgException, CArgException::eInvalidArg, "Expected location to be set");
    }

    if(!v.GetLocation().GetId()
       || !v.GetLocation().GetId()->Equals(ss.GetGenomic_id()))
    {
        NCBI_THROW(CArgException, CArgException::eInvalidArg, "Expected genomic_id in the variation to be the same as in spliced-seg");
    }

    long start = v.GetLocation().GetStart(eExtreme_Positional);
    long stop = v.GetLocation().GetStop(eExtreme_Positional);

    long closest_start = 0; //closest-exon-boundary for bio-start of variation location
    long closest_stop = 0; //closest-exon-boundary for bio-stop of variation location

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

    //adjust location
    if(start != closest_start || stop != closest_stop) {
        CRef<CSeq_loc> loc = sequence::Seq_loc_Merge(v.GetLocation(), CSeq_loc::fMerge_SingleRange, NULL);
        loc->SetInt().SetFrom(closest_start);
        loc->SetInt().SetTo(closest_stop);
        v.SetLocation().Assign(*loc);
    }

    //add offsets
    if(start != closest_start) {
        int offset = start - closest_start;
        CRef<CDelta_item> delta(new CDelta_item);
        delta->SetAction(CDelta_item::eAction_offset);
        delta->SetSeq().SetLiteral().SetLength(abs(offset));

        int sign = (v.GetLocation().GetStrand() == eNa_strand_minus ? -1 : 1) * (offset < 0 ? -1 : 1);
        if(sign < 0) {
            delta->SetMultiplier(-1);
        }
        if(v.GetLocation().GetStrand() == eNa_strand_minus) {
            v.SetData().SetInstance().SetDelta().push_back(delta);
        } else {
            v.SetData().SetInstance().SetDelta().push_front(delta);
        }
    }

    if(stop != closest_stop && start != stop) {
        int offset = stop - closest_stop;
        CRef<CDelta_item> delta(new CDelta_item);
        delta->SetAction(CDelta_item::eAction_offset);
        delta->SetSeq().SetLiteral().SetLength(abs(offset));
        int sign = (v.GetLocation().GetStrand() == eNa_strand_minus ? -1 : 1) * (offset < 0 ? -1 : 1);
        if(sign < 0) {
            delta->SetMultiplier(-1);
        }
        if(v.GetLocation().GetStrand() == eNa_strand_minus) {
            v.SetData().SetInstance().SetDelta().push_front(delta);
        } else {
            v.SetData().SetInstance().SetDelta().push_back(delta);
        }
    }
}


CRef<CSeq_feat> CVariationUtil::Remap(const CSeq_feat& variation_feat, const CSeq_align& aln)
{
    CRef<CSeq_feat> feat(new CSeq_feat);
    feat->Assign(variation_feat);

    if(!feat->GetData().IsVariation()) {
        NCBI_THROW(CArgException, CArgException::eInvalidArg, "feature must be of variation-feat type");
    }

    //temporarily transfer root location to variation-feat, so we can only
    //concentrate on mapping variation-refs recursively. will transfer back when done.
    feat->SetData().SetVariation().SetLocation().Assign(feat->GetLocation());

    CRef<CSeq_loc_mix> locs(new CSeq_loc_mix);
    for(CTypeIterator<CVariation_ref> it1(Begin(feat->SetData().SetVariation())); it1; ++it1) {
        //Step1: collect locs to remap (non-const refs)
        locs->Set().clear();
        CVariation_ref& vr = *it1;
        if(vr.IsSetLocation()) {
            locs->Set().push_back(CRef<CSeq_loc>(&vr.SetLocation()));

            //save the original in ext-locs
            typedef CVariation_ref::TExt_locs::value_type TExtLoc;
            TExtLoc ext_loc(new TExtLoc::TObjectType);
            ext_loc->SetId().SetStr("mapped-from");
            ext_loc->SetLocation().Assign(vr.GetLocation());
            vr.SetExt_locs().push_back(ext_loc);
        }

        /*
         * Not sure whether we want to remap seq-locs in delta-inst.
         * E.g. if the original is an inversion, the delta-loc will reference
         * location on this sequence on the opposite strand. Do we want to
         * have this location remapped or leave it as is? Surely,
         * we don't want to remap just ANY seq-loc here, as it may not
         * necessarily be mappable in this context. Current implementation
         * remaps inst-locs that are on the same sequence that is represented
         * in tha variation-ref (i.e. we would remap inst-loc of an inversion, but not
         * of a transposon or far-loc insertion).
         */
        if(vr.GetData().IsInstance()) {
            NON_CONST_ITERATE(CVariation_inst::TDelta, it2, vr.SetData().SetInstance().SetDelta()) {
                CRef<CDelta_item> delta = *it2;
                if(delta->GetSeq().IsLoc()) {
                    const CSeq_id* delta_seq_id = delta->GetSeq().GetLoc().GetId();

                    bool is_represented_in_variation = false;
                    for(CTypeConstIterator<CSeq_loc> it3(Begin(variation_feat.GetLocation())); it3; ++it3) {
                        //note that we're checking against top-level location of the variation, as
                        //the location within a sub-variation-ref may have been yanked during compactification,
                        //(see rebuild-locs rules)
                        const CSeq_id* variation_seq_id = it3->GetId();
                        if(delta_seq_id != NULL && variation_seq_id != NULL && delta_seq_id->Equals(*variation_seq_id)) {
                            is_represented_in_variation = true;
                            break;
                        }
                    }

                    if(is_represented_in_variation) {
                        locs->Set().push_back(CRef<CSeq_loc>(&delta->SetSeq().SetLoc()));
                    }
                }
            }

            if(vr.GetLocation().GetId()
               && aln.GetSegs().IsSpliced()
               && aln.GetSegs().GetSpliced().GetGenomic_id().Equals(*vr.GetLocation().GetId()))
            {
                s_AddIntronicOffsets(vr, aln.GetSegs().GetSpliced());
            }
        }

        //note: originally I tried using seq-loc type-iterator to process each primitive seq-loc individually, but that
        //does not work right because after remapping a loc the type iterator will try find and try to process the already-mapped
        //primitive-locs.
        NON_CONST_ITERATE(CSeq_loc_mix::Tdata, it2, locs->Set()) {
            CSeq_loc& loc = **it2;
            if(!loc.GetId()) {
                NCBI_THROW(CException, eUnknown, "Encountered location with non-unique id within variation-ref; can't remap");
            }

            CSeq_align::TDim target_row(-1);
            for(int i = 0; i < 2; i++) {
                if(sequence::IsSameBioseq(*loc.GetId(), aln.GetSeq_id(i), m_scope )) {
                    target_row = 1 - i;
                }
            }
            if(target_row == -1) {
                NCBI_THROW(CException, eUnknown, "The alignment has no row for seq-id " + loc.GetId()->AsFastaString());
            }

            try {
                CRef<CSeq_loc_Mapper> mapper(new CSeq_loc_Mapper(
                        aln,
                        target_row,
                        m_scope));
                CRef<CSeq_loc> mapped_loc = mapper->Map(loc);
                CRef<CSeq_loc> merged_mapped_loc = sequence::Seq_loc_Merge(*mapped_loc, CSeq_loc::fSortAndMerge_All, NULL);
                loc.Assign(*merged_mapped_loc);
            } catch (CException& e) {
                string s("");
                loc.GetLabel(&s);
                NCBI_RETHROW_SAME(e, "Could not remap loc " + s + " to row "
                                      + NStr::IntToString(target_row)
                                      + " via alignment of " + aln.GetSeq_id(0).AsFastaString()
                                      + "-" + aln.GetSeq_id(1).AsFastaString());
            }

        }

        if(vr.GetData().IsInstance()
           && vr.GetLocation().GetId()
           && aln.GetSegs().IsSpliced()
           && aln.GetSegs().GetSpliced().GetGenomic_id().Equals(*vr.GetLocation().GetId()))
        {
            s_ResolveIntronicOffsets(vr);
        }
    }

    //transfer the root location of the variation back to feat.location
    feat->SetLocation(feat->SetData().SetVariation().SetLocation());
    feat->SetData().SetVariation().ResetLocation();

    return feat;
}



#if 0
CRef<CSeq_loc> CVariationUtil::x_CreateFlankingLoc(const CSeq_loc& loc, TSeqPos len, bool upstream)
{
    CRef<CSeq_loc> flank(new CSeq_loc);
    flank->SetInt().SetId().Assign(sequence::GetId(loc, NULL));
    flank->SetInt().SetStrand(sequence::GetStrand(loc, NULL));

    long start = sequence::GetStart(loc, NULL, eExtreme_Positional);
    long stop = sequence::GetStop(loc, NULL, eExtreme_Positional);

    CBioseq_Handle bsh = m_scope.GetBioseqHandle(flank->GetInt().GetId());
    long max_pos = bsh.GetInst_Length() - 1;

    flank->SetInt().SetTo(min(max_pos, stop + len));
    flank->SetInt().SetFrom(max((long)0, start - len));

    if(upstream ^ (sequence::GetStrand(loc, NULL) == eNa_strand_minus)) {
        if(start == 0) {
            flank->SetEmpty().Assign(sequence::GetId(loc, NULL));
        } else {
            flank->SetInt().SetTo(start - 1);
        }
    } else {
        if(stop == max_pos) {
            flank->SetEmpty().Assign(sequence::GetId(loc, NULL));
        } else {
            flank->SetInt().SetFrom(stop + 1);
        }
    }

    return flank;
}
#endif



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

    static const string alphabet = "ACGT";
    string codon = "AAA";
    CSeqTranslator translator;
    for(size_t i0 = 0; i0 < 4; i0++) {
        codon[0] = alphabet[i0];
        for(size_t i1 = 0; i1 < 4; i1++) {
            codon[1] = alphabet[i1];
            for(size_t i2 = 0; i2 < 4; i2++) {
                codon[2] = alphabet[i2];
                string prot("");
                translator.Translate(codon, prot, CSeqTranslator::fIs5PrimePartial);
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



//vr must be a prot missense or nonsense (inst) with location set; inst must not have offsets.
CRef<CSeq_feat> CVariationUtil::ProtToPrecursor(const CSeq_feat& prot_variation_feat)
{
    if(!prot_variation_feat.GetData().IsVariation()) {
        NCBI_THROW(CArgException, CArgException::eInvalidArg, "Expected variation-feature");
    }

    const CVariation_ref& vr = prot_variation_feat.GetData().GetVariation();

    if(!vr.GetData().IsInstance()) {
        NCBI_THROW(CArgException, CArgException::eInvalidArg, "Expected variation.inst");
    }

    if(!vr.GetData().GetInstance().GetDelta().size() == 1) {
        NCBI_THROW(CArgException, CArgException::eInvalidArg, "Expected single-element delta");
    }

    const CDelta_item& delta = *vr.GetData().GetInstance().GetDelta().front();
    if(delta.IsSetAction() && delta.GetAction() != CDelta_item::eAction_morph) {
        NCBI_THROW(CArgException, CArgException::eInvalidArg, "Expected morph action");
    }

    if(!delta.IsSetSeq() || !delta.GetSeq().IsLiteral() || delta.GetSeq().GetLiteral().GetLength() != 1) {
        NCBI_THROW(CArgException, CArgException::eInvalidArg, "Expected literal of length 1 in inst.");
    }

    CSeq_data variant_prot_seq;
    CSeqportUtil::Convert(delta.GetSeq().GetLiteral().GetSeq_data(), &variant_prot_seq, CSeq_data::e_Iupacaa);


    if(sequence::GetLength(prot_variation_feat.GetLocation(), NULL) != 1) {
        NCBI_THROW(CArgException, CArgException::eInvalidArg, "Expected single-aa location");
    }

    SAnnotSelector sel(CSeqFeatData::e_Cdregion, true);
       //note: sel by product; location is prot; found feature is mrna having this prot as product
    CRef<CSeq_loc_Mapper> prot2precursor_mapper;
    for(CFeat_CI ci(*m_scope, prot_variation_feat.GetLocation(), sel); ci; ++ci) {
        prot2precursor_mapper.Reset(new CSeq_loc_Mapper(ci->GetMappedFeature(), CSeq_loc_Mapper::eProductToLocation, m_scope));
        break;
    }

    if(!prot2precursor_mapper) {
        NCBI_THROW(CException, eUnknown, "Can't create prot2precursor mapper. Is this a prot?");
    }

    CRef<CSeq_loc> nuc_loc = prot2precursor_mapper->Map(prot_variation_feat.GetLocation());
    if(!nuc_loc->IsInt() || sequence::GetLength(*nuc_loc, NULL) != 3) {
        NCBI_THROW(CException, eUnknown, "AA does not remap to a single codon.");
    }


    CSeqVector v(*nuc_loc, m_scope, CBioseq_Handle::eCoding_Iupac);

    string original_allele_codon; //nucleotide allele on the sequence
    v.GetSeqData(v.begin(), v.end(), original_allele_codon);


    vector<string> variant_codons;
    s_CalcPrecursorVariationCodon(original_allele_codon, variant_prot_seq.GetIupacaa(), variant_codons);

    if(variant_codons.size() != 1) {
        NCBI_THROW(CException, CException::eInvalid, "Can't calculate unique variant codon for this variation: " + original_allele_codon + " -> " + NStr::Join(variant_codons, "|"));
    }

    string variant_codon = variant_codons.front();

    //If the original and variant codons have terminal bases shared, we can truncate the variant codon and location accordingly.
    while(variant_codon.length() > 1 && variant_codon.at(0) == original_allele_codon.at(0)) {
        variant_codon = variant_codon.substr(1);
        original_allele_codon = variant_codon.substr(1);
        if(nuc_loc->GetStrand() == eNa_strand_minus) {
            nuc_loc->SetInt().SetTo()--;
        } else {
            nuc_loc->SetInt().SetFrom()++;
        }
    }
    while(variant_codon.length() > 1 &&
          variant_codon.at(variant_codon.length() - 1) == original_allele_codon.at(original_allele_codon.length() - 1))
    {
        variant_codon.resize(variant_codon.length() - 1);
        original_allele_codon.resize(variant_codon.length() - 1);
        //Note: normally given a protein, the parent will be a mRNA and the CDS location
        //will have plus strand; however, the parent could be MT, so we can't assume plus strand
        if(nuc_loc->GetStrand() == eNa_strand_minus) {
            nuc_loc->SetInt().SetFrom()++;
        } else {
            nuc_loc->SetInt().SetTo()--;
        }
    }

    CRef<CDelta_item> delta2(new CDelta_item);
    delta2->SetSeq().SetLiteral().SetLength(variant_codon.length());
    delta2->SetSeq().SetLiteral().SetSeq_data().SetIupacna().Set(variant_codon);

    CRef<CSeq_feat> feat(new CSeq_feat);

    //merge loc to convert int of length 1 to a pnt as necessary
    feat->SetLocation(*sequence::Seq_loc_Merge(*nuc_loc, CSeq_loc::fSortAndMerge_All, NULL));
    CVariation_inst& inst = feat->SetData().SetVariation().SetData().SetInstance();
    inst.SetType(variant_codon.length() == 1 ? CVariation_inst::eType_snv : CVariation_inst::eType_mnp);
    inst.SetDelta().push_back(delta2);

    return feat;
}

END_NCBI_SCOPE

