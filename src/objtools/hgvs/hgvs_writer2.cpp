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
#include <objects/variation/Variation.hpp>
#include <objects/variation/VariantPlacement.hpp>
#include <objects/variation/VariationMethod.hpp>

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
#include <objects/general/User_object.hpp>
#include <objects/seqloc/Seq_loc_equiv.hpp>

#include <serial/iterator.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/seq_vector.hpp>
#include <objtools/hgvs/hgvs_parser2.hpp>
#include <objtools/hgvs/variation_util2.hpp>

BEGIN_NCBI_SCOPE

namespace variation {


CRef<CVariantPlacement> CHgvsParser::x_AdjustPlacementForHgvs(const CVariantPlacement& p, const CVariation_inst& inst)
{
    //todo: implement
    CRef<CVariantPlacement> placement(new CVariantPlacement);
    placement->Assign(p);
    return placement;
}

CRef<CSeq_literal> CHgvsParser::x_FindAssertedSequence(const CVariation& v)
{
    CRef<CSeq_literal> asserted_seq;

    if(!v.GetData().IsSet() || v.GetData().GetSet().GetType() != CVariation::TData::TSet::eData_set_type_package) {
        return asserted_seq;
    }

    ITERATE(CVariation::TData::TSet::TVariations, it, v.GetData().GetSet().GetVariations()) {
        const CVariation& v2 = **it;
        if(!v2.GetData().IsInstance() || !v2.GetData().GetInstance().IsSetObservation()) {
            continue;
        }

        //capture asserted sequence
        if(v2.GetData().GetInstance().GetObservation() & CVariation_inst::eObservation_asserted) {
            asserted_seq.Reset(new CSeq_literal);
            asserted_seq->Assign(v2.GetData().GetInstance().GetDelta().front()->GetSeq().GetLiteral());
        }
    }

    return asserted_seq;
}


string CHgvsParser::AsHgvsExpression(const CVariation& variation,  const CSeq_id* seq_id)
{
    return x_AsHgvsExpression(variation, seq_id, NULL);
}

string CHgvsParser::x_AsHgvsExpression(
        const CVariation& variation,
        const CSeq_id* seq_id,
        const CSeq_literal* asserted_seq)
{
    //Find the placement to use (It is possible not to have one at the top level, if subvariations have their own)
    CRef<CVariantPlacement> placement;
    if(variation.IsSetPlacements()) {
        ITERATE(CVariation::TPlacements, it, variation.GetPlacements()) {
            const CVariantPlacement& p = **it;
            if(!seq_id || (p.GetLoc().GetId() && sequence::IsSameBioseq(*p.GetLoc().GetId(), *seq_id, m_scope))) {
                placement.Reset(new CVariantPlacement);
                placement->Assign(p);
                break;
            }
        }
        if(!placement) {
            NCBI_THROW(CArgException, CArgException::eInvalidArg, "Variations.placements is set, but could not find requested one");
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
        return x_AsHgvsExpression(*flipped_variation, seq_id, flipped_asserted_seq.GetPointerOrNull());
    }

    //if this variation is a package containing asserted sequence, unwrap it and call recursively
    //(as asserted sequence does not participate in HGVS expression as independent instance subexpression)
    if(variation.GetData().IsSet()
       && variation.GetData().GetSet().GetType() == CVariation::TData::TSet::eData_set_type_package)
    {
        CRef<CVariation> v(new CVariation);
        v->Assign(variation);
        asserted_seq = x_FindAssertedSequence(*v);
    }


    string hgvs_data_str = "";
    if(variation.GetData().IsSet()) {
        const CVariation::TData::TSet& vset = variation.GetData().GetSet();
        string delim_type =
                vset.GetType() == CVariation::TData::TSet::eData_set_type_compound    ? ""
              : vset.GetType() == CVariation::TData::TSet::eData_set_type_haplotype   ? ";"
              : vset.GetType() == CVariation::TData::TSet::eData_set_type_products
                || vset.GetType() == CVariation::TData::TSet::eData_set_type_mosaic   ? ","
              : vset.GetType() == CVariation::TData::TSet::eData_set_type_alleles
                || vset.GetType() == CVariation::TData::TSet::eData_set_type_genotype
                || vset.GetType() == CVariation::TData::TSet::eData_set_type_package  ? "]+["
              : vset.GetType() == CVariation::TData::TSet::eData_set_type_individual  ? "(+)"
              : "](+)[";

        string delim = "";
        size_t count(0);
        ITERATE(CVariation::TData::TSet::TVariations, it, variation.GetData().GetSet().GetVariations()) {
            const CVariation& v2 = **it;

            //asserted or reference instances don't participate in HGVS expressions as individual subvariations
            if(v2.GetData().IsInstance()
               && v2.GetData().GetInstance().IsSetObservation()
               && !(v2.GetData().GetInstance().GetObservation() & CVariation_inst::eObservation_variant))
            {
                continue;
            }

            hgvs_data_str += delim + x_AsHgvsExpression(**it, seq_id, asserted_seq);
            delim = delim_type;
            count++;
        }

        bool is_unbracketed =
                vset.GetType() == CVariation::TData::TSet::eData_set_type_compound
             || vset.GetType() == CVariation::TData::TSet::eData_set_type_package && count <= 1;

        if(!is_unbracketed) {
            hgvs_data_str = "[" + hgvs_data_str + "]";
        }

    } else if(variation.GetData().IsInstance()) {
        if(placement) {
            placement = x_AdjustPlacementForHgvs(*placement, variation.GetData().GetInstance());
        }
        hgvs_data_str = x_AsHgvsInstExpression(variation.GetData().GetInstance(), placement, asserted_seq);
    }

    string hgvs_loc_str = "";
    if(placement) {
        hgvs_loc_str = AsHgvsExpression(*placement);
    }

    return hgvs_loc_str + hgvs_data_str;
}


string CHgvsParser::x_SeqLiteralToStr(const CSeq_literal& literal)
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
            out = sd->GetIupacna().Get();
        } else if(sd->IsIupacaa()
               || sd->IsNcbi8aa()
               || sd->IsNcbieaa()
               || sd->IsNcbipaa()
               || sd->IsNcbistdaa())
        {
            CSeqportUtil::Convert(*sd, sd, CSeq_data::e_Iupacaa, 0, literal.GetLength() );
            out = sd->GetIupacaa().Get();
        }

    } else {
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


/////////////////////////////////////////////////////////////////////////////////

long CHgvsParser::s_GetHgvsPos(long abs_pos, const TSeqPos* atg_pos)
{
    if(!atg_pos) {
        return abs_pos;
    } else {
        long pos = (long)abs_pos + 1 - *atg_pos; //hgvs absolute coordinates are 1-based.
        if(pos <= 0) {
            pos--;
        }
        return pos;
    }
}

string CHgvsParser::s_IntWithFuzzToStr(long pos, const TSeqPos* hgvs_ref_pos, bool with_sign, const CInt_fuzz* fuzz)
{
    long hgvs_pos = s_GetHgvsPos(pos, hgvs_ref_pos);
    string out = NStr::IntToString(abs(hgvs_pos));

    if(fuzz) {
        if(fuzz->IsRange()) {
            string from = NStr::IntToString(s_GetHgvsPos(fuzz->GetRange().GetMin(), hgvs_ref_pos));
            string to = NStr::IntToString(s_GetHgvsPos(fuzz->GetRange().GetMax(), hgvs_ref_pos));
            out = "(" + from + "_" + to + ")";
        } else if(fuzz->IsLim()) {
            if(fuzz->GetLim() == CInt_fuzz::eLim_gt || fuzz->GetLim() == CInt_fuzz::eLim_tr) {
                out = "(" + out + "_?)";
            } else if(fuzz->GetLim() == CInt_fuzz::eLim_lt || fuzz->GetLim() == CInt_fuzz::eLim_tl) {
                out = "(?_" + out + ")";
            } else {
                out = "(" + out + ")";
            }
        }
    }
    if(with_sign || hgvs_pos < 0) {
        out = (pos >= 0 ? "+" : "-") + out;
    }


    return out;
}

string CHgvsParser::s_GetHgvsSeqId(const CVariantPlacement& vp)
{
    string moltype = "";

    if(vp.GetMol() == CVariantPlacement::eMol_genomic) {
        moltype = "g.";
    } else if(vp.GetMol() == CVariantPlacement::eMol_cdna) {
        moltype = "c.";
    } else if(vp.GetMol() == CVariantPlacement::eMol_rna) {
        moltype = "r.";
    } else if(vp.GetMol() == CVariantPlacement::eMol_protein) {
        moltype = "p.";
    } else if(vp.GetMol() == CVariantPlacement::eMol_mitochondrion) {
        moltype = "mt.";
    } else {
        moltype = "u.";
    }

    return sequence::GetId(vp.GetLoc(), NULL).GetSeqIdString(true) + ":" + moltype;
}

string CHgvsParser::s_OffsetPointToString(
        TSeqPos anchor_pos,
        const CInt_fuzz* anchor_fuzz,
        TSeqPos anchor_ref_pos,
        const long* offset_pos,
        const CInt_fuzz* offset_fuzz)
{
    string anchor_str = s_IntWithFuzzToStr(anchor_pos, &anchor_ref_pos, false, anchor_fuzz);

    string offset_str = "";
    if(offset_pos) {
        offset_str = s_IntWithFuzzToStr(*offset_pos, NULL, true, offset_fuzz);
    }
    return anchor_str+offset_str;
}

string CHgvsParser::AsHgvsExpression(const CVariantPlacement& p)
{
    return s_GetHgvsSeqId(p) + x_GetHgvsLoc(p);
}

string CHgvsParser::x_GetHgvsLoc(const CVariantPlacement& vp)
{
    //for c.-based coordinates, the first pos as start of CDS.
    TSeqPos first_pos = 0;
    if(vp.GetMol() == CVariantPlacement::eMol_cdna) {
        CBioseq_Handle bsh = m_scope->GetBioseqHandle(vp.GetLoc());
        for(CFeat_CI ci(bsh); ci; ++ci) {
            const CMappedFeat& mf = *ci;
            if(mf.GetData().IsCdregion()) {
                first_pos = sequence::GetStart(mf.GetLocation(), NULL, eExtreme_Biological);
                break;
            }
        }
    }

    string loc_str = "";
    if(vp.GetLoc().IsWhole()) {
        ; //E.g. "NG_12345.6:g.=" represents no-change ("=") on the whole "NG_12345.6:g."
    } else if(vp.GetLoc().IsPnt()) {
        long start_offset = 0;
        if(vp.IsSetStart_offset()) {
            start_offset = vp.GetStart_offset();
        }
        loc_str = s_OffsetPointToString(
                vp.GetLoc().GetPnt().GetPoint(),
                vp.GetLoc().GetPnt().IsSetFuzz() ? &vp.GetLoc().GetPnt().GetFuzz() : NULL,
                first_pos,
                (vp.IsSetStart_offset() ? &start_offset : NULL),
                vp.IsSetStart_offset_fuzz() ? &vp.GetStart_offset_fuzz() : NULL);

        if(vp.GetMol() == CVariantPlacement::eMol_protein) {
            loc_str = vp.GetSeq().GetSeq_data().GetNcbieaa().Get().substr(0,1) + loc_str;
        }
    } else {
        CConstRef<CSeq_loc> int_loc;
        if(vp.GetLoc().IsInt()) {
            int_loc.Reset(&vp.GetLoc());
        } else {
            int_loc = sequence::Seq_loc_Merge(vp.GetLoc(), CSeq_loc::fMerge_SingleRange, m_scope);
        }


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
                first_pos,
                vp.IsSetStart_offset() ? &biostart_offset : NULL,
                vp.IsSetStart_offset_fuzz() ? &vp.GetStart_offset_fuzz() : NULL);
        if(vp.GetMol() == CVariantPlacement::eMol_protein) {
            biostart_str = vp.GetSeq().GetSeq_data().GetNcbieaa().Get().substr(0,1) + biostart_str;
        }


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
                first_pos,
                vp.IsSetStop_offset() ? &biostop_offset : NULL,
                vp.IsSetStop_offset_fuzz() ? &vp.GetStop_offset_fuzz() : NULL);
        if(vp.GetMol() == CVariantPlacement::eMol_protein) {
            const string& prot_str = vp.GetSeq().GetSeq_data().GetNcbieaa().Get();
            biostop_str = prot_str.substr(prot_str.size() - 1,1) + biostop_str;
        }


        if(sequence::GetStrand(vp.GetLoc(), NULL) == eNa_strand_minus) {
            swap(biostart_str, biostop_str);
        }

        loc_str = biostart_str + "_" + biostop_str;
    }
    return loc_str;
}


TSeqPos CHgvsParser::x_GetInstLength(const CVariation_inst& inst, const CSeq_loc& this_loc)
{
    TSeqPos len(0);

    ITERATE(CVariation_inst::TDelta, it, inst.GetDelta()) {
        const CDelta_item& d = **it;
        int multiplier = d.IsSetMultiplier() ? d.GetMultiplier() : 1;
        TSeqPos d_len(0);
        if(d.GetSeq().IsLiteral()) {
            d_len = d.GetSeq().GetLiteral().GetLength();
        } else if(d.GetSeq().IsThis()) {
            d_len = sequence::GetLength(this_loc, m_scope);
        } else if(d.GetSeq().IsLoc()) {
            d_len = sequence::GetLength(d.GetSeq().GetLoc(), m_scope);
        } else {
            NCBI_THROW(CException, eUnknown, "Unhandled code");
        }
        len += d_len * multiplier;
    }
    return len;
}

string CHgvsParser::x_GetInstData(const CVariation_inst& inst, const CSeq_loc& this_loc)
{
    CNcbiOstrstream ostr;
    ITERATE(CVariation_inst::TDelta, it, inst.GetDelta()) {
        const CDelta_item& d = **it;
        int multiplier = d.IsSetMultiplier() ? d.GetMultiplier() : 1;

        string d_seq;
        if(d.GetSeq().IsLiteral()) {
            d_seq = x_SeqLiteralToStr(d.GetSeq().GetLiteral());
        } else if(d.GetSeq().IsThis()) {
            d_seq = x_LocToSeqStr(this_loc);
        } else if(d.GetSeq().IsLoc()) {
            d_seq = x_LocToSeqStr(d.GetSeq().GetLoc());
        } else {
            HGVS_THROW(eLogic, "Unhandled");
        }
        for(int i = 0; i < multiplier; i++) {
            ostr << d_seq;
        }
    }

    return CNcbiOstrstreamToString(ostr);
}


string CHgvsParser::x_AsHgvsInstExpression(
        const CVariation_inst& inst,
        const CVariantPlacement* placement,
        const CSeq_literal* explicit_asserted_seq)
{
    string inst_str = "";

    const CSeq_literal* asserted_seq =
            explicit_asserted_seq ? explicit_asserted_seq
          : placement && placement->IsSetSeq() ? &placement->GetSeq()
          : NULL;

    bool append_delta = false;

    if(inst.GetType() == CVariation_inst::eType_identity
       || inst.GetType() == CVariation_inst::eType_prot_silent)
    {
        inst_str = "=";
    } else if(inst.GetType() == CVariation_inst::eType_inv) {
        inst_str = "inv";
    } else if(inst.GetType() == CVariation_inst::eType_snv) {
        inst_str = (asserted_seq ? x_SeqLiteralToStr(*asserted_seq) : "N" )+ ">";
        append_delta = true;
    } else if(inst.GetType() == CVariation_inst::eType_mnp
              || inst.GetType() == CVariation_inst::eType_delins)
    {
        inst_str = "del" + (asserted_seq ? x_SeqLiteralToStr(*asserted_seq) : "" ) + "ins";
        append_delta = true;
    } else if(inst.GetType() == CVariation_inst::eType_del) {
        inst_str = "del";
        if(asserted_seq) {
            if(asserted_seq->GetLength() == 1) {
                ; //...del
            } else if(asserted_seq->GetLength() < 20) {
                //...delACGT
                inst_str += x_SeqLiteralToStr(*asserted_seq);
            } else {
                //...del25
                inst_str += NStr::IntToString(asserted_seq->GetLength());
            }
        }
    } else if(inst.GetType() == CVariation_inst::eType_ins) {
        append_delta = true;
        //If the insertion is this*2 then this is a dup
        bool is_dup = false;
        if(inst.GetDelta().size() == 1) {
           const CDelta_item& delta = **inst.GetDelta().begin();
           if(delta.GetSeq().IsThis() && delta.IsSetMultiplier() && delta.GetMultiplier() == 2) {
               is_dup = true;
           }
        }

        inst_str = is_dup ? "dup" : "ins";
    } else if(inst.GetType() == CVariation_inst::eType_microsatellite) {
        inst_str = "";
        append_delta = true;
    } else if(inst.GetType() == CVariation_inst::eType_prot_missense
           || inst.GetType() == CVariation_inst::eType_prot_nonsense
           || inst.GetType() == CVariation_inst::eType_prot_neutral)
    {
        append_delta = true;
    } else {
        NCBI_THROW(CException, eUnknown, "Cannot process this type of variation-inst");
    }

#if 0
    //append the deltas
    ITERATE(CVariation_inst::TDelta, it, inst.GetDelta()) {
        const CDelta_item& delta = **it;
        if(delta.GetSeq().IsThis()) {

            //"this" does not appear in HGVS nomenclature - we simply skip it as we
            //have adjusted the location according to HGVS convention for insertinos
            if(inst.GetType() == CVariation_inst::eType_ins && &delta == inst.GetDelta().begin()->GetPointer()) {
                ; //only expecting this this is an isertion and this is the first element of delta
            } else {
                NCBI_THROW(CException, eUnknown, "'this-loc' is not expected here");
            }

        } else if(delta.GetSeq().IsLiteral()) {
            out += x_SeqLiteralToStr(delta.GetSeq().GetLiteral(), flipped_strand);
        } else if(delta.GetSeq().IsLoc()) {

            CRef<CSeq_loc> delta_loc(new CSeq_loc());
            delta_loc->Assign(delta.GetSeq().GetLoc());
            if(flipped_strand) {
                delta_loc->FlipStrand();
            }

            string delta_loc_str;
            //the repeat-unit in microsattelite is always literal sequence:
            //NG_011572.1:g.5658NG_011572.1:g.5658_5660(15_24)  - incorrect
            //NG_011572.1:g.5658CAG(15_24) - correct
            if(inst.GetType() != CVariation_inst::eType_microsatellite) {
                delta_loc_str = x_SeqLocToStr(*delta_loc, true);
            } else {
                delta_loc_str = x_LocToSeqStr(*delta_loc);
            }

            out += delta_loc_str;

        } else {
            NCBI_THROW(CException, eUnknown, "Encountered unhandled delta class");
        }

        //add multiplier, but make sure we're dealing with SSR.
        if(delta.IsSetMultiplier()) {
            if(inst.GetType() == CVariation_inst::eType_microsatellite && !delta.GetSeq().IsThis()) {
                string multiplier_str = x_IntWithFuzzToStr(
                        delta.GetMultiplier(),
                        delta.IsSetMultiplier_fuzz() ? &delta.GetMultiplier_fuzz() : NULL);

                if(!NStr::StartsWith(multiplier_str, "(")) {
                    multiplier_str = "[" + multiplier_str + "]";
                    //In HGVS-land the fuzzy multiplier value existis as is, but an exact value
                    //is enclosed in brackets a-la allele-set.
                }

                out += multiplier_str;
            } else {
                NCBI_THROW(CException, eUnknown, "Multiplier value is set in unexpected context (only STR supported)");
            }
        }
    }
#endif

    return inst_str;
}




};

END_NCBI_SCOPE

