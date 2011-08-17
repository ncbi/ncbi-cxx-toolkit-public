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
#include <objects/general/Dbtag.hpp>

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
#include <objects/seqfeat/VariantProperties.hpp>
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
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/Object_id.hpp>

#include <objects/seqloc/Seq_loc_equiv.hpp>

#include <serial/iterator.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/seq_vector.hpp>
#include <misc/hgvs/variation_util2.hpp>

#include <objects/seqloc/Na_strand.hpp>

BEGIN_NCBI_SCOPE

namespace variation {

template<typename T>
void ChangeIdsInPlace(T& container, sequence::EGetIdType id_type, CScope& scope)
{
    for(CTypeIterator<CSeq_id> it = Begin(container); it; ++it) {
        CSeq_id& id = *it;
        CSeq_id_Handle idh = sequence::GetId(id, scope, id_type);
        id.Assign(*idh.GetSeqId());
    }
}

TSeqPos CVariationUtil::s_GetLength(const CVariantPlacement& p, CScope* scope)
{
    return ncbi::sequence::GetLength(p.GetLoc(), scope)
        + (p.IsSetStop_offset() ? p.GetStop_offset() : 0)
        - (p.IsSetStart_offset() ? p.GetStart_offset() : 0);
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
    CRef<CSeq_loc> loc(new CSeq_loc);
    loc->Assign(p.GetLoc());
    if(loc->IsPnt()) {
        //convert to interval
        loc = sequence::Seq_loc_Merge(*loc, CSeq_loc::fMerge_SingleRange, NULL);
        if(!p.IsSetStop_offset() && p.IsSetStart_offset()) {
            //In case of point, only start-offset may be specified. In this case
            //temporarily set stop-offset to be the same (it will be reset below),
            //such that start and stop of point-as-interval are adjusted consistently.
            p.SetStop_offset(p.GetStart_offset());
        }
    }

    if(!loc->IsInt() && (p.IsSetStart_offset() || p.IsSetStop_offset())) {
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
        //convert to point
        loc = sequence::Seq_loc_Merge(*loc, CSeq_loc::fSortAndMerge_All, NULL);
    }

    p.SetLoc().Assign(*loc);
}


void CVariationUtil::s_AddIntronicOffsets(CVariantPlacement& p, const CSpliced_seg& ss, CScope* scope)
{
    if(!p.GetLoc().IsPnt() && !p.GetLoc().IsInt()) {
        NCBI_THROW(CException, eUnknown, "Expected simple loc (int or pnt)");
    }

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

    CRef<CVariantPlacement> p2(new CVariantPlacement);
    p2->Assign(p);

    if(aln.GetSegs().IsSpliced()
       && sequence::IsSameBioseq(aln.GetSegs().GetSpliced().GetGenomic_id(),
                                 *p2->GetLoc().GetId(), m_scope))
    {
        s_AddIntronicOffsets(*p2, aln.GetSegs().GetSpliced(), m_scope);
    }

    CSeq_align::TDim target_row = -1;
    for(int i = 0; i < 2; i++) {
        if(sequence::IsSameBioseq(*p2->GetLoc().GetId(), aln.GetSeq_id(i), m_scope )) {
            target_row = 1 - i;
        }
    }
    if(target_row == -1) {
        NCBI_THROW(CException, eUnknown, "The alignment has no row for seq-id " + p2->GetLoc().GetId()->AsFastaString());
    }

    CRef<CSeq_loc_Mapper> mapper(new CSeq_loc_Mapper(aln, target_row, m_scope));
    mapper->SetMergeAll();

    CRef<CVariantPlacement> p3 = x_Remap(*p2, *mapper);

    if(p3->GetLoc().GetId()
       && aln.GetSegs().IsSpliced()
       && sequence::IsSameBioseq(aln.GetSegs().GetSpliced().GetGenomic_id(), *p3->GetLoc().GetId(), m_scope))
    {
        s_ResolveIntronicOffsets(*p3);
    }

    return p3;
}


CRef<CVariantPlacement> CVariationUtil::Remap(const CVariantPlacement& p, CSeq_loc_Mapper& mapper)
{
    CRef<CVariantPlacement> p2 = x_Remap(p, mapper);
    if((p.IsSetStart_offset() || p.IsSetStop_offset()) && p2->GetMol() == CVariantPlacement::eMol_genomic
       || p.GetMol() == CVariantPlacement::eMol_genomic
          && p2->GetMol() != CVariantPlacement::eMol_genomic
          && p2->GetMol() != CVariantPlacement::eMol_unknown /*in case remapped to nothing*/)
    {
        NcbiCerr << "Mapped: " << MSerial_AsnText << *p2;
        //When mapping an offset-placement to a genomic placement, may need to resolve offsets.
        //or, when mapping from genomic to a product coordinates, may need to add offsets. In above cases
        //we need to use the seq-align-based mapping.
        NCBI_THROW(CException, eUnknown, "Cannot use this method to remap between genomic and cdna coordinates");
    }

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
    p2->SetLoc(*mapped_loc);
    p2->SetPlacement_method(CVariantPlacement::ePlacement_method_projected);
    AttachSeq(*p2);

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
    if(sequence::GetLength(p.GetLoc(), m_scope) <= max_len
       && (!p.IsSetStart_offset() || p.GetStart_offset() == 0)
       && (!p.IsSetStop_offset() || p.GetStop_offset() == 0))
    {
        CRef<CSeq_literal> literal = x_GetLiteralAtLoc(p.GetLoc());
        p.SetSeq(*literal);
        ret = true;
    }

    return ret;
}

CVariantPlacement::TMol CVariationUtil::GetMolType(const CSeq_id& id)
{
    CBioseq_Handle bsh = m_scope->GetBioseqHandle(id);
    if(!bsh) {
        return CVariantPlacement::eMol_unknown;
    }

    if(bsh.IsSetDescr()) {
        ITERATE(CBioseq_Handle::TDescr::Tdata, it, bsh.GetDescr().Get()) {
            const CSeqdesc& desc = **it;
            if(!desc.IsSource()) {
                continue;
            }
            if(desc.GetSource().IsSetGenome() && desc.GetSource().GetGenome() == CBioSource::eGenome_mitochondrion) {
                return CVariantPlacement::eMol_mitochondrion;
            }
        }
    }

    const CMolInfo* m = sequence::GetMolInfo(bsh);
    if(!m) {
        return CVariantPlacement::eMol_unknown;
    }

    if(   m->GetBiomol() == CMolInfo::eBiomol_genomic
       || m->GetBiomol() == CMolInfo::eBiomol_other_genetic)
    {
        return CVariantPlacement::eMol_genomic;
    } else if(m->GetBiomol() == CMolInfo::eBiomol_mRNA
          || m->GetBiomol() == CMolInfo::eBiomol_ncRNA
          || m->GetBiomol() == CMolInfo::eBiomol_cRNA
          || m->GetBiomol() == CMolInfo::eBiomol_transcribed_RNA)
    {
        return CVariantPlacement::eMol_cdna;
    } else if(m->GetBiomol() == CMolInfo::eBiomol_peptide) {
        return CVariantPlacement::eMol_protein;
    } else {
        return CVariantPlacement::eMol_unknown;
    }

}


CVariationUtil::SFlankLocs CVariationUtil::CreateFlankLocs(const CSeq_loc& loc, TSeqPos len)
{
    TSignedSeqPos start = sequence::GetStart(loc, m_scope, eExtreme_Positional);
    TSignedSeqPos stop = sequence::GetStop(loc, m_scope, eExtreme_Positional);

    CBioseq_Handle bsh = m_scope->GetBioseqHandle(loc);
    TSignedSeqPos max_pos = bsh.GetInst_Length() - 1;

    SFlankLocs flanks;
    flanks.upstream.Reset(new CSeq_loc);
    flanks.upstream->SetInt().SetId().Assign(sequence::GetId(loc, NULL));
    flanks.upstream->SetInt().SetStrand(sequence::GetStrand(loc, NULL));
    flanks.upstream->SetInt().SetTo(min(max_pos, stop + (TSignedSeqPos)len));
    flanks.upstream->SetInt().SetFrom(max((TSignedSeqPos)0, start - (TSignedSeqPos)len));

    flanks.downstream.Reset(new CSeq_loc);
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

    static const string iupac_nuc_ambiguity_codes = "NTGKCYSBAWRDMHVN";
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

    if(!v.GetData().GetInstance().GetDelta().size() == 1) {
        NCBI_THROW(CException, eUnknown, "Expected single-element delta");
    }

    const CDelta_item& delta = *v.GetData().GetInstance().GetDelta().front();
    if(delta.IsSetAction() && delta.GetAction() != CDelta_item::eAction_morph) {
        NCBI_THROW(CException, eUnknown, "Expected morph action");
    }

    if(!delta.IsSetSeq() || !delta.GetSeq().IsLiteral() || delta.GetSeq().GetLiteral().GetLength() != 1) {
        NCBI_THROW(CException, eUnknown, "Expected literal of length 1 in inst.");
    }

    CSeq_data variant_prot_seq;
    CSeqportUtil::Convert(delta.GetSeq().GetLiteral().GetSeq_data(), &variant_prot_seq, CSeq_data::e_Iupacaa);

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

    if(sequence::GetLength(placement.GetLoc(), NULL) != 1) {
        NCBI_THROW(CException, eUnknown, "Expected single-aa location");
    }

    SAnnotSelector sel(CSeqFeatData::e_Cdregion, true);
       //note: sel by product; location is prot; found feature is mrna having this prot as product
    CRef<CSeq_loc_Mapper> prot2precursor_mapper;
    for(CFeat_CI ci(*m_scope, placement.GetLoc(), sel); ci; ++ci) {
        prot2precursor_mapper.Reset(new CSeq_loc_Mapper(ci->GetMappedFeature(), CSeq_loc_Mapper::eProductToLocation, m_scope));
        break;
    }

    if(!prot2precursor_mapper) {
        NCBI_THROW(CException, eUnknown, "Can't create prot2precursor mapper. Is this a prot?");
    }

    CRef<CSeq_loc> nuc_loc = prot2precursor_mapper->Map(placement.GetLoc());
    ChangeIdsInPlace(*nuc_loc, sequence::eGetId_ForceAcc, *m_scope);

    if(!nuc_loc->IsInt() || sequence::GetLength(*nuc_loc, NULL) != 3) {
        NCBI_THROW(CException, eUnknown, "AA does not remap to a single codon.");
    }



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

    //Note: The result describes whole codons, even for point mutations, i.e. common suffx/prefix are not truncated.
    return v2;
}





CRef<CSeq_literal> CVariationUtil::x_GetLiteralAtLoc(const CSeq_loc& loc)
{
    CSeqVector v(loc, *m_scope, CBioseq_Handle::eCoding_Iupac);
    string seq;
    v.GetSeqData(v.begin(), v.end(), seq);
    CRef<CSeq_literal> literal(new CSeq_literal);
    literal->SetLength(seq.length());
    if(v.IsProtein()) {
        literal->SetSeq_data().SetNcbieaa().Set(seq);
    } else if (v.IsNucleotide()) {
        literal->SetSeq_data().SetIupacna().Set(seq);
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
        CSeqportUtil::Append(&(c->SetSeq_data()),
                             a.GetSeq_data(), 0, a.GetLength(),
                             b.GetSeq_data(), 0, b.GetLength());

        c->SetLength(a.GetLength() + b.GetLength());

        if(a.IsSetFuzz() || b.IsSetFuzz()) {
            c->SetFuzz().SetLim(CInt_fuzz::eLim_unk);
        }
    }
    return c;
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
                CConstRef<CSeq_literal> this_literal = s_FindFirstLiteral(v);
                if(!this_literal) {
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
                CConstRef<CSeq_literal> this_literal = s_FindFirstLiteral(v);
                if(!this_literal) {
                    NCBI_THROW(CException, eUnknown, "Could not find literal for 'this' location in placements");
                } else {
                    CRef<CSeq_literal> cat_literal = s_CatLiterals(di.GetSeq().GetLiteral(), *this_literal);
                }
            }
        }
    }
}



void CVariationUtil::AttachProteinConsequences(CVariation& v, const CSeq_id* target_id)
{
    v.Index();
    const CVariation::TPlacements* placements = s_GetPlacements(v);

    if(!placements || placements->size() == 0) {
        return;
    }

    if(v.GetData().IsSet()) {
        NON_CONST_ITERATE(CVariation::TData::TSet::TVariations, it, v.SetData().SetSet().SetVariations()) {
            AttachProteinConsequences(**it, target_id);
        }
        return;
    }

    if(!v.GetData().IsInstance()) {
        return;
    }

    if(v.GetData().GetInstance().IsSetObservation()
       && !(v.GetData().GetInstance().GetObservation() & CVariation_inst::eObservation_variant))
    {
        //only compute placements for variant instances; (as for asserted/reference there's no change)
        return;
    }


    SAnnotSelector sel(CSeqFeatData::e_Cdregion);
    sel.SetAdaptiveDepth();
    sel.SetResolveAll();
    sel.SetIgnoreStrand(); //this alone does not work, e.g. NC_000001.9:g.113258125C>T  won't find the CDS, even though it is there

    ITERATE(CVariation::TPlacements, it, *placements) {
        const CVariantPlacement& placement = **it;

        if(!placement.GetLoc().GetId()
           || target_id && !sequence::IsSameBioseq(*target_id,
                                                   sequence::GetId(placement.GetLoc(), NULL),
                                                   m_scope))
        {
            continue;
        }

        if(placement.IsSetStart_offset() && (placement.IsSetStop_offset() || placement.GetLoc().IsPnt())) {
            continue; //intronic.
        }
        CRef<CSeq_loc> loc(new CSeq_loc);
        loc->Assign(placement.GetLoc());
        loc->SetStrand(eNa_strand_both);

        for(CFeat_CI ci(*m_scope, *loc , sel); ci; ++ci) {
            CRef<CVariation> prot_variation = TranslateNAtoAA(v.GetData().GetInstance(), placement, ci->GetMappedFeature());
            CVariation::TConsequence::value_type consequence(new CVariation::TConsequence::value_type::TObjectType);
            consequence->SetVariation(*prot_variation);
            v.SetConsequence().push_back(consequence);
        }
    }
}

//translate IUPACNA literal; do not translate last partial codon.
string Translate(const CSeq_data& literal)
{
    string prot_str("");
    string nuc_str = literal.GetIupacna().Get();
    nuc_str.resize(nuc_str.size() - (nuc_str.size() % 3)); //truncate last partial codon, as translator may otherwise still be able to translate it

    CSeqTranslator translator;
    translator.Translate(
            nuc_str,
            prot_str,
            CSeqTranslator::fIs5PrimePartial);

    return prot_str;
}

CVariation_inst::EType CalcInstTypeForAA(const string& prot_ref_str, const string& prot_delta_str)
{
    CVariation_inst::EType inst_type;
    if(prot_delta_str.size() == 0 && prot_ref_str.size() > 0) {
        inst_type = CVariation_inst::eType_del;
    } else if(prot_delta_str.size() != prot_ref_str.size()) {
        inst_type  =CVariation_inst::eType_prot_other;
    } else if(prot_ref_str == prot_delta_str) {
        inst_type = CVariation_inst::eType_prot_silent;
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


void CVariationUtil::FlipStrand(CVariation& v) const
{
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
        FlipStrand(v.SetData().SetInstance());
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
    if(!loc.IsInt()) {
        NCBI_THROW(CException, eUnknown, "Expected Int location");
    }

    if(!v.IsSetPlacements() || v.GetPlacements().size() != 1) {
        NCBI_THROW(CException, eUnknown, "Expected single placement");
    }

    const CSeq_loc& orig_loc = v.GetPlacements().front()->GetLoc();
    CRef<CSeq_loc> sub_loc = orig_loc.Subtract(loc, 0, NULL, NULL);
    if(sub_loc->Which() && sequence::GetLength(*sub_loc, NULL) > 0) {
        //NcbiCerr << MSerial_AsnText << v;
        //NcbiCerr << MSerial_AsnText << loc;
        NCBI_THROW(CException, eUnknown, "Location must be a superset of the variation's loc");
    }

    CRef<CSeq_loc> suffix_loc(new CSeq_loc);
    suffix_loc->Assign(loc);
    suffix_loc->SetInt().SetFrom(sequence::GetStop(orig_loc, NULL, eExtreme_Positional) + 1);

    CRef<CSeq_loc> prefix_loc(new CSeq_loc);
    prefix_loc->Assign(loc);
    prefix_loc->SetInt().SetTo(sequence::GetStart(orig_loc, NULL, eExtreme_Positional) - 1);

    if(sequence::GetStrand(loc, NULL) == eNa_strand_minus) {
        swap(prefix_loc, suffix_loc);
    }

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
        CRef<CVariantPlacement> p(new CVariantPlacement);
        p->SetLoc().Assign(cds_feat.GetProduct());
        p->SetMol(CVariantPlacement::eMol_protein);
        CRef<CVariation> v(new CVariation);
        v->SetData().SetUnknown();
        v->SetPlacements().push_back(p);
        ChangeIdsInPlace(*v, sequence::eGetId_ForceAcc, *m_scope);
        return v;
    }


    //create an inst-variation from the provided inst with the single specified placement
    CRef<CVariation> v(new CVariation);
    v->SetData().SetInstance().Assign(nuc_inst);
    v->ResetPlacements();

    CRef<CVariantPlacement> p(new CVariantPlacement);
    p->Assign(nuc_p);
    v->SetPlacements().push_back(p);

    //normalize to delins form so we can deal with it uniformly
    ChangeToDelins(*v);

    //detect frameshift before we start extending codon locs, etc.
    const CDelta_item& nuc_delta = *v->GetData().GetInstance().GetDelta().front();
    int frameshift_phase = ((long)sequence::GetLength(p->GetLoc(), NULL) - (long)nuc_delta.GetSeq().GetLiteral().GetLength()) % 3;


    if(!SameOrientation(sequence::GetStrand(cds_feat.GetLocation(), NULL),
                        sequence::GetStrand(p->GetLoc(), NULL)))
    {
        //need the variation and cds to be in the same orientation, such that
        //sequence represents the actual codons and AAs when mapped to protein
        FlipStrand(*v);
    }

    if(verbose) NcbiCerr << "Normalized variation: " << MSerial_AsnText << *v;

    //Map to protein and back to get the affected codons

    CRef<CSeq_loc_Mapper> nuc2prot_mapper(new CSeq_loc_Mapper(cds_feat, CSeq_loc_Mapper::eLocationToProduct, m_scope));
    CRef<CSeq_loc_Mapper> prot2nuc_mapper(new CSeq_loc_Mapper(cds_feat, CSeq_loc_Mapper::eProductToLocation, m_scope));

    CRef<CSeq_loc> prot_loc = nuc2prot_mapper->Map(p->GetLoc());
    CRef<CSeq_loc> codons_loc = prot2nuc_mapper->Map(*prot_loc);
    TSignedSeqPos frame = abs(
              (TSignedSeqPos)p->GetLoc().GetStart(eExtreme_Biological)
            - (TSignedSeqPos)codons_loc->GetStart(eExtreme_Biological));

    codons_loc->SetId(sequence::GetId(p->GetLoc(), NULL)); //restore the original id, as mapping forward and back may have changed the type
    ChangeIdsInPlace(*prot_loc, sequence::eGetId_ForceAcc, *m_scope); //not strictly required, but generally prefer accvers in the output for readability
                                                                      //as we're dealing with various types of mols


    /*
     * extend codons-loc and the variation by two bases downstream, since a part of the next
     * codon may become part of the variation (e.g. 1-base deletion in a codon
     * results in first base of the downstream codon becoming part of modified one)
     * If, on the other hand, the downstream codon does not participate, there's
     * only two bases if it, so it won't get translated.
     */
    SFlankLocs flocs = CreateFlankLocs(*codons_loc, 2);
    CRef<CSeq_loc> extended_codons_loc = sequence::Seq_loc_Add(*codons_loc, *flocs.downstream, CSeq_loc::fSortAndMerge_All, NULL);
    x_AdjustDelinsToInterval(*v, *extended_codons_loc);
    const CSeq_literal& extended_variant_codons_literal = v->GetData().GetInstance().GetDelta().front()->GetSeq().GetLiteral();
    string variant_codons_str = extended_variant_codons_literal.GetSeq_data().GetIupacna().Get();
    variant_codons_str.resize(variant_codons_str.size() - (variant_codons_str.size() % 3));


    if(verbose) NcbiCerr << "Adjusted-for-codons " << MSerial_AsnText << *v;

    AttachSeq(*p);
    string prot_ref_str = Translate(p->GetSeq().GetSeq_data());
    string prot_delta_str = Translate(extended_variant_codons_literal.GetSeq_data());

    //Constructing protein-variation
    CRef<CVariation> prot_v(new CVariation);

    //will have two placements: one describing the AA, and the other describing codons
    CRef<CVariantPlacement> prot_p(new CVariantPlacement);
    prot_p->SetLoc(*prot_loc);
    prot_p->SetMol(GetMolType(sequence::GetId(prot_p->GetLoc(), NULL)));
    AttachSeq(*prot_p);
    prot_v->SetPlacements().push_back(prot_p);

    CRef<CVariantPlacement> codons_p(new CVariantPlacement);
    codons_p->SetLoc(*codons_loc);
    codons_p->SetMol(GetMolType(sequence::GetId(codons_p->GetLoc(), NULL)));
    codons_p->SetFrame(frame);
    AttachSeq(*codons_p);
    prot_v->SetPlacements().push_back(codons_p);


    prot_v->SetVariant_prop().SetEffect(CalcEffectForProt(prot_ref_str, prot_delta_str));
    if(prot_v->SetVariant_prop().GetEffect() == 0) {
        prot_v->SetVariant_prop().ResetEffect();
    }

    prot_v->SetData().SetInstance().SetType(CalcInstTypeForAA(prot_ref_str, prot_delta_str));



    //set inst data
    CRef<CDelta_item> di(new CDelta_item);
    prot_v->SetData().SetInstance().SetDelta().push_back(di);

    if(prot_delta_str.size() > 0) {

#if 0
        di->SetSeq().SetLiteral().SetSeq_data().SetNcbieaa().Set(prot_delta_str);
        di->SetSeq().SetLiteral().SetLength(prot_delta_str.size());
        prot_v->SetData().SetInstance().SetDelta().push_back(di);
#else
        //Use NA alphabet instead of AA, as the dbSNP requested original codons.
        //The user can always translate this to AAs.
        di->SetSeq().SetLiteral().SetSeq_data().SetIupacna().Set(variant_codons_str);
        di->SetSeq().SetLiteral().SetLength(variant_codons_str.size());
#endif

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

    if(p.GetEffect() & CVariantProperties::eEffect_frameshift) {
        terms.push_back(eSO_frameshift_variant);
    }
    if(p.GetEffect() & CVariantProperties::eEffect_missense) {
        terms.push_back(eSO_non_synonymous_codon);
    }
    if(p.GetEffect() & CVariantProperties::eEffect_nonsense || p.GetEffect() & CVariantProperties::eEffect_stop_gain) {
        terms.push_back(eSO_stop_gained);
    }
    if(p.GetEffect() & CVariantProperties::eEffect_nonsense || p.GetEffect() & CVariantProperties::eEffect_stop_loss) {
        terms.push_back(eSO_stop_lost);
    }
    if(p.GetEffect() & CVariantProperties::eEffect_synonymous) {
        terms.push_back(eSO_synonymous_codon);
    }
}

string CVariationUtil::AsString(ESOTerm term)
{
    if(term == eSO_2KB_upstream_variant) {
        return "2KB_upstream_variant";
    } else if(term == eSO_500B_downstream_variant) {
        return "500B_downstream_variant";
    } else if(term == eSO_splice_donor_variant) {
        return "splice_donor_variant";
    } else if(term == eSO_splice_acceptor_variant) {
        return "splice_acceptor_varian";
    } else if(term == eSO_intron_variant) {
        return "intron_variant";
    } else if(term == eSO_5_prime_UTR_variant) {
        return "5_prime_UTR_variant";
    } else if(term == eSO_3_prime_UTR_variant) {
        return "3_prime_UTR_variant";
    } else if(term == eSO_coding_sequence_variant) {
        return "coding_sequence_variant";
    } else if(term == eSO_nc_transcript_variant) {
        return "nc_transcript_variant";
    } else if(term == eSO_synonymous_codon) {
        return "synonymous_codon";
    } else if(term == eSO_non_synonymous_codon) {
        return "non_synonymous_codon";
    } else if(term == eSO_stop_gained) {
        return "stop_gained";
    } else if(term == eSO_stop_lost) {
        return "stop_lost";
    } else if(term == eSO_frameshift_variant) {
        return "frameshift_variant";
    } else {
        return "other_variant";
    }
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



void CVariationUtil::x_SetVariantProperties(CVariantProperties& prop, CVariantPlacement& placement)
{
    //todo: need to zero-out uninteresting bits if we have interesting bits. Perhaps this can
    //be accomplished by truncating indexable neighborhood locations by subtracting the union of all gene ranges.

    if(!prop.IsSetGene_location()) {
        //need to zero-out the bitmask, otherwise in debug mode it will be preset to a magic value,
        //and then modifying it with "|=" will produce garbage.
        prop.SetGene_location(0);
    }

    //for offset-style intronic locations (not genomic and have offset), can infer where we are based on offset
    if(!placement.IsSetMol() || placement.GetMol() != CVariantPlacement::eMol_genomic) {
        CBioseq_Handle bsh = m_scope->GetBioseqHandle(placement.GetLoc());
        if(placement.IsSetStart_offset()) {
            x_SetVariantPropertiesForIntronic(prop, placement.GetStart_offset(), placement.GetLoc(), bsh);
        }
        if(placement.IsSetStop_offset()) {
            x_SetVariantPropertiesForIntronic(prop, placement.GetStop_offset(), placement.GetLoc(), bsh);
        }
    }

    CVariantPropertiesIndex::TGeneIDAndPropVector v;
    m_variant_properties_index.GetLocationProperties(placement.GetLoc(), v);

    //collapse all gene-specific location properties into prop
    ITERATE(CVariantPropertiesIndex::TGeneIDAndPropVector, it, v) {
        int gene_id = it->first;
        CVariantProperties::TGene_location loc_prop = it->second;
        if(gene_id != 0) {
            s_AttachGeneIDdbxref(placement, gene_id);
        }
        prop.SetGene_location() |= loc_prop;
    }

#if 0
    if(prop.GetGene_location() == 0) {
        prop.ResetGene_location();
    }
#endif

}



void CVariationUtil::x_SetVariantPropertiesForIntronic(CVariantProperties& p, int offset, const CSeq_loc& loc, CBioseq_Handle& bsh)
{
    if(!p.IsSetGene_location()) {
        //need to zero-out the bitmask, otherwise in debug mode it will be preset to a magic value,
        //and then modifying it with "|=" will produce garbage.
        p.SetGene_location(0);
    }

    if(loc.GetStop(eExtreme_Positional) + 1 >= bsh.GetInst_Length() && offset > 0) {
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
    for(CTypeIterator<CVariation> it(Begin(v)); it; ++it) {
        CVariation& v2 = *it;
        if(!v2.IsSetPlacements()) {
            continue;
        }
        NON_CONST_ITERATE(CVariation::TPlacements, it2, v2.SetPlacements()) {
            x_SetVariantProperties(v2.SetVariant_prop(), **it2);
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
            x_Index(idh);
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
        v.push_back(*it);
    }
}

void CVariationUtil::CVariantPropertiesIndex::x_Add(const CSeq_loc& loc, int gene_id, CVariantProperties::TGene_location prop)
{
    for(CSeq_loc_CI ci(loc); ci; ++ci) {
        m_loc2prop[ci.GetSeq_id_Handle()][ci.GetRange()].push_back(TGeneIDAndProp(gene_id, prop));
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

    CRef<CSeq_loc> all_gene_ranges(new CSeq_loc(CSeq_loc::e_Mix));
    for(ci.Rewind(); ci; ++ci) {
        const CMappedFeat& mf = *ci;
        if(mf.GetData().IsGene()) {
            all_gene_ranges->SetMix().Set().push_back(
                    sequence::Seq_loc_Merge(mf.GetLocation(), CSeq_loc::fMerge_SingleRange, NULL));
        }
    }
    all_gene_ranges->ResetStrand();
    SFastLocSubtract subtract_all_gene_overlap_from(*all_gene_ranges);

    CRef<CSeq_loc> all_gene_neighborhoods(new CSeq_loc(CSeq_loc::e_Mix));
    for(ci.Rewind(); ci; ++ci) {
        const CMappedFeat& mf = *ci;
        CMappedFeat parent_mf = ft.GetParent(mf);
        if(!mf.GetLocation().GetId()) {
            continue;
        }

        int gene_id = s_GetGeneID(mf, ft);

        if(!parent_mf) {
            TLocsPair p = s_GetNeighborhoodLocs(mf.GetLocation(), bsh.GetInst_Length() - 1);

            //exclude any gene overlap from neighborhood (i.e. we don't care if location is in
            //neighborhood of gene-A if it is within gene-B.

            //First, reset strands (as we did with gene_ranges), as we need to do this in strand-agnostic
            //manner. Note, a smart thing to do would be to set all-gene-ranges strand to "both", with an
            //expectation that subtracting such a loc from either plus or minus loc would work, but unfortunately
            //it doesn't. Resetting strand, however, is fine, because our indexing is strand-agnostic.
            p.first->ResetStrand();
            p.second->ResetStrand();

#if 0
            //all_gene_ranges is a big complex loc, and subtracting with Seq_loc_Subtract
            //for every neighborhood is slow (takes almost 10 seconds for NC_000001),
            //so we'll use fast map-based subtractor instead
            p.first = sequence::Seq_loc_Subtract(*p.first, *all_gene_ranges, CSeq_loc::fSortAndMerge_All, NULL);
            p.second = sequence::Seq_loc_Subtract(*p.second, *all_gene_ranges, CSeq_loc::fSortAndMerge_All, NULL);
#else
            subtract_all_gene_overlap_from(*p.first);
            subtract_all_gene_overlap_from(*p.second);
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

            p = s_GetUTRLocs(mf.GetLocation(), parent_mf.GetLocation());
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
        CRef<CSeq_loc> sub_loc = sequence::Seq_loc_Add(*all_gene_neighborhoods, *all_gene_ranges, 0, NULL);
        sub_loc->ResetStrand();
        CRef<CSeq_loc> intergenic_loc = sequence::Seq_loc_Subtract(
                *range_loc,
                *sub_loc,
                CSeq_loc::fSortAndMerge_All,
                NULL);
        x_Add(*intergenic_loc, 0, CVariantProperties::eGene_location_intergenic);
    }
}

int CVariationUtil::CVariantPropertiesIndex::s_GetGeneID(const CMappedFeat& mf, feature::CFeatTree& ft)
{
    int gene_id = 0;
    if(mf.IsSetDbxref()) {
        ITERATE(CSeq_feat::TDbxref, it, mf.GetDbxref()) {
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

    p.first->SetInt().SetFrom(p.first->GetTotalRange().GetFrom() < flank1_len ? 0 : p.first->GetTotalRange().GetFrom() - flank1_len);
    p.second->SetInt().SetTo(p.second->GetTotalRange().GetTo() > max_pos ? max_pos : p.second->GetTotalRange().GetTo() + flank2_len);

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
        if(seqint.GetLength() >= 4) {
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








#if 0
/// Flatten variation to a collection of variation-ref features
/// (note that we use vector of feats, rather than seq-annot, as
/// we have different seq-ids for different variant-placements).
///
///
typedef vector<CRef<CSeq_feat> > TFeats;
void ToVariationRefs(const CVariation& v, TFeats& feats) const;


void CVariationUtil::ToVariationRefs(const CVariation& v, TFeats& feats) const
{
    //if subvariations have their own placements, features will be created at their level - this level is just a container.
    bool is_container = false;
    if(v.GetData().IsSet()) {
        ITERATE(CVariation::TData::TSet::TVariations, it, v.GetData().GetSet().GetVariations()) {
            const CVariation& subvariation = **it;
            for(CTypeConstIterator<CVariantPlacement> it2(Begin(subvariation)); it2; ++it2) {
                is_container = true;
                break;
            }
        }
    }

    if(is_container) {
        //recurse
        ITERATE(CVariation::TData::TSet::TVariations, it, v.GetData().GetSet().GetVariations()) {
            ToVariationRefs(**it, feats);
        }
    } else if(v.IsSetPlacements()) {
        ITERATE(CVariation::TPlacements, it, v.GetPlacements()) {

        }
    }

}


CRef<CVariation_ref> CVariationUtil::x_AsVariation_ref(const CVariation& v) const
{
    CRef<CVariation_ref> vr(new CVariation_ref);
    vr->SetData().Assign(v.GetData().GetInstance());
    if(v.IsSetConsequence()) {

    }

    return vr;
}
#endif


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

