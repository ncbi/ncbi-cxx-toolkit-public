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
 *
 */

#include <ncbi_pch.hpp>

#include <objects/general/Object_id.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/Dbtag.hpp>

#include <objects/seqloc/Seq_point.hpp>
#include <objects/seqloc/Seq_loc_equiv.hpp>

#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Spliced_seg.hpp>
#include <objects/seqalign/Spliced_exon.hpp>
#include <objects/seqalign/Product_pos.hpp>
#include <objects/seqalign/Prot_pos.hpp>

#include <objects/variation/Variation.hpp>
#include <objects/variation/VariantPlacement.hpp>
#include <objects/variation/VariationMethod.hpp>

#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/Genetic_code.hpp>
#include <objects/seqfeat/Variation_inst.hpp>
#include <objects/seqfeat/Delta_item.hpp>
#include <objects/seqfeat/Ext_loc.hpp>
#include <objects/seqfeat/SeqFeatXref.hpp>
#include <objects/seqfeat/BioSource.hpp>

#include <objects/seq/seqport_util.hpp>
#include <objects/seq/Seq_literal.hpp>
#include <objects/seq/Seq_data.hpp>
#include <objects/seq/Numbering.hpp>
#include <objects/seq/Num_ref.hpp>
#include <objects/seq/Annot_descr.hpp>
#include <objects/seq/Annotdesc.hpp>
#include <objects/seq/Seq_descr.hpp>

#include <serial/iterator.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/seq_loc_mapper.hpp>

#include <misc/hgvs/hgvs_parser2.hpp>
#include <misc/hgvs/variation_util2.hpp>

BEGIN_NCBI_SCOPE


namespace variation {


CRef<CVariantPlacement> CHgvsParser::x_AdjustPlacementForHgvs(const CVariantPlacement& p, const CVariation_inst& inst)
{
    CRef<CVariantPlacement> placement(new CVariantPlacement);
    placement->Assign(p);

    if(inst.GetType() == CVariation_inst::eType_ins
       && inst.GetDelta().size() > 0
       && inst.GetDelta().front()->IsSetAction()
       && inst.GetDelta().front()->GetAction() == CDelta_item::eAction_ins_before
       && p.GetLoc().IsPnt()
       && !(p.IsSetStart_offset() && p.IsSetStop_offset()))
    {
        //insertion: convert the loc to dinucleotide representation as necessary.
        CVariationUtil util(*m_scope);
        CVariationUtil::SFlankLocs flanks = util.CreateFlankLocs(p.GetLoc(), 1);
        CRef<CSeq_loc> dinucleotide_loc = sequence::Seq_loc_Add(*flanks.upstream, p.GetLoc(), CSeq_loc::fSortAndMerge_All, NULL);
        placement->SetLoc(*dinucleotide_loc);
    } else if(inst.GetType() == CVariation_inst::eType_microsatellite) {
        CRef<CSeq_loc> loc = sequence::Seq_loc_Merge(p.GetLoc(), CSeq_loc::fMerge_SingleRange, NULL);
        TSeqPos unit_length = x_GetInstLength(inst, p, false);

        if(loc->GetStrand() == eNa_strand_minus) {
            loc->SetInt().SetFrom(loc->GetInt().GetTo() - (unit_length - 1) );
        } else {
            loc->SetInt().SetTo(loc->GetInt().GetFrom() + unit_length - 1 );
        }
        placement->SetLoc(*loc);
        //todo: do we need to do anything special if it is an offset-loc?
    }

    return placement;
}

CConstRef<CSeq_literal> CHgvsParser::x_FindAssertedSequence(const CVariation& v)
{
    CConstRef<CSeq_literal> asserted_seq;

    if(!v.GetData().IsSet() || v.GetData().GetSet().GetType() != CVariation::TData::TSet::eData_set_type_package) {
        return asserted_seq;
    }

    ITERATE(CVariation::TData::TSet::TVariations, it, v.GetData().GetSet().GetVariations()) {
        const CVariation& v2 = **it;
        if(!v2.GetData().IsInstance() || !v2.GetData().GetInstance().IsSetObservation()) {
            continue;
        }

        //Note that we are only interested in allerted observation if it has the same
        //placement as its sibling (i.e. placement attached at the parent level, rather at v2). This situation may
        //arise when we compute a nucleotide precursor variation from a protein variation, and truncate
        //common suffix and prefix. The variant allele may be truncated differently than the asserted allele,
        //and they each will get different placements.
        if(v2.GetData().GetInstance().GetObservation() & (CVariation_inst::eObservation_asserted | CVariation_inst::eObservation_reference)
           && !v2.IsSetPlacements()
           && v2.GetData().GetInstance().GetDelta().size() > 0 //VAR-528
           && v2.GetData().GetInstance().GetDelta().front()->IsSetSeq()
           && v2.GetData().GetInstance().GetDelta().front()->GetSeq().IsLiteral())
        {
            asserted_seq.Reset(&v2.GetData().GetInstance().GetDelta().front()->GetSeq().GetLiteral());
            break;
        }
    }

    return asserted_seq;
}


string CHgvsParser::AsHgvsExpression(const CVariation& variation,  CConstRef<CSeq_id> seq_id)
{
    //create a copy so we can call Index on it, and attach
    //seq to placements, as necessary, as it will potentially be used to construct
    //asserted-sequence part of HGVS

    CRef<CVariation> v(new CVariation);
    v->Assign(variation);
    v->Index();

#if 0
    //sometimes we don't need it, and creating it is slow
    CVariationUtil util(*m_scope);
    for(CTypeIterator<CVariantPlacement> it(Begin(*v)); it; ++it) {
        CVariantPlacement& p = *it;
        if(!p.IsSetSeq()) {
            util.AttachSeq(p);
        }
    }
#endif

    return x_AsHgvsExpression(*v, seq_id, CConstRef<CSeq_literal>(NULL));
}

string CHgvsParser::x_AsHgvsExpression(
        const CVariation& variation,
        CConstRef<CSeq_id> seq_id,
        CConstRef<CSeq_literal> asserted_seq)
{
    //Find the placement to use (It is possible not to have one at the top level, if subvariations have their own)
    CRef<CVariantPlacement> placement;

    const CVariation::TPlacements* placements = CVariationUtil::s_GetPlacements(variation);
    if(placements) {
        ITERATE(CVariation::TPlacements, it, *placements) {
            const CVariantPlacement& p = **it;
            if(!seq_id || (p.GetLoc().GetId() && sequence::IsSameBioseq(*p.GetLoc().GetId(), *seq_id, m_scope))) {
                placement.Reset(new CVariantPlacement);
                placement->Assign(p);
                break;
            }
        }
        if(!placement) {
            NCBI_THROW(CException, eUnknown, "Variations.placements is set, but could not find requested one");
        }
    }

    //Hgvs can't represent opposite orientation; flip as necessary and call recursively
    if(placement && placement->GetLoc().GetStrand() == eNa_strand_minus) {
        CRef<CVariation> flipped_variation(new CVariation);
        flipped_variation->Assign(variation);

        CVariationUtil util(*m_scope);
        util.FlipStrand(*flipped_variation);

        CRef<CSeq_literal> flipped_asserted_seq;

        if(asserted_seq) {
            flipped_asserted_seq.Reset(new CSeq_literal);
            flipped_asserted_seq->Assign(*asserted_seq);
            if(asserted_seq->IsSetSeq_data()) {
                CSeqportUtil::ReverseComplement(
                        asserted_seq->GetSeq_data(),
                        &flipped_asserted_seq->SetSeq_data(),
                        0, asserted_seq->GetLength());
            }
        }
        return x_AsHgvsExpression(*flipped_variation, seq_id, flipped_asserted_seq);
    }

    //Asserted sequence does not participate in HGVS expression as independent instance subexpression.
    //Instead, we'll find it here (e.g. "A") and pass down to create variant subexpressions, e.g. [A>C]+[A>G]
    if(variation.GetData().IsSet()
       && variation.GetData().GetSet().GetType() == CVariation::TData::TSet::eData_set_type_package)
    {
        asserted_seq = x_FindAssertedSequence(variation);
    }

    string hgvs_data_str = "";
    size_t subvariation_count(0);
    if(variation.GetData().IsSet()) {
        const CVariation::TData::TSet& vset = variation.GetData().GetSet();
        string delim_type =
                vset.GetType() == CVariation::TData::TSet::eData_set_type_compound    ? ""
              : vset.GetType() == CVariation::TData::TSet::eData_set_type_haplotype   ? ";"
              : vset.GetType() == CVariation::TData::TSet::eData_set_type_products    ? ","
              : vset.GetType() == CVariation::TData::TSet::eData_set_type_mosaic       ? "/"
              : vset.GetType() == CVariation::TData::TSet::eData_set_type_alleles
                || vset.GetType() == CVariation::TData::TSet::eData_set_type_genotype
                || vset.GetType() == CVariation::TData::TSet::eData_set_type_package  ? ";"
              : vset.GetType() == CVariation::TData::TSet::eData_set_type_individual  ? "(;)"
              : "(;)";

        string delim = "";
        ITERATE(CVariation::TData::TSet::TVariations, it, variation.GetData().GetSet().GetVariations()) {
            const CVariation& v2 = **it;

            //asserted or reference instances don't participate in HGVS expressions as individual subvariation expressions
            //Exception: it is the only member of the set: (JIRA: VAR-626)
            if(v2.GetData().IsInstance()
               && variation.GetData().GetSet().GetVariations().size() > 1
               && v2.GetData().GetInstance().IsSetObservation()
               && !(v2.GetData().GetInstance().GetObservation() & CVariation_inst::eObservation_variant))
            {
                continue;
            }

            string subvariation_expr = x_AsHgvsExpression(**it, seq_id, asserted_seq);

            hgvs_data_str += delim + subvariation_expr;
            delim = delim_type;
            subvariation_count++;
        }
    } else if(variation.GetData().IsInstance()) {
        if(placement) {
            placement = x_AdjustPlacementForHgvs(*placement, variation.GetData().GetInstance());
        }

        hgvs_data_str = x_AsHgvsInstExpression(variation, placement, asserted_seq);
    } else if(variation.GetData().IsUnknown()) {
        hgvs_data_str = "?";
    } else if(variation.GetData().IsNote()) {
        hgvs_data_str = ":" + variation.GetData().GetNote();
    } else {
        hgvs_data_str = ":OTHER";
    }

    if(variation.IsSetFrameshift()) {
        hgvs_data_str += "fs";
        if(variation.GetFrameshift().IsSetX_length()) {
            hgvs_data_str += "*" + NStr::NumericToString(variation.GetFrameshift().GetX_length());
        }
    }

    if(variation.IsSetMethod()
      && find(variation.GetMethod().GetMethod().begin(),
              variation.GetMethod().GetMethod().end(),
              CVariationMethod::eMethod_E_computational) != variation.GetMethod().GetMethod().end())
    {
        hgvs_data_str = "(" + hgvs_data_str + ")";
    }


    bool is_bracketed = false; //will compute whether need to put this subexpression in brackets
    bool location_within_brackets = true;
        //will compute whether the location prefix should be factored from brackets, e.g. NM_004004.2:c.35[dupG;A>G]
        //or within the brackets, e.g. [NM_004004.2:c.35delG]+[NM_006783.1:c.689_690insT]

    if(variation.GetParent()) {
        //If a variation is an element of an alleles|genotype set,
        //it describes an allele and must be bracketed.
        CVariation::TData::TSet::TType type = variation.GetParent()->GetData().GetSet().GetType();
        is_bracketed =
              type == CVariation::TData::TSet::eData_set_type_alleles
           || type == CVariation::TData::TSet::eData_set_type_genotype;


        if(variation.GetData().IsInstance()
            && variation.GetData().GetInstance().GetType() == CVariation_inst::eType_microsatellite)
        {
            //Except for microsatellites, as they are bracketed at the inst-level (see the SSR grammar for details)
            is_bracketed = false;
        }
    } else if(subvariation_count > 1) {
        //Root non-singleton variation: it describes a single allele (i.e. also needs to be bracketed)
        //UNLESS it is a set that itself describes individual alleles.
        CVariation::TData::TSet::TType type = variation.GetData().GetSet().GetType();
        is_bracketed =
                type != CVariation::TData::TSet::eData_set_type_alleles
             && type != CVariation::TData::TSet::eData_set_type_genotype
             && type != CVariation::TData::TSet::eData_set_type_compound;

        location_within_brackets = false;
    }

    string hgvs_loc_str = "";
    if(placement && variation.IsSetPlacements()) {
        //prefix the placement only if it is defined at this level (otherwise will be handled at the parent level)
        hgvs_loc_str = AsHgvsExpression(*placement);
    }

    if(is_bracketed) {
        if(location_within_brackets) {
            hgvs_data_str = "[" + hgvs_loc_str + hgvs_data_str + "]";
        } else {
            hgvs_data_str = hgvs_loc_str + "[" + hgvs_data_str + "]";
        }
    } else {
        hgvs_data_str = hgvs_loc_str + hgvs_data_str;
    }

    return hgvs_data_str;
}


string Ncbieaa2HgvsAA(const string& prot_str)
{
    string out = "";
    //convert to 3-letter AA codes
    const static char* ncbieaa = "-ABCDEFGHIKLMNPQRSTVWXYZU*O";

    //Note: with Hgvs-flavor "Xaa"
    const static char* iupac3aa = "---AlaAsxCysAspGluPheGlyHisIleLysLeuMetAsnProGlnArgSerThrValTrpXaaTyrGlxSecTerPyl";

    for(size_t i = 0; i < prot_str.size(); i++) {
        char aa = prot_str[i];
        size_t pos = CTempString(ncbieaa).find(aa);
        if(pos == NPOS) {
            //Can't convert. Use ncbistdaa alphabet
            out = prot_str;
            break;
        } else {
            out += CTempString(iupac3aa).substr(pos*3, 3);
        }
    }

    return out;
}

string CHgvsParser::x_SeqLiteralToStr(const CSeq_literal& literal, bool translate, bool is_mito)
{
    string out("");

    if(literal.IsSetSeq_data()) {
        CRef<CSeq_data> sd(new CSeq_data);
        sd->Assign(literal.GetSeq_data());

        if(  sd->IsIupacna()
          || sd->IsNcbi2na()
          || sd->IsNcbi4na()
          || sd->IsNcbi8na()
          || sd->IsNcbipna())
        {
            CSeqportUtil::Convert(*sd, sd, CSeq_data::e_Iupacna, 0, literal.GetLength() );
            const string& nuc_str = sd->GetIupacna().Get();

            if(translate) {
                CGenetic_code code;
                code.SetId(is_mito ? 2 : 1);
                CSeqTranslator::Translate(
                        nuc_str,
                        out,
                        CSeqTranslator::fIs5PrimePartial,
                        &code);
                out = Ncbieaa2HgvsAA(out);
            } else {
                out = nuc_str;
            }
        } else if(sd->IsIupacaa()
               || sd->IsNcbi8aa()
               || sd->IsNcbieaa()
               || sd->IsNcbipaa()
               || sd->IsNcbistdaa())
        {
            string prot_str;
            CSeqportUtil::Convert(*sd, sd, CSeq_data::e_Ncbieaa, 0, literal.GetLength() );
            prot_str = sd->GetNcbieaa().Get();
            out = Ncbieaa2HgvsAA(prot_str);
        }

    } else {
        if(translate && literal.GetLength() > 0) {
            NcbiCerr << MSerial_AsnText << literal;
            NCBI_THROW(CException, eUnknown, "Not supported");
        }
        out = s_IntWithFuzzToStr(literal.GetLength(), NULL, false, literal.IsSetFuzz() ? &literal.GetFuzz() : NULL);
    }
    return out;
}


string CHgvsParser::x_LocToSeqStr(const CSeq_loc& loc)
{
    CSeqVector v(loc, m_scope, CBioseq_Handle::eCoding_Iupac);
    string seq_str;
    v.GetSeqData(v.begin(), v.end(), seq_str);
    return seq_str;
}


TSignedSeqPos CHgvsParser::s_GetHgvsPos(TSeqPos abs_pos, const TSeqPos* atg_pos)
{
    if(!atg_pos) {
        return abs_pos;
    } else {
        TSignedSeqPos pos = (TSignedSeqPos)abs_pos + 1 - *atg_pos; //hgvs absolute coordinates are 1-based.
        if(pos <= 0) {
            pos--;
        }
        return pos;
    }
}


string CHgvsParser::s_IntWithFuzzToStr(
    long pos, 
    const TSeqPos* hgvs_ref_pos, 
    bool with_sign, 
    const CInt_fuzz* fuzz)
{
/* 
 * with_sign indicates whether the sign is mandatory and must be factored out 
 * (as offset part of an intronic expression)
 * In this case we'll prefix the sign in the end, and will adjust for sign
 * of values inside the expressions by multiplying by k
 */
    long hgvs_pos = s_GetHgvsPos(pos, hgvs_ref_pos);
    int sign = hgvs_pos >= 0 ? 1 : -1;
    int k = (with_sign && sign == -1) ? -1 : 1;

    string val = "";
    if(fuzz && fuzz->IsRange()) {
        string from = NStr::LongToString(k*s_GetHgvsPos(fuzz->GetRange().GetMin(), hgvs_ref_pos));
        string to   = NStr::LongToString(k*s_GetHgvsPos(fuzz->GetRange().GetMax(), hgvs_ref_pos));
        val = "(" + from + "_" + to + ")";
    } else {
        val = NStr::LongToString(k*hgvs_pos);
        val =   !fuzz ? 
                     val
              : !fuzz->IsLim() ?
                    "(" + val + ")"
              : fuzz->GetLim() == CInt_fuzz::eLim_gt || fuzz->GetLim() == CInt_fuzz::eLim_tr ?
                    "(" + val + "_?)"
              : fuzz->GetLim() == CInt_fuzz::eLim_lt || fuzz->GetLim() == CInt_fuzz::eLim_tl ?
                    "(?_" + val + ")"
              :     "(" + val + ")";
    }

    return (!with_sign  ? "" 
           : sign >= 0  ? "+" 
           :              "-") + val;
}

string CHgvsParser::s_SeqIdToHgvsStr(const CVariantPlacement& vp, CScope* scope)
{
    string moltype = "";

    if(vp.GetMol() == CVariantPlacement::eMol_genomic) {
        moltype = "g.";
    } else if(vp.GetMol() == CVariantPlacement::eMol_cdna) {
        moltype = "c.";
    } else if(vp.GetMol() == CVariantPlacement::eMol_rna) {
        moltype = "n.";
    } else if(vp.GetMol() == CVariantPlacement::eMol_protein) {
        moltype = "p.";
    } else if(vp.GetMol() == CVariantPlacement::eMol_mitochondrion) {
        moltype = "m.";
    } else {
        moltype = "u.";
    }

    string idstr;
    {{
        const CSeq_id& id = sequence::GetId(vp.GetLoc(), NULL);
        idstr = scope && id.IsGi() ? sequence::GetAccessionForGi(id.GetGi(), *scope)
                                   : id.GetSeqIdString(true);
    
        if(NStr::StartsWith(idstr, "LRG:")) {
            idstr = idstr.substr(4);
        }
    }}

    return (vp.GetLoc().GetStrand() == eNa_strand_minus ? "o" : "") //in HGVS minus-strand is prefixed with "o"
           +idstr + ":" + moltype;
}

string CHgvsParser::s_OffsetPointToString(
        TSeqPos anchor_pos,
        const CInt_fuzz* anchor_fuzz,
        TSeqPos anchor_ref_pos,
        TSeqPos effective_seq_length,
        const long* offset_pos,
        const CInt_fuzz* offset_fuzz)
{
    if(offset_pos && (anchor_pos == 0 || anchor_pos >= effective_seq_length - 1)) {
        //JIRA:VAR-343
        //If we have an offset-point anchored to either start of end of the transcript (less polyA),
        //then the anchor+offset must be resolved, i.e. reported as absolute position relative the origin of the coordinate system.
        //That is, intronic positions are reported relative to closest exon boundary, while near-gene positions are reported
        //relative to the coordinate system origin, which could be start of the sequence, cds-start, or cds-stop, depending on the context.
        //
        // Note: not sure whether anchor_fuzz and/or offset_fuzz should be used, and whether it needs to be modified 
        long resolved_pos = anchor_pos + *offset_pos;
        return s_IntWithFuzzToStr(resolved_pos, &anchor_ref_pos, false, anchor_fuzz);
    } else {
        string anchor_str = s_IntWithFuzzToStr(anchor_pos, &anchor_ref_pos, false, anchor_fuzz);
        string offset_str = !offset_pos ? "" : s_IntWithFuzzToStr(*offset_pos, NULL, true, offset_fuzz);
        return anchor_str+offset_str;
    }
}

string CHgvsParser::AsHgvsExpression(const CVariantPlacement& p)
{
    return s_SeqIdToHgvsStr(p, m_scope) + x_PlacementCoordsToStr(p);
}

string CHgvsParser::x_PlacementCoordsToStr(const CVariantPlacement& orig_vp)
{    
    //For protein placement we'll need seq-data (e.g. p.123Glu)
    CRef<CVariantPlacement> vp_ref;

    CVariationUtil util(*m_scope);

    if(orig_vp.GetMol() == CVariantPlacement::eMol_protein && !orig_vp.IsSetSeq()) {
        vp_ref.Reset(new CVariantPlacement);
        vp_ref->Assign(orig_vp);
        util.AttachSeq(*vp_ref);
    }
    const CVariantPlacement& vp = vp_ref ? *vp_ref : orig_vp;
        
    CBioseq_Handle bsh;
    {{
        const CSeq_id& id = sequence::GetId(vp.GetLoc(), NULL);
        if(id.IsGeneral() && id.GetGeneral().GetDb() == "LRG") {
            CRef<CSeq_id_Resolver> lrg_resolver(new CSeq_id_Resolver__LRG(*m_scope));
            if(lrg_resolver->CanCreate(id.GetGeneral().GetTag().GetStr())) {
                CSeq_id_Handle idh = lrg_resolver->Get(id.GetGeneral().GetTag().GetStr());
                bsh = m_scope->GetBioseqHandle(idh);
            }
        } else {
            bsh = m_scope->GetBioseqHandle(id);
        }
    }}

    //we'll need to detect when an anchor in anchor+offset case occurs at last position of 
    //the last exon; we'll need to know the effective length.
    size_t effective_seq_length = util.GetEffectiveTranscriptLength(bsh);

    //For c.-based coordinates, the first pos as start of CDS.
    //If the position falls it 3'-UTR, the origin is CDS-stop.
    TSeqPos first_pos = 0;
    TSeqPos cds_last_pos = 0;
    if(vp.GetMol() == CVariantPlacement::eMol_cdna) {
        for(CFeat_CI ci(bsh); ci; ++ci) {
            const CMappedFeat& mf = *ci;
            if(mf.GetData().IsCdregion()) {
                first_pos = sequence::GetStart(mf.GetLocation(), NULL, eExtreme_Biological);
                cds_last_pos = sequence::GetStop(mf.GetLocation(), NULL, eExtreme_Biological);
                break;
            }
        }
    }

    string loc_str = "";
    if(vp.GetLoc().IsEmpty() || vp.GetLoc().IsNull()) {
        loc_str = "?";

        //Note: it is possible that the location is not known, but the sequence is known, e.g. if 
        //protein variation was derived from a variation on a partial CDS.

        if(vp.GetMol() == CVariantPlacement::eMol_protein && vp.IsSetSeq() && vp.GetSeq().IsSetSeq_data()) {
            //prepend first AA of asserted sequence
            string aa = vp.GetSeq().GetSeq_data().GetNcbieaa().Get().substr(0,1);
            loc_str = Ncbieaa2HgvsAA(aa) + loc_str;
        }

    } else if(vp.GetLoc().IsWhole()) {
        ; //E.g. "NG_12345.6:g.=" represents no-change ("=") on the whole "NG_12345.6:g."
    } else if(vp.GetLoc().IsPnt() && CVariationUtil::s_GetLength(vp, NULL) == 1) {
        //Note, if this is a point, but we have stop-offset, we need to treat it as interval
        //This happens when we have an offset-based placement with start==stop, which gets
        //collapsed to a single point after remapping instead of remaining a single-base interval.

        const CSeq_point& pnt = vp.GetLoc().GetPnt();

        long start_offset = 0;
        if(vp.IsSetStart_offset()) {
            start_offset = vp.GetStart_offset();
        }

        bool is_cdsstop_relative = cds_last_pos && pnt.GetPoint() > cds_last_pos;

        loc_str = s_OffsetPointToString(
                vp.GetLoc().GetPnt().GetPoint(),
                vp.GetLoc().GetPnt().IsSetFuzz() ? &vp.GetLoc().GetPnt().GetFuzz() : NULL,
                is_cdsstop_relative ? cds_last_pos + 1 : first_pos,
                effective_seq_length,
                (vp.IsSetStart_offset() ? &start_offset : NULL),
                vp.IsSetStart_offset_fuzz() ? &vp.GetStart_offset_fuzz() : NULL);

        if(is_cdsstop_relative) {
            loc_str = "*" + loc_str;
        }

        if(vp.GetMol() == CVariantPlacement::eMol_protein) {
            //prepend first AA of asserted sequence
            string aa = vp.GetSeq().GetSeq_data().GetNcbieaa().Get().substr(0,1);
            loc_str = Ncbieaa2HgvsAA(aa) + loc_str;
        }
    } else {
        CConstRef<CSeq_loc> int_loc;
        if(vp.GetLoc().IsInt()) {
            int_loc.Reset(&vp.GetLoc());
        } else {
            int_loc = sequence::Seq_loc_Merge(vp.GetLoc(), CSeq_loc::fMerge_SingleRange, m_scope);
        }

        bool is_biostart_cdsstop_relative = cds_last_pos && int_loc->GetStart(eExtreme_Biological) > cds_last_pos;
        const CInt_fuzz* biostart_fuzz = sequence::GetStrand(vp.GetLoc(), NULL) == eNa_strand_minus ?
                (int_loc->GetInt().IsSetFuzz_to() ? &int_loc->GetInt().GetFuzz_to() : NULL)
              : (int_loc->GetInt().IsSetFuzz_from() ? &int_loc->GetInt().GetFuzz_from() : NULL);
        long biostart_offset = 0;
        if(vp.IsSetStart_offset()) {
            biostart_offset = vp.GetStart_offset();
        }
        string biostart_str = s_OffsetPointToString(
                int_loc->GetStart(eExtreme_Biological),
                biostart_fuzz,
                is_biostart_cdsstop_relative ? cds_last_pos + 1 : first_pos,
                effective_seq_length,
                vp.IsSetStart_offset() ? &biostart_offset : NULL,
                vp.IsSetStart_offset_fuzz() ? &vp.GetStart_offset_fuzz() : NULL);
        if(vp.GetMol() == CVariantPlacement::eMol_protein) {
            string aa = vp.GetSeq().GetSeq_data().GetNcbieaa().Get().substr(0,1);
            biostart_str = Ncbieaa2HgvsAA(aa) + biostart_str;
        }
        if(is_biostart_cdsstop_relative) {
            biostart_str = "*" + biostart_str;
        }


        bool is_biostop_cdsstop_relative = cds_last_pos && int_loc->GetStop(eExtreme_Biological) > cds_last_pos;
        const CInt_fuzz* biostop_fuzz = sequence::GetStrand(vp.GetLoc(), NULL) == eNa_strand_minus ?
                (int_loc->GetInt().IsSetFuzz_from() ? &int_loc->GetInt().GetFuzz_from() : NULL)
              : (int_loc->GetInt().IsSetFuzz_to() ? &int_loc->GetInt().GetFuzz_to() : NULL);
        long biostop_offset = 0;
        if(vp.IsSetStop_offset()) {
            biostop_offset = vp.GetStop_offset();
        }
        string biostop_str = s_OffsetPointToString(
                int_loc->GetStop(eExtreme_Biological),
                biostop_fuzz,
                is_biostop_cdsstop_relative ? cds_last_pos + 1 : first_pos,
                effective_seq_length,
                vp.IsSetStop_offset() ? &biostop_offset : NULL,
                vp.IsSetStop_offset_fuzz() ? &vp.GetStop_offset_fuzz() : NULL);
        if(vp.GetMol() == CVariantPlacement::eMol_protein) {
            //prepend last aa of the asserted sequence
            const string& prot_str = vp.GetSeq().GetSeq_data().GetNcbieaa().Get();
            biostop_str = Ncbieaa2HgvsAA(prot_str.substr(prot_str.size() - 1,1)) + biostop_str;
        }
        if(is_biostop_cdsstop_relative) {
            biostop_str = "*" + biostop_str;
        }

        if(sequence::GetStrand(vp.GetLoc(), NULL) == eNa_strand_minus) {
            swap(biostart_str, biostop_str);
        }

        loc_str = biostart_str + "_" + biostop_str;
    }
    return loc_str;
}


TSeqPos CHgvsParser::x_GetInstLength(const CVariation_inst& inst, const CVariantPlacement& p, bool account_for_multiplier)
{
    TSeqPos len(0);

    ITERATE(CVariation_inst::TDelta, it, inst.GetDelta()) {
        const CDelta_item& d = **it;
        int multiplier = d.IsSetMultiplier() && account_for_multiplier ? d.GetMultiplier() : 1;
        TSeqPos d_len(0);
        if(d.GetSeq().IsLiteral()) {
            d_len = d.GetSeq().GetLiteral().GetLength();
        } else if(d.GetSeq().IsThis()) {
            d_len = CVariationUtil::s_GetLength(p, m_scope);
        } else if(d.GetSeq().IsLoc()) {
            d_len = sequence::GetLength(d.GetSeq().GetLoc(), m_scope);
        } else {
            NCBI_THROW(CException, eUnknown, "Unhandled code");
        }
        len += d_len * multiplier;
    }
    return len;
}

bool IsMitochondrion(CBioseq_Handle bsh)
{
    const CBioSource* bs = sequence::GetBioSource(bsh);
    return bs && bs->GetGenome() == CBioSource::eGenome_mitochondrion;
}

string CHgvsParser::x_AsHgvsInstExpression(
        const CVariation& variation,
        CConstRef<CVariantPlacement> placement,
        CConstRef<CSeq_literal> explicit_asserted_seq)
{

    CBioseq_Handle bsh;
    if(placement) {
        bsh = m_scope->GetBioseqHandle(sequence::GetId(placement->GetLoc(), NULL));
    }
    bool is_mito = bsh && IsMitochondrion(bsh);

    const CVariation_inst& inst = variation.GetData().GetInstance();
    bool is_prot_inst =
            inst.GetType() == CVariation_inst::eType_prot_missense
         || inst.GetType() == CVariation_inst::eType_prot_nonsense
         || inst.GetType() == CVariation_inst::eType_prot_neutral
         || inst.GetType() == CVariation_inst::eType_prot_other
         || inst.GetType() == CVariation_inst::eType_prot_silent;

    if(is_prot_inst && placement && placement->GetMol() != CVariantPlacement::eMol_protein) {
        NCBI_THROW(CException, eUnknown, "Can't make protein HGVS expression for nucleotide placement");
    }
    bool is_prot = is_prot_inst || (placement && placement->GetMol() == CVariantPlacement::eMol_protein);


    CConstRef<CSeq_literal> asserted_seq(NULL);
    {{
        //Priority for using asserted-sequence:
        //use from placement (instantiate if necessary); otherwise use explicit packaged asserted-observation
        //seq-literal passed from above. Will only use it if have seq-data (SNP-5605) and don't have fuzz (VAR-638)
        if(    placement 
            && placement->IsSetSeq() 
            && placement->GetSeq().IsSetSeq_data() 
            && !CTypeConstIterator<CInt_fuzz>(Begin(placement->GetLoc())))
        {
            asserted_seq.Reset(&placement->GetSeq());
        } else {
#if 0
        //don't automatically fetch asserted sequence, because want to allow explicit assertions, e.g.
        //NC_000001.9:g.(2472747_?)_(?_2489105)inv16359, but don't compute 16359 automatically because
        //this may not be applicable to fuzzy locs

            if(placement) {
                //have placement but no sequence, see if we can fetch it
                CRef<CVariantPlacement> p2(new CVariantPlacement);
                p2->Assign(*placement);
                CVariationUtil util(*m_scope);
                if(util.AttachSeq(*p2)) {
                    asserted_seq.Reset(&p2->GetSeq());
                }
            }
#endif

            if(!asserted_seq
               && explicit_asserted_seq
               && !(placement && placement->GetMol() == CVariantPlacement::eMol_protein))
            {
                /*
                 * Getting seq from placement might or might not have worked (e.g. can't get for intronic case).
                 * If asserted sequence is not filled out, see if we have apriori asserted sequence, except
                 * cannot use explicit asserted seq to construct prot inst, as it could be partially-specified: e.g.
                 * "NP_079142.2:p.C11_G21delinsGlnSerLys - the asserted seq is C..G, so we cannot construct
                 * del??ins representation that asserts the sequence being deleted within a delins.
                 */
                asserted_seq = explicit_asserted_seq.GetPointer();
            }
        }
    }}

    static const size_t s_max_literal_length = 16;

    string asserted_seq_str =
           !asserted_seq                                     ? ""
         : asserted_seq->GetLength() < s_max_literal_length  ? x_SeqLiteralToStr(*asserted_seq, is_prot, is_mito)
         :                                                     NStr::NumericToString(asserted_seq->GetLength());


    string inst_str = "";
    bool append_delta = false;
    if(inst.GetType() == CVariation_inst::eType_identity
       || inst.GetType() == CVariation_inst::eType_prot_silent)
    {
        //Prepend the asserted sequence, but only if its lengh is under threshold.
        //If it is too long, it can't be used, as it will be represented by a number, and
        //the preceding context also ends with a number (location): e.g
        //  NC_000001:g.100000A=        - correct
        //  NC_000001:g.100000_100123=  - correct
        //  NC_000001:g.100000_100123124= - wrong, can't use literal's length "124"
        inst_str = (asserted_seq && asserted_seq->GetLength() < s_max_literal_length  && !is_prot ? asserted_seq_str : "") + "=";
    } else if(inst.GetType() == CVariation_inst::eType_inv) {
        inst_str = "inv" + asserted_seq_str;
    } else if(inst.GetType() == CVariation_inst::eType_snv) {
        inst_str = (asserted_seq ? asserted_seq_str : "N" )+ ">";
        append_delta = true;
    } else if(inst.GetType() == CVariation_inst::eType_mnp
              || inst.GetType() == CVariation_inst::eType_delins
              || inst.GetType() == CVariation_inst::eType_prot_other)
    {
        if(inst.GetType() == CVariation_inst::eType_prot_other &&
           placement && placement->GetLoc().IsPnt() &&
              placement->GetLoc().GetPnt().GetPoint() == 0)
        {   
            inst_str = "extMet-";
        } else if(inst.GetType() == CVariation_inst::eType_prot_other
                  && placement && placement->GetLoc().IsPnt()
                  && bsh 
                  && placement->GetLoc().GetPnt().GetPoint() == bsh.GetInst_Length() - 1)
        {   
            inst_str = "ext*";
        } else if(inst.GetType() == CVariation_inst::eType_prot_other) {
            inst_str = "delins";
        } else {
            inst_str = "del" + asserted_seq_str + "ins";
        }

        append_delta = true;
    } else if(inst.GetType() == CVariation_inst::eType_del) {
        if(placement && placement->GetLoc().IsWhole()) {
            inst_str = "0"; //whole-product deletion
        } else if(is_prot) {
            inst_str = "del"; //do not generate asserted part for protein expressions: SNP-4623
        } else {
            inst_str = "del" + asserted_seq_str;
        }
    } else if(inst.GetType() == CVariation_inst::eType_ins) {
        //If the insertion is this*2 then this is a dup
        bool is_dup = false;
        if(inst.GetDelta().size() == 1) {
           const CDelta_item& delta = **inst.GetDelta().begin();
           if(delta.GetSeq().IsThis() && delta.IsSetMultiplier() && delta.GetMultiplier() == 2) {
               is_dup = true;
           }
        }

        if(is_dup) {
            inst_str = "dup" + asserted_seq_str;
            append_delta = false;
        } else {
            inst_str = "ins";
            append_delta = true;
        }
    } else if(inst.GetType() == CVariation_inst::eType_microsatellite) {
        inst_str = "";
        append_delta = true;
    } else if(inst.GetType() == CVariation_inst::eType_transposon) {
        inst_str = "con";
        append_delta = true;
    } else if(inst.GetType() == CVariation_inst::eType_prot_missense
           || inst.GetType() == CVariation_inst::eType_prot_nonsense
           || inst.GetType() == CVariation_inst::eType_prot_neutral)
    {
        append_delta = true;
    } else {
        inst_str = "?";
    }


    if(append_delta) {
        ITERATE(CVariation_inst::TDelta, it, inst.GetDelta()) {

            const CDelta_item& delta = **it;

            if(variation.GetData().GetInstance().GetType() == CVariation_inst::eType_microsatellite
               && variation.GetParent()
               && !variation.IsSetPlacements()
               && variation.GetParent()->GetData().GetSet().GetVariations().front().GetPointer() != &variation)
            {
                /*
                 * Don't use literal subsequent subvariations in a multi-allele microsatellite expression,
                 * e.g. NM_000815.2:c.100_101TC[5]+[3], as opposed to NM_000815.2:c.100_101TC[5]+TC[3] -
                 * in the second subexpression we simply want [3] instead of TC[3]
                 */
                ;
            } else if(delta.GetSeq().IsThis()) {
                ;
            } else if(delta.GetSeq().IsLiteral()) {
                CRef<CSeq_literal> literal(new CSeq_literal);
                literal->Assign(delta.GetSeq().GetLiteral());
                if(NStr::StartsWith(inst_str, "extMet") || NStr::StartsWith(inst_str, "extX")) {
                    literal->SetLength()--;
                    //length of extension is one less than the length of the sequence that replaces first or last AA
                }

                string variant_str = x_SeqLiteralToStr(*literal, is_prot, is_mito);
                if(inst_str == variant_str + ">") {
                    //instead of "G>G" etc want to report "G="
                    inst_str[inst_str.size() - 1] = '='; //overwrite '>' with '='
                } else {
                    inst_str += variant_str;
                }
            } else if(delta.GetSeq().IsLoc()) {
                string delta_loc_str;
                //the repeat-unit in microsattelite is always literal sequence:
                //NG_011572.1:g.5658NG_011572.1:g.5658_5660(15_24)  - incorrect
                //NG_011572.1:g.5658CAG(15_24) - correct
                if(inst.GetType() == CVariation_inst::eType_microsatellite) {
                    delta_loc_str = x_LocToSeqStr(delta.GetSeq().GetLoc());
                } else {
                    CRef<CVariantPlacement> p_tmp(new CVariantPlacement);
                    p_tmp->SetLoc().Assign(delta.GetSeq().GetLoc());
                    CVariationUtil util(*m_scope);
                    p_tmp->SetMol(util.GetMolType(sequence::GetId(p_tmp->GetLoc(), NULL)));

                    if(p_tmp->GetLoc().GetId()
                       && placement->GetLoc().GetId()
                       && p_tmp->GetLoc().GetId()->Equals(*placement->GetLoc().GetId()))
                    {
                        //if delta has same seq-id as placement, omit the seq-id header,
                        //e.g. NM_000815.2:c.100delTins5_10
                        //instead of NM_000815.2:c.100delTinsNM_000815.2:c.5_10
                        delta_loc_str = x_PlacementCoordsToStr(*p_tmp);
                    } else {
                        //with header NM_000815.3:c.100delTinsNM_000815.2:c.5_10
                        delta_loc_str = AsHgvsExpression(*p_tmp);
                    }
                }

                inst_str += delta_loc_str;

            } else {
                NCBI_THROW(CException, eUnknown, "Unhandled delta class");
            }

            //add multiplier, but make sure we're dealing with SSR.
            if(delta.IsSetMultiplier()) {
                if(inst.GetType() == CVariation_inst::eType_microsatellite) {
                    string multiplier_str = s_IntWithFuzzToStr(
                            delta.GetMultiplier(),
                            NULL,
                            false,
                            delta.IsSetMultiplier_fuzz() ? &delta.GetMultiplier_fuzz() : NULL);

                    if(!NStr::StartsWith(multiplier_str, "(")) {
                        multiplier_str = "[" + multiplier_str + "]";
                        //In HGVS-land the fuzzy multiplier value (in parentheses) existis as is, but an exact value
                        //is enclosed in brackets like an allele-set.
                    }

                    inst_str += multiplier_str;
                } else if(!NStr::StartsWith(inst_str, "dup")) {
                    //multiplier is expected for dup or ssr representation only
                    NCBI_THROW(CException, eUnknown, "Multiplier value is set in unexpected context (only STR supported)");
                }
            }
        }
    }

    return inst_str;
}

};

END_NCBI_SCOPE

